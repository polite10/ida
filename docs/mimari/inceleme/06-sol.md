# sol incelemesi: 06-protokol-ve-hil

## Hüküm

Bölüm bu haliyle kabul edilemez. Sekiz başlıklı yapı, temel sorumluluk sınırları ve araştırma düzeltmelerinin çoğu uygundur; mesaj boyutlarının büyük bölümü de doğru hesaplanmıştır. Ancak iskelet 0.3’ün zorunlu zaman çerçevesi ve kimlikle tekilleştirilen olay sözleşmesi uygulanmamış; adım bütünlüğü, güvenli çıktının önceliği ve yeniden bağlanma davranışı eksik kalmıştır. VCP yedeğinin gecikme hesabı yeterliliği göstermiyor; Gilbert–Elliott modelinin süre formülü de verilen ayrık uygulamayla uyuşmuyor. Kritik maddeler giderilmeli, bütçeler tamamlanmış mesaj kümesiyle yenilenmeli ve bölüm yeniden incelenmelidir. Bu incelemede dosya değiştirilmedi, kurulum yapılmadı.

## Düzeltilmesi gerekenler

Satırlar, aksi belirtilmedikçe `docs/mimari/06-protokol-ve-hil.md` dosyasına aittir.

| # | Yer (başlık ya da satır) | Sorun | Doğrusu / önerilen düzeltme | Önem (kritik / önemli / küçük) |
|---|---|---|---|---|
| 1 | §2.2, §2.4, §2.6; 167, 175, 224 | IMU hem adım tetikleyicisi hem parti sonlandırıcısı. Bölüm ayrıca sim zamanının **yalnız örneklerle** taşındığını söylüyor. İskelet §4’ün IMU’dan bağımsız zaman çerçevesi yok. IMU kesilince SIL sanal saati ve bariyer yanıtı üretilemez; 5 saniyelik gerçek zaman gözetimi bu senaryoyu çözmez. | Her kontrol periyodunda sıra numarası ve sim zamanı taşıyan zaman çerçevesi tanımlansın. IMU’suz partinin tamamlanması, SIL sanal saatinin ilerlemesi, `tick_wall` çağrısı ve bariyer yanıtı açıkça tanımlansın. Kullanıcı duraklatması ayrıca işaretlensin. Yeni mesaj bütçelere eklensin. | kritik |
| 2 | §2.2; 80–87, §3; 355 | Atış, `launcher` içindeki seviye ve yükselen kenarla temsil ediliyor. Benzersiz olay kimliği ve alıcı tekilleştirmesi yok. İskelet §3.2’ye aykırı. Kaybolan düşük seviye sonraki atışı gizleyebilir; araya güvenli düşük çıktı girdikten sonra gecikmiş yüksek seviye yeniden kenar oluşturabilir. | Sürekli itici komutu ile atış/silahlandırma olayları ayrı taşınsın. Olay kimliği, oturum kapsamı, tekrar gönderme, kabul/uygulama yanıtı ve alıcı tekilleştirmesi tanımlansın. Fiziksel tetik darbesinin süresi, olay kabulünden sonra yürütülsün. | kritik |
| 3 | §2.4; 171, 175, 183, §2.7; 258, §4; 371 | Sürekli komutun kullanım süresi sınırsız. Bağlantı trafiği sürerken aktüatör yanıtları sürekli kaybolursa son itki süresiz tutulabilir. Adım dışı güvenli çıktının “bariyer dışında uygulanması”, eski/yinelenen çıktının reddiyle uzlaştırılmamış. Güvenli çıktıdan sonra önbellekteki normal çıktı tekrar uygulanabilir. | Komuta geçerlilik süresi ve güvenlik nesli tanımlansın. Asenkron güvenli çıktı ayrı kimlikle kabul edilsin; eski normal çıktılar bunu geri alamamalı. Bariyer kaçırmasında son komut yalnız geçerlilik süresi boyunca tutulmalı. | kritik |
| 4 | §2.2 ortak başlık, §2.6 | `H` yalnız ölçüm zamanı taşıyor. Yerel alım zamanı, onun hangi noktada alınacağı ve A4’e aktarılması tanımsız. `t_report` sensörün raporlama zamanıdır; alım zamanı değildir. Eski/geçersiz/yinelenen veriyle canlılığın yenilenmemesi de uygulanabilir kurala bağlanmamış. | Ölçüm zamanı, modellenmiş teslim zamanı ve gerçek yerel alım zamanı ayrıştırılsın. Alım damgasının saat alanı ve alma noktası belirlensin. Tazelik, kabul edilmiş ölçüm ilerlemesi ve yerel alım üzerinden denetlensin; farklı saat alanları doğrudan çıkarılmasın. | kritik |
| 5 | §2.4; 165–173 | `n_aux` yalnız çerçeve sayısını doğruluyor. Bir MAG kaybolup bir BARO yinelenirse sayı doğru kalabilir. Ayrıca sonar DATA mesajlarında `step_seq` yok; birkaç adıma yayılan parçaların parti sayımına nasıl katıldığı tanımlanmıyor. | Parti üyeleri benzersiz kimliklerle doğrulansın; yinelenenler sayımı tamamlamasın. Manifest/üyelik özeti veya eşdeğer sözleşme tanımlansın. Sonlandırıcı kaybı, eksik parti, sıra bozulması ve gelecek parti verisinin tamponlanması açık olsun. | kritik |
| 6 | §2.1; 54, §2.4; 167, §2.2 GCS mesajları | GCS mesajı ilk sonraki IMU’ya atanıyor. IMU kaybolursa veya çerçeveler yer değiştirirse aynı komut başka adıma taşınabilir. Standart mesajlarda uygulama adımı/oturum zarfı yok. İzin daraltan komutların atıştan önce uygulanacağı da A1↔A4 eşlemesinde zorunlu kılınmamış. Yalnız `sysid` ile yönlendirme, mesaj türü ve hedef denetimini karşılamıyor. | Standart GCS içeriği korunarak HIL teslim zarfında oturum, uygulama adımı ve komut kimliği taşınsın. Kaynak, hedef ve izin verilen mesaj türleri birlikte doğrulansın. Disarm ve izin daraltma aynı adımda atış değerlendirmesinden önce uygulansın. | kritik |
| 7 | §2.5; 215–218 | Yeniden birleştirme için parça varlık bitmap’i, yinelenen parça davranışı, indis sınırları ve tampon sahipliği yok. `frag_idx=255` gibi bir değerin reddi alan sözleşmesinde bulunmuyor. Yeni HEAD’in önceki profile ait olup olmadığı da yalnız 16 bit kimliğe dayanıyor. | `n_samples`, müzakere edilmiş azami boyut, `frag_idx`, parça uzunluğu ve HEAD durumu doğrulansın. Aynı parça iki kez tamamlanma sayısını artırmasın; çelişkili tekrar reddedilsin. Oturum/örnek/profil anahtarı ve tampon bırakma sözleşmesi tanımlansın. | önemli |
| 8 | §2.4; 160–162, §2.5; 216–220 | Parti `t_n`’e kadar teslim edilecek veriden kuruluyor, fakat toplu kısmın IMU(n−1)’den hemen sonra gönderildiği söyleniyor. O anda gelecekte hazır olacak veriye erişilemez. Ayrıca sonar ve algılama aynı toplu pencereyi paylaşmasına rağmen kapasite hesabı yalnız profile dayanıyor. | Hazır olma, kuyruğa alma, iletme ve çekirdeğe teslim adımları ayrı tanımlansın. Yalnız önceden hazır veri gönderilsin; bir adımlık ek gecikmenin başlangıç noktası açık olsun. Sonar ve algılama ortak bütçe ve öncelik altında hesaplansın. | önemli |
| 9 | §2.3, §2.5, §6, §7; 455 | VCP’de profil hızının sınırlanacağı söyleniyor; uygulanabilir sınır verilmemiş, tanımlı davranış kuyruk dolunca profil atmak. Yaklaşık bir DATA/adım bütçesi 200 Hz’te 117 profil/s’yi taşıyamaz. Ayrıca en kötü yük, 3 Mbaud kapasitenin %50’sini aşmama kabulüne de sığmıyor. | Taşıma başına izin verilen sensör/GCS yükü ve profil hızı hesaplanıp koşu öncesinde doğrulansın. Yetersiz yapılandırma açıkça reddedilsin veya sensörün modellenmiş çalışma ayarı değiştirilsin; sürekli kuyruk kaybı olağan çalışma kabul edilmesin. | önemli |
| 10 | §2.4; 185–196, §6 | 444 B kritik bölüm hesabı aktüatör yanıtını dışarıda bırakıyor. 200 Hz’te GCS’nin adım başına ortalama payı iki katına çıkıyor. 200 Hz taşıma payındaki 3,45/2,45 ms hesabı da verilen formülden çıkmıyor. Bu nedenle “VCP ≥2 Mbaud +200 Hz” yedeğinin yeterliliği gösterilmemiş. | Aşağıdaki yeniden hesap esas alınsın. İstek, yanıt, oturum trafiği, kuyruk beklemesi ve kart servis süreleri birlikte bütçelensin. GCS için ortalama hız yanında azami patlama sınırı tanımlansın. | önemli |
| 11 | §2.4; 190–194; komşu 01 §2.4 | Bütçede kart için yalnız 1 ms çekirdek hesabı var. `01-kart-yazilimi.md` alımdan çıktı yazımına yaklaşık 1,57 ms öngörüyor. Ayrıştırma, girdi kurma, kodlama ve bekleyen ana döngü işi 1,1 ms’lik taşıma payında açıkça karşılanmıyor. | 01 ve 06 tek uçtan uca zaman çizelgesi kullansın. 01’in tahminleriyle birincil durumda taşıma/PC payı yaklaşık **0,53 ms** kalır. M1 geri döngüsü ile M5 kart servis ölçümünün sınırları ayrıştırılsın. | önemli |
| 12 | §2.4; 181–194, §3 `StepBarrier` | `L_act=2,5 ms` yedeği iki isteği örtüştürüyor; fakat arayüz tek bir `await(n)` ve tek “beklenen seq” üzerinden anlatılmış. Bekleyen çağrı sürerken sonraki partinin üretimi/gönderimi ve yanıtların tutulması tanımlı değil. `W_n`’nin ilk bayt mı, yazma tamamlanması mı olduğu da belirsiz. | Sınırlı uçuş penceresi, her isteğin son tarihi, gönderim sahibi ve yanıt tamponu tanımlansın. Fizik ile G/Ç’nin ilerleme sırası gösterilsin. Son tarihler sabit `(T₀,t₀)` eşlemesiyle ilişkilendirilsin; geciken gönderim bütçeyi sessizce kaydırmasın. | önemli |
| 13 | §2.3; 149–156, §2.1; 54, §4; 377 | USB 2.0 için 53 MB/s iki bağımsız yön kapasitesi gibi sunuluyor; iki yön ortak veri yolunu paylaşır. Ayrıca tek akışta GCS/HIL mantıksal ayrımı fiziksel tıkanmayı kaldırmaz. Gönderilmiş bir düşük öncelikli çerçeve aktüatör/IMU önceliğiyle kesilemez. | USB’de ortak veri yolu yükü de hesaplanmalı. Her yönde önceden kuyruğa verilebilecek bayt miktarı sınırlandırılmalı; kısmi yazma ve çerçeve bütünlüğü korunmalı. “HIL etkilenmez” ifadesi ölçülmüş ve sınırlandırılmış gecikme koşuluna bağlanmalı. [USB-IF mimari açıklaması](https://www.usb.org/sites/default/files/Embedded%20USB2%20Version%202.0%20Supplement%20to%20the%20USB%202.0%20Specification_0.pdf). | önemli |
| 14 | §2.7; 242–256, §2.2 `HELLO/ACK` | Durum adları var; başarılı ACK’nin hangi tarafı ne zaman HAZIR’a geçirdiği, başlangıç/reset başarısızlığı, el sıkışma son tarihi ve SIL reset karşılığı yok. `run_id/session_nonce` veri mesajlarına bağlanmıyor. Eski oturumdan kalan çıktı yeni oturumda kabul edilebilir. `SEQ_RESET` ortak bayrak tablosunda da tanımlı değil. | Tam geçiş tablosu ve oturum kimliği doğrulaması yazılsın. Yeniden bağlanmada ayrıştırıcı, hazırlık, TX, bariyer ve sonar tamponları temizlensin. Sıra sıfırlaması yalnız doğrulanmış başlangıç geçişinde kabul edilsin; olay tekilleştirme durumu bu kuralla uyumlu olsun. | kritik |
| 15 | §2.6; 232, §2.7; 252–253 | Kopma “500 ms hiç bayt yok” ile ölçülüyor. Çöp bayt veya sağlıklı kalan `TIMESYNC`, bozulan veri akışını canlı gösterebilir. Buna karşılık sessiz HAZIR durumunda yalnız 1 Hz heartbeat kullanılırsa 500 ms eşiği normal aralıkta aşılır. | Taşıma etkinliği, doğrulanmış eş canlılığı ve sensör tazeliği ayrı sayaçlarla izlensin. Eşikler her durumun garanti edilen mesaj periyoduna bağlansın. Başka mesajların gelişi arızalı kontrol akışının canlılığını yenilemesin. | önemli |
| 16 | §2.2; 101, §2.7; 248, §7; 426 | `dialect_hash`, `(msgid, CRC_EXTRA, ad)` listesinden üretiliyor. Sekiz bit `CRC_EXTRA` tam şema özeti değildir; MAVLink uzantı alanları ona dahil değildir. Dolayısıyla uzantı düzeni farklı iki şema aynı hash’i üretebilir. Tam hash eşitliğiyle minor uyumluluğun ilişkisi de belirsiz. | Kanonik tam alan/semantik sözleşmesinden özet üretilecek şekilde düzeltin. Minor uyumluluk ve yetenek müzakeresi ayrıca tanımlansın. Her düzen değişikliğinin mutlaka CRC hatası oluşturacağı kabul testi kaldırılmalı. [MAVLink serileştirme kuralları](https://mavlink.io/en/guide/serialization.html). | önemli |
| 17 | §2.9; 277 | Sabit 10 ms ızgarada `r=1−exp(−λ_BGΔ)` kullanılırken ortalama kötü durum süresinin tam `1/λ_BG` olduğu söyleniyor. Bu ayrık zincirde ortalama süre `Δ/r`’dir. İki yönlü sürekli zaman zincirinin tam geçiş matrisi de verilen iki bağımsız formül değildir. | Ya üstel kalış süreleriyle olay zamanlı model kullanılsın ya da ayrık modelin gerçek süreleri ve yaklaşım sınırı yazılsın. Sürekli zaman zinciri örnekleniyorsa `exp(QΔ)` kullanılsın. Telemetri hızından bağımsızlık testi korunsun. | önemli |
| 18 | §2.9; 295–304 | Uygulama mesaj kaybı ile TCP kaybı ayrımı doğru başlatılmış; ancak her kaybın yalnız bir `RTO` beklemesine çevrilmesi tam TCP modeli değildir. Tekrarlı yeniden iletim, geri çekilme, kesinti boyunca kuyruk ve yeniden bağlantıda bekleyen verinin akıbeti yok. Telsiz çerçeve kaybı da fiziksel paketleme modeli olarak sunuluyor. | Profillerin hangi doğruluk düzeyini temsil ettiği açıklansın. TCP için sıralı teslim, kalıcı kesinti, yeniden deneme ve yeniden bağlanma sözleşmesi tamamlansın; sade durma modeli yaklaşık model diye adlandırılsın. Bit bozma, TCP uygulama çıktısına fiziksel hataymış gibi uygulanmamalı. [RFC 9293](https://www.rfc-editor.org/rfc/rfc9293.html), [RFC 6298](https://www.rfc-editor.org/rfc/rfc6298.html). | önemli |
| 19 | §2.9; 269 | “Emülatör USB’nin önünde olduğu için taban gecikmeyi çıkarmaya gerek yok” gerekçesi yeterli değil. HIL’de GCS komutunun gerçek alım gecikmesine USB ve kart servis gecikmesi yine eklenir; kartın failsafe süreleri duvar saatiyle çalışır. | Modellenen bağlantı gecikmesi ile HIL köprüsünün ek gecikmesi ayrı raporlansın. Uçtan uca hedefin nasıl tutulduğu ve tutulamayan kısa gecikmelerin nasıl işaretlendiği tanımlansın. USB gecikmesi yalnız aktüatör bariyerinin konusu değildir. | önemli |
| 20 | §5; 389, §3; 359, §7; 437–438 | İki yönlü A1 kaydını yeniden oynatma için yeterli sayıyor. İskelet A6 ayrıca uygulanan fizik adımını, zaman aşımı kararlarını ve düşürülen veriyi zorunlu tutuyor. HIL `IDA_ACTUATOR` dizisinin tamamında zaman ölçümleri de bulunduğundan ham dizilerin eşitliği beklenemez. | İşlevsel çıktı ile süre/tanılama alanları ayrıştırılsın. Uygulama adımları, bariyer kararları, güvenli çıktılar, düşürmeler, oturumlar ve gerekli başlangıç durumu kaydedilsin. Bit eşitliği testi yalnız sabitlenen platform ve kanonik işlevsel iz üzerinde yapılsın. | önemli |
| 21 | §2.2 ve §3 A1↔A4 | Birimler ve çerçeveler tam eşlenmemiş: özel A1’de gradian, µs, mm ve derece ölçekli alanlar var; iskelet ortak kabulleri radyan/ns/SI istiyor. `q_nb` için Hamilton ve `(w,x,y,z)` açık değil. Algılama `values[6]` taşıyor fakat yalnız ilk üç bileşenin kovaryansı tanımlı. | Tel kodlaması istisnaları açıkça belirtilip A4 dönüşümleri yazılsın veya özel mesajlar ortak birimlere geçirilsin. Sensör montaj dönüşümünün sahibi, kovaryans sırası/birimleri ve algılama türü başına geçerli alanlar tanımlansın. Altı boyutlu poz destekleniyorsa belirsizlik sözleşmesi tamamlanmalı. | önemli |
| 22 | §2.5; 215–219 | 1.324/692 B hesabı, kullanılmayan DATA kuyruğunun sıfırlanmasına ve dizinin XML’de diğer u8 alanlardan sonra bulunmasına bağlı. Sade kademede `sample_period` ölçeklemesi ise tamsayı alan nedeniyle her zaman menzili tam koruyamaz. | Kodlama öncesi kullanılmayan kuyruk sıfırlansın; tel sırası üretilmiş başlıkla sınansın. Etkin örnek sayısı/periyot için nicemleme, geçerli aralık, gerçekleşen menzil ve hata toleransı tanımlansın. [MAVLink alan sırası](https://mavlink.io/en/guide/serialization.html), [Ping360 alanları](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/). | önemli |
| 23 | §3; 336; komşu 02 §2.8 | Tek küresel MAVLink kanal durumu ve `MAVLINK_COMM_NUM_BUFFERS=1`, aynı süreçte birden çok SIL çekirdeği vaadiyle uyuşmuyor. `ida_link_t*` almak tek başına kütüphanenin statik durumunu örneğe özel yapmaz. | Ayrıştırıcı, TX sıra sayacı ve oturum durumu her `ida_link_t` örneğine ait olsun veya her koşucunun ayrı süreç olacağı açıkça sınırlandırılsın. C kütüphanesinin çağıranın verdiği durum/tampon API’si kullanılabilir. [MAVLink C uygulaması](https://raw.githubusercontent.com/mavlink/c_library_v2/master/mavlink_helpers.h). | önemli |
| 24 | §2.2 `IDA_BOARD_STATUS/HELLO`, §3; 353 | Komşu 01 §3.4’ün istediği açılış ve hata kayıtlarının önemli kısmına mesaj karşılığı yok: silikon revizyonu, saat/PHY yapılandırması, FPSCR, fault register’ları vb. Reset nedeni de ST’nin birlikte bulunabilen bayrakları yerine tek enum gibi verilmiş. | 01’in ürettiği kayıtların tamamı A1 mesajlarına eşlensin; ham reset bayrakları ile yorumlanan neden ayrı taşınsın. Sıra taşması ve u16 süre alanlarının doygunluk davranışı tanımlansın. | önemli |
| 25 | §4; 367, §7; 440 | “Sonraki `0xFD`’de” ve “bir sonraki geçerli çerçevede” senkron garantisi fazla güçlü. Bozulmuş uzunluk alanı sonraki geçerli çerçeveyi eski yükün parçası olarak tükettirebilir. Enjekte edilen tek olay da birden fazla belirti/sayaç üretebilir. | Sabitlenmiş ayrıştırıcıya uygun, bayt/çerçeve cinsinden sınırlı toparlanma ölçütü tanımlansın. Enjeksiyon olayı ile CRC, sıra boşluğu, parti eksikliği ve profil kaybı sayaçları ayrı değerlendirilsin. [Ayrıştırıcı durum makinesi](https://raw.githubusercontent.com/mavlink/c_library_v2/master/mavlink_helpers.h). | önemli |
| 26 | §2.2; 99, §7; 431 | `IDA_LOOPBACK` en çok 255 B yük taşıyabilir; 256 B **yük** testi bu mesajla yapılamaz. Ayrıca yankı testi aynı büyüklüğü geri gönderir; 64 B istek/48 B aktüatör yanıtı ölçümüyle aynı deney değildir. | 256 B’nin MAVLink çerçevesi mi uygulama yükü mü olduğu açıklansın. 256 B çerçeve için 244 B yük kullanılabilir. Simetrik geri döngü ve gerçek kontrol alışverişi ayrı test edilsin. | küçük |

## Yeniden hesaplananlar

Aşağıdakiler alan listelerinden ve belirtilen hızlardan yaptığım hesaplardır; donanım ölçümü değildir. Yeni zaman/olay/oturum alanları eklendiğinde bütçeler yeniden değişecektir.

**Mesaj boyutları**

Klasik imzasız MAVLink 2 çerçevesi için `çerçeve = yük + 12 B`, azami yük `255 B`, azami çerçeve `267 B` alınmıştır. İmzalama ayrıca 13 B ekler. [Resmî tel biçimi](https://mavlink.io/en/guide/serialization.html).

| Mesaj | Yeniden hesaplanan yük / çerçeve, B | Bölümle karşılaştırma |
|---|---:|---|
| Ortak `H` | 15 | Doğru: `8+4+2+1` |
| IMU | 49 / 61 | Doğru |
| MAG | 27 / 39 | Doğru |
| BARO | 23 / 35 | Doğru |
| GNSS | 49 / 61 | Doğru |
| DVL | 85 / 97 | Doğru |
| Ping1D | 32 / 44 | Doğru |
| Ping360 HEAD | 32 / 44 | Doğru |
| Ping360 DATA, tam parça | 255 / 267 | Doğru |
| BATT | 27 / 39 | Doğru |
| LEAK | 16 / 28 | Doğru |
| DETECTION | 78 / 90 | Doğru |
| ACTUATOR | 36 / 48 | Doğru |
| NAV_STATE | 111 / 123 | Doğru |
| BOARD_STATUS | 44 / 56 | Doğru |
| Ping360 CONFIG | 17 / 29 | Doğru |
| HELLO | 38 / 50 | Bölümde boyut verilmemiş |
| HELLO_ACK | 9 / 21 | Bölümde boyut verilmemiş |
| LOOPBACK, azami | 255 / 267 | 256 B yük taşıyamaz |
| FC_STATUS | Belirlenemiyor | Alanları yok; yaklaşık 40/52 B doğrulanabilir boyut hesabı değildir |

`TIMESYNC` için güncel tanımın azami yükü 18 B, çerçevesi 30 B’dir; 16 B taban yüke iki hedef uzantısı eklenmiştir. Bölümün 30 B üst sınırı uygundur. Ancak yön başına 321 B/s hesabı, toplam **10 istek-cevap alışverişi/s** varsayar; iki tarafın bağımsız 10 Hz istek başlatması halinde trafik artar. [Üretilmiş TIMESYNC tanımı](https://raw.githubusercontent.com/mavlink/c_library_v2/master/common/mavlink_msg_timesync.h).

**Sonar parçalama**

Sıfırla doldurulmuş kullanılmayan kuyruk ve en kötü geçerli örnek içeriği için:

\[
F(N)=\lceil N/251\rceil,\qquad
P(N)=44+N+16F(N).
\]

Buradaki 16 B, her DATA parçasının 4 B metadata ve 12 B çerçeve ek yüküdür.

| Örnek sayısı | DATA sayısı | Son parçadaki örnek | Toplam B |
|---:|---:|---:|---:|
| 200 | 1 | 200 | 260 |
| 250 | 1 | 250 | 310 |
| 251 | 1 | 251 | 311 |
| 252 | 2 | 1 | 328 |
| 502 | 2 | 251 | 578 |
| 600 | 3 | 98 | 692 |
| 1199 | 5 | 195 | 1323 |
| 1200 | 5 | 196 | 1324 |

Dolayısıyla bölümün **1.324 B**, **692 B** ve 1200 örnekte **%10,3 ek yük** değerleri doğru. 600 örnekte ek yük `%15,33` olur; bant genişliği örnek sayısıyla tam orantılı değildir.

1200 örnek için son DATA çerçevesi `196+4+12=212 B`; 600 örnek için `98+4+12=114 B` çıkar. Sondaki sıfırların kırpılması geçerli sıfır örnekleri kaybettirmez; alıcı sıfırla tamamlar. Bunun için gönderenin kullanılmayan kuyruğu da gerçekten sıfırlaması gerekir. [MAVLink kırpma ve sıfırlama uygulaması](https://raw.githubusercontent.com/mavlink/c_library_v2/master/mavlink_helpers.h).

**400 Hz bant genişliği**

Yavaş sensörler:

\[
39(100)+35(25)+61(5)+97(15)+44(20)+39(10)+28(10)
=8085\ {\rm B/s}.
\]

Sonarsız ve GCS’siz aşağı yön:

\[
24400+8085+10800+321=43606\ {\rm B/s}.
\]

Üreticinin 33 s ve 3,42 s tarama sürelerini 400 açısal konumla kullanınca:

| Büyüklük | Yeniden hesap | Bölüm |
|---|---:|---:|
| 50 m profil hızı | 12,1212/s | ≈12,1/s |
| 50 m sonar trafiği | 16.048,48 B/s | 16.050 |
| 1 m Ethernet referansı profil hızı | 116,9591/s | ≈117/s |
| Hızlı sonar trafiği | 154.853,80 B/s | 154.900 |
| Sim → kart, 50 m | **75.654,48 B/s** | 75.656 |
| Sim → kart, hızlı sonar | **214.459,80 B/s** | 214.506 |
| Kart → sim | **52.501 B/s** | 52.501 |
| İki yön ham kayıt, 50 m | **128.155,48 B/s** | ≈130 kB/s |
| İki yön ham kayıt, hızlı sonar | **266.960,80 B/s** | ≈270 kB/s |

Farklar yuvarlamadan kaynaklanıyor. Üretici süreleri doğru aktarılmış; 3,42 s **1 m Ethernet** referansıdır. Bunun 1200 örneklik yapılandırmanın tümü için fiziksel üst hız olduğu ayrıca kanıtlanmış değildir; bölümün tahmin olarak kullanması korunmalı. [Ping360 üretici tablosu](https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/).

8N1 kapasitesi `baud/10` alınarak:

| Taşıma | Kapasite, B/s/yön | Aşağı, 50 m | Aşağı, hızlı sonar | Yukarı |
|---|---:|---:|---:|---:|
| 921.600 baud | 92.160 | %82,09 | **%232,70** | %56,97 |
| 2 Mbaud | 200.000 | %37,83 | **%107,23** | %26,25 |
| 3 Mbaud | 300.000 | %25,22 | %71,49 | %17,50 |

Seri kapasite sonuçları esas olarak doğru. Bununla birlikte bölümün `%50 sürdürülebilir kapasite` kabulü, hızlı aşağı yön için en az **428.919,60 B/s** ölçülmüş kapasite gerektirir. Tam yük 3 Mbaud’da bu kabule sığmaz.

USB hesabının aritmetiği:

\[
13\cdot512/(125\ \mu s)=53.248.000\ {\rm B/s}.
\]

Bu teorik referansla yönlerin oranları `%0,142 / %0,403 / %0,099`; iki yön toplamı 50 m’de `%0,241`, hızlı sonarda `%0,501` olur. Referans, iki bağımsız yön garantisi veya ölçülmüş CDC kapasitesi olarak kullanılamaz.

200 Hz’e geçildiğinde yalnız IMU ve ACTUATOR hızlarını yarıya indirirsek:

- Aşağı yön: 50 m’de **63.454,48 B/s**, hızlı sonarda **202.259,80 B/s**.
- Yukarı yön: **42.901 B/s**.
- Hızlı aşağı yön 2 Mbaud’a yine sığmaz.
- 200 Hz’te adım başına 267 B toplu bütçenin sürekli kapasitesi `53.400 B/s`; yalnız sonar için bile en fazla yaklaşık **40,33 profil/s** eder. HEAD/parça yerleştirmesi ve algılamalar bu sınırı daha da daraltabilir.

**Bariyer ve seri gecikme**

Fizik `1.250.000 ns = 1,25 ms = 800 Hz`; kontrol 400 Hz’te iki, 200 Hz’te dört fizik adımıdır.

Bölümün formülüne göre:

\[
B_{\rm taşıma}=L_{\rm act}+1,25-C_{\rm sim}-C_{\rm kart}.
\]

`C_sim=0,40 ms`, `C_kart=1,00 ms` varsayımıyla:

| Durum | Taşıma/PC payı |
|---|---:|
| `L_act=1,25 ms` | **1,10 ms** |
| `L_act=2,50 ms` | **2,35 ms** |

İkinci sonuç kontrolün 200 Hz olmasından bağımsızdır; formülde kontrol periyodu yoktur. Satır 196’daki **3,45−1=2,45 ms** yerine **3,35−1=2,35 ms** çıkar.

Yalnız IMU ve yanıt:

\[
61+48=109\ {\rm B}.
\]

921.600 baud’da **1,1827 ms**, 2 Mbaud’da **0,545 ms**, 3 Mbaud’da **0,3633 ms** sürer. Bölümün bu hesabı doğru.

Bütün yavaş sensörler aynı adıma geldiğinde kritik sensör toplamı:

\[
61+39+35+61+97+44+39+28=404\ {\rm B}.
\]

GCS’nin 16.000 B/s payını düzgün dağıtılmış kabul edersek:

| Kontrol | GCS/adım | Kritik istek | Yanıt dahil | 2 Mbaud seri süre | 3 Mbaud seri süre |
|---|---:|---:|---:|---:|---:|
| 400 Hz | 40 B | 444 B | **492 B** | **2,460 ms** | **1,640 ms** |
| 200 Hz | 80 B | 484 B | **532 B** | **2,660 ms** | **1,773 ms** |

Bunlara kart hesabı ve işletim sistemi gecikmeleri eklenir. Dolayısıyla **200 Hz, 2 Mbaud, `L_act=2,5 ms`** birleşimi, bu varsayımlarla bile 2,35 ms taşıma payını aşar. Oturum mesajları ve gerçek GCS patlamaları henüz dahil değildir.

Ayrıca 267 B’lik başlamış bir çerçevenin seri hattı boşaltması 921.600 baud’da **2,897 ms**, 2 Mbaud’da **1,335 ms** sürer. Kuyruğun başına öncelikli mesaj koymak, bu aktarımı geri alamaz.

01’in **1,57 ms** kart servis tahmini kullanılırsa birincil taşıma/PC payı:

\[
2,50-0,40-1,57=\mathbf{0,53\ ms}.
\]

**Kayıp modeli**

Bölümdeki ayrık zincir için kötü durumdan çıkma olasılığı:

\[
r=1-e^{-\lambda_{BG}\Delta_e}.
\]

Izgara üzerinde kötü durumda kalınan ortalama süre:

\[
E[T_B]=\frac{\Delta_e}{r},
\]

tam olarak `1/λ_BG` değildir. Örneğin `λ_BG=100 s⁻¹`, `Δ_e=0,01 s` için:

\[
r=0,63212056,\qquad E[T_B]=15,8198\ {\rm ms}.
\]

Bölümün iddiası **10 ms** verir; fark yaklaşık **%58,2**.

Sürekli zamanlı iki durumlu zincirin tam uç-nokta geçişleri, `a=λ_GB`, `b=λ_BG` için:

\[
P_{GB}=\frac{a}{a+b}(1-e^{-(a+b)\Delta_e}),\qquad
P_{BG}=\frac{b}{a+b}(1-e^{-(a+b)\Delta_e}).
\]

Tam `1/λ` kalış süreleri isteniyorsa olay zamanlı üstel beklemeler daha doğrudan tanımdır. Sabit zaman tabanı seçimi telemetri hızına bağımlılık sorununu çözer; mevcut süre eşitliğini doğrulamaz.

**Radyo denklemleri**

Serbest uzay sabiti, `d` metre ve `f` Hz için:

\[
20\log_{10}(4\pi/c)=-147,5522.
\]

Bölümdeki **−147,55 doğru**. Birimler, `d=0` koruması ve modelin yakın alan dışında kullanımı açıklaştırılmalı.

ITU-R P.527-6’nın su denklemlerini yeniden uyguladım. `20 °C, 35 ppt` için DC iletkenlik **4,79127 S/m** çıktı. ITU’nun iletkenlik terimindeki yuvarlatılmış `18σ/f_GHz` kullanıldığında:

| Ortam | 433 MHz | 915 MHz | 2400 MHz |
|---|---:|---:|---:|
| Deniz suyu, dB/m | 662,625 | 825,616 | 1084,143 |
| Saf su modeline 0,01 S/m eklenmiş örnek, dB/m | 10,237 | 39,315 | 256,927 |

Bölümün **663/826/1084** ve **10,2/39,3/256,9** referansları yeniden üretildi. 433 MHz’te 10 cm için yaklaşık **66,26 dB** ve **1,024 dB** sonuçları da doğru. [ITU-R P.527-6, §5.1.1–5.1.2](https://www.itu.int/dms_pubrec/itu-r/rec/p/R-REC-P.527-6-202109-I%21%21PDF-E.pdf).

Uygulamada `ε′` mutlak permitivite olmalı veya `ε₀` açıkça çarpılmalı. ITU’nun toplam kayıp bileşeni kullanılıyorsa DC iletkenlik ikinci kez eklenmemeli. Lojistik başarı fonksiyonu boyutsal olarak uygun; `w>0`, `M₅₀` ve kötü durum başarısı `h` tanımlanmalı. Bunlar ölçülmüş parametreler değildir.

**Diğer sayısal sınırlar**

- 400 açısal konum için bir gradian `π/200 rad = 0,9°`; doğru.
- Sonar menzil bağıntısı `R=c·N·sample_period·25 ns/2`; örnek sayısını azaltıp periyodu oranlamak ideal aritmetikte menzili korur, tamsayı nicemlemesinde tolerans gerekir.
- İki 1200 B sonar tamponu örnek başına **2400 B** veri alanıdır; metadata/bitmap dahil değildir.
- `uint16 profile_seq`, 116,959 profil/s’de **560,33 s ≈9,34 dakika** sonra sarar. Oturum ve eski parça reddi önemlidir.
- `uint32 step_seq`, 400 Hz’te **124,28 gün** sonra sarar; düz `<`/`>` karşılaştırmaları için taşma sözleşmesi gerekir.
- `uint16 step_us/rx_to_tx_us` en çok **65,535 ms** temsil eder; daha uzun duraklamalarda doygunluk/taşma davranışı tanımsız.
- `256 B/adım` telemetri bütçesi 400 Hz’te **102.400 B/s**, 200 Hz’te **51.200 B/s** eder. Ortalama kapasite yeterli görünse de aynı adımdaki çıktı patlamalarını garanti etmez.
- 30 dakikalık 400 Hz koşu **720.000 kontrol adımıdır**. `p99,9≤bütçe`, kaçırma sıfır demek değildir; bölümün ayrı kaçırmasız koşu kabulü korunmalıdır.
- HEARTBEAT `21×8=168 bit/s`; M16’nın 10 bit/s hızında tek çerçevenin salt veri süresi **16,8 s** olur. Olağan 1 Hz akışın sığmadığı sonucu doğru.
- 10 ms, 400 Hz’te dört; 200 Hz’te iki kontrol periyodudur. El sıkışma 200 ms, kopma 500 ms, yeniden açma 1 s, toparlanma 3 s ve SIL gözetimi 5 s değerleri ölçüm sonucu değil tasarım hedefi/eşiğidir.

## İskelete ya da diğer bölümlere notlar

**İskelet uyumu**

Sekiz ana başlık mevcut. A1, A3, A4, A5 ve A6 genel olarak doğru amaçlarla kullanılmış; sensör/fizik üretimi ve kart sürücüsü kapsamları komşu bölümlere bırakılmış. Yeni dış yer istasyonu arayüzünün §8’de öneri olarak sunulması uygun.

Bununla birlikte satır 16’daki **A8**, araştırma belgesindeki açık soruya atıftır; §8’de önerilen **A8 arayüzüyle** karışıyor. `00/Açık soru A8` şeklinde nitelendirilmeli. Yeni arayüz, iskelet değişmeden mevcut sözleşme gibi kullanılamaz.

İskelet 0.3’ün şu hükümleri bölümde uygulanmamış veya eksik uygulanmış:

- §3.1: alım zamanı ve ortak birim/çerçeve eşlemeleri.
- §3.2: sürekli komutun kullanım süresi, olay kimliğiyle tekilleştirme ve aynı adımda izin daraltma önceliği.
- §4: IMU’dan bağımsız zaman çerçevesi ve iki ölçütlü tazelik.
- A6: uygulama adımları, bariyer kararları ve düşürülen verilerin kaydı.

Bunlar yeni iskelet önerisi değildir; mevcut bağlayıcı hükümlerdir.

**Komşu bölümler ve önceki incelemeler**

| Belge | Uyumsuzluk / yapılması gereken eşleme |
|---|---|
| `docs/mimari/01-kart-yazilimi.md` §2.4 | Kart servis bütçesi 06’dan daha geniştir. İki bölümün gecikme ölçüm noktaları birleştirilmeli. |
| Aynı belge §2.4, §8.2 | DUP çıktısında tetik sıfırlanıyor; 06 önceki aktüatör çerçevesini yeniden gönderiyor. İkisi de iskeletin olay kimliği sözleşmesine geçirilmelidir. |
| Aynı belge §3.3 | `ida_conv_*`/biriktirici API’si ile 06’nın `ida_link_*` API’sinin ilişkisi açık değil. İki bağımsız dönüştürücü tasarlanmış gibi bırakılmamalı. |
| Aynı belge §3.4 | Açılış, hata ve tanılama kayıtlarının A1 karşılıkları eksik. |
| `docs/mimari/02-ortak-cekirdek.md` §2.2, §3.1 | IMU tetikleme, eski tetik gösterimi ve komut işleme sırası iskelet 0.3’e göre güncellenmemiş. 06, eski komşu sözleşmesini bağlayıcı kabul edemez. |
| Aynı belge §2.8 | Tek süreçte çoklu çekirdek ile küresel tek MAVLink kanalı uyuşmuyor. IMU’suz sanal zaman ilerlemesi de birlikte düzeltilmeli. |
| `docs/mimari/04-simulator-cekirdegi.md` §2.11 | Hâlâ eski fizik/zamanlama ifadeleri var. 06’nın 1,25 ms kullanımı doğrudur; 04 buna uymalı. 30 Hz kamera 800 Hz tabanda 26/27 adımla zamanlanır. |
| Aynı belge §3 `ActuatorFrame` | Seviye biçimindeki fırlatıcı alanları olay kimliğine geçirilmeli; sürekli komut geçerliliği ve asenkron güvenli çıktı eklenmeli. |
| `docs/mimari/inceleme/02-sol.md` | IMU’suz SIL, alım zamanı, disarm/atış sırası, önbellekte tetik ve eksiksiz kayıt bulguları 06’da kapanmamış. |
| `docs/mimari/inceleme/04-sol.md` | 1,25 ms geçişi ve komut uygulama zamanının açıklaştırılması talepleriyle uyumlu bir ortak zaman çizelgesi gerekli. |

`03`, `05`, `07`, `08` ve `09` mimari dosyaları mevcut değil. Bunlara verilen beklentiler incelenebilir sözleşme yerine geçmiyor. Özellikle FC mesaj alanları, algılama belirsizliği ve sonar üretim/teslim zamanlaması henüz tamamlanmış sayılmaz.

**00 düzeltmelerinin madde bazında durumu**

| Düzeltme | Durum |
|---|---|
| 06.1 | Özel dialect gerekçesi doğru; standart mesajların varlığı inkâr edilmemiş. |
| 06.2 | `ENCAPSULATED_DATA` biliniyor; hazır sonar standardı olmadığı doğru ayrılmış. |
| 06.3 | Eski “ayrı sensör hızlarıyla uyuşmaz” iddiası tekrarlanmamış. `fields_updated` olanağı gerekçede açıkça anılabilir. |
| 06.4 | Yön başına kapasite ayrımı doğru; yeni kritik parti/yanıt gecikme hesabı eksik. |
| 06.5 | Filtre yükü hesabı kapsam dışında. Kullanılan 1 ms kart bütçesi tam platform süresi değildir. |
| 06.6 | “f32 zorunlu” iddiası yok. |
| 06.7 | Hatalı 10 km ULP değeri tekrarlanmamış. |
| 06.8 | Bariyerin bütün firmware saatini sanallaştırmadığı doğru; iskeletin ek zaman çerçevesi uygulanmamış. |
| 06.9 | DVL eklentisi hakkındaki eski yanlış tekrarlanmamış. |
| 06.10 | Koşulsuz ArduSub SURFACE iddiası yok; eylem 02’ye bırakılmış. |
| 06.11 | 400 Hz iskelet hedefi olarak kullanılmış; ham IMU hızıyla karıştırılmamış. |
| 06.12 | QGC telemetri, görev ve kip desteği doğru ayrılmış; uyum hâlâ prototip kabulüne bağlı. |
| 06.13 | USB yüzünden klasik HIL kaldırıldığına dair doğrulanmamış gerekçe kullanılmamış. |
| 06.14 | COBS/CRC ve açık serileştirme ayrımı doğru. Şema özeti ve hata davranışı tamamlanmalı. |
| 08.1–08.9 | Eski kart/araç zinciri yanlışları bu bölümde tekrarlanmıyor; esas olarak 01/09 kapsamındalar. |
| 08.10 | 12 B ek yük, 255 B yük sınırı, özel dialect ve statik tampon olanağı doğru aktarılmış. |
| 08.11 | Yedek biçimde padding/endian/bool/IEEE-754 açık serileştirmesi doğru tarif edilmiş. |
| 08.12 | Taşma ve eksik girdi anılmış; adım bütünlüğü ve tam servis bütçesi eksik. |
| 08.13 | Hatalı “boş malloc bağlantı hatası üretir” iddiası yok. |
| 08.14 | Platformlar arası bit eşitliği vaat edilmiyor; ham kayıt eşitliği testi yine daraltılmalı. |
| 08.15 | USB ve işlemci başarımı ölçülmüş gibi sunulmuyor. |
| 09.1–09.3 | Eski C++/chrono yasakları tekrarlanmamış. |
| 09.4 | Philox seçimi uygun; GE ayrıklaştırma hatası bundan bağımsız. |
| 09.5–09.7 | Üretim FP politikasına aykırı yeni iddia yok; platform içi tekrarın kapsamı netleşmeli. |
| 09.8 | `usbser.sys` veya macOS için doğrulanmamış genel kusur iddiası yok. |
| 09.9–09.11 | Foxglove/Qt/ikinci tam sahne hakkındaki yanlış gerekçeler kullanılmıyor. |
| 09.12 | USB HS taşıması ölçüme bağlı bırakılmış; kartta doğrulanmış çalışma iddia edilmiyor. |
| 09.13–09.14 | CI ve QGC derleme sürümü konuları bu bölümde kullanılmıyor. |
| 09.15 | Genel QGC eklentisinin sınırı doğru aktarılmış. |
| 09.16 | C17/C++20 ayrımı korunmuş. |
| 11.1 | Tam sönüm formülü ve referans sayılar doğru; mutlak/bağıl permitivite ve DC çift sayımı uygulamada açıklanmalı. |
| 11.2 | İkili kopma evrensel kural yapılmamış; kaba profil olarak ayrılmış. |
| 11.3 | Doğrulanmamış 25 cm menzil iddiası taşınmamış. |
| 11.4 | Hatalı batarya varsayılanları/eylem numaraları tekrarlanmamış. |
| 11.5 | 80 Mbit/s pratik ve 300 m test edilmiş ayrımı doğru. |
| 11.6–11.8 | Hatalı hassas T200 verileri, 30 A koruma eşiği ve motor içi su arızası kullanılmıyor. |
| 11.9 | HEARTBEAT kapasite hesabı doğru; koşulsuz yayılma gecikmesi eklenmiyor. |
| 11.10–11.12 | Güç modeli kapsamı dışında; eski doğruluk/enerji iddiaları tekrarlanmamış. |
| 11.13 | TCP ile uygulama çerçeve kaybı ayrılmış; kuyruk/yeniden iletim/yeniden bağlanma modeli tamamlanmamış. |
| 11.14 | Radyo varyant/firmware ve engelli görüş hattı koşulu korunmuş. |
| 11.15 | Paket hızından bağımsız zaman tabanı doğru yönde; `1/λ` eşitliği verilen ayrık modelde yanlış. |
| 11.16 | M64 durumu ve M16 kapasitesi doğru; akustik profil açılırsa hareket sınırı ve paketleme ayrıca sözleşmeye girmeli. |
| 11.17–11.18 | ESC ve elektrik bağlaşımı 04’e bırakılmış; eski yanlış hükümler kullanılmıyor. |
| 11.19 | Ayrı reset nedenleri anılmış; yazılım resetinin gerçek BOR doğrulaması olduğu iddia edilmiyor. Bayrakların kayıpsız taşınması eksik. |

K1–K10 kapsamındaki araç, kart, dil, platform, web paneli ve araştırma amacı kararlarına aykırı öneri bulunmadı. Ö2/Ç3 ve M9 doğrultusunda MAVLink birincil, açık serileştirmeli biçim yedek tutulmuş. Ç1’in HIL saat sınırı ve Ö11’in kaçırmayı sayıp koşuyu geçersiz kılma ilkesi korunmuş. Ancak M1–M3, M5 ve M17 tamamlanmadan taşıma/gecikme yeterliliği; M25 tamamlanmadan radyo ve bağlantı parametreleri kesinleşmiş sayılamaz.