// test_program.cpp — ida_sil programının kendisi (komut satırı, depo dosyası, çıkış kodu):
// program ayrı süreç olarak başlatılır, sahte sim ona TCP üzerinden bağlanır.
#include <atomic>
#include <cstdlib>
#include <thread>

#include "test_destek.hpp"
#include "turetme.hpp"

namespace ida::sil::test {

namespace {

// Programı başlatır, bir oturum koşturur; dinleyici kapanınca program bekleme süresi sonunda
// 0 ile çıkar. BOARD_HELLO'yu döndürür.
bool bir_kosu(const Baglam& b, const std::filesystem::path& depo, std::uint32_t nonce,
              mavlink_ida_board_hello_t* h)
{
    tasima::Dinleyici d = tasima::Dinleyici::ac(0);
    if (!KONTROL(d.gecerli())) {
        return false;
    }
    std::string komut = "\"" + b.program + "\" --a1-port " + std::to_string(d.port()) +
                        " --a1-seed 0000000000000001 --bekleme-ms 1000 --gunluk 1 --olcum" +
                        " --depo " + depo.string();
#if defined(_WIN32)
    komut = "\"" + komut + "\"";   // cmd /c tırnak kuralı
#endif
    std::atomic<int> donus{-1};
    std::thread t([&komut, &donus] { donus.store(std::system(komut.c_str())); });
    bool tamam = false;
    {
        tasima::Soket s = d.kabul(10000);
        if (KONTROL(s.gecerli())) {
            SahteSim sim(std::move(s));
            tamam = KONTROL(el_sikis(sim, nonce, 55u, h));
            for (std::uint32_t seq = 1; tamam && seq <= 100u; ++seq) {
                tamam = KONTROL(parti(sim, seq));
            }
            mavlink_ida_session_close_t sc{};
            sc.sess = 55u;
            sc.reason = static_cast<std::uint8_t>(IDAW_CLOSE_REASON_RUN_END);
            sim.gonder(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, sc);
            KONTROL(sim.bekle(MAVLINK_MSG_ID_IDA_CALL_TRACE, nullptr, 5000));
            // Önce dinleyici kapanır (yeniden bağlanma reddedilir), sonra bağlantı; program
            // bekleme süresi sonunda 0 ile çıkar.
            d.kapat();
        }
    }
    d.kapat();
    t.join();
    KONTROL(donus.load() == 0);
    return tamam;
}

}  // namespace

int test_program(const Baglam& b)
{
    if (!KONTROL(!b.program.empty())) {
        return hata_sayisi();
    }
    const std::filesystem::path depo = b.gecici / "depo.bin";
    mavlink_ida_board_hello_t h{};
    if (!KONTROL(bir_kosu(b, depo, 0x77u, &h))) {
        return hata_sayisi();
    }
    KONTROL(h.boot_count == 1u && h.boot_count_ok == 1u);
    KONTROL(h.nonce == turetme::u32_sifirsiz(1u, turetme::nonce_board_etiketi(1u, 0u)));
    KONTROL(std::filesystem::exists(depo));
    // İkinci başlatma: açılış sayacı dosyadan (A4-K86).
    mavlink_ida_board_hello_t h2{};
    if (KONTROL(bir_kosu(b, depo, 0x78u, &h2))) {
        KONTROL(h2.boot_count == 2u && h2.boot_count_ok == 1u);
        KONTROL(h2.nonce == turetme::u32_sifirsiz(1u, turetme::nonce_board_etiketi(2u, 0u)));
    }
    return hata_sayisi();
}

}  // namespace ida::sil::test
