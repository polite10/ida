# sol incelemesi: sozlesmeler

## Hüküm

**Dondurulamaz.** Revizyon listesi, fizik sınırı sırası ve kapsam temizliği büyük ölçüde eşitlenmiş. Ancak bekleyen genişletici komutun iptalinde bir boşluk, reddedilen partiden taşınan emniyet örneğinde kabul çelişkisi ve iki yanlış sayısal çıkarım kalmış.

Öncelik kullanıcı kararları → iskelet **0.10** → `docs/gorevler/W1-uyum.md` olarak uygulandı. Dosya değiştirilmedi, kurulum yapılmadı. Aşağıdaki A1–A8 adları `docs/sozlesmeler/` altındaki ilgili sözleşmeleri gösterir.

**Önceki çapraz incelemenin kapanışları**

| Bulgu | Hüküm | Bakılan yer | Sonuç / kalan eksik |
|---|---|---|---|
| P1-1 — Hedef seçimi ve opak FC alanları | KAPANDI | A1 Tablo 3.6, K64; A4 K90–K91, K119–K121; `core/include/ida/port.h`; A6 K52 | İşlem ve alan tanımları kaldırılmış; FC argümanları sıfıra kapatılmış. Eski alanları hâlâ varmış gibi anan açıklamalar yeni bulgu 6’da. |
| P1-2 — Görev türleri ve alan eşlemesi | KAPANDI | A1 Tablo 3.6a, K122; A4 K51; A5 K55–K57; `scenarios/sema/a5.schema.json` → `gorev`, `gorev_cozum`; senaryo örnekleri | Yedi tür ve zorunlu alanlar eşleşiyor; türe özgü anlamsal denetim A5-K55 ile bağlanmış. |
| P1-3 — Yolda bekleyen genişletici | KISMEN | A3 K101; A1 K36, K63, K112, K119; A4 K45, K48, K105 | Emülatör içi ve kart erteleme tamponu kapatılmış; aradaki sim gönderim kuyruğu açık: yeni bulgu 1. |
| P1-4 — Revizyon genişliği ve doyma | KAPANDI | A1 K106/H49; A3 K117–K118; A3/A6 `yuk_emniyet_girdiler`; `protocol/ida.xml` | `u32`, başlangıç 1, sıfırlanmama ve doyma sonrası ret eşleşiyor. |
| P1-5 — Tek artış listesi | KAPANDI | İskelet §3.2; A1 K89/K106; A3 K117; A5 K63 | Altı işlem ve artırmayanlar aynı anlamda; K89 ikinci artış tanımlamıyor. |
| P1-6 — Fizik sınırı sırası | KAPANDI | İskelet §3.2; A1 K85; A3 K95 | Beş adım aynı sırada ve aynı koşullarla yazılmış. |
| P1-7 — Panel–senaryo hakemliği | KAPANDI | A5 K62–K63/K109; A1 K85; A3 K95; A6 K35 | Senaryo gevşetmesi 4. adımda; önceki adımlarda artış varsa kalıcı ret ve olay kaydı var. |
| P1-8 — Tüp başına fiş/pim | KISMEN | A3 §3.4, K89/K93–K97; A3 şeması `girdi_secimi`, istekler ve `olay_emniyet_girdisi_degisti`; A6 ortak yükleri | Durumlar, istekler ve jeton bağı tamamlanmış; olay şeması tüpü zorunlu kılmıyor: yeni bulgu 5. |
| P1-9 — Emniyet örneğinin korunması | KISMEN | A1 K31/K124/E20; A4 K32/K102/K116 | Öncelikli yerleştirme ve taşmada oturum sonu eşleşiyor. Bayatlık varsayımı ve kapasite hesabı hatalı: yeni bulgular 3–4. |
| P1-10 — Değer düzeyinde kısıtlayıcı içerik | KAPANDI | İskelet §3.2; A4 K44–K45/K111/K113/K116 | Önceki değerden bağımsız kısıtlayıcı içerik korunuyor; çağrı geneli ret ile alanın korunması ayrılmış. |
| P1-11 — Olay süreleri | KAPANDI | A1 §3.9/K104; A4 K66/K107; `port.h` süre sabitleri | `apply_by` 16–1000 ms, `hold` 60–450 ms. |
| P1-12 — Sıra numaraları | KISMEN | İskelet §3.1; A1 K31/K66/K123; A4 Bölüm 2/K25 | Ortak karşılaştırma tanımlı; K66 uzlaştırması sınırda bu tanımı sağlamıyor: yeni bulgu 2. |
| P1-13 — Kamera genişliği ve pozlama | KAPANDI | A2 §3.6/§3.8; `sim/include/ida/a2_wire.h`; A8 K36 | Kalibrasyon `u16`; pozlama pozitif, çift ve ≤1 s; A8 ortak sınırlara atıf veriyor. |
| P1-14 — A1 özeti ve A8 sürümü | KAPANDI | A6 §3.5; A3/A6 `yuk_algilama_kayit`; iki kanal şeması | Tek `a1_dialect_hash` kullanılıyor; algılama yükünde sürüm iki `u16` alana ayrılmış. |
| P1-15 — Günlük ve ret kanalları | KAPANDI | A4 §3.15–§3.16, K90–K91/K120; A1 §3.7; A6 K52 | Günlük kodları, argüman anlamları ve engel bitleri kapalı; `reason ≥ 0x100` kullanılmıyor. |

