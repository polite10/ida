// Kabul 4 (EMNİYET): olayların uygulanması ve yetkili durum (A1-K81–K89, K106, K118;
// iskelet §3.2) ve emniyet örneğinin partiye konması (A1-K56). Sahte kart olayları üretir.
#include "test_destek.hpp"

using namespace ida::kopru;
using namespace ida::kopru::test;

namespace {

constexpr std::uint16_t kStep = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_THR_VALID | IDAW_ACT_FLAGS_ARMED);
constexpr std::uint16_t kStepGuvenli =
    static_cast<std::uint16_t>(IDAW_ACT_FLAGS_THR_VALID | IDAW_ACT_FLAGS_IS_SAFE);

struct OlayIstegi {
    std::uint8_t kind;
    std::uint8_t tube;
    std::uint32_t rev;
    std::uint64_t kimlik = 0;
};

// Sonraki TF'ye verilecek yanıt: olaylar (önce) ve yanıt.
struct Plan {
    std::vector<OlayIstegi> olaylar;
    std::uint16_t bayrak = kStep;
    std::vector<std::uint64_t> kimlikler;
};

std::array<std::uint16_t, 8> notr()
{
    std::array<std::uint16_t, 8> p{};
    p.fill(1500);
    return p;
}

void plan_kur(SahteDuzenek& z, Plan& plan)
{
    z.sk.tf_kancasi = [&z, &plan](const SahteKart::Tf& tf) {
        for (const OlayIstegi& o : plan.olaylar) {
            plan.kimlikler.push_back(z.sk.olay(o.kind, o.tube, tf.seq, tf.t, o.rev, 1, o.kimlik));
        }
        z.sk.yanit(tf.seq, plan.bayrak, notr(), static_cast<std::uint8_t>(plan.olaylar.size()));
        plan.olaylar.clear();
        plan.bayrak = kStep;
    };
}

std::uint8_t sonuc_bul(const SahteKart& sk, std::uint64_t id, std::size_t kacinci = 0)
{
    std::size_t k = 0;
    for (const auto& r : sk.sonuclar) {
        if (r.event_id == id) {
            if (k == kacinci) {
                return r.result;
            }
            ++k;
        }
    }
    return 0;
}

// Fiş takılı, pim çıkık (tüp 0): izin açılabilir durum. Gevşetme bir sınırda uygulanır.
void tup0_hazirla(SahteDuzenek& z)
{
    EmniyetAtama a;
    a.fis[0] = true;
    a.pim[0] = false;
    z.k.emniyet_ata(a);
    (void)z.periyot();
}

}  // namespace

