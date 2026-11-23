// W4-K15 (EMNİYET; üçüncü dar sol denetimi, docs/gorevler/inceleme/W4-kopru-sol-3.md): olay
// kümesi sayıyla kapanır; sayılmamış genişletici olay sayım ihlalidir; hata dönüşü beklemez;
// geçmişten atılmış revizyonda emniyet örneği güncel yetkili durumla birleştirilir.
#include "test_destek.hpp"

using namespace ida::kopru;
using namespace ida::kopru::test;

namespace {

ida::sensor::Olcum emniyet_kaydi(std::uint32_t seq, std::int64_t t, std::uint8_t estop, std::uint8_t t0,
                                 std::uint8_t t1, std::uint32_t rev)
{
    ida::sensor::Olcum o;
    o.hdr.tur = ida::sensor::SensorTuru::firlatici_geri_okuma;
    o.hdr.seq = seq;
    o.hdr.t_meas_ns = t;
    o.hdr.flags = ida::sensor::kBayrakGecerli;
    ida::sensor::EmniyetOlcumu e;
    e.estop = estop;
    e.tube = {t0, t1};
    e.safety_rev = rev;
    o.yuk = e;
    return o;
}

constexpr std::uint8_t kL = static_cast<std::uint8_t>(IDAW_TUBE_FLAGS_LOADED);
constexpr std::uint8_t kF = static_cast<std::uint8_t>(IDAW_TUBE_FLAGS_PLUG_IN);
constexpr std::uint8_t kPo = static_cast<std::uint8_t>(IDAW_TUBE_FLAGS_PIN_OUT);
constexpr std::uint8_t kI = static_cast<std::uint8_t>(IDAW_TUBE_FLAGS_PERMIT_OPEN);
constexpr std::uint16_t kThr = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_THR_VALID);
constexpr std::uint16_t kArmed = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_ARMED);
constexpr std::uint16_t kSafe = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_IS_SAFE);

std::array<std::uint16_t, 8> pwm_hepsi(std::uint16_t v)
{
    std::array<std::uint16_t, 8> p{};
    p.fill(v);
    return p;
}

void gevsek_hazirla(SahteDuzenek& z)
{
    EmniyetAtama a;
    a.fis = {true, true};
    a.pim = {false, false};
    z.k.emniyet_ata(a);
    (void)z.periyot();
}

std::uint8_t sonuc_bul(const SahteKart& sk, std::uint64_t id)
{
    for (const auto& r : sk.sonuclar) {
        if (r.event_id == id) {
            return r.result;
        }
    }
    return 0;
}

std::vector<std::uint8_t> kes(SahteKart& sk, std::size_t n)
{
    std::vector<std::uint8_t> v(sk.giden.begin() + static_cast<std::ptrdiff_t>(n), sk.giden.end());
    sk.giden.resize(n);
    return v;
}

bool protokol_kapanisi(const SahteKart& sk)
{
    for (std::uint8_t r : sk.kapanis_nedenleri) {
        if (r == IDAW_CLOSE_REASON_PROTOCOL) {
            return true;
        }
    }
    return false;
}

KopruAyari hil()
{
    KopruAyari ka;
    ka.kip = CalistirmaKipi::hil_1x;
    return ka;
}

}  // namespace

