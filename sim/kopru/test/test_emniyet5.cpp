// W4-K15 ek (7) (EMNİYET; beşinci dar sol denetimi, docs/gorevler/inceleme/W4-kopru-sol-5.md):
// kümeye bağlı çerçevelerin canlılık (T_bağ) katkısı kümenin kararına bağlıdır. Her vaka HIL'de:
// son bağımsız geçerli alım en geç t0; çerçeveler t0 + 490/491 ms'de gelir; t0 + 505 ms'de
// T_bağ (500 ms) denetlenir.
#include "test_destek.hpp"

using namespace ida::kopru;
using namespace ida::kopru::test;

namespace {

constexpr std::uint16_t kThr = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_THR_VALID);
constexpr std::uint16_t kArmed = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_ARMED);
constexpr std::uint16_t kSafe = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_IS_SAFE);
constexpr std::uint16_t kTick = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_SRC_TICK);
constexpr std::uint16_t kAsync = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_ASYNC);
constexpr std::int64_t kMsT = 1'000'000;

std::array<std::uint16_t, 8> pwm_hepsi(std::uint16_t v)
{
    std::array<std::uint16_t, 8> p{};
    p.fill(v);
    return p;
}

// Hedef partinin TF'si gönderilir, kart henüz yanıtlamaz; t0 = o anki duvar (son bağımsız
// geçerli alım en geç t0).
struct Duzen {
    SahteDuzenek z;
    std::uint32_t rev = 0;
    std::uint32_t hedef = 0;
    std::int64_t t_hedef = 0;
    std::int64_t t0 = 0;

    Duzen() : z(hil())
    {
        (void)z.el_sikis();
        EmniyetAtama a;
        a.fis = {true, true};
        a.pim = {false, false};
        z.k.emniyet_ata(a);
        (void)z.periyot();
        rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
        z.sk.tf_kancasi = [this](const SahteKart::Tf& tf) {
            hedef = tf.seq;
            t_hedef = tf.t;
        };
        (void)z.periyot();
        t0 = z.t;
    }
    static KopruAyari hil()
    {
        KopruAyari ka;
        ka.kip = CalistirmaKipi::hil_1x;
        return ka;
    }
    void ver(std::int64_t t)
    {
        std::vector<std::uint8_t> w;
        w.swap(z.sk.giden);
        z.k.al(w.data(), w.size(), t0 + t);
    }
    std::uint64_t olay(std::uint8_t kind, std::uint64_t kimlik = 0)
    {
        return z.sk.olay(kind, 0, hedef, t_hedef, kind == IDAW_EVENT_KIND_SAFETY_CLOSE ? 0u : rev, 1, kimlik);
    }
    void yanit(std::uint16_t f, std::uint8_t n_ev, std::uint8_t rc = 0)
    {
        const bool thr = (f & kThr) != 0u;
        z.sk.yanit(hedef, f, pwm_hepsi(thr && (f & kSafe) != 0u ? 1500 : 1600), n_ev, rc);
    }
    // Yuvanın uygulama anı işlenene kadar sınırları ilerletir (kart yanıt vermez).
    void yuvayi_isle()
    {
        z.sk.tf_kancasi = [](const SahteKart::Tf&) {};
        for (int j = 0; j < 8; ++j) {
            z.sinir_ilerle();
        }
    }
    bool koptu()
    {
        z.k.zamanla(t0 + 505 * kMsT);
        return z.k.sayaclar().oturum_kopma == 1u;
    }
};

}  // namespace

// sol-5 karşı örneği: FIRE (490 ms), sonra ACTUATOR(n_events = 1, IS_SAFE = 1, ARMED = 1, OK)
// (491 ms): küme topluca reddedilir; FIRE'ın alımı da canlılığı yenilemez.
int test_canlilik_olay_yanit_ret()
{
    Denetci d;
    Duzen u;
    (void)u.olay(IDAW_EVENT_KIND_FIRE);
    u.ver(490 * kMsT);
    u.yanit(static_cast<std::uint16_t>(kThr | kSafe | kArmed), 1);
    u.ver(491 * kMsT);
    DOGRULA(d, u.z.k.sayaclar().kume_reddedilen == 1u, "küme reddedildi");
    DOGRULA(d, u.koptu(), "reddedilen kümenin önceki olayı canlılığı yenilemedi (T_bağ doldu)");
    return d.hata;
}

// Eksik kümenin önceki üyeleri: SAFETY_OPEN (490) + ACTUATOR(n_events = 2) (491); küme hiç
// tamamlanmaz: T_bağ dolar.
int test_canlilik_eksik_kume()
{
    Denetci d;
    Duzen u;
    (void)u.olay(IDAW_EVENT_KIND_SAFETY_OPEN);
    u.ver(490 * kMsT);
    u.yanit(static_cast<std::uint16_t>(kThr | kArmed), 2);
    u.ver(491 * kMsT);
    DOGRULA(d, u.koptu(), "eksik kümenin üyeleri canlılığı yenilemedi (T_bağ doldu)");
    return d.hata;
}

