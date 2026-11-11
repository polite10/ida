# Codex doğrulaması: 04-hidrodinamik

## Hüküm tablosu

İlk 10 satır, raporun Bölüm 5’indeki maddelere karşılık gelir. Rapor bütünüyle okundu; dosyalarda değişiklik yapılmadı. Kaynaktan doğrulanan bilgiler ile bağımsız hesaplar aşağıda ayrılmıştır.

| # | İddia (kısa) | Hüküm | Kanıt (URL) | Not |
|---|---|---|---|---|
| 1 | von Benzon’un Heavy parametreleri, itici transfer fonksiyonu, 0,72 m/s ve 85 N | DOĞRU | [Makale, Denk. 19, Tablo 4 ve A1](https://www.mdpi.com/2077-1312/10/12/1898) | Verilen sayılar kaynakta mevcut. Eklenmiş kütlenin deneysel olarak ölçüldüğü sonucu çıkarılmamalı. |
| 2 | Wu’da deney yapılamaması; Sandøy’den aktarım ve dönme katsayılarının kopyalanması | KISMEN DOĞRU | [Wu tezi](https://flex.flinders.edu.au/file/27aa0064-9de2-441c-8a17-655405d5fc2e/1/ThesisWu2018.pdf) | Tezin özeti deneylerin yapılamadığını doğruluyor. Bölüm 5.2 ve tabloları açma girişimleri zaman aşımına uğradı; Sandøy ve kopyalama ayrıntıları doğrulanamadı. |
| 3 | T200 azami itkileri ve 16 V’ta 1472–1528 µs sıfır-itki aralığı | KISMEN DOĞRU | [Üretici sayfası](https://bluerobotics.com/store/thrusters/t100-t200-thrusters/t200-thruster-r2-rp/), [XLSX](https://cad.bluerobotics.com/T200-Public-Performance-Data-10-20V-September-2019.xlsx) | 12/16/20 V itki değerleri doğru. XLSX araç tarafından okunamadı; ölü bölge doğrulanamadı. |
| 4 | Gecikmeli ivmeden eklenmiş kütle koşullu kararlı; DART örtük; Bullet/MuJoCo desteği yok | KISMEN DOĞRU | [Gazebo belgesi](https://gazebosim.org/api/sim/9/classgz_1_1sim_1_1systems_1_1Hydrodynamics.html), [kaynak kod](https://raw.githubusercontent.com/gazebosim/gz-sim/gz-sim9/src/systems/hydrodynamics/Hydrodynamics.cc) | Gazebo bağlamında doğru. “Koşulsuz kararlı” bütün simülasyonun garantisi değildir; MuJoCo hakkında kapsam ayrımı gerekir. |
| 5 | Hunt–Whitcomb ICRA 2024 hibrit modeli ve verilen sayısal sonuçlar | KISMEN DOĞRU | [Yazarların laboratuvar kaydı](https://dscl.lcsr.jhu.edu/2024/08/25/recent-dscl-publications/), [NSF tam metin adresi](https://par.nsf.gov/servlets/purl/10565587) | Eser ve DOI doğrulandı. Tam metne erişilemedi; 0,13 m, −0,2 m, \(10^{-11}\), sabit \(M_A,D\) ve örnekleme ayrıntıları doğrulanamadı. |
| 6 | JONSWAP normalizasyonu, σ ve β değerleri | DOĞRU | [SINTEF SIMA](https://sima.sintef.no/docs/5.0/simo/modelling/environment/waves.html), [MSS kodu](https://raw.githubusercontent.com/cybergalactic/MSS/master/LIBRARY/environment/waveSpectrum.m) | Yazılan spektrum ve MSS’nin γ seçim kuralı doğrulanıyor. |
| 7 | Rapier/parry genel 6×6 eklenmiş kütleyi temsil edemez; aşkın fonksiyonlarda determinizm sorunu vardır | DOĞRU | [parry 0.31.1 kodu](https://raw.githubusercontent.com/dimforge/parry/v0.31.1/src/mass_properties/mass_properties.rs), [Rapier determinizm belgesi](https://rapier.rs/docs/user_guides/rust/determinism/) | Kısıt, skaler öteleme kütlesi ve rijit cisim atalet yapısıdır. `f64` kullanılması bu yapısal kısıtı kaldırmaz. |
| 8 | MSS ve PVS’nin belirtilen entegrasyon şemaları ve adımları | DOĞRU | [SIMremus100](https://raw.githubusercontent.com/cybergalactic/MSS/master/CRAFT/AUV/SIMremus100.m), [PVS main](https://raw.githubusercontent.com/cybergalactic/PythonVehicleSimulator/master/src/python_vehicle_simulator/main.py), [PVS gnc](https://raw.githubusercontent.com/cybergalactic/PythonVehicleSimulator/master/src/python_vehicle_simulator/lib/gnc.py) | MSS: 0,05 s; Euler-açılı durumda RK4, quaternion durumunda tarif edilen karma şema. PVS örneği: 0,02 s ve Euler. |
| 9 | Eidsvik yönteminde %10–20/%30–100 hata ve %10 boyut koşulu | KISMEN DOĞRU | [von Benzon §4.3](https://www.mdpi.com/2077-1312/10/12/1898), [Eidsvik kayıt adresi](https://ntnuopen.ntnu.no/ntnu-xmlui/handle/11250/2468120) | von Benzon bu ifadeleri gerçekten aktarıyor. Özgün Eidsvik metnine erişilemedi; oranlar evrensel hata garantisi sayılamaz. |
| 10 | Kanat–Munk momenti karşılaştırmasından statik yön kararlılığı eşitsizliği | DOĞRU | [Fossen’in denklemleri](https://www.fossen.biz/html/marineCraftModel.html) | Küçük açı, sabit katsayılar, uygun kanat alanı ve moment kolu varsayımlarıyla bağımsız türetim doğruluyor. Dinamik kararlılık garantisi değildir. |
| 11 | BlueROV2 karesel sönümleri arasında “5–8 kat” fark | YANLIŞ | [von Benzon Tablo A1](https://www.mdpi.com/2077-1312/10/12/1898) | Raporun kendi Wu sayılarıyla oranlar surge/sway/heave için **7,76 / 10,02 / 5,14**. |
| 12 | 1 ms RK4 kararlı; 20 ms ileri Euler “sınırda” | KISMEN DOĞRU | [İtici modeli](https://www.mdpi.com/2077-1312/10/12/1898), [MIT sayısal kararlılık kaynağı](https://math.mit.edu/~gs/dela/delasol_ch3.pdf) | Verilen doğrusal modlarda 1 ms RK4 kararlı. Üçüncü dereceden itici için 20 ms Euler kararsız. |
| 13 | Fırlatmada \(J=(m_t+A_{11,t})v_0\) ve hızın doğrudan atanması | KISMEN DOĞRU | [Genelleştirilmiş kütle yapısı](https://www.fossen.biz/html/marineCraftModel.html) | Sınırlı bir idealizasyon olabilir. Son bağıl çıkış hızı, geri tepme ve bağlı su kütlesi birlikte çözülmemiş. |
| 14 | Görev öğesi için \(C_D=0,3–1,0\); fırlatma testinde limit hızdan belirleme | KISMEN DOĞRU | [PVS REMUS modeli](https://raw.githubusercontent.com/cybergalactic/PythonVehicleSimulator/master/src/python_vehicle_simulator/vehicles/remus100.py) | Sürükleme denklemi ve REMUS’taki 0,42 doğru. Yeni geometri için aralık doğrulanamıyor; pasif yatay fırlatmada sıfırdan farklı limit hız yoktur. |
| 15 | T200 RPM, \(k\), \(K_T\) ve ölü bölge hesapları | DOĞRULANAMADI | [Üretici ham verisi](https://cad.bluerobotics.com/T200-Public-Performance-Data-10-20V-September-2019.xlsx) | Ham tablo okunamadı. Verilen RPM doğru kabul edilirse bazı hesaplar aritmetik olarak tutarlı; bu, ölçümü doğrulamaz. |
| 16 | Anlık yüzeye göre hidrostatik basınca FK basıncı eklenmesi | KISMEN DOĞRU | [MIT dalga basıncı](https://web.mit.edu/fluids-modules/www/potential_flows/LecturesHTML/lec19/n6.html) | \(z_{\rm rel}\) anlık yüzeye göre tanımlanırsa dalga katkısı çift sayılır. Ortalama yüzeye göre koordinat açıkça tanımlanmalı. |
| 17 | Sonlu derinlikte tüm dalga büyüklüklerinin aynı cosh oranıyla azalması | KISMEN DOĞRU | [NTNU doğrusal dalga teorisi](https://oivarn.folk.ntnu.no/hercules_ntnu/LWTcourse/lwt_new_2000_Part_A.pdf) | Basınç, yatay hız ve düşey hızın derinlik fonksiyonları farklıdır. |
| 18 | Dönme eklenmiş ataletinin \(L^4\) ile ölçeklenmesi | YANLIŞ | [Eklenmiş atalet formülleri](https://mujoco.readthedocs.io/en/stable/computation/fluid.html#added-mass) | Geometrik benzerlikte dönme eklenmiş ataleti \(\rho L^5\) ile ölçeklenir. |
| 19 | Değişken \(M_A\) için yalnızca \(\dot M_A\nu\) ekleyerek geçiş fiziğini açıklama | KISMEN DOĞRU | [Fossen’in Lagrange/Kirchhoff çerçevesi](https://www.fossen.biz/html/marineCraftModel.html) | Çarpım türevi doğru; genel serbest-yüzey denklemi ve slamming modeli bundan ibaret değildir. |
| 20 | \(z<-\lambda_{\min}/2\) olduğunda “derin” modele geçmek | YANLIŞ | [MSS dalga basıncı modeli](https://raw.githubusercontent.com/cybergalactic/MSS/master/LIBRARY/environment/spheroidRAO.m) | Uzun dalgalar daha derine nüfuz eder. Ölçüt \(\lambda_{\min}\) üzerinden kurulamaz. |
| 21 | `simba/num-traits` üzerinde yalnızca `libm` açmak determinizmi sağlar | KISMEN DOĞRU | [simba kaynak kodu](https://raw.githubusercontent.com/dimforge/simba/master/src/scalar/complex.rs), [Rust f64 belgesi](https://doc.rust-lang.org/std/primitive.f64.html) | `std` açıkken sıradan `libm` özelliği beklenen yönlendirmeyi sağlamayabilir; `libm_force` veya doğrudan çağrı gerekir. |
| 22 | Aracri ve arkadaşlarında 0,16/0,27/0,34 hata ve 0,91 korelasyon | KISMEN DOĞRU | [CNR birincil kaydı](https://iris.cnr.it/handle/20.500.14243/448967) | Hata değerleri özette mevcut. 0,91 bulunamadı; tam metin erişimi kısıtlı. İlk yazar Kyle L. Walker. |
| 23 | Dalga hesabının 0,3–0,6 saniye/simülasyon saniyesi tutacağı | DOĞRULANAMADI | — | İşlem sayısı hesabı makul; süre için donanım, derleme ve ölçülmüş benchmark verilmemiş. |

## Yanlış ya da yanıltıcı bulunanlar

**1. Katsayı setleri arasındaki farkın büyüklüğü ve doğrulama kapsamı**

Raporda fark “5–8 kat” olarak özetleniyor. Raporun verdiği sayılardan yeniden hesapladım:

\[
141/18{,}18=7{,}76,\qquad
217/21{,}66=10{,}02,\qquad
190/36{,}99=5{,}14.
\]

Sway farkı yaklaşık on kattır. Ayrıca von Benzon’da sönüm deneylerden, eklenmiş kütle ampirik yöntemden gelir. Dolayısıyla bütün parametre setinin tankta doğrulanmış olduğu izlenimi yanıltıcıdır. [Kaynak: von Benzon, §4.2–4.3 ve Tablo A1](https://www.mdpi.com/2077-1312/10/12/1898).

Bu karşılaştırma, Wu’nun özgün tablolarını okuyabildiğim anlamına gelmez: oranlar **rapordaki Wu rakamları kullanılarak** hesaplandı. Özgün tablolardaki sayılar ve aktarım ayrıntıları erişim nedeniyle açık kalmıştır.

**2. RK4 önerisi makul; ileri Euler karşılaştırması yanlış**

Transfer fonksiyonunun kutuplarını yeniden hesapladım:

\[
\lambda_1=-13{,}1599,\qquad
\lambda_{2,3}=-37{,}9201\pm82{,}5955i.
\]

İleri Euler’in koşulu:

\[
|1+h\lambda|<1
\quad\Longrightarrow\quad
h<\frac{-2\Re(\lambda)}{|\lambda|^2}.
\]

Karmaşık kutuplar için sınır **9,18 ms** çıkıyor. \(h=20\) ms’de büyütme çarpanı yaklaşık **1,669**: model kararsızdır.

Klasik RK4 için bağımsız hesap:

\[
R(z)=1+z+\frac{z^2}{2}+\frac{z^3}{6}+\frac{z^4}{24}.
\]

\(h=1\) ms’de kutuplar üzerindeki \(|R(h\lambda)|\) değerleri yaklaşık **0,98693** ve **0,96279**. İncelenen modlar kararlıdır. Rapordaki surge ve roll yerel zaman sabitlerini de sırasıyla **91,6 ms** ve **37,7 ms** buldum.

Bununla birlikte üç yerel zaman sabiti, değişken batıklık, temas, balast ve bütün bağlaşımlı sistem için kararlılık ispatı değildir. Kararlılık ile yeterli doğruluk da ayrı koşullardır. [Model kaynağı](https://www.mdpi.com/2077-1312/10/12/1898), [sayısal yöntem kaynağı](https://math.mit.edu/~gs/dela/delasol_ch3.pdf).

**3. Dalga basıncında çift sayım riski var**

Raporun önerdiği basınç:

\[
p=\rho g(-z_{\rm rel})+\rho g\sum_i a_i e^{k_i z}\cos\theta_i.
\]

Burada \(z\), yukarı pozitif ve **ortalama su yüzeyine göre** koordinat olsun. Doğrusal Bernoulli denkleminden doğru birinci mertebe basınç:

\[
p-p_{\rm atm}
=-\rho gz+\rho g\sum_i a_iF_{p,i}(z)\cos\theta_i.
\]

Eğer rapordaki \(z_{\rm rel}=z-\zeta\) ise ilk terim zaten \(\rho g\zeta\) içerir. Üzerine tam FK katkısı eklenince bu katkı çift sayılır. Anlık derinlik kullanılacaksa eşdeğer ifade:

\[
p-p_{\rm atm}
=\rho g(\zeta-z)
+\rho g\sum_i a_i\bigl[F_{p,i}(z)-1\bigr]\cos\theta_i.
\]

Islak yüzeyi kırpmak ile basınç referansını seçmek ayrı işlemlerdir. Rapor bu ayrımı yeterince açık yapmıyor. [Kaynak: MIT dalga basıncı denklemleri](https://web.mit.edu/fluids-modules/www/potential_flows/LecturesHTML/lec19/n6.html).

Ayrıca sonlu derinlikte, faz işaretleri seçilen konvansiyona bağlı olmak üzere genlikler:

\[
p'=\rho ga\frac{\cosh k(z+h)}{\cosh kh}\cos\theta,
\]

\[
u=a\omega\frac{\cosh k(z+h)}{\sinh kh}\cos\theta,\qquad
w=a\omega\frac{\sinh k(z+h)}{\sinh kh}\sin\theta.
\]

Tek bir derinlik çarpanını basınç ve bütün hız bileşenlerine uygulamak yanlış olur. [Kaynak: NTNU](https://oivarn.folk.ntnu.no/hercules_ntnu/LWTcourse/lwt_new_2000_Part_A.pdf).

**4. İvmeli dalga akışı, sabit akıntı sadeleştirmesine doğrudan konamaz**

Rapordaki FK’nin ayrıca bulunması gerektiği gözlemi doğrudur; fakat uygulama tarifi çift sayımı engelleyecek kadar açık değildir.

Tek eksende, sabit eklenmiş kütle \(A\), araç hızı \(v\), su hızı \(u_w\) için bağımsız türetim:

\[
m\dot v=F_{\rm diğer}+\rho\nabla\dot u_w
+A(\dot u_w-\dot v),
\]

\[
(m+A)\dot v
=F_{\rm diğer}+(\rho\nabla+A)\dot u_w.
\]

Dolayısıyla \(A\dot u_w\) göreli-ivme teriminde zaten varsa yeniden dış kuvvet olarak eklenmemeli. Tam \(M\dot\nu_r\) sadeleştirmesine geçilmişse de sabit akıntı varsayımının ivmeli dalgalarda geçerli olduğu kabul edilmemeli. [MSS’de FK ve düşük frekanslı difraksiyon ayrımı](https://raw.githubusercontent.com/cybergalactic/MSS/master/LIBRARY/environment/spheroidRAO.m).

**5. Dönme eklenmiş ataleti için ölçekleme üssü yanlış**

Raporda dönme eklenmiş ataleti \(L^4\) ile ölçekleniyor. Bağımsız boyut analizi:

\[
I_A\sim\rho\int r^2\,dV\sim\rho L^5.
\]

Geometrik benzerlikte:

- Öteleme eklenmiş kütlesi: \(\rho L^3\).
- Öteleme–dönme bağlaşım katsayıları: \(\rho L^4\).
- Dönme eklenmiş ataleti: \(\rho L^5\).
- Açısal hızın karesiyle çarpılan dönme sürükleme katsayısı: \(\rho L^5\).

Raporda sonuncusu doğru, dönme eklenmiş ataleti yanlıştır. [Kaynak: elipsoid eklenmiş atalet formülleri](https://mujoco.readthedocs.io/en/stable/computation/fluid.html#added-mass).

Lamb formüllerindeki kütle ölçeği de genel durumda aracın kuru kütlesi değil, **yer değiştiren sıvının kütlesidir**. PVS’nin nötr yüzer sferoidinde bunlar eşit seçildiği için ayrım görünmez. [PVS kodu](https://raw.githubusercontent.com/cybergalactic/PythonVehicleSimulator/master/src/python_vehicle_simulator/vehicles/remus100.py).

**6. Torpido statik kararlılık türetimi tutarlı, kapsamı sınırlı**

Bağımsız kontrol için \(A_{ii}\)’leri pozitif fiziksel eklenmiş kütle olarak alıyorum. Küçük \(\beta=v/U\), sıfır yaw hızı ve simetrik gövdede:

\[
N_{\rm Munk}=-(A_{22}-A_{11})uv
\approx-(A_{22}-A_{11})U^2\beta.
\]

Arkada bulunan kanadın yan kuvveti ve momenti:

\[
Y_f\approx-\tfrac12\rho U^2S_fC_{L\alpha}\beta,
\]

\[
N_f=x_fY_f
=\tfrac12\rho U^2S_fC_{L\alpha}|x_f|\beta.
\]

Sabit hareket doğrultusuna göre küçük yönelim sapmasında \(\beta\approx-\delta\psi\). Geri getirici moment için gerçekten:

\[
\boxed{\tfrac12\rho S_fC_{L\alpha}|x_f|>A_{22}-A_{11}}
\]

elde edilir. İki tarafın birimi kg’dır; \(U^2\) sadeleşir. [Denklemsel temel: Fossen](https://www.fossen.biz/html/marineCraftModel.html).

Ancak \(S_f\), ilgili düzlemde etkin toplam alan; \(x_f\), kuvvet merkezinin uygun referansa göre kolu olmalıdır. Katsayılar Reynolds sayısıyla değişirse hızdan bağımsızlık yaklaşık kalır.

Bu bir **statik moment koşuludur**. Serbest aracın sway–yaw dinamik kararlılığı için doğrusal sistemin özdeğerleri de sınanmalıdır; iki durumlu modelde \(\operatorname{tr}(A)<0\) ve \(\det(A)>0\) gerekir.

**7. Fırlatma tepkisi modeli koşulsuz geçerli değil**

Rapordaki namlu noktası hızı \(v+\omega\times r\) ve açısal darbe \(r\times J\) doğrudur. Sorun, çıkış hızının tanımı ve geri tepmenin birlikte çözülmemesidir.

Basit tek eksenli, başlangıçta duran sistemde \(v_0\), **atış sonrasındaki bağıl ayrılma hızı** olsun. Araç ve görev öğesinin etkin kütleleri \(M_e,m_e\) ise:

\[
V_v^+=-\frac{J}{M_e},\qquad
V_t^+=\frac{J}{m_e},
\]

\[
v_0=V_t^+-V_v^+
=J\left(\frac1{m_e}+\frac1{M_e}\right),
\]

\[
\boxed{J=\frac{M_em_e}{M_e+m_e}v_0}.
\]

\(J=m_ev_0\), araç geri tepmesinin ihmal edildiği sınırdır. \(v_0\) atış öncesi namlu hızına eklenen mutlak hız artışı olarak tanımlanırsa başka bir idealizasyon elde edilir; bunlar karıştırılmamalı.

Eklenmiş kütle, yalnızca “tüpte su var mı?” sorusuyla belirlenemez. Görev öğesinin çevresindeki su, tüp içindeki su, mekanizma ve araçla hidrodinamik etkileşim önemlidir. Kullanılan etkin kütle modeli açıkça belirtilmelidir. [Kütle matrisi çerçevesi](https://www.fossen.biz/html/marineCraftModel.html).

Süre hesabı da genellenemez: duruştan sabit ivmeyle \(v_0\)’a ulaşılırsa \(t=2L/v_0\). Örneğin \(L=0,2\) m ve \(v_0=2\) m/s için süre **200 ms** olur. Mekanizma bilinmeden olayı tek fizik adımına sıkıştırmak gerekçelendirilmiş değildir.

**8. Sürükleme tahmini doğrulanmış bir katsayı seti değil**

\[
X=-\tfrac12\rho C_DA_{\rm ön}u|u|
\]

fiziksel olarak tutarlıdır. PVS’de REMUS için cephe alanına göre 0,42 vardır; bu sayı yeni yarışma öğesinin katsayısını belirlemez. [Kaynak](https://raw.githubusercontent.com/cybergalactic/PythonVehicleSimulator/master/src/python_vehicle_simulator/vehicles/remus100.py).

Pasif yatay fırlatma için:

\[
m_e\dot u=-cu^2,\qquad c=\tfrac12\rho C_DA,
\]

\[
\frac1{u(t)}=\frac1{u_0}+\frac{c}{m_e}t.
\]

Sıfırdan farklı limit hız oluşmaz. Hızın zamanla azalmasından, \(m_e\) biliniyorsa \(C_D\) bulunabilir. Negatif yüzer bir öğenin düşürme testinde ise terminal hızdan:

\[
C_D=\frac{2(m-\rho\nabla)g}{\rho A U_T^2}
\]

elde edilir; burada sabit yönelim ve diğer kuvvetlerin ihmal edilebilirliği gerekir. Dolayısıyla “düşürme veya fırlatma testinde limit hız” aynı yöntem değildir.

**9. Değişken eklenmiş kütleyi yumuşatmak fiziksel tutarlılığı sağlamaz**

\[
\frac{d}{dt}(M_A\nu)=M_A\dot\nu+\dot M_A\nu
\]

bir çarpım türevi olarak doğrudur. Bunun bütün geçiş denklemini verdiği sonucu çıkmaz.

Örneğin tek boyutlu kapalı Lagrange modelinde:

\[
T=\tfrac12[m+A(z)]\dot z^2
\]

seçilirse:

\[
[m+A(z)]\ddot z+\tfrac12A'(z)\dot z^2=Q.
\]

Burada doğrudan momentum türevinden yazılan katsayıdan farklı bir terim çıkar. Bu örnek suya giriş için evrensel bir slamming yasası değildir; enerji, geometri ve momentum akısının birlikte ele alınması gerektiğini gösterir. [Temel çerçeve](https://www.fossen.biz/html/marineCraftModel.html).

C¹ geçiş, sayısal süreklilik sağlar. Enerji korunumu veya doğru giriş/çıkış fiziğini garanti etmez.

**10. Derinlik eşiğinde yanlış dalga boyu kullanılıyor**

Rapordaki \(-\lambda_{\min}/2\) eşiği kısa dalgaları bastırır; uzun dalgaları bastırmaz:

\[
e^{-kd}=e^{-2\pi d/\lambda}.
\]

Örneğin \(d=0,5\) m’de 20 m dalga boylu bileşenin çarpanı yaklaşık **0,855** kalır. Bütün önemli bileşenler için \(\varepsilon\) eşiği istenirse:

\[
d\geq\frac{\lambda_{\max}}{2\pi}\ln(1/\varepsilon)
\]

gibi bileşenlerin tamamını kapsayan bir ölçüt gerekir. Nokta modeline geçiş ayrıca gövde boyunca akış değişiminin küçüklüğüne bağlıdır. [Kaynak: MSS](https://raw.githubusercontent.com/cybergalactic/MSS/master/LIBRARY/environment/spheroidRAO.m).

**11. Deterministik matematik seçimi eksik tarif edilmiş**

Rapier’in platformlar arası determinizm uyarısı doğrudur. Ancak `simba` kodunda `std` açık ve `libm_force` kapalıyken standart kayan nokta fonksiyonlarına giden uygulama seçilir. Yalnızca `libm` özelliğini açmak yeterli olmayabilir. [Kaynak kod](https://raw.githubusercontent.com/dimforge/simba/master/src/scalar/complex.rs).

Doğrudan `libm` çağrıları veya uygun `libm_force` yapılandırması, bağımlılıklardaki çağrıları da kapsamalı. Quaternion, RNG dağılımları ve diğer yardımcı hesaplar denetlenmeden yalnızca dalga `cos` çağrısını değiştirmek yeterli değildir. Rust belgeleri standart aşkın fonksiyonların kesinliğini platformlar arasında garanti etmez. [Rust belgesi](https://doc.rust-lang.org/std/primitive.f64.html).

**12. Erişilemeyen kaynakların sonuçları açık bırakılmalı**

- **Hunt–Whitcomb:** Çalışma gerçektir; yazarların üniversite kaydında DOI **10.1109/ICRA57147.2024.10610798** vardır. NSF tam metni açılamadı; IEEE sayfası bot kontrolü gösterdi. Rapordaki ayrıntılı sayısal sonuçlar teyit edilmedi. [Üniversite kaydı](https://dscl.lcsr.jhu.edu/2024/08/25/recent-dscl-publications/).
- **T200 XLSX:** Dosya adresi bulundu, fakat tarayıcı aracı XLSX içeriğini desteklemedi; doğrudan erişim de başarısız oldu. 1472–1528 µs, 4 µs örnekleme ve RPM değerlerini doğrulanmış saymıyorum. Verilen 3533 RPM varsayımıyla \(k=0,01485\) ve tatlı suda \(K_T\approx0,445\) hesaplanıyor; bu yalnızca koşullu aritmetik kontroldür.
- **T200 gerilim üssü:** Üreticinin doğrulanan azami ileri itkilerinden 12→16 V için **1,207**, 16→20 V için **1,093** bulunuyor. Raporun yaklaşık üs ifadesi bu aralıkta makul; genel bir pervane yasası değildir. [Üretici](https://bluerobotics.com/store/thrusters/t100-t200-thrusters/t200-thruster-r2-rp/).
- **Walker/Aracri:** CNR özeti hata değerlerini destekliyor; 0,91 korelasyonu ve ayrıntılı model doğrulanamadı. Sabitlenmiş ROV deneyi, hibrit aracın yüzey geçişini doğrulamaz. [CNR kaydı](https://iris.cnr.it/handle/20.500.14243/448967).

## Rapor dışı ek bulgular

- **PVS torpido şablonu, önerilen kararlılık sınamasını doğrudan yapamaz.** `torpedo.py` de REMUS gibi bazı eklenmiş-kütle Coriolis terimlerini, açıkça Munk momentini de kapsayacak biçimde sıfırlıyor. Bu kod aynen taşınırsa kanat–Munk eşitsizliği sınanamaz. Ayrıca karşılaştırılan referansın hangi terimleri içerdiği kaydedilmelidir. [Kaynak](https://raw.githubusercontent.com/cybergalactic/PythonVehicleSimulator/master/src/python_vehicle_simulator/vehicles/torpedo.py).

- **Rapier hakkındaki temel kısıt doğru; bütün genel fizik motorlarını dışlamak aşırı geniştir.** parry genel simetrik 3×3 dönme ataletini asal eksenlere ayırarak temsil edebilir; sorun bunun genel hidrodinamik 6×6 matris yerine geçmemesidir. Gazebo/DART bunun için bir karşı örnektir. Stonefish ise öteleme eklenmiş kütlesini eksenler arasında ortalayarak kısıtı aşar ve bunu belgelendirir. [parry](https://raw.githubusercontent.com/dimforge/parry/v0.31.1/src/mass_properties/mass_properties.rs), [Stonefish](https://stonefish.readthedocs.io/en/v1.5/theory.html).

- **MuJoCo ayrımı özellikle önemlidir.** Yerleşik elipsoid modelinde eklenmiş kütle kaynaklı kuvvetler vardır. İncelenen kaynak kodda ivmeye bağlı parçalar `NULL` ivmeyle devre dışı bırakılır; hız bağlaşım terimleri hesaplanır. Bu, genel \(M_A\dot\nu\) desteğiyle aynı değildir. “Hiç eklenmiş kütle modeli yok” ifadesi de doğru olmaz. [Belge](https://mujoco.readthedocs.io/en/stable/computation/fluid.html), [uygulama](https://raw.githubusercontent.com/google-deepmind/mujoco/main/src/engine/engine_passive.c).

- **Wang–Shang kaynağında ciddi bir sayısal tutarsızlık var.** Tablo II’de araç kütlesi 0,3 kg, hacim \(0,15\ \mathrm{m^3}\) yazıyor. Tatlı suda bu, yaklaşık 150 kg yer değiştiren su demektir. Birim/yazım hatası olasıdır; bu tablo düzeltilmeden nicel doğrulama referansı olamaz. Doğrusal batıklık fonksiyonunun kaynakta bulunması, fiziksel doğruluğunu kanıtlamaz. [Kaynak, Tablo II ve Denk. 21](https://arxiv.org/html/2507.03869v1).

- **200–250 Hz’de dondurulan kuvvetler bütün çözümün RK4 mertebesini düşürebilir.** Bağımsız çıkarım: kuvvet yalnızca adım başında hesaplanıp ara aşamalarda sabit tutulursa, durum ve zaman bağımlılığındaki hata dış kuvvet güncellemesinden gelir. Tam sistemde 16 kat yakınsama beklenmemeli; çok oranlı şema ayrıca ölçülmelidir.

- **Hidrostatik ağ gerçek su deplasmanını temsil etmeli.** Açık çerçeveli aracın dış zarfını kapalı hacim kabul etmek, su dolan boşlukları yüzerlik hacmine ekleyebilir. Basınç gövdeleri, köpük, serbest su alanları ve balast ayrı tanımlanmalı; görev öğesi ayrıldığında araç kütlesi, CG’si ve ataleti güncellenmelidir. Kapalı hacim gereksiniminin geometrik temeli [Kerner’de](https://www.gamedeveloper.com/programming/water-interaction-model-for-boats-in-video-games) açıklanıyor.

- **Morison için verilen yaklaşık \(\lambda>5D\) sınırı doğrulanabiliyor.** DNV bunu ince elemanlar için belirtir; karmaşık, hacimli ROV’a tek noktadan uygulamanın doğruluğunu garanti etmez. [DNV’nin açıklaması](https://www.dnv.com/software/FAQ/When-should-I-use-diffraction-theory-to-calculate-wave-forces-on-a-structure/).

## Raporlar arası çelişki değerlendirmesi

Başka bir raporla karşılaştırma istenmediğinden değerlendirme bu dosyayla sınırlıdır.

Dosya içinde iki önemli gerilim vardır: Wu’daki tahminler haklı olarak sorgulanırken von Benzon’un ampirik eklenmiş kütlesine daha güçlü bir doğrulama anlamı yükleniyor; ayrıca Munk momentini koruyan fizik önerilirken bu momenti sıfırlayan PVS torpido kodu şablon gösteriliyor.

## Mimari için net sonuçlar

1. **Özel, f64 kullanan 6-DOF çekirdek savunulabilir.** Genel 6×6 kütle matrisi desteklenmeli; Rapier’in kısıtı doğru gerekçeyle belirtilmeli.
2. **Dalga basıncı ve Morison katkıları yeniden düzenlenmeli.** Hidrostatik, FK ve eklenmiş-kütle ivmesi her biri yalnızca bir kez sayılmalı.
3. **1 ms RK4 başlangıç değeri olarak tutulabilir.** Kabulü, bütün sistemde yakınsama ve çalışma zarfı testlerine bağlı olmalı.
4. **von Benzon seti benchmark olabilir; hedef aracın kalibrasyonu değildir.** Eklenmiş kütle ve sönümün kaynakları ayrı tutulmalı; \(L^4\) dönme ataleti ölçeklemesi düzeltilmeli.
5. **Yüzey geçişindeki \(M_A(s),D(s)\) deneysel olarak doğrulanmamış yaklaşım sayılmalı.** Yumuşatma, enerji ve momentum doğrulamasının yerini almamalı.
6. **T200 tablosu uygun başlangıç kaynağıdır.** Ham veriler okunmadan ölü bölge/RPM uyumları kesinleştirilmemeli; ESC ve donanım revizyonu kaydedilmeli.
7. **Görev öğesinin ayrılması çok gövdeli olay olarak modellenmeli.** Bağıl çıkış hızı, geri tepme, araç parametre değişimi ve etkin su kütlesi tutarlı çözülmeli.
8. **Determinizm bütün matematik zincirini kapsamalı.** Bağımlılıklar sabitlenmeli, uygun matematik arka ucu seçilmeli ve macOS/Windows tekrar oynatma testleri yapılmalı.