// (5), sol-3 #1: (rev = R, PERMIT_OPEN = 1); R'de izin hiç açılmadı ve R 256 artışla geçmişten
// atıldı; kalan revizyonlarda da izin kapalı. Örnek olduğu gibi gönderilmez: güncel yetkili
// durumla birleştirilir (izin biti kapanır) ve güncel revizyonla gider.
int test_emniyet_gecmis_tahliyesi()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t r = z.k.dis_durum().emniyet.emniyet_revizyonu;
    z.sinir_kancasi = [&](std::uint64_t b) {
        EmniyetAtama a;
        a.acil_durdurma = (b % 2u) == 0u;
        z.k.emniyet_ata(a);
    };
    for (int n = 0; n < 66; ++n) {   // 264 sınır: her sınırda bir revizyon
        (void)z.periyot();
    }
    z.sinir_kancasi = nullptr;
    // Son atama: bırak (gevşek son durum).
    EmniyetAtama a;
    a.acil_durdurma = false;
    z.k.emniyet_ata(a);
    (void)z.periyot();
    const std::uint32_t guncel = z.k.dis_durum().emniyet.emniyet_revizyonu;
    DOGRULA(d, guncel > r + 256u && !z.k.acil_durdurma(), "R geçmişten atıldı (R+" + s(guncel - r) + ")");
    const std::size_t bas = z.sk.emniyet.size();
    const std::uint8_t g = static_cast<std::uint8_t>(kL | kF | kPo);
    (void)z.periyot({emniyet_kaydi(70, z.t + 1'000'000, 0, static_cast<std::uint8_t>(g | kI), g, r)});
    DOGRULA(d, z.sk.emniyet.size() == bas + 1u, "bir emniyet üyesi");
    if (z.sk.emniyet.size() == bas + 1u) {
        const auto& p = z.sk.emniyet[bas];
        DOGRULA(d, (p.tube[0] & kI) == 0u, "açılmamış izin karta açık bildirilmedi (W4-K12 (2))");
        DOGRULA(d, p.safety_rev == guncel, "güncel revizyonla gönderildi (R+" + s(p.safety_rev - r) + ")");
    }
    return d.hata;
}

// (3), sol-3 #2: SAFETY_OPEN ve n_events = 1, IS_SAFE = 1 yanıtı kabul edildi; aynı partinin
// sayılmamış FIRE'ı gelir: sayım ihlali, oturum PROTOCOL ile biter, izin açılmaz.
int test_sayim_ihlali_sonradan_fire()
{
    Denetci d;
    SahteDuzenek z(hil());
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    std::uint32_t hedef = 0;
    std::int64_t t_hedef = 0;
    bool bir_kez = true;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        if (bir_kez) {
            bir_kez = false;
            hedef = tf.seq;
            t_hedef = tf.t;
            (void)z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, tf.seq, tf.t, rev);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kSafe | kArmed), pwm_hepsi(1500), 1);
            return;
        }
        z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600));
    };
    bool hic_acik = false;
    z.sinir_kancasi = [&](std::uint64_t) { hic_acik = hic_acik || z.k.izin_acik(0); };
    (void)z.periyot();
    const std::uint64_t ates = z.sk.olay(IDAW_EVENT_KIND_FIRE, 0, hedef, t_hedef, rev);
    for (int n = 0; n < 4; ++n) {
        (void)z.periyot();
    }
    hic_acik = hic_acik || z.k.izin_acik(0);
    DOGRULA(d, z.k.sayaclar().olay_sayim_ihlali == 1u, "sayım ihlali (" + s(z.k.sayaclar().olay_sayim_ihlali) + ")");
    DOGRULA(d, protokol_kapanisi(z.sk), "IDA_SESSION_CLOSE(PROTOCOL)");
    DOGRULA(d, !hic_acik && z.k.tup_dolu(0), "izin açılmadı, tüp dolu");
    DOGRULA(d, sonuc_bul(z.sk, ates) != IDA_EVR_APPLIED, "FIRE uygulanmadı");
    return d.hata;
}

