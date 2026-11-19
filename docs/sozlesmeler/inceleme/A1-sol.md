# sol incelemesi: A1-kart-baglantisi

## Hüküm

**Aday dondurulamaz.** XML’in çerçeve boyları, CRC değerleri ve altın dizileri tutarlı; ancak emniyet geçişleri, zamanlama kabulü, kuyruk davranışı ve sonar tesliminde kritik açıklar var. İki tarafın yalnız bu sözleşmeden uyumlu uygulamalar üretmesi henüz güvence altında değil.

## Düzeltilmesi gerekenler

Aşağıdaki bölüm ve kural atıfları `docs/sozlesmeler/A1-kart-baglantisi.md` içindir.

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | §3.6; `protocol/ida.xml:168`; §3.7 `FC_STATUS` | Kapsam dışında olduğu söylenen hedef seçimi, `FC_OP_SELECT` ile açıkça mevcut. Anlamı sınırlandırılmamış `arg0/arg1` ve `ext[]` kapsam sınırını dolanabilen genel kanallar bırakıyor. | Kapsam dışı işlemi çıkar; opak alanların izin verilen emniyet ve durum kapsamını bağlayıcı biçimde sınırla. | kritik |
| 2 | K13, K16, K22 | Oturum sonu ve yeni oturum başlangıcı “bir sonraki çekirdek çağrısına” bırakılıyor. El sıkışma o çağrıdan önce tamamlanırsa tek `link_event` alanında `SESSION_END` ile `SESSION_START` çakışıyor; daraltmanın işlendiği garanti değil. | Oturum sonu daraltmasının yeni başlangıçtan ve yeni girdilerin işlenmesinden önce tamamlanmasını zorunlu kıl. | kritik |
| 3 | K78, K85 | Güvenli `ASYNC` çıktıdan **sonra** gelen olağan yanıt iptal edilmiyor. İkisi aynı fizik sınırına düşerse K85’in dördüncü sırasında hangisinin son uygulanacağı tanımsız. | Aynı adımda çakışan sürekli çıktıların önceliğini ve iptal kapsamını tek anlamlı tanımla. | kritik |
| 4 | K86, K89; AB-014, AB-029 kapanışları | Sanal emniyet girdisinin kapanması izni kaldırıyor; fakat bekleyen eski `SAFETY_OPEN`’ı hükümsüzleştirmiyor. Girdi yeniden açıldığında eski olay, yalnız uygulanmış `SAFETY_CLOSE` kimlikleriyle karşılaştırıldığı için yeniden izin verebiliyor. | Her daraltıcı emniyet değişiminin bekleyen izin olaylarına etkisini tanımla; eski yetkinin kendiliğinden geri gelmesini engelle. | kritik |
| 5 | §3.7; K72–K88 | CRC’si doğru fakat alanları geçersiz çıktılarda sim davranışı eksik: örneğin geçersiz `kind`, `tube`, `call_kind`, `n_events` veya hiç gönderilmemiş partiye bağlı olay. K83/K84 son durumda uygulama anını ve TF yazım başlangıcını bile bulamıyor. | Alıcı doğrulama sırasını, ret sonucunu, sayaçları ve hiçbir etki uygulanmayacağını tanımla. | kritik |
| 6 | §3.7 `CALL_TRACE`; `protocol/ida.xml:704` | Tarif edilen sıra **28** damga içeriyor: `1 + 1 + 16 + 2 + 8`. Dizi **27** elemanlı. Son algılama damgası taşınamıyor; tarife göre doldurma sınır dışına yazmaya da yol açabilir. | Dizi ve indisleri düzelt; boyları, CRC_EXTRA’yı, özeti, altın diziyi ve bant hesabını birlikte güncelle. | kritik |
| 7 | K05, K51 | K51’in “yükü 6 bayttan kısa DATA” reddi MAVLink kırpmasıyla çelişiyor. `seq=1`, `inst=0`, `frag_idx=0`, sıfır profil parçasının geçerli tel yükü yalnız **1 bayt**. | Uzunluk denetimini MAVLink sıfırla tamamlama kuralıyla uyumlu yap. Geçerli sıfır parçayı bozuk sayma. | kritik |
| 8 | E12; K36, K51 | `⌈1329/110⌉=13` hesabı parçaların bölünemezliğini atlıyor. **258 baytlık DATA, 110 bayta sığmaz.** Komut önceliği nedeniyle sonar ilerlemesine ayrıca alt sınır yok; 32 kapanış yeterlilik kanıtı değil. | Tam çerçeveler üzerinden ilerleme sınırı kur; yetim süresini bu sınırdan türet veya garantisiz olduğunu açıkça belirt. | kritik |
| 9 | K34–K36, K103; E13; §3.10 | Dört olay sonucu sınırı ile “her sonucu kritik üye olarak, ertelemeden gönder” birlikte uygulanamıyor: yeni olaylar ve eski olayların yinelemeleri aynı pencerede dört sonucu aşabilir. Ayrıca 691 bayt, K34’ün bütün sensör kapasitelerinin üst sınırı değil. | Taşma ve sonuç birleştirme davranışını tanımla; izin verilen yapılandırmaların gerçek kritik tepesini ve zaman bütçesini doğrula. | kritik |
| 10 | K67, K90, K95; §3.9 bütçesi | K90, `CMD_RESULT` ve `CALL_TRACE` dahil bekleyen çerçeveleri kuyruk dolunca atabiliyor; K67/K95 bunların gönderilmesini zorunlu tutuyor. Ayrıca taşımaya önceden verilen baytlar sınırlanmadığından kritik çıktının önünde yalnız **bir** çerçeve bulunacağı kanıtlanmıyor. | Kaybedilebilir mesajları ayır; zorunlu sonuçların teslim/başarısızlık kuralını ve taşıma kuyruğundaki azami engellemeyi sabitle. | kritik |
| 11 | K43, K75, K79; §3.11 | Kabul, planlanan duvar son tarihi yerine sim’in fizik adımını hesaplayıp hesaplamadığına bağlı. Sim gecikip yanıtı sonradan hazır bulursa gerçek zaman kaçırılması görünmeyebilir. Bariyerde geç gelip uygulanan parti de `parti_kacirildi` sayılmıyor. | Planlanan son tarihin kaçırılmasını yuva sonucundan ayrı izle; bariyerde kurtarılanlar dahil her kaçırılan son tarihi say ve geçersizliği bağla. | kritik |
| 12 | §3.9 E7; §8.2 | İzin verilen `apply_by_ns = 15 ms`, zorunlu `T_olay_yin < apply_by_ns` koşulunu sağlamıyor: **15 < 15 yanlış**. | Geçerli parametre aralığını bütün zorunlu eşitsizliklerle uyumlu tanımla. | kritik |
| 13 | K102; §3.8 | İki SIL koşusunda bayt eşitliği, yalnız KEEPALIVE istisnasıyla mümkün değil: nonce’lar, `sess`, `boot_count` ve bunlardan türeyen olay kimlikleri için aynı başlangıç koşulu tanımlanmamış. | Başlangıç kalıcı durumunu ve kimlik üretimini sınama girdisine bağla veya karşılaştırmada normalleştirilecek alanları açıkça belirt. | kritik |

