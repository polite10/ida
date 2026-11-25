// Başsız sim sunucusu: fizik + sensör kümesi + A1 köprüsü + zaman yöneticisi (sunucu.hpp).
#include "ida/sunucu/sunucu.hpp"

#include "a2_baglanti.hpp"
#include "a3_baglanti.hpp"
#include "kayit_baglanti.hpp"
#include "senaryo_hizmeti.hpp"
#include "ida/a3/kaynak.hpp"
#include "ida/a3/sunucu.hpp"
#include "ida/a3/zaman.hpp"
#include "ida/fizik/dunya.hpp"
#include "ida/kopru/karsi.hpp"
#include "ida/kopru/param_blogu.hpp"
#include "ida/kopru/tasima.hpp"
#include "ida/senaryo/senaryo.hpp"
#include "ida/senaryo/sonar_yukleyici.hpp"
#include "ida/sensor/sensor_kumesi.hpp"
#include "ida/sonar/sonar_kumesi.hpp"
#include "senaryo_plani.hpp"
#include "yardimci.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <deque>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <thread>
#include <type_traits>

namespace ida::sunucu {
namespace {

using Saat = std::chrono::steady_clock;
using kopru::kH;
using kopru::kMs;
using kopru::kP;

constexpr std::int64_t kCSim = 400'000;   // A1 §3.9 kalem k (tahmin)

// IDAW sabitleri (protocol/ida.xml; üretilmiş başlık bu çeviri birimine alınmaz).
constexpr std::uint8_t kCmdHeartbeat = 1, kCmdArm = 2, kCmdDisarm = 3, kCmdSetMode = 4, kCmdMissionBegin = 7,
                       kCmdMissionEnd = 9, kCmdParamCommit = 13, kCmdParamSave = 14;
constexpr std::uint16_t kActThr = 1, kActSafe = 2, kActArmed = 4;
constexpr std::uint8_t kAckAccepted = 1;
constexpr std::uint8_t kPstoreOk = 2;
constexpr std::uint8_t kCloseRunEnd = 5, kCloseShutdown = 1;

// HIL döngüsünün evreleri (ölçüm; duvar saati).
enum Evre : std::size_t {
    kEvreUyanma = 0,   // sınır bekleyişinden uyanma gecikmesi (hedef ana göre)
    kEvreTfUyanma,     // TF bekleyişinden uyanma gecikmesi
    kEvreKopru,        // köprünün sınır işlemi (A1-K85)
    kEvreSensor,       // sensör kümesi adımı ve boşaltma
    kEvreKodlama,      // ölçümlerin A1 üyesine kodlanması
    kEvreParti,        // parti kapanışı (TF kodlama)
    kEvreYaz,          // taşımaya yazma
    kEvreOkuma,        // taşımadan okuma ve çözme (bekleme içindekiler dahil)
    kEvreFizik,        // fizik adımı
    kEvreSatir,        // satır kaydı
    kEvreSayisi
};
constexpr const char* kEvreAdi[kEvreSayisi] = {"uyanma", "tf_uyanma", "kopru", "sensor", "kodlama",
                                               "parti", "yaz", "okuma", "fizik", "satir"};

struct Euler {
    double yalpa, bas_kic, yon;
};

Euler euler(double w, double x, double y, double z)
{
    const double sp = std::clamp(2.0 * (w * y - z * x), -1.0, 1.0);
    return {std::atan2(2.0 * (w * x + y * z), 1.0 - 2.0 * (x * x + y * y)), std::asin(sp),
            std::atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z))};
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
void ekle(std::string& s, const char* bicim, ...)
{
    char tampon[512];
    va_list ap;
    va_start(ap, bicim);
    const int n = std::vsnprintf(tampon, sizeof tampon, bicim, ap);
    va_end(ap);
    if (n > 0) {
        s.append(tampon, std::min(static_cast<std::size_t>(n), sizeof tampon - 1U));
    }
}

// W5-a3-senaryo: dünyalar arasında yaşayan hizmetler ve yeniden kurma isteği. A3 açıkken `kos` bir sürücü
// döngüsüdür (`kos_a3`): her dünya (`Calistirici`) bu yapının üzerinde kurulur ve yıkılır; yapı yalnız
// süreç boyunca yaşaması gerekenleri tutar. Dünyanın kendi durumu (fizik, sensörler, sonar, çekirdek,
// köprü, emniyet girdileri, kayıt dosyası, A3 durum makinesi) burada DEĞİLDİR: her dünya sıfırdan kurulur.
struct A3Kalici {
    std::unique_ptr<SenaryoHizmeti> hizmet;
    std::unique_ptr<A3Alici> alici;
    std::unique_ptr<a3::Sunucu> sunucu;
    std::unique_ptr<A2Baglanti> a2;           // süreç başına bir A2 sunucusu (kilit, dinleyici); tanım dünya başına
    std::uint64_t durum_rev = 1;              // A3-K122: `durum_rev` süreç boyunca artar (istemci bayat revizyonu yakalar)
    // W5-K55 (EMNİYET; A3-K117, A3-K67): emniyet revizyonu ve doyma bayrağı süreç boyunca yaşar; yeniden
    // kurulumda geri dönmez. Her dünya biterken köprüsünün son değeri (oturum sonu artışından sonra) buraya
    // yazılır (kosu_bitir); sıradaki dünyanın köprüsü bu değerle kurulur ve `yuklendi` durumunda (köprü yokken)
    // yayınlanan revizyon da budur. Süreç başında 1 (yalnız sınama: Ayar::sinama_ilk_emniyet_rev).
    std::uint32_t emniyet_rev = 1;
    bool emniyet_rev_doydu = false;
    bool surekli = false;                     // Ayar::a3_surekli
    std::uint64_t dunya_no = 0;               // kurulan dünya sayısı
    // Yeni dünya isteği. `yukle`: senaryo/yukle (sonucu yeni dünya `yuklendi` olunca verilir); `sifirla`
    // (sonucu isteği alan dünya verdi); `yeniden`: komutsuz yeniden kurma (süreklilik kipinde başlamadan biten
    // dünya, ör. hazırlıkta durdurma).
    struct Istek {
        enum class Tur : std::uint8_t { yukle, sifirla, yeniden };
        Tur tur = Tur::yeniden;
        A3Giris giris;
        std::uint64_t aday = 0;
        Saat::time_point kabul{};               // isteğin dünya tarafından kabul edildiği an (ölçüm)
        bool basarisiz = false;                 // yeni dünya kurulamadı; önceki senaryo geri kuruldu
        std::string hata_kodu, ileti;
    };
    std::optional<Istek> istek;                 // biten dünyanın bıraktığı
    std::optional<Istek> tamamlanacak;          // yeni dünyanın kurulunca sonuçlandıracağı
    std::int64_t son_yeniden_kurma_ns = 0;      // son yeniden kurmanın kabulden `yuklendi`ye süresi (ölçüm)
};

class Calistirici {
public:
    explicit Calistirici(const Ayar& a, A3Kalici* kalici = nullptr) : a_(a), kalici_(kalici) {}
    Calistirici(const Calistirici&) = delete;
    Calistirici& operator=(const Calistirici&) = delete;
    ~Calistirici()
    {
        dur_ = true;
        if (karsi_is_.joinable()) {
            karsi_is_.join();
        }
    }

    // Sol 4. denetim #1: dış sınırda istisna yakalanır ve Sonuc::hata'ya çevrilir (süreç
    // std::terminate ile düşmez); oturum açıksa kapanışı kos_ic'teki koruyucu yapmıştır.
    // W5-bagla-a2: A2 sunucusu her çıkışta (olağan, hata, sinyal, istisna) burada, tek yerde
    // kapanır: kos_ic döndükten, yani dünya döngüsü bittikten ve A1 kapanışı (olağan bitiş, erken
    // kapanış ya da kapanış koruyucusu) tamamlandıktan sonra. İkinci bir kapanış yolu değildir;
    // A1 kapanışını geciktirmez.
    Sonuc kos() noexcept
    {
        try {
            Sonuc r = kos_ic();
            kosu_bitir(r);
            return r;
        } catch (const std::exception& e) {
            try {
                s_.hata = std::string("istisna: ") + e.what();
                s_.tamam = false;
                kosu_bitir(s_);
                return s_;
            } catch (...) {
                return Sonuc{};
            }
        } catch (...) {
            try {
                s_.hata = "istisna (bilinmeyen tür)";
                s_.tamam = false;
                kosu_bitir(s_);
                return s_;
            } catch (...) {
                return Sonuc{};
            }
        }
    }

    // `a3_baslat` dünyayı A3'e bağladı ve `yuklendi` durumunu yayınladı (sürücü: dünya kurulamadı mı?).
    bool a3_kuruldu() const noexcept { return a3_kuruldu_; }

private:
    Sonuc kos_ic();
    void kosu_bitir(Sonuc& r) noexcept;

    // ---- Zaman ----
    std::int64_t duvar() const
    {
        if (a_.kip == Kip::sil) {
            return sil_duvar_;
        }
        return std::chrono::duration_cast<std::chrono::nanoseconds>(Saat::now() - bas_).count();
    }
    std::int64_t gercek() const
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(Saat::now() - bas_).count();
    }

    void gunluk(int duzey, const std::string& m) const
    {
        if (a_.gunluk >= duzey) {
            std::fprintf(stderr, "ida_sim: %s\n", m.c_str());
        }
    }

    // ---- G/Ç ----
    // Gönderim yolu (sol #7): köprünün baytları bekleyen tampona eklenir; tampon engellemeden,
    // kısmi yazımın konumu korunarak boşaltılır. Döngü yazıma hiçbir zaman süresiz bağlanmaz:
    // ilerleme yoksa sonraki çağrıda (her bekleme uyanmasında) yeniden denenir. Yazma hatası
    // ya da T_bağ (HIL; SIL'de T_SIL) boyunca ilerlemeyen yazım bağlantı kaybıdır (A1-K23).
    void yaz()
    {
        k_->giden_al(tampon_);
        if (a_.giden_kancasi && !tampon_.empty()) {
            a_.giden_kancasi(tampon_.data(), tampon_.size());   // sim'in göndermeye karar verdiği dizi
        }
        if (!tas_) {
            cikis_.clear();
            cikis_k_ = 0;
            return;
        }
        if (!tampon_.empty()) {
            if (cikis_k_ >= cikis_.size()) {
                cikis_.clear();
                cikis_k_ = 0;
                cikis_ilerleme_ = gercek();
            }
            cikis_.insert(cikis_.end(), tampon_.begin(), tampon_.end());
            if (a_.a1_bayt_kancasi) {
                a_.a1_bayt_kancasi(true, tampon_.data(), tampon_.size());
            }
        }
        cikis_hizmet();
    }

    void cikis_hizmet()
    {
        if (sil_yazma_bitti_) {
            cikis_.clear();   // W4-K26 (2): karşı uç artık okumuyor; dizi kancada kayıtlı
            cikis_k_ = 0;
            return;
        }
        if (!tas_ || cikis_k_ >= cikis_.size()) {
            return;
        }
        const std::int64_t e0 = gercek();
        for (int j = 0; j < 8 && cikis_k_ < cikis_.size(); ++j) {
            const long long k = tas_->yaz_biraz(cikis_.data() + cikis_k_, cikis_.size() - cikis_k_);
            if (k < 0) {
                if (a_.kip == Kip::sil) {
                    // W4-K26 (2) (EMNİYET): SIL'de yazma hatası bağlantıyı burada bitirmez; alım
                    // kuyruğu akış sırasıyla işlenmeye devam eder, akışın sonu kuyruk bitince.
                    sil_yazma_bitti_ = true;
                    cikis_.clear();
                    cikis_k_ = 0;
                    return;
                }
                baglanti_koptu("yazma hatası");
                return;
            }
            if (k == 0) {
                break;
            }
            cikis_k_ += static_cast<std::size_t>(k);
            cikis_ilerleme_ = gercek();
        }
        if (cikis_k_ >= cikis_.size()) {
            cikis_.clear();
            cikis_k_ = 0;
        } else {
            const std::int64_t sinir = a_.kip == Kip::hil ? k_->ayar().t_bag_ns : a_.t_sil_ns;
            if (gercek() - cikis_ilerleme_ > sinir || cikis_.size() - cikis_k_ > (std::size_t{1} << 20U)) {
                ++s_.yazma_takilma;
                baglanti_koptu("yazma ilerlemiyor");
                return;
            }
        }
        evre_[kEvreYaz] += gercek() - e0;
    }

    // EMNİYET (W4-K28 (3), W4-K25 (5); W4-bagla-a3 sol-3 #2, #3; w4-mac-ci sol 3. denetim #3):
    // köprünün bekleyen çıkışını (ör. SESSION_CLOSE) mutlak süre sınırına (`son`, gercek()
    // ölçeğinde) dek tamamlar. Kısmi ya da 0 yazımda karşı tarafın baytları okunmaya devam eder
    // (iki yönün tamponları dolup kilitlenmesin) ve 1 ms sonra yeniden denenir. Süre dolarsa,
    // bağlantı koparsa ya da SIL'de yazma hatası olduysa açıkça sonuçlanır: sayılır, günlüğe
    // yazılır, false döner (çağıran yine kapatır). true: hepsi yazıldı ve (seri portta) işletim
    // sisteminin gönderim kuyruğu da boşaldı (uygulama tamponunun boşalması telin boşalması
    // sayılmaz). Taşıma yoksa yapılacak iş yoktur (true). Son tarih her turda denetlenir; tur
    // sınırlıdır (pompa'nın okuma işi HIL'de de sınırlı).
    bool cikisi_bitir(std::int64_t son)
    {
        if (!tas_) {
            return true;
        }
        yaz();
        while (tas_ && cikis_k_ < cikis_.size()) {
            if (gercek() >= son) {
                ++s_.kapanis_yazim_eksik;
                gunluk(1, "kapanış: bekleyen " + std::to_string(cikis_.size() - cikis_k_) +
                              " B süre sınırında yazılamadı");
                return false;
            }
            const std::size_t once = cikis_k_;
            pompa(0);   // cikis_hizmet de çağrılır
            if (tas_ && cikis_k_ == once && cikis_k_ < cikis_.size()) {
                sure_uyu(kMs);
            }
        }
        if (!tas_ || sil_yazma_bitti_) {
            ++s_.kapanis_yazim_eksik;
            return false;
        }
        if (!tas_->cikis_bosalt(std::max<std::int64_t>(son - gercek(), 0) / 1000)) {
            ++s_.kapanis_yazim_eksik;
            gunluk(1, "kapanış: işletim sisteminin gönderim kuyruğu süre sınırında boşalmadı");
            return false;
        }
        return true;
    }

    // W4-sunucu-kucuk (kapsam 1): SIGINT/SIGTERM (Windows: konsol denetim olayı) bayrağı. Bayrak
    // görülünce koşu var olan hata kapanışıyla biter (ikinci bir kapanış yolu yok): `hata` ayrılmışsa
    // ona dokunulmaz; bekleme döngüleri ve sınır turu bunu her turda yoklar. true: kesildi.
    bool sinyal_kesti()
    {
        if (!kapanis_istendi()) {
            return false;
        }
        if (!s_.sinyal_kesti) {
            s_.sinyal_kesti = true;
            gunluk(1, "kapanış sinyali alındı: düzgün kapanış");
        }
        if (s_.hata.empty()) {
            s_.hata = "sinyalle kesildi";
        }
        return true;
    }

    // Oturum kurulamadan ya da koşu başlamadan biten yolların kapanışı (W4-K28 (3); hazırlıkta
    // durdurma, ön denetim ya da hazırlık kalması, bağlantı beklemesinin iptali; sol 3. denetim
    // #3, A1-K21): oturum açıksa (hazir, kosuyor) SESSION_CLOSE(SHUTDOWN) sıraya girer (köprüde
    // oturum biter: izinler kapanır, iticiler nötr); bekleyen çıkış mutlak süre sınırıyla
    // tamamlanır; sonra taşıma düzgün kapanır.
    // Sol 4. denetim #3: SESSION_CLOSE'dan sonra yerel daraltma gerçekten uygulanır (izinler
    // kapanır, fizik girdisi nötr; kapanis_daraltmasi).
    // W5-K59 (A1-K128): dünya `sifirla`/`senaryo/yukle` ile sonlanıyorsa (süreç sürer, çalıştırma nesli
    // değişir) oturum RUN_END ile kapanır; öteki her kapanış bugünkü gibi SHUTDOWN'dur.
    std::uint8_t a1_kapanis_nedeni() const { return a3_yeniden_ ? kCloseRunEnd : kCloseShutdown; }
    // W5-K55: bu dünyanın başlangıç emniyet revizyonu ve doyma bayrağı (süreç boyunca yaşayan değer).
    std::uint32_t ilk_emniyet_rev() const { return kalici_ != nullptr ? kalici_->emniyet_rev : a_.sinama_ilk_emniyet_rev; }
    bool ilk_emniyet_rev_doydu() const
    {
        return kalici_ != nullptr ? kalici_->emniyet_rev_doydu : a_.sinama_ilk_emniyet_rev == 0xFFFFFFFFu;
    }
    void erken_kapanis()
    {
        if (!k_) {
            return;
        }
        if (k_->durum() == kopru::OturumDurumu::kosuyor || k_->durum() == kopru::OturumDurumu::hazir) {
            k_->oturum_kapat(a1_kapanis_nedeni(), duvar());
        }
        kapanis_daraltmasi();
        (void)cikisi_bitir(gercek() + kKapanisYazimNs);
        if (tas_) {
            tas_->kapat_duzgun(kKapanisBosaltmaUs);
            tas_.reset();
        }
    }

    // Sol 4. denetim #3 (EMNİYET; SIL ve HIL): kapanışta köprünün yetkili durumunda izinler
    // kapatılır ve yürürlükteki itki silinir (Kopru::kapanis_daralt; sil_kisitlayan_uygula'nın
    // oturum sonu dalıyla aynı yardımcı), fiziğin itici girdisi nötrlenir. Yeni kart çıktısı
    // tüketilmez, fizik ilerlemez; CSV'nin son satırı değişmez, sonuç Sonuc::kapanis_* alanlarında.
    void kapanis_daraltmasi()
    {
        if (!k_) {
            return;
        }
        s_.kapanis_oncesi_itki = k_->itki_yururlukte();
        s_.kapanis_oncesi_izin = k_->izin_acik(0) || k_->izin_acik(1);
        const kopru::KisitSonucu d = k_->kapanis_daralt();
        pwm_yur_ = d.pwm_us;
        (void)dn_.itici_komutu_ver(dn_.adim_no(), d.pwm_us);
        s_.kapanis_daraltildi = true;
        s_.kapanis_izin_acik = k_->izin_acik(0) || k_->izin_acik(1);
        s_.kapanis_itki_yururlukte = k_->itki_yururlukte();
    }

    // Sol 4. denetim #1 (EMNİYET): oturumun bütün yaşamını kapsayan kapanış koruyucusu. Kapsamdan
    // her çıkışta (istisna dahil) kapanış yapılmadıysa ortak güvenli kapanışı (erken_kapanis)
    // çağırır; kendisi bellek ayırmaz ve istisna fırlatmaz, kapanış içindeki istisnalar yutulur ve
    // sayılır. Olağan bitiş ve açık erken kapanış `yapildi()` ile işaretler (çift kapanış yok).
    class KapanisKoruyucu {
    public:
        explicit KapanisKoruyucu(Calistirici& c) noexcept : c_(c) {}
        KapanisKoruyucu(const KapanisKoruyucu&) = delete;
        KapanisKoruyucu& operator=(const KapanisKoruyucu&) = delete;
        ~KapanisKoruyucu() noexcept
        {
            if (yapildi_) {
                return;
            }
            try {
                c_.erken_kapanis();
            } catch (...) {
                ++c_.s_.kapanis_istisna;
            }
        }
        void yapildi() noexcept { yapildi_ = true; }

    private:
        Calistirici& c_;
        bool yapildi_ = false;
    };

    // Bağlantı kaybı (A1-K23): oturum biter (iticiler sonraki sınırdan nötr, izinler kapanır;
    // fizik sürer), taşıma kapatılır; HIL'de port 1 s aralıkla yeniden açılmaya çalışılır.
    void baglanti_koptu(const char* neden)
    {
        gunluk(1, std::string("bağlantı kaybı: ") + neden);
        ++s_.baglanti_kopma;
        k_->akis_kapandi(duvar());
        if (tas_) {
            tas_->kapat();
            tas_.reset();
        }
        cikis_.clear();
        cikis_k_ = 0;
        sil_gelen_temizle();
        sonraki_acma_ = gercek() + kYenidenAcma;
    }

    // HIL: taşıma yoksa portu 1 s aralıkla yeniden açar; açılınca yeni el sıkışma (A1-K23, H29).
    void baglanti_bakim();
    std::unique_ptr<kopru::Tasima> port_ac(std::string& hata);

    // Eldeki baytları alır; ilk okuma en çok bekle_us bekler. HIL'de baytlar hemen köprüye
    // verilir. SIL'de (A1-K121, K102) yalnız alım kuyruğuna eklenir ve köprüye sil_besle() verir:
    // çerçeveler yalnız sim'in beklediği noktalarda (yuva bekleme, el sıkışma), akıştaki sırayla
    // ve bekleme koşulu sağlandığı çerçevede durularak işlenir. Böylece oturumun bitiş anı,
    // kapanış çerçevesi ve yeniden başlama anı baytların geliş zamanına bağlı olmaz. Akışın
    // kapanması da (karşı uç kapattı) akıştaki yerinde, kuyruk bitince işlenir.
    void pompa(std::int64_t bekle_us)
    {
        const bool sil = a_.kip == Kip::sil;
        if (ic_ && sil) {
            (void)ic_->pompala();
            bekle_us = 0;
        }
        cikis_hizmet();
        if (!tas_ || (sil && sil_akis_bitti_)) {
            return;
        }
        alim_.resize(16384);   // yığında değil (MSVC C6262)
        const std::int64_t e0 = gercek();
        std::size_t bu_tur = 0;
        for (;;) {
            std::size_t istek = alim_.size();
            if (!sil) {
                // W4-K28 sol-3 #2: HIL'de de bir turun işi sınırlı (sürekli veri basan karşı taraf
                // dünya döngüsünü ve kapanışın son tarih denetimini kilitleyemez). Geri basınç YOK:
                // HIL'de okumamak veri kaybıdır; kalan baytlar sonraki turda okunur.
                if (bu_tur >= kHilOkumaTuru) {
                    break;
                }
            } else {
                // W4-K26 (3): bir turun işi sınırlı (gözetim denetimine dönülür) ve kuyruğun
                // bellek tavanı her eklemeden önce: tavan doluysa okunmaz (geri basınç; TCP
                // karşı tarafı yavaşlatır). Tavan karar üretmez: kuyruğun doluluğu geliş
                // zamanına bağlıdır; protokol hatası yalnız akış konumuna bağlı tüketim
                // sınırından (yuva_bekle, kSilBeklemeAzami) gelir.
                const std::size_t dolu = sil_gelen_.size() - sil_gelen_k_;
                if (bu_tur >= kSilOkumaTuru || dolu >= kSilGelenAzami) {
                    break;
                }
                istek = std::min(istek, kSilGelenAzami - dolu);
            }
            std::size_t n = 0;
            const kopru::OkumaSonucu r = tas_->oku(alim_.data(), istek, bekle_us, n);
            bekle_us = 0;
            if (r == kopru::OkumaSonucu::tamam) {
                if (a_.a1_bayt_kancasi) {
                    a_.a1_bayt_kancasi(false, alim_.data(), n);
                }
                if (sil) {
                    bu_tur += n;
                    sil_gelen_.insert(sil_gelen_.end(), alim_.data(), alim_.data() + n);
                    s_.sil_kuyruk_en_cok = std::max<std::uint64_t>(s_.sil_kuyruk_en_cok, sil_gelen_.size() - sil_gelen_k_);
                } else {
                    bu_tur += n;
                    k_->al(alim_.data(), n, duvar());
                }
                continue;
            }
            if (r == kopru::OkumaSonucu::kapandi) {
                if (sil) {
                    sil_akis_bitti_ = true;
                } else {
                    baglanti_koptu("taşıma kapandı (aygıt kalktı ya da karşı uç kapattı)");
                }
            }
            break;
        }
        evre_[kEvreOkuma] += gercek() - e0;
    }

    // SIL: alım kuyruğunu akış sırasıyla köprüye verir; dur() doğru olunca o çerçevede durur ve
    // kalan baytlar kuyrukta kalır. En çok `azami` bayt verilir (W4-K26 (3): tüketim sınırı
    // akıştaki bayt konumunda, okuma ve besleme parçalarından bağımsız uygulanır).
    // Genel kural (EMNİYET, sol 3. denetim #1): alım kuyruğundaki her olayın (bayt tüketimi ve
    // köprünün çerçeve kararları, W4-K14 tampon taşması, bekleme bütçesinin aşımı, akışın sonu)
    // kararı akıştaki konum sırasıyla verilir; aynı turda görülen iki olaydan akışta önce olan önce
    // karara bağlanır. Akışın sonu (EOF) son bayttan sonradır: yalnız kuyruk bittiğinde, bekleyen
    // hâlâ veri istiyorsa ve bu beslemenin bütçesi tükenmemişse işlenir (bütçe tam son baytta
    // bittiyse önce çağıranın bütçe kararı verilir). Yazma hatasının kendi kararı yoktur (çıktı
    // atılır; W4-K26 (2)). Dönüş: dur().
    bool sil_besle(const std::function<bool()>& dur, std::uint64_t azami = UINT64_MAX)
    {
        std::uint64_t verilen = 0;
        while (sil_gelen_k_ < sil_gelen_.size() && verilen < azami && !dur()) {
            const std::size_t parca = static_cast<std::size_t>(
                std::min<std::uint64_t>(sil_gelen_.size() - sil_gelen_k_, azami - verilen));
            const std::size_t n = k_->al(sil_gelen_.data() + sil_gelen_k_, parca, duvar(), dur);
            sil_gelen_k_ += n;
            sil_beslenen_ += n;
            verilen += n;
        }
        if (sil_gelen_k_ >= sil_gelen_.size()) {
            sil_gelen_.clear();
            sil_gelen_k_ = 0;
        } else if (sil_gelen_k_ > (std::size_t{1} << 16U)) {
            sil_gelen_.erase(sil_gelen_.begin(), sil_gelen_.begin() + static_cast<std::ptrdiff_t>(sil_gelen_k_));
            sil_gelen_k_ = 0;
        }
        const bool tamam = dur();
        if (!tamam && sil_akis_bitti_ && sil_gelen_.empty() && verilen < azami) {
            baglanti_koptu("taşıma kapandı (karşı uç kapattı)");
        }
        return tamam;
    }

    void sil_gelen_temizle()
    {
        sil_gelen_.clear();
        sil_gelen_k_ = 0;
        sil_akis_bitti_ = false;
        sil_yazma_bitti_ = false;
    }

    // HIL: hedef duvar anına kadar okuyup bekler. Okuma engelsizdir; arada mutlak son tarihe
    // göre kısa uykular (≤ 0,2 ms; yüksek çözünürlüklü), son 0,15 ms döndürülerek beklenir.
    // select'in uzun beklemesi kullanılmaz: ölçümde uyanma gecikmesinin kaynağıydı.
    void bekle_ve_oku(std::int64_t hedef)
    {
        for (;;) {
            const std::int64_t simdi = gercek();
            k_->zamanla(simdi);
            yaz();
            if (simdi >= hedef) {
                return;
            }
            pompa(0);
            const std::int64_t kalan = hedef - gercek();
            if (kalan > 150'000) {
                sure_uyu(std::min<std::int64_t>(kalan - 150'000, 200'000));
            }
        }
    }

    // Bekleme sonucu: tamam; oturum bitti (çağıran yeniden kurar, özyineleme yok); hata (s_.hata).
    enum class Bekleme { tamam, oturum_bitti, hata };
    bool baglanti_kabul(std::int64_t son_mutlak = std::numeric_limits<std::int64_t>::max());
    bool oturum_kur();
    Bekleme hazirlik(std::int64_t son_mutlak);
    Bekleme yuva_bekle(std::uint32_t seq, std::int64_t son_mutlak = std::numeric_limits<std::int64_t>::max());
    bool dosya_plani();
    bool plan_hazirla(const sensor::SensorKumesiAyari& sa);
    void plan_sinir(std::uint64_t b);
    void plan_sinir_sonrasi(std::uint64_t b, const kopru::SinirSonucu& ss);
    void gorev_hizmet(std::int64_t t);
    void olay_sonucu(std::size_t i, const char* sonuc, std::int64_t t, const char* neden = "");
    // W4-duraklat (W4-K28 (2); EMNİYET): sınır b'nin bir turu. İlerleyen kipte (duran = false)
    // olağan sınır işlemi; duran kipte AYNI işlev, aynı sıra; atlanan yalnız (1) senaryonun zaman
    // çizelgesi, (2) fizik adımı ve sim zamanını ilerleten işler (sensör ve sonar adımı, parti),
    // (3) üye gönderimi (köprünün duran kipi). Fizik adımı turdan sonra, çağıranda; yalnız
    // çalıştırma duraklatılmış değilken. false: döngü biter (durdur ya da hata).
    bool sinir_turu(std::uint64_t b, bool duran);
    // Turun sonunda duraklatmanın girişi ve çıkışı. true: sınır b'nin sonraki turu duran kipte.
    bool duraklatma_gecisi(std::uint64_t b, bool duran);
    bool sinir_on(std::uint64_t b, bool duran);
    void adim_sonrasi(std::uint64_t b);
    void satir_ekle(std::uint64_t b, std::optional<std::uint32_t> seq);
    void satir_tamamla(Satir& r);
    void csv_yaz(const Satir& r);
    void evre_ozeti_kur();

    // ---- A3 (W4-bagla-a3) ----
    enum class BaslatBekleme : std::uint8_t { baslat, sinyal, yeniden };
    bool a3_baslat();
    BaslatBekleme a3_baslat_bekle();
    // W5-a3-senaryo: `sifirla` / `senaryo/yukle` kabulü (dünya sonlanır, sıradaki dünya istenir), süreklilik
    // beklemesi (çalıştırma bitti; sunucu ayakta, yeni komut beklenir), yeni dünyanın ön denetim kaydı ve
    // `senaryo/yukle` sonucunun verilmesi.
    void a3_yeniden_kabul(const A3Giris& g, std::uint64_t b);
    bool a3_onde_kisit_var(std::uint32_t komut_no) const;
    bool a3_ertelenen_yeniden_bitir(std::uint64_t b);
    void a3_emniyet_baslangic_yayinla();
    bool a3_bosta_bekle();
    void a3_kalanlari_reddet();
    void a3_sunucuyu_durdur();
    void a3_yukleme_sonucu();
    void a3_on_denetim_kaydet();
    bool a3_yeniden_iste_yeniden();
    bool a3_devam_edecek() const;
    void a3_cek(std::uint16_t atla);
    bool a3_run_uyar(const a3::Komut& k) const;
    bool a3_gevsetme_onu_acik(const A3Giris& g);
    void a3_calistirma_isle(std::uint64_t b, bool ertele = false);
    bool a3_sinir_basi(std::uint64_t b, bool duran);
    bool a3_hazirlik_durdu(bool kanca = true);
    void a3_emniyet_ver(std::uint64_t b);
    void a3_sinir_sonrasi(std::uint64_t b, const kopru::SinirSonucu& ss, bool duran);
    void a3_sonuclari_isle(std::uint64_t b, const kopru::OperatorSonucu* sonuc, std::size_t n,
                           const sensor::EmniyetDurumu& once);
    void a3_yi_uygula(std::uint64_t b, const A3Giris& g, const kopru::SinirSonucu* ss);
    void a3_yi_izle();
    void a3_yayinla(std::uint64_t b, bool duran);
    void a3_sensor_yayinla();
    void a3_durum_yayinla(std::int64_t t);
    void a3_gecerlilik(std::uint64_t b);
    void a3_oturum_izle();
    void a3_olay_kararlari();
    void a3_ariza_izle(std::uint64_t b);
    void a3_gecis(a3::CalistirmaDurumu yeni, const char* neden, bool ic);
    void a3_sonuc(const A3Giris& g, bool uygulandi, std::uint64_t b, const char* kod, std::string_view ayrinti = {});
    void a3_olay(std::uint16_t tur, std::uint8_t onem, std::uint8_t kaynak, std::uint32_t komut_no,
                 std::uint32_t istemci, std::string_view ayrinti);
    void a3_senaryo_ogesi(std::size_t i, const char* sonuc, std::int64_t t, const char* neden);
    void a3_emniyet_olaylari(std::uint64_t b, const sensor::EmniyetDurumu& once);
    void a3_anlik_guncelle();
    void a3_kayitlari_aktar();
    void a3_bitir();
    template <typename F>
    void a3_olc(F&& f);

    // ---- A2 (W5-bagla-a2) ----
    bool a2_baslat();
    void a2_kapat(Sonuc& r, bool son) noexcept;
    void sil_hiz_bekle(std::uint64_t b);

    // ---- A6 kayıt (W5-bagla-kayit) ----
    // `kb_` null: kayıt kapalı (W5-K40: `--kayit-dizini` yok) → bugünkü davranış, hiçbir kayıt çağrısı yok.
    // Tek yapı, iki hedef (W5-K42): A3'e giden yük aynen kayda da verilir; A3 kapalıyken yalnız kayda.
    bool kayit_baslat();
    void kayit_kapat(Sonuc& r) noexcept;
    void kayit_yayinla(std::uint64_t b);                 // A3 kapalıyken yayın noktası (dünya durumundan gelen kanallar)
    void kayit_denetim();                                // 1 Hz (duvar): taslak, sayaç ve durum olayları
    void kayit_taslak_hemen();                           // güvenceyi değiştirebilecek bir komut uygulandı (A6-K53)
    kayit::KapanisAlanlari kayit_alanlari(kayit::BitisDurumu bitis) const;
    void kayit_oge(std::size_t i, const char* sonuc, std::int64_t t, const char* neden);
    std::uint64_t kayit_dusen_() const;
    void fizik_yayinla(std::uint64_t b);
    void kart_yayinla(std::uint64_t b);
    void cekirdek_yayinla(std::uint64_t b);
    template <kayit::Kanal K, typename Y>
    void kayit_yaz(std::int64_t t, std::uint64_t adim, const Y& y)
    {
        const auto bas = Saat::now();
        kb_->yaz<K>(t, adim, y);
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(Saat::now() - bas).count();
        ++s_.kayit.cagri_adet;
        s_.kayit.cagri_toplam_ns += ns;
        s_.kayit.cagri_en_kotu_ns = std::max<std::int64_t>(s_.kayit.cagri_en_kotu_ns, ns);
    }
    // A3'e giden yapı iki hedefe de (A3 açıksa A3'e, kayıt açıksa kayda): ayrı alan doldurma kodu yok (W5-K42).
    template <a3::Kanal KA, kayit::Kanal KK, typename Y>
    void yayin(std::int64_t t, std::uint64_t adim, const Y& y)
    {
        static_assert(std::is_same_v<Y, typename a3::KanalYuku<KA>::tip> &&
                          std::is_same_v<Y, typename kayit::KanalYuku<KK>::tip>,
                      "A3 ve A6 yükü aynı tip olmalı (W5-K36, K42)");
        if (yay_ != nullptr && !a3_sessiz_) {
            a3_olc([&] { (void)yay_->yayinla<KA>(t, y); });
        }
        if (kb_) {
            kayit_yaz<KK>(t, adim, y);
        }
    }

