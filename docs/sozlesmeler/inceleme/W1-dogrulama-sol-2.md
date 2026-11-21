# sol incelemesi: sozlesmeler

## Hüküm

**Dondurulamaz.** Uyum turu önceki sorunların çoğunu gidermiş; ancak hata yanıtının olaylarla eşlenmesi, emniyet örneği kapasitesinin yeni hesabı ve birliklerin kayıt seçicileri kritik sorun içeriyor. Yeni sonuç bekleme yasağıyla mevcut izin yenileme garantisi de birlikte sağlanamıyor.

İnceleme A1–A4 ve A4–A8 kapsamındadır. Aşağıdaki kısaltmalar sırasıyla `docs/sozlesmeler/A1-kart-baglantisi.md`, `docs/sozlesmeler/A4-port.md` ve `docs/sozlesmeler/A8-kamera-algilama.md` belgelerini gösterir. Öncelik kullanıcı kararları > iskelet 0.10 > `docs/gorevler/W1-uyum.md` olarak uygulandı. Hiçbir dosya değiştirilmedi, kurulum yapılmadı.

## Düzeltilmesi gerekenler

**Uyum turunun bıraktığı veya ürettiği çelişkiler**

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| Y1 | A1 §3.7 `ACTUATOR.batch_seq`, K45/K73 ↔ A4 K57/K116, §4.14 | Hata yanıtının kimliği düzeltildi, fakat olay sayısıyla bağı düzeltilmedi. Son kabul edilen TF=10 iken TF=11 çağrısı `BAD_INPUT` dönsün ve aynı çağrıdaki kısıtlayıcı emniyet örneği `SAFETY_CLOSE` üretsin. Yanıt `batch_seq=11,n_events=1`; olay A4 gereği `seq=10,t_decide=TF10.t` taşır. K73, yuva 11 için aynı numaralı bir olay beklediğinden yuva tamamlanamaz. | A1’in `n_events` ve yuva tamamlama kurallarında bu hata yolunu çöz. Kapatma olayının A4’teki kendi `seq/t_decide` anlamını koruyarak, yeni yanıt yuvasının başka partiye bağlı olayı beklemesini önle. | kritik |
| Y2 | A1 E20/K124 ↔ A1 K05, `protocol/ida.xml` `IDA_SAFETY`; A4 K102 | E20, emniyet taşıyan pencerenin **en az 63 B** olduğunu varsayıyor. Geçerli `flags=1,inst=0,estop=0,tube=[0,0]` örneğinde `SAFETY` yükü 17 B’ye kırpılıyor: çerçeve **29 B**, TF ile pencere **58 B**. Dolayısıyla 0,21 ms ve buradan çıkarılan 3,15 ms kapasite sınırı geçerli değil. | E20’yi korunan çerçevelerin gerçek en küçük tel boylarıyla yeniden hesapla. Bu geçerli örnekle bile aynı hesap yöntemi `15×58/300=2,90 ms` verir. K124’ün kapasite aşımında oturum kapatma yolu korunmalı. | kritik |
| Y3 | A4 §3.14/K121; `core/include/ida/port.h` `ida_cmd_t.u.param` ↔ `docs/sozlesmeler/A6-kayit.md` K52 | `PARAM_GET` yalnız `param.id` kullanıyor; başlık yorumu da bunu söylüyor. Yeni seçici tablosu ise `PARAM_GET` için bütün `param` yapısını etkin sayıyor; içteki `v` birliğini `param.type` seçiyor. Sıfırdan hazırlanmış geçerli bir GET girdisinde kullanılmayan `type=0` olur. A6-K52 tanımsız seçiciyi bozuk kayıt saydığından sıradan GET girdisi güvenceli tekrara uygun kodlanamıyor. | A4’te GET’in yalnız `id`, SET’in `id/type/v` taşıdığını koşullu alan tablosunda açıkça ayır; A6 kodlamasını bu tanıma bağla. Kullanılmayan GET değer türünü okumayı gerektirme. | kritik |
| Y4 | A4 §3.5 ↔ A4 K67/K106; A1 K80/K120 | Sonuç tablosu eski anlamları koruyor: `DROPPED_NO_SLOT` yalnız yer yokluğu; `EXPIRED_NO_ACK` son geçerlilikte yinelemenin bırakılması olarak açıklanmış. Güncel kurallar ilkine kova boşluğunu ekliyor, ikincisini **95 ms daha sonra** üretiyor. | §3.5’i iki neden ve `apply_by+95 ms` zamanıyla güncelle. Yinelemenin durmasıyla sonucun üretilmesini ayır. | önemli |
| Y5 | A1 §3.7 `valid_for_ns`, §3.9 dayanak sütunu ↔ A4 K58/K66, `port.h` süre sabitleri | A1’in alan tablosunda hâlâ `valid_for=1 ms–1 s`; §3.9’da A4’ün `apply_by` aralığı hâlâ `1 ms–1 s` diye anılıyor. Güncel ortak aralıklar sırasıyla **60–450 ms** ve **16–1000 ms**. | A1’deki eski aralıkları kaldır; geçerli sayısal kaynağı §3.9/K104 olarak tekleştir. | önemli |
| Y6 | A1 K95, §3.7 `CALL_TRACE.call_kind=4` ↔ A1 K11/K22/K107; A4 K122 | Yeni oturum sonu çağrısının izini gönderme yolu tutarsız: K22 önce `IDLE`’a geçiyor, K11 burada yalnız `BOARD_HELLO` gönderilmesine izin veriyor; K95 ise oturum sonu çağrısı için de tam bir `CALL_TRACE` gönderilmesini istiyor. `SESSION_END=4` adı mevcut, fakat bu izin hangi durumda gönderilebileceği çözülmemiş. | A1’de oturum sonu izinin gönderim veya gönderilememe kuralını açıkça tanımla; K11/K22/K95’i ve eksik iz sayımını buna göre eşitle. | önemli |
| Y7 | A1 §3.7 ve Ek A, §8.2’ye atıflar; A4 §7 K44/K45 sınaması ↔ A4 K119, güncel `port.h` | A1, kaldırılmış `fc_ref` ve `ext_len/ext[]` alanlarını bazı yerlerde hâlâ mevcut A4 alanları gibi anlatıyor; Ek A’daki “§8.2 A4 md.5” artık kaldırma maddesine değil oturum sonu çağrısına gidiyor. A4’ün uygunluk sınaması da genişletici listesinde hâlâ `FC_SELECT` içeriyor. | Güncel alan eşlemesini kaldırılmış alanlardan temizle; tarihsel açıklamaları açıkça tarihsel tut. `FC_SELECT`’i kabul edilebilir işlem olarak kullanan sınama satırını kaldır ve atıfları güncelle. | önemli |
| Y8 | `docs/gorevler/W1-uyum.md`, A8 “+ P2-15” ↔ A8 K30 | Görev ayrıntılı sayaçların bileşende ve sim kabul kapısında tutulup A3/A6 üzerinden yayımlanmasını istiyor. A8-K30 ise bileşenin kendi sayaçlarının dışa açılmadığını söylüyor. Çekirdekte tek toplam sayaç kararı karşılanmış; görevin bileşen tarafı karşılanmamış. | Ayrıntılı sayaç yükümlülüğünü görevle eşitle veya bu daraltmayı mimarın açık kararı olarak kaydet. Çekirdeğin 22 alanını genişletmek gerekmiyor. | önemli |

