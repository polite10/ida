// test_pencere.cpp — kabul 4: çağrı penceresi (W3-K10). Çekirdek çağrısı sürerken (take ile
// put_out arası) sahte sim yeni baytlar gönderir: bir daraltıcı (DISARM) ve sonraki partinin
// IMU'su. Koşucu bu baytları pencere kapanana kadar işlemez: modülün ihlal yolu (oturumun
// PROTOCOL ile bitmesi) hiç çalışmaz, koşucunun ihlal sayacı sıfırdır, DISARM'ın sonucu ve
// etkisi sonraki çağrıdadır.
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>

#include "test_destek.hpp"

namespace ida::sil::test {

namespace {

constexpr unsigned u(unsigned x)
{
    return x;
}

struct Pencere {
    std::mutex mu;
    std::condition_variable cv;
    std::uint32_t hedef = 0;
    bool icinde = false;
    bool gonderildi = false;
    bool zaman_asimi = false;
    bool acik_goruldu = false;
};

}  // namespace

int test_pencere(const Baglam&)
{
    constexpr std::uint32_t kHedef = 10u;
    Pencere p;
    p.hedef = kHedef;
    Kancalar k;
    k.cagri_icinde = [&p](int tur, const ida_in_t& in) {
        if (tur != static_cast<int>(IDA_LINK_CAGRI_STEP) || in.tf.seq != p.hedef) {
            return;
        }
        std::unique_lock<std::mutex> kilit(p.mu);
        p.icinde = true;
        p.cv.notify_all();
        // Sim baytları gönderene kadar çağrı penceresi açık kalır.
        if (!p.cv.wait_for(kilit, std::chrono::seconds(5), [&p] { return p.gonderildi; })) {
            p.zaman_asimi = true;
        }
        kilit.unlock();
        // Baytların koşucunun soket tamponuna ulaşması için kısa süre (geri döngü).
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    };
    Duzen d;
    if (!duzen_kur(&d, {}, std::move(k))) {
        return hata_sayisi();
    }
    if (!KONTROL(el_sikis(*d.sim, 0xABCu, 321u))) {
        return hata_sayisi();
    }
    for (std::uint32_t seq = 1; seq < kHedef; ++seq) {
        if (!KONTROL(parti(*d.sim, seq))) {
            return hata_sayisi();
        }
    }
    // Hedef parti: üyeler ve TF; ACTUATOR beklenmeden çağrının başlaması beklenir.
    {
        PartiKurucu pk(*d.sim);
        pk.imu(kHedef, parti_zamani(kHedef));
        pk.tf(kHedef, parti_zamani(kHedef), true);
    }
    PartiKurucu sonraki(*d.sim);
    {
        std::unique_lock<std::mutex> kilit(p.mu);
        if (!KONTROL(p.cv.wait_for(kilit, std::chrono::seconds(5), [&p] { return p.icinde; }))) {
            return hata_sayisi();
        }
        // Çağrı sürüyor: sonraki partinin daraltıcısı ve IMU'su şimdi gönderilir.
        sonraki.simple(77u, static_cast<std::uint8_t>(IDA_CMD_DISARM), 0u);
        sonraki.imu(kHedef + 1u, parti_zamani(kHedef + 1u));
        p.gonderildi = true;
        p.cv.notify_all();
    }
    Cerceve act;
    KONTROL(d.sim->bekle(MAVLINK_MSG_ID_IDA_ACTUATOR, &act, 5000, [](const Cerceve& x) {
        return x.coz<mavlink_ida_actuator_t>().batch_seq == kHedef;
    }));
    const std::size_t act_sira = d.sim->imlec();
    // Pencere kapandıktan sonra TF(11): DISARM ve IMU aynı partide.
    sonraki.tf(kHedef + 1u, parti_zamani(kHedef + 1u), true);
    Cerceve act2;
    KONTROL(d.sim->bekle(MAVLINK_MSG_ID_IDA_ACTUATOR, &act2, 5000, [](const Cerceve& x) {
        return x.coz<mavlink_ida_actuator_t>().batch_seq == kHedef + 1u;
    }));
    KONTROL(act2.coz<mavlink_ida_actuator_t>().sess == 321u);   // oturum sürüyor
    KONTROL(u(act2.coz<mavlink_ida_actuator_t>().core_rc) == IDA_RC_OK);
    mavlink_ida_cmd_result_t r{};
    KONTROL(sonuc_bekle(*d.sim, 77u, &r));
    KONTROL(u(r.ack) == IDA_ACK_ACCEPTED && r.sess == 321u);
    {
        // DISARM'ın sonucu hedef çağrının ACTUATOR'ından sonra gelir (pencere dışında işlendi).
        const auto hepsi = d.sim->cerceveler();
        for (std::size_t i = 0; i < hepsi.size(); ++i) {
            const Cerceve& c = hepsi[i];
            if (c.msgid() == MAVLINK_MSG_ID_IDA_CMD_RESULT &&
                c.coz<mavlink_ida_cmd_result_t>().cmd_id == 77u) {
                KONTROL(i >= act_sira);
            }
            // Modülün ihlal yolu oturumu PROTOCOL ile bitirirdi (W3-K10).
            KONTROL(c.msgid() != MAVLINK_MSG_ID_IDA_SESSION_CLOSE);
        }
    }
    for (std::uint32_t seq = kHedef + 2u; seq < kHedef + 30u; ++seq) {
        KONTROL(parti(*d.sim, seq));
    }
    KONTROL(!p.zaman_asimi);
    KONTROL(d.kosucu->bitir() == Cikis::Tamam);
    KONTROL(d.kosucu->istatistik().pencere_ihlali == 0u);
    KONTROL(d.kosucu->istatistik().cagri[IDA_LINK_CAGRI_OTURUM_SONU] == 1u);   // yalnız bitişte
    return hata_sayisi();
}

}  // namespace ida::sil::test
