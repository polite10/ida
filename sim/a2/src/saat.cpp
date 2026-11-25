// saat.cpp — bkz. saat.hpp.
#include "saat.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <chrono>
#include <thread>
#include <time.h>
#endif

namespace ida::a2::ic {

std::int64_t duvar_ns() noexcept
{
#if defined(_WIN32)
    static const std::int64_t frekans = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return static_cast<std::int64_t>(f.QuadPart);
    }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    const std::int64_t sayac = static_cast<std::int64_t>(c.QuadPart);
    // Taşmasız: tam saniye ve kalan ayrı çevrilir.
    return (sayac / frekans) * 1'000'000'000 + ((sayac % frekans) * 1'000'000'000) / frekans;
#elif defined(__APPLE__)
    return static_cast<std::int64_t>(clock_gettime_nsec_np(CLOCK_UPTIME_RAW));
#else
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
#endif
}

void kisa_bekle_ms(int ms) noexcept
{
    if (ms <= 0) {
        return;
    }
#if defined(_WIN32)
    // CREATE_WAITABLE_TIMER_HIGH_RESOLUTION = 0x00000002 (Windows 10 1803+; eski SDK'da tanımsız
    // olabilir). Tutamaç iş parçacığı ömrüne bağlıdır ve iş parçacığı bitince kapatılır.
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
    if (zt.h != nullptr) {
        LARGE_INTEGER sure{};
        sure.QuadPart = -static_cast<LONGLONG>(ms) * 10'000;  // göreli, 100 ns birim
        if (SetWaitableTimer(zt.h, &sure, 0, nullptr, nullptr, FALSE) != 0) {
            (void)WaitForSingleObject(zt.h, INFINITE);
            return;
        }
    }
    Sleep(static_cast<DWORD>(ms));
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

}  // namespace ida::a2::ic
