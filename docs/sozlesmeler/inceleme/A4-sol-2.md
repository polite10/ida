# sol ikinci incelemesi: A4-port

## Hüküm

**Dondurulamaz.** İlk incelemedeki sorunlar giderilmiş; ancak iki kritik ve bir önemli yeni bulgu var. Hata tablosu çakışan koşulları tek sonuca bağlamıyor; çelişkili emniyet örneklerinin işlenmesi de belirsiz.

## İlk incelemedeki maddelerin durumu

| # | Durum | Not |
|---|---|---|
| 1 | giderildi | K45/K104, daraltıcı bulunan çağrıdaki bütün genişleticileri engelliyor; `SESSION_END` dahil. |
| 2 | giderildi | K67 ilk kabulü, K106 taşıma yinelemesini ayrı tanımlıyor. |
| 3 | giderildi | K81, B sınıfı kısıtını commit anında ve aynı çağrıdaki `ARM` için denetliyor. |
| 4 | giderildi | Kimlikle çoklanan 12 sonuç yuvası var; ikinci `PARAM_SAVE` açıkça reddediliyor. |
| 5 | giderildi | K28/K57 ve §4.14, geçersiz zamanın çıktıya taşınmasını ve hatada sürekli komut üretimini engelliyor. |
| 6 | giderildi | K103 ve §4.14: `IMU_REJECTED`, tick davranışı, `nav_valid = 0`, genişletici yok. |
| 7 | giderildi | K37/K108, gecikme mandalının kurulmasını, çözülmesini ve zorunlu tepkisini tanımlıyor. |
| 8 | giderildi | Kullanılabilir çıktı sınırına çağrı süresi eklenmiş: 34,5 ms. |
| 9 | giderildi | K102 tek bekleyen parti ve birleştirme getiriyor; A1 geliş koşulları bağımlılık olarak belirtilmiş. |
| 10 | giderildi | K46, dört daraltıcı türü kuyruktan bağımsız mandallarla taşıyor. |
| 11 | giderildi | K66/K107, üreticide duvar saatini, alıcıda sim zamanını açıkça bağlıyor. |
| 12 | giderildi | K109/K110, kayıtlı dilim sayısını ve zamandan bağımsız iş miktarını tanımlıyor. |
| 13 | giderildi | Eski `background → NOT_INIT` çelişkisi kaldırılmış; parametre işlevlerinin NULL ve hata çıktıları tanımlanmış. |

## Yeni bulgular

Aşağıda **A4**, `docs/sozlesmeler/A4-port.md` anlamındadır.

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem | Kapatılacağı yer |
|---|---|---|---|---|---|
| 1 | A4 K25/K26/K45; §3.11, §4.14 | Aynı çağrıda `DISARM` mandalı + `ARM` + `DUP_SEQ` varsa K45 `NARROWED`, tablo `CALL_REJECTED` istiyor. Ayrıca yinelenen sıra ile yapısal hata birlikteyse `DUP_SEQ`/`BAD_INPUT` önceliği yok. | Birlikte oluşan koşullar için dönüş kodu ve komut ret nedeni önceliğini tekleştir; tabloyu buna uydur. | kritik | A4, W1 |
| 2 | A4 K33/K35/K111 | Aynı `(t_meas, type, inst, seq)` anahtarını taşıyan, biri `estop=0`, diğeri `estop=1` olan iki örnekte sıralama anahtarları eşit. İlkini kabul etmek girdinin sırasına göre daraltmayı değiştiriyor; çelişkili eşitlik için seçim/ret kuralı yok. | Çelişkili eşit anahtarlar için sıradan bağımsız tek politika tanımla; emniyet daraltmasının kaybolmasını önle. | kritik | A4, W1 |
| 3 | A4 K08/K93; §4.15; `core/include/ida/port.h` `ida_core_background` | K08 aralık dışı `now_w` için `BAD_INPUT` istiyor; `background` imzası ve tablosu yalnız `IDA_BG_IDLE/MORE` tanımlıyor. Bu girdide işin ve durumun ne olacağı açık değil. | K08’in işlev kapsamını ve arka plan çağrısının geçersiz saat davranışını mevcut imzayla tutarlı biçimde belirt. | önemli | A4, W1 |

## Yeniden hesaplananlar

- 200 Hz → **5 ms**; dört fizik adımı. `L_act = 10 ms` → **sekiz fizik adımı**.
- MP4 değeri: `3321,96875 / 5000` → **%66,439375**; 3,5 ms bütçeye kalan **178,03125 µs**. Platform kalemleri toplamı **0,675 ms**.
- Dağıtım: `3,5 + 0,5` → **4 ms**. IMU sınırı: `20 + 11 + 3,5` → **34,5 ms**; yalnız tick yolunda **31,5 ms**. Hesap, belgede belirtildiği gibi arada hata dönüşü olmadığını varsayıyor.
- 3 Mbaud 8N1 → **300.000 B/s/yön**, **1500 B/periyot**; %60’ı **900 B/periyot**.
- Sonuç kapasitesi: `8 + 4 = 12`. Parametre kapasitesi: `(8000 − 24) / 8 = 997` kayıt; 8192 B sayfada **192 B** kalır. [ST AN5766](https://www.st.com/content/st_com/en/technical-documents/an5766-migrating-within-stm32u5-series-microcontrollers-stmicroelectronics_asset/chunk-003-topic-an5766-flash-memory.html), 8 KB sayfayı doğruluyor.
- Son olay sayacı aralığı **16 değer** içeriyor. `2⁶² ns ≈ 146,14 yıl`; azami `uint32_t` süre eklenmesi `int64_t` taşırmıyor.
- Bağımsız bit hesabıyla CRC: `"123456789"` → **0xCBF43926**; [CRC RevEng](https://reveng.sourceforge.io/crc-catalogue/17plus.htm#crc.cat.crc-32-iso-hdlc) ile aynı.

## İskelete ya da diğer bölümlere notlar

- Öncelik iskelet **0.8** ve görev belgelerine verildi; eski bölümlerin 400 Hz/USB HS ifadeleri bulgu sayılmadı.
- `port.h`, yeni sonuç, mandal, sağlık ve arka plan alanlarını içeriyor; algılama tipleri istenen `ida_det_record_t` / `ida_det_batch_t` adlarıyla değerle taşınıyor.
- A1/A5/A6/A8 sözleşmeleri ve `core/include/ida/algilama_kaydi.h` bu çalışma ağacında yok; çapraz bütünleşme ve gerçek A8 başlığıyla derleme **doğrulanmadı**.
- [Ping360 birincil belgesi](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/), 400 gradyan, 1200 örnek üst sınırı ve 25 ns birimini doğruluyor. Daha geniş port aralıkları artık açıkça simülasyon tercihi olarak ayrılmış.
- MAVLink önem kodları ve 16 karakterlik parametre alanı, [sabitlenmiş `common.xml`](https://raw.githubusercontent.com/mavlink/mavlink/3fbb88560a06/message_definitions/v1.0/common.xml) üzerinden doğrulandı. WG14 N2176 PDF’i web aracında ayrıştırılamadı; bu kaynak doğrulanmış sayılmadı.
- Atış kontrol incelemesi emniyet zinciri ve arayüzlerle sınırlı tutuldu. Hiçbir dosya değiştirilmedi.