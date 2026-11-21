# sol incelemesi: A4-port

## Hüküm

**Dondurulamaz.** Başlık dosyası derleniyor ve alanlar büyük ölçüde belgeyle örtüşüyor; ancak emniyet önceliği, hata çıktıları, olay yinelemesi ve parametre işlemlerinde bağımsız uygulamaları uyumsuz kılacak boşluklar var. Beş kritik bulgunun tamamının kapandığı hükmü desteklenmiyor.

## Düzeltilmesi gerekenler

Aşağıda **A4**, `docs/sozlesmeler/A4-port.md`; **başlık**, `core/include/ida/port.h` anlamındadır.

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | A4 K45, K64, K73 | Daraltıcıları önce uygulamak, etkilerinin çağrı sonuna kadar korunmasını sağlamıyor: aynı çağrıdaki sonraki `ARM` daraltmayı geri alabilir. Ayrıca daraltıcı komut bulunan çağrıda `SAFETY_OPEN` yasaklanmıyor; K64 bu yasağı yalnız `FIRE` için getiriyor. | Daraltma bulunan çağrının son durumunu ve genişletici komut/olayların reddini bağlayıcı tanımla; `SESSION_END` için de uygula. | kritik |
| 2 | A4 K63, K67 | K63 aynı kimlikle yinelemeyi platforma veriyor; K67 ise iletilen her olayın sayacının önceki iletilenlerden büyük olmasını istiyor. Sözcüğü sözcüğüne uygulamada ikinci iletim kapıdan geçemiyor. | İlk olay kabulü ile taşıma yinelemesini ayır; kimlik denetiminin hangi aşamada çalıştığını açıkça belirt. | kritik |
| 3 | A4 K81; başlık `IDA_PCLS_B` | B sınıfı parametrelerde yalnız `PARAM_SET` sırasında silahlılık denetleniyor. Silahsızken bekletilen değer, araç silahlandırıldıktan sonra `PARAM_COMMIT` ile uygulanabilir. | B sınıfı kısıtını uygulama anında da denetle; aynı çağrıdaki `ARM`/`COMMIT` sırasını sabitle. | kritik |
| 4 | A4 §3.8, K43, K47, K82; başlık `ida_out_t` | Sekiz komut işlenebiliyor ama yalnız bir `param_value` ve bir `param_save` yuvası var. İki `PARAM_GET` veya iki kabul edilmiş `PARAM_SAVE` sonucunun nasıl döneceği tanımsız. | Sonuçları komut kimliğiyle çokla ya da ilgili komutlar için açık kapasite ve erteleme/ret kuralı koy. | kritik |
| 5 | A4 K28, K56, K57 | Geçersiz `tf.t` nedeniyle `BAD_INPUT` dönen çağrıda K57 aynı geçersiz zamanı `out.t_ref` olarak yazdırıyor. Ayrıca taze IMU’lu hatalı tick için K28’in yeniden üretilmiş sürekli komutu ile K56’nın `thr_valid = 0` kuralının önceliği belirsiz. | Her hata dönüşü için geçerli referans zamanı, `thr_valid`, kullanım süresi ve güvenli çıktı önceliğini tek tabloda sabitle. | kritik |
| 6 | A4 K17, K33, K64, K87 | IMU’nun bulunması step için yeterli; örnek reddedildiğinde çağrının nasıl devam edeceği tanımlanmıyor. Bu step yine `OK`, `nav_valid = 1` ve genişletici olay üretebilen çağrı olarak yorumlanabilir. | IMU reddinde hesapların, zaman ilerlemesinin, dönüş kodunun ve çıktı izinlerinin davranışını açıkça tanımla. | kritik |
| 7 | A4 K37, H9; kapanış AB-062 | Gecikmiş IMU için yalnız “bayat sayar” ve `IMU_LAG` bitini “kaldırır” deniyor; bit işlemi ve zorunlu emniyet tepkisi açık değil. H9 davranışı taslak 02 tablosuna bırakıyor. Düzenli gecikmiş akış K39’un alım bayatlığına hiç düşmeyebilir. | Bitin set/clear koşullarını ve gecikme halinde uygulanacak emniyet davranışını sözleşmeye yaz. | kritik |
| 8 | A4 K18, K23, K39; uygunluk K39 | `T_imu_stale + 11 ms` çıktı sınırı çağrının çalışma süresini içermiyor. Platform çıktıyı ancak çağrı döndüğünde kullanabilir. | Sınırı çağrı başlangıcına mı kullanılabilir çıktıya mı koyduğunu belirt; hesap ve kabul testini buna göre düzelt. | kritik |
| 9 | A4 K16, K19, K20, K23 | Her kapanan partiye ≤0,5 ms’de başlama güvencesi, yeniden girilemeyen 3,5 ms’lik step sırasında başka partinin kapanması durumunu karşılamıyor. A1’den bu durumu dışlayan bir geliş/burst sınırı istenmemiş. | Geliş varsayımlarını A1 ile bağla; dağıtım sınırını kesilemeyen çağrılar ve birikmiş partiler dahil hesapla. | kritik |
| 10 | A4 K46; uygunluk K32/K46; kapanış AB-078 | “Hiçbir doluluk durumunda daraltıcı atılmaz” güvencesinin sonlu tamponla nasıl sağlandığı yok. Sekizden fazla daraltıcı geldiğinde erteleme kapasitesi, doluluk davranışı ve gecikme sınırı tanımsız. | Trafik sınırı, geri basınç veya anlamı koruyan birleştirme politikasını ve azami gecikmeyi sabitle; taşma testine yalnız daraltıcılardan oluşan yük ekle. | kritik |
| 11 | A4 K66, §3.8 `hold_ns` | Süreli iznin `hold_ns` süresinin hangi saat alanında ilerlediği belirtilmiyor. Bariyer beklerken sim zamanı durur, kartın duvar saati ilerler; iki yorum farklı emniyet davranışı üretir. | İzin süresinin saatini, duraklama/bariyer davranışını ve A1 tarafının sorumluluğunu iskelet §4 ile uyumlu bağla. | kritik |
| 12 | A4 K24, K94, K95; §10 A6 beklentileri | HIL’de yetişmeyen arka plan işi düşebilir; tekrar oynatıcı ise her step öncesi işi bitiriyor. Kayıtlı düşürme kararını çekirdeğe uygulatacak girdi/API yok; yalnız `bg.dropped` kaydı sonucu yeniden üretmeye yetmiyor. | Tamamlanma/düşürülme düzenini yeniden oynatılabilir girdiye dönüştür veya arka plan çağrı izinin kaydı ve yeniden yürütülmesini sözleşmeye bağla. | kritik |
| 13 | A4 §3.11, K85, K93, K101; başlık 839–848 | §3.11, `background` için `IDA_RC_NOT_INIT` tanımlıyor; işlev yalnız `IDA_BG_IDLE/MORE` döndürebiliyor. Parametre işlevlerinin çıktı işaretçileri için NULL ve hata halinde çıktı kuralları da eksik. | İşlev bazında dönüş/hata tablosunu imzalarla eşitle; bütün işaretçi parametrelerinin önkoşullarını belirt. | önemli |

