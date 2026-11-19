# sol ikinci incelemesi: A1-kart-baglantisi

## Hüküm

**Aday dondurulamaz.** İlk incelemenin 13 maddesinden 7’si giderilmiş, 6’sı kısmen giderilmiştir. Yeni sıralama ve sonar kuralları belirgin açıkları kapatıyor; ancak kapsam, doğrulama, kuyruk kanıtları, zaman bütçesi ve SIL eşitliği hâlâ sorunlu. Kanonik özet de mevcut XML’e ait değil.

Hedef baştan sona okundu. Hesaplar ve altın çerçeveler bağımsız olarak yeniden üretildi. Dosya değiştirilmedi, kurulum yapılmadı.

## İlk incelemedeki maddelerin durumu

| # | Durum | Not |
|---|---|---|
| 1 | kısmen | FC işlemleri kapalı ve argümansız; `fc_ref` ile `ext[]` kaldırılmış. Ancak `TARGET_ALIGN` ve anlamı açık bırakılmış görev parametreleri kalmış; yeni bulgu 1. |
| 2 | giderildi | K107 ve zaman diyagramı, `SESSION_END` çağrısının dönüşünü yeni el sıkışma yanıtından ve `SESSION_START`’tan önce zorunlu kılıyor. |
| 3 | giderildi | K78/K105 üretim sırasını, eski sürekli çıktıların iptalini ve aynı fizik sınırındaki son değeri belirliyor. |
| 4 | kısmen | Olağan kapanıp açılmalarda eski revizyon reddediliyor ve ret saklanıyor. Sayaç ömrü, genişliği ve A3 ortaklığı tutarsız; yeni bulgu 3. |
| 5 | kısmen | Geçersiz enum ve gönderilmemiş parti örnekleri kapsanmış. Geçerli aralıktaki fakat birbirleriyle çelişen alanlar hâlâ kabul edilebiliyor; yeni bulgu 4. |
| 6 | giderildi | Dizi 28 eleman; indisler, yük boyu ve CRC_EXTRA doğru. Yeni özet hatası ayrıca bulgu 2’de. |
| 7 | giderildi | K51 mantıksal, sıfırla tamamlanmış alanları denetliyor. Bir baytlık geçerli DATA örneği doğru. |
| 8 | giderildi | Ayrılmış yer ve kredi, tam çerçevelerle ilerlemeyi sağlıyor. 13 pencere üst sınırı, kuyrukta korunup teslim edilen satırlar için muhafazakâr bir sınır. |
| 9 | kısmen | Kritik yük artık gerçekten sınırlı. Sonuç kuyruğunun taşmazlık ve teslim süresi kanıtları geçerli değil; yeni bulgu 5. |
| 10 | kısmen | Zorunlu ve kaybedilebilir sınıflar ayrılmış; sürücüye verilen çerçeve sınırlandırılmış. E17 kanıtında geliş ve hizmet pencereleri uyuşmuyor; yeni bulgu 7. |
| 11 | giderildi | K108 planlanan duvar son tarihini yuva sonucundan ayırıyor; bariyerde kurtarılan geç yanıt da geçersizlik üretiyor. |
| 12 | giderildi | Alt sınır 16 ms olmuş; özgün `15 < 15` hatası giderilmiş. Diğer kanıtlardaki kalan sorunlar aşağıda ayrı gösterildi. |
| 13 | kısmen | Tohum ve kalıcı başlangıç durumu tanımlanmış. Bunlar K102’nin bütün çerçeve dizisi eşitliğini sağlamıyor; yeni bulgu 9. |

## Yeni bulgular

