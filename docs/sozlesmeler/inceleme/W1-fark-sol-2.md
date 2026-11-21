# sol incelemesi: sozlesmeler

## Hüküm

**Dondurulamaz.** Önceki bulguların çoğu kapanmış; ancak yeni metinde fizik ve nominal kamera montajını karıştıran bir kural ve yeni çalıştırmada aynı karenin iki kez düşürülmesine yol açabilen bir işlem sırası var.

İnceleme `40b8d63..6b63c6e` farkının **PARÇA 2** kapsamıyla sınırlıdır. Öncelik kullanıcı kararları → iskelet 0.11 → `docs/gorevler/W1-uyum.md` olarak uygulandı. Dosya değiştirilmedi, kurulum yapılmadı.

Aşağıdaki A2–A8 kısaltmaları `docs/sozlesmeler/` altındaki ilgili sözleşmeleri; P3 ve P4, önceki `W1-dogrulama-sol-3.md` ve `W1-dogrulama-sol-4.md` incelemelerini gösterir.

**Önceki bulguların kapanışı**

| Bulgu | Hüküm | Baktığım yer | Kalan eksik |
|---|---|---|---|
| P3-D1 — Sim kabul kapısının zaman durumu | KAPANDI | A8-K17/K78/K79; A4-K122 | Nesil artışında sim’in zaman izi sıfırlanıyor; sayaç durumları korunuyor. |
| P3-D2 — Yeni çalıştırmada nesil | KAPANDI | A2-K80/K85/K96; A1-K128 | Yeni çalıştırma artık nesli artırıyor. Yeni kapanış bileşiminin sorunu aşağıda #2. |
| P3-D3 — Okuma sürerken eşlemenin kaldırılması | KAPANDI | A8-K43/K71/K75/K77/K82 | Bütün kaldırma yollarında önce okumaların bitirilmesi zorunlu. |
| P3-D4 — Yerel taban derinliği | KAPANDI | A5-K36; A7-K56; `assets/manifest.schema.json` | Yerel uzunluk ve dünya derinliği ayrılmış; statik poz uygulanıyor. |
| P3-D5 — Açık kademede kamera bulunmaması | KAPANDI | A5-K80/K114; `scenarios/sema/a5.schema.json` kamera çözüm dalı; A2 §3.3; A8-K35 | Tam bir etkin kamera ve eksiksiz yapılandırma yükleme koşulu olmuş. |
| P3-D6 — Kuaterniyon kabul kümeleri | KAPANDI | A5-K32/K114; A2 §3.6; A8-K36 | Kamera istisnası aynı norm karesi sınırını kullanıyor; A5 normalleştirmiyor. |
| P3-D7 — Karşılıklı atıflar ve f64 açıklaması | KISMEN | A2 §3.6, §6, “A8’den beklenen”; A8 §5/§8.2 | Eski atıflar düzelmiş; fakat yeni bit eşitliği ifadesi montaj kaynaklarını karıştırıyor: #1. |
| P3-D10 — Ayrıntılı sayaçların yayımlanması | KAPANDI | W1-uyum D-16; A8-K30; A3/A6 `algilama/sayaclar` | Mimarın bilinçli daraltması uygulanmış: ayrıntılı sayaçlar sim kabul kapısında; bileşen iç sayaçları dışarı açılmıyor. |
| P3 — “Kapsam sınırı” notu | KAPANDI | A8 §6.2 | “Konumu sürdürme” iddiası çıkarılmış; ifade kamera algılama kayıtlarıyla sınırlı. |
| P4-Y1 — PARAM_SAVE kodlama sırası | KAPANDI | A6-K52; A4 §3.14; `port.h` | `gen + len + blob`; uzunluk bir kez yazılıyor. Örnek 40 B. |
| P4-Y2 — Tanımsız seçicili girdiler | KAPANDI | A6-K52; A4-K30/§3.14 | Başlık korunuyor, okunmayan gövde yazılmıyor; tekrar gövdeyi sıfırlıyor. A6’daki eski beklenti notu #5. |
| P4-Y3 — Arka plan sayımı | KAPANDI | A6-K55; A4-K109/K117/§3.11 | BAD_INPUT yürütülüyor fakat sayılmıyor; kurulmamış örneğin IDLE dönüşü platform sayımına giriyor. |
| P4-Y4 — A5 olay kimlikleri | KAPANDI | A3 olay şemalarının dört ilgili kimlik alanı; A5 §3.10 | `^[A-Za-z][A-Za-z0-9_]{0,31}$` eşleşiyor; `Freeze_1` taşınabiliyor. |
| P4-Y5 — Sekizden büyük çelişkiler | KISMEN | A3-K125; A6-K61; `olay_atama_celiskisi` | Kapasite ve birleştirme bağı tamam; olay sıra numarası kuralı iki biçimde okunuyor: #3. |
| P4-Y6 — Zamanlama olay nedenleri | KAPANDI | A3-K71, Tablo 4.7b/4.8; `olay_gercek_zaman_kacirildi`; A1-K79 | On iki neden mevcut; `hil_duraklatma` adı eşitlenmiş. |
| P4-Y7 — Kare sürümleri | KAPANDI | A6 `yuk_kamera_kare`; A2 §3.8; `a2_wire.h` | İki parçalı A2 sürümü ile kalibrasyon, dalga ve ortam sürümleri eklenmiş; tipler eşleşiyor. |
| P4-Y8 — A6’nın kademe payı | KAPANDI | W1-uyum D-12; A6-K28/§6; A1-K43; kanal kataloğu | TF zamanları her kademede ölçülüyor; yalnız telemetri/tamda kaydediliyor. Yeni HTTP açıklaması #4’te tutarsız. |
| P4-Y10 — Kart geri okuması | KAPANDI | A3-K97; A4 §3.16/K126; ortak yükler | Gönderilen SAFETY yerine kartın bildirdiği engel maskesi kullanılıyor; bitler doğru. |
| P4-Y11 — Çok oturum sınaması | KAPANDI | A6 §7, K27/K57 sınamaları; A3-K67/K108 | Tek dosyada kopma/yeniden bağlanma ile iki dosyalı sıfırlama ayrılmış. |
| P4-Y12 — `sil_n_bg` şeması | KAPANDI | A6 açılış şeması; A6-K54 | `kip=sil` koşulunda zorunlu; onluk u32 deseni sınırları karşılıyor. |
| P4-Y13 — `kare_seq` atfı | KAPANDI | A3/A6 ortak algılama yükleri; A8-K16/K62 | İlgili açıklamalar `kare_sirasi` olarak düzeltilmiş. |
| P1 bulgu 5 — Emniyet olayında tüp koşulu | KAPANDI | A3-K89; `olay_emniyet_girdisi_degisti.allOf`; `girdi_secimi` | Fiş/pimde tüp zorunlu; acil durdurmada yasak. A6 aynı olay tanımını kullanıyor. |

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `docs/sozlesmeler/A2-gorsellestirici.md` §3.6, “A8’den beklenen” md.14 ↔ A5-K114 ↔ A8-K53 | Yeni ifade A8’in **montaj alanlarını A2’deki bitleriyle aynen taşıdığını** söylüyor. A2’nin montajı fizik gerçeği, A8’inki nominaldir. Geçerli örnek: fizik montaj konumu `(0,20;0;0)`, nominal montaj `(0,21;0;0)`. Yeni A2 cümlesi birinciyi kopyalatırken A5/A8 ikinciyi zorunlu kılar. Yanlış okuma montaj hatasını ortadan kaldırır ve A8’e fizik gerçeği verir. | Bit eşitliğini her bağlantının **kendi A5 kaynağına** bağlayın: fizik montajı A2’ye, nominal montaj A8’e. Ortak iç parametre/bozulma alanlarının kayıpsız aktarımını ayrıca belirtin. | kritik |
| 2 | A2-K96 adım 1 ve 3 ↔ A2-K81/K64/K49 ↔ A6-K18/K27 | Yeni çalıştırmada K81 yanıtlanmamış isteği `calisma_nesli_degisti` nedeniyle düşürür, fakat yuvasını yanıt gelene kadar görselleştiricide tutar. Ardından K96, K64’ü uygulatır; K64 **yanıtlanmamış bütün istekleri** yeniden `gorsellestirici_yok` nedeniyle düşürür. Tek istek için iki neden/kayıt/sayaç artışı üreten okuma mümkündür. 100 periyotta tek düşmenin iki sayılması, %1 sınırında geçerlilik sonucunu değiştirebilir. | K96/K64 bileşiminde her `kare_sirasi` için düşme kararı ve sayımının tek olduğunu yazın. K81’de sonuçlandırılan isteğin K64’te yeniden düşürülmemesini; yuva kapatma ile düşme kaydını ayırın. | kritik |
| 3 | A3-K125 ↔ A6-K61 | Aynı cümle parçaların “**ardışık olay sıra numaralarıyla**” yayımlandığını ve “**arada başka bir olay bulunabileceğini**” söylüyor. Parçalar 100 ve 102, aradaki başka olay 101 ise ikinci hüküm sağlanır, birinci sağlanmaz. | Araya olay girebiliyorsa parçaların sıra numaralarını “artan” olarak tanımlayın; birleştirmeyi `celiski_kimligi/parca_no` ile yapın. | önemli |
| 4 | A6 §6, “`olaylar` kademesinin sınırları” ↔ A3-K68 | Yeni A6 paragrafı gerekli kanal bulunmamasını **409 `tekrar_kosulu_saglanmiyor`** ile reddettiriyor. A3-K68 aynı kademe/kanal eksikliği için **422 `gecersiz_deger` + `ayrinti.gereken`** diyor. Örneğin olaylar kaydından fizik tekrarı iki farklı yanıt alıyor. | A6 açıklamasını A3-K68’e eşitleyin. 409’u sürüm/platform/özet koşulu uyuşmazlığında bırakın. | önemli |
| 5 | A6 §10, “A4’ten beklenen” md.6 ↔ güncel A4-K30 | Bu turda eklenen beklenti, A4-K30’un yalnız “reddedilir” dediğini ve gövdenin okunmaması cümlesinin eklenmesi gerektiğini söylüyor. Güncel K30 istenen cümleyi zaten içeriyor. | Beklentiyi karşılandı olarak işaretleyin; mevcut eksiklik iddiasını kaldırın. | küçük |