## Yeniden hesaplananlar

- **Zaman:** 200 Hz → 5 ms; dört adet 1,25 ms fizik adımı. İki periyotluk `L_act` → 10 ms, sekiz fizik adımı.
- **MP4 bütçesi:** 3321,96875 µs / 5000 µs = **%66,439375**. 3,5 ms bütçeye kalan pay **178,03125 µs**; platforma ayrılan toplam pay **1,5 ms**. A4’te sayılan platform kalemleri toplamı **0,675 ms**. Bunlar gerçek çekirdek için süre kanıtı değildir; tahmin etiketi doğru.
- **IMU kesilmesi:** K18 çağrı başlangıçlarını sınırlıyorsa kullanılabilir çıktı sınırı tick yolunda **31,5 ms**, step yolunda **34,5 ms** olabilir; yazılan **31 ms** kanıtlanmıyor.
- **Taşıma:** 3 Mbaud, 8N1 → yön başına 300 000 B/s → 200 Hz’te 1500 B/periyot; %60 → **900 B/periyot**. MP3 gecikme ölçümleri dağıtım için 0,5 ms güvencesi sağlamıyor.
- **Boyutlar:** 8000 B blokta en çok **997** parametre kaydı bulunur; 8192 B sayfada **192 B** ek yer kalır. Son 16 olay sayacı değeri hesabı doğru. ST, ilgili aile için [8 KB flash sayfalarını](https://www.st.com/content/st_com/en/technical-documents/an5766-migrating-within-stm32u5-series-microcontrollers-stmicroelectronics_asset/chunk-003-topic-an5766-flash-memory.html) doğruluyor.
- **Zaman üst sınırı:** 2⁶² ns ≈ **146,14 yıl**; buna azami `uint32_t` süre eklemek `int64_t` taşırmıyor.
- **CRC:** bağımsız bit hesabıyla `"123456789"` → **0xCBF43926**; [CRC RevEng kaydıyla](https://reveng.sourceforge.io/crc-catalogue/17plus.htm#crc.cat.crc-32-iso-hdlc) aynı.
- **Derleme:** başlık, sistem Clang ile C17/C++20 ve ARM GCC ile C17 kiplerinde `-pedantic-errors -Wall -Wextra -fsyntax-only` denetimlerinden uyarısız geçti. Bu, davranış uygunluğu testi değildir.

## İskelete ya da diğer bölümlere notlar

- A4 başlığındaki **AB-059–AB-084’ün 26’sı da** kapanış tablosunda; **21 kapandı / 5 devredildi** sayımı doğru. AB-059’un sağlık ayrımı kural düzeyinde karşılanmış. AB-060 ve AB-064 için yukarıdaki öncelik/hata çıktısı sorunları; AB-062 için gecikme tepkisi; AB-078 için taşma politikası giderilmeden toplu kapanış hükmü korunmamalı.
- Eski bölümlerdeki 400 Hz ve USB HS ifadeleri bulgu sayılmadı. İnceleme iskelet **0.8** ve W1 görev önceliğiyle yapıldı.
- [Ping360 birincil belgesi](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/) 400 gradyan, 1200 örnek üst sınırı ve 25 ns birimini doğruluyor. A4’ün daha geniş kabul aralıklarının cihaz sınırı mı, simülasyon tercihi mi olduğu ayrıca belirtilmeli.
- MAVLink önem kodları ve 16 karakterlik parametre adı iddiası, belgede verilen [sabitlenmiş `common.xml`](https://raw.githubusercontent.com/mavlink/mavlink/3fbb88560a06/message_definitions/v1.0/common.xml) üzerinden doğrulandı.
- C17 N2176 PDF’i web aracında ayrıştırılamadı; bu kaynağa erişim doğrulanmış sayılmadı. A1/A5/A6/A8’in ileride sağlayacağı kurallar da mevcut ve tamamlanmış sözleşmeler gibi kabul edilmedi.
- Hedefte kapsam sınırını aşan hedefleme veya atış çözümü ayrıntısı görülmedi. Hiçbir dosya değiştirilmedi.