Aşağıdaki bölüm ve kural atıfları, aksi belirtilmedikçe `docs/sozlesmeler/A1-kart-baglantisi.md` içindir.

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem | Kapatılacağı yer |
|---|---|---|---|---|---|
| 1 | §3.6; `protocol/ida.xml:144`, `protocol/ida.xml:556` | **Kapsam dışı hedefleme yolu tamamen kapanmamış.** `IDA_MI_TYPE_TARGET_ALIGN = 8` taşınıyor; `CMD_MISSION_ITEM.p[4]` anlamı çekirdek uygulamasına bırakılıyor. Dolayısıyla yalnız FC tablosunu kapatmak, hedefleme işlemi ve tanımsız parametre kanalı kalmadığı iddiasını sağlamıyor. | `TARGET_ALIGN`’ı bu arayüzden çıkar veya açıkça reddet. Taşınabilen görev türlerinin parametre anlamlarını kapalı biçimde sınırla; kapsam dışı anlamları sonraki uygulamaya bırakma. | kritik | A1/XML; A4 görev öğesi tablosuyla birlikte |
| 2 | §3.2 `dialect_hash`; §7.3; `protocol/ida.xml:53` | **Kanonik özet yanlış.** Tanımlanan algoritma mevcut XML için 534 satır ve `0x4B9E97C1439CF7FE` üretiyor. Belgedeki 533 satır / `0x6481E5EBBF66F5F1`, yalnız `E IDA_CLOSE_REASON IDA_CLOSE_REASON_TX_OVERFLOW 6` satırı çıkarılınca elde ediliyor. | Mevcut XML’in özetini düzelt; bütün enum girdilerinin kanonik metne katılmasını doğrula. Sonraki XML değişikliklerinde yeniden üret. | kritik | A1 §3.2, doğrulama kaydı ve özet üretimi |
| 3 | K106; §8.2 A3 md. 0; XML `safety_rev`; A3-K117 | **“Aynı sayaç” iki farklı sözleşmeye sahip.** A1’de çalıştırma başında 1’den başlayan, daraltmalarda artan `u32`; A3’te süreç boyunca yaşayan, açılmalar ve başka yetki değişimleri dahil artan `u64`. Ayrıca A1, `0xFFFFFFFF` sonrasını tanımlamıyor. Sarma, doyma veya geniş sayacı daraltma, mevcut eşitlik ve tekdüzelik kurallarını farklı biçimlerde bozuyor. | Tek sayaç kullanılacaksa genişlik, başlangıç, artış olayları ve tükenme davranışını ortaklaştır. Oturum yenilenmesinde eski damgaların ayrılması ve platformun ilk örneğe kadar 0 kullanması birlikte sınansın. | kritik | A1/A3 ortak emniyet revizyonu sözleşmesi |
| 4 | Tablo 4.6 V4; K73, K83; A4-K28/K64 | **Alanlar arası doğrulama eksik.** Örneğin `SAFETY_OPEN` veya `FIRE` ile `call_kind = TICK_GRID` aralık denetiminden geçiyor; A4 bu olayları tick çıktısında yasaklıyor. K83 ise böyle bir olayı ilk fizik sınırında değerlendirmeye alıyor. Benzer biçimde `core_rc = BAD_INPUT` ve `THR_VALID = 1` birleşimi tabloda reddedilmiyor. | Olay türü–çağrı türü ve dönüş kodu–çıktı bayrakları birleşimlerini V4’te tanımla. Çelişkili çerçeve, K109’un tek ret ve etkisizlik yoluna girsin. | kritik | A1 Tablo 4.6 ve uygunluk testleri |
| 5 | E8, E16; K80/K81/K111 | **Sonuç kuyruğunun taşmazlık kanıtı yanlış.** Saklı sonuç isteyen gecikmiş bir yineleme, kartta artık açık olmayan bir olaya ait olabilir. Bu yüzden kuyruk girişleri `N_olay = 8` ile sınırlanamaz; dokuz farklı sonuçlanmış olayın gecikmiş yinelemeleri karşı örnektir. Yeni sonuç önceliği de yineleme kuyruğunun iki partide boşalacağını garanti etmez. Ayrıca ikinci sonuç partisinin ek gecikmesi E8’de yoktur: mevcut hesabın bir pencere uzaması `55 + 10 + 14,5 = 79,5 ms > 75 ms` verir. | Yeni sonuçlar ve saklı sonuç talepleri için geliş/hizmet sınırlarını ayrı kur. Kuyruk kapasitesini ve kartın sonuç bekleme süresini aynı kanıttan türet; taşmayı yalnız “varsayım bozuldu” diye açıklama. | kritik | A1 K111, E8/E16 ve sonuç kaybı sınamaları |
| 6 | §3.9 bütçe e; E18; A4-K18/K19/K23 | **9,733 ms bütün izin verilen çağrı sıralarını kapsamıyor.** Dağıtımın 0,5 ms olması, TF geldiğinde başka çekirdek çağrısı sürmemesine bağlı. TF’den hemen önce başlayan, izin verilen 0,5 ms’lik ızgara tick’i bile bütçeye ek bekleme getirir: aynı toplam **10,233 ms** olur. A4, başka bir step sürerken toplam başlangıç gecikmesine 4 ms’ye kadar izin veriyor. | Önceki kesilemeyen çağrıyı bütçeye kat veya bu çakışmayı engelleyen bağlayıcı zamanlama kuralı koy. `B_krit` ve E18’i bundan sonra yeniden türet. | kritik | A1/A4 çağrı zamanlaması ve uçtan uca bütçe |
| 7 | E17; K39/K40/K110 | **48 çerçevelik kuyruk için verilen kanıt yeterli değil.** E17 “partide en çok 8 komut”tan “bir periyotta en çok 8 sonuç” çıkarıyor. K39 aynı yarı açık 5 ms duvar penceresinde iki TF’ye izin veriyor; iki sekiz komutlu parti 16 sonuç üretebilir. A4’ün birleştirme ve mandal sonuçları da yalnız parti başına sayımla sınırlandırılamaz. İki periyotluk toplam bayt karşılaştırması, bütün ara anlardaki kuyruk doluluğunu kanıtlamıyor. | Geliş patlamasını ve hizmeti aynı saat alanında, bütün zaman aralıkları için sınırla; birleştirme, mandal sonuçları ve yinelemeleri dahil et. Bu kanıttan `Q_ii` türet. | kritik | A1 E17 ve kart gönderim kuyruğu sözleşmesi |
| 8 | §3.10, O13; iskelet §3; `docs/gorevler/W1-A1.md` | **Yukarı yönün %60 gereği karşılanmıyor.** Belge bunu açıkça söylüyor; fakat O13’e yazmak daha yüksek öncelikli gereği değiştirmiyor. Hesaplanan yaklaşık %84,27, iskeletin her yönde ortalama %60 sınırıyla uyumsuz. | Bağlayıcı trafik kurallarıyla 900 B/periyot sınırını sağla veya iskelette mimarın açık değişikliğini bekle. Ölçüm, bu sözleşme çelişkisini tek başına kapatmaz. | kritik | A1 bant kuralları veya mimarın iskelet değişikliği |
| 9 | K102/K114; K12/K13/K40 | **SIL bayt eşitliği hâlâ garanti değil.** Bir koşuda ikinci `SIM_HELLO` gelmeden el sıkışma tamamlanabilir; diğerinde koşucu 100 ms’den geç çalışıp iki HELLO’yu `HANDSHAKE` durumunda işleyerek iki `BOARD_HELLO` gönderir. Ek BOARD_HELLO, K102 istisnasında yoktur. Parti birleştirme ve arka plan çağrılarının taşıma/çalıştırma takvimine bağlılığı da başlangıç tohumuyla çözülmemiştir. | Eşitlik kapsamını ve karşılaştırma yöntemini tanımla. Tam eşitlik korunacaksa SIL çağrı/birleştirme/arka plan takvimini belirlenimci yap; el sıkışma tekrarlarını ve başlık değişiminden etkilenen CRC’leri de ele al. | kritik | A1 SIL sözleşmesi; A4/A8 ile birlikte |
| 10 | §3.7 `CORE_COUNTERS`, `STATUS.health`; XML 46067; güncel A4 adayı | **Güncel komşu adayla eşleme bozulmuş.** A4 düzeltme turu 2’de `ida_core_counters_t` 22 alan; A1 hâlâ 18 alanı yapı sırasıyla taşıdığını söylüyor. Yeni alanlar araya eklendiğinden ham sıra eşlemesi sayaçları yanlış adlarla yorumlatır. A4 sağlık yapısına eklenen iki bayrağın A1 karşılığı da yoktur. | A4’ün dondurulacak adayını sabitle; sayaç ve sağlık eşlemesini açık alan listesiyle güncelle. İlgili boyları, CRC’leri, özeti ve bant hesabını yeniden üret. | kritik | A1/XML ↔ A4 çapraz uyum incelemesi |

