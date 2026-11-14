# W0-MP1a — Kart araç zinciri ve fabrika durumu (ön tur)

| | |
|---|---|
| Kimlik, dalga | `W0-MP1a`, W0 (ölçüm prototipi) |
| Uygulayıcı | `ida-uygulayici` (Opus) |
| İnceleyen | sol (`tools/sol-incele.sh`; çağrıyı mimar başlatır) |
| Dal | `olcum/mp1-arac-zinciri` |
| Tarih | 2026-11-14 |

## Amaç

Bu Mac'ten STM32U5A9J-DK kartına derle-yükle-ayıkla zincirinin çalıştığını kanıtlamak, kartın fabrika durumunu kayda geçirmek ve 160 MHz saat yapılandırmasını denemek. Sonraki bütün kartlı ölçümler (MP2–MP5) bu zincire dayanır. MP1'in tamamı değil, **ön turudur**: uzun süreli koşular (1 saat, USB HS yükü altında SMPS ve LDO) MP2 ile birlikte yapılır.

## Okunacaklar

- `AGENTS.md`
- `docs/mimari/09-test-ve-is-akisi.md` (sürüm 0.2): 2.1, 2.6, 3.3, 6.1 (MP1 satırı)
- `docs/mimari/01-kart-yazilimi.md` (0.2): açılış dizisi (güç, HSE, PLL1 başlıkları), §6, §7'de K0–K2
- `docs/research/00-duzeltmeler-ve-kararlar.md`: 4.1 ve 4.8 düzeltme tabloları, Ç4, Ç8, M4, M6, M21
- Gerektiğinde `docs/research/08-kart-c-yigini.md` ve doğrulaması (ham rapor bilinen hatalar içerir; önce 00)

Bölüm 01'de "doğrulanmadı" işaretli her kayıt değeri (PLL aralık kodları, booster dizisi vb.) birincil kaynaktan (RM0456, kartın kullanım kılavuzu UM2967, ST'nin STM32CubeU5 örnekleri) denetlenir; kaynağı raporda yazılır.

## Ortam (mimar tarafından 2026-11-14'te gözlendi)

- Mac: Apple M3 Pro, 18 GB, macOS 26.5.2. Kart bir USB hub üzerinden bağlı; `STLINK_V3` (STMicroelectronics) yüksek hızda numaralanmış; bir `usbmodem` seri aygıtı görünüyor. Kartın ikinci (kullanıcı) USB bağlantısı takılı **değil**; bu iş onu gerektirmez.
- STM32CubeIDE kurulu; içinde GNU Tools for STM32 14.3.rel1 (`arm-none-eabi-gcc`), `STM32_Programmer_CLI` ve `ST-LINK_gdbserver` var (`/Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/` altında; yolları `find` ile bul, sürümleri kaydet). Homebrew'deki arm-none-eabi-gcc **kullanılmaz** (09 §2.6).
- `cmake` ve `ninja` kurulu değil; Homebrew ile kur (kullanıcı araç kurulumuna izin verdi). Kurduğun her şeyi sürümüyle rapora yaz.
- İndirilen kaynaklar (STM32CubeU5 HAL/CMSIS; sabit etiket ya da commit) depoya girmez: `${IDA_CACHE:-$HOME/.cache/ida}` altına indir; sürümü ve commit'i rapora yaz. Depoya kişisel makine yolu yazma.

## Kapsam ve sıra

