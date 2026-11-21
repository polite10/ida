// Başsız sim sunucusu: fizik + sensör kümesi + A1 köprüsü + zaman yöneticisi (sunucu.hpp).
#include "ida/sunucu/sunucu.hpp"

#include "ida/fizik/dunya.hpp"
#include "ida/kopru/karsi.hpp"
#include "ida/kopru/param_blogu.hpp"
#include "ida/kopru/tasima.hpp"
#include "ida/sensor/sensor_kumesi.hpp"
#include "yardimci.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <functional>
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
constexpr std::uint8_t kCmdHeartbeat = 1, kCmdArm = 2, kCmdDisarm = 3, kCmdSetMode = 4, kCmdParamCommit = 13,
                       kCmdParamSave = 14;
constexpr std::uint16_t kModGuided = 6;
constexpr std::uint8_t kSpDepth = 2, kSpYaw = 32;
constexpr std::uint16_t kActThr = 1, kActSafe = 2, kActArmed = 4;
constexpr std::uint8_t kAckAccepted = 1;
constexpr std::uint8_t kPstoreOk = 2;
constexpr std::uint8_t kCloseRunEnd = 5, kCloseShutdown = 1;

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

struct Olay {
    std::int64_t t;
    std::function<void()> f;
};

class Calistirici {
public:
    explicit Calistirici(const Ayar& a) : a_(a), sekil_(fizik::ida0_arac().itici_modeli.sekil_polinomu) {}
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
    void yaz()
    {
        k_->giden_al(tampon_);
        if (!tampon_.empty() && tas_ && tas_->acik()) {
            (void)tas_->yaz(tampon_.data(), tampon_.size());
        }
    }

    // Eldeki baytları köprüye verir; ilk okuma en çok bekle_us bekler.
    void pompa(std::int64_t bekle_us)
    {
        if (ic_ && a_.kip == Kip::sil) {
            (void)ic_->pompala();
            bekle_us = 0;
        }
        if (!tas_) {
            return;
        }
        alim_.resize(16384);   // yığında değil (MSVC C6262)
        for (;;) {
            std::size_t n = 0;
            const kopru::OkumaSonucu r = tas_->oku(alim_.data(), alim_.size(), bekle_us, n);
            bekle_us = 0;
            if (r == kopru::OkumaSonucu::tamam) {
                k_->al(alim_.data(), n, duvar());
                if (ic_ && a_.kip == Kip::sil && k_->giden_var()) {
                    yaz();
                    (void)ic_->pompala();
                }
                continue;
            }
            if (r == kopru::OkumaSonucu::kapandi) {
                k_->akis_kapandi(duvar());
                tas_.reset();
            }
            break;
        }
    }

    // HIL: hedef duvar anına kadar okuyup bekler.
    void bekle_ve_oku(std::int64_t hedef)
    {
        for (;;) {
            const std::int64_t simdi = gercek();
            k_->zamanla(simdi);
            yaz();
            if (simdi >= hedef) {
                return;
            }
            const std::int64_t kalan_us = (hedef - simdi) / 1000;
            if (kalan_us > 300 && tas_) {
                pompa(std::min<std::int64_t>(kalan_us - 200, 2000));
            } else if (kalan_us > 0) {
                pompa(0);
                std::this_thread::yield();
            } else {
                pompa(0);
            }
        }
    }

    bool baglanti_kabul();
    bool oturum_kur();
    bool hazirlik();
    bool yuva_bekle(std::uint32_t seq);
    void senaryo_kur();
    bool sinir_on(std::uint64_t b);
    void adim_sonrasi(std::uint64_t b);
    void satir_ekle(std::uint64_t b, std::uint32_t seq);
    void satir_tamamla(Satir& r);
    void csv_yaz(const Satir& r);

