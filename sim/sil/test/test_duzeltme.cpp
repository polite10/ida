// test_duzeltme.cpp — W4-sil sol incelemesinin karşı örnekleri (docs/gorevler/inceleme/
// W4-sil-sol.md) ve W4-K10:
//   alim_damgasi          #1  A4-K14: SIL'de bütün alım damgaları çağrının now_w'si
//   tf_reddi              #2  protokolün reddettiği TF saati ve zamanlayıcıları ilerletmez
//   reboot_tfsiz_kapanis  #5  REBOOT → yeni oturum → TF'siz kapanış: saat geri gitmez;
//                             W4-K10 (b): REBOOT sonrası saat 0'dan, olağan oturum düşmez
//   core_ok_sifir         notlar: health.core_ok = 0 → SAFE_STATE, çekirdek bir daha çağrılmaz
//   durdur_gonderim       #6  karşı taraf okumazken durdur() tamamlanır
//   depo_kalicilik        #3  geçici dosya → kalıcılaştırma → doğrulama → adlandırma → dizin;
//                             her adımın hatasında OK verilmez
#include <chrono>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <thread>

#include "depo.hpp"
#include "test_destek.hpp"

namespace ida::sil::test {

namespace {

constexpr unsigned u(unsigned x)
{
    return x;
}

using Saat = std::chrono::steady_clock;

}  // namespace

// ---- #1 ------------------------------------------------------------------------------------

int test_alim_damgasi(const Baglam&)
{
    struct Gozlem {
        std::mutex mu;
        unsigned adim = 0, uyusmaz = 0, ping = 0, algilama = 0, ornek = 0, bayat = 0;
    } g;
    Kancalar k;
    k.cagri_icinde = [&g](int tur, const ida_in_t& in) {
        if (tur != static_cast<int>(IDA_LINK_CAGRI_STEP)) {
            return;
        }
        std::lock_guard<std::mutex> kilit(g.mu);
        ++g.adim;
        const std::int64_t w = in.now_w.ns;
        unsigned hata = 0;
        hata += in.tf.t_rx.ns != w ? 1u : 0u;
        hata += in.imu.hdr.t_rx.ns != w ? 1u : 0u;
        for (unsigned i = 0; i < in.n_samples && i < IDA_PORT_MAX_SAMPLES; ++i) {
            hata += in.samples[i].hdr.t_rx.ns != w ? 1u : 0u;
            ++g.ornek;
        }
        for (unsigned i = 0; i < in.n_det_batch && i < IDA_PORT_MAX_DET_BATCH; ++i) {
            hata += in.det_batch_t_rx[i].ns != w ? 1u : 0u;
        }
        for (unsigned i = 0; i < in.n_det && i < IDA_PORT_MAX_DET_IN; ++i) {
            hata += in.det_t_rx[i].ns != w ? 1u : 0u;
            ++g.algilama;
        }
        if (in.ping360 != nullptr) {
            hata += in.ping360->hdr.t_rx.ns != w ? 1u : 0u;
            ++g.ping;
        }
        g.uyusmaz += hata;
    };
    k.cagri_sonrasi = [&g](int tur, const ida_in_t&, ida_out_t& out) {
        if (tur == static_cast<int>(IDA_LINK_CAGRI_STEP) &&
            ((out.status.fs_reasons & IDA_FSR_IMU_STALE) != 0u ||
             (out.fc.inhibit_mask & IDA_FCI_IMU_STALE) != 0u)) {
            std::lock_guard<std::mutex> kilit(g.mu);
            ++g.bayat;
        }
    };
    Duzen d;
    if (!duzen_kur(&d, {}, std::move(k))) {
        return hata_sayisi();
    }
    if (!KONTROL(el_sikis(*d.sim, 0x101u, 31u))) {
        return hata_sayisi();
    }
    // İlk parti büyük bir sim zamanında (10 s): yeni alınan IMU bayat sayılmamalı.
    constexpr std::int64_t kTaban = 10000000000;
    for (std::uint32_t seq = 1; seq <= 40u; ++seq) {
        const std::int64_t t = kTaban + parti_zamani(seq);
        PartiKurucu p(*d.sim);
        p.imu(seq, t);
        p.baro(seq, t - 1000000);
        p.mag(seq, t - 2000000);
        p.emniyet(seq, t, 1u);
        if (seq % 5u == 0u) {
            p.algilama(seq, seq, t - 3000000);
        }
        if (seq % 7u == 0u) {
            p.ping360(seq, t - 4000000);
        }
        p.tf(seq, t, true);
        if (!KONTROL(d.sim->bekle(MAVLINK_MSG_ID_IDA_ACTUATOR, nullptr, 5000,
                                  [seq](const Cerceve& x) {
                                      return x.coz<mavlink_ida_actuator_t>().batch_seq == seq;
                                  }))) {
            return hata_sayisi();
        }
    }
    KONTROL(d.kosucu->bitir() == Cikis::Tamam);
    std::printf("  adım %u, örnek %u, algılama %u, ping %u, uyuşmaz damga %u, IMU bayat %u\n",
                g.adim, g.ornek, g.algilama, g.ping, g.uyusmaz, g.bayat);
    KONTROL(g.adim == 40u);
    KONTROL(g.ornek >= 120u && g.algilama >= 8u && g.ping >= 5u);
    KONTROL(g.uyusmaz == 0u);
    KONTROL(g.bayat == 0u);
    return hata_sayisi();
}

// ---- #2 ------------------------------------------------------------------------------------

namespace {

// (1, 5 ms) partisi, yinelenen TF (1, 1 s), (2, 10 ms) partisi. parca = 0: çerçeve çerçeve,
// kilitli adım; parca > 0: aynı bayt akışı o boyda parçalarla, beklemeden.
bool tf_reddi_kosusu(std::size_t parca, const std::vector<std::uint8_t>& kayit,
                     std::vector<std::uint8_t>* giden, std::vector<std::uint8_t>* gelen,
                     std::int64_t* ikinci_now_w)
{
    std::mutex mu;
    Kancalar k;
    k.cagri_icinde = [&mu, ikinci_now_w](int tur, const ida_in_t& in) {
        if (tur == static_cast<int>(IDA_LINK_CAGRI_STEP) && in.tf.seq == 2u) {
            std::lock_guard<std::mutex> kilit(mu);
            *ikinci_now_w = in.now_w.ns;
        }
    };
    Duzen d;
    if (!duzen_kur(&d, {}, std::move(k))) {
        return false;
    }
    if (parca == 0u) {
        if (!KONTROL(el_sikis(*d.sim, 0x202u, 42u))) {
            return false;
        }
        if (!KONTROL(parti(*d.sim, 1u))) {
            return false;
        }
        mavlink_ida_time_frame_t m{};
        m.t_sim_ns = 1000000000;
        m.batch_seq = 1u;
        m.sess = 42u;
        m.tf_flags = 0u;
        m.run_mode = static_cast<std::uint8_t>(IDAW_RUN_MODE_SIL);
        d.sim->gonder(MAVLINK_MSG_ID_IDA_TIME_FRAME, m);
        Cerceve a;
        if (!KONTROL(parti(*d.sim, 2u, &a))) {
            return false;
        }
        KONTROL(u(a.coz<mavlink_ida_actuator_t>().core_rc) == IDA_RC_OK);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } else {
        for (std::size_t i = 0; i < kayit.size(); i += parca) {
            const std::size_t n = std::min(parca, kayit.size() - i);
            d.sim->bayt_gonder(kayit.data() + i, n);
        }
        KONTROL(d.sim->bekle(MAVLINK_MSG_ID_IDA_ACTUATOR, nullptr, 5000, [](const Cerceve& x) {
            return x.coz<mavlink_ida_actuator_t>().batch_seq == 2u;
        }));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    *giden = d.sim->gonderilen();
    *gelen = d.sim->alinan();
    KONTROL(d.kosucu->bitir() == Cikis::Tamam);
    return true;
}

}  // namespace

int test_tf_reddi(const Baglam&)
{
    std::vector<std::uint8_t> giden, gelen1;
    std::int64_t now1 = -1;
    if (!KONTROL(tf_reddi_kosusu(0u, {}, &giden, &gelen1, &now1))) {
        return hata_sayisi();
    }
    KONTROL(now1 == parti_zamani(2u));   // reddedilen TF'nin 1 s'si saate girmedi
    {
        // Reddedilen TF zamanlayıcıları ilerletmez: iki ACTUATOR arasında KEEPALIVE yok
        // (1 s'lik saat, T_canlı = 100 ms'yi aşıp KEEPALIVE ürettirirdi).
        ida_link_alici_t al{};
        (void)ida_link_alici_kur(&al, IDA_LINK_ROL_SIM);
        unsigned act = 0, keepalive_arada = 0;
        std::size_t i = 0;
        while (i < gelen1.size()) {
            std::size_t t = 0;
            ida_link_cerceve_t c{};
            if (ida_link_al(&al, gelen1.data() + i, gelen1.size() - i, &t, &c) != IDA_LINK_CERCEVE ||
                t == 0u) {
                break;
            }
            i += t;
            if (c.msgid == MAVLINK_MSG_ID_IDA_ACTUATOR) {
                ++act;
            } else if (c.msgid == MAVLINK_MSG_ID_IDA_KEEPALIVE && act == 1u) {
                ++keepalive_arada;
            }
        }
        KONTROL(act == 2u);
        KONTROL(keepalive_arada == 0u);
    }
    for (const std::size_t parca : {1u, 7u, 1000u}) {
        std::vector<std::uint8_t> g2, gelen2;
        std::int64_t now2 = -1;
        if (!KONTROL(tf_reddi_kosusu(parca, giden, &g2, &gelen2, &now2))) {
            return hata_sayisi();
        }
        KONTROL(now2 == parti_zamani(2u));
        if (!KONTROL(gelen2 == gelen1)) {
            std::printf("  parça %zu: çıktı farklı (%zu / %zu B)\n", parca, gelen2.size(),
                        gelen1.size());
        }
    }
    return hata_sayisi();
}

// ---- #5 ve W4-K10 (b) -----------------------------------------------------------------------

int test_reboot_tfsiz_kapanis(const Baglam&)
{
    std::mutex mu;
    std::uint32_t geri_adim = 0;
    std::int64_t son_now = -1;
    bool azaldi = false;
    Kancalar k;
    k.cagri_sonrasi = [&](int, const ida_in_t& in, ida_out_t& out) {
        std::lock_guard<std::mutex> kilit(mu);
        geri_adim += out.status.cnt.wall_backstep;
        azaldi = azaldi || in.now_w.ns < son_now;
        son_now = in.now_w.ns;
    };
    Duzen d;
    if (!duzen_kur(&d, {}, std::move(k))) {
        return hata_sayisi();
    }
    if (!KONTROL(el_sikis(*d.sim, 0x301u, 51u))) {
        return hata_sayisi();
    }
    for (std::uint32_t seq = 1; seq <= 20u; ++seq) {   // saat 100 ms
        KONTROL(parti(*d.sim, seq));
    }
    // Yeni nonce'lu SIM_HELLO oturumu bitirir; SESSION_ACCEPT(REBOOT).
    mavlink_ida_sim_hello_t sh{};
    sh.dialect_hash = IDA_LINK_DIALECT_HASH;
    sh.nonce = 0x302u;
    sh.ctrl_period_ns = 5000000u;
    sh.proto_major = 1u;
    d.sim->gonder(MAVLINK_MSG_ID_IDA_SIM_HELLO, sh);
    Cerceve c;
    if (!KONTROL(d.sim->bekle(MAVLINK_MSG_ID_IDA_BOARD_HELLO, &c, 5000, [](const Cerceve& x) {
            return x.coz<mavlink_ida_board_hello_t>().peer_nonce == 0x302u;
        }))) {
        return hata_sayisi();
    }
    {
        std::lock_guard<std::mutex> kilit(mu);
        son_now = -1;   // yeni çekirdek ömrü
    }
    mavlink_ida_session_accept_t sa{};
    sa.nonce_sim = 0x302u;
    sa.nonce_board = c.coz<mavlink_ida_board_hello_t>().nonce;
    sa.result = static_cast<std::uint8_t>(IDAW_SESSION_RESULT_REBOOT);
    sa.run_mode = static_cast<std::uint8_t>(IDAW_RUN_MODE_SIL);
    d.sim->gonder(MAVLINK_MSG_ID_IDA_SESSION_ACCEPT, sa);
    // Yeni oturum; TF gelmeden kapanış: oturum sonu çağrısının now_w'si init'inkinden küçük
    // olmamalı (A4-K09), çekirdek geri adım saymamalı.
    if (!KONTROL(el_sikis(*d.sim, 0x303u, 52u))) {
        return hata_sayisi();
    }
    mavlink_ida_session_close_t sc{};
    sc.sess = 52u;
    sc.reason = static_cast<std::uint8_t>(IDAW_CLOSE_REASON_RUN_END);
    d.sim->gonder(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, sc);
    KONTROL(d.sim->bekle(MAVLINK_MSG_ID_IDA_CALL_TRACE, nullptr, 5000, [](const Cerceve& x) {
        return u(x.coz<mavlink_ida_call_trace_t>().call_kind) == IDAW_CALL_KIND_SESSION_END;
    }));
    // W4-K10 (b): REBOOT sonrası saat 0'dan; hazırlık oturumundan sonra sim zamanını baştan
    // başlatan olağan oturum "sim zamanı geri döndü" sayılmaz, bağlantı düşmez.
    if (!KONTROL(el_sikis(*d.sim, 0x304u, 53u))) {
        return hata_sayisi();
    }
    for (std::uint32_t seq = 1; seq <= 20u; ++seq) {
        Cerceve a;
        if (!KONTROL(parti(*d.sim, seq, &a))) {
            return hata_sayisi();
        }
        KONTROL(u(a.coz<mavlink_ida_actuator_t>().core_rc) == IDA_RC_OK);
    }
    KONTROL(!d.sim->kapandi());
    KONTROL(d.kosucu->bitir() == Cikis::Tamam);
    const Istatistik& ist = d.kosucu->istatistik();
    KONTROL(ist.geri_donus == 0u && ist.baglanti == 1u && ist.yeniden_kurulum == 1u);
    std::lock_guard<std::mutex> kilit(mu);
    KONTROL(geri_adim == 0u);
    KONTROL(!azaldi);
    return hata_sayisi();
}

// ---- core_ok = 0 -----------------------------------------------------------------------------

int test_core_ok_sifir(const Baglam&)
{
    Kancalar k;
    k.cagri_sonrasi = [](int tur, const ida_in_t& in, ida_out_t& out) {
        if (tur == static_cast<int>(IDA_LINK_CAGRI_STEP) && in.tf.seq == 10u) {
            out.health.core_ok = false;   // enjeksiyon: çekirdek iç tutarlılığını kaybetti
        }
    };
    Duzen d;
    if (!duzen_kur(&d, {}, std::move(k))) {
        return hata_sayisi();
    }
    if (!KONTROL(el_sikis(*d.sim, 0x401u, 61u))) {
        return hata_sayisi();
    }
    for (std::uint32_t seq = 1; seq <= 9u; ++seq) {
        KONTROL(parti(*d.sim, seq));
    }
    // 10. çağrının çıktısı yazılmadan oturum SAFE_STATE ile biter: yazımı başlamamış
    // çerçeveler (ACTUATOR dahil) silinir (A1-K22 (1)); sim iticileri nötre alır (A1-K23).
    {
        PartiKurucu p(*d.sim);
        p.imu(10u, parti_zamani(10u));
        p.tf(10u, parti_zamani(10u), true);
    }
    Cerceve c;
    KONTROL(d.sim->bekle(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, &c, 5000));
    KONTROL(u(c.coz<mavlink_ida_session_close_t>().reason) == IDAW_CLOSE_REASON_SAFE_STATE);
    // Koşucu kendiliğinden çıkar; sonraki parti çekirdeğe ulaşmaz.
    {
        PartiKurucu p(*d.sim);
        p.imu(11u, parti_zamani(11u));
        p.tf(11u, parti_zamani(11u), true);
    }
    KONTROL(d.kosucu->bitmesini_bekle(5000));
    KONTROL(d.kosucu->cikis() == Cikis::GuvenliBekleme);
    const Istatistik& ist = d.kosucu->istatistik();
    KONTROL(ist.cagri[IDA_LINK_CAGRI_STEP] == 10u);
    KONTROL(ist.cagri[IDA_LINK_CAGRI_OTURUM_SONU] == 0u);
    KONTROL(d.sim->kapanmayi_bekle(5000));
    return hata_sayisi();
}

// ---- #6 ------------------------------------------------------------------------------------

int test_durdur_gonderim(const Baglam&)
{
    Duzen d;
    if (!duzen_kur(&d)) {
        return hata_sayisi();
    }
    d.sim->alim_tamponu(4096);
    if (!KONTROL(el_sikis(*d.sim, 0x501u, 71u))) {
        return hata_sayisi();
    }
    std::uint32_t seq = 1;
    for (; seq <= 5u; ++seq) {
        KONTROL(parti(*d.sim, seq));
    }
    // Sim okumayı bırakır ama partileri göndermeyi sürdürür: koşucunun çıktısı birikir,
    // koşucu gönderimde bekler, okuması durur, sim'in gönderimi de durur.
    d.sim->okumayi_durdur();
    std::atomic<bool> gonderme_dur{false};
    std::atomic<std::uint32_t> ilerleme{0};
    SahteSim* sim = d.sim.get();
    std::thread gonderen([sim, &gonderme_dur, &ilerleme, seq]() {
        for (std::uint32_t s = seq; !gonderme_dur.load(); ++s) {
            const std::int64_t t = parti_zamani(s);
            mavlink_ida_imu_t m{};
            m.t_meas_ns = t;
            m.seq = s;
            m.window_ns = 5000000u;
            m.accel[2] = -9.81f;
            m.flags = static_cast<std::uint16_t>(IDA_HF_VALID);
            mavlink_ida_time_frame_t f{};
            f.t_sim_ns = t;
            f.batch_seq = s;
            f.sess = 71u;
            f.n_members = 1u;
            f.tf_flags = static_cast<std::uint8_t>(IDAW_TF_FLAGS_IMU);
            f.run_mode = static_cast<std::uint8_t>(IDAW_RUN_MODE_SIL);
            std::vector<std::uint8_t> b = sim->kodla(MAVLINK_MSG_ID_IDA_IMU, &m, sizeof m);
            const auto b2 = sim->kodla(MAVLINK_MSG_ID_IDA_TIME_FRAME, &f, sizeof f);
            b.insert(b.end(), b2.begin(), b2.end());
            if (!sim->bayt_gonder_durdurulabilir(b.data(), b.size(), &gonderme_dur)) {
                return;
            }
            ilerleme.store(s);
        }
    });
    // İlerleme 500 ms boyunca durunca koşucu gönderimde bekliyordur.
    bool durdu = false;
    const auto son = Saat::now() + std::chrono::seconds(60);
    std::uint32_t onceki = 0;
    auto degisim = Saat::now();
    while (Saat::now() < son) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        const std::uint32_t simdi = ilerleme.load();
        if (simdi != onceki) {
            onceki = simdi;
            degisim = Saat::now();
        } else if (onceki > 0u && Saat::now() - degisim > std::chrono::milliseconds(500)) {
            durdu = true;
            break;
        }
    }
    KONTROL(durdu);
    std::printf("  gönderim %u partide durdu\n", static_cast<unsigned>(onceki));
    const auto t0 = Saat::now();
    const Cikis c = d.kosucu->bitir();
    const auto sure =
        std::chrono::duration_cast<std::chrono::milliseconds>(Saat::now() - t0).count();
    std::printf("  durdur() %lld ms'de tamamlandı\n", static_cast<long long>(sure));
    KONTROL(c == Cikis::Tamam);
    KONTROL(sure < 2000);
    gonderme_dur.store(true);
    d.sim->kapat();
    gonderen.join();
    return hata_sayisi();
}

// ---- #3 ------------------------------------------------------------------------------------

int test_depo_kalicilik(const Baglam& b)
{
    using A = Depo::Adim;
    const std::filesystem::path yol = b.gecici / "depo.bin";
    const std::vector<std::uint8_t> eski = {1, 2, 3, 4};
    const std::vector<std::uint8_t> yeni = {9, 8, 7, 6, 5};
    {
        Depo d(yol.string(), 0u);
        (void)d.yukle();
        KONTROL(d.acilis_ayir().ok);
        KONTROL(u(d.kaydet(eski.data(), 4u, 1u).store) == IDA_PSTORE_OK);
        // Adımlar sırasıyla, her yazımda.
        const std::vector<A> beklenen = {A::Yazma, A::Kalicilastirma, A::Dogrulama,
                                         A::YenidenAdlandirma, A::DizinKaliciligi};
        KONTROL(d.adim_izi().size() == 2u * beklenen.size());
        for (std::size_t i = 0; i < d.adim_izi().size(); ++i) {
            KONTROL(d.adim_izi()[i] == beklenen[i % beklenen.size()]);
        }
    }
    struct Vaka {
        A adim;
        unsigned fail;
        bool eski_kalir;   // adlandırmadan önceki hatada dosya eskisidir
    };
    const Vaka vakalar[] = {{A::Yazma, IDA_PSFAIL_FLASH, true},
                            {A::Kalicilastirma, IDA_PSFAIL_FLASH, true},
                            {A::Dogrulama, IDA_PSFAIL_VERIFY, true},
                            {A::YenidenAdlandirma, IDA_PSFAIL_FLASH, true},
                            {A::DizinKaliciligi, IDA_PSFAIL_FLASH, false}};
    for (const Vaka& v : vakalar) {
        Depo d(yol.string(), 0u);
        KONTROL(d.yukle() == Depo::Yukleme::Tamam);
        d.hata_enjekte(v.adim);
        const auto s = d.kaydet(yeni.data(), 5u, 2u);
        KONTROL(u(s.store) == IDA_PSTORE_FAIL && u(s.fail) == v.fail);
        KONTROL(d.blok().has_value() && d.blok()->veri == eski);   // bellekte eski blok
        {
            Depo okunan(yol.string(), 0u);
            (void)okunan.yukle();
            KONTROL(okunan.blok().has_value());
            if (okunan.blok().has_value()) {
                KONTROL((okunan.blok()->veri == eski) == v.eski_kalir);
            }
        }
        std::filesystem::path gecici = yol;
        gecici += ".yeni";
        KONTROL(!std::filesystem::exists(gecici));
        KONTROL(!d.acilis_ayir().ok);   // aynı adımda hata: sayaç da kalıcılaşamaz
        // Sonraki vaka için eski içerik geri yazılır.
        Depo geri(yol.string(), 0u);
        (void)geri.yukle();
        KONTROL(u(geri.kaydet(eski.data(), 4u, 1u).store) == IDA_PSTORE_OK);
    }
    return hata_sayisi();
}

}  // namespace ida::sil::test