## Yeniden hesaplananlar

**Önceki çapraz incelemenin kapanışı**

Kaynak: `docs/sozlesmeler/inceleme/W1-capraz-sol-2.md`. Kapanışlar yazarların bildiriminden değil güncel metinden verildi.

| Bulgu | Hüküm | Bakılan yer ve kalan eksik |
|---|---|---|
| P2-1 — kapalı listeler ve opak alanlar | KISMEN | A4 K51/K90/K91/K119, §3.15–3.16 ve `port.h`: gerçek tür ve alanlar kaldırılmış, argümanlar sıfırla sınırlı. Güncel eşleme ve sınama metninde Y7 kalıyor. |
| P2-2 — `SET_MODE` | KAPANDI | A1 §3.6/K64; A4 §3.4/K46: hedef 2–7, daraltıcı hedef 2–5; 0/1 dönüştürücüde `LINK_BAD_FIELD`. |
| P2-3 — daraltıcı FC tüpü ve kaynak | KAPANDI | A1 §3.6/K64; A4 K46: DISARM/ABORT bütün tüpler, telde `tube=255`; mandalda `source` olmamasının bilgi amaçlı olduğu açıklanmış. |
| P2-4 — hata yanıtının parti kimliği | KISMEN | A1 §3.7/K45 ve XML `ACTUATOR.batch_seq` artık `in.tf.seq`. Fakat olayların eski parti bağını `n_events` ile birleştiren Y1 kalıyor. |
| P2-5 — arka plan takvimi | KAPANDI | A1 K121 ↔ A4 K95/K109: step ve tick öncesi tam `N_bg`, oturum sonunda sıfır; tekrarda yalnız kayıtlı çağrılar. |
| P2-6 — tick takvimi | KAPANDI | A4 K18/K19/K23, A1 K117’ye bağlı; koşulsuz ızgara zorunluluğu kaldırılmış. |
| P2-7 — SIL oturum sonu çağrısı | KISMEN | A4 K11/K122 ve XML `SESSION_END=4` mevcut; çağrı zamanı uyumlu. İz gönderim yolu Y6’da açık. |
| P2-8 — sonuç zamanı | KISMEN | A1 K80 ve A4 K106 `apply_by+95 ms` diyor. A4 §3.5’in eski açıklaması Y4’te kalmış. |
| P2-9 — olay tablosu ve kova | KISMEN | A1 K80/K120 ve A4 K67 yedi+bir yuva ve taşmada oturum kapanışında uyumlu. Sonuç tablosu Y4’te, yenileme ilişkisi M1’de tutarsız. |
| P2-10 — süre aralıkları | KISMEN | A4 K58/K66 ve başlık sabitleri doğru; A1’in eski alan açıklamaları Y5’te. |
| P2-11 — IMU bayatlığı | KAPANDI | A4 K39, `IDA_IMU_STALE_*`; A1 E10: üst sınır 30 ms ile hesaplanmış. |
| P2-12 — kamera kapalı başlığı | KAPANDI | A4 §6/K118 ↔ A8 K64: kayıt yok, 1 Hz durum başlığı var; geldiği çağrıda `n_det_batch>0`. |
| P2-13 — canlı SIL eşitliği | KAPANDI | A1 K102/K121; A4 §7; iskelet §4: canlı eşitlik kamera kapalı koşula bağlanmış, HIL bit eşitliği kaldırılmış. |
| P2-14 — parti atomikliği | KAPANDI | A4 K41 ↔ A8 K27/K29: tek bozuk kayıt bütün partiyi reddettiriyor. |
| P2-15 — çekirdek ret sayaçları | KAPANDI | A8 K30/§8.2 md.3, A4 §10: çekirdekte tek toplam `det_rejected` yeterli kabul edilmiş. Görevin ek bileşen yükümlülüğü Y8’de ayrıca açık. |
| P2-16 — algılama bayatlığı | KAPANDI | A4 K118, §3.16 `DET_STALE`, `IDA_DET_STALE_DEFAULT_NS=2 s`. |
| P2-17 — zaman üst sınırı | KAPANDI | A8 K81/K27 TIME satırı ve `IDA_DET_TIME_MAX_NS`: 0…2⁶²; fark hesabından önce aralık denetimi. |
| P2-18 — enum çakışması | KAPANDI | XML’de bütün enum ve girdi adları `IDAW_`; `port.h` adlarıyla kesişim boş. |
| P2-19 — neden aralıkları | KAPANDI | A4 K120, §3.8 ve `IDA_REASON_*` sınırları ↔ A1 §3.7: çekirdek 0–15, bağlantı 128–191. |
| P2-20 — alım damgası | KAPANDI | A1 K48/§3.1 ↔ A4 K14/§4.13/§10 md.14: aynı damgalama kuralı ve sayısal sapma sınırı. Donanımda ölçülmüş sayılmadı. |
| P2-21 — günlük kanalı | KAPANDI | A4 §3.15/K90: 23 kapalı kod ve kod başına argüman anlamı; A1 §3.7 bu tabloya bağlı. |
| İ1 — revizyonun kapalı artış listesi | KAPANDI | İskelet 0.10 §3.2 ↔ A1 K106/K89: altı işlem, başlangıç 1, doyma ve artırmayanlar uyumlu. |
| İ2 — atış kontrol kapsamı | KAPANDI | İskelet §2/§5/§9 güncel dar sınıra çekilmiş; gerçek A1/A4 türlerinde hedef seçimi ve opak FC yükü yok. Y7’de eski sınama adı kalmış. |