    const Ayar& a_;
    Sonuc s_;
    SekilTersi sekil_;
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
    std::vector<std::uint8_t> alim_;
    std::vector<sensor::Olcum> olcumler_;
    std::vector<Olay> olaylar_;
    std::size_t olay_i_ = 0;
    std::array<double, 8> pwm_yur_{};
    std::array<double, 8> son_pwm_yur_{};
    bool estop_bu_adim_ = false;
    bool onceki_estop_ = false;       // satırın adımının (t − h, t] acil durdurma durumu
    std::uint32_t id_arm_ = 0, id_kip_ = 0, id_sp_ = 0, id_sp2_ = 0, id_disarm_ = 0;
    std::int64_t t_disarm_istek_ = -1;
    bool satir_bekliyor_ = false;
    std::int64_t son_tf_yazim_ = -1;
    std::int64_t w0_ = 0;
};

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
    gunluk(2, "bağlantı: " + tas_->ad());
    return true;
}

bool Calistirici::oturum_kur()
{
    const std::int64_t son = gercek() + a_.baglanti_bekleme_ns;
    if (!tas_ && !baglanti_kabul()) {
        return false;
    }
    k_->el_sikisma_baslat(duvar());
    for (;;) {
        yaz();
        pompa(500);
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
        const std::uint32_t seq = k_->parti_kapat(t_h, duvar());
        yaz();
        if (seq == 0u) {
            s_.hata = "hazırlık oturumu koptu";
            return false;
        }
        if (a_.kip == Kip::sil) {
            if (!yuva_bekle(seq)) {
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
    for (;;) {
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
                tas_->kapat();
                tas_.reset();
            }
            ++s_.yeniden_baglanti;
            return oturum_kur();
        }
    }
}

void Calistirici::senaryo_kur()
{
    auto ns = [](double t) { return static_cast<std::int64_t>(std::llround(t * 1e9)); };
    if (a_.acil_durdurma_s >= 0.0) {
        olaylar_.push_back({ns(a_.acil_durdurma_s), [this] {
                                kopru::EmniyetAtama e;
                                e.acil_durdurma = true;
                                k_->emniyet_ata(e);
                            }});
    }
    if (a_.acil_kalkar_s >= 0.0) {
        olaylar_.push_back({ns(a_.acil_kalkar_s), [this] {
                                kopru::EmniyetAtama e;
                                e.acil_durdurma = false;
                                k_->emniyet_ata(e);
                            }});
    }
    if (a_.senaryo == "z1") {
        const Z1& z = a_.z1;
        for (double t = z.hb_ilk_s; t < a_.sure_s; t += 1.0) {
            if (z.t_hb_kes_s >= 0.0 && t >= z.t_hb_kes_s) {
                break;
            }
            const std::int64_t tn = ns(t);
            olaylar_.push_back({tn, [this, tn] { (void)k_->komut_basit(kCmdHeartbeat, 0, tn); }});
        }
        const std::int64_t ta = ns(z.t_arm_s), tk = ns(z.t_kip_s), ts = ns(z.t_sp_s), tm = ns(z.t_manevra_s),
                           td = ns(z.t_disarm_s);
        olaylar_.push_back({ta, [this, ta] { id_arm_ = k_->komut_basit(kCmdArm, 0, ta); }});
        olaylar_.push_back({tk, [this, tk] { id_kip_ = k_->komut_basit(kCmdSetMode, kModGuided, tk); }});
        olaylar_.push_back({ts, [this, ts] {
                                kopru::GuidedHedef h;
                                h.mask = static_cast<std::uint8_t>(kSpDepth | kSpYaw);
                                h.depth = static_cast<float>(a_.z1.hedef_derinlik_m);
                                h.yaw = static_cast<float>(a_.z1.hedef_yon_rad);
                                id_sp_ = k_->komut_guided(h, ts);
                            }});
        olaylar_.push_back({tm, [this, tm] {
                                kopru::GuidedHedef h;
                                h.mask = static_cast<std::uint8_t>(kSpDepth | kSpYaw);
                                h.depth = static_cast<float>(a_.z1.hedef2_derinlik_m);
                                h.yaw = static_cast<float>(a_.z1.hedef2_yon_rad);
                                id_sp2_ = k_->komut_guided(h, tm);
                            }});
        olaylar_.push_back({td, [this, td] {
                                id_disarm_ = k_->komut_basit(kCmdDisarm, 0, td);
                                t_disarm_istek_ = td;
                            }});
    }
    std::stable_sort(olaylar_.begin(), olaylar_.end(), [](const Olay& x, const Olay& y) { return x.t < y.t; });
}

// Sınır b: senaryo, köprünün emniyet ve sürekli çıktı işlemleri, acil durdurma engeli, arıza,
// itici komutu, sensörler. false: sensör ya da fizik hatası.
bool Calistirici::sinir_on(std::uint64_t b)
{
    const std::int64_t t = static_cast<std::int64_t>(b) * kH;
    while (olay_i_ < olaylar_.size() && olaylar_[olay_i_].t <= t) {
        olaylar_[olay_i_].f();
        ++olay_i_;
    }
    const kopru::SinirSonucu ss = k_->sinir_isle(b, duvar());
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
    } else if (a_.senaryo == "z1" && t >= static_cast<std::int64_t>(std::llround(a_.z1.t_manevra_s * 1e9))) {
        for (int kanal : a_.z1.ariza_kanallari) {
            const auto c = static_cast<std::size_t>(kanal);
            pwm[c] = sekil_.olcekle(pwm[c], a_.z1.ariza_carpani, notr, 400.0,
                                    static_cast<double>(arac_.esc.olu_bant_us));
        }
    }
    (void)dn_.itici_komutu_ver(b, pwm);
    kume_.dis_durum_ver(k_->dis_durum());
    if (kume_.adim(dn_.durum(), b) != sensor::AdimSonucu::tamam) {
        s_.hata = "sensör kümesi adımı başarısız";
        return false;
    }
    olcumler_.clear();
    kume_.bosalt(t, olcumler_);
    for (const sensor::Olcum& o : olcumler_) {
        k_->olcum_ekle(o);
    }
    return true;
}

