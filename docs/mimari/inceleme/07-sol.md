# sol incelemesi: 07-gorsellestirme

## Hüküm

Bölüm bu haliyle kabul edilemez. Sekiz başlıklı yapı, ayrı fizik süreci, kamera algılamasının PC’de konumlandırılması ve araştırma düzeltmelerinin çoğu uygundur. Ancak dalga fazı komşu bölümle uyuşmuyor; iki zamanlı çizimin dönüşüm ve geri okuma sırası eksik; paylaşımlı bellek halkası çoklu okuyucu, çökme ve yeniden bağlanma için güvenli bir protokol tanımlamıyor. Kamera teslim bütçesi de uçtan uca kapanmamış. İskelet 0.6’ya geçişte A8 ve türetilmiş ağ kuralları güncellenmelidir. İnceleme okumaya, bağımsız aritmetiğe ve resmi kaynaklara dayanır; dosya değiştirilmedi, kurulum veya çalışma zamanı deneyi yapılmadı.

## Düzeltilmesi gerekenler

| # | Yer (başlık ya da satır) | Sorun | Doğrusu / önerilen düzeltme | Önem (kritik / önemli / küçük) |
|---|---|---|---|---|
| 1 | §2.7, 182–190; B04 §2.4 | **Dalga fazı aynı değil.** B04 `cos(ωt−kx+φ)`, bu bölüm `cos(kx−ωt+φ)` kullanıyor. Aynı `φ` için bunlar genel olarak eşit değildir. “B04 konvansiyonuna göre” notu somut denklemi düzeltmiyor. | B04’ün denklemini aynen kullan veya dönüşümde `φ_görsel=−φ_fizik` tanımla. CPU’daki faz indirgemesi de aynı işaretle yapılsın. Tek bileşenli, sıfırdan farklı fazlı test eklenmeli. | kritik |
| 2 | §2.4, 91–97; §3.6 | Sahne düğümlerine poz yazılması, pozun elle çizimden önce RenderingServer’a ulaştığını garanti etmiyor. Godot dönüşüm bildirimlerini biriktirir; aynı çağrı içinde sensör durumu uygulanıp geri alınırsa ara durum çizime hiç ulaşmayabilir. | Kamera geçişinden önce değişen gövdeler, kamera ve ışıkların dönüşüm bildirimlerinin işlendiği açık bir aşama tanımla; gerekirse ilgili düğümlerde `force_update_transform` kullan. Operatör durumuna dönüş için de aynı sınır gerekli. [4.7.2 Node3D belgesi](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/doc/classes/Node3D.xml), [SceneTree uygulaması](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/scene/main/scene_tree.cpp). | kritik |
| 3 | §2.4–2.5; §8, 481 | `force_draw` sonrasında “geri okuma kuyruğa alınır” demek, doğru kareyi okumayı henüz tanımlamıyor. İş parçacıklı kipte `draw` yalnız çizim komutunu kuyruğa bırakır. Yanlış yerleştirilen geri okuma önceki dokuyu okuyabilir; damgası yine yeni `t_meas` olur. | Dönüşümleri aktar → sensör çizimini sırala → aynı render kuyruğunda o çizimin doku okumasını sırala → operatör durumunu uygula düzenini somutlaştır. Geri çağrı değişmez istek kimliği, nesil ve üst veri taşımalı. GPU sonucunu ana iş parçacığında beklemek zorunlu değildir; komut sırası zorunludur. [4.7.2 çizim uygulaması](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/servers/rendering/rendering_server_default.cpp), [geri okuma sözleşmesi](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/doc/classes/RenderingDevice.xml). | kritik |
| 4 | §2.4, 93–99; §2.7 | TAA/SDFGI kapatılması iki zamanlı sahnenin bütün zaman bağımlılıklarını çözmüyor. Malzeme `TIME` kullanımı, animasyon, otomatik pozlama, parçacıklar, ışık ve ortam değişiklikleri için hangi zamanın yetkili olduğu yok. `dt` iki çizime verilirse görsel alt sistemler ayrıca ilerleyebilir. | Sensör yolundaki bütün zaman bağımlı girdiler `t_meas` ile sürülsün; otomatik ilerleyenler kapatılsın veya açıkça kapsam dışı bırakılsın. `frame_step` anlamı tanımlansın. `CameraRequest`, dalga/ortam/ışık sürümlerini de sabitlesin; şu an yalnız gövde listesi ve kamera yapılandırma sürümü var. | önemli |
| 5 | §2.4, 112–120; §6 geri okuma satırı | `max_fps=120` bir **üst sınırdır**, 120 gerçek çizim/s güvencesi değildir. Gizli pencerede yalnız kamera isteği geldiğinde elle çizim yapılırsa döngünün hızlı dönmesi geri okuma kuyruğunu aynı hızla ilerletmeyebilir. SIL, teslimde beklerken kuyruğun ilerlemesini de durdurabilir. | Bekleyen geri okumaları ilerleten çizim/submit takvimi tanımlansın; sim beklerken görselleştiricinin bağımsız ilerlemesi güvenceye alınsın. Bütçe `IPC + çizime bekleme + çizim + geri okuma + halka + bildirim` toplamıyla kurulsun; algılama ve A1 teslimi ayrıca eklensin. | kritik |
| 6 | §3.4, 326–330; §3.5 | **Çoklu okuyucu protokolü eksik.** “Her geçişin tek yazarı” ifadesi bunu sağlamıyor. Algılama ve kayıt aynı anda okuyabilir; ilk okuyucu yuvayı boşaltınca ikincinin verisi ezilebilir. Ayrıca salt okunur eşleyen süreç `durum` ve `okuyucu_sayısı` alanlarını değiştiremez. | Sim sahiplik yöneticisi olsun veya kontrol alanı ayrı yazılabilir eşlensin. Okuyucular yayın sırasında sabitlensin; bütün okuyucular bırakmadan yuva boşalmasın. Süreçler arası atomiklerin platform sözleşmesi, hizası ve release/acquire sırası belirtilecek kadar açık olmalı. | kritik |
| 7 | §3.5, `SlotRelease`; §3.4 | `SlotRelease {halka_nesli, yuva}` aynı nesildeki yuva yeniden kullanımını ayıramıyor. Yinelenen veya gecikmiş bırakma yeni kareyi serbest bırakabilir. `FrameAvailable` ile sahiplik kazanımı da atomik bir işlem olarak tanımlanmamış. | Bırakma ve sahiplenme belirteci en az nesil, yuva, kare sıra numarası ve okuyucu kimliği içersin. Yinelenen bırakma etkisiz olmalı; sim hangi okuyucunun hangi kareyi tuttuğunu bilmelidir. | kritik |
| 8 | §3.1, 297; §4, 377–385 | Algılama veya kayıt süreci çökerse tuttuğu yuvaların nasıl kurtarılacağı yok. Görselleştirici yeniden bağlandığında mevcut halkayı “sıfırlamak”, eski okuyucular hâlâ okurken üzerine yazılmasına yol açabilir. macOS’ta bütün önek eşleşmelerini silmek başka canlı oturumu da etkileyebilir. | Bağlantı nesli ve okuyucu yaşam döngüsü tanımlansın. Yeni nesil ayrı eşleme nesnesiyle açılsın; eski eşlemeler kullanım bitene kadar yaşasın. Yalnız sahipliği kanıtlanan kalıntılar temizlensin. Sim, görselleştirici, algılayıcı ve kayıt süreçlerinin ayrı ayrı öldürülmesi sınanmalı. | kritik |
| 9 | §2.4, 119; §4, 383 | Geç istek, çizilmekte olan istek, geri okuması beklenen istek ve hazır kare için iptal kuralları ayrılmıyor. İptalden sonra gelen callback’in kareyi yeniden yayımlaması mümkün. “Daha yeni istek gelince eskisini düşür” politikası farklı kameralar arasında da belirsiz. | Kamera başına sınırlı kuyruk ve istek durum makinesi tanımla. İptal edilen callback yalnız kaynaklarını bırakmalı; kare yayımlamamalı. Yeni istek politikası kamera bazında ve yalnız bekleyen işler için açıklanmalı; devam eden işlerin ömrü sınırlandırılmalı. | önemli |
| 10 | §2.4, 99 | Yedek yöntemde operatör zamanının geriye gitmeyeceği kanıtlanmıyor. Aynı TCP akışında sıralı gelmek, işlenmesi geciken `CameraRequest`’in daha önce gösterilmiş son durumdan eski olmasını engellemez. | Örneğin `t_meas=100 ms` isteği beklerken operatör `116,25 ms` durumunu göstermiş olabilir. Monoton operatör gösterimi için açık seçim/bekletme kuralı koy veya yedeğin zaman sıçramasına izin verdiğini belirt. | önemli |
| 11 | §2.1, 40; §2.4, 89–106 | “Son iki durum” tamponu, bir yayın gecikmesi ve araya giren olaylarla yeterli olmayabilir. Olay adımında tek snapshot göndermek sınırın **iki tarafını** sağlamaz. Sınır öncesinde eski pozda beklemek hareketi dondurur; P3’teki zaman hatası sınırı bunu ölçmüyor. | Zaman aralığına göre boyutlanan geçmiş tut; olay öncesi ve sonrası durumları açıkça ayır. Eşitlikte hangi tarafın seçildiğini belirt. Reset/atlamada geçmiş ve bekleyen istekler nesille temizlensin. Kabul testi zaman etiketinin yanında poz ve yönelim hatasını da ölçsün. | önemli |
| 12 | §2.4, 110; §2.9, 268; B05 §2.5 | `L_kam` B05’te pozlama **sonrası** gecikmedir, fakat burada pozlama ortasına ekleniyor. Sonuçta pozlamanın bitmesinden önce teslim mümkün. `t_teslim_alg` de kartın gerçek alım anı diye adlandırılıyor; B06 v0.2 toplu pencere/parti gecikmesi ekliyor. | `t_teslim_kare=t_meas+T_pozlama/2+L_sonrası` kullan veya `L_kam`ı merkezden başlayan toplam gecikme olarak yeniden tanımla. Kare hazır olma, algılama hazır olma, A1 partiye giriş ve kart alım zamanlarını ayır. `t_capture=t_meas` korunmalı. | kritik |
| 13 | §2.9, 251–270; §3.2, 310; §3.4, 329 | Yer gerçeği sınırı kendi içinde çelişiyor: bileşene poz/yer gerçeği verilmediği söyleniyor, fakat yapılandırma ve halka fizik gerçeği montajını taşıyor. Ayrıca yer gerçeğinden anlamsal algılama üreteci aynı bileşene takılabilir deniyor; bu A8’in “yer gerçeği almaz” şartına uymaz. | Çizim için gerçek kamera modeli ile algılayıcının bildiği nominal kalibrasyonu ayır. Algılayıcıya yalnız izin verilen nominal üst veri verilsin. Anlamsal üreteç gerekiyorsa sim içinde ayrı test kaynağı olsun; görüntü işleyen A8 bileşenine yer gerçeği açılmasın. | önemli |
| 14 | §2.5, 128–135 | RD dokusunun doğrudan `RGBA8_SRGB` olduğu varsayılmış. RD geri okuması dokunun ham biçimini verir; bu çağrı kendiliğinden renk uzayı, ton eşleme veya kanal biçimi dönüşümü yapmaz. | Son sensör çıktısı için biçimi sabit, açık bir doku/geçiş tanımla. Doğrusal ışık, optik işlem, pozlama, ton eşleme, sRGB kodlama ve RGBA8 nicemleme sırası yazılsın. Ek dönüşüm varsa kopya ve GPU bütçesine girsin. [4.7.2 RenderingDevice](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/doc/classes/RenderingDevice.xml). | önemli |
| 15 | §2.5, 133, 149–158 | Genel `fx,fy,cx,cy` değerlerinin yalnız FOV ve ana nokta kaydırmasıyla temsil edildiği kanıtlanmıyor. Optik denklemindeki `z` için de derinlik tamponundan fiziksel menzile dönüşüm yok. Eksenel derinlik ile kamera ışını boyunca mesafe farklıdır. | Desteklenen iç parametre kısıtlarını veya frustum/yeniden örnekleme dönüşümünü tanımla. `frustum_offset` yalnız `PROJECTION_FRUSTUM` altında etkilidir. Derinlik yeniden kurulup uygun ışın uzunluğu kullanılmalı; görüş kenarlarında test yapılmalı. [4.7.2 Camera3D](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/doc/classes/Camera3D.xml). | önemli |
| 16 | §2.7, 191–197; P7 | `Δx≤λ_min/16` yalnız tek bileşen için yaklaşık %1,93 genlik sınırı verir; toplam yüzeyin P7’deki `%1 H_s` sınırını sağladığını göstermez. 25 m yarıçap ile 256×256 ağın hangi `λ_min` için yeterli olduğu da belirtilmemiş. | Üçgen ağın gerçek kenar/yön geometrisiyle toplam hata bütçesi çıkar. Ağ sıklığını spektruma göre seç veya desteklenmeyen spektrumu reddet. Kamera görüşündeki su, 25 m yarıçapın dışına uzanıyorsa sensör görüntüsünün geçerlilik alanını belirt. | önemli |
| 17 | §2.7, 184, 197; §6 dalga satırı | Doğrudan 64 bileşen toplamı uygulanabilir bir adaydır; fakat sınırsız bileşen kabulü, uniform yerleşimi ve ek çizim geçişleri bütçelenmemiş. FFT yedeği de rastgele yön/frekanslı bileşenleri ızgaraya kayıpsız yerleştiremez. | Bileşen üst sınırı ve taşma davranışı tanımla. Uniform/tampon düzeni ve shader derleme maliyetini ölç. FFT yedeğinde yeniden örnekleme/projeksiyon hatasını da bütçele. Godot döngü ve uniform dizilerini destekler; masaüstü uniform sınırı genellikle 65.536 B’dir, performans garantisi değildir. [4.7 shader belgesi](https://docs.godotengine.org/en/4.7/tutorials/shaders/shader_reference/shading_language.html#uniform-limits). | önemli |
| 18 | §5, 401–406; §3.4 | Sade kademede görüntü–fizik tutarsızlığını yalnız “yorumlama” notuyla bırakmak, yüzey yakınındaki algılama değerlendirmesine yetmez. Sözü verilen `dalga_görseli` alanı üst veri şemasında yok. Tam kademede de yarı batık kameranın bütün karesine tek ortam uygulanması benzer soruna yol açıyor. | Karelerde tutarsızlık/model geçerliliği alanları tanımla ve hangi deneylerin geçersiz olduğunu makinece değerlendirilebilir yap. Su yüzeyi geçişi olan deneyleri sınırlı modelle kabul etme veya uygun maske/model ekle. | önemli |
| 19 | §5, 406; §6, 420–423 | Düşük Windows hedefi için yalnız çözünürlük ve kamera hızı azaltılmış; operatör çözünürlüğü/hızı ve geri okuma için ek çizim yükü sınırlandırılmamış. Düşük süreç önceliği GPU ve paylaşılan bellek bant genişliği izolasyonu sağlamaz. | Operatör görünümü, kamera, shader ve bellek için ayrı sınırlar koy. Kamera kapalı ama 3B sade açık çalışma ile bütün görselleştiricinin kapatılması ayrılmalı. Yerleşik sis yedeğinin kanallı zayıflama/geri saçılımla aynı sensör modeli olmadığı kayda girmeli. Başarı yalnız birleşik HIL yükü altında M12/M20/M21/M30 ile kabul edilmeli. | önemli |
| 20 | §3.5, 347; §8, 469 | Kamera algılayıcı bağlantısının iskelette adsız olduğu söyleniyor; iskelet 0.6 bunu **A8** olarak tanımlıyor. | §3.5 başlığı, süreç şeması ve bütün atıfları A8 olarak güncelle; yeni arayüz önerisini kaldır. Girişteki sürüm 0.5 atfını düzelt. | önemli |
| 21 | §2.8 manifest; §8, 459 | Türetilmiş dışbükey ağları kaynak varlık diye depoya koyma önerisi iskelet 0.6 A7 ile açıkça çelişiyor. Manifestte türetilmiş her çıktının özeti de listelenmiyor. | Kaynaklar depoda, türetilmiş ağlar derleme çıktısında olsun. Araç sürümü/ayarları ve **çıktı özetleri** manifestte bulunsun. Yeniden üretim özeti tutmazsa başarısızlık politikası tanımlansın; yalnız sürüm ve tohum eşitliği yeterli sayılmasın. | önemli |
| 22 | §4, 376–386; §2.4, 122 | Geçersizlik kuralları uyuşmuyor: normal HIL’de `%1` kayıp eşiği varken çökme/algılayıcı kaybında `kamera_gerekli` koşusu doğrudan geçersiz. `ViewerStatus` 1 Hz, fakat kesilme zaman aşımı belirtilmemiş. | Genel kayıp eşiği ile tek olayda geçersiz kılan durumları ayrı tanımla. Algılama sonucu kaybı için de eşik/süre ver. Görselleştirici canlılığı, render ilerlemesi ve kamera üretimi ayrı izlenmeli; son tarih karşılaştırmasında eşitlik kuralı belirtilmeli. | önemli |
| 23 | §7, P4–P13 | Testler bazı kritik yaşam döngülerini kapsamıyor: iki okuyuculu halka yarışı, okuyucu çökmesi, iptal sonrası callback, sim yeniden başlatma, reset sırasında bekleyen işler. P4’ün `%1` kayıp ölçütü de algılama ve karta teslim zincirini kapsamıyor. | Bu durumlar için kabul ölçütü ekle. P4’e uçtan uca algılama/A1 teslim süresini ekle; iki zamanlı testleri olay, ışık/ortam değişimi ve uzun geri okuma kuyruğuyla çalıştır. P5’te piksel farkının renk uzayı ve deterministik ayarları sabitlenmeli. | önemli |

## Yeniden hesaplananlar

Aşağıdakiler aritmetik sonuçlardır; ölçülmüş performans değildir.

**Kamera yükü**

| Yapılandırma | Bayt/kare | MB/s — ondalık | MiB/s | Bölümle karşılaştırma |
|---|---:|---:|---:|---|
| 640×480×4, 30 Hz | 1.228.800 | 36,864 | 35,15625 | Doğru |
| Aynısı, iki kamera | 2.457.600 toplam | 73,728 | 70,3125 | Doğru |
| 320×240×4, 15 Hz | 307.200 | 4,608 | 4,39453125 | Doğru |
| 1280×720×4, 30 Hz | 3.686.400 | 110,592 | 105,46875 | Doğru |

§5’teki sıkıştırılmamış kayıt hızı **36,9 MB/s** doğru yuvarlamadır. Tek kamera için 30 dakikalık ham kayıt **66,3552 GB**, bir saatlik kayıt **132,7104 GB** eder; üst veri ve dosya maliyeti hariçtir.

**Kopyalar ve halka belleği**

- Tarif edilen yolda GPU→staging aktarımı, staging→`PackedByteArray` ve `PackedByteArray`→halka bulunur. Son ikisi birer tam CPU kopyasıysa 640×480@30 için **73,728 MB/s kopyalanan yük**, okuma+yazma olarak **147,456 MB/s CPU bellek trafiği** oluşturur. Algoritmanın piksel okumaları, kayıt, sürücü ve biçim dönüşümü buna dahil değildir. Fiziksel bellek trafiği önbellek/bellek mimarisine bağlıdır.
- Dört yuvalı halka kamera başına yalnız pikseller için:

| Çözünürlük | Dört yuva | MiB |
|---|---:|---:|
| 320×240 | 1.228.800 B | 1,171875 |
| 640×480 | 4.915.200 B | 4,6875 |
| 1280×720 | 14.745.600 B | 14,0625 |

İki 640×480 kamera için **9.830.400 B = 9,375 MiB** gerekir. Başlık, üst veri, hizalama, GPU dokuları ve Godot’un bekleyen geri okuma tamponları ayrıca eklenir. “3 kuyruk + 1 okunan” dört yuvanın yeterli olduğunu kanıtlamaz: GPU kuyruğu ile CPU halkası farklı kaynaklardır; kayıt okuyucusunun tutma süresi de sınırlanmalıdır.

`/ida-cam-<8 hex>` adı **17 karakterdir**; bölümdeki sayım doğrudur.

**Geri okuma kuyruğu**

`L_kuyruk≈N/F_gerçek`:

| Gerçek çizim hızı | 1 kare | 2 kare | 3 kare |
|---|---:|---:|---:|
| 30/s | 33,333 ms | 66,667 ms | 100 ms |
| 60/s | 16,667 ms | 33,333 ms | 50 ms |
| 120/s | 8,333 ms | 16,667 ms | 25 ms |

Bölümün 30 ve 60/s hesapları doğru. Ancak bunlar toplam gecikme değildir; 120/s satırı da `max_fps=120` ayarıyla garanti edilemez. API, geri çağrı gecikmesini kare kuyruğuyla ilişkilendirir ve büyük dokuların bant genişliği maliyetini ayrıca belirtir. [4.7.2 RenderingDevice sözleşmesi](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/doc/classes/RenderingDevice.xml).

**Poz akışı**

`4+1+1+2+24+32+24+24 = 112 B/gövde` doğrudur.

- `16×112+64 = 1.856 B/ileti`.
- `1.856×60 = 111.360 B/s = 0,11136 MB/s`.
- Aynı büyüklükte istek varsayımıyla `×30 = 55.680 B/s`.
- Toplam **167.040 B/s**; tek kameranın ham piksel hızının **%0,453125’i**. “Yaklaşık %0,5” doğrudur.

Bu hesap henüz tam tel boyutu değildir: `CameraRequest` başlığı, fırlatıcılar, torpido durumları, olay yayınları ve TCP/IP maliyeti kesinleşmemiştir. Ayrıca `süreklilik_no` için burada örtük olarak **u16** ayrılmış; yeniden kullanım/taşma davranışı tanımlanmamıştır.

**Zamanlama**

800 Hz tabanda, faz sıfırken en yakın adıma bağlama:

| Akış | İlk adım numaraları | Tekrarlayan aralıklar |
|---|---|---|
| Kamera 30 Hz | 27, 53, 80, 107, 133, 160 | 27, 26, 27 |
| Kamera 15 Hz | 53, 107, 160, 213, 267, 320 | 53, 54, 53 |
| Poz yayını 60 Hz | 13, 27, 40, 53, 67, 80 | 13, 14, 13 |

Bu akışlarda azami ideal-zaman sapması **0,416667 ms**; genel en yakın sınır kuralının üst sınırı **0,625 ms**’dir. §8’deki “53/54” ifadesi uygundur; katı dönüşüm anlamında kullanılmamalıdır. Bir yayın periyodu **16,666667 ms** olsa da gerçek yayın aralıkları **16,25 veya 17,5 ms** olur; “son iki durum” tamponu buna göre değerlendirilmelidir.

**Dalga ve geometri**

- `256×256 = 65.536` köşe.
- `65.536×64 = 4.194.304` bileşen değerlendirmesi/geçiş.
- `×(60+30) = 377.487.360/s ≈ 3,8×10⁸/s`: bölümün hesabı doğru.
- Operatör gerçekten 120/s çizilirse kamera 30/s ile toplam **629.145.600/s** olur. Gölgeler ve diğer geometri geçişleri bu sayımlara dahil değildir.
- `Δx=λ/16` için `(kΔx)²/8 = π²/512 = 0,01927657`: **%1,927657** doğru.
- Eski örnekte `(π/2)²/8 = 0,308425`; gerçek orta nokta farkı `√2/2−1/2 = 0,207107`. Bölümün karşılaştırması doğru.
- Fakat toplam sınır yaklaşık `0,01927657 Σ|a_i|` biçimindedir; bundan `%1 H_s` sonucu çıkmaz.
- 256 köşenin 50 m genişliği örttüğü varsayılırsa `Δx=50/255≈0,19608 m`; `λ/16` koşulu ancak **λ_min≥3,13725 m** için sağlanır. Daha kısa dalgalarda ağ büyümelidir.
- Faz uyuşmazlığı için karşı örnek: `a=1`, `kx=0`, `φ=π/4`, `ωt=π/4`. B04 yüksekliği **0**, 07 yüksekliği **1** verir.

**Dönüşümler ve duyarlık**

`det C=+1`, `CᵀC=I` ve glTF→Godot yerel dönüşümünün `diag(−1,1,−1)` olması doğrudur; bu Y çevresinde 180° dönüşümdür.

`2⁻²⁴×1000 m = 0,0596046 mm` hesabı doğrudur; ancak bu yalnız konumun temsil yuvarlaması ölçeğidir. Dönüşüm, büyük konumların çıkarılması ve dalga fazı hesabının toplam hatasını kanıtlamaz.

**Varsayımlar ve test sayıları**

4 yuva, 25 m yarıçap, 120 FPS üst sınırı, 1 s yeniden deneme, 60 s bekleme, 30 hata, `%1` kayıp, 5 s toparlanma, 1 mm/1 cm geometrik eşikler ve piksel toleransları ölçüm sonucu olarak sunulmamış; bu yönüyle etiketleme çoğunlukla doğru. Bunların yeterliliği aritmetikle doğrulanamaz.

P2’de bir saatte **216.000** periyodik yayın; P4’te 30 dakikada **54.000 kare** vardır. “Düşen kare <%1” en fazla **539** kayba izin verir. P11’in iki saatinde 30 Hz kamera çalışırsa **216.000 kare** oluşur. Test süreleri kaynaklara dayalı garanti değil, seçilmiş kabul varsayımlarıdır.

## İskelete ya da diğer bölümlere notlar

**İskelet 0.6 değişikliklerinin durumu**

| Değişiklik | 07’deki durum |
|---|---|
| Algılama bağlantısının A8 olması | Aktarılmamış: §3.5 ve §8 hâlâ adsız bağlantı/yeni A8 öneriyor. |
| Malzeme varsayılanlarının A7’de olması | §2.8’de doğru yazılmış. |
| Senaryoya özgü kalibrasyonun A5’te geçersiz kılması | §2.8’de doğru; §8 madde 3 bunu hâlâ yeni öneri gibi sunuyor. Güncel bağlayıcı kurala çevrilmeli. |
| Türetilmiş ağların depoya girmemesi | §8 madde 1 açıkça tersini öneriyor. Sabit araç sürümüyle üretim ve çıktı özeti doğrulaması eklenmeli. |

Sorumluluk sınırları genel olarak doğru: A1 taşıması B06’ya, fizik/çarpışma B04’e, sonar B05’e, paneller ve kayıt B08’e bırakılmış. Sekiz ana başlık eksiksiz. STM32U5A9J-DK hedefi, C/C++, NED, sabit `O_b`, kuaterniyon sırası ve fizik adımıyla açık bir çatışma yok. A2’nin hızlar, sahne olayları ve kamera istekleriyle genişletilmesi iskelette açıklanabilir; A8 için yeni numara gerekmiyor.

**Komşu belgelerle uyuşmazlıklar**

- `docs/mimari/04-simulator-cekirdegi.md` dalga fazını zaten tanımlıyor; 07’nin §3.7’de bunu hâlâ beklenen iş sayması güncel değil. `VizState` hızları, süreklilik bilgisi ve olay öncesi/sonrası durumlar ise gerçekten eksik.
- B04 ve `docs/mimari/03-atis-kontrol.md` hâlâ pasif/itkisiz torpido ifadeleri taşıyor; iskelet 0.5–0.6 itkili torpidoyu bağlayıcı kılıyor. Bu konuda 07’nin itki durumu istemesi doğrudur.
- `docs/mimari/05-sensorler-ve-sonar.md` fizik gerçeği montajında referansı hareketli CG sayıyor; 07 ve iskelet sabit `O_b` kullanıyor. B05 düzeltilmeli.
- B05’in pozlama sonrası gecikmeyi pozlama ortasına ekleme hatası 07’ye taşınmış. Bu, `docs/mimari/inceleme/05-sol.md` bulgu 3’te de belirtilmiş.
- B04 olay sınırı interpolasyonu için `docs/mimari/inceleme/04-sol-2.md` bulgu 7 hâlâ geçerlidir. 07’nin süreklilik numarası doğru yönde, fakat iki taraflı snapshot sözleşmesi tamamlanmamış.
- İnceleme sırasında mevcut `docs/mimari/06-protokol-ve-hil.md` **v0.2** okundu. Artık A8’i ve `values[3]` düzenini kullanıyor; 07’nin eski taslağa ilişkin notu güncellenmeli. Bununla birlikte B06’da `t_sigma_ns` ve `calib_ver` hâlâ görünmüyor. Ayrıca B06 **≤4 algılama/kare**, 07/B03 **≤8** kabul ediyor. B06’nın 120 kayıt/s ve **10.200 B/s** hesabı, sekiz kayıtla aynı kalamaz: mevcut 85 B çerçeveyle bile **20.400 B/s** gerekir; eksik alanlar eklenince tekrar hesaplanmalıdır.
- B06’nın toplu pencere teslimi nedeniyle 07’deki `t_teslim_alg` doğrudan kart alım anı sayılamaz.
- İskeletin A7 kuralı uygulanabilirlik bakımından ölçüm ister: sabit araç sürümü tek başına platformlar arası aynı CoACD çıktısını garanti etmez. Çıktı özeti tutmadığında başarısızlık veya onaylı yeniden üretim yolu tanımlanmalıdır; bu belirsizlik türetilmiş ağı depoya koyma yetkisi vermez.
- `docs/research/00-duzeltmeler-ve-kararlar.md` içindeki Ç2/Ç15 ve açık sorularda Ö6’nın hâlâ açık görünmesi, daha yeni **K9** ile birlikte okunmalıdır: web panel tercihi kesindir; ayrı pencere/gömme yerleşimi öneridir.

**00 kararları ve ölçümleri**

| Kayıt | Aktarım durumu |
|---|---|
| Ö5 | Tek Godot sahnesi ve ayrı süreç doğru aktarılmış. |
| Ö5-k | Gizli/minimize sürekli kamera kabul kapısı ve görünür pencere yedeği doğru; yedeğe geçişin deney kapsamı kabulüne bağlı olduğu korunmalı. |
| Ö9 | Jolt’un eklentiye alınmaması, çarpışmanın B04’te kalması uygun. |
| Ö10 | Ayrı pencere başlangıcı uygun; teknik zorunluluk veya kullanıcı kararı sayılmamalı. |
| Ö11 | Kademeler ve bilinmeyen düşük Windows donanımı doğru; ortak kaynak yükü bütçesi eksik. |
| K9 | Panellerin web olması doğru aktarılmış. |
| M12 | Kamera/geri okuma testleri var; 720p deney noktası tabloda kalmış, kabul testine açıkça bağlanmamış. |
| M13 | Görünürlük ve App Nap riski korunmuş; çalıştırılarak doğrulanmış sayılmıyor. |
| M14 | Kalibrasyon gereği korunmuş. Açık veriyle sınırlama K8’e uygun; belirli sanal kamera/su ayarının fiziksel doğrulanması sayılmaz. |
| M28 | İki işletim sistemi, sürekli üretim ve içerik kontrolü iyi aktarılmış; dönüşüm/çizim sırası ve kuyruk ilerlemesi eklenmeli. |
| M29 | Uzun koşu ve shader başlangıç süresi ayrılmış; §4’te ikisinin tek hata satırında ele alınması netleştirilmeli. |
| M30 | Veri hızı, kopya ve kuyruk ölçümü var; halka kapasitesi ve uçtan uca teslim bütçesi tamamlanmamış. |
| M32 | Kesme/LOD/zamanlama ele alınmış; faz uyuşmazlığı ve toplam ağ hata sınırı açık. |
| M33 | Sabit sürüm, CMake/SCons, CRT ve imza koşulları aktarılmış. |

**10 ve 12 numaralı rapor düzeltmelerinin madde bazında durumu**

| Düzeltme | 07’ye aktarım |
|---|---|
| 10.1 | “Ortak C++ RT soyutlaması yok” yanlışı tekrarlanmıyor; kapsam dışında. |
| 10.2 | Slang/Metal için genel RT yokluğu iddiası tekrarlanmıyor. |
| 10.3 | MoltenVK deneysel/kararlı ayrımına ters iddia yok. |
| 10.4 | HoloOcean multipath/octree yanlış çıkarımları tekrarlanmıyor. |
| 10.5 | Alt-milisaniye sonar garantisi tekrarlanmıyor; sonar B05’e bırakılmış. |
| 10.6 | RGB son işlemin spektral doğruluk sağlamadığı doğru aktarılmış. |
| 10.7 | Ağ interpolasyonu ve LOD farkı kabul edilmiş; sayısal toplam sınır tamamlanmamış. |
| 10.8 | Eski 15–40 ms toplam gecikme garantisi tekrarlanmıyor; kuyruk dahil edilmiş, uçtan uca bütçe açık. |
| 10.9 | Embree platformlar arası bit eşitliği garantisi tekrarlanmıyor. |
| 10.10 | APL-UW frekans genellemesi tekrarlanmıyor; kapsam dışında. |
| 10.11 | Doğrulanmamış eko eşleşmesi kanıt diye kullanılmıyor. |
| 10.12 | Donanımsal RT zorunluluğu getirilmiyor; uygun. |
| 12.1 | Otomatik çizim istisnaları ve macOS/Windows ayrımı doğru aktarılmış. |
| 12.2 | Küçük pencerenin koşulsuz çözüm olmadığı doğru. |
| 12.3 | `force_draw` kaynak desteği ile sürekli üretim ölçümü doğru ayrılmış; uygulama sırası eksik. |
| 12.4 | Varsayılan `UPDATE_WHEN_VISIBLE`, kapalı `UPDATE_DISABLED` ayrımı doğru. |
| 12.5 | LibGodot’un 4.6’dan beri mevcut olduğu ve yüzeye gömmenin ayrı sorun olduğu doğru. |
| 12.6 | Jolt statik sensör kısıtı yanlış biçimde genellenmiyor. |
| 12.7 | Temas koordinatı sorumluluğu B04’te bırakılmış; eski başlık tutarsızlığı tekrarlanmıyor. |
| 12.8 | macOS imza/yetki gereğinin koşullu olduğu doğru. |
| 12.9 | Shader derlemesi ile çalışma donması ayrılmış; 4.6.x’e dönüş çözüm sayılmıyor. |
| 12.10 | `CMAKE_MSVC_RUNTIME_LIBRARY` doğru yazılmış. |
| 12.11 | Kamera aritmetiği ve paylaşımlı belleğin bütünü kopyasız yapmadığı doğru. |
| 12.12 | Gerçek çizim hızı esas alınmış; hızlı döngünün gerçek çizim üretmesi henüz güvence altında değil. |
| 12.13 | Mobil cihaz sürücüsü ile Mobile renderer genellemesi tekrarlanmıyor. |
| 12.14 | Kesme, ağ, LOD ve zamanlama farkları ele alınmış; faz hatası nedeniyle tutarlılık sağlanmış değil. |
| 12.15 | Hareketsizlik bakım riski sayılmış, kesin terk edilmişlik sayılmamış. |
| 12.16 | Vendor uzantısı ve geniş manifest yaklaşımı doğru; türetilmiş çıktı özetleri eksik. |
| 12.17 | Süreç izolasyonu ile deney geçerliliği doğru ayrılmış; geçersizlik eşikleri birleştirilmeli. |

Resmi **4.7.2** kaynakları, `force_draw` bağlamasının görünürlük kapısından geçmediğini doğruluyor; macOS örtülme/gizlenme ile Windows minimize davranışı da bölümde doğru anlatılmış. Ancak bunlar kesintisiz, zamanında kamera üretimi kanıtı değildir. [API bağlaması](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/servers/rendering/rendering_server.cpp), [ana döngü](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/main/main.cpp), [macOS görünürlük hesabı](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/platform/macos/godot_window_delegate.mm), [Windows çizilebilirlik hesabı](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/platform/windows/display_server_windows.cpp).

Aynı `World3D`’yi paylaşan viewport’lar, `UPDATE_ONCE/ALWAYS` kipleri ve başsız kipte RenderingDevice bulunmaması resmi sözleşmeyle uyumludur. Bunların hiçbiri paylaşılan sahnenin iki zamanını kendiliğinden ayırmaz. [Viewport](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/doc/classes/Viewport.xml), [SubViewport](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/doc/classes/SubViewport.xml), [RenderingDevice](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/doc/classes/RenderingDevice.xml).