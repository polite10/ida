# sol incelemesi: 04-simulator-cekirdegi

## Hüküm

Bölüm bu haliyle kabul edilemez. Sekiz başlıklı yapı, sorumluluk sınırları, başsız fizik yaklaşımı ve düzeltmelerin çoğunun aktarılması uygundur; ancak dalga kinematiğindeki işaret hatası, itici modelinin kaynakla uyuşmayan işlem sırası, fırlatma sırasında kütle ve kuvvetlerin eksik tanımlanması fiziksel sonuçları değiştirir. İskelet 0.2’ye geçiş yalnızca “1 ms” ifadelerini değiştirmekle tamamlanmaz: zamanlayıcı, temas koşulu, hesap yükü ve yakınsama testleri de yenilenmelidir. Aşağıdaki kritik ve önemli bulgular giderildikten, eksik parametreler tamamlandıktan ve sayısal kabul ölçütleri gerçek çözüm şemasına uyarlandıktan sonra yeniden incelenmelidir.

## Düzeltilmesi gerekenler

Yerler, aksi belirtilmedikçe `docs/mimari/04-simulator-cekirdegi.md` içindedir.

| # | Yer (başlık ya da satır) | Sorun | Doğrusu / önerilen düzeltme | Önem (kritik / önemli / küçük) |
|---|---|---|---|---|
| 1 | §2.10, §5, §6, §8; satır 283, 385, 398, 442–443 | Bölüm iskelet 0.1’in 1 ms adımında kalmış. §8’deki uyuşmazlık artık çözülmüş olmasına rağmen yeniden karar konusu yapılıyor. | Esas adım `1.250.000 ns`, fizik 800 Hz, kontrol 400 Hz için iki adım olmalı. Eski uyuşmazlık ve 500 Hz önerisi kaldırılmalı; aşağıdaki bütün bağımlı hesaplar güncellenmeli. | önemli |
| 2 | §2.4; satır 100, 109–115 | `θ=ωt−k·x+φ`, `ζ=a cosθ` seçildiği halde yukarı pozitif düşey hızın işareti yanlış. Bu, yalnız konvansiyon farkı değildir: verilen hız alanı süreklilik ve yüzey kinematik koşulunu sağlamıyor. | `w_yukarı=−aω sinh(k(z+h))/sinh(kh) sinθ`; sabit noktadaki düşey ivme de `−aω² … cosθ` olmalı. NED’e dönüşüm bundan sonra yapılmalı. | kritik |
| 3 | §2.3 ve `IEnvironmentView`; satır 72–80, 339–347 | Gövde hızı CG’de, su hızı CB’de tanımlanıyor. Hareket eden örnekleme noktasında su hızının türevinden uzaysal taşınım terimi eksik. `water_acceleration`ın Eulerian ivme mi, gövde boyunca türev mi verdiği belirtilmemiş. | Genelleştirilmiş hızları aynı referans noktasında tanımla. `d(Rᵀv_w(x(t),t))/dt` için dönme ve `(ẋ·∇)v_w` terimlerini kullan veya bunları ihmal eden düzgün akış yaklaşımını açıkça sınırla. FK ivmesi ile göreli hız türevini ayrı tanımla. | önemli |
| 4 | §2.4; satır 94 | “Moment basınç merkezine göre alınır” ifadesi hareket denklemindeki gövde momentiyle uyuşmuyor. | Her yüzey kuvvetini gövde referans noktasına taşı: `dτ=[RᵀdF; r_b×RᵀdF]`. Basınç merkezi kuvvetin uygulama noktası olabilir; momentin referansı değildir. | önemli |
| 5 | §2.7.2–2.7.3; satır 199–204 | Kaynağın doğrusal dinamiği ve doğrusal olmayan itki eğrisi ters sırayla uygulanmış. Kaynak `K(s)F(u)` kullanırken bölüm `F(K(s)u)` tanımlıyor; bunlar eşdeğer değildir. | Durağan itki isteğini eğriden üretip dinamik bloğa uygula. Başka bir sıra seçilecekse bunu yeni, doğrulanmamış model olarak işaretle; von Benzon modelinin aynısı sayma. [Kaynak: Denk. 13–19](https://vbn.aau.dk/ws/portalfiles/portal/505520780/jmse_10_01898.pdf). | kritik |
| 6 | §2.6, §2.8; satır 160–178, 250–259 | `13,5 kg` ve `0,0137 m³` değerlerinin iki yüklü torpidoyu içerip içermediği tanımlanmamış. Torpido ayrı gövde olarak çözülürken araçtan ancak çıkışta çıkarılması, fırlatma boyunca çift kütle/deplasman sayımına açık. | Boş araç, bağlı yük ve ayrılmış gövde durumlarını tanımla. Torpido bağımsız dinamik gövdeye dönüştüğünde kütlesi ana gövdede ikinci kez bulunmamalı. Her durumun CG, atalet, deplasman ve başlangıç hızlarını ver. | kritik |
| 7 | §2.8; satır 253–259 ve test 9 | Darbe formülü belirli bir idealizasyon için doğru; fakat bundan hesaplanan sabit kuvvetin, sürükleme ve değişen yönelim altında 200 ms sonunda hem `v₀=2 m/s` hem `L=0,2 m` sağlayacağı çıkarılmış. Kinematik kılavuzun tepki denklemleri yok. | Darbe testini dış kuvvetsiz, sabit kütleli referans durumla sınırla. Sonlu süreli modelde tüp koordinatı, kısıt kuvvetleri, etkin kütle ve dış yükleri birlikte çöz; çıkışı süreye değil tüp sonuna ulaşmaya bağla. | kritik |
| 8 | §2.8; satır 234–248 | Verilen hacim silindir hacmi; kullanılan Lamb katsayıları ise prolate spheroid katsayıları. Aynı `L/D` bunları aynı geometri yapmaz. Ayrıca torpidonun ataletleri, CG/CB’si, kanat alanı, konumu ve kaldırma eğimi yok; ilan edilen kararlılık denetimi hesaplanamıyor. | Tek geometri seç ve hacim, kütle dağılımı, eklenmiş kütle ile kanat verilerini tamamla. Silindir için spheroid katsayıları kullanılacaksa yaklaşım olduğunu belirt. [Lamb formüllerinin uygulaması](https://raw.githubusercontent.com/cybergalactic/PythonVehicleSimulator/master/src/python_vehicle_simulator/vehicles/torpedo.py). | önemli |
| 9 | §2.7.6 ve §4; satır 228, 373 | Yakınsama koşulu doğru, fakat koşul bozulunca önerilen bir adım gecikmeli yedek aynı sorunu çözmüyor. Yakınsamamış son yinelemeyle devam etmek geçerli bir bara çözümü sağlamaz. | Toplam yükü içeren gerilim denklemini ve geçerli gerilim aralığını tanımla. Kalıntı başarısızlığında aralıklı kök çözümü gibi sağlam yedek kullan; çözüm yoksa güç çökmesi/model dışı durum üret. Yakınsamayan deneyin geçerliliğini belirt. | önemli |
| 10 | §2.7.5; satır 212–224 | Hücreden pakete yalnız `R₀` dönüşümü verilmiş. `Q` birimi, paket `R₁/C₁/OCV` dönüşümü ve başlangıç durumları eksik. `Q=18` kullanılması SoC türevini 3600 kat yanlış yapar. | `Q=64.800 C`, `OCV_p=4 OCV_h`, `R₁,p=(4/6)R₁,h`, `C₁,p=(6/4)C₁,h` yazılmalı. Başlangıç SoC ve RC gerilimi tanımlanmalı. | önemli |
| 11 | §2.7.3–2.7.6; satır 204, 207, 228 | Frenleme adlandırılmış, ancak elektrik denklemi yalnız pozitif durağan akım üretiyor. Rejenerasyonun gerilim ve enerjiye etkisi tanımlanmamış. `I_maks(V)` de 12–20 V dışında tanımsız. | Frenleme modelinin imzalı akımını veya açıkça modellenmeyen etkilerini belirt. Gerilim aralığı dışındaki davranışı tanımla; dışdeğerleme durumunu açık bir geçerlilik alanıyla taşı. | önemli |
| 12 | §2.1, §2.10, §6 ve sayısal doğrulama; satır 52–55, 286, 398, 430 | “Bütün kuvvetler her aşamada” ifadesi bara geriliminin ve temasın tutulmasıyla çelişiyor. Batarya ayrı ilerletiliyor. Düzgün bölgede bile bütün bağlaşımlı sistem için dördüncü mertebe ve 16 kat hata azalması garanti değil. | Bağlaşımlı durumları aynı RK aşamalarında çöz veya ayrık bağlaşımın gerçek mertebesini belirt. 16 kat beklentisini yalnız gerçekten RK4 ile birlikte çözülen düzgün alt sistemlere uygula. | önemli |
| 13 | §2.9; satır 277 | Temas için `m_eff=m+A₃₃` yalnız CG’den geçen düşey kuvvette geçerli. Köşe teması dönmeyi uyarır ve etkin kütleyi azaltır. Normal hızın işareti de tanımsız. | Her temas için `m_eff⁻¹=wᵀM⁻¹w` kullan; normal yönünü ve yaklaşma hızını tanımla. Çoklu temasların bağlaşımını ve tutulan kuvvet şemasını sınayan kararlılık testi ekle. | önemli |
| 14 | §2.4 ve §6; satır 120–123, 401 | Nokta modeline geçişteki üstel sönüm ölçütü derin suya aittir; bölüm ise sonlu derinliği de destekliyor. Sözü edilen gövde boyunca akış değişimi sınırı sayısallaştırılmamış. | Her bileşenin gerçek sonlu derinlik fonksiyonuyla sınama yap. Nokta yaklaşımı için ayrıca gövde boyutuna bağlı uzaysal değişim ölçütü tanımla. | önemli |
| 15 | Test 5–6; satır 422–423 | Tam batık roll sertliğinde `W·BG` kullanılmış; CG orijinliyken momenti üreten `B·BG`’dir. Yeni pozitif yüzerlikte `B≠W`. Heave uç hız testi de net yüzerliğin kapatılıp kapatılmadığını söylemiyor. | Roll periyodunu `B` ile hesapla. Sönüm testinde ağırlık/yüzerliği kapat veya heave kuvvetinden net yüzerliği düş. | önemli |
| 16 | §2.6; satır 193 | Rank sonucu uygun, fakat yatay ve dikey grupların bağımsızlığı fazla güçlü anlatılmış. Yatay iticilerin `z=0,085 m` ofseti roll/pitch üretir. | “Tam matris altı eksende tahsis edilebilir” denmeli; yatay grubun çapraz momentleri 02’de kullanılan matrise dahil edilmeli. | önemli |
| 17 | §2.6 ve §8; satır 160–165, 178, 444, 449 | Kaynak nitelikleri gereksiz yere bilinmiyor bırakılmış. “Heavy oranından %20” gerekçesi de kaynak boyutlarıyla aynı değil. | Tablo A1’de kütle deneysel, ataletler ve CB CAD, CG tanımlanmış referanstır. Kaynak Heavy boyutları `0,46×0,38×0,58 m`; genişlik/boy oranı yaklaşık `1,261`. Yer tutucu `1,20` oranı bağımsız mimari seçim olarak kalabilir. [Kaynak: Tablo 5 ve A1](https://vbn.aau.dk/ws/portalfiles/portal/505520780/jmse_10_01898.pdf). | küçük |
| 18 | §2.11; satır 293 | İskelet “en yakın adım” derken bölüm daima sonraki adımı seçiyor. Kamera için 33/34 adım örneği de artık yanlış. | Tek yuvarlama kuralı seçip iskelete uy. 30 Hz, 800 Hz tabanda 26/27 adımla temsil edilir; gerçek örnekleme zamanı kaydedilmeli. | önemli |
| 19 | §2.11; satır 303 | Sayaç yalnız gerçek zamanlı iki kipte tanımlanmış; iskelet her kipte sayım istiyor. Ayrıca `1 dt` toleransı, daha küçük gerçek zaman ihlallerini sayımdan çıkarıyor. | Ham son tarih ihlali sayısını, gecikme büyüklüğünü ve geçersizlik eşiğini ayır. SIL için 1x hesap bütçesi aşımının tanısal anlamını belirt. | önemli |
| 20 | §5; satır 389 | Sonar işçilerinin dünya iş parçacığının adım süresini etkilemeyeceği ölçümsüz kesinlikte söylenmiş. CPU, önbellek ve bellek bant genişliği ortaktır. | “Dünya iş parçacığı sonucu beklemez” denebilir; süreye etkisi M11/M15 kapsamında ölçülmelidir. | önemli |
| 21 | §3; satır 328–360 | Asenkron işçilere verilen `const WorldSnapshot&` için ömür/sahiplik yok. Poz interpolasyonu, quaternion bileşen sırası, ivme anlamı, fırlatma anındaki çerçeve değişimi ve çarpışma filtreleri eksik. | Değişmez snapshot sahipliğini ve zaman aralığını tanımla; `ṗ_n=Rν₁:₃` dahil kinematiği açıkla. Bağlı torpido/ana araç çarpışma filtreleri ve hareketli hedef sorgusunun zamanını ver. | önemli |
| 22 | §2.1, §2.7.5 ve §6; satır 52, 215, 405 | Elektrik çözümü yanlış alt başlığa yönlendiriliyor. Haritalar yokken “yalnız ideal kaynak” denirken yedek tabloda tanımsız B0 bulunuyor. | Bağlaşım atfı §2.7.6 olmalı. İdeal kaynak, B0 ve B1 kiplerinin denklemleri, zorunlu girdileri ve izin koşulları tutarlı tanımlanmalı. | küçük |

## 1,25 ms adımın etkilediği yerler

- **§2.10, §5, §6 ve §8:** `dt=1 ms`, `1000 Hz`, `1 kHz` ifadeleri esas fizik için `dt=1,25 ms`, `800 Hz` olmalı. §8’in birinci önerisi iskelet 0.2’de zaten çözülmüştür.

- **İtici kararlılığı — satır 283:** Paydanın köklerini yeniden hesapladım:

  \[
  \lambda_1=-13{,}159881,\qquad
  \lambda_{2,3}=-37{,}920059\pm82{,}595533i.
  \]

  Klasik RK4 için

  \[
  R(z)=1+z+\frac{z^2}{2}+\frac{z^3}{6}+\frac{z^4}{24}
  \]

  kullanıldığında `h=0,00125 s` için:

  \[
  |R(h\lambda_1)|=0{,}98368471,\qquad
  |R(h\lambda_{2,3})|=0{,}95370591.
  \]

  **Bu doğrusal modlar 1,25 ms’de de kararlıdır.** Eski `0,98693 / 0,96279` değerleri 1 ms’ye aittir. Euler sınırı fizik adımının değişmesinden etkilenmez: `h<9,18166 ms`; 20 ms’de büyütme `1,66948`’dir.

- **Surge ve roll yerel zaman sabitleri — satır 283:** Fiziksel katsayılara bağlı olduklarından değişmezler; fakat çalışma noktaları yazılmalıdır:

  \[
  \tau_u=\frac{19{,}86}{13{,}7+2(141)(0{,}72)}
  =91{,}6305\ \mathrm{ms},
  \]

  \[
  \tau_p=\frac{0{,}449}{2(1{,}19)(5)}
  =37{,}7311\ \mathrm{ms}.
  \]

  Bunlar sırasıyla `u=0,72 m/s` ve `p=5 rad/s` içindir. Yeni adımda yaklaşık 73,3 ve 30,2 adımla örneklenirler; bütün sistemin kararlılık kanıtı değildirler.

- **Temas hesabı — satır 277:** Verilen merkezi düşey temas varsayımıyla:

  \[
  \omega_c=\sqrt{10^5/32{,}18}=55{,}7451\ \mathrm{rad/s},
  \qquad \omega_ch=0{,}0696814.
  \]

  Eski `0,056` güncellenmelidir. `0,2` mimari sınırı bu özel durumda sağlanır. Ancak yer tutucu kutunun köşesinde düşey temas için `m_eff≈3,718 kg` bulunur; bu kez `ω_ch≈0,2050` ve seçilen sınır aşılır. Yeni adımda bu sınır için gerekli etkin kütle en az `3,90625 kg`’dır.

- **Zamanlayıcı ve alt sistem hızları — satır 197, 286, 293, 399:** Tam kat olan hızlar ve örnek kesirli hızlar şöyledir:

  | Hız | Periyot | 800 Hz tabanda adım sayısı |
  |---|---:|---:|
  | Kontrol/IMU 400 Hz | 2,5 ms | 2 |
  | Kontrol yedeği 200 Hz | 5 ms | 4 |
  | 100 Hz | 10 ms | 8 |
  | 50 Hz | 20 ms | 16 |
  | Çok oranlı kuvvet 250 Hz | 4 ms | 3/4 adım |
  | Kamera 30 Hz | 33⅓ ms | 26/27 adım |
  | Yayın 60 Hz seçilirse | 16⅔ ms | 13/14 adım |

  250 Hz artık düzgün aralıklarla fizik sınırlarına oturmaz; gerçek aralıklar 3,75/5 ms olur. 200 Hz ise tam oturur. 30 Hz için en yakına yuvarlamada örnek adım dizisi `27,53,80,107,133,160…`; aralıklar `27,26,27…` olabilir. İskeletin kuralıyla zaman hatası en çok `0,625 ms`; bölümün sonraki sınıra yuvarlamasında `1,25 ms`’ye yaklaşabilir.

- **Hesap yükü — satır 288 ve 443:** Aynı basit işlem sayımı:

  \[
  500\cdot64\cdot800=2{,}56\cdot10^7
  \]

  köşe–bileşen değerlendirmesi/s verir. Fakat birincil yöntem bunları **dört RK aşamasında** hesaplıyorsa temel sayı:

  \[
  4\cdot500\cdot64\cdot800=1{,}024\cdot10^8
  \]

  değerlendirme/s olur. Gerçek `cos` çağrısı sayısı önbellekleme ve ortak hesaplara bağlıdır. Bunlar işlem sayımıdır; ölçülmüş çalışma süresi değildir.

- **Gerçek zaman sürücüsü — satır 300–303:** Fizik son tarihleri `1,25 ms` aralıklıdır; `1 dt` toleransı da `1,25 ms` olur. Kontrol/IMU bariyeri iki fizik adımında bir kurulmalıdır. Bariyer bütçesi, fizik hesap süresi ve giriş/çıkış fazı 06 ile açıkça eşlenmelidir.

- **Alt adımlama ve yakınsama — satır 398, 430, 442:** Eşit iki alt adım `0,625 ms` olmalıdır. `0,5 ms`, yeni esas adımın tam böleni değildir; iç alt adım olarak korunacaksa eşit olmayan bölme veya ayrı zaman modeli gerekir. Yakınsama dizisi `1,25 → 0,625 → 0,3125 ms` olabilir. Eski 1 ms tabana göre 0,5 ms’nin “iki kat yük” karşılaştırması artık esas karşılaştırma değildir.

- **Sürelerin adım karşılıkları:** Fiziksel süreler değişmez. Varsayılan 200 ms fırlatma 160, 2 s ESC başlatma 1600, 120 s torpido ömrü 96.000 ve 600 s enerji testi 480.000 fizik adımıdır.

## Yeniden hesaplananlar

**Hareket denklemi, basınç ve eklenmiş kütle**

CG orijininde verilen toplam kütle matrisi:

\[
M=\operatorname{diag}
(19{,}86,\ 20{,}62,\ 32{,}18,\ 0{,}449,\ 0{,}365,\ 0{,}592).
\]

Pozitif tanımlıdır. Rijit cisim ataletleri de fiziksel üçgen eşitsizliklerini sağlar:

\[
I_x+I_y-I_z=0{,}12,\quad
I_x+I_z-I_y=0{,}40,\quad
I_y+I_z-I_x=0{,}34>0.
\]

Basınç ifadesindeki çift sayım düzeltmesi doğrudur:

\[
\rho g(\zeta-z)+\rho g\sum_i a_i(F_{p,i}-1)\cos\theta_i
=-\rho gz+\rho g\sum_i a_iF_{p,i}\cos\theta_i.
\]

Dolayısıyla klasik yüzerlik ayrıca eklenmezse dalga yüksekliği iki kez sayılmaz. Bu eşdeğerlik, doğrusal dalga yaklaşımının sınırları içinde geçerlidir; anlık yüzey üstündeki uzatma ayrıca doğrulanmalıdır. [Doğrusal basınç ve hız alanı](https://web.mit.edu/fluids-modules/www/potential_flows/LecturesHTML/lec19/n6.html).

Tek eksende de:

\[
m\dot v=F_{\mathrm{diğer}}+\rho\nabla\dot u_w
+A(\dot u_w-\dot v)
\]

olduğundan:

\[
(m+A)\dot v=F_{\mathrm{diğer}}+(\rho\nabla+A)\dot u_w.
\]

Bölümün FK’yi basınç integralinden, `A·u̇_w` katkısını göreli ivmeden alma düzeni bu düzeyde **tek sayım yapıyor**. Sorun, kullanılan su türevinin hareketli ve uzaysal değişimli dalga alanındaki tanımıdır.

Düşey hız işaretini doğrudan yüzey koşuluyla denetledim:

\[
\zeta_t=-a\omega\sin\theta.
\]

Bölümün `w(0)=+aω sinθ` ifadesi bunun tersidir. Ayrıca verilen işaretlerle iki boyutlu akışta:

\[
\partial_xu+\partial_zw
=2a\omega k
\frac{\cosh k(z+h)}{\sinh kh}\sin\theta\ne0.
\]

Önerilen eksi işareti hem sürekliliği hem yüzey koşulunu düzeltir.

**İDA-0 yüzerliği, merkezleri ve yer tutucu ağ**

\[
W=13{,}5\cdot9{,}82=132{,}57\ \mathrm N.
\]

| Durum | Tam batık yüzerlik | Net yukarı kuvvet | Denge halinde su üstünde kalan hacim |
|---|---:|---:|---:|
| Tatlı su, `ρ=1000` | 134,534 N | 1,964 N | 0,200 L |
| Deniz suyu, `ρ=1025` | 137,89735 N | 5,32735 N | 0,52927 L |

**Hafif pozitif yüzerlik iddiası bu tablo için doğrulanıyor.** Deniz suyundaki kütle eşdeğeri fark `0,5425 kg`’dır. Kaynak deplasmanından değişim:

\[
(0{,}0137-0{,}0134)/0{,}0134=2{,}2388\%.
\]

Bu değişiklik mimari varsayımı olarak açıkça ayrılmıştır; kaynakla gizli çelişki değildir. Ancak küçük hacim değişiminden yüzey sertliğinin de küçük değişeceği sonucu çıkarılamaz.

Yer tutucu kutuda:

\[
H=0{,}0137/(0{,}40\cdot0{,}48)=0{,}0713542\ \mathrm m.
\]

Tatlı sudaki denge su çekimi `0,0703125 m`, serbest borda yalnız `1,0417 mm`’dir. Yüzey, gövde koordinatında yaklaşık `z_b=−0,0446354 m` konumundadır. Bu konumda dikey itici eksenlerinin derinliği yaklaşık 44,6 mm olur; bölümün kendi havalanma modeliyle:

\[
h/R=1{,}1746,\qquad \beta\approx0{,}3455.
\]

Dolayısıyla yüzeyde dikey itki, bollard değerinin yaklaşık %34,5’ine iner. Bu sonuç yer tutucu ağ ve varsayımsal havalanma eğrisine aittir; ölçülmüş T200 davranışı değildir.

**İtici tahsis matrisi**

`a=1/√2`, `b=0,085/√2`, `c=(0,156+0,111)/√2` alındığında tablodan:

\[
B=
\begin{bmatrix}
a&a&a&a&0&0&0&0\\
-a&a&a&-a&0&0&0&0\\
0&0&0&0&1&1&1&1\\
b&-b&-b&b&0{,}218&-0{,}218&0{,}218&-0{,}218\\
b&b&b&b&-0{,}120&-0{,}120&0{,}120&0{,}120\\
-c&c&-c&c&0&0&0&0
\end{bmatrix}.
\]

Bağımsız sayısal hesap:

\[
\operatorname{rank}(B)=6.
\]

Yatay ve dikey alt matrislerin rankları ayrı ayrı 3’tür. Dolayısıyla iki yönlü, doymamış iticilerle altı eksenin tamamı üretilebilir. Bununla birlikte yatay grubun:

\[
K=-0{,}085Y,\qquad M=0{,}085X
\]

çapraz momentleri vardır; saf surge/sway için dikey grubun bunları dengelemesi gerekir. Rank sonucu, doyum altında her istenen wrench’in üretilebileceğini göstermez.

Dört yatay ve dört dikey itici düzeni üreticinin Heavy açıklamasıyla uyumludur. Kaynaktaki dikey pozitif yön `−z`, bölümdeki `+z` olduğundan pozitif PWM ve itki yönü dönüşümü açıkça kaydedilmelidir; bu fark tek başına fizik hatası değildir. [Heavy üretici belgesi](https://bluerobotics.com/learn/bluerov2-heavy-configuration-retrofit-kit-installation/).

**İtici eğrisi ve dinamik sıra**

Polinom için:

\[
F(1)=30{,}4,\qquad
u_{\max}=0{,}971656,\qquad
F(u_{\max})=30{,}621753.
\]

Uçtaki azalma yaklaşık `%0,724`’tür; bölümün “%1’den küçük” hesabı doğrudur. İleri itki gerilim üsleri de:

\[
n_{12\to16}=1{,}206875,\qquad
n_{16\to20}=1{,}092926
\]

olarak doğrulandı. Kullanılan uç değerler üretici tablosuyla uyumludur. [T200 teknik tablosu](https://bluerobotics.com/learn/technical-reference/).

Ancak birim basamak için `y=K(s)1` denirse kaynak model `30,4y`, bölümdeki model `F(y)` üretir. Küçük `y` için:

\[
F(y)\approx8{,}9y.
\]

Başlangıç geçişindeki kuvvet oranı yaklaşık `8,9/30,4=0,293` olur. İşlem sırasının değiştirilmesi küçük bir gösterim farkı değildir.

**Torpido geometrisi ve kararlılık**

`L=0,20 m`, `D=0,04 m` için:

\[
\nabla_{\mathrm{silindir}}=\pi(0{,}02)^2(0{,}20)
=2{,}51327\cdot10^{-4}\ \mathrm{m^3},
\]

\[
\nabla_{\mathrm{sferoid}}=\frac43\pi(0{,}10)(0{,}02)^2
=1{,}67552\cdot10^{-4}\ \mathrm{m^3}.
\]

Bölümün hacmi silindire karşılık gelir. Prolate spheroid formüllerini yeniden hesapladığımda:

\[
k_1=0{,}0591212,\quad
k_2=0{,}8942605,\quad
k'=0{,}6998510
\]

çıkar; katsayıların aritmetiği doğrudur, geometri eşlemesi eksiktir.

Bölümün yuvarlatılmış hacim ve katsayılarıyla:

\[
A_{22}-A_{11}
=(0{,}894-0{,}059)(1000)(0{,}000251)
=0{,}209585\ \mathrm{kg}.
\]

`≈0,21 kg` hesabı doğrudur. Aynı dış ölçülerde spheroid seçilirse bu değer yaklaşık `0,140 kg` olur. Kanat verileri bulunmadığından eşitsizliğin sol tarafı ve sway–yaw özdeğerleri hesaplanamaz.

Verilen `0,26 kg` torpido, bölümün hacmiyle tatlı suda yaklaşık `0,08838 N`, deniz suyunda `0,02676 N` aşağı kuvvete sahiptir; hafif negatif yüzerlik iddiası bu hacim için tutarlıdır.

**Fırlatma tepkisi**

Tablodaki `(0,10; 0,10; 0,10) m` noktasını namlu noktası, ekseni `+x` kabul eden sabit geometrili idealizasyonda:

\[
w=[1,0,0,0,0{,}1,-0{,}1]^T,
\]

\[
M_e^{-1}=\frac1{19{,}86}
+\frac{0{,}1^2}{0{,}365}
+\frac{0{,}1^2}{0{,}592},
\qquad M_e=10{,}5662\ \mathrm{kg}.
\]

Tüp suyu sıfır alınırsa:

\[
m_e=0{,}26+0{,}059(1000)(0{,}000251)=0{,}274809\ \mathrm{kg},
\]

\[
J=\frac{2}{1/M_e+1/m_e}=0{,}535686\ \mathrm{N\,s}.
\]

Araçtaki değişimler:

\[
\Delta u=-0{,}026973\ \mathrm{m/s},\quad
\Delta q=-0{,}146763\ \mathrm{rad/s},\quad
\Delta r=+0{,}090487\ \mathrm{rad/s}.
\]

Namlu noktasının eksenel geri tepmesi yaklaşık `−0,050698 m/s`, torpidonun ileri hızı `1,949302 m/s` olur; farkları `2 m/s`’dir. **Darbe formülü bu idealizasyonda doğru.**

`200 ms` boyunca `F=J/t=2,67843 N` uygulanırsa, yalnız torpido eksenel sürüklemesini eklemek bile hedef sonucu değiştirir:

\[
c=\tfrac12\rho C_DA=0{,}263894\ \mathrm{kg/m},
\]

\[
v_t(t)=\sqrt{F/c}\tanh\!\left(\frac{\sqrt{Fc}}{m_e}t\right).
\]

Bu basitleştirilmiş karşı örnekte 200 ms sonunda bağıl hız yaklaşık `1,7884 m/s`, bağıl yer değiştirme `0,18893 m` olur. Dolayısıyla sabit kuvvet/süre seçimiyle `2 m/s` ve `0,20 m` çıkışının birlikte garanti edilmesi doğru değildir.

**Batarya ve elektrik bağlaşımı**

Hücreden paket dönüşümüyle:

\[
R_{\mathrm{top}}=0{,}025\frac46+0{,}01
=0{,}0266667\ \Omega,
\]

\[
Q=18\cdot3600=64.800\ \mathrm C,\qquad
C_{1,p}=2000\frac64=3000\ \mathrm F.
\]

Sabit RC durumu için:

\[
V=G(V)=E-R_{\mathrm{top}}I_{\mathrm{top}}(V)
\]

ve yerel yineleme koşulu:

\[
|G'(V^*)|=|R_{\mathrm{top}}I'_{\mathrm{top}}(V^*)|<1.
\]

Toplam akıma `20/V` yardımcı yükü de dahil edilmelidir. Örneğin 12–16 V doğrusal akım yaklaşımının 16 V tarafında, sekiz tam komutlu iticiyle:

\[
I'_{\mathrm{top}}=8(1{,}75)-20/16^2=13{,}921875\ \mathrm{A/V},
\]

\[
|G'|=0{,}37125.
\]

Bu örnek yerel yakınsamayı destekler; bütün çalışma zarfını kanıtlamaz. `|G'|=0,9` olduğunda sekiz yineleme başlangıç hatasını yalnız `0,9^8≈0,4305` katına indirir.

Yalnız sabit güç yükünde:

\[
V^2-EV+RP=0.
\]

Pozitif gerçek çözüm için `E²≥4RP` gerekir. Fiziksel çözümün hiç bulunmaması ile sayısal yinelemenin yakınsamaması ayrı olaylardır.

**Uç hız, roll ve heave testleri**

Sönüm denklemlerinin pozitif kökleri:

\[
u_\infty=0{,}729363,\quad
v_\infty=0{,}625863,\quad
w_\infty=0{,}712608\ \mathrm{m/s}.
\]

Bölümün üç sayısı doğrudur; sonuncusu net yüzerliğin kapatıldığı sönüm testidir. Tatlı suda 120 N aşağı itki ve mevcut net yüzerlikle heave hızı yaklaşık `0,706117 m/s` olur.

Tam batık roll için:

\[
T_\phi=2\pi\sqrt{\frac{0{,}449}{B(0{,}01)}}.
\]

Tatlı suda `3,62984 s`, deniz suyunda `3,58530 s` bulunur. Bölümdeki `3,66 s`, `B=W` kabulüne karşılık gelir.

Yer tutucu kutunun yüzey heave frekansı:

\[
\omega_3=\sqrt{\frac{1000(9{,}82)(0{,}192)}{32{,}18}}
=7{,}65444\ \mathrm{rad/s}.
\]

Bu mod 1,25 ms’de yaklaşık 657 adım/periyotla temsil edilir; zaman adımı açısından sorun görünmez.

**Nokta modeli eşiği**

Derin suda `λ=20 m`, `ε=0,01` için:

\[
d_{\min}=\frac{20}{2\pi}\ln100=14{,}6587\ \mathrm m.
\]

Fakat sonlu derinlik `h=15 m` olduğunda aynı noktada:

\[
F_p(-d)=\frac{\cosh k(h-d)}{\cosh kh}
\approx0{,}01807>0{,}01.
\]

Dolayısıyla düzeltilmiş uzun dalga ölçütü dahi sonlu derinlikte doğrudan yeterli değildir. Bu, kullanılan sonlu derinlik fonksiyonundan çıkan bağımsız karşı örnektir.

## İskelete ya da diğer bölümlere notlar

- **Yapı ve kapsam:** İstenen sekiz ana başlık vardır. Fizik, çevre, güç, torpido, temas, zaman ve iş parçacıkları kapsamı uygundur. Sensör modelleri 05’e, taşıma/protokol 06’ya, görüntü 07’ye ve senaryo/kayıt biçimleri 08’e bırakılmıştır. A1, A2, A3, A5, A6 ve A7 numaraları doğru kullanılmıştır; fizik çekirdeğinin A4’ü doğrudan uygulaması gerekmiyor.

- **02 ile eşleme:** `docs/mimari/02-ortak-cekirdek.md` §2.5, CG referanslı tam tahsis matrisini bekliyor. Buradaki çapraz momentler, pozitif komut yönleri ve yük ayrılmasında değişen referans açıkça aktarılmalı. 02’deki yaklaşık karekök ters eğri de 04’ün polinom/tablo modeliyle aynı eğri değildir; kontrolcü model hatası mı, geçici yaklaşım mı olduğu belirtilmeli.

- **05–07 için gerekli sözleşmeler:** İvmenin anlamı, quaternion sırası, sensör montaj referansı, zamanlanmış snapshot sahipliği ve geçmiş durum/interpolasyon erişimi tamamlanmalı. Kamera yakalama zamanı ile operatör çizim zamanı ayrımı korunmalı. Çarpışma sorgularında bağlı torpidonun ana araçla ve kendi tüpüyle etkileşim filtresi tanımlanmalı.

- **06 ve tekrar üretilebilirlik:** Duvar saatine bağlı gerçek zamanlı gelişler kendiliğinden tekrarlanabilir değildir. Komutun gerçekten uygulandığı fizik adımı, bariyer zaman aşımı, geç sonuç kararı ve düşürülen veri kaydedilmelidir. “Aynı giriş” yalnız mesaj içeriği olarak tanımlanmamalıdır.

- **08 ve kayıt:** Olaylarla sınırlı kalite kademesi ile tam yeniden oynatma iddiasının ilişkisi açıklanmalı. Tam tekrar için gerekli giriş ve zamanlama kayıtlarının hangi kademelerde zorunlu olduğu iskelet/A6 düzeyinde belirlenmelidir.

**Düzeltme tablolarının madde bazında sonucu**

| Düzeltmeler | Bölümdeki durum |
|---|---|
| **04.1** | Sönüm ile ampirik eklenmiş kütle ayrımı korunmuş. Kaynak nitelikleri web kontrolüyle tamamlanabilir. |
| **04.2** | Euler düzeltmesi doğru; RK4 sayıları eski adıma ait. Yeniden hesap yukarıda. |
| **04.3** | Basınç çift sayımı doğru düzeltilmiş. |
| **04.4** | Üç ayrı derinlik fonksiyonu doğru; seçilen fazla düşey hız işareti yanlış. |
| **04.5** | Tek eksenli FK/eklenmiş kütle tek sayımı doğru; uzaysal akış türevi eksik. |
| **04.6** | Yanlış `L⁴` dönme ataleti aktarılmamış; yer değiştiren su kütlesi kullanılmış. Torpido geometrisi ve dönme ataleti tanımı tamamlanmalı. |
| **04.7** | Bağıl hız ve etkin kütle düzeltmesi alınmış; sonlu süreli çok gövdeli uygulama tamamlanmamış. |
| **04.8** | Pasif fırlatmada yavaşlama ile düşürmede terminal hız doğru ayrılmış; `C_D=0,42` varsayım olarak işaretli. |
| **04.9** | C¹ geçişin fiziksel doğrulama olmadığı doğru belirtilmiş; `M_A(s)` seçeneği henüz tam bir fizik modeli değil. |
| **04.10** | `λ_min` hatası giderilmiş; yeni ölçüt yalnız derin su biçiminde kalmış. |
| **04.11** | Statik/dinamik kararlılık ayrımı ve Munk uyarısı doğru; kanat parametreleri olmadığından denetim uygulanabilir değil. |
| **04.12** | Genel motorlar hakkında eski aşırı dışlama taşınmamış. |
| **04.13** | Doğrulanmamış XLSX/ölü bant değerleri kesin veri yapılmamış. |
| **04.14** | Hunt–Whitcomb sayısal sonuçları doğrulanmış gibi kullanılmamış. |
| **04.15** | Wang–Shang doğrusal geçişi fiziksel kanıt sayılmamış. |
| **04.16** | Ölçülmemiş süre ve çok oranlı mertebe uyarısı korunmuş; dört RK aşaması işlem sayımında eksik, birincil elektrik bağlaşımının mertebe etkisi atlanmış. |
| **04.17** | Gerçek deplasman ağı şartı doğru; yükün birleşik kütle/deplasman muhasebesi eksik. |
| **04.18** | `0,91` korelasyon taşınmamış; sabitlenmiş ROV deneyinin yüzey geçişini doğrulamadığı belirtilmiş. Eidsvik hata aralıkları garanti olarak yorumlanmamalı. |
| **09.4–09.7** | Philox, dağılım sürümü, fizik için `double`, FP politikası ve Eigen uyarıları uygun aktarılmış. |
| **09.11** | İkinci tam 3B sahne önerisi tekrarlanmamış; tek Godot sınırı korunmuş. |
| **09.1–09.3, 09.8–09.10, 09.12–09.16** | Bu bölümde düzeltilmesi gereken eski iddialar kullanılmıyor; ilgili araç zinciri, bağlantı ve uygulama kararları kendi bölümlerine bırakılmış. |
| **11.6–11.8** | Komut ölü bandı/sıfır-itki ayrımı, 30 A’nın koruma eşiği olmaması ve suyla dolu T200 davranışı doğru aktarılmış. |
| **11.10–11.12** | 1-RC’ye 30 mV garantisi verilmemiş; 30Q verisi ve paket dereceleri doğru ayrılmış. Paket RC/birim dönüşümleri eksik. |
| **11.17** | Frenleme ve yeniden kalkış adlandırılmış; imzalı akım ve enerji davranışı tamamlanmamış. |
| **11.18** | Yakınsama koşulu doğru; başarısızlık davranışı ve yedek çözüm yetersiz. |
| **11.19** | Sanal batarya çökmesi, reset ve gerçek BOR doğru ayrılmış. |
| **11.1–11.5, 11.9, 11.13–11.16** | Haberleşme/arıza güvenliği kapsamındalar; hatalı eski değer veya formülleri bu bölüme sızmamış. |
| **12.6–12.7** | Jolt sorgu düzeni, sensör olayı ve temas koordinatı düzeltmeleri alınmış. Özel tepkinin referans ve etkin kütle tanımı eksik. |
| **12.14** | Ortak dalga listesinin tek başına yeterli olmadığı ve kesme üst sınırı doğru aktarılmış. |
| **12.16** | Manifest ve koordinat dönüşümü yaklaşımı uygun; verilen glTF→gövde dönüşümünün determinantı `+1`, sağ ellilik korunuyor. |
| **12.17** | Fizik sürecinin sürmesi ile kamera kullanan deneyin geçerliliği doğru ayrılmış. |
| **12.1–12.5, 12.8–12.13, 12.15** | Görselleştirici/derleme kapsamına bırakılmış; bu bölümde eski yanlış hükümler yeniden ileri sürülmüyor. |

**Kurallar:** Rust veya Rust tabanlı araç önerisi, ardıl kart önerisi, gerçek sensör tedariki gereksinimi ya da yarışma kuralından türetilmiş tasarım kısıtı bulunmadı. Rakamların çoğu varsayım veya yayımlanmış veri olarak ayrılmıştır. Buna karşılık sonar işçilerinin adım süresini etkilemeyeceği ifadesi ölçülmemiş bir performans güvencesidir; elektrik kalıntısının ADC çözünürlüğüyle gerekçelendirilmesi de henüz tanımlanmamış ölçüm zincirine dayanmaktadır.