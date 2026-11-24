// sol düzeltme turu, A (EMNİYET): #1 (W4-K12), #2, #3, #4, #11 karşı örnekleri.
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

// İki tüpün fişi takılı, pimi çıkık (gevşek) yetkili durum.
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

}  // namespace

// #1, W4-K12 (1): tüp 0'ı gevşetip tüp 1'i kısıtlayan kayıt: tüp 1'in kısıtlaması o partide.
int test_emniyet_capraz_alan()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    const std::size_t bas = z.sk.emniyet.size();
    (void)z.periyot({emniyet_kaydi(10, z.t + 1'000'000, 0, static_cast<std::uint8_t>(kL | kPo),
                                   static_cast<std::uint8_t>(kL | kF | kPo), rev),
                     emniyet_kaydi(11, z.t + 2'000'000, 0, static_cast<std::uint8_t>(kL | kF | kPo),
                                   static_cast<std::uint8_t>(kL | kPo), rev)});
    (void)z.periyot();
    DOGRULA(d, z.sk.emniyet.size() == bas + 2u, "iki emniyet üyesi (" + s(z.sk.emniyet.size() - bas) + ")");
    if (z.sk.emniyet.size() == bas + 2u) {
        const auto& p1 = z.sk.emniyet[bas];
        const auto& p2 = z.sk.emniyet[bas + 1];
        DOGRULA(d, (p1.tube[0] & kF) == 0u && (p1.tube[1] & kF) == 0u,
                "ilk partide iki tüpün fişi de çıkık: tüp 1'in kısıtlaması ertelenmedi");
        DOGRULA(d, (p2.tube[0] & kF) != 0u && (p2.tube[1] & kF) == 0u && p2.seq == 11u,
                "tüp 0'ın gevşetmesi sonraki partide");
        DOGRULA(d, p1.seq < p2.seq && p1.safety_rev <= rev && p2.safety_rev <= rev, "seq artan, revizyon ≤ yetkili");
    }
    return d.hata;
}

// #1, W4-K12 (2): izin açılması (gevşetme) ve acil durdurma (kısıtlama) aynı kayıtta. Acil
// durdurma o partide. Sim'de hiç açılmamış izni bildiren kayıt hiçbir revizyonla değişmezi
// sağlayamaz: izin biti kapatılarak (yetkili durumla birleştirilerek) gönderilir.
int test_emniyet_izin_acik()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    const std::size_t bas = z.sk.emniyet.size();
    const std::uint8_t g = static_cast<std::uint8_t>(kL | kF | kPo);
    (void)z.periyot({emniyet_kaydi(20, z.t + 1'000'000, 0, g, g, rev),
                     emniyet_kaydi(21, z.t + 2'000'000, 1, static_cast<std::uint8_t>(g | kI), g, rev)});
    (void)z.periyot();
    DOGRULA(d, z.sk.emniyet.size() == bas + 2u, "iki emniyet üyesi");
    if (z.sk.emniyet.size() == bas + 2u) {
        const auto& p1 = z.sk.emniyet[bas];
        const auto& p2 = z.sk.emniyet[bas + 1];
        DOGRULA(d, p1.estop == 1u && (p1.tube[0] & kI) == 0u, "ilk partide acil durdurma basılı, izin kapalı");
        DOGRULA(d, p2.estop == 1u && (p2.tube[0] & kI) == 0u,
                "yetkili durumda açılmamış izin karta açık bildirilmez (W4-K12 (2))");
        DOGRULA(d, z.k.sayaclar().emniyet_rev_eslesmedi >= 1u, "uygun revizyon bulunamadığı sayıldı");
    }
    return d.hata;
}

// #2 (EMNİYET): reddedilen yinelenen yanıt olayın yetki dayanağını değiştirmez. İlk yanıt
// güvenli (IS_SAFE, silahsız), ikinci (daha yeni out_seq) silahlı: FIRE reddedilir.
int test_yinelenen_yanit_olay()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    std::uint64_t acma = 0, ates = 0;
    int asama = 0;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        if (asama == 1) {   // izni aç (tutarlı yanıt)
            acma = z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, tf.seq, tf.t, rev);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600), 1);
        } else if (asama == 2) {   // FIRE: ilk yanıt güvenli, yinelenen silahlı
            ates = z.sk.olay(IDAW_EVENT_KIND_FIRE, 0, tf.seq, tf.t, rev);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kSafe), pwm_hepsi(1500), 1);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600), 1);
        } else {
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600));
        }
        asama = asama > 0 ? 3 : 0;
    };
    asama = 1;
    for (int n = 0; n < 3; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, sonuc_bul(z.sk, acma) == IDA_EVR_APPLIED && z.k.izin_acik(0), "izin açıldı (düzenek)");
    asama = 2;
    for (int n = 0; n < 3; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, z.k.sayaclar().yanit_ret_yinelenen == 1u, "yinelenen yanıt reddedildi");
    DOGRULA(d, sonuc_bul(z.sk, ates) == IDA_EVR_REJ_SAFE, "FIRE kabul edilmiş (güvenli) yanıta göre REJ_SAFE");
    DOGRULA(d, z.k.tup_dolu(0), "tüp dolu kaldı");
    return d.hata;
}