**İstenen U maddeleri**

| Madde | Hüküm | Bakılan yer ve kalan eksik |
|---|---|---|
| U7 | KISMEN | A1 §3.9/K104, A4 K39/K58/K66 ve `port.h` sabitleri uyumlu; Y5’teki eski aralıklar temizlenmemiş. |
| U9 | KAPANDI | A1 K102/K121, A4 §7; A6 K34/K56: kamera koşulu ve SIL port tekrarı sınırı aynı. |
| U10 | KAPANDI | A1 K121, A4 K95/K109; A6 K54/K55 ve üst veri `sil_n_bg`: kayıt dışı çağrı eklenmiyor. |
| U11 | KISMEN | A1 K117/K107, A4 K18/K122 ve XML çağrı türleri uyumlu; oturum sonu izinin taşınması Y6’da açık. |
| U12 | KISMEN | A1 K80/K120, A4 K67/K106/K123: temel kapı aynı. Y4’te sonuç açıklamaları, M1’de yenileme garantisi tutarsız. |
| U15 | KAPANDI | XML adları, kanonik özet ve altın çerçeveler doğrulandı; ad kesişimi yok. |
| U19 | KAPANDI | A1 K128, A4 K122, A8 K17: nesil değişimi yeni A1 oturumu; algılamanın zaman ilerleme durumu sıfırlanıyor, algılama sayaç kabul durumu korunuyor. |