int test_olay_izin_yok()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    Plan p;
    plan_kur(z, p);
    (void)z.periyot();
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    // Varsayılan yetkili durum: fiş çıkık, pim takılı (iskelet §5): izin açan olay ve FIRE
    // reddedilir, tüp değişmez.
    p.olaylar = {{IDAW_EVENT_KIND_SAFETY_OPEN, 0, rev}, {IDAW_EVENT_KIND_FIRE, 1, rev}};
    (void)z.periyot();
    for (int n = 0; n < 3; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, p.kimlikler.size() == 2u, "iki olay gönderildi");
    DOGRULA(d, sonuc_bul(z.sk, p.kimlikler[0]) == IDA_EVR_REJ_SAFE, "SAFETY_OPEN: REJ_SAFE (fiş çıkık, pim takılı)");
    DOGRULA(d, sonuc_bul(z.sk, p.kimlikler[1]) == IDA_EVR_REJ_SAFE, "FIRE: REJ_SAFE");
    DOGRULA(d, !z.k.izin_acik(0) && !z.k.izin_acik(1), "izin kapalı");
    DOGRULA(d, z.k.tup_dolu(0) && z.k.tup_dolu(1), "tüpler dolu");

    // Girdiler izin verir ama acil durdurma basılı: yine REJ_SAFE.
    tup0_hazirla(z);
    EmniyetAtama e;
    e.acil_durdurma = true;
    z.k.emniyet_ata(e);
    (void)z.periyot();
    const std::uint32_t rev2 = z.k.dis_durum().emniyet.emniyet_revizyonu;
    p.olaylar = {{IDAW_EVENT_KIND_SAFETY_OPEN, 0, rev2}};
    (void)z.periyot();
    for (int n = 0; n < 3; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, sonuc_bul(z.sk, p.kimlikler[2]) == IDA_EVR_REJ_SAFE, "acil durdurmada SAFETY_OPEN: REJ_SAFE");
    DOGRULA(d, !z.k.izin_acik(0), "izin kapalı");

    // Acil durdurma kalktı; eski revizyonla damgalanmış olay reddedilir (A1-K89, H17).
    e.acil_durdurma = false;
    z.k.emniyet_ata(e);
    (void)z.periyot();
    p.olaylar = {{IDAW_EVENT_KIND_SAFETY_OPEN, 0, rev2}};
    (void)z.periyot();
    for (int n = 0; n < 3; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, sonuc_bul(z.sk, p.kimlikler[3]) == IDA_EVR_REJ_SAFE, "eski revizyonlu olay: REJ_SAFE");
    DOGRULA(d, z.k.sayaclar().olay_rev_ret >= 1u, "olay_rev_ret sayıldı");
    DOGRULA(d, !z.k.izin_acik(0) && z.k.tup_dolu(0), "izin kapalı, tüp dolu");

    // Güncel revizyonla açılır; acil durdurma basılınca aynı sınırda kapanır.
    const std::uint32_t rev3 = z.k.dis_durum().emniyet.emniyet_revizyonu;
    p.olaylar = {{IDAW_EVENT_KIND_SAFETY_OPEN, 0, rev3}};
    (void)z.periyot();
    (void)z.periyot();
    (void)z.periyot();
    DOGRULA(d, sonuc_bul(z.sk, p.kimlikler[4]) == IDA_EVR_APPLIED && z.k.izin_acik(0), "güncel revizyonla izin açıldı");
    z.k.emniyet_ata(EmniyetAtama{true, {}, {}, 0, {}});
    (void)z.periyot();
    DOGRULA(d, !z.k.izin_acik(0), "acil durdurma izni kapattı (A1-K89)");
    DOGRULA(d, z.k.dis_durum().emniyet.emniyet_revizyonu == rev3 + 1u, "tek revizyon artışı");

    // Tutarsız yanıt (FIRE için IS_SAFE = 1, ARMED = 0): REJ_SAFE (A1-K118).
    z.k.emniyet_ata(EmniyetAtama{false, {}, {}, 0, {}});
    (void)z.periyot();
    const std::uint32_t rev4 = z.k.dis_durum().emniyet.emniyet_revizyonu;
    p.olaylar = {{IDAW_EVENT_KIND_SAFETY_OPEN, 0, rev4}};
    (void)z.periyot();
    (void)z.periyot();
    (void)z.periyot();
    DOGRULA(d, z.k.izin_acik(0), "izin yeniden açıldı");
    p.olaylar = {{IDAW_EVENT_KIND_FIRE, 0, rev4}};
    p.bayrak = kStepGuvenli;
    (void)z.periyot();
    (void)z.periyot();
    (void)z.periyot();
    DOGRULA(d, sonuc_bul(z.sk, p.kimlikler[6]) == IDA_EVR_REJ_SAFE, "tutarsız yanıtlı FIRE: REJ_SAFE");
    DOGRULA(d, z.k.tup_dolu(0), "tüp dolu kaldı");
    return d.hata;
}

int test_olay_yinelenen()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    Plan p;
    plan_kur(z, p);
    tup0_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    p.olaylar = {{IDAW_EVENT_KIND_SAFETY_OPEN, 0, rev}};
    (void)z.periyot();
    (void)z.periyot();
    (void)z.periyot();
    DOGRULA(d, z.k.izin_acik(0), "izin açıldı");
    const std::uint64_t id = p.kimlikler[0];
    // Aynı kimlik yeniden (kartın yinelemesi): yeniden uygulanmaz, saklı sonuç DUPLICATE.
    p.olaylar = {{IDAW_EVENT_KIND_SAFETY_OPEN, 0, rev, id}};
    (void)z.periyot();
    (void)z.periyot();
    (void)z.periyot();
    DOGRULA(d, sonuc_bul(z.sk, id, 0) == IDA_EVR_APPLIED, "ilk sonuç APPLIED");
    DOGRULA(d, sonuc_bul(z.sk, id, 1) == IDA_EVR_DUPLICATE, "yineleme: DUPLICATE (A1-K81)");
    DOGRULA(d, z.k.sayaclar().olay_sonuc[IDA_EVR_APPLIED] == 1u, "bir kez uygulandı");
    // SAFETY_CLOSE iki kez: tek uygulama, tek revizyon artışı.
    const std::uint32_t rev_once = z.k.dis_durum().emniyet.emniyet_revizyonu;
    p.olaylar = {{IDAW_EVENT_KIND_SAFETY_CLOSE, 0, 0}};
    (void)z.periyot();
    (void)z.periyot();
    (void)z.periyot();
    const std::uint64_t kid = p.kimlikler.back();
    p.olaylar = {{IDAW_EVENT_KIND_SAFETY_CLOSE, 0, 0, kid}};
    (void)z.periyot();
    (void)z.periyot();
    (void)z.periyot();
    DOGRULA(d, !z.k.izin_acik(0), "izin kapandı");
    DOGRULA(d, z.k.dis_durum().emniyet.emniyet_revizyonu == rev_once + 1u, "yinelenen SAFETY_CLOSE tek artış");
    DOGRULA(d, sonuc_bul(z.sk, kid, 1) == IDA_EVR_DUPLICATE, "yinelenen kapanış: DUPLICATE");
    return d.hata;
}

