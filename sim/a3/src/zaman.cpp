// zaman.cpp — bkz. ida/a3/zaman.hpp.
#include "ida/a3/zaman.hpp"

#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ida::a3 {

void hassas_bekle(std::chrono::steady_clock::time_point hedef)
{
#if defined(_WIN32)
    const auto kalan = hedef - std::chrono::steady_clock::now();
    if (kalan <= std::chrono::steady_clock::duration::zero()) {
        return;
    }
    // CREATE_WAITABLE_TIMER_HIGH_RESOLUTION = 0x00000002 (eski SDK'larda tanımsız olabilir).
    // Sol #12: tutamaç iş parçacığı ömrüne bağlıdır; iş parçacığı bitince CloseHandle çağrılır
    // (CreateWaitableTimerExW belgesi: tutamaç CloseHandle ile kapatılır).
    struct Zamanlayici {
        HANDLE h = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002u, TIMER_ALL_ACCESS);
        Zamanlayici() = default;
        Zamanlayici(const Zamanlayici&) = delete;
        Zamanlayici& operator=(const Zamanlayici&) = delete;
        ~Zamanlayici()
        {
            if (h != nullptr) {
                CloseHandle(h);
            }
        }
    };
    static thread_local Zamanlayici zt;
    HANDLE z = zt.h;
    const long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(kalan).count();
    if (z != nullptr) {
        LARGE_INTEGER sure{};
        sure.QuadPart = -static_cast<LONGLONG>((ns + 99) / 100);  // göreli, 100 ns birim
        if (SetWaitableTimer(z, &sure, 0, nullptr, nullptr, FALSE) != 0) {
            (void)WaitForSingleObject(z, INFINITE);
            return;
        }
    }
    // Yüksek çözünürlüklü zamanlayıcı yok (Windows 10 1803 öncesi): kaba bekleme.
    Sleep(static_cast<DWORD>(ns / 1'000'000));
#else
    std::this_thread::sleep_until(hedef);
#endif
}

}  // namespace ida::a3