**Sözleşme başlıklarındaki “+” maddeleri**

| Madde | Hüküm | Bakılan yer ve kalan eksik |
|---|---|---|
| A1 + P2-2 | KAPANDI | §3.6/K64: `SET_MODE=2…7`. |
| A1 + P2-3 | KAPANDI | §3.6/K64: daraltıcı FC’de `tube=255`; kaynak bilgisinin mandalda taşınmaması açıklanmış. |
| A1 + P2-4 | KISMEN | §3.7/K45/XML kimlik kaynağı doğru; Y1. |
| A1 + P2-11 | KAPANDI | E10: `30+14,5<60`. |
| A1 + P2-20 | KAPANDI | K48/§3.1: erken 0,86 ms, geç 0,1 ms; kesme gecikmesi yükümlülüğü ölçüme bırakılmış. |
| A1 + P4-18 | KAPANDI | K127: yalnız PC’de yeniden kurma girişi, oturum değerleri kayıtlı el sıkışmadan; eşitlik güvencesi yok. |
| A1 + U1 tablo atıfları | KISMEN | §3.7’de günlük, neden ve engel tabloları doğru; Ek A’daki eski alan ve madde atıfları Y7’de. |
| A1 + §8.2 beklenti listesi | KISMEN | Liste yenilenmiş; aşağıdaki karşı denetimde Y4–Y7 ile ilişkili eksikler var. |
| A4 + P2-12 ve §6 | KAPANDI | Kamera kapalı durum başlığı korunmuş; Ping360 üst sınırı iki kademede de 1200. |
| A4 + P2-14 | KAPANDI | K41: parti atomik. |
| A4 + P2-16 | KAPANDI | K118: varsayılan 2 s ve `DET_STALE` biti. |
| A4 + P2-20 | KAPANDI | K14/§4.13: damga kaynağı, sapma ve 34,5 ms’nin başlangıcı açık. |
| A4 + P3-14 | KAPANDI | O9/§10 A5 md.3: parametre kataloğu W3’te; fizikten türetim beklentisi yok. |
| A4 + P4-1 | KISMEN | §3.14/K121 ve başlık seçicileri eklenmiş; GET’in iç birlik seçicisi Y3’te çözümsüz. |
 XT| A8 + P2-15 | KISMEN | K30 çekirdek beklentisini daraltıyor; bileşen sayaçlarının yayımlanması Y8’de karşılanmıyor. |
