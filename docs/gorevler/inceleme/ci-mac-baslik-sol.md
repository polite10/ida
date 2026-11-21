# sol incelemesi: cmake

## Hüküm

Belirtilen diff’te kritik bulgu yok; yeni denemelerde bir **önemli** yol işleme hatası var. Kök genişlemesinde mevcut denetimi etkisizleştiren bir değişiklik saptanmadı.

Talimat gereği derleme ve CTest çalıştırılmadı; değerlendirme dosya okuması ve birincil kaynak doğrulamasına dayanıyor.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `tools/denetim/deneme/cekirdek-stm32/calistir.cmake:53` | `_sahte_kaynak`, `CMAKE_C_FLAGS` içine tırnaksız gömülüyor. Derleme yolu boşluk içerirse `kaynak_dizini*` kipleri yapılandırmada düşer. Dıştaki CMake tırnakları, önbelleğe yazılan bayrak değerini korumaz; değer yeniden komut satırı olarak ayrıştırılır. [CMake belgesi](https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_FLAGS.html) | Bayrak değerinin içinde yolu tırnakla: `"-DCMAKE_C_FLAGS=-resource-dir=\"${_sahte_kaynak}\""`. Boşluk içeren bir derleme diziniyle doğrula. | önemli |

## Yeniden hesaplananlar

Sayısal hesap yok; akış ve yol karşılaştırmaları yeniden izlendi:

- **Sınır denetimi:** eklenen kök altında da ST/CMSIS/RTOS ve işletim sistemi kimlik denetimleri çalışıyor. Normal derleyici yerleşiminde çekirdek dışı depo başlıkları kök dışında kalıyor; `REALPATH` nedeniyle bunlara yönelen sembolik bağlantılar da muafiyet kazanmıyor.
- **Derleyici yolları:** MSVC ve clang-cl mevcut `MSVC` dalında kalıyor; Arm GCC yeni sorgudan derleyici kimliği nedeniyle dönüyor. Bu diff kaynaklı gerileme görülmedi. clang-cl’nin `MSVC` kapsamına girmesi [CMake tarafından belgeleniyor](https://cmake.org/cmake/help/latest/variable/MSVC.html).
- **Denemeler:** ASan listesi mevcutsa `asan` ve `kaynak_dizini`, kök eklemesinin geri alınmasını yakalar. Liste yoksa `EXISTS` koşulu kontrolü atlar; o araç zincirindeki başarı özgün hatanın sınandığını kanıtlamaz. Clang de varsayılan listeyi yalnız dosya mevcutsa ekliyor. [LLVM kaynağı](https://github.com/llvm/llvm-project/blob/release/17.x/clang/lib/Driver/SanitizerArgs.cpp)
- **Negatif deneme:** `kaynak_dizini_stm32`, sistem kökündeki yasak kimliğin yakalanmasını sınar; başlık zaten örtük `include/` köküne girdiğinden genişletilen kökün kendisine özgü bir sınama değildir.

## İskelete ya da diğer bölümlere notlar

İskelet, W2-depo ve A4’ün platformdan bağımsız çekirdek şartıyla çelişki bulunmadı.

GitHub’ın güncel görüntü belgesi `Xcode_26.6.app` yolunu doğruluyor. Yolun derleyiciden sorgulanması bu ada bağımlılığı kaldırıyor; bu koşucuya özgü başka bir engel saptanmadı. Gerçek koşucuda başarı **doğrulanmadı**. [Koşucu görüntüsü](https://github.com/actions/runner-images/blob/main/images/macos/macos-26-arm64-Readme.md)