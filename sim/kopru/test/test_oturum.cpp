// Kabul 1–2: oturum (el sıkışma, 10 000 parti, kopma ve yeniden kurulum, yanlış dialect_hash)
// ve parti içeriği; karşı taraf gerçek çekirdekli ida_link üst katmanıdır (bellek taşıması).
#include "test_destek.hpp"

using namespace ida::kopru;
using namespace ida::kopru::test;

// A5 Ek D.7 vektörleri (ana_tohum = 1).
int test_d7_vektorleri()
{
    Denetci d;
    DOGRULA(d, d7_u64(1, "a1/board_tohum", 0) == 0xb1536801d0cf436cULL, "a1/board_tohum");
    DOGRULA(d, d7_u32(1, "a1/run_id", 0) == 0x3f25d322u, "a1/run_id");
    DOGRULA(d, d7_u32(1, "ida/a1/nonce_sim/0", 0) == 0xe6a501d3u, "nonce_sim/0");
    DOGRULA(d, d7_u32(0xb1536801d0cf436cULL, "ida/a1/nonce_board/1/0", 0) == 0x0d2c29dcu, "nonce_board/1/0");
    DOGRULA(d, (d7_u32(1, "ida/a1/sess/0", 0) & 0xFFFFu) == 18101u, "sess/0 n=0");
    DOGRULA(d, (d7_u32(1, "ida/a1/sess/0", 1) & 0xFFFFu) == 6699u, "sess/0 n=1");
    Kopru k(KopruAyari{});
    DOGRULA(d, k.run_id() == 0x3f25d322u, "SIL run_id a1/run_id'den (A1-K126)");
    return d.hata;
}

int test_el_sikisma()
{
    Denetci d;
    IcDuzenek z;
    // İlk SIM_HELLO'nun nonce'u (A1-K114).
    z.k->el_sikisma_baslat(0);
    std::vector<std::uint8_t> v;
    z.k->giden_al(v);
    ida_link_alici_t a{};
    (void)ida_link_alici_kur(&a, IDA_LINK_ROL_KART);
    std::size_t tuk = 0;
    ida_link_cerceve_t c;
    DOGRULA(d, ida_link_al(&a, v.data(), v.size(), &tuk, &c) == IDA_LINK_CERCEVE &&
                   c.msgid == MAVLINK_MSG_ID_IDA_SIM_HELLO,
            "SIM_HELLO yazıldı");
    mavlink_ida_sim_hello_t h{};
    (void)ida_link_coz(&c, c.msgid, &h, sizeof h);
    DOGRULA(d, h.nonce == 0xe6a501d3u && h.run_id == 0x3f25d322u && h.ctrl_period_ns == 5'000'000u &&
                   h.dialect_hash == IDA_LINK_DIALECT_HASH,
            "SIM_HELLO alanları");
    (void)z.sim_ucu->yaz(v.data(), v.size());
    z.pompala();
    DOGRULA(d, z.k->durum() == OturumDurumu::kosuyor, "oturum kuruldu");
    DOGRULA(d, z.k->sess() == 18101u && z.karsi->sess() == 18101u, "sess = ida/a1/sess/0 (" + s(z.k->sess()) + ")");
    DOGRULA(d, z.k->sayaclar().oturum_kurulan == 1u, "oturum_kurulan 1");
    DOGRULA(d, z.k->kart().boot_count == 1u && z.k->kart().role == 2u, "BOARD_HELLO: boot_count 1, rol SIL");
    return d.hata;
}