| A8 + P2-17 | KAPANDI | K81 ve başlık sabiti: 0…2⁶², taşmasız fark. |
| A8 + P3-16 | KAPANDI | K23 ↔ A5 K113 ve sınıf tablosu: 0–3 aynı anlamlarla, 4–255 ayrılmış. |
| A8 + A4 beklentileri | KAPANDI | Atomiklik K41; toplam sayaç `status.cnt.det_rejected`; bayatlık K118; oturum sonu zaman sıfırlaması K122. |

**A1 §8.2 “A4’ten beklenenler” karşı denetimi**

| Madde | Hüküm | Karşı belgede açılan yer / eksik |
|---|---|---|
| 1 — neden aralıkları | KAPANDI | A4 K120/§3.8. |
| 2 — süresi geçmiş kapatma | KAPANDI | A4 K66, `SAFETY_CLOSE` istisnası olmadığını bilinçli seçim olarak söylüyor. |
| 3 — süre aralıkları | KISMEN | A4 K39/K58/K66 ve §10 md.5 doğru; A1’in kendi eski açıklaması Y5. |
| 4 — kapalı listeler | KISMEN | A4 K51/K90/K91/K119 ve §3.15–3.16 doğru; uygunluk satırındaki `SELECT` Y7. |
| 5 — oturum sonu çağrısı | KISMEN | A4 K122 doğru; A1’de iz gönderimi Y6. |
| 6 — olay tablosu ve sonuç zamanı | KISMEN | A4 K67/K106/K123 mevcut; §3.5 Y4, yenileme ilişkisi M1. |
| 7 — tick takvimi | KAPANDI | A4 K18 doğrudan A1-K117’ye bağlı. |
| 8 — sabit arka plan sayısı | KAPANDI | A4 K95. |
| 9 — platformdaki revizyon damgası | KAPANDI | A4 olay yapısında ayrı revizyon yok; damgalama A1-K106’da platforma ait. |
| 10 — emniyet örneklerinin korunması | KAPANDI | A4 K102 artık emniyet örneklerini önce yerleştiriyor ve A1-K124’e bağlıyor. A1 listesindeki “atıf beklenir” notu eskimiş. |
| 11 — alım damgası | KAPANDI | A4 K14/§4.13/§10 md.14. |
| 12 — sıra aritmetiği | KAPANDI | A4 §2/K25, iskelet §3.1 ile aynı. |
| 13 — parti geliş sınırı | KAPANDI | A4 §10 md.8, A1 E21/K40’a bağlı; olağan ve fiziksel geliş varsayımları ayrılmış. |

**A4 §10 “A1’den beklenenler” karşı denetimi**

