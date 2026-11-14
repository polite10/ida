# MP1a — Kart araç zinciri ve fabrika durumu (ön tur)

| | |
|---|---|
| Görev | `docs/gorevler/W0-MP1a.md` (W0, ölçüm prototipi) |
| Ölçüm | M6 (fabrika durumu), M21 (Mac yarısı), M4 (kısa tur); B01 K0–K2 |
| Uygulayan | `ida-uygulayici` (Opus) |
| Dal | `olcum/mp1-arac-zinciri` |
| Tarih | 2026-11-14 |
| Durum | Ölçüm raporu; inceleme (sol) bekliyor |

Bu rapor MP1'in **ön turudur**: 1 saatlik ve USB HS yükü altındaki SMPS/LDO koşuları MP2 ile birlikte yapılacak (görev belgesi, Amaç). Geçme ölçütleri görev belgesinin dondurduğu hedeflerdir; aşağıdaki "geçti/geçmedi" yalnız onlara göredir.

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
| ST kaynakları | STM32CubeU5 `v1.9.0` (`d88042df24f16799957c24f00c2b234c9e306188`); alt modül commit'leri: `stm32u5xx-hal-driver` `2552682ee5a0d6f826a6435750fc9267be65868b` (etiket v1.6.3), `cmsis-device-u5` `624374fa1e21ca195d6f2102ac0caaa50d0ea4c8` (v1.4.3), `stm32u5x9j-dk-bsp` `a5723a4ff404cb659b3c095d59527a0b0c2062f0` | `tools/olcum/mp1/kaynak-indir.sh`; `${IDA_CACHE}/src` altında, depoya girmez |
| libc | newlib-nano (`--specs=nano.specs --specs=nosys.specs`) | Picolibc bu araç zinciri paketinde yok (4. başlık, bulgu 3) |

Homebrew'deki `arm-none-eabi-gcc` kullanılmadı (09 §2.6). Araç yolları `tools/olcum/mp1/ortam.sh` içinde CubeIDE paketinden bulunur; ortam değişkenleriyle ezilebilir.

## 2. Komutlar ve dosyalar

Bütün komutlar depo kökünden; ham veri `${IDA_CACHE:-$HOME/.cache/ida}` altındadır, depoya girmez.

| Adım | Komut | Ham veri |
|---|---|---|
| K0 | `sh tools/olcum/mp1/k0-oku.sh` | `${IDA_CACHE}/mp1/k0/` (`ob-displ.txt`, `ham-yazmaclar.txt`, `probe-listesi.txt`) |
| Yedek | `sh tools/olcum/mp1/yedek-al.sh` | `${IDA_CACHE}/yedek/` (iki okuma, `SHA256SUMS`) |
| Kaynaklar | `sh tools/olcum/mp1/kaynak-indir.sh` | `${IDA_CACHE}/src/` |
| K1 derle-yükle | `sh tools/olcum/mp1/k1-derle-yukle.sh` (içinde iki kez `derle.sh` + `yukle.sh smps`) | `${IDA_CACHE}/mp1/k1/` |
| K1 hata ayıklama | `sh tools/olcum/mp1/k1-hata-ayikla.sh` (`gdb/k1-iwdg.gdb`) | `${IDA_CACHE}/mp1/k1/gdb.log`, `gdb-seri.csv` |
| K2 | `sh tools/olcum/mp1/k2-tur.sh smps 630`, `sh tools/olcum/mp1/k2-tur.sh ldo 630` | `${IDA_CACHE}/mp1/k2/` |

Aygıt yazılımı: `tools/olcum/mp1/fw/main.c` (tek dosya; HAL ile saat, USART1, IWDG; DWT 64 bit). İki varyant aynı kaynaktan, yalnız `MP1_REG_SMPS` tanımıyla ayrılır (`mp1_smps.elf`, `mp1_ldo.elf`). PC aracı: `tools/olcum/mp1/pc/mp1_seri.cpp` (`kayit` ve `analiz` alt komutları).

