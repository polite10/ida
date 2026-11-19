// W0-MP3a masaüstü testleri: sahte hat (bkz. sahte_hat.hpp). Aynı anda tek örnek (kart mantığı tekil).
#include "sahte_hat.hpp"

#include "mp3_kart.h"

#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <random>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <deque>
#include <algorithm>

namespace mp3test {

namespace {

// Öykünücü iş parçacığının durumu (kart mantığının platform işlevleri bağlamsız C göstericileridir).
std::vector<uint8_t> g_cikis;
uint32_t g_kart_baud = MP3_KONTROL_BAUD;
mp3_hw_t g_hw{};
const HataAyar *g_h = nullptr;

uint32_t z_us()
{
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
uint32_t z_tx_bos() { return g_cikis.size() >= 16384 ? 0u : static_cast<uint32_t>(16384 - g_cikis.size()); }
// Her yazım bir parça; parça hazır olma zamanına (µs) kadar PC'ye gönderilmez (KOSU'da kp_gecikme_us).
std::deque<std::pair<int64_t, size_t>> g_parca;
int64_t z_simdi_us()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void z_tx_yaz(const uint8_t *p, uint32_t n)
{
    g_cikis.insert(g_cikis.end(), p, p + n);
    const int64_t gec = (g_h != nullptr && mp3_kart_durum() == MP3_D_KOSU) ? g_h->kp_gecikme_us : 0;
    g_parca.emplace_back(z_simdi_us() + gec, n);
}
size_t z_hazir_bayt()
{
    const int64_t t = z_simdi_us();
    size_t n = 0;
    for (const auto &[hz, uz] : g_parca) {
        if (hz > t) break;
        n += uz;
    }
    return n;
}
void z_tuket(size_t n)
{
    while (n > 0 && !g_parca.empty()) {
        const size_t k = std::min(n, g_parca.front().second);
        g_parca.front().second -= k;
        n -= k;
        if (g_parca.front().second == 0) g_parca.pop_front();
    }
}
int z_tx_bosaldi() { return g_cikis.empty() ? 1 : 0; }
int z_baud_hesapla(uint32_t baud, mp3_baud_t *b)
{
    if (g_h->ret_baud.count(baud) != 0) {
        std::memset(b, 0, sizeof *b);
        return -1;
    }
    return mp3_baud_hesapla(160000000u, baud, b);
}
void z_baud_uygula(const mp3_baud_t *b)
{
    if (g_h->kart_hiz_degistirmez && b->baud != MP3_KONTROL_BAUD) return;
    g_kart_baud = b->baud;
}
void z_hw_oku(mp3_hw_t *h, int sifirla)
{
    *h = g_hw;
    if (sifirla) {
        g_hw = mp3_hw_t{};
        g_hw.uart_ore = g_h->enjekte_ore;
        g_hw.rx_tasma = g_h->enjekte_tasma;
        g_hw.rx_halka_boy = 16384;
    }
}
void z_kimlik_ek(char *s, uint32_t kap) { std::snprintf(s, kap, " usart_hz=160000000 sahte=1"); }

const mp3_platform_t PLATFORM = {z_us, z_tx_bos, z_tx_yaz, z_tx_bosaldi, z_baud_hesapla, z_baud_uygula, z_hw_oku, z_kimlik_ek};

}  // namespace

SahteHat::SahteHat(const HataAyar &h) : h_(h)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) std::abort();
    if (h_.parca_azami > 0) {
        const int boy = 2048;
        for (int fd : sv) {
            setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &boy, sizeof boy);
            setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &boy, sizeof boy);
        }
    }
    fd_kart_ = sv[1];
    fcntl(fd_kart_, F_SETFL, fcntl(fd_kart_, F_GETFL) | O_NONBLOCK);
    mp3::YazmaKanca yk = nullptr;
    if (h_.pc_yazma_gecikme_us > 0) {
        yk = [this, sayac = std::make_shared<int64_t>(0)](int fd, const uint8_t *p, size_t n) -> long {
            if ((*sayac)++ >= h_.pc_yazma_gecikme_sonra)
                std::this_thread::sleep_for(std::chrono::microseconds(h_.pc_yazma_gecikme_us));
            return static_cast<long>(::write(fd, p, n));
        };
    }
    port_.reset(mp3::posix_fd_ac(sv[0], "sahte", [this](uint32_t b) {
        if (h_.os_ret_baud.count(b) != 0) return false;
        pc_baud_.store(b);
        return true;
    }, yk));
    g_cikis.clear();
    g_parca.clear();
    g_kart_baud = MP3_KONTROL_BAUD;
    g_hw = mp3_hw_t{};
    g_hw.rx_halka_boy = 16384;
    g_h = &h_;
    mp3_kart_baslat(&PLATFORM, "sahte-kart");
    t_ = std::thread([this] { dongu(); });
}