**U maddeleri ve tur içinde kabul edilen yorumlar**

| Madde | Hüküm | Bakılan yer | Sonuç / kalan eksik |
|---|---|---|---|
| U1 | KAPANDI | A1 Tablo 3.6/3.6a; A4 K51/K90/K91/K119/K120; A5 K55; A6 K52; başlık, XML ve katalog | İşlevsel kapalı listeler eşleşiyor. |
| U2 | KAPANDI | İskelet §3.2; A1 K89/K106; A3 K117 | Altı işlem, artırmayanlar, genişlik ve doyma aynı. Dünya revizyonunun JSON alt sınırının 1 olması, 0’ın yalnız platformdaki “örnek görülmedi” değeri olmasıyla tutarlı. |
| U3 | KAPANDI | A1 K85; A3 K95; A5 K62–K63; A6 K35 | Sıra ve senaryo ret davranışı eşleşiyor. |
| U4 | KISMEN | A3 K101; A1 K63; A4 K105 | Tanımlanan iki iptal noktası mevcut; sim’in A1 gönderim kuyruğu kapsam dışında kalmış. |
| U5 | KISMEN | A4 K116; A1 K31/K124/E20; A4 K102 | U5a kapanmış; U5b’de yeni bulgular 3–4 kalıyor. |
| U6 | KISMEN | A3 K89/K93/K94/K96; A3 şeması; A6 ortak yükleri | Olay şemasındaki tüp zorunluluğu eksik. |
| U8 | KISMEN | A1 K123/K66; A4 Bölüm 2/K25 | Karşılaştırma aynı; uzlaştırma algoritmasının sonucu her zaman bu pencereye girmiyor. |
| Kabul edilen yorum 1 — Çağrı geneli ret ayrımı | KAPANDI | İskelet §3.2; A4 K44/K45/K116 | Süregelen kısıtlama kendi alanını ve kapsadığı tüpleri koruyor; ilgisiz komutların reddi zorunlu değil. |
| Kabul edilen yorum 2 — Kapasite aşımında oturum sonu | KISMEN | A1 K124/H52/E20; A4 K102 | Güvenli geri dönüş eşleşiyor; E20’nin 3,15 ms hesabı yanlış. Ölçüm koşulları henüz doğrulanmış değil. |
| Kabul edilen yorum 3 — `sifirla` ve çalıştırma bağı | KAPANDI | A3 K57/K67/K94/K117; A1 K128 | `sifirla` için bağımsız artış yok; etkin oturum kapanışının artışı ayrı. Kuyruktaki istekler uygulanmadan önce `run_id` ile yeniden denetleniyor. |
| Kabul edilen yorum 4 — İDA-0 kamera değerleri | KAPANDI | A5 §8 madde 9; `scenarios/ida0.toml` kamera açıklamaları | Değerler ölçüm/kalibrasyon değil, mimari varsayım olarak işaretlenmiş. |