// (3), sol-3 #3: küçük bildirilen sayı ile parçalı teslim. SAFETY_OPEN gelir, FIRE kaybolur,
// yanıt n_events = 1 bildirir: küme tam ve tutarlı görünür, izin a_n'de açılabilir. FIRE
// yinelemesi +15 ms'de gelince sayım ihlali: oturum biter, oturum sonu izni kapatır; FIRE
// uygulanmaz.
int test_sayim_ihlali_kucuk_sayi()
{
    Denetci d;
    SahteDuzenek z(hil());
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    std::uint64_t ates = 0;
    std::uint32_t hedef = 0;
    std::int64_t t_hedef = 0;
    bool bir_kez = true;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        if (bir_kez) {
            bir_kez = false;
            hedef = tf.seq;
            t_hedef = tf.t;
            (void)z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, tf.seq, tf.t, rev);
            const std::size_t n = z.sk.giden.size();
            ates = z.sk.olay(IDAW_EVENT_KIND_FIRE, 0, tf.seq, tf.t, rev);
            (void)kes(z.sk, n);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kSafe | kArmed), pwm_hepsi(1500), 1);
            return;
        }
        z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600));
    };
    for (int n = 0; n < 4; ++n) {   // a_n işlendi
        (void)z.periyot();
    }
    (void)z.sk.olay(IDAW_EVENT_KIND_FIRE, 0, hedef, t_hedef, rev, 1, ates);   // yineleme
    for (int n = 0; n < 2; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, z.k.sayaclar().olay_sayim_ihlali == 1u, "sayım ihlali (" + s(z.k.sayaclar().olay_sayim_ihlali) + ")");
    DOGRULA(d, protokol_kapanisi(z.sk), "IDA_SESSION_CLOSE(PROTOCOL)");
    DOGRULA(d, !z.k.izin_acik(0), "oturum sonu izni kapattı");
    DOGRULA(d, z.k.tup_dolu(0) && sonuc_bul(z.sk, ates) != IDA_EVR_APPLIED, "FIRE uygulanmadı, tüp dolu");
    return d.hata;
}

// (3): yanıt gelmeden alınmış iki genişletici olay, yanıt n_events = 1: sayıyı aşan genişletici
// olay sayım ihlalidir; hiçbiri uygulanmaz.
int test_sayim_ihlali_yanittan_once()
{
    Denetci d;
    SahteDuzenek z(hil());
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    bool bir_kez = true;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        if (bir_kez) {
            bir_kez = false;
            (void)z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, tf.seq, tf.t, rev);
            (void)z.sk.olay(IDAW_EVENT_KIND_FIRE, 0, tf.seq, tf.t, rev);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600), 1);
            return;
        }
        z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600));
    };
    bool hic_acik = false;
    z.sinir_kancasi = [&](std::uint64_t) { hic_acik = hic_acik || z.k.izin_acik(0); };
    for (int n = 0; n < 5; ++n) {
        (void)z.periyot();
    }
    hic_acik = hic_acik || z.k.izin_acik(0);
    DOGRULA(d, z.k.sayaclar().olay_sayim_ihlali == 1u, "sayım ihlali");
    DOGRULA(d, protokol_kapanisi(z.sk), "IDA_SESSION_CLOSE(PROTOCOL)");
    DOGRULA(d, !hic_acik && z.k.tup_dolu(0), "izin açılmadı, tüp dolu");
    return d.hata;
}

// A1-K81: tam yuvaya aynı event_id'li yineleme ihlal değildir; oturum sürer, olay uygulanır.
int test_tam_yuvaya_yineleme()
{
    Denetci d;
    SahteDuzenek z(hil());
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    std::uint64_t acma = 0;
    std::uint32_t hedef = 0;
    std::int64_t t_hedef = 0;
    bool bir_kez = true;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        if (bir_kez) {
            bir_kez = false;
            hedef = tf.seq;
            t_hedef = tf.t;
            acma = z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, tf.seq, tf.t, rev);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600), 1);
            return;
        }
        z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600));
    };
    (void)z.periyot();
    (void)z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, hedef, t_hedef, rev, 1, acma);   // yineleme
    for (int n = 0; n < 3; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, z.k.sayaclar().olay_sayim_ihlali == 0u && !protokol_kapanisi(z.sk), "ihlal yok, oturum sürüyor");
    DOGRULA(d, sonuc_bul(z.sk, acma) == IDA_EVR_APPLIED && z.k.izin_acik(0), "SAFETY_OPEN uygulandı");
    return d.hata;
}

