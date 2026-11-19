# sol incelemesi: A8-kamera-algilama

## Hüküm

**Dondurulamaz.** Aşağıdaki kritik belirsizlikler bağımsız uygulamaların farklı kabul, ret ve teslim davranışları üretmesine yol açabilir. Belge ile başlık dosyasının alanları, sabitleri ve yapı boyutları eşleşiyor.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | A8 §3.5 `WELCOME.camera_count`, §3.6 | İskelet §10/25 tek kamera belirlerken sözleşme 1–4 kamera kabul ediyor. Bütçe yalnız tek kamerayı kapsıyor; dört kamerada trafik 48 000 B/s, eşzamanlı tepe 1 600 B olabilir. | Bu sürümde `camera_count = 1` zorunlu olsun; diğer değerlerin ret davranışı tanımlansın. | kritik |
| 2 | A8-K37, K13–K15 | En yeni kare seçildikten sonra, son tarihi henüz gelmemiş daha eski bir kare yeniden aday olabiliyor. Böylece üretim sırası ile karta teslim sırası ters düşüyor; geçerli sonuç sıra veya zaman denetiminde reddediliyor. | Sunulan karelerin kesin artmasını zorunlu kıl; yeni kare seçildiğinde geride bırakılan kareleri kalıcı olarak `SKIPPED_BUSY` yap. | kritik |
| 3 | A8-K14, K51 | Kayıtların `source` ve `inst` alanlarının parti başlığıyla eşitliği zorunlu değil. Kamera partisi içine geçerli bir PING360/RB2 kaydı veya başka kamera örneğinin kaydı konabiliyor. | Kayıtların kaynak, örnek, kalibrasyon ve üretici alanlarının hangi başlık alanlarına eşit olacağını ve uyuşmazlığın sonucunu açıkça tanımla. | kritik |
| 4 | A8-K16, K19, K51; §5 | Hata kapsamı çelişiyor: K16 sıra bozukluğunda partiyi reddediyor, K19 `SYNTHETIC` için tüm partiyi hata yapıyor; K51 bu kurallara uymayan kaydı çıkarıp kalanları iletiyor. Kalibrasyon uyuşmazlığı da §5’te `CALIB_MISMATCH`, K51’de `COMPONENT_ERROR` veya kayıt çıkarma olabiliyor. | Hata başına tek sonuç belirleyen öncelikli doğrulama tablosu yaz; kayıt çıkarma ile bütün partiyi reddetmeyi ayır. | kritik |
| 5 | A8-K05; §8.2 A4/2 | Kartın RB3 kabulü, bileşenin `HELLO` yetenek bitine bağlanmış; bu bilgi karta aktarılmıyor. Kart zorunlu denetimi uygulayamaz. | `HELLO` yetenek denetimini sim’e ait kıl; kartın yalnız aldığı alanlarla sınayabileceği kuralları ayrıca belirt. | kritik |
| 6 | A8 §3.3 `n_rejected`, K51–K52 | Bileşenden gelen `n_rejected` için sıfır koşulu veya sim’de sıfırlama yok. Gelen değer 255 iken bir kayıt çıkarılması u8 sayacını 0’a sarabilir. | Bileşenin gönderdiği değeri 0 ile sınırla; sim sayacı kendi doğrulamasından yeniden hesaplasın. | kritik |
| 7 | A8 §3.5 `CAMERA_CONFIG`, `READY`; K28 | Sıfır/NaN odak uzaklığı, geçersiz montaj kuaterniyonu veya yanlış `config_ver_ack` için zorunlu davranış yok. `ring_mapped = 1` el sıkışmayı tamamlamaya yetiyor. | Yapılandırma alanlarının geçerlilik koşullarını, ACK eşitliğini ve başarısızlıkta kapanış nedenini tanımla. | kritik |
| 8 | A8-K07 | “Cholesky başarılı” kabul ölçütü hesap duyarlılığına ve FMA kullanımına bağlı. Aynı binary32 kovaryans PC’de kabul edilip kartta reddedilebilir; aşağıdaki karşı örnek bunu gösteriyor. | Sayısal doğrulama yönteminin duyarlılığını, yuvarlama/FMA politikasını ve sınır kararını sabitle; ortak kabul/ret örnekleri ver. | kritik |
| 9 | A8 §3.3 `t_meas_ns`, K22, K56 | Boş sonar partisine izin veriliyor; fakat zamanı “kayıtların en büyük zamanı” olarak tanımlı. Boş kümenin maksimumu yok; üretici başlığı dolduramaz. | Boş sonar partisinin zamanını giriş birimine bağlayan açık bir kural tanımla. | kritik |

