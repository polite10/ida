// saat.hpp — duvar saati (A2 §2, A2-K08): aynı makinedeki bütün süreçlerde ortak tekdüze saat, ns.
// macOS: clock_gettime_nsec_np(CLOCK_UPTIME_RAW) (= mach_absolute_time; uykuda ilerlemez).
// Windows: QueryPerformanceCounter, QueryPerformanceFrequency ile ns'ye çevrilir.
// Sim zamanıyla karışmaz (iskelet §4).
#pragma once

#include <cstdint>

namespace ida::a2::ic {

std::int64_t duvar_ns() noexcept;

// G/Ç döngüsünün kısa beklemesi (ms). Windows'ta Sleep ve WSAPoll zaman aşımları sistem saati
// tikine (≈ 15,6 ms) yuvarlandığı için yüksek çözünürlüklü beklenebilir zamanlayıcı kullanılır.
void kisa_bekle_ms(int ms) noexcept;

}  // namespace ida::a2::ic
