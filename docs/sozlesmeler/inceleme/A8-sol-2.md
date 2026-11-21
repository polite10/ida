# sol ikinci incelemesi: A8-kamera-algilama

## Hüküm

**Dondurulamaz.** İlk incelemenin sekiz maddesi giderilmiş; 4. madde kısmen giderilmiş. Doğrulama ve durum seçimindeki aşağıdaki çelişkiler bağımsız uygulamaların farklı sonuç üretmesine yol açar. Belgeyle başlık dosyasının alanları ve sabitleri eşleşiyor.

## İlk incelemedeki maddelerin durumu

| # | Durum | Not |
|---|---|---|
| 1 | giderildi | K35 tek kamerayı zorunlu kılıyor; diğer değerlerde kapanış tanımlı. |
| 2 | giderildi | K46 eski karenin yeniden sunulmasını engelliyor; durum seçimine ilişkin yeni bulgu aşağıda. |
| 3 | giderildi | K19 alan eşitliklerini ve ret nedenlerini belirliyor. |
| 4 | kısmen | Bütün parti reddi tanımlı; K28 ve protokol kuralları hâlâ farklı sonuçlara izin veriyor. |
| 5 | giderildi | Yetenek denetimi yalnız sim’de; kart denetimleri ayrı. |
| 6 | giderildi | `n_rejected` kaldırılmış; sayaçlar parti dışında. |
| 7 | giderildi | K36–K37 yapılandırma, ACK ve kapanış koşullarını tanımlıyor. |
| 8 | giderildi | Taşınan Cholesky çarpanına yalnız karşılaştırma uygulanıyor; karşı örnekler yeniden doğrulandı. |
| 9 | giderildi | K11/K66 boş sonar partisinin zamanını tarama satırının gönderim anına bağlıyor. |

## Yeni bulgular

Yerler `docs/sozlesmeler/A8-kamera-algilama.md` içindedir.

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem | Kapatılacağı yer |
|---|---|---|---|---|---|
| 1 | K27–K28 | `FRAME_INVALID`, `count=0`, yanlış `calib_ver` içeren ve STRUCT/ECHO’yu geçen yanıt: tablo `CALIB_MISMATCH`, K28’in son cümlesi `FRAME_INVALID` gönderilmesini emrediyor. | Bileşenin durumu ancak bütün uygulanabilir denetimler geçince korunsun. | kritik | A8 K28 |
| 2 | §3.3; K27 `TIME` | Başlık zaman kısıtları tabloya eksiksiz bağlanmamış. Örneğin boş sonar partisinde `window_ns=1`, §3.3/K11’e aykırı olduğu halde K12/K13/K17 denetimlerinden geçiyor. | Başlığın negatif zaman, pencere üst sınırı, sonarda sıfır pencere ve K64 koşullarını `TIME` satırına açıkça ekle. | kritik | A8 K27 |
| 3 | K46; K58/K63 | K46, `M_k`’ya kadar sunulmayan her kareye `SKIPPED_BUSY` veriyor; kare hazır fakat bileşen yoksa K63 `COMPONENT_DOWN` veriyor. “Bütün eski sunulmamış kareler” ifadesi kapanmış partileri de kapsıyor. | K46’yı henüz kapanmamış karelerle sınırla; nihai durum yalnız K63 önceliğiyle seçilsin. | kritik | A8 K46 |
| 4 | K27 `STRUCT`; K39/K42 | `count=0`, yük uzunluğu 41 olan `DETECTION_BATCH`: STRUCT parti reddi, K42 tür boyutu aşımında bağlantı kapatma öngörüyor. K39’un fazla yükü yok sayması da ayrıştırılmamış. | Çerçeve hatası, parti uzunluk hatası ve minor uzantısı için kapsam ve önceliği açıkça ayır. | kritik | A8 K27/K39/K42 |

## Yeniden hesaplananlar

- **Yerleşim:** `core/include/ida/algilama_kaydi.h` alan alan eşleşiyor. Kayıt başlangıçları `0,8,12,16,20,24,36,60,64,66,68,70,71,72,73,74,75,76,77,78`; başlık başlangıçları `0,8,12,16,18,20,22,23,24,25,26,27`. Boyutlar **80/32 B**, hizalama **8 B**, ayrılmış alanlar çıkarılınca **78/27 B**. [Arm ABI](https://github.com/ARM-software/abi-aa/blob/main/aapcs32/aapcs32.rst) ve [Windows ABI](https://learn.microsoft.com/en-us/cpp/build/x64-software-conventions?view=msvc-170) kurallarıyla uyumlu.
- **Derleme:** Clang ile macOS arm64 C17/C++20, Cortex-M33 ve Windows x64 hedefli C17 sözdizimi ve mevcut yerleşim denetimleri geçti. GNU/MSVC üretim derlemeleri ve kart çalışması sınanmadı.
- **Cholesky:** sıra, birimler ve ölçülmeyen satır/sütunların sıfırlanması tutarlı. Binary32 alt sınırlar `0x3a83126f` ve `0x358637bd`; eşitlik kabul, bir önceki değer ret. Üst sınırlar 1000 ve 10 dahil; sonraki binary32 değerler ret.
- **Karşı örnek:** pivot binary64’te `4,097819306103645·10⁻¹⁰`, ayrı binary32 işlemlerde 0. Taşınan `L_ee=2,0243071048753336·10⁻⁵`; yeniden kurulan `c_ee`, ayrı işlemlerle ve FMA ile `0,010000000707805157`. E1–E6 kararları sırasıyla **kabul, ret, kabul, ret, ret, ret**. Yazarın gösterim değişikliğine ilişkin gerekçesi doğru.
- **Taşıma:** `399 B/parti`, `11 970 B/s`, `59,85 B/periyot`, bütçenin `%6,65`’i; tepe kapasitenin `%26,6`’sı. Sekiz kayıt `759 B`; A8 soketinde dört kayıtlı mesaj `376 B`. Sayaç süresi `414,252 gün`. MAVLink ek yükü ve sınırları [birincil kaynakla doğrulandı](https://mavlink.io/en/guide/serialization.html).

## İskelete ya da diğer bölümlere notlar

- Tek kamera, sensör çerçevesi, zaman ayrımı ve yer gerçeği sınırı iskeletle uyumlu. SIL’de beklememe ve kademelerin A2’ye bırakılması verilen kararlar doğrultusunda uygulanmış.
- A1, A2 ve A4 sözleşmeleri çalışma ağacında bulunmuyor; karşılıklı uyumları doğrulanamadı.
- `force_draw` için ana iş parçacığı koşulu [resmî belgede doğrulandı](https://docs.godotengine.org/en/stable/classes/class_renderingserver.html#class-renderingserver-method-force-draw). Windows görünürlük sınırı ölçülmüş gibi sunulmuyor.
- Atış kontrol kapsam sınırının dışına çıkan içerik görülmedi.