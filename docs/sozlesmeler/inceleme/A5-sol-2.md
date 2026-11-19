# sol ikinci incelemesi: A5-senaryo

## Hüküm

**Dondurulmaya uygun değil.** İlk 13 maddenin 9’u giderilmiş, 4’ü kısmen giderilmiş. Altın dosyanın özeti doğru; fakat belgedeki varsayılanlarla yeniden çözümleme aynı sonucu vermiyor.

## İlk incelemedeki maddelerin durumu

| # | Durum | Not |
|---|---|---|
| 1 | kısmen | K109 emniyet atamalarını, K62 bitiş–başlangıç sırasını düzeltiyor. Ancak K63, çelişen koşullu atamalarda sonrakini uyguluyor; verilen “çelişen eşzamanlı atama hatadır” kararına istisna getiriyor. |
| 2 | giderildi | K30 uzunluk eşitliğini doğru dizilerle sınırlıyor. |
| 3 | giderildi | K45 üç zorunluluk koşulunu kapsıyor; örnekte orijin var. |
| 4 | kısmen | Çözümleme sırası ve eksik nesneler tanımlanmış; DVL varsayılanları altın dosyayla uyuşmuyor (Y1). |
| 5 | giderildi | Çözümlenmiş biçimin zorunlulukları ve koleksiyondaki `x_` ayrımı şemada mevcut. |
| 6 | giderildi | Ham kaynak özetleri kanonik nesneden çıkarılmış. |
| 7 | giderildi | K96/K97 dönüşümü ve tür sınırlarını belirliyor; sayısal vektörler tutuyor. |
| 8 | kısmen | Philox ve dönüşüm vektörleri doğru; GE süreçlerinin başlatılması ve sayaçları eksik (Y2). |
| 9 | kısmen | Önceki belirsiz türler tanımlanmış veya reddedilmiş; başlangıçta donma hâlâ tanımsız (Y3). |
| 10 | giderildi | Trim katkısı açık; Ek A hesapları tutuyor. |
| 11 | giderildi | K104/K105 sıfır polinomu ve geçersiz transfer fonksiyonlarını dışlıyor. |
| 12 | giderildi | K79 kartlı kipte istenen dört değeri sınırlıyor ve kanıt kaydı istiyor. |
| 13 | giderildi | AB-089 uygulamaya devredilmiş; K108 zorunlu uyarıyı ve üst veriyi tanımlıyor. |

## Yeni bulgular

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem | Kapatılacağı yer |
|---|---|---|---|---|---|
| Y1 | `docs/sozlesmeler/A5-senaryo.md` §3.6, K16; `scenarios/ornek-havuz.cozumlenmis.json` | DVL’nin dört açı varsayılanı farklıdır. Örneğin belgede `isin_acisi=0.39270`, altın dosyada `0.39269908169872414`. Aynı TOML farklı ışın geometrileri üretir. | Dört varsayılanı tam sayısal değerleriyle tekleştir; altın dosyayı ve özeti eşitle. | kritik | A5 §3.6 ve altın örnek |
| Y2 | Aynı belge §3.10, Ek D.4; `docs/mimari/06-protokol-ve-hil.md` §2.11 | GE başlangıç durumu belirtilmemiştir. Ayrıca emülatörün olay zamanlı GE süreleri için yalnız “çerçeve sırası” sayacı verilmiştir; çerçeveler arasındaki birden çok geçişin çekilişleri tanımsızdır. Başlangıçtaki arıza ve kesinti dizileri değişebilir. | Başlangıç durumunu, geçiş sayacını ve süre/başarı çekilişlerinin ayrı sayaç konumlarını sabitle. | kritik | A5 Ek D |
| Y3 | `docs/sozlesmeler/A5-senaryo.md` Tablo 3.10-A | `zaman_ns=0` ile `donma` veya `donma_zaman_damgasi` kabul ediliyor; fakat tekrarlanacak önceki çıktı henüz yoktur. İlk örneği dondurmak, sıfır üretmek veya kayıt üretmemek arasında seçim bırakılmıştır. | Önceki çıktı yokken davranışı sabitle veya bu başlangıcı reddet. | kritik | A5 arıza sözleşmesi |

## Yeniden hesaplananlar

- Altın JSON **9477 bayt** ve JCS biçiminde. SHA-256 gerçekten `209f758fad29d6fe15a9087cfc2357b50f1b468f8e8254b1df8238e28cebf1d2`. Kanonikleştirme [RFC 8785](https://www.rfc-editor.org/rfc/rfc8785.html) kurallarıyla denetlendi.
- TOML’leri belgedeki varsayılanlarla bağımsız çözümlediğimde yalnız dört DVL açısı farklı çıktı; özet **`0212a8326a7d38f6f11cff1c18dab46dc50b35f474f61b6429ea29ce1d1655d5`** oldu.
- Ek A’nın altı durumu `1e-4` bağıl toleransı sağlıyor. Trimli kütleler **14 / 14,175 / 14,35 kg**; trim katkısı `Ixx/Iyy/Izz = 0,00125/0,00625/0,005`, `Ixz=−0,0025 kg·m²`.
- Altı toplam kütle matrisi Cholesky denetiminden geçti; tahsis matrisi rankı **6**. Basınç hız sınırı **25,974025974 Hz**; transfer fonksiyonu kararlı ve DC kazancı **1**.
- K96 binary64 sınır vektörleri ve Ek D.3’ün beş vektörü yeniden üretildi. Ek D.2’nin üç çıktısı da [Random123’ün yayımlanmış vektörleriyle](https://raw.githubusercontent.com/DEShawResearch/random123/main/tests/kat_vectors) birebir eşleşiyor.

## İskelete ya da diğer bölümlere notlar

Orijin koşulları, mimari varsayım etiketleri, kamera–görünüm bağı ve iki kademede 1200 Ping360 örneği uygulanmış. K63’ün koşullu atama istisnası verilen kararla eşitlenmeli.

İki TOML ve altın JSON, şemada kullanılan anahtarları kapsayan bellek içi yapısal denetimden geçti. `jsonschema` kurulu değildi; kurulum yapılmadı. A4/A7 bütünleşmesi ve Windows çalışması sınanmadı.

T200 uç değerleri [üretici referansıyla](https://docs.bluerobotics.com/downloads/), ESC PWM değerleri [ESC sayfasıyla](https://bluerobotics.com/store/thrusters/speed-controllers/besc30-r3/) uyumlu. TDK Rev 1.6 PDF’si ilk açılışta erişildi; ayrıntılı tablo sorgusu zaman aşımına uğradı, alternatif adres açılamadı. Bu nedenle bütün IMU profil değerleri dış kaynaktan doğrulanmış sayılmadı.