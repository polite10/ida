// ida/a3/zaman.hpp — hassas bekleme (A3 yayın tiki ve sahte dünya için).
//
// Windows'ta Sleep, sleep_for/sleep_until ve WSAPoll zaman aşımları sistem saati tikine (varsayılan
// ≈ 15,6 ms) yuvarlanır; 20 ms'lik yayın tiki (A3-K33) ve 5 ms'lik dünya döngüsü bununla tutmaz.
// Windows'ta yüksek çözünürlüklü beklenebilir zamanlayıcı (CreateWaitableTimerExW,
// CREATE_WAITABLE_TIMER_HIGH_RESOLUTION; Windows 10 1803+) kullanılır; yoksa Sleep'e düşülür.
// Diğer platformlarda std::this_thread::sleep_until.
#pragma once

#include <chrono>

namespace ida::a3 {

// `hedef` anına kadar (steady_clock) bekler; geçmişse hemen döner.
void hassas_bekle(std::chrono::steady_clock::time_point hedef);

}  // namespace ida::a3
