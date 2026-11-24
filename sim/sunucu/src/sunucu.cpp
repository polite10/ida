// Başsız sim sunucusu: fizik + sensör kümesi + A1 köprüsü + zaman yöneticisi (sunucu.hpp).
#include "ida/sunucu/sunucu.hpp"

#include "a3_baglanti.hpp"
#include "ida/a3/kaynak.hpp"
#include "ida/a3/sunucu.hpp"
#include "ida/a3/zaman.hpp"
#include "ida/fizik/dunya.hpp"
#include "ida/kopru/karsi.hpp"
#include "ida/kopru/param_blogu.hpp"
#include "ida/kopru/tasima.hpp"
#include "ida/senaryo/senaryo.hpp"
#include "ida/sensor/sensor_kumesi.hpp"
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
        if (!tas_ || cikis_k_ >= cikis_.size()) {
            return;
        }
        const std::int64_t e0 = gercek();
        for (int j = 0; j < 8 && cikis_k_ < cikis_.size(); ++j) {
            const long long k = tas_->yaz_biraz(cikis_.data() + cikis_k_, cikis_.size() - cikis_k_);
            if (k < 0) {
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
        for (;;) {
            std::size_t n = 0;
            const kopru::OkumaSonucu r = tas_->oku(alim_.data(), alim_.size(), bekle_us, n);
            bekle_us = 0;
            if (r == kopru::OkumaSonucu::tamam) {
                if (a_.a1_bayt_kancasi) {
                    a_.a1_bayt_kancasi(false, alim_.data(), n);
                }
                if (sil) {
                    sil_gelen_.insert(sil_gelen_.end(), alim_.data(), alim_.data() + n);
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
        if (sil && sil_gelen_.size() - sil_gelen_k_ > kSilGelenAzami) {
            baglanti_koptu("SIL alım kuyruğu taştı");   // karşı taraf kilitli adımı bozuyor
        }
        evre_[kEvreOkuma] += gercek() - e0;
    }

    // SIL: alım kuyruğunu akış sırasıyla köprüye verir; dur() doğru olunca o çerçevede durur ve
    // kalan baytlar kuyrukta kalır. Kuyruk bitti ve akış kapandıysa (bekleyen hâlâ veri
    // istiyorsa) bağlantı kaybı burada işlenir. Dönüş: dur().
    bool sil_besle(const std::function<bool()>& dur)
    {
        while (sil_gelen_k_ < sil_gelen_.size() && !dur()) {
            sil_gelen_k_ += k_->al(sil_gelen_.data() + sil_gelen_k_, sil_gelen_.size() - sil_gelen_k_, duvar(), dur);
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

    // ---- A3 (W4-bagla-a3) ----
    bool a3_baslat();
    bool a3_baslat_bekle();
    void a3_cek();
    void a3_calistirma_isle(std::uint64_t b);
    bool a3_sinir_basi(std::uint64_t b);
    void a3_duraklatma_adimi(std::uint64_t b);
    void a3_emniyet_ver(std::uint64_t b);
    void a3_sinir_sonrasi(std::uint64_t b, const kopru::SinirSonucu& ss);
    void a3_yi_uygula(std::uint64_t b, const a3::Komut& k, const kopru::SinirSonucu* ss);
    void a3_yi_izle();
    void a3_yayinla(std::uint64_t b, bool zorla);
    void a3_sensor_yayinla();
    void a3_durum_yayinla(std::int64_t t);
    void a3_gecerlilik(std::uint64_t b);
    void a3_oturum_izle(std::uint64_t b);
    void a3_olay_kararlari(std::uint64_t b);
    void a3_ariza_izle(std::uint64_t b);
    void a3_gecis(a3::CalistirmaDurumu yeni, const char* neden, bool ic);
    void a3_sonuc(const a3::Komut& k, bool uygulandi, std::uint64_t b, std::string_view kod,
                  std::string_view ayrinti = {});
    void a3_olay(std::uint16_t tur, std::uint8_t onem, std::uint8_t kaynak, std::uint32_t komut_no,
                 std::uint32_t istemci, std::string_view ayrinti);
    void a3_senaryo_ogesi(std::size_t i, const char* sonuc, std::int64_t t, const char* neden);
    void a3_emniyet_olaylari(std::uint64_t b, const sensor::EmniyetDurumu& once,
                             const std::vector<std::pair<std::uint32_t, a3::Komut>>& operator_degisen);
    void a3_anlik_guncelle();
    void a3_bitir();
    template <typename F>
    void a3_olc(F&& f);

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
    static constexpr std::size_t kSilGelenAzami = std::size_t{1} << 26U;   // 64 MiB
    // W4-K25 (5): düzgün kapanışta karşı tarafın kapatması en çok bu kadar beklenir (TCP).
    static constexpr std::int64_t kKapanisBosaltmaUs = 500'000;
    std::array<std::int64_t, kEvreSayisi> evre_{};
    std::vector<std::array<std::int64_t, kEvreSayisi>> profil_;   // HIL: sınır başına
    std::vector<std::pair<std::int64_t, std::uint64_t>> tf_gec_;   // HIL: (TF geç yazım, sınır)
    std::vector<sensor::Olcum> olcumler_;
    std::array<double, 8> pwm_yur_{};
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

    // ---- A3 (W4-bagla-a3) ----
    std::unique_ptr<A3Alici> a3al_;
    std::unique_ptr<a3::Sunucu> a3sv_;
    a3::Yayinci* yay_ = nullptr;                 // null: A3 kapalı (bugünkü davranış)
    a3::AnlikDurum a3d_;                          // dünyanın yetkili çalıştırma durumu (A3 Tablo 4.7a)
    std::string a3_run_id_;                       // A3-K63 metin kimliği (çalıştırma başlayınca)
    bool a3_anlik_kirli_ = false;
    bool a3_durum_kirli_ = false;                 // durum kanalları sonraki yayın noktasında
    std::vector<a3::Komut> a3_kisit_, a3_genel_;
    std::vector<std::pair<std::uint32_t, a3::Komut>> a3_emniyet_bekleyen_;   // köprüye verilen operatör istekleri
    bool duraklatildi_ = false;
    bool a3_durdur_ = false;
    std::uint64_t adimla_kalan_ = 0;              // fizik adımı
    double hiz_ = 0.0;                            // SIL hız çarpanı (A3-K66; 0: olabildiğince hızlı)
    std::int64_t hiz_t0_sim_ = 0;
    Saat::time_point hiz_t0_duvar_{};
    std::int64_t duraklama_bas_ = 0;              // HIL: duraklatmanın başladığı duvar anı
    std::uint64_t a3_b_ = 0;                      // son işlenen sınır (olayların adım numarası)
    struct YiIzi {
        std::uint32_t komut_no = 0;
        std::uint32_t istemci = 0;
        std::string komut;
        std::uint32_t cmd_id = 0;
        std::uint16_t param_id = 0;   // param_get
    };
    std::vector<YiIzi> a3_yi_;
    std::map<std::uint64_t, std::uint32_t> a3_olay_sira_;   // A6-K41 normallestirilmis_sira
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
    bool a3_nav_var_ = false;
    std::uint64_t a3_kacirma_adet_ = 0, a3_kacirma_ilk_ = 0, a3_kacirma_son_ = 0;
    std::int64_t a3_kacirma_olay_duvar_ = 0;
    std::map<std::string, std::uint64_t> a3_arizalar_;   // etkin arızalar: kimlik → başlangıç adımı
    std::size_t a3_celiski_n_ = 0, a3_gevsetme_n_ = 0;
    std::uint32_t a3_celiski_kimligi_ = 0;
    bool a3_doydu_bildirildi_ = false;
    std::int64_t a3_carpan_ref_duvar_ = 0, a3_carpan_ref_sim_ = 0;
    float a3_gerceklesen_ = std::numeric_limits<float>::quiet_NaN();
    std::int64_t a3_sinir_ns_ = 0;               // bu sınırdaki yayın çağrılarının toplamı
    std::int64_t a3_son_duraklama_yayini_ = 0;
    sensor::EmniyetDurumu a3_emn_once_{};         // sınır işleminden önceki yetkili emniyet durumu
    std::optional<a3::Komut> a3_baslat_komutu_;   // `baslat` (a3_bekle): sonucu ön denetimden sonra
    std::uint32_t a3_son_komut_no_ = 0;
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
    for (;;) {
        (void)sil_besle(dur);
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
    s_.olay_sonuclari.push_back({plan_.olaylar[i].kimlik, sonuc, t, neden, "senaryo"});
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
bool Calistirici::sinir_on(std::uint64_t b)
{
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    plan_sinir(b);
    if (yay_ != nullptr) {
        // W4-bagla-a3 (EMNİYET): panelin emniyet istekleri ve daraltıcı komutları senaryonun
        // atamalarıyla aynı sınırda, aynı köprü yolundan (emniyet_ata, komut_*) girer; sırayı
        // (A1-K85) köprü uygular.
        a3_emniyet_ver(b);
    }
    const std::int64_t e0 = gercek();
    const kopru::SinirSonucu ss = k_->sinir_isle(b, duvar());
    const std::int64_t e1 = gercek();
    plan_sinir_sonrasi(b, ss);
    if (yay_ != nullptr) {
        a3_sinir_sonrasi(b, ss);
    }
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
    if (a_.sinir_kancasi) {
        Ayar::SinirGozlemi g;
        g.b = b;
        g.derinlik = dn_.durum().konum_n[2];
        g.pwm = pwm;
        g.itici_carpani = carpanlar;
        g.acil_durdurma = estop_bu_adim_;
        a_.sinir_kancasi(g);
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

// ======================================================================== A3 (W4-bagla-a3)
//
// Dünya iş parçacığının A3 tarafı. Yayın çağrıları (Yayinci) engellemez, kilit almaz, bellek
// ayırmaz (kitaplığın güvencesi); yükler yalnız string_view/span taşır, olay ayrıntıları sabit
// tamponda yazılır. Bu dosyadaki çağrı yerleri fizik ve köprü sırasını değiştirmez: köprüye
// yalnız panel komutu varken dokunulur.

namespace {

constexpr std::uint64_t kYok64 = std::numeric_limits<std::uint64_t>::max();
constexpr std::uint64_t kHizliSinir = 16;    // 50 Hz sim zamanı (fizik/durum, kart/durum)
constexpr std::uint64_t kDurumSinir = 80;    // 10 Hz (A3 Tablo 3.3 varsayılanları)
constexpr std::uint64_t kYavasSinir = 400;   // 2 Hz

const char* girdi_durumu(bool takili_ya_da_basili, bool acil)
{
    return acil ? (takili_ya_da_basili ? "basili" : "serbest") : (takili_ya_da_basili ? "takili" : "cikarilmis");
}

std::string komut_adi(const a3::Komut& k)
{
    switch (k.tur) {
    case a3::KomutTuru::calistirma_baslat: return "calistirma/baslat";
    case a3::KomutTuru::calistirma_duraklat: return "calistirma/duraklat";
    case a3::KomutTuru::calistirma_surdur: return "calistirma/surdur";
    case a3::KomutTuru::calistirma_adimla: return "calistirma/adimla";
    case a3::KomutTuru::calistirma_hiz: return "calistirma/hiz";
    case a3::KomutTuru::calistirma_durdur: return "calistirma/durdur";
    case a3::KomutTuru::calistirma_sifirla: return "calistirma/sifirla";
    case a3::KomutTuru::emniyet_degistir: return "emniyet/degistir";
    case a3::KomutTuru::yer_istasyonu_komut: return "yer_istasyonu/" + k.yi_komut;
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
    a3_sinir_ns_ += ns;
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
}

void Calistirici::a3_sonuc(const a3::Komut& k, bool uygulandi, std::uint64_t b, std::string_view kod,
                           std::string_view ayrinti)
{
    if (yay_ == nullptr) {
        return;
    }
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    a3::KomutSonucuGirdisi s;
    s.komut_no = k.komut_no;
    s.uygulandi = uygulandi;
    s.uygulama_adimi_var = uygulandi;
    s.uygulama_adimi = b;
    s.t_ns = t;
    s.adim_no = b;
    s.hata_kodu = kod;
    s.sonuc_ayrinti_json = ayrinti;
    a3_olc([&] { (void)yay_->komut_sonucu(s); });
    // Kapsam 4: A3'ten gelen komutlar olay kaydına senaryodakilerle aynı biçimde, kaynağıyla.
    s_.olay_sonuclari.push_back({"a3/" + std::to_string(k.komut_no) + "/" + komut_adi(k),
                                 uygulandi ? "uygulandi" : "reddedildi", t, std::string(kod), "a3"});
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
    a3b::JsonYaz j;
    j.metin("onceki", a3b::durum_adi(eski)).metin("sonraki", a3b::durum_adi(yeni));
    if (neden != nullptr) {
        j.metin("neden", neden);
    }
    a3_olay(1, 0, 0, 0, 0, j.bitir());
    a3_durum_kirli_ = true;
    a3_anlik_kirli_ = true;
    a3_durum_yayinla(static_cast<std::int64_t>(a3_b_) * kH);
    a3_anlik_guncelle();
}

void Calistirici::a3_anlik_guncelle()
{
    if (a3al_ == nullptr) {
        return;
    }
    a3d_.sim_t_ns = static_cast<std::int64_t>(a3_b_) * kH;
    a3d_.adim_no = a3_b_;
    a3d_.kip = a_.kip == Kip::hil ? a3::ZamanKipi::hil_1x : a3::ZamanKipi::sil;
    if (a3d_.durum == a3::CalistirmaDurumu::yuklendi) {
        a3d_.run_id.reset();   // A3-K124
    }
    a3d_.gecerli = a3_gecerli_;
    a3d_.gercek_zamani_kacirdi = a3_rt_kacirdi_;
    a3d_.gecerlilik_nedenleri = a3_gecerlilik_;
    // Kademeler: 3B görünüm, kamera ve sonar ida_sim'de yoktur (kapali). Kayıt (A6) bu dilimde
    // yoktur; sözleşmenin değer kümesinde "yok" olmadığı için en alt kademe gösterilir (rapor).
    a3d_.kademe_gorunum = 0;
    a3d_.kademe_kamera = 0;
    a3d_.kademe_sonar = 0;
    a3d_.kademe_kayit = 0;
    if (!s_.davranis_ozeti.empty()) {
        a3d_.senaryo_ozeti = "sha256:" + s_.davranis_ozeti;
    }
    if (k_) {
        const sensor::EmniyetDurumu e = k_->dis_durum().emniyet;
        a3d_.emniyet_rev = e.emniyet_revizyonu;
        a3d_.emniyet_rev_doydu = k_->emniyet_rev_doydu();
        a3d_.acil_durdurma = e.acil_durdurma ? 1u : 0u;
        a3d_.silahlandirma_fisi = {static_cast<std::uint8_t>(e.fis[0] ? 1u : 0u), static_cast<std::uint8_t>(e.fis[1] ? 1u : 0u)};
        a3d_.emniyet_pimi = {static_cast<std::uint8_t>(e.pim[0] ? 1u : 0u), static_cast<std::uint8_t>(e.pim[1] ? 1u : 0u)};
        a3d_.a1_run_id = k_->run_id();
        const kopru::Sayaclar& c = k_->sayaclar();
        if (a3d_.sayaclar.size() != 5u) {
            a3d_.sayaclar = {{"parti_gonderilen", 0}, {"parti_kacirildi", 0}, {"oturum_kopma", 0},
                             {"sim_gecikmesi", 0}, {"a3_halka_tasma", 0}};
        }
        a3d_.sayaclar[0].second = static_cast<std::int64_t>(c.parti_gonderilen);
        a3d_.sayaclar[1].second = static_cast<std::int64_t>(c.parti_kacirildi);
        a3d_.sayaclar[2].second = static_cast<std::int64_t>(c.oturum_kopma);
        a3d_.sayaclar[3].second = static_cast<std::int64_t>(s_.zaman.sim_gecikmesi);
        a3d_.sayaclar[4].second = static_cast<std::int64_t>(yay_ != nullptr ? yay_->halka_tasma() : 0u);
    } else {
        a3d_.emniyet_rev = 1;
        a3d_.acil_durdurma = plan_.acil_durdurma ? 1u : 0u;
        a3d_.silahlandirma_fisi = {static_cast<std::uint8_t>(plan_.fis[0] ? 1u : 0u), static_cast<std::uint8_t>(plan_.fis[1] ? 1u : 0u)};
        a3d_.emniyet_pimi = {static_cast<std::uint8_t>(plan_.pim[0] ? 1u : 0u), static_cast<std::uint8_t>(plan_.pim[1] ? 1u : 0u)};
    }
    if (a3_son_degisim_adimi_ != kYok64) {
        a3d_.son_degisim_adimi = a3_son_degisim_adimi_;
    }
    if (a3al_->anlik_ver(a3d_)) {
        a3_anlik_kirli_ = false;   // G/Ç kopyalıyorsa sonraki çağrıda yeniden denenir
    } else {
        a3_anlik_kirli_ = true;
    }
}

bool Calistirici::a3_baslat()
{
    a3al_ = std::make_unique<A3Alici>(a_.kip == Kip::hil);
    a3::SunucuAyari sa;
    sa.port = a_.a3_port;
    sa.statik_kok = a_.a3_kok;
    sa.sim_surumu = "ida_sim";
    const int gunluk_duzeyi = a_.gunluk;
    sa.gunluk = [gunluk_duzeyi](std::string_view m) {
        if (gunluk_duzeyi >= 1) {
            std::fprintf(stderr, "ida_sim: %.*s\n", static_cast<int>(m.size()), m.data());
        }
    };
    std::string hata;
    a3sv_ = a3::Sunucu::baslat(sa, *a3al_, hata);
    if (!a3sv_) {
        s_.hata = "A3 sunucusu başlatılamadı: " + hata;
        a3al_.reset();
        return false;
    }
    yay_ = &a3sv_->yayinci();
    a3d_.durum = a3::CalistirmaDurumu::yuklendi;
    a3d_.durum_rev = 1;
    a3_anlik_guncelle();
    a3_durum_yayinla(0);
    gunluk(1, "A3 127.0.0.1:" + std::to_string(a3sv_->port()));
    if (a_.a3_acilis) {
        a_.a3_acilis(a3sv_->acilis_adresi());
    }
    if (a_.a3_hazir) {
        a_.a3_hazir(a3sv_->port(), a3sv_->jeton());
    }
    return true;
}

void Calistirici::a3_cek()
{
    a3al_->cek(a3_kisit_, a3_genel_);
    for (const a3::Komut& k : a3_kisit_) {
        a3_son_komut_no_ = std::max(a3_son_komut_no_, k.komut_no);
    }
    for (const a3::Komut& k : a3_genel_) {
        a3_son_komut_no_ = std::max(a3_son_komut_no_, k.komut_no);
    }
}

// `yuklendi`'de `baslat` beklenir (A3-K62); `sifirla` yalnız durum revizyonunu artırır (A3-K67).
// Canlı çalıştırma gerektiren istekler reddedilir (A3-K91).
bool Calistirici::a3_baslat_bekle()
{
    gunluk(1, "A3: çalıştırma panelden `baslat` ile başlayacak");
    std::int64_t son_yayin = 0;
    for (;;) {
        if (a_.a3_sinir_kancasi) {
            a_.a3_sinir_kancasi(0, *a3al_);
        }
        a3_cek();
        for (const a3::Komut& k : a3_kisit_) {
            a3_sonuc(k, false, 0, "canli_calistirma_yok");
        }
        a3_kisit_.clear();
        std::vector<a3::Komut> genel;
        genel.swap(a3_genel_);
        bool basla = false;
        for (a3::Komut& k : genel) {
            if (basla || !a3b::calistirma_mi(k.tur)) {
                a3_sonuc(k, false, 0, basla ? "gecersiz_durum" : "canli_calistirma_yok");
                continue;
            }
            if (k.beklenen_durum_rev != a3d_.durum_rev) {
                a3_sonuc(k, false, 0, "durum_rev_uyusmuyor",
                         a3b::JsonYaz().sayi_u("guncel_durum_rev", a3d_.durum_rev).bitir());
                continue;
            }
            if (k.beklenen_run_id.has_value()) {
                a3_sonuc(k, false, 0, "beklenen_durum_uyusmuyor");
                continue;
            }
            if (k.tur == a3::KomutTuru::calistirma_baslat) {
                ++a3d_.durum_rev;   // A3-K122; sonuç ön denetimden sonra (A3-K62)
                a3_baslat_komutu_ = std::move(k);
                basla = true;
            } else if (k.tur == a3::KomutTuru::calistirma_sifirla) {
                ++a3d_.durum_rev;
                a3_sonuc(k, true, 0, {});
                a3_durum_kirli_ = true;
            } else {
                a3_sonuc(k, false, 0, "gecersiz_durum");
            }
        }
        if (basla) {
            return true;
        }
        if (a3_durum_kirli_ || gercek() - son_yayin > 100 * kMs) {
            a3_durum_yayinla(0);
            a3_anlik_guncelle();
            son_yayin = gercek();
        } else if (a3_anlik_kirli_) {
            a3_anlik_guncelle();
        }
        sure_uyu(2 * kMs);
    }
}

// Çalıştırma kontrolü komutları (A3-K61…K66, K122, K123): sınırın başında, durumla aynı atomik
// adımda yeniden denetlenir.
void Calistirici::a3_calistirma_isle(std::uint64_t b)
{
    std::vector<a3::Komut> kalan;
    for (a3::Komut& k : a3_genel_) {
        if (!a3b::calistirma_mi(k.tur)) {
            kalan.push_back(std::move(k));
            continue;
        }
        const bool hil = a_.kip == Kip::hil;
        const bool run_uyar = a3d_.run_id.has_value() == k.beklenen_run_id.has_value() &&
                              (!a3d_.run_id || *a3d_.run_id == *k.beklenen_run_id);
        if (k.beklenen_durum_rev != a3d_.durum_rev) {
            a3_sonuc(k, false, b, "durum_rev_uyusmuyor", a3b::JsonYaz().sayi_u("guncel_durum_rev", a3d_.durum_rev).bitir());
            continue;
        }
        if (!run_uyar) {
            a3_sonuc(k, false, b, "beklenen_durum_uyusmuyor",
                     a3b::JsonYaz().sayi_u("guncel_durum_rev", a3d_.durum_rev).bitir());
            continue;
        }
        if (!a3b::gecis_gecerli(a3d_.durum, k.tur) ||
            (k.tur == a3::KomutTuru::calistirma_sifirla && a3d_.durum != a3::CalistirmaDurumu::yuklendi)) {
            a3_sonuc(k, false, b, "gecersiz_durum", a3b::JsonYaz().sayi_u("guncel_durum_rev", a3d_.durum_rev).bitir());
            continue;
        }
        if (hil && (k.tur == a3::KomutTuru::calistirma_adimla || k.tur == a3::KomutTuru::calistirma_hiz)) {
            a3_sonuc(k, false, b, "kipte_gecersiz", a3b::JsonYaz().sayi_u("guncel_durum_rev", a3d_.durum_rev).bitir());
            continue;
        }
        if (hil && k.tur == a3::KomutTuru::calistirma_duraklat && !k.gecersizligi_onayla) {
            a3_sonuc(k, false, b, "onay_gerekli");
            continue;
        }
        ++a3d_.durum_rev;   // A3-K122
        switch (k.tur) {
        case a3::KomutTuru::calistirma_duraklat:
            duraklatildi_ = true;
            adimla_kalan_ = 0;
            if (hil) {
                // A1-K41, K79; Tablo 4.7b: HIL'de duraklatma çalıştırmayı geçersiz kılar; TF ve
                // üyeler durur, canlılık sürer.
                k_->hil_duraklatma_bildir();
                duraklama_bas_ = gercek();
            }
            a3_gecis(a3::CalistirmaDurumu::duraklatildi, nullptr, false);
            break;
        case a3::KomutTuru::calistirma_surdur:
            duraklatildi_ = false;
            if (hil) {
                w0_ += gercek() - duraklama_bas_;   // W(t) = W0 + t sürer (A1-K29: sayılar boşluksuz)
            }
            a3_gecis(a3::CalistirmaDurumu::kosuyor, nullptr, false);
            break;
        case a3::KomutTuru::calistirma_adimla:
            duraklatildi_ = false;
            adimla_kalan_ = static_cast<std::uint64_t>(k.periyot_sayisi) * kopru::kPeriyotAdim;   // A3-K65
            a3_gecis(a3::CalistirmaDurumu::kosuyor, "adimla", false);
            break;
        case a3::KomutTuru::calistirma_hiz:
            hiz_ = k.carpan;   // A3-K66 (yalnız SIL; 0: olabildiğince hızlı)
            hiz_t0_sim_ = static_cast<std::int64_t>(b) * kH;
            hiz_t0_duvar_ = Saat::now();
            a3_durum_kirli_ = true;
            break;
        case a3::KomutTuru::calistirma_durdur:
            a3_durdur_ = true;
            duraklatildi_ = false;
            a3_gecis(a3::CalistirmaDurumu::durduruldu, nullptr, false);
            break;
        default:
            break;
        }
        a3_sonuc(k, true, b, {});
    }
    a3_genel_ = std::move(kalan);
}

bool Calistirici::a3_sinir_basi(std::uint64_t b)
{
    a3_b_ = b;
    a3_sinir_ns_ = 0;
    if (a_.a3_sinir_kancasi) {
        a_.a3_sinir_kancasi(b, *a3al_);
    }
    a3_cek();
    a3_calistirma_isle(b);
    bool durakladi = false;
    while (duraklatildi_ && !a3_durdur_) {
        durakladi = true;
        a3_duraklatma_adimi(b);
    }
    if (a3_durdur_) {
        return false;
    }
    if (a_.kip == Kip::sil) {
        if (durakladi) {
            hiz_t0_sim_ = static_cast<std::int64_t>(b) * kH;
            hiz_t0_duvar_ = Saat::now();
        }
        if (hiz_ > 0.0 && b % kopru::kPeriyotAdim == 0) {
            // A3-K66: SIL hız çarpanı (yalnız bekleme; bayt akışı ve CSV değişmez).
            const double ns = static_cast<double>(static_cast<std::int64_t>(b) * kH - hiz_t0_sim_) / hiz_;
            a3::hassas_bekle(hiz_t0_duvar_ + std::chrono::nanoseconds(static_cast<std::int64_t>(ns)));
        }
    }
    return true;
}

// Duraklatılmış sınırda bir yoklama (≈ 1 ms). EMNİYET (A1-K85, A3-K58, K95): duraklatılmışken gelen
// emniyet ve yer istasyonu istekleri beklemez; aynı köprü yolundan (emniyet_ata, komut_* →
// sinir_isle), aynı sırayla, sim zamanı ilerlemeden işlenir. Fizik adımı ve sensörler koşmaz.
void Calistirici::a3_duraklatma_adimi(std::uint64_t b)
{
    a3_cek();
    a3_calistirma_isle(b);
    if (!duraklatildi_ || a3_durdur_) {
        return;
    }
    if (!a3_kisit_.empty() || !a3_genel_.empty()) {
        a3_emniyet_ver(b);
        const kopru::SinirSonucu ss = k_->sinir_isle(b, duvar());
        a3_sinir_sonrasi(b, ss);
    }
    if (a_.kip == Kip::hil) {
        // A1-K41: TF ve üyeler durur, canlılık (KEEPALIVE) ve bağlantı bakımı sürer.
        k_->zamanla(gercek());
        yaz();
        pompa(0);
        baglanti_bakim();
    }
    if (gercek() - a3_son_duraklama_yayini_ > 100 * kMs || a3_durum_kirli_) {
        a3_yayinla(b, true);
        a3_son_duraklama_yayini_ = gercek();
    } else if (a3_anlik_kirli_) {
        a3_anlik_guncelle();
    }
    sure_uyu(kMs);
}

// Sınır işleminden önce: kısıtlayan istekler (emniyet ve daraltıcı yer istasyonu komutları) ve
// gevşeten emniyet istekleri köprüye verilir. EMNİYET: kısıtlayan istek `beklenen_run_id`'ye
// bakılmadan uygulanır (W4-K19); revizyon istemez (A3-K92, K104, K119).
void Calistirici::a3_emniyet_ver(std::uint64_t b)
{
    a3_emn_once_ = k_->dis_durum().emniyet;
    a3_emniyet_bekleyen_.clear();
    const bool canli = a3b::canli(a3d_.durum);
    for (a3::Komut& k : a3_kisit_) {
        if (!canli) {
            a3_sonuc(k, false, b, "canli_calistirma_yok");
            continue;
        }
        if (k.tur == a3::KomutTuru::emniyet_degistir) {
            k_->emniyet_ata(operator_atamasi(k));
            a3_emniyet_bekleyen_.emplace_back(k.komut_no, std::move(k));
        } else {
            a3_yi_uygula(b, k, nullptr);   // daraltıcı: A1-K85 adım 2 (köprü revizyonu artırır)
        }
    }
    a3_kisit_.clear();
    std::vector<a3::Komut> kalan;
    for (a3::Komut& k : a3_genel_) {
        if (k.tur != a3::KomutTuru::emniyet_degistir) {
            kalan.push_back(std::move(k));
            continue;
        }
        // A3-K57: beklenen_run_id uygulamadan hemen önce yeniden karşılaştırılır (kısıtlayan hariç).
        const bool run_uyar = a3d_.run_id.has_value() && k.beklenen_run_id.has_value() &&
                              *a3d_.run_id == *k.beklenen_run_id;
        if (!canli) {
            a3_sonuc(k, false, b, "canli_calistirma_yok");
            continue;
        }
        if (!k.kisitlayan && !run_uyar) {
            a3_sonuc(k, false, b, "beklenen_durum_uyusmuyor");
            continue;
        }
        k_->emniyet_ata(operator_atamasi(k));
        a3_emniyet_bekleyen_.emplace_back(k.komut_no, std::move(k));
    }
    a3_genel_ = std::move(kalan);
}

// Sınır işleminden sonra: operatör emniyet isteklerinin sonuçları, emniyet olayları, genişletici
// ve nötr yer istasyonu komutları (A1-K85 adım 4'ün yerinde: revizyon bu sınırda arttıysa
// genişletici `kisitlama_hakim`).
void Calistirici::a3_sinir_sonrasi(std::uint64_t b, const kopru::SinirSonucu& ss)
{
    std::vector<std::pair<std::uint32_t, a3::Komut>> degisen;
    for (const kopru::OperatorSonucu& o : ss.operator_sonuclari) {
        auto it = std::find_if(a3_emniyet_bekleyen_.begin(), a3_emniyet_bekleyen_.end(),
                               [&o](const auto& p) { return (p.first == 0u ? 1u : p.first) == o.kimlik; });
        if (it == a3_emniyet_bekleyen_.end()) {
            continue;
        }
        const a3::Komut& k = it->second;
        a3b::JsonYaz ay;
        ay.sayi_u("guncel_emniyet_rev", o.rev_sonra);
        switch (o.sonuc) {
        case kopru::AtamaSonucu::uygulandi:
            a3_sonuc(k, true, b, {});
            if (o.degisti) {
                degisen.emplace_back(k.komut_no, k);
            }
            break;
        case kopru::AtamaSonucu::kisitlama_hakim: a3_sonuc(k, false, b, "kisitlama_hakim", ay.bitir()); break;
        case kopru::AtamaSonucu::rev_uyusmuyor: a3_sonuc(k, false, b, "emniyet_rev_uyusmuyor", ay.bitir()); break;
        case kopru::AtamaSonucu::rev_doydu: a3_sonuc(k, false, b, "emniyet_rev_doydu"); break;
        }
        a3_emniyet_bekleyen_.erase(it);
    }
    for (const auto& [no, k] : a3_emniyet_bekleyen_) {
        (void)no;
        a3_sonuc(k, false, b, "sunucu_hatasi");   // köprü sonuç vermedi: olmamalı
    }
    a3_emniyet_bekleyen_.clear();
    a3_emniyet_olaylari(b, a3_emn_once_, degisen);
    a3_emn_once_ = k_->dis_durum().emniyet;
    if (ss.yuva_kacirildi) {
        if (a3_kacirma_adet_ == 0u) {
            a3_kacirma_ilk_ = ss.yuva_seq;
        }
        a3_kacirma_son_ = ss.yuva_seq;
        ++a3_kacirma_adet_;
    }
    std::vector<a3::Komut> genel;
    genel.swap(a3_genel_);
    for (const a3::Komut& k : genel) {
        a3_yi_uygula(b, k, &ss);
    }
}

void Calistirici::a3_yi_uygula(std::uint64_t b, const a3::Komut& k, const kopru::SinirSonucu* ss)
{
    if (k.tur != a3::KomutTuru::yer_istasyonu_komut) {
        a3_sonuc(k, false, b, "sunucu_hatasi");
        return;
    }
    if (!a3b::canli(a3d_.durum)) {
        a3_sonuc(k, false, b, "canli_calistirma_yok");
        return;
    }
    if (!k.kisitlayan) {
        const bool run_uyar = a3d_.run_id.has_value() && k.beklenen_run_id.has_value() &&
                              *a3d_.run_id == *k.beklenen_run_id;
        if (!run_uyar) {
            a3_sonuc(k, false, b, "beklenen_durum_uyusmuyor");
            return;
        }
    }
    if (k.yi_sinif == a3::KomutSinifi::genisletici) {
        // EMNİYET (A3-K95 (4), K118): genişletici, kabul edildiği revizyona bağlıdır.
        const std::uint32_t rev = k_->emniyet_revizyonu();
        a3b::JsonYaz ay;
        ay.sayi_u("guncel_emniyet_rev", rev);
        if (ss != nullptr && ss->rev_adim3_sonu != ss->rev_adim_basi) {
            a3_sonuc(k, false, b, "kisitlama_hakim", ay.bitir());
            return;
        }
        if (k_->emniyet_rev_doydu()) {
            a3_sonuc(k, false, b, "emniyet_rev_doydu");
            return;
        }
        if (!k.beklenen_emniyet_rev || *k.beklenen_emniyet_rev != rev) {
            a3_sonuc(k, false, b, "emniyet_rev_uyusmuyor", ay.bitir());
            return;
        }
    }
    a3b::YiKarsilik y;
    if (!a3b::yi_karsilik(k.yi_komut, k.yi_parametreler_json, y)) {
        a3_sonuc(k, false, b, "gecersiz_deger");
        return;
    }
    a3b::JsonYaz j;
    if (y.tur == a3b::YiTur::desteklenmiyor) {
        // Karşılığı olmayan komut (W4-bagla-a3 kapsam 3): komut ağ geçidinde işlendi, sonuç
        // `yer_istasyonu_yaniti` olayında `desteklenmiyor` (A3 §3.5).
        a3_sonuc(k, true, b, {});
        j.sayi_u("komut_no", k.komut_no).metin("komut", k.yi_komut).metin("sonuc", "desteklenmiyor");
        a3_olay(12, 0, 6, k.komut_no, k.istemci_id, j.bitir());
        return;
    }
    // Senaryonun operatör komutlarıyla aynı köprü yolu (komut_basit, komut_guided, komut_fc).
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    std::uint32_t id = 0;
    switch (y.tur) {
    case a3b::YiTur::basit: id = k_->komut_basit(y.a1_tur, y.arg, t); break;
    case a3b::YiTur::guided: {
        kopru::GuidedHedef h;
        h.mask = y.sp_mask;
        h.pos_ne = y.pos_ne;
        h.depth = y.depth;
        h.altitude = y.altitude;
        h.vel_ned = y.vel_ned;
        h.yaw = y.yaw;
        h.yaw_rate = y.yaw_rate;
        id = k_->komut_guided(h, t);
        break;
    }
    case a3b::YiTur::fc: id = k_->komut_fc(y.a1_tur, static_cast<std::uint8_t>(y.arg), t); break;
    case a3b::YiTur::desteklenmiyor: break;
    }
    a3_sonuc(k, true, b, {});
    const std::optional<kopru::KomutSonucu> r = k_->komut_sonucu(id);
    const bool oturumsuz = r && r->yerel && r->reason == 129u;   // IDAW_REASON_LINK_SESSION_END
    j.sayi_u("komut_no", k.komut_no).metin("komut", k.yi_komut).sayi_u("emniyet_rev", k_->emniyet_revizyonu())
        .metin("durum", oturumsuz ? "oturum_yok" : "karta_iletildi");
    a3_olay(11, 0, 6, k.komut_no, k.istemci_id, j.bitir());
    if (!oturumsuz) {
        a3_yi_.push_back({k.komut_no, k.istemci_id, k.yi_komut, id, y.arg});
    }
}

// Yer istasyonu komutlarının kart yanıtı ya da yerel sonucu (A3-K101, K102).
void Calistirici::a3_yi_izle()
{
    for (auto it = a3_yi_.begin(); it != a3_yi_.end();) {
        const std::optional<kopru::KomutSonucu> r = k_->komut_sonucu(it->cmd_id);
        if (!r) {
            ++it;
            continue;
        }
        a3b::JsonYaz j;
        j.sayi_u("komut_no", it->komut_no).metin("komut", it->komut);
        if (r->yerel && r->ack == 2u && (r->reason == 10u || r->reason == 129u)) {
            // A1-K129 (bayat yetki: NARROWED) ya da oturum bitti (LINK_SESSION_END): karta gitmedi.
            j.sayi_u("emniyet_rev", k_->emniyet_revizyonu()).metin("durum", r->reason == 10u ? "bayat_yetki" : "oturum_yok");
            a3_olay(11, 0, 6, it->komut_no, it->istemci, j.bitir());
        } else {
            const char* sonuc = r->ack == 1u ? "kabul" : r->ack == 3u ? "suruyor" : r->ack == 4u ? "yineleme"
                                : r->ack == 5u ? "desteklenmiyor" : "ret";
            a3b::JsonYaz v;
            v.metin("ack", a3b::ack_adi(r->ack)).sayi_u("reason", r->reason);
            if (it->komut == "param_get") {
                v.sayi_u("param_id", it->param_id);
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
                            v.ham("value", senaryo::sayi_dizilimi(static_cast<double>(f)));
                        }
                    }
                }
            }
            j.metin("sonuc", sonuc).ham("veri", v.bitir());
            a3_olay(12, 0, 6, it->komut_no, it->istemci, j.bitir());
        }
        it = a3_yi_.erase(it);
    }
}

// emniyet_girdisi_degisti (A3-K96), son değişim ve revizyon doygunluğu; senaryonun gevşetme
// retleri ve atama çelişkileri (A3 §3.5, A5 §3.15).
void Calistirici::a3_emniyet_olaylari(std::uint64_t b, const sensor::EmniyetDurumu& once,
                                      const std::vector<std::pair<std::uint32_t, a3::Komut>>& operator_degisen)
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
        for (const auto& [no, k] : operator_degisen) {
            (void)no;
            if (k.girdi == a.g && (a.tup < 0 || k.tup == a.tup)) {
                op = &k;
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
        s_.emniyet_degisimleri.push_back({static_cast<std::int64_t>(b) * kH, a.ad, a.o, a.s,
                                          op != nullptr ? "operator" : "senaryo", op != nullptr ? op->komut_no : 0u,
                                          simdi.emniyet_revizyonu});
        a3_son_degisim_adimi_ = b;
        a3_son_degisim_kaynagi_ = op != nullptr ? 2u : 1u;
        a3_durum_kirli_ = true;
        a3_anlik_kirli_ = true;
    }
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
            std::string deg = "[";
            std::string kim = "[";
            for (std::size_t i = p * 8u; i < std::min(n, p * 8u + 8u); ++i) {
                deg += (i > p * 8u ? ",\"" : "\"") + std::string(c.degerler[i] ? "true" : "false") + "\"";
                kim += (i > p * 8u ? ",\"" : "\"") + c.olay_kimlikleri[i] + "\"";
            }
            deg += "]";
            kim += "]";
            a3b::JsonYaz j;
            j.sayi("t_ns", c.t_ns).metin("alan", c.alan).sayi_u("celiski_kimligi", a3_celiski_kimligi_)
                .sayi_u("parca_no", p + 1u).sayi_u("parca_sayisi", parca).mantik("son_parca", p + 1u == parca)
                .ham("degerler", deg).ham("olay_kimlikleri", kim).metin("uygulanan_deger", c.uygulanan_deger ? "true" : "false");
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
void Calistirici::a3_olay_kararlari(std::uint64_t b)
{
    (void)b;
    k_->olay_kararlari_al(a3_kararlar_);
    for (const kopru::OlayKarari& o : a3_kararlar_) {
        const auto ins = a3_olay_sira_.emplace(o.event_id, static_cast<std::uint32_t>(a3_olay_sira_.size() + 1u));
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
        j.metin("olay_turu", tur).metin("sonuc", sonuc).sayi_u("normallestirilmis_sira", ins.first->second);
        if (o.sonuc == 1u && o.t_apply_ns >= 0) {
            j.sayi_u("uygulama_adimi", static_cast<std::uint64_t>(o.t_apply_ns / kH));
        }
        a3_olay(8, 3, 0, 0, 0, j.bitir());
        a3_durum_kirli_ = true;
    }
    a3_kararlar_.clear();
}

// a1_oturum_degisti (tür 9) ve kart_sifirlandi (tür 10).
void Calistirici::a3_oturum_izle(std::uint64_t b)
{
    (void)b;
    const bool kopuk = !tas_ && a_.kip == Kip::hil;
    const std::uint8_t simdi = a1_oturum_kodu(k_->durum(), kopuk);
    const std::uint8_t once = a1_oturum_kodu(a3_a1_onceki_, a3_a1_kopuk_);
    const kopru::KartBilgisi& kb = k_->kart();
    if (simdi != once) {
        const char* neden = simdi == 3u                                ? "el_sikisma_tamam"
                             : k_->durum() == kopru::OturumDurumu::reddedildi ? "uyumsuzluk"
                             : kopuk                                    ? "aygit_kalkti"
                             : k_->yeniden_baslatma_bekleniyor()        ? "reboot_istendi"
                             : kb.son_kapanis_nedeni == 2u              ? "beklenmedik_sifirlama"
                                                                        : "canlilik_kaybi";
        a3b::JsonYaz j;
        j.metin("onceki", a1_oturum_adi(once)).metin("sonraki", a1_oturum_adi(simdi)).sayi_u("sess", k_->sess())
            .sayi_u("acilis_sayaci", kb.boot_count).metin("neden", neden)
            .mantik("planli", std::string_view(neden) == "reboot_istendi" || simdi == 3u);
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

// Etkin arızalar (sensör kümesi ve itici çarpanı): ariza_degisti (A3-K86) ve ariza/etkin.
void Calistirici::a3_ariza_izle(std::uint64_t b)
{
    std::map<std::string, std::uint64_t> simdi;
    for (const sensor::EtkinAriza& e : kume_.active()) {
        simdi[e.tanim.kimlik] = static_cast<std::uint64_t>(e.t_baslangic_ns / kH);
    }
    for (const IticiAriza& a : itici_) {
        simdi[a.kimlik] = static_cast<std::uint64_t>(a.baslangic_ns / kH);
    }
    bool degisti = false;
    for (const auto& [id, bas] : simdi) {
        if (a3_arizalar_.count(id) == 0u) {
            a3b::JsonYaz j;
            j.metin("fault_id", id).metin("islem", "uygulandi").metin("kaynak", "senaryo").sayi_u("uygulama_adimi", bas);
            a3_olay(6, 0, 3, 0, 0, j.bitir());
            degisti = true;
        }
    }
    for (const auto& [id, bas] : a3_arizalar_) {
        (void)bas;
        if (simdi.count(id) == 0u) {
            a3b::JsonYaz j;
            j.metin("fault_id", id).metin("islem", "bitti").metin("kaynak", "senaryo").sayi_u("uygulama_adimi", b);
            a3_olay(6, 0, 3, 0, 0, j.bitir());
            degisti = true;
        }
    }
    a3_arizalar_ = std::move(simdi);
    if (!degisti && b % kYavasSinir != 0u) {
        return;
    }
    std::vector<a3::yuk_ariza_etkin_arizalar> v;
    std::vector<std::string> hedefler;   // string_view'lerin sahibi
    std::vector<std::string> turler;
    const std::vector<sensor::EtkinAriza> sa = kume_.active();
    hedefler.reserve(sa.size() + itici_.size());
    turler.reserve(sa.size() + itici_.size());
    for (const sensor::EtkinAriza& e : sa) {
        a3::yuk_ariza_etkin_arizalar x;
        x.fault_id = e.tanim.kimlik;
        x.sinif = 0;
        hedefler.push_back(e.tanim.hedef);
        x.hedef = hedefler.back();
        turler.emplace_back(ariza_turu_adi(e.tanim.tur));
        x.tur = turler.back();
        x.kaynak = 0;
        x.baslangic_adimi = static_cast<std::uint64_t>(e.t_baslangic_ns / kH);
        x.bitis_adimi = e.t_bitis_ns == std::numeric_limits<std::int64_t>::max() ? kYok64
                                                                                 : static_cast<std::uint64_t>(e.t_bitis_ns / kH);
        v.push_back(x);
    }
    for (const IticiAriza& a : itici_) {
        a3::yuk_ariza_etkin_arizalar x;
        x.fault_id = a.kimlik;
        x.sinif = 1;
        std::string h;
        for (int c = 0; c < 8; ++c) {
            if ((a.maske & (1u << c)) != 0u) {
                h += (h.empty() ? "t" : ",t") + std::to_string(c + 1);
            }
        }
        hedefler.push_back(h);
        x.hedef = hedefler.back();
        turler.emplace_back("itki_carpani");
        x.tur = turler.back();
        x.kaynak = 0;
        x.baslangic_adimi = static_cast<std::uint64_t>(a.baslangic_ns / kH);
        x.bitis_adimi = a.bitis_ns == std::numeric_limits<std::int64_t>::max() ? kYok64 : static_cast<std::uint64_t>(a.bitis_ns / kH);
        v.push_back(x);
    }
    a3::yuk_ariza_etkin y;
    y.arizalar = v;
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    a3_olc([&] { (void)yay_->yayinla<a3::Kanal::ariza_etkin>(t, y); });
}

// Durum kanalları (10 Hz ve değişimde): calistirma/durum, sayac/gercek_zaman, emniyet/girdiler,
// atis/durum, baglanti/durum.
void Calistirici::a3_durum_yayinla(std::int64_t t)
{
    if (yay_ == nullptr) {
        return;
    }
    a3_durum_kirli_ = false;
    a3::yuk_calistirma_durum c;
    c.durum = static_cast<std::uint8_t>(a3d_.durum);
    c.kip = static_cast<std::uint8_t>(a_.kip == Kip::hil ? a3::ZamanKipi::hil_1x : a3::ZamanKipi::sil);
    c.run_id = a3d_.run_id ? std::string_view(*a3d_.run_id) : std::string_view();
    c.adim_no = a3_b_;
    c.hedef_carpan = a_.kip == Kip::hil ? 1.0F : static_cast<float>(hiz_);
    const std::int64_t duvar_simdi = gercek();
    if (duraklatildi_) {
        a3_gerceklesen_ = 0.0F;
        a3_carpan_ref_duvar_ = duvar_simdi;
        a3_carpan_ref_sim_ = t;
    } else if (duvar_simdi - a3_carpan_ref_duvar_ >= 1'000'000'000) {
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
    c.tekrar_guvencesi = 0;   // kayıt (A6) yok: güvence yok
    c.kademe_gorunum = 0;
    c.kademe_kamera = 0;
    c.kademe_sonar = 0;
    c.kademe_kayit = 0;
    c.a1_oturum = k_ ? a1_oturum_kodu(k_->durum(), !tas_ && a_.kip == Kip::hil && a3b::canli(a3d_.durum)) : 255u;
    c.son_komut_no = a3_son_komut_no_;   // dünyaya ulaşan en büyük komut numarası (okuma; rapor)
    c.durum_rev = a3d_.durum_rev;
    a3_olc([&] { (void)yay_->yayinla<a3::Kanal::calistirma_durum>(t, c); });
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
    g.donma_onceki_cikti_yok = ks.donma_onceki_cikti_yok;
    g.atama_celiskisi = s_.atama_celiskileri.size();
    g.gevsetme_reddedildi = s.gevsetme_reddedildi;
    g.komut_bayat_yetki = s.komut_bayat_yetki;
    g.a3_halka_tasma = yay_->halka_tasma();
    a3_olc([&] { (void)yay_->yayinla<a3::Kanal::sayac_gercek_zaman>(t, g); });

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
    a3_olc([&] { (void)yay_->yayinla<a3::Kanal::emniyet_girdiler>(t, eg); });

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
    a3_olc([&] { (void)yay_->yayinla<a3::Kanal::atis_durum>(t, ad); });

    a3::yuk_baglanti_durum bd;
    bd.a1_oturum = c.a1_oturum;
    bd.sess = k_->durum() == kopru::OturumDurumu::kosuyor ? k_->sess() : 0u;
    bd.acilis_sayaci = kb.boot_count;
    // d_asagi_ort_ns: bağlantı emülatörü yok → yok (INT64_MIN, varsayılan); emulator: boş.
    a3_olc([&] { (void)yay_->yayinla<a3::Kanal::baglanti_durum>(t, bd); });
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
            a3_olc([&] { (void)yay_->yayinla<a3::Kanal::sensor_basinc>(t, y); });
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
            a3_olc([&] { (void)yay_->yayinla<a3::Kanal::sensor_dvl>(t, y); });
        } else if (const auto* p = std::get_if<sensor::BataryaOlcumu>(&o.yuk)) {
            a3::yuk_batarya y;
            olcum_basligi(y, o.hdr);
            y.gerilim_v = p->voltage;
            y.akim_a = p->current;
            a3_olc([&] { (void)yay_->yayinla<a3::Kanal::sensor_batarya>(t, y); });
        }
    }
}

// Yayın noktası (dünya iş parçacığı; fizik sınırındaki sıranın sonu). `zorla`: duraklatma
// yoklaması (sensör kaydı yok; durum kanalları yinelenir).
void Calistirici::a3_yayinla(std::uint64_t b, bool zorla)
{
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    a3_olay_kararlari(b);
    a3_yi_izle();
    a3_oturum_izle(b);
    a3_gecerlilik(b);
    if (!zorla) {
        a3_sensor_yayinla();
    }
    if (b % kHizliSinir == 0u || zorla) {
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
        a3_olc([&] { (void)yay_->yayinla<a3::Kanal::fizik_durum>(t, f); });

        const kopru::KartBilgisi& kb = k_->kart();
        const bool yeni_nav = kb.nav.var && (!a3_nav_var_ || kb.nav.batch_seq != a3_nav_seq_ || kb.nav.t_ns != 0);
        if ((kb.nav.var || kb.durum.var) && (yeni_nav || kb.durum.batch_seq != a3_durum_seq_ || zorla)) {
            a3_nav_var_ = kb.nav.var;
            a3_nav_seq_ = kb.nav.batch_seq;
            a3_durum_seq_ = kb.durum.batch_seq;
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
            a3_olc([&] { (void)yay_->yayinla<a3::Kanal::kart_durum>(t, k); });
        }
        a3_ariza_izle(b);
    }
    if (b % kDurumSinir == 0u || zorla || a3_durum_kirli_) {
        a3_durum_yayinla(t);
    }
    if (b % kYavasSinir == 0u || zorla) {
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
            a3_olc([&] { (void)yay_->yayinla<a3::Kanal::kart_cekirdek_sayaclari>(t, y); });
        }
    }
    // A3-K72: kaçırılan partiler en çok saniyede bir özetlenir (duvar).
    if (a3_kacirma_adet_ > 0u && gercek() - a3_kacirma_olay_duvar_ >= 1'000'000'000) {
        a3b::JsonYaz j;
        j.sayi_u("ilk_step_seq", a3_kacirma_ilk_).sayi_u("son_step_seq", a3_kacirma_son_).sayi_u("adet", a3_kacirma_adet_);
        a3_olay(4, 1, 0, 0, 0, j.bitir());
        a3_kacirma_adet_ = 0;
        a3_kacirma_olay_duvar_ = gercek();
    }
    if (b % kDurumSinir == 0u || a3_anlik_kirli_ || zorla) {
        a3_anlik_guncelle();
    }
    s_.a3_sinir_en_kotu_ns = std::max(s_.a3_sinir_en_kotu_ns, a3_sinir_ns_);
}

// Koşu sonu: kalan komutlar reddedilir, son durum yayınlanır, sunucu düzgün durur (A3-K49).
void Calistirici::a3_bitir()
{
    if (yay_ == nullptr) {
        return;
    }
    a3_cek();
    for (const a3::Komut& k : a3_kisit_) {
        a3_sonuc(k, false, a3_b_, "canli_calistirma_yok");
    }
    for (const a3::Komut& k : a3_genel_) {
        a3_sonuc(k, false, a3_b_, a3b::calistirma_mi(k.tur) ? "gecersiz_durum" : "canli_calistirma_yok");
    }
    a3_kisit_.clear();
    a3_genel_.clear();
    if (k_) {
        a3_yi_izle();
        a3_oturum_izle(a3_b_);
    }
    a3_durum_yayinla(static_cast<std::int64_t>(a3_b_) * kH);
    a3_anlik_guncelle();
    s_.a3_halka_tasma = yay_->halka_tasma();
    if (a_.a3_kapanis_bekleme_s > 0.0) {
        // Son çerçevelerin yayın tikinde (A3-K33, 20 ms) gönderilmesi için.
        const auto son = Saat::now() + std::chrono::milliseconds(static_cast<std::int64_t>(a_.a3_kapanis_bekleme_s * 1000.0));
        while (Saat::now() < son) {
            a3_cek();
            for (const a3::Komut& k : a3_kisit_) {
                a3_sonuc(k, false, a3_b_, "canli_calistirma_yok");
            }
            for (const a3::Komut& k : a3_genel_) {
                a3_sonuc(k, false, a3_b_, a3b::calistirma_mi(k.tur) ? "gecersiz_durum" : "canli_calistirma_yok");
            }
            a3_kisit_.clear();
            a3_genel_.clear();
            sure_uyu(5 * kMs);
        }
    }
    a3sv_->durdur();
    yay_ = nullptr;
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

    // ---- A3 sunucusu (W4-bagla-a3): yalnız --a3-port verildiyse ----
    if (a_.a3_acik) {
        if (!a3_baslat()) {
            return s_;
        }
        if (a_.a3_bekle && !a3_baslat_bekle()) {
            return s_;
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
    if (yay_ != nullptr) {
        // A3-K62, K63: çalıştırma başladı (baslat ya da kendiliğinden); ön denetim A1 el sıkışmasıdır.
        a3d_.run_id = a3_run_id_;
        a3d_.a1_run_id = k_->run_id();
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

    if (a_.kip == Kip::hil) {
        zamanlayici_onceligi();   // HIL 1x: duvar saatine kilitli döngü
    }
    bas_ = Saat::now();
    if (!oturum_kur()) {
        s_.sayac = k_->sayaclar();
        s_.kart = k_->kart();
        if (yay_ != nullptr) {
            // A3-K62: ön denetim kaldı → yuklendi; `baslat` reddedildi (on_denetim_kaldi).
            a3_gecis(a3::CalistirmaDurumu::yuklendi, "on_denetim_kaldi", true);
            if (a3_baslat_komutu_) {
                a3_sonuc(*a3_baslat_komutu_, false, 0, "on_denetim_kaldi");
                a3_baslat_komutu_.reset();
            }
            a3_bitir();
        }
        return s_;
    }
    if (yay_ != nullptr) {
        a3_gecis(a3::CalistirmaDurumu::kosuyor, "on_denetim_gecti", true);
        if (a3_baslat_komutu_) {
            a3_sonuc(*a3_baslat_komutu_, true, 0, {});
            a3_baslat_komutu_.reset();
        }
        a3_oturum_izle(0);
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
    std::vector<std::int64_t> tf_yazim;
    tf_yazim.reserve(static_cast<std::size_t>(n_sinir / 4U + 1U));
    if (a_.kip == Kip::hil) {
        profil_.reserve(static_cast<std::size_t>(n_sinir + 1U));
        tf_gec_.reserve(static_cast<std::size_t>(n_sinir / 4U + 1U));
    }

    for (std::uint64_t b = 0; b <= n_sinir; ++b) {
        const std::int64_t t_b = static_cast<std::int64_t>(b) * kH;
        // W4-bagla-a3: çalıştırma denetimi (duraklat, sürdür, adımla, hız, durdur) sınırın başında;
        // duraklatılmışken sim zamanı burada durur (emniyet istekleri beklemez: A1-K85, A3-K58).
        if (yay_ != nullptr && !a3_sinir_basi(b)) {
            break;
        }
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
        if (yay_ != nullptr) {
            // Yayın noktası: fizik sınırındaki sıranın sonu (köprü, sensörler, parti, yuva), fizik
            // adımından önce; durum b sınırınındır (CSV satırıyla aynı an). Sırayı değiştirmez.
            a3_yayinla(b, false);
        }
        const std::int64_t e_f = gercek();
        if (dn_.adim() != fizik::AdimDurumu::tamam) {
            s_.hata = "fizik sayısal hata";
            break;
        }
        adim_sonrasi(b);
        if (adimla_kalan_ > 0u && --adimla_kalan_ == 0u) {
            duraklatildi_ = true;   // A3-K65: n periyot ilerledi
            a3_gecis(a3::CalistirmaDurumu::duraklatildi, "adimla_bitti", true);
        }
        if (a_.kip == Kip::hil) {
            evre_[kEvreFizik] += gercek() - e_f;
            profil_.push_back(evre_);
        }
    }
    if (yay_ != nullptr) {
        // Tablo 4.7a iç geçişleri: süre doldu → bitti; çalıştırmayı durduran hata → hata
        // (durdur komutu durumu zaten `durduruldu` yaptı).
        if (!a3_durdur_) {
            a3_gecis(s_.hata.empty() ? a3::CalistirmaDurumu::bitti : a3::CalistirmaDurumu::hata,
                     s_.hata.empty() ? "sure_doldu" : "calistirma_hatasi", true);
        }
        s_.calistirma_sonu = a3d_.durum == a3::CalistirmaDurumu::bitti        ? "bitti"
                             : a3d_.durum == a3::CalistirmaDurumu::durduruldu ? "durduruldu"
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
