# firmware/third_party/ — ST ve Arm kaynakları

Kart yazılımının üçüncü taraf kaynakları `st.cmake` ile yapılandırma sırasında CPM üzerinden alınır (W3-kart dilim 1); kaynak dosyaları depoya girmez, derleme dizininin `_deps/` altına iner. Her kaynak, sürüm etiketinin işaret ettiği commit'in GitHub arşividir ve arşivin SHA-256'sı `st.cmake`'tedir (Unity ile aynı yöntem, `cmake/IdaBagimliliklar.cmake`). Hareketli dal ya da etiketle güncelleme yapılmaz; sürüm yükseltme ayrı bir iştir.

| Kaynak | Sürüm | Commit | Kullanılan |
|---|---|---|---|
| STM32U5 HAL (`STMicroelectronics/stm32u5xx-hal-driver`) | v1.6.3 | `2552682ee5a0d6f826a6435750fc9267be65868b` | HAL, CORTEX, RCC, PWR, FLASH, GPIO, ICACHE, RNG |
| CMSIS aygıt (`STMicroelectronics/cmsis-device-u5`) | v1.4.3 | `624374fa1e21ca195d6f2102ac0caaa50d0ea4c8` | aygıt başlıkları, `system_stm32u5xx.c` |
| CMSIS 6 çekirdek (`ARM-software/CMSIS_6`) | v6.2.0 | `6f0a58d01aa9bd2feba212097f9afe7acd991d52` | `CMSIS/Core/Include` |

HAL ve aygıt commit'leri STM32CubeU5 v1.9.0'ın alt modül olarak sabitlediği commit'lerdir (MP1–MP5 prototiplerinde kartta kullanılanlarla aynı). HAL yapılandırması (`stm32u5xx_hal_conf.h`) sabitlenmiş şablondan derleme dizinine üretilir; yalnız kullanılan modüller açılır. ST'nin başlangıç dosyası ve bağlayıcı betiği kullanılmaz: ikisi de `firmware/bsp/` altında sahiplenilmiş kendi dosyalarımızdır.

**W2'deki plandan sapma.** W2-depo bu kaynakların git alt modülü olarak alınmasını öngörüyordu. Otomatik derlemenin checkout adımı (`.github/workflows/derleme*.yml`) alt modül çekmediği ve o dosyalar bu işin sahipliğinde olmadığı için CPM seçildi; böylece CI'daki firmware derlemesi değişiklik gerektirmeden çalışır. Yapılandırma ağ erişimi ister (CPM'in öteki bağımlılıkları gibi).

USB yığını (TinyUSB, USBX) alınmaz (K17). Bu kaynaklar çekirdeğe (`core/`) bağlanamaz; yalnız `firmware/` hedefleri kullanır.