SahteHat::~SahteHat()
{
    dur_.store(true);
    if (t_.joinable()) t_.join();
    if (fd_kart_ >= 0) ::close(fd_kart_);
    port_.reset();
    g_h = nullptr;
}

void SahteHat::dongu()
{
    std::mt19937 rng(12345);
    uint8_t b[65536];
    int onceki = mp3_kart_durum();
    int64_t pk_i = -1, kp_i = -1;   // KOSU'dan beri bayt sırası (-1: henüz KOSU yok)
    auto parca = [&](size_t azami) -> size_t {
        if (h_.parca_azami == 0) return azami;
        const size_t k = 1 + rng() % h_.parca_azami;
        return k < azami ? k : azami;
    };
    while (!dur_.load()) {
        bool is = false;
        const int d = mp3_kart_durum();
        if (d != onceki) {
            if (d == MP3_D_KOSU) { kosu_girisi_++; pk_i = 0; kp_i = 0; }
            onceki = d;
        }
        const bool kosuda = d == MP3_D_KOSU;
        const bool uyusmaz = pc_baud_.load() != g_kart_baud;
        // PC -> kart
        if (!(h_.kosuda_okuma_dur && kosuda)) {
            const ssize_t n = ::read(fd_kart_, b, parca(sizeof b));
            if (n == 0) break;   // PC kapattı
            if (n > 0) {
                is = true;
                std::vector<uint8_t> v;
                v.reserve(static_cast<size_t>(n));
                for (ssize_t i = 0; i < n; ++i) {
                    uint8_t c = b[i];
                    if (uyusmaz) c = static_cast<uint8_t>(c ^ 0xA7);
                    if (pk_i >= 0) {
                        const int64_t s = pk_i++;
                        if (s == h_.pk_dusur) continue;
                        if (s == h_.pk_cevir) c ^= 1;
                    }
                    v.push_back(c);
                }
                if (!(h_.kosuda_sus && kosuda)) mp3_kart_besle(v.data(), static_cast<uint32_t>(v.size()));
            }
        }
        if (!(h_.kosuda_sus && kosuda)) mp3_kart_adim();
        // kart -> PC
        const size_t hazir = z_hazir_bayt();
        if (hazir > 0 && !(h_.kosuda_sus && kosuda)) {
            const size_t k = parca(hazir);
            std::vector<uint8_t> v;
            v.reserve(k);
            size_t tuketilen = 0;
            bool kapat = false;
            for (; tuketilen < k; ++tuketilen) {
                uint8_t c = g_cikis[tuketilen];
                if (uyusmaz) c = static_cast<uint8_t>(c ^ 0xA7);
                if (kp_i >= 0) {
                    const int64_t s = kp_i++;
                    if (h_.kopma_kp >= 0 && s >= h_.kopma_kp) { kapat = true; break; }
                    if (s == h_.kp_dusur) continue;
                    if (s == h_.kp_cevir) c ^= 1;
                }
                v.push_back(c);
            }
            size_t yazilan = 0;
            while (yazilan < v.size() && !dur_.load()) {
                const ssize_t w = ::write(fd_kart_, v.data() + yazilan, v.size() - yazilan);
                if (w > 0) { yazilan += static_cast<size_t>(w); continue; }
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
            g_cikis.erase(g_cikis.begin(), g_cikis.begin() + static_cast<std::ptrdiff_t>(tuketilen));
            z_tuket(tuketilen);
            is = true;
            if (kapat) {
                ::shutdown(fd_kart_, SHUT_RDWR);
                ::close(fd_kart_);
                fd_kart_ = -1;
                break;
            }
        }
        if (!is) std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
}

}  // namespace mp3test
