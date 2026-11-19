# sol incelemesi: 00-iskelet

## Hüküm

0.8, aşağıdaki üç kritik bulgu giderilmeden sözleşmelere esas alınmamalı. İnceleme yalnız belirtilen 0.8 değişiklikleriyle sınırlıdır.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | §4, bariyer beklemesi | `L_act` aşıldığında çalıştırma yalnız “kaymalı”, 50 ms aşılınca geçersiz sayılıyor. Oysa Ö11, HIL’de gerçek zamanın kaçırıldığı çalıştırmanın geçersiz işaretlenmesini ister. Bu ayrım başarısız zamanlama testini geçerli bırakabilir. | Gerçek zaman sınırı kaçırıldığında geçersizlik kaydedilsin; inceleme amacıyla bekleyerek devam etmek ayrı bir davranış olsun. | kritik |
| 2 | §4, “sonuç tekrarlanabilir kalır”; üç kipin eşitliği | Aynı fizik adımında uygulama, bekleme sırasında duvar saatiyle ilerleyen firmware’in aynı çıktıyı vereceğini garanti etmez. §4’ün kendi saat ayrımıyla ve Ç1’le çelişiyor. Bağlantı zaman aşımını uzatmak diğer emniyet sürelerini sanallaştırmaz. | Güvenceyi kabul edilen komutların uygulama adımlarıyla sınırlayın; firmware’in duvar saatine bağlı davranışları için tekrarlanabilirlik ve kipler arası eşitlik iddiasını kaldırın. | kritik |
| 3 | §4, bekleme sınırından sonraki davranış | “Son geçerli komutla ilerler” hükmü, komutun kullanım süresi dolduğunda ve zaman aşımından sonra eski yanıt geldiğinde ne yapılacağını tanımlamıyor. Sürenin 50 ms’den uzun olması, art arda yanıtsız partilerde bunu çözmez. Bariyersiz HIL’de 10 ms’den geç gelen yanıtın durumu da açık değil. | Süresi dolmuş komutun uygulanamayacağını, süre dolumundaki aktüatör davranışını ve geç yanıtın kabul/ret kuralını tanımlayın. Sabit `L_act` güvencesinin geç yanıt istisnasını açıkça yazın. | kritik |
| 4 | §4 takılma gerekçesi; §8 A1 gidiş-dönüş satırı | RTT uçlarının “bilgisayar kaynaklı” olduğu doğrulanmış gibi sunuluyor. MP3, macOS/USB/ST-LINK etkilerini ayırmadığını söylüyor. “600 saniyede bir iki kez” de RTT sayımıyla örtüşmüyor: 10 ms üzerindeki yanıtlar profillere göre 4/4/4/2. | Nedeni “ayrılmadı” olarak aktarın; RTT uçlarıyla PC yazma takvimi aşımlarını ayırın. Temiz host yinelemesinin açık olduğunu belirtin (`docs/olcumler/MP3-vcp.md`, Uzun kampanya). | önemli |
| 5 | Başlangıçtaki 0.8 özeti; §3 A2 durumu; §8 kamera | “A2 Mac’te ölçüldü” ve “Kesin (Mac)” ifadeleri kanıtı genişletiyor. MP6, GDScript çizim/geri okuma ön turudur; A2 soket/paylaşımlı bellek hattını ve GDExtension’ı ölçmedi. Tam ekran, zamansal etkiler ve uzun koşular da doğrulanmış değil. | Seçilen tasarım yolunu, yalnız üç görünmez pencere durumunda alınan ön kanıttan ayırın; A2 bütünleme ve kalan Mac kabul kapılarını açık bırakın (`docs/olcumler/MP6-godot-kamera.md` §6, §8). | önemli |
| 6 | §6 sonar varsayılanı; §8 sonar ayrıntısı | “20.000–40.000 ışın sığıyor” farklı bütçeleri birleştiriyor. MP7’de orta sahne, 7 m ve `D=2` için 20.000 ışın 9,33 ms sınırına; 40.014 ışın yalnız **tahmini** 25,17 ms bütçesine sığıyor. Sade kademenin yakınsaması da yalnız 7 m’de sınandı. | İki eşiği ayrı verin; 50 m’de yalnız sürenin ölçüldüğünü ve işçilerin dünya adımına etkisinin ölçülmediğini koruyun. Azaltılmış ışın sayısını bütünleşik HIL yeterliliği gibi sunmayın (`docs/olcumler/MP7-sonar-suresi.md` §4.4, §6). | önemli |
| 7 | §10 karar 3; §3.1 zaman damgası | Karar 3, çok yollu profilde kutu zamanının nominal doğrudan yol zamanı olduğunu kabul ediyor; fakat yönlendirdiği §3.1 hâlâ bunu gerçek “yansıma anı” olarak anlatıyor. Kabul edilen istisna gövdeye işlenmemiş. | §3.1’e çok yollu profil istisnasını ekleyin; nominal kutu zamanı ile fiziksel yansıma olayını ayırın. | önemli |

## Yeniden hesaplananlar

- 8N1: bayt başına `1 + 8 + 1 = 10` bit. `3.000.000 / 10 = 300.000 bayt/s/yön`; `300.000 / 200 = 1.500 bayt/periyot`; `%60 = 900 bayt/periyot`. Doğru; bu, protokol ek yükünü de kapsayan nominal hat kapasitesidir.
- `200 Hz → 5 ms`; `5 / 1,25 = 4` fizik adımı. İki kontrol periyodu `10 ms = 8` fizik adımıdır.
- `2,2 + 3,32238125 = 5,52238125 ms > 5 ms`. Toplama doğru; ancak RTT ortancasıyla yer tutucu maksimumunun toplamı, üretim sisteminin en kötü durum kanıtı değildir.
- MP4’ün `-O2` ağır yük değerleri doğru aktarılmış: 400 Hz’te `241.000/241.000` aşım; 200 Hz’te `0/121.000`. İkinci koşunun kontrol `dt`’si hâlâ 2,5 ms varsayımıdır; gerçek 200 Hz çekirdek ölçümü değildir.
- Kamera: `300 × 30 = 9.000` kare. Saat: `16 / 4 × 80 / 2 = 160 MHz`. Aktarılan hesaplar doğru.

## İskelete ya da diğer bölümlere notlar

§10’un 26 satırı, `docs/mimari/acik-bulgular.md` §6’daki numaralarla eşleşiyor. Gövdeye işlenme bakımından yukarıdaki 7. bulgu açık. K17’ye aykırı etkin USB/yedek VCP kararı veya etkin 400 Hz hedefi kalmamış; tarihçe ifadeleri bulgu sayılmadı. 01–09’un eski hükümleri yeniden incelenmedi.

Canlı birincil kaynaklar, [Godot’un ana iş parçacığı şartını](https://docs.godotengine.org/en/4.7/classes/class_renderingserver.html#class-renderingserver-method-force-draw), [ST’nin geçici AHB ÷2 ve 5 µs dizisini](https://community.st.com/stm32-mcus-products-25/rm0456-10-5-4-unclear-restrictions-for-stm32u59x-5ax-5fx-5gx-devices-129223) ve [14.3.rel1’in Newlib bileşenini](https://github.com/STMicroelectronics/gnu-tools-for-stm32/blob/14.3.rel1/README.md) destekliyor. UM2967 PDF tam metni zaman aşımı/boyut sınırı nedeniyle açılamadı; donanım bağlantısı bu tam metinden bağımsız doğrulanmış sayılmadı.

Hiçbir dosya değiştirilmedi.