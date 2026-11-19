# MP3a — sol ön incelemesine yanıt (düzeltme turu 1)

| | |
|---|---|
| İnceleme | `docs/olcumler/inceleme/MP3-sol-on.md` (8 kritik, 3 önemli) |
| Görev | `docs/gorevler/W0-MP3a-duzeltme-1.md` |
| Uygulayan | `ida-uygulayici` (Opus) |
| Tarih | 2026-11-18 |

On bir bulgunun hepsini haklı buldum ve giderdim; reddettiğim bulgu yok. Bulgu 1'in sonucu ilk teslim raporumdaki baud tablosunun da yanlış olduğunu gösterdi; rapordaki tablo düzeltildi, eski tablo tarihli geçmişte korundu.

Test kanıtı: son derlemede (`derle.sh`, `$IDA_CACHE/mp3/derle-20261118-192703/`) masaüstü testleri **163 denetim, 0 başarısız**. Aynı kaynak UBSan ile de 163/0 geçti; UBSan çalışma zamanı hatası yok. ASan/TSan bu macOS'ta hâlâ başlamıyor. Kart üzerinde kısa ön tarama yapıldı (§12).

## 1. OVER8 BRR geri hesabı (kritik) — giderildi

**Değişiklik:**
- `mp3_brr_baud()`, yazılmış BRR'den ST `LL_USART_GetBaudRate` ile aynı çözümle etkin hızı verir: OVER8'de `D = (BRR & 0xFFF0) | ((BRR & 7) << 1)`; geçersiz BRR (D < 16, OVER8'de BRR[3] = 1) için 0.
- `mp3_baud_hesapla()` her adayı (OVER16 ⌊f/b⌋, ⌈f/b⌉; OVER8'de en yakın iki çift bölücü) önce BRR'ye kodlar, sonra hızı bu işlevle geri çözer; seçim ve ppm bu değerle yapılır.
- Çözümleyici kayıttaki USART saati, BRR ve OVER8'den hızı yeniden hesaplar. Bildirilen hız ve ppm tutmazsa kayıt GECERSIZ olur.

**Sonuç:** 160 MHz'te OVER8 çift bölücüyle OVER16'yla aynı hızları veriyor. Bu yüzden OVER8 yalnız OVER16 bölücüsü 16'nın altına düştüğünde, yani 12 Mbaud'da seçiliyor. Yeni değerler:

| İstenen | Kip | BRR | BRR'den baud | Hata |
|---|---|---|---|---|
| 115 200 | OVER16 | 1389 | 115 191 | −78 ppm |
| 921 600 | OVER16 | 174 | 919 540 | −2 235 ppm |
| 2 000 000 | OVER16 | 80 | 2 000 000 | 0 |
| 3 000 000 | OVER16 | 53 | 3 018 868 | +6 289 ppm |
| 6 000 000 | OVER16 | 27 | 5 925 926 | −12 345 ppm |
| 12 000 000 | OVER8 | 0x15 (D = 26) | 12 307 692 | +25 641 ppm |

Hepsi nominal 160 MHz'ten hesaptır; fiziksel hız ölçülmedi. Kart taramada aynı BRR/OVER8 değerlerini `KUR_YANIT` ile bildirdi, çözümleyici her kayıtta yeniden çözdü.

**Test:** `birim()` baud bloğu. Beklenen değerler el hesabıdır (aynı işlevden üretilmez): altı hız için BRR, OVER8, hız ve ppm; sol'un karşı örneği `mp3_brr_baud(160 MHz, 0x15, OVER8) = 12 307 692`; geçersiz BRR'ler. Kayıt testi: `kart_hesap_baud` ve `pc_hesap_baud` birlikte değiştirilse de BRR geri hesabı nedeniyle GECERSIZ.

## 2. DMA alımında FIFO'daki baytların tüketilmesi (kritik) — giderildi

**Değişiklik:** Konum hesabı saf işleve çıkarıldı (`ortak/mp3_halka.{h,c}`). Kart PRIMASK altında sırayla yarım-tur sayacını, `CBR1.BNDT`'yi ve `CSR.FIFOL`'u okur.
- **alt** = (tur tabanı + kaynak ilerlemesi) − FIFO düzeyi. Hedef RAM'e yazılmış olduğu kesin baytların alt sınırıdır; BNDT, FIFOL'dan önce okunduğu için aradaki ilerleme bu sınırı bozmaz. Değer tekdüzedir. Tüketim yalnız buna göre yapılır.
- **ust** = kaynak ilerlemesi. Üzerine yazma denetimi bununla yapılır: kopyadan önce ve sonra `ust − okunan > N` ise taşma sayılır ve bayt kabul edilmez.
- Global sayımla çalışıldığı için düğüm geçişinde FIFO'da önceki bloktan bayt kalsa da hesap değişmez. Kopyadan önce `__DMB()` var.

Dayanak sol'un gösterdiği ST `stm32-dma3` sürücüsüdür (alımda FIFO düzeyi kalana eklenir); RM0456'yı yerel olarak okumadım.

**Test:** `halka_modeli()`. Model kaynağı (RDR → en çok 8 B FIFO) ve hedefi (FIFO → RAM) ayrı ilerletir; blok sonunda FIFO boşalınca TC ve yeniden yükleme yapar. HT için iki çeşitleme var: hedefe ya da kaynağa göre. Kesme hizmeti rastgele geciktirilir. Okuyucu BNDT ile FIFOL arasında bir DMA adımına izin verir.
- Her çeşitlemede yaklaşık 80 000 okuma.
- `alt` hiçbir zaman RAM'e yazılandan büyük çıkmadı; `ust` her okumada okuma anındaki kaynakla aynı; tekdüzelik bozulmadı.
- Tüketicinin okuduğu her bayt doğru değerdi.
- Uzun gecikme evrelerinde taşma 20 kez yakalandı ve hiçbir yanlış bayt kabul edilmedi.

Kartta: taramadaki 10 koşuda `dma_rx_hata`, `rx_tasma`, UART hata sayaçları ve bütün içerik/CRC/sıra sayaçları kart tarafında 0; alım halkasının azami doluluğu en çok 125/16 384 B (12 M akış).

**Doğrulanmayan:** GPDMA kanal 0 FIFO derinliği ve HT olayının kaynağa mı hedefe mi bağlı olduğu RM'den okunmadı (iki çeşitleme de sınandı). Yarışın kartta ne sıklıkla oluştuğu ölçülmedi.

## 3. Test hedefi derlenmiyordu; betik başarıyla bitebiliyordu (kritik) — giderildi

**Değişiklik:** Kayıt düzenleme yardımcıları (`Satir`, `coz`, `imzala`, `alan_degistir`, `hk`) kullanımlarından önceye taşındı. `derle.sh` artık bütün PC hedeflerini derler ve `mp3_test`'i koşturur; derleme hatası, uyarı ya da başarısız test sıfır dışı çıkış verir. Test sonucu derleme özetine yazılır.

**Test:** Ağacın geçici bir kopyasına `DENETLE(false)` eklenip `derle.sh` çalıştırıldı: 161 denetimde 1 başarısız, **çıkış kodu 1**. Gerçek derleme: çıkış 0, 163/0. Eski `mp3_test` ikilisi ve geçici kopyalar kanıt sayılmadı; bütün sayılar `derle.sh`'nin temiz derlemesinden.

## 4. Geride kalan gönderici geçebiliyordu (kritik) — giderildi

**Değişiklik (mimarın sabitlediği toleranslarla):**
- PC her yazma başlangıcının plandan gecikmesini ölçer (`tx_gec_azami_ns`, yazma başı anında) ve son yazmanın tamamlanmasını kaydeder (`t_son_yaz_ns`).
- Kart akışta son veri çerçevesini gönderim halkasına koyduğu anı (`tx_son_us`, yeni OZET alanı) ve azami takvim gecikmesini bildirir.
- Çözümleyici dört sınırı denetler; herhangi biri aşılırsa sayaçlar eşleşse bile GECMEDI:
  - PC'de yazma başlangıcı ≤ plan + 10 ms;
  - PC'de son yazma ≤ pencere + max(10 ms, pencerenin %1'i);
  - kartta gönderim gecikmesi ≤ 10 ms;
  - kartta son gönderim ≤ pencere + max(10 ms, %1).
- Gecikme 1 s'yi aşarsa gönderim kesilir (`takvim_kesildi`, GECMEDI).
- Takvim alanları ayarlardan yeniden üretilemezse kayıt GECERSIZ olur: aralık profil ve BRR'den yeniden hesaplanır; akışta son plan zamanı `(gönderilen − 1) × aralık` olmalı.
- Rapor PLAN ile GERÇEKLEŞEN'i ayrı satırlarda verir: gerçek gönderim süresi ve ortalama tel hızı.

**Test:**
- Uçtan uca "yavaş gönderici": her yazma 8 ms (aralık ≈ 3,75 ms). Sayaçlar eşleşiyor, tek neden takvim; sonuç GECMEDI.
- Kayıt testleri: beş takvim alanı sınırın üstüne çekildiğinde her biri GECMEDI.

**Kartta:** 12 Mbaud akışta PC yazma gecikmesi 776 968 µs'ye çıktı, son yazma t0 + 10,73 s'de bitti (pencere + pay 10,10 s); bu koşu bu denetimle GECMEDI (§12).

## 5. Echo R zamanları başlık ve özetle eşleşmiyordu (kritik) — giderildi

**Değişiklik:**
- R plan zamanı `sıra × aralık` olmalı.
- Gönderilen isteklerde `plan ≤ yazma başı ≤ yazma sonu`, yazma başı önceki yazma sonundan önce olamaz, yanıt yazma başından önce olamaz.
- R'den hesaplananlar P'yle birebir tutmalı: gönderilen, atlanan ve yanıtlı sayıları; azami takvim gecikmesi; azami yazma süresi; son yazma sonu; son plan; son yanıt.
- Gözlenen bekleyen sayısı kuyruk sınırını aşamaz.

Tutmazsa GECERSIZ.

**Test:** Bütün R zamanlarını 0 yapmak, yalnız son yazma sonunu 1 ns kaydırmak, P'deki azami takvim gecikmesini 1 ns artırmak: üçü de GECERSIZ. Bir R yanıtını silip P'yi tutarlı biçimde düzeltmek: GECMEDI ("zaman aşımı").

## 6. Zorunlu alanlar hükümde doğrulanmıyordu (kritik) — giderildi

**Değişiklik:** Tamamlanmış koşuda şunlar zorunlu (tutmazsa GECERSIZ):
- `kart_kabul = 1`, `kart_neden = 0`;
- `os_tcsetattr = 1`, `os_ioctl = 1`;
- `os_istenen` ve işletim sisteminin `cfgetispeed/cfgetospeed` geri okuması = istenen baud (kartta 921 600–12 M arasında hepsi eşit döndü);
- BRR/OVER8/saatten hız ve ppm;
- seçimin kayıttaki saat ve hızla yeniden üretilmesi.

Sayaç kapsamı açıkça tanımlandı (`mp3_cerceve.h`, `oturum.hpp`):

- **Kart penceresi:** koşuyu başlatan ESLE'den sonra başlar, ilk BITIR dahil biter. `rx_iyi` = veri + hatalı/yinelenen veri + denetim çerçeveleri (yeni `rx_kontrol`, `rx_kontrol_bayt`). `rx_bayt` = ayrıştırıcının penceredeki tükettiği bütün baytlar (yeni `tuketilen` sayacından).
- **PC penceresi:** ilk ESLE_YANIT'tan sonra başlar, ilk OZET dahil biter; sayaçlar OZET anında dondurulur. `rx_okunan_bayt = esle_oncesi_bayt + rx_bayt + ozet_sonrasi_bayt` (okunan baytlar `read()` toplamıdır, ayrıştırıcıdan bağımsız).

Çözümleyici şunları denetler:
- her iki uçta `rx_iyi` eşitliği;
- denetim bayt sayıları (kart: 20 × çerçeve; PC: yinelenen ESLE_YANIT'lar + bir OZET);
- hatasız koşuda pencere baytı = veri × boy + denetim baytı;
- iki ucun veri bayt eşleşmesi (kart alınan veri baytı = PC gönderilen ve tersi; tutmazsa GECMEDI).

**Test:** 16 tek alanlı sahtecilik GECERSIZ: kabul, ioctl, tcsetattr, BRR, OVER8, saat, istenen ve geri okuma hızı, aralık, oran, P/K `rx_iyi`, `rx_bayt`, `rx_kontrol`, `rx_okunan_bayt`. İki uçta birlikte `rx_iyi = rx_bayt = 0` de GECERSIZ. Tutarlı sahte (kart bir çerçeve fazla göndermiş) GECMEDI. Temiz kayıtlar ve bütün temiz uçtan uca koşular GECTI.

## 7. Echo bekleyen sayısı atlanan sıraları içeriyordu (kritik) — giderildi

**Değişiklik:** `BekleyenKuyruk` (`oturum.hpp`):
- Kuyruğa yalnız gerçekten gönderilen istekler (yazma sonu zamanıyla) girer. Yanıtı gelen ya da yazma sonundan beri zaman aşımı süresini geçen istek tek kez çıkar; atlanan sıra kuyruğa girmez.
- Sınıflama çözümleyicininkiyle aynı: zaman aşımından sonra gelen yanıt bir kez "zaman aşımı (geç gelen)" sayılır, ikinci kez sayılmaz.
- Okuyucu yanıtı yalnız gönderilmiş bir isteğe (yazma başı kaydı var) kabul eder; değilse `fazla_yanit` sayar. Yazma başı ve yanıt dizileri atomik.

**Test:** `kuyruk()` sol'un dizisini birebir oynatır: sınır 2; 2 ve 4 atlanır. Eski kodun 3 ve 2 gösterdiği anlarda yeni kuyruk 1 ve 0 gösteriyor. Ayrıca zaman aşımı tek kez çıkıyor ve geç yanıt kuyruğu etkilemiyor.

## 8. Yazma süre sınırı ilerleme dallarında denetlenmiyordu (kritik) — giderildi

**Değişiklik:** `PosixPort::yaz` süre sınırını ilk deneme dışında her yazma denemesinden önce denetler; aşımda `ZamanAsimi` döner ve gerçekten yazılan bayt sayısını korur. Test için `::write` yerine geçen bir kanca eklendi (yalnız `posix_fd_ac`).

**Test:** `yazma_suresi()`: her çağrı 1 bayt yazıp 5 ms sürüyor; 276 bayt 50 ms sınırla gönderiliyor. Sonuç `zaman_asimi`, 8–9 bayt, yaklaşık 50–56 ms.

## 9. %80 sınırı trafik seçeneğiyle değişiyordu (önemli) — giderildi

**Değişiklik:**
- Akış sınırı `AKIS_ORAN_SINIR = 0,80` sabit, echo fiziksel sınırı `ECHO_KAPASITE_SINIR = 1,00`; çözümleyicinin dışarıdan oran parametresi kaldırıldı.
- CLI `--oran` 0,80'i aşarsa kullanım hatası (çıkış 4) verir; `kos()` da reddeder ve kayıt oluşturmaz.
- Profil kayda yazılır (`profil`, `oran_ppm`, `hiz_milli`):
  - `mp3-akis-v1`: 256 B, %80;
  - `mp3-echo-v1`: 64/256 B, 200/400 istek/s;
  - diğerleri `deneysel`.
- Çözümleyici profil adını ve aralığı ayarlardan yeniden üretir; tutmazsa GECERSIZ.
- `--kampanya` ile deneysel profil GECMEDI olur. `kampanya.sh` ve `tara` bu kipi kullanır.

**Test:** %90 oran koşturulmadı, kayıt dosyası oluşmadı. %50 oran GECTI ama profil `deneysel`; `--kampanya` ile GECMEDI. Deneysel kaydın profil adını `mp3-akis-v1` yapmak GECERSIZ. Echo 256 B × 400/s (921 600'de %120) koşturulmadan GECMEDI. `secenek()`: `0.9 > 0.80` reddediliyor.

## 10. Ön tarama süreç hatası başarı koduna dönüşebiliyordu (önemli) — giderildi

**Değişiklik:**
- `kos.sh` aracı boru hattı olmadan çalıştırır (`if araç > dosya; then rc=0; else rc=$?; fi`), çıktıyı sonra basar.
- `tara` çıkış kodu: 0 bütün koşular GECTI; 1 tarama tamamlandı ama en az bir aday GECMEDI/BASLAMADI; 2 en az bir koşu GECERSIZ; 4 kullanım/ortam hatası.
- Tarama mantığı sınanabilir işleve (`tara()`) taşındı.

**Test:** Uçtan uca tarama: iki aday temizken 0; bir aday kartça reddedilince 1 ve tabloda BASLAMADI. Kartta gerçek tarama 12 M akış GECMEDI olduğu için 1 döndü ve `kos.sh` bunu `ortam.txt`'ye "çıkış kodu: 1" olarak kaydetti.

## 11. Sayısal seçenekler daraltıldıktan sonra sınanıyordu (önemli) — giderildi

**Değişiklik:** `pc/secenek.hpp`:
- `tam_sayi` yalnız rakam (ya da onaltılık) kabul eder, `strtoull` + `errno`, tam metin; sınır geniş türde denetlenir, sonra daraltılır.
- `ondalik` yalnız rakam ve nokta kabul eder, `strtod`, sonlu değer ve aralık denetimi yapar (`nan`, `inf`, üslü gösterim ve işaret reddedilir).
- Bütün CLI sayıları bu işlevlerden geçer. Bilinmeyen ya da yinelenen seçenek ve eksik değer kullanım hatasıdır.

**Test:** `secenek()`: `65536` > 256 reddediliyor; `-1`, `12a`, `" 1"`, boş metin, 2⁶⁴, `nan`, `inf`, `1e3`, `1.2.3` reddediliyor; `ff` onaltılık 255 kabul ediliyor.

## 12. Kart doğrulaması (kısa)

1. `yedek-dogrula.sh`: yedek geçerli (SHA-256 `1f13870b…2457`).
2. `onceki-imaj.sh`: kartta MP4 imajı duruyordu, kimlik `20261117-185727-og-200-5fe214ad`, ilk 256 KB SHA-256 `e80fc091…3513` (`$IDA_CACHE/mp3/onceki-imaj-20261118-192258/`).
3. MP3 imajı yüklendi; `KIMLIK` saat 160 MHz, USART saati 160 MHz (`CCIPR1 = 0` → PCLK2), `uart_bekle_hata = 0` bildirdi.
4. Ön tarama (`kos.sh tara`; akış 10 s, echo 5 s; imaj `20261118-192327-mp3-782a472b`; `$IDA_CACHE/mp3/kos/20261118-192402-tara/`):

| Aday | Akış 256 B %80 | Echo 64 B 200/s |
|---|---|---|
| 921 600 | GECTI | GECTI |
| 2 000 000 | GECTI | GECTI |
| 3 000 000 | GECTI | GECTI |
| 6 000 000 | GECTI | GECTI |
| 12 000 000 | **GECMEDI** | GECTI |

12 Mbaud akışta iki ayrı sorun görüldü. Kart PC'nin 34 783 çerçevesini hatasız aldı. Ama:
- kart→PC yönünde bir çerçeve CRC hatasıyla kayboldu;
- PC→kart yazmaları plandan 777 ms'ye kadar geride kaldı.

Tek, kısa koşudur; neden ayrımı yapılmadı.

Son derleme (`20261118-192703-mp3-ca321d0d`) aynı aygıt yazılımı kaynağıyla yalnız kimlik farkıyla yeniden yüklendi ve `KIMLIK` ile doğrulandı. Taramadan sonra yalnız test dosyası değişti; `mp3_vcp` iki derlemede bayt bayt aynı (`8c0fa4b6…`).

## Kalan belirsizlikler

- GPDMA FIFO/HT ayrıntıları RM'den okunmadı (§2).
- Kart taraması tekil ve kısa; 600 s kampanya yapılmadı.
- 12 Mbaud'da BRR hatası +2,56 % (hesap); ST-LINK tarafının gerçek hızı bilinmiyor.
- ASan/TSan bu macOS'ta başlamıyor; yalnız UBSan koştu.
- Windows yazılmadı.
- Takvim toleransları mimar seçimidir.