## 3. Sonuçlar

### 3.1 K0 — fabrika durumu (ilk yüklemeden önce)

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

Gözlem: CLI her bağlantıda "Debug in Low Power mode enabled" yazar; yani bağlanırken DBGMCU'nun düşük güç hata ayıklama ayarına dokunuyor. Bu kalıcı olmayan bir hata ayıklama yazmacıdır (seçenek baytı ya da flash değil); "yazma yok" ifadesi kalıcı bellek için geçerlidir. CLI'ın tam olarak hangi yazmaca hangi değeri yazdığı **doğrulanmadı**.

01 §2.8 ve §4 ile ilişki: `SRAM_RST` ve `SRAM2_RST` 1 olduğundan `.noinit` sistem sıfırlamasında korunur (ardışık arıza sayacı tasarımı bu varsayıma dayanır). `BOR_LEV` = 0, 1,8 V'luk VDD ile uyumlu tek düzeydir (01 §4). `IO_VDD_HSLV` = 1 ve `IO_VDDIO2_HSLV` = 1 fabrikada açık.

**K0: geçti.** Seçenek baytları ve `REV_ID` kayıtlı; kalıcı belleğe yazma yapılmadı (yukarıdaki DBGMCU notuyla).

### 3.2 Fabrika yazılımının yedeği

`-u 0x08000000 0x400000` iki kez (Hot Plug kipinde), ikisi aynı:

| Dosya | Boyut | SHA-256 |
|---|---|---|
| `${IDA_CACHE}/yedek/u5a9j-dk-dahili-flash-fabrika.bin` | 4.194.304 B | `1f13870b132a61d34f3649c96e9a789914f692dd45081f10dcd80250e0e22457` |

Silinmemiş (≠ `0xFF`) bayt sayısı 3.907.315; fabrika demosu dahili flash'ın neredeyse tamamını kullanıyor. Harici OSPI flash okunmadı ve yedeklenmedi (görev belgesi: dokunulmaz); demonun OSPI içeriğine bağlı olup olmadığı **doğrulanmadı**.

Geri yükleme (yapılmadı; gerektiğinde): `STM32_Programmer_CLI -c port=SWD -w ${IDA_CACHE}/yedek/u5a9j-dk-dahili-flash-fabrika.bin 0x08000000 -v -rst`.

Bu çalışmada yalnız sektör 0–2 (3 × 8 KB) silinip yazıldı (`Erasing internal memory sectors [0 2]`); fabrika görüntüsünün geri kalanı yerinde ama demo artık açılmaz.

**Yedek: geçti.**

### 3.3 K1 — derle, yükle, ayıkla

**Derleme ve yükleme, iki tur.** `k1-derle-yukle.sh` her turda `derleme/` dizinini silip `cmake --preset mp1-fw && cmake --build --preset mp1-fw` (ve PC aracı) çalıştırır, `mp1_smps.elf`'i `STM32_Programmer_CLI -c port=SWD -w <elf> -v -rst` ile yükler ve sıfırlamadan sonraki seri satırları kaydeder.

| | Tur 1 | Tur 2 |
|---|---|---|
| `mp1_smps.elf` SHA-256 | `7d8029dd479d5bcde1982b731e62c89975c78167ac4f0438be37a2a2dc33b815` | aynı |
| `mp1_smps.bin` SHA-256 | `bef4a4f58e1ccaad8cce90c0773421553933af65556ae9af54241189fba4085d` | aynı |
| `mp1_ldo.elf` / `.bin` | `6f4b9a9f…` / `226f4bae…` | aynı |
| Yükleme | sektör 0–2 silindi, "Download verified successfully" | aynı |
| Açılış satırı | `B MP1a reg_istenen=SMPS … idcode=0x30006481 dbg_apb1fzr1=0x00001000 saat_adim=0` | aynı |

İmaj boyutu: 19.120 B kod, 12 B veri, 1.776 B bss.