// (3): sayıyı aşan SAFETY_CLOSE ihlal değildir ve uygulanır; oturum sürer.
int test_sayiyi_asan_kapanis()
{
    Denetci d;
    SahteDuzenek z(hil());
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    std::uint64_t acma = 0;
    std::uint32_t hedef = 0;
    std::int64_t t_hedef = 0;
    int asama = 0;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        if (asama == 0) {
            asama = 1;
            acma = z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, tf.seq, tf.t, rev);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600), 1);
            return;
        }
        if (asama == 2) {
            asama = 3;
            hedef = tf.seq;
            t_hedef = tf.t;
        }
        z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600));
    };
    for (int n = 0; n < 3; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, sonuc_bul(z.sk, acma) == IDA_EVR_APPLIED && z.k.izin_acik(0), "izin açık (düzenek)");
    asama = 2;
    (void)z.periyot();   // n_events = 0 yanıtlı parti
    const std::uint64_t kapama = z.sk.olay(IDAW_EVENT_KIND_SAFETY_CLOSE, 0, hedef, t_hedef, 0);
    for (int n = 0; n < 3; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, sonuc_bul(z.sk, kapama) == IDA_EVR_APPLIED && !z.k.izin_acik(0), "sayıyı aşan SAFETY_CLOSE uygulandı");
    DOGRULA(d, z.k.sayaclar().olay_sayim_ihlali == 0u && !protokol_kapanisi(z.sk), "ihlal yok, oturum sürüyor");
    return d.hata;
}

// (4), sol-3 #11: hata dönüşü (BAD_INPUT, n_events = 1) olay beklemeden tamamlanır ve
// canlılığı yeniler: T_bağ dolmaz.
int test_hata_donusu_canlilik()
{
    Denetci d;
    SahteDuzenek z(hil());
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    std::uint32_t hedef = 0;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) { hedef = tf.seq; };   // yanıt yok (henüz)
    (void)z.periyot();
    const std::int64_t t0 = z.t;
    z.sk.yanit(hedef, 0u, pwm_hepsi(0), 1, static_cast<std::uint8_t>(IDA_RC_BAD_INPUT));
    std::vector<std::uint8_t> w;
    w.swap(z.sk.giden);
    z.k.al(w.data(), w.size(), t0 + 490'000'000);
    z.k.zamanla(t0 + 505'000'000);
    DOGRULA(d, z.k.sayaclar().oturum_kopma == 0u && z.k.durum() == OturumDurumu::kosuyor,
            "hata dönüşü canlılığı yeniledi (kopma " + s(z.k.sayaclar().oturum_kopma) + ")");
    DOGRULA(d, z.k.yanit(hedef).has_value(), "hata dönüşü kabul edildi (olay beklenmedi)");
    return d.hata;
}

// (1): küme eksik kalırsa genişletici olay son geçerlilikte REJ_EXPIRED alır.
int test_eksik_kume_suresi()
{
    Denetci d;
    SahteDuzenek z(hil());
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    std::uint64_t acma = 0;
    bool bir_kez = true;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        if (bir_kez) {
            bir_kez = false;
            acma = z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, tf.seq, tf.t, rev);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600), 2);
            return;
        }
        z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600));
    };
    bool hic_acik = false;
    z.sinir_kancasi = [&](std::uint64_t) { hic_acik = hic_acik || z.k.izin_acik(0); };
    for (int n = 0; n < 8; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, sonuc_bul(z.sk, acma) == IDA_EVR_REJ_EXPIRED, "REJ_EXPIRED (" + s(sonuc_bul(z.sk, acma)) + ")");
    DOGRULA(d, !hic_acik, "izin açılmadı");
    return d.hata;
}

// Bekleyen yanıtlar sınırsız birikmez: küme tablosu 256 ile sınırlı, tahliye sayılır, izin
// açılmaz.
int test_bekleyen_kume_siniri()
{
    Denetci d;
    SahteDuzenek z(hil());
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600), 1);   // olay hiç gelmez
    };
    for (int n = 0; n < 300; ++n) {
        (void)z.periyot();
    }
    const Sayaclar& sy = z.k.sayaclar();
    DOGRULA(d, sy.kume_en_cok <= 256u, "küme tablosu sınırlı (" + s(sy.kume_en_cok) + ")");
    DOGRULA(d, sy.kume_bekleyen_tahliye > 0u, "bekleyen kümeler tahliye edildi (" + s(sy.kume_bekleyen_tahliye) + ")");
    DOGRULA(d, !z.k.izin_acik(0) && !z.k.izin_acik(1), "izin açılmadı");
    DOGRULA(d, sy.parti_kacirildi >= 290u, "eksik kümeli yuvalar kaçırıldı (" + s(sy.parti_kacirildi) + ")");
    return d.hata;
}