int test_olay_suresi_gecmis()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    Plan p;
    plan_kur(z, p);
    tup0_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    // Olay partisinin TF'sine yanıt verilir (olayı sayar: W4-K15, sayılmamış genişletici olay
    // sayım ihlalidir) ama olayın ilk gönderimi kaybolur; yinelemesi a_n + 25 ms sonra gelir.
    SahteKart::Tf olay_tf{};
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        std::uint8_t n_ev = 0;
        if (olay_tf.seq == 0) {
            olay_tf = tf;
            n_ev = 1;
        }
        z.sk.yanit(tf.seq, kStep, notr(), n_ev);
    };
    (void)z.periyot();
    for (int n = 0; n < 7; ++n) {
        (void)z.periyot();
    }
    const std::uint64_t id = z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, olay_tf.seq, olay_tf.t, rev);
    alisveris(z.k, z.sk, z.t);
    (void)z.periyot();
    (void)z.periyot();
    DOGRULA(d, sonuc_bul(z.sk, id) == IDA_EVR_REJ_EXPIRED, "son geçerliliği geçmiş olay: REJ_EXPIRED (A1-K84)");
    DOGRULA(d, !z.k.izin_acik(0), "izin açılmadı");
    DOGRULA(d, z.k.sayaclar().olay_gec == 1u, "olay_gec 1 (A1-K83)");
    return d.hata;
}

int test_olay_fire_kabul()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    Plan p;
    plan_kur(z, p);
    tup0_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    DOGRULA(d, rev == 2u, "gevşetme revizyonu bir artırdı (A1-K106 (6))");
    // Aynı partide izin açan olay ve FIRE: önce SAFETY_OPEN, sonra FIRE (A1-K85 (3)).
    p.olaylar = {{IDAW_EVENT_KIND_FIRE, 0, rev}, {IDAW_EVENT_KIND_SAFETY_OPEN, 0, rev}};
    (void)z.periyot();
    (void)z.periyot();
    (void)z.periyot();
    DOGRULA(d, sonuc_bul(z.sk, p.kimlikler[1]) == IDA_EVR_APPLIED, "SAFETY_OPEN uygulandı");
    DOGRULA(d, sonuc_bul(z.sk, p.kimlikler[0]) == IDA_EVR_APPLIED, "FIRE uygulandı (aynı sınırda izin açıldıktan sonra)");
    DOGRULA(d, !z.k.tup_dolu(0) && z.k.tup_dolu(1), "yalnız tüp 0 boşaldı");
    DOGRULA(d, z.k.dis_durum().emniyet.emniyet_revizyonu == rev, "SAFETY_OPEN ve FIRE revizyonu artırmaz");
    p.olaylar = {{IDAW_EVENT_KIND_FIRE, 0, rev}};
    (void)z.periyot();
    (void)z.periyot();
    (void)z.periyot();
    DOGRULA(d, sonuc_bul(z.sk, p.kimlikler[2]) == IDA_EVR_REJ_TUBE_EMPTY, "boş tüpe FIRE: REJ_TUBE_EMPTY");
    // İzin hold_ns (200 ms) sonra kendiliğinden kapanır ve revizyon artar.
    for (int n = 0; n < 45; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, !z.k.izin_acik(0), "izin süresi doldu (A1-K86)");
    DOGRULA(d, z.k.dis_durum().emniyet.emniyet_revizyonu == rev + 1u, "dolan izin tek artış (A1-K106 (5))");
    return d.hata;
}