int test_on_bin_parti()
{
    Denetci d;
    IcDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    std::uint32_t eksik = 0;
    std::uint32_t onceki = 0;
    for (int n = 0; n < 10'000; ++n) {
        const std::uint32_t seq = z.periyot();
        if (!z.k->yuva_tamam(seq)) {
            ++eksik;
        }
        if (n > 0 && seq != onceki + 1u) {
            ++eksik;
        }
        onceki = seq;
    }
    const Sayaclar& sy = z.k->sayaclar();
    DOGRULA(d, eksik == 0u, "her partinin yuvası kendi periyodunda tamam (eksik " + s(eksik) + ")");
    DOGRULA(d, sy.parti_gonderilen == 10'000u, "10 000 parti");
    DOGRULA(d, sy.parti_kacirildi == 0u && sy.yanit_ret_gec == 0u && sy.yanit_ret_yinelenen == 0u &&
                   sy.yanit_ret_oturum == 0u && sy.yanit_ret_eski_uretim == 0u,
            "kaçırılan ya da reddedilen yanıt yok");
    DOGRULA(d, sy.alan_ret == 0u && sy.bag_ret == 0u, "Tablo 4.6 retleri yok (alan " + s(sy.alan_ret) + ", bağ " +
                                                          s(sy.bag_ret) + ")");
    DOGRULA(d, sy.rx_crc_err == 0u && sy.rx_resync_bytes == 0u && sy.rx_seq_gap == 0u, "çerçeve hatası yok");
    DOGRULA(d, sy.calistirma_gecersiz == 0u, "çalıştırma zamanlama bakımından geçerli");
    DOGRULA(d, sy.batch_incomplete == 0u, "eksik parti yok (A1-K30)");
    const KarsiIstatistik& ki = z.karsi->istatistik();
    DOGRULA(d, ki.cagri_step == 10'000u && ki.cagri_tick == 0u, "10 000 step çağrısı (" + s(ki.cagri_step) + ")");
    DOGRULA(d, ki.sample_rejected == 0u && ki.sample_future == 0u && ki.imu_rejected == 0u,
            "çekirdek örnek reddetmedi (A4-K33): " + s(ki.sample_rejected) + "/" + s(ki.sample_future) + "/" +
                s(ki.imu_rejected));
    DOGRULA(d, z.k->kart().sayac.var && z.k->kart().sayac.sample_rejected == 0u, "IDA_CORE_COUNTERS alındı");
    // W4-K14 (SIL): ACTUATOR dışı çıktı sonraki ACTUATOR'la işlenir; son çağrınınki kapanışta.
    DOGRULA(d, sy.cagri_izi == 9'999u, "son çağrı dışında her çağrının izi işlendi (" + s(sy.cagri_izi) + ")");
    z.k->oturum_kapat(static_cast<std::uint8_t>(IDAW_CLOSE_REASON_RUN_END), z.t);
    DOGRULA(d, z.k->sayaclar().cagri_izi == 10'000u, "kapanışta son izi de işlendi (" + s(z.k->sayaclar().cagri_izi) + ")");
    return d.hata;
}

int test_kopma_yeniden()
{
    Denetci d;
    IcDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    std::uint32_t seq = 0;
    for (int n = 0; n < 200; ++n) {
        seq = z.periyot();
    }
    const std::uint16_t s1 = z.k->sess();
    // Taşıma koptu (SIL: TCP kapandı; A1-K99): iki taraf oturumu bitirir.
    z.sim_ucu->kapat();
    (void)z.karsi->pompala();
    z.k->akis_kapandi(z.t);
    DOGRULA(d, z.k->durum() == OturumDurumu::kapali, "oturum bitti");
    DOGRULA(d, z.k->sayaclar().oturum_kopma == 1u, "oturum_kopma 1");
    DOGRULA(d, (z.k->sayaclar().calistirma_gecersiz & kGecPlanlanmamisBitis) != 0u, "planlanmamış bitiş geçersiz");
    // Yeni bağlantı, yeni el sıkışma.
    auto [a, b] = bellek_cifti();
    z.sim_ucu = std::move(a);
    z.karsi->tasima_degistir(std::move(b));
    DOGRULA(d, z.el_sikis(), "yeniden el sıkışma");
    DOGRULA(d, z.k->sess() != s1 && z.k->sess() != 0u, "yeni sess");
    const std::uint32_t seq2 = z.periyot();
    DOGRULA(d, seq2 == seq + 1u, "batch_seq çalıştırma içinde kesintisiz (A1 §4.2)");
    for (int n = 0; n < 50; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, z.k->yuva_tamam(z.k->son_parti()), "yeni oturumda yuvalar tamam");
    // Planlı kapanış ve yeni çalıştırma: kartta sessions_since_boot > 0 → REBOOT (A1-K18, K17).
    z.k->oturum_kapat(static_cast<std::uint8_t>(IDAW_CLOSE_REASON_RUN_END), z.t);
    z.pompala();
    DOGRULA(d, z.k->durum() == OturumDurumu::kapali, "planlı kapanış");
    DOGRULA(d, z.el_sikis(), "REBOOT sonrası oturum");
    DOGRULA(d, z.karsi->istatistik().yeniden_kurulum == 1u, "karşı taraf yeniden başladı (REBOOT)");
    DOGRULA(d, z.k->kart().boot_count == 2u && z.k->kart().sessions_since_boot == 0u, "yeni açılış");
    for (int n = 0; n < 50; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, z.k->yuva_tamam(z.k->son_parti()), "REBOOT sonrası yuvalar tamam");
    DOGRULA(d, z.k->sayaclar().oturum_kurulan == 3u, "üç oturum kuruldu");
    return d.hata;
}

int test_yanlis_dialect()
{
    Denetci d;
    KopruAyari ka;
    ka.dialect_hash = IDA_LINK_DIALECT_HASH ^ 1u;
    IcDuzenek z(ka);
    (void)z.el_sikis();
    DOGRULA(d, z.k->durum() == OturumDurumu::reddedildi, "çalıştırma başlamadı (H25)");
    DOGRULA(d, z.k->kart().son_oturum_reddi == IDAW_SESSION_RESULT_REJECT_DIALECT, "REJECT_DIALECT");
    DOGRULA(d, z.karsi->sess() == 0u, "kart oturumsuz (IDLE)");
    DOGRULA(d, z.periyot() == 0u, "oturumsuz parti gönderilmez");
    return d.hata;
}
