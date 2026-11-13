# sol incelemesi: 03-atis-kontrol

## Hüküm

Bölüm bu haliyle kabul edilmemeli. Sekiz başlıklı yapı, modül sınırları, C17 seçimi, algılama düzeyindeki arayüz ve araştırma düzeltmelerinin çoğu uygundur. Ancak iskelet 0.5’in itkili torpido gereği uygulanmamış; olay iptali, reset ve yeniden bağlantı, kilitli arıza, geri okuma tazeliği ve takip belirsizliği konusunda kabulü engelleyen açıklıklar vardır. Sayısal örneklerin çoğu yeniden üretilebiliyor; bunlar tasarımın uygulanabilirliğini veya emniyetini kanıtlamıyor. Ayrıca bölüm, güncel 02’de giderilmiş bazı sorunları hâlâ açık gösteriyor. Aşağıdaki düzeltmeler ve bölümler arası sözleşmeler tamamlandıktan sonra yeniden incelenmeli.

## Düzeltilmesi gerekenler

Satır numaraları, aksi belirtilmedikçe `docs/mimari/03-atis-kontrol.md` dosyasına aittir.

| # | Yer (başlık ya da satır) | Sorun | Doğrusu / önerilen düzeltme | Önem (kritik / önemli / küçük) |
|---|---|---|---|---|
| 1 | §2.5, 149–156; §8.3–8.4, 436–442 | İlk sürüm yalnız pasif torpidoyu kapsıyor; sürekli itki sonraya bırakılmış. İskelet 0.5 §5, ayrılma sonrasında süreli sabit itkiyi ve yaklaşık 3 m/s, 10 m mertebesindeki hedefi ilk modele alıyor. | Tablo üretimi, parametre özeti, uçuş süresi ve menzil değerlendirmesi itkili modeli kapsamalı. Pasif hesap yalnız önceki seçimin gerekçesi olarak tutulmalı. §8.3’teki seçenekler güncel kararla kapatılmalı. | kritik |
| 2 | §2.6, 197–206 | `FAULT_LATCHED` yalnız `FC_RESET` ile terk edilir deniyor; fakat “her durumdan” çalışan `FC_DISARM`, izin kaybı veya duvar çağrısı onu `SAFE` yapabiliyor. Ardından yeniden silahlandırma mümkün. | Arıza mandalı, güvenli çıktı üretiminden ayrı korunmalı. Evrensel güvenli davranış mandalı temizlememeli; yalnız tanımlı reset koşulları temizlemeli. Eşzamanlı arıza/reset/disarm öncelikleri yazılmalı. | kritik |
| 3 | §2.1, 75; §2.7, 227–230 | Aynı adımda yeni `FC_FIRE` reddediliyor; önceki adımda üretilmiş, olay kutusunda veya taşıma kuyruğunda bulunan atışın iptali tanımlanmıyor. `SAFE`’e geçiş, yoldaki olayı kendiliğinden geri alamaz. | İptal edilen yetkiye bağlı bekleyen olayların kartta, 06’da ve simde akıbeti tanımlanmalı. Alıcı, uygulama anında iptal edilmiş yetkiye ait olayı reddedebilmeli. Test, yeni komutun yanında önceden gönderilmiş olayı da kapsamalı. | kritik |
| 4 | §2.7, 227; §2.8, 252 | “Daraltan önce” sıralaması tek başına yeterli değil: aynı adıma gelen `EMNIYET_KAPAT`, ardından eski `EMNIYET_AC` ve `ATESLE` ile etkisizleştirilebilir. | Aynı adımda daraltmanın genişletici olaylarla geri alınamayacağı ve eski yetkinin yeniden açılamayacağı alıcı kuralı tanımlanmalı. Senaryo kaynaklı emniyet değişiklikleri de bu sıraya dahil edilmeli. | kritik |
| 5 | §2.7, 228, 239; §3.1, 277, 287–288 | `(boot_id, olay_id)` biçimi var; `boot_id`’nin iskeletteki **açılış sayacı** olarak üretimi, güç kaybı, sayaç taşması ve yeniden bağlantıda tekilleştirme belleğinin korunması yok. `boot_salt` aynı sözleşme değildir. Komut ve jeton kapsamı da eksik. | Açılış sayacının sahibi, üretimi ve bozulma davranışı 01/06 ile belirlenmeli. Aynı açılıştaki yeniden bağlantı eski olayları yeni yapmamalı. Komut kimliği, jeton ve çözüm sırasının oturum/reset kapsamı açık olmalı. | kritik |
| 6 | §2.7, 221, 229; §3.1, 287 | İskelet her olaya son geçerlilik isterken yalnız `ATESLE` için bu alan var. Gecikmiş `EMNIYET_AC` alındığında yeniden 200 ms izin açıp açmadığı belirsiz. Ayrıca emniyet süresi sim zamanında; iskeletin emniyet süreleri duvar saatinde. | Bütün olayların geçerlilik anlamı tanımlanmalı. İzin ömrünün başlangıcı açık olmalı; yeniden teslim ömrü uzatmamalı. Modelin sim zamanındaki süresi ile HIL’deki emniyet gözetiminin duvar süresi ayrılmalı. | kritik |
| 7 | §2.6, 194–195; §4, 343 | Atış 20 ms’de geçersizleşiyor, ilk ACK zaman aşımı 50 ms. Dolayısıyla zaman aşımıyla yapılan ilk tekrar zaten uygulanamaz. `FIRING`’de izin yenilenmiyor; kalan izin ömrü de tanımlı değil. Ret yanıtlarının durum geçişleri eksik. | Uygulama son tarihi, ACK bekleme süresi ve yeniden gönderme politikası birlikte tanımlanmalı. `GEÇ`, `İZİNSİZ`, `ENGEL`, `BOŞ_TÜP` ve önceki ret sonucuna ait `YİNELEME` ayrı işlenmeli. | önemli |
| 8 | §2.5, 158–173; §2.6, 193–194 | Çözüm 50 Hz hesaplanıyor; atış adımında yalnız açısal hızın yeniden denetlendiği açık. Aradaki izin, iz, parametre, hizalanma ve geri okuma değişikliklerinin eski çözümü nasıl geçersizleştirdiği eksik. | Çözümün dayandığı sürümler ve geçerlilik koşulları belirlenmeli. Atış adımında bütün hızlı engeller ve çözümün güncelliği denetlenmeli. Her ölçüt kaybında jeton, pencere ve dwell davranışı yazılmalı. | kritik |
| 9 | §2.6, 179–206 | “Her durum × olay tanımlı” denmesine rağmen tablo yalnız normal çıkışları veriyor. Genel ölçüt kaybı, `SELECT`/`ARM`’ın farklı durumlardaki etkisi, `TEST_ENTER/EXIT`, eşzamanlı olay öncelikleri ve bazı `FIRING` kesintileri tanımsız. | Tam geçiş tablosu veya genel ret/öncelik kuralları verilmeli. `TEST`’in evrensel izin koşullarıyla ilişkisi ve kuru testte beklenen ret yanıtları açıklanmalı. | önemli |
| 10 | §3.1, 289; §4, 345–346 | Fırlatıcı geri okumasında alım zamanı, geçerlilik ve bayatlık davranışı yok. Eski “dolu/emniyet açık” verisi kullanılabilir. “Fiş takılı ve acil durdurma etkin” ise doğal olarak mümkün bir durum; başlı başına çelişki değildir. | Geri okumanın ölçüm ilerlemesi ve alım tazeliği denetlenmeli; bilinmeyen durum atışı engellemeli. Gerçek çelişki, girdilerin birleşiminden beklenen mekanizma durumuna göre tanımlanmalı. | kritik |
| 11 | §2.4, 125; §2.5, 152 | `Q_nav`’ı yalnız `p_sigma`/`att_sigma` artışından üretmek zamanlar arası korelasyonu belirlemez. Marjinal σ sabitken göreli hata büyüyebilir. Mutlak seyrüsefer belirsizliğini bütünüyle atış ölçütünden çıkarmak genel olarak gerekçelendirilmemiş. | Hedef–seyrüsefer çapraz kovaryansı ve zamanlar arası hata modeli tanımlanmalı; alternatif yaklaşım muhafazakâr sınırla gerekçelendirilmeli. Schmidt filtresi de yalnız ofset durumu eklemekten ibaret veya otomatik olarak “kesin” değildir. [Birincil consider-filter çalışması](https://ntrs.nasa.gov/api/citations/20120010515/downloads/20120010515.pdf). | kritik |
| 12 | §2.3, 107; §2.4, 123–127 | Ölçüm, montaj ve zaman belirsizliği anılıyor; `R_eff`, Jacobian’lar ve ortak hata bileşenlerinin tutulduğu yer tanımlı değil. Sabit montaj hatasını her güncellemede bağımsız saymak da yapay küçülme üretir. | Aktif ölçüm türü için belirsizlik dönüşümü, çapraz terimler ve ortak kalibrasyon hatalarının modeli yazılmalı. Yaklaşımın tutarlılık sınırı belirtilmeli. | önemli |
| 13 | §2.4, 143; §3.1, 281, 286 | Reset kaydırması için Δ gerekiyor, fakat arayüz yalnız sayaç taşıyor. Δ yoksa kovaryans büyütmenin miktarı belirsiz. Konum, hız ve tutum değişiklikleri ayrılmamış; eski kümeler ve işler de resetten sonra sonuç üretebilir. | Reset bildirimi ve iz/kovaryans dönüşümü tanımlanmalı; yeterli bilgi yoksa etkilenen izler yeniden başlatılmalı. Reset öncesi iş, küme ve sıralama kayıtları geçersizleştirilmeli. | önemli |
| 14 | §3.2, 302–304; §4, 335 | `RB2` ve `B2` için kullanılmayan kovaryans satırı sıfır; buna rağmen 3×3 Cholesky zorunlu. Bu matrisler tekildir ve geçerli iki bileşenli kayıtlar reddedilir. | Türün aktif 2×2 veya 3×3 alt matrisi denetlenmeli. Üst üçgen kodlamasında “simetrik değil” denetiminin anlamı da açıklanmalı. | önemli |
| 15 | §3.2, 296–309; §4, 333–336 | Aynı kamera karesindeki birden çok algılama aynı `t_capture` taşır; “aynı üreticide zaman ilerlemiyor” kuralı ikinci kaydı reddedebilir. Parti/kare kimliği yok; üretici başına sıra da çoklu sensör örneklerini ayırmıyor. | Kare/parti kimliği ve kayıt sırası ayrılmalı. Donma denetimi tekil hedef kayıtlarına değil kare/profil ilerlemesine uygulanmalı. Kimlik kapsamı kaynak, örnek ve üretici yaşam döngüsünü içermeli. | önemli |
| 16 | §2.3, 105; §2.4, 123 | Sonarın yükseliş ölçmediği kabul edilmiş; fakat dünya kümesi tekrar `RB2`’ye indirgenirken “ileri/geri dönüşüm birbirini götürür” deniyor. Farklı pozlardan toplanan 3B kümenin iki bileşene indirgenmesi tersinir değildir. | Eksik yükselişin temsil biçimi ve bunun küme merkezi/kovaryansa etkisi tanımlanmalı. ±12,5° önseli ölçülmüş yükseliş veya mutlak görünürlük sınırı sayılmamalı. | önemli |
| 17 | §2.3, 105–107 | Araç hareketi telafi ediliyor; hareketli hedefin tarama süresindeki hareketi telafi edilmiyor. `t_sigma = yayılım/√12`, ağırlıklı küme merkezinin hata modeli yerine kullanılamaz. `J_t` de yalnız araç hareketinden kurulmuş. | Kümenin uzamsal yayılımı, hedef hareketi, zaman belirsizliği ve referans anı ayrı modellenmeli. “Yayılım”ın tam aralık mı standart sapma mı olduğu açıklanmalı. | önemli |
| 18 | §2.3, 100, 105 | 03 kutu merkezini `(i+½)Δr` alıyor; 05 §2.4.3 kutuyu `iΔr` ile tanımlıyor. Küme eşiğindeki “2 kutu” ile metre cinsindeki diğer terim de açıkça dönüştürülmemiş. | Örnek anı/kutu merkezi sözleşmesi 05/06 ile birleştirilmeli. Eşik `2Δr` gibi tek birimde yazılmalı. Üretici protokolü yarım kutu ofsetini tek başına doğrulamıyor. [Ping360 protokolü](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/). | önemli |
| 19 | §2.4, 131 | 60 ms tamponun yakalama zamanına göre bir eşik mi, teslimden sonra ek bekleme mi olduğu belirsiz. Geçmişteki ölçümle güncel zamana yayılmış iz kovaryansının nasıl eşlendiği yazılmamış. | Takip zaman ufku, serbest bırakma kuralı, eşit zaman sırası ve geç kayıt politikası tanımlanmalı. Durum, kovaryans ve Jacobian aynı ölçüm anında buluşmalı. | önemli |
| 20 | §2.4, 119–143 | `CV6` başlangıç hızı/kovaryansı ve süreç gürültüsünün ayrık modeli yok. Kaçırılmış kamera fırsatı için boş kare/görünürlük bilgisi arayüzde yok. `RECONFIRM` için silme koşulu belirtilmemiş. | Başlatma, model değişimi, fırsat kimliği, negatif gözlem, kaçırma ve bütün iz durumlarının silme kuralları tamamlanmalı. Aynı taramadaki ilişkili gözlemler bağımsız onay kanıtı sayılmamalı. | önemli |
| 21 | §2.4, 129 | GNN’nin yeni iz kukla maliyeti tanımlı değil; ilişkilendirme sonucu buna doğrudan bağlı. Farklı ölçüm türleri birlikte kullanılırsa yalnız `d² + ln det(S)` yeterli olasılık sözleşmesi değildir. | Yeni iz/yanlış alarm ve kaçırma maliyetleri, sınıf uyumu ve parti türleri tanımlanmalı. Farklı boyutlar karşılaştırılıyorsa normalleştirme korunmalı. | önemli |
| 22 | §2.5, 152, 162; §2.8, 249 | Hedef yönelimi rastgeleleşebiliyor; fakat iz yalnız konum/hız tutuyor ve algılama hedef düzlemi yönelimi taşımıyor. Dolayısıyla “hedef düzlemindeki” hata genel durumda hesaplanamaz. İki eksen sınırı da keyfî isabet bölgesine içerilmeyi garanti etmez. | Desteklenen hedef geometrisi ve bilinen/kestirilen yönelim ayrılmalı. İsabet bölgesi, torpido boyutu ve belirsizlik bölgesinin geometrik ilişkisi tanımlanmalı; yer gerçeği FC’ye taşınmamalı. | önemli |
| 23 | §2.5, 173 | Gecikme kovaryansında matris `Σ_v τ²` ile skaler `(ωτR)²` toplanıyor. Bilinen açısal hareket ile hareket kestirim hatası karıştırılmış; gecikme belirsizliğinin katkısı açık değil. | Boyutları tutarlı Jacobian’lar kullanılmalı; bilinen hareket öngörüye, hız/tutum/zaman hataları kovaryansa girmeli. Toplam gecikme sensör yolları için ayrı kurulmalı. | önemli |
| 24 | §2.7, 219–223; §7, 407 | Çalışma sırasında fiş takılı, pim çıkık ve acil durdurma pasifken üç sanal engel de kaldırılmıştır. Ortak bir kart arızası geçerli biçimli açma/atış olayları üretirse bu katmanlar atışı engellemez. | Emniyet iddiası işletim durumuna göre sınırlandırılmalı. Sanal engellerin kartça değiştirilememesi ile bütün engeller açıkken tek kart arızasına dayanıklılık aynı güvence olarak sunulmamalı. Simde bağımsız denetlenen koşullar açıkça listelenmeli. | kritik |
| 25 | §2.8, 252–255 | Sanal girdilerin kart dışında olması doğru gerek; fakat 04/05/06’da tamamlanmış karşılığı yok. Aynı fizik adımındaki senaryo değişikliği ile atışın sırası da 04 §2.1’de belirsiz kalıyor. | Simde yetkili emniyet durumu ile karta gönderilen bozulabilir geri okuma ayrılmalı. A1 çıktıları yetkili girdileri değiştirememeli; olay uygulama anında yetkili durum denetlenmeli. | kritik |
| 26 | §2.9, 260–264 | FreeRTOS yedeğinde yalnız bellek bariyeri yeterli değil. Süresi dolmuş iş düşürülürken çalışan işçinin yuvaya yazmayı bırakması ve yuvanın yeniden kullanımı tanımsız. | Senkronize sahiplik devri, iş nesli ve iptal/tamamlanma protokolü tanımlanmalı. RTOS’suz tek bağlamda yarış yok; bu sonuç iki görevli yedeğe taşınamaz. [C bellek modeli, §5.1.2.4](https://open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf). | önemli |
| 27 | §2.9, 262–266; §5, 360 | Sabit `due_step`, yalnız tüketim tarihini sabitler; sonucun yetişip yetişmediği HIL zamanlamasına bağlıdır. SIL her işi bitirerek ilerlerken HIL’de düşen işi yeniden üretebilir. “Telemetri” kademesinin gerekli kayıtları içerdiği de gösterilmemiş. | Yeniden oynatma, kaydedilmiş tüketildi/düştü kararını uygulamalı. Tam FC girdileri, duvar çağrıları, resetler ve gerekli sonar/algılama içeriği kayıt sözleşmesine bağlanmalı. `bg_late` kaydı varsa bunun hangi tekrar güvencesini koruduğu ayrılmalı. | önemli |
| 28 | §3.1, 284–285; §2.3, 115 | FC komut kapasitesi 4, güncel 02’nin kapasitesi 8. Kesme/bölme sırasında `DISARM`’ın düşmesi veya sonraki adıma kalması mümkün. İz, tepe, küme, ACK ve olay kutusu taşma politikaları da eksik. | Sınırlar uçtan uca eşlenmeli; daraltıcı komutlar kapasite taşmasında kaybolmamalı. Her tampon için deterministik taşma ve güvenli davranış yazılmalı. | kritik |
| 29 | §3.3, 315 | “Diğerleri `ARMED` altında” ifadesi belirsiz. Geometri, kalibrasyon veya tablo değişikliği sürmekte olan iş, iz, çözüm ve jetonları geçersizleştirebilir; güncel 02 bazılarını yalnız devre dışıyken değiştiriyor. | İzin verilen durumlar açık listelenmeli. Parametre grupları atomik uygulanmalı; etkilenen iş/iz/çözüm ve yetkiler geçersizleştirilmeli. 02 ile aynı değiştirilebilirlik sınıfları kullanılmalı. | önemli |
| 30 | §2.3, 115; §7, 411 | Ortalama profil yükü hızlı kısmın en kötü süresini kanıtlamıyor. Küme kapatma, dönüşümler, 8×16 kapılama, atama ve filtre güncellemelerinin aynı adıma yığılması sınırlandırılmamış. | Adım başına iş sınırları ve ertelenme davranışı tanımlanmalı. 150 µs hedefi en kötü dolulukta ölçülmeli; bellek hesabına atama çalışma alanı ve tüm kuyruklar eklenmeli. | önemli |
| 31 | §7, 387, 397–408 | Rastgele 8×24 atamaları kaba kuvvetle karşılaştırmak olağan birim testi için aşırı pahalı. F3, otonom GCS kaybında devam kuralıyla çelişiyor. F8, `FIRING`’de zaten uygulanmış fırlatmayı geri alınabilirmiş gibi ele alıyor. | Küçük boyutta tam arama, büyük boyutta bağımsız çözüm/doğrulama kullanılmalı. A1 ve GCS kaybı ayrılmalı. Emniyet enjeksiyonu, olayın gerçek uygulanma adımından önce/sonra ayrı sınanmalı. | önemli |
| 32 | §8.1, 420–423; §3.1 | Güncel 02 sürüm 0.2, `fire[2]` yerine olaylar kullanıyor; kip/arıza FC’den önce uygulanıyor; reset sınırı ve hizalanma yanıtı var. Bölüm eski incelemeyi güncel durum gibi aktarıyor. İki belge ayrıca farklı arka plan imzası ve olay sahipliği tanımlıyor. | Eski sürüme ait bulgular kapanmış olarak ayrılmalı. Güncel 02 §3.7 ile FC arayüzü birleştirilmeli; olay kimliğini kimin ürettiği ve ACK’nin nereye döndüğü tekleştirilmeli. | önemli |
| 33 | §2.3, 107; §3.1; §8.2 | `D07 N8` dönüşümün dayanağı değil; ilgili sonuç N6. Güncel 02’de FC arayüzü §3.7, §3.6 parametrelerdir. İskelet 0.5’te karşılanmış öneriler hâlâ açık gösteriliyor. | Atıflar ve sürüm kaydı güncellenmeli; kabul edilmiş iskelet değişiklikleri öneri listesinden ayrılmalı. | küçük |

## Yeniden hesaplananlar

Bunlar bağımsız hesap ve sözleşme denetimleridir; kart performans ölçümü değildir.

**CFAR ve kapılama**

Bağımsız, aynı ortalamalı üstel güç örnekleri için eğitim toplamının dağılımından:

\[
P_{fa}=(1+\alpha/N)^{-N}
\quad\Rightarrow\quad
\alpha=N(P_{fa}^{-1/N}-1).
\]

Formül doğru. `N`, iki taraftaki **toplam** eğitim hücresi sayısı olmalıdır. Bölüm sayısal `N` ve `Pfa` seçmediği için tek bir sayısal α yeniden hesaplanamaz. Örnek olarak `N=20`, `Pfa=10⁻³` seçilirse `α=8,250751` çıkar; bu bölümün seçilmiş parametresi değildir. Ping360 verisinde nominal oranın garanti olmadığı uyarısı doğru aktarılmış. [MathWorks CFAR açıklaması](https://www.mathworks.com/help/phased/ug/constant-false-alarm-rate-cfar-detection.html).

| Hesap | Sonuç | Bölümle karşılaştırma |
|---|---:|---|
| %99, 2 serbestlik derecesi | `−2 ln(0,01) = 9,210340` | 9,210 doğru |
| %99, 3 serbestlik derecesi | `11,344867` | 11,345 doğru |
| Açgözlü örnek | `1+100=101` | Doğru |
| Küresel en iyi atama | `2+2=4` | Doğru |
| Tam 8×24 bire bir atama olasılıkları | `24!/16! = 29.654.190.720` | Kaba kuvvet testinin boyutu uygun değil |

Ki-kare sonuçları [NIST tablosuyla](https://www.itl.nist.gov/div898/handbook/eda/section3/eda3674.htm) uyumlu. Kovaryans veya ilişkilendirme modeli tutarsızsa bu eşikler %99 kapılama başarımı sağlamaz. NIS kabul ölçütü de yalnız kapıdan geçen örneklerin ortalaması üzerinden kurulacaksa seçim yanlılığı dikkate alınmalıdır.

**Pasif torpido örneği**

Bölümün seçtiği `ρ=1000 kg/m³`, `C_D=0,42`, `A=0,001257 m²`, `m_e=0,1849 kg`, `u₀=2 m/s` için:

\[
b=\tfrac12\rho C_D A=0,263970\ \mathrm{kg/m},
\qquad
L=m_e/b=0,700458\ \mathrm m,
\]

\[
u(s)=u_0e^{-s/L},
\qquad
t(s)=\frac{L}{u_0}(e^{s/L}-1).
\]

| Mesafe | Süre | Hız | Bölüm |
|---|---:|---:|---|
| 1 m | 1,109818 s | 0,479750 m/s | ≈1,1 s ve ≈0,48 m/s doğru |
| 1,5 m | 2,630856 s | 0,234968 m/s | ≈2,6 s doğru |

1 m’deki sürüklemesiz düşüş hesabı, **düşey etkin kütle** `m_z=0,175+0,1498=0,3248 kg` kullanılırsa:

\[
z=\frac{0,073}{2(0,3248)}(1,109818)^2
=0,138414\ \mathrm m.
\]

Dolayısıyla ≈0,14 m yeniden üretilebiliyor. Eksenel `0,1849 kg` kullanılırsa aynı sonuç çıkmaz; düşey etkin kütle açıkça yazılmalı. Bu, başlangıç düşey hızı sıfır olan sadeleştirilmiş hesaptır; tam kanatlı 6-SD yörünge doğrulaması değildir.

Bu denklemler pasif modeli doğrular. İskelet 0.5’in itkili modeli için uçuş süresi veya etkili menzil henüz hesaplanmış değildir. `10/3≈3,33 s`, yalnız 10 m boyunca sabit 3 m/s kabulünün kinematik ölçeğidir; hızlanma, akıntı ve itki süresini doğrulamaz. Pasif modelin yavaşlamasından tek başına kesin “etkili menzil 1 m” sınırı da çıkarılamaz; bunun için isabet/son hız ölçütü gerekir.

**Sonar, zamanlama ve işlem bütçesi**

| Hesap | Sonuç | Değerlendirme |
|---|---:|---|
| 400 Hz kontrol | 2,5 ms; 2 fizik adımı | Doğru |
| 200 Hz kontrol | 5 ms; 4 fizik adımı | Doğru |
| 50/25 Hz çözüm | 20/40 ms | Doğru |
| `K_bg=8`, 400 Hz | 20 ms | Doğru |
| `K_bg=16`, 400 Hz | 40 ms | Yedek gecikmesi iki katına çıkar |
| `K_bg=4`, 200 Hz | 20 ms | Doğru |
| 7 m akustik dinleme | `2×7/1500=9,333 ms` | Doğru |
| Yalnız akustik üst hız | 107,143 profil/s | ≈107 doğru; gerçek profil hızı değildir |
| 7 m / 1200 kutu | 5,833 mm/kutu | Doğru |
| 03–05 yarım kutu farkı | 2,917 mm | Sözleşme farkı giderilmeli |
| 0,1–0,3 ms × 107,143/s | %1,071–3,214 işlemci süresi | ≈%1–3 uygun yuvarlama |
| 160 MHz’te 0,1–0,3 ms | 16.000–48.000 çevrim | “Birkaç on bin” ile uyumlu; ölçülmedi |
| 150 µs × 400 Hz | %6 işlemci süresi | FC hızlı kısmının hedef yükü |
| 4 yuva, 20 ms bekleme | 7 m akustik üst hızında ortalama 2,143 profil | Ortalama kapasite uygun görünebilir; yığılma kanıtı değil |

`sample_period×25 ns`, zaman boyutundadır; `c_p` ile çarpılıp ikiye bölünmesi menzil verir. Darbe kaynaklı koruma genişliği `c_p·tx_duration/2` de boyutsal olarak doğrudur. Bu alanların tel birimleriyle SI dönüşümü tek yerde tanımlanmalıdır. [Ping360 alan tanımları](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/).

**Toplam gecikme**

Bölümden tek bir toplam gecikme çıkarılamıyor. Özellikle `D_sıra`nın anlamı sonucu değiştiriyor:

- Yakalama zamanına göre serbest bırakma eşiğiyse, teslim/arka plan gecikmesiyle örtüşür; hepsi doğrudan toplanmaz.
- Teslimden sonra ayrıca 60 ms bekletmeyse, arka plan gecikmesine eklenir.

İkinci yorumda, bütün kalemlere ayrı üst faz payı ayıran örnek toplam:

\[
L_{\text{teslim}}+2,5+20+60+20+2,5+(2,5+1,25)
=L_{\text{teslim}}+108,75\ \mathrm{ms}.
\]

Bu yalnız belirtilen varsayımlarla yapılan toplamdır; küme kapanma süresi ve operatör beklemesi dahil değildir. Kamera yolunda sonar arka plan işi bulunmaz. Durum makinesi çözümün hesaplandığı aynı adımda çalışıyorsa ayrıca 2,5 ms eklemek de gerekmeyebilir. Önce olayların zaman çizgisi kesinleştirilmeli.

`Σ_v τ²` konum kovaryansı boyutundadır. `(ωτR)²` de m² verir; fakat skalerin matrise hangi doğrultuda eklendiği ve `ω`nın neden belirsizlik sayıldığı yazılmamış. Bilinen hareketin telafisi ile telafi hatasının kovaryansı ayrılmalıdır.

**Bellek**

Verilen yaklaşık kalemlerin toplamı:

\[
5+5+3+4+2,5+1=20,5\ \mathrm{KB}.
\]

Bu toplama göre `<32 KB` aritmetik olarak mümkün; tamamlanmış bütçe değildir.

| Kalem | Bağımsız hesap |
|---|---|
| Dört ham profil | `4×1200=4800 B`; poz kopyaları ve yuva üst verisi hariç |
| 1200 `float` CFAR alanı | 4800 B |
| Bir `CV6` durum ve tam kovaryansı | `(6+36)×4=168 B`; kimlik, zaman ve yaşam döngüsü hariç |
| 16 izin yalnız durum/kovaryansı | 2688 B |
| 8×24 `float` maliyet matrisi | 768 B; algoritma çalışma alanı hariç |
| Algılama kaydının sıkıştırılmamış alan toplamı | 68 B |
| Listelenen sırayla, yaygın 8 bayt hizalamalı C yerleşimi | 80 B/kayıt; 32 kayıt = 2560 B = 2,5 KiB |

Son satır bir ABI örneğidir; kart ve iki PC derlemesindeki gerçek `sizeof` değerleri ayrıca doğrulanmalıdır. Yörünge tablosunun satır sayısı ve bütün kapasite sınırları verilmediği için 1 KB tablo ve toplam üst sınır doğrulanamıyor.

**Diğer süreler ve gözetim**

| Parametre | Değer | 400 Hz karşılığı |
|---|---:|---:|
| Atış son geçerliliği | 20 ms | 8 periyot |
| ACK bekleme | 50 ms | 20 periyot |
| Sıralama gecikmesi | 60 ms | 24 periyot |
| Arka plan gözetimi / nominal IWDG | 100 ms | 40 periyot |
| Süreli izin | 200 ms | 80 periyot |
| Atış penceresi | 300 ms | 120 periyot |
| Algılama üst yaşı / tekleme beklemesi | 500 ms | 200 periyot |
| FC kalp atışı / değerlendirme | 1 s | 400 periyot |
| Silahlandırma | 2 s | 800 periyot |
| Jeton | 10 s | 4000 periyot |
| İz güncellemesiz silme | 30 s | 12.000 periyot |
| Hizalanma | 60 s | 24.000 periyot |

Bunlar sürelerin aritmetik karşılığıdır; sim ve duvar saatinin birbirine dönüştürülmesi değildir. 200 Hz’te periyot sayıları yarıya iner, süreler korunmalıdır.

IWDG formülüyle:

\[
t_{\min}=4/32000=125\ \mu s,
\qquad
t_{\max}=4096×1024/32000=131,072\ s.
\]

Bölümün ≈125 µs–131 s düzeltmesi doğrudur. Gerçek süre LSI frekansına bağlıdır; [ST sürücüsü](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx-hal-driver/main/Src/stm32u5xx_hal_iwdg.c) de bunu belirtir. 100 ms çalışma süresi hâlâ ölçülmemiş seçimdir.

`T_ack=50 ms` nedeniyle 20 ms ömürlü atışın ilk zaman aşımı tekrarı zaten geçtir. Yenileme her kontrol adımında yapılırsa 32 bit olay sayacı 400 Hz’te yaklaşık 124,28 günde taşar; taşma davranışı sözleşmede bulunmalıdır.

**Belirsizlik ve geometri karşı örnekleri**

İki zamanın konum hatası için:

\[
\operatorname{Var}(\delta p_2-\delta p_1)
=P_2+P_1-C_{21}-C_{12}.
\]

`P₁=P₂=1` iken tam korelasyonda sonuç 0, korelasyon yoksa 2’dir. İki durumda da marjinal σ artışı sıfırdır. Dolayısıyla yalnız σ artışından `Q_nav` üretmek yeterli değildir.

Dünya dönüşümünün nominal ifadesi ve ters ölçüm dönüşümü doğrudur:

\[
p_W=p_B+R_{WB}(R_{BS}p_S+t_{BS}).
\]

Fakat nominal dönüşümün doğru olması kovaryansın doğru taşındığını göstermez; ortak hatalar ve çapraz kovaryanslar ayrıca gerekir.

Dairesel isabet bölgesinde eksen başına sınırlar da yeterli değildir: `e_y=e_z=0,9ρ` iki eksen koşulunu sağlar, fakat merkeze uzaklık `1,273ρ` olur ve dairenin dışındadır. `ρ_hedef`in geometrik anlamı kesinleştirilmelidir.

## İskelete ya da diğer bölümlere notlar

**İskelet 0.5’e uyum**

| Konu | Durum |
|---|---|
| Sekiz başlıklı yapı | Uygun |
| Parçalar ve sınırlar | Genel olarak uygun; torpido hesabı örnek ölçek olarak verilmiş, fizik uygulaması 04’e bırakılmış |
| A1 ve A4 | Doğru arayüzler seçilmiş; olay, geri okuma ve algılama sözleşmeleri tamamlanmamış |
| A2 | Kamera üretimi 07’ye bırakılmış; uygun |
| A3, A5, A6, A7 | Panel, senaryo, kayıt ve varlık sorumlulukları genel olarak doğru yerlere verilmiş |
| SI, NED ve sensör–gövde dönüşümü | Temel geometri uygun; kuaterniyon ve sabit gövde referansı ortak kabullerine açık atıf eklenmeli |
| Sürekli komut / olay ayrımı | Doğru yönde; bütün olayların geçerliliği, açılış sayacı ve iptal davranışı eksik |
| Daraltıcı komutların önceliği | Kart içi sıra tanımlı; alıcıda eski yetkinin geri gelmesini önleyen kural eksik |
| İki saat ve tazelik | Genel ilke doğru; süreli izin ve geri okuma/algılama alım tazeliği tamamlanmalı |

**0.5 ile gelen değişikliklerin ayrı durumu**

1. **İtkili torpido ve menzil hedefi:** Uygulanmamış. §8.4’te açıkça ilk sürüm dışına çıkarılması bağlayıcı iskeletle çelişiyor.
2. **Sanal emniyet girdileri:** Bölümde doğru yerde, sim sunucusunda tanımlanmış. Karşılıkları 04/05/06’da tamamlanmalı.
3. **Fırlatıcı durum geri okuması:** Alanlar önerilmiş; tazelik, geçersizlik ve yetkili sim durumundan ayrımı eksik.
4. **Açılış sayacı + olay sayacı:** İkili kimlik biçimi var; açılış sayacının gerçekleştirme sözleşmesi yok. 06’nın rastgele `boot_id` tanımıyla da uyumsuz.
5. **Profilde gönderim zamanı, kutuda yansıma zamanı:** 03’ün yaklaşımı güncel iskeletle uyumlu. §8.1/8.2’de artık çözülmüş konu gibi kaydedilmeli.
6. **Yakın bölgede kamera ve iz öngörüsü:** Uyumlu. İkinci öne bakan menzil ölçer seçeneği §8.3’ten kaldırılmalı; güncel iskelet bunu seçmiyor.

**Komşu belgeler**

- `docs/mimari/02-ortak-cekirdek.md` **sürüm 0.2**, eski adım sırasını ve `fire[2]` gösterimini düzeltmiş. `docs/mimari/inceleme/02-sol.md` ile yazar yanıtı güncel metinle birlikte okunmalı; eski bulgular otomatik olarak açık sayılamaz. Buna karşılık olay alanları, ACK dönüşü, FC arka plan imzası ve izin genişletmeleri hâlâ ortaklaştırılmalı.
- Güncel 02’nin duvar çağrısı yolunda `fcif` çağrısı açıkça görünmüyor; 03 ise bu çağrıda `SAFE` olmayı bekliyor. IMU’suz çağrının FC’ye nasıl ulaştığı yazılmalı.
- `docs/mimari/01-kart-yazilimi.md` hâlâ tetik alanlarını sıfırlayan eski gösterimi kullanıyor. `.noinit` açılış sayacı, güç kaybında benzersizliğin tek başına kanıtı değildir.
- `docs/mimari/04-simulator-cekirdegi.md` hâlâ sürekli itkiyi sonraya bırakıyor. Olaylarda geçerlilik, yanıt ve tam emniyet durumu eksik. `docs/mimari/inceleme/04-sol-2.md` içindeki torpido referansı ve darbe modeli açıklıkları da tablo üretimi için kapanmış varsayılamaz.
- `docs/mimari/05-sensorler-ve-sonar.md` ile kutu merkezi, gönderim/yansıma pozunun kullanımı, son 8 bit çıktının anlamı ve sade/tam kademedeki darbe yanıtı birleştirilmeli. `docs/mimari/inceleme/05-sol.md` bu model açıklıklarını ayrıca bildiriyor; CFAR kalibrasyonu bunları kendiliğinden çözmez.
- `docs/mimari/06-protokol-ve-hil.md` içindeki yükselen kenarlı ateşleme hâlâ iskelete aykırı. Olay önceliği, geçerlilik, oturum, ACK ve geri okuma mesajları eklenmeli. `docs/mimari/inceleme/06-sol.md` bulguları bu konuda geçerliliğini koruyor.
- 07, 08 ve 09 mimari belgeleri henüz mevcut değil. Bunlara bırakılan kamera algılaması, kayıt içeriği ve kabul sayıları tamamlanmış sözleşme sayılamaz.

**00 düzeltmelerinin madde bazında durumu**

| Dayanak | Bölümdeki durum |
|---|---|
| 07.1 — Kamera bağlantısından “imkânsızlık” sonucu çıkarılması | Eski yanlış tekrarlanmıyor. Kameranın PC’de çalışması iskelet seçimi olarak kullanılıyor. |
| 07.2 — TinyML için NPU/GPU zorunluluğu | Tekrarlanmıyor. |
| 07.3 — U5 görüntü işleme süreleri | Doğrulanmamış eski süreler kullanılmıyor. |
| 07.4 — Ölçülmüş menzil / tasarım hesabı ayrımı | Genel olarak korunmuş; “etkili menzil ≈1 m” sonucu ayrıca başarı ölçütü gerektiriyor. Yarışma kısmı gereksinim yapılmamış. |
| 07.5 — CFAR varsayımları | Doğru aktarılmış. Kalibrasyonun geçerliliği kullanılan sensör/alıcı modeline bağlı kalıyor. |
| 07.6 — GNN ve alfa-beta eşdeğerlikleri | Eski yanlışlar düzeltilmiş; yeni atama maliyeti ve test boyutu açıklıkları var. |
| 07.7 — Dış kalibrasyon ve seyrüsefer belirsizliği | Dönüşüm doğru aktarılmış; korelasyonun uygulanabilir matematik modeli tamamlanmamış. |
| 07.8 — Uçuş, düşüş ve akıntı | Eski yanlış düşüş/sürüklenme rakamları tekrarlanmıyor. Yeni pasif hesap yeniden üretilebiliyor; güncel itkili modeli karşılamıyor. |
| 07.9 — Piksel ölçeği | Eski iyimser değerler kullanılmıyor. |
| 07.10 — IWDG aralığı | Doğru aktarılmış. |
| 07.11 — Serbest çalışan PWM ve bağımsız engeller | Temel düzeltme doğru aktarılmış; bütün sanal engeller kaldırılmışken kalan güvence sınırı yeterince açık değil. |
| Ç5 — Kamera algılayıcısının yeri | Algılama düzeyindeki sınır korunmuş; kartta imkânsızlık iddiası yok. |
| M8 — Kartta kamera/TinyML ölçümü | Bu mimaride kamera PC’de olduğundan ilk kabulün ön koşulu yapılması gerekmiyor; ölçüm yapılmış gibi sunulmuyor. |
| M19 — Watchdog/emniyet | Doğru referans verilmiş; gerçek IWDG süresi ve sim tarafındaki kilitlenme/reset davranışı henüz ölçümle kapanmış değil. |

K1–K10 kapsamındaki araç, kart, dil, platform, simülasyon ve yarışma dışı çalışma sınırları korunmuş. İki fırlatıcı ve emniyet girdileri yarışma zorunluluğu olarak gerekçelendirilmemiş. Yeni performans ve bellek rakamları genel olarak tahmin diye işaretlenmiş; sorun etiketleme değil, bu tahminlerin henüz eksiksiz kapasite ve zamanlama hesabına dayanmamasıdır.