## Yeniden hesaplananlar

**XML ve altın çerçeveler.** Standart kitaplıkla bağımsız hesapta 37 mesajın boyları ve CRC_EXTRA değerleri, 38 çerçevenin CRC’si ve belirtilen alanlardan yeniden üretilen yükleri doğru çıktı. Alan sıralaması, kırpma ve CRC_EXTRA yöntemi canlı [MAVLink serileştirme belgesiyle](https://mavlink.io/en/guide/serialization.html) karşılaştırıldı.

| Mesaj | Yük | Azami çerçeve | CRC_EXTRA | Sonuç |
|---|---:|---:|---:|---|
| SAFETY | 22 | 34 | 93 | doğru |
| ACTUATOR | 38 | 50 | 145 | doğru |
| EVENT | 37 | 49 | 124 | doğru |
| FC_STATUS | 14 | 26 | 46 | doğru |
| CALL_TRACE | 137 | 149 | 182 | doğru |

Kanonik metin **37 mesaj + 313 alan + 184 enum girdisi = 534 satır**; özet **`0x4B9E97C1439CF7FE`**. Belgedeki özet yanlış; neden bulgu 2’de.

`"123456789"` CRC sonucu **`0x6F91`**. Bir baytlık sıfır DATA çerçevesi **13 B** ve CRC baytları **`dc a6`**; doğru.

**Uçtan uca bütçe.**

| Kalem | Yeniden hesaplanan en kötü ms | Niteliği |
|---|---:|---|
| a — sim yazım gecikmesi | 0,500000 | tahmin |
| b — 258 B engelleme | 0,860000 | anma hızından hesap; köprü tamponu sınırı doğrulanmadı |
| c — 520 B kritik yük | 1,733333 | hesap |
| d — aşağı yön köprü payı | 0,400000 | ölçümden türetilmiş tahmin |
| e — dağıtım | 0,500000 | tahmin; önceki çağrının beklemesi hariç |
| f — gerçek çekirdek bütçesi | 3,500000 | tahmin |
| g — kodlama | 0,100000 | tahmin |
| h — 156 B engelleme | 0,520000 | hesap |
| i — 4 olay + ACTUATOR, 246 B | 0,820000 | hesap |
| j — yukarı yön köprü payı | 0,400000 | ölçümden türetilmiş tahmin |
| k — sim hazırlığı | 0,400000 | tahmin |
| **Toplam** | **9,733333** | verilen kalemlerin doğru toplamı |
| **10 ms’ye pay** | **0,266667** | eksik bekleme kalemi nedeniyle genel güvence değil |

Olağan toplam **5,860000 ms**, pay **4,140000 ms**. Yalnız önceki tick’in 0,5 ms beklemesi eklendiğinde en kötü toplam **10,233333 ms**.

`docs/olcumler/MP4-dongu-yuku.md` §4.2’deki **3,32196875 ms**, 200 Hz’te **yer tutucunun ölçülen maksimumudur**; gerçek çekirdeğin 3,5 ms bütçesi ölçüm değildir. MP3 echo ölçümleri de iki yöndeki ayrı 0,4 ms köprü paylarını doğrudan ölçmemiştir.

**E1–E18 ve parametre köşeleri.** Beş değişken aralığın 32 köşesinde E1–E10’un yazılı sayısal ifadeleri denetlendi; aritmetik ihlal yok. Bu sonuç, eksik gecikme veya geliş varsayımlarını doğrulamaz.

| # | Yeniden hesap / değerlendirme |
|---|---|
| E1 | `5 < 10`: doğru. |
| E2 | `55 < 60` ve `450 < 500`: doğru. |
| E3 | `60 ≥ 10 + 5 + 14,5 = 29,5`: doğru; RTT gözlenen değerdir. |
| E4 | `4 × 100 = 400 ≤ 500`: doğru. |
| E5 | `500 > 14,5` ve `500 > 55`: doğru. |
| E6 | `16 ≥ 15`: doğru. |
| E7 | `15 < 16`: doğru. |
| E8 | Yazılı ifade `74,5 ≤ 75 < 100`: doğru. İkinci sonuç penceresiyle aynı hesap `79,5 > 75`; teslim güvencesi kurulmamış. |
| E9 | `60/2 = 30 ≥ 29,5`; `450 < 500`: doğru; koşulsuz teslim kanıtı değil. |
| E10 | `20 + 14,5 = 34,5 < 60`: doğru. |
| E11 | En az 2,5 ms TF aralığı, yarı açık 5 ms’de en çok iki TF verir: doğru. |
| E12 | Altı çerçeveli satırda HEAD ilk parçadan en çok beş pencere sonra gelir; sekiz kapanış yeterli. |
| E13 | `348 ≤ 520`; kalan `172 ≥ 89`; `520 + 399 + 258 = 1177 ≤ 1200`: doğru. |
| E14 | `260 ≥ 258`: doğru; ayrılmış yerle birlikte tam çerçeve ilerlemesi sağlar. |
| E15 | `520 + 59,85 + 60 + 260 = 899,85 ≤ 900`: doğru, uzun dönem ortalaması olarak. |
| E16 | `8 ≥ 8` doğru; kuyruk girişlerini açık olaylarla sınırlayan tümevarım yanlış. |
| E17 | `24 × 2 = 48`, `1096 × 2 = 2192`, hizmet hesabı `2488 B`: aritmetik doğru; geliş sınırı ve taşmazlık çıkarımı geçerli değil. |
| E18 | `9,733333 ≤ 10`: eksik kalemli toplam için doğru; bütün izin verilen sıralamalar için yanlış. |

**Sonar.** `1200` örnekli satır **1329 B**, `600` örnekli satır **693 B**. Kredi hızı **52 000 B/s**; azami boylarla yaklaşık **39,127** ve **75,036 satır/s**.

Her pencerede en az bir tam çerçeve gönderildiğinde, altı çerçeveli satır en çok altı pencerede tamamlanır. **`1 + 6 + 6 = 13 pencere = 65 ms`**, teslim edilen satır için muhafazakâr plan sınırıdır; seyreltilip atılan satıra veya duvar saati gecikmesine teslim garantisi vermez. Üreticinin `200–1200` örnek ve `80–40000` sample-period aralıkları canlı [Ping360 protokolüyle](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/) uyuşuyor.

Kutu formülünde `sample_period = 80`, `i = 0`, `c = 1500 m/s` için nominal zaman farkı **500 ns**, menzil **0,00075 m**; doğru.

**Bant.**

| Hesap | Sonuç |
|---|---:|
| Yön başına kapasite | 300 000 B/s = 1500 B/periyot |
| %60 sınırı | 900 B/periyot |
| Sim→kart bağlayıcı ortalama | **899,85 B/periyot = %59,99** |
| Sim→kart varsayımsal 50 m profili | yaklaşık **288,0 B/periyot** |
| Sim→kart sonar kredi sınırındaki profil | **467,585 B/periyot** |
| Kart→sim varsayımsal profil | **353,4 B/periyot = %23,56** |
| İlk adayın genişletilmiş varsayımları, yeni boylarla | **854,02 B/periyot = %56,9347** |
| Kart→sim yazılı hız varsayımlarının tam toplamı | **1264,0303 B/periyot = %84,2687** |

Son satırın **1264,1** olarak yukarı yuvarlanması sorun değil. Ancak bunun 1500’den küçük olması, E17’nin kuyruk kanıtını veya her duvar penceresinde talebin kapasiteyi aşmayacağını göstermez. Düşük öncelikli sınıfın ertelenmesi/atılması ve zorunlu sınıfın geliş sınırı birlikte kanıtlanmalıdır. Ayrıca sonuç **900 B/periyot gereğini karşılamıyor**.

## İskelete ya da diğer bölümlere notlar

`docs/gorevler/W1-A1.md` ana depoda bulundu ve okundu. A4, A8 ve A3 adayları ilgili çalışma ağaçlarından salt okunur incelendi; A4’ün erişilen güncel adayı **düzeltme turu 2** olduğundan, ilk tura dayanan A1 eşlemeleri dondurmadan önce yeniden uzlaştırılmalıdır.

K107’nin ayrı oturum sonu tick’i doğru bir çözüm: bu çağrı bir SIL ızgara tick’i değildir. K105’in üretim sırası ve K112’nin tam çerçeveli kredi yaklaşımı da gerekçeleri sınandığında geçerlidir. Buna karşılık E16/E17/E18’e dayanan kapanış hükümleri mevcut biçimleriyle sürdürülemez.

Canlı kaynakta pymavlink 2.4.50’nin [örnek başına durum alan yardımcı işlevleri](https://raw.githubusercontent.com/ArduPilot/pymavlink/v2.4.50/generator/C/include_v2.0/mavlink_helpers.h) doğrulandı. `common.xml`’i dışarıda bırakma gerekçesi, güncel [birincil XML’deki](https://raw.githubusercontent.com/mavlink/mavlink/master/message_definitions/v1.0/common.xml) büyük enum değerleri ve [2.4.50 C üreteci](https://raw.githubusercontent.com/ArduPilot/pymavlink/v2.4.50/generator/mavgen_c.py) bakımından destekleniyor; ancak **2.4.50 paketine ait belirtilen `common.xml` adresi 404 verdi**. Paket içeriğinin tam sürüm doğrulaması ve derleme bu incelemede yapılmış sayılmamalıdır.

[ST ürün sayfasından](https://www.st.com/en/evaluation-tools/stm32u5a9j-dk.html) STLINK-V3E/VCP köprüsü doğrulandı. **UM2967 PDF’sinin tam metnine erişim zaman aşımına uğradı**; yalnız arama çıktısındaki USART1/VCP bölümü görülebildi. Köprünün tampon ve gecikme sınırları doğrulanmış değildir.

İskelet 0.8’in önceliği korundu. Eski bölümlerdeki USB, 400 Hz, eski gecikme ve harici yer istasyonu farklılıkları bulgu sayılmadı.