| Madde | Hüküm | Karşı belgede açılan yer / eksik |
|---|---|---|
| 1 — alan eşlemesi ve TF damgası | KAPANDI | A1 §3.4–3.8, Ek A/K48; kaldırılmış alanlara ilişkin açıklama artıkları Y7’de. |
| 2 — tüp ve emniyet geri okuması | KAPANDI | A1 §3.4 `IDA_SAFETY`, K56/K89. |
| 3 — olay ve sonuç alanları | KAPANDI | A1 §3.7 ve XML 46061/46046. |
| 4 — alıcıdaki izin süresi | KAPANDI | A1 K86: uygulama anından sim zamanında `hold_ns`. |
| 5 — süre aralıkları | KISMEN | A1 §3.9/K104 doğru; §3.7’de Y5. |
| 6 — Ping360 kutu zamanı | KAPANDI | A1 K52. |
| 7 — dönüş koduna göre yanıt | KISMEN | A1 K31/K44/K45 doğru ayrımı yapıyor; hata yanıtı ve olay tamamlama bağı Y1’de açık. |
| 8 — parti geliş sınırı | KAPANDI | A1 E21/K40: belirtilen varsayımlar ve sayılar mevcut. |
| 9 — birleşen partilerin yanıtı | KAPANDI | A1 K40/K75; eski partiler ayrı yanıt almıyor, kaçırılmış sayılıyor. |
| 10 — komut dönüşümü ve kimlik | KAPANDI | A1 §3.6/K63–K68/K123. |
| 11 — platform sayaçları | KAPANDI | A1 Tablo 3.11b/K30/K31. |
| 12 — olay yuvasının ömrü | KISMEN | A1 K80/K120 doğru; karşı taraftaki açıklama Y4 ve M1 ilişkisi kalıyor. |
| 13 — bekleyen genişletici | KAPANDI | A1 K63: ertelenmiş genişleticilerin her birine `REJECTED/NARROWED`. |
| 14 — damga sapması | KAPANDI | A1 K48/§3.1; ölçüm henüz yok. |
| 15 — oturum sonu çağrı türü | KISMEN | XML `IDAW_CALL_KIND_SESSION_END=4` ve A1 §3.7/K95 mevcut; gönderim yolu Y6. |
| 16 — kısıtlayıcı örneğin korunması | KAPANDI | A1 K31/K124 ve H52 koruma yolunu tanımlıyor. Kapasite hesabındaki yanlış Y2’de ayrıca açık. |
| 17 — kapalı tablolar ve FC alanları | KAPANDI | A1 §3.6–3.7/K64; A4 §3.15–3.16/K120’ye atıflar doğru. |

**Süreler ve sayısal eşitsizlikler**

