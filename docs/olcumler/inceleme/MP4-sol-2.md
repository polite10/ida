# sol ikinci incelemesi: mp4

## Hüküm

**Kabul yok.** İlk incelemenin #1, #3, #4 ve #5 maddeleri giderilmiş; kritik #2 kısmen giderilmiş. Aynı düzeltme kapsamında **3 kritik ve 1 önemli** açık bulgu var: çözümleyici bazı sayısal olarak olanaksız kayıtları hâlâ `GEÇERLİ` sayıyor.

Tamamlanmış O2 kaydının bütünlüğünü ve yeniden analizini doğruladım. Aşağıdaki kusurlar O2 ham verisinin bozuk olduğunu göstermiyor; PC tarafında, korunmuş kayıtlarla giderilebilir ve sınanabilir. Firmware yükleme veya kart ölçümünü tekrarlama gerektirmiyor.

Dosya değiştirmedim, kurulum yapmadım, karta veya seri porta erişmedim. Og/200 Hz sonuçlarının yokluğunu planlı kabul ettim; bu kampanyalara ve nihai rapora kabul vermiyorum.

## İlk incelemedeki maddelerin durumu

| # | Durum | Not |
|---|---|---|
| 1 | giderildi | Beklenen hız, maske ve faz adımı zorunlu; `kos.sh` bunları açıkça geçiriyor. Boş/daraltılmış maske, eksik/çift B2, yanlış saat/UART, sıfır hız ve yanlış T toplamı reddediliyor. Doğru beklenen parametrelerle veri hatası sonucu `rc=1` aldım; kullanım hatasını kapanış kanıtı saymadım. |
| 2 | kısmen | İlk rapordaki eksik S, `Q.nan` ve yanlış `lat_max` örnekleri artık reddediliyor; hatalı kayıtta konsolun sayısal özeti bastırılıyor. Ancak aşağıdaki Y1–Y3 yanlış kabulleri ve Y4 çıktı sorunu sürüyor. |
| 3 | giderildi | `duvar_cyc` doğru biçimde ilk adım başından son adım başına süre alt sınırı olarak sunuluyor. 600 saniye bu değerle denetleniyor; son adım dahil süre aralık olarak veriliyor. O2 hesabıyla eşleşiyor. |
| 4 | giderildi | Rapor geçiş adımlarını ve ölçüm çevresindeki işleri kapsama dahil ediyor; açılıştan beri monoton tepe iddiası kaldırılmış. Çözümleyici fazlarda gözlenen maksimumları kullanıyor: O2 için `3452 / 2500 B`. |
| 5 | giderildi | Çevrim/160 değerleri beş ondalıkla kesin gösteriliyor. `58 çevrim = 0,36250 µs`; eski yanlış `≤0,362` beklentisi kaldırılmış. Rastgele histogram denetimi de geçti. |

Yazarın özgün `hatali_saat` yeniden üretimindeki sınırlama açıklaması doğru: yeni B2 üzerinde eski değiştirme yinelenen alan oluşturuyor ve biçimden reddediliyor. Ayrıca alanları yinelemeden `saat_adim=7 hesap_hz=80000000` verdim; araç saat kurulumu ve frekans hatalarıyla `rc=1` döndü. Dolayısıyla bu kapanış yalnız biçim hatasına dayanmıyor.

