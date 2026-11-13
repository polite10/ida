# sol incelemesi: 05-sensorler-ve-sonar

## Hüküm

Bölüm bu haliyle kabul edilmemeli. Sekiz başlıklı yapı, sorumluluk ayrımı ve araştırma düzeltmelerinin çoğunun aktarılması uygundur; ancak iskelet 0.4’ün sabit gövde referansı ve ölçüm zamanı kuralları uygulanmamış, bazı gecikmeler nedenselliği ihlal etmiş, IMU ve DVL denklemlerinde düzeltme gerektiren noktalar kalmıştır. Sonar tarafında eşevreli hesabın, çok yollu yayılımın, işçi sahnesinin sahipliğinin ve arıza bağımsızlığının uygulanabilir sözleşmeleri tamamlanmamıştır. Hesapların çoğu doğru ve ölçümden ayrılmıştır; bu durum modelin doğruluğunu veya gerçek zamanlı çalışabileceğini kanıtlamaz. Bölümün tamamı okundu; dosya değiştirilmedi, kurulum yapılmadı.

## Düzeltilmesi gerekenler

Aşağıdaki satır numaraları `docs/mimari/05-sensorler-ve-sonar.md` dosyasına aittir.

| # | Yer (başlık ya da satır) | Sorun | Doğrusu / önerilen düzeltme | Önem (kritik / önemli / küçük) |
|---|---|---|---|---|
| 1 | §3, 279–280; §8, 448 | Referans noktası hareketli CG kabul ediliyor; montaj kolu her örnekte buna göre değiştiriliyor. İskelet 0.4 §3.1 ve güncel 04 §2.2 bunun tersini söylüyor. | Sabit `O_b` kullanılsın. Montajlar doğrudan bu noktaya göre tanımlansın; yük değişimi `r_g` ve ataletleri değiştirsin. Hareketli referans düzeltmesi kaldırılmalı. | kritik |
| 2 | §2.2 tablosu; §3, 240–255 | Aralık sonu, geçerlilik anı ve pencere süresi tek sözleşmede birleştirilmemiş. IMU’da `t_meas` aralık sonu; basınç ve bataryada orta. `window_ns` ise A1’e aktarılacak alanlar arasında yok. | İskelet 0.4 uyarınca aralık sonu ve süresi taşınmalı; tek geçerlilik anı gerektiğinde orta hesaplanmalı. Kontrol sınırı, IMU aralık sonu ve ölçümün geçerlilik anı ayrı anlamlar olmalı. 02/06 eşlemesi güncellenmeli. | kritik |
| 3 | §2.2 tablosu, 82 ve 88; §2.5, 220 | Batarya 100 ms boyunca ortalanıyor, ortasına damgalanıyor, fakat 1 ms sonra teslim ediliyor: pencerenin gelecekteki 49 ms’si henüz oluşmamış olur. Basınç gecikmesi sütunu pencere ortasından başlayan yarım D1 süresini açıklamıyor. Kamera gecikmesi de pozlama sonrası tanımlanmasına rağmen ortadan başlatılıyor. | `t_teslim ≥ t_pencere_sonu` zorunlu olsun. Bataryada belirtilen okuma payıyla merkezden gecikme 51 ms; basınçta `T_D1/2 + T_D2 + T_okuma`; kamerada `T_pozlama/2 + L_sonrası` kullanılmalı. | kritik |
| 4 | §2.2, 85–86; §2.4.3, 189–193 | Ping1D ve Ping360 ölçüm zamanı gönderim anı seçilmiş. İskelet §3.1 akustik ölçümlerde yansıma anını zorunlu tutuyor. | `t_tx` ayrı alan olmalı. Ping1D mesafesinin geçerliliği yansıma anına bağlanmalı. Ping360’ta kutuların farklı yansıma zamanları olduğu açıkça temsil edilmeli; tek profil damgasının anlamı iskelet düzeyinde netleştirilmeli. | kritik |
| 5 | §2.2, 69; §8, 444–445 | En yakın sınıra bağlama yerine iki kuralla da çalışılabileceği söyleniyor; hata sınırı bir adım veriliyor. Ayrıca 04’ün hâlâ 1 ms ve sonraki sınırı kullandığı iddiaları artık yanlış. | Bağlayıcı kural en yakın sınırdır; sapma en çok 0,625 ms. Güncel 04 §2.11 bunu zaten uyguluyor. Eski uyumsuzluk kayıtları güncellenmeli. Tamamlanma/teslim olaylarının fiziksel süreden önce çalışmaması ayrıca tanımlanmalı. | önemli |
| 6 | §2.3.1, 107–110 | Özgül kuvvetin gövde formülü doğru; fakat çıktı sensör çerçevesinde denmesine rağmen `f_s = R_bsᵀ f_b` dönüşümü yazılmamış. | Hata modelindeki ivme girdisi açıkça `R_bsᵀ f_b` olmalı. Dünya dönüşü sınamasında da jiroskop beklentisine `R_bsᵀ` eklenmeli. Kimlik olmayan montajla kabul testi bulunmalı. | önemli |
| 7 | §2.3, 99; IMU parametre tablosu | Üretici gürültü yoğunluğu doğrudan Allan/Kalibr `N` kabul edilip √2 farkı sonraya bırakılıyor; buna rağmen sayısal çıktı gürültüsü kesin hesaplanıyor. | Konvansiyon şimdi sabitlenmeli. TDK’nin 100 Hz bantta verdiği RMS değerleri, yoğunluğun tek taraflı olduğunu destekliyor. Dikdörtgen aralık ortalaması için `σ = ASD/√(2Δt)`; Kalibr biçiminde `N = ASD/√2` kullanılmalı veya farklı seçim açıkça varsayım olarak adlandırılmalı. [TDK veri sayfası](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/ds-000347-icm-42688-p-v1.6.pdf), [Kalibr modeli](https://raw.githubusercontent.com/wiki/ethz-asl/kalibr/IMU-Noise-Model.md). | önemli |
| 8 | §2.3, 95–97 | Sabit `σ_∞` ile `τ → ∞` limitinin rastgele yürüyüş olduğu yanlış. Bu limitte süreç donar. | Sürekli sürücü yoğunluğu `q = 2σ_∞²/τ` tanımlansın. Rastgele yürüyüş limiti ancak `q` sabit tutulursa elde edilir. Başlangıç bias dağılımı ve durağanlığa geçiş de belirtilmeli. | önemli |
| 9 | §2.3.1–2.3.7; §6 | Bütün sensörlerin gürültü/bias modelleri tamamlanmış değil. DVL’de `σ_b(r)` tanımsız; basınç sıcaklık kaymasının yasası yok; manyetometrede vaat edilen sıcaklık terimi denklemde yok; ADC/PWM gürültüsünün bant genişliği ve zaman modeli belirsiz. Tablodaki jiroskop doğrusalsızlığı da ölçüm denkleminde uygulanmıyor. | Her kanal için ayrık örnek σ’sı mı, sürekli yoğunluk mu kullanıldığı; filtre/pencere; bias yasası; başlangıç; zorunlu profil değerleri tanımlanmalı. Sabit dönüşüm yapılandırmasının örnek gürültüsü, çıktı hızı değişti diye otomatik ölçeklenmemeli. Seçilen süzgecin ayrık güncellemesi ve sırası verilmelidir. | önemli |
| 10 | §2.3.2, 128–130; §3, 299 | Manyetik alan T, `B_n` nT verilmiş; dönüşüm açık değil. İtici akımının yön değiştirince işaret değiştirdiği varsayılıyor; güncel 04 modeli ise negatif olmayan besleme akımı üretiyor. | İç alan birimi T olmalı; nT girişinde `10⁻⁹` dönüşümü uygulanmalı. Besleme akımı ile motor yönü/faz akımı ayrılmalı. `K_k I_k` yaklaşımının yön bağımlılığı ayrı parametreyle tanımlanmalı. | önemli |
| 11 | §2.3.4, 143 | CEP bütçesinin %90/%10 paylaşımının varyansa mı standart sapmaya mı uygulandığı belirsiz. Standart sapmalar paylaştırılırsa 2 m CEP korunmuyor. | Varyans paylaşımı belirtilsin: `σ_GM = √0,9 σ_toplam`, `σ_beyaz = √0,1 σ_toplam`. Düşey hata ve hız hatasının zaman modeli de açık olmalı. | önemli |
| 12 | §2.3.4, 141–142 | Su kalınlığı yalnız uydu kaybını sürüyor; fix kaybolmadan önce doğruluk bozulması tanımlanmamış. Uydu sayısının nasıl üretildiği ve yeniden edinim sayacının tekrar suya girildiğinde ne yaptığı yok. Üreticinin tipik 2/24 saniyesi dağılımın fiziksel alt sınırına çevriliyor. | Uydu havuzu, zaman korelasyonu, kalite bozulması, suya yeniden giriş ve edinim ilerlemesinin sıfırlanması/durması tanımlanmalı. 2/24 s alt sınırları üretici garantisi olarak sunulmamalı; seçilecek dağılım `[T/K]` olmalı. [u-blox test koşulları](https://content.u-blox.com/sites/default/files/NEO-M9N-00B_DataSheet_UBX-19014285.pdf). | önemli |
| 13 | §2.3.5, 149 | Piston tipi varsayımında ses hızı ölçeği ters: `c_gerçek/c_ayar` kullanılıyor. | Küçük Doppler yaklaşımında `Δf ≈ 2f₀v/c_gerçek`; cihazın ürettiği hız `v̂ ≈ v·c_ayar/c_gerçek` olur. `κ=1` için oran ters çevrilmeli. Dönüştürücü tipi bilinmiyorsa bu yapısal belirsizlik korunmalı. [FFI/Kongsberg çalışması, §5.2](https://www.navlab.net/Publications/DVL_Velocity_Aiding_in_the_HUGIN_1000_Integrated_Inertial_Navigation_System.pdf). | önemli |
| 14 | §2.3.5, 147–151 | Mod tablosu irtifa aralığıdır; geçerlilikte doğrudan eğimli `r_i` ile karşılaştırılıyor. Dört ışının azimutları, kimlikleri, `altitude` hesabı ve histerezis eşikleri yok. Yedi alt ışından en yakını seçilirken hız izdüşümünün hangi doğrultuya ait olduğu belirsiz. | Işın vektörleri/kimlikleri ve ağırlıkları verilmeli; irtifa ile eğimli menzil ayrılmalı. Geçerli ışınlardan çözüm ve kilit kaybı kuralları tam tanımlanmalı. [Üretici mod tablosu](https://docs.waterlinked.com/dvl/range-mode/). | önemli |
| 15 | §2.3.5, 149–153 | Dinamik sonar sahnesinden kesişim alınıyor ama hızda yansıtıcı yüzeyin hızı çıkarılmıyor. Yansıma anında olay çalıştırılıp gönderim zamanı geriye doğru türetiliyor; farklı ışın menzilleri ve hiç kilit bulunamaması durumunda zaman tanımsız. | Sabit dip maskesi veya yüzeye göre bağıl hız kullanılmalı. Gönderim, ışınların yansıması, rapor ve teslim olayları nedensel zincir oluşturmalı. Kilitsiz raporun periyodu, damgası ve yeniden arama davranışı tanımlanmalı. `r̄/c` yaklaşımının sınırı belirtilmeli. | önemli |
| 16 | §2.3.3, 135; §7 basınç testi | 18,08 ms maksimum dönüşüme dayanılırken pencere 17,5 ms seçilmiş. Bu seçimin fiziksel dönüşüm süresi mi yoksa yalnız ızgara yaklaşımı mı olduğu belirsiz. | Maksimum süreyi güvenle temsil etmek isteniyorsa tamamlanma erken olmamalı; 15 adım = 18,75 ms kullanılabilir. Tipik süre modelleniyorsa kaynak/varsayım ayrılmalı. Gürültü testi açılış ofseti ve sıcaklık etkisi çıkarılmış artık üzerinde yapılmalı. | önemli |
| 17 | §2.4.2, 180–185 | Fiziksel basınç ile yoğunluk `I=abs(p)²` diye eşitleniyor; empedans ve referanslar yok. `B` genlik deseni denmesine rağmen yoğunlukta `B²` kullanılıyor; bunun tek yönlü mü birleşik gidiş-dönüş deseni mi olduğu açıklanmamış. | Normalize büyüklük kullanılıyorsa adı/birimi açıkça yazılmalı; fiziksel yoğunlukta RMS basınç için `I=p_rms²/(ρc)` kullanılmalı. Desen `B_tx`, `B_rx` olarak ayrılmalı; aynı tek yönlü genlik deseninde yoğunluk ağırlığı `abs(B_tx B_rx)²=B⁴` olur. dB referansları ve sıfır tabanı tanımlanmalı. | önemli |
| 18 | §2.4.2–2.4.3; §2.6; §6 yakınsama | Faz saçıcıya ait deniyor, RNG anahtarı ise ping/ışın numarası. Fiziksel olarak sabit yüzey parçasının fazının ardışık pinglerde ve ışın sayısı değişince nasıl korunduğu yok. Tek gerçekleşimde kutu yoğunluğunun %5 yakınsaması güvenilir ölçüt değil. | Yüzeye bağlı kalıcı saçıcı alanı veya açıkça istatistiksel, zaman korelasyonlu yaklaşım seçilmeli. Yakınsama ortalama enerji, varyans ve uzaysal/zamansal korelasyon üzerinde sınanmalı. Dur-ve-atla yaklaşımının yalnız kutu hatasıyla eşevreli doğruluk sağladığı ima edilmemeli. | önemli |
| 19 | §2.4.3, 192; §5, 374 | Dönüş yolunun görünür olması, speküler yansıma yasasını sağladığını tek başına göstermez. Yolun geometrik yayılımı, her bacağın soğurması ve sekme sayımının anlamı eksik. Yük hesabı görünürlük sorgularını eksik sayabilir. | Geçerli yolun geometrisi, speküler/difüz etkileşim türü, toplam uzunluğu ve katsayıları tanımlanmalı. Doğrudan yolun `r⁻⁴` yasası çok yollu duruma yalnız toplam uzunluğun yarısı konarak genellenmemeli. Kesişim ve dönüş sorguları ayrı sayılmalı. | önemli |
| 20 | §2.4.3, 193–194; §5 | Darbe zarfı yalnız tam kademede var. Sade kademede `tx_duration` ölçümün menzil yayılımını etkilemiyor. Son 8 bit değerin genlik, yoğunluk veya logaritmik alıcı çıktısı olduğu da tanımlı değil. | Her kademede darbe/alıcı bant genişliğinin menzil yanıtı korunmalı; eşevresiz yaklaşık çekirdek kullanılabilir. TVG, gürültü, log dönüşümü ve kuantalama sırası belirlenmeli. Rayleigh testi kuantalama/log dönüşümü öncesindeki uygun büyüklüğe uygulanmalı. [Choi vd., §2.3–2.4](https://www.frontiersin.org/journals/robotics-and-ai/articles/10.3389/frobt.2021.706646/full). | önemli |
| 21 | §2.4.2, 184 | Ainslie–McColm çağrısında pH girdisi yok; katsayılar, giriş birimleri ve geçerlilik alanı bölümde verilmemiş. Tatlı su uygulaması doğrulanmamışken varsayılan model olarak seçiliyor. | Tam denklem/sürümlü kaynak, `f` kHz ve derinlik km dönüşümleri, pH ve ortam sınırları yazılmalı. Tatlı su için kullanılan saf-su yaklaşımı `[T]` olarak sınırlandırılmalı. Bu incelemede de özgün makale/NPL tam içeriğine erişilemedi; fiziksel doğrulama kapanmış sayılmamalı. | önemli |
| 22 | §2.4.4, 199–206; §7 Ping360 testi | 1 m üretici taraması aynı 1200 örnekli yapılandırmayla sınanabilecekmiş gibi bırakılmış. Oysa minimum `sample_period=80` ile 1200 örnek en az 1,8 m pencere verir. | Menzil–örnek sayısı–tamsayı periyot birlikte doğrulanmalı. Üretici hız testlerinde kullanılan bilinmeyen ayarlar açık kalmalı; geçerli referans yapılandırması kurulmadan süre eşleşmesi kabul ölçütü olmamalı. [Ping360 protokolü](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/). | önemli |
| 23 | §2.4.4, 200–205; §3 `on_command` | Tek ping, otomatik tarama, hareket ettirip göndermeme, durdurma ve komut çakışması davranışları tanımsız. `T_motor(num_steps)` mutlak açı komutundaki gerçek kafa hareketini kapsamaz. “Reddet ya da sınırla” uygulanabilir tek davranış değildir. | Tam komut/durum tablosu yazılmalı; hedef açı, mevcut açı, yön/sector dönüşü, `transmit`, bekleyen komut ve etkinleşme sırası belirlenmeli. 06’nın CONFIG alanları gerçek komut anlamlarını karşılamalı. [Üretici komut tanımları](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/). | önemli |
| 24 | §2.4.5; §7 Ping1D testi | Üreticinin %0,5 menzil çözünürlüğü, mesafe doğruluk sınırına dönüştürülmüş. Üretici bunu waterfall aralığına bağlı çözünürlük olarak ayırıyor. Ayrıca `T_dinleme` hesabında `scan_start` kaynaklı uçuş süresi açık değil. | Mesafe doğruluğu için ayrı kaynak veya `[T]` kabul eşiği kullanılmalı. Dinleme/çevrim süresi en uzak dinlenen mesafeyi kapsamalı; pencerenin yalnız uzunluğu kullanılmamalı. [Ping Sonar özellikleri](https://bluerobotics.com/store/sonars/echosounders/ping-sonar-r2-rp/). | önemli |
| 25 | §2.4.1, 169–171; §3, 281 | Değişmez `PoseSet`, paylaşılan Embree sahnesini değişmez yapmaz. Önceki sonar işi sorgularken sonraki ping/DVL için instance dönüşümü veya dalga ağı güncellenebilir. | İş başına değişmez sahne sürümü, işçiye özel üst sahne veya sorgu/güncelleme evrelerinin sıralanması tanımlanmalı. Embree değişiklik, commit ve sorguların eşzamanlı yapılmasını yasaklıyor. Bekleyen iş sayısı ve reset sonrası eski sonuçların iptali de sınırlanmalı. [Embree sözleşmesi](https://raw.githubusercontent.com/RenderKit/embree/v4.4.1/doc/src/api/rtcCommitScene.md). | kritik |
| 26 | §2.1, 62–63; §2.6; §4, 322–333 | “Ölçüm zinciri arıza bilmez” ve “hepsi ölçümden sonra” ifadeleri, model içi `beam_alive`/`sector_gain` kancalarıyla çelişiyor. Ayrı RNG, model durumu ve çekiliş sayısının bağımsızlığını garanti etmez. | Arızasız temel ölçüm ve ham ışın verisi koşulsuz üretilsin; kısmi arıza ayrı kopya üzerinde çözüme uygulansın. Durumlu cihaz arızaları ayrı sınıf olsun. RNG anahtarında kanal, bileşen ve çekiliş alt indeksi tanımlanmalı. | önemli |
| 27 | §3 arıza denetimi; §4 arıza tablosu | Aynı anda birden fazla arızanın uygulama sırası, gürültü artışında `k≥1`, birim/kanal denetimi ve donma sonrası davranış tanımlanmamış. Motor takılması gibi durumlu arızalarda “arıza biter bitmez temel diziyle aynı” şartı genel olarak sağlanamaz. | Sabit bileşim sırası ve parametre doğrulaması eklenmeli. Bit eşitliği yalnız temel ölçüm akışı için istenmeli; arızalı cihaz durumu için toparlanma sözleşmesi yazılmalı. Donmuş damga ve sıra bozulması testleri genel artan-zaman kuralından açıkça ayrılmalı. | önemli |
| 28 | §3, 255 ve 297; §2.6, 226 | Üretim `seq` numarası A1 alan listesinden çıkarılmış; 06’nın başlığı yalnız teslim adımının `step_seq` numarasını taşıyor. Dolayısıyla sensör kaybının `seq` boşluğu bırakacağı sözleşmesi uçtan uca sağlanmıyor. `drain` sırası da 06’nın sırasıyla aynı değil. | Üretim sırası, teslim adımı ve MAVLink çerçeve sırası ayrılmalı; A4’e kadar eşleme yapılmalı. 05 teslim kuyruğunun, 06 parti sıralamasının sahibi olsun; sıralama dönüşümü açıkça yazılsın. | önemli |
| 29 | §5–§6 | Kalite kademesi ile ölçüm sonucunda seçilen yedek birbirine karışıyor. Örnek sayısını azaltma ve periyodu büyütme, yalnız iç hesap ayrıntısını değil profil çözünürlüğünü ve seri iletim süresini de değiştiriyor. | İskelet §6’daki kademe/yedek ayrımı korunmalı. Tamsayı periyot yuvarlama, sınır aşımı, gerçekleşen menzil ve bunun zamanlamaya etkisi 05/06’da tek sözleşme olmalı. | önemli |
| 30 | §7, 403–436 | Aynı veri kümesi hem uydurma hem kabul için kullanılıyor. Genel gürültü testleri bias/sıcaklık/kuantalamayı ayırmıyor; yalnız iki tohum istatistiksel güvence sağlamıyor. Göreli tolerans sıfıra yakın değerlerde tanımsız. | Kalibrasyon ve bağımsız kabul verileri ayrılmalı. Testlerde etkiler teker teker açılmalı; örnek süresi, başlangıç geçişi ve güven aralıkları belirtilmeli. Mutlak+bağıl tolerans kullanılmalı. Sonar için tek gerçekleşim yerine topluluk istatistikleri sınanmalı. | önemli |
| 31 | Etiketler; §8 | Bazı yapısal bilinmezler kalibrasyon parametresi sayılıyor: üç ışınla çözüm desteği ve dönüştürücü tipi kalibrasyonla belirlenecek sürekli katsayılar değildir. WMM alanı ve seçilecek ADC bit sayısı da kendiliğinden `[K]` değildir. | Algoritma/cihaz yeteneği, yapılandırma, çevresel girdi ve deneyle uydurulacak katsayı ayrı sınıflandırılmalı. TVG, malzeme saçılımı, sıcaklık katsayıları ve gecikme dağılımları için `[K]` uygundur; seçilmiş varsayılanlar ayrıca `[T]` taşımalıdır. | önemli |

## Yeniden hesaplananlar

Aşağıdakiler bağımsız aritmetik ve denklem denetimleridir; performans ölçümü değildir.

**Zaman ızgarası**

| Hesap | Sonuç ve karşılaştırma |
|---|---|
| Fizik | `1/800 = 1,25 ms`; doğru. |
| Kontrol | 400 Hz → 2,5 ms → 2 fizik adımı; 200 Hz → 5 ms → 4 adım. IMU pencere tablosu yedek hızda 5 ms’ye geçmeli. |
| Kamera | `800/30 = 26⅔` adım. İlk örnek sınırları `27,53,80,107,133,160…`; aralıklar `27,26,27…`. |
| DVL | 15 Hz → `53⅓` adım; doğru. |
| En yakına bağlama | En çok `1,25/2 = 0,625 ms` sapma. “Bir adım” gevşek sınır; sonraki sınır seçeneği bağlayıcı kurala aykırı. |
| “Kayma 0 ns” | Rasyonel fazda birikimli sürüklenme olmaması doğrudur; her örneğin ideal zamana tam eşit olması değildir. Örneğin 30 Hz’in 10⁶’ncı örneğinde en yakın sınır farkı yaklaşık 0,416667 ms’dir. |

**IMU**

Bölümdeki SI dönüşümleri doğru:

- `0,0028 °/s/√Hz = 4,88692·10⁻⁵ rad/s/√Hz`.
- `65/70 µg/√Hz = 6,37432·10⁻⁴ / 6,86465·10⁻⁴ m/s²/√Hz`.
- `10 °/h = 4,84814·10⁻⁵ rad/s`.
- `±0,5 °/s = ±0,00872665 rad/s`; `±20 mg = ±0,196133 m/s²`.
- Sıcaklık katsayıları: `8,72665·10⁻⁵ rad/s/°C` ve `0,001471 m/s²/°C`.

Bölümün seçtiği `N/√Δt` konvansiyonuyla 400 Hz sonuçları:

| Kanal | Yeniden hesap | Bölüm |
|---|---:|---:|
| Jiroskop | `9,77384·10⁻⁴ rad/s` | ≈ `9,8·10⁻⁴`; aritmetik doğru |
| İvme X/Y | `0,0127486 m/s²` | ≈ `0,013`; doğru yuvarlama |
| İvme Z | `0,0137293 m/s²` | Ayrı verilmemiş |

Ancak üretici yoğunluğunun tek taraflı yorumu ve ideal aralık ortalamasıyla bunlar sırasıyla **`6,91115·10⁻⁴`, `0,00901465`, `0,00970809`** olur. TDK’nin 100 Hz bantta `0,0028√100=0,028 °/s RMS` vermesi bu ayrımı görünür kılıyor. [TDK, tablolar 1–2](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/ds-000347-icm-42688-p-v1.6.pdf).

16 bit ve seçilen aralıklarda ideal kuantalama:

- Jiroskop: `1000/65536 = 0,0152588 °/s ≈ 2,66316·10⁻⁴ rad/s`.
- İvmeölçer: `8g/65536 = 0,00119710 m/s²`.

GM denkleminin durağan varyans denetimi:

\[
\phi=e^{-\Delta t/\tau},\quad
q_d=\sigma_\infty^2(1-\phi^2),\quad
\frac{q_d}{1-\phi^2}=\sigma_\infty^2.
\]

Bu kısım doğrudur. 400 Hz’te:

| Süreç | `φ` | Yenilik standart sapması |
|---|---:|---:|
| Jiroskop, `τ=1000 s`, `σ∞=10 °/h` | `0,9999975000` | `1,08407·10⁻⁷ rad/s` |
| İvme, `τ=300 s`, `σ∞=0,001 m/s²` | `0,9999916667` | `4,08247·10⁻⁶ m/s²` |

Sabit `σ∞` ile `τ→∞` olduğunda yenilik sıfıra gider. Rastgele yürüyüş için `σ∞²=qτ/2` tutulmalıdır.

Özgül kuvvet denklemindeki `ω×v`, teğetsel ve merkezcil kaldıraç terimleri ile yerçekiminin çıkarılması, sabit gövde referansında doğrudur. Eksik işlem sensör çerçevesine dönüşümdür.

**Basınç ve derinlik**

\[
d_{\rm görünür}=\frac{p_{\rm abs}-p_{\rm atm}}{\rho g}
=d+\frac{p_{\rm dalga}}{\rho g}
+C_p\frac{v_r^2}{2g}+\frac{b+n}{\rho g}.
\]

`ρ=1000 kg/m³`, `g=9,80665 m/s²` için:

| Hesap | Sonuç |
|---|---:|
| `0,2 mbar` | 20 Pa; doğru |
| 20 Pa’nın derinlik karşılığı | 2,039 mm; “≈2 mm” doğru |
| ±50 mbar ofset | ±0,5099 m; çözünürlükle karıştırılmamalı |
| 10 Pa kuantalama | 1,0197 mm |
| `v=1 m/s`, `C_p=1` dinamik basınç | 500 Pa → 5,099 cm görünür derinlik |
| İki maksimum dönüşüm | `2×18,08=36,16 ms` → 27,6549 Hz |
| Buna 1 ms okuma eklenince | 26,9107 Hz |
| 14 fizik adımı | 17,5 ms |
| Güvenli yukarı bağlanan 15 adım | 18,75 ms; iki dönüşüm+1 ms → 25,9740 Hz |

25 Hz seçimi her iki muhafazakâr bütçeye sığıyor. Sorun hız seçiminden çok pencere ve damga sözleşmesidir.

04’ün basınç sorgusunu kullanmak dalga basıncını ikinci kez eklemeyi önler. Örneğin derin-su yaklaşımında 0,1 m genlikli, 5 m dalga boylu bir bileşenin 1 m derinlikteki görünür derinlik genliği `0,1e^(−2π/5)=0,02846 m` olur. Bu yalnız seçilmiş analitik örnektir; sonlu derinlikte 04’ün `F_p` işlevi kullanılmalıdır.

**GNSS**

Dairesel izotropik normalde:

\[
CEP=\sigma\sqrt{2\ln2},\qquad
\sigma=\frac{2}{\sqrt{2\ln2}}=1,69864\ {\rm m}.
\]

Dönüşüm doğru. Standart sapmaları %90/%10 paylaştırmak:

\[
CEP_{\rm çıkan}=2\sqrt{0,9^2+0,1^2}=1,81108\ {\rm m}
\]

üretir. Varyans paylaşımında doğru değerler `σ_GM=1,61147 m`, `σ_beyaz=0,537158 m` olur.

Verilen lojistik eğri:

| Su kalınlığı | İzleme olasılığı |
|---|---:|
| 0 mm | 0,997527 |
| 30 mm | 0,5 |
| 50 mm | 0,017986 |

Eğri matematiksel olarak doğru; tuzluluk, uydu geometrisi ve zaman korelasyonu eklenmeden evrensel fizik modeli sayılmaz.

**DVL**

Düz taban ve 22,5° ışın açısında:

\[
r=\frac{h}{\cos22,5^\circ}=1,082392h.
\]

İrtifa tablosunu doğrudan `r` ile kullanmak yaklaşık %8,24 geometrik fark yaratır.

Simetrik dört ışın ve eşit bağımsız ışın gürültüsünde örnek kovaryans hesabı:

\[
\sigma_x=\sigma_y=
\frac{\sigma_b}{\sqrt2\sin22,5^\circ}
=1,84776\sigma_b,\qquad
\sigma_z=\frac{\sigma_b}{2\cos22,5^\circ}
=0,541196\sigma_b.
\]

Bu, `fom` ve kovaryans sınaması için analitik referans olabilir; A50’nin iç algoritmasını doğrulamaz.

Ses hızı örneği: `c_gerçek=1500`, `c_ayar=1475 m/s`, gerçek hız 1 m/s ise piston varsayımında çıktı **0,983333 m/s** olmalıdır. Bölümün oranı **1,016949 m/s** verir.

Yansıma damgasından sonraki `r/c + işlem + iletim` ilişkisi doğrudur; üreticinin zaman tanımı bunu destekliyor. [Water Linked API](https://docs.waterlinked.com/dvl/dvl-json-protocol/).

**Ping360 örnekleme ve tarama**

\[
\Delta r=\frac{c\,s\,25{\rm ns}}2,\qquad
T_{\rm dinleme}=n\,s\,25{\rm ns},\qquad
R=n\Delta r.
\]

`c=1500 m/s` için:

| Yapılandırma | Yeniden hesap | Karşılaştırma |
|---|---|---|
| 7 m, 1200 örnek | `Δr=5,83333 mm`, `T=9,33333 ms`, akustik sınır `107,1429 ping/s` | Bölüm doğru |
| Aynı menzilin tamsayı periyodu | İdeal `s=311,111…`; `s=311` ile `R=6,9975 m` | Yuvarlama sözleşmesi eksik |
| 50 m, 1200 örnek | `Δr=41,6667 mm`, `T=66,6667 ms`, akustik sınır 15 ping/s | Bölüm doğru |
| `s=2222` ile | `R=49,995 m` | Gerçekleşen menzil raporlanmalı |
| 1 m, 1200 örnek | İdeal `s=44,444… <80` | Geçersiz yapılandırma |
| 1200 örnek, minimum `s=80` | `R=1,8 m` | 1 m referansında örnek sayısı azaltılmalı |
| 1 m, 600 örnek, `s=89` | `R=1,00125 m` | Geçerli örnek yapılandırma |
| Açısal adım | `360/400=0,9°=π/200 rad` | Doğru |

Üreticinin tarama sürelerinden, 400 konum varsayımıyla:

| Tam tarama | Açı başına süre | Profil hızı |
|---|---:|---:|
| 33 s | 82,5 ms | 12,1212 Hz |
| 3,42 s | 8,55 ms | 116,959 Hz |
| 4,17 s | 10,425 ms | 95,9233 Hz |
| 4,26 s | 10,65 ms | 93,8967 Hz |

Süreler üretici tablosuyla uyuşuyor; bilinmeyen örnekleme ayarlarını belirlemiyor. [Ping360 özellikleri](https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/).

115200 baud, 8N1 hesabı:

\[
1200\times10/115200=104,1667\ {\rm ms},
\quad
400\times104,1667{\rm ms}=41,6667\ {\rm s}.
\]

Bölüm doğru; çerçeve ek yükü bu alt sınırı büyütür. Bu yapılandırma ardışık iletimle 33 saniyeyi üretemez.

50 m’de 1 m/s hareketin dinleme boyunca yer değiştirmesi `0,066667 m`; 1200 kutuda **1,6 kutu**. Bölüm doğru. Bununla birlikte bu, eşevreli faz hatasının küçük olduğunu göstermez: 750 kHz’te dalga boyu 2 mm; 0,1 mm menzil değişimi çift yönlü fazı 36° değiştirir.

**Sonar yükü ve akustik dönüşümler**

- `5000×3=15.000 sorgu/ping`; 100 Hz’te **1,5 milyon sorgu/s**: doğru.
- `15.000×1200=18 milyon karmaşık katkı/ping`: doğru.
- Katkı başına varsayılan en az dört gerçek işlemle **72 milyon gerçek işlem/ping**: aritmetik doğru; faz üretimi, geometri ve veri erişimi dışarıda.
- Üç kesişim bacağına iki dönüş görünürlük sorgusu eklenen örnek düzen `5000×5×100=2,5 milyon sorgu/s` eder. Gerçek sayı yol üretim algoritmasına bağlıdır.
- `ceil(1200×2° radyan)=42`, `ceil(1200×25° radyan)=524`; çarpımları **22.008**: doğru.
- HoloOcean ifadesinin argümanı `(1200/1199)cos1°=1,000681596`: gerçek `arccos` yok; bölüm doğru biçimde kullanmıyor.

“Birkaç milyon basit işlem” ise darbe çekirdeği, katkı sayısı ve işlem tanımı verilmeden yeniden üretilebilir bütçe değildir. M11 gereği sürüyor.

Doğrudan nokta saçıcıda `p∝r⁻²`, `I∝r⁻⁴` ile iki yönlü soğurma çarpanları doğru:

\[
A_p=10^{-2\alpha r/20},\qquad
A_I=10^{-2\alpha r/10}=A_p^2.
\]

Ham rapordaki formülün **aritmetiğini**, `S=0`, `T=20°C`, derinlik 0 kabulüyle yeniden yaptım:

\[
\alpha=0,00049f_{\rm kHz}^2e^{-20/27}/1000.
\]

| Frekans | Hesaplanan α | 50 m menzilde çift yönlü kayıp |
|---|---:|---:|
| 750 kHz | `0,131407 dB/m` | `13,1407 dB`; genlik `0,220275`, yoğunluk `0,0485209` |
| 115 kHz | `0,00308953 dB/m` | `0,308953 dB` |

Bölümdeki yaklaşık değerler doğru. Ancak **özgün formülün ve tatlı su uygulamasının birincil kaynaktan doğrulaması kapanmadı**: [NPL sayfası](https://resource.npl.co.uk/acoustics/techguides/seaabsorption/physics.html) zaman aşımı verdi; [Ainslie–McColm özgün makalesine](https://doi.org/10.1121/1.421258) erişilemedi. Aritmetik uyum fiziksel geçerlilik kanıtı değildir.

**Diğer denklemler ve değerler**

- `dA=r²dΩ/cosθ` düzlemsel yerel yüzey yaklaşımında doğrudur; sıyıran geliş ve sonlu üçgen alanı için sayısal sınır gerekir.
- Bağımsız gürültü eklemesinde `σ_nom√(k²−1)` toplam standart sapmayı `kσ_nom` yapar; yalnız `k≥1` ve aynı kanalın gürültü tanımı için.
- Üstel yoğunluk çarpanı, ortalaması 1 seçilirse ortalama enerjiyi korur; ölçek belirtilmemiştir.
- PSM ters dönüşümleri `V_analog=V_bara/11`, `I_analog=0,330+I/37,8788` olur. 100 A’da yaklaşık 2,970 V: tutarlı. 100 A sürekli değer değildir. [PSM özellikleri](https://bluerobotics.com/store/comm-control-power/control/psm-asm-r2-rp/).
- Termal denklem boyutsal olarak doğrudur: `C` J/K, `k` W/K, `P_e` W olmalı. Sayısal değer ve ayrık güncelleme verilmediğinden nicel doğrulanamaz.
- 1000/5000 ışın, yedi DVL alt ışını, 64 kayıtlık kuyruk, 60° geliş sınırı, 0,5–3 s arama, sıcaklık/kalibrasyon katsayıları ve kabul eşikleri tasarım varsayımıdır. Bunların “doğru sonucu” kaynak veya ölçüm olmadan hesaplanamaz; çoğunun `[T/K]` etiketi bunu doğru yansıtıyor.

## İskelete ya da diğer bölümlere notlar

**İskelet ve sınırlar**

Sekiz ana başlık mevcut. A1–A7 numaralarında yanlış arayüz ataması bulmadım. Fizik/çevre 04’e, taşıma 06’ya, görüntü üretimi 07’ye, dosya biçimleri/kayıt 08’e bırakılmış. C/C++, başsız sim sunucusu, yer gerçeğinin çekirdekten saklanması ve mevcut kart odağı korunuyor; Rust, tedarik veya yarışma kuralından gereksinim türetilmiyor.

Buna karşılık bölümün girişindeki iskelet sürümü 0.3 atfı güncel değildir. §8’de “bölüm iskelete uydu” hükmü de yukarıdaki referans ve zaman çelişkileri nedeniyle doğru değildir.

İskelet §3.1, **menzil boyunca farklı yansıma anları içeren bir sonar profili** için tek damganın anlamını ayrıca açıklamalıdır. Bu açıklık giderilene kadar bölüm gönderim zamanını tek taraflı olarak ölçüm zamanı seçemez.

**Komşu belgeler**

| Belge | Güncel durum / gerekli eşleme |
|---|---|
| `docs/mimari/04-simulator-cekirdegi.md` | Artık 1,25 ms, en yakın sınır, sabit `O_b`, açık `ν̇/a_n` ve paylaşımlı snapshot sahipliği var. 05 §8’in bu konulardaki eski eksiklik iddiaları güncellenmeli. |
| Aynı belge §3 | `water_pressure`, tuzluluk, tek seferlik sensör olayları ve sensörün istediği sıcaklık alanları hâlâ ortak sözleşmeye eklenmeli. İtici akımları “işaretli” değildir. |
| `docs/mimari/02-ortak-cekirdek.md` | Basınçtan derinliğe dönüşümün yeri doğru biçimde çekirdekte. Dalga basıncı gerçek sensör çıktısında kalmalı; kestiriciye yer gerçeğiyle çıkarılmış derinlik verilmemeli. Aralık sonu/orta anlamı, montaj çerçevesi, alım zamanı ve üretim sırası ortaklaştırılmalı. |
| `docs/mimari/06-protokol-ve-hil.md` | BARO dönüşüm sonu, BATT anlık örnek, Ping gönderim zamanı tanımları 05/iskeletle uyuşmuyor. Pencere alanları, sensör üretim sırası, CONFIG komut anlamları ve örnekleme nicemlemesi tamamlanmalı. |
| Aynı belge §2.4 | Önceki IMU’dan hemen sonra gönderilecek toplu kısmın gelecekte hazır olacak sonar verisini içermesi sorunu sürüyor. 05’in modellenmiş hazır zamanı bunu çözmüyor; üretim, kuyruğa alma ve çekirdeğe teslim ayrı belirlenmeli. |
| `docs/mimari/inceleme/01-sol.md`, `02-sol.md`, `06-sol.md` | Alım zamanı, IMU’dan bağımsız zaman çerçevesi ve IMU kaybında SIL ilerlemesi bulguları açık. 05’in kayıp/donma testleri bu ortak yol düzelmeden uçtan uca çalışamaz. |
| `docs/mimari/inceleme/04-yanit.md` | 04’ün giderdiği bulgular, 05’te hâlâ güncel eksiklik gibi listelenmemeli. |
| 03, 07, 08, 09 | Mimari dosyaları henüz yok. Kamera pozlama/teslim sözleşmesi, bağımsız kabul verileri, kayıt kapsamı ve algılamaya taşınacak zaman anlamı tamamlanmış sayılmaz. |

**00 belgesinin 05 düzeltmeleri**

| Madde | Aktarım durumu |
|---|---|
| 05.1 | Kesin GM ayrıklaştırması alınmış; yeni eklenen rastgele yürüyüş limiti yanlış. |
| 05.2 | Baud hesabı ve açı başına zamanlama alınmış; yapılandırma ve komut davranışı eksik. |
| 05.3 | DVL yansıma damgası ve kalan `r/c` alınmış; nedensel olay üretimi ve kilitsiz durum tamamlanmamış. |
| 05.4 | Keskin 3 cm kapısı kaldırılmış; kalite bozulması ve zaman korelasyonu eksik. |
| 05.5 | Test koşulları aktarılmış; tipik sürelerin dağılım alt sınırına çevrilmesi yeniden düzeltilmeli. |
| 05.6 | USB’nin deterministik teslim sağladığı yanlış tekrarlanmıyor. Gerçek alım ve modellenmiş teslim ayrımı komşu sözleşmede açık kalıyor. |
| 05.7 | Ethernet emülasyonunun imkânsız olduğu yanlış tekrarlanmıyor; kablo emülasyonu mevcut kapsam dışında. |
| 05.8 | Otomatik 10× şişirme kaldırılmış; filtre uyarısı alınmış. Yoğunluk konvansiyonu ve ayrık filtre tamamlanmamış. |
| 05.9 | “512 ışında 30 FPS” kapasite garantisi tekrarlanmıyor. |
| 05.10 | Bütün HoloOcean sonarlarının ray casting’e geçtiği genellemesi tekrarlanmıyor. |
| 05.11 | Genlik/yoğunluk soğurma dönüşümleri doğru alınmış; fiziksel birimler ve tatlı su geçerliliği açık. |
| 05.12 | 13 ms’nin gözlenen maksimum olduğu doğru aktarılmış. Gecikme dağılımının negatif değer üretmemesi ayrıca tanımlanmalı. |
| 05.13 | Desteklenmeyen performans/deri derinliği iddiaları tekrarlanmıyor. 50 m kırılma ihmali doğru biçimde `[T]`; ortam bağımsız eşik değildir. |

**00 belgesinin 10 düzeltmeleri**

| Madde | Aktarım durumu |
|---|---|
| 10.1 | Ortak C++ RT soyutlaması olmadığı yanlış tekrarlanmıyor. |
| 10.2 | Metal inline ray query olanağı dışlanmıyor. |
| 10.3 | Mac’te Vulkan RT yolunun hiç olmadığı iddiası tekrarlanmıyor. |
| 10.4 | Örneklerde kapalı multipath ayarından destek yokluğu çıkarılmıyor. |
| 10.5 | Hesaplar ve “ölçüm değil” uyarısı alınmış; dönüş sorguları ve uygulama bütçesi tamamlanmalı. |
| 10.6 | Kamera spektral modeli 07’ye bırakılmış; ucuzluk garantisi tekrarlanmıyor. |
| 10.7 | Ortak dalga kaynağı alınmış; sonar yüzey yamasının ızgara/enterpolasyon hata sınırı eksik. |
| 10.8 | Kuyruk gecikmesi uyarısı alınmış; pozlama ortasından teslim hesabında yarım pozlama süresi eksik. |
| 10.9 | Platformlar arası bit eşitliği vaat edilmiyor; doğru. |
| 10.10 | APL-UW’nin 750 kHz’e kalibrasyonsuz taşınmaması doğru aktarılmış. |
| 10.11 | Deneyle fiziksel eko doğruluğunun kanıtlandığı iddiası tekrarlanmıyor; histogram benzerliğinin sınırı açıklanmış. |
| 10.12 | Donanım RT’ye bağımlı taban kurulmamış; CPU Embree seçimiyle uyumlu. |

**11’in sensörle ilgili düzeltmeleri ve ölçüm maddeleri**

- **11.1–11.2:** Keskin suya giriş kapısı kullanılmaması yaklaşımı korunmuş. GNSS geçişi yine de tatlı su deneyinin bu antene aktarımıdır; tuzluluk/sinyal koşullarıyla kalibre edilmiş model sayılmaz.
- **11.12:** PSM ölçekleri ve 100 A’nın sürekli olmadığı doğru alınmış; ölçüm hattı güç hattından ayrılmış.
- **11.15:** Gilbert–Elliott için zaman tabanı istenmesi doğru. Geçiş hızları, tam geçiş matrisi veya olay zamanlı beklemeler henüz tanımlanmadığından ayrıklaştırma kapanmış değildir.
- **11.17:** 04’ün besleme akımını motor yönüyle işaretli akım saymak doğru değil; manyetik girişim girdisi yeniden tanımlanmalı.
- **11.19:** Sanal batarya, reset ve gerçek brownout ayrımı bozulmamış; sensör bölümünün dışında kalması uygun.
- **Ç7, Ç9, Ç13, Ç19:** Sırasıyla platform toleransı, ölçülmemiş sonar maliyeti, akustik gecikmenin başlangıç noktası ve bilinen baud adayları genel olarak doğru aktarılmış.
- **M11:** Süre ve yakınsama zorunluluğu korunmuş; sahne sahipliği, gerçek sorgu sayısı ve faz modeli sabitlenmeden ölçüm anlamlı olmaz.
- **M14:** Açık veriyle uydurma mevcut kapsam için makul; gerçek sensör doğrulamasının yerine geçtiği söylenmemeli. K8 nedeniyle tedarik gereksinimi üretilmemesi doğru.
- **M12/M13/M28/M30:** Kamera üretimi ve teslim ölçümleri doğru biçimde açık tutulmuş; pozlama süresi, GPU kuyruğu ve HIL geç-kare kararları aynı zaman çizelgesinde birleştirilmeli.