# sol incelemesi: 02-ortak-cekirdek

## Hüküm

Bölüm bu haliyle kabul edilmemeli. Kapsam, sekiz başlıklı yapı, arayüz numaraları ve temel teknoloji kararları iskeletle büyük ölçüde uyumlu; araştırmalardaki önemli yanlışların çoğu da tekrarlanmamış. Ancak gecikmiş ölçümlerin füzyonu, durum ile kovaryansın zaman eşlemesi, IMU’suz SIL ilerlemesi ve atış çıktısının emniyeti kritik açıklıklar içeriyor. Aşağıdaki kritik maddeler giderildikten, A4 sözleşmesi tamamlandıktan ve önemli maddeler için uygulanabilir davranış ile kabul ölçütleri tanımlandıktan sonra yeniden incelenmeli. İşlemci kapasitesi hakkındaki kabul ayrıca M5 ölçümüne bağlı kalmalı.

## Düzeltilmesi gerekenler

Bütün satır numaraları, aksi belirtilmedikçe `docs/mimari/02-ortak-cekirdek.md` dosyasına aittir.

| # | Yer (başlık ya da satır) | Sorun | Doğrusu / önerilen düzeltme | Önem (kritik / önemli / küçük) |
|---|---|---|---|---|
| 1 | §2.3, satır 108–128 | Durum vektörü verilmiş; nominal hareket denklemleri, hata dinamiği, ayrık geçiş matrisi ve süreç gürültüsünün oluşturulması verilmemiş. `P = FPFᵀ + Q_d` tek başına yeniden uygulanabilir bir filtre tanımı değildir. | Kuaterniyon bileşen sırası ve çarpım kuralı, sağdan yerel hata tanımı, yerçekimi vektörü, nominal entegrasyon, `F` ve `Q_d` üretimi, bias modeli ve başlangıç hizalama/kovaryansı tanımlansın. Sürekli zaman Jacobian’ı ile ayrık geçiş matrisi ayrı adlandırılsın. | önemli |
| 2 | §2.2–2.3, satır 77, 90, 112–126 | Nominal durum 400 Hz’te, kovaryans 100 Hz’te ilerliyor; ölçümler ise geldikçe güncelleniyor. Ara adımlarda kullanılan `P` ile ölçüm modelinin hangi zamana ait olduğu belirsiz. Güncellemeden sonra birikmiş IMU aralığının nasıl tüketileceği de tanımsız. | Her füzyonda durum, Jacobian ve kovaryans aynı zaman ufkunda buluşmalı. Ya füzyondan önce kovaryans ilgili zamana taşınmalı ya da bütün füzyon tanımlı bir 100 Hz ufkunda yapılmalı. Birikmiş aralıkların güncelleme sınırlarında bölünmesi açıklansın. | kritik |
| 3 | §2.3 “Gecikme”, satır 124; §6 | Geçmiş durumdan yenilik hesaplayıp düzeltmeyi doğrudan şimdiki duruma uygulamak, gecikmiş ölçüm güncellemesini tanımlamıyor. Geçmiş kovaryans, geçmiş–şimdi çapraz kovaryansı ve düzeltmenin taşınması yok. Düşük araç hızı bu matematiksel eksikliği ortadan kaldırmaz. | Gecikmeli füzyon ufku ve güncel çıktı kestirimi veya geçmişte güncelleme ve IMU ile yeniden yayılım tanımlansın. Yaklaşık yöntem korunacaksa kazanç/kovaryans denklemleri, azami gecikme ve geçerlilik sınırı açık olsun; yalnız “SIL’de ölçülecek” denmesi yeterli değil. | kritik |
| 4 | §2.3 ölçüm modelleri, satır 118–126 | Basınç ve DVL modelleri kısmen açık; manyetometre yalnız “yön açısı”, GNSS yalnız “anten kolu” olarak geçiyor. Ölçüm Jacobian’ları ve belirsizliklerin türetilmesi yok. `eph` veya FOM’un hangi istatistiksel anlama geldiği belirlenmeden doğrudan varyansa çevrilemez. | Tilt telafili manyetik yön, açı yeniliğinin sarılması, manyetik geçerlilik koşulları, GNSS anten konumu/hızı ve DVL kovaryans dönüşümü tanımlansın. Kullanılan `R` matrislerinin birimleri, kaynağı ve geçersiz kovaryans davranışı verilsin. | önemli |
| 5 | §2.3–2.5, satır 130–152, 167 | Fiziksel yüzey/dalış rejimi, kestiricinin kaynak kipine bağlanmış. Yüzeyde GNSS kaybı ile dalış aynı olay değildir. Ayrıca `DIVE` sırasında yüzey kontrolcüsünün `Z` yetkisi açık değil; dalış kaynağına geçmek için derinleşmek, derinleşmek için dalış kontrolünü açmak gerekebilir. | Fiziksel rejim, sensör kullanılabilirliği ve güdüm aşaması ayrı durumlar olsun. `TRANSITION` giriş/çıkışları; `DIVE` sırasında dikey yetki; GNSS kesintisinde yüzey davranışı ve GNSS beklemeden güvenli yükselme açıkça tanımlansın. | önemli |
| 6 | §2.3 sağlık; §4, satır 140, 303, 309–311 | NaN/Inf ve negatif kovaryans köşegeni, diğer `EST_BAD` nedenleriyle birlikte 2 s bekletiliyor. Bu sırada kontrolün hangi veriyi kullanacağı belirtilmemiş. Ayrıca pozitif köşegenler kovaryansın pozitif yarı tanımlı olduğunu kanıtlamaz. | Sayısal çökme aynı çağrıda kontrol/atış iznini kaldırıp sonlu, güvenli çıktı üretmeli. Geçici yenilik reddi için kullanılan bekleme bundan ayrı olsun. Kovaryans geçerliliği uygun ayrıştırma veya eşdeğer denetimle kontrol edilsin. | kritik |
| 7 | §2.8, satır 214–217; §4 IMU kaybı | SIL sanal saati son alınan çerçeveden türetiliyor; işlem ise IMU gelince ilerliyor. IMU kesilmesinde sanal saati ilerleten ve `tick_wall` çağıran olay tanımlanmamış. Bariyer de yanıt beklerse test kilitlenebilir. | IMU üretiminden bağımsız sanal zaman/sağlık olayları tanımlansın. IMU kesilirken zaman ilerlemeli, duvar çağrıları yapılmalı ve arıza çıktısı alınabilmeli; kullanıcı duraklatması ise bundan ayrılmalı. | kritik |
| 8 | §3.1 `ida_core_tick_wall`; §4 | Duvar çağrısı yalnız `now_w` alıyor. IMU yokken gelen `DISARM`, bağlantı olayı veya başka bir sensör örneği çekirdeğe aktarılamıyor. Bu çağrının çıktısındaki IMU sıra numarasının anlamı da yok. | IMU gerektirmeyen olay/sağlık girdisi tanımlansın. Çağrının hangi modülleri çalıştırdığı, çağrı periyodunun üst sınırı, çıktı geçerliliği ve asenkron güvenli çıktının A1’de nasıl kabul edildiği belirtilsin. | önemli |
| 9 | §2.2 tazelik; §4, satır 293–309 | Ölçüm zamanı duran veya geriden gelen HIL akışında, örnek yaşı da geriden gelen IMU’ya göre küçük kalabilir. Gecikmiş IMU’lar düzenli teslim edilirse “10 ms akış yok” koşulu da tetiklenmeyebilir. `DUP_SEQ`/`BAD_INPUT` gelişlerinin canlılığı yenileyip yenilemediği belirsiz. | Ölçüm tazeliği yanında yerel alım zamanı, son kabul edilmiş IMU ilerlemesi ve HIL zaman ilerleme sapması denetlensin. Yinelenen, eski ve geçersiz örnekler geçerli veri canlılığını yenilemesin. Farklı saat alanları doğrudan çıkarılmasın. | kritik |
| 10 | §2.6 ve §4 arıza tablosu | Bağlantı kaybı `DEPTH_HOLD` ve `STATION_KEEP` gibi kiplerde tanımsız. `HOLD` eyleminin kesin davranışı yok. “Sürerse”, “tekrar” ve “sensör başına sınır” ifadelerinin süre/eşik karşılığı verilmemiş; coğrafi sınır denetimi anılıyor ama tanımlanmıyor. | Her kip × arıza için eylem ve ön koşul matrisi verilsin. Kestirim yetersizken `HOLD`/`SURFACE` yedeği, arıza temizleme koşulları, batarya kritik beklemesi, derinlik tekrar sayacı, bayatlık sınırları ve coğrafi sınır parametreleri tanımlansın. | önemli |
| 11 | §2.2 adım sırası, satır 91–96; §3.6 izin | Atış modülü, operatör komutlarını uygulayan kip yöneticisinden önce çağrılıyor. Aynı girdideki `DISARM` veya atışa izin vermeyen kip değişikliği, atış iznine bir adım geç yansıyabilir. Çekirdek tetik çıktısını aynen geçiriyor. | Arm/disarm ve izin daraltan komutlar önce uygulansın; atış modülü o adımın güncel iznini görsün. Son çıktının güncel arm, kip ve arıza izniyle tutarlı olması 02–03 sözleşmesinde zorunlu kılınsın. | kritik |
| 12 | §3.1 `DUP_SEQ`, satır 232, 238; §6 | Önceki çıktının yeniden gönderilmesine izin veriliyor; önceki çıktıda `fire[2]` bulunabilir. Adım başında sıfırlama, önbellekteki çıktının yeniden gönderilmesini güvenli yapmaz. Duvar arızasından sonra eski motor çıktısının tekrar gönderilmesi de engellenmemiş. | Sürekli aktüatör komutu ile tek seferlik tetik olayı ayrılmalı. Tetikte olay kimliği ve alıcıda yinelenenleri eleme zorunlu olsun. Yeniden gönderim güncel güvenlik durumunu aşamasın; eski çıktının kullanım süresi sınırlansın. | kritik |
| 13 | §3.1, §3.4, §3.6 | A4 alan listeleri tamamlanmamış: komut türlerinin çoğunun yük alanları, setpoint çerçevesi/birimleri, komut kimliği/yanıtı, olay kayıtları ve platform sağlık alanları yok. `scan_leg_active` ile `ida_align_status_t` anlatılıyor ama iletim yerleri verilmemiş; arka plan imzası `...` ile bırakılmış. | İçerik ve işlev imzası düzeyinde sözleşme tamamlansın. Özellikle manuel altı eksenin kuvvete ölçeklenmesi, setpoint türleri, hizalanma yanıtı, tarama bayrağı ve hata dönüşlerinin çıktı semantiği tanımlansın. | önemli |
| 14 | §3.2–3.3, satır 245, 252–262 | Gerçek donanımda damganın “veri hazır anındaki kart saati” olması, DVL için tanımlanan yansıma anıyla çelişiyor. IMU damgasının aralık başı/sonu olduğu, montaj dönüşümlerinin nerede uygulandığı ve isteğe bağlı DVL kovaryansının nasıl işaretlendiği açık değil. | Ölçüm geçerlilik zamanı ile teslim/alım zamanı ayrı tutulsun. Sensör saatinin ortak ölçüm zamanına eşlemesi, IMU aralık semantiği, dönüşüm sahibi, alan türleri/birimleri ve isteğe bağlı alan geçerliliği tanımlansın. Böylece gerçek sürücü aynı anlamları koruyabilir. | önemli |
| 15 | §2.2, §2.7, §3.1, §3.6 | Sonraki adıma kalan örneklerin kuyruk kapasitesi ve taşma önceliği yok. Sonar işaretçisi yalnız çağrı boyunca geçerli; arka plan algılamasının onu nasıl saklayacağı belirtilmemiş. Arka plan ile hızlı adım aynı `fc` durumuna erişiyor. `ida_core_t` için statik ayırma biçimi/hizalama da verilmemiş. | Sabit kapasiteler, kopyalama/sahiplik, taşma davranışı ve arka plan–adım eşzamanlılık sözleşmesi verilsin. Kamuya açık tam durum tipi veya derleme zamanı boyut/hizalama sözleşmesi sağlansın. `ida_core_sizeof()` tek başına statik ayırma sözleşmesi değildir. | önemli |
| 16 | §3.1 tekrar üretim iddiası; §7 | Yalnız adım girdisinin kaydı bit düzeyinde tekrar için yeterli deniyor. Oysa `tick_wall`, arka plan atış işlemi, başlangıç parametreleri ve bu işlemlerin sırası da durumu değiştirebilir. | Başlangıç durumu/parametreleri ve çekirdeği etkileyen bütün çağrıların sıralı izi kaydedilsin. Arka plan sonuçlarının hangi adımda tüketildiği deterministik olsun. İşaretçi içeren girdiler içerikleriyle kaydedilsin. | önemli |
| 17 | §2.3 GNSS sıfırlaması; §3.6 geçmiş | Konum sıfırlamasının anten kolu düzeltmesi, hız durumu ve eski seyrüsefer geçmişiyle ilişkisi tanımlanmamış. Sıfırlama sınırının iki yanındaki durumları ara değerlemek yapay poz üretebilir. | Sıfırlanan konum gövde referansına dönüştürülsün; hız ve çapraz kovaryans politikası belirtilsin. Geçmiş sorgusu reset sınırını bildirsin, sınır üzerinden ara değerlemesin; ufuk dışı sorgu hata dönsün. | önemli |
| 18 | §2.5 tahsis, satır 169–171 | Ağırlıklı sözde tersin ağırlıkları, birimleri, kuvvet sınırları ve doyma giderme algoritması tanımlanmamış. 04’teki yatay iticiler roll/pitch momenti de üretir; yüzeyde dikey iticileri sınırlamak tam altı eksen tahsisini etkiler. | Rejime özgü eksen hedefleri ve itici sınırlarıyla tahsis tanımlansın. Ağırlıkların anlamı, rank/kötü koşullanma denetimi, sınırlı yineleme sayısı ve doyma sonrası elde edilen kuvvet/moment hesabı verilsin. | önemli |
| 19 | §2.5 itki eğrisi ve ölü bant | `T ∝ n·abs(n)` ilişkisinden PWM komutunun karekök olması doğrudan çıkmaz; `n` devir hızıdır. 04’ün geçici polinomu veya tablo modeliyle aynı ters dönüşüm garanti edilmiyor. `abs(u)=25/400` ise tanımlanan dur bandının sınırındadır. | 04 ile aynı sürümlü durağan eğrinin monoton tersi kullanılsın veya kontrol modeli farkı açıkça tanımlansın. Ölü bant sınırının dahil olup olmadığı ve PWM nicemlemesi belirtilsin; sıfırdan farklı en küçük komut sınırın dışına düşmeli. | önemli |
| 20 | §2.2 ve §2.5 kontrol tablosu | Hız döngüleri §2.2’de 100 Hz, §2.5’te 400 Hz iç döngü olarak görünüyor. NED dikey hızından gövde `Z` kuvvetine dönüşüm ve integralin rejim/kazanç değişimindeki aktarımı eksik. Doyma bayraklarının nasıl üretildiği verilmemiş. | Tek hız tablosu kullanılsın. Çerçeve dönüşümleri ve yüzerlik ileri beslemesi açıklansın. İntegral dondurma, istenen ve tahsis sonrası elde edilebilir eksen çıktısı arasındaki kalıntıdan türetilsin; bütün ilgili integral durumlarının geçiş davranışı tanımlansın. | önemli |
| 21 | §2.5 “Pasif yüzeye çıkışın hızı”, satır 173 | Yaklaşık 0,1 m/s yalnız karesel sönümden geliyor. 04’ün aynı parametre setindeki doğrusal heave sönümü bu düşük hızda baskın; ihmal sonuçta iki kattan fazla hata yaratıyor. | Tam batık, durgun tatlı su için yaklaşık 0,047 m/s kullanılsın. Yüzeye yaklaşma, yönelim ve dalga etkilerinin bu denge hesabının dışında olduğu belirtilsin. | önemli |
| 22 | §2.2 hesap bütçesi; §6 Joseph yedeği | Ortalama ara toplam doğru; fakat genel yoğun Joseph yedeğinin adım başına sekiz güncellemeyle 1 ms hedefine uyduğu gösterilmemiş. Tahsisin “48 çarpma” maliyeti de yalnız doymasız matris çarpımıdır. | Ortalama sensör yükü ile izin verilen en kötü yığılma ayrı bütçelensin. Genel Joseph’e geçiş, gerekirse güncelleme sınırını veya hızı değiştiren koşullu seçenek olsun. Tahsis, kapılama, beyazlatma, reset ve arka plan kesilmeleri bütçeye eklensin. | önemli |
| 23 | §3.5 parametreler | Alan başına aralık denetimi geometrinin veya kovaryansın geçerliliğini sağlamaz. Çalışırken geometrinin değiştirilmesi yasaklanmamış; buna karşılık sözde ters yalnız devre dışıyken yenileniyor. CRC/sürüm hatasından sonra arm engelinin nasıl kaldırılacağı belirsiz. | Parametreler değiştirilebilirlik sınıflarına ayrılsın; ilişkili değişiklikler atomik doğrulansın. Tahsis ve türetilmiş değerler tutarlı yenilensin. CRC’nin açık kodlanmış kalıcı biçim üzerinde olduğu ve geçerli parametreye dönüş yolu belirtilsin. | önemli |
| 24 | §7 kabul ölçütleri | Sayısal toleransların ve senaryo başarı sınırlarının ilk sonuçlar görüldükten sonra seçilmesi, kusurlu davranışı kabul sınırına dönüştürebilir. Kritik zaman/arıza yolları için de kesin davranış testleri eksik. | Ayar verisi ile bağımsız kabul senaryoları ayrılsın. Kabulden önce süre, doğruluk ve emniyet sınırları dondurulsun. IMU’suz SIL, gecikmiş HIL, aynı adımda disarm/atış, tetik tekrarı, reset sınırı ve kovaryans zaman eşlemesi testleri eklensin. | önemli |
| 25 | §2.7 ve §6 | CMSIS başlıkları yasaklanırken CMSIS-DSP yedek olarak öneriliyor. Ayrıca GNU linker’ın `--wrap` denetimi için macOS/MSVC kapsamı açıklanmamış. | CMSIS-DSP kullanılacaksa bağımlılık sınırı ve aynı matematik arayüzünün SIL karşılığı tanımlansın; [Arm başlığı](https://raw.githubusercontent.com/ARM-software/CMSIS-DSP/main/Include/arm_math_types.h) hedef derlemede CMSIS derleyici başlığına başvuruyor. GNU bağlantı denetiminin çalışacağı yapılandırma ile diğer platform denetimleri ayrıştırılsın. | küçük |
| 26 | §8.2, satır 360–365 | İlk üç iskelet önerisi mevcut iskelet §4’te zaten bulunuyor. Geciken adım politikası da bütünüyle açıkmış gibi bırakılmış; iskelet bütçe aşımında son komutla ilerleme ve çalıştırmayı geçersiz sayma kararını vermiş. | Güncel iskelete göre eski öneriler kaldırılıp kalan gerçek açıklıklar yazılsın. 400/200 Hz için 2,5/5 ms periyotları ve 1,25 ms fizik adımıyla iki/dört adım ilişkisi açıkça kaydedilsin. | küçük |

## Yeniden hesaplananlar

**1. ESKF boyutları ve öngörü**

Nominal durumun depolama boyutu `3+3+4+3+3=16`; kuaterniyonun üç serbestlik derecesi nedeniyle hata durumu `3+3+3+3+3=15` boyutludur. Bu seçim tutarlıdır.

Sağdan yerel tutum hatası, `R=R_nb`, `f=a_m−b_a` ve `ω=ω_m−b_g` için kendi türetimim:

\[
\dot p=v,\qquad
\dot v=Rf+g_n,\qquad
\dot q=\tfrac12q\otimes[0,\omega].
\]

\[
\dot{\delta p}=\delta v,
\]

\[
\dot{\delta v}=-R[f]_\times\delta\theta-R\delta b_a-Rn_a,
\]

\[
\dot{\delta\theta}=-[\omega]_\times\delta\theta-\delta b_g-n_g.
\]

Bias’lar random walk seçilirse hata sıralamasındaki sürekli zaman Jacobian’ı:

\[
A=
\begin{bmatrix}
0&I&0&0&0\\
0&0&-R[f]_\times&0&-R\\
0&0&-[\omega]_\times&-I&0\\
0&0&0&0&0\\
0&0&0&0&0
\end{bmatrix}.
\]

Bütün bloklar `3×3` boyutundadır. Ayrık geçiş `Φ` kullanılarak:

\[
P^-=\Phi P^+\Phi^\mathsf T+Q_d,\qquad
Q_d=\int_0^{\Delta t}\Phi(\tau)LQ_cL^\mathsf T
\Phi(\tau)^\mathsf T\,d\tau.
\]

Belgede `F` adıyla kullanılan matrisin bu ayrık geçiş olduğu açıklanmalı. IMU aralığının büyümesi veya kaybolması halinde `Φ` ve `Q_d` aynı süreyi temsil etmelidir.

**2. Ölçüm modelleri, kapılama ve reset**

Basınçtan derinlik dönüşümünün boyutu doğrudur:

\[
(P-P_{\rm atm})/(\rho g)\quad\rightarrow\quad {\rm m}.
\]

Sağdan yerel hata için basınçtan türetilen derinlik ölçümünün Jacobian’ı:

\[
H_d=
\begin{bmatrix}
e_3^\mathsf T&0&-e_3^\mathsf TR[l_p]_\times&0&0
\end{bmatrix}.
\]

DVL modeli, `R_dvl` DVL çerçevesinden gövdeye dönüşüm ve `l_dvl` gövde referansından DVL’ye kol ise boyutsal ve işaret bakımından tutarlıdır:

\[
h_{\rm DVL}=R_{\rm dvl}^{\mathsf T}
(R^\mathsf Tv+\omega\times l_{\rm dvl}).
\]

Burada hız referans noktası tanımlanmalıdır. Durum IMU merkezindeyse kollar da o noktadan ölçülmeli veya IMU–gövde kolu ayrıca modellenmelidir.

`m` boyutlu ölçümde `H: m×15`, `S: m×m`, `K:15×m` olur. Belgede verilen `yᵀS⁻¹y` kapısı doğrudur; `γ` ilgili ölçüm boyutuna göre seçilmelidir. `R=LLᵀ` için beyazlatma hem yeniliğe hem Jacobian’a uygulanır:

\[
y'=L^{-1}y,\qquad H'=L^{-1}H.
\]

Ardışık skaler güncellemelerde sonraki yenilikler önceki düzeltmeleri hesaba katmalıdır.

Verilen reset bloğunun eksi işareti, sağdan yerel hata enjeksiyonuyla uyumludur:

\[
G_\theta\simeq I-\tfrac12[\widehat{\delta\theta}]_\times.
\]

Bu, birinci mertebe yaklaşımdır. `G` sıfırlama öncesindeki **kestirilmiş düzeltmeyle** kurulmalı; `δx=0` işleminden sonra sıfır vektöründen hesaplanmamalıdır. Bu konvansiyon [Solà’nın yerel hata ve reset türetimiyle](https://arxiv.org/pdf/1711.02508) uyumludur.

**3. Gecikmiş ölçüm için karşı örnek**

Kendi skaler kontrolüm: geçmiş durumda `P₀=1`, geçmişten şimdiye bağımsız süreç gürültüsü `Q=1`, ölçüm varyansı `R=1` olsun. Güncel önsel varyans `P₁=2`, geçmiş–şimdi çapraz kovaryansı `1` olur.

Geçmiş ölçümün güncel duruma doğru kazancı:

\[
K_{1,0}=\frac{1}{1+1}=0,5,\qquad
P_1^+=2-\frac{1^2}{2}=1,5.
\]

Geçmiş yeniliğe güncel kovaryanstan olağan kazanç uygulanırsa:

\[
K=\frac{2}{2+1}=\frac23,\qquad
P^+=2-\frac{4}{3}=\frac23.
\]

Sonuç gereğinden fazla güvenlidir. Yalnız geçmiş pozu bulmak yeterli değildir. Belgede yedek gösterilen [PX4 yöntemi](https://docs.px4.io/main/en/advanced_config/tuning_the_ecl_ekf), ölçümleri gecikmeli ufukta birleştirip tamponlanmış IMU ile güncel çıktı üretir.

**4. İşlemci yükü**

Hedefin 160 MHz Cortex-M33 ve tek duyarlıklı FPU özellikleri [ST ürün belgesiyle](https://www.st.com/en/microcontrollers-microprocessors/stm32u5a9nj.html) uyumludur. Aşağıdakiler işlem sayısı hesabıdır; kart ölçümü değildir.

İki yoğun `15×15` matris çarpımı:

\[
N_\times=2(15^3)=6750,
\]

\[
N_+=2(15^2)(14)=6300.
\]

`Q_d` eklemesi ayrıca 225 toplama getirir; toplam **6750 çarpma + 6525 toplama** olur.

Skaler Joseph formunu açarsak, `u=Phᵀ`, `s=hu+r`, `k=u/s` için:

\[
P^+=P-ku^\mathsf T-uk^\mathsf T+s\,kk^\mathsf T.
\]

Bu yapı yoğun kübik çarpımlar gerektirmez; maliyet kareseldir. Belgede kullanılan yaklaşık `4n²=900 MAC eşdeğeri` varsayımıyla:

| Aritmetik modeli | 100 Hz yayılım + 200 skaler güncelleme/s | 3–6 çevrim/MAC varsayımıyla CPU |
|---|---:|---:|
| Optimize skaler | 855.000 MAC/s | %1,603–3,206 |
| Her skalerde iki yoğun Joseph çarpımı | 2.025.000 MAC/s | %3,797–7,594 |

Bu ara toplamlar yeniden hesaplandığında doğrulanıyor. Ancak bölümdeki sensör hızlarından **200 skaler/s** yükünün nasıl seçildiği açıklanmalı; bu sayı bir kapasite garantisi değildir.

Sekiz güncelleme ile bir kovaryans yayılımının aynı adıma gelmesi halinde:

| Aritmetik modeli | Adım başına yaklaşık MAC | 3–6 çevrim/MAC ile süre |
|---|---:|---:|
| Optimize skaler | 13.950 | 0,262–0,523 ms |
| Genel yoğun Joseph | 60.750 | 1,139–2,278 ms |

Genel Joseph yedeği, bu varsayımlarla bile **1 ms hedefini aşar**. Sürelere nominal entegrasyon, Jacobian, kapılama, beyazlatma, reset, bellek erişimi ve diğer modüller dahil değildir. Ayrıca `-ffp-contract=off` altında MAC eşdeğeri tek bir fused komut sayılmaz. [Arm’ın M33 zamanlama açıklaması](https://community.arm.com/forums/f/architectures-and-processors-forum/43612/cycle-count-for-a-subroutine-on-cortex-m33/158767) da gerçek çevrim sayısının ölçülmesini gerektiriyor.

**5. Sekiz itici tahsisi**

`docs/mimari/04-simulator-cekirdegi.md` satır 184–191’deki geometriden, her sütunu `[ε_i; r_i×ε_i]` alarak yeniden kurduğum matris:

\[
B\simeq
\begin{bmatrix}
 .7071& .7071& .7071& .7071&0&0&0&0\\
-.7071& .7071& .7071&-.7071&0&0&0&0\\
0&0&0&0&1&1&1&1\\
 .0601&-.0601&-.0601& .0601& .218&-.218& .218&-.218\\
 .0601& .0601& .0601& .0601&-.120&-.120& .120& .120\\
-.1888& .1888&-.1888& .1888&0&0&0&0
\end{bmatrix}.
\]

Satır sırası `[X,Y,Z,K,M,N]`; kuvvet girdisi Newton’dur. İlk üç satır boyutsuz, moment satırları metre boyutundadır. Eliminasyonla **rank(B)=6** çıktı; sınırsız ve arızasız durumda altı eksen tahsisi mümkündür.

Yatay grubun bağlaşımı:

\[
K=-0,085\,Y,\qquad M=0,085\,X.
\]

Dolayısıyla yatay kuvvet üretirken sıfır roll/pitch momenti istenirse dikey iticiler telafi yapar. Yüzeydeki dikey yetki sınırı tahsise dahil edilmelidir.

`8×6` matris–vektör çarpımı **48 çarpma ve 40 toplama** gerektirir. Bu yalnız doymasız doğrusal bölümdür; eğri tersleme, sınırlar ve doyma giderme dahil değildir.

**6. Pasif yükselme**

04’teki tatlı su parametreleri:

\[
F_B=(1000\cdot0,0137-13,5)\cdot9,82=1,964\ {\rm N}.
\]

Doğrusal ve karesel heave sönümüyle yükselme hızının büyüklüğü:

\[
33v+190v^2=1,964,
\]

\[
v=\frac{-33+\sqrt{33^2+4\cdot190\cdot1,964}}{380}
\simeq\mathbf{0,0469\ {\rm m/s}}.
\]

Yalnız karesel terim kullanılırsa `0,1017 m/s` çıkar. Farkın nedeni doğrusal sönümün bu hızda baskın olmasıdır. Karşı kontrol olarak `33v+190v²=120` denklemi `0,7126 m/s` verir; 04’teki yaklaşık `0,71 m/s` ile uyumludur.

**7. Sayı çözünürlüğü ve zaman oranları**

- 10.000 m’de binary32 aralığı: `2^(13−23)=0,0009765625 m`, yani **0,9765625 mm**. Bölüm doğru düzeltmeyi kullanmış.
- 3600 s’de binary32 aralığı: `2^(11−23)=0,000244140625 s`, yani **0,244140625 ms**. Mutlak zamanı tamsayı tutma gerekçesi doğru.
- Fizik adımı **1,25 ms**; 400 Hz kontrol periyodu **2,5 ms = iki fizik adımı**, 200 Hz periyodu **5 ms = dört fizik adımı**.
- **10 ms**, 400 Hz’te dört; 200 Hz’te iki periyottur. Eşik sabit süre mi, dört periyot mu olacak, ayrılmalı.
- 5 Hz’te beş ardışık örneğin ilkinden sonuncusuna **0,8 s** geçer; yaklaşık 1 s ifadesi ilk örneği bekleme fazını içeriyorsa geçerlidir.
- GNSS tamsayı farkı genişletilmiş türde alınmalı ve boylam sarılması uygulanmalı. ±180° sınırında `int32_t` değerleri doğrudan çıkarmak taşabilir.

## İskelete ya da diğer bölümlere notlar

**İskelet uyumu:** Sekiz ana başlık mevcut. Bölüm 9’daki 02 kapsamı büyük ölçüde karşılanıyor; A4 doğru numaralandırılmış, A1/A5/A6 atıfları uygun. Atış kontrolün iç algoritması ve kart donanımı yazılmamış; bunlardan beklenenler belirtilmiş. İskeletin sensör seti korunmuş; kamera yerine dış algılama kaydı alınması doğru. İç basınç ve sıcaklığın açık konu olarak bırakılması kapsam genişletme sayılmaz.

**06 düzeltmeleri:** 06.1–06.4, 06.9, 06.13 ve 06.14’teki yanlış MAVLink/protokol iddiaları bölüme taşınmamış; ilgili ayrıntıların çoğu 06’nın kapsamında. 06.5’in yük hesabı koşullu tahmin olarak aktarılmış, fakat en kötü adım bütçesi eksik. 06.6–06.8’in tek duyarlık tercihi, doğru ULP ve HIL saat ayrımı korunmuş. 06.10’daki sızıntının arm/eylem bağımlılığı, 06.11’deki IMU ile kontrol hızı ayrımı ve 06.12’deki QGC kabul ayrımı doğru kullanılmış.

**08 düzeltmeleri:** 08.1–08.9’un bu bölümde kullanılan kısımları bakımından eski yanlış iddiaların tekrarına rastlanmadı; donanıma ilişkin çoğu madde kapsam dışında. 08.10–08.11’deki MAVLink ve açık serileştirme düzeltmelerine aykırı bir tel protokolü önerilmiyor. Ancak kalıcı parametre biçimi ve kayıt biçimi ayrıca tanımlanmalı. 08.12’nin zamanlama/taşma/bayatlık gereklilikleri anılmış fakat tamamlanmamış. 08.13–08.15’in bellek denetimi, FMA sınırı ve ölçülmemiş kapasite uyarıları esas olarak korunmuş.

**09 düzeltmeleri:** 09.5–09.6’daki üretim FP politikasını koruma ve platformlar arası bit eşitliği beklememe düzeltmeleri doğru kullanılmış. 09.15’in QGC ayrımı ve 09.16’nın C17 tercihi korunmuş. Diğer maddelerin ilgili yanlış genellemeleri kullanılmıyor; bu bölümde konu edilmeyen her düzeltmenin ayrıca tekrar edilmesi gerekmiyor.

**Diğer bölümlerin ihtiyaçları:** 01 ile duvar çağrısının sıklığı, yerel zaman damgaları ve watchdog canlılık sözleşmesi; 03 ile güncel izin, tek seferlik tetik, arka plan sahipliği ve geçmiş sorguları; 05 ile ölçüm zamanı/kovaryans semantiği; 06 ile IMU’suz zaman ilerlemesi, asenkron güvenli çıktı ve yeniden bağlanma kimliği birlikte netleştirilmeli. 08, fizik gerçeği ile otopilot parametrelerini ayrı tutmalı; 09 bağımsız kabul sınırlarını sahiplenmeli.

`docs/mimari/04-simulator-cekirdegi.md` hâlâ bazı yerlerde **1 ms** fizik adımı kullanıyor. Güncel `docs/mimari/00-iskelet.md` uyarınca bunlar **1,25 ms** ile eşitlenmeli.

Ölçülmemiş yük ve bellek değerleri genel olarak tahmin/hedef diye işaretlenmiş; kapasite garantisi verilmemiş. Ardıl kart, gerçek sensör tedariki veya yarışma kuralından gereksinim türetilmemiş. Rust önerisi yok; tarihsel `match` atfı C durum tablosuyla açıklanmış. Hiçbir dosya değiştirilmedi, hiçbir şey kurulmadı.