1. **K0 — fabrika durumu (ilk iş, hiçbir yüklemeden önce).** `STM32_Programmer_CLI -c port=SWD` ile salt okunur: seçenek baytları (`-ob displ`), aygıt kimliği, `REV_ID`, flash boyutu, ST-LINK aygıt yazılımı sürümü. `TZEN`, `RDP`, `BOR_LEV`, SRAM silme ve ECC seçenek baytları rapora girer.
2. **Fabrika yazılımının yedeği.** İlk yüklemeden önce dahili flash'ın tamamını oku ve `${IDA_CACHE}/yedek/` altına kaydet (boyut ve SHA-256 rapora). Okuma başarısızsa (ör. `RDP` ≠ 0) **yükleme yapma, dur ve raporla**.
3. **K1 — derle, yükle, ayıkla.** `tools/olcum/mp1/` altında en küçük aygıt yazılımı: CMake + Ninja, CubeIDE'deki 14.3.rel1 araç zinciri, platform kodu `gnu17`. Kullanıcı LED'ini yakıp söndürür; USART1 (ST-LINK sanal seri portu) üzerinden 115200 baud'da satır yazar. Yükleme `STM32_Programmer_CLI` ile; `ST-LINK_gdbserver` + `arm-none-eabi-gdb` toplu kipte kesme noktası ve yığın izi alınır. IWDG açıkken işlemci hata ayıklayıcıda durdurulduğunda kartın sıfırlanmadığı gösterilir (hata ayıklama dondurma biti).
4. **K2 — 160 MHz (kısa tur).** HSE 16 MHz + PLL1 ile `SYSCLK` 160 MHz, gerilim ölçeği Range 1. `SYSCLK` kaynağı ve frekansı kayıtlardan geri okunur. DWT çevrim sayacının PC saatine oranı seri port üzerinden en az 10 dakika ölçülür (beklenen 160 MHz'ten sapma ppm olarak). Tur **SMPS ile ve LDO ile ayrı ayrı** yapılır. HSE açılmaz ya da PLL kilitlenmezse sessizce başka kaynağa dönülmez; durum seri porttan bildirilir.
5. **Rapor:** `docs/olcumler/MP1-arac-zinciri.md` (09 §3.3 biçimi).

## Geçme ölçütleri (bu görev belgesi dondurur; ölçülmüş değer değil, hedef)

| # | Ölçüt |
|---|---|
| K0 | Seçenek baytları ve `REV_ID` kayıtlı; kayıt hiçbir yazma içermeden alınmış |
| Yedek | Dahili flash yedeği alınmış, SHA-256 kayıtlı |
| K1 | Temiz dizinden tek komut dizisiyle derleme ve yükleme iki kez üst üste aynı sonucu verir; kesme noktasında durur, yığın izi `main`'e kadar çözülür; IWDG açıkken 10 s durdurulmuş işlemci sıfırlanmaz, devam ettirilince çalışır |
| K2 | Geri okunan `SYSCLK` kaynağı PLL1, frekans 160 MHz; DWT/PC oranının sapması kartın HSE kristalinin veri sayfası toleransı içinde (tolerans kaynağıyla yazılır); SMPS ve LDO turlarının ikisi de sıfırlama ve hata olmadan tamamlanır |

Bir ölçüt geçmezse zorlamadan raporla: ne denendi, ne görüldü, olası neden. "Geçmedi" geçerli bir sonuçtur.

## Dosya sahipliği

- Yazılır: `tools/olcum/mp1/**`, `docs/olcumler/MP1-arac-zinciri.md`, gerekirse `.gitignore` (derleme çıktısı ve ham veri için ek satır).
- Dokunulmaz: `docs/mimari/**`, `docs/research/**`, `docs/DURUM.md`, `AGENTS.md`, `README.md`, `docs/gorevler/**`, başka `tools/` dosyaları.
- Bu kod atılabilir prototiptir; ürün dizinlerini (`firmware/`, `core/`) oluşturma.

## Yasaklar

- **Seçenek baytlarına yazma yok.** `RDP`, `TZEN`, önyükleme ayarı, yazma koruması değiştirilmez; toplu silme (`mass erase`) yalnız dahili flash'ın programlanması için gereken kadarıyla yapılır. Harici flash'a (OSPI) dokunulmaz.
- ST-LINK aygıt yazılımını güncelleme; güncelleme önerilirse rapora yaz.
- Sözleşme (A1–A8) yazma ya da varsayma; USB aygıt yığını, RTOS, ekran bu işin kapsamında değil.
- Ölçülmemiş rakamı sonuç gibi yazma. Kapsam dışı: ardıl kart, başka mikrodenetleyici, Rust.
- Commit atma, `git add` yapma, `git push` yapma, başka agent başlatma. Dalı (`git switch -c olcum/mp1-arac-zinciri`) oluşturabilirsin; commit'i kayıt agent'ı atar.

## Rapor ve bitiş

Son iletinde ayrı başlıklarla: **yapılan**, **doğrulanan** (komut ve gerçek çıktı özetiyle), **doğrulanmayan**, **kurulan araçlar**, **çalışma dizininin yolu ve dal adı**, görev belgesinde hatalı bulduğun nokta. İş, rapor dosyası yazılıp dört ölçütün her biri "geçti" ya da gerekçeli "geçmedi" olarak işaretlendiğinde biter. Karta erişim kesilirse (kablo, hub, izin) beklemeden dur ve durumu bildir. Düzeltme turu sınırı iki.
