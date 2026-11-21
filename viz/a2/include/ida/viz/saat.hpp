// Duvar saati (A2 §2): aynı makinedeki süreçlerde ortak tekdüze saat, ns.
// macOS: clock_gettime_nsec_np(CLOCK_UPTIME_RAW) (= mach_absolute_time; Godot'nun
// Time.get_ticks_usec kaynağıyla aynı). Windows: QueryPerformanceCounter, frekansla ns'ye.
#ifndef IDA_VIZ_SAAT_HPP
#define IDA_VIZ_SAAT_HPP

#include <cstdint>

namespace ida::viz {

std::int64_t duvar_ns();

}  // namespace ida::viz

#endif