## Yeniden hesaplananlar

**Port kodlaması**

| Denetim | Bağımsız sonuç |
|---|---|
| GNSS örneği | Başlık 28 B + GNSS gövdesi 34 B = **62 B**. Önceki örneğin bütün baytları eşleşiyor. |
| FC komutu | `cmd_id/type/source` 6 B + FC gövdesi 10 B = **16 B**. `09000000 10 fa 05 01 00000000 00000000`. |
| PARAM_SAVE sonucu | Sonuç başlığı 8 B + `gen` 4 B + `len` 4 B + blob 24 B = **40 B**; sonda ikinci uzunluk yok. |
| Tanımsız örnek türü | `hdr.type=99` için **28 B başlık**, sıfır gövde baytı. A4-K30/§3.14 gövdeyi okumadığı için sıfırlanmış gövdeyle aynı ret davranışı kurulabilir. |
| PARAM_GET / PARAM_SET | GET komutu 6+2 = **8 B**; SET komutu 6+2+1+4 = **13 B**. Değer ham 32 bittir; tür 7 gibi reddedilecek değerler dosyayı bozmaz. |
| A4-K130 | Taşınan SAFETY örneğinin `VALID=0` biti korunur; gövdesi tanımlı SAFETY üyesidir ve kaydedilir. Ret, tazelik/sıra izini ilerletmez; kısıtlayıcı içerik ayrıca uygulanır. |
| `out=NULL` | A6-K54 boş `cikti` ile aynı NULL çağrısını kurduruyor. Bekletilen kapanma/günlük sonraki yazılabilir çağrının çıktısıyla karşılaştırılıyor. |