    Ayar a_;
    Sonuc s_;
    Plan plan_;
    struct PlanDurumu {
        bool basladi = false;
        std::int64_t tetik_b = -1;   // koşul sağlandı: bu sınırda başlar (A5-K63)
        std::int64_t baslangic_ns = 0;   // başladığı sınır (A3 ariza/etkin)
        bool reddedildi = false;         // başlatılamadı (sensör arızası reddi)
        // W4-bagla-sonar: koşullu sonar arızası, sonar adımı önceden hesaplandığı için başlayacağı
        // sınırın adımından önce (önceki sınırın sonunda) sonar kümesine verilir; sonuç burada.
        bool sonar_verildi = false;
        sensor::ArmSonucu sonar_sonuc{};
    };
    std::vector<PlanDurumu> pd_;
    struct IticiAriza {
        std::uint8_t maske = 0;
        double carpan = 1.0;
        std::int64_t bitis_ns = 0;   // t_b + sure_ns'e eşit ya da sonraki ilk sınırda biter (A5-K64)
        std::string kimlik;          // A5 olay kimliği (A3 ariza/etkin)
        std::int64_t baslangic_ns = 0;
    };
    std::vector<IticiAriza> itici_;
    std::vector<std::int64_t> hb_;   // yer istasyonu canlılığı (A1-K70) anları
    std::size_t hb_i_ = 0;
    std::vector<std::size_t> sinir_emniyet_;   // bu sınırın emniyet atamaları (plan indisi)
    int gorev_gonderilen_ = 0;
    std::uint32_t gorev_son_id_ = 0;
    bool gorev_bitti_ = false;
    std::array<double, 8> fizik_carpan_{1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};   // fiziğe verilmiş son itki çarpanı
    fizik::AracParametreleri arac_;
    fizik::Dunya dn_;
    sensor::SensorKumesi kume_;
    // W4-bagla-sonar: yalnız Ayar::sonar_acik ile kurulur (yoksa null; koşu bu iş öncesiyle aynı).
    std::optional<senaryo::SonarKurulumu> sonar_kurulum_;
    std::unique_ptr<sonar::SonarKumesi> sonar_;
    std::vector<sonar::SonarOlcumu> sonar_olcumler_;
    bool sonar_hedefi(const std::string& ad) const;
    bool sonar_kur_(const sensor::SensorKumesiAyari& sa);
    void sonar_adimi(std::int64_t t);
    bool sonar_onceden(std::uint64_t b);
    void sonar_csv_ekle(const sonar::SonarOlcumu& o);
    void a3_sonar_yayinla();
    std::unique_ptr<kopru::Kopru> k_;
    std::unique_ptr<kopru::Tasima> tas_;
    std::unique_ptr<kopru::TcpDinleyici> din_;
    std::unique_ptr<kopru::KarsiTaraf> ic_;
    std::unique_ptr<AltSurec> surec_;
    std::thread karsi_is_;
    std::atomic<bool> dur_{false};
    kopru::ParamBlogu pb_;
    Saat::time_point bas_ = Saat::now();
    std::int64_t sil_duvar_ = 0;
    std::vector<std::uint8_t> tampon_;
    std::vector<std::uint8_t> cikis_;       // gönderilmeyi bekleyen baytlar
    std::size_t cikis_k_ = 0;               // cikis_'te yazılmış önek
    std::int64_t cikis_ilerleme_ = 0;       // son ilerleme anı (gercek)
    std::int64_t sonraki_acma_ = 0;         // HIL: sonraki yeniden açma denemesi (gercek)
    static constexpr std::int64_t kYenidenAcma = 1'000'000'000;   // A1-K23: 1 s
    bool pty_cikarildi_ = false;
    bool pty_takildi_ = false;
    std::vector<std::uint8_t> alim_;
    std::vector<std::uint8_t> sil_gelen_;   // SIL: alınmış, köprüye henüz verilmemiş baytlar
    std::size_t sil_gelen_k_ = 0;           // sil_gelen_'de verilmiş önek
    bool sil_akis_bitti_ = false;           // SIL: karşı uç kapattı (kuyruktan sonra işlenir)
    bool sil_yazma_bitti_ = false;          // SIL: yazım hata aldı (W4-K26 (2); bağlantı kuyrukta biter)
    std::uint64_t sil_beslenen_ = 0;        // SIL: köprüye verilen toplam bayt
    static constexpr std::size_t kSilGelenAzami = std::size_t{1} << 26U;   // 64 MiB (W4-K26 (3))
    static constexpr std::size_t kSilOkumaTuru = std::size_t{1} << 20U;    // pompa turu başına en çok 1 MiB
    // W4-K28 sol-3 #2: HIL pompa turu başına en çok 256 KiB (3 Mbaud hatta ≈ 0,85 s; olağan
    // akışta bir periyotta bir kaç KiB gelir, sınır yalnız bozuk/sürekli akışta devreye girer;
    // kalan baytlar işletim sisteminin tamponunda kalır, sonraki pompada okunur).
    static constexpr std::size_t kHilOkumaTuru = std::size_t{1} << 18U;
    // W4-K26 (3): bir yuva beklemesinde, yuva tamamlanmadan işlenebilecek en çok bayt (kartın bir
    // çağrısının meşru çıktısı birkaç KiB; W4-K14'ün 128 çerçevelik sınırının bayt karşılığından çok
    // büyük). Aşılırsa oturum protokol hatasıyla biter (akış sırasına bağlı, belirlenimci).
    static constexpr std::uint64_t kSilBeklemeAzami = std::uint64_t{1} << 22U;   // 4 MiB
    // W4-K25 (5): düzgün kapanışta karşı tarafın kapatması en çok bu kadar beklenir (TCP).
    static constexpr std::int64_t kKapanisBosaltmaUs = 500'000;
    // W4-K28 (3): kapanışta bekleyen çıkışın tamamlanması için mutlak süre sınırı.
    static constexpr std::int64_t kKapanisYazimNs = 500'000'000;
    // Sol 3. denetim #2: bir oturum kurma işleminde en çok hazırlık denemesi (tahmin; olağan
    // hazırlık bir denemede biter, yeniden açılış ve kopmalar için pay).
    static constexpr int kHazirlikDenemeAzami = 5;
    std::uint64_t oturum_kur_derinlik_ = 0;
    std::array<std::int64_t, kEvreSayisi> evre_{};
    std::vector<std::array<std::int64_t, kEvreSayisi>> profil_;   // HIL: sınır başına
    std::vector<std::pair<std::int64_t, std::uint64_t>> tf_gec_;   // HIL: (TF geç yazım, sınır)
    std::vector<sensor::Olcum> olcumler_;
    std::array<double, 8> pwm_yur_{};
    Ayar::SinirGozlemi gozlem_;   // bu sınırın fiziğe verilen girdisi (kancaya adımdan önce)
    std::array<double, 8> son_pwm_yur_{};
    std::string pty_yol_;
    bool estop_bu_adim_ = false;
    bool onceki_estop_ = false;       // satırın adımının (t − h, t] acil durdurma durumu
    std::uint32_t id_arm_ = 0, id_kip_ = 0, id_sp_ = 0, id_sp2_ = 0, id_disarm_ = 0;
    std::int64_t t_disarm_istek_ = -1;
    std::deque<std::size_t> bekleyen_satir_;   // kart tarafı doldurulmamış satırlar (sırayla)
    void satirlari_tamamla(std::size_t birak)
    {
        while (bekleyen_satir_.size() > birak) {
            satir_tamamla(s_.satirlar[bekleyen_satir_.front()]);
            bekleyen_satir_.pop_front();
        }
    }
    std::int64_t son_tf_yazim_ = -1;
    std::int64_t w0_ = 0;
    std::vector<std::int64_t> tf_yazim_;      // TF yazım başlangıçları (gercek)

    // ---- W4-duraklat ----
    std::optional<std::uint64_t> adimla_hedef_;   // adımlamanın yeniden duraklatacağı sınır (A3-K65)
    std::int64_t duraklama_duvar_ = 0;            // duraklatmanın yürürlüğe girdiği an (gercek)
    std::int64_t duran_yayin_duvar_ = 0;          // duran kipte son durum yayını (gercek)
    std::uint64_t duran_tur_ = 0;                 // duran kip turları (çalıştırma boyunca; ölçüm)
    static constexpr std::int64_t kDuranTurNs = 1'000'000;   // duran kip turunun bekleme payı (1 ms)
    // Bir duraklatmada köprüye verilen (tele kip kalkınca çıkacak) emniyet dışı yer istasyonu komutu.
    std::size_t duran_yi_verilen_ = 0;
    static constexpr std::size_t kDuranYiAzami = 256;   // A3-K59'un kuyruk boyu (mimari seçimi)

    // ---- A6 kayıt (W5-bagla-kayit) ----
    std::unique_ptr<KayitBaglanti> kb_;
    std::optional<KayitGirdisi> kayit_girdi_;     // senaryodan (dosya_plani); yalnız kayıt açıkken
    std::uint64_t yayin_b_ = 0;                   // yayın noktasının sınırı (A3 kapalıyken a3_b_ ilerlemez)
    a3::yuk_sayac_gercek_zaman kayit_son_sayac_;  // kapanış kaydının `sayaclar` alanı
    sensor::EmniyetDurumu kayit_emn_;             // emniyet girdisi değişim denetimi (A3 kapalıyken)
    bool kayit_emn_var_ = false;
    std::int64_t kayit_denetim_duvar_ = 0;        // 1 Hz denetimin son anı (gercek)
    std::uint64_t kayit_eksik_onceki_ = 0;        // yazıcının `kayit/eksik` kaydı sayısı (olay için)
    std::uint64_t kayit_dusen_onceki_ = 0;
    kayit::YaziciDurumu kayit_durum_onceki_ = kayit::YaziciDurumu::acik;
    bool kayit_kapandi_ = false;

    // ---- A2 (W5-bagla-a2) ----
    // null: A2 kapalı (W5-K31: `--a2-port` yok, görünüm kapalı ya da tanım kurulamadı) → bugünkü
    // davranış. Dolu ama A2'siz (kilit/port alınamadı) olabilir: yayın çağrıları hemen döner.
    // A3 açıkken sunucu A3Kalici'dedir (dünyalar arasında yaşar); kapalıyken a2_sahip_'tedir.
    A2Baglanti* a2_ = nullptr;
    std::unique_ptr<A2Baglanti> a2_sahip_;
    a2b::Tanim a2_tanim_;                         // senaryodan (dosya_plani)
    a2b::TanimSonucu a2_tanim_sonucu_ = a2b::TanimSonucu::hata;
    std::string a2_tanim_hatasi_;

    // ---- A3 (W4-bagla-a3) ----
    // W4-K27 (5): dünya tarafındaki A3 depoları A3 başlarken ayrılır; hizmet sırasında bellek
    // ayrılmaz, kilit alınmaz.
    // A3 sunucusu ve komut alıcısı süreç boyunca yaşar (A3Kalici); dünya yalnız işaret eder.
    A3Kalici* kalici_ = nullptr;
    A3Alici* a3al_ = nullptr;
    a3::Sunucu* a3sv_ = nullptr;
    a3::Yayinci* yay_ = nullptr;                 // null: A3 kapalı (bugünkü davranış)
    // W5-a3-senaryo: bu dünya `sifirla` ya da `senaryo/yukle` ile sonlandırıldı; sıradaki dünya istendi.
    std::optional<A3Kalici::Istek> a3_yeniden_;
    // W5-K56 (EMNİYET): kabul edilmiş ama etkisi bu sınırın emniyet adımının sonuna ertelenmiş `sifirla` /
    // `senaryo/yukle` (aynı sınırda çekilmiş, komut numarası daha küçük kısıtlayan istek var). Yalnız
    // a3_sinir_basi'nde dolar; aynı turda sinir_on'da (a3_ertelenen_yeniden_bitir) sonuçlanır.
    std::optional<A3Giris> a3_yeniden_ertelenen_;
    bool a3_kuruldu_ = false;                    // `a3_baslat` tamam (`yuklendi` yayınlandı)
    // `sifirla`/`senaryo/yukle` kabul edildi: bu dünya artık A3'e veri yayınlamaz (kayıt dosyasına son
    // sayaçlar yine yazılır); eski dünyanın bayat kareleri yeni dünyanın `yuklendi` durumunun üstüne binmez.
    bool a3_sessiz_ = false;
    std::string a3_ret_ileti_;                   // senaryo/yukle ret nedeni (hizmete iletilir)
    std::string kanonik_;                        // çözümlenmiş senaryonun kanonik baytları (A3-K80; yalnız A3 açıkken)
    struct A3Durum {                              // dünyanın yetkili çalıştırma durumu (A3 Tablo 4.7a)
        a3::CalistirmaDurumu durum = a3::CalistirmaDurumu::yuklendi;
        bool run_var = false;
        std::uint64_t durum_rev = 1;
    };
    A3Durum a3d_;
    std::string a3_run_id_;                       // A3-K63 metin kimliği (çalıştırma başlamadan üretilir)
    AnlikPod a3p_;                                // anlık görüntünün dünya tarafı
    bool a3_anlik_kirli_ = false;
    bool a3_durum_kirli_ = false;                 // durum kanalları sonraki yayın noktasında
    std::vector<A3Giris> a3_kisit_, a3_genel_;
    std::vector<A3Giris> a3_emniyet_bekleyen_;    // köprüye verilen operatör emniyet istekleri
    std::vector<A3Giris> a3_degisen_;             // yetkili durumu değiştiren operatör istekleri
    std::vector<kopru::EmniyetAtama> a3_atama_;   // sınırın operatör emniyet atamaları
    std::vector<kopru::OperatorSonucu> a3_op_sonuc_;
    bool a3_durdur_ = false;
    bool a3_on_denetim_kaldi_ = false;            // A3-K62: ön denetim kaldı, `baslat` reddedildi (kayıt bitis_durumu)
    double hiz_ = 0.0;                            // SIL hız çarpanı (A3-K66; 0: olabildiğince hızlı)
    std::int64_t hiz_t0_sim_ = 0;
    Saat::time_point hiz_t0_duvar_{};
    std::uint64_t a3_b_ = 0;                      // son işlenen sınır (olayların adım numarası)
    struct YiIzi {
        std::uint32_t komut_no = 0;
        std::uint32_t istemci = 0;
        const char* ad = "";
        std::uint32_t cmd_id = 0;
        std::uint16_t param_id = 0;   // param_get
    };
    std::array<YiIzi, 256> a3_yi_{};
    std::size_t a3_yi_n_ = 0;
    std::array<std::uint64_t, 4096> a3_olay_sira_{};   // A6-K41 normallestirilmis_sira
    std::size_t a3_olay_sira_n_ = 0;
    std::array<std::uint8_t, 2> a3_tup_son_olay_{};
    std::vector<kopru::OlayKarari> a3_kararlar_;
    std::uint64_t a3_son_degisim_adimi_ = std::numeric_limits<std::uint64_t>::max();
    std::uint8_t a3_son_degisim_kaynagi_ = 0;     // 0 başlangıç, 1 senaryo, 2 operatör
    std::uint32_t a3_gecerlilik_ = 0;             // Tablo 4.8 bitleri (yalnız eklenir)
    bool a3_gecerli_ = true;
    bool a3_rt_kacirdi_ = false;
    kopru::OturumDurumu a3_a1_onceki_ = kopru::OturumDurumu::kapali;
    bool a3_a1_kopuk_ = false;
    std::uint32_t a3_boot_onceki_ = 0;
    bool a3_boot_var_ = false;
    std::uint32_t a3_nav_seq_ = 0, a3_durum_seq_ = 0;
    std::int64_t a3_nav_t_ = 0;
    bool a3_nav_var_ = false, a3_durum_var_ = false;
    std::uint64_t a3_kacirma_adet_ = 0, a3_kacirma_ilk_ = 0, a3_kacirma_son_ = 0;
    std::int64_t a3_kacirma_olay_duvar_ = 0;
    struct A3Ariza {                              // senaryonun arıza olayı (önceden hazırlanmış)
        std::size_t plan_i = 0;
        std::string hedef;
        const char* tur = "";
        std::uint8_t sinif = 0;
        bool aktif = false;
    };
    std::vector<A3Ariza> a3_arizalar_;
    std::vector<a3::yuk_ariza_etkin_arizalar> a3_ariza_yuk_;
    std::size_t a3_celiski_n_ = 0, a3_gevsetme_n_ = 0;
    std::uint32_t a3_celiski_kimligi_ = 0;
    bool a3_doydu_bildirildi_ = false;
    std::int64_t a3_carpan_ref_duvar_ = 0, a3_carpan_ref_sim_ = 0;
    float a3_gerceklesen_ = std::numeric_limits<float>::quiet_NaN();
    std::int64_t a3_sinir_ns_ = 0;               // bu sınırdaki bütün A3 hizmeti (ölçüm)
    sensor::EmniyetDurumu a3_emn_once_{};         // sınır işleminden önceki yetkili emniyet durumu
    std::optional<A3Giris> a3_baslat_komutu_;     // `baslat` (a3_bekle): sonucu ön denetimden sonra
    std::uint32_t a3_son_komut_no_ = 0;
    bool a3_kapaniyor_ = false;
    struct A3KomutKaydi {
        std::uint32_t komut_no = 0;
        a3::KomutTuru tur = a3::KomutTuru::calistirma_baslat;
        const char* yi_ad = nullptr;
        std::int64_t t_ns = 0;
        bool uygulandi = false;
        const char* kod = nullptr;
    };
    std::vector<A3KomutKaydi> a3_komut_kaydi_;
    struct A3EmniyetKaydi {
        std::int64_t t_ns = 0;
        const char* alan = "";
        bool onceki = false, sonraki = false;
        bool operator_ = false;
        std::uint32_t komut_no = 0;
        std::uint32_t emniyet_rev = 0;
    };
    std::vector<A3EmniyetKaydi> a3_emniyet_kaydi_;
};

std::unique_ptr<kopru::Tasima> Calistirici::port_ac(std::string& hata)
{
    if (a_.karsi == Karsi::seri) {
        return kopru::seri_ac(a_.seri, kopru::SeriAyar{}, hata);
    }
    if (a_.karsi != Karsi::pty) {
        hata = "yeniden açılacak port yok";
        return nullptr;
    }
    // Kartsız sınama: takma anı gelmeden "aygıt" yoktur; gelince yeni bir sözde uçbirim açılır
    // ve kart rolündeki karşı taraf ana uca bağlanır.
    if (!pty_takildi_) {
        if (a_.pty_takma_s < 0.0 || static_cast<double>(dn_.zaman_ns()) * 1e-9 < a_.pty_takma_s) {
            hata = "aygıt yok";
            return nullptr;
        }
        kopru::SozdeUcbirim su = kopru::sozde_ucbirim_ac(hata);
        if (!su.ana) {
            return nullptr;
        }
        pty_yol_ = su.yol;
        if (karsi_is_.joinable()) {
            dur_ = true;
            karsi_is_.join();
        }
        ic_->tasima_degistir(std::move(su.ana));
        dur_ = false;
        karsi_is_ = std::thread([this] { ic_->calistir([this] { return dur_.load(); }); });
        pty_takildi_ = true;
    }
    kopru::SeriAyar sea;
    sea.hiz_zorunlu = false;
    return kopru::seri_ac(pty_yol_, sea, hata);
}

void Calistirici::baglanti_bakim()
{
    if (a_.kip != Kip::hil) {
        return;
    }
    // Kartsız sınama: aygıtın kalkması (ana uç kapanır; sim bunu okumada görür).
    if (a_.karsi == Karsi::pty && a_.pty_cikarma_s >= 0.0 && !pty_cikarildi_ &&
        static_cast<double>(dn_.zaman_ns()) * 1e-9 >= a_.pty_cikarma_s) {
        pty_cikarildi_ = true;
        dur_ = true;
        if (karsi_is_.joinable()) {
            karsi_is_.join();
        }
        ic_->tasima_degistir(nullptr);
        gunluk(1, "sınama: aygıt çıkarıldı");
    }
    if (tas_ || gercek() < sonraki_acma_) {
        return;
    }
    sonraki_acma_ = gercek() + kYenidenAcma;
    ++s_.port_acma_denemesi;
    std::string hata;
    tas_ = port_ac(hata);
    if (!tas_) {
        gunluk(2, "port açılamadı: " + hata);
        return;
    }
    ++s_.port_acildi;
    ++s_.yeniden_baglanti;
    gunluk(1, "port yeniden açıldı: " + tas_->ad() + "; yeni el sıkışma");
    tas_->giris_bosalt();   // A1-K12
    k_->el_sikisma_baslat(duvar());
}

