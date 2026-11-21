#include "ida/viz/saat.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <time.h>
#else
#include <time.h>
#endif

namespace ida::viz {

std::int64_t duvar_ns() {
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

}  // namespace ida::viz