int test_emniyet_ornegi()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    auto kayit = [](std::uint32_t seq, std::int64_t t, std::uint8_t estop, std::uint8_t t0) {
        ida::sensor::Olcum o;
        o.hdr.tur = ida::sensor::SensorTuru::firlatici_geri_okuma;
        o.hdr.seq = seq;
        o.hdr.t_meas_ns = t;
        o.hdr.flags = ida::sensor::kBayrakGecerli;
        ida::sensor::EmniyetOlcumu e;
        e.estop = estop;
        e.tube = {t0, static_cast<std::uint8_t>(IDAW_TUBE_FLAGS_LOADED)};
        e.safety_rev = 1;
        o.yuk = e;
        return o;
    };
    const std::uint8_t acik = static_cast<std::uint8_t>(IDAW_TUBE_FLAGS_LOADED | IDAW_TUBE_FLAGS_PLUG_IN |
                                                        IDAW_TUBE_FLAGS_PIN_OUT);
    (void)z.periyot({kayit(1, 1'000'000, 0, acik)});
    // Aynı pencerede kapanma (estop 1) ve ardından açılma (estop 0): kapanma bu partide, açılma
    // sonraki partide.
    (void)z.periyot({kayit(2, 6'000'000, 1, acik), kayit(3, 7'000'000, 0, acik)});
    (void)z.periyot();
    // Aynı pencerede iki kısıtlama: tek kayıt, en kısıtlayıcı birleşim.
    (void)z.periyot({kayit(4, 16'000'000, 0, static_cast<std::uint8_t>(IDAW_TUBE_FLAGS_LOADED | IDAW_TUBE_FLAGS_PLUG_IN)),
                     kayit(5, 17'000'000, 0, static_cast<std::uint8_t>(IDAW_TUBE_FLAGS_LOADED))});
    std::uint32_t fazla = 0;
    for (const auto& u : z.sk.parti_uyeleri) {
        std::uint32_t n = 0;
        for (std::uint32_t m : u) {
            n += m == MAVLINK_MSG_ID_IDA_SAFETY ? 1u : 0u;
        }
        fazla += n > 1u ? 1u : 0u;
    }
    DOGRULA(d, fazla == 0u, "partide en çok bir IDA_SAFETY");
    DOGRULA(d, z.sk.emniyet.size() == 4u, "dört emniyet üyesi (" + s(z.sk.emniyet.size()) + ")");
    if (z.sk.emniyet.size() == 4u) {
        DOGRULA(d, z.sk.emniyet[1].estop == 1u && z.sk.emniyet[1].seq == 2u, "kapanma kendi partisinde");
        DOGRULA(d, z.sk.emniyet[2].estop == 0u && z.sk.emniyet[2].seq == 3u, "açılma sonraki partide");
        DOGRULA(d, z.sk.emniyet[3].tube[0] == IDAW_TUBE_FLAGS_LOADED && z.sk.emniyet[3].seq == 5u,
                "iki kayıt en kısıtlayıcı birleşimle tek kayıt");
    }
    return d.hata;
}

// W4-K4 (EMNİYET): emniyet örneğinin revizyonu, girdi değerlerinin sim'de yetkili olduğu
// revizyondur; gevşetmenin bildirimi gecikmiş bir kayıt yeni revizyonla gönderilmez.
int test_emniyet_rev_durum()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    z.k.emniyet_ata(EmniyetAtama{true, {}, {}, 0, {}});
    (void)z.periyot();
    const std::uint32_t rev_basili = z.k.dis_durum().emniyet.emniyet_revizyonu;
    z.k.emniyet_ata(EmniyetAtama{false, {}, {}, 0, {}});
    (void)z.periyot();
    const std::uint32_t rev_kalkti = z.k.dis_durum().emniyet.emniyet_revizyonu;
    DOGRULA(d, rev_kalkti == rev_basili + 1u, "kalkış revizyonu artırdı");
    ida::sensor::Olcum o;
    o.hdr.tur = ida::sensor::SensorTuru::firlatici_geri_okuma;
    o.hdr.seq = 1;
    o.hdr.t_meas_ns = z.t;
    o.hdr.flags = ida::sensor::kBayrakGecerli;
    ida::sensor::EmniyetOlcumu e;
    e.estop = 1;   // sensör kalkışı henüz algılamadı
    e.tube = {static_cast<std::uint8_t>(IDAW_TUBE_FLAGS_LOADED), static_cast<std::uint8_t>(IDAW_TUBE_FLAGS_LOADED)};
    e.safety_rev = rev_kalkti;
    o.yuk = e;
    const std::size_t once = z.sk.emniyet.size();
    (void)z.periyot({o});
    DOGRULA(d, z.sk.emniyet.size() == once + 1u, "emniyet örneği gönderildi");
    if (z.sk.emniyet.size() == once + 1u) {
        DOGRULA(d, z.sk.emniyet.back().estop == 1u && z.sk.emniyet.back().safety_rev == rev_basili,
                "(estop = 1) acil durdurmanın revizyonuyla gider (" + s(z.sk.emniyet.back().safety_rev) + ")");
    }
    DOGRULA(d, z.k.sayaclar().emniyet_rev_duzeltme == 1u, "emniyet_rev_duzeltme 1");
    return d.hata;
}
