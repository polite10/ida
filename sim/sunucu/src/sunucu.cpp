// Başsız sim sunucusu: fizik + sensör kümesi + A1 köprüsü + zaman yöneticisi (sunucu.hpp).
#include "ida/sunucu/sunucu.hpp"

#include "ida/fizik/dunya.hpp"
#include "ida/kopru/karsi.hpp"
#include "ida/kopru/param_blogu.hpp"
#include "ida/kopru/tasima.hpp"
#include "ida/senaryo/senaryo.hpp"
#include "ida/sensor/sensor_kumesi.hpp"
#include "senaryo_plani.hpp"
#include "yardimci.hpp"

#include <algorithm>
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
#include <thread>

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

class Calistirici {
public:
    explicit Calistirici(const Ayar& a) : a_(a) {}
    Calistirici(const Calistirici&) = delete;
    Calistirici& operator=(const Calistirici&) = delete;
    ~Calistirici()
    {
        dur_ = true;
        if (karsi_is_.joinable()) {
            karsi_is_.join();
        }
    }

    Sonuc kos();

private:
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
            if (sil && bu_tur >= kSilOkumaTuru) {
                break;   // W4-K26 (3): bir turun işi sınırlı; gözetim denetimine dönülür
            }
            std::size_t n = 0;
            const kopru::OkumaSonucu r = tas_->oku(alim_.data(), alim_.size(), bekle_us, n);
            bekle_us = 0;
            if (r == kopru::OkumaSonucu::tamam) {
                if (sil) {
                    bu_tur += n;
                    if (sil_gelen_.size() - sil_gelen_k_ + n > kSilGelenAzami) {
                        // W4-K26 (3) (EMNİYET): bellek tavanı eklemeden önce; oturum protokol
                        // hatasıyla biter, kuyruk (kilitli adımı bozan akış) atılır.
                        ++s_.sil_kuyruk_tasma;
                        k_->protokol_hatasi(duvar());
                        sil_gelen_.clear();
                        sil_gelen_k_ = 0;
                        break;
                    }
                    sil_gelen_.insert(sil_gelen_.end(), alim_.data(), alim_.data() + n);
                    s_.sil_kuyruk_en_cok = std::max<std::uint64_t>(s_.sil_kuyruk_en_cok, sil_gelen_.size() - sil_gelen_k_);
                } else {
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
    // kalan baytlar kuyrukta kalır. Kuyruk bitti ve akış kapandıysa (bekleyen hâlâ veri
    // istiyorsa) bağlantı kaybı burada işlenir. Dönüş: dur().
    bool sil_besle(const std::function<bool()>& dur)
    {
        while (sil_gelen_k_ < sil_gelen_.size() && !dur()) {
            const std::size_t n = k_->al(sil_gelen_.data() + sil_gelen_k_, sil_gelen_.size() - sil_gelen_k_, duvar(), dur);
            sil_gelen_k_ += n;
            sil_beslenen_ += n;
        }
        if (sil_gelen_k_ >= sil_gelen_.size()) {
            sil_gelen_.clear();
            sil_gelen_k_ = 0;
        } else if (sil_gelen_k_ > (std::size_t{1} << 16U)) {
            sil_gelen_.erase(sil_gelen_.begin(), sil_gelen_.begin() + static_cast<std::ptrdiff_t>(sil_gelen_k_));
            sil_gelen_k_ = 0;
        }
        const bool tamam = dur();
        if (!tamam && sil_akis_bitti_ && sil_gelen_.empty()) {
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

    bool baglanti_kabul();
    bool oturum_kur();
    bool hazirlik();
    bool yuva_bekle(std::uint32_t seq);
    bool dosya_plani();
    bool plan_hazirla(const sensor::SensorKumesiAyari& sa);
    void plan_sinir(std::uint64_t b);
    void plan_sinir_sonrasi(std::uint64_t b, const kopru::SinirSonucu& ss);
    void gorev_hizmet(std::int64_t t);
    void olay_sonucu(std::size_t i, const char* sonuc, std::int64_t t, const char* neden = "");
    bool sinir_on(std::uint64_t b);
    void adim_sonrasi(std::uint64_t b);
    void satir_ekle(std::uint64_t b, std::optional<std::uint32_t> seq);
    void satir_tamamla(Satir& r);
    void csv_yaz(const Satir& r);
    void evre_ozeti_kur();

    Ayar a_;
    Sonuc s_;
    Plan plan_;
    struct PlanDurumu {
        bool basladi = false;
        std::int64_t tetik_b = -1;   // koşul sağlandı: bu sınırda başlar (A5-K63)
    };
    std::vector<PlanDurumu> pd_;
    struct IticiAriza {
        std::uint8_t maske = 0;
        double carpan = 1.0;
        std::int64_t bitis_ns = 0;   // t_b + sure_ns'e eşit ya da sonraki ilk sınırda biter (A5-K64)
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
    // W4-K26 (3): bir yuva beklemesinde, yuva tamamlanmadan işlenebilecek en çok bayt (kartın bir
    // çağrısının meşru çıktısı birkaç KiB; W4-K14'ün 128 çerçevelik sınırının bayt karşılığından çok
    // büyük). Aşılırsa oturum protokol hatasıyla biter (akış sırasına bağlı, belirlenimci).
    static constexpr std::uint64_t kSilBeklemeAzami = std::uint64_t{1} << 22U;   // 4 MiB
    // W4-K25 (5): düzgün kapanışta karşı tarafın kapatması en çok bu kadar beklenir (TCP).
    static constexpr std::int64_t kKapanisBosaltmaUs = 500'000;
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

bool Calistirici::baglanti_kabul()
{
    if (a_.karsi != Karsi::tcp || !din_) {
        return tas_ != nullptr;
    }
    std::string hata;
    tas_ = din_->kabul(a_.baglanti_bekleme_ns / 1000, hata);
    if (!tas_) {
        s_.hata = "SIL koşucusu bağlanmadı: " + hata;
        return false;
    }
    sil_gelen_temizle();   // A1-K99: her bağlantı yeni bir akıştır
    gunluk(2, "bağlantı: " + tas_->ad());
    return true;
}

bool Calistirici::oturum_kur()
{
    const std::int64_t son = gercek() + a_.baglanti_bekleme_ns;
    if (!tas_ && !baglanti_kabul()) {
        return false;
    }
    // SIL (A1-K121 (iv), K23): oturum köprüde bittiyse el sıkışma orada başlamıştır (giden
    // kuyrukta varsa SESSION_CLOSE, ardından SIM_HELLO): yeniden başlatılmaz. Böylece kapanış
    // çerçevesi silinmez ve el sıkışma tek SIM_HELLO'yla tamamlanır.
    if (a_.kip != Kip::sil || k_->durum() != kopru::OturumDurumu::el_sikisma) {
        k_->el_sikisma_baslat(duvar());
    }
    const auto el_bitti = [this] {
        const kopru::OturumDurumu d = k_->durum();
        return d == kopru::OturumDurumu::kosuyor || d == kopru::OturumDurumu::reddedildi;
    };
    for (;;) {
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
                if (!hazirlik()) {
                    return false;
                }
                k_->el_sikisma_baslat(duvar());
                continue;
            }
            return true;
        }
        if (!tas_) {
            ++s_.yeniden_baglanti;
            if (!baglanti_kabul()) {
                return false;
            }
            k_->el_sikisma_baslat(duvar());
        }
        if (gercek() > son) {
            s_.hata = "el sıkışma zaman aşımı";
            return false;
        }
    }
}

// A1-K19: hazırlık oturumu: fizik yürütülmez, yalnız TF; her parametre için PARAM_SET, sonra
// PARAM_COMMIT ve PARAM_SAVE (kaynak 250); hepsi kabul edilir, kart bloğu kaydeder ve
// param_blob_crc beklenene eşit olunca RUN_END. Ardından REBOOT'lu yeni el sıkışma gelir.
bool Calistirici::hazirlik()
{
    ++s_.hazirlik_oturumu;
    gunluk(1, "parametre bloğu uyuşmuyor: hazırlık oturumu (A1-K19)");
    std::int64_t t_h = 0;
    const std::int64_t eski_duvar = sil_duvar_;
    sil_duvar_ = 0;
    std::vector<std::uint32_t> idler;
    for (const kopru::ParamDegeri& p : pb_.tumu) {
        idler.push_back(k_->komut_param_set(p.id, p.type, p.bits, t_h, 250));
    }
    idler.push_back(k_->komut_basit(kCmdParamCommit, 0, t_h, 250));
    idler.push_back(k_->komut_basit(kCmdParamSave, 0, t_h, 250));
    bool tamam = false;
    for (int n = 0; n < 8000 && !tamam; ++n) {
        t_h += kP;
        if (a_.kip == Kip::sil) {
            sil_duvar_ = t_h;
        } else {
            bekle_ve_oku(gercek() + kP);
        }
        const std::optional<std::uint32_t> seq = k_->parti_kapat(t_h, duvar());
        yaz();
        if (!seq) {
            s_.hata = "hazırlık oturumu koptu";
            return false;
        }
        if (a_.kip == Kip::sil) {
            if (!yuva_bekle(*seq)) {
                return false;
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
                return false;
            }
        }
        const kopru::KartDurum& d = k_->kart().durum;
        tamam = hepsi && d.var && d.param_store == kPstoreOk && d.param_blob_crc == pb_.crc;
    }
    if (!tamam) {
        s_.hata = "hazırlık oturumu tamamlanmadı (param_blob_crc eşleşmedi)";
        return false;
    }
    k_->oturum_kapat(kCloseRunEnd, duvar());
    yaz();
    pompa(0);
    k_->kip_ayarla(a_.kip == Kip::sil ? kopru::CalistirmaKipi::sil : kopru::CalistirmaKipi::hil_1x);
    sil_duvar_ = eski_duvar;
    return true;
}

// SIL kilitli adım (A1-K121 (ii), K76): yuva tamamlanana kadar bekler.
bool Calistirici::yuva_bekle(std::uint32_t seq)
{
    const auto bas = Saat::now();
    // Alım kuyruğu yalnız burada (ve el sıkışmada) işlenir; yuva tamamlanınca ya da oturum
    // bitince o çerçevede durulur: sonrası sonraki yuvanın beklemesinde işlenir (W4-K14).
    const auto dur = [this, seq] {
        return k_->yuva_tamam(seq) || k_->durum() != kopru::OturumDurumu::kosuyor;
    };
    const std::uint64_t beslenen_bas = sil_beslenen_;
    for (;;) {
        (void)sil_besle(dur);
        if (!k_->yuva_tamam(seq) && k_->durum() == kopru::OturumDurumu::kosuyor &&
            sil_beslenen_ - beslenen_bas > kSilBeklemeAzami) {
            // W4-K26 (3) (EMNİYET): yuvayı tamamlamayan sınırsız akış (ör. kesintisiz 0x00).
            ++s_.sil_kuyruk_tasma;
            k_->protokol_hatasi(duvar());
            sil_gelen_.clear();
            sil_gelen_k_ = 0;
        }
        if (k_->yuva_tamam(seq)) {
            return true;
        }
        if (k_->durum() != kopru::OturumDurumu::kosuyor) {
            // Oturum bitti (karşı taraf kapattı): yeniden kurulur; bu yuva kaçırılır.
            gunluk(1, "oturum bitti (parti " + std::to_string(seq) + "); yeniden kuruluyor");
            ++s_.yeniden_baglanti;
            return oturum_kur();
        }
        pompa(1000);
        if (Saat::now() - bas > std::chrono::nanoseconds(a_.t_sil_ns)) {
            k_->sil_gozetimi_doldu(duvar());   // A1-K76
            gunluk(1, "T_SIL doldu (parti " + std::to_string(seq) + ")");
            if (ic_) {
                s_.hata = "süreç içi karşı taraf yanıt vermedi";
                return false;
            }
            if (tas_) {
                tas_->kapat_duzgun(kKapanisBosaltmaUs);   // W4-K25 (5)
                tas_.reset();
            }
            sil_gelen_temizle();
            ++s_.yeniden_baglanti;
            return oturum_kur();
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
    if (!plan_kur(*cs.cozumlenmis, cs.rapor, plan_, hata)) {
        s_.hata = "senaryo bu dilimde yürütülemez: " + hata;
        s_.hata_turu = HataTuru::senaryo_yurutulemez;
        return false;
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
    for (std::size_t i = 0; i < plan_.olaylar.size(); ++i) {
        const PlanOlayi& o = plan_.olaylar[i];
        if (o.tur != PlanOlayTuru::sensor) {
            continue;
        }
        sensor::ArmSonucu r;
        if (o.zaman_ns) {
            r = kume_.arm(o.ariza);   // zamanlı: canlı kümede; başlangıç ve bitişi küme yürütür
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
    s_.olay_sonuclari.push_back({plan_.olaylar[i].kimlik, sonuc, t, neden});
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
                const sensor::ArmSonucu r = kume_.arm(f);
                if (!r.tamam) {
                    // Ön denetimden geçtiği için beklenmez; nedeni olduğu gibi yazılır.
                    const bool hedef = r.hata.find("hedef") != std::string::npos;
                    olay_sonucu(i, "reddedildi", t, hedef ? "hedef_yok" : "desteklenmiyor");
                    s_.senaryo_uyarilari.push_back("olaylar[" + o.kimlik + "] başlatılamadı: " + r.hata);
                    break;
                }
            }
            olay_sonucu(i, "uygulandi", t);
            break;
        case PlanOlayTuru::itici:
            itici_.push_back({o.kanal_maskesi, o.carpan,
                              o.sure_ns > 0 ? t + o.sure_ns : std::numeric_limits<std::int64_t>::max()});
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
bool Calistirici::sinir_on(std::uint64_t b)
{
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    plan_sinir(b);
    const std::int64_t e0 = gercek();
    const kopru::SinirSonucu ss = k_->sinir_isle(b, duvar());
    const std::int64_t e1 = gercek();
    plan_sinir_sonrasi(b, ss);
    evre_[kEvreKopru] += e1 - e0;
    son_pwm_yur_ = pwm_yur_;
    pwm_yur_ = ss.pwm_us;
    std::array<double, 8> pwm = ss.pwm_us;
    const double notr = static_cast<double>(arac_.esc.pwm_notr_us);
    // EMNİYET (iskelet §5, W3-K1): acil durdurma iticilerin enerjisini keser; köprüden gelen
    // komut ne olursa olsun itici girdisi nötrdür.
    onceki_estop_ = estop_bu_adim_;
    estop_bu_adim_ = k_->acil_durdurma();
    if (estop_bu_adim_) {
        pwm.fill(notr);
        ++s_.estop_adim;
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
    return true;
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

Sonuc Calistirici::kos()
{
    // ---- Senaryo: A5 dosyası ya da yerleşik ----
    s_.kip = a_.kip;
    if (!a_.senaryo_dosyasi.empty()) {
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

    // ---- Köprü ----
    kopru::KopruAyari ka;
    ka.kip = a_.kip == Kip::sil ? kopru::CalistirmaKipi::sil : kopru::CalistirmaKipi::hil_1x;
    ka.ana_tohum = a_.tohum;
    ka.esc_alt_us = static_cast<std::uint16_t>(arac_.esc.pwm_min_us);
    ka.esc_notr_us = static_cast<std::uint16_t>(arac_.esc.pwm_notr_us);
    ka.esc_ust_us = static_cast<std::uint16_t>(arac_.esc.pwm_max_us);
    ka.beklenen_blok_crc = pb_.crc;
    if (a_.kip == Kip::hil) {
        // A1-K126: kartlı kiplerde run_id metin kimliğinden; metin kimliği (A3/A6) bu dilimde
        // yok: başlangıç duvar saatinden türetilmiş sıfırdan farklı değer.
        const auto z = static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
        ka.run_id = static_cast<std::uint32_t>(z ^ (z >> 32U)) | 1u;
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

    if (a_.kip == Kip::hil) {
        zamanlayici_onceligi();   // HIL 1x: duvar saatine kilitli döngü
    }
    bas_ = Saat::now();
    if (!oturum_kur()) {
        s_.sayac = k_->sayaclar();
        s_.kart = k_->kart();
        return s_;
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
    std::vector<std::int64_t> tf_yazim;
    tf_yazim.reserve(static_cast<std::size_t>(n_sinir / 4U + 1U));
    if (a_.kip == Kip::hil) {
        profil_.reserve(static_cast<std::size_t>(n_sinir + 1U));
        tf_gec_.reserve(static_cast<std::size_t>(n_sinir / 4U + 1U));
    }

    for (std::uint64_t b = 0; b <= n_sinir; ++b) {
        const std::int64_t t_b = static_cast<std::int64_t>(b) * kH;
        if (a_.kip == Kip::sil) {
            sil_duvar_ = t_b;
            pompa(0);
            k_->zamanla(duvar());
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
        baglanti_bakim();
        if (!sinir_on(b)) {
            break;
        }
        const bool kontrol = b > 0 && b % kopru::kPeriyotAdim == 0;
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
                tf_yazim.push_back(yazim);
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
                if (gon && !yuva_bekle(seq)) {
                    break;
                }
                satirlari_tamamla(gon ? 1u : 0u);   // W4-K14: önceki satır ACTUATOR(n)'den sonra
            }
        } else {
            yaz();
        }
        if (a_.kip == Kip::sil && k_->durum() != kopru::OturumDurumu::kosuyor) {
            // A1-K121 (iv): SIL'de sim zamanı el sıkışmada ilerlemez. Oturum yuva beklemesi
            // dışında (sim tarafında; ör. A1-K111) bittiyse yeni oturum fizik ilerlemeden kurulur.
            gunluk(1, "oturum bitti (sınır " + std::to_string(b) + "); yeniden kuruluyor");
            ++s_.yeniden_baglanti;
            if (!oturum_kur()) {
                break;
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
        if (a_.sinir_kancasi) {
            a_.sinir_kancasi(gozlem_);
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
    k_->oturum_kapat(kCloseShutdown, duvar());
    yaz();
    pompa(0);
    if (tas_) {
        // W4-K25 (5): düzgün kapanış; SIL'de alım kuyruğu artık işlenmez, atılan baytlar da.
        tas_->kapat_duzgun(kKapanisBosaltmaUs);
    }
    dur_ = true;
    if (karsi_is_.joinable()) {
        karsi_is_.join();
    }
    if (surec_) {
        surec_->sonlandir();
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
    z.tf_sayisi = tf_yazim.size();
    for (std::size_t i = 1; i < tf_yazim.size(); ++i) {
        z.periyotlar_ns.push_back(tf_yazim[i] - tf_yazim[i - 1]);
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

Sonuc kos(const Ayar& ayar)
{
    Calistirici c(ayar);
    return c.kos();
}

}  // namespace ida::sunucu