**Arka plan yürütümü**

| Dizi | Sayım ve sonuç |
|---|---|
| Kurulu örnek: `BAD_INPUT, MORE, step(bg_slices=1)` | İki background çağrısı da yürütülür; yalnız MORE sayılır: **1**, tutarlı. |
| Kurulu örnek: `IDLE, MORE, step(bg_slices=2)` | **2**, tutarlı. |
| Kurulmamış örnek: background, ardından step | Background **IDLE** döner; platform **1** sayar. Step NOT_INIT döner; çekirdek kurulmamış bellekte sayaç güncellemez/karşılaştırmaz. |
| Sonraki init | Yeni kuşağın sayımı sıfırdan başlar; önceki kurulmamış örneğin IDLE çağrısı yeni kuşağa taşınmaz. |
| `out=NULL` step/tick | Platform sayımı o çağrıdan sonra yeniden başlar. Çekirdek karşılaştırmadan sıfırlar; A6 kayıt tutarlılığını denetler. |

**Parçalama**

Bütün parçalarda aynı çelişki kimliği, zaman, alan ve uygulanan değer bulunur; öğeler dosya sırasını korur.

| Öğe | Parça sayısı | Parçalardaki öğeler | `parca_no` | `son_parca` |
|---:|---:|---|---|---|
| 9 | 2 | 8 + 1 | 1, 2 | false, true |
| 16 | 2 | 8 + 8 | 1, 2 | false, true |
| 17 | 3 | 8 + 8 + 1 | 1, 2, 3 | false, false, true |
| 4096 | 512 | 512 × 8 | 1…512 | Yalnız 512’de true |

