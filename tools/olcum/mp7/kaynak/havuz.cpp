#include "havuz.hpp"

#if defined(__APPLE__)
#include <pthread/qos.h>
#endif

namespace mp7 {

void qos_yuksek() {
#if defined(__APPLE__)
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
}

Havuz::Havuz(int toplam) : toplam_(toplam < 1 ? 1 : toplam) {
  for (int i = 1; i < toplam_; ++i) isciler_.emplace_back([this] { dongu(); });
}

Havuz::~Havuz() {
  {
    std::lock_guard<std::mutex> l(m_);
    dur_ = true;
    ++nesil_;
  }
  cv_.notify_all();
  for (auto& t : isciler_) t.join();
}

void Havuz::parcala() {
  for (;;) {
    const int b = sonraki_.fetch_add(parca_);
    if (b >= adet_) break;
    const int e = b + parca_ < adet_ ? b + parca_ : adet_;
    (*f_)(b, e);
  }
}

void Havuz::dongu() {
  qos_yuksek();
  uint64_t gorulen = 0;
  for (;;) {
    {
      std::unique_lock<std::mutex> l(m_);
      cv_.wait(l, [&] { return nesil_ != gorulen; });
      gorulen = nesil_;
      if (dur_) return;
    }
    parcala();
    biten_.fetch_add(1, std::memory_order_acq_rel);
  }
}

void Havuz::calistir(int adet, int parca, const std::function<void(int, int)>& f) {
  if (toplam_ == 1) {
    f(0, adet);
    return;
  }
  {
    std::lock_guard<std::mutex> l(m_);
    f_ = &f;
    adet_ = adet;
    parca_ = parca < 1 ? 1 : parca;
    sonraki_.store(0);
    biten_.store(0);
    ++nesil_;
  }
  cv_.notify_all();
  parcala();
  const int bekle = toplam_ - 1;
  while (biten_.load(std::memory_order_acquire) < bekle) std::this_thread::yield();
}

}  // namespace mp7
