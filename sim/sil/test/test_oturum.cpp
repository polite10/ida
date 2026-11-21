// test_oturum.cpp — kabul 1: sahte sim ile gerçek TCP üzerinden oturum (el sıkışma, 1000 parti,
// her partiye bir aktüatör çıktısı, kopma ve yeniden bağlanma, oturum sonu) ve sim zamanının
// geri başlaması (A4-K11).
#include <algorithm>
#include <chrono>
#include <cstdio>

#include "test_destek.hpp"
#include "turetme.hpp"

namespace ida::sil::test {

namespace {

constexpr std::uint64_t kTohum = UINT64_C(0xb1536801d0cf436c);   // A5 Ek D.7 a1/board_tohum

constexpr unsigned u(unsigned x)
{
    return x;
}

// Oturum sonu çağrısının izi (A1-K22 (3), call_kind = SESSION_END).
bool oturum_sonu_izi_bekle(SahteSim& sim)
{
    return sim.bekle(MAVLINK_MSG_ID_IDA_CALL_TRACE, nullptr, 5000, [](const Cerceve& x) {
        return u(x.coz<mavlink_ida_call_trace_t>().call_kind) == IDAW_CALL_KIND_SESSION_END;
    });
}

void sure_yaz(const char* ad, std::vector<double> us)
{
    if (us.empty()) {
        return;
    }
    std::sort(us.begin(), us.end());
    double toplam = 0.0;
    for (const double x : us) {
        toplam += x;
    }
    const auto yuzde = [&us](double p) {
        const auto i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(us.size() - 1u));
        return us[i];
    };
    std::printf("  ölçüm: %s: n %zu, ort %.1f us, p50 %.1f us, p99 %.1f us, en çok %.1f us\n", ad,
                us.size(), toplam / static_cast<double>(us.size()), yuzde(50.0), yuzde(99.0),
                us.back());
}

void kosucu_suresi_yaz(const Istatistik& ist)
{
    const auto yaz = [](const char* ad, const SureIstatistigi& s) {
        if (s.sayi == 0u) {
            return;
        }
        std::printf("  ölçüm: %s: n %llu, ort %.1f us, p50 %.0f us, p99 %.0f us, en çok %.1f us\n",
                    ad, static_cast<unsigned long long>(s.sayi),
                    static_cast<double>(s.toplam_ns) / static_cast<double>(s.sayi) / 1000.0,
                    s.yuzdelik_us(50.0), s.yuzdelik_us(99.0),
                    static_cast<double>(s.en_cok_ns) / 1000.0);
    };
    yaz("koşucu çağrı turu (take..put_out, N_bg dahil)", ist.tur);
    yaz("koşucu çekirdek çağrısı (step/tick)", ist.cekirdek);
}

}  // namespace

int test_oturum(const Baglam&)
{
    Duzen d;
    if (!duzen_kur(&d, [](Ayarlar& a) { a.a1_tohum = kTohum; })) {
        return hata_sayisi();
    }
    // El sıkışma: nonce A1-K114 ve A5 Ek D.7 vektörü (boot_count 1, deneme 0).
    mavlink_ida_board_hello_t h{};
    if (!KONTROL(el_sikis(*d.sim, 0xe6a501d3u, 18101u, &h))) {
        return hata_sayisi();
    }
    KONTROL(h.nonce == 0x0d2c29dcu);
    KONTROL(u(h.role) == IDAW_ROLE_SIL);
    KONTROL(h.boot_count == 1u && h.boot_count_ok == 1u);
    KONTROL(u(h.reset_cause) == IDA_RESET_SIL_START);
    KONTROL(u(h.init_rc) == IDA_RC_PARAM_DEFAULTS);   // blok yok
    KONTROL(h.param_blob_len == 0u && h.param_blob_crc == 0u);
    KONTROL(h.prev_sess == 0u && h.sessions_since_boot == 0u);
    KONTROL(h.fw_version == kSurumSayisi);
    KONTROL(u(h.port_major) == IDA_PORT_VERSION_MAJOR && u(h.port_minor) == IDA_PORT_VERSION_MINOR);

    // 1000 parti; her birine tek bir ACTUATOR, çekirdek dönüşü OK; out_seq artar (A1-K105).
    std::vector<double> gidis_donus;
    std::uint32_t onceki_out = 0;
    bool ilk = true;
    for (std::uint32_t seq = 1; seq <= 1000u; ++seq) {
        const auto t0 = std::chrono::steady_clock::now();
        Cerceve a;
        if (!KONTROL(parti(*d.sim, seq, &a))) {
            return hata_sayisi();
        }
        gidis_donus.push_back(
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count());
        const auto m = a.coz<mavlink_ida_actuator_t>();
        KONTROL(u(m.core_rc) == IDA_RC_OK);
        KONTROL(m.sess == 18101u);
        KONTROL(ilk || m.out_seq == onceki_out + 1u);
        ilk = false;
        onceki_out = m.out_seq;
    }
    {
        std::size_t n_act = 0;
        std::uint32_t beklenen = 1;
        bool sira = true;
        for (const Cerceve& c : d.sim->cerceveler()) {
            if (c.msgid() == MAVLINK_MSG_ID_IDA_ACTUATOR) {
                sira = sira && c.coz<mavlink_ida_actuator_t>().batch_seq == beklenen;
                ++beklenen;
                ++n_act;
            }
            KONTROL(c.msgid() != MAVLINK_MSG_ID_IDA_SESSION_CLOSE);
        }
        KONTROL(n_act == 1000u);
        KONTROL(sira);
    }
    sure_yaz("sahte sim parti gidiş-dönüşü (ilk üyeden ACTUATOR'a, 1000 parti)", gidis_donus);

    // Kopma: sim bağlantıyı kapatır; koşucu oturumu bitirir ve yeniden bağlanır (A1-K99).
    d.sim->kapat();
    if (!KONTROL(yeniden_kabul(&d))) {
        return hata_sayisi();
    }
    mavlink_ida_board_hello_t h2{};
    if (!KONTROL(el_sikis(*d.sim, 0x55AA1234u, 777u, &h2))) {
        return hata_sayisi();
    }
    KONTROL(h2.boot_count == 1u);                // aynı açılış
    KONTROL(h2.sessions_since_boot == 1u && h2.prev_sess == 18101u);
    KONTROL(h2.nonce == turetme::u32_sifirsiz(kTohum, turetme::nonce_board_etiketi(1u, 1u)));
    for (std::uint32_t seq = 1001; seq <= 1100u; ++seq) {
        Cerceve a;
        if (!KONTROL(parti(*d.sim, seq, &a))) {
            return hata_sayisi();
        }
        const auto m = a.coz<mavlink_ida_actuator_t>();
        KONTROL(m.sess == 777u && u(m.core_rc) == IDA_RC_OK);
        KONTROL(m.out_seq == onceki_out + 1u);   // oturumlar arasında sıfırlanmaz
        onceki_out = m.out_seq;
    }

    // Oturum sonu: SESSION_CLOSE -> oturum sonu çağrısı ve izi.
    mavlink_ida_session_close_t sc{};
    sc.sess = 777u;
    sc.reason = static_cast<std::uint8_t>(IDAW_CLOSE_REASON_RUN_END);
    d.sim->gonder(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, sc);
    KONTROL(oturum_sonu_izi_bekle(*d.sim));

    KONTROL(d.kosucu->bitir() == Cikis::Tamam);
    const Istatistik& ist = d.kosucu->istatistik();
    KONTROL(ist.cagri[IDA_LINK_CAGRI_STEP] == 1100u);
    KONTROL(ist.cagri[IDA_LINK_CAGRI_IZGARA] == 0u);           // SIL'de ızgara yok
    KONTROL(ist.cagri[IDA_LINK_CAGRI_OTURUM_SONU] == 2u);      // kopma + SESSION_CLOSE
    KONTROL(ist.arka_plan == 1100u);                            // N_bg = 1, step başına
    KONTROL(ist.baglanti == 2u);
    KONTROL(ist.pencere_ihlali == 0u);
    std::printf("  çekirdek boyu %u B (alan %u B)\n", ist.core_size, kCekirdekAlani);
    kosucu_suresi_yaz(ist);
    return hata_sayisi();
}

