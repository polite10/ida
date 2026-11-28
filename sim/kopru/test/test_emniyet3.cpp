// İkinci dar sol denetimi (EMNİYET; docs/gorevler/inceleme/W4-kopru-sol-2.md): #1, #2, #3, #11
// karşı örnekleri.
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

EmniyetAtama estop(bool b)
{
    EmniyetAtama a;
    a.acil_durdurma = b;
    return a;
}

// sk.giden'in n. baytından sonrasını keser ve döndürür (çerçeve kaybı).
std::vector<std::uint8_t> kes(SahteKart& sk, std::size_t n)
{
    std::vector<std::uint8_t> v(sk.giden.begin() + static_cast<std::ptrdiff_t>(n), sk.giden.end());
    sk.giden.resize(n);
    return v;
}

}  // namespace

// #1 (W4-K12 (3)): aynı pencerede acil durdurma bas–bırak–bas (R+1, R+2, R+3). Köprüye ulaşan
// son kayıt (estop = 1, rev = R+2) (gevşetme gecikmesi); R+3'ün kaydı atlandı. Tam eşleşen en
// yeni yetkili revizyon R+3'tür (örneğin revizyonu seçimi üstten sınırlamaz).
int test_emniyet_rev_ileri()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    (void)z.periyot();
    const std::uint32_t r = z.k.dis_durum().emniyet.emniyet_revizyonu;
    const std::uint64_t b0 = z.sinir;
    z.sinir_kancasi = [&](std::uint64_t b) {
        if (b == b0 + 1u) {
            z.k.emniyet_ata(estop(true));
        } else if (b == b0 + 2u) {
            z.k.emniyet_ata(estop(false));
        } else if (b == b0 + 3u) {
            z.k.emniyet_ata(estop(true));
        }
    };
    const std::size_t bas = z.sk.emniyet.size();
    const std::uint8_t g = static_cast<std::uint8_t>(kL | kF | kPo);
    // Kayıtlar: R+1 (estop 1) ve R+2'de hâlâ estop 1 (gevşetme gecikmesi); R+3'ün kaydı yok.
    for (int j = 0; j < 4; ++j) {
        z.sinir_ilerle();
    }
    z.sinir_kancasi = nullptr;
    DOGRULA(d, z.k.dis_durum().emniyet.emniyet_revizyonu == r + 3u && z.k.acil_durdurma(),
            "yetkili revizyonlar R+1..R+3, son durum basılı");
    z.k.olcum_ekle(emniyet_kaydi(40, z.t - 3'000'000, 1, g, g, r + 1u));
    z.k.olcum_ekle(emniyet_kaydi(41, z.t - 1'000'000, 1, g, g, r + 2u));
    (void)z.k.parti_kapat(z.t, z.t);
    alisveris(z.k, z.sk, z.t);
    DOGRULA(d, z.sk.emniyet.size() == bas + 1u, "bir emniyet üyesi");
    if (z.sk.emniyet.size() == bas + 1u) {
        const auto& p = z.sk.emniyet[bas];
        DOGRULA(d, p.estop == 1u && p.safety_rev == r + 3u,
                "gönderilen (estop " + s(p.estop) + ", rev R+" + s(p.safety_rev - r) + "); beklenen (1, R+3)");
    }
    return d.hata;
}

// #2 (EMNİYET): toplu doğrulamada reddedilen yanıt hiçbir geçmişe girmez: yanit(n) onu
// döndürmez, yanıt süresi kaydedilmez.
int test_reddedilen_yanit_gecmis()
{
    Denetci d;
    KopruAyari ka;
    ka.kip = CalistirmaKipi::hil_1x;
    SahteDuzenek z(ka);
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    std::uint32_t hedef = 0;
    bool bir_kez = true;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        if (bir_kez) {
            bir_kez = false;
            hedef = tf.seq;
            (void)z.sk.olay(IDAW_EVENT_KIND_FIRE, 0, tf.seq, tf.t, rev);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kSafe), pwm_hepsi(1500), 1);
            return;
        }
        z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600));
    };
    const std::size_t sure_bas = z.k.yanit_sureleri().size();
    (void)z.periyot();
    for (int n = 0; n < 3; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, !z.k.yanit(hedef).has_value(), "reddedilen yanıt yanit(n)'de yok");
    DOGRULA(d, z.k.yanit_sureleri().size() == sure_bas + 3u,
            "reddedilen yanıtın süresi kaydedilmedi (" + s(z.k.yanit_sureleri().size() - sure_bas) + " / 3)");
    DOGRULA(d, z.k.yanit(hedef + 1u).has_value(), "sonraki tutarlı yanıt kabul edildi");
    return d.hata;
}