## Yeniden hesaplananlar

**Zamanlama:** `P=5 ms`, `h=1,25 ms`, `L_act=10 ms=8h` doğru.

| Eşitsizlik | Yeniden hesap / değerlendirme |
|---|---|
| E1 | `5 < 10`: doğru. |
| E2 | `50 < 100 < 500`: aritmetik doğru; farklı saat alanlarında tek başına davranış garantisi değil. |
| E3 | `100 ≥ 10+5+14,5 = 29,5`: doğru. Son komut `a` anında uygulanmışsa 19 kaçırma `a+95 ms`’ye ulaşır; nötr sınırı `a+100 ms`’dir. |
| E4 | `4×100 = 400 ≤ 500`: doğru; teslim gecikmesini ve sınır anındaki işlem sırasını kapsamıyor. |
| E5 | `500 > 14,5` ve `500 > 50`: doğru. |
| E6 | Varsayılanda `20 ≥ 15`: doğru. |
| E7 | Varsayılanda `15 < 20`: doğru; izin verilen 15 ms sınırında yanlış. |
| E8 | `50+5+14,5 = 69,5 ≤ 75 < 100`: aritmetik doğru; RTT gözlenen maksimum, garanti edilmiş üst sınır değil. |
| E9 | `200/2 = 100 ≥ 29,5`; `200 < 500`: aritmetik doğru. Bariyerde sim ve duvar süreleri ayrışır. |
| E10 | `20+14,5 = 34,5 < 100`: doğru; nötr çıktının tesliminin ayrıca karşılanması gerekir. |
| E11 | En az 2,5 ms aralık, yarı açık 5 ms pencerede en çok iki TF verir: doğru. |
| E12 | `⌈1329/110⌉=13` aritmetik doğru, çerçeve teslimi çıkarımı yanlış. |
| E13 | `691+399=1090 ≤1200`: verilen örnekte doğru; genel kritik tepe kanıtı değil. |