int test_geri_donus(const Baglam&)
{
    Duzen d;
    if (!duzen_kur(&d)) {
        return hata_sayisi();
    }
    if (!KONTROL(el_sikis(*d.sim, 0x11u, 10u))) {
        return hata_sayisi();
    }
    for (std::uint32_t seq = 1; seq <= 20u; ++seq) {
        if (!KONTROL(parti(*d.sim, seq))) {
            return hata_sayisi();
        }
    }
    // A1-K128: süreksizlikten önce SESSION_CLOSE(RUN_END), yeni el sıkışma, yeni neslin TF'si.
    mavlink_ida_session_close_t sc{};
    sc.sess = 10u;
    sc.reason = static_cast<std::uint8_t>(IDAW_CLOSE_REASON_RUN_END);
    d.sim->gonder(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, sc);
    mavlink_ida_board_hello_t h{};
    if (!KONTROL(el_sikis(*d.sim, 0x22u, 11u, &h))) {
        return hata_sayisi();
    }
    KONTROL(h.boot_count == 1u);
    {
        PartiKurucu p(*d.sim);
        p.imu(1u, parti_zamani(1u));
        p.tf(1u, parti_zamani(1u), true);   // 5 ms < son TF (100 ms): sim zamanı geri başladı
    }
    // Koşucu bağlantıyı kapatır (oturum iki tarafta biter), çekirdeği yeni boot_count ile kurar
    // ve yeniden bağlanır.
    KONTROL(d.sim->kapanmayi_bekle(5000));
    for (const Cerceve& c : d.sim->cerceveler()) {
        if (c.msgid() == MAVLINK_MSG_ID_IDA_ACTUATOR) {
            KONTROL(c.coz<mavlink_ida_actuator_t>().sess == 10u);   // yeni nesle çıktı yok
        }
    }
    if (!KONTROL(yeniden_kabul(&d))) {
        return hata_sayisi();
    }
    mavlink_ida_board_hello_t h2{};
    if (!KONTROL(el_sikis(*d.sim, 0x33u, 12u, &h2))) {
        return hata_sayisi();
    }
    KONTROL(h2.boot_count == 2u && h2.boot_count_ok == 1u);
    KONTROL(u(h2.reset_cause) == IDA_RESET_SIL_REBOOT);
    KONTROL(h2.sessions_since_boot == 0u);
    for (std::uint32_t seq = 1; seq <= 20u; ++seq) {
        Cerceve a;
        if (!KONTROL(parti(*d.sim, seq, &a))) {
            return hata_sayisi();
        }
        KONTROL(u(a.coz<mavlink_ida_actuator_t>().core_rc) == IDA_RC_OK);
    }
    KONTROL(d.kosucu->bitir() == Cikis::Tamam);
    KONTROL(d.kosucu->istatistik().geri_donus == 1u);
    KONTROL(d.kosucu->istatistik().pencere_ihlali == 0u);
    return hata_sayisi();
}

}  // namespace ida::sil::test