Değişen olay ayrıntılarının bağımsız üretilen üst sınırları:

| Olay | Bayt |
|---|---:|
| `gercek_zaman_kacirildi` | 112 |
| `surec_durumu` | 1150 |
| `senaryo_ogesi_uygulandi` | 185 |
| `atama_celiskisi` | 2906 |
| `gevsetme_reddedildi` | 554 |
| `donma_onceki_cikti_yok` | 170 |

Hepsi Tablo 3.5b ile aynı ve **8192 B ayrıntı sınırının altında**. Çelişki parçası, 22 B olay yükü sabiti ve 24 B A3 başlığıyla **2952 B** olur. Tablodaki en büyük olay ayrıntısı 4966 B; aynı eklerle **5012 B**.

**Kart geri okuması ve olay şemaları**

- `ESTOP`: bit 4 doğrudan; fişler: bit 5–6’nın tersi; pimler: bit 7–8 doğrudan. A3-K97 ile A4 §3.16 eşleşiyor.
- `0x00026FFF` içinde bitler **0–11, 13, 14, 17** kurulu; **12, 15, 16** kapalı. Kurulmamış çekirdek A3’te acil durdurma basılı, fişler çıkık, pimler takılı görünür.
- On iki zamanlama nedeni Tablo 4.8’deki 0–11 sırasıyla ve A1-K79’la eşleşiyor. Aynı adımda birden çok neden için en küçük bit seçimi tanımlı.
- Fiş/pim olayının tüpsüz biçimi reddediliyor; `tup=f1` kabul ediliyor. Acil durdurmada bunun tersi geçerli.
- `a2_veda_nedeni` kapalı listesi `a2_wire.h` değerleri **0–5** ile aynı; `yeni_calistirma=5`.
- Üç komut bekleme noktası A3-K101, A1-K129 ve A1-K63’te mevcut. Sim kuyruğunda düşen komutun `komut_bayat_yetki` taşıyıcısı iki katalogda da var.

**Nesil, oturum ve halka yürütümü**

| Durum | Yürütüm |
|---|---|
| Yeni çalıştırma | Nesil artar; sim zaman kabul izi sıfırlanır; eski sonuçlar reddedilir; RUN_EPOCH gönderilir. A1 eski oturumu kapatıp yeni el sıkışma yapar. A2-K96 eski A2 oturumunu kapatır; yeni oturum yeni halka ve tam yapılandırmayla kurulur. Yeni kayıt dosyası açılır; eski oturumun gecikmiş kapanışı A6-K27’ye göre yeni dosyanın başında bulunabilir. Düşme sayımındaki çakışma #2’dir. |
| Sıfırlama | Nesil artar; A1 oturumu yenilenir. A2 oturumu ve halkası sırf nesil değiştiği için yenilenmez. Eski tüketici sonucu reddedilir, bırakması kabul edilir. Mevcut kayıt kapanır; sonraki başlatma yeni dosyadır. |
| Atlama | Nesil artar; A1 oturumu yenilenir; A2 oturumu korunur. Eski sonuçlar reddedilir; yeni neslin zamanı eski zamanla karşılaştırılmaz. A2’nin atlama bloğu yeni durumu taşır. |

Önceki dört halka yürütümü de tekrar değerlendirildi:

| Yürütüm | Sonuç |
|---|---|
| Bileşen okurken kopma | Sim başvuruyu korur, etkin halkayı emekliye ayırır. K82 bileşenin kendi eşlemesini okuma bitmeden kaldırmasını yasaklar. |
| 1 s bırakma zaman aşımı | Yuva geri alınmaz; yeni halka teklif edilir. Eski okuma/bırakma bitmeden başarılı ACK yoktur. |
| Yenileme sırasında eski kare okunması | Eski ve yeni halka ayrı nesnelerdir; K71/K82 okuma bitişini, K88 sim’in bırakma koşulunu korur. |
| Sıfırlama sırasında yoldaki sonuç | Eski nesil sonucu karta gitmez; bırakma işlenir. Eski son zaman 10 s iken yeni neslin 1 s partisi zaman ilerleme denetimine takılmaz. |