void Calistirici::adim_sonrasi(std::uint64_t b)
{
    const fizik::DurumAnlik& d = dn_.durum();
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

void Calistirici::satir_ekle(std::uint64_t b, std::uint32_t seq)
{
    if (satir_bekliyor_) {
        satir_tamamla(s_.satirlar.back());
    }
    const fizik::DurumAnlik& d = dn_.durum();
    Satir r;
    r.t_s = static_cast<double>(b) * 1.25e-3;
    r.parti = seq;
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
    s_.satirlar.push_back(r);
    satir_bekliyor_ = true;
}

// Kart tarafı alanları bir parti sonra doldurulur (yanıt ve telemetri o ana kadar gelmiştir;
// SIL'de veri sırasına bağlı ve belirlenimci).
void Calistirici::satir_tamamla(Satir& r)
{
    satir_bekliyor_ = false;
    const auto y = r.parti != 0u ? k_->yanit(r.parti) : std::nullopt;
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

Sonuc Calistirici::kos()
{
    // ---- Fizik: İDA-0, örnek havuz, düz suda yüzer denge ----
    arac_ = fizik::ida0_arac();
    const fizik::OrtamParametreleri ortam = fizik::havuz_ortami();
    fizik::BaslangicDurumu bas;
    fizik::KurulumSonucu kr = dn_.kur(arac_, ortam, bas);
    if (kr.tamam) {
        bas.konum[2] = dn_.turetilmis().denge_konumu_z;
        kr = dn_.kur(arac_, ortam, bas);
    }
    if (!kr.tamam) {
        s_.hata = "fizik kurulumu: " + kr.hata;
        return s_;
    }
    s_.denge_derinligi_m = bas.konum[2];
    pwm_yur_.fill(static_cast<double>(arac_.esc.pwm_notr_us));
    son_pwm_yur_ = pwm_yur_;

    // ---- Sensörler ----
    sensor::SensorKumesiAyari sa = sensor::ida0_sensor_kumesi();
    sa.ana_tohum = a_.tohum;
    sa.guc = sensor::guc_parametreleri(arac_);
    const sensor::KurulumSonucu ks = kume_.kur(sa);
    if (!ks.tamam) {
        s_.hata = "sensör kümesi: " + ks.hata;
        return s_;
    }

    // ---- Parametre bloğu (A5'ten; A1-K18): havuzun yerçekimi, kalibrasyondaki O_b derinliği ----
    pb_ = kopru::parametre_blogu({{"YER_G", ortam.yercekimi}, {"BARO_KAL_D", s_.denge_derinligi_m}});
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

    // ---- Taşıma ve karşı taraf ----
    std::string hata;
    switch (a_.karsi) {
    case Karsi::ic: {
        auto [x, y] = kopru::bellek_cifti();
        tas_ = std::move(x);
        kopru::KarsiAyari kk;
        kk.a1_tohum = k_->board_tohumu();
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

    senaryo_kur();
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

    const auto n_sinir = static_cast<std::uint64_t>(std::llround(a_.sure_s * 1e9)) / static_cast<std::uint64_t>(kH);
    if (a_.csv_uret) {
        s_.csv = csv_basligi();
    }
    s_.satirlar.reserve(static_cast<std::size_t>(n_sinir / 4U + 1U));
    w0_ = gercek() + 50 * kMs;   // HIL: W(t) = W0 + t
    std::vector<std::int64_t> tf_yazim;
    tf_yazim.reserve(static_cast<std::size_t>(n_sinir / 4U + 1U));

    for (std::uint64_t b = 0; b <= n_sinir; ++b) {
        const std::int64_t t_b = static_cast<std::int64_t>(b) * kH;
        if (a_.kip == Kip::sil) {
            sil_duvar_ = t_b;
            pompa(0);
            k_->zamanla(duvar());
        } else {
            bekle_ve_oku(w0_ + t_b - kCSim);
            // A1-K108: uygulama anları a_n kontrol sınırlarıdır (L_act = 2P).
            if (b % kopru::kPeriyotAdim == 0 && gercek() > w0_ + t_b) {
                ++s_.zaman.sim_gecikmesi;
            }
        }
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
            }
            if (id_disarm_ != 0u && s_.t_disarm_ns < 0) {
                s_.t_disarm_ns = t_b;   // DISARM'ı taşıyan partinin kapanışı
            }
            const std::int64_t yazim = gercek();
            seq = k_->parti_kapat(t_b, duvar(), a_.kip == Kip::hil ? w0_ + t_b : -1);
            yaz();
            if (seq != 0u) {
                tf_yazim.push_back(yazim);
                son_tf_yazim_ = yazim;
                if (a_.kip == Kip::hil) {
                    s_.zaman.gec_yazim_en_cok_ns = std::max(s_.zaman.gec_yazim_en_cok_ns, yazim - (w0_ + t_b));
                }
            }
            satir_ekle(b, seq);
            if (a_.kip == Kip::sil && seq != 0u && !yuva_bekle(seq)) {
                break;
            }
        } else {
            yaz();
        }
        if (dn_.adim() != fizik::AdimDurumu::tamam) {
            s_.hata = "fizik sayısal hata";
            break;
        }
        adim_sonrasi(b);
    }
    // Son satırın kart tarafı ve planlı kapanış (A1-K21).
    if (a_.kip == Kip::sil) {
        pompa(0);
    } else {
        bekle_ve_oku(gercek() + 20 * kMs);
    }
    if (satir_bekliyor_) {
        satir_tamamla(s_.satirlar.back());
    }
    k_->oturum_kapat(kCloseShutdown, duvar());
    yaz();
    pompa(0);
    if (tas_) {
        tas_->kapat();
    }
    dur_ = true;
    if (karsi_is_.joinable()) {
        karsi_is_.join();
    }
    if (surec_) {
        surec_->sonlandir();
    }

    // ---- Sonuç ----
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
        z.rx_crc_kosu = s_.sayac.rx_crc_err - rx_bas.rx_crc_err;
        z.rx_resync_kosu = s_.sayac.rx_resync_bytes - rx_bas.rx_resync_bytes;
        z.rx_seq_gap_kosu = s_.sayac.rx_seq_gap - rx_bas.rx_seq_gap;
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
