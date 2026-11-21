# firmware/ — kart yazılımı (STM32U5A9J-DK)

Kartta koşan C yazılımı (iskelet §7, bölüm 01). Yalnız firmware kökünde (`fw-*` ön ayarları, `cmake/toolchains/arm-none-eabi-cm33.cmake`) derlenir; platform kodu `gnu17`'dir, uzantı yalnız bu dizinde kullanılır. Durum: W3-kart dilim 1 (kart destek katmanı, seri bağlantı, döngü); A1 oturumu, gönderim yöneticisi, kalıcı bellek ve ekran sonraki dilimlerdedir. USB yığını yoktur (K17).

## Dizinler

- `bsp/` — kart destek katmanı: başlangıç kodu ve vektör tablosu (`baslangic.c`), bağlayıcı betiği (`stm32u5a9_ida.ld`: SRAM1 veri, SRAM2'de `.noinit` ve 16 KB ana yığın, Bank 2'nin son dört sayfası kalıcı belleğe ayrılmış), güç ve saat (160 MHz; geçişte geçici AHB ÷2), IWDG (açılışta 2 s, çalışmada 100 ms), DWT, duvar saati ve 200 Hz ızgara (TIM2, 1 MHz), sıfırlama nedeni ve `.noinit` hata kaydı (`acilis_kaydi.c`, `hata.c`), LED'ler, RNG, NVIC bekçisi.
- `link/` — A1 bağlantı sürücüsü: USART1 + GPDMA1 (ST-LINK sanal seri portu, 3 000 000 baud), 16 KB dairesel alım halkası, alım olayları (0xFD karakter eşleşmesi, hat boşluğu, DMA yarım/tam) ve damgaları, olay sınırlı blok teslimi (`alim.c`, donanımsız), DMA ile gönderim, hata sayaçları.
- `app/` — ana döngü (`main.c`), çekirdeğin çağırıcısı (`cagirici.c`: `ida_core_t`'nin statik sahibi, 10 ms ızgara tick'i, arka plan dilimi, DWT ile süre), A1 tanılama ve duyuru kodlaması (`tanilama.c`), süre istatistiği (`istatistik.c`), SWD ile okunan ölçüm bloğu (`olcum.h`).
- `araclar/` — yükleme (`yukle.sh`), araç yolları (`ortam.sh`, `arac.sh`), PC yanı duman aracı (`duman.py`), ölçüm bloğu okuyucu (`olcum_oku.py`). Kullanım aşağıda.
- `test/` — donanımsız modüllerin birim testleri (Unity; PC kökünde `tests/CMakeLists.txt` içerir).
- `third_party/` — ST ve Arm kaynaklarının sabitlenmesi (`st.cmake`, CPM; ayrıntı `third_party/BENIOKU.md`).
- `yer_tutucu/` — W2'nin bağlayıcı betiği; kartta bağlanan birim test imajlarının (`cmake/IdaFirmware.cmake` varsayılanı) ve bellek ayırma denemesinin bağlanması için kalır.
- `deneme/` bellek ayırma denetimlerinin bilerek bozulmuş girdileri; `test_destek/` birim testlerinin kartta bağlanması için.

Bağlamada bellek ayırma simgeleri `--wrap` ile tanımsız bırakılır; malloc ailesine giden her başvuru bağlama hatasıdır. Her derlemede harita (`.map`), boyut raporu (`.boyut.txt`) ve yığın kullanımı raporu (`yigin-kullanimi.txt`) üretilir; firmware hedeflerinde işlev başına `-Wstack-usage=1024` uyarı (hata) sınırıdır.

## Kullanım (macOS; Windows'ta araç yolları ortam değişkeniyle verilir, duman aracı Windows'ta çalışmaz)

```
cmake --preset fw-release && cmake --build --preset fw-release     # ARM_GCC_BIN ortamda ya da PATH'te
firmware/araclar/yukle.sh fw-release                               # sıfırlama altında bağlanır
python3 -I firmware/araclar/duman.py izle --sure 5                 # BOARD_HELLO, LINK_STATS
python3 -I firmware/araclar/duman.py yuk --sure 600 --bayt 900     # %60 yük, sayaç karşılaştırması
python3 -I firmware/araclar/olcum_oku.py --onayar fw-release       # ölçüm bloğu (SWD)
python3 -I firmware/araclar/olcum_oku.py --onayar fw-measure --sinama iwdg   # yalnız fw-measure
```

Araç yolları: `ARM_GCC_BIN`, `CUBEPROG_BIN` (verilmezse macOS'ta STM32CubeIDE eklentilerinde aranır), seri uç `IDA_SERI_UC` (verilmezse ilk `/dev/cu.usbmodem*`). `yukle.sh` fabrika yedeği denetimini yapar; yedeği olmayan makinede `IDA_YEDEK_DENETIMI=0`.

## Bilinen davranışlar

- STM32_Programmer_CLI bağlanırken bütün NVIC kesmelerini kapatıyor ve önceliklerini 0'a döndürüyor ("Clear all interrupts"; kartta gözlendi). Ana döngünün NVIC bekçisi kayıtlı kesmelerin önceliğini ve açıklığını, hata işleyicilerinin SHCSR bitlerini her turda geri yükler (`nvic_yeniden` sayacı). Bekçi olmadan ölçüm bloğunun okunması ızgarayı durdurup IWDG sıfırlamasına yol açıyordu; yalnız açıklık geri yüklenirken öncelik 0'da kalan seri kesmeleri ana bağlamın BASEPRI maskesinden çıkıp alım verisini bozuyordu (%60 yükte 10 s'de 5 938 CRC hatası; öncelik de geri yüklenince 0).
- Dilim 1'de A1 oturumu yoktur: kart IDLE'da `IDA_BOARD_HELLO` duyurusu (200 ms) gönderir; `IDA_LINK_STATS` (1 Hz) yayını yalnız bu dilimin duman testine özgüdür ve A1-K11'den sapar (dilim 2'de oturum kurallarına bağlanır).