**Uçtan uca bütçe:** Verilen kalemlerle en kötü toplam **9,430 ms**, olağan toplam tam tel süreleriyle **5,8467 ms**; kalan paylar **0,570 ms** ve **4,1533 ms**. Belgedeki 5,84 ms, yuvarlatılmış satırların toplamıdır.

MP3, echo dağılımlarını ve sabit yükte akışı ölçmüş; MP4, **yer tutucunun** 200 Hz ağır yük maksimumunu **3,32196875 ms** ölçmüş. Gerçek çekirdeğin 3,5 ms bütçesi, dağıtım/kodlama, sim hazırlığı ve yön başına köprü payları tahmin. **0,57 ms’nin gerçekçi olduğu doğrulanmış değil**; özellikle kuyruk engellemesi henüz sınırlandırılmamış.

**Bant:** `3 000 000 / 10 = 300 000 B/s`; `300 000 / 200 = 1500 B/periyot`; `%60 = 900 B/periyot`.

| Hesap | Sonuç |
|---|---:|
| Aşağı yön kritik sensörler, tablodaki hızlarla | 131,925 B/periyot |
| Ping360 dışı aşağı yön | 207,210 B/periyot |
| +600 B Ping360 payı | **807,210 B/periyot = %53,814** |
| Yukarı yön olağan toplam | 347,870 B/periyot |
| +186 B günlük +311,4 B algılama | **845,270 B/periyot = %56,351** |
| Çağrı izi 28 elemana düzeltilirse aynı profil | **851,270 B/periyot = %56,751** |

%53,8 ve %56,4, **tablodaki varsayımsal hız profili için** doğru. Bu hızların tamamı bağlayıcı üst sınırlar değil. Ayrıca 200 örnekli satırlar kabul edildiğinden 173 satır/s, bütün yapılandırmaların sonar kayıt hızı üst sınırı sayılamaz.

Tek örnekli verilen tepe: `29+59+377+102+124=691 B`; kamera ile **1090 B**. Ancak örneğin üç DVL örneği ve üç kamera kaydıyla kritik yük **869 B**, toplam **1178 B** olur; pencereye sığmasına rağmen aynı gecikme hesabı **10,023 ms**’ye çıkar.

Ping360: 1200 örnek **1329 B**, 600 örnek **693 B**. Nominal kutu formülü doğru: `Δt=sample_period×25 ns`, yansıma zamanı `t_tx+(i+½)Δt/2`. Örneğin `sample_period=80`, `i=0`, `c=1500 m/s` için **500 ns**, **0,00075 m**. Üreticinin örnek sayısı ve periyot sınırları da uyuşuyor. [Blue Robotics protokolü](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/)

**MAVLink:** 37 mesajın yük/çerçeve boyları ve CRC_EXTRA değerleri yeniden hesaplandı; 37 altın çerçevenin CRC’si doğrulandı. Alan değerlerinden yeniden üretilen örnekler:

| Mesaj | Çerçeve boyu | Son iki CRC baytı | Sonuç |
|---|---:|---|---|
| 46000 | 33 B | `dd ba` | Aynı |
| 46010 | 29 B | `18 c1` | Aynı |
| 46030 | 138 B | `b6 e5` | Aynı |
| 46060 | 42 B | `4c 17` | Aynı |

