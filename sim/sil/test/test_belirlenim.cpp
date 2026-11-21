// test_belirlenim.cpp — kabul 5: belirlenimcilik (A1-K102). Aynı başlangıç durumuyla:
//   koşu 1: kilitli adımla (her partide ACTUATOR beklenir) oturum; iki yönün baytları kaydedilir;
//   koşu 2: koşu 1'de sim'in gönderdiği bayt akışı, beklemeden ve değişen boyda parçalarla
//           yeniden gönderilir;
//   koşu 3: koşu 1'in aynısı (canlı, kilitli adım).
// Üç koşuda koşucunun çıktı baytları aynı olmalıdır (TCP parçalanmasına ve duvar saatine bağlı
// davranış yok).
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>

#include "test_destek.hpp"

namespace ida::sil::test {

namespace {

constexpr std::uint64_t kTohum = UINT64_C(0x5eed5eed5eed5eed);

void ayarla(Ayarlar& a)
{
    a.a1_tohum = kTohum;
    a.n_bg = 2u;
}

// Kilitli adımlı koşu; sim'in gönderdiği ve aldığı baytlar.
bool kilitli_kosu(std::vector<std::uint8_t>* giden, std::vector<std::uint8_t>* gelen)
{
    Duzen d;
    if (!duzen_kur(&d, ayarla)) {
        return false;
    }
    if (!KONTROL(el_sikis(*d.sim, 0x13572468u, 9876u))) {
        return false;
    }
    std::uint32_t cmd = 1;
    for (std::uint32_t seq = 1; seq <= 300u; ++seq) {
        const auto ek = [seq, &cmd](PartiKurucu& p) {
            if (seq == 20u) {
                p.simple(cmd++, static_cast<std::uint8_t>(IDA_CMD_PARAM_GET), 0u);
            }
            if (seq == 40u) {
                p.simple(cmd++, static_cast<std::uint8_t>(IDA_CMD_ARM), 0u);
            }
            if (seq == 60u) {
                p.simple(cmd++, static_cast<std::uint8_t>(IDA_CMD_SET_MODE),
                         static_cast<std::uint16_t>(IDA_MODE_GUIDED));
                p.guided(cmd++, static_cast<std::uint8_t>(IDA_SP_DEPTH | IDA_SP_YAW), 1.5f, 0.3f);
            }
            if (seq == 150u) {
                p.simple(cmd++, static_cast<std::uint8_t>(IDA_CMD_DISARM), 0u);
                p.simple(cmd++, static_cast<std::uint8_t>(IDA_CMD_ARM), 0u);
            }
            if (seq == 200u) {
                p.simple(cmd++, static_cast<std::uint8_t>(IDA_CMD_SET_MODE),
                         static_cast<std::uint16_t>(IDA_MODE_STABILIZE));
            }
        };
        if (!KONTROL(parti(*d.sim, seq, nullptr, ek))) {
            return false;
        }
    }
    mavlink_ida_session_close_t sc{};
    sc.sess = 9876u;
    sc.reason = static_cast<std::uint8_t>(IDAW_CLOSE_REASON_RUN_END);
    d.sim->gonder(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, sc);
    KONTROL(d.sim->bekle(MAVLINK_MSG_ID_IDA_CALL_TRACE, nullptr, 5000, [](const Cerceve& x) {
        return static_cast<unsigned>(x.coz<mavlink_ida_call_trace_t>().call_kind) ==
               IDAW_CALL_KIND_SESSION_END;
    }));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    *giden = d.sim->gonderilen();
    *gelen = d.sim->alinan();
    KONTROL(d.kosucu->bitir() == Cikis::Tamam);
    return true;
}

// Kayıtlı akışı parça parça, beklemeden gönderir; koşucunun çıktısını döndürür.
bool tekrar_kosu(const std::vector<std::uint8_t>& giden, std::size_t beklenen,
                 std::vector<std::uint8_t>* gelen)
{
    Duzen d;
    if (!duzen_kur(&d, ayarla)) {
        return false;
    }
    static constexpr std::size_t kParca[] = {1u, 7u, 64u, 3u, 500u, 29u, 2u, 1400u, 13u};
    std::size_t i = 0, j = 0;
    while (i < giden.size()) {
        const std::size_t n = std::min(kParca[j % (sizeof kParca / sizeof kParca[0])],
                                       giden.size() - i);
        d.sim->bayt_gonder(giden.data() + i, n);
        i += n;
        ++j;
    }
    KONTROL(d.sim->bayt_bekle(beklenen, 10000));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));   // fazlası gelmesin
    *gelen = d.sim->alinan();
    KONTROL(d.kosucu->bitir() == Cikis::Tamam);
    return true;
}

std::size_t ilk_fark(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b)
{
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return i;
        }
    }
    return n;
}

}  // namespace

int test_belirlenim(const Baglam&)
{
    std::vector<std::uint8_t> giden1, gelen1, giden3, gelen3, gelen2;
    if (!KONTROL(kilitli_kosu(&giden1, &gelen1))) {
        return hata_sayisi();
    }
    std::printf("  koşu 1: sim -> koşucu %zu B, koşucu -> sim %zu B\n", giden1.size(),
                gelen1.size());
    KONTROL(gelen1.size() > 10000u);
    if (!KONTROL(tekrar_kosu(giden1, gelen1.size(), &gelen2))) {
        return hata_sayisi();
    }
    if (!KONTROL(gelen2 == gelen1)) {
        std::printf("  koşu 2 farklı: boy %zu / %zu, ilk fark %zu\n", gelen2.size(),
                    gelen1.size(), ilk_fark(gelen1, gelen2));
    }
    if (!KONTROL(kilitli_kosu(&giden3, &gelen3))) {
        return hata_sayisi();
    }
    KONTROL(giden3 == giden1);
    if (!KONTROL(gelen3 == gelen1)) {
        std::printf("  koşu 3 farklı: boy %zu / %zu, ilk fark %zu\n", gelen3.size(),
                    gelen1.size(), ilk_fark(gelen1, gelen3));
    }
    return hata_sayisi();
}

}  // namespace ida::sil::test