// #3 (EMNİYET, HIL): SAFETY_OPEN ve n_events = 2, IS_SAFE = 1 yanıtı ulaşır, FIRE kaybolur:
// küme eksikken izin açılmaz; FIRE yinelemesi gelince küme tutarsız: ikisi de reddedilir.
int test_eksik_kume_tutarsiz()
{
    Denetci d;
    KopruAyari ka;
    ka.kip = CalistirmaKipi::hil_1x;
    SahteDuzenek z(ka);
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    std::uint64_t acma = 0, ates = 0;
    std::uint32_t hedef = 0;
    std::int64_t t_hedef = 0;
    int asama = 0;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        if (asama == 0) {
            asama = 1;
            hedef = tf.seq;
            t_hedef = tf.t;
            acma = z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, tf.seq, tf.t, rev);
            const std::size_t n = z.sk.giden.size();
            ates = z.sk.olay(IDAW_EVENT_KIND_FIRE, 0, tf.seq, tf.t, rev);
            (void)kes(z.sk, n);   // FIRE kayboldu
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kSafe | kArmed), pwm_hepsi(1500), 2);
            return;
        }
        z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600));
    };
    bool hic_acik = false;
    z.sinir_kancasi = [&](std::uint64_t) { hic_acik = hic_acik || z.k.izin_acik(0); };
    for (int n = 0; n < 3; ++n) {
        (void)z.periyot();
    }
    // FIRE yinelemesi (t_n + 15 ms civarı): aynı kimlik.
    (void)z.sk.olay(IDAW_EVENT_KIND_FIRE, 0, hedef, t_hedef, rev, 1, ates);
    for (int n = 0; n < 6; ++n) {
        (void)z.periyot();
    }
    hic_acik = hic_acik || z.k.izin_acik(0);
    DOGRULA(d, !hic_acik, "izin hiçbir sınırda açılmadı");
    DOGRULA(d, sonuc_bul(z.sk, acma) == IDA_EVR_REJ_SAFE, "SAFETY_OPEN REJ_SAFE (" + s(sonuc_bul(z.sk, acma)) + ")");
    DOGRULA(d, sonuc_bul(z.sk, ates) == IDA_EVR_REJ_SAFE, "FIRE REJ_SAFE (" + s(sonuc_bul(z.sk, ates)) + ")");
    DOGRULA(d, z.k.tup_dolu(0), "tüp dolu");
    return d.hata;
}

// #3 (EMNİYET, HIL): aynı durum, FIRE hiç gelmez: SAFETY_OPEN süresi dolunca reddedilir, izin
// hiç açılmaz.
int test_eksik_kume_gelmez()
{
    Denetci d;
    KopruAyari ka;
    ka.kip = CalistirmaKipi::hil_1x;
    SahteDuzenek z(ka);
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    std::uint64_t acma = 0;
    bool bir_kez = true;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        if (bir_kez) {
            bir_kez = false;
            acma = z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, tf.seq, tf.t, rev);
            const std::size_t n = z.sk.giden.size();
            (void)z.sk.olay(IDAW_EVENT_KIND_FIRE, 0, tf.seq, tf.t, rev);
            (void)kes(z.sk, n);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600), 2);
            return;
        }
        z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600));
    };
    bool hic_acik = false;
    z.sinir_kancasi = [&](std::uint64_t) { hic_acik = hic_acik || z.k.izin_acik(0); };
    for (int n = 0; n < 10; ++n) {
        (void)z.periyot();
    }
    hic_acik = hic_acik || z.k.izin_acik(0);
    DOGRULA(d, !hic_acik, "eksik kümeyle izin açılmadı");
    DOGRULA(d, sonuc_bul(z.sk, acma) == IDA_EVR_REJ_EXPIRED || sonuc_bul(z.sk, acma) == IDA_EVR_REJ_SAFE,
            "SAFETY_OPEN reddedildi (" + s(sonuc_bul(z.sk, acma)) + ")");
    return d.hata;
}