A2’nin “A8’den beklenen” 16 maddesinin karşılıkları sırasıyla **K38; K72; K73/K48; K48; K74/K45; K45/K50/K82; K70/K71; K77; K75/K70; K78/K79/K17; K80/K16; K76/K63; K10–K12; K36/K52/K53; K35/K68; RING_OFFER/K70** içinde bulundu. Madde 14’ün montaj açıklaması #1 nedeniyle düzeltilmelidir.

**Senaryo, kare düzeni ve dosyalar**

- Taban örneği: `t=3`, statik D=1 → **D_taban=4 m**. Derinlik 3 ret, 4 kabul. `x=y=0` yönelim koşulunda yerel +z, NED +D olarak kalır.
- D6 kuaterniyonu için `norm²−1 ≈ 7,9999999998×10⁻⁶`, `norm−1 ≈ 3,999992×10⁻⁶`: kamera kabulü, diğer kuaterniyonlarda ret.
- Halka başlığı **128 B**, yuva başlığı **256 B**. Yeni halka tablosunun ofsetleri başlıkla aynı. Kare kaydındaki kalibrasyon `u16` ve dalga/ortam `u32` alanları, yuva başlığındaki sırasıyla **44, 48, 52** ofsetli kaynaklarla eşleşiyor.
- `sil_n_bg`: `0` ve `4294967295` kabul; `01` ve `4294967296` ret. SIL koşulundaki zorunluluk mevcut.
- Altın dosya: **10.527 B**, SHA-256 `d1735f8b7eafabede9350c591bb4db7aa3a563b8ac4f45500200d33d4427cd92`; A5-K17 ile aynı.
- Örnek manifest: **6.030 B**, SHA-256 `29063c2d7f89a80e452dfbb66504e9470d5c928d0fcf7dd295497e2b22a22ca4`.
- İki kanal dosyası geçerli UTF-8/JSON; yinelenen anahtar bulunmadı. A3’ün **83 yerel `$ref`**’i çözülüyor. Ortak **24 yük** derin eşit; ortak **25 kanalın** yük/sürüm/zaman anlamı eşit.
- Standart JSON Schema doğrulayıcısı kurulu olmadığından tam 2020-12 üst şema doğrulaması yapılmadı; ilgili koşullar tanım ve sınır örnekleri üzerinden denetlendi.

**Tel biçiminin korunması**

`protocol/ida.xml` açıklamalar çıkarılarak tabanla karşılaştırıldı: mesaj, alan, enum adı ve değerleri değişmemiş. Yeniden hesap:

`37 M + 334 F + 194 E = 565 satır` → **`dialect_hash=0x3939788984A5FF80`**.

A1 Ek B değişmemiş. `port.h` ve `algilama_kaydi.h` yorumlar çıkarıldığında tabanla aynı; `a2_wire.h` farkı yalnız `VedaNedeni::yeni_calistirma=5`.

## İskelete ya da diğer bölümlere notlar

İskelet 0.11’in üçüncü bekleme noktası A3/A1’de karşılanmış. Taşınan emniyet örneğinin gevşetememesi A4-K130 ve A6 kaydında aynı biçimde uygulanmış. D-16’nın sayaç daraltması mimar kararı olarak değerlendirilmiştir; yeniden tartışılmamıştır.

A8 §6.2 kapsam düzeltmesi uygun. Karşılık denetiminde açılan A4 §3.17; ret kodları, emniyet engelleri ve durum uygunluğu üzerinden arayüz davranışını tanımlıyor. İncelenen değişikliklerde hedef seçimi, hedefleme çözümü, atış ölçütü veya bunları taşıyan yeni opak alan saptanmadı.

Canlı birincil kaynaklardan MAVLink alan sıralaması/CRC_EXTRA hesabı [MAVLink serileştirme belgesinden](https://mavlink.io/en/guide/serialization.html), kanonik JSON kuralları [RFC 8785’ten](https://www.rfc-editor.org/rfc/rfc8785.html), şema koşulları [JSON Schema belirtiminden](https://json-schema.org/draft/2020-12/json-schema-validation) denetlendi. Eşleme kaldırmanın adresleri geçersizleştirmesi [Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-unmapviewoffile) ve [Apple](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/munmap.2.html) belgeleriyle doğrulandı. Open Group `munmap` sayfası 403 verdi; o kaynak doğrulanmış sayılmadı.

Bunlar belge, hesap ve şema denetimleridir. Gerçek halka görünürlüğü, kart davranışı ve Windows zamanlaması ölçülmüş sayılmamıştır.

dondurulamaz