bool Calistirici::baglanti_kabul(std::int64_t son_mutlak)
{
    if (a_.karsi != Karsi::tcp || !din_) {
        return tas_ != nullptr;
    }
    std::string hata;
    // Bekleme kendi payını alır; süren bir hazırlık işleminin mutlak son tarihini aşmaz.
    const std::int64_t pay = std::max<std::int64_t>(0, std::min(a_.baglanti_bekleme_ns, son_mutlak - gercek()));
    // 100 ms dilimler (toplam aynı pay): W4-K27 (4) çalıştırma denetimi (A3 açıkken) ve kapanış
    // sinyali her dilimde yoklanır. Kabul edilmiş durdurma kazanır. Pay 0 ise bir kez denenir.
    // Her dilimin süresi mutlak son tarihten hesaplanır (dilim gecikmeleri birikmez; toplam bekleme
    // pay'ı aşmaz).
    const std::int64_t kabul_son = gercek() + pay;
    for (;;) {
        if (sinyal_kesti()) {
            return false;
        }
        if (yay_ != nullptr && a3_hazirlik_durdu()) {
            s_.hata = "hazırlanırken durduruldu";
            return false;
        }
        const std::int64_t kalan_ns = kabul_son - gercek();
        tas_ = din_->kabul(std::min<std::int64_t>(std::max<std::int64_t>(kalan_ns / 1000, 0), 100'000), hata);
        if (tas_ || kalan_ns <= 100'000'000) {
            break;
        }
    }
    // Bağlantı yoksa ve son dilimde (ya da bekleme sırasında) sinyal geldiyse "zaman aşımı" değil
    // sinyaldir (select EINTR'ı 0 döndürür; çıkış kodu 5, özet `kapanış: sinyal`).
    if (!tas_ && sinyal_kesti()) {
        return false;
    }
    if (!tas_) {
        s_.hata = "SIL koşucusu bağlanmadı: " + hata;
        return false;
    }
    sil_gelen_temizle();   // A1-K99: her bağlantı yeni bir akıştır
    if (a_.tasima_sarmalayici) {
        tas_ = a_.tasima_sarmalayici(std::move(tas_));   // yalnız sınama
    }
    gunluk(2, "bağlantı: " + tas_->ad());
    return true;
}

bool Calistirici::oturum_kur()
{
    // Süre sınırı her aşamanın başında yeniden kurulur (W4-K26 mimar kararı): bağlantının kabulü
    // kendi payını alır (baglanti_kabul), el sıkışma kabulden sonra; hazırlık oturumunun süresi
    // sınıra girmez (kendi sınırı hazirlik_azami_ns), çalıştırma oturumunun el sıkışması
    // hazırlıktan sonra yeniden pay alır. SIL'de bu beklemelerde sim zamanı ilerlemez (W4-K25 (1)).
    //
    // Sol 3. denetim #2 (EMNİYET): yeniden kurma özyinelemesizdir. Hazırlık ya da yuva beklemesi
    // oturumun bittiğini döndürür (Bekleme::oturum_bitti); yeniden kurma bu döngüde yürür.
    // Bir hazırlık işlemi başlayınca mutlak son tarihi (hazirlik_azami_ns) yeniden bağlantılar
    // boyunca korunur ve bütün iç beklemelere taşınır (aşama payı onu aşamaz); hazırlık denemesi
    // de kHazirlikDenemeAzami ile sınırlıdır. Hazırlık tamamlanınca çalıştırma oturumunun el
    // sıkışması kendi payını alır.
    ++oturum_kur_derinlik_;
    s_.oturum_kur_derinlik_en_cok = std::max<std::uint64_t>(s_.oturum_kur_derinlik_en_cok, oturum_kur_derinlik_);
    struct Derinlik {
        std::uint64_t& d;
        ~Derinlik() { --d; }
    } derinlik{oturum_kur_derinlik_};
    //
    // Sol 4. denetim #2 (EMNİYET): kurma işleminin tamamı (ilk kurulum ya da koşu ortasında yeniden
    // kurma; hazırlık gerekip gerekmediğinden bağımsız) mutlak son tarihe (kurma_azami_ns) ve en
    // çok kurma_deneme_azami denemeye (bağlantı kabulleri ve el sıkışma başlangıçları) bağlıdır;
    // her iç bekleme bu son tarihi aşamaz. Aşılırsa false (çağıran ortak güvenli kapanışla biter).
    constexpr std::int64_t kSonsuz = std::numeric_limits<std::int64_t>::max();
    const std::int64_t kurma_son = gercek() + a_.kurma_azami_ns;
    int kurma_deneme = 0;
    std::int64_t hazirlik_son = kSonsuz;   // süren hazırlık işleminin mutlak son tarihi
    bool hazirlik_suruyor = false;
    int hazirlik_deneme = 0;
    const auto ust = [&] { return hazirlik_suruyor ? std::min(kurma_son, hazirlik_son) : kurma_son; };
    const auto pay = [&](std::int64_t p) { return std::min(gercek() + p, ust()); };
    const auto dene = [&] {
        if (++kurma_deneme > a_.kurma_deneme_azami) {
            s_.hata = "oturum kurulamadı (" + std::to_string(a_.kurma_deneme_azami) + " deneme)";
            return false;
        }
        return true;
    };
    const auto kabul = [&] {
        if (!dene()) {
            return false;
        }
        if (!baglanti_kabul(ust())) {
            if (gercek() >= ust()) {
                s_.hata = hazirlik_suruyor && hazirlik_son <= kurma_son ? "hazırlık oturumu süre sınırını aştı"
                                                                       : "oturum kurma süre sınırını aştı";
            }
            return false;
        }
        return true;
    };
    if (sinyal_kesti()) {
        return false;
    }
    if (!tas_ && !kabul()) {
        return false;
    }
    std::int64_t son = pay(a_.baglanti_bekleme_ns);
    // SIL (A1-K121 (iv), K23): oturum köprüde bittiyse el sıkışma orada başlamıştır (giden
    // kuyrukta varsa SESSION_CLOSE, ardından SIM_HELLO): yeniden başlatılmaz. Böylece kapanış
    // çerçevesi silinmez ve el sıkışma tek SIM_HELLO'yla tamamlanır.
    if (!dene()) {
        return false;
    }
    if (a_.kip != Kip::sil || k_->durum() != kopru::OturumDurumu::el_sikisma) {
        k_->el_sikisma_baslat(duvar());
    }
    const auto el_bitti = [this] {
        const kopru::OturumDurumu d = k_->durum();
        return d == kopru::OturumDurumu::kosuyor || d == kopru::OturumDurumu::reddedildi;
    };
    for (;;) {
        if (sinyal_kesti()) {
            return false;   // W4-sunucu-kucuk: kapanış sinyali (hata kapanışı)
        }
        if (a3_hazirlik_durdu()) {
            return false;   // W4-K27 (4)
        }
        yaz();
        if (a_.kip != Kip::sil) {
            pompa(500);
        } else if (!sil_besle(el_bitti)) {
            pompa(500);
            (void)sil_besle(el_bitti);
        }
        k_->zamanla(duvar());
        if (k_->durum() == kopru::OturumDurumu::reddedildi) {
            s_.hata = "el sıkışma reddedildi (IDAW_SESSION_RESULT " + std::to_string(k_->kart().son_oturum_reddi) + ")";
            return false;
        }
        if (k_->durum() == kopru::OturumDurumu::kosuyor) {
            if (k_->hazirlik_gerekli()) {
                if (hazirlik_deneme >= kHazirlikDenemeAzami) {
                    s_.hata = "hazırlık oturumu " + std::to_string(hazirlik_deneme) + " denemede tamamlanamadı";
                    return false;
                }
                ++hazirlik_deneme;
                if (!hazirlik_suruyor) {
                    hazirlik_suruyor = true;
                    if (hazirlik_son == kSonsuz) {
                        hazirlik_son = gercek() + a_.hazirlik_azami_ns;
                    }
                }
                // W4-sunucu-kucuk kapsam 3 (açık bulgu (a)): hazırlık beklemesi iki son tarihin
                // küçüğüyle sınırlanır (hazırlık kurma sınırını aşamaz); aşımda var olan açık hata.
                const Bekleme r = hazirlik(std::min(hazirlik_son, kurma_son));
                if (r == Bekleme::hata) {
                    if (kurma_son < hazirlik_son && s_.hata == "hazırlık oturumu süre sınırını aştı") {
                        s_.hata = "oturum kurma süre sınırını aştı";
                    }
                    return false;
                }
                if (r == Bekleme::oturum_bitti) {
                    // Hazırlık oturumu koptu: aynı hazırlık işlemi, aynı mutlak son tarihle sürer.
                    if (tas_ && k_->durum() != kopru::OturumDurumu::el_sikisma) {
                        if (!dene()) {
                            return false;
                        }
                        k_->el_sikisma_baslat(duvar());
                    }
                    son = pay(a_.baglanti_bekleme_ns);
                    continue;   // taşıma yoksa yeni bağlantı döngünün sonunda kabul edilir
                }
                hazirlik_suruyor = false;
                if (!dene()) {
                    return false;
                }
                k_->el_sikisma_baslat(duvar());
                son = pay(a_.baglanti_bekleme_ns);   // çalıştırma oturumunun el sıkışması
                continue;
            }
            return true;
        }
        if (!tas_) {
            ++s_.yeniden_baglanti;
            if (!kabul() || !dene()) {
                return false;
            }
            k_->el_sikisma_baslat(duvar());
            son = pay(a_.baglanti_bekleme_ns);   // yeni bağlantının el sıkışması
        }
        if (gercek() > son) {
            if (son >= kurma_son) {
                s_.hata = "oturum kurma süre sınırını aştı";
            } else if (hazirlik_suruyor && son >= hazirlik_son) {
                s_.hata = "hazırlık oturumu süre sınırını aştı";
            } else {
                s_.hata = "el sıkışma zaman aşımı";
            }
            return false;
        }
    }
}

// A1-K19: hazırlık oturumu: fizik yürütülmez, yalnız TF; her parametre için PARAM_SET, sonra
// PARAM_COMMIT ve PARAM_SAVE (kaynak 250); hepsi kabul edilir, kart bloğu kaydeder ve
// param_blob_crc beklenene eşit olunca RUN_END. Ardından REBOOT'lu yeni el sıkışma gelir.
// Dönüş: tamam (RUN_END gönderildi); oturum_bitti (hazırlık oturumu koptu; çağıran aynı mutlak
// son tarihle yeniden kurar); hata (s_.hata). Gözetim: en çok 8000 periyot (sim zamanı 40 s), SIL'de
// her yuva T_SIL (duvar) ve hazırlık işleminin mutlak son tarihi `son_mutlak` (duvar; yeniden
// bağlantılar boyunca korunur, yavaş yanıt veren ya da tekrar tekrar kapanan kartla asılma yok).
Calistirici::Bekleme Calistirici::hazirlik(std::int64_t son_mutlak)
{
    ++s_.hazirlik_oturumu;
    gunluk(1, "parametre bloğu uyuşmuyor: hazırlık oturumu (A1-K19)");
    std::int64_t t_h = 0;
    const std::int64_t eski_duvar = sil_duvar_;
    sil_duvar_ = 0;
    struct DuvarGeri {
        std::int64_t& d;
        std::int64_t eski;
        ~DuvarGeri() { d = eski; }
    } duvar_geri{sil_duvar_, eski_duvar};
    std::vector<std::uint32_t> idler;
    for (const kopru::ParamDegeri& p : pb_.tumu) {
        idler.push_back(k_->komut_param_set(p.id, p.type, p.bits, t_h, 250));
    }
    idler.push_back(k_->komut_basit(kCmdParamCommit, 0, t_h, 250));
    idler.push_back(k_->komut_basit(kCmdParamSave, 0, t_h, 250));
    bool tamam = false;
    for (int n = 0; n < 8000 && !tamam; ++n) {
        // Kabul edilmiş durdurma önce (W4-K27 (4), W4-K28 (3)): durdurma kazanır (Bekleme::hata;
        // s_.hata kos()'un durdurma dalında temizlenir); sonra hazırlığın mutlak son tarihi. Erken
        // dönüşlerin kapanışı (SESSION_CLOSE) kos()'taki erken_kapanis'te.
        if (sinyal_kesti()) {
            return Bekleme::hata;   // W4-sunucu-kucuk: kapanış sinyali (hata kapanışı)
        }
        if (a3_hazirlik_durdu()) {
            return Bekleme::hata;   // W4-K27 (4): hazırlık oturumu yarıda bırakılır
        }
        if (gercek() > son_mutlak) {
            s_.hata = "hazırlık oturumu süre sınırını aştı";
            return Bekleme::hata;
        }
        t_h += kP;
        if (a_.kip == Kip::sil) {
            sil_duvar_ = t_h;
        } else {
            bekle_ve_oku(gercek() + kP);
        }
        const std::optional<std::uint32_t> seq = k_->parti_kapat(t_h, duvar());
        yaz();
        if (!seq) {
            gunluk(1, "hazırlık oturumu koptu; yeniden kuruluyor");
            return Bekleme::oturum_bitti;
        }
        if (a_.kip == Kip::sil) {
            const Bekleme r = yuva_bekle(*seq, son_mutlak);
            if (r != Bekleme::tamam) {
                return r;
            }
        } else {
            pompa(0);
        }
        bool hepsi = true;
        for (std::uint32_t id : idler) {
            const auto r = k_->komut_sonucu(id);
            if (!r) {
                hepsi = false;
                continue;
            }
            if (r->ack != kAckAccepted) {
                s_.hata = "hazırlık: komut reddedildi (cmd_id " + std::to_string(id) + ", neden " +
                          std::to_string(r->reason) + ")";
                return Bekleme::hata;
            }
        }
        const kopru::KartDurum& d = k_->kart().durum;
        tamam = hepsi && d.var && d.param_store == kPstoreOk && d.param_blob_crc == pb_.crc;
    }
    if (!tamam) {
        s_.hata = "hazırlık oturumu tamamlanmadı (param_blob_crc eşleşmedi)";
        return Bekleme::hata;
    }
    k_->oturum_kapat(kCloseRunEnd, duvar());
    yaz();
    pompa(0);
    k_->kip_ayarla(a_.kip == Kip::sil ? kopru::CalistirmaKipi::sil : kopru::CalistirmaKipi::hil_1x);
    return Bekleme::tamam;
}

// SIL kilitli adım (A1-K121 (ii), K76): yuva tamamlanana kadar bekler. Oturum biterse (karşı
// taraf kapattı, protokol hatası, T_SIL) yeniden kurmaz: Bekleme::oturum_bitti döner (çağıran
// kurar; özyineleme yok). son_mutlak: süren hazırlık işleminin mutlak son tarihi (duvar).
Calistirici::Bekleme Calistirici::yuva_bekle(std::uint32_t seq, std::int64_t son_mutlak)
{
    const auto bas = Saat::now();
    // Alım kuyruğu yalnız burada (ve el sıkışmada) işlenir; yuva tamamlanınca ya da oturum
    // bitince o çerçevede durulur: sonrası sonraki yuvanın beklemesinde işlenir (W4-K14).
    const auto dur = [this, seq] {
        return k_->yuva_tamam(seq) || k_->durum() != kopru::OturumDurumu::kosuyor;
    };
    // W4-K26 (3) (EMNİYET): bu beklemede yuva tamamlanmadan en çok kSilBeklemeAzami bayt
    // tüketilir. Besleme kalan bütçeyle sınırlanır; bütçe yuva tamamlanmadan bittiği anda (tam o
    // baytta) oturum protokol hatasıyla biter. Karar yalnız akıştaki bayt konumuna bağlıdır: yuvayı
    // tamamlayan çerçevenin son baytı bütçenin içindeyse geçerli, dışındaysa (aynı okumada gelse de)
    // değil. Kalan kuyruk atılmaz; el sıkışmada akış sırasıyla işlenir.
    const std::uint64_t beslenen_bas = sil_beslenen_;
    for (;;) {
        if (sinyal_kesti()) {
            return Bekleme::hata;   // W4-sunucu-kucuk: kapanış sinyali (hata kapanışı)
        }
        const std::uint64_t harcanan = sil_beslenen_ - beslenen_bas;
        (void)sil_besle(dur, harcanan < kSilBeklemeAzami ? kSilBeklemeAzami - harcanan : 0u);
        if (!k_->yuva_tamam(seq) && k_->durum() == kopru::OturumDurumu::kosuyor &&
            sil_beslenen_ - beslenen_bas >= kSilBeklemeAzami) {
            ++s_.sil_kuyruk_tasma;
            k_->protokol_hatasi(duvar());
        }
        if (k_->yuva_tamam(seq)) {
            return Bekleme::tamam;
        }
        if (k_->durum() != kopru::OturumDurumu::kosuyor) {
            // Oturum bitti (karşı taraf kapattı): çağıran yeniden kurar; bu yuva kaçırılır.
            gunluk(1, "oturum bitti (parti " + std::to_string(seq) + "); yeniden kuruluyor");
            ++s_.yeniden_baglanti;
            return Bekleme::oturum_bitti;
        }
        pompa(1000);
        if (gercek() > son_mutlak) {
            s_.hata = "hazırlık oturumu süre sınırını aştı";
            return Bekleme::hata;
        }
        if (Saat::now() - bas > std::chrono::nanoseconds(a_.t_sil_ns)) {
            k_->sil_gozetimi_doldu(duvar());   // A1-K76
            gunluk(1, "T_SIL doldu (parti " + std::to_string(seq) + ")");
            if (ic_) {
                s_.hata = "süreç içi karşı taraf yanıt vermedi";
                return Bekleme::hata;
            }
            if (tas_) {
                tas_->kapat_duzgun(kKapanisBosaltmaUs);   // W4-K25 (5)
                tas_.reset();
            }
            sil_gelen_temizle();
            ++s_.yeniden_baglanti;
            return Bekleme::oturum_bitti;
        }
    }
}

// ---- Senaryo planı ----

// A5 dosyası: çözümleme ve doğrulama (A5-K90), plan; komut satırı çelişkileri. false: s_ hatalı.
bool Calistirici::dosya_plani()
{
    // EMNİYET (sol #1): yalnız yerleşik planda anlamı olan seçenekler dosyalı koşuda sessizce yok
    // sayılmaz; başlamadan kullanım hatasıdır. Dosyanın emniyet ve arıza olayları yalnız
    // dosyadadır (davranış özetine girer).
    {
        // Verildi sayılır: bayrak ya da varsayılandan (−1) farklı her değer (NaN dahil); değerin
        // işareti ve yineleme sonucu etkilemez.
        std::string secenek;
        if (a_.acil_verildi || !(a_.acil_durdurma_s == -1.0)) {
            secenek = "--acil";
        } else if (a_.acil_kalk_verildi || !(a_.acil_kalkar_s == -1.0)) {
            secenek = "--acil-kalk";
        } else if (a_.ariza_verildi) {
            secenek = "--ariza";
        }
        if (!secenek.empty()) {
            s_.hata = secenek + " senaryo dosyasıyla birlikte kullanılamaz (emniyet ve arıza olayları "
                                "dosyanın `olaylar` dizisindedir)";
            s_.hata_turu = HataTuru::kullanim;
            return false;
        }
    }
    std::shared_ptr<const senaryo::DosyaKaynagi> kaynak = a_.senaryo_kaynagi;
    if (!kaynak) {
        kaynak = std::make_shared<senaryo::DiskKaynagi>(a_.depo_koku);
    }
    const senaryo::CozumSonucu cs = senaryo::senaryo_coz(*kaynak, a_.senaryo_dosyasi);
    s_.davranis_ozeti = cs.rapor.davranis_ozeti.value_or("");
    s_.kaynak_ozetleri = cs.rapor.kaynak_ozetleri;
    // Rapor her durumda değiştirilmeden saklanır; bulgular (A5-K71 uyarıları dahil) kaynak ve alan
    // bilgisiyle taşınır (sol #4).
    s_.dogrulama_raporu = cs.rapor.json();
    for (const senaryo::Bulgu& b : cs.rapor.bulgular) {
        s_.dogrulama_bulgulari.push_back({b.kural, b.onem == senaryo::Onem::hata ? "hata" : "uyari", b.dosya, b.yol,
                                          b.satir.value_or(-1), b.ileti});
    }
    if (!cs.rapor.gecerli || !cs.cozumlenmis) {
        s_.hata = "senaryo geçersiz: " + std::to_string(cs.rapor.hata_sayisi()) + " hata (doğrulama raporu)";
        s_.hata_turu = HataTuru::senaryo_gecersiz;
        return false;
    }
    std::string hata;
    if (!plan_kur(*cs.cozumlenmis, cs.rapor, plan_, hata, a_.sil_hiz)) {
        s_.hata = "senaryo bu dilimde yürütülemez: " + hata;
        s_.hata_turu = HataTuru::senaryo_yurutulemez;
        return false;
    }
    {
        // W4-sunucu-kucuk kapsam 5: senaryodaki etkin sonar sayısı (A7'ye bağlı değil); bayrak verilmemişse
        // tek satır bilgi. Varsayılan değişmez: sonar yalnız `--sonar` ile açılır.
        std::size_t n = 0;
        if (const senaryo::Deger* sl = cs.cozumlenmis->agac().yol_bul("arac.sensorler");
            sl != nullptr && sl->tablo_mu()) {
            for (const auto& [ad, sn] : sl->tablo) {
                const senaryo::Deger* tur = sn.tablo_mu() ? sn.bul("tur") : nullptr;
                const senaryo::Deger* etkin = sn.tablo_mu() ? sn.bul("etkin") : nullptr;
                if (senaryo::deneysel_mi(ad) || tur == nullptr || !tur->dizgi_mi() ||
                    (tur->dizgi != "ping1d" && tur->dizgi != "ping360") || etkin == nullptr ||
                    !etkin->mantiksal_mi() || !etkin->mantik) {
                    continue;
                }
                ++n;
            }
        }
        s_.senaryo_etkin_sonar = n;
        if (n > 0 && !a_.sonar_acik && !a_.sonar_verildi) {
            gunluk(1, "senaryoda " + std::to_string(n) + " etkin sonar var; `--sonar` ile açılır");
        }
    }
    if (a_.sonar_acik) {
        // W4-bagla-sonar: sonar kurulumu da yalnız doğrulanmış senaryodan (sensor_kur ile aynı kural);
        // A7 manifesti çözümleyiciyle aynı kaynaktan.
        sonar_kurulum_ = senaryo::sonar_kur(*cs.cozumlenmis, *kaynak, cs.rapor, &hata);
        if (!sonar_kurulum_) {
            s_.hata = "senaryo bu dilimde yürütülemez: sonar: " + hata;
            s_.hata_turu = HataTuru::senaryo_yurutulemez;
            return false;
        }
    }
    if (a_.kayit_acik) {
        // W5-bagla-kayit: kayıt ekleri ve üst verisi yalnız doğrulanmış senaryodan (çözümlenmiş senaryo,
        // doğrulama raporu, A7 manifesti; A6 §3.6). Kurulamazsa koşu başlamaz (kayıt istenip alınamazsa
        // sessizce kayıtsız sürülmez).
        KayitGirdisi kg;
        std::string kh;
        if (!kayit_girdisi_kur(*cs.cozumlenmis, cs.rapor, *kaynak, kg, kh)) {
            s_.hata = "kayıt girdisi kurulamadı: " + kh;
            s_.kayit.istendi = true;
            s_.kayit.hata = kh;
            return false;
        }
        kayit_girdi_ = std::move(kg);
    }
    if (a_.a2_acik) {
        // W5-bagla-a2: çalıştırma tanımının senaryodan gelen kısmı (kademe, A7 manifest yolu ve
        // özeti, ortam, malzeme optiği); yalnız doğrulanmış senaryodan, çözümleyiciyle aynı kaynaktan.
        // Kurulamazsa koşu A2'siz sürer (a2_baslat).
        a2_tanim_sonucu_ = a2b::tanim_kur(*cs.cozumlenmis, cs.rapor, *kaynak, a_.depo_koku, a2_tanim_, a2_tanim_hatasi_);
    }
    auto kullanim = [&](const std::string& m) {
        s_.hata = m;
        s_.hata_turu = HataTuru::kullanim;
        return false;
    };
    // Okuma kararı (W4-sim-senaryo): kip, süre ve tohum davranış özetine girer; dosyadandır.
    if (a_.kip_verildi && a_.kip != plan_.kip) {
        return kullanim("--kip dosyanın calistirma.kip değeriyle çelişiyor");
    }
    if (a_.tohum_verildi) {
        return kullanim("--tohum senaryo dosyasıyla verilemez (rastgelelik.ana_tohum; A5-K82)");
    }
    if (a_.sure_verildi) {
        return kullanim("--sure senaryo dosyasıyla verilemez (calistirma.sure_ns)");
    }
    a_.kip = plan_.kip;
    a_.tohum = plan_.tohum;
    a_.sure_s = static_cast<double>(plan_.sure_ns) * 1e-9;
    if (!a_.karsi_verildi) {
        a_.karsi = a_.kip == Kip::sil ? Karsi::ic : Karsi::seri;
    }
    if (a_.kip == Kip::hil && (a_.karsi == Karsi::ic || a_.karsi == Karsi::tcp)) {
        return kullanim("hil kipinde karşı taraf pty ya da seri olmalı");
    }
    if (a_.kip == Kip::sil && (a_.karsi == Karsi::pty || a_.karsi == Karsi::seri)) {
        return kullanim("sil kipinde karşı taraf ic ya da tcp olmalı");
    }
    if (a_.karsi == Karsi::seri && a_.seri.empty()) {
        return kullanim("hil kipi --seri <aygıt> ya da --karsi pty ister");
    }
    s_.senaryo_uyarilari = plan_.uyarilar;
    if (a_.a3_acik) {
        kanonik_ = cs.cozumlenmis->kanonik();   // GET /senaryo/cozumlenmis (A3-K80)
    }
    return true;
}

// Plan yürütmesinin hazırlığı (sensör kümesi kurulduktan sonra): yer istasyonu canlılığı,
// zamanlı sensör arızaları (kümede kurulur; başlangıç ve bitişi küme yürütür), koşullu sensör
// arızalarının ön denetimi. false: s_ hatalı (başlamadan).
bool Calistirici::plan_hazirla(const sensor::SensorKumesiAyari& sa)
{
    pd_.assign(plan_.olaylar.size(), PlanDurumu{});
    hb_.clear();
    hb_i_ = 0;
    // A1-K70: yer istasyonu canlılığı 1 Hz. Yerleşikte yalnız z1; dosyada her zaman (sim'in yer
    // istasyonu ağ geçidi; A5 olayı değildir).
    if (plan_.dosyadan || a_.senaryo == "z1") {
        const Z1& z = a_.z1;
        for (double t = z.hb_ilk_s; t < a_.sure_s; t += 1.0) {
            if (!plan_.dosyadan && z.t_hb_kes_s >= 0.0 && t >= z.t_hb_kes_s) {
                break;
            }
            hb_.push_back(static_cast<std::int64_t>(std::llround(t * 1e9)));
        }
    }
    // Koşullu arızaların ön denetimi ayrı, geçici bir kümede yapılır (sol #2): canlı kümede arıza
    // kimliği tüketilmez (`disarm` kaydı silmez; sonraki `arm` "kimlik tekil değil" derdi).
    std::unique_ptr<sensor::SensorKumesi> gecici;
    std::unique_ptr<sonar::SonarKumesi> gecici_sonar;
    for (std::size_t i = 0; i < plan_.olaylar.size(); ++i) {
        const PlanOlayi& o = plan_.olaylar[i];
        if (o.tur != PlanOlayTuru::sensor) {
            continue;
        }
        sensor::ArmSonucu r;
        const bool sonar_ariza = sonar_hedefi(o.ariza.hedef);   // W4-bagla-sonar kapsam 6
        if (o.zaman_ns) {
            // zamanlı: canlı kümede; başlangıç ve bitişi küme yürütür. Sonar hedefi sonar kümesinde
            // (desteklenmeyen tür burada açık hatayla reddedilir; sessiz atlama yok).
            r = sonar_ariza ? sonar_->arm(o.ariza) : kume_.arm(o.ariza);
        } else if (sonar_ariza) {
            if (!gecici_sonar) {
                gecici_sonar = std::make_unique<sonar::SonarKumesi>();
                if (!gecici_sonar->kur(sonar_kurulum_->ayar, sonar_kurulum_->sahne).tamam) {
                    s_.hata = "koşullu sonar arızalarının ön denetimi: geçici küme kurulamadı";
                    return false;
                }
            }
            r = gecici_sonar->arm(o.ariza);
        } else {
            if (!gecici) {
                gecici = std::make_unique<sensor::SensorKumesi>();
                if (!gecici->kur(sa).tamam) {
                    s_.hata = "koşullu sensör arızalarının ön denetimi: geçici küme kurulamadı";
                    return false;
                }
            }
            r = gecici->arm(o.ariza);
        }
        if (!r.tamam) {
            s_.hata = "senaryo bu dilimde yürütülemez: olaylar[" + o.kimlik + "]: " + r.hata;
            s_.hata_turu = HataTuru::senaryo_yurutulemez;
            return false;
        }
    }
    return true;
}

void Calistirici::olay_sonucu(std::size_t i, const char* sonuc, std::int64_t t, const char* neden)
{
    s_.olay_sonuclari.push_back({plan_.olaylar[i].kimlik, sonuc, t, neden, "senaryo"});
    if (kb_) {
        kayit_oge(i, sonuc, t, neden);
    }
    if (yay_ != nullptr) {
        a3_senaryo_ogesi(i, sonuc, t, neden);
    }
}

// Görev yüklemesi (A5-K57; A1-K69): MISSION_BEGIN(count), öğeler, MISSION_END; her ileti
// öncekinin ACCEPTED sonucundan sonra (A3 mission_upload ağ geçidi kuralı); ilk ACCEPTED olmayan
// sonuçta durur.
void Calistirici::gorev_hizmet(std::int64_t t)
{
    if (!plan_.gorev_var || gorev_bitti_ || t < plan_.gorev_yukleme_ns) {
        return;
    }
    const auto n = static_cast<int>(plan_.gorev.size());
    if (a_.gorev_beklemeden && gorev_gonderilen_ == n + 2) {
        s_.gorev_durumu = "gonderildi";
        gorev_bitti_ = true;
        return;
    }
    if (gorev_gonderilen_ > 0 && !a_.gorev_beklemeden) {
        const auto r = k_->komut_sonucu(gorev_son_id_);
        if (!r) {
            return;
        }
        if (r->ack != kAckAccepted) {
            s_.gorev_durumu = "reddedildi";
            s_.gorev_basarisiz_adim = gorev_gonderilen_ - 1;
            s_.gorev_ack = r->ack;
            s_.gorev_neden = r->reason;
            gorev_bitti_ = true;
            return;
        }
        if (gorev_gonderilen_ == n + 2) {
            s_.gorev_durumu = "tamamlandi";
            gorev_bitti_ = true;
            return;
        }
    }
    if (gorev_gonderilen_ == 0) {
        s_.gorev_durumu = "bitmedi";
        gorev_son_id_ = k_->komut_basit(kCmdMissionBegin, static_cast<std::uint16_t>(n), t);
    } else if (gorev_gonderilen_ <= n) {
        gorev_son_id_ = k_->komut_gorev_ogesi(plan_.gorev[static_cast<std::size_t>(gorev_gonderilen_ - 1)], t);
    } else {
        gorev_son_id_ = k_->komut_basit(kCmdMissionEnd, 0, t);
    }
    ++gorev_gonderilen_;
}

// Sınır b'nin başında (köprünün sınır işleminden önce): yer istasyonu canlılığı, görev,
// olayların bitişleri ve başlangıçları (A5-K62: önce bitişler, sonra başlangıçlar; her grupta
// dosya sırası). Emniyet atamaları köprüye verilir; A1-K85'in sırasını köprü uygular.
void Calistirici::plan_sinir(std::uint64_t b)
{
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    while (hb_i_ < hb_.size() && hb_[hb_i_] <= t) {
        (void)k_->komut_basit(kCmdHeartbeat, 0, hb_[hb_i_]);
        ++hb_i_;
    }
    gorev_hizmet(t);
    // Bitişler (yalnız itici arızası burada; sensör arızalarının bitişini küme yürütür).
    itici_.erase(std::remove_if(itici_.begin(), itici_.end(), [t](const IticiAriza& a) { return t >= a.bitis_ns; }),
                 itici_.end());
    // Başlangıçlar.
    sinir_emniyet_.clear();
    for (std::size_t i = 0; i < plan_.olaylar.size(); ++i) {
        PlanDurumu& d = pd_[i];
        const PlanOlayi& o = plan_.olaylar[i];
        if (d.basladi) {
            continue;
        }
        const bool basla = o.zaman_ns ? t >= *o.zaman_ns
                                       : (d.tetik_b >= 0 && static_cast<std::uint64_t>(d.tetik_b) <= b);
        if (!basla) {
            continue;
        }
        d.basladi = true;
        d.baslangic_ns = t;
        switch (o.tur) {
        case PlanOlayTuru::emniyet:
            sinir_emniyet_.push_back(i);   // sonuç köprünün sınır işleminden sonra yazılır
            break;
        case PlanOlayTuru::gcs: {
            const std::uint32_t id = k_->komut_basit(o.komut, o.arg, t);
            if (o.komut == kCmdArm && id_arm_ == 0u) {
                id_arm_ = id;
            } else if (o.komut == kCmdSetMode && id_kip_ == 0u) {
                id_kip_ = id;
            } else if (o.komut == kCmdDisarm && id_disarm_ == 0u) {
                id_disarm_ = id;
                t_disarm_istek_ = t;
            }
            olay_sonucu(i, "uygulandi", t);
            break;
        }
        case PlanOlayTuru::guided: {
            const std::uint32_t id = k_->komut_guided(o.guided, t);
            (o.guided_no == 1 ? id_sp_ : id_sp2_) = id;
            olay_sonucu(i, "uygulandi", t);
            break;
        }
        case PlanOlayTuru::sensor:
            if (!o.zaman_ns) {
                sensor::SensorArizasi f = o.ariza;
                f.zaman_ns = t;
                const sensor::ArmSonucu r = d.sonar_verildi ? d.sonar_sonuc : kume_.arm(f);
                if (!r.tamam) {
                    // Ön denetimden geçtiği için beklenmez; nedeni olduğu gibi yazılır.
                    const bool hedef = r.hata.find("hedef") != std::string::npos;
                    d.reddedildi = true;
                    olay_sonucu(i, "reddedildi", t, hedef ? "hedef_yok" : "desteklenmiyor");
                    s_.senaryo_uyarilari.push_back("olaylar[" + o.kimlik + "] başlatılamadı: " + r.hata);
                    break;
                }
            }
            olay_sonucu(i, "uygulandi", t);
            break;
        case PlanOlayTuru::itici:
            itici_.push_back({o.kanal_maskesi, o.carpan,
                              o.sure_ns > 0 ? t + o.sure_ns : std::numeric_limits<std::int64_t>::max(), o.kimlik, t});
            olay_sonucu(i, "uygulandi", t);
            break;
        }
    }
    for (std::size_t i : sinir_emniyet_) {
        const PlanOlayi& o = plan_.olaylar[i];
        kopru::EmniyetAtama e;
        if (o.girdi == EmniyetGirdisi::acil_durdurma) {
            e.acil_durdurma = o.deger;
        } else if (o.girdi == EmniyetGirdisi::fis) {
            e.fis[static_cast<std::size_t>(o.tup)] = o.deger;
        } else {
            e.pim[static_cast<std::size_t>(o.tup)] = o.deger;
        }
        k_->emniyet_ata(e);
    }
}

// Köprünün sınır işleminden sonra: emniyet atamalarının sonuçları (A5-K63, §3.15).
void Calistirici::plan_sinir_sonrasi(std::uint64_t b, const kopru::SinirSonucu& ss)
{
    if (sinir_emniyet_.empty()) {
        return;
    }
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    // Aynı alana aynı sınırda farklı değerler: kısıtlayan uygulanır, atama_celiskisi yazılır.
    std::map<std::string, AtamaCeliskisi> alanlar;
    for (std::size_t i : sinir_emniyet_) {
        const PlanOlayi& o = plan_.olaylar[i];
        AtamaCeliskisi& c = alanlar[emniyet_alani(o)];
        c.t_ns = t;
        c.alan = emniyet_alani(o);
        c.degerler.push_back(o.deger);
        c.olay_kimlikleri.push_back(o.kimlik);
    }
    for (auto& [alan, c] : alanlar) {
        (void)alan;
        const bool farkli = std::find(c.degerler.begin(), c.degerler.end(), !c.degerler.front()) != c.degerler.end();
        if (farkli) {
            // Kısıtlayan değer: acil durdurma basılı (true), fiş çıkık (false), pim takılı (true).
            c.uygulanan_deger = c.alan.find(".fis.") == std::string::npos;
            s_.atama_celiskileri.push_back(c);
        }
    }
    for (std::size_t i : sinir_emniyet_) {
        const PlanOlayi& o = plan_.olaylar[i];
        if (!kisitlayan(o) && ss.gevsetme_reddedildi) {
            s_.gevsetme_retleri.push_back({t, o.kimlik, emniyet_alani(o), o.deger, ss.rev_adim_basi, ss.rev_adim3_sonu});
            olay_sonucu(i, "reddedildi", t, "gevsetme_reddedildi");
        } else {
            olay_sonucu(i, "uygulandi", t);
        }
    }
}

// Sınır b: senaryo, köprünün emniyet ve sürekli çıktı işlemleri, acil durdurma engeli, arıza,
// itici komutu, sensörler. false: sensör ya da fizik hatası.
// W4-duraklat: duran kipte aynı işlev, aynı sıra; senaryonun zaman çizelgesi (plan_sinir ve
// sonuçları) ve zamanı ilerleten sensör/sonar adımı atlanır. Köprünün emniyet işlemleri (A1-K85),
// acil durdurma engeli ve itici girdisinin yeniden değerlendirilmesi iki kipte aynı koddur: fizik
// adımı b, sınırın son turunun (duraklatmadan çıkış turu dahil) girdisiyle koşar.
bool Calistirici::sinir_on(std::uint64_t b, bool duran)
{
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    if (!duran) {
        plan_sinir(b);   // (1) zaman çizelgesi: aynı sınırın olayları yeniden girmez
    }
    if (yay_ != nullptr) {
        // W4-bagla-a3 (EMNİYET): panelin emniyet istekleri ve daraltıcı komutları senaryonun
        // atamalarıyla aynı sınırda, aynı köprü yolundan (emniyet_ata, komut_*) girer; sırayı
        // (A1-K85) köprü uygular.
        a3_emniyet_ver(b);
    }
    const std::int64_t e0 = gercek();
    const kopru::SinirSonucu ss = k_->sinir_isle(b, duvar());
    const std::int64_t e1 = gercek();
    if (!duran) {
        plan_sinir_sonrasi(b, ss);
    }
    if (yay_ != nullptr) {
        a3_sinir_sonrasi(b, ss, duran);
        if (a3_ertelenen_yeniden_bitir(b)) {
            // W5-K56: ertelenmiş `sifirla`/`senaryo/yukle`. Sınamadan küçük numaralı kısıtlayanlar bu sınırda
            // uygulandı ve sonuçlandı; dünya, `sifirla`nın sınır başında kabul edildiği yolla aynı biçimde
            // (a3_durdur_ kurulu, hata yok) sonlanır: A1 oturumu kapanır, kapanış daraltması uygulanır.
            return false;
        }
    }
    evre_[kEvreKopru] += e1 - e0;
    if (!duran) {
        son_pwm_yur_ = pwm_yur_;   // (t − h, t] adımının komutu: aynı sınırın yeni turunda değişmez
    }
    pwm_yur_ = ss.pwm_us;
    std::array<double, 8> pwm = ss.pwm_us;
    const double notr = static_cast<double>(arac_.esc.pwm_notr_us);
    // EMNİYET (iskelet §5, W3-K1): acil durdurma iticilerin enerjisini keser; köprüden gelen
    // komut ne olursa olsun itici girdisi nötrdür. (Sayaç `estop_adim` fizik adımında sayılır.)
    if (!duran) {
        onceki_estop_ = estop_bu_adim_;
    }
    estop_bu_adim_ = k_->acil_durdurma();
    if (estop_bu_adim_) {
        pwm.fill(notr);
    }
    std::array<double, 8> carpanlar{};
    carpanlar.fill(1.0);
    for (const IticiAriza& a : itici_) {
        for (std::size_t c = 0; c < 8; ++c) {
            if ((a.maske & (1u << c)) != 0u) {
                carpanlar[c] *= a.carpan;
            }
        }
    }
    // A5 aktuator/itki_carpani (W4-K20): çarpan fizikte, süzgeç ve β sonrasındaki mekanik kuvvete
    // uygulanır; darbe genişliği ve elektrik tarafı arızasız komutu görür. Çakışan arızaların
    // çarpanları çarpılır (A5-K64). Bu sınırdan (adım b'den) itibaren geçerlidir.
    if (carpanlar != fizik_carpan_) {
        if (!dn_.itki_carpani_ver(b, carpanlar)) {
            s_.hata = "itki çarpanı fiziğe verilemedi";
            return false;
        }
        fizik_carpan_ = carpanlar;
    }
    (void)dn_.itici_komutu_ver(b, pwm);
    // Gözlem fizik adımından hemen önce bildirilir (SIL'de W4-K26 (1) komutu değiştirebilir).
    gozlem_.b = b;
    gozlem_.derinlik = dn_.durum().konum_n[2];
    gozlem_.pwm = pwm;
    gozlem_.itici_carpani = carpanlar;
    gozlem_.acil_durdurma = estop_bu_adim_;
    gozlem_.konum = dn_.durum().konum_n;
    gozlem_.yonelim = dn_.durum().yonelim;
    if (duran) {
        return true;   // (2) sensör ve sonar adımı zamanı ilerletir: duran kipte yok
    }
    const std::int64_t e2 = gercek();
    kume_.dis_durum_ver(k_->dis_durum());
    if (kume_.adim(dn_.durum(), b) != sensor::AdimSonucu::tamam) {
        s_.hata = "sensör kümesi adımı başarısız";
        return false;
    }
    olcumler_.clear();
    kume_.bosalt(t, olcumler_);
    const std::int64_t e3 = gercek();
    evre_[kEvreSensor] += e3 - e2;
    for (const sensor::Olcum& o : olcumler_) {
        if (a_.olcum_kancasi) {
            a_.olcum_kancasi(o);
        }
        k_->olcum_ekle(o);
    }
    evre_[kEvreKodlama] += gercek() - e3;
    if (sonar_) {
        // W4-bagla-sonar: sonar kümesi aynı sınırda, aynı teslim kuralıyla (t_teslim ≤ t; W4-K9);
        // rastgelelik kendi A5 Ek D akışlarından (öteki sensörlerin dizisi değişmez).
        // Sınır b'nin adımı (ping hesabı) önceki sınırın sonunda yapıldı (sonar_onceden); burada
        // yalnız boşaltma ve köprüye verme (kısa; ölçüm: sonar_bosaltma_en_uzun_ns).
        const std::int64_t s0 = gercek();
        sonar_adimi(t);
        s_.sonar_bosaltma_en_uzun_ns = std::max(s_.sonar_bosaltma_en_uzun_ns, gercek() - s0);
    }
    return true;
}

bool Calistirici::sonar_hedefi(const std::string& ad) const
{
    if (!sonar_ || !sonar_kurulum_) {
        return false;
    }
    for (const sonar::SonarAyari& a : sonar_kurulum_->ayar.sonarlar) {
        if (a.etkin && a.ad == ad) {
            return true;
        }
    }
    return false;
}

namespace {

// W4-K27 (5) (W4-bagla-sonar sol #5): `g_sonar_sayim` > 0 iken bellek ayırması dünya iş parçacığındaki
// sonar hesabına ve boşaltmasına sayılır (sınama kancası `sonar_ayirma_sayilir`). Köprüye verme,
// sınama kancaları ve CSV bu kapsamın dışındadır.
thread_local int g_sonar_sayim = 0;

class SonarAyirmaKapsami {
public:
    SonarAyirmaKapsami() { ++g_sonar_sayim; }
    ~SonarAyirmaKapsami() { --g_sonar_sayim; }
    SonarAyirmaKapsami(const SonarAyirmaKapsami&) = delete;
    SonarAyirmaKapsami& operator=(const SonarAyirmaKapsami&) = delete;
};

// A1 §3.9 `A_sensör` (A1-K113, E13): TF, IMU ve emniyet dışındaki kritik sensör baytının parti
// (kontrol periyodu P) başına ortalaması, en yüksek hızlarla. Sensör kümesindeki her etkin MAG,
// BARO, GNSS, DVL, BATT ve LEAK örneği için çerçeve boyu × P / periyot; periyot `hiz_hz`'ten ya da
// profil hızından (sim/sensor modeller.cpp: MAG 100, BARO 25, GNSS 5, BATT 10, LEAK 10 Hz; DVL
// modlarının en yüksek hızı 15 Hz), saat kayması (1 + ppm·10⁻⁶) ile. Ping1D: sonar kümesinin en
// kısa ortalama ping aralığıyla. Sonuç bayttır (tahmin değil, yapılandırmadan hesap).
double a_sensor_bayt(const sensor::SensorKumesiAyari& sa, const std::vector<std::int64_t>& ping1d_aralik_ns)
{
    double toplam = 0.0;
    for (const sensor::SensorAyari& a : sa.sensorler) {
        if (!a.etkin) {
            continue;
        }
        double hiz = 0.0;
        switch (a.tur) {
        case sensor::SensorTuru::manyetometre: hiz = a.hiz_hz.value_or(100.0); break;
        case sensor::SensorTuru::basinc: hiz = a.hiz_hz.value_or(25.0); break;
        case sensor::SensorTuru::gnss: hiz = a.hiz_hz.value_or(5.0); break;
        case sensor::SensorTuru::dvl: hiz = 15.0; break;
        case sensor::SensorTuru::batarya_olcer: hiz = a.hiz_hz.value_or(10.0); break;
        case sensor::SensorTuru::sizinti: hiz = a.hiz_hz.value_or(10.0); break;
        default: continue;   // IMU, emniyet geri okuması, sonar (aşağıda), kamera
        }
        const double periyot_ns = 1e9 / hiz * (1.0 + a.saat_kaymasi_ppm * 1e-6);
        toplam += static_cast<double>(kopru::kritik_sensor_cerceve_boyu(a.tur)) * static_cast<double>(kP) / periyot_ns;
    }
    const double p1d = static_cast<double>(kopru::kritik_sensor_cerceve_boyu(sensor::SensorTuru::ping1d));
    for (const std::int64_t t : ping1d_aralik_ns) {
        toplam += p1d * static_cast<double>(kP) / static_cast<double>(t);
    }
    return toplam;
}

}  // namespace

// Sonar kümesinin kurulumu (sensör kümesinden sonra, plan hazırlığından önce). false: s_ hatalı.
bool Calistirici::sonar_kur_(const sensor::SensorKumesiAyari& sa)
{
    if (sonar_kurulum_->etkin_sayisi == 0u) {
        // Senaryoda etkin sonar yok: küme kurulmaz, koşu sonarsız koşuyla bayt bayt aynıdır.
        s_.sonar_uyarilari.emplace_back("--sonar verildi ama senaryoda etkin sonar yok");
        s_.senaryo_uyarilari.push_back(s_.sonar_uyarilari.back());
        sonar_kurulum_.reset();
        return true;
    }
    sonar::SonarKumesiAyari& ay = sonar_kurulum_->ayar;
    ay.adim_ns = kH;
    for (sonar::SonarAyari& x : ay.sonarlar) {
        if (auto* y = std::get_if<sonar::Ping360Yapilandirmasi>(&x.yapilandirma)) {
            // Kalite kademesi (K16, Ö11; W4-K18 (c)): kurulum alanının kısılması. Menzil korunur:
            // örnek periyodu örnek sayısıyla ters orantılı (A1-K52 r = c · n · sp · 25 ns / 2).
            if (a_.ping360_ornek > 0) {
                const double sp = static_cast<double>(y->sample_period) * static_cast<double>(y->n_samples) /
                                  static_cast<double>(a_.ping360_ornek);
                y->n_samples = static_cast<std::uint16_t>(a_.ping360_ornek);
                y->sample_period = static_cast<std::uint16_t>(std::clamp(std::llround(sp), 80LL, 40000LL));
            }
            y->bekleme_ns = static_cast<std::int64_t>(std::llround(a_.ping360_bekleme_ms * 1e6));
        }
    }
    sonar_ = std::make_unique<sonar::SonarKumesi>();
    const sensor::KurulumSonucu r = sonar_->kur(ay, sonar_kurulum_->sahne);
    if (!r.tamam) {
        s_.hata = "senaryo bu dilimde yürütülemez: sonar kümesi: " + r.hata;
        s_.hata_turu = HataTuru::senaryo_yurutulemez;
        sonar_.reset();
        return false;
    }
    // W4-bagla-sonar sol #2: örnek/periyot dönüşümünden sonra gerçekleşen Ping360 darbesi cihaz
    // aralığında (otomatik darbe modelde 1–1000 µs'ye kırpılır; açık değer `kur`da denetlenir).
    for (const sonar::SonarAyari& x : ay.sonarlar) {
        if (const auto* y = std::get_if<sonar::Ping360Yapilandirmasi>(&x.yapilandirma); y != nullptr && x.etkin) {
            const std::uint16_t d = sonar::ping360_darbe_us(*y);
            if (d < 1u || d > 1000u) {
                s_.hata = "senaryo bu dilimde yürütülemez: sonar: " + x.ad + " darbe süresi " + std::to_string(d) +
                          " µs, cihaz aralığı 1–1000 µs";
                s_.hata_turu = HataTuru::senaryo_yurutulemez;
                sonar_.reset();
                return false;
            }
        }
    }
    // W4-bagla-sonar sol #1: A1 §3.9 `A_sensör` ön denetimi (A1-K113, E13). Etkin Ping1D'lerin en
    // kısa ortalama ping aralığıyla üretebileceği bayt, öteki kritik sensörlerle birlikte parti
    // başına 150 B'ı aşıyorsa senaryo çalıştırılmadan reddedilir (sınırdaki değer kabul).
    {
        const double a_sensor = a_sensor_bayt(sa, sonar_->ping1d_en_kisa_aralik_ns());
        s_.sonar_a_sensor_bayt = a_sensor;
        if (!(a_sensor <= kopru::kASensorAzamiB)) {
            char m[160];
            std::snprintf(m, sizeof m, "%.4f B > %.0f B", a_sensor, kopru::kASensorAzamiB);
            s_.hata = std::string("senaryo bu dilimde yürütülemez: A1 §3.9 A_sensör ön denetimi (A1-K113, E13): kritik "
                                  "sensör baytı parti başına ") +
                      m + " (Ping1D en kısa ping aralığıyla, öteki sensörlerle birlikte)";
            s_.hata_turu = HataTuru::senaryo_yurutulemez;
            sonar_.reset();
            return false;
        }
    }
    s_.sonar_acik = true;
    for (const std::string& u : sonar_kurulum_->uyarilar) {
        s_.sonar_uyarilari.push_back(u);
    }
    for (const std::string& u : r.uyarilar) {
        s_.sonar_uyarilari.push_back("sonar kümesi: " + u);
    }
    for (const std::string& u : s_.sonar_uyarilari) {
        s_.senaryo_uyarilari.push_back(u);
    }
    // Teslim deposu (W4-K27 (5); sol #5): kapasite yapılandırmadan (etkin sonar × sonar başına
    // kuyruk sınırı); taşan kayıt düşürülür ve `sonar_teslim_tasma` sayılır, depo büyümez.
    sonar_olcumler_.reserve(sonar_->teslim_kapasitesi());
    const auto n_sinir = static_cast<std::size_t>(plan_.sure_ns / kH);
    s_.sonar_adim_ping_ns.reserve(std::min<std::size_t>(n_sinir + 1u, std::size_t{1} << 20U));
    if (!a_.sonar_csv_yolu.empty()) {
        s_.sonar_csv =
            "tur,sensor_id,inst,seq,t_meas_ns,t_tx_ns,t_hazir_ns,t_teslim_ns,flags,ariza_maskesi,mesafe_m,guven,"
            "scan_start,scan_length,ping_number,tx_offset_ns,aci_grad,n_ornek,ornek_periyodu,darbe_suresi_us,"
            "frekans_khz,kazanc,veri_n,veri_crc32\n";
    }
    return true;
}

// Sonar kümesinin sınır b adımı (ping hesabı; sade kademede ping başına ≈ 0,4–1,4 ms, ölçüm).
// Sınır b'nin fizik durumu, fizik adımı b − 1 bitince hazırdır; adım o anda (HIL'de sınır b'nin
// bekleyişinden önce) yapılır ki ping hesabı sınır b'nin işlemini ve TF(n) yazımını geciktirmesin
// (A1-K39: TF W(t_n)'de; C_sim = 0,4 ms bütçesi). Sonar yalnız fizik durumuna ve kendi arızalarına
// bağlıdır (köprü, emniyet ve senaryo atamaları ona girmez): sonuç sınır b'nin içinde adımlamakla
// bayt bayt aynıdır. Koşullu sonar arızası, başlayacağı sınırın (tetik_b = b) adımından önce verilir
// (A5-K63 ile aynı başlangıç sınırı). Süre ölçülür (duvar; dünya iş parçacığı).
bool Calistirici::sonar_onceden(std::uint64_t b)
{
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    for (std::size_t i = 0; i < plan_.olaylar.size(); ++i) {
        const PlanOlayi& o = plan_.olaylar[i];
        PlanDurumu& d = pd_[i];
        if (o.tur == PlanOlayTuru::sensor && !o.zaman_ns && !d.basladi && !d.sonar_verildi && d.tetik_b >= 0 &&
            static_cast<std::uint64_t>(d.tetik_b) == b && sonar_hedefi(o.ariza.hedef)) {
            sensor::SensorArizasi f = o.ariza;
            f.zaman_ns = t;
            d.sonar_sonuc = sonar_->arm(f);
            d.sonar_verildi = true;
        }
    }
    const std::uint64_t ping_once = sonar_->sayaclar().ping;
    const std::int64_t s0 = gercek();
    sensor::AdimSonucu sr;
    {
        const SonarAyirmaKapsami kapsam;   // W4-K27 (5): ping hesabı bellek ayırmaz
        sr = sonar_->adim(dn_.durum(), b);
    }
    if (sr != sensor::AdimSonucu::tamam) {
        s_.hata = "sonar kümesi adımı başarısız";
        return false;
    }
    const std::int64_t ns = gercek() - s0;
    s_.sonar_adim_toplam_ns += ns;
    s_.sonar_adim_en_uzun_ns = std::max(s_.sonar_adim_en_uzun_ns, ns);
    if (sonar_->sayaclar().ping != ping_once) {
        ++s_.sonar_adim_ping_sinir;
        if (s_.sonar_adim_ping_ns.size() < s_.sonar_adim_ping_ns.capacity()) {
            s_.sonar_adim_ping_ns.push_back(ns);   // yer önceden ayrıldı: döngüde ayırma yok
        }
    }
    return true;
}

// Sınırın sonar kayıtları: boşaltma (t_teslim ≤ t), köprüye verme (Ping1D kritik, Ping360 toplu).
void Calistirici::sonar_adimi(std::int64_t t)
{
    {
        // W4-K27 (5): önceki sınırın kayıtlarının tamponları havuza döner; boşaltma sabit kapasiteli
        // depoya (taşan kayıt düşer, sayılır). Bellek ayırmaz.
        const SonarAyirmaKapsami kapsam;
        sonar_->geri_al(sonar_olcumler_);
        s_.sonar_teslim_tasma += sonar_->bosalt(t, sonar_olcumler_, sonar_olcumler_.capacity());
    }
    for (const sonar::SonarOlcumu& o : sonar_olcumler_) {
        if (a_.sonar_kancasi) {
            a_.sonar_kancasi(o);
        }
        if (const auto* p = std::get_if<sonar::Ping1dOlcumu>(&o.yuk)) {
            kopru::Ping1dVerisi v;
            v.distance = p->distance;
            v.scan_start = p->scan_start;
            v.scan_length = p->scan_length;
            v.ping_number = p->ping_number;
            v.tx_offset_ns = p->tx_offset_ns;
            v.confidence = p->confidence;
            k_->ping1d_ekle(o.hdr, v);
            ++s_.sonar_ping1d;
        } else if (const auto* q = std::get_if<sonar::Ping360Olcumu>(&o.yuk)) {
            kopru::Ping360Verisi v;
            v.angle_grad = q->angle_grad;
            v.n_samples = static_cast<std::uint16_t>(q->data.size());
            v.sample_period = q->sample_period;
            v.tx_duration_us = q->tx_duration_us;
            v.tx_freq_khz = q->tx_freq_khz;
            v.gain = q->gain;
            v.data = q->data.data();
            k_->ping360_ekle(o.hdr, v);
            ++s_.sonar_ping360;
        }
        if (!a_.sonar_csv_yolu.empty()) {
            sonar_csv_ekle(o);
        }
    }
}

void Calistirici::sonar_csv_ekle(const sonar::SonarOlcumu& o)
{
    const sensor::OlcumBasligi& h = o.hdr;
    std::string& c = s_.sonar_csv;
    const unsigned long long t_tx = h.t_tx_ns == sensor::kTxYok ? 0ULL : static_cast<unsigned long long>(h.t_tx_ns);
    if (const auto* p = std::get_if<sonar::Ping1dOlcumu>(&o.yuk)) {
        ekle(c, "ping1d,%u,%u,%u,%lld,%llu,%lld,%lld,%u,%u,%.9g,%u,%.9g,%.9g,%u,%u,,,,,,,%zu,%08x\n",
             static_cast<unsigned>(h.sensor_id), static_cast<unsigned>(h.inst), h.seq, static_cast<long long>(h.t_meas_ns),
             t_tx, static_cast<long long>(h.t_hazir_ns), static_cast<long long>(h.t_teslim_ns),
             static_cast<unsigned>(h.flags), h.ariza_maskesi, static_cast<double>(p->distance),
             static_cast<unsigned>(p->confidence), static_cast<double>(p->scan_start), static_cast<double>(p->scan_length),
             p->ping_number, p->tx_offset_ns, p->profil.size(),
             kopru::crc32_iso_hdlc(p->profil.data(), p->profil.size()));
    } else if (const auto* q = std::get_if<sonar::Ping360Olcumu>(&o.yuk)) {
        ekle(c, "ping360,%u,%u,%u,%lld,%llu,%lld,%lld,%u,%u,,,,,,,%u,%u,%u,%u,%u,%u,%zu,%08x\n",
             static_cast<unsigned>(h.sensor_id), static_cast<unsigned>(h.inst), h.seq, static_cast<long long>(h.t_meas_ns),
             t_tx, static_cast<long long>(h.t_hazir_ns), static_cast<long long>(h.t_teslim_ns),
             static_cast<unsigned>(h.flags), h.ariza_maskesi, static_cast<unsigned>(q->angle_grad),
             static_cast<unsigned>(q->n_samples), static_cast<unsigned>(q->sample_period),
             static_cast<unsigned>(q->tx_duration_us), static_cast<unsigned>(q->tx_freq_khz),
             static_cast<unsigned>(q->gain), q->data.size(), kopru::crc32_iso_hdlc(q->data.data(), q->data.size()));
    }
}

void Calistirici::adim_sonrasi(std::uint64_t b)
{
    const fizik::DurumAnlik& d = dn_.durum();
    // A5-K63: koşul her fizik adımı sonunda değerlendirilir; ilk sağlandığı adımı izleyen
    // sınırda (b + 1) olay bir kez başlar.
    for (std::size_t i = 0; i < plan_.olaylar.size(); ++i) {
        const PlanOlayi& o = plan_.olaylar[i];
        if (o.kosul && !pd_[i].basladi && pd_[i].tetik_b < 0 && o.kosul->sagliyor(d.konum_n[2])) {
            pd_[i].tetik_b = static_cast<std::int64_t>(b + 1u);
        }
    }
    const double notr = static_cast<double>(arac_.esc.pwm_notr_us);
    bool hepsi_notr = true;
    double en_buyuk_itki = 0.0;
    double en_buyuk_istek = 0.0;
    for (std::size_t i = 0; i < 8; ++i) {
        hepsi_notr = hepsi_notr && d.pwm_us[i] == notr;
        en_buyuk_itki = std::max(en_buyuk_itki, std::abs(d.itki[i]));
        en_buyuk_istek = std::max(en_buyuk_istek, std::abs(d.itki_istegi[i]));
    }
    if (estop_bu_adim_) {
        ++s_.estop_adim;   // acil durdurma etkin geçen fizik adımı (duraklatma turları sayılmaz)
    }
    if (estop_bu_adim_ && (!hepsi_notr || en_buyuk_istek != 0.0)) {
        ++s_.ihlal_estop;
    }
    if (s_.t_disarm_ns >= 0) {
        const std::int64_t t_b = static_cast<std::int64_t>(b) * kH;
        if (hepsi_notr && s_.t_pwm_notr_ns < 0) {
            s_.t_pwm_notr_ns = t_b;
        }
        if (!hepsi_notr) {
            s_.t_pwm_notr_ns = -1;
        }
        if (en_buyuk_itki < 0.05 && s_.t_itki_sifir_ns < 0) {
            s_.t_itki_sifir_ns = d.t_ns;
        }
        if (en_buyuk_itki >= 0.05) {
            s_.t_itki_sifir_ns = -1;
        }
    }
}

void Calistirici::satir_ekle(std::uint64_t b, std::optional<std::uint32_t> seq)
{
    if (a_.kip == Kip::hil) {
        satirlari_tamamla(0);
    }
    const fizik::DurumAnlik& d = dn_.durum();
    Satir r;
    r.t_s = static_cast<double>(b) * 1.25e-3;
    r.parti = seq.value_or(0u);
    r.parti_var = seq.has_value();
    r.derinlik = d.konum_n[2];
    const Euler e = euler(d.yonelim[0], d.yonelim[1], d.yonelim[2], d.yonelim[3]);
    r.yon = e.yon;
    r.yalpa = e.yalpa;
    r.bas_kic = e.bas_kic;
    for (std::size_t i = 0; i < 8; ++i) {
        r.pwm_yur[i] = static_cast<std::int32_t>(son_pwm_yur_[i]);
        r.pwm_fiz[i] = static_cast<std::int32_t>(d.pwm_us[i]);
        r.itki_istegi[i] = d.itki_istegi[i];
        r.pwm_uret[i] = arac_.esc.pwm_notr_us;
    }
    r.tau_z = d.itici_kuvvet_momenti[2];
    r.tau_n = d.itici_kuvvet_momenti[5];
    r.estop = onceki_estop_ ? 1u : 0u;
    r.itki_carpani = fizik_carpan_;
    s_.satirlar.push_back(r);
    bekleyen_satir_.push_back(s_.satirlar.size() - 1u);
}

// Kart tarafı alanları bir parti sonra doldurulur. HIL: sonraki partinin gönderiminde (yanıt ve
// telemetri o ana kadar gelmiştir). SIL (W4-K14): sonraki partinin ACTUATOR'ı alındıktan sonra;
// köprü n. çağrının ACTUATOR dışı çıktısını ACTUATOR(n + 1) ile işler, böylece satır n çağrı
// n'in telemetrisini geliş zamanından bağımsız taşır. Son satır kapanıştan önce, o anda
// işlenmiş durumla (ACTUATOR(N) ve çağrı N − 1'in çıktısı) tamamlanır.
void Calistirici::satir_tamamla(Satir& r)
{
    const auto y = r.parti_var ? k_->yanit(r.parti) : std::nullopt;
    if (y) {
        r.yanit_var = 1;
        r.thr_valid = (y->act_flags & kActThr) != 0u ? 1u : 0u;
        r.is_safe = (y->act_flags & kActSafe) != 0u ? 1u : 0u;
        r.armed = (y->act_flags & kActArmed) != 0u ? 1u : 0u;
        r.fs_level = y->fs_level;
        r.rc = y->core_rc;
        for (std::size_t i = 0; i < 8; ++i) {
            r.pwm_uret[i] = r.thr_valid != 0u ? static_cast<std::int32_t>(y->pwm_us[i]) : arac_.esc.pwm_notr_us;
        }
    }
    const kopru::KartBilgisi& kb = k_->kart();
    if (kb.nav.var) {
        r.derinlik_k = static_cast<double>(kb.nav.depth);
        const Euler e = euler(static_cast<double>(kb.nav.q_nb[0]), static_cast<double>(kb.nav.q_nb[1]),
                              static_cast<double>(kb.nav.q_nb[2]), static_cast<double>(kb.nav.q_nb[3]));
        r.yon_k = e.yon;
        r.yalpa_k = e.yalpa;
        r.bas_kic_k = e.bas_kic;
        r.est_level = kb.nav.est_level;
    }
    if (kb.durum.var) {
        r.kip = kb.durum.mode;
        r.fs_reasons = kb.durum.fs_reasons;
    }
    r.sess = k_->sess();
    r.kacirilan = k_->sayaclar().parti_kacirildi;
    r.gecersiz = k_->sayaclar().calistirma_gecersiz;
    if (y) {
        r.yanit_suresi_ns = y->yanit_suresi_ns;
    }
    r.fc_state = kb.fc_state;
    r.fc_inhibit = kb.fc_inhibit_mask;
    r.sim_izin = static_cast<std::uint8_t>((k_->izin_acik(0) ? 1u : 0u) | (k_->izin_acik(1) ? 2u : 0u));
    if (a_.csv_uret) {
        csv_yaz(r);
    }
}

void Calistirici::csv_yaz(const Satir& r)
{
    std::string& c = s_.csv;
    ekle(c, "%.4f,%u,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f", r.t_s, r.parti, r.derinlik, r.derinlik_k, r.yon,
         r.yon_k, r.yalpa, r.yalpa_k, r.bas_kic, r.bas_kic_k);
    for (std::int32_t v : r.pwm_uret) {
        ekle(c, ",%d", v);
    }
    for (std::int32_t v : r.pwm_yur) {
        ekle(c, ",%d", v);
    }
    for (std::int32_t v : r.pwm_fiz) {
        ekle(c, ",%d", v);
    }
    ekle(c, ",%u,%u,%u,0x%x,%u,%u,%u,%u,%d,%u,%u,%llu,0x%x\n", static_cast<unsigned>(r.kip),
         static_cast<unsigned>(r.armed), static_cast<unsigned>(r.fs_level), static_cast<unsigned>(r.fs_reasons),
         static_cast<unsigned>(r.est_level), static_cast<unsigned>(r.thr_valid), static_cast<unsigned>(r.is_safe),
         static_cast<unsigned>(r.yanit_var), r.rc, static_cast<unsigned>(r.estop), static_cast<unsigned>(r.sess),
         static_cast<unsigned long long>(r.kacirilan), static_cast<unsigned>(r.gecersiz));
    c.pop_back();   // satır sonu: ek sütunlar
    ekle(c, ",%lld,%u,0x%x,%u\n", static_cast<long long>(r.yanit_suresi_ns / 1000), static_cast<unsigned>(r.fc_state),
         static_cast<unsigned>(r.fc_inhibit), static_cast<unsigned>(r.sim_izin));
}

// HIL evre ölçümünün özeti: evre başına p50/p99/en çok (µs) ve TF'nin en geç yazıldığı beş
// periyotta (TF sınırı ve önceki üç sınır) evrelerin toplamı.
void Calistirici::evre_ozeti_kur()
{
    std::string& o = s_.evre_ozeti;
    o = "evre (µs): p50 / p99 / en çok\n";
    std::vector<std::int64_t> v;
    v.reserve(profil_.size());
    for (std::size_t e = 0; e < kEvreSayisi; ++e) {
        v.clear();
        for (const auto& p : profil_) {
            v.push_back(p[e]);
        }
        std::sort(v.begin(), v.end());
        ekle(o, "  %-10s %8.1f %8.1f %9.1f\n", kEvreAdi[e], 1e-3 * static_cast<double>(v[v.size() / 2]),
             1e-3 * static_cast<double>(v[std::min(v.size() - 1, (v.size() * 99) / 100)]),
             1e-3 * static_cast<double>(v.back()));
    }
    std::vector<std::pair<std::int64_t, std::uint64_t>> g = tf_gec_;
    std::sort(g.begin(), g.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
    ekle(o, "en geç beş TF (geç yazım µs; o periyodun evre toplamları µs):\n");
    for (std::size_t i = 0; i < std::min<std::size_t>(5, g.size()); ++i) {
        const std::uint64_t b = g[i].second;
        std::array<std::int64_t, kEvreSayisi> top{};
        for (std::uint64_t j = (b >= 3 ? b - 3 : 0); j <= b && j < profil_.size(); ++j) {
            for (std::size_t e = 0; e < kEvreSayisi; ++e) {
                top[e] += profil_[static_cast<std::size_t>(j)][e];
            }
        }
        ekle(o, "  t=%.3f s geç %.1f:", 1.25e-3 * static_cast<double>(b), 1e-3 * static_cast<double>(g[i].first));
        for (std::size_t e = 0; e < kEvreSayisi; ++e) {
            ekle(o, " %s %.1f", kEvreAdi[e], 1e-3 * static_cast<double>(top[e]));
        }
        o += "\n";
    }
}

// ======================================================================== A3 (W4-bagla-a3)
//
// Dünya iş parçacığının A3 tarafı (W4-K27 (5)): istek çekme, yayın hazırlığı ve yayın bellek
// ayırmaz ve kilit almaz. Kalıcı depolar (istek vektörleri, kayıt tamponları, arıza tabloları)
// A3 başlarken ayrılır; değişmeyen içerik (run_id, senaryo özeti, arıza hedefleri) önceden
// üretilir. Köprü ve fizik çağrıları (senaryonun kullandığı yollar) bu güvencenin dışındadır.
// Yayın çağrıları fizik ve köprü sırasını değiştirmez.

namespace {

constexpr std::uint64_t kYok64 = std::numeric_limits<std::uint64_t>::max();
constexpr std::uint64_t kHizliSinir = 16;    // 50 Hz sim zamanı (fizik/durum, kart/durum)
constexpr std::uint64_t kDurumSinir = 80;    // 10 Hz (A3 Tablo 3.3 varsayılanları)
constexpr std::uint64_t kYavasSinir = 400;   // 2 Hz
constexpr std::size_t kGenelAzami = 512;     // dünya tarafında bekleyen genel istek (halka 256 + hazırlıkta biriken)
constexpr std::size_t kKayitAzami = 65536;   // Sonuc'a yazılacak A3 komut ve emniyet kayıtları

// W4-K27 (5): ölçüm ve sınama kapsamı. `g_a3_derinlik` dünya tarafındaki A3 hizmetinin iç içe
// derinliği (süre en dıştakinde ölçülür); `g_a3_sayim` > 0 iken bellek ayırması A3 hizmetine
// sayılır (köprü ve fizik çağrıları sırasında 0).
thread_local int g_a3_derinlik = 0;
thread_local int g_a3_sayim = 0;

class A3Hizmet {
public:
    explicit A3Hizmet(std::int64_t& toplam) : toplam_(toplam), dis_(g_a3_derinlik == 0)
    {
        if (dis_) {
            bas_ = Saat::now();
        }
        ++g_a3_derinlik;
        ++g_a3_sayim;
    }
    ~A3Hizmet()
    {
        --g_a3_sayim;
        --g_a3_derinlik;
        if (dis_) {
            toplam_ += std::chrono::duration_cast<std::chrono::nanoseconds>(Saat::now() - bas_).count();
        }
    }
    A3Hizmet(const A3Hizmet&) = delete;
    A3Hizmet& operator=(const A3Hizmet&) = delete;

private:
    std::int64_t& toplam_;
    bool dis_;
    Saat::time_point bas_{};
};

// Köprü/fizik çağrısı: ayırma sayımı dışı (süre A3 hizmetine dahil kalır).
class KopruCagrisi {
public:
    KopruCagrisi() : eski_(g_a3_sayim) { g_a3_sayim = 0; }
    ~KopruCagrisi() { g_a3_sayim = eski_; }
    KopruCagrisi(const KopruCagrisi&) = delete;
    KopruCagrisi& operator=(const KopruCagrisi&) = delete;

private:
    int eski_;
};

const char* girdi_durumu(bool takili_ya_da_basili, bool acil)
{
    return acil ? (takili_ya_da_basili ? "basili" : "serbest") : (takili_ya_da_basili ? "takili" : "cikarilmis");
}

const char* calistirma_adi(a3::KomutTuru t)
{
    switch (t) {
    case a3::KomutTuru::calistirma_baslat: return "calistirma/baslat";
    case a3::KomutTuru::calistirma_duraklat: return "calistirma/duraklat";
    case a3::KomutTuru::calistirma_surdur: return "calistirma/surdur";
    case a3::KomutTuru::calistirma_adimla: return "calistirma/adimla";
    case a3::KomutTuru::calistirma_hiz: return "calistirma/hiz";
    case a3::KomutTuru::calistirma_durdur: return "calistirma/durdur";
    case a3::KomutTuru::calistirma_sifirla: return "calistirma/sifirla";
    case a3::KomutTuru::senaryo_yukle: return "senaryo/yukle";
    case a3::KomutTuru::emniyet_degistir: return "emniyet/degistir";
    case a3::KomutTuru::yer_istasyonu_komut: return "yer_istasyonu";
    }
    return "?";
}

// Panelin emniyet isteğinin köprü ataması (operatör isteği; tek alan).
kopru::EmniyetAtama operator_atamasi(const a3::Komut& k)
{
    kopru::EmniyetAtama e;
    const auto t = static_cast<std::size_t>(k.tup == 1 ? 1 : 0);
    switch (k.girdi) {
    case a3::EmniyetGirdisi::acil_durdurma: e.acil_durdurma = k.hedef_durum == a3::GirdiDurumu::basili; break;
    case a3::EmniyetGirdisi::silahlandirma_fisi: e.fis[t] = k.hedef_durum == a3::GirdiDurumu::takili; break;
    case a3::EmniyetGirdisi::emniyet_pimi: e.pim[t] = k.hedef_durum == a3::GirdiDurumu::takili; break;
    }
    e.operator_kimligi = k.komut_no == 0u ? 1u : k.komut_no;
    if (!k.kisitlayan) {
        e.beklenen_rev = k.beklenen_emniyet_rev;
    }
    return e;
}

std::uint8_t a1_oturum_kodu(kopru::OturumDurumu d, bool kopuk)
{
    if (kopuk) {
        return 4u;
    }
    switch (d) {
    case kopru::OturumDurumu::kapali: return 0u;
    case kopru::OturumDurumu::el_sikisma: return 1u;
    case kopru::OturumDurumu::hazir: return 2u;
    case kopru::OturumDurumu::kosuyor: return 3u;
    case kopru::OturumDurumu::reddedildi: return 0u;
    }
    return 0u;
}

const char* a1_oturum_adi(std::uint8_t k)
{
    static constexpr const char* a[] = {"kapali", "el_sikisma", "hazir", "kosuyor", "kopuk"};
    return k < 5u ? a[k] : "kapali";
}

const char* ariza_turu_adi(sensor::ArizaTuru t)
{
    static constexpr const char* a[] = {"ofset", "olcek", "suruklenme_dogrusal", "suruklenme_ustel", "gurultu_artisi",
                                        "sicrama", "donma", "donma_zaman_damgasi", "yanlis_gecerlilik",
                                        "kendini_bildiren", "kayip", "gec_teslim", "motor_takilmasi",
                                        "dvl_isin_kaybi", "guc_cevrimi"};
    const auto i = static_cast<std::size_t>(t);
    return i < sizeof a / sizeof a[0] ? a[i] : "?";
}

template <std::size_t N>
void metin_kopyala(std::array<char, N>& d, std::string_view s)
{
    const std::size_t n = std::min(s.size(), N - 1u);
    for (std::size_t i = 0; i < n; ++i) {
        d[i] = s[i];
    }
    d[n] = '\0';
}

// Sabit tamponda JSON dizesi dizisi (atama_celiskisi; A3-K125 parçaları).
class DiziYaz {
public:
    void ekle(std::string_view s)
    {
        yaz_(n_ == 1u ? "\"" : ",\"");
        for (const char c : s) {
            if (static_cast<unsigned char>(c) < 0x20u) {
                continue;
            }
            if (c == '"' || c == '\\') {
                yaz_("\\");
            }
            yaz_(std::string_view(&c, 1));
        }
        yaz_("\"");
    }
    std::string_view bitir()
    {
        yaz_("]");
        return tasti_ ? std::string_view("[]") : std::string_view(t_.data(), n_);
    }

private:
    void yaz_(std::string_view s)
    {
        if (tasti_ || s.size() > t_.size() - n_) {
            tasti_ = true;
            return;
        }
        for (const char c : s) {
            t_[n_] = c;
            ++n_;
        }
    }
    std::array<char, 768> t_{'['};
    std::size_t n_ = 1;
    bool tasti_ = false;
};

template <typename Y>
void olcum_basligi(Y& y, const sensor::OlcumBasligi& h)
{
    y.sensor_id = h.sensor_id;
    y.inst = h.inst;
    y.seq = h.seq;
    y.flags = h.flags;
    y.window_ns = static_cast<std::int64_t>(h.window_ns);
    y.t_tx_ns = h.t_tx_ns;
    y.t_hazir_ns = h.t_hazir_ns;
    y.t_teslim_ns = h.t_teslim_ns;
    // parti_step_seq: köprü kaydın girdiği partiyi dışa vermiyor (kayabilir: A1-K113) → yok.
    y.fault_mask = h.ariza_maskesi;
    // quality: sensör modeli kalite üretmiyor → yok (NaN, varsayılan).
    y.model_version = h.model_surumu;
}

}  // namespace

template <typename F>
void Calistirici::a3_olc(F&& f)
{
    const auto bas = Saat::now();
    f();
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(Saat::now() - bas).count();
    ++s_.a3_yayin_cagrisi;
    s_.a3_cagri_en_kotu_ns = std::max<std::int64_t>(s_.a3_cagri_en_kotu_ns, ns);
}

bool Calistirici::a3_run_uyar(const a3::Komut& k) const
{
    if (!a3d_.run_var) {
        return !k.beklenen_run_id.has_value();
    }
    return k.beklenen_run_id.has_value() && *k.beklenen_run_id == a3_run_id_;
}

void Calistirici::a3_olay(std::uint16_t tur, std::uint8_t onem, std::uint8_t kaynak, std::uint32_t komut_no,
                          std::uint32_t istemci, std::string_view ayrinti)
{
    if (yay_ == nullptr) {
        return;
    }
    a3::yuk_olay o;
    o.tur = tur;
    o.onem = onem;
    o.kaynak = kaynak;
    o.adim_no = a3d_.durum == a3::CalistirmaDurumu::kosuyor || a3d_.durum == a3::CalistirmaDurumu::duraklatildi
                    ? a3_b_
                    : kYok64;
    o.komut_no = komut_no;
    o.istemci_id = istemci;
    o.ayrinti = ayrinti;
    const std::int64_t t = static_cast<std::int64_t>(a3_b_) * kH;
    a3_olc([&] { (void)yay_->olay(t, o); });
    if (kb_) {
        kayit_yaz<kayit::Kanal::olay>(t, a3_b_, o);   // aynı yapı (W5-K42)
    }
}

void Calistirici::a3_sonuc(const A3Giris& g, bool uygulandi, std::uint64_t b, const char* kod, std::string_view ayrinti)
{
    if (yay_ == nullptr) {
        return;
    }
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    a3::KomutSonucuGirdisi s;
    s.komut_no = g.k.komut_no;
    s.uygulandi = uygulandi;
    s.uygulama_adimi_var = uygulandi;
    s.uygulama_adimi = b;
    s.t_ns = t;
    s.adim_no = b;
    s.hata_kodu = kod != nullptr ? std::string_view(kod) : std::string_view();
    s.sonuc_ayrinti_json = ayrinti;
    a3_olc([&] { (void)yay_->komut_sonucu(s); });
    if (g.k.tur == a3::KomutTuru::senaryo_yukle && g.k.senaryo_aday != 0u && kalici_ != nullptr && kalici_->hizmet) {
        // W5-a3-senaryo: bekleyen `senaryo/yukle` çağrısı sonuç olay halkasına yazıldıktan sonra serbest kalır.
        if (uygulandi) {
            kalici_->hizmet->yukleme_tamam(g.k.senaryo_aday);
        } else {
            kalici_->hizmet->yukleme_reddedildi(g.k.senaryo_aday, kod != nullptr ? kod : "sunucu_hatasi", a3_ret_ileti_);
            a3_ret_ileti_.clear();
        }
    }
    if (kb_) {
        // `a3/komut_sonucu` A6'ya özgü yüktür (A3 kanalı değil): A3'ün sonuç girdisinden kurulur.
        kayit::yuk_a3_komut_sonucu ks;
        ks.komut_no = g.k.komut_no;
        ks.sonuc = uygulandi ? 0u : 1u;
        ks.uygulama_adimi = uygulandi ? b : ks.uygulama_adimi;
        ks.hata_kodu = s.hata_kodu;
        ks.ayrinti = ayrinti.empty() ? std::string_view("{}") : ayrinti;
        kayit_yaz<kayit::Kanal::a3_komut_sonucu>(t, b, ks);
        if (uygulandi) {
            kayit_taslak_hemen();   // A6-K53: sonucu değiştirebilecek bir komut uygulandı; 1 Hz'i beklenmez
        }
    }
    // Kapsam 4: A3'ten gelen komutlar olay kaydına senaryodakilerle aynı biçimde, kaynağıyla
    // (önceden ayrılmış kayıt tamponuna; Sonuc'a koşu sonunda çevrilir).
    if (a3_komut_kaydi_.size() < kKayitAzami) {
        a3_komut_kaydi_.push_back({g.k.komut_no, g.k.tur, g.yi.ad, t, uygulandi, kod});
    } else {
        ++s_.a3_kayit_atilan;
    }
    ++s_.a3_komut;
}

void Calistirici::a3_gecis(a3::CalistirmaDurumu yeni, const char* neden, bool ic)
{
    if (yay_ == nullptr) {
        return;
    }
    const a3::CalistirmaDurumu eski = a3d_.durum;
    a3d_.durum = yeni;
    if (ic) {
        ++a3d_.durum_rev;   // A3-K122: iç geçiş (komutun kendi artışı uygulandığı yerde)
    }
    if (yeni == a3::CalistirmaDurumu::yuklendi) {
        a3d_.run_var = false;   // A3-K124
    }
    a3b::JsonYaz j;
    j.metin("onceki", a3b::durum_adi(eski)).metin("sonraki", a3b::durum_adi(yeni));
    if (neden != nullptr) {
        j.metin("neden", neden);
    }
    a3_olay(1, 0, 0, 0, 0, j.bitir());
    a3_durum_kirli_ = true;
    a3_durum_yayinla(static_cast<std::int64_t>(a3_b_) * kH);
    a3_anlik_guncelle();
}

// Anlık görüntü (kilitsiz üçlü tampon; bellek ayırmaz).
void Calistirici::a3_anlik_guncelle()
{
    if (a3al_ == nullptr) {
        return;
    }
    AnlikPod& p = a3p_;
    p.durum = static_cast<std::uint8_t>(a3d_.durum);
    p.kip = static_cast<std::uint8_t>(a_.kip == Kip::hil ? a3::ZamanKipi::hil_1x : a3::ZamanKipi::sil);
    p.run_var = a3d_.run_var;
    // `sifirla`/`senaryo/yukle` kabul edildi: dünya yeniden kurulacak; görüntü, yeni bir dünyanın `yuklendi`
    // görüntüsüdür (sim zamanı, sayaçlar, geçerlilik ve emniyet durumu başlangıç değerinde). Bayat değer
    // (ör. basılı acil durdurma) kısa bir süre de olsa yeni durumun yanında görünmez.
    const bool temiz = a3_yeniden_.has_value();
    p.sim_t_ns = temiz ? 0 : static_cast<std::int64_t>(a3_b_) * kH;
    p.adim_no = temiz ? 0u : a3_b_;
    p.gecerli = temiz || a3_gecerli_;
    p.gercek_zamani_kacirdi = !temiz && a3_rt_kacirdi_;
    p.gecerlilik_nedenleri = temiz ? 0u : a3_gecerlilik_;
    p.durum_rev = a3d_.durum_rev;
    p.kademe_sonar = sonar_ ? sonar::kKademeSade : 0u;   // W4-bagla-sonar: 0 kapali, 1 sade
    // W5-bagla-a2: yalnız A2 gerçekten dinliyorken senaryonun kademesi; nesne var ama dinlemiyorsa
    // (kilit başkasında, port dolu, tanım yok) görünüm kapalıdır: 0.
    p.kademe_gorunum = (a2_ && a2_->dinliyor()) ? static_cast<std::uint8_t>(a2_->kademe()) : std::uint8_t{0};
    // W5-K58: geçiş penceresinde (yeni dünya `yuklendi` olana dek) yüklü senaryo yoktur: özet null yayınlanır
    // ve giriş denetimi `baslat`'ı reddeder (A3Alici::calistirma_denetle_). Yeni dünya kendi özetiyle ve
    // `kuruluyor = false` ile yazar (a3_baslat).
    p.kuruluyor = temiz;
    if (temiz) {
        p.a1_run_id = 0;
        p.sayac = {};
        p.ozet_var = false;
    }
    if (k_ && !temiz) {
        const sensor::EmniyetDurumu e = k_->dis_durum().emniyet;
        p.emniyet_rev = e.emniyet_revizyonu;
        p.emniyet_rev_doydu = k_->emniyet_rev_doydu();
        p.acil = e.acil_durdurma ? 1u : 0u;
        p.fis = {static_cast<std::uint8_t>(e.fis[0] ? 1u : 0u), static_cast<std::uint8_t>(e.fis[1] ? 1u : 0u)};
        p.pim = {static_cast<std::uint8_t>(e.pim[0] ? 1u : 0u), static_cast<std::uint8_t>(e.pim[1] ? 1u : 0u)};
        p.a1_run_id = k_->run_id();
        const kopru::Sayaclar& c = k_->sayaclar();
        p.sayac = {static_cast<std::int64_t>(c.parti_gonderilen), static_cast<std::int64_t>(c.parti_kacirildi),
                   static_cast<std::int64_t>(c.oturum_kopma), static_cast<std::int64_t>(s_.zaman.sim_gecikmesi),
                   static_cast<std::int64_t>(yay_ != nullptr ? yay_->halka_tasma() : 0u)};
    } else {
        // W5-K55 (A3-K117): revizyon geri dönmez. Köprü varsa (geçiş penceresi: eski dünyanın köprüsü, oturum
        // sonu artışı uygulandıkça) onun değeri; köprü yokken (`yuklendi`) süreç boyunca yaşayan değer.
        p.emniyet_rev = k_ ? k_->emniyet_revizyonu() : ilk_emniyet_rev();
        p.emniyet_rev_doydu = k_ ? k_->emniyet_rev_doydu() : ilk_emniyet_rev_doydu();
        p.acil = plan_.acil_durdurma ? 1u : 0u;
        p.fis = {static_cast<std::uint8_t>(plan_.fis[0] ? 1u : 0u), static_cast<std::uint8_t>(plan_.fis[1] ? 1u : 0u)};
        p.pim = {static_cast<std::uint8_t>(plan_.pim[0] ? 1u : 0u), static_cast<std::uint8_t>(plan_.pim[1] ? 1u : 0u)};
    }
    p.son_degisim_var = !temiz && a3_son_degisim_adimi_ != kYok64;
    p.son_degisim_adimi = temiz ? 0u : a3_son_degisim_adimi_;
    a3al_->anlik_yaz(p);
    a3_anlik_kirli_ = false;
}

// W5-bagla-a2 (W5-K31): A2 sunucusunun başlatılması. false: koşu başlamaz (A2-K93: geçersiz port).
// Görünüm kademesi kapalıysa (A2-K02) ya da çalıştırma tanımı kurulamadıysa dinleyici açılmaz, kilit
// alınmaz ve neden tek satır yazılır; kilit ya da port alınamazsa koşu A2'siz sürer (A2-K05, K79).
bool Calistirici::a2_baslat()
{
    if (a2_tanim_sonucu_ == a2b::TanimSonucu::gorunum_kapali) {
        s_.a2.durum = "gorunum_kapali";
        s_.a2.aciklama = "kalite.gorunum = kapali";
        gunluk(1, "A2: kalite.gorunum = \"kapali\"; dinleyici açılmadı (A2-K02)");
        return true;
    }
    if (a2_tanim_sonucu_ != a2b::TanimSonucu::tamam) {
        s_.a2.durum = "tanim_yok";
        s_.a2.aciklama = a2_tanim_hatasi_;
        gunluk(1, "A2: çalıştırma tanımı kurulamadı (" + a2_tanim_hatasi_ + "); A2'siz çalışılıyor");
        return true;
    }
    A2BaglantiAyari ba;
    ba.port = a_.a2_port;
    ba.sinama_bos_port = a_.a2_sinama_bos_port;
    ba.kilit_dosyasi = a_.a2_kilit_dosyasi;
    ba.gonderme_kuyrugu_bayt = a_.a2_gonderme_kuyrugu_bayt;
    const int gunluk_duzeyi = a_.gunluk;
    // Sunucunun günlüğü (G/Ç iş parçacığından da çağrılır; dünya iş parçacığından çağrılmaz).
    ba.gunluk = [gunluk_duzeyi](std::string_view m) {
        if (gunluk_duzeyi >= 1) {
            std::fprintf(stderr, "ida_sim: %.*s\n", static_cast<int>(m.size()), m.data());
        }
    };
    s_.a2.kademe = static_cast<std::uint8_t>(a2_tanim_.kademe);
    if (kalici_ != nullptr && kalici_->a2) {
        // W5-a3-senaryo (W5-K49): süreçteki A2 sunucusu önceki dünyadan kalır (kilit, dinleyici, bağlı
        // görselleştirici); yeni dünyanın tanımı verilir ve `calistirma_basladi` A2 çalıştırma nesli artırır.
        a2_ = kalici_->a2.get();
        a2_->yeni_tanim(a2_tanim_);
        s_.a2.durum = a2_->durum_adi();
        s_.a2.aciklama = a2_->aciklama();
        s_.a2.port = a2_->port();
        return true;
    }
    std::string hata;
    a2_sahip_ = A2Baglanti::baslat(ba, a2_tanim_, hata);
    if (!a2_sahip_) {
        s_.hata = hata;
        s_.hata_turu = HataTuru::kullanim;
        return false;
    }
    if (kalici_ != nullptr) {
        kalici_->a2 = std::move(a2_sahip_);
        a2_ = kalici_->a2.get();
    } else {
        a2_ = a2_sahip_.get();
    }
    s_.a2.durum = a2_->durum_adi();
    s_.a2.aciklama = a2_->aciklama();
    s_.a2.port = a2_->port();
    if (a2_->dinliyor()) {
        gunluk(1, a2_->aciklama());   // A2'sizken nedeni sunucu kendi günlüğüne yazdı (tek satır)
    }
    if (a_.a2_hazir) {
        a_.a2_hazir(a2_->port(), a2_->dinliyor());
    }
    return true;
}

// Koşunun her çıkışında (kos): çalıştırma bitti → `calistirma_durdu()`; süreç kapanıyor →
// `kapaniyor()` + `durdur()`. Durum ve sayaçlar döndürülen sonuca yazılır.
void Calistirici::a2_kapat(Sonuc& r, bool son) noexcept
{
    if (a2_) {
        a2_->kapat(r.a2, son);
    }
}

bool Calistirici::a3_baslat()
{
    // W5-a3-senaryo: A3 sunucusu ve komut alıcısı süreçteki ilk dünyada kurulur, sonraki dünyalar ona bağlanır
    // (port, erişim jetonu, olay sırası, komut numaraları ve bağlı istemciler dünyalar arasında sürer).
    const bool ilk_dunya = !kalici_->sunucu;
    if (ilk_dunya) {
        kalici_->alici = std::make_unique<A3Alici>(a_.kip == Kip::hil);
        a3::SunucuAyari sa;
        sa.port = a_.a3_port;
        sa.statik_kok = a_.a3_kok;
        sa.sim_surumu = "ida_sim";
        sa.senaryo = kalici_->hizmet.get();
        const int gunluk_duzeyi = a_.gunluk;
        sa.gunluk = [gunluk_duzeyi](std::string_view m) {
            if (gunluk_duzeyi >= 1) {
                std::fprintf(stderr, "ida_sim: %.*s\n", static_cast<int>(m.size()), m.data());
            }
        };
        std::string hata;
        kalici_->sunucu = a3::Sunucu::baslat(sa, *kalici_->alici, hata);
        if (!kalici_->sunucu) {
            s_.hata = "A3 sunucusu başlatılamadı: " + hata;
            kalici_->alici.reset();
            return false;
        }
    } else {
        kalici_->alici->kip_ayarla(a_.kip == Kip::hil);
    }
    a3al_ = kalici_->alici.get();
    a3sv_ = kalici_->sunucu.get();
    yay_ = &a3sv_->yayinci();
    // W4-K27 (5): dünya tarafının kalıcı depoları burada ayrılır; değişmeyen içerik önceden üretilir.
    a3_kisit_.reserve(A3Alici::kKisitYuva + 4u);
    a3_genel_.reserve(kGenelAzami + A3Alici::kGenelKuyruk + 2u * A3Alici::kDenetimYolu);
    a3_emniyet_bekleyen_.reserve(kGenelAzami + A3Alici::kKisitYuva);
    a3_atama_.reserve(kGenelAzami + A3Alici::kKisitYuva);
    a3_op_sonuc_.resize(kGenelAzami + A3Alici::kKisitYuva);
    a3_degisen_.reserve(kGenelAzami + A3Alici::kKisitYuva);
    a3_kararlar_.reserve(1024);
    a3_komut_kaydi_.reserve(kKayitAzami);
    a3_emniyet_kaydi_.reserve(kKayitAzami);
    if (!s_.davranis_ozeti.empty()) {
        a3p_.ozet_var = true;
        metin_kopyala(a3p_.ozet, "sha256:" + s_.davranis_ozeti);
    }
    for (std::size_t i = 0; i < plan_.olaylar.size(); ++i) {
        const PlanOlayi& o = plan_.olaylar[i];
        if (o.tur == PlanOlayTuru::sensor) {
            a3_arizalar_.push_back({i, o.ariza.hedef, ariza_turu_adi(o.ariza.tur), 0u, false});
        } else if (o.tur == PlanOlayTuru::itici) {
            std::string h;
            for (int c = 0; c < 8; ++c) {
                if ((o.kanal_maskesi & (1u << c)) != 0u) {
                    h += (h.empty() ? "t" : ",t") + std::to_string(c + 1);
                }
            }
            a3_arizalar_.push_back({i, h, "itki_carpani", 1u, false});
        }
    }
    a3_ariza_yuk_.reserve(a3_arizalar_.size());
    a3d_ = A3Durum{};
    a3d_.durum_rev = kalici_->durum_rev;   // A3-K122: durum_rev dünyalar arasında da yalnız artar
    if (kalici_->hizmet) {
        kalici_->hizmet->yuklu_ayarla(kanonik_, s_.davranis_ozeti.empty() ? std::string() : "sha256:" + s_.davranis_ozeti);
    }
    a3_anlik_guncelle();
    a3_durum_yayinla(0);
    a3_emniyet_baslangic_yayinla();   // W5-K58
    if (ilk_dunya) {
        gunluk(1, "A3 127.0.0.1:" + std::to_string(a3sv_->port()));
        if (a_.a3_acilis) {
            a_.a3_acilis(a3sv_->acilis_adresi());
        }
        if (a_.a3_hazir) {
            a_.a3_hazir(a3sv_->port(), a3sv_->jeton());
        }
    }
    a3_kuruldu_ = true;
    a3_yukleme_sonucu();
    return true;
}

// W5-K58: dünya `yuklendi` olduğunda (köprü henüz yok) senaryonun başlangıç emniyet değerleriyle bir
// `emniyet/girdiler` karesi yayınlanır. Çekirdek `yuklendi`'ye dönüşte son-değer önbelleğini boşalttığı için
// bu kare olmadan yeni abone hiç kare alamaz, eski abone önceki dünyanın son karesini (ör. basılı acil
// durdurma) göstermeyi sürdürürdü. Değerler `GET /emniyet` ile aynıdır (aynı kaynak: plan ve süreç boyunca
// yaşayan revizyon). Yalnız A3'e gider; kayıt dosyası bu anda açık değildir (çalıştırma başlamadı).
void Calistirici::a3_emniyet_baslangic_yayinla()
{
    a3::yuk_emniyet_girdiler eg;
    eg.acil_durdurma = plan_.acil_durdurma ? 1u : 0u;
    eg.silahlandirma_fisi = {static_cast<std::uint8_t>(plan_.fis[0] ? 1u : 0u), static_cast<std::uint8_t>(plan_.fis[1] ? 1u : 0u)};
    eg.emniyet_pimi = {static_cast<std::uint8_t>(plan_.pim[0] ? 1u : 0u), static_cast<std::uint8_t>(plan_.pim[1] ? 1u : 0u)};
    eg.son_degisim_adimi = a3_son_degisim_adimi_;
    eg.son_degisim_kaynagi = a3_son_degisim_kaynagi_;
    eg.emniyet_rev = ilk_emniyet_rev();
    eg.emniyet_rev_doydu = ilk_emniyet_rev_doydu() ? 1u : 0u;
    a3_olc([&] { (void)yay_->yayinla<a3::Kanal::emniyet_girdiler>(0, eg); });
}

// Önceki dünyanın bıraktığı `senaryo/yukle` isteği bu dünya `yuklendi` olunca sonuçlanır (A3-K77): yeni
// dünya kuruldu, durum ve anlık görüntü yayınlandı. Başarısızsa (yeni senaryo bu dünyada kurulamadı ve
// önceki senaryo geri kuruldu) ret olarak bildirilir. `sifirla`nın sonucunu isteği alan dünya verdi.
void Calistirici::a3_yukleme_sonucu()
{
    if (!kalici_->tamamlanacak) {
        return;
    }
    A3Kalici::Istek ist = std::move(*kalici_->tamamlanacak);
    kalici_->tamamlanacak.reset();
    const std::int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(Saat::now() - ist.kabul).count();
    kalici_->son_yeniden_kurma_ns = ns;
    s_.yeniden_kurma_ns = ns;
    if (ist.tur != A3Kalici::Istek::Tur::yukle) {
        return;
    }
    if (ist.basarisiz) {
        a3_ret_ileti_ = ist.ileti;
        a3_sonuc(ist.giris, false, 0, ist.hata_kodu.c_str());
        return;
    }
    a3b::JsonYaz j;
    const auto g = kalici_->hizmet ? kalici_->hizmet->aday_al(ist.aday) : nullptr;
    if (g && !g->ad.empty()) {
        j.metin("ad", g->ad);
    }
    j.metin("senaryo_ozeti", s_.davranis_ozeti.empty() ? std::string("sha256:") + std::string(64, '0') : "sha256:" + s_.davranis_ozeti);
    a3_olay(13, 0, 0, ist.giris.k.komut_no, ist.giris.k.istemci_id, j.bitir());   // senaryo_yuklendi
    a3_sonuc(ist.giris, true, 0, nullptr);
}

// Tutarlı kesimle çekiş (W4-K27 (2)). `atla`: alınmayacak kısıtlayan yuvalar (hazırlıkta yer
// istasyonu komutları birikmez: yuvasında bekler, kitaplığın mandalı birleştirir).
void Calistirici::a3_cek(std::uint16_t atla)
{
    const std::size_t yer = a3_genel_.size() < kGenelAzami ? kGenelAzami - a3_genel_.size() : 0u;
    a3al_->cek(a3_kisit_, a3_genel_, atla, std::min<std::size_t>(yer, A3Alici::kGenelKuyruk));
    for (const A3Giris& g : a3_kisit_) {
        a3_son_komut_no_ = std::max(a3_son_komut_no_, g.k.komut_no);
    }
    for (const A3Giris& g : a3_genel_) {
        a3_son_komut_no_ = std::max(a3_son_komut_no_, g.k.komut_no);
    }
}

// EMNİYET (W4-K27 (2)): bir gevşetme, kendisinden önce teslim edilmiş (küçük sıra numaralı) ve
// henüz işlenmemiş bir kısıtlayanın önüne geçemez. Kesim bunu sağlar; değişmez burada denetlenir:
// ihlal sayılır ve gevşetme reddedilir (kısıtlayan yön).
bool Calistirici::a3_gevsetme_onu_acik(const A3Giris& g)
{
    const std::uint64_t bekleyen = a3al_->bekleyen_kisit_en_kucuk(A3Alici::kButunYuvalar);
    bool engel = bekleyen != 0u && bekleyen < g.sira;
    for (const A3Giris& r : a3_kisit_) {
        engel = engel || r.sira < g.sira;
    }
    if (engel) {
        ++s_.a3_sira_korumasi;
    }
    return !engel;
}

// `yuklendi`'de `baslat` beklenir (A3-K62); `sifirla` yalnız durum revizyonunu artırır (A3-K67: dünya
// henüz hiç koşmadı, yeniden kurulacak bir şey yoktur); `senaryo/yukle` dünyayı yeni senaryoyla yeniden
// kurdurur (A3-K77). Canlı çalıştırma gerektiren istekler reddedilir (A3-K91).
Calistirici::BaslatBekleme Calistirici::a3_baslat_bekle()
{
    gunluk(1, "A3: çalıştırma panelden `baslat` ile başlayacak");
    std::int64_t son_yayin = 0;
    for (;;) {
        if (sinyal_kesti()) {
            return BaslatBekleme::sinyal;   // W4-sunucu-kucuk: `baslat` beklenirken kapanış sinyali
        }
        if (a_.a3_sinir_kancasi) {
            a_.a3_sinir_kancasi(0, *a3al_);
        }
        a3_cek(0);
        for (const A3Giris& g : a3_kisit_) {
            a3_sonuc(g, false, 0, "canli_calistirma_yok");
        }
        a3_kisit_.clear();
        bool basla = false;
        bool yeniden = false;
        for (A3Giris& g : a3_genel_) {
            const a3::Komut& k = g.k;
            if (basla || !a3b::calistirma_mi(k.tur)) {
                a3_sonuc(g, false, 0, basla ? "gecersiz_durum" : "canli_calistirma_yok");
                continue;
            }
            if (k.beklenen_durum_rev != a3d_.durum_rev) {
                a3_sonuc(g, false, 0, "durum_rev_uyusmuyor", a3b::JsonYaz().sayi_u("guncel_durum_rev", a3d_.durum_rev).bitir());
                continue;
            }
            if (k.beklenen_run_id.has_value()) {
                a3_sonuc(g, false, 0, "beklenen_durum_uyusmuyor");
                continue;
            }
            if (k.tur == a3::KomutTuru::calistirma_baslat) {
                ++a3d_.durum_rev;   // A3-K122; sonuç ön denetimden sonra (A3-K62)
                a3_baslat_komutu_ = std::move(g);
                basla = true;
            } else if (k.tur == a3::KomutTuru::calistirma_sifirla) {
                ++a3d_.durum_rev;
                a3_sonuc(g, true, 0, nullptr);
                a3_durum_kirli_ = true;
            } else if (k.tur == a3::KomutTuru::senaryo_yukle) {
                ++a3d_.durum_rev;   // A3-K122
                a3_yeniden_kabul(g, 0);
                yeniden = true;
                basla = true;       // bu turun öteki istekleri reddedilir (durum_rev değişti)
            } else {
                a3_sonuc(g, false, 0, "gecersiz_durum");
            }
        }
        a3_genel_.clear();
        if (yeniden) {
            return BaslatBekleme::yeniden;
        }
        if (basla) {
            return BaslatBekleme::baslat;
        }
        if (a3_durum_kirli_ || gercek() - son_yayin > 100 * kMs) {
            a3_durum_yayinla(0);
            a3_anlik_guncelle();
            son_yayin = gercek();
        }
        sure_uyu(2 * kMs);
    }
}

// `sifirla` (canlı ya da bitmiş çalıştırma) ve `senaryo/yukle` kabulü (W5-a3-senaryo; A3-K67, K77). Durum
// `yuklendi` olur ve `run_id` null olur (A3-K124); dünya döngüsü `durdur` ile aynı yoldan sonlanır (A1 oturumu
// kapanır, kayıt dosyası `durduruldu` ile kapanır) ve sürücü yeni dünyayı kurar. `durum_rev` çağıranda
// artmıştır. Sıfırlamanın kendisi `emniyet_rev`'i artırmaz (iskelet §3.2); revizyon geri de dönmez: eski
// dünyanın oturum sonu artışından sonraki değer yeni dünyaya geçer (W5-K55; A3-K67, K117).
void Calistirici::a3_yeniden_kabul(const A3Giris& g, std::uint64_t b)
{
    (void)b;
    A3Kalici::Istek ist;
    ist.tur = g.k.tur == a3::KomutTuru::senaryo_yukle ? A3Kalici::Istek::Tur::yukle : A3Kalici::Istek::Tur::sifirla;
    ist.giris = g;
    ist.aday = g.k.senaryo_aday;
    ist.kabul = Saat::now();
    a3_yeniden_ = std::move(ist);
    a3_durdur_ = true;
    adimla_hedef_.reset();
    a3_gecis(a3::CalistirmaDurumu::yuklendi, g.k.tur == a3::KomutTuru::senaryo_yukle ? "senaryo_yukle" : "sifirla", false);
}

// W5-K56: çekilmiş ve henüz köprüye verilmemiş kısıtlayanlar arasında komut numarası `komut_no`'dan küçük olan.
bool Calistirici::a3_onde_kisit_var(std::uint32_t komut_no) const
{
    for (const A3Giris& g : a3_kisit_) {
        if (g.k.komut_no < komut_no) {
            return true;
        }
    }
    return false;
}

// W5-K56 (EMNİYET): ertelenmiş `sifirla`/`senaryo/yukle` yürürlüğe girer. Bu sınırın emniyet adımı (kısıtlayan
// isteklerin köprüde uygulanması ve sonuçları) tamamlandıktan sonra çağrılır. true: dünya döngüsü biter.
bool Calistirici::a3_ertelenen_yeniden_bitir(std::uint64_t b)
{
    if (!a3_yeniden_ertelenen_) {
        return false;
    }
    const A3Giris g = std::move(*a3_yeniden_ertelenen_);
    a3_yeniden_ertelenen_.reset();
    a3_yeniden_kabul(g, b);
    if (g.k.tur != a3::KomutTuru::senaryo_yukle) {
        a3_sonuc(g, true, b, nullptr);   // `senaryo/yukle`nin sonucunu yeni dünya `yuklendi` olunca verir
    }
    return true;
}

// Çalıştırma kontrolü komutları (A3-K61…K66, K122, K123): durumla aynı atomik adımda yeniden
// denetlenir. Öteki istekler sırasıyla yerinde kalır (yerinde sıkıştırma; bellek ayırmaz).
// `ertele` (yalnız a3_sinir_basi; W5-K56): kabul edilen `sifirla`/`senaryo/yukle`, aynı sınırda çekilmiş ve
// komut numarası daha küçük bir kısıtlayan varsa hemen yürürlüğe girmez; bkz. aşağıdaki dal.
void Calistirici::a3_calistirma_isle(std::uint64_t b, bool ertele)
{
    std::size_t yaz = 0;
    for (std::size_t i = 0; i < a3_genel_.size(); ++i) {
        A3Giris& g = a3_genel_[i];
        const a3::Komut& k = g.k;
        if (!a3b::calistirma_mi(k.tur)) {
            if (yaz != i) {
                a3_genel_[yaz] = std::move(g);
            }
            ++yaz;
            continue;
        }
        const bool hil = a_.kip == Kip::hil;
        if (k.beklenen_durum_rev != a3d_.durum_rev) {
            a3_sonuc(g, false, b, "durum_rev_uyusmuyor", a3b::JsonYaz().sayi_u("guncel_durum_rev", a3d_.durum_rev).bitir());
            continue;
        }
        if (!a3_run_uyar(k)) {
            // A3 §3.9: ayrıntı güncel run_id, durum ve durum revizyonu (A3-K54, K123).
            a3b::JsonYaz ay;
            if (a3d_.run_var) {
                ay.metin("guncel_run_id", a3_run_id_);
            } else {
                ay.ham("guncel_run_id", "null");
            }
            ay.metin("guncel_durum", a3b::durum_adi(a3d_.durum)).sayi_u("guncel_durum_rev", a3d_.durum_rev);
            a3_sonuc(g, false, b, "beklenen_durum_uyusmuyor", ay.bitir());
            continue;
        }
        const bool hazirlikta = a3d_.durum == a3::CalistirmaDurumu::hazirlaniyor;
        if (!a3b::gecis_gecerli(a3d_.durum, k.tur)) {
            a3_sonuc(g, false, b, "gecersiz_durum", a3b::JsonYaz().sayi_u("guncel_durum_rev", a3d_.durum_rev).bitir());
            continue;
        }
        if (hil && (k.tur == a3::KomutTuru::calistirma_hiz || k.tur == a3::KomutTuru::calistirma_adimla)) {
            a3_sonuc(g, false, b, "kipte_gecersiz", a3b::JsonYaz().sayi_u("guncel_durum_rev", a3d_.durum_rev).bitir());
            continue;
        }
        if (hil && k.tur == a3::KomutTuru::calistirma_duraklat && !k.gecersizligi_onayla) {
            a3_sonuc(g, false, b, "onay_gerekli");   // Tablo 4.7b (girişte de denetlenir)
            continue;
        }
        if (k.tur == a3::KomutTuru::calistirma_adimla && (k.periyot_sayisi < 1u || k.periyot_sayisi > 10000u)) {
            a3_sonuc(g, false, b, "gecersiz_deger");   // A3-K65
            continue;
        }
        ++a3d_.durum_rev;   // A3-K122
        switch (k.tur) {
        case a3::KomutTuru::calistirma_hiz:
            hiz_ = k.carpan;   // A3-K66 (yalnız SIL; 0: olabildiğince hızlı)
            hiz_t0_sim_ = static_cast<std::int64_t>(b) * kH;
            hiz_t0_duvar_ = Saat::now();
            a3_durum_kirli_ = true;
            // A3-K122/K123: durum_rev arttı; anlık görüntü komut_sonucu'ndan ÖNCE yenilenir (a3_gecis gibi).
            a3_anlik_guncelle();
            break;
        case a3::KomutTuru::calistirma_durdur:
            a3_durdur_ = true;
            adimla_hedef_.reset();
            // Tablo 4.7a: hazırlanırken durdur → yuklendi (W4-K27 (4)); koşarken ya da duraklatılmışken
            // → durduruldu.
            a3_gecis(hazirlikta ? a3::CalistirmaDurumu::yuklendi : a3::CalistirmaDurumu::durduruldu, nullptr, false);
            break;
        case a3::KomutTuru::calistirma_duraklat:
            // W4-duraklat: duraklatma bu turun (sınır b'nin) işlemi bittikten sonra, fizik adımı
            // b'den önce yürürlüğe girer (duraklatma_gecisi). HIL'de çalıştırma geçersiz (A1-K41, K79).
            adimla_hedef_.reset();
            if (hil) {
                KopruCagrisi kc;
                k_->hil_duraklatma_bildir();
            }
            a3_gecis(a3::CalistirmaDurumu::duraklatildi, nullptr, false);
            break;
        case a3::KomutTuru::calistirma_surdur:
            // Çıkış bu turun emniyet kesimi ve A1-K85 adımları tamamlanınca (duraklatma_gecisi).
            a3_gecis(a3::CalistirmaDurumu::kosuyor, nullptr, false);
            break;
        case a3::KomutTuru::calistirma_adimla:
            // A3-K65: n kontrol periyodu (4n fizik adımı) ilerlenir; sınır b + 4n işlendikten sonra
            // (yayın dahil) çalıştırma iç geçişle yeniden duraklatılır (a3_sinir_basi).
            adimla_hedef_ = b + kopru::kPeriyotAdim * static_cast<std::uint64_t>(k.periyot_sayisi);
            a3_gecis(a3::CalistirmaDurumu::kosuyor, nullptr, false);
            break;
        case a3::KomutTuru::calistirma_sifirla:
        case a3::KomutTuru::senaryo_yukle:
            // W5-a3-senaryo (A3-K67, K77): canlı çalıştırma varsa durdurulur (aşağıda, `durdur` ile aynı kapanış
            // yolu), durum `yuklendi` olur ve dünya baştan kurulur.
            if (ertele && (a3d_.durum == a3::CalistirmaDurumu::kosuyor || a3d_.durum == a3::CalistirmaDurumu::duraklatildi) &&
                a3_onde_kisit_var(k.komut_no)) {
                // W5-K56 (EMNİYET; A3-K57, K95): aynı sınırda çekilmiş, komut numarası bu istekten küçük bir
                // kısıtlayan var. O istek eski dünyada uygulanmalıdır: istek kabul edilmiştir (durum_rev arttı;
                // bu turun sonraki denetim komutları bayatlar) ama dünya bu sınırın emniyet adımını olağan yoldan
                // (a3_emniyet_ver → köprü sinir_isle → a3_sinir_sonrasi) tamamladıktan sonra sonlanır
                // (sinir_on → a3_ertelenen_yeniden_bitir). Sonuç ve `yuklendi` geçişi orada.
                a3_yeniden_ertelenen_ = std::move(g);
                a3_anlik_guncelle();   // giriş denetimi yeni durum_rev'i görsün (A3-K122, K123)
                continue;
            }
            a3_yeniden_kabul(g, b);
            break;
        default:
            break;
        }
        if (k.tur != a3::KomutTuru::senaryo_yukle) {
            a3_sonuc(g, true, b, nullptr);   // `senaryo/yukle`nin sonucunu yeni dünya `yuklendi` olunca verir
        }
    }
    a3_genel_.resize(yaz);   // küçültme: bellek ayırmaz
}

// Sınır başı (her turda; W4-duraklat): istek çekme ve çalıştırma denetimi; SIL hız çarpanı (yalnız
// ilerleyen kipte: duran kipte sim zamanı ilerlemez).
bool Calistirici::a3_sinir_basi(std::uint64_t b, bool duran)
{
    a3_b_ = b;
    a3_sinir_ns_ = 0;
    if (a_.a3_sinir_kancasi) {
        a_.a3_sinir_kancasi(b, *a3al_);   // sınama (hizmet kapsamı dışında)
    }
    {
        A3Hizmet h(a3_sinir_ns_);
        if (!duran && adimla_hedef_ && b >= *adimla_hedef_) {
            // A3-K65 / Tablo 4.7a: adımlama bitti (4n fizik adımı); sınır b'nin bu turu ilerleyen
            // kipte işlenir (yayın bir sınır geriden kalmaz), sonra duraklatma yeniden yürürlüğe
            // girer. EMNİYET (W4-K28 (3) deseni): iç geçiş kabul edilmiş bir isteği bayatlatmaz:
            // üretici kilidi altında son çekiş, kararlar, sonra iç geçiş ve yeni görüntü. Bu kilit
            // W4-K27 (5)'in ("sınır döngüsünde kilit alınmaz") istisnasıdır (mimar kararı, W4-duraklat
            // düzeltme turu): adımlama yalnız SIL'dedir (Tablo 4.7b), kilit kısa süre ve sim zamanı
            // durmuşken tutulur; HIL döngüsünde kilit alınmaz.
            std::unique_lock<std::mutex> kilit = a3al_->gecis_kilidi();
            adimla_hedef_.reset();
            a3_cek(0);
            a3_calistirma_isle(b, true);
            if (a3d_.durum == a3::CalistirmaDurumu::kosuyor && !a3_durdur_ && !a3_yeniden_ertelenen_) {
                a3_gecis(a3::CalistirmaDurumu::duraklatildi, "adimla_tamamlandi", true);
            }
        } else {
            a3_cek(0);
            a3_calistirma_isle(b, true);
        }
    }
    if (a3_durdur_) {
        return false;
    }
    if (!duran) {
        sil_hiz_bekle(b);
    }
    return true;
}

// A3-K66: SIL hız çarpanı (yalnız bekleme; bayt akışı ve CSV değişmez). Yalnız ilerleyen turda, kontrol
// sınırlarında; çarpan 0 iken (varsayılan) hiçbir şey yapmaz. Çarpanın kaynağı panelin
// `calistirma/hiz` komutu ya da `--sil-hiz`'dir (W5-bagla-a2).
void Calistirici::sil_hiz_bekle(std::uint64_t b)
{
    if (a_.kip == Kip::sil && hiz_ > 0.0 && b % kopru::kPeriyotAdim == 0) {
        const double ns = static_cast<double>(static_cast<std::int64_t>(b) * kH - hiz_t0_sim_) / hiz_;
        a3::hassas_bekle(hiz_t0_duvar_ + std::chrono::nanoseconds(static_cast<std::int64_t>(ns)));
    }
}

// Operatör emniyet isteklerinin sonuçları ve emniyet olayları (sınırda).
void Calistirici::a3_sonuclari_isle(std::uint64_t b, const kopru::OperatorSonucu* sonuc, std::size_t n,
                                    const sensor::EmniyetDurumu& once)
{
    a3_degisen_.clear();
    for (std::size_t j = 0; j < n; ++j) {
        const kopru::OperatorSonucu& o = sonuc[j];
        std::size_t i = 0;
        while (i < a3_emniyet_bekleyen_.size() &&
               (a3_emniyet_bekleyen_[i].k.komut_no == 0u ? 1u : a3_emniyet_bekleyen_[i].k.komut_no) != o.kimlik) {
            ++i;
        }
        if (i == a3_emniyet_bekleyen_.size()) {
            continue;
        }
        A3Giris& g = a3_emniyet_bekleyen_[i];
        a3b::JsonYaz ay;
        ay.sayi_u("guncel_emniyet_rev", o.rev_sonra);
        switch (o.sonuc) {
        case kopru::AtamaSonucu::uygulandi:
            a3_sonuc(g, true, b, nullptr);
            if (o.degisti) {
                a3_degisen_.push_back(std::move(g));
            }
            break;
        case kopru::AtamaSonucu::kisitlama_hakim: a3_sonuc(g, false, b, "kisitlama_hakim", ay.bitir()); break;
        case kopru::AtamaSonucu::rev_uyusmuyor: a3_sonuc(g, false, b, "emniyet_rev_uyusmuyor", ay.bitir()); break;
        case kopru::AtamaSonucu::rev_doydu: a3_sonuc(g, false, b, "emniyet_rev_doydu"); break;
        }
        g.k.komut_no = 0xFFFFFFFFu;   // işlendi (kimlik 0xFFFFFFFF hiçbir isteğe denk gelmez)
    }
    for (A3Giris& g : a3_emniyet_bekleyen_) {
        if (g.k.komut_no != 0xFFFFFFFFu) {
            a3_sonuc(g, false, b, "sunucu_hatasi");   // köprü sonuç vermedi: olmamalı
        }
    }
    a3_emniyet_bekleyen_.clear();
    a3_emniyet_olaylari(b, once);
}

// Sınır işleminden önce: kısıtlayan istekler (emniyet ve daraltıcı yer istasyonu komutları) ve
// gevşeten emniyet istekleri köprüye verilir (senaryonun atamalarıyla aynı sınırda, aynı yoldan).
// EMNİYET: kısıtlayan istek `beklenen_run_id`'ye bakılmadan uygulanır (W4-K19); revizyon istemez.
void Calistirici::a3_emniyet_ver(std::uint64_t b)
{
    A3Hizmet h(a3_sinir_ns_);
    a3_emn_once_ = k_->dis_durum().emniyet;
    a3_emniyet_bekleyen_.clear();
    const bool canli = a3b::canli(a3d_.durum);
    if (a3_yeniden_ertelenen_) {
        // W5-K56 (EMNİYET): bu sınır eski dünyanın son sınırıdır (`sifirla`/`senaryo/yukle` kabul edildi,
        // etkisi bu adımın sonuna ertelendi). Komut numarası sıfırlamadan küçük kısıtlayanlar aşağıdaki olağan
        // yoldan uygulanır; çekilmiş geri kalan her istek (sonradan gelen kısıtlayan, gevşeten emniyet isteği,
        // yer istasyonu komutu) `canli_calistirma_yok` ile reddedilir. Hiçbiri sonuçsuz kalmaz (A3-K57).
        const std::uint32_t sinir_no = a3_yeniden_ertelenen_->k.komut_no;
        std::size_t kalan = 0;
        for (std::size_t i = 0; i < a3_kisit_.size(); ++i) {
            if (a3_kisit_[i].k.komut_no < sinir_no) {
                if (kalan != i) {
                    a3_kisit_[kalan] = std::move(a3_kisit_[i]);
                }
                ++kalan;
            } else {
                a3_sonuc(a3_kisit_[i], false, b, "canli_calistirma_yok");
            }
        }
        a3_kisit_.resize(kalan);   // küçültme: bellek ayırmaz
        for (const A3Giris& g : a3_genel_) {
            a3_sonuc(g, false, b, "canli_calistirma_yok");
        }
        a3_genel_.clear();
    }
    for (A3Giris& g : a3_kisit_) {
        if (!canli) {
            a3_sonuc(g, false, b, "canli_calistirma_yok");
            continue;
        }
        if (g.k.tur == a3::KomutTuru::emniyet_degistir) {
            {
                KopruCagrisi kc;
                k_->emniyet_ata(operator_atamasi(g.k));
            }
            a3_emniyet_bekleyen_.push_back(std::move(g));
        } else {
            a3_yi_uygula(b, g, nullptr);   // daraltıcı: A1-K85 adım 2 (köprü revizyonu artırır)
        }
    }
    a3_kisit_.clear();
    std::size_t yaz = 0;
    for (std::size_t i = 0; i < a3_genel_.size(); ++i) {
        A3Giris& g = a3_genel_[i];
        if (g.k.tur != a3::KomutTuru::emniyet_degistir) {
            if (yaz != i) {
                a3_genel_[yaz] = std::move(g);
            }
            ++yaz;
            continue;
        }
        // A3-K57: beklenen_run_id uygulamadan hemen önce yeniden karşılaştırılır (kısıtlayan hariç).
        if (!canli) {
            a3_sonuc(g, false, b, "canli_calistirma_yok");
        } else if (!g.k.kisitlayan && !a3_run_uyar(g.k)) {
            a3_sonuc(g, false, b, "beklenen_durum_uyusmuyor");
        } else if (!g.k.kisitlayan && !a3_gevsetme_onu_acik(g)) {
            a3_sonuc(g, false, b, "kisitlama_hakim");
        } else {
            {
                KopruCagrisi kc;
                k_->emniyet_ata(operator_atamasi(g.k));
            }
            a3_emniyet_bekleyen_.push_back(std::move(g));
        }
    }
    a3_genel_.resize(yaz);
}

// Sınır işleminden sonra: operatör emniyet isteklerinin sonuçları, emniyet olayları, genişletici
// ve nötr yer istasyonu komutları (A1-K85 adım 4'ün yerinde).
// W4-duraklat, duran kip: A3-K58'e göre duraklatılmışken gelen komut beklemeden uygulanır
// (uygulama_adimi o anki adım; genişletici revizyon ve sıra denetimiyle) ve köprüye verilir; köprü
// onu tele duraklatma kalkınca yazar (A1-K41: toplu üye yalnız TF'den sonra; W4-K27 (1), (3): komut
// birikir, sürdürmedeki ilk sınırda/pencerede işlenir; gönderimde revizyon değiştiyse A1-K129 ile
// atılır). Bekleyen bellek sınırlıdır: bir duraklatmada köprüye en çok kDuranYiAzami komut verilir;
// fazlası a3_genel_'de (en çok kGenelAzami), sonra genel halkada bekler; o da doluysa kitaplık
// `mesgul` döner. Kısıtlayanlar (daraltıcı) sınırsız beklemez: a3_emniyet_ver'de her turda köprüye
// verilir (revizyon hemen; tür başına birleşir, düşmez).
void Calistirici::a3_sinir_sonrasi(std::uint64_t b, const kopru::SinirSonucu& ss, bool duran)
{
    A3Hizmet h(a3_sinir_ns_);
    a3_sonuclari_isle(b, ss.operator_sonuclari.data(), ss.operator_sonuclari.size(), a3_emn_once_);
    a3_emn_once_ = k_->dis_durum().emniyet;
    if (ss.yuva_kacirildi) {
        if (a3_kacirma_adet_ == 0u) {
            a3_kacirma_ilk_ = ss.yuva_seq;
        }
        a3_kacirma_son_ = ss.yuva_seq;
        ++a3_kacirma_adet_;
    }
    std::size_t i = 0;
    for (; i < a3_genel_.size(); ++i) {
        if (duran) {
            if (duran_yi_verilen_ >= kDuranYiAzami) {
                break;   // sıra korunur: kalanlar a3_genel_'de bekler
            }
            ++duran_yi_verilen_;
        }
        a3_yi_uygula(b, a3_genel_[i], &ss);
    }
    if (i == a3_genel_.size()) {
        a3_genel_.clear();
    } else {
        a3_genel_.erase(a3_genel_.begin(), a3_genel_.begin() + static_cast<std::ptrdiff_t>(i));   // bellek ayırmaz
    }
}

void Calistirici::a3_yi_uygula(std::uint64_t b, const A3Giris& g, const kopru::SinirSonucu* ss)
{
    const a3::Komut& k = g.k;
    if (k.tur != a3::KomutTuru::yer_istasyonu_komut) {
        a3_sonuc(g, false, b, "sunucu_hatasi");
        return;
    }
    if (!a3b::canli(a3d_.durum)) {
        a3_sonuc(g, false, b, "canli_calistirma_yok");
        return;
    }
    if (!k.kisitlayan && !a3_run_uyar(k)) {
        a3_sonuc(g, false, b, "beklenen_durum_uyusmuyor");
        return;
    }
    if (k.yi_sinif == a3::KomutSinifi::genisletici) {
        // EMNİYET (A3-K95 (4), K118; W4-K27 (2)): genişletici, kabul edildiği revizyona bağlıdır ve
        // önce teslim edilmiş bir kısıtlayanı geçemez.
        const std::uint32_t rev = k_->emniyet_revizyonu();
        a3b::JsonYaz ay;
        ay.sayi_u("guncel_emniyet_rev", rev);
        if ((ss != nullptr && ss->rev_adim3_sonu != ss->rev_adim_basi) || !a3_gevsetme_onu_acik(g)) {
            a3_sonuc(g, false, b, "kisitlama_hakim", ay.bitir());
            return;
        }
        if (k_->emniyet_rev_doydu()) {
            a3_sonuc(g, false, b, "emniyet_rev_doydu");
            return;
        }
        if (!k.beklenen_emniyet_rev || *k.beklenen_emniyet_rev != rev) {
            a3_sonuc(g, false, b, "emniyet_rev_uyusmuyor", ay.bitir());
            return;
        }
    }
    const a3b::YiKarsilik& y = g.yi;
    a3b::JsonYaz j;
    if (y.tur == a3b::YiTur::desteklenmiyor) {
        // Karşılığı olmayan komut (W4-bagla-a3 kapsam 3): komut ağ geçidinde işlendi, sonuç
        // `yer_istasyonu_yaniti` olayında `desteklenmiyor` (A3 §3.5).
        a3_sonuc(g, true, b, nullptr);
        j.sayi_u("komut_no", k.komut_no).metin("komut", y.ad).metin("sonuc", "desteklenmiyor");
        a3_olay(12, 0, 6, k.komut_no, k.istemci_id, j.bitir());
        return;
    }
    // Senaryonun operatör komutlarıyla aynı köprü yolu (komut_basit, komut_guided, komut_fc).
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    std::uint32_t id = 0;
    {
        KopruCagrisi kc;
        switch (y.tur) {
        case a3b::YiTur::basit: id = k_->komut_basit(y.a1_tur, y.arg, t); break;
        case a3b::YiTur::guided: {
            kopru::GuidedHedef hd;
            hd.mask = y.sp_mask;
            hd.pos_ne = y.pos_ne;
            hd.depth = y.depth;
            hd.altitude = y.altitude;
            hd.vel_ned = y.vel_ned;
            hd.yaw = y.yaw;
            hd.yaw_rate = y.yaw_rate;
            id = k_->komut_guided(hd, t);
            break;
        }
        case a3b::YiTur::fc: id = k_->komut_fc(y.a1_tur, static_cast<std::uint8_t>(y.arg), t); break;
        case a3b::YiTur::desteklenmiyor: break;
        }
    }
    a3_sonuc(g, true, b, nullptr);
    const std::optional<kopru::KomutSonucu> r = k_->komut_sonucu(id);
    const bool oturumsuz = r && r->yerel && r->reason == 129u;   // IDAW_REASON_LINK_SESSION_END
    j.sayi_u("komut_no", k.komut_no).metin("komut", y.ad).sayi_u("emniyet_rev", k_->emniyet_revizyonu())
        .metin("durum", oturumsuz ? "oturum_yok" : "karta_iletildi");
    a3_olay(11, 0, 6, k.komut_no, k.istemci_id, j.bitir());
    if (!oturumsuz) {
        if (a3_yi_n_ < a3_yi_.size()) {
            a3_yi_[a3_yi_n_++] = YiIzi{k.komut_no, k.istemci_id, y.ad, id, y.arg};
        } else {
            ++s_.a3_kayit_atilan;   // izleme tablosu dolu: kart yanıtı olaya çevrilmez (sayılır)
        }
    }
}

// Yer istasyonu komutlarının kart yanıtı ya da yerel sonucu (A3-K101, K102).
void Calistirici::a3_yi_izle()
{
    std::size_t yaz = 0;
    for (std::size_t i = 0; i < a3_yi_n_; ++i) {
        const YiIzi z = a3_yi_[i];
        const std::optional<kopru::KomutSonucu> r = k_->komut_sonucu(z.cmd_id);
        if (!r) {
            a3_yi_[yaz++] = z;
            continue;
        }
        a3b::JsonYaz j;
        j.sayi_u("komut_no", z.komut_no).metin("komut", z.ad);
        if (r->yerel && r->ack == 2u && (r->reason == 10u || r->reason == 129u)) {
            // A1-K129 (bayat yetki: NARROWED) ya da oturum bitti (LINK_SESSION_END): karta gitmedi.
            j.sayi_u("emniyet_rev", k_->emniyet_revizyonu()).metin("durum", r->reason == 10u ? "bayat_yetki" : "oturum_yok");
            a3_olay(11, 0, 6, z.komut_no, z.istemci, j.bitir());
            continue;
        }
        const char* sonuc = r->ack == 1u ? "kabul" : r->ack == 3u ? "suruyor" : r->ack == 4u ? "yineleme"
                            : r->ack == 5u ? "desteklenmiyor" : "ret";
        a3b::JsonYaz v;
        v.metin("ack", a3b::ack_adi(r->ack)).sayi_u("reason", r->reason);
        if (std::string_view(z.ad) == "param_get") {
            v.sayi_u("param_id", z.param_id);
            // A3-K102: ACCEPTED ve PARAM_VALUE'da tür ve değer; değilse yazılmaz.
            if (r->kind == IDA_RES_PARAM_VALUE && r->ack == 1u && r->param_type >= 1u && r->param_type <= 3u) {
                static constexpr const char* kTip[] = {"", "I32", "U32", "F32"};
                v.metin("param_type", kTip[r->param_type]);
                if (r->param_type == 1u) {
                    v.sayi("value", static_cast<std::int32_t>(r->value_bits));
                } else if (r->param_type == 2u) {
                    v.sayi_u("value", r->value_bits);
                } else {
                    float f = 0.0F;
                    std::memcpy(&f, &r->value_bits, sizeof f);
                    if (std::isfinite(f)) {
                        char sb[48];
                        const int sn = std::snprintf(sb, sizeof sb, "%.9g", static_cast<double>(f));
                        v.ham("value", std::string_view(sb, sn > 0 ? static_cast<std::size_t>(sn) : 0u));
                    }
                }
            }
        }
        j.metin("sonuc", sonuc).ham("veri", v.bitir());
        a3_olay(12, 0, 6, z.komut_no, z.istemci, j.bitir());
    }
    a3_yi_n_ = yaz;
}

// emniyet_girdisi_degisti (A3-K96), son değişim ve revizyon doygunluğu; senaryonun gevşetme
// retleri ve atama çelişkileri (A3 §3.5, A5 §3.15).
void Calistirici::a3_emniyet_olaylari(std::uint64_t b, const sensor::EmniyetDurumu& once)
{
    const sensor::EmniyetDurumu simdi = k_->dis_durum().emniyet;
    struct Alan {
        a3::EmniyetGirdisi g;
        int tup;
        bool o, s;
        const char* ad;
    };
    const Alan alanlar[] = {
        {a3::EmniyetGirdisi::acil_durdurma, -1, once.acil_durdurma, simdi.acil_durdurma, "acil_durdurma"},
        {a3::EmniyetGirdisi::silahlandirma_fisi, 0, once.fis[0], simdi.fis[0], "silahlandirma_fisi.f1"},
        {a3::EmniyetGirdisi::silahlandirma_fisi, 1, once.fis[1], simdi.fis[1], "silahlandirma_fisi.f2"},
        {a3::EmniyetGirdisi::emniyet_pimi, 0, once.pim[0], simdi.pim[0], "emniyet_pimi.f1"},
        {a3::EmniyetGirdisi::emniyet_pimi, 1, once.pim[1], simdi.pim[1], "emniyet_pimi.f2"},
    };
    for (const Alan& a : alanlar) {
        if (a.o == a.s) {
            continue;
        }
        const a3::Komut* op = nullptr;
        for (const A3Giris& g : a3_degisen_) {
            if (g.k.girdi == a.g && (a.tup < 0 || g.k.tup == a.tup)) {
                op = &g.k;
            }
        }
        const bool acil = a.g == a3::EmniyetGirdisi::acil_durdurma;
        // Kısıtlayan yön: acil basılı, fiş çıkık, pim takılı.
        const bool artiran = a.g == a3::EmniyetGirdisi::silahlandirma_fisi ? !a.s : a.s;
        a3b::JsonYaz j;
        j.metin("girdi", acil ? "acil_durdurma" : (a.g == a3::EmniyetGirdisi::silahlandirma_fisi ? "silahlandirma_fisi" : "emniyet_pimi"));
        if (!acil) {
            j.metin("tup", a.tup == 0 ? "f1" : "f2");
        }
        j.metin("onceki", girdi_durumu(a.o, acil)).metin("sonraki", girdi_durumu(a.s, acil))
            .metin("kaynak", op != nullptr ? "operator" : "senaryo").metin("yon", artiran ? "artiran" : "azaltan")
            .sayi_u("uygulama_adimi", b).sayi_u("emniyet_rev", simdi.emniyet_revizyonu);
        a3_olay(7, 3, op != nullptr ? 1 : 3, op != nullptr ? op->komut_no : 0u, op != nullptr ? op->istemci_id : 0u, j.bitir());
        if (a3_emniyet_kaydi_.size() < kKayitAzami) {
            a3_emniyet_kaydi_.push_back({static_cast<std::int64_t>(b) * kH, a.ad, a.o, a.s, op != nullptr,
                                         op != nullptr ? op->komut_no : 0u, simdi.emniyet_revizyonu});
        } else {
            ++s_.a3_kayit_atilan;
        }
        a3_son_degisim_adimi_ = b;
        a3_son_degisim_kaynagi_ = op != nullptr ? 2u : 1u;
        a3_durum_kirli_ = true;
        a3_anlik_kirli_ = true;
    }
    a3_degisen_.clear();
    if (once.emniyet_revizyonu != simdi.emniyet_revizyonu) {
        a3_durum_kirli_ = true;
        a3_anlik_kirli_ = true;
    }
    if (k_->emniyet_rev_doydu() && !a3_doydu_bildirildi_) {
        a3_doydu_bildirildi_ = true;
        a3_olay(30, 3, 0, 0, 0, a3b::JsonYaz().sayi_u("emniyet_rev", simdi.emniyet_revizyonu).bitir());
    }
    // Senaryonun gevşetme retleri (A3 §3.5 tür 26) ve atama çelişkileri (tür 25; A3-K125 parçaları).
    for (; a3_gevsetme_n_ < s_.gevsetme_retleri.size(); ++a3_gevsetme_n_) {
        const GevsetmeReddi& g = s_.gevsetme_retleri[a3_gevsetme_n_];
        a3b::JsonYaz j;
        j.sayi("t_ns", g.t_ns).metin("olay_kimligi", g.olay_kimligi).metin("alan", g.alan)
            .metin("istenen_deger", g.istenen_deger ? "true" : "false").sayi_u("revizyon_adim_basi", g.revizyon_adim_basi)
            .sayi_u("revizyon_adim_3_sonu", g.revizyon_adim_3_sonu);
        a3_olay(26, 3, 3, 0, 0, j.bitir());
    }
    for (; a3_celiski_n_ < s_.atama_celiskileri.size(); ++a3_celiski_n_) {
        const AtamaCeliskisi& c = s_.atama_celiskileri[a3_celiski_n_];
        ++a3_celiski_kimligi_;
        const std::size_t n = c.degerler.size();
        const std::size_t parca = std::max<std::size_t>(1u, (n + 7u) / 8u);
        for (std::size_t p = 0; p < parca; ++p) {
            DiziYaz deg;
            DiziYaz kim;
            for (std::size_t i = p * 8u; i < std::min(n, p * 8u + 8u); ++i) {
                deg.ekle(c.degerler[i] ? "true" : "false");
                kim.ekle(c.olay_kimlikleri[i]);
            }
            a3b::JsonYaz j;
            j.sayi("t_ns", c.t_ns).metin("alan", c.alan).sayi_u("celiski_kimligi", a3_celiski_kimligi_)
                .sayi_u("parca_no", p + 1u).sayi_u("parca_sayisi", parca).mantik("son_parca", p + 1u == parca)
                .ham("degerler", deg.bitir()).ham("olay_kimlikleri", kim.bitir())
                .metin("uygulanan_deger", c.uygulanan_deger ? "true" : "false");
            a3_olay(25, 1, 3, 0, 0, j.bitir());
        }
    }
}

void Calistirici::a3_senaryo_ogesi(std::size_t i, const char* sonuc, std::int64_t t, const char* neden)
{
    const PlanOlayi& o = plan_.olaylar[i];
    // `olay_kimligi` deseni ^[A-Za-z][A-Za-z0-9_]{0,31}$ (A5 kimlikleri); yerleşik planın üretilmiş
    // adları uymazsa olay yayınlanmaz (kayıt Sonuc'ta kalır).
    const std::string& id = o.kimlik;
    bool uyar = !id.empty() && id.size() <= 32u && std::isalpha(static_cast<unsigned char>(id[0])) != 0;
    for (const char c : id) {
        uyar = uyar && (std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_') && static_cast<unsigned char>(c) < 0x80u;
    }
    if (!uyar) {
        return;
    }
    const char* tur = o.tur == PlanOlayTuru::emniyet                                    ? "emniyet_girdisi"
                      : o.tur == PlanOlayTuru::sensor || o.tur == PlanOlayTuru::itici ? "ariza"
                                                                                       : "diger";
    a3b::JsonYaz j;
    j.metin("olay_kimligi", id).metin("tur", tur).sayi("t_ns", t);
    if (std::string_view(sonuc) != "uygulanmadi") {
        j.sayi_u("uygulama_adimi", static_cast<std::uint64_t>(t / kH));
    }
    j.metin("sonuc", sonuc);
    if (neden != nullptr && neden[0] != '\0') {
        j.metin("neden", neden);
    }
    a3_olay(21, 0, 3, 0, 0, j.bitir());
}

// atis_olayi_sonucu (A3-K98; A6-K41 normallestirilmis_sira) ve atis/durum'un tüp sonucu.
void Calistirici::a3_olay_kararlari()
{
    {
        KopruCagrisi kc;
        k_->olay_kararlari_al(a3_kararlar_);
    }
    for (const kopru::OlayKarari& o : a3_kararlar_) {
        std::uint32_t sira = 0;
        for (std::size_t i = 0; i < a3_olay_sira_n_; ++i) {
            if (a3_olay_sira_[i] == o.event_id) {
                sira = static_cast<std::uint32_t>(i + 1u);
            }
        }
        if (sira == 0u && a3_olay_sira_n_ < a3_olay_sira_.size()) {
            a3_olay_sira_[a3_olay_sira_n_++] = o.event_id;
            sira = static_cast<std::uint32_t>(a3_olay_sira_n_);
        }
        const char* tur = o.kind == 1u ? "SAFETY_OPEN" : o.kind == 2u ? "SAFETY_CLOSE" : o.kind == 3u ? "FIRE" : "bilinmiyor";
        // IDA_EVR: 1 APPLIED, 2 DUPLICATE, 3 REJ_EXPIRED, 4 REJ_SAFE, 5 REJ_TUBE_EMPTY, 6 REJ_SESSION.
        const char* sonuc = o.sonuc == 1u ? "uygulandi" : o.sonuc == 2u ? "yineleme" : o.sonuc == 4u ? "ret_emniyet"
                            : o.sonuc == 3u ? "ret_suresi_gecmis" : "ret_diger";
        const std::uint8_t kod = o.sonuc == 1u ? 1u : o.sonuc == 2u ? 2u : o.sonuc == 4u ? 3u : o.sonuc == 3u ? 4u : 5u;
        a3b::JsonYaz j;
        if (o.tube <= 1u) {
            j.sayi_u("tup", o.tube);
            a3_tup_son_olay_[o.tube] = kod;
        } else if (o.kind == 2u) {
            a3_tup_son_olay_ = {kod, kod};   // iki tüpün izni (tube = 255)
        }
        j.metin("olay_turu", tur).metin("sonuc", sonuc);
        if (sira != 0u) {
            j.sayi_u("normallestirilmis_sira", sira);
        }
        if (o.sonuc == 1u && o.t_apply_ns >= 0) {
            j.sayi_u("uygulama_adimi", static_cast<std::uint64_t>(o.t_apply_ns / kH));
        }
        a3_olay(8, 3, 0, 0, 0, j.bitir());
        a3_durum_kirli_ = true;
    }
    a3_kararlar_.clear();
}

// a1_oturum_degisti (tür 9) ve kart_sifirlandi (tür 10).
void Calistirici::a3_oturum_izle()
{
    const bool kopuk = !tas_ && a_.kip == Kip::hil;
    const std::uint8_t simdi = a1_oturum_kodu(k_->durum(), kopuk);
    const std::uint8_t once = a1_oturum_kodu(a3_a1_onceki_, a3_a1_kopuk_);
    const kopru::KartBilgisi& kb = k_->kart();
    if (simdi != once) {
        // W5-K59: `sifirla`/`senaryo/yukle` ile sonlanan dünyada süreç kapanmıyor; neden planlı sıfırlamadır.
        const char* neden = a3_kapaniyor_                                    ? (a3_yeniden_ ? "planli_sifirlama" : "sim_kapanisi")
                            : simdi == 3u                                     ? "el_sikisma_tamam"
                            : k_->durum() == kopru::OturumDurumu::reddedildi ? "uyumsuzluk"
                            : kopuk                                           ? "aygit_kalkti"
                            : k_->yeniden_baslatma_bekleniyor()               ? "reboot_istendi"
                            : kb.son_kapanis_nedeni == 2u                     ? "beklenmedik_sifirlama"
                                                                              : "canlilik_kaybi";
        a3b::JsonYaz j;
        j.metin("onceki", a1_oturum_adi(once)).metin("sonraki", a1_oturum_adi(simdi)).sayi_u("sess", k_->sess())
            .sayi_u("acilis_sayaci", kb.boot_count).metin("neden", neden)
            .mantik("planli", a3_kapaniyor_ || std::string_view(neden) == "reboot_istendi" || simdi == 3u);
        a3_olay(9, 0, 0, 0, 0, j.bitir());
        a3_a1_onceki_ = k_->durum();
        a3_a1_kopuk_ = kopuk;
        a3_durum_kirli_ = true;
    }
    if (kb.boot_count != 0u) {
        if (a3_boot_var_ && kb.boot_count != a3_boot_onceki_) {
            a3b::JsonYaz j;
            j.sayi_u("onceki_acilis_sayaci", a3_boot_onceki_).sayi_u("yeni_acilis_sayaci", kb.boot_count)
                .mantik("planli", k_->yeniden_baslatma_bekleniyor() || s_.hazirlik_oturumu > 0u);
            a3_olay(10, 1, 2, 0, 0, j.bitir());
        }
        a3_boot_var_ = true;
        a3_boot_onceki_ = kb.boot_count;
    }
}

// Geçerlilik (A3-K70, K71; Tablo 4.8): bitler yalnız eklenir.
void Calistirici::a3_gecerlilik(std::uint64_t b)
{
    const kopru::Sayaclar& c = k_->sayaclar();
    const std::uint32_t bitler = a3b::gecerlilik_bitleri(c.calistirma_gecersiz, c.beklenmeyen_sifirlama,
                                                         kume_.sayaclar().kuyruk_tasma);
    const std::uint32_t yeni = bitler & ~a3_gecerlilik_;
    if (yeni == 0u) {
        return;
    }
    a3_gecerlilik_ |= bitler;
    const std::uint32_t g = a3b::gecersiz_kilan(a_.kip == Kip::hil);
    for (unsigned bit = 0; bit < 32u; ++bit) {
        if (((yeni >> bit) & 1u) == 0u || ((g >> bit) & 1u) == 0u) {
            continue;
        }
        a3_gecerli_ = false;
        a3b::JsonYaz j;
        j.metin("neden", a3b::neden_adi(bit)).sayi_u("ilk_adim", b);
        a3_olay(2, 1, 0, 0, 0, j.bitir());
    }
    const std::uint32_t rt = yeni & g & 0x0FFFu;
    if (rt != 0u && !a3_rt_kacirdi_) {
        a3_rt_kacirdi_ = true;
        unsigned ilk = 0;
        while (((rt >> ilk) & 1u) == 0u) {
            ++ilk;
        }
        a3b::JsonYaz j;
        j.sayi_u("ilk_adim", b).metin("neden", a3b::neden_adi(ilk)).sayi_u("neden_biti", ilk);
        a3_olay(3, 1, 0, 0, 0, j.bitir());
    }
    a3_durum_kirli_ = true;
    a3_anlik_kirli_ = true;
}

// Etkin arızalar (senaryonun sensör ve itici arızaları; plan durumundan): ariza_degisti (A3-K86)
// ve ariza/etkin. Tablolar A3 başlarken kuruldu; burada bellek ayrılmaz.
void Calistirici::a3_ariza_izle(std::uint64_t b)
{
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    bool degisti = false;
    for (A3Ariza& a : a3_arizalar_) {
        const PlanOlayi& o = plan_.olaylar[a.plan_i];
        const PlanDurumu& d = pd_[a.plan_i];
        const bool aktif = d.basladi && !d.reddedildi && (o.sure_ns <= 0 || t < d.baslangic_ns + o.sure_ns);
        if (aktif == a.aktif) {
            continue;
        }
        a.aktif = aktif;
        degisti = true;
        a3b::JsonYaz j;
        j.metin("fault_id", o.kimlik).metin("islem", aktif ? "uygulandi" : "bitti").metin("kaynak", "senaryo")
            .sayi_u("uygulama_adimi", aktif ? static_cast<std::uint64_t>(d.baslangic_ns / kH) : b);
        a3_olay(6, 0, 3, 0, 0, j.bitir());
    }
    if (!degisti && b % kYavasSinir != 0u) {
        return;
    }
    a3_ariza_yuk_.clear();
    for (const A3Ariza& a : a3_arizalar_) {
        if (!a.aktif) {
            continue;
        }
        const PlanOlayi& o = plan_.olaylar[a.plan_i];
        const PlanDurumu& d = pd_[a.plan_i];
        a3::yuk_ariza_etkin_arizalar x;
        x.fault_id = o.kimlik;
        x.sinif = a.sinif;
        x.hedef = a.hedef;
        x.tur = a.tur;
        x.kaynak = 0;
        x.baslangic_adimi = static_cast<std::uint64_t>(d.baslangic_ns / kH);
        x.bitis_adimi = o.sure_ns > 0 ? static_cast<std::uint64_t>((d.baslangic_ns + o.sure_ns + kH - 1) / kH) : kYok64;
        a3_ariza_yuk_.push_back(x);
    }
    a3::yuk_ariza_etkin y;
    y.arizalar = a3_ariza_yuk_;
    yayin<a3::Kanal::ariza_etkin, kayit::Kanal::ariza_etkin>(t, b, y);
}

// Durum kanalları (10 Hz ve değişimde): calistirma/durum, sayac/gercek_zaman, emniyet/girdiler,
// atis/durum, baglanti/durum.
void Calistirici::a3_durum_yayinla(std::int64_t t)
{
    if (yay_ == nullptr && !kb_) {
        return;   // ne A3 ne kayıt: bugünkü davranış
    }
    a3_durum_kirli_ = false;
    const std::uint64_t adim = static_cast<std::uint64_t>(t / kH);
    const std::uint8_t a1_oturum = k_ ? a1_oturum_kodu(k_->durum(), !tas_ && a_.kip == Kip::hil && a3b::canli(a3d_.durum)) : 255u;
    if (yay_ != nullptr) {
    // `calistirma/durum` A3 durum makinesinin (a3d_) yetkili durumudur: A3 kapalıyken üretilmez.
    a3::yuk_calistirma_durum c;
    c.durum = static_cast<std::uint8_t>(a3d_.durum);
    c.kip = static_cast<std::uint8_t>(a_.kip == Kip::hil ? a3::ZamanKipi::hil_1x : a3::ZamanKipi::sil);
    c.run_id = a3d_.run_var ? std::string_view(a3_run_id_) : std::string_view();
    c.adim_no = a3_b_;
    c.hedef_carpan = a_.kip == Kip::hil ? 1.0F : static_cast<float>(hiz_);
    const std::int64_t duvar_simdi = gercek();
    if (duvar_simdi - a3_carpan_ref_duvar_ >= 1'000'000'000) {
        if (a3_carpan_ref_duvar_ > 0) {
            a3_gerceklesen_ = static_cast<float>(static_cast<double>(t - a3_carpan_ref_sim_) /
                                                 static_cast<double>(duvar_simdi - a3_carpan_ref_duvar_));
        }
        a3_carpan_ref_duvar_ = duvar_simdi;
        a3_carpan_ref_sim_ = t;
    }
    c.gerceklesen_carpan = a3_gerceklesen_;
    c.gecerli = a3_gecerli_ ? 1u : 0u;
    c.gercek_zamani_kacirdi = a3_rt_kacirdi_ ? 1u : 0u;
    c.gecerlilik_nedenleri = a3_gecerlilik_;
    // Kayıt yoksa 0 (yok); kayıt açıksa dosyanın şu an alabileceği en yüksek düzey (kademe, kip ve W5-K41 tavanı).
    c.tekrar_guvencesi = kb_ ? static_cast<std::uint8_t>(kb_->guvence_ust_siniri()) : 0u;
    // W5-bagla-a2: A2 gerçekten dinliyorsa senaryonun görünüm kademesi (A3 §3.4: 0 kapali, 1 sade,
    // 2 tam); bayrak yoksa ya da A2 açılamadıysa (kilit başkasında, port dolu, tanım yok) 0.
    c.kademe_gorunum = (a2_ && a2_->dinliyor()) ? static_cast<std::uint8_t>(a2_->kademe()) : std::uint8_t{0};
    c.kademe_kamera = 0;
    c.kademe_sonar = sonar_ ? sonar::kKademeSade : 0u;   // W4-bagla-sonar (A3 §3.4: 0 kapali, 1 sade)
    c.kademe_kayit = kb_ ? static_cast<std::uint8_t>(kb_->kademe()) : std::uint8_t{0};   // W5-bagla-kayit
    c.a1_oturum = a1_oturum;
    c.son_komut_no = a3_son_komut_no_;   // dünyaya ulaşan en büyük komut numarası (okuma; rapor)
    c.durum_rev = a3d_.durum_rev;
    yayin<a3::Kanal::calistirma_durum, kayit::Kanal::calistirma_durum>(t, adim, c);
    a3_sessiz_ = a3_yeniden_.has_value();   // durum çerçevesi gitti; geri kalan kanallar A3'e artık gitmez
    }
    if (!k_) {
        return;
    }
    const kopru::Sayaclar& s = k_->sayaclar();
    a3::yuk_sayac_gercek_zaman g;
    g.parti_gonderilen = s.parti_gonderilen;
    g.parti_kacirildi = s.parti_kacirildi;
    g.parti_kacirildi_birlesik = s.parti_kacirildi_birlesik;
    g.bariyer_bekleme = s.bariyer_bekleme;
    g.tf_gec_yazim_ns = std::max<std::int64_t>(static_cast<std::int64_t>(s.tf_gec_yazim_ns_en_buyuk), s_.zaman.gec_yazim_en_cok_ns);
    g.yanit_ret_yinelenen = s.yanit_ret_yinelenen;
    g.yanit_ret_gec = s.yanit_ret_gec;
    g.yanit_ret_oturum = s.yanit_ret_oturum;
    g.yanit_iptal_esz = s.yanit_iptal_esz;
    g.esz_guvenli_uygulanan = s.esz_guvenli_uygulanan;
    g.komut_suresi_doldu = s.komut_suresi_doldu;
    g.olay_alinan = s.olay_alinan;
    g.olay_gec = s.olay_gec;
    g.olay_eksik = s.olay_eksik;
    for (std::size_t i = 0; i < 6; ++i) {
        g.olay_sonuc[i] = s.olay_sonuc[i + 1u];
    }
    g.komut_birlestirildi = s.komut_birlestirildi;
    g.komut_ertelendi = s.komut_ertelendi;
    g.komut_sonucsuz = s.komut_sonucsuz;
    g.pencere_tasma = s.pencere_tasma;
    g.rx_crc_err = s.rx_crc_err;
    g.rx_resync_bytes = s.rx_resync_bytes;
    g.rx_unknown_msgid = s.rx_unknown_msgid;
    g.rx_bad_len = s.rx_bad_len;
    g.rx_wrong_sys = s.rx_wrong_sys;
    g.rx_iflag = s.rx_iflag;
    g.rx_mav1 = s.rx_mav1;
    g.rx_seq_gap = s.rx_seq_gap;
    g.oturum_kurulan = s.oturum_kurulan;
    g.oturum_kopma = s.oturum_kopma;
    g.beklenmeyen_sifirlama = s.beklenmeyen_sifirlama;
    g.calistirma_gecersiz = (a3_gecerlilik_ & 0x0FFFu) != 0u ? 1u : 0u;
    g.neden_bitleri = a3_gecerlilik_ & 0x0FFFu;
    g.son_tarih_kacirildi = s.son_tarih_kacirildi;
    g.sim_gecikmesi = std::max<std::uint64_t>(s.sim_gecikmesi, s_.zaman.sim_gecikmesi);
    g.yanit_ret_eski_uretim = s.yanit_ret_eski_uretim;
    g.olay_rev_ret = s.olay_rev_ret;
    g.sonuc_kuyrugu_en_buyuk = s.sonuc_kuyrugu_en_buyuk;
    g.kritik_erteleme = s.kritik_erteleme;
    g.kritik_erteleme_en_buyuk = s.kritik_erteleme_en_buyuk;
    g.cagri_cakismasi = s.cagri_cakismasi;
    g.sonuc_talebi_atildi = s.sonuc_talebi_atildi;
    g.emniyet_rev_doydu = s.emniyet_rev_doydu ? 1u : 0u;
    const sensor::Sayaclar ks = kume_.sayaclar();
    g.sensor_kuyruk_tasma = ks.kuyruk_tasma;
    // W4-bagla-sonar: A1 Tablo 3.11a Ping360 sayaçları ve sonar kuyruğu (05 §4). `sonar_gec` 0:
    // sonar bu dilimde dünya iş parçacığında eşzamanlı hesaplanır, sonucu geç kalamaz.
    g.ping360_seyreltildi = s.ping360_seyreltildi;
    g.ping360_gecikme_en_buyuk = s.ping360_gecikme_en_buyuk;
    if (sonar_) {
        g.sonar_kuyruk_dolu = sonar_->sayaclar().kuyruk_tasma;
    }
    g.donma_onceki_cikti_yok = ks.donma_onceki_cikti_yok;
    g.atama_celiskisi = s_.atama_celiskileri.size();
    g.gevsetme_reddedildi = s.gevsetme_reddedildi;
    g.komut_bayat_yetki = s.komut_bayat_yetki;
    g.a3_halka_tasma = yay_ != nullptr ? yay_->halka_tasma() : 0u;
    if (kb_) {
        g.kayit_dusen = kayit_dusen_();   // A6-K30: üretici halkasında düşen ileti
    }
    yayin<a3::Kanal::sayac_gercek_zaman, kayit::Kanal::sayac_gercek_zaman>(t, adim, g);
    if (kb_) {
        kayit_son_sayac_ = g;
    }

    const sensor::EmniyetDurumu e = k_->dis_durum().emniyet;
    const kopru::KartBilgisi& kb = k_->kart();
    a3::yuk_emniyet_girdiler eg;
    eg.acil_durdurma = e.acil_durdurma ? 1u : 0u;
    eg.silahlandirma_fisi = {static_cast<std::uint8_t>(e.fis[0] ? 1u : 0u), static_cast<std::uint8_t>(e.fis[1] ? 1u : 0u)};
    eg.emniyet_pimi = {static_cast<std::uint8_t>(e.pim[0] ? 1u : 0u), static_cast<std::uint8_t>(e.pim[1] ? 1u : 0u)};
    eg.son_degisim_adimi = a3_son_degisim_adimi_;
    eg.son_degisim_kaynagi = a3_son_degisim_kaynagi_;
    if (kb.fc_var) {
        // A3-K97: kartın etkin değerleri yalnız IDA_FC_STATUS.inhibit_mask'ten.
        const std::uint32_t m = kb.fc_inhibit_mask;
        eg.kart_acil_durdurma = static_cast<std::uint8_t>((m >> 4u) & 1u);
        std::uint8_t fark = eg.kart_acil_durdurma != eg.acil_durdurma ? 1u : 0u;
        for (std::size_t i = 0; i < 2; ++i) {
            eg.kart_silahlandirma_fisi[i] = ((m >> (5u + i)) & 1u) != 0u ? 0u : 1u;
            eg.kart_emniyet_pimi[i] = ((m >> (7u + i)) & 1u) != 0u ? 1u : 0u;
            fark = static_cast<std::uint8_t>(fark | (eg.kart_silahlandirma_fisi[i] != eg.silahlandirma_fisi[i] ? (2u << i) : 0u) |
                                             (eg.kart_emniyet_pimi[i] != eg.emniyet_pimi[i] ? (8u << i) : 0u));
        }
        eg.geri_okuma_farki = fark;
    }
    eg.emniyet_rev = e.emniyet_revizyonu;
    eg.emniyet_rev_doydu = k_->emniyet_rev_doydu() ? 1u : 0u;
    yayin<a3::Kanal::emniyet_girdiler, kayit::Kanal::emniyet_girdiler>(t, adim, eg);

    std::array<a3::yuk_atis_durum_tupler, 2> tupler{};
    for (int i = 0; i < 2; ++i) {
        auto& x = tupler[static_cast<std::size_t>(i)];
        x.tup = static_cast<std::uint8_t>(i);
        x.dolu = k_->tup_dolu(i) ? 1u : 0u;
        x.sureli_izin_gecerli = k_->izin_acik(i) ? 1u : 0u;
        const std::int64_t bitis = k_->izin_bitis_ns(i);
        x.izin_kalan_ns = bitis >= 0 ? std::max<std::int64_t>(0, bitis - t) : -1;
        x.son_olay_sonucu = a3_tup_son_olay_[static_cast<std::size_t>(i)];
    }
    a3::yuk_atis_durum ad;
    if (kb.fc_var) {
        ad.fc_durumu = kb.fc_state;
        ad.engel_maskesi = kb.fc_inhibit_mask;
        ad.kart_son_olay_kimligi = kb.fc_son_olay_kimligi;
    }
    ad.tupler = tupler;
    yayin<a3::Kanal::atis_durum, kayit::Kanal::atis_durum>(t, adim, ad);

    if (yay_ != nullptr) {   // `baglanti/durum` yalnız A3'te (A6 kataloğunda karşılığı yok)
        a3::yuk_baglanti_durum bd;
        bd.a1_oturum = a1_oturum;
        bd.sess = k_->durum() == kopru::OturumDurumu::kosuyor ? k_->sess() : 0u;
        bd.acilis_sayaci = kb.boot_count;
        // d_asagi_ort_ns: bağlantı emülatörü yok → yok (INT64_MIN, varsayılan); emulator: boş.
        a3_olc([&] { (void)yay_->yayinla<a3::Kanal::baglanti_durum>(t, bd); });
    }
}

// Sensör kanalları (beş panelin kullandığı: basınç, DVL, batarya): sınırda teslim edilen kayıtlar.
void Calistirici::a3_sensor_yayinla()
{
    for (const sensor::Olcum& o : olcumler_) {
        const std::int64_t t = o.hdr.t_meas_ns;   // zaman anlamı `olcum` (A3-K26)
        if (const auto* b = std::get_if<sensor::BaroOlcumu>(&o.yuk)) {
            a3::yuk_basinc y;
            olcum_basligi(y, o.hdr);
            y.basinc_pa = b->p_abs;
            y.sicaklik_c = b->temp_c;
            yayin<a3::Kanal::sensor_basinc, kayit::Kanal::sensor_basinc>(t, yayin_b_, y);
        } else if (const auto* d = std::get_if<sensor::DvlOlcumu>(&o.yuk)) {
            a3::yuk_dvl y;
            olcum_basligi(y, o.hdr);
            if ((d->dvl_flags & sensor::kDvlHizGecerli) != 0u) {
                y.hiz_sensor_mps = d->vel;
            }
            if (d->altitude >= 0.0F) {
                y.irtifa_m = d->altitude;
            }
            y.fom_mps = d->fom;
            y.isin_gecerli = d->beam_valid;
            yayin<a3::Kanal::sensor_dvl, kayit::Kanal::sensor_dvl>(t, yayin_b_, y);
        } else if (const auto* p = std::get_if<sensor::BataryaOlcumu>(&o.yuk)) {
            a3::yuk_batarya y;
            olcum_basligi(y, o.hdr);
            y.gerilim_v = p->voltage;
            y.akim_a = p->current;
            yayin<a3::Kanal::sensor_batarya, kayit::Kanal::sensor_batarya>(t, yayin_b_, y);
        }
    }
}

// Sonar kanalları (W4-bagla-sonar; A3 §3.3 kanal 30, 31): sınırda teslim edilen, karta verilen
// kayıtların aynısı (arıza zinciri sonrası). Zaman anlamı `gonderim` (A3-K26): Ping360 t_meas
// (gönderim anı, A1-K53), Ping1D t_meas − tx_offset_ns (A1-K54). Satır ve profil baytları
// kaydın kendi tamponundan (`span`) kodlanır: bu hizmette bellek ayrılmaz (W4-K27 (5)).
// `parti_step_seq` öteki sensör kanallarındaki gibi "yok" (köprü kaydın partisini dışa vermez;
// Ping360'ın teslim partisi HEAD'in partisidir ve yayın anında henüz belli değildir).
void Calistirici::a3_sonar_yayinla()
{
    for (const sonar::SonarOlcumu& o : sonar_olcumler_) {
        if (const auto* q = std::get_if<sonar::Ping360Olcumu>(&o.yuk)) {
            a3::yuk_ping360 y;
            olcum_basligi(y, o.hdr);
            y.aci_grad = q->angle_grad;
            y.n_ornek = static_cast<std::uint16_t>(q->data.size());
            y.ornek_periyodu = q->sample_period;
            y.darbe_suresi_us = q->tx_duration_us;
            y.frekans_khz = q->tx_freq_khz;
            y.kazanc = q->gain;
            y.kademe = q->kademe;
            y.ses_hizi_mps = q->ses_hizi_mps;
            y.ornekler = std::span<const std::uint8_t>(q->data.data(), q->data.size());
            const std::int64_t t = o.hdr.t_meas_ns;
            yayin<a3::Kanal::sonar_ping360, kayit::Kanal::sonar_ping360>(t, yayin_b_, y);
        } else if (const auto* p = std::get_if<sonar::Ping1dOlcumu>(&o.yuk)) {
            a3::yuk_ping1d y;
            olcum_basligi(y, o.hdr);
            if ((o.hdr.flags & sensor::kBayrakGecerli) != 0u) {
                y.mesafe_m = p->distance;   // geçersizse "yok" (NaN; A3 §3.4)
            }
            y.guven = static_cast<float>(p->confidence) / 100.0F;   // A3 [0, 1]; A1 %
            y.kademe = p->kademe;
            y.profil = std::span<const std::uint8_t>(p->profil.data(), p->profil.size());
            const std::int64_t t = o.hdr.t_meas_ns - static_cast<std::int64_t>(p->tx_offset_ns);
            yayin<a3::Kanal::sonar_ping1d, kayit::Kanal::sonar_ping1d>(t, yayin_b_, y);
        }
    }
}

// Yayın noktası (dünya iş parçacığı; fizik sınırındaki sıranın sonu). W4-duraklat: duran kipte
// olaylar, sonuçlar, oturum ve geçerlilik her turda; sim zamanına bağlı yayınlar (sensör ve sonar
// kayıtları, 50/10/2 Hz kanallar) yalnız ilerleyen kipte (yinelenmez); durum kanalları değişimde ve
// duvar saatiyle 10 Hz.
// `fizik/durum` (A3: 50 Hz sim zamanı, `kHizliSinir`; A6: her kontrol periyodu sınırı, 200 Hz): aynı
// yapı iki hedefe de verilir (W5-K42); yapı yalnız en az bir hedef sırası gelince kurulur.
void Calistirici::fizik_yayinla(std::uint64_t b)
{
    const bool a3_sira = yay_ != nullptr && b % kHizliSinir == 0u;
    const bool kayit_sira = kb_ != nullptr && b % kopru::kPeriyotAdim == 0u;
    if (!a3_sira && !kayit_sira) {
        return;
    }
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    const fizik::DurumAnlik& d = dn_.durum();
    std::array<a3::yuk_fizik_durum_govdeler, 1> govde{};
    govde[0].govde_id = 1;
    govde[0].tur = 0;
    const int dolu = (plan_.dolu[0] ? 1 : 0) + (plan_.dolu[1] ? 1 : 0);   // fiziğin kurulumdaki kütle durumu
    govde[0].kutle_durumu = static_cast<std::uint8_t>(dolu == 2 ? 0 : dolu == 1 ? 1 : 2);
    govde[0].p_n = {d.konum_n[0], d.konum_n[1], d.konum_n[2]};
    govde[0].q_nb = {d.yonelim[0], d.yonelim[1], d.yonelim[2], d.yonelim[3]};
    govde[0].nu = {d.dogrusal_hiz_b[0], d.dogrusal_hiz_b[1], d.dogrusal_hiz_b[2],
                   d.acisal_hiz_b[0], d.acisal_hiz_b[1], d.acisal_hiz_b[2]};
    {
        // a_n = R(q) (ν̇₁:₃ + ω × v): O_b'nin NED eylemsiz ivmesi (04).
        const auto& v = d.dogrusal_hiz_b;
        const auto& w = d.acisal_hiz_b;
        const double ax = d.dogrusal_ivme_b[0] + (w[1] * v[2] - w[2] * v[1]);
        const double ay = d.dogrusal_ivme_b[1] + (w[2] * v[0] - w[0] * v[2]);
        const double az = d.dogrusal_ivme_b[2] + (w[0] * v[1] - w[1] * v[0]);
        const double qw = d.yonelim[0], qx = d.yonelim[1], qy = d.yonelim[2], qz = d.yonelim[3];
        govde[0].a_n = {(1.0 - 2.0 * (qy * qy + qz * qz)) * ax + 2.0 * (qx * qy - qw * qz) * ay + 2.0 * (qx * qz + qw * qy) * az,
                        2.0 * (qx * qy + qw * qz) * ax + (1.0 - 2.0 * (qx * qx + qz * qz)) * ay + 2.0 * (qy * qz - qw * qx) * az,
                        2.0 * (qx * qz - qw * qy) * ax + 2.0 * (qy * qz + qw * qx) * ay + (1.0 - 2.0 * (qx * qx + qy * qy)) * az};
    }
    std::array<a3::yuk_fizik_durum_iticiler, 8> itici{};
    const double notr = static_cast<double>(arac_.esc.pwm_notr_us);
    for (std::size_t i = 0; i < itici.size(); ++i) {
        const double p = d.pwm_us[i];
        const double u = p >= notr ? (p - notr) / std::max(1.0, static_cast<double>(arac_.esc.pwm_max_us) - notr)
                                   : (p - notr) / std::max(1.0, notr - static_cast<double>(arac_.esc.pwm_min_us));
        itici[i].u = static_cast<float>(std::clamp(u, -1.0, 1.0));
        itici[i].itki_n = static_cast<float>(d.itki[i]);
        itici[i].akim_a = std::numeric_limits<float>::quiet_NaN();   // fizikte güç modeli yok
    }
    a3::yuk_fizik_durum f;
    f.adim_no = b;
    f.govdeler = govde;
    f.iticiler = itici;
    f.bara_gerilimi_v = static_cast<float>(arac_.batarya.v_sabit);   // ideal kaynak (A5 arac.batarya)
    f.paket_akimi_a = std::numeric_limits<float>::quiet_NaN();
    f.soc = std::numeric_limits<float>::quiet_NaN();
    if (a3_sira) {
        a3_olc([&] { (void)yay_->yayinla<a3::Kanal::fizik_durum>(t, f); });
    }
    if (kayit_sira) {
        kayit_yaz<kayit::Kanal::fizik_durum>(t, b, f);
    }
}

// `kart/durum` (50 Hz sim zamanı): aynı yapı iki hedefe de (W5-K42).
void Calistirici::kart_yayinla(std::uint64_t b)
{
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    if (b % kHizliSinir == 0u) {
        const kopru::KartBilgisi& kb = k_->kart();
        // Yalnız yeni bildirimde (son IDA_NAV ya da IDA_STATUS değişti).
        const bool yeni = kb.nav.var != a3_nav_var_ || kb.nav.batch_seq != a3_nav_seq_ || kb.nav.t_ns != a3_nav_t_ ||
                          kb.durum.batch_seq != a3_durum_seq_ || kb.durum.var != a3_durum_var_;
        if ((kb.nav.var || kb.durum.var) && yeni) {
            a3_nav_var_ = kb.nav.var;
            a3_nav_seq_ = kb.nav.batch_seq;
            a3_nav_t_ = kb.nav.t_ns;
            a3_durum_seq_ = kb.durum.batch_seq;
            a3_durum_var_ = kb.durum.var;
            a3::yuk_kart_durum k;
            k.kart_t_ns = kb.nav.var ? kb.nav.t_ns : 0;
            k.acilis_sayaci = kb.boot_count;
            if (kb.durum.var) {
                k.kip = kb.durum.mode;
                k.silahli = kb.durum.armed;
                k.saglik = kb.durum.health;
            } else {
                k.silahli = 255u;
            }
            if (kb.nav.var) {
                k.kestirim_gecerli = kb.nav.est_level == IDA_EST_OK || kb.nav.est_level == IDA_EST_DEGRADED ? 1u : 0u;
                k.p_n = kb.nav.p_ned;
                k.v_n = kb.nav.v_ned;
                k.q_nb = kb.nav.q_nb;
                k.derinlik_m = kb.nav.depth;
            } else {
                k.kestirim_gecerli = 255u;
            }
            yayin<a3::Kanal::kart_durum, kayit::Kanal::kart_durum>(t, b, k);
        }
    }
}

// `kart/cekirdek_sayaclari` (2 Hz): aynı yapı iki hedefe de (W5-K42).
void Calistirici::cekirdek_yayinla(std::uint64_t b)
{
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    if (b % kYavasSinir == 0u) {
        const kopru::KartBilgisi& kb = k_->kart();
        if (kb.sayac.var) {
            const kopru::KartSayaclari& c = kb.sayac;
            a3::yuk_kart_cekirdek_sayaclari y;
            y.acilis_sayaci = kb.boot_count;
            y.sample_rejected = c.sample_rejected;
            y.sample_future = c.sample_future;
            y.sample_conflict = c.sample_conflict;
            y.sample_dup_dropped = c.sample_dup_dropped;
            y.narrow_from_rejected = c.narrow_from_rejected;
            y.bg_bad_input = c.bg_bad_input;
            y.imu_rejected = c.imu_rejected;
            y.imu_gap = c.imu_gap;
            y.bad_input = c.bad_input;
            y.dup_seq = c.dup_seq;
            y.old_seq = c.old_seq;
            y.wall_backstep = c.wall_backstep;
            y.cmd_rejected = c.cmd_rejected;
            y.widen_blocked = c.widen_blocked;
            y.evres_ignored = c.evres_ignored;
            y.event_unknown = c.event_unknown;
            y.det_rejected = c.det_rejected;
            y.ping360_rejected = c.ping360_rejected;
            y.bg_dropped = c.bg_dropped;
            y.bg_slice_mismatch = c.bg_slice_mismatch;
            y.log_dropped = c.log_dropped;
            y.step_miss = c.step_miss;
            y.saglik = kb.durum.var ? kb.durum.health : 0u;
            yayin<a3::Kanal::kart_cekirdek_sayaclari, kayit::Kanal::kart_cekirdek_sayaclari>(t, b, y);
        }
    }
}

void Calistirici::a3_yayinla(std::uint64_t b, bool duran)
{
    yayin_b_ = b;
    A3Hizmet hizmet(a3_sinir_ns_);
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    a3_olay_kararlari();
    a3_yi_izle();
    a3_oturum_izle();
    a3_gecerlilik(b);
    if (duran) {
        if (a3_durum_kirli_ || gercek() - duran_yayin_duvar_ >= 100 * kMs) {
            a3_durum_yayinla(t);
            duran_yayin_duvar_ = gercek();
        }
        if (a3_anlik_kirli_) {
            a3_anlik_guncelle();
        }
        return;
    }
    a3_sensor_yayinla();
    if (sonar_) {
        a3_sonar_yayinla();
    }
    fizik_yayinla(b);
    kart_yayinla(b);
    if (b % kHizliSinir == 0u) {
        a3_ariza_izle(b);
    }
    if (b % kDurumSinir == 0u || a3_durum_kirli_) {
        a3_durum_yayinla(t);
    }
    cekirdek_yayinla(b);
    // A3-K72: kaçırılan partiler en çok saniyede bir özetlenir (duvar).
    if (a3_kacirma_adet_ > 0u && gercek() - a3_kacirma_olay_duvar_ >= 1'000'000'000) {
        a3b::JsonYaz j;
        j.sayi_u("ilk_step_seq", a3_kacirma_ilk_).sayi_u("son_step_seq", a3_kacirma_son_).sayi_u("adet", a3_kacirma_adet_);
        a3_olay(4, 1, 0, 0, 0, j.bitir());
        a3_kacirma_adet_ = 0;
        a3_kacirma_olay_duvar_ = gercek();
    }
    if (b % kDurumSinir == 0u || a3_anlik_kirli_) {
        a3_anlik_guncelle();
    }
}

// Koşu sonu: kalan komutlar reddedilir, son durum yayınlanır, sunucu düzgün durur (A3-K49).

// W4-K27 (4): `hazirlaniyor`'da (A1 el sıkışması ve hazırlık oturumu beklemeleri) çalıştırma
// denetimi işlenir; kabul edilen `durdur` çalıştırmayı `yuklendi`'ye döndürür ve hazırlık biter
// (sonradan kendiliğinden başlamaz). Emniyet ve yer istasyonu istekleri birikir (kısıtlayanlar
// yuvasında, ötekiler en çok kGenelAzami) ve ilk sınırda işlenir. Dönüş true: durduruldu.
// `kanca` false: sınama kancası çağrılmaz (gecis_kilidi altındaki son çekiş; kanca `al` çağırır).
bool Calistirici::a3_hazirlik_durdu(bool kanca)
{
    if (yay_ == nullptr || a3d_.durum != a3::CalistirmaDurumu::hazirlaniyor) {
        return yay_ != nullptr && a3_durdur_;
    }
    if (kanca && a_.a3_sinir_kancasi) {
        a_.a3_sinir_kancasi(0, *a3al_);
    }
    a3_cek(A3Alici::kButunYuvalar);
    a3_calistirma_isle(0);
    if (a3_durum_kirli_) {
        a3_durum_yayinla(0);
    }
    return a3_durdur_;
}

// A3 kayıtlarını Sonuc'a çevirir (koşu sonunda; kapsam 4: senaryo olaylarıyla aynı biçim).
void Calistirici::a3_kayitlari_aktar()
{
    for (const A3KomutKaydi& r : a3_komut_kaydi_) {
        std::string ad = calistirma_adi(r.tur);
        if (r.tur == a3::KomutTuru::yer_istasyonu_komut) {
            ad += "/";
            ad += r.yi_ad != nullptr ? r.yi_ad : "";
        }
        s_.olay_sonuclari.push_back({"a3/" + std::to_string(r.komut_no) + "/" + ad, r.uygulandi ? "uygulandi" : "reddedildi",
                                     r.t_ns, r.kod != nullptr ? r.kod : "", "a3"});
    }
    a3_komut_kaydi_.clear();
    for (const A3EmniyetKaydi& e : a3_emniyet_kaydi_) {
        s_.emniyet_degisimleri.push_back({e.t_ns, e.alan, e.onceki, e.sonraki, e.operator_ ? "operator" : "senaryo",
                                          e.komut_no, e.emniyet_rev});
    }
    a3_emniyet_kaydi_.clear();
}

// Koşu sonu: kalan istekler reddedilir, son durum yayınlanır, sunucu düzgün durur (A3-K49). W5-a3-senaryo:
// sıradaki dünya istendiyse (`sifirla`, `senaryo/yukle`) ya da süreklilik kipinde çalıştırma bittiyse sunucu
// kapanmaz: A3 hizmeti süreç boyunca yaşar ve kapanışı sürücüye (ya da süreklilik beklemesinin sonuna) kalır.
void Calistirici::a3_kalanlari_reddet()
{
    // Sıradaki dünya istendi ya da süreklilik beklemesi gelecekse alıcıda BEKLEYEN istekler çekilmez: onları
    // o işler (sıfırlamadan hemen sonraki `baslat` ya da kapanış sırasında gelen `sifirla` reddedilip kaybolmaz).
    // W5-K56 (EMNİYET; A3-K57): bu dünyanın alıcıdan ÇEKTİĞİ istekler ise her durumda burada sonuçlanır.
    // `sifirla`/`senaryo/yukle` ile aynı sınırda çekilip uygulanmamış kısıtlayanlar, hazırlıkta ya da son turda
    // birikmiş emniyet ve yer istasyonu istekleri dünya nesnesiyle birlikte sessizce yok olmaz: nihai sonuçlarını
    // (`canli_calistirma_yok`) alır ve bu dünyanın kaydına geçer (`main`'deki davranış).
    if (!a3_devam_edecek()) {
        a3_cek(0);
    }
    for (const A3Giris& g : a3_kisit_) {
        a3_sonuc(g, false, a3_b_, "canli_calistirma_yok");
    }
    for (const A3Giris& g : a3_genel_) {
        a3_sonuc(g, false, a3_b_, a3b::calistirma_mi(g.k.tur) ? "gecersiz_durum" : "canli_calistirma_yok");
    }
    a3_kisit_.clear();
    a3_genel_.clear();
}

void Calistirici::a3_sunucuyu_durdur()
{
    if (yay_ == nullptr) {
        return;
    }
    if (a_.a3_kapanis_bekleme_s > 0.0) {
        // Son çerçevelerin yayın tikinde (A3-K33, 20 ms) gönderilmesi için.
        const auto son = Saat::now() + std::chrono::milliseconds(static_cast<std::int64_t>(a_.a3_kapanis_bekleme_s * 1000.0));
        while (Saat::now() < son) {
            a3_kalanlari_reddet();
            sure_uyu(5 * kMs);
        }
    }
    if (kalici_ != nullptr && kalici_->hizmet) {
        kalici_->hizmet->kapaniyor();   // bekleyen senaryo/yukle çağrısı G/Ç iş parçacığını tutmasın
    }
    a3sv_->durdur();
    yay_ = nullptr;
}

void Calistirici::a3_bitir()
{
    if (yay_ == nullptr) {
        return;
    }
    a3_kalanlari_reddet();
    if (k_) {
        a3_kapaniyor_ = true;   // planlı kapanış (A1-K21): oturum değişiminin nedeni
        a3_yi_izle();
        a3_oturum_izle();
    }
    a3_durum_yayinla(static_cast<std::int64_t>(a3_b_) * kH);
    a3_anlik_guncelle();
    s_.a3_halka_tasma = yay_->halka_tasma();
    if (!a3_devam_edecek()) {
        a3_sunucuyu_durdur();
    }
    a3_kayitlari_aktar();
}

// A3 hizmeti bu dünyadan sonra da yaşayacak mı (sıradaki dünya istendi ya da süreklilik kipinde çalıştırma
// sona erdi / başlamadan bitti)? Hata ile biten kurulum ve sinyal sürekliliği bitirir.
bool Calistirici::a3_devam_edecek() const
{
    if (a3_yeniden_) {
        return true;
    }
    if (kalici_ == nullptr || !kalici_->surekli || kapanis_istendi() || s_.sinyal_kesti) {
        return false;
    }
    switch (a3d_.durum) {
    case a3::CalistirmaDurumu::bitti:
    case a3::CalistirmaDurumu::durduruldu:
    case a3::CalistirmaDurumu::hata: return true;
    case a3::CalistirmaDurumu::yuklendi: return s_.hata.empty() || a3_on_denetim_kaldi_;
    default: return false;
    }
}

// Süreklilik kipinde, çalıştırma başlamadan biten dünya (hazırlıkta durdurma, ön denetimde kalma) için
// komutsuz yeniden kurma isteği: `yuklendi` durumu sürer, yeni dünya `baslat` bekler.
bool Calistirici::a3_yeniden_iste_yeniden()
{
    if (kalici_ == nullptr || !kalici_->surekli || kapanis_istendi() || a3_yeniden_) {
        return false;
    }
    A3Kalici::Istek ist;
    ist.tur = A3Kalici::Istek::Tur::yeniden;
    ist.kabul = Saat::now();
    a3_yeniden_ = std::move(ist);
    return true;
}

// Süreklilik beklemesi (`--a3-surekli`): çalıştırma bitti/durduruldu/hata, dünya durdu, ama A3 sunucusu
// ayakta ve durum makinesi sürüyor. `sifirla` ve `senaryo/yukle` (Tablo 4.7a) alınır ve yeni dünya istenir
// (`a3_yeniden_` dolar, true); öteki her istek bu durumda geçersizdir. false: kapanış istendi (sinyal).
bool Calistirici::a3_bosta_bekle()
{
    gunluk(1, std::string("A3: çalıştırma ") + a3b::durum_adi(a3d_.durum) +
                  "; `calistirma/sifirla` ya da `senaryo/yukle` bekleniyor (--a3-surekli)");
    std::int64_t son_yayin = gercek();
    for (;;) {
        if (kapanis_istendi()) {
            return false;
        }
        if (a_.a3_sinir_kancasi) {
            a_.a3_sinir_kancasi(0, *a3al_);
        }
        a3_cek(0);
        for (const A3Giris& g : a3_kisit_) {
            a3_sonuc(g, false, a3_b_, "canli_calistirma_yok");
        }
        a3_kisit_.clear();
        a3_calistirma_isle(a3_b_);   // sifirla ve senaryo/yukle (başka hiçbir denetim komutu bu durumda geçerli değildir)
        for (const A3Giris& g : a3_genel_) {
            a3_sonuc(g, false, a3_b_, "canli_calistirma_yok");
        }
        a3_genel_.clear();
        if (a3_yeniden_) {
            return true;
        }
        if (gercek() - son_yayin > 100 * kMs) {
            a3_durum_yayinla(static_cast<std::int64_t>(a3_b_) * kH);
            a3_anlik_guncelle();
            son_yayin = gercek();
        }
        sure_uyu(2 * kMs);
    }
}

// Ön denetim raporu (A3 §3.8): çalıştırma başlarken bir kez. Kayıt açıksa dosyanın eki olan belge (aynı
// üretici; W5-K49), değilse aynı üreticiyle kayıtsız biçim. `GET /on_denetim` bunu verir.
void Calistirici::a3_on_denetim_kaydet()
{
    if (kalici_ == nullptr || !kalici_->hizmet) {
        return;
    }
    if (kb_) {
        kalici_->hizmet->on_denetim_ekle(kb_->on_denetim_kimligi(), kb_->on_denetim_json());
        return;
    }
    OnDenetimGirdisi g;
    g.run_id = a3_run_id_;
    g.kip = a_.kip == Kip::hil ? "hil_1x" : "sil";
    const OnDenetimRaporu r = on_denetim_kur(g);
    kalici_->hizmet->on_denetim_ekle(r.kimlik, r.json);
}

// Her çıkışın tek kapanış yolu: dünya döngüsü ve A1 kapanışı bitti. A2 ve kayıt kapanır (yeni dünya ya da
// süreklilik beklemesi varsa A2 sunucusu ayakta kalır; kayıt dosyası her durumda burada kapanır: çalıştırma
// başına bir dosya, A6). W5-a3-senaryo: sıradaki dünya istendiyse sonucu sürücüye bırakır; süreklilik
// kipinde çalıştırma bitmişse sunucu ayakta `sifirla`/`senaryo/yukle` bekler.
void Calistirici::kosu_bitir(Sonuc& r) noexcept
{
    if (kalici_ != nullptr && k_) {
        // W5-K55 (EMNİYET; A3-K117, A3-K67): dünyanın köprüsü yıkılmadan önce son revizyon ve doyma bayrağı
        // süreç boyunca yaşayan duruma alınır. Oturum sonu artışı bu noktada uygulanmıştır (oturum_kapat +
        // kapanis_daraltmasi; her çıkış yolunda). Değer yalnız artar; doyma bir kez kurulunca kalkmaz.
        kalici_->emniyet_rev = std::max(kalici_->emniyet_rev, k_->emniyet_revizyonu());
        kalici_->emniyet_rev_doydu = kalici_->emniyet_rev_doydu || k_->emniyet_rev_doydu();
    }
    if (!a3_yeniden_ && yay_ != nullptr && a3d_.durum == a3::CalistirmaDurumu::yuklendi && a3_devam_edecek()) {
        // Çalıştırma başlamadan bitti (hazırlıkta durdurma, ön denetimde kalma): `yuklendi`'de kal; yeni dünya
        // `baslat` bekler. Başka bir hata (kurulum) sürekli yeniden denemeye dönmez: süreç bugünkü gibi biter.
        (void)a3_yeniden_iste_yeniden();
    }
    const bool bekle = !a3_yeniden_ && yay_ != nullptr && a3_devam_edecek();
    a2_kapat(r, !(a3_yeniden_ || bekle));
    kayit_kapat(r);   // W5-bagla-kayit: A1 kapanışından ve A2 kapanışından sonra, aynı tek kapanış yolu
    r.dunya_sayisi = kalici_ != nullptr ? kalici_->dunya_no : 1u;
    r.yeniden_kurma_ns = s_.yeniden_kurma_ns;
    if (a_.kosu_sonu_kancasi) {
        try {
            a_.kosu_sonu_kancasi(r);
        } catch (...) {
        }
    }
    if (bekle) {
        try {
            const bool yeni = a3_bosta_bekle();
            if (!yeni) {
                a2_kapat(r, true);
                a3_sunucuyu_durdur();
            }
        } catch (...) {
            a3_yeniden_.reset();
            try {
                a3_sunucuyu_durdur();
            } catch (...) {
            }
        }
    }
    if (kalici_ != nullptr && a3_yeniden_) {
        kalici_->istek = std::move(a3_yeniden_);
        a3_yeniden_.reset();
        kalici_->durum_rev = a3d_.durum_rev;
    }
}

// ======================================================================== Sınır turu (W4-duraklat)
//
// Tek kod yolu (W4-K28 (2); EMNİYET): ilerleyen ve duran kip aynı işlevdir; duran kipte atlanan yalnız
// (1) senaryonun zaman çizelgesi (sinir_on), (2) sim zamanını ilerleten işler: HIL'de duvar saatine
// kilitli bekleme, SIL hız beklemesi, sensör/sonar adımı (sinir_on), parti ve yuva beklemesi (TF sim
// zamanını taşır; burada `kontrol`), fizik adımı (çağıranda), (3) üye gönderimi (köprünün duran
// kipi: dar_hizmet ve daraltıcının tele yazımı). Geri kalan her iş (istek çekme ve çalıştırma
// denetimi, kart çıktılarının alımı, A1-K85'in beş adımı, kısıtlayan yuvaların boşaltılması, acil
// durdurma engeli, itici girdisinin yeniden değerlendirilmesi, SIL'in kısıtlayan erken uygulaması,
// yayın) iki kipte aynı sırayla aynı koddur.
bool Calistirici::sinir_turu(std::uint64_t b, bool duran)
{
    const std::int64_t t_b = static_cast<std::int64_t>(b) * kH;
    // W4-sunucu-kucuk: kapanış sinyali her turda (duran kipte de) görülür; hata kapanışına girilir.
    if (sinyal_kesti()) {
        return false;
    }
    // W4-bagla-a3: istek çekme ve çalıştırma denetimi turun başında (her turda; duran kipte
    // sürdür/adımla/durdur burada alınır). Duraklatma ve çıkış turun sonunda (duraklatma_gecisi).
    if (yay_ != nullptr && !a3_sinir_basi(b, duran)) {
        return false;
    }
    if (yay_ == nullptr && !duran) {
        sil_hiz_bekle(b);   // --sil-hiz, A3 kapalıyken (A3 açıkken a3_sinir_basi'nde; aynı işlev)
    }
    if (a_.kip == Kip::sil) {
        if (duran) {
            // SIL duran kip: sim zamanı ve kilitli adım donuk; alım kuyruğu köprüye yalnız sim'in
            // bekleme noktalarında verilir (W4-K25 (2)), duran tur bekleme noktası değildir.
            sure_uyu(kDuranTurNs);
        }
        sil_duvar_ = t_b;
        pompa(0);
        k_->zamanla(duvar());
    } else if (duran) {
        // HIL duran kip (A1-K41): duvar saatine kilitli bekleme yok; kart çıktıları her turda
        // alınıp köprüde tüketilir, IDA_KEEPALIVE sürer (zamanla).
        evre_.fill(0);
        bekle_ve_oku(gercek() + kDuranTurNs);
    } else {
        evre_.fill(0);
        bekle_ve_oku(w0_ + t_b - kCSim);
        evre_[kEvreUyanma] = std::max<std::int64_t>(0, gercek() - (w0_ + t_b - kCSim));
        // A1-K108: uygulama anları a_n kontrol sınırlarıdır (L_act = 2P).
        if (b % kopru::kPeriyotAdim == 0 && gercek() > w0_ + t_b) {
            ++s_.zaman.sim_gecikmesi;
            k_->sim_gecikmesi_bildir();   // A1-K108, K79 ortak yol (sol #6)
        }
    }
    if (duran) {
        ++duran_tur_;
    }
    baglanti_bakim();
    if (!sinir_on(b, duran)) {
        return false;
    }
    // (2)/(3): parti (TF sim zamanını taşır) yalnız ilerleyen kipte; sınır b'nin partisi, varsa,
    // duraklatmadan önceki ilerleyen turda gönderildi.
    const bool kontrol = !duran && b > 0 && b % kopru::kPeriyotAdim == 0;
    std::uint32_t seq = 0;
    if (kontrol) {
        if (a_.kip == Kip::hil) {
            std::int64_t hedef = w0_ + t_b;
            if (son_tf_yazim_ >= 0) {
                hedef = std::max(hedef, son_tf_yazim_ + kP / 2);   // A1-K39
            }
            bekle_ve_oku(hedef);
            evre_[kEvreTfUyanma] = std::max<std::int64_t>(0, gercek() - hedef);
        }
        if (id_disarm_ != 0u && s_.t_disarm_ns < 0) {
            s_.t_disarm_ns = t_b;   // DISARM'ı taşıyan partinin kapanışı
        }
        const std::int64_t yazim = gercek();
        const std::optional<std::uint32_t> gon = k_->parti_kapat(t_b, duvar(), a_.kip == Kip::hil ? w0_ + t_b : -1);
        evre_[kEvreParti] += gercek() - yazim;
        yaz();
        if (gon) {
            seq = *gon;
            tf_yazim_.push_back(yazim);
            son_tf_yazim_ = yazim;
            if (a_.kip == Kip::hil) {
                s_.zaman.gec_yazim_en_cok_ns = std::max(s_.zaman.gec_yazim_en_cok_ns, yazim - (w0_ + t_b));
                tf_gec_.emplace_back(yazim - (w0_ + t_b), b);
            }
        }
        const std::int64_t e_s = gercek();
        satir_ekle(b, gon);
        evre_[kEvreSatir] += gercek() - e_s;
        if (a_.kip == Kip::sil) {
            if (gon) {
                const Bekleme r = yuva_bekle(seq);
                if (r == Bekleme::hata || (r == Bekleme::oturum_bitti && !oturum_kur())) {
                    return false;
                }
            }
            satirlari_tamamla(gon ? 1u : 0u);   // W4-K14: önceki satır ACTUATOR(n)'den sonra
        }
    } else {
        yaz();   // duran kipte HIL telinde yalnız IDA_KEEPALIVE (ve oturum kapanışı/el sıkışma)
    }
    if (a_.kip == Kip::sil && k_->durum() != kopru::OturumDurumu::kosuyor) {
        // A1-K121 (iv): SIL'de sim zamanı el sıkışmada ilerlemez. Oturum yuva beklemesi
        // dışında (sim tarafında; ör. A1-K111) bittiyse yeni oturum fizik ilerlemeden kurulur.
        gunluk(1, "oturum bitti (sınır " + std::to_string(b) + "); yeniden kuruluyor");
        ++s_.yeniden_baglanti;
        if (!oturum_kur()) {
            return false;
        }
    }
    if (a_.kip == Kip::sil) {
        // W4-K26 (1) (EMNİYET): bu sınırın yuva beklemesinde işlenen kısıtlayan çerçeveler
        // (oturum sonu, eşzamansız güvenli çıktı) fizik bu sınırdan ilerlemeden yürürlüğe girer.
        const kopru::KisitSonucu kisit = k_->sil_kisitlayan_uygula(b);
        if (kisit.degisti) {
            pwm_yur_ = kisit.pwm_us;
            std::array<double, 8> girdi = kisit.pwm_us;
            if (estop_bu_adim_) {
                girdi.fill(static_cast<double>(arac_.esc.pwm_notr_us));
            }
            (void)dn_.itici_komutu_ver(b, girdi);
            gozlem_.pwm = girdi;
        }
    }
    // Sınama kancası fizik adımı b'nin girdisini görür: yalnız bu turdan sonra fizik koşacaksa
    // (çalıştırma duraklatılmış değilse; durum yalnız turun başında değişir).
    if (a_.sinir_kancasi && !(yay_ != nullptr && a3d_.durum == a3::CalistirmaDurumu::duraklatildi)) {
        a_.sinir_kancasi(gozlem_);
    }
    if (yay_ != nullptr) {
        // Yayın noktası: fizik sınırındaki sıranın sonu (A3 emniyet istekleri sinir_isle'den
        // önce, köprü, sensörler, parti, yuva, SIL'in kısıtlayan erken uygulaması), fizik
        // adımından hemen önce; durum b sınırınındır (CSV satırıyla aynı an). Sırayı değiştirmez.
        a3_yayinla(b, duran);
        s_.a3_sinir_en_kotu_ns = std::max(s_.a3_sinir_en_kotu_ns, a3_sinir_ns_);
    }
    if (kb_ && !duran) {
        // W5-bagla-kayit: kayıt çağrıları, emniyet girdilerinin uygulanmasından (A1-K85, A3 emniyet
        // istekleri) ve karta gidecek partinin hazırlanmasından (parti_kapat, yaz) SONRA; hiçbir dalda
        // onları geciktirmez ya da koşula bağlamaz. A3 açıksa kayıt çağrıları a3_yayinla içinde (aynı yapı,
        // iki hedef; W5-K42), kapalıyken burada. Üretici çağrısı beklemez; kilit, G/Ç, bekleme yok.
        if (yay_ == nullptr) {
            kayit_yayinla(b);
        }
        kayit_denetim();
    }
    if (a2_ && !duran) {
        // W5-bagla-a2: A2 yayın noktası (A2-K20–K23). Turun SON işidir: A3 emniyet istekleri ve
        // köprünün emniyet işlemleri (sinir_on → A1-K85), acil durdurma engeli, itici komutu, sensör
        // ve sonar adımı, parti kapanışı ve yazımı, yuva beklemesi, SIL'in kısıtlayan erken uygulaması
        // ve A3 yayını bu satırdan ÖNCE tamamlanmıştır; hiçbiri bu çağrıya bağlı değildir. Yalnız
        // ilerleyen turda ve sınır başına bir kez (duran turlarda durum gitmez: A2-K24). Durum sınır
        // b'nindir (dn_.durum().adim_no = b; CSV satırı ve A3 fizik/durum ile aynı an), fizik adımı
        // b'den önce. Çağrı engellemez, kilit almaz, bellek ayırmaz, G/Ç yapmaz (A2-K47).
        a2_->adim_siniri(dn_.durum());
    }
    return true;
}

// W4-duraklat geçişleri, turun sonunda (A3 durumu yalnız turun başında, a3_sinir_basi'nde değişir):
//   giriş  bu tur ilerleyen kipteydi ve çalıştırma duraklatıldı: sınır b'nin olağan işlemi tamam;
//          fizik adımı b'den ÖNCE duran kipe geçilir (köprünün üye gönderimi durur).
//   çıkış  bu tur duran kipteydi ve çalıştırma sürdürüldü (surdur, adimla): turun emniyet kesimi ve
//          A1-K85 adımları tamamlandı, itici girdisi bu turda yeniden değerlendirildi; fizik adımı b
//          bu girdiyle koşar. HIL'de W(t) duraklatma süresi kadar kaydırılır (yetişme koşusu yok);
//          SIL hız çarpanının başvurusu yenilenir. A1-K29: TF'ler boşluksuz sürer (sim zamanı donuktu).
bool Calistirici::duraklatma_gecisi(std::uint64_t b, bool duran)
{
    const bool durakli = yay_ != nullptr && a3d_.durum == a3::CalistirmaDurumu::duraklatildi;
    if (durakli) {
        if (!duran) {
            k_->duraklatma_ayarla(true);
            duraklama_duvar_ = gercek();
            duran_yayin_duvar_ = duraklama_duvar_;
            duran_yi_verilen_ = 0;
            if (a2_) {
                a2_->duraklat();   // A2-K24: bu turda verilen sınır b'nin durumu, sonra `duraklat` olayı
            }
            gunluk(2, "duraklatıldı (sınır " + std::to_string(b) + ")");
        }
        return true;
    }
    if (duran) {
        k_->duraklatma_ayarla(false);
        if (a2_) {
            a2_->surdur();   // `devam` olayı; akış sınır b + 1'den sürer (adımlamada da)
        }
        if (a_.kip == Kip::hil) {
            w0_ += gercek() - duraklama_duvar_;
        } else if (hiz_ > 0.0) {
            hiz_t0_sim_ = static_cast<std::int64_t>(b) * kH;
            hiz_t0_duvar_ = Saat::now();
        }
        gunluk(2, "sürdürüldü (sınır " + std::to_string(b) + ")");
    }
    return false;
}

// ======================================================================== A6 kayıt (W5-bagla-kayit)
//
// Kayıt çağrıları dünya döngüsünde emniyet girdilerinin uygulanmasından ve karta gidecek partinin
// hazırlanmasından SONRA gelir (sinir_turu: A3 yayın noktasının yanı); hiçbir dalda onları geciktirmez
// ya da koşula bağlamaz. Üretici çağrısı beklemez (kilit, bellek ayırma, G/Ç yok: kitaplığın sözü).
// `kapanis_taslagi` kilit alır; yalnız 1 Hz'te ve bir A3 komutu uygulanınca çağrılır (üretici yolu değil).

std::uint64_t Calistirici::kayit_dusen_() const
{
    const kayit::UreticiSayaclari u = kb_->uretici_sayaclari();
    return u.dusen_halka + u.dusen_gecersiz;
}

// Kapanış kaydının çalıştırmaya ait alanları (taslak ve kapanış aynı kaynaktan). `bitis`: A6-K45.
kayit::KapanisAlanlari Calistirici::kayit_alanlari(kayit::BitisDurumu bitis) const
{
    kayit::KapanisAlanlari a;
    a.bitis_durumu = bitis;
    const fizik::DurumAnlik& d = dn_.durum();
    a.son_adim_no = d.adim_no;   // tamamlanan fizik adımı sayısı
    a.son_sim_t_ns = static_cast<std::int64_t>(d.adim_no) * kH;
    a.gecerli = a3_gecerli_;
    a.gercek_zamani_kacirdi = a3_rt_kacirdi_;
    std::string n;
    for (unsigned bit = 0; bit < 32u; ++bit) {
        if (((a3_gecerlilik_ >> bit) & 1u) != 0u && !a3b::neden_adi(bit).empty()) {
            n += (n.empty() ? "" : ",");
            n += std::string(a3b::neden_adi(bit));
        }
    }
    a.gecerlilik_nedenleri = n;
    a.sayaclar = kayit_sayac_listesi(kayit_son_sayac_);
    // canli_mudahale_adimi: yalnız `tekrar_fizik` koşusunda anlamlıdır (A6-K53); bu dilimde tekrar kipi
    // yok (sil, hil_1x), müdahale alanı hiçbir zaman dolmaz.
    return a;
}

void Calistirici::kayit_taslak_hemen()
{
    if (!kb_ || kayit_kapandi_) {
        return;
    }
    const auto bas = Saat::now();
    kb_->taslak_guncelle(kayit_alanlari(kayit::BitisDurumu::hata));
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(Saat::now() - bas).count();
    s_.kayit.taslak_en_kotu_ns = std::max<std::int64_t>(s_.kayit.taslak_en_kotu_ns, ns);
}

// 1 Hz (duvar): kapanış taslağı, `kayit_eksik` ve `kayit_durumu` olayları (A6-K12, K30, K31; yalnız A3'te).
void Calistirici::kayit_denetim()
{
    const std::int64_t d = gercek();
    if (d - kayit_denetim_duvar_ < 1'000'000'000) {
        return;
    }
    kayit_denetim_duvar_ = d;
    kayit_taslak_hemen();
    const kayit::YaziciSayaclari ys = kb_->yazici_sayaclari();
    if (ys.eksik_kaydi > kayit_eksik_onceki_ || kayit_dusen_() > kayit_dusen_onceki_) {
        if (yay_ != nullptr) {
            const std::uint64_t dusen = kayit_dusen_();
            a3b::JsonYaz j;
            j.metin("kanal", "(birden_cok)").sayi_u("kayip_sayisi", std::max<std::uint64_t>(1u, dusen - kayit_dusen_onceki_));
            a3_olay(16, 1, 0, 0, 0, j.bitir());
        }
        kayit_eksik_onceki_ = ys.eksik_kaydi;
        kayit_dusen_onceki_ = kayit_dusen_();
    }
    const kayit::YaziciDurumu durum = kb_->durum();
    if (durum != kayit_durum_onceki_) {
        kayit_durum_onceki_ = durum;
        if (yay_ != nullptr) {
            const char* islem = durum == kayit::YaziciDurumu::disk_dolu       ? "disk_dolu"
                                : durum == kayit::YaziciDurumu::yazma_hatasi  ? "yazma_hatasi"
                                : durum == kayit::YaziciDurumu::sinira_ulasti ? "sinira_ulasti"
                                                                              : "kapandi";
            a3b::JsonYaz j;
            j.metin("islem", islem);
            a3_olay(15, 1, 0, 0, 0, j.bitir());
        }
    }
}

// A3 kapalıyken yayın noktası: dünya durumundan gelen kanallar (fizik, sensör, sonar, kart, durum).
// A3 durum makinesine bağlı kanallar (`olay`, `calistirma/durum`, `ariza/etkin`, `a3/komut_sonucu`) üretilmez.
void Calistirici::kayit_yayinla(std::uint64_t b)
{
    yayin_b_ = b;
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    a3_gecerlilik(b);   // `gecerli`/`gercek_zamani_kacirdi` ve `sayac/gercek_zaman` için (A3'e bağlı olay yazmaz)
    a3_sensor_yayinla();
    if (sonar_) {
        a3_sonar_yayinla();
    }
    fizik_yayinla(b);
    kart_yayinla(b);
    // `emniyet/girdiler` "değişimde" (A6-K18): bir sınırda bile olsa değişim yakalanır; ayrıca 10 Hz.
    const sensor::EmniyetDurumu e = k_->dis_durum().emniyet;
    const bool degisti = !kayit_emn_var_ || e.acil_durdurma != kayit_emn_.acil_durdurma || e.fis != kayit_emn_.fis ||
                         e.pim != kayit_emn_.pim || e.emniyet_revizyonu != kayit_emn_.emniyet_revizyonu;
    if (b % kDurumSinir == 0u || degisti || a3_durum_kirli_) {
        kayit_emn_ = e;
        kayit_emn_var_ = true;
        a3_durum_yayinla(t);
    }
    cekirdek_yayinla(b);
}

// `senaryo/oge_uygulama` (A6-K25): A6'ya özgü yük; A3'ün senaryo olayından (tür 21) ayrı doldurulur.
void Calistirici::kayit_oge(std::size_t i, const char* sonuc, std::int64_t t, const char* neden)
{
    const PlanOlayi& o = plan_.olaylar[i];
    kayit::yuk_senaryo_oge_uygulama y;
    y.olay_kimligi = o.kimlik;
    // A6 öğe türü: sensör ve itici arızası 0 (ariza), emniyet girdisi 1. A6'da `gcs_komutu` (ARM, DISARM,
    // SET_MODE) ve `guided` için açık karşılık yok (`gorev` = görev yüklemesi, `ortam` = ortam olayı), `diger`.
    y.tur = o.tur == PlanOlayTuru::emniyet ? 1u : (o.tur == PlanOlayTuru::sensor || o.tur == PlanOlayTuru::itici) ? 0u : 4u;
    const std::string_view sn(sonuc);
    const std::uint64_t adim = static_cast<std::uint64_t>(t / kH);
    y.sonuc = sn == "uygulandi" ? 0u : sn == "reddedildi" ? 1u : 2u;
    // Koşullu öğede koşulun sağlandığı sınır, zamanlıda uygulama adımı; hiç sağlanmayan öğede (uygulanmadi)
    // çalıştırmanın son adımı (kayıt çalıştırma sonunda yazılır).
    y.kosul_saglandi_adimi = (o.kosul && pd_[i].tetik_b >= 0) ? static_cast<std::uint64_t>(pd_[i].tetik_b) : adim;
    if (y.sonuc == 0u) {
        y.uygulama_adimi = adim;
    }
    const std::string_view nd(neden != nullptr ? neden : "");
    y.neden = nd == "gevsetme_reddedildi" ? 1u : nd == "desteklenmiyor" ? 2u : nd == "hedef_yok" ? 3u : nd == "kosul_saglanmadi" ? 4u : 0u;
    y.ayrinti = "{}";
    kayit_yaz<kayit::Kanal::senaryo_oge_uygulama>(t, adim, y);
}

// Kayıt dosyasını açar: dünya ve köprü kuruldu, A1 oturumu henüz kurulmadı, ilk fizik adımından önce.
// false: dosya açılamadı (aynı adlı dosya varsa A6-K01) → çalıştırma başlamaz.
bool Calistirici::kayit_baslat()
{
    s_.kayit.istendi = true;
    KayitBaglantiAyari ba;
    ba.dizin = a_.kayit_dizini;
    // A3 açıksa A3'ün kimliği (dosya adı = A3 run_id); yalnız sınamada sabitlenebilir.
    ba.run_id = !a_.kayit_run_id.empty() ? a_.kayit_run_id : (!a3_run_id_.empty() ? a3_run_id_ : a3b::run_id_uret());
    ba.a1_run_id = std::to_string(k_->run_id());
    ba.kip = a_.kip == Kip::sil ? "sil" : "hil_1x";
    ba.tohum = std::to_string(a_.tohum);
    ba.sil_n_bg = std::to_string(kopru::KarsiAyari{}.n_bg);   // sim `n_bg`'yi geçersiz kılmaz (koşucu varsayılanı)
    ba.depo_koku = a_.depo_koku;
    ba.a2_port = (a2_ && a2_->dinliyor()) ? std::to_string(a2_->port()) : std::string();
    ba.a3_acik = yay_ != nullptr;
    ba.sonar_acik = sonar_ != nullptr;
    ba.halka_bayt = a_.kayit_halka_bayt;
    ba.ilk_sira = a_.kayit_ilk_sira;
    ba.disk_siniri_bayt = a_.kayit_disk_siniri_bayt;
    ba.girdi = *kayit_girdi_;
    std::string hata;
    kb_ = KayitBaglanti::ac(ba, hata);
    if (!kb_) {
        s_.kayit.hata = hata;
        s_.hata = "kayıt açılamadı: " + hata;
        return false;
    }
    s_.kayit.acildi = true;
    s_.kayit.yol = kb_->yol().generic_string();
    s_.kayit.run_id = ba.run_id;
    s_.kayit.kademe = ba.girdi.kademe;
    kayit_denetim_duvar_ = gercek();
    gunluk(1, "kayıt: " + s_.kayit.yol + " (kademe " + s_.kayit.kademe + ")");
    if (a_.kayit_hazir) {
        a_.kayit_hazir(s_.kayit.yol, s_.kayit.run_id);
    }
    return true;
}

// Tek kapanış yolundan (kos): dünya döngüsü, A1 kapanışı ve A2 kapanışı bittikten sonra. İkinci çağrı etkisizdir.
void Calistirici::kayit_kapat(Sonuc& r) noexcept
{
    if (!kb_ || kayit_kapandi_) {
        return;
    }
    kayit_kapandi_ = true;
    try {
        kayit::BitisDurumu bitis = kayit::BitisDurumu::hata;
        if (r.sinyal_kesti) {
            bitis = kayit::BitisDurumu::sim_kapanisi;
        } else if (a3_on_denetim_kaldi_) {
            bitis = kayit::BitisDurumu::on_denetim_kaldi;   // A6-K45: A3-K62'deki ön denetim kaldı; baslat reddedildi
        } else if (r.calistirma_sonu == "durduruldu" || a3_durdur_) {
            bitis = kayit::BitisDurumu::durduruldu;         // A3 `durdur` (koşarken ya da hazırlıkta)
        } else if (r.tamam) {
            bitis = kayit::BitisDurumu::bitti;
        }
        // Son `sayac/gercek_zaman` (A6-K44): üretici bu iş parçacığıdır, kapanıştan önce. Yayın bir istisna
        // atarsa `kapat` yine çağrılır (aksi hâlde ~Yazici tavansız bir taslakla kapatırdı).
        try {
            if (k_) {
                a3_durum_yayinla(static_cast<std::int64_t>(dn_.durum().adim_no) * kH);
            }
        } catch (...) {
            r.kayit.hata = "kayıt kapanışında son sayaç yayını başarısız";
        }
        const kayit::KapanisSonucu ks = kb_->kapat(kayit_alanlari(bitis));
        KayitOzeti& o = r.kayit;
        o.dosya_kapandi = ks.dosya_kapandi;
        o.bitis_durumu = std::string(kayit::bitis_durumu_adi(ks.bitis_durumu));
        const auto g = ks.kapanis_kaydi.find("tekrar_guvencesi");
        o.guvence = g != ks.kapanis_kaydi.end() ? g->second : std::string();
        const auto gn = ks.kapanis_kaydi.find("guvence_nedenleri");
        o.guvence_nedenleri = gn != ks.kapanis_kaydi.end() ? gn->second : std::string();
        if (!ks.hata.empty()) {
            o.hata = ks.hata;
        }
        const kayit::UreticiSayaclari u = kb_->uretici_sayaclari();
        o.uretilen = u.uretilen;
        o.kuyruklanan = u.kuyruklanan;
        o.dusen_halka = u.dusen_halka;
        o.dusen_gecersiz = u.dusen_gecersiz;
        o.suzulen = u.suzulen;
        o.kapali_atilan = u.kapali_atilan;
        o.yazilan_ileti = ks.sayaclar.yazilan_ileti;
        o.eksik_kaydi = ks.sayaclar.eksik_kaydi;
        o.bildirilen_dusen = ks.sayaclar.bildirilen_dusen;
        o.yazimda_kaybolan = ks.sayaclar.yazimda_kaybolan;
        o.kapali_iken_atilan = ks.sayaclar.kapali_iken_atilan;
        o.kapanan_parca = ks.sayaclar.kapanan_parca;
        o.dosya_bayt = ks.sayaclar.dosya_bayt;
        o.uretilen_kanallar = kb_->uretilen_kanallar();
        o.eksik_kanallar = kb_->eksik_kanallar();
    } catch (...) {
        r.kayit.hata = "kayıt kapanışında istisna";
    }
}

Sonuc Calistirici::kos_ic()
{
    // ---- Senaryo: A5 dosyası ya da yerleşik ----
    s_.kip = a_.kip;
    if (a_.sonar_acik && a_.senaryo_dosyasi.empty()) {
        // W4-bagla-sonar: sahne senaryonun ortamındandır; yerleşik senaryoda tanım yok (uydurulmaz).
        s_.hata = "--sonar yalnız senaryo dosyasıyla kullanılabilir (sahne senaryonun ortam tanımından)";
        s_.hata_turu = HataTuru::kullanim;
        return s_;
    }
    if (!a_.sonar_acik && (a_.ping360_ornek != 0 || a_.ping360_bekleme_ms != 0.0 || !a_.sonar_csv_yolu.empty())) {
        s_.hata = "--ping360-ornek, --ping360-bekleme-ms ve --sonar-csv --sonar ister";
        s_.hata_turu = HataTuru::kullanim;
        return s_;
    }
    if (a_.sonar_acik && (a_.ping360_ornek != 0 && (a_.ping360_ornek < 200 || a_.ping360_ornek > 1200))) {
        s_.hata = "--ping360-ornek 200–1200 olmalı (A1 §3.4)";
        s_.hata_turu = HataTuru::kullanim;
        return s_;
    }
    if (!(a_.ping360_bekleme_ms >= 0.0 && a_.ping360_bekleme_ms <= 10000.0)) {
        s_.hata = "--ping360-bekleme-ms 0–10000 olmalı";
        s_.hata_turu = HataTuru::kullanim;
        return s_;
    }
    s_.kayit.istendi = a_.kayit_acik;
    if (a_.kayit_acik && a_.senaryo_dosyasi.empty()) {
        // W5-bagla-kayit: A6 ekleri (çözümlenmiş senaryo, A7 manifesti) senaryo dosyasındandır; yerleşik
        // senaryoda yoktur ve uydurulmaz (`--sonar` ve `--a2-port` ile aynı kural).
        s_.hata = "--kayit-dizini yalnız senaryo dosyasıyla kullanılabilir (çözümlenmiş senaryo ve A7 manifesti kayıt eklerindendir)";
        s_.hata_turu = HataTuru::kullanim;
        return s_;
    }
    if (a_.kayit_acik && a_.kayit_dizini.empty()) {
        s_.hata = "--kayit-dizini boş olamaz";
        s_.hata_turu = HataTuru::kullanim;
        return s_;
    }
    s_.a2.istendi = a_.a2_acik;
    if (a_.a2_acik && a_.senaryo_dosyasi.empty()) {
        // W5-bagla-a2: A2 çalıştırma tanımı (A7 varlık kümesi, ortam, görünüm kademesi) senaryodandır;
        // yerleşik senaryoda tanım yok (uydurulmaz; `--sonar` ile aynı kural).
        s_.hata = "--a2-port yalnız senaryo dosyasıyla kullanılabilir (varlık kümesi ve ortam senaryonun tanımından)";
        s_.hata_turu = HataTuru::kullanim;
        return s_;
    }
    if (!(std::isfinite(a_.sil_hiz) && a_.sil_hiz >= 0.0)) {
        s_.hata = "--sil-hiz sonlu ve ≥ 0 olmalı";
        s_.hata_turu = HataTuru::kullanim;
        return s_;
    }
    if (!a_.senaryo_dosyasi.empty()) {
        s_.senaryo_yolu = a_.senaryo_dosyasi;
        if (!dosya_plani()) {
            return s_;
        }
        s_.kip = a_.kip;   // dosyadan seçilen etkin kip (sol #5)
    } else {
        // EMNİYET (sol #1 dar denetimi): verilen acil durdurma anları sonlu ve ≥ 0, kalkış
        // basıştan sonra olmalı; geçersiz değer sessizce "yok" sayılmaz.
        const bool acil = a_.acil_verildi || !(a_.acil_durdurma_s == -1.0);
        const bool kalk = a_.acil_kalk_verildi || !(a_.acil_kalkar_s == -1.0);
        std::string h;
        if (acil && !(std::isfinite(a_.acil_durdurma_s) && a_.acil_durdurma_s >= 0.0)) {
            h = "--acil sonlu ve ≥ 0 bir süre olmalı";
        } else if (kalk && !(std::isfinite(a_.acil_kalkar_s) && a_.acil_kalkar_s >= 0.0)) {
            h = "--acil-kalk sonlu ve ≥ 0 bir süre olmalı";
        } else if (acil && kalk && !(a_.acil_kalkar_s > a_.acil_durdurma_s)) {
            h = "--acil-kalk --acil'den sonra olmalı";
        } else if (kalk && !acil) {
            h = "--acil-kalk yalnız --acil ile verilebilir";
        }
        if (!h.empty()) {
            s_.hata = h;
            s_.hata_turu = HataTuru::kullanim;
            return s_;
        }
        plan_ = yerlesik_plan(a_);
    }
    if (a_.sil_hiz > 0.0 && a_.kip != Kip::sil) {
        s_.hata = "--sil-hiz yalnız SIL kipinde kullanılabilir (HIL duvar saatine kilitlidir)";
        s_.hata_turu = HataTuru::kullanim;
        return s_;
    }
    hiz_ = a_.sil_hiz;   // SIL hız çarpanının başlangıcı (0: olabildiğince hızlı; A3-K66 ile aynı düzenek)

    // ---- Fizik: dosyadan (fizik_kur) ya da yerleşik İDA-0, örnek havuz, düz suda yüzer denge ----
    fizik::OrtamParametreleri ortam;
    fizik::BaslangicDurumu bas;
    fizik::KurulumSonucu kr;
    if (plan_.fizik) {
        arac_ = plan_.fizik->arac;
        ortam = plan_.fizik->ortam;
        bas = plan_.fizik->baslangic;
        kr = dn_.kur(arac_, ortam, bas);
    } else {
        arac_ = fizik::ida0_arac();
        ortam = fizik::havuz_ortami();
        kr = dn_.kur(arac_, ortam, bas);
        if (kr.tamam) {
            bas.konum[2] = dn_.turetilmis().denge_konumu_z;
            kr = dn_.kur(arac_, ortam, bas);
        }
    }
    if (!kr.tamam) {
        s_.hata = "fizik kurulumu: " + kr.hata;
        return s_;
    }
    s_.denge_derinligi_m = bas.konum[2];
    pwm_yur_.fill(static_cast<double>(arac_.esc.pwm_notr_us));
    son_pwm_yur_ = pwm_yur_;

    // ---- Sensörler: dosyadan (sensor_kur) ya da yerleşik İDA-0 ----
    sensor::SensorKumesiAyari sa;
    if (plan_.sensor) {
        sa = *plan_.sensor;
    } else {
        sa = sensor::ida0_sensor_kumesi();
        sa.ana_tohum = a_.tohum;
        sa.guc = sensor::guc_parametreleri(arac_);
    }
    const sensor::KurulumSonucu ks = kume_.kur(sa);
    if (!ks.tamam) {
        s_.hata = "sensör kümesi: " + ks.hata;
        return s_;
    }
    if (sonar_kurulum_ && !sonar_kur_(sa)) {
        return s_;
    }
    if (!plan_hazirla(sa)) {
        return s_;
    }

    // ---- Parametre bloğu (A1-K18): dosyada otopilot.parametreler (A5-K53); yerleşikte havuzun
    // yerçekimi ve kalibrasyondaki O_b derinliği ----
    pb_ = plan_.dosyadan ? kopru::parametre_blogu(plan_.parametreler)
                         : kopru::parametre_blogu({{"YER_G", ortam.yercekimi}, {"BARO_KAL_D", s_.denge_derinligi_m}});
    if (!pb_.hata.empty()) {
        s_.hata = pb_.hata;
        return s_;
    }

    // ---- A2 sunucusu (W5-bagla-a2; W5-K31): yalnız --a2-port verildiyse; süreç ömrünce tutulur ----
    if (a_.a2_acik && !a2_baslat()) {
        return s_;
    }

    // ---- A3 sunucusu (W4-bagla-a3): yalnız --a3-port verildiyse ----
    if (a_.a3_acik) {
        if (!a3_baslat()) {
            return s_;
        }
        if (a_.a3_bekle) {
            const BaslatBekleme bb = a3_baslat_bekle();
            if (bb == BaslatBekleme::sinyal) {
                s_.calistirma_sonu = "yuklendi";   // W4-sunucu-kucuk: sinyal, `baslat` beklenirken (oturum yok)
                a3_bitir();
                return s_;
            }
            if (bb == BaslatBekleme::yeniden) {
                // W5-a3-senaryo (A3-K77): `senaryo/yukle` alındı; bu dünya hiç koşmadı, sürücü yenisini kurar.
                s_.calistirma_sonu = "yuklendi";
                s_.tamam = true;
                a3_bitir();
                return s_;
            }
        }
        a3_run_id_ = a3b::run_id_uret();   // A3-K63: çalıştırmanın başladığı an
    }

    // ---- Köprü ----
    kopru::KopruAyari ka;
    ka.kip = a_.kip == Kip::sil ? kopru::CalistirmaKipi::sil : kopru::CalistirmaKipi::hil_1x;
    ka.ana_tohum = a_.tohum;
    ka.esc_alt_us = static_cast<std::uint16_t>(arac_.esc.pwm_min_us);
    ka.esc_notr_us = static_cast<std::uint16_t>(arac_.esc.pwm_notr_us);
    ka.esc_ust_us = static_cast<std::uint16_t>(arac_.esc.pwm_max_us);
    ka.beklenen_blok_crc = pb_.crc;
    // W5-K55 (A3-K117): köprü, süreç boyunca yaşayan revizyon ve doyma bayrağıyla kurulur (süreç başında 1).
    ka.ilk_emniyet_rev = ilk_emniyet_rev();
    ka.ilk_emniyet_rev_doydu = ilk_emniyet_rev_doydu();
    if (a_.kip == Kip::hil) {
        // A1-K126: kartlı kiplerde run_id metin kimliğinden; metin kimliği (A3/A6) bu dilimde
        // yok: başlangıç duvar saatinden türetilmiş sıfırdan farklı değer.
        const auto z = static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
        ka.run_id = static_cast<std::uint32_t>(z ^ (z >> 32U)) | 1u;
        if (!a3_run_id_.empty()) {
            ka.run_id = a3b::a1_run_id(a3_run_id_);   // A3-K63, A1-K126: metin kimliği varken
        }
    }
    k_ = std::make_unique<kopru::Kopru>(ka);
    const std::string yh = k_->yapilandirma_hatasi();
    if (!yh.empty()) {
        s_.hata = "yapılandırma (A1-K104): " + yh;
        return s_;
    }
    // Başlangıç emniyet durumu (A5-K86, K87; yerleşikte varsayılan).
    if (!k_->emniyet_baslangic(plan_.acil_durdurma, plan_.fis, plan_.pim, plan_.dolu)) {
        s_.hata = "başlangıç emniyet durumu kurulamadı";
        return s_;
    }
    // ---- A6 kayıt dosyası (W5-bagla-kayit; W5-K40): yalnız --kayit-dizini verildiyse. Dünya ve köprü
    // kuruldu; A1 oturumu ve ilk fizik adımı henüz yok. Aynı adlı dosya varsa çalıştırma başlamaz (A6-K01).
    if (a_.kayit_acik && !kayit_baslat()) {
        return s_;
    }
    if (yay_ != nullptr) {
        a3_on_denetim_kaydet();   // GET /on_denetim (W5-a3-senaryo); kayıt açıksa dosya ekiyle aynı belge
        // A3-K62, K63: çalıştırma başladı (baslat ya da kendiliğinden); ön denetim A1 el sıkışmasıdır.
        a3d_.run_var = true;
        metin_kopyala(a3p_.run_id, a3_run_id_);
        a3_emn_once_ = k_->dis_durum().emniyet;
        a3_gecis(a3::CalistirmaDurumu::hazirlaniyor, a_.a3_bekle ? nullptr : "kendiliginden_baslatma", !a_.a3_bekle);
        a3_anlik_guncelle();
    }

    // ---- Taşıma ve karşı taraf ----
    std::string hata;
    switch (a_.karsi) {
    case Karsi::ic: {
        auto [x, y] = kopru::bellek_cifti();
        tas_ = std::move(x);
        kopru::KarsiAyari kk;
        kk.a1_tohum = k_->board_tohumu();
        kk.cagri_kancasi = a_.ic_cagri_kancasi;
        kk.cikti_kancasi = a_.ic_cikti_kancasi;
        ic_ = std::make_unique<kopru::KarsiTaraf>(std::move(y), kk);
        if (!ic_->kur()) {
            s_.hata = "süreç içi karşı taraf: " + ic_->hata();
            return s_;
        }
        break;
    }
    case Karsi::tcp: {
        din_ = kopru::tcp_dinle(a_.a1_port, hata);
        if (!din_) {
            s_.hata = "TCP: " + hata;
            return s_;
        }
        char tohum[32];
        std::snprintf(tohum, sizeof tohum, "%016llx", static_cast<unsigned long long>(k_->board_tohumu()));
        gunluk(1, "A1 TCP 127.0.0.1:" + std::to_string(din_->port()) + " (--a1-seed " + tohum + ")");
        if (a_.tcp_hazir) {
            a_.tcp_hazir(din_->port(), k_->board_tohumu());
        }
        if (!a_.sil_yolu.empty()) {
            std::vector<std::string> av = {a_.sil_yolu, "--a1-port", std::to_string(din_->port()), "--a1-seed", tohum};
            if (!a_.sil_depo.empty()) {
                av.push_back("--depo");
                av.push_back(a_.sil_depo);
            }
            av.push_back("--gunluk");
            av.push_back(a_.gunluk >= 2 ? "2" : "1");
            surec_ = surec_baslat(av, hata);
            if (!surec_) {
                s_.hata = "koşucu başlatılamadı: " + hata;
                return s_;
            }
        }
        break;
    }
    case Karsi::pty: {
        kopru::SozdeUcbirim su = kopru::sozde_ucbirim_ac(hata);
        if (!su.ana) {
            s_.hata = hata;
            return s_;
        }
        kopru::SeriAyar sea;
        sea.hiz_zorunlu = false;
        tas_ = kopru::seri_ac(su.yol, sea, hata);
        if (!tas_) {
            s_.hata = "seri_ac(" + su.yol + "): " + hata;
            return s_;
        }
        kopru::KarsiAyari kk;
        kk.rol = kopru::KarsiRol::kart;
        kk.param_blok = pb_.blok;   // kartın kalıcı bloğu A5'tekiyle aynı (hazırlık sınamada ayrı)
        ic_ = std::make_unique<kopru::KarsiTaraf>(std::move(su.ana), kk);
        if (!ic_->kur()) {
            s_.hata = "karşı taraf: " + ic_->hata();
            return s_;
        }
        karsi_is_ = std::thread([this] { ic_->calistir([this] { return dur_.load(); }); });
        break;
    }
    case Karsi::seri: {
        tas_ = kopru::seri_ac(a_.seri, kopru::SeriAyar{}, hata);
        if (!tas_) {
            s_.hata = "seri_ac(" + a_.seri + "): " + hata;
            return s_;
        }
        tas_->giris_bosalt();   // A1-K12
        break;
    }
    }

    if (tas_ && a_.tasima_sarmalayici) {
        tas_ = a_.tasima_sarmalayici(std::move(tas_));   // yalnız sınama
    }
    if (a_.kip == Kip::hil) {
        zamanlayici_onceligi();   // HIL 1x: duvar saatine kilitli döngü
    }
    bas_ = Saat::now();
    // Sol 4. denetim #1 (EMNİYET): buradan sonraki her çıkışta (istisna dahil) ortak güvenli
    // kapanış; olağan bitiş ve açık erken kapanış koruyucuyu işaretler.
    KapanisKoruyucu koruyucu(*this);
    bool kuruldu = oturum_kur();
    if (kuruldu && yay_ != nullptr) {
        // EMNİYET (W4-K28 (3); sol-2 #5): hazırlığın tamamlanması ile kabul edilmiş durdurmanın
        // sahipliği birlikte çözülür. Üretici kilidi altında son çekiş: son el sıkışma pompası
        // sırasında `hazirlaniyor` görüntüsüyle kabul edilen `durdur` burada (aynı revizyonla)
        // işlenir ve kazanır; kilit bırakılmadan `kosuyor` görüntüsü yazılır, sonraki her istek
        // yeni görüntüyle denetlenir. İç geçiş kabul edilmiş durdurmayı geçersizleştiremez.
        std::unique_lock<std::mutex> kilit = a3al_->gecis_kilidi();
        if (a3_hazirlik_durdu(false)) {
            kuruldu = false;
        } else {
            a3_gecis(a3::CalistirmaDurumu::kosuyor, "on_denetim_gecti", true);
            a3_anlik_guncelle();
        }
    }
    if (!kuruldu) {
        // EMNİYET (W4-K28 (3); sol-2 #6; w4-mac-ci sol 3. denetim #3, A1-K21): bütün erken
        // bitişlerde (hazırlıkta durdurma, hazırlıkta süre sınırı ya da komut reddi, bağlantı
        // beklemesinin iptali) oturum açıksa SESSION_CLOSE tamamen yazılır (mutlak süre sınırıyla)
        // ve taşıma düzgün kapanır.
        erken_kapanis();
        koruyucu.yapildi();
        s_.sayac = k_->sayaclar();
        s_.kart = k_->kart();
        if (yay_ != nullptr && a3_durdur_) {
            // W4-K27 (4): hazırlanırken durdurma: çalıştırma `yuklendi`'de kalır, başlamaz.
            if (a3_baslat_komutu_) {
                a3_sonuc(*a3_baslat_komutu_, true, 0, nullptr);
                a3_baslat_komutu_.reset();
            }
            s_.calistirma_sonu = "yuklendi";
            s_.hata.clear();
            a3_bitir();
            s_.tamam = true;
            return s_;
        }
        if (yay_ != nullptr) {
            // A3-K62: ön denetim kaldı → yuklendi; `baslat` reddedildi (on_denetim_kaldi).
            a3_gecis(a3::CalistirmaDurumu::yuklendi, "on_denetim_kaldi", true);
            a3_on_denetim_kaldi_ = true;   // kayıt kapanışı: `on_denetim_kaldi` (A6-K45); `durduruldu` değil
            s_.calistirma_sonu = "yuklendi";   // W4-K27 (4): sonradan kendiliğinden başlamaz
            if (a3_baslat_komutu_) {
                a3_sonuc(*a3_baslat_komutu_, false, 0, "on_denetim_kaldi");
                a3_baslat_komutu_.reset();
            }
            a3_bitir();
        }
        return s_;
    }
    if (yay_ != nullptr) {
        if (a3_baslat_komutu_) {
            a3_sonuc(*a3_baslat_komutu_, true, 0, nullptr);
            a3_baslat_komutu_.reset();
        }
        a3_oturum_izle();
        a3_anlik_guncelle();
        hiz_t0_duvar_ = Saat::now();
    }
    gunluk(2, "oturum kuruldu: sess " + std::to_string(k_->sess()));
    const kopru::Sayaclar rx_bas = k_->sayaclar();   // çalıştırma oturumunun başındaki çerçeve sayaçları

    const auto n_sinir = static_cast<std::uint64_t>(plan_.sure_ns) / static_cast<std::uint64_t>(kH);
    if (a_.csv_uret) {
        s_.csv = csv_basligi();
        s_.csv.reserve(static_cast<std::size_t>(n_sinir / 4U + 2U) * 320U);   // döngüde yeniden ayırma yok
    }
    s_.satirlar.reserve(static_cast<std::size_t>(n_sinir / 4U + 1U));
    w0_ = gercek() + 50 * kMs;   // HIL: W(t) = W0 + t
    tf_yazim_.reserve(static_cast<std::size_t>(n_sinir / 4U + 1U));
    if (a_.kip == Kip::hil) {
        profil_.reserve(static_cast<std::size_t>(n_sinir + 1U));
        tf_gec_.reserve(static_cast<std::size_t>(n_sinir / 4U + 1U));
    }

    if (hiz_ > 0.0) {
        hiz_t0_duvar_ = Saat::now();   // --sil-hiz: başvuru anı (A3 kapalıyken de)
    }
    if (a2_) {
        // W5-bagla-a2 (A2-K80 (1), K96): yeni çalıştırma; dünya kuruldu, A1 oturumu hazır, ilk fizik
        // adımından önce. Tanım sınır 0'ın durumunu taşır; ilk `adim_siniri` sınır 1'dedir. Dünya
        // A2 bağlantısını beklemez (A2-K47): çağrı engellemez.
        (void)a2_->calistirma_basladi(a_.kip, ortam, dn_.durum(), plan_.dolu);
    }
    // W4-bagla-sonar: sınır 0'ın sonar adımı (sonrakiler bir önceki fizik adımından hemen sonra).
    const bool sonar_bas = !sonar_ || sonar_onceden(0);
    for (std::uint64_t b = 0; sonar_bas && b <= n_sinir; ++b) {
        // W4-duraklat (W4-K28 (2); EMNİYET): sınır b'nin turları. İlk tur ilerleyen kipte; tur
        // sonunda çalıştırma duraklatılmışsa sınır b AYNI işlevle (sinir_turu) duran kipte yeniden
        // işlenir; fizik adımı b yalnız duraklatma kalkınca, son turun girdisiyle koşar.
        bool duran = false;
        bool devam = true;
        for (;;) {
            if (!sinir_turu(b, duran)) {
                devam = false;
                break;
            }
            if (!duraklatma_gecisi(b, duran)) {
                break;
            }
            duran = true;
        }
        if (!devam) {
            break;
        }
        const std::int64_t e_f = gercek();
        if (dn_.adim() != fizik::AdimDurumu::tamam) {
            s_.hata = "fizik sayısal hata";
            break;
        }
        adim_sonrasi(b);
        if (a_.kip == Kip::hil) {
            evre_[kEvreFizik] += gercek() - e_f;
            profil_.push_back(evre_);
        }
        if (sonar_ && b < n_sinir && !sonar_onceden(b + 1u)) {
            break;   // W4-bagla-sonar: sınır b + 1'in sonar adımı, o sınırın bekleyişinden önce
        }
    }
    if (yay_ != nullptr) {
        // EMNİYET (W4-K28 sol-3 #1): koşunun son sınırından sonra kabul edilmiş bir `durdur`,
        // `bitti` iç geçişiyle bayatlamaz: üretici kilidi altında son çekiş (kısıtlayan yuvalar
        // a3_bitir'de), durdurma kazanır; kilit bırakılmadan son görüntü yazılır.
        // W5-K56: ertelenmiş sıfırlama aynı turda sinir_on'da sonuçlanır; tur oraya varmadan kesildiyse
        // (olmamalı) kabul edilmiş istek burada yürürlüğe girer, sonuçsuz kalmaz.
        (void)a3_ertelenen_yeniden_bitir(a3_b_);
        std::unique_lock<std::mutex> kilit = a3al_->gecis_kilidi();
        if (!a3_durdur_ && s_.hata.empty()) {
            a3_cek(A3Alici::kButunYuvalar);
            a3_calistirma_isle(a3_b_);
        }
        // Tablo 4.7a iç geçişleri: süre doldu → bitti; çalıştırmayı durduran hata → hata
        // (durdur komutu durumu zaten `durduruldu` yaptı).
        if (!a3_durdur_) {
            a3_gecis(s_.hata.empty() ? a3::CalistirmaDurumu::bitti : a3::CalistirmaDurumu::hata,
                     s_.hata.empty() ? "sure_doldu" : "calistirma_hatasi", true);
        }
        a3_anlik_guncelle();
        kilit.unlock();
        s_.calistirma_sonu = a3d_.durum == a3::CalistirmaDurumu::bitti        ? "bitti"
                             : a3d_.durum == a3::CalistirmaDurumu::durduruldu ? "durduruldu"
                             : a3d_.durum == a3::CalistirmaDurumu::yuklendi   ? "yuklendi"   // `sifirla` (W5-a3-senaryo)
                                                                               : "hata";
    }
    // Hiç başlamayan olaylar (A5 §3.15 olay sonucu: uygulanmadi; t = çalıştırma sonu).
    for (std::size_t i = 0; i < plan_.olaylar.size(); ++i) {
        if (!pd_[i].basladi) {
            olay_sonucu(i, "uygulanmadi", plan_.sure_ns, plan_.olaylar[i].kosul ? "kosul_saglanmadi" : "");
        }
    }
    // Son satırın kart tarafı ve planlı kapanış (A1-K21).
    if (a_.kip == Kip::sil) {
        pompa(0);
    } else {
        bekle_ve_oku(gercek() + 20 * kMs);
    }
    satirlari_tamamla(0);
    // Oturum içi çerçeve sayaçları SESSION_CLOSE gönderiminden önce dondurulur (kart oturum
    // sonunda yazımı başlamamış çerçeveleri siler; A1-K22 (1)).
    const kopru::Sayaclar rx_son = k_->sayaclar();
    k_->oturum_kapat(a1_kapanis_nedeni(), duvar());   // W5-K59: sıfırlama/yüklemede RUN_END (A1-K128)
    kapanis_daraltmasi();   // sol 4. denetim #3: izinler kapalı, fizik girdisi nötr (son satır değişmez)
    (void)cikisi_bitir(gercek() + kKapanisYazimNs);   // W4-K28 (3): SESSION_CLOSE tamamen yazılır
    pompa(0);
    if (tas_) {
        // W4-K25 (5): düzgün kapanış; SIL'de alım kuyruğu artık işlenmez, atılan baytlar da.
        tas_->kapat_duzgun(kKapanisBosaltmaUs);
    }
    koruyucu.yapildi();
    dur_ = true;
    if (karsi_is_.joinable()) {
        karsi_is_.join();
    }
    if (surec_) {
        surec_->sonlandir();
    }
    if (yay_ != nullptr) {
        a3_bitir();
    }

    // ---- Sonuç ----
    if (!profil_.empty()) {
        evre_ozeti_kur();
    }
    s_.sayac = k_->sayaclar();
    s_.kart = k_->kart();
    auto ack = [&](std::uint32_t id, int& a, int* neden = nullptr) {
        const auto r = id != 0u ? k_->komut_sonucu(id) : std::nullopt;
        if (r) {
            a = r->ack;
            if (neden != nullptr) {
                *neden = r->reason;
            }
        }
    };
    ack(id_arm_, s_.arm_ack, &s_.arm_neden);
    ack(id_kip_, s_.kip_ack);
    ack(id_sp_, s_.sp_ack);
    ack(id_sp2_, s_.sp2_ack);
    ack(id_disarm_, s_.disarm_ack);
    if (ic_) {
        s_.karsi_sample_rejected = ic_->istatistik().sample_rejected;
        s_.karsi_sample_future = ic_->istatistik().sample_future;
        s_.karsi_imu_rejected = ic_->istatistik().imu_rejected;
        s_.karsi_gecerli_red = ic_->istatistik().gecerli_red;
    }
    ZamanOlcumu& z = s_.zaman;
    z.tf_sayisi = tf_yazim_.size();
    for (std::size_t i = 1; i < tf_yazim_.size(); ++i) {
        z.periyotlar_ns.push_back(tf_yazim_[i] - tf_yazim_[i - 1]);
    }
    if (!z.periyotlar_ns.empty()) {
        z.periyot_en_az_ns = *std::min_element(z.periyotlar_ns.begin(), z.periyotlar_ns.end());
        z.periyot_en_cok_ns = *std::max_element(z.periyotlar_ns.begin(), z.periyotlar_ns.end());
        double top = 0.0;
        for (std::int64_t p : z.periyotlar_ns) {
            top += static_cast<double>(p);
        }
        z.periyot_ort_ns = top / static_cast<double>(z.periyotlar_ns.size());
    }
    z.gercek_zaman_kacirma = z.sim_gecikmesi + s_.sayac.parti_kacirildi + s_.sayac.son_tarih_kacirildi;
    {
        std::vector<std::int64_t> ys = k_->yanit_sureleri();
        std::sort(ys.begin(), ys.end());
        z.yanit_sayisi = ys.size();
        if (!ys.empty()) {
            z.yanit_p50_ns = ys[ys.size() / 2];
            z.yanit_p99_ns = ys[std::min(ys.size() - 1, (ys.size() * 99) / 100)];
            z.yanit_en_cok_ns = ys.back();
        }
        z.rx_crc_kosu = rx_son.rx_crc_err - rx_bas.rx_crc_err;
        z.rx_resync_kosu = rx_son.rx_resync_bytes - rx_bas.rx_resync_bytes;
        z.rx_seq_gap_kosu = rx_son.rx_seq_gap - rx_bas.rx_seq_gap;
        z.rx_sira_disi_kosu = rx_son.rx_sira_disi - rx_bas.rx_sira_disi;
        z.rx_kayip_kosu = rx_son.rx_kayip - rx_bas.rx_kayip;
        const std::vector<std::int64_t>& bt = k_->sira_boslugu_zamanlari();
        for (std::size_t j = static_cast<std::size_t>(rx_bas.rx_seq_gap);
             j < static_cast<std::size_t>(rx_son.rx_seq_gap) && j < bt.size() && z.sira_boslugu_t_ns.size() < 3u; ++j) {
            z.sira_boslugu_t_ns.push_back(bt[j]);
        }
    }
    if (!a_.csv_yolu.empty() && a_.csv_uret) {
        std::ofstream f(a_.csv_yolu, std::ios::binary);   // fopen değil: MSVC C4996
        f.write(s_.csv.data(), static_cast<std::streamsize>(s_.csv.size()));
        if (!f) {
            s_.hata = "CSV yazılamadı: " + a_.csv_yolu;
            return s_;
        }
    }
    if (sonar_) {
        const sonar::SonarSayaclari sc = sonar_->sayaclar();
        s_.sonar_ping = sc.ping;
        s_.sonar_isin = sc.isin;
        s_.sonar_havada = sc.havada;
        s_.sonar_kayip = sc.kayip;
        s_.sonar_kuyruk_tasma = sc.kuyruk_tasma;
        s_.sonar_tampon_yetersiz = sc.tampon_yetersiz;
        if (!a_.sonar_csv_yolu.empty()) {
            std::ofstream f(a_.sonar_csv_yolu, std::ios::binary);
            f.write(s_.sonar_csv.data(), static_cast<std::streamsize>(s_.sonar_csv.size()));
            if (!f) {
                s_.hata = "sonar CSV'si yazılamadı: " + a_.sonar_csv_yolu;
                return s_;
            }
        }
    }
    s_.tamam = s_.hata.empty();
    return s_;
}

}  // namespace

const char* csv_basligi()
{
    return "t_s,parti,derinlik_m,derinlik_k_m,yon_rad,yon_k_rad,yalpa_rad,yalpa_k_rad,bas_kic_rad,bas_kic_k_rad,"
           "pwm_uret0,pwm_uret1,pwm_uret2,pwm_uret3,pwm_uret4,pwm_uret5,pwm_uret6,pwm_uret7,"
           "pwm_yur0,pwm_yur1,pwm_yur2,pwm_yur3,pwm_yur4,pwm_yur5,pwm_yur6,pwm_yur7,"
           "pwm_fiz0,pwm_fiz1,pwm_fiz2,pwm_fiz3,pwm_fiz4,pwm_fiz5,pwm_fiz6,pwm_fiz7,"
           "kip,armed,fs_level,fs_reasons,est_level,thr_valid,is_safe,yanit_var,core_rc,estop,sess,"
           "parti_kacirildi,calistirma_gecersiz,yanit_sure_us,fc_state,fc_inhibit,sim_izin\n";
}

int a3_ayirma_sayilir() noexcept { return g_a3_sayim; }
int sonar_ayirma_sayilir() noexcept { return g_sonar_sayim; }

namespace {

// `senaryo/yukle` ile gelen senaryo için yeni dünyanın ayarı: komut satırından gelen ve yeni senaryoyla
// çelişebilecek seçenekler uygulanmaz (kip, süre ve tohum senaryoya aittir; yerleşik senaryoya özgü acil
// durdurma ve arıza seçenekleri dosyayla birlikte kullanılamaz).
Ayar ayar_yukle(const Ayar& taban, const SenaryoGoruntusu& g)
{
    Ayar a = taban;
    a.senaryo_dosyasi = g.yol;
    a.senaryo_kaynagi = g.kaynak;
    a.kip_verildi = false;
    a.tohum_verildi = false;
    a.sure_verildi = false;
    a.acil_verildi = false;
    a.acil_kalk_verildi = false;
    a.ariza_verildi = false;
    a.acil_durdurma_s = -1.0;
    a.acil_kalkar_s = -1.0;
    return a;
}

// W5-a3-senaryo: A3 açıkken sürücü döngüsü. Her dünya (`Calistirici`) sıfırdan kurulur ve bittiğinde yıkılır;
// A3 sunucusu, A2 sunucusu ve senaryo hizmeti (`A3Kalici`) süreç boyunca yaşar. Yeni dünya bir önceki dünyadan
// hiçbir durum devralmaz: komut satırından gelen özgün `Ayar`dan (ya da yüklenen senaryonun görüntüsünden)
// kurulur. `sifirla`/`senaryo/yukle` yoksa ve `--a3-surekli` verilmediyse döngü bir tur döner ve koşu bugünkü
// davranışla aynıdır (W5-K48).
Sonuc kos_a3(const Ayar& ayar)
{
    A3Kalici kalici;
    kalici.surekli = ayar.a3_surekli;
    kalici.emniyet_rev = ayar.sinama_ilk_emniyet_rev;   // üretimde 1 (A3-K117); yalnız sınama yükseltir
    kalici.emniyet_rev_doydu = ayar.sinama_ilk_emniyet_rev == 0xFFFFFFFFu;
    SenaryoHizmeti::Ayar ha;
    ha.depo_koku = ayar.depo_koku;
    ha.dizin = ayar.senaryo_dizini;
    ha.sil_hiz = ayar.sil_hiz;
    kalici.hizmet = std::make_unique<SenaryoHizmeti>(ha);

    Ayar bu = ayar;       // kurulacak dünyanın ayarı
    Ayar son_iyi = ayar;  // `yuklendi`ye ulaşan son dünyanın ayarı (yeni senaryo kurulamazsa geri kurulur)
    Sonuc son;
    for (std::uint64_t n = 0;; ++n) {
        kalici.dunya_no = n + 1u;
        if (bu.dunya_kancasi) {
            bu.dunya_kancasi(n);
        }
        bool kuruldu = false;
        {
            Calistirici c(bu, &kalici);
            son = c.kos();
            kuruldu = c.a3_kuruldu();
        }   // dünya burada yıkılır; A3 hizmeti kalır
        if (kuruldu) {
            son_iyi = bu;
        }
        if (kapanis_istendi()) {
            break;
        }
        if (!kuruldu && kalici.tamamlanacak) {
            // Bu dünya bir isteğin sonucu olarak kurulacaktı ama kurulamadı (A5'in geçerli saydığı ama bu dünyada
            // yürütülemeyen bir senaryo, `--kip` ile çelişen karşı taraf vb.). `senaryo/yukle` ise önceki senaryo
            // geri kurulur ve istek reddedilir; durum bozulmaz (A3-K77). Sıfırlama ya da geri kurma da kurulamazsa
            // süreç bugünkü gibi hata ile biter.
            A3Kalici::Istek ist = std::move(*kalici.tamamlanacak);
            kalici.tamamlanacak.reset();
            if (ist.tur != A3Kalici::Istek::Tur::yukle || ist.basarisiz) {
                break;
            }
            ist.basarisiz = true;
            ist.hata_kodu = son.hata_turu == HataTuru::senaryo_gecersiz ? "senaryo_gecersiz" : "gecersiz_deger";
            ist.ileti = son.hata;
            kalici.tamamlanacak = std::move(ist);
            bu = son_iyi;
            bu.a3_bekle = true;
            continue;
        }
        if (!kalici.istek) {
            break;
        }
        A3Kalici::Istek ist = std::move(*kalici.istek);
        kalici.istek.reset();
        Ayar sonraki = bu;
        if (ist.tur == A3Kalici::Istek::Tur::yukle) {
            const auto g = kalici.hizmet->aday_al(ist.aday);
            if (g) {
                sonraki = ayar_yukle(ayar, *g);
            } else {
                ist.basarisiz = true;   // aday kayboldu (olmamalı): aynı senaryoyla yeniden kur ve isteği reddet
                ist.hata_kodu = "sunucu_hatasi";
                ist.ileti = "senaryo adayı bulunamadı";
            }
        }
        sonraki.a3_bekle = true;   // sıfırlanan ya da yüklenen dünya `baslat` bekler (A3-K62)
        kalici.tamamlanacak = std::move(ist);
        bu = std::move(sonraki);
    }
    if (kalici.hizmet) {
        kalici.hizmet->kapaniyor();
    }
    if (kalici.sunucu) {
        kalici.sunucu->durdur();
    }
    if (kalici.a2) {
        A2Ozeti o;
        kalici.a2->kapat(o, true);
    }
    return son;
}

}  // namespace

Sonuc kos(const Ayar& ayar)
{
    if (!ayar.a3_acik) {
        Calistirici c(ayar);   // A3 yok: bugünkü yol, bayt bayt aynı (W5-K48)
        return c.kos();
    }
    return kos_a3(ayar);
}

}  // namespace ida::sunucu