// Ret sonrası yinelemeler: küme 10 ms'de reddedilir; FIRE'ın karar öncesi yinelemesi (490) ve
// karar sonrası (saklı sonuç talebi) yinelemesi canlılığı yenilemez.
int test_canlilik_ret_yineleme()
{
    Denetci d;
    {
        Duzen u;
        const std::uint64_t id = u.olay(IDAW_EVENT_KIND_FIRE);
        u.yanit(static_cast<std::uint16_t>(kThr | kSafe | kArmed), 1);
        u.ver(10 * kMsT);
        (void)u.olay(IDAW_EVENT_KIND_FIRE, id);   // karar öncesi yineleme
        u.ver(490 * kMsT);
        DOGRULA(d, u.koptu(), "karar öncesi yineleme yenilemedi");
    }
    {
        Duzen u;
        const std::uint64_t id = u.olay(IDAW_EVENT_KIND_FIRE);
        u.yanit(static_cast<std::uint16_t>(kThr | kSafe | kArmed), 1);
        u.ver(10 * kMsT);
        u.yuvayi_isle();   // olay REJ_SAFE ile karara bağlandı (saklı)
        (void)u.olay(IDAW_EVENT_KIND_FIRE, id);   // karar sonrası yineleme
        u.ver(490 * kMsT);
        DOGRULA(d, u.koptu(), "reddedilmiş olayın yinelemesi yenilemedi");
    }
    return d.hata;
}

// Kararı bekleyen kümenin olayının yinelemesi: SAFETY_OPEN (10) + ACTUATOR(n_events = 2) (11);
// SAFETY_OPEN yinelemesi (490) canlılığı yenilemez.
int test_canlilik_bekleyen_yineleme()
{
    Denetci d;
    Duzen u;
    const std::uint64_t id = u.olay(IDAW_EVENT_KIND_SAFETY_OPEN);
    u.ver(10 * kMsT);
    u.yanit(static_cast<std::uint16_t>(kThr | kArmed), 2);
    u.ver(11 * kMsT);
    (void)u.olay(IDAW_EVENT_KIND_SAFETY_OPEN, id);
    u.ver(490 * kMsT);
    DOGRULA(d, u.koptu(), "bekleyen kümenin yinelemesi yenilemedi");
    return d.hata;
}

// Doğrulanan küme: SAFETY_OPEN (490) + ACTUATOR(n_events = 1, ARMED) (491): alımlar
// canlılığa işlenir; 505 ms'de yanlış T_bağ aşımı yok.
int test_canlilik_dogrulanan_kume()
{
    Denetci d;
    Duzen u;
    (void)u.olay(IDAW_EVENT_KIND_SAFETY_OPEN);
    u.ver(490 * kMsT);
    u.yanit(static_cast<std::uint16_t>(kThr | kArmed), 1);
    u.ver(491 * kMsT);
    DOGRULA(d, !u.koptu(), "doğrulanan kümenin alımı canlılığa işlendi (kopma yok)");
    return d.hata;
}

// Beklemeden uygulananlar alındıkları anda yeniler: SAFETY_CLOSE, hata dönüşü, ASYNC çıktı,
// uygulanmış olayın yinelemesi.
int test_canlilik_aninda()
{
    Denetci d;
    {
        Duzen u;
        (void)u.olay(IDAW_EVENT_KIND_SAFETY_CLOSE);
        u.ver(490 * kMsT);
        DOGRULA(d, !u.koptu(), "SAFETY_CLOSE yeniledi");
    }
    {
        Duzen u;
        u.yanit(0u, 1, static_cast<std::uint8_t>(IDA_RC_BAD_INPUT));
        u.ver(490 * kMsT);
        DOGRULA(d, !u.koptu(), "hata dönüşü yeniledi");
    }
    {
        Duzen u;
        u.yanit(static_cast<std::uint16_t>(kThr | kSafe | kTick | kAsync), 0);
        u.ver(490 * kMsT);
        DOGRULA(d, !u.koptu(), "ASYNC çıktı yeniledi");
    }
    {
        Duzen u;
        const std::uint64_t id = u.olay(IDAW_EVENT_KIND_SAFETY_OPEN);
        u.yanit(static_cast<std::uint16_t>(kThr | kArmed), 1);
        u.ver(10 * kMsT);
        u.yuvayi_isle();   // SAFETY_OPEN uygulandı (saklı)
        DOGRULA(d, u.z.k.izin_acik(0), "izin açıldı (düzenek)");
        (void)u.olay(IDAW_EVENT_KIND_SAFETY_OPEN, id);
        u.ver(490 * kMsT);
        DOGRULA(d, !u.koptu(), "uygulanmış olayın yinelemesi yeniledi");
    }
    return d.hata;
}