| Denetim | Bağımsız sonuç |
|---|---|
| Ortak süreler | `apply_by=16…1000 ms`, `hold=60…450 ms`, `valid_for=60…450 ms`; A4 kuralları ve `port.h` aynı. A1’in eski açıklamaları Y5. |
| Bayatlık | `T_imu_stale=5…30 ms`, varsayılan 20 ms; `T_det_stale` varsayılan 2 s. A4 ve başlık sabitleri eşit. |
| Bekleme ve sonuç payı | A1 `T_bekle=10…35 ms`, varsayılan 30 ms; platform sonuç payı 95 ms; çekirdek bilinmeyen sonuç eşiği 100 ms. A4 K106/K70 aynı ayrı süreleri kullanıyor. |
| E1 | `5<10`: sağlanıyor. |
| E2 | `35<60` ve `450<500`: sağlanıyor. |
| E3 | `60≥10+5+14,5=29,5`: sağlanıyor. |
| E4 | `4×100=400≤500`: sağlanıyor. |
| E5 | `500>14,5` ve `500>35`: sağlanıyor. |
| E6 | `16≥10+5=15`: sağlanıyor. |
| E7 | `15<16`: sağlanıyor. |
| E8 | `2×35+5+14,5+0,86=90,36≤95<100`: aritmetik sağlanıyor. |
| E9 | `60/2=30≥29,5`; `450<500`: aritmetik sağlanıyor; yenileme garantisinin yeni K123 ile ilişkisi M1’de sorunlu. |
| E10 | `30+14,5=44,5<60`: bütün izinli IMU eşikleri için sağlanıyor. Gerçek alımdan sayılan geç yön sınırı tahmini 44,6 ms. |
| E11 | TF başlangıçları en az 2,5 ms aralıklıysa yarı açık 5 ms aralıkta en çok iki TF: doğru. |
| E12 | `8≥6`: yazılı sonar hizmet varsayımı altında sağlanıyor. |
| E13 | `29+59+34+102+124=348`; `520−348=172≥89`; `520+399+258=1177≤1200`: doğru. |
| E14 | `260≥258`: sağlanıyor. |
| E15 | `520+59,85+60+260=899,85≤900`; `1200≤1350`: doğru. |
| E16 | Ek süre `14,5+1,25+35+5+35=90,75<95 ms`; iki pencerede sekiz sonuç kapasitesi doğru. |
| E17 | Kesin hız `39×1,2+49×(1,4+8/15)=141,5333 B/ms`; iki ms’de kuyruk hesabı yaklaşık 2437,07 B ve 58,27 çerçeve, yukarı yuvarlanınca **59<64**. |
| E18 | Kalemlerin toplamı **9,7333 ms**; 10 ms’ye pay **0,2667 ms**. Önceki tick ile 10,2333 ms, azami başlangıç beklemesiyle 13,2333 ms. Bunlar ölçüm değil koşullu bütçe. |
| E19 | Kesin hızlarla `75+5×141,5333+500=1282,6667 B/periyot`, kapasitenin **%85,5111**’i; %90 altında. |
| E20 | Yazılı 63 B alt sınırı yanlış: geçerli kırpılmış örnekle 58 B. İlgili 3,15 ms sonucu doğrulanmadı; Y2. Gönderen aralığının korunduğu dalın `15×2,5=37,5 ms` aritmetiği doğru. |
| E21 | Yazılı geliş varsayımlarıyla `1+floor(5/2,5)=3`, `1+floor(10/2,5)=5`; 29 B TF için `1+floor(5×300/29)=52`: aritmetik doğru. |

E1–E10, değiştirilebilir altı büyüklüğün **64 uç köşe birleşiminde** yeniden hesaplandı; aritmetik ihlal bulunmadı. Bu sonuç taşıma, hizmet ve çağrı süresi varsayımlarını ölçülmüş yapmaz.

**Olay tablosu ve dolu tablo karşı örneği**

A1-K80/K120 ile A4-K67/K106 aynı temel kapıyı tarif ediyor: izin açan olaylarda yer yokluğu veya kova boşluğu `DROPPED_NO_SLOT`; kapatma olayı kova boşken bekletiliyor; son geçerlilikte yineleme duruyor, sonuçsuz yuva 95 ms daha tutuluyor. A4 §3.5 bununla henüz eşitlenmemiştir.

Sekiz yuva doluyken yeni `SAFETY_CLOSE` için yürütülen yol şöyledir: olay düşürülüp çalışmaya devam edilmiyor; platform `EVENT_OVERFLOW=7` ile kapanışı deniyor, oturumu bitiriyor; A4-K71/K73 bekleyen olayları ve yetkileri sonlandırıyor, A1-K23 sim tarafındaki izinleri kapatıyor. Kapanış çerçevesi ulaşmazsa sim bunu anında öğrenmiş sayılmaz; kendi bağlantı ve izin süreleri işlemeye devam eder.

**XML, enum ve derleme denetimleri**

| Denetim | Sonuç |
|---|---|
| XML ileti / alan / enum / enum girdisi | **37 / 334 / 31 / 194** |
| Kanonik satırlar | **37+334+194=565** |
| Yeniden hesaplanan `dialect_hash` | **`0x3939788984A5FF80`**, belgeyle eşit |
| Altın çerçeveler | **38/38**, CRC, `CRC_EXTRA`, ileti kimliği ve boy denetimi geçti |
| CRC denetim girdisi | `"123456789"` → **`0x6F91`** |
| Enum önekleri | 31 enum ve 194 girdinin tamamı `IDAW_` |
| XML enum adları ile `port.h` makrolarının kesişimi | **Boş küme** |
| A4 ayna enum değerleri | **136/136 eşit** |
| Gerçek A8 başlığıyla `port.h` | Apple Clang 17, C17 ve C++20 sözdizimi denetimi geçti |
| XML’deki bütün enum bildirimleriyle gerçek başlıklar | Aynı çeviri biriminde C17 ve C++20 denetimi geçti |
| Algılama yapı boyları | Kayıt **80 B**, parti başlığı **32 B**; başlıktaki yerleşim doğrulamaları geçti |
| Sayaç ve sağlık eşlemesi | A1/A4’te **22/22 sayaç adı**, **8/8 sağlık biti** uyumlu |

