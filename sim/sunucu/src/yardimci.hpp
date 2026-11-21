// Sunucunun iç yardımcıları: itici arızası (darbe genişliği ölçeklemesi) ve alt süreç.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ida::sunucu {

// İtki arızası: fizik arayüzünde kanal başına itki ölçeği yoktur; arızalı kanalın darbe
// genişliği, durağan itki F_ss = şekil(|u|)·F_max (04 §2.7.2) çarpan kadar küçülecek biçimde
// nötre doğru ölçeklenir. tests/zincir'deki yöntemle aynıdır (orada test kodudur; burada
// sunucunun senaryo arızasıdır).
class SekilTersi {
public:
    explicit SekilTersi(const std::array<double, 5>& c);
    double sekil(double u) const;
    double ters(double hedef) const;
    double olcekle(double pwm, double carpan, double notr, double kazanc, double olu_bant) const;

private:
    static constexpr int kN = 4000;
    std::array<double, kN + 1> tablo_{};
};

// Çağıran iş parçacığının zamanlayıcı önceliğini yükseltir (HIL 1x döngüsü). macOS: QoS
// USER_INTERACTIVE; Windows: THREAD_PRIORITY_HIGHEST. Başarısızlık sessizdir.
void zamanlayici_onceligi();

// Yüksek çözünürlüklü uyku (ns): macOS mach_wait_until, Windows yüksek çözünürlüklü beklenebilir
// zamanlayıcı (derlenir; ölçülmedi), öteki POSIX nanosleep. Çağıran mutlak son tarihten kalan
// süreyi verir ve son kısmı döndürerek bekler.
void sure_uyu(std::int64_t ns);

// Alt süreç (SIL koşucusu; A1-K97). POSIX posix_spawn, Windows CreateProcess.
class AltSurec {
public:
    virtual ~AltSurec() = default;
    virtual bool calisiyor() = 0;
    virtual void sonlandir() = 0;
};

std::unique_ptr<AltSurec> surec_baslat(const std::vector<std::string>& argv, std::string& hata);

// Windows komut satırı (sol #14): argümanlar Microsoft CRT'nin ayrıştırma kurallarına göre
// kodlanır ("Parsing C command-line arguments"): boşluk, sekme, satır sonu ya da tırnak içeren
// (ya da boş) argüman tırnağa alınır; tırnaktan önceki n ters bölü 2n + 1'e (tırnak kaçar),
// kapanış tırnağından önceki n ters bölü 2n'ye çoğaltılır; başka yerdeki ters bölü olduğu gibi
// kalır. Program adı (argv[0]) CRT'de kaçış olmadan ayrıştırıldığından yalnız tırnağa alınır
// (tırnak içeremez: false). Platformdan bağımsızdır (sınanabilsin diye).
bool windows_komut_satiri(const std::vector<std::string>& argv, std::string& satir);

}  // namespace ida::sunucu