// #3 (HIL): parçalı teslim, tutarlı küme: FIRE kaybolur, yinelemesi süresi içinde gelir; küme
// tamamlanınca SAFETY_OPEN ve FIRE (geç) uygulanır. Kısıtlayan olay beklemez: aynı partideki
// SAFETY_CLOSE küme eksikken de uygulanır.
int test_eksik_kume_tutarli()
{
    Denetci d;
    KopruAyari ka;
    ka.kip = CalistirmaKipi::hil_1x;
    SahteDuzenek z(ka);
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    std::uint64_t acma = 0, ates = 0, kapama = 0;
    std::uint32_t hedef = 0;
    std::int64_t t_hedef = 0;
    int asama = 0;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        if (asama == 1) {
            asama = 2;
            hedef = tf.seq;
            t_hedef = tf.t;
            acma = z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, tf.seq, tf.t, rev);
            const std::size_t n = z.sk.giden.size();
            ates = z.sk.olay(IDAW_EVENT_KIND_FIRE, 0, tf.seq, tf.t, rev);
            (void)kes(z.sk, n);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600), 2);
            return;
        }
        if (asama == 3) {
            asama = 4;
            // İzin açıkken: SAFETY_CLOSE + kaybolan FIRE (tüp 1), n_events = 2.
            kapama = z.sk.olay(IDAW_EVENT_KIND_SAFETY_CLOSE, 0, tf.seq, tf.t, 0);
            const std::size_t n = z.sk.giden.size();
            (void)z.sk.olay(IDAW_EVENT_KIND_FIRE, 1, tf.seq, tf.t, z.k.dis_durum().emniyet.emniyet_revizyonu);
            (void)kes(z.sk, n);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600), 2);
            return;
        }
        z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600));
    };
    // W5-K5: tüp 0 için kartın kabul ettiği operatör FC_FIRE'ı.
    {
        const std::uint32_t fid = z.k.komut_fc(static_cast<std::uint8_t>(IDAW_FC_OP_FIRE), 0u, z.t);
        (void)z.periyot();
        (void)z.periyot();
        z.sk.komut_sonucu(fid, static_cast<std::uint8_t>(IDAW_ACK_ACCEPTED), 0u);
        alisveris(z.k, z.sk, z.t);
    }
    asama = 1;
    for (int n = 0; n < 3; ++n) {   // TF(n) ilk periyodun sonunda; a_n = t_n + 10 ms işlendi
        (void)z.periyot();
    }
    DOGRULA(d, sonuc_bul(z.sk, acma) == 0u && !z.k.izin_acik(0), "küme eksikken SAFETY_OPEN bekliyor");
    (void)z.sk.olay(IDAW_EVENT_KIND_FIRE, 0, hedef, t_hedef, rev, 1, ates);   // yineleme
    for (int n = 0; n < 2; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, sonuc_bul(z.sk, acma) == IDA_EVR_APPLIED, "küme tamamlanınca SAFETY_OPEN uygulandı (" +
                                                             s(sonuc_bul(z.sk, acma)) + ")");
    DOGRULA(d, sonuc_bul(z.sk, ates) == IDA_EVR_APPLIED && !z.k.tup_dolu(0), "FIRE uygulandı (" +
                                                                                 s(sonuc_bul(z.sk, ates)) + ")");
    DOGRULA(d, (z.k.sayaclar().calistirma_gecersiz & kGecGecOlay) != 0u, "geç uygulanan olay sayıldı (A1-K79)");
    // İzni yeniden aç (tüp 0 boş: izin açılabilir) — kapatma vakası için tüp 0'ın izni gerekir.
    std::uint64_t acma2 = 0;
    z.sk.tf_kancasi = [&, prev = z.sk.tf_kancasi](const SahteKart::Tf& tf) mutable {
        if (acma2 == 0u) {
            acma2 = z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, tf.seq, tf.t, z.k.dis_durum().emniyet.emniyet_revizyonu);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600), 1);
            return;
        }
        prev(tf);
    };
    (void)z.periyot();
    (void)z.periyot();
    (void)z.periyot();
    DOGRULA(d, sonuc_bul(z.sk, acma2) == IDA_EVR_APPLIED && z.k.izin_acik(0), "izin yeniden açıldı (düzenek)");
    asama = 3;
    (void)z.periyot();
    (void)z.periyot();
    (void)z.periyot();
    DOGRULA(d, sonuc_bul(z.sk, kapama) == IDA_EVR_APPLIED && !z.k.izin_acik(0),
            "SAFETY_CLOSE küme eksikken beklemeden uygulandı (" + s(sonuc_bul(z.sk, kapama)) + ")");
    return d.hata;
}

// #11 (EMNİYET, HIL): toplu doğrulamada reddedilecek yanıt canlılığı (T_bağ) yenilemez.
int test_tutarsiz_yanit_canlilik()
{
    Denetci d;
    KopruAyari ka;
    ka.kip = CalistirmaKipi::hil_1x;
    SahteDuzenek z(ka);
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    std::uint32_t hedef = 0;
    std::int64_t t_hedef = 0;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        hedef = tf.seq;
        t_hedef = tf.t;
        (void)z.sk.olay(IDAW_EVENT_KIND_FIRE, 0, tf.seq, tf.t, rev);   // yanıt yok (henüz)
    };
    (void)z.periyot();   // FIRE t_n'de alındı: son geçerli çerçeve
    const std::int64_t t0 = z.t;
    // Tutarsız yanıt T_bağ dolmadan biraz önce gelir (yuvası henüz işlenmedi).
    z.sk.yanit(hedef, static_cast<std::uint16_t>(kThr | kSafe), pwm_hepsi(1500), 1);
    std::vector<std::uint8_t> w;
    w.swap(z.sk.giden);
    z.k.al(w.data(), w.size(), t0 + 490'000'000);
    z.k.zamanla(t0 + 505'000'000);
    DOGRULA(d, z.k.sayaclar().oturum_kopma == 1u,
            "tutarsız yanıt canlılığı yenilemedi: T_bağ doldu (kopma " + s(z.k.sayaclar().oturum_kopma) + ")");
    (void)t_hedef;
    return d.hata;
}
