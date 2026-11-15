# MP1a — Kart araç zinciri ve fabrika durumu (ön tur)

| | |
|---|---|
| Görev | `docs/gorevler/W0-MP1a.md` (W0, ölçüm prototipi); ölçüt düzeltmesi ve düzeltme turu 1 dahil |
| Ölçüm | M6 (fabrika durumu), M21 (Mac yarısı), M4 (kısa tur); B01 K0–K2 |
| Uygulayan | `ida-uygulayici` (Opus) |
| Dal | `olcum/mp1-arac-zinciri` |
| Tarih | İlk tur 2026-11-14; düzeltme turu 1 2026-11-15 |
| Durum | Düzeltme turu 1 sonrası; sol incelemesi `inceleme/MP1-sol.md`, yanıt `inceleme/MP1-yanit.md` |

Bu rapor MP1'in **ön turudur**: 1 saatlik ve USB HS yükü altındaki SMPS/LDO koşuları MP2 ile birlikte yapılacak (görev belgesi, Amaç). Geçme ölçütleri görev belgesinin dondurduğu hedeflerdir; K0 ve K2 ölçütleri mimarın "Ölçüt düzeltmesi" başlığıyla değişmiştir ve aşağıdaki hükümler düzeltilmiş ölçütlere göredir.

**Düzeltme turu 1'de değişenler (özet).** 160 MHz'e geçiş dizisine STM32U59x/5Ax için geçici AHB ÷2 ve ≥ 5 µs bekleme eklendi; K1 ve K2 yeni imajla yinelendi (ilk turun K2 sonuçları 3.5'te tarihçe olarak durur, hükme katılmaz). Betikler programlayıcı çıkış kodunu korur, doğrulanmış yedek olmadan yüklemez, istenen regülatörü `REGS` ile karşılaştırır. Çözümleyici geçersiz koşuda eğim hesaplamaz ve başarısız döner.

## 1. Ortam

| Kalem | Değer | Nasıl alındı |
|---|---|---|
| Makine | Apple M3 Pro, macOS 26.5.2 (25F84) | `sysctl machdep.cpu.brand_string`, `sw_vers` |
| Kart | STM32U5A9J-DK, ST-LINK üzerinden USB hub'a bağlı; ikinci (kullanıcı) USB takılı değil | `STM32_Programmer_CLI -l st` |
| ST-LINK | STLINK-V3, aygıt yazılımı `V3J17M11`; VCP `/dev/cu.usbmodem*` | aynı |
| Kart derleyicisi | GNU Tools for STM32 14.3.rel1 (`arm-none-eabi-gcc 14.3.1 20250623`, derleme `14.3.rel1.20251027-0700`); gdb 15.2.90.20241229-git | STM32CubeIDE eklentisi `gnu-tools-for-stm32.14.3.rel1.macosaarch64_1.0.0.202602081740`; `--version` |
| Yükleme | STM32CubeProgrammer CLI 2.23.0 | CubeIDE eklentisi `cubeprogrammer.macosaarch64_1.0.100.202603051304` |
| Hata ayıklama | ST-LINK_gdbserver 7.14.0 | CubeIDE eklentisi `stlink-gdb-server.macosaarch64_1.0.100.202604010938` |
| Derleme sistemi | CMake 4.5.0, Ninja 1.13.2 (Homebrew; bu iş için kuruldu) | `cmake --version`, `ninja --version` |
| PC derleyicisi | Apple clang (sistem), C++20 | CMake `mp1-pc` ön ayarı |
| ST kaynakları | STM32CubeU5 `v1.9.0` (`d88042df24f16799957c24f00c2b234c9e306188`); alt modül commit'leri: `stm32u5xx-hal-driver` `2552682ee5a0d6f826a6435750fc9267be65868b` (V1.6.3, 03-June-2026), `cmsis-device-u5` `624374fa1e21ca195d6f2102ac0caaa50d0ea4c8` (v1.4.3), `stm32u5x9j-dk-bsp` `a5723a4ff404cb659b3c095d59527a0b0c2062f0` | `tools/olcum/mp1/kaynak-indir.sh`; `${IDA_CACHE}/src` altında, depoya girmez |
| libc | newlib-nano (`--specs=nano.specs --specs=nosys.specs`) | Picolibc bu araç zinciri paketinde yok (bulgu 3) |

Homebrew'deki `arm-none-eabi-gcc` kullanılmadı (09 §2.6). Araç yolları `tools/olcum/mp1/ortam.sh` içinde CubeIDE paketinden bulunur; ortam değişkenleriyle ezilebilir.

## 2. Komutlar ve dosyalar

Bütün komutlar depo kökünden; ham veri `${IDA_CACHE:-$HOME/.cache/ida}` altındadır, depoya girmez.

| Adım | Komut | Ham veri |
|---|---|---|
| K0 | `sh tools/olcum/mp1/k0-oku.sh` | `${IDA_CACHE}/mp1/k0/` (`ob-displ.txt`, `ham-yazmaclar.txt`, `probe-listesi.txt`) |
| Yedek | `sh tools/olcum/mp1/yedek-al.sh` (yalnız ilk kez); denetim `sh tools/olcum/mp1/yedek-dogrula.sh` | `${IDA_CACHE}/yedek/` (iki okuma, `SHA256SUMS`, okuma günlükleri) |
| Kaynaklar | `sh tools/olcum/mp1/kaynak-indir.sh` | `${IDA_CACHE}/src/` |
| K1 derle-yükle | `sh tools/olcum/mp1/k1-derle-yukle.sh` (iki kez `derle.sh` + `yukle.sh smps`) | `${IDA_CACHE}/mp1/k1/` |
| K1 hata ayıklama | `sh tools/olcum/mp1/k1-hata-ayikla.sh` (`gdb/k1-iwdg.gdb`) | `${IDA_CACHE}/mp1/k1/gdb.log`, `gdb-seri.csv` |
| K2 | `caffeinate -dims sh tools/olcum/mp1/k2-iki-tur.sh 640` (sırayla `k2-tur.sh smps`, `k2-tur.sh ldo`) | `${IDA_CACHE}/mp1/k2/` |
| Çözümleme | `tools/olcum/mp1/derleme/pc/mp1_seri analiz <csv> 160000000 SMPS\|LDO [600]` | — |

`yukle.sh` önce `yedek-dogrula.sh`'i çalıştırır (iki okuma dosyası tam 4.194.304 bayt, SHA-256'ları eşit ve `SHA256SUMS` ile tutarlı, okuma günlükleri başarılı); geçmezse yüklemez (çıkış 3). Programlayıcının çıkış kodu ≠ 0 ya da "Download verified successfully" yoksa çıkış 1. `k1-derle-yukle.sh`, `k2-tur.sh` ve `k2-iki-tur.sh` her adımın çıkış kodunu denetler ve ilk başarısızlıkta durur (`k2-iki-tur.sh` SMPS turu başarısızsa LDO turuna geçmez). `k1-hata-ayikla.sh` gdb ve seri kayıt çıkış kodlarını saklar; gdb betiği `IWDG_SONUC SIFIRLAMA_VAR` durumunda 2 ile çıkar; betik, gdb ya da kayıt hatasında, `SIFIRLAMA_YOK` ve `main`'e uzanan yığın izi görülmezse ya da karşı denemede `iwdgrst=1` açılışı gelmezse 1 döner. Yedek düzeltme turunda yeniden okunmadı (fabrika görüntüsü artık bozuk); mevcut dosyalar `yedek-dogrula.sh` ile denetlendi.

Aygıt yazılımı: `tools/olcum/mp1/fw/main.c` (tek dosya; HAL ile saat, USART1, IWDG; DWT 64 bit). İki varyant aynı kaynaktan, yalnız `MP1_REG_SMPS` tanımıyla ayrılır (`mp1_smps.elf`, `mp1_ldo.elf`). PC aracı: `tools/olcum/mp1/pc/mp1_seri.cpp` (`kayit` ve `analiz`).

## 3. Sonuçlar

### 3.1 K0 — fabrika durumu (ilk yüklemeden önce, 2026-11-14)

Bağlantı `mode=HOTPLUG` (çekirdek sıfırlanmadı, durdurulmadı); yalnız `-ob displ`, `-r32` ve `-l` komutları. Hiçbir seçenek baytı, flash ya da OSPI yazma komutu çalıştırılmadı.

| Alan | Değer | Kaynak |
|---|---|---|
| `DBGMCU_IDCODE` | `0x30006481`: `DEV_ID` = `0x481`, **`REV_ID` = `0x3000`** (CLI yorumu "Rev C") | `-r32 0xE0044000` |
| Flash boyutu | `FLASHSIZE_BASE` = `0x1000` → 4096 KB | `-r32 0x0BFA07A0` |
| Paket kaydı | `PACKAGE_BASE` = `0x0101F71C` (ham) | `-r32 0x0BFA0500` |
| Hedef gerilim | 1,80 V | CLI |
| `FLASH_OPTR` | `0x7BEFF8AA` | `-r32 0x40022040` |
| `RDP` | `0xAA` (Level 0, koruma yok) | `-ob displ` |
| `TZEN` | 0 (TrustZone kapalı) | aynı |
| `BOR_LEV` | 0 (eşik ≈ 1,7 V) | aynı |
| SRAM silme | `SRAM_RST` = 1 (sistem sıfırlamasında SRAM1/3/4 silinmez), `SRAM2_RST` = 1 (SRAM2 silinmez) | aynı |
| ECC | `SRAM2_ECC` = 1, `SRAM3_ECC` = 1, `BKPRAM_ECC` = 1 (üçü de **kapalı**; bit 1 = kapalı) | aynı |
| IWDG/WWDG | `IWDG_SW` = 1 (yazılımla başlatılır), `IWDG_STOP` = 1, `IWDG_STDBY` = 1, `WWDG_SW` = 1 | aynı |
| Önyükleme | `nSWBOOT0` = 0, `nBOOT0` = 1 (ana flash), `NSBOOTADD0` = `0x08000000`, `NSBOOTADD1` = `0x0BF90000`, `SWAP_BANK` = 0 | aynı; `0x40022044` = `0x0800007F 0x0BF9007F` |
| Yazma koruması | WRP1A/1B/2A/2B: başlangıç `0xFF` > bitiş `0x00` (koruma yok), kilitsiz | `0x40022058`, `0x40022068` = `0xFF00FFFF` |
| Diğer | `nRST_STOP/STDBY/SHDW` = 1, `PA15_PUPEN` = 1, `IO_VDD_HSLV` = 1, `IO_VDDIO2_HSLV` = 1 | `-ob displ` |
| ST-LINK aygıt yazılımı | `V3J17M11`; CLI güncelleme uyarısı vermedi; güncelleme yapılmadı | CLI |

CLI her bağlantıda "Debug in Low Power mode enabled" yazar; bağlanırken DBGMCU'nun düşük güç hata ayıklama ayarına dokunuyor. Bu geçici bir hata ayıklama yazmacıdır (seçenek baytı ya da flash değil); yazılan değer **doğrulanmadı**.

01 §2.8 ve §4 ile ilişki: `SRAM_RST` ve `SRAM2_RST` 1 olduğundan seçenek baytlarına göre `.noinit` sistem sıfırlamasında silinmez; bu, uygulamada `.noinit`'in korunduğunun çalıştırılarak sınanması değildir. `BOR_LEV` = 0, 1,8 V'luk VDD ile uyumlu tek düzeydir (01 §4).

**`REV_ID` ve errata.** `0x3000` değerini CLI "Rev C" diye yorumluyor. sol incelemesinin okuduğu ES0553 **Rev 6**'nın (STM32U59xxx/5Axxx errata; [st.com PDF](https://www.st.com/resource/en/errata_sheet/es0553-stm32u59xxx-and-stm32u5axxx-device-errata-stmicroelectronics.pdf)) cihaz kimliği tablosu yalnız `0x3001` (X) ve `0x3003` (W) revizyonlarını listeler; `0x3000` listede yoktur. Bu liste sol ikinci incelemesinde ES0553 Rev 6'dan doğrulandı (PDF bu ortamdan açılamadı). İlk turdaki "Rev 5: 0x3001/0x3002" ifadesi doğrulanmamış bir arama özetiydi ve geri alındı. Sonuç: kartın silikonu ES0553 Rev 6'nın listelediği revizyonlardan biri değildir; errata kapsamı (M6) açık kalır.

**K0: geçti** (düzeltilmiş ölçüt: kalıcı belleğe ve seçenek baytlarına yazma yok). Seçenek baytları ve `REV_ID` kayıtlı.

### 3.2 Fabrika yazılımının yedeği (2026-11-14)

`-u 0x08000000 0x400000` iki kez (Hot Plug kipinde), ikisi aynı:

| Dosya | Boyut | SHA-256 |
|---|---|---|
| `${IDA_CACHE}/yedek/u5a9j-dk-dahili-flash-fabrika.bin` | 4.194.304 B | `1f13870b132a61d34f3649c96e9a789914f692dd45081f10dcd80250e0e22457` |
| `${IDA_CACHE}/yedek/u5a9j-dk-dahili-flash-fabrika-ikinci-okuma.bin` | 4.194.304 B | aynı |

2026-11-15'te `yedek-dogrula.sh` iki dosyanın da tam boyutlu, özetlerinin eşit ve `SHA256SUMS` ile tutarlı, iki okuma günlüğünün "Data read successfully" içerdiğini doğruladı (çıkış 0). İlk turdaki `yedek-al.sh` okumaların çıkış kodunu ve boyutu denetlemiyordu (sol md.3); betik düzeltildi, yedek yeniden alınmadı. Bu yüzden ilk turdaki okumaların çıkış kodu kayıtlı değildir; geçerlilik boyut, özet eşitliği ve günlük satırına dayanır.

Silinmemiş (≠ `0xFF`) bayt sayısı 3.907.315. Harici OSPI flash okunmadı ve yedeklenmedi; demonun OSPI'ye bağlı olup olmadığı **doğrulanmadı**. Geri yükleme (yapılmadı): `STM32_Programmer_CLI -c port=SWD -w ${IDA_CACHE}/yedek/u5a9j-dk-dahili-flash-fabrika.bin 0x08000000 -v -rst`. Bu çalışmada yalnız sektör 0–2 (3 × 8 KB) silinip yazıldı.

**Yedek: geçti.**

### 3.3 K1 — derle, yükle, ayıkla (düzeltme turu 1 imajıyla, 2026-11-15)

**Derleme ve yükleme, iki tur.** `k1-derle-yukle.sh` her turda `derleme/` dizinini silip `cmake --preset mp1-fw && cmake --build --preset mp1-fw` (ve PC aracı) çalıştırır, yedeği doğrular, `mp1_smps.elf`'i yükler ve sıfırlamadan sonraki seri satırları kaydeder.

| | Tur 1 | Tur 2 |
|---|---|---|
| `mp1_smps.elf` SHA-256 | `7ceddeb5ab16d6b5bf9264cf0da7f52c8dbd94f2b1f9024c6665dcdace60febd` | aynı |
| `mp1_smps.bin` SHA-256 | `1a58eb6c3029764b157ca499f946eaba3070cd56ed7bdac481ba64e2f04cdde2` | aynı |
| `mp1_ldo.elf` / `.bin` | `772dd69556e5482f9bec4ff44ea82f30d4904e210847cca4be42de7c114c572d` / `cf58f6089bf15806a95a1d8141f264792450dcc7dcdef07eb73388a6225e05f7` | aynı |
| Yükleme | yedek geçerli; sektör 0–2 silindi; "Download verified successfully"; çıkış 0 | aynı |
| Açılış satırı | `B MP1a reg_istenen=SMPS … idcode=0x30006481 dbg_apb1fzr1=0x00001000 saat_adim=0 gecis_hpre=0x00000008 gecis_bekleme_cyc=420 gecis_hclk_hz=80000000` | aynı |

İmaj boyutu: 19.600 B kod, 12 B veri, 1.784 B bss.

**Hata ayıklama.** `ST-LINK_gdbserver -p 61234 -l 1 -d -g -cp <CubeProgrammer bin>` çalışan hedefe bağlandı; `arm-none-eabi-gdb -batch -x tools/olcum/mp1/gdb/k1-iwdg.gdb`:

- `break mp1_isaret` → durdu. Yığın izi: `#0 mp1_isaret (n=21) main.c:326` · `#1 satir_gonder (sira=21) main.c:339` · `#2 main () main.c:406`.
- **IWDG açık, dondurma biti açık** (`DBGMCU_APB1FZR1` = `0x00001000`): IWDG nominal ≈ 1,0 s (LSI 32 kHz / 32, reload 999; LSI ölçülmedi). İşlemci kesme noktasında 10 s bekletildi, sonra `continue`: `IWDG_SONUC SIFIRLAMA_YOK`. Seri kayıtta T 21 ile T 22 arası 11,458 s; arada açılış satırı yok. (`mp1_sayac` kesme noktası atamadan önce okunduğu için 20 → 21, işlev argümanı 21 → 22 ilerler.)
- **Karşı deneme:** dondurma biti gdb'den temizlendi (`0x00000000`), işlemci yeniden durduruldu; seri porttan yeni açılış satırı geldi: `rcc_csr=0x24004400 iwdgrst=1`. İşlemci sıfırlamadan sonra gdb'nin denetimi dışında çalışmaya başladı.

İlk turda (imaj `7d8029dd…`) aynı denemeler aynı sonucu vermişti (T 47–48 arası 11,450 s). İkinci düzeltme turunda, çıkış kodlarını denetleyen `k1-hata-ayikla.sh` ile kartta bir kez daha koşuldu (önce `yukle.sh smps`, imaj `7ceddeb5…`): gdb çıkış 0, kayıt çıkış 0, `IWDG_SONUC SIFIRLAMA_YOK`, yığın izi `main`'e ulaştı, karşı denemede `iwdgrst=1`; betik çıkış kodu 0.

**K1: geçti** (Mac yarısı). Windows yarısı (M21) bu işin kapsamında değil.

### 3.4 K2 — 160 MHz (kısa tur; düzeltme turu 1)

**Kodun açılışta yaptığı** (`fw/main.c` `saat_kur`; değerler ST'nin STM32U5x9J-DK `Applications/USBX/Ux_Host_CDC_ACM` örneğinden, geçiş adımı ST'nin STM32U59x/5Ax kısıtından):

1. `__HAL_RCC_PWR_CLK_ENABLE`, `HAL_PWREx_DisableUCPDDeadBattery`;
2. `HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY | PWR_LDO_SUPPLY)` (varyanta göre);
3. `HAL_PWREx_ControlVoltageScaling(SCALE1)` — HAL, Range 1/2'ye geçerken EPOD `BOOSTEN`'i kendisi kurar;
4. HSE açılır (ayrı çağrı; açılmazsa adım 3 hatası);
5. PLL1: kaynak HSE, `PLLMBOOST` = DIV1, M = 4, N = 80, P = 8, Q = 2, R = 2, `PLLRGE` = `RCC_PLLVCIRANGE_0`, `FRACN` = 0 (kilitlenmezse adım 4 hatası);
6. `HAL_RCC_ClockConfig(SYSCLK = PLL, AHB ÷2, APB1/2/3 ÷1, FLASH_LATENCY_4)`: HAL, bölücü büyürken `HPRE`'yi SYSCLK geçişinden **önce** yazar ve geçişten önce `BOOSTRDY`'yi bekler (`stm32u5xx_hal_rcc.c`, `HAL_RCC_ClockConfig`). Sonuç: AHB ÷2, SYSCLK = PLL1, HCLK = 80 MHz;
7. DWT ile en az **5 µs** bekleme (`SystemCoreClock` = 80 MHz'te 400 çevrim);
8. `HAL_RCC_ClockConfig(HCLK ÷1)` (dönüş hatası adım 7).

Adım 6–8'in dayanağı: RM0456 §10.5.4'teki "STM32U59x/5Ax/5Fx/5Gx'te Range 1'de en büyük sistem frekansı artışı ya da azalışı 80 MHz'dir" kısıtı. Bu kısıtın uygulama dizisi ST teknik moderatörünün 2024-03-08 tarihli yanıtındadır: "AHB bölücüyle ÷2 (HPRE = 0b1000), PLL1'i aç, sistem saatini PLL1'e geçir, **5 µs** bekle, bölücüyü 1'e döndür" ([ST topluluk, RM0456 10.5.4](https://community.st.com/stm32-mcus-products-25/rm0456-10-5-4-unclear-restrictions-for-stm32u59x-5ax-5fx-5gx-devices-129223)); yanıtta RM0456 revizyonu belirtilmez. AN5766'nın ST HTML metni ([web sayfası](https://www.st.com/content/st_com/en/technical-documents/an5766-migrating-within-stm32u5-series-microcontrollers-stmicroelectronics_asset/chunk-004-topic-an5766-power.html)) **3 µs** verir; bu değer sol ikinci incelemesinde ST'nin HTML metninden doğrulandı (sayfa bu ortamdan açılamadı; AN5766 PDF revizyonu ve RM0456 metni doğrulanmadı). Görev belgesi gereği iki süreden büyüğü (5 µs) kullanıldı. Sabitlenen HAL (V1.6.3) bu ÷2–bekle–÷1 adımını kendisi yapmaz; ST'nin DK örneği de yapmaz (doğrudan ÷1).

Bir adım hata verirse kod başka bir saat kaynağına dönmez; **o anki saat durumunda** ölçümü durdurur: kırmızı LED yanar, USART1 o anki PCLK2'ye göre kurulur, seri porta `E SAAT_HATASI adim=<n> hal_rc=<k>` yazılır, DWT ölçümü başlamaz. O anki saat durumu adıma göre değişir: adım 1–4'te HAL SYSCLK'i değiştirmediği için SYSCLK hâlâ MSIS 4 MHz'tir; adım 5'te HAL'in nerede durduğuna göre MSIS ya da PLL1 (AHB ÷2) olabilir; adım 6 (geri okuma tutmadı) ve 7 (÷1'e dönüş) hatasında SYSCLK zaten PLL1'dir ve MSIS'e dönülmez (sol ikinci incelemesi, yeni bulgu 3). **Gerçekten denenen** yalnız adım 6 hatasıdır (ilk turdaki geri okuma hatası, bulgu 4); HSE'nin açılmaması (adım 3) ve PLL'in kilitlenmemesi (adım 4) **denenmedi**.

**Kayıtta gözlenen.** Açılış satırı (`B`) geçişi gösterir: `gecis_hpre=0x00000008` (PLL1'e geçişten hemen sonra AHB ÷2), `gecis_bekleme_cyc=420` (80 MHz'te 5,25 µs, DWT ölçümü), `gecis_hclk_hz=80000000`. Açılıştaki ve her 60 s'deki `K`/`P` satırları:

| Alan | Okunan | Anlamı |
|---|---|---|
| `RCC_CFGR1` | `0x0000000F` → `SWS` = 3 | SYSCLK kaynağı **PLL1** |
| `RCC_CFGR2` | `0x00000000` | `HPRE` = ÷1 (geçişten sonra), APB ÷1 |
| `RCC_PLL1CFGR` | `0x00040313` | `PLL1SRC` = 3 (HSE), `PLL1RGE` = 0, `PLL1FRACEN` = 1, **`PLL1M[11:8]` = 0x3 → M = 4**, `PLL1MBOOST` = 0 (÷1), `PLL1REN` = 1; P ve Q çıkışları kapalı |
| `RCC_PLL1DIVR` | `0x01010E4F` | ham N/P/Q/R alanları 79/7/1/1 → N = 80, P = 8, Q = 2, R = 2 |
| `RCC_PLL1FRACR` | `0x00000000` | FRACN = 0 |
| Hesap | 16 MHz / 4 × 80 / 2 | **160.000.000 Hz** (yazmaçlardan, nominal HSE ile); `HAL_RCC_GetSysClockFreq` = 160.000.000 |
| `RCC_CR` | `0x03030005` | `HSERDY` = 1, `PLL1RDY` = 1 |
| `FLASH_ACR` | `0x00000104` | `LATENCY` = 4, `PRFTEN` = 1 |
| `PWR_VOSR` | `0x0007C000` | `VOS` = 3 (Range 1), `VOSRDY`, `BOOSTEN`, `BOOSTRDY` = 1 |
| `PWR_SVMSR` | SMPS: `0x00038002`; LDO: `0x00038000` | `REGS` = 1 (SMPS) / 0 (LDO), `ACTVOS` = 3 |
| `uart_hata` | 0 | `HAL_UART_Transmit` hata sayacı (düzeltme turunda kayda eklendi) |

**DWT/PC oranı.** Aygıt yazılımı DWT `CYCCNT`'yi 64 bite genişletir ve her 160·10⁶ çevrimde `T <sıra> <çevrim>` yazar. PC aracı her satırın `\n` baytının okunduğu anı iki saatle damgalar: `steady_clock` (libc++'ın Apple uygulamasında `CLOCK_MONOTONIC_RAW`; düzeltilmemiş) ve `system_clock` (NTP ile disiplinli). Seri kayıt yüklemeden **önce** açılır; ölçüm penceresi istenen regülatörle açılan imajın ilk `B` satırından başlar. Çözümleyici pencerede yeni açılış (sıfırlama), `E` satırı, sıra boşluğu, geri giden sayaç ya da PC zamanı, istenenden farklı `REGS`, sıfırdan farklı `uart_hata`, PLL1/160 MHz olmayan `K` satırı ya da 600 s'den kısa pencere görürse eğim hesaplamaz ve başarısız döner. `system_clock`'ta `steady_clock`'a göre > 5 ms sıçrama varsa yalnız o saatin eğimi geçersiz sayılır. İkinci düzeltme turunda (sol ikinci incelemesi, yeni bulgu 1) ret koşulları sıkılaştırıldı: pencerenin ilk T sırası 1 olmalı; açılıştaki sıra 0 ve son T'den önceki her 60'ın katı için tam bir `K` ve bir `P` satırı bulunmalı; `T` satırları ve zorunlu alanlar (`sira`, `hesap_hz`, `uart_hata`, `saat_adim`) kesin sayısal çözülür, eksik ya da bozuk alan (ör. `uart_hata=abc`) koşuyu geçersiz kılar. Karşı örnekler reddedildi: SMPS kaydından ilk dokuz T satırı çıkarılınca ("ilk T sırası 1 değil"), bir `P`'de `uart_hata=abc` yapılınca ve `K sira=300` satırı silinince. 2026-11-15 SMPS ve LDO kayıtları sıkılaştırılmış çözümleyiciyle yine `GEÇERLİ KOŞU` çıktı; oranlar değişmedi (−6,668 / −3,829 ppm).

Turlar 2026-11-15, `caffeinate -dims` altında: SMPS 02:17–02:28, LDO 02:28–02:39 (yerel saat); her biri 640 s kayıt. Yüklenen imajlar K1'deki özetlerle aynı (`mp1_smps.elf` `7ceddeb5…`, `mp1_ldo.elf` `772dd695…`); iki yükleme de yedek denetiminden geçti ve "Download verified successfully" verdi.

| | SMPS | LDO |
|---|---|---|
| Açılış satırı (pencere başı) | `reg_istenen=SMPS … saat_adim=0 gecis_hpre=0x00000008 gecis_bekleme_cyc=420` | `reg_istenen=LDO … saat_adim=0 gecis_hpre=0x00000008 gecis_bekleme_cyc=420` |
| Ölçüm penceresi (`steady_clock`) | 637,0 s | 637,0 s |
| T satırı, sıra | 638 (1 → 638) | 638 (1 → 638) |
| Sıra boşluğu / sayaç geri gidişi / PC zamanı geri gidişi | 0 / 0 / 0 | 0 / 0 / 0 |
| Pencerede yeni açılış (sıfırlama) / `E` (saat ya da başka hata) | 0 / 0 | 0 / 0 |
| `K` satırları: SYSCLK = PLL1 ve 160 MHz | 11/11 | 11/11 |
| `P` satırları: `REGS` = istenen | 11/11 (SMPS) | 11/11 (LDO) |
| `uart_hata` en büyük | 0 | 0 |
| Çözümleyici sonucu | `GEÇERLİ KOŞU` (çıkış 0) | `GEÇERLİ KOŞU` (çıkış 0) |
| **DWT/PC oranı, `steady_clock`'a göre** | **159.998.933,12 Hz → −6,67 ppm** (std. hata 0,013; ilk yarı −7,07, ikinci yarı −6,52) | **159.999.387,34 Hz → −3,83 ppm** (std. hata 0,117; ilk yarı −4,77, ikinci yarı −3,90) |
| DWT/PC oranı, `system_clock`'a göre | +18,44 ppm (ilk yarı +18,04, ikinci yarı +18,59) | **geçersiz:** t ≈ 24 s'de `system_clock` `steady_clock`'a göre ≈ +180 ms sıçradı |
| Satır varışı artığı (`steady_clock`) | rms 61 µs, en büyük 309 µs | rms 544 µs, en büyük 7,4 ms |

LDO turundaki büyük artık, tek tek satırların PC'ye 4–10 ms geç varmasından gelir (sıra 266–267 ve 577; komşu satırlar olağan): Mac tarafındaki varış gecikmesidir, kart sayaçları kesintisizdir. Standart hatalar yalnız varış titreşiminden gelen istatistiksel belirsizliktir.

Oranlar turdan tura değişti: `steady_clock`'a göre ilk turda −5,18 / −5,69 ppm, bu turda −6,67 / −3,83 ppm. Bu farkın regülatöre, sıcaklığa ya da Mac saatine bağlı olup olmadığı ayrılmadı; regülatörler arasında bir sonuç çıkarılmaz. `sntp time.apple.com` sistem saati farkı (yalnız bağlam, ±0,07–0,09 s): SMPS turu başında +0,486 s, sonunda +0,497 s; LDO turu başında +0,501 s, sonunda +0,316 s.

**İki PC saati.** `steady_clock` ile `system_clock` arasında **bağıl hız farkı** ölçüldü: bu turun SMPS koşusunda ≈ 25,1 ppm (`system_clock` daha yavaş), ilk turun SMPS koşusunda ≈ 18,0 ppm (`system_clock` daha hızlı). İşaretin değişmesi ve iki sıçrama (−91 ms, +180 ms), `system_clock`'un `timed` tarafından kaydırılıp sıçratılarak düzeltildiğini gösterir. Bu, iki Mac saatinin birbirine göre hızıdır; hangisinin gerçeğe yakın olduğu, mutlak referans hatası ve belirsizlik sınırı bu düzenekle **belirlenmedi**. Oran bu yüzden kart saatinin adı geçen Mac saatine oranıdır; mutlak frekans doğruluğu iddiası yoktur (düzeltilmiş K2 ölçütü). Kartın HSE'si Zephyr kart belgesine göre 16 MHz kristaldir (ikincil kaynak); parça numarası ve toleransı birincil kaynaktan alınamadı.

**K2: geçti** (düzeltilmiş ölçüt):
- Geri okunan `SYSCLK` kaynağı PLL1, frekans yazmaçlardan 160 MHz: iki varyantta açılışta ve 10 dk boyunca her 60 s'de (11/11).
- DWT/PC oranı raporlandı: `steady_clock`'a (Apple'da `CLOCK_MONOTONIC_RAW`) göre SMPS −6,67 ppm, LDO −3,83 ppm; `system_clock`'a göre SMPS +18,44 ppm, LDO geçersiz (saat sıçraması). Mutlak doğruluk iddiası yoktur.
- SMPS ve LDO turları ≥ 10 dk (637 s pencere) sıfırlama, saat hatası, sıra boşluğu ve UART hatası olmadan tamamlandı; `REGS` istenen regülatörle her 60 s'de eşleşti.

### 3.5 Tarihçe: ilk turun K2 koşuları (2026-11-14; hükme katılmaz)

İlk tur imajları (`mp1_smps.elf` `7d8029dd…`, `mp1_ldo.elf` `6f4b9a9f…`) AHB ÷2 geçiş adımı **olmadan** doğrudan ÷1'e geçiyordu (sol md.1). O turlarda SMPS ve LDO 628 s'lik pencerelerde sıfırlama, hata ve sıra boşluğu yoktu; oran `steady_clock`'a göre −5,18 ppm (SMPS) ve −5,69 ppm (LDO), `system_clock`'a göre −23,19 ppm (SMPS; LDO'da t ≈ 596 s'de −91 ms saat sıçraması nedeniyle geçersiz). O kayıtlarda UART hata sayacı yoktu ve LDO turunun açılış satırı port kapalıyken kaybolmuştu; düzeltilmiş çözümleyici ikisini de geçersiz sayar. Ham veri `${IDA_CACHE}/mp1/k2/*-20261114-*`.

## 4. Bulgular

1. **01'deki "Birincil (ST'nin DK örneği)" sütunu yalnız USBX Host örneklerine uyar.** STM32CubeU5 v1.9.0'da DK'nın `Templates/TrustZoneDisabled`, `Examples/RCC/RCC_ClockConfig`, `Examples/BSP` ve `Examples/DSI/DSI_VideoMode_SingleBuffer` örnekleri PLL1'i MSIS 4 MHz'ten besler; HSE 16 MHz + PLL1 (M = 4, N = 80, R = 2) dizisi `Applications/USBX/Ux_Host_CDC_ACM` örneğindedir.
2. **STM32U59x/5Ax'te 80 MHz'i aşan geçiş adımı.** ST'nin DK örnekleri ve sabitlenen HAL, MSIS 4 MHz'ten PLL1 160 MHz'e AHB ÷1 ile doğrudan geçer; ST'nin RM0456 §10.5.4 açıklaması geçici ÷2 ve 5 µs bekleme ister (3.4). 01 §2.2 md.8'e taşınmalı.
3. **Picolibc bu araç zinciri paketinde yok.** CubeIDE'nin GNU Tools for STM32 14.3.rel1 (macOS arm64) paketinde `picolibc.specs` ya da Picolibc kitaplığı bulunmadı; prototip newlib-nano ile bağlandı (01 §2.10, 00 08.7).
4. **HAL, `FRACN` = 0 iken de `PLL1FRACEN`'i 1 yapar.** İlk sürümde geri okuma bunu "kesirli kip" sayıp `E SAAT_HATASI adim=6` bildirdi; hesap `RCC_PLL1FRACR` dahil edilerek düzeltildi. Ürün yazılımındaki `SYSCLK` geri okuması `FRACN`'i hesaba katmalı.
5. **01 §2.2 md.8'in "doğrulanmadı" kayıtları:**
   - `PLLRGE`: sabitlenen HAL'in `stm32u5xx_hal_rcc.h` dosyasında `__HAL_RCC_PLL_VCIRANGE` açıklaması: `RCC_PLLVCIRANGE_0` (`0x0`) "4 ile 8 MHz arası", `RCC_PLLVCIRANGE_1` (`0x3`) "8 ile 16 MHz arası". ST'nin SVD dosyası (`STM32U5A9.svd`, CubeIDE `productdb.debug_2.2.500` eklentisi) `0x3`'ü aynı biçimde tanımlar. Birincil dizide giriş 4 MHz → `0x0`; yedek dizide 16 MHz → `0x3`. RM0456 tablosu ayrıca okunamadı.
   - `PLLMBOOST`: SVD'ye göre EPOD booster giriş saati bölücüsü, `0x0` = ÷1 … `0x8` = ÷16; ST örneği DIV1 (booster saati 16 MHz). Booster giriş aralığı (4–16 MHz) yalnız ST topluluk yanıtında görüldü; **RM0456 ile doğrulanmadı**.
   - Booster dizisi HAL tarafından yönetilir (`ControlVoltageScaling` `BOOSTEN`'i kurar, `ClockConfig` `BOOSTRDY`'yi bekler); kartta `BOOSTEN` = `BOOSTRDY` = 1.
   - Flash bekleme durumu: ST örnekleri 160 MHz/Range 1'de 4; kartta `LATENCY` = 4. **RM0456 tablosuyla doğrulanmadı**.
6. **VCP, port kapalıyken veri tutuyor ve kaybediyor.** PC portu açık değilken gelen satırların bir kısmı port açılınca tek parça geliyor, bir kısmı kayboluyor (ilk turda yeni imajın açılış satırları ve ilk 3–5 T satırı gelmedi). Düzeltme turunda kayıt yüklemeden önce açılır. Kayıp ölçen araçlar (MP3) portu kart sıfırlanmadan önce açık tutmalı.
7. **IWDG sıfırlaması hata ayıklama altında:** dondurma biti kapalıyken IWDG durdurulmuş işlemciyi sıfırladı ve işlemci gdb'nin denetimi dışında çalışmaya başladı; firmware'den kurulan dondurma biti (`__HAL_DBGMCU_FREEZE_IWDG`) bunu önlüyor.
8. **`REV_ID` `0x3000`, ES0553 Rev 6'da listelenmiyor** (sol'un aktarımı; 3.1).

## 5. Doğrulanmayanlar

- RM0456 (§10.5.4 metni ve revizyonu, PLL aralık tablosu, booster giriş aralığı, gecikme tablosu), AN5766 PDF revizyonu, UM2967 (kristal parça numarası ve toleransı) ve DS13543 doğrulanmadı. AN5766'nın 3 µs değeri (HTML) ve ES0553 Rev 6 listesi sol ikinci incelemesinde doğrulandı; bu ortamdan açılamadılar.
- HSE'nin ve Mac saatlerinin mutlak frekansı; iki Mac saatinden hangisinin doğru olduğu.
- HSE açılmaması ve PLL kilitlenmemesi hata yolları (yalnız adım 6 denendi).
- `.noinit`'in sıfırlamada korunması (yalnız seçenek baytı okundu).
- İlk turdaki yedek okumalarının programlayıcı çıkış kodları (o zaman kaydedilmiyordu).
- LSI frekansı ve gerçek IWDG süresi (nominal ≈ 1 s; M19).
- 1 saatlik SMPS/LDO koşuları, USB HS yükü (MP2 ile); Windows'ta aynı zincir (M21'in Windows yarısı; MP8); Picolibc ile derleme.
- CubeProgrammer CLI'ın bağlantıda DBGMCU'ya yazdığı değer.

## 6. Ölçüt özeti (düzeltilmiş ölçütlere göre)

| # | Ölçüt | Sonuç |
|---|---|---|
| K0 | Seçenek baytları ve `REV_ID` kayıtlı; kalıcı belleğe ve seçenek baytlarına yazma yok | **Geçti** |
| Yedek | Dahili flash yedeği, SHA-256 kayıtlı | **Geçti** |
| K1 | Temiz dizinden iki tur aynı sonuç; kesme noktası, `main`'e kadar yığın izi; IWDG açıkken 10 s durdurulan işlemci sıfırlanmaz, devam edince çalışır | **Geçti** |
| K2 | Geri okunan `SYSCLK` kaynağı PLL1, 160 MHz; DWT/PC oranı hangi PC saatine göre olduğu belirtilerek raporlanır; SMPS ve LDO turları sıfırlama, saat hatası ve sıra boşluğu olmadan | **Geçti:** PLL1/160 MHz (11/11); oran `steady_clock`'a göre SMPS −6,67, LDO −3,83 ppm (`system_clock`'a göre SMPS +18,44 ppm, LDO geçersiz); iki 637 s'lik tur sıfırlamasız, saat hatasız, boşluksuz |
