# firmware/ — kart yazılımı (STM32U5A9J-DK)

Kartta koşan C yazılımı (iskelet §7, bölüm 01). Yalnız firmware kökünde (`fw-*` ön ayarları, `cmake/toolchains/arm-none-eabi-cm33.cmake`) derlenir; platform kodu `gnu17`'dir, uzantı yalnız bu dizinde kullanılır. Durum: W3-kart dilim 2 (dilim 1: kart destek katmanı, seri bağlantı, döngü; dilim 2: `ida_link` üst katmanı ile A1 oturumu, üç sınıflı gönderim yöneticisi, kalıcı bellek, PC'de sahte sim); ekran sonraki dilimdedir. USB yığını yoktur (K17).

## Dizinler

- `bsp/` — kart destek katmanı: başlangıç kodu ve vektör tablosu (`baslangic.c`), kalıcı bölgenin bloklamayan flash sürücüsü (`flash.c`: sayfa silme, 16 B yazım, bölgedeki ECC çift hata NMI'sinin yutulması), bağlayıcı betiği (`stm32u5a9_ida.ld`: SRAM1 veri, SRAM2'de `.noinit` ve 16 KB ana yığın, Bank 2'nin son dört sayfası kalıcı belleğe ayrılmış), güç ve saat (160 MHz; geçişte geçici AHB ÷2), IWDG (açılışta 2 s, çalışmada 100 ms), DWT, duvar saati ve 200 Hz ızgara (TIM2, 1 MHz), sıfırlama nedeni ve `.noinit` hata kaydı (`acilis_kaydi.c`, `hata.c`), LED'ler, RNG, NVIC bekçisi.
- `link/` — A1 bağlantı sürücüsü: USART1 + GPDMA1 (ST-LINK sanal seri portu, 3 000 000 baud), 16 KB dairesel alım halkası, alım olayları (0xFD karakter eşleşmesi, hat boşluğu, DMA yarım/tam) ve damgaları, olay sınırlı blok teslimi (`alim.c`, donanımsız), DMA ile gönderim, hata sayaçları.
- `app/` — ana döngü (`main.c`: `ida_link` üst katmanının platform tablosu, alım → take → çekirdek → put_out akışı, S_kart ölçümü), çekirdeğin çağırıcısı (`cagirici.c`: `ida_core_t`'nin statik sahibi, step/tick ayrı DWT süreleri, `prev_overrun`, arka plan dilimi), üç sınıflı gönderim yöneticisi (`gonderim.c`, donanımsız; A1-K110: kritik, zorunlu Q_ii = 64, kaybedilebilir R_iii = 500 B/periyot), kalıcı bellek (`kalici.c`, donanımsız: açılış sayacı, A/B parametre sayfaları, adım adım kayıt), süre istatistiği (`istatistik.c`), SWD ile okunan ölçüm bloğu (`olcum.h`). Dilim 1'in `tanilama.c`'si kaldırıldı (duyuru ve LINK_STATS artık üst katmanda).
- `araclar/` — yükleme (`yukle.sh`), araç yolları (`ortam.sh`, `arac.sh`), PC yanı duman aracı (`duman.py`), sahte sim (`sahte_sim.py`: `protocol/ida.xml`'den kodlayıcı, el sıkışma, 200 Hz parti, komutlar, oturum/T_bağ/parametre kaydı/yeniden bağlanma kipleri), ölçüm bloğu okuyucu (`olcum_oku.py`). Kullanım aşağıda.
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
python3 -I firmware/araclar/sahte_sim.py oturum --sure 600 --komut --olcum fw-release
python3 -I firmware/araclar/sahte_sim.py tbag                       # T_bağ aşımında güvenli çıktı
python3 -I firmware/araclar/sahte_sim.py param --deger 25 --olcum fw-release   # kayıt, REBOOT
python3 -I firmware/araclar/sahte_sim.py yeniden                    # program kapanıp yeniden açılır
```

Kayıt sırasında IWDG sınaması (yalnız fw-measure): `olcum_oku.py --onayar fw-measure --sinama kayit_iwdg` ile işaret konur, ardından `sahte_sim.py param --kayit-iwdg`; kayıt başlayıp yazım 1 KB'ı geçince döngü durur, IWDG kartı sıfırlar, açılışta eski ya da yeni blok tutarlı olmalıdır.

Araç yolları: `ARM_GCC_BIN`, `CUBEPROG_BIN` (verilmezse macOS'ta STM32CubeIDE eklentilerinde aranır), seri uç `IDA_SERI_UC` (verilmezse ilk `/dev/cu.usbmodem*`). `yukle.sh` fabrika yedeği denetimini yapar; yedeği olmayan makinede `IDA_YEDEK_DENETIMI=0`.

## Bilinen davranışlar

- STM32_Programmer_CLI bağlanırken bütün NVIC kesmelerini kapatıyor ve önceliklerini 0'a döndürüyor ("Clear all interrupts"; kartta gözlendi). Ana döngünün NVIC bekçisi kayıtlı kesmelerin önceliğini ve açıklığını, hata işleyicilerinin SHCSR bitlerini her turda geri yükler (`nvic_yeniden` sayacı). Bekçi olmadan ölçüm bloğunun okunması ızgarayı durdurup IWDG sıfırlamasına yol açıyordu; yalnız açıklık geri yüklenirken öncelik 0'da kalan seri kesmeleri ana bağlamın BASEPRI maskesinden çıkıp alım verisini bozuyordu (%60 yükte 10 s'de 5 938 CRC hatası; öncelik de geri yüklenince 0).
- Dilim 2'den beri IDLE davranışı A1'dir (`ida_link` üst katmanı): kart IDLE'da yalnız `IDA_BOARD_HELLO` duyurur, `IDA_LINK_STATS` yalnız oturumda gönderilir. Duman aracının `--olcum` kipi bu yüzden sayaçları ölçüm bloğundan (`say_*`) okur ve oturum çerçevelerini (46000–46004) yük havuzuna koymaz.
- Flash işlemleri bloklamaz (Bank 2'de silme ya da yazma sürerken kod Bank 1'den koşar); süreler ölçüm bloğunda (`fl_*`, `kal_*`). Kalıcı bölge okunurken çift ECC hatası çıkarsa (kayıt yarıda kesilmiş sayfa) NMI işleyicisi bunu tanır, bayrağı temizler ve döner (`fl_ecc_nmi`).