## Yeniden hesaplananlar

- **Yerleşim:** kayıt 80 B, parti başlığı 32 B; ikisi de hedef ABI’lerde 8 B hizalıdır. Belge sırasıyla kayıt alan başlangıçları: `0, 8, 12, 16, 20, 24, 36, 60, 64, 66, 68, 70, 71, 72, 73, 74, 75, 76, 77, 78`; başlıkta: `0, 8, 12, 16, 18, 20, 22, 23, 24, 25, 26, 27, 28`. Açık ayrılmış alanlar çıkarılınca 78 ve 28 B kalıyor. Hesap [AAPCS32](https://github.com/ARM-software/abi-aa/blob/main/aapcs32/aapcs32.rst), [AAPCS64](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst) ve [Windows x64 ABI](https://learn.microsoft.com/en-us/cpp/build/x64-software-conventions?view=msvc-170) ile uyumlu.
- **Derleme:** Clang ile macOS arm64 C17/C++20, Cortex-M33 C17 ve Windows x64 ABI C17 sözdizimi/düzen denetimleri geçti. Bunlar üretim GNU/MSVC derlemelerinin veya kart çalışmasının doğrulaması değildir.
- **Taşıma:** `3 000 000 / 10 = 300 000 B/s`; 200 Hz’te `1 500 B/periyot`, %60 hedef `900 B/periyot`. İmzasız ayrı mesaj varsayımıyla `40 + 4×90 = 400 B`; 30 Hz’te `12 000 B/s`, ortalama `60 B/periyot`: hedefin %6,67’si. Tepe kapasitenin %26,67’sidir. Sekiz kayıt 760 B, %50,67 eder. MAVLink 2’nin 12 B ek yükü, 255 B yük sınırı, little-endian düzeni ve isteğe bağlı 13 B imzası [birincil kaynaktan doğrulandı](https://mavlink.io/en/guide/serialization.html).
- **A8 soketi:** dört kayıtlı `DETECTION_BATCH`, çerçeve başlığıyla `16 + 40 + 4×80 = 376 B`.
- **Sayaç:** `2³² / (30×4) / 86400 = 414,252 gün`; K13 hesabı doğru.
- **K07 karşı örneği:** B2 alt matrisi için `c_aa=1`, `c_ae=0,10000000149011612`, `c_ee=0,010000000707805157`. Binary64 hesapta ikinci Cholesky pivotu yaklaşık `4,0978193×10⁻¹⁰ > 0`; ayrı binary32 çarpma ve çıkarmada 0. Aynı taşınan değerler farklı karara yol açıyor.

## İskelete ya da diğer bölümlere notlar

- A8 başlığındaki **6/6 bulgu** kapanış tablosunda mevcut. AB-236’nın W5’e devri belirtilmiş. Yer gerçeği sınırı, piksel biçimi anlaşması ve görünürlük sınırı için A8 tarafında kurallar bulunuyor.
- Zamanın pozlama sonu ve süre olarak taşınması, ortanın türetilmesi, sensör çerçevesi ve alım zamanının ayrılması iskelet §3.1 ile uyumlu. Eski bölümlerin farklı zaman adlandırmalarını bulgu saymadım.
- Algılama algoritması, takip veya atış çözümü ayrıntısı bulunmadı. Bileşene açıkça yer gerçeği veren alan yok; nominal montaj ile fizik gerçeği ayrılmış.
- A1, A2 ve A4 sözleşmeleri bu çalışma ağacında bulunmadığından karşılıklı uyumları doğrulanamadı. Özellikle halka bölgesinin yer gerçeği içermemesi A2’de ayrıca doğrulanmalı.
- MP6 rakamları Mac ön ölçümü olarak doğru sınırlandırılmış. `force_draw` ana iş parçacığı koşulu [resmî API belgesiyle](https://docs.godotengine.org/en/stable/classes/class_renderingserver.html#class-renderingserver-method-force-draw) uyumlu. C17 N2176 PDF’i web aracında açılamadı; doğrulanmış kaynak sayılmadı.