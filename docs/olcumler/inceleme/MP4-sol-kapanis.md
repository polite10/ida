# sol ikinci incelemesi: mp4

## Hüküm

**Daraltılmış kapsamın kapanışını engelleyen kritik veya önemli bulgu saptamadım.** Y5 genel tutarlılık formülüyle çözülmemiş; taşmalı kampanyaların bütünüyle reddedilmesiyle desteklenen kapsamdan çıkarılmış. Kod bu kararı uyguluyor. Y6 ve Y7 düzeltilmiş. Bir küçük belge bulgusu kaldı.

Nihai raporun temel sınırları dürüst: ölçülen yer tutucudur, taşıma dahil değildir, gerçek çekirdekle tekrar gerekir; FNV eşleşmesi bit eşitliği kanıtı sayılmıyor. **D1 ve D2 geçmedi; D3 ve D4 sonuçları korundu.** Çözümleyici kapanışı, performans hedeflerinin sağlandığı anlamına gelmez.

Dosya değiştirmedim, kurulum yapmadım; karta veya seri porta erişmedim.

## İlk incelemedeki maddelerin durumu

| # | Durum | Not |
|---|---|---|
| Y1 | giderildi | Yeni kapsam altında taşmalı kayıt kabul yolu kaldırılmış. Eksik H reddediliyor; taşmasız olağan ve son kutudaki sınır örnekleri kabul ediliyor. Genel taşma tutarlılığı çözülmüş sayılmamalı. |
| Y2 | giderildi | ELF kaynaklı yığın sınırı ve kapsama denetimleri korunmuş. Gerçek O2 kaydında `yigin_tepe=4294967295` bozması yeniden `rc=1` verdi; sayısal özet yok. |
| Y3 | giderildi | Bütün örnekler temizken maksimumların eşitliği denetleniyor. Gerçek O2 A’da `lat_temiz_max=86→59` bozması yeniden reddedildi. |
| Y4 | giderildi | Kampanya hata kararı dışa aktarımdan önce. Son fazdaki taşma da önceki geçerli fazların CSV üretmesine yol açmıyor. Yeni dosya yazımı ve yazma hatasında geri alma yollarını bu turda çalıştırmadım. |
| Y5 | giderildi | **Kapsam daraltmasıyla.** Dört toplamın tamamı `KAPSAM DIŞI`, `rc=1`; sayısal özet yok. Dışa aktarım aşamasına ulaşılmıyor. Geçerli taşmalı iki sınır toplamının da reddedilmesi yeni görevle uyumlu. |
| Y6 | giderildi | Rapor §4.5 tekil çıktı baytlarının bulunmadığını ve bit eşitliğinin doğrulanmadığını açıkça söylüyor. Hash eşitliğinden girdi eşitliği çıkarılamaması birincil kaynakla da uyumlu. [RFC 9923 §1.2](https://datatracker.ietf.org/doc/rfc9923/) |
| Y7 | giderildi | Rapor §1 ve §4.1, 400 Hz ve 200 Hz değerlerini ayrı veriyor. Değerler ham K satırlarıyla eşleşiyor. |

## Yeni bulgular

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem | Kapatılacağı yer |
|---|---|---|---|---|---|
| Y8 | `docs/olcumler/MP4-dongu-yuku.md:30` | Periyot düzeltmesinden sonra `kabul eder.000).` metin artığı kalmış. | `000).` parçasını kaldır. Sayısal sonuç etkilenmiyor. | küçük | Nihai rapor §1 |

## Yeniden hesaplananlar

`analiz_test.py` içindeki dosya oluşturmadan çalıştırılabilen bölümleri, girdileri `/dev/stdin` üzerinden vererek çalıştırdım: **85 denetim geçti, 0 başarısız.** Bunlar Y5’in dört toplamını, kısmi/tam taşmayı, gecikme taşmasını ve aralık sınırlarını içeriyor.

**103 denetimlik komutun tamamını yeniden çalıştırmadım.** Geçici dosya yazan ve sözde uçbirim kullanan testleri çalıştırmadım; yazarın 103 sonucunu bütünüyle bağımsız doğrulanmış saymıyorum.

Y5 için önceki hesabı tekrarladım:

```text
alt = 240999 × 3300000 + 4000000 = 795300700000
üst = 3300000 + 240999 × 4000000 = 963999300000
```

`795300000000`, `964000000000` ve yukarıdaki iki sınır toplamı artık reddediliyor. Aynı örnekleri `--hist` ile de sınadım: `rc=1`, sayısal özet yok, mevcut dokuz histogram değişmedi ve ek dosya oluşmadı. Son C fazındaki adım/gecikme taşmaları da reddedildi.

Aralık hesabı: `200000 × 16 = 3200000` çevrim, yani **20 ms**. `3199999` çevrim destekleniyor; `3200000` çevrim, `tasan=0` bildirilse bile reddediliyor.

Beş geçerli kaydın yeniden analizindeki sayısal satırlar v3 ve v4 ile aynı. **33 v4 histogram CSV’sinin tamamı v3 karşılıklarıyla bayt düzeyinde aynı.** Kesilen Og kaydı `rc=1`; histogramı yok.

| Kayıt | B’den itibaren satır | CRC/sıra hatası | Sonuç |
|---|---:|---:|---|
| O2 400 Hz | 3.539 | 0 | geçerli |
| Og 400 Hz | 3.604 | 0 | geçerli |
| O2 200 Hz | 623 | 0 | geçerli |
| Og 200 Hz | 3.490 | 0 | geçerli |
| Kısa | 302 | 0 | geçerli; kabul kampanyası değil |
| Kesilen Og | 243 | 0 | geçersiz; fazlar ve T eksik |

CRC’yi ayrı bit tabanlı uygulamayla hesapladım. Dört uzun ham kaydın ve ilgili ELF’lerin SHA-256 değerleri bağımsız doğrulama belgeleriyle eşleşti.

| Koşu / faz | Süre alt sınırı, s | p99,9 üst sınırı, µs | Maksimum, µs | Aşım |
|---|---:|---:|---:|---:|
| O2 400 A | 602,49750004375 | 1338,09375 | 1338,91875 | 0 |
| O2 400 B | 821,4287758375 | 3321,59375 | 3322,38125 | 241.000 |
| O2 400 C | 602,4974999625 | 1300,69375 | 1301,73125 | 0 |
| Og 400 A | 602,49750005625 | 2571,79375 | 2572,78125 | 3.013 |
| Og 400 B | 1573,7124755 | 6420,89375 | 6421,69375 | 241.000 |
| Og 400 C | 602,49749984375 | 2306,79375 | 2307,84375 | 0 |
| O2 200 B | 604,99499995625 | 3321,59375 | 3321,96875 | 0 |
| Og 200 B | 788,22253429375 | 6420,69375 | 6421,45000 | 121.000 |

TIM2 ölçümleri: 400 Hz’te O2/kısa **399.998**, Og **400.000**; 200 Hz’te O2 **799.998**, Og **800.001** çevrim. Beklenen değerler sırasıyla **400.000** ve **800.000**.

Yığın sınırları önceki hesaplarla aynı: O2 **1.586.416 B**, Og **1.586.024 B**, O2-200 **1.486.416 B**, Og-200 **1.486.024 B**. `nm` sembol değerlerinin varsayılan onaltılık gösterimi canlı GNU belgesinden doğrulandı. [GNU Binutils `nm`](https://sourceware.org/binutils/docs/binutils/nm.html)

Arşivlenmiş objdump üzerinde D3 yeniden `rc=0`: O2/Og **46/61 işlev**, çift duyarlıklı yordam/komut ve dolaylı çağrı **0**. `kos.sh` sözdizimi denetimi geçti; uçtan uca kart koşusu yapılmadı.

## İskelete ya da diğer bölümlere notlar

- `W0-MP4a-kapsam-daraltma.md`, §2.4’teki kapsam daraltma yolunu izliyor. Taşma desteğinin kaldırılması gerçek ölçümlerin geçerliliğini değiştirmiyor; tamamlanmış kayıtlarda taşma yok.
- Yer tutucu sonuçları kontrol hızını kesinleştirmiyor. Gerçek çekirdek, taşıma dahil süre, aynı imajla uzun tekrar ve görevdeki kesme→adım başı gecikme yüzdelikleri açık kalıyor; bunlar kapanmış gibi sunulmuyor.
- STM32U5A9’un 160 MHz, tek duyarlıklı FPU ve 2,5 MB SRAM özellikleri canlı ST kaynağından doğrulandı. Mutlak kart saati bu incelemede ölçülmedi. [ST ürün belgesi](https://www.st.com/en/microcontrollers-microprocessors/stm32u5a9bj.html)
- Ana görev bu worktree’de bulunmadığından ana depodaki `docs/gorevler/W0-MP4a.md` okundu. İstenen kapsam belgesine, rapora ve iki bağımsız doğrulamaya erişildi.
- GNC ve atış kontrol geliştirmesine yönelik yeni öneri verilmedi; inceleme değişen kapsam ve regresyonlarla sınırlı tutuldu.