// #3 (EMNİYET): aynı partide SAFETY_OPEN ve FIRE, yanıtta IS_SAFE = 1: yanıt FIRE ile tutarsız;
// yanıt ve ona bağlı bütün izin açan olaylar birlikte reddedilir, izin açılmaz.
int test_toplu_tutarlilik()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.dis_durum().emniyet.emniyet_revizyonu;
    std::uint64_t acma = 0, ates = 0;
    bool bir_kez = true;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        if (bir_kez) {
            bir_kez = false;
            acma = z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, tf.seq, tf.t, rev);
            ates = z.sk.olay(IDAW_EVENT_KIND_FIRE, 0, tf.seq, tf.t, rev);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kSafe | kArmed), pwm_hepsi(1500), 2);
            return;
        }
        z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600));
    };
    for (int n = 0; n < 4; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, sonuc_bul(z.sk, acma) == IDA_EVR_REJ_SAFE, "SAFETY_OPEN reddedildi (tutarsız yanıtın olayı)");
    DOGRULA(d, sonuc_bul(z.sk, ates) == IDA_EVR_REJ_SAFE, "FIRE reddedildi");
    DOGRULA(d, !z.k.izin_acik(0) && z.k.tup_dolu(0), "izin hiç açılmadı, tüp dolu");
    return d.hata;
}

// #4 (A1-K105): kart yeniden açılınca (yeni boot_count) out_seq 1'den başlar; yeni açılışın
// çıktıları uygulanır (gerçek uygulanan darbe genişliği).
int test_yeni_acilis_out_seq()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    z.sk.pwm = pwm_hepsi(1550);
    for (int n = 0; n < 40; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, z.sinirlar.back().pwm_us[0] == 1550.0, "ilk açılışta komut uygulanıyor");
    z.k.oturum_kapat(static_cast<std::uint8_t>(IDAW_CLOSE_REASON_RUN_END), z.t);
    alisveris(z.k, z.sk, z.t);
    // Kart yeniden açıldı: yeni boot_count, out_seq ve oturum sayısı sıfır.
    z.sk.boot_count = 2;
    z.sk.out_seq = 0;
    z.sk.sessions_since_boot = 0;
    z.sk.pwm = pwm_hepsi(1650);
    DOGRULA(d, z.el_sikis(), "yeni açılışla el sıkışma");
    for (int n = 0; n < 6; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, z.sinirlar.back().pwm_us[0] == 1650.0,
            "yeni açılışın çıktısı uygulandı (uygulanan " + s(static_cast<std::uint64_t>(z.sinirlar.back().pwm_us[0])) + ")");
    DOGRULA(d, z.k.sayaclar().yanit_ret_eski_uretim == 0u, "eski üretim reddi yok");
    return d.hata;
}

// #11 (A1-K109): geçersiz çerçeve durum değiştirmez: alan reddi alan aktüatör HAZIR'ı
// bitirmez; eski oturumlu çerçeveler T_bağ'ı yenilemez.
int test_gecersiz_cerceve_etkisiz()
{
    Denetci d;
    KopruAyari ka;
    ka.kip = CalistirmaKipi::hil_1x;   // T_bağ yalnız HIL'de
    Kopru k(ka);
    SahteKart sk;
    sk.oto_yanit = false;
    k.el_sikisma_baslat(0);
    // Kabulden sonra karta KEEPALIVE yazdırmadan: yalnız geçersiz aktüatör gelsin.
    std::vector<std::uint8_t> v;
    k.giden_al(v);
    sk.al(v);   // BOARD_HELLO
    std::vector<std::uint8_t> w;
    w.swap(sk.giden);
    k.al(w.data(), w.size(), 0);
    k.giden_al(v);
    sk.al(v);   // SESSION_ACCEPT → sahte KEEPALIVE yazar; onu at
    sk.giden.clear();
    DOGRULA(d, k.durum() == OturumDurumu::hazir, "HAZIR");
    // Geçersiz aktüatör (alan V2: valid_for_ns yanlış).
    sk.yanit(1, kThr, pwm_hepsi(1500), 0, 0, 7u);
    w.swap(sk.giden);
    k.al(w.data(), w.size(), 1'000'000);
    DOGRULA(d, k.durum() == OturumDurumu::hazir, "geçersiz aktüatör HAZIR'ı bitirmedi");
    // Eski oturumlu canlılık çerçeveleri T_bağ'ı yenilemez.
    const std::uint16_t gercek = sk.sess;
    sk.sess = static_cast<std::uint16_t>(gercek + 1u);
    for (std::int64_t t = 100; t <= 600; t += 100) {
        mavlink_ida_keepalive_t kk{};
        kk.sess = sk.sess;
        kk.state = 2;
        sk.yaz(MAVLINK_MSG_ID_IDA_KEEPALIVE, kk);
        w.clear();
        w.swap(sk.giden);
        k.al(w.data(), w.size(), t * 1'000'000);
        k.zamanla(t * 1'000'000);
    }
    DOGRULA(d, k.sayaclar().oturum_kopma == 1u && k.durum() == OturumDurumu::el_sikisma,
            "eski oturumlu çerçeveler oturumu canlı tutmadı: T_bağ doldu, yeni el sıkışma");
    DOGRULA(d, k.sayaclar().yanit_ret_oturum >= 5u, "eski oturum çerçeveleri sayıldı");
    return d.hata;
}
