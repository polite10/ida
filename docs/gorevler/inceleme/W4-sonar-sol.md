# sol incelemesi: sonar

## Hüküm

**Kabul edilmemeli: 6 kritik, 2 önemli bulgu var.**

`19cd77a` ve `1bc176a` incelendi; `sim/sonar` kaynakları, başlıkları ve testleri okundu. Bulgular kaynak incelemesine ve dosya üretmeden yapılan bağımsız hesaplara dayanıyor. Derleme, CTest, sanitizer ve Windows/MSVC doğrulaması çalıştırılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `sim/sonar/src/sonar_kumesi.cpp:259`, `:293`, `:364` | **Dönüştürücü havadayken de su içi akustik hesap yapılıyor.** Montajın dünya konumu hesaplanıyor fakat su içinde olma denetimi yok. Örneğin su yüzeyinin 1 m üstündeki aşağı bakan Ping1D, yüzey düzlemini yaklaşık 1 m uzaktaki normal bir saçıcı olarak işler. Bu, hibrit aracın yüzey davranışını bozar; 05 §4 havadaki Ping1D için güven 0, Ping360 için yalnız gürültü ister. | Dönüştürücünün su içinde olup olmadığını ortam/sahne üzerinden belirle; havadayken geometrik ekoları üretme. Yüzeye çıkış ve eğimli montajla havaya çıkan sonar karşı örneklerini ekle. | kritik |
| 2 | `sim/sonar/src/sonar_kumesi.cpp:445` | **Geçerli Ping1D ayarı tanımsız davranışa götürüyor.** `scan_start=99,75`, `scan_length=0,25`, `otomatik=true` doğrulamadan geçer. İlk ping sonunda `std::clamp(l, 0.5, 0.25)` çağrılır; alt sınır üst sınırdan büyüktür. Bu, [`std::clamp` önkoşulunu](https://eel.is/c++draft/alg.clamp) ihlal eder. | Otomatik pencerenin asgari uzunluğunu kalan kullanılabilir menzille sınırla; bütün kabul edilen pencerelerde `lo ≤ hi` değişmezini koru. | kritik |
| 3 | `sim/sonar/src/sonar_kumesi.cpp:265`, `:721` | **Frekans değişince soğurma güncellenmiyor.** `s.alfa` yalnız kurulumda hesaplanıyor. Sonraki yapılandırma yeni `tx_freq_khz` bildirirken ışınlar ve TVG eski frekansın katsayısıyla hesaplanıyor. Tatlı suda 750→1000 kHz değişiminde, 50 m’de ham enerji hatası yaklaşık **10,22 dB**. | Yeni yapılandırma uygulanırken `sogurma` geçersiz kılması yoksa katsayıyı yeni frekanstan hesapla; açık sabit verilmişse onu koru. Yeniden yapılandırmayı aynı frekansla doğrudan kurulmuş kümeyle karşılaştır. | kritik |
| 4 | `sim/sonar/src/sonar_kumesi.cpp:333`, `:794` | **Yeniden yapılandırmanın motor süresi yanlış açıdan hesaplanıyor.** Ping atılınca `s.aci` sonraki açıya ilerletiliyor; yapılandırıcı bunu mevcut kafa açısı sayıyor. İlk ping 0 gradyanda, adım 200 iken süren ping sırasında başlangıcı 200 yapmak motor farkını 0 çıkarır. `t_grad=1 ms` için gereken 200 ms hareket kaybolur. | Son gönderimin açısını ve sonraki istenen açıyı ayrı tut; yeni komutun hareketini tanımlanan gerçek kafa durumundan hesapla. Süren ping sırasında komut değişimini sıfır olmayan motor süreleriyle sına. | kritik |
| 5 | `sim/sonar/src/demet.cpp:146`, `:158` | **Pencere dışında kalan ekolar evrişimden önce siliniyor.** Darbe çekirdeği için ışın menzili genişletilmiş olsa da merkezi pencere dışında olan hücreler ince diziye alınmıyor. Böylece çekirdeğin pencereye taşan kısmı kayboluyor; 05 §2.4.4’teki evrişim davranışı uygulanmıyor. Aşağıdaki birim enerjili karşı örnekte son kutunun beklenen katkısı yaklaşık **0,683391**, kod yolunda 0. | İnce diziyi çekirdek desteği kadar iki yönde genişlet; katkıları biriktirip evriştirdikten sonra istenen pencereyi kırp. Alt ve üst sınıra komşu ekoları sına. | kritik |
| 6 | `sim/sonar/include/ida/sonar/ayarlar.hpp:76`, `sim/sonar/src/sonar_kumesi.cpp:334`, `:440` | **A5’in sonarlar için geçerli `saat_kaymasi_ppm` alanı temsil edilmiyor.** `SonarAyari` bu alanı taşımıyor; zamanlayıcı da kullanmıyor. A5 §3.6/K41 yalnız IMU’da bu alanı yasaklar. Sonar için kabul edilen sıfır dışı ayarın davranışa aktarılması mümkün değil. | Alanı kurulum arayüzüne taşı ve cihaz zamanlamasına uygula. Sıfır dışı kaymayla uzun süreli faz farkını ve fizik adımına bağlamayı doğrula. | kritik |
| 7 | `sim/sonar/src/sonar_kumesi.cpp:84` | **Açık darbe süresi doğrulanmıyor.** `transmit_duration_us=65535` kurulumda ve yeniden yapılandırmada kabul ediliyor. [Üretici protokolü](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/) cihaz aralığını 1–1000 µs olarak veriyor. A4’ün alanı daraltmaması, cihaz modelinin bu sınırı kaldırdığı anlamına gelmez. | Otomatik seçim için ayrılmış 0 dışındaki girdileri cihaz aralığıyla doğrula; geçersiz yeniden yapılandırmada önceki ayarı koru. | önemli |
| 8 | `sim/sonar/src/akustik.cpp:69`, `sim/sonar/src/sonar_kumesi.cpp:617` | **Mackenzie geçerlilik alanı dışında sessizce kullanılıyor.** Kurulum 40 °C ve 50‰ tuzluluğu kabul edip Mackenzie’ye gönderiyor. Erişilen [NPL rehberi](https://www.comm-tec.com/library/technical_papers/speedsw.pdf) üst sınırları 30 °C ve 40‰ verir. Sonuçta doğrulanmış alan ile dışdeğerleme ayrılmıyor. | Geçerlilik alanını denetle; dışındaki hesapları açıkça doğrulanmamış dışdeğerleme olarak bildir. A5’in daha geniş ortam aralığının nasıl karşılanacağını mimari karara bağla. | önemli |

## Yeniden hesaplananlar

Hesaplar ürün işlevleri çağrılmadan, ayrı ifadelerle yapıldı.

| Hesap | Bağımsız sonuç |
|---|---|
| UNESCO: `T68=40 °C`, `S=40`, `P=1000 bar` | **1731,995394206 m/s**; yayımlanan 1731,995 değerinin test toleransı içinde. Katsayılar [MBARI referans uygulamasıyla](https://www.mbari.org/wp-content/uploads/2016/03/SW_svel.txt) uyuşuyor. |
| Tatlı su: `T90=20 °C`, yüzey | `T68=20,0048 °C`; **1482,358043351 m/s**. |
| Aynı su, 10 m; `ρ=1000`, `g=9,82` | Gösterge basıncı **0,982 bar**; **1482,520159338 m/s**. kg/kg→‰ ve Pa→bar dönüşümleri doğru. |
| Mackenzie: `T=40`, `S=40`, `D=10000 m` | **1718,7736 m/s**; [MBARI’nin 1718,774 denetim değeriyle](https://www.mbari.org/wp-content/uploads/2016/03/Sndspd.txt) uyuşuyor. Bu denetim girdisi geçerlilik alanının dışındadır. |
| Mackenzie: `T=25`, `S=35`, `D=1000 m` | **1550,7440275 m/s**. Aritmetik doğrulandı; bu değerin özgün makaledeki yayımlanmış denetim değeri olduğu doğrulanmadı. |
| Ainslie–McColm, tatlı su, 20 °C, yüzey | 115 kHz: **0,003089528064 dB/m**; 750 kHz: **0,131407148277 dB/m**; 1000 kHz: **0,233612708048 dB/m**. |
| Ainslie–McColm: `f=3 kHz`, `T=10`, `S=37‰`, `z=0,1 km`, `pH=8,1` | **0,211544957042 dB/km**. Üç terimin katsayıları, üs işaretleri ve birimleri erişilen [araştırma kaynağındaki denklemlerle](https://ecap.nat.fau.de/wp-content/uploads/2018/04/2013_Kiessling_Master.pdf) karşılaştırıldı. |
| 750→1000 kHz, tatlı su, 50 m | Çift yol soğurma farkı **10,22055598 dB**; eski katsayıyla ham enerji **10,52097 kat** yüksek kalır. |
| `c=1500`, `n=1200`, `sp=311` | Kutu genişliği **5,83125 mm**, menzil **6,9975 m**, dinleme **9,33 ms**. |
| `c=1500`, `n=1200`, `sp=2222` | Kutu genişliği **41,6625 mm**, menzil **49,995 m**, dinleme **66,66 ms**. |
| Evrişim sınırı karşı örneği | `n=1200`, `sp=311`, `tx=31 µs`; eko gecikmesi **9331,94375 µs**. İnce konum **4800,5**, dizi uzunluğu **4800**, çekirdek yarı genişliği **16**. Birim hücre enerjisinin son kutuya katkısı **0,68339100346** olmalı; `:146` katkıyı tamamen atıyor. |

Işın ağırlıklarında `dΩ=ΔθΔφ cosφ` bulunduğundan ışın sayısı artınca enerjiye ayrıca bir `N` çarpanı gelmiyor. Bağımsız orta nokta hesabında desen integrali 5000, 9996 ve 20000 ışında sırasıyla **0,008506964865**, **0,008506964518**, **0,008506964471** çıktı. Bu, normalizasyonu destekler; gerçek sahnedeki yakınsama kabulünün yerine geçmez.

İki yönlü yayılım ve soğurmanın enerji ağırlığına aktarımı doğru: `B⁴·σ·dΩ/(r² cosθ)` ve `10^(−2αr/10)`. Montaj dönüşüm sırası `R_nb·R_bs·R_kafa`; artan Ping360 açısı NED’de +x’ten +y’ye döner ve 400 gradyanda sarar.

## İskelete ya da diğer bölümlere notlar

- **W4-K18’de kabul edilen üç durum bulgu yapılmadı:** Ping360 damgası gönderim anı; sonarda `gurultu_artisi` reddi; sözleşme değişene kadar 1200 örnek üretip yakınsamayı 600 örnekte sınamak.
- Sade kademedeki hücre enerji toplamı ve kutu başına üstel benek, MP7 §3’te açıklanan sade modelle uyumlu. `test_enerji.cpp` yakınsaması yalnız belirtilen çözümsel sahne ve 600 örnek yapılandırmasını sınar.
- Temel sonar rastgeleliği 64 bit ping sayacını koruyor. Arıza çekilişlerinde 32 bit `seq` kullanımı A5 Ek D.4’ün açık tanımıdır; tek başına sayaç sarması hatası sayılmadı.
- W4-K1 değişikliğinde tuzluluk dönüşümü ve açık sabit ses hızının önceliği doğru. Yüzeyde değerlendirme belgelenmiş bir model kabulüdür.
- Mevcut testlerde havadaki dönüştürücü, yüksek `scan_start`, frekans değişiminde soğurma, motor süreli yeniden yapılandırma ve pencere dışından taşan çekirdek karşı örnekleri yok.
- **Erişim sınırı:** NPL’nin doğrudan ses hızı ve soğurma sayfaları zaman aşımı/502 verdi; ses hızı için NPL belgesinin erişilebilir kopyası kullanıldı. Mackenzie ve Ainslie–McColm özgün makalelerinin tam metinlerine erişilemedi; özgün metin doğrulaması yapılmış sayılmadı. Soğurma formülünün aritmetik uyumu, tatlı sudaki fiziksel doğruluğunun deneysel doğrulaması değildir.