Kanonik metin **533 satır**, özet **`0xAEA95FC1D32CA393`**; CRC denetim girdisi sonucu **`0x6F91`**. Ek yük, alan sıralaması, CRC_EXTRA ve kırpma açıklamaları birincil kaynakla uyuşuyor. [MAVLink serileştirme](https://mavlink.io/en/guide/serialization.html)

46000–46099 özel dialect için kullanılabilir; bu, resmî tahsis iddiası değildir. [MAVLink kimlik kuralları](https://mavlink.io/en/guide/define_xml_element.html)

`common.xml`’i içermeme gerekçesi teknik olarak geçerli: ilgili enum değerleri `2147483648`; üreteç bunları C enum’u olarak yayıyor ve standarttaki `int` sınırıyla çelişiyor. Bu incelemede derleme yeniden çalıştırılmadı. [common.xml](https://github.com/mavlink/mavlink/blob/master/message_definitions/v1.0/common.xml), [2.4.50 üreteci](https://github.com/ArduPilot/pymavlink/blob/v2.4.50/generator/mavgen_c.py), [WG14 §6.7.2.2](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2310.pdf)

## İskelete ya da diğer bölümlere notlar

`docs/mimari/acik-bulgular.md` A1 başlığındaki **49 bulgunun tamamı** kapanış tablosunda; ek AB-211, AB-215, AB-224 ile toplam **52**. Sayısal özet doğru, kapanış hükümlerinin tümü doğrulanmış değil.

| Kritik bulgu | Derin denetim sonucu |
|---|---|
| AB-001 | Süre dolması ve oturum bitişinde sıfır itki isteği açık; ESC’nin son değeri tutmasına bırakılmıyor. A1 kısmı karşılanmış. |
| AB-002 | Nonce/FIFO eski akışı ayırıyor; fakat hızlı yeniden bağlantıda oturum sonunun işlenmesi açık kalıyor (#2). |
| AB-014 | Kapalı izin olayı engelliyor; sanal girdinin kapanıp açılması eski bekleyen izinleri hükümsüzleştirmiyor (#4). |
| AB-015 | `CLOSE=8`, eski `OPEN=7` örneği reddediliyor; aynı adımda sürekli çıktılar arasındaki öncelik eksik (#3). |
| AB-016 | Açılış üst bitleri denetleniyor, sonuçlar çalıştırma boyunca saklanıyor; açılış sayacı üretimi A4 olmadan doğrulanamadı. |
| AB-017 | Alıcı ömrü uygulama anından sim zamanında sayılıyor; üretici duvar saati davranışı yalnız A4 atfına dayanıyor, doğrulanamadı. |
| AB-027 | `n_events` geçerli girdide bariyeri tamamlıyor; geçersiz sayı ve olay bağı için alıcı kuralı eksik (#5). |
| AB-028 | `REJ_SAFE` sonrası aynı kimliğin yeniden gelmesi saklı ret sonucunu alıyor; sonradan uygulanmıyor. Karşılanmış. |
| AB-029 | Eski açmanın daha yeni kapatma olayını aşması engellenmiş; sanal emniyet iptalleri aynı kapsama bağlanmamış (#4). |
| AB-030 | HEAD’in son gönderilmesi teslim partisi çelişkisini çözüyor; ayrı parçalama ve ilerleme hataları sürüyor (#7–8). |
| AB-040 | `PERMIT_OPEN` ve olay sonucu alanları mevcut; A4’e fiilî eşleme doğrulanamadı. |
| AB-044 | Yinelemelerde aynı `cmd_id` taşınması tanımlı; çekirdeğin tekilleştirmesi A4 olmadan doğrulanamadı. |

`docs/gorevler/W1-A1.md`, A4/A8 adayları ve ilgili başlık dosyaları bu çalışma ağacında bulunamadı. Bunların atıfları **doğrulanamadı**; eksiklikleri tek başına A1 kusuru sayılmadı. UM2967 PDF’sine erişim zaman aşımına uğradı; paketlenmiş 2.4.50 `common.xml` bağlantısı da alınamadı. Bu kaynaklara ilişkin tam sürüm doğrulaması yapılmış sayılmamalı.

İskelet 0.8’in önceliği korundu; eski bölümlerdeki USB, 400 Hz, eski gecikme ve harici yer istasyonu farkları bulgu sayılmadı. Dosya değiştirilmedi, kurulum yapılmadı.