// Kapanış isteği ve sinyal işleyicileri (W4-sunucu-kucuk kapsam 1).
//
// İşleyici yalnız kilitsiz bir atomik sayacı artırır: G/Ç, kilit ve bellek ayırma yoktur.
// Windows'ta konsol denetim işleyicisi sistemin ayrı bir iş parçacığında çağrılır; orada da yalnız
// bayrak kurulur (pencere kapatma olayında işleyici, süreç sonlandırılmadan önce düzgün kapanışın
// bitmesini sınırlı süre bekler; bu bekleme de kilit ya da G/Ç değildir).
#include "ida/sunucu/sunucu.hpp"

#include <atomic>
#include <csignal>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace ida::sunucu {

namespace {

static_assert(std::atomic<int>::is_always_lock_free, "sinyal işleyicisinde kilitsiz atomik gerekir");

std::atomic<int> g_istek{0};          // kapanış isteği sayısı (ilk: düzgün kapanış; ikinci: hemen çık)
std::atomic<bool> g_bitti{false};     // düzgün kapanış tamamlandı

[[noreturn]] void hemen_cik() noexcept
{
#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), static_cast<UINT>(kCikisIkinciSinyal));
    for (;;) {
        Sleep(1000);
    }
#else
    _exit(kCikisIkinciSinyal);   // POSIX: asenkron sinyal güvenli
#endif
}

#ifdef _WIN32
BOOL WINAPI konsol_isleyici(DWORD olay)
{
    switch (olay) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        kapanis_sinyali();
        return TRUE;
    case CTRL_CLOSE_EVENT: {
        // İşleyici dönünce süreç sonlandırılır: düzgün kapanışın bitmesi (sınırlı süre; sistem
        // en geç birkaç saniye sonra süreci sonlandırır) beklenir.
        kapanis_sinyali();
        for (int i = 0; i < 450 && !g_bitti.load(std::memory_order_acquire); ++i) {
            Sleep(10);
        }
        return TRUE;
    }
    default:
        return FALSE;
    }
}
#else
void sinyal_isleyici_c(int) { kapanis_sinyali(); }
#endif

#ifdef _WIN32
void crt_isleyici(int) { kapanis_sinyali(); }
#endif

}  // namespace

void kapanis_iste() noexcept
{
    int beklenen = 0;
    (void)g_istek.compare_exchange_strong(beklenen, 1, std::memory_order_acq_rel);
}

bool kapanis_istendi() noexcept
{
    return g_istek.load(std::memory_order_acquire) > 0;
}

void kapanis_sifirla() noexcept
{
    g_istek.store(0, std::memory_order_release);
    g_bitti.store(false, std::memory_order_release);
}

void kapanis_sinyali() noexcept
{
    if (g_istek.fetch_add(1, std::memory_order_acq_rel) >= 1) {
        hemen_cik();
    }
}

void kapanis_tamamlandi() noexcept
{
    g_bitti.store(true, std::memory_order_release);
}

bool sinyal_kur() noexcept
{
#ifdef _WIN32
    bool tamam = SetConsoleCtrlHandler(konsol_isleyici, TRUE) != 0;
    tamam = std::signal(SIGINT, crt_isleyici) != SIG_ERR && tamam;
    tamam = std::signal(SIGTERM, crt_isleyici) != SIG_ERR && tamam;
    return tamam;
#else
    struct sigaction sa;
    sa.sa_handler = sinyal_isleyici_c;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;   // bekleyen sistem çağrıları yeniden başlar; döngüler bayrağı kendileri yoklar
    return sigaction(SIGINT, &sa, nullptr) == 0 && sigaction(SIGTERM, &sa, nullptr) == 0;
#endif
}

}  // namespace ida::sunucu