## Yeni bulgular

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem | Kapatılacağı yer |
|---|---|---|---|---|---|
| Y1 | `tools/olcum/mp4/pc/mp4_seri.cpp:620` | **Bütün örnekler taşmışsa histogramın süre toplamı denetimi atlanıyor.** Test üreticisinin kampanyasında H satırlarını kaldırıp E’de `h=0`, F’de `tasan=1000 max=4000000` yaptım; sıra ve CRC yenilendi. `min=96003 top=96035119` korunmasına rağmen `rc=0 / GEÇERLİ`. Oysa 1000 örneğin tamamı 3.200.000 çevrim sınırının dışındaysa toplam en az **3.200.000.000 çevrim** olmalı; bildirilen minimum da aralık içinde olamaz. | Histogram toplamının alt/üst sınır hesabını H boşken de uygula. `tasan=n` durumunda minimumun taşma aralığında olduğunu doğrula. Geçerli ve geçersiz bütün-örnekler-taşmış durumlarını teste ekle. | kritik | W0-MP4a PC çözümleyicisi ve analiz testleri; ilk #2 kapanışı |
| Y2 | `tools/olcum/mp4/pc/mp4_seri.cpp:605` | **`yigin_tepe` üst sınırı denetlenmiyor.** Gerçek O2 kaydında yalnız A’nın `yigin_tepe=3392` alanını `4294967295` yapıp CRC’yi yeniledim. Gerçek uzun kampanya beklentileri ve 600 saniye eşiğiyle `rc=0`; koşu özeti **4.294.967.295 B yığın** bildiriyor. O2 map dosyasındaki boyanan alanın azami büyüklüğü yalnız **1.586.416 B**. `yigin_tepe=1`, `yigin_kayit=3000` çelişkisi de kabul ediliyor. | `yigin_tepe` için hedefin bellek sınırını ve iki yığın ölçümünün kapsama ilişkisini denetle. Üst sınır ve `yigin_tepe < yigin_kayit` örneklerini teste ekle. | kritik | W0-MP4a PC çözümleyicisi ve analiz testleri; ilk #2 kapanışı |
| Y3 | `tools/olcum/mp4/pc/mp4_seri.cpp:567` | **Bütün örnekler temizken temiz maksimum genel maksimumdan küçük olabiliyor.** Gerçek O2 A’da `lat_temiz_n=n=241000`, `lat_max=86`. Yalnız `lat_temiz_max=86→59` değişikliği ve yeni CRC ile uzun kampanya yine `GEÇERLİ`; temiz maksimum yanlış biçimde **0,36875 µs** basılıyor. Bütün örnekler aynı kümeye ait olduğundan maksimum **0,53750 µs** olmalı. | `lat_temiz_n=n` ise `lat_temiz_max=lat_max` eşitliğini zorunlu kıl; bu karşı örneği teste ekle. | kritik | W0-MP4a PC çözümleyicisi ve analiz testleri; ilk #2 kapanışı |
| Y4 | `tools/olcum/mp4/pc/mp4_seri.cpp:697`; `tools/olcum/mp4/pc/mp4_seri.cpp:716` | **Histogram CSV’leri kampanyanın geçerliliği kesinleşmeden yazılıyor.** Dosya yazımı faz döngüsünde, T denetiminden ve son `hata.n` kararından önce. Eksik/yanlış T durumunda bütün fazların; bir faz hatalıysa diğer fazların CSV’leri oluşturulabilir. Bu, yanıt dosyasındaki “herhangi bir hata varsa histogram CSV’leri yazılmaz” iddiasıyla çelişiyor. | Bütün denetimler tamamlandıktan sonra dışa aktar. Eksik T ve geçersiz başka faz durumlarında hiçbir histogram dosyası oluşmadığını sınayan test ekle. Bu bulgu kaynak akışından doğrulandı; dosya yazma deneyi yapmadım. | önemli | W0-MP4a PC dışa aktarımı, analiz testleri ve yanıt belgesi |

## Yeniden hesaplananlar

Mevcut testlerin analiz bölümünü geçici CSV oluşturmadan, girdiyi `/dev/stdin` üzerinden vererek çalıştırdım: **58 denetim geçti, 0 başarısız**. Buna 25 rastgele histogramın yüzdelik denetimi dahildir. Dosya yazan kayıt/PTY ve çağrı grafiği testlerini içeren tam 67’lik komutu bu incelemede yeniden çalıştırmadım.

İlk raporun yedi olumsuz örneği, zorunlu `--beklenen-*` parametreleri verilmişken:

| Örnek | Çıkış |
|---|---:|
| `faz_yok` | 1 |
| `B2_yok` | 1 |
| Özgün `hatali_saat` | 1 |
| `hz_sifir` | 1 |
| `S_alanlari_yok` | 1 |
| `Q_nan` | 1 |
| `lat_max_sifir` | 1 |

Hepsinde sayısal konsol özeti bastırıldı. Buna karşılık Y1–Y3 örnekleri `rc=0` verdi. Y2 ve Y3’te **400 Hz, maske 7, 241.000 adım, en az 240.000 adım ve 600 saniye** beklentileri korundu; ham dosya değiştirilmedi.

O2 ham kaydının SHA-256 değeri:

```text
09809edaf837670d4e6f300b8f5fce66df171b7a7070890025fb4ff7ecb06d85
```

CRC’yi ayrı bit tabanlı hesapla yeniden denetledim: açılıştan itibaren **3.539 satır**, sıra **0…3538**, CRC/sıra hatası **0**. Her fazın adım ve birleşik gecikme histogramı **241.000 örnek** içeriyor; taşma yok. T toplamı:

`2×400 + 3×(400 + 241000) = 725000 adım`.

| O2 fazı | Süre alt sınırı, s | p50 ≤, µs | p99 ≤, µs | p99,9 ≤, µs | Maksimum, µs | Aşım / atlanan tetik |
|---|---:|---:|---:|---:|---:|---:|
| A | 602,49750004375 | 601,69375 | 1336,49375 | 1338,09375 | 1338,91875 | 0 / 0 |
| B | 821,4287758375 | 3319,39375 | 3321,09375 | 3321,59375 | 3322,38125 | 241000 / 87573 |
| C | 602,4974999625 | 1299,19375 | 1300,09375 | 1300,69375 | 1301,73125 | 0 / 0 |

Üç faz da 600 saniye alt sınırını sağlıyor. A’nın son adım dahil süre aralığı **[602,4980953875; 602,4988389625] s**. Gecikme p99,9 üst sınırları A/B/C için **0,45000 / 2497,79375 / 0,47500 µs**. Yeniden üretilen sayısal çıktı, ayrı v2 analizindeki satırlarla eşleşti.

Kısa kayıtta da **302 satır**, sıra **0…301**, CRC/sıra hatası **0**, faz başına iki histogram toplamı **4000** doğrulandı. Kabul kampanyası sayılmadı.

O2 için **D1 geçmedi**: B’de 241.000 aşım var. **D2 geçmedi**: B maksimumu `3322,38125 µs > 1000 µs`. O2’nin histogram, yığın ve kod boyutu verileri mevcut; bütün kampanyanın D4 kapanışı Og verisiyle ayrıca değerlendirilmeli.

Arşivlenmiş objdump dosyaları üzerinde D3 aracı yeniden `rc=0` verdi: O2/Og’de **46/61 erişilen işlev**, çift duyarlıklı yordam/komut ve dolaylı çağrı **0**. Mevcut O2/Og/kısa ELF hash’leri ilk incelemedeki değerlerle aynı.

## İskelete ya da diğer bölümlere notlar

- Bu tur ölçüm ve kabul çözümleyicisiyle sınırlı. Yer tutucu yükün başarısız sonucu geçerli bir ölçüm sonucudur; gerçek çekirdeğin kontrol hızı veya taşıma dahil uçtan uca bütçesi için kabul değildir.
- Rapor gecikmeyi güncelleme olayı → ana döngü örnekleme noktası olarak açıkça ayırmış. Kesme → adım başlangıcı yüzdeliklerinin bu özetlerden türetilemeyeceği açıklaması doğru.
- Ana depodaki güncel `AGENTS.md` ve ana görev okundu. `docs/gorevler/W0-MP4a.md` bu worktree’de bulunmadığından ana depodaki kopyasına başvuruldu; devam ekleri öncelikli tutuldu.
- Canlı birincil kaynak denetiminde STM32U5A9’un 160 MHz, tek duyarlıklı FPU ve yaklaşık 2,5 MB SRAM özellikleri doğrulandı. Bu kapasite Y2’de kabul edilen yaklaşık 4 GB yığın değerini dışlar. [ST ürün belgesi](https://www.st.com/en/microcontrollers-microprocessors/stm32u5a9bj.html)
- Yeni B2 denetimindeki USART UE/TE/FIFOEN, ICACHE enable ve flash latency bitleri ST’nin sürümlü CMSIS başlığıyla eşleşiyor. FPCCR tembel yığınlama bitleri Arm başlığıyla doğrulandı. Bunlar kartın mutlak saatinin fiziksel ölçümü değildir. [ST CMSIS v1.4.3](https://github.com/STMicroelectronics/cmsis-device-u5/blob/v1.4.3/Include/stm32u5a9xx.h), [Arm Cortex-M33 CMSIS](https://github.com/ARM-software/CMSIS_6/blob/main/CMSIS/Core/Include/core_cm33.h)