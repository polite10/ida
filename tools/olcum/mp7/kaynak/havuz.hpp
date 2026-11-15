// Ping içi paralellik için kalıcı iş parçacığı havuzu.
// Işınlar parça parça dağıtılır; sonuçlar ışın numarasıyla yazılır, böylece
// sonuç iş parçacığı sayısından ve sırasından bağımsızdır (05 §2.6).
#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace mp7 {

class Havuz {
 public:
  explicit Havuz(int toplam);  // çağıran dahil toplam iş parçacığı
  ~Havuz();
  int boyut() const { return toplam_; }
  // f(baslangic, bitis): [0, adet) aralığını parça parça işler.
  void calistir(int adet, int parca, const std::function<void(int, int)>& f);

 private:
  void dongu();
  void parcala();
  int toplam_;
  std::vector<std::thread> isciler_;
  std::mutex m_;
  std::condition_variable cv_;
  uint64_t nesil_ = 0;
  bool dur_ = false;
  const std::function<void(int, int)>* f_ = nullptr;
  int adet_ = 0, parca_ = 1;
  std::atomic<int> sonraki_{0};
  std::atomic<int> biten_{0};
};

void qos_yuksek();  // macOS: iş parçacığını USER_INTERACTIVE QoS'a al (P çekirdek tercihi)

}  // namespace mp7
