# sol incelemesi: mp6

## Hüküm

**Bu haliyle kabul edilmemeli: 2 kritik, 4 önemli bulgu var.** Görünür pencere için beş dakikalık kabul kanıtı eksik; ayrıca asenkron okumanın p99 değeri rapor içinde çelişiyor.

Kare kimliğinin geri okunan piksellerden çözülmesi ve istek kimliğiyle karşılaştırılması doğru kurulmuş. Godot 4.7.2 kaynak zinciri de kullanılan eşzamanlı `force_draw` yolunu destekliyor. Ancak bunlar pencere durumuna ve ölçüm yorumlarına ilişkin eksikleri kapatmıyor.

Hedefteki bütün dosyalar ve ölçüm raporu okundu; `.godot/` dışlandı. Ham veri incelenmedi, GPU koşusu yapılmadı ve hiçbir dosya değiştirilmedi. Sayısal gözlemlerin doğruluğu, raporun kendi iç tutarlılığıyla sınırlı olarak denetlendi.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `docs/olcumler/MP6-godot-kamera.md:112,180,182` | **G1 yeterli kanıt olmadan geçti işaretlenmiş.** Raporun kendi görünürlük ölçütüne göre `b_gorunur` koşusunda 11 saniye doğrulanmamış; durum yalnız 288/299 kayıt aralığında tutulmuş. Diğer görünür koşu geçersiz, okuma koşuları ise yalnız 120 saniye. Dolayısıyla kesintisiz beş dakikalık görünür koşu gösterilmiyor. Aynı sorun G3’ün “görünür referans” nitelemesini de etkiliyor. | Açıklanamayan aralığı çözmeden G1’i geçti sayma. En az bir yolu görünürlük doğrulamasıyla kesintisiz 300 saniye yinele; geçerli referans üzerinden G3 karşılaştırmasını yenile. Mevcut eşitlik sonuçları korunabilir fakat bütün referansın görünür koşulda üretildiği iddiası koşullandırılmalı. | kritik |
| 2 | `docs/olcumler/MP6-godot-kamera.md:130,141` | `okuma_1280x720_async` için metin p99’u yaklaşık **68 ms** veriyor; tabloda p99 **58,86 ms**, maksimum **68,31 ms**. Maksimum değer p99 olarak aktarılmış görünüyor. | Ham özette hangi değerin doğru olduğunu denetle; metin ve tabloyu aynı sonuçtan üret. Tablodaki değerler doğruysa p99 58,86 ms, maksimum 68,31 ms olmalı. | kritik |
| 3 | `tools/olcum/mp6/proje/olcum.gd:419–441`; `tools/olcum/mp6/kampanya.sh:37`; rapor §2 ve §4.4 | **İkinci okuma GPU beklemesini dışlamıyor.** Aynı dokunun yeniden okunması da yeni GPU kopyası, gönderim ve senkronizasyon içeriyor. Kaynakta her eşzamanlı okumada ara tampon oluşturulup `_flush_and_stall_for_all_frames()` çağrılıyor. Ayrıca `--cift=1` koşularında `t_ready_us`, ikinci okumadan sonra alınıyor. | İkinci süreyi “yeni sahne çizimi olmadan tekrarlanan eşzamanlı geri okuma” diye adlandır. Saf aktarım süresi veya GPU beklemesiz süre sayma. İlk okumanın ne kadarının çizim beklemesi olduğu ayrıca ölçülmedi; “büyük kısmı” ifadesini çıkar veya çıkarım olarak işaretle. İlk verinin hazır zamanını ikinci okumadan önce kaydet. [Godot 4.7.2 geri okuma kaynağı](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/servers/rendering/rendering_device.cpp) | önemli |
| 4 | `tools/olcum/mp6/analiz.py:225–231`; `docs/olcumler/MP6-godot-kamera.md:63,103` | Rapor bütün karşılaştırmaların `b_gorunur` referansıyla yapıldığını söylüyor; betik `a_*` koşularını **`a_gorunur`** ile karşılaştırıyor. Bu nedenle `a_tamekran` için 8805 üretilen kareden yalnız 2930’u karşılaştırılmış. Bu, bildirilen ortak referans yöntemi değil. | Referansı açıkça seçilebilir yap veya raporda koşu başına gerçek referansı belirt. Karşılaştırılmayan kare sayısını göster; 2930/2930 eşitliği 8805 karenin tamamına ilişkin kanıt gibi sunma. | önemli |
| 5 | `tools/olcum/mp6/proje/olcum.gd:317–329`; `tools/olcum/mp6/analiz.py:35–69,159–164`; rapor §4.1, §6 | **Tam ekran koşulunun sürdüğüne ilişkin sunulan kanıt eksik.** `frames_drawn`’ın durması ve ölçüm penceresinin ekran listesinde bulunmaması, başka uygulamanın tam ekranda kalmasını tek başına doğrulamaz. Örtü sürecinin kipi yalnız başlangıçta bir kez yazılıyor; çözümleme örtünün tam ekran olarak sürmesini denetlemiyor. Buna rağmen §6, dört durumda olağan döngünün kare üretmediği sonucuna varıyor; aynı rapor `a_tamekran` koşusunu geçersiz sayıyor. | Tam ekran örtünün kimliğini, etkinliğini ve durumunu koşu boyunca doğrula; mevcut dış kayıtlarda yeterli kanıt varsa onu açıkça raporla. Aksi halde bu koşulu doğrulanmadı say. Yol (a) için beş dakikalık tam ekran sonucunu, geçersiz koşudan veya önceki yaklaşık 100 saniyeden genelleme. [Apple pencere listesi tanımı](https://developer.apple.com/documentation/coregraphics/cgwindowlistoption/optiononscreenonly.md) | önemli |
| 6 | `tools/olcum/mp6/analiz.py:114–116,136–137,239–245`; rapor G3 | Konum hatası hesabı **küre bulunamayan kareleri dışlıyor**. Sayı JSON’a yazılıyor fakat gösterilen tabloda ve G3 dayanağında bulunmuyor. Dolayısıyla 0,309 px maksimum, bütün karelerin konum şartını sağladığını tek başına kanıtlamıyor. Bu, kürenin gerçekten kaybolduğu iddiası değil; kabul kanıtındaki boşluk. | Her G3 koşusu ve referansı için bulunan/bulunamayan kare sayılarını raporla. G3’ün geçmesi için bütün gerekli karelerde konumun bulunmasını ve hatanın ≤ 1 px olmasını zorunlu tut; bulunamayan kareyi istatistik dışında bırakıp kabul etme. | önemli |

## Yeniden hesaplananlar

| Hesap | Yeniden sonuç | Değerlendirme |
|---|---:|---|
| 30 Hz × 300 s | 9000 istek | Doğru |
| 30 Hz × 120 s / 20 s | 3600 / 600 istek | Doğru |
| `a_gorunur`: 5875 / 9000 × 100 | %65,2778 → %65,28 | Doğru |
| `a_tamekran`: 195 / 9000 × 100 | %2,1667 → %2,17 | Doğru |
| Görünür tutulan bölüm: (3284 − 3124) / 3284 × 100 | %4,8721 → %4,9 | Doğru |
| Asenkron koşu: (3600 − 3598) / 3600 × 100 | %0,05556 | Koşu sonunda teslim edilmeyen oran; iki isteğin gerçekten kaybolduğunu kanıtlamaz |
| 640 × 480 RGB8 / RGBA8 | 921.600 / 1.228.800 B | Doğru |
| 1280 × 720 RGB8 / RGBA8 | 2.764.800 / 3.686.400 B | Hesaplanan boyutlar |
| 640 × 480, 30 Hz RGB8 / RGBA8 | 27,648 / 36,864 MB/s | Ondalık MB; ek maliyetler hariç |
| Kuyruk 2 kare, 30 çizim/s | 66,667 ms | Rapordaki 66,7 ms hesabı doğru |
| Kuyruk 2 kare, 120 çizim/s | 16,667 ms | Düzenli çizim varsayımıyla yaklaşık hesap |
| Varsayılan kampanya | 10 durum + 6 okuma + 1 gizli asenkron + 2 başsız = 19 koşu | Doğru |

`durum_tutuldu.saniye`, gerçek süre toplamı değil, uygun bulunan kayıt aralıklarının sayısıdır (`analiz.py:163–164`). Bu nedenle 299/299 ifadesi doğrudan 299 saniye ölçüldüğü şeklinde okunmamalı; süre, zaman damgası farklarından hesaplanmalı.

Kuyruk hesabı tam gecikme tahmini değildir. İsteğin çizim çevrimine göre konumu ve ek çizimler sonucu etkiler. Godot kaynağında `swap_buffers(false)` da kare halkasını ilerletir; asenkron geri çağrılar ilgili kare beklenirken çalıştırılır. Görünür koşuda 120 Hz olağan çizime 30 Hz elle çizim de eklenir. [Godot 4.7.2 kuyruk uygulaması](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/servers/rendering/rendering_device.cpp)

Rapordaki 0,309 px < 1 px, gölge oranı 0,49 < 0,75 ve MAD 0 ≤ 2/255 karşılaştırmaları aritmetik olarak doğru. Ham kareler olmadan bu ölçülmüş değerler, MD5 eşitlikleri ve yüzdelikler bağımsız yeniden üretilemedi.

## İskelete ya da diğer bölümlere notlar

- **Çizim yolu kaynakla destekleniyor.** `force_draw`, doğrudan `RenderingServer::draw`’a bağlı; ayrı render iş parçacığı yokken `_draw` doğrudan yürütülüyor. Bu yol `Main::iteration` görünürlük kapısına bağlı değil. [API bağlantısı](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/servers/rendering/rendering_server.cpp), [çizim uygulaması](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/servers/rendering/rendering_server_default.cpp)

- **`UPDATE_ONCE` gerçekten çizim seçimine giriyor.** Viewport görünür kabul edilip çiziliyor ve ardından `DISABLED` durumuna dönüyor. `false`, ekran sunumunu kapatıyor; kamera dokusunun çizimini kapatmıyor. [Viewport kaynağı](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/servers/rendering/renderer_viewport.cpp), [RD kare sonu](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/servers/rendering/renderer_rd/renderer_compositor_rd.cpp)

- **Kare kimliği denetiminde belirtilen yanlış pozitif yolu bulunmadı.** Kimlik, `veri` içindeki üst ve tümleyen alt şeritten çözülüyor; sayaçtan kopyalanmıyor. Bayat görüntü yeni istek kimliğiyle uyuşmazsa üretilen kare sayılmıyor. Bu, tek başına 3B içeriğin tazeliğini kanıtlamaz; konum ve referans karşılaştırması da gerekli.

- **Kopya açıklaması belirtilen düzeyde uyumlu.** Eşzamanlı RD yolunda ara tampondan çıktı vektörüne CPU kopyası var. Saydam olmayan hedefte RGBA8 → RGB8 dönüşümü ayrıca yapılıyor. Sürücü içindeki olası ek kopyalar ve paylaşımlı belleğe teslim bu prototipte ölçülmedi. [Doku biçimi ve dönüşüm kaynağı](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/servers/rendering/renderer_rd/storage_rd/texture_storage.cpp)

- **Saat kaynağı doğru belirtilmiş.** Godot zaman damgaları macOS’ta başlangıca göre `mach_absolute_time` kullanıyor. Dış gözlemdeki `Date.now()` ve kabuk `date` kayıtları ise takvim saatidir; doğrudan aynı saat alanı değildir. [Godot saat kaynağı](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/drivers/unix/os_unix.cpp)

- **Ö5-k, M28 ve M30 kapanmış sayılmamalı.** Mac’te kısa süreli olumlu kanıt mevcut; Windows, uzun koşular, `thread_model=2`, zamansal etkiler ve su altı optiği ayrı kalıyor. Rapor bunların çoğunu doğru biçimde açık bırakmış. Ayrıca prototip, 07 §2.4’teki operatör zamanı ile kamera zamanı arasında gidip gelme düzenini sınamıyor; bu sınır da doğrulanmayanlara eklenmeli.

- **Sürüm ve erişim sınırı:** Godot 4.7.2’nin resmî sürüm kaydı, commit kısa kimliği ve rapordaki macOS ZIP SHA-256 değeri canlı birincil kaynakla uyuşuyor. Çalıştırılan yerel ikilinin özeti yeniden alınmadı. `SHA512-SUMS.txt` indirilebilir içeriğine erişilemedi; SHA-512 eşleşmesi bağımsız doğrulanmış sayılmadı. [Resmî sürüm notu](https://godotengine.org/article/maintenance-release-godot-4-7-2/), [resmî varlık özetleri](https://github.com/godotengine/godot-builds/releases/expanded_assets/4.7.2-stable)