`pymavlink` yerelde yoktu; kurulum yapılmadı. Dolayısıyla bu incelemede gerçek üreteç başlığı yeniden üretilip derlenmiş sayılmadı. Ad kümeleri karşılaştırıldı ve XML enum bildirimleri gerçek başlıklarla birlikte ayrıca derlendi.

MAVLink’in alan sıralaması, küçük uçluluk, kırpma ve CRC kuralları canlı [resmî serileştirme belgesiyle](https://mavlink.io/en/guide/serialization.html), enum bildirimi davranışı [pymavlink 2.4.50 üreteç kaynağıyla](https://raw.githubusercontent.com/ArduPilot/pymavlink/v2.4.50/generator/mavgen_c.py) denetlendi. Kırpılmış `SAFETY` karşı örneği bu kurala göre bağımsız kodlandı ve CRC’si hesaplandı. CRC algoritması ayrıca [Microchip AN752](https://ww1.microchip.com/downloads/en/AppNotes/00752a.pdf) ile karşılaştırıldı.

Ping360’ın 200–1200 örnek ve 25 ns birimindeki 80–40000 örnekleme dönemi sınırları [üreticinin protokol belgesinde](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/) doğrulandı. ST’nin [ürün sayfasına](https://www.st.com/en/evaluation-tools/stm32u5a9j-dk.html) erişildi; UM2967 PDF’si zaman aşımı verdi. Bu nedenle kılavuzdaki USART1 bağlantı ayrıntıları bu web denetiminde yeniden doğrulanmış sayılmadı. Kart, Windows, gerçek kesme gecikmesi ve uçtan uca zamanlama ölçümü yapılmadı.

## İskelete ya da diğer bölümlere notlar

**Mimar kararının mevcut kurallarla iç tutarlılığı**

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| M1 | `docs/gorevler/W1-uyum.md` U12(c), A4 K123 ↔ A4 K107; A1 E9 | Sonucu beklenen izin olayı varken yeni izin olayı yasaklanıyor; K107 ise ardışık yenilemelerin `hold/2` aralığını aşmamasını koşulsuz istiyor. `hold=60 ms` ve sonucun 40 ms’de dönmesi örneğinde ilk 30 ms boyunca yeni olay K123 ile yasak; K107’nin 30 ms sınırına uyulamıyor. A1’in sonuç bekleme kuralları bu gecikmeyi dışlamıyor. | U12’nin sonucu bekleme yasağını koru. K107 ve E9’daki kesintisiz yenileme iddiasını, sonucun zamanında alınabildiği koşulla sınırla; aksi durumda iznin güvenli biçimde düşebileceğini açıkça belirt. | kritik |

İskelet 0.10’daki revizyon listesi ve fizik sınırındaki beş adım A1-K106/K85 ile uyumludur. Eski 01–09 bölümlerindeki farklılıklar bulgu sayılmadı.

`protocol/komutlar.json` içindeki `fc_fire`, onaylı genişletici komuttur; parametre şeması yalnız `tube=0…1` kabul ediyor ve ek özellikleri reddediyor. Hedef, menzil, yön veya atış ölçütü alanı yok. A1/A4 gerçek arayüzlerinde `FIRE` olayını platformun üretmesine izin veren bir yol bulunmadı; üretim çekirdeğe ait. Kalan kapsam artığı Y7’deki eski `FC_SELECT` sınama adıdır.

dondurulamaz