**Hata ayıklama.** `ST-LINK_gdbserver -p 61234 -l 1 -d -g -cp <CubeProgrammer bin>` çalışan hedefe bağlandı; `arm-none-eabi-gdb -batch -x tools/olcum/mp1/gdb/k1-iwdg.gdb`:

- `break mp1_isaret` → durdu. Yığın izi:
  `#0 mp1_isaret (n=47) main.c:296` · `#1 satir_gonder (sira=47) main.c:309` · `#2 main () main.c:373`.
- **IWDG açık, dondurma biti açık** (`DBGMCU_APB1FZR1` = `0x00001000`, `DBG_IWDG_STOP`): IWDG süresi nominal ≈ 1,0 s (LSI 32 kHz / 32, reload 999; LSI ölçülmedi). İşlemci kesme noktasında 10 s bekletildi (`shell sleep 10`, 16:13:26 → 16:13:37), sonra `continue`: sonraki kesme noktasında sayaç 47 → 48, gdb çıktısı `IWDG_SONUC SIFIRLAMA_YOK`. Seri kayıtta T 47 ile T 48 arası 11,45 s; arada açılış (`B`) satırı yok.
- **Karşı deneme** (görev belgesinde istenmedi; testin anlamlı olduğunu göstermek için): dondurma biti gdb'den temizlendi (`0x00000000`), işlemci kesme noktasında durduruldu. Yaklaşık 2 s sonra seri porttan yeni açılış satırı geldi: `rcc_csr=0x24004400 iwdgrst=1 pinrst=1`. Yani bit kapalıyken IWDG durdurulmuş işlemciyi sıfırladı ve işlemci sıfırlamadan sonra (gdb hâlâ "durdu" sanırken) kendiliğinden çalışmaya devam etti.

**K1: geçti** (Mac yarısı). Temiz dizinden iki tur aynı özetler ve aynı açılış; kesme noktası ve `main`'e kadar yığın izi; dondurma biti açıkken 10 s durdurulan işlemci sıfırlanmadı ve devam ettirilince çalıştı. Windows yarısı (M21) bu işin kapsamında değil.

### 3.4 K2 — 160 MHz (kısa tur)

**Saat dizisi** (`fw/main.c` `saat_kur`): ST'nin STM32U5x9J-DK `Applications/USBX/Ux_Host_CDC_ACM` örneğindeki `SystemClock_Config` ve `SystemPower_Config` ile aynı değerler:

1. `__HAL_RCC_PWR_CLK_ENABLE`, `HAL_PWREx_DisableUCPDDeadBattery`;
2. `HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY | PWR_LDO_SUPPLY)` (varyanta göre);
3. `HAL_PWREx_ControlVoltageScaling(SCALE1)` — HAL, Range 1/2'ye geçerken EPOD `BOOSTEN`'i kendisi kurar;
4. HSE açık (ayrı çağrı; açılmazsa adım 3 hatası);
5. PLL1: kaynak HSE, `PLLMBOOST` = DIV1, M = 4, N = 80, P = 8, Q = 2, R = 2, `PLLRGE` = `RCC_PLLVCIRANGE_0`, `FRACN` = 0 (ayrı çağrı; kilitlenmezse adım 4 hatası);
6. `HAL_RCC_ClockConfig(SYSCLK = PLL, AHB/APB1/2/3 = /1, FLASH_LATENCY_4)`; HAL, PLL'e geçmeden önce `BOOSTRDY`'yi bekler.

Her adımın hatasında kaynak değiştirilmez: işlemci MSIS 4 MHz'te kalır, kırmızı LED yanar, seri porta `E SAAT_HATASI adim=<n> hal_rc=<k>` yazılır, ölçüm yapılmaz. Bu yol ilk denemede (aşağıdaki bulgu 4) gerçekten çalıştı ve hatayı bildirdi.

**Geri okuma** (iki varyantta da açılışta ve her 60 s'de bir; `K` ve `P` satırları):

| Alan | Okunan | Anlamı |
|---|---|---|
| `RCC_CFGR1` | `0x0000000F` → `SWS` = 3 | SYSCLK kaynağı **PLL1** |
| `RCC_PLL1CFGR` | `0x00040313` | `PLL1SRC` = 3 (HSE), `PLL1RGE` = 0, `PLL1FRACEN` = 1, `PLL1M` = 0 (÷4), `PLL1MBOOST` = 0 (÷1), `PLL1REN` = 1 |
| `RCC_PLL1DIVR` | `0x01010E4F` | N = 80, P = 8, Q = 2, R = 2 |
| `RCC_PLL1FRACR` | `0x00000000` | FRACN = 0 |
| Hesap | 16 MHz / 4 × 80 / 2 | **160.000.000 Hz** (yazmaçlardan); `HAL_RCC_GetSysClockFreq` = 160.000.000 |
| `RCC_CR` | `0x03030005` | `HSERDY` = 1, `PLL1RDY` = 1 |
| `FLASH_ACR` | `0x00000104` | `LATENCY` = 4, `PRFTEN` = 1 |
| `PWR_VOSR` | `0x0007C000` | `VOS` = 3 (Range 1), `VOSRDY`, `BOOSTEN`, `BOOSTRDY` = 1 |
| `PWR_SVMSR` (SMPS) | `0x00038002` | `REGS` = 1 (SMPS), `ACTVOS` = 3 |
| `PWR_SVMSR` (LDO) | `0x00038000` | `REGS` = 0 (LDO), `ACTVOS` = 3 |

LDO varyantında diğer bütün yazmaçlar SMPS ile aynı okundu (`RCC_CR`, `CFGR1`, `PLL1CFGR`, `PLL1DIVR`, `PLL1FRACR`, `PWR_VOSR`, `FLASH_ACR`).

**DWT/PC oranı.** Aygıt yazılımı DWT `CYCCNT`'yi 64 bite genişletir ve her 160·10⁶ çevrimde `T <sıra> <çevrim>` yazar. PC aracı her satırın `\n` baytının okunduğu anı iki saatle damgalar ve en küçük kareler eğimini (çevrim/s) çıkarır. Port açılırken tamponda bekleyen satırlar tek parça geldiği için ilk 0,3 s'nin satırları atılır.

Turlar 2026-11-14'te, SMPS 16:16–16:27, LDO 16:27–16:38 (yerel saat); her biri 630 s kayıt. Yüklenen imajlar K1'deki özetlerle aynı (`mp1_smps.elf` `7d8029dd…`, `mp1_ldo.elf` `6f4b9a9f…`).

| | SMPS | LDO |
|---|---|---|
| Ölçüm penceresi (PC, `steady_clock`) | 628,0 s | 628,0 s |
| Çözümlenen T satırı | 629 (sıra 6 → 634) | 629 (sıra 5 → 633) |
| Sıra boşluğu | 0 | 0 |
| Pencerede `B` (açılış = sıfırlama) / `E` (hata) satırı | 0 / 0 | 0 / 0 |
| `K` satırları: SYSCLK = PLL1 ve 160 MHz | hepsi (11/11; biri önceki imajdan tamponda kalmış) | hepsi (10/10) |
| `REGS` (her 60 s) | SMPS | LDO |
| **DWT/PC, `steady_clock`'a göre** | **159.999.170,99 Hz → −5,18 ppm** (eğimin istatistiksel std. hatası 0,013 ppm; ilk yarı −5,41, ikinci yarı −5,31) | **159.999.090,22 Hz → −5,69 ppm** (std. hata 0,014 ppm; ilk yarı −6,05, ikinci yarı −5,67) |
| DWT/PC, `system_clock`'a göre | −23,19 ppm (ilk yarı −23,37, ikinci yarı −23,18) | **geçersiz:** t ≈ 596 s'de sistem saati `steady_clock`'a göre ≈ 91 ms geri sıçradı; sıçramadan önceki ilk yarı −23,92 ppm |
| Satır varışı artığı (`steady_clock`) | rms 61 µs, en büyük 326 µs | rms 64 µs, en büyük 272 µs |

Tablodaki değerler, turlardan sonra çözümleyiciye "ölçüm penceresi" ve saat sıçraması denetimi eklenerek **aynı CSV'lerin** yeniden çözümlenmesinden gelir (`tools/olcum/mp1/derleme/pc/mp1_seri analiz <csv>`); kartta yeni koşu yapılmadı. İlk çözümleme SMPS turunda bir `B` satırı saymıştı: o satır bu turun kendi açılış satırıdır ve port açılınca önceki imajın tampondaki 80 satırından hemen sonra gelmiştir (bulgu 6); sıra 6'dan kesintisiz sürer.

Standart hatalar yalnız satır varış titreşiminden gelen istatistiksel belirsizliktir; PC saatinin kendi hatasını içermez.

**PC saatinin belirsizliği.** Kartın HSE'si bağımsız bir frekans referansıyla ölçülmedi; ölçülen şey kart saatinin Mac saatine oranıdır. Mac'in iki saati aynı turda yaklaşık **18 ppm** farklı eğim verdi (SMPS: −5,18 ve −23,19 ppm): `steady_clock` düzeltilmemiş donanım zaman tabanıdır, `system_clock` NTP ile disiplinlidir ve `timed` onu kaydırarak ya da sıçratarak düzeltir (LDO turundaki 91 ms'lik sıçrama). Hangisinin gerçeğe yakın olduğu bu düzenekle ayrılamaz; fark, referansın belirsizliğinin alt sınırı sayılmalıdır. `sntp time.apple.com` (yalnız okuma) sistem saati farkı: SMPS turu başında +0,2176 ± 0,076 s, sonunda +0,2053 ± 0,075 s; LDO turu başında +0,2067 ± 0,069 s, sonunda +0,2897 ± 0,086 s. SMPS turundaki değişimin yönü `system_clock`'un tur boyunca hızlı çalıştığı yorumuyla uyumludur, ama tek ölçümlerin belirsizliği 630 s'de ±240 ppm sınırına karşılık geldiğinden bu bir kanıt değildir. İki varyant arasındaki −5,18 / −5,69 ppm farkı (0,5 ppm) regülatöre mi, sıcaklığa mı, PC saatine mi bağlı, ayrılmadı.

**Tolerans.** Kartın HSE'si Zephyr kart belgesine göre 16 MHz kristaldir (ikincil kaynak). Kristalin parça numarası ve frekans toleransı birincil kaynaktan (UM2967 malzeme listesi ya da kart şeması) **alınamadı**: st.com bu ortamdan PDF indirmeye yanıt vermedi (WebFetch ve `curl` zaman aşımı). Bu yüzden ölçülen sapma bir veri sayfası toleransıyla karşılaştırılamadı.

**K2: geçmedi (kanıt eksik).** Alt ölçütler:
- `SYSCLK` kaynağı PLL1, frekans yazmaçlardan 160 MHz: **geçti** (iki varyantta, açılışta ve 10 dk boyunca her 60 s'de).
- SMPS ve LDO turlarının ikisi de ≥ 10 dk sıfırlamasız ve hatasız: **geçti** (sıra kesintisiz; çevrim sayaçları açılıştan beri sürekli; pencerede `B`/`E` yok).
- DWT/PC sapmasının kaynaklı HSE kristal toleransı içinde olması: **gösterilemedi.** Ölçülen sapma Mac'in ham saatine göre −5,2 / −5,7 ppm, NTP saatine göre ≈ −23 ppm; tolerans değeri birincil kaynaktan alınamadı ve PC referansının kendisi en az ≈ 18 ppm belirsiz. Kapatmak için: UM2967 malzeme listesinden kristalin toleransı, ve kalibre bir referans (ör. GPS 1PPS ya da frekans sayacı ile MCO çıkışı, PA8) ya da daha uzun, NTP'si kararlı bir kayıt. Bu düzeneği kurmak bu işin kapsamında değildir.

## 4. Bulgular

1. **01'deki "Birincil (ST'nin DK örneği)" sütunu yalnız USBX Host örneklerine uyar.** STM32CubeU5 v1.9.0'da DK'nın `Templates/TrustZoneDisabled`, `Examples/RCC/RCC_ClockConfig`, `Examples/BSP` ve `Examples/DSI/DSI_VideoMode_SingleBuffer` örnekleri PLL1'i **MSIS 4 MHz**'ten besler (M = 1, N = 80, R = 2, `PLLRGE` = 0, `PLLMBOOST` = DIV1, gecikme 4; `ConfigSupply(SMPS)`). HSE 16 MHz + PLL1 (M = 4, N = 80, R = 2) dizisi `Applications/USBX/Ux_Host_CDC_ACM` örneğindedir (bu iş onu kullandı).
2. **01 §2.2 md.8'in "doğrulanmadı" kayıtları:**
   - `PLLRGE`: ST'nin SVD dosyası (`STM32U5A9.svd`, CubeIDE `productdb.debug_2.2.500` eklentisi) `0x3` = "PLL1 input (ref1_ck) clock range frequency between 8 and 16 MHz" der; diğer kodları listelemez. HAL `RCC_PLLVCIRANGE_0` = `0x0`, `RCC_PLLVCIRANGE_1` = `0x3`'tür. Birincil dizide giriş 4 MHz ve ST örneği `RCC_PLLVCIRANGE_0` kullanır; yedek dizide (16 MHz giriş) `RCC_PLLVCIRANGE_1` (`0x3`) gerekir. 0x0'ın 4–8 MHz anlamına geldiği yalnız ikincil kaynaklarda (Embassy `stm32-metapac` belgesi) görüldü; RM0456 metni okunamadı.
   - `PLLMBOOST`: SVD'ye göre EPOD booster giriş saati bölücüsü, `0x0` = ÷1 … `0x8` = ÷16. ST'nin DK örneği DIV1 kullanır (booster saati HSE'de 16 MHz). Booster giriş aralığının 4–16 MHz olduğu yalnız bir ST topluluk yanıtında görüldü (RM0456 §10.5.4'e atıf); **RM0456 metniyle doğrulanmadı**.
   - Booster dizisi: HAL `ControlVoltageScaling(SCALE1)` `BOOSTEN`'i kurar; `OscConfig` PLL'i yapılandırırken `BOOSTEN`'i geçici kapatıp geri açar; `ClockConfig` PLL'e geçmeden `BOOSTRDY`'yi bekler (`stm32u5xx_hal_pwr_ex.c`, `stm32u5xx_hal_rcc.c`, commit `2552682`). Kartta `BOOSTEN` = `BOOSTRDY` = 1 okundu.
   - Flash bekleme durumu: ST örnekleri 160 MHz/Range 1'de `FLASH_LATENCY_4`; kartta `LATENCY` = 4 okundu. RM0456 tablosuyla **doğrulanmadı**.
3. **Picolibc bu araç zinciri paketinde yok.** CubeIDE'nin GNU Tools for STM32 14.3.rel1 (macOS arm64) paketinde `picolibc.specs` ya da Picolibc kitaplığı bulunmadı; yalnız newlib/newlib-nano var. 01 §2.10 ve 00 08.7'deki "birincil Picolibc" bu derleyici paketiyle doğrudan sağlanamıyor; prototip yedek olan newlib-nano ile bağlandı. Picolibc'nin CubeCLT ya da CubeMX ile ayrı geldiği **doğrulanmadı**.
4. **HAL, `FRACN` = 0 iken de `PLL1FRACEN`'i 1 yapar** (`HAL_RCC_OscConfig` her zaman `__HAL_RCC_PLL_FRACN_ENABLE`). İlk sürümde geri okuma `FRACEN = 1`'i "kesirli kip" sayıp hesabı reddetti ve aygıt yazılımı doğru biçimde `E SAAT_HATASI adim=6` bildirdi; hesap `RCC_PLL1FRACR` dahil edilerek düzeltildi. Ürün yazılımındaki `SYSCLK` geri okuması (01 §2.2 md.8) `FRACN`'i hesaba katmalı.
5. **`REV_ID` = `0x3000`.** CLI bunu "Rev C" diye yorumluyor. ES0553 (STM32U59x/5Ax errata) PDF'i bu ortamdan açılamadı; bir arama özeti (ikincil, **doğrulanmadı**) ES0553 Rev 5'in yalnız `0x3001` (X) ve `0x3002` (W) revizyonlarını listelediğini söylüyor. `0x3000`'in errata kapsamına girip girmediği M6'nın açık noktası olarak kalır.
6. **VCP, port kapalıyken veri tutuyor ve kaybediyor:** PC portu açık değilken gelen satırların bir kısmı ST-LINK/sürücü tarafında saklanıp port açılınca tek parça geliyor (K1'de 30, K2 SMPS turunda önceki imajdan 80 satır), bir kısmı ise kayboluyor (K2'de yeni imajın açılış satırları ve ilk 3–5 T satırı hiç gelmedi; SMPS turunun açılış satırı porttan 0,9 s gecikmeli geldi). Zaman damgası kullanan araçlar ilk yığını atmalı; kayıp ölçen araçlar (MP3) portu kart sıfırlanmadan önce açık tutmalı.
7. **IWDG sıfırlaması hata ayıklama altında:** dondurma biti kapalıyken IWDG durdurulmuş işlemciyi sıfırladı ve işlemci gdb'nin denetimi dışında çalışmaya başladı. 01 §2.2 md.4'teki "hata ayıklama derlemesinde dondurma biti açık" kuralı gerekli; bit firmware'den kurulduğunda (`__HAL_DBGMCU_FREEZE_IWDG`) çalışıyor.
8. **Kart USB numaralandırması:** ST-LINK hub üzerinden sorunsuz çalıştı; bu oturumda erişim kesilmedi.

## 5. Doğrulanmayanlar

- UM2967 (kristal parça numarası ve toleransı), RM0456 (PLL aralık kodlarının tam tablosu, booster giriş aralığı, gecikme tablosu), DS13543 ve ES0553 PDF'leri bu ortamdan açılamadı; yukarıda ilgili her kayıt ikincil ya da ST yazılım kaynağıyla işaretlendi.
- HSE'nin mutlak frekansı (bağımsız referans yok; yalnız Mac saatine oran).
- LSI frekansı ve gerçek IWDG süresi (nominal ≈ 1 s; M19).
- 1 saatlik SMPS/LDO koşuları, USB HS yükü (MP2 ile).
- Windows'ta aynı zincir (M21'in Windows yarısı; MP8).
- Picolibc ile derleme.
- CubeProgrammer CLI'ın bağlantıda DBGMCU'ya yazdığı değer.

## 6. Ölçüt özeti

| # | Ölçüt (görev belgesi) | Sonuç |
|---|---|---|
| K0 | Seçenek baytları ve `REV_ID` kayıtlı; yazma yok | **Geçti** |
| Yedek | Dahili flash yedeği, SHA-256 kayıtlı | **Geçti** |
| K1 | Temiz dizinden iki tur aynı sonuç; kesme noktası, `main`'e kadar yığın izi; IWDG açıkken 10 s durdurulan işlemci sıfırlanmaz, devam edince çalışır | **Geçti** |
| K2 | `SYSCLK` = PLL1, 160 MHz; DWT/PC sapması HSE kristal toleransı içinde (kaynaklı); SMPS ve LDO turları sıfırlamasız ve hatasız | **Geçmedi (kanıt eksik):** PLL1/160 MHz ve iki hatasız 10 dk tur geçti; sapma (−5,2 / −5,7 ppm, Mac ham saatine göre) kaynaklı toleransla karşılaştırılamadı, PC referansı ≈ 18 ppm belirsiz |