## Düzeltilmesi gerekenler

**Uyum turu sonrasında kalan veya ortaya çıkan çelişkiler**

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `docs/sozlesmeler/A3-web.md` K101 ↔ `docs/sozlesmeler/A1-kart-baglantisi.md` K36/K63/K112/K119 ↔ `docs/sozlesmeler/A4-port.md` K45/K48/K105 | **Emülatör ile kart arasında bekleyen genişletici korunmuyor.** Revizyon 7’deki `ARM` emülatörden çıkar, denetimi geçer ve sim’in toplu komut kuyruğunda kredi/kapasite bekler. Sonraki `DISARM` kritik yoldan önce gönderilip uygulanır. `ARM` henüz kartta olmadığından K63 onu iptal edemez; daha sonra ayrı çağrıda ulaşabilir. K48 yalnız görülmüş kimlikleri eler; bu `ARM` henüz görülmemiştir. | A1’in sim tarafındaki bekleme boyunca kabul revizyonu bağını koruyun; gönderimden önce yeniden denetleyin veya araya giren kısıtlamada bekleyen eski genişleticileri sonuçlandırarak iptal edin. Mevcut iki kontrol noktası arasındaki boşluğu kapatın. | kritik |
| 2 | `docs/sozlesmeler/A1-kart-baglantisi.md` K66 ↔ K123; `docs/mimari/00-iskelet.md` §3.1 | **Uzlaştırmanın “yeni kimlik” sonucu yanlış.** `last_cmd_id=10`, `h=11`, `c=2147483658` olsun. `c−h=2147483647` olduğundan K66 `c`’yi seçer. Ancak `c−last_cmd_id=2147483648`; K123’e göre seçilen kimlik eskidir. | Seçimi doğrudan `last_cmd_id` karşısındaki geçerli pencereyle doğrulayın; seçilen sonuç farkı 1…`2³¹−1` içinde olmalı. Ardıl değer hesaplanırken sıfırı atlama da ilk komut için açıkça uygulanmalı. | kritik |
| 3 | `docs/sozlesmeler/A1-kart-baglantisi.md` E20 ↔ K05, §3.2 ve `protocol/ida.xml` → `IDA_SAFETY` | **63 B en küçük pencere boyu değildir.** `IDA_SAFETY`’nin 34 B’si azami çerçeve boyudur. `flags=VALID`, `inst=0`, `estop=0`, `tube=[0,0]` için son sıfırlar kırpılır: yük 17 B, çerçeve 29 B olur. TF ile pencere 58 B’dir. Dolayısıyla 0,21 ms ve ondan türetilen 3,15 ms koşulu geçerli değildir. | Alt sınırı kırpılmış geçerli çerçevelerden yeniden çıkarın; E20’deki koşullu kapasite hesabını düzeltin. K124/H52’nin taşmada oturum sonu davranışını koruyun. MAVLink sıfır kırpması birincil belgede zorunludur. [MAVLink serileştirme](https://mavlink.io/en/guide/serialization.html#empty-byte-payload-truncation-mavlink-2). | kritik |
| 4 | `docs/sozlesmeler/A1-kart-baglantisi.md` K31/K124 ↔ `docs/sozlesmeler/A4-port.md` K33/K34/K102/K116 | **Reddedilen partinin emniyet örneği mutlaka bayat değildir.** Son kabul edilen SAFETY `seq=10, t=1 s, estop=1` olsun. `DUP` TF’nin penceresinde `seq=11, t=1 s, estop=0` bulunursa A1 örneği değiştirmeden sonraki çağrıya taşır. A4-K34 eşit zamanı ve büyük örnek sayacını kabul eder; sonraki çağrıda başka kısıtlayıcı örnek yoksa K116 gevşetmeyi uygular. A1’in “çekirdek bayat/yinelenen diye reddeder” sonucu çıkmıyor. | Parti reddinin örneğin gevşetici içeriğini kabul ettirmeyecek biçimde sonraki çağrıya nasıl taşındığını tanımlayın. Kısıtlayıcı alanlar korunmalı; örneğin kendi sayacının da bayat olacağı varsayılmamalı. A1 ve A4 aynı sonucu tarif etmeli. | kritik |
| 5 | `docs/sozlesmeler/A3-web.md` K89/K96 ↔ `docs/sozlesmeler/sema/a3-kanallar.json` → `$defs/olay_emniyet_girdisi_degisti`; A6 ortak olay kaydı | **Olay şemasında tüp isteğe bağlı.** İstek şemaları `girdi_secimi` ile doğru koşullandırılmış; olay şeması aynı bağı kullanmıyor ve `required` içinde `tup` yok. Örneğin pimin takıldığını bildiren, bütün diğer alanları dolu ama tüpsüz olay şemaya uyuyor. Acil durdurma olayına tüp eklenmesi de engellenmiyor. | Olay şemasını K89’daki seçim kuralına bağlayın: fiş/pimde `tup` zorunlu, acil durdurmada yasak. A6’nın kullandığı olay doğrulaması aynı tanımı izlemeli. `properties` içinde bulunmak tek başına zorunluluk getirmez. [JSON Schema doğrulama belirtimi](https://json-schema.org/draft/2020-12/json-schema-validation#section-6.5.3). | önemli |
| 6 | `docs/sozlesmeler/A1-kart-baglantisi.md` §3.7, Ek A, §8.2 A4 madde 10 ↔ güncel `core/include/ida/port.h` ve A4-K102 | **Bazı karşı belge anlatımları eski kalmış.** §3.7/Ek A kaldırılmış `ida_event_t.fc_ref` ve `ida_fc_status_t.ext_len/ext[]` üyelerini hâlâ mevcut A4 alanları gibi anıyor. §8.2, A4-K102’nin henüz “emniyet örnekleri önce” demediğini söylüyor; güncel K102 bunu zaten içeriyor. | Bu açıklamaları güncel karşı tanımlarla eşitleyin. Aktif başlık/XML alanı olarak yeniden ekleme gereği çıkarılmamalı; mevcut işlevsel kapsam temizliği korunmalı. | küçük |

## Yeniden hesaplananlar

**Yedi geliş sırasının yeniden yürütülmesi**

| Sıra | A1 | A3 | A4 | A5 |
|---|---|---|---|---|
| 1. Aynı sınırda kısıtla + gevşet; iki geliş sırası | K85: kısıtlama önce, gevşetme ret. | K95: aynı. | K45/K116: aynı çağrıda ilgili kısıtlama korunur. | K63 aynı hakemliğe bağlı; karşıt iki zamanlı atama K62 ile yüklemede reddedilir. |
| 2. R7’de alınmış gevşetme; N’de durumu değiştirmeyen kısıtlama; N+1’de teslim | K106: R8; R7 damgalı izin olayı ret. Kart tamponundaki genişletici K63 ile iptal. Sim kuyruğu için bulgu 1 kalıyor. | K92/K117: R8; eski istek/jeton ve emülatördeki komut ret. | K105 platform iptalini ister; komutun revizyonunu kendisi taşımaz. | Panel isteğinin hakemliği ortak sıradadır; senaryo reddedilen gevşetmeyi yeniden kurmaz. |
| 3. Önce açık; bayat kapalı + yeni açık örnek aynı çağrıda | K31/K124 kapalı örneği korur. | K95 kısıtlamayı üstün tutar. | K116: etkin değer kapalı; K111/K45 yeni kapanışı uygular. | K63: çelişen atamalarda kısıtlama üstün. |
| 4. Önce zaten kapalı; bayat kapalı + yeni açık | Örnekler korunarak A4’e ulaşır. | Durumu değiştirmeyen kısıtlama da revizyon artırır. | Kapalı kalır; yeni kapanış olayı yok. Kendi alanının gevşemesi engellenir. | K63: kısıtlayan atama uygulanır; gevşetme ret. |
| 5. Koşullu senaryo bırakması + panel acil durdurması | K85 adım 2, sonra adım 4: basılı, bırakma ret. | K95: aynı. | Teslim edilen kısıtlama K116 ile korunur. | K63: `gevsetme_reddedildi`; yeniden deneme yok. |
| 6. R7, acil durdurma basılı; `SAFETY_CLOSE` + panel bırakması | K85/K87: kapanış R8; bırakma ret; **basılı, R8**. | K95/K117: **basılı, R8**. | Zaten kısıtlayan etkin değer korunur; yinelenen kapanış üretmez. | Ortak sıraya bağlı; ek atama yok. |
| 7. Onaylı `ARM` emülatörde bekler; `DISARM`; sonra teslim | Emülatör ARM’ı atar; kart erteleme tamponu varyantında K63 atar. | K101: revizyon farklı, `bayat_yetki`. | K105/K45 bu iki yolun kart tarafını tamamlar. | Aynı emülatör yolunu kullanır. Aradaki sim kuyruğu varyantı bulgu 1’dir. |

Bu tablodaki 3–4. sıralar **örneğin kendisinin gerçekten bayat olduğu** durum içindir. Bayat TF ile yeni örnek sayacının birleşimi ayrı olarak bulgu 4’te açık kalıyor.

**A4-K45/K116 ayrımı için üç sıra**

| Başlangıç ve çağrı | Sonuç |
|---|---|
| Etkin `estop=0`; bayat `estop=1`, yeni `estop=0` ve genişletici komut | Etkin `estop=1`; yeni kapanış ve çağrı geneli genişletici reddi. |
| Etkin `estop=1`; bayat `estop=1` ve yeni `estop=0` | Etkin değer 1 kalır; yeni kapanış olayı üretilmez. Yalnız kabul edilen `estop=0` taşıyan sonraki çağrı gevşetebilir. |
| Tüp f1’in pimi zaten takılı; aynı çağrıda f1’in takılı değeri yinelenir ve f2’nin pimi geçerli örnekle çıkarılır | f1 kısıtlı kalır. f2’nin kendi alanında kısıtlayıcı içerik yoksa gevşemesi mümkündür; f1’e izin açan olay ve `FIRE` üretilemez. Yeni kapanış olmadığından ilgisiz komutların topluca reddi gerekmez. |

Bu ayrımın kendisinde kısıtlayıcı değeri kaybettiren yeni bir yol bulunmadı. Sorun, bulgu 4’teki teslim/kabul sınırında.

**Sayısal sonuçlar**

| Denetim | Yeniden hesaplanan sonuç |
|---|---|
| Revizyon son değeri | `2³²−1 = 4 294 967 295`; sonraki kısıtlamada aynı değerde kalır, gevşetmeler reddedilir. |
| Tek acil durdurmanın iki tüpü kapatması | Komut işlemi için **+1**; K89’dan ek artış yok. Sonradan uygulanan ayrı bir `SAFETY_CLOSE`, ayrı işlem olarak +1’dir. |
| Aynı sınırda iki iznin dolması | Toplam **+1**; iki tüp için +2 değil. |
| E20 kırpılmış pencere | SAFETY 29 B + TF 29 B = **58 B**; `58/300 = 0,193333 ms`. |
| E20 kapasite koşulu | Belgedeki `S≤1+w` modeli korunursa `15×0,193333 = 2,90 ms`; **3,15 ms değil**. On altı ek pencere 3,093333 ms’ye sığar; önceki bekleyen örnekle 17 olur. |
| E8 | `2×35+5+14,5+0,86 = 90,36 ms ≤95 ms <100 ms`: doğru. |
| E9 payı | `60/2−(10+5+14,5)=0,5 ms`: doğru; 14,5 ms gözlemdir, garantili üst sınır değildir. |
| E10 | `30+14,5=44,5 ms <60 ms`: doğru. |
| Uçtan uca bütçe | **9,733333 ms**; 10 ms’ye pay **0,266667 ms**. Tahmini girdilere dayalı hesap. |
| Olay kimliği | `(5<<32)\|3 = 21 474 836 483`: doğru. |
| `IDA_EVENT` boyu | **37 B yük, 49 B imzasız çerçeve**. |
| XML özeti | **565 kanonik satır**, `dialect_hash=0x3939788984A5FF80`: bağımsız yeniden hesaplamayla eşleşti. |
| A3/A6 ortak yükler | İki JSON kataloğundaki ortak yük tanımları derin eşit. |

Seri aritmetiğin yarım uzay sınırı birincil [RFC 1982 §3.2](https://www.rfc-editor.org/rfc/rfc1982.html#section-3.2) ile denetlendi. İskeletin bu farkı “eski/ret” sayan mimar kararı değiştirilmedi; bulgu 2 doğrudan projenin kendi karşılaştırmasına dayanıyor.

## İskelete ya da diğer bölümlere notlar

**Önceki iskelet notlarının kapanışı**

| Önceki not | Hüküm | Bakılan yer | Sonuç |
|---|---|---|---|
| §2/§5/§9 kapsam ifadeleri | KAPANDI | `docs/mimari/00-iskelet.md` §2, §5, §9 | Sözleşmelerde hedef seçimi, hedefleme çözümü ve atış ölçütü açıkça dışlanmış. |
| Revizyonun genel artış ifadesi | KAPANDI | İskelet §3.2 | Altı işlemlik kapalı liste ve artırmayanlar yazılmış. |
| Süreli izinlerin saat alanları | KAPANDI | İskelet §4; A1 §3.9; A4 K66 | Üretici duvar saati, alıcı uygulama anından sim zamanı olarak ayrılmış. |
| W1 ortak belgesinin eski sürüm atfı | KAPANDI | `docs/gorevler/W1-ortak.md`, “Öncelik sırası” | Güncel sürüm ve uyum turundaki 0.10 açıkça belirtilmiş. |

**Kapsam denetimi:** Kaldırılan adların aktif tanımları `port.h`, A5 şeması ve örnekleri, A3 katalogları, A6 kodlama tanımı, `ida.xml` ve `komutlar.json` içinde bulunmadı. XML’deki eski `IDA_FC_SELECT` adı yalnız kaldırıldığını açıklayan metinde kalıyor. A1’deki yanıltıcı mevcut-alan anlatımları bulgu 6’dadır.

A4 §3.15’in 23 günlük kodu ve §3.16’nın 18 engel biti emniyet tanılamasıyla sınırlı. Bu tablolarda yeni hedef seçimi, hedefleme çözümü veya atış ölçütü kanalı saptanmadı.

`protocol/komutlar.json` içindeki `fc_fire`, `genisletici`, `onay:true`, yalnız zorunlu `tube:0..1` parametreli ve ek özelliğe kapalı. Hedef, menzil, yön veya ölçüt taşımıyor. A1 Tablo 3.6/K125, A3-K99/K100 ve A4-K63/K67, `FIRE` olayının üretimini çekirdekte tutuyor. U16 kararı bulgu sayılmadı.

**Mimar kararlarına ilişkin sonuç:** K45/K116 ayrımı ve kapasite aşımında oturumun kapanması kararlarında ayrıca bir kapsam gevşemesi saptanmadı. U4’ün uygulama noktaları arasındaki boşluk ve U5b’nin bayatlık varsayımı, kararların amaçladığı emniyet sonucunu tamamlamıyor; bunlar bulgu 1 ve 4’tür.

Belirtilen yerel kaynaklara ve kullanılan birincil web kaynaklarına erişildi. `jsonschema` kurulu olmadığından tam otomatik JSON Schema doğrulaması yapılmadı; şema karşı örneği tanım üzerinden denetlendi. Kart ölçümü veya ürün davranışı testi yapılmadı; W3/W4 ölçümleri kapanmış sayılmadı.

dondurulamaz