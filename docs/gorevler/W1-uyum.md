# W1-uyum — Çapraz uyum turu: mimar kararları ve iş dağılımı

Tarih: 2026-11-20. Önceki adım: `docs/gorevler/W1-capraz.md`. Dayanak: sol'un dört parçalı çapraz incelemesi, bütünleştirme dalında (`sozlesme/w1-butunlesme`) `docs/sozlesmeler/inceleme/W1-capraz-sol-{1,2,3,4}.md`. Dördünün de hükmü "dondurulamaz"dır; toplam 72 bulgu (çoğu iki ya da üç parçada yinelenir). Aşağıda bulgular `P<parça>-<no>` diye anılır.

Bu belge her bulguyu bir sözleşmeye atar ve sözleşmeler arasında ortak olan her değeri **burada** sabitler. İskelet bu kararlarla 0.10 oldu (`docs/mimari/00-iskelet.md` §2, §3, §3.1, §3.2, §4, §5, §8, §9); iskeletle bu belge birlikte okunur.

## Turun kuralları

1. Her yazar yalnız **kendi dalında, kendi dosyalarını** değiştirir. Öteki sözleşmelerin güncel metni bütünleştirme dalından **salt okunur** okunur.
2. Yazarlar aynı anda çalışır; karşı sözleşmenin bugünkü metni bu belgeyle çelişiyorsa **bu belge geçerlidir**, karşı taraf bu belgeye göre değişecektir. Karşı sözleşmenin sahibi olduğu bir değeri yineleme: kural ya da bölüm numarasıyla atıf ver. Bu belgede sabitlenmemiş, iki sözleşmeyi bağlayan yeni bir ad ya da sayı gerekiyorsa onu aşağıda **sahibi** yazılan taraf tanımlar; öteki taraf yalnız atıf verir ve raporunda "karşı taraftan beklediğim" diye yazar.
3. Tek tur. Bulgu yeniden tartışılmaz; bir kararı yanlış buluyorsan uygula **ve** itirazını gerekçesiyle raporla (karar bir emniyet ya da kapsam sınırını gevşetiyorsa uygulama, raporla).
4. Kural numaraları kararlıdır: var olan numara başka anlama kaydırılmaz; yeni kural sıradaki numarayı alır; kaldırılan kuralın numarası "kaldırıldı (W1-uyum)" notuyla kalır.
5. Makinece okunur tanımlar (başlık, XML, şema, örnekler, altın dosyalar) belgeyle aynı turda güncellenir ve yazarın kendi denetiminden geçer (derleme, şema doğrulaması, özetlerin yeniden üretimi). Kurulum yapılmaz; elde olmayan araç "doğrulanmadı" diye yazılır.
6. **Kapsam sınırı:** hedef seçimi, hedefleme çözümü, önleme hesabı ve atış ölçütü hiçbir sözleşmede tanımlanmaz, hiçbir alanda taşınmaz; bir bulguyu kapatmak için yeni opak alan, serbest argüman ya da "anlamı uygulamaya bırakılmış" kanal açılmaz. Torpido güçlendirilmez.
7. Commit atılmaz. Rapor: dokunulan dosyalar; aşağıdaki her madde için **kapandı / kısmen / uygulanmadı (neden)**; karşı taraftan beklenenler; kendi denetimlerinin sonucu; doğrulanamayanlar.

## Ortak kararlar

**U1 — Kapalı listeler (P1-1, P1-2, P1-15, P2-1, P2-21, P3-17, P4-19).** FC işlemlerinin ve görev öğesi türlerinin tek tanımı A1 Tablo 3.6 / 3.6a'dır. A4 aynı kapalı listeyi taşır: `IDA_FC_SELECT`, `IDA_MI_TARGET_ALIGN`, `fc_ref`, `ext_len`, `ext[]` kaldırılır; FC komutunun `arg0`/`arg1` alanları bu sürümde 0 olmak zorundadır (değilse komut reddedilir). Görev öğesinin `p[]`, `frame`, konum ve `depth` alanlarının anlamı, birimi ve aralığı için A4, A1 Tablo 3.6a'ya atıf verir ve çekirdeğin aynı denetimi yaptığını yazar. A5'in görev öğeleri A1 Tablo 3.6a'nın alanlarını **bire bir** taşır (ayrı gösterim ve dönüşüm yok; hepsi zorunlu alan, SI birimi; `TARGET_ALIGN` yok). Günlük kodu ve ret nedeni de opak kanal olamaz: A4 günlük kodlarının kapalı tablosunu (kod, anlam, `arg0`/`arg1` anlamı) ve `inhibit_mask` bitlerinin kapalı tablosunu (yalnız emniyet engelleri) yazar; tabloda olmayan kod üretilmez; yeni kod sözleşme revizyonu ister. `reason ≥ 0x100` aralığı "ayrılmış, kullanılmaz" olur; FC retleri ortak 0–15 tablosundaki kodlarla bildirilir, bu turda yeni neden kodu eklenmez. A1 bu tabloları yinelemez, A4'e atıf verir.

**U2 — Emniyet revizyonu (P1-4, P1-5, P2-İ1, P4-4).** Tek tanım iskelet §3.2'dedir: `uint32`, başlangıç 1, sıfırlanmaz, `0xFFFFFFFF`'te doyar, doyduktan sonra bütün gevşetmeler reddedilir; artıran altı işlem ve artırmayanlar orada sayılıdır. A1-K106 ve A3-K117 bu listeyi **aynen** yazar; A1-K89 bağımsız artış tanımlamaz (tek girdi iki tüpü kapatsa da bir artış). A3/A6 ortak yüklerinde ve HTTP şemalarında alan `u32` olur (JSON'da 0…4294967295).

**U3 — Fizik sınırındaki sıra (P1-6, P1-7, P4-5).** İskelet §3.2 "Fizik sınırındaki sıra" beş adımı A1-K85 ve A3-K95'te aynen yazılır; A5 ve A6 atıf verir. A5'in zamanlı ve koşullu emniyet atamaları bu sıranın 2. adımında (kısıtlayan) ve 4. adımında (gevşeten) işlenir; senaryonun gevşeten ataması, o sınırda revizyon 1–3. adımlarda arttıysa reddedilir, yeniden denenmez ve olay olarak kaydedilir (`gevsetme_reddedildi`). A5 yükleme doğrulaması: aynı fizik sınırına düşen iki **zamanlı** atamadan biri kısıtlayan öteki gevşetense (alanları farklı olsa da) senaryo reddedilir.

**U4 — Yolda bekleyen genişletici komut (P1-3).** (a) Sim tarafı, sahibi A3: genişletici yer istasyonu komutu kabul anındaki revizyona bağlanır; bağlantı emülatöründen çıkıp A1'e verileceği anda revizyon farklıysa atılır (sonuç `bayat_yetki`, kayda geçer). Kabul edilen her daraltıcı yer istasyonu komutu revizyonu artırır (iskelet §3.2 md.1). (b) Kart tarafı, sahibi A1: platform bir daraltıcı komutu mandala aldığında, ertelenmiş kuyruğunda bekleyen bütün genişletici komutları atar ve her biri için ret sonucu üretir. A4'ün "aynı çağrıda daraltıcı varsa genişletici reddedilir" kuralı sürer; A4 bu platform yükümlülüğünü "A1'den beklenenler"e yazar. Tel biçimi değişmez.

**U5 — Kısıtlayıcı içerik kaybolmaz (P1-9, P1-10).** (a) A4-K116: "kısıtlayıcı içerik" bir emniyet örneğinin kısıtlayan yönde taşıdığı her değerdir (acil durdurma basılı, fiş çıkık, pim takılı); önceki kabul edilen değere göre geçiş aranmaz. Bir çağrıdaki herhangi bir emniyet örneği (kabul edilen ya da reddedilen) bir değeri kısıtlayan yönde taşıyorsa çağrı sonunda çekirdeğin gördüğü değer kısıtlayandır; gevşeten değer ancak hiçbir örneğin kısıtlayan değer taşımadığı sonraki bir çağrıda kabul edilir. Yeni kapanış olayı ve sayaç yalnız etkin değer değiştiğinde üretilir. (b) A1: eski ya da yinelenen bir partinin üyeleri atılırken, CRC'si ve oturumu geçerli `IDA_SAFETY` üyesi **atılmaz**; sıradaki çekirdek çağrısına verilir (çekirdek onu bayat diye reddeder, kısıtlayıcı içeriğini uygular). Platform bir çağrıya girecek örnekleri seçerken emniyet örneklerini önce yerleştirir; emniyet örneği kapasite nedeniyle düşürülmez (A1 bir çağrıya düşebilecek emniyet örneği sayısının örnek kapasitesini aşamayacağını hesapla gösterir).

**U6 — Fiş ve pim tüp başınadır (P1-8, P4-6).** A3'ün durum yükü, değiştirme ve onay istekleri, jeton bağı ve olayları fiş ile pimi tüp başına (`f1`, `f2`) taşır; acil durdurma tektir. A6 ortak yükü aynı tanımdan gelir.

**U7 — Süre aralıkları (P1-11, P2-10, P2-11).** Bu uygulamanın geçerli aralıklarının sahibi A1-K104'tür. A4'ün çıktı doğrulaması ve `port.h` sabitleri aynı aralıkları kullanır: `apply_by` 16–1000 ms, `hold` 60–450 ms, `valid_for` 60–450 ms. A4'ün `T_imu_stale` parametresi 5–30 ms'ye sınırlanır (varsayılan 20 ms); A1-E10 en kötü durumu 30 ms ile yazar (30 + 14,5 = 44,5 ms < 60 ms).

**U8 — Sıra numaraları (P1-12).** İskelet §3.1 "Sıra numaraları". A1-K31/K66'daki "boşluğun üst sınırı yoktur" kalkar; A1 ve A4 aynı tanımı kullanır (oturum ve komut kimliği uzlaştırması dahil).

**U9 — Canlı SIL eşitliği (P2-13, P3-6, P4-3).** İskelet §4 "Canlı koşular arası eşitlik". A1-K102/K121 ve A5-K85'in iki canlı koşu arasındaki eşitlik güvencesi "kamera kapalıyken" koşuluna bağlanır. A4 §7'deki HIL kaydını canlı çıktıyla bit eşitliği üzerinden kabul eden uygunluk satırı kalkar; güvence yalnız SIL port kaydının tekrarıdır (A6-K34/K56).

**U10 — Arka plan çağrı takvimi (P2-5, P4-2).** Sahibi A1-K121: canlı SIL'de her step ve tick çağrısından önce tam `N_bg` arka plan çağrısı; oturum sonu çağrısından önce arka plan çağrısı yapılmaz. A4-K95 buna eşitlenir ("IDLE dönene kadar" kalkar). Tekrarda arka plan çağrıları **yalnız** kayıtlı çağrı dizisinden yürütülür (A6-K54/K55); A4-K109'daki "`bg_slices` kadar çağrı eklenir" ifadesi kalkar, `bg_slices` yalnız tutarlılık denetimidir (kayıtlı arka plan çağrısı sayısıyla uyuşmazsa tekrar hata verir). `N_bg`, A6 üst verisinde `sil_n_bg` (u32) alanında saklanır (SIL çalıştırmalarında zorunlu).

**U11 — Tick takvimi ve oturum sonu çağrısı (P2-6, P2-7).** Zaman çerçevesi akarken ızgara tick'inin takvimi A1-K117'dir; A4-K18/K19/K23 buna eşitlenir, "çağrılar arası en uzun süre" güvencesi bu takvimden türetilir. A4 oturum sonu çağrısını ızgara tick'inden ayrı bir çağrı türü olarak tanımlar (`tf.present = 0`, bağlantı olayı oturum sonu; SIL'de `now_w` = son kabul edilen zaman çerçevesinin zamanı); SIL'deki ızgara tick'i yasağı sürer, oturum sonu çağrısı onun istisnasıdır (A1-K107).

**U12 — Olay tablosu, kova ve sonuç zamanı (P2-8, P2-9).** Sayıların sahibi A1 (K80, K120, E8, E16). A4-K67/K106 eşitlenir: (a) `DROPPED_NO_SLOT` iki nedenle üretilir: tablo dolu ya da (izin açan olayda) kova boş; (b) sekiz yuvanın biri `SAFETY_CLOSE`'a ayrılır, izin açan olaylar ve `FIRE` en çok yedi yuva kullanır; `SAFETY_CLOSE` yer yokluğundan düşürülmez: ayrılmış yuva da doluyken yeni bir `SAFETY_CLOSE` gelirse platform oturumu kapatır (A1 yeni kapanış nedeni tanımlar: `EVENT_OVERFLOW` = 7) ve alıcıdaki bütün izinler oturum sonuyla düşer; (c) son geçerlilik dolunca platform yinelemeyi durdurur, yuvayı sonuç gelene ya da A1'in sonuç payı (95 ms) dolana kadar açık tutar, `EXPIRED_NO_ACK` ancak bu pay sonuçsuz dolunca çekirdeğe verilir; sonucu bekleyen olayın izni açık varsayılmaz ve aynı tüp için yeni izin açan olay ya da `FIRE` üretilmez.

**U13 — Kamera yapılandırmasının tek kaynağı (P1-13, P3-7, P3-8, P3-9, P3-10).** Geçerli aralıkların sahibi A2 §3.6'dır ve A8'in bugünkü (daha dar) sınırlarına çekilir: `pozlama_ns` pozitif, çift, ≤ 1 000 000 000; kalibrasyon sürümü 16 bit (alan `u16` + ayrılmış `u16`, yerleşim değişmeden); ana nokta ve odak sınırları A8'in bugünkü değerleri. A8 sayıyı yinelemez, A2'ye atıf verir. Senaryodan gelen her kamera alanını A5 taşır (iç parametreler, bozulma, pozlama, kalibrasyon sürümü, nominal ve fizik gerçeği montajı, kırpma uzaklıkları, `L_alg`, su optiği, ışık, kameranın gerekli olup olmadığı; varsayılanlar A5 Ek E'de, A2/A8'in bugünkü varsayılanlarıyla aynı). Süreç ayarları (soket, paylaşılan bellek adı, halka yuva sayısı) senaryo alanı **değildir**: A2 bunları "sim yapılandırması" olarak kendi içinde tanımlar ve "A5'ten beklenen" listesinden çıkarır. Montaj: A5 ve A8 montajı S çerçevesinde verir (x ileri); A2 optik çerçeveye (z ileri) sabit dönüşümü ve bir uygunluk örneğini yazar (birim montaj kuaterniyonu → kamera gövde +x yönüne bakar). Tek kamera: A5 doğrulaması en çok bir etkin kamera ve `inst = 0` ister. A5'te kamera kademe değerleri için kaynak A2 gösterilir (A8 değil).

**U14 — Halka ve nesil yaşam döngüsü (P3-1…P3-5).** Kaynak A2'dir (K49, K53, K56–K65, K74, K80–K92, "A8'den beklenenler"). A8 protokolü buna tamamlanır: kopma ve zaman aşımında yuva bırakılmaz, halka emekliye ayrılır; halka yenileme iletisi ve yeni nesli onaylayan yanıt; eski okumalar bitmeden yeni halkanın onaylanmaması; oturum sonu bildirimi; kamera yolu kapatılan tüketicinin dönüşü; çalıştırma nesli bildirimi ve eski nesil sonuçların reddi; kare sayacının ömrü A2'deki gibi (süreç boyunca); `tutarsiz` bayraklı ya da yapılandırma/kalibrasyon sürümü tutmayan kareden kayıt üretilmemesi; A2'nin bütün düşme nedeni kodlarının (1–8, 33–39) A8 kare durumlarına kapalı eşlemesi (özgün neden kayıtta korunur). İleti adlarının ve kodlarının sahibi A8'dir; A2 "A8'den beklenenler" listesini A8'in kural numaralarına bağlar.

**U15 — Tel enum adları (P2-18).** `protocol/ida.xml`'deki bütün enum adları ve girdileri `IDAW_` önekini alır (değerler aynı; ileti adları `IDA_` kalır); `port.h` sabitleri `IDA_` önekinde kalır. A1 kanonik özeti yeniden üretir, altın çerçeveleri yeniden doğrular. İki başlığın aynı çeviri biriminde derlenmesi W2'de derleme denetimi olur.

**U16 — Komut kataloğu (P4-13, P4-14).** Sahibi A1: `protocol/komutlar.json`, A1'in kapalı komut listesinden türetilir. Biçim: `{"surum": "1.0", "komutlar": [{"ad": <küçük harfli ad>, "a1": {"mesaj": …, "tur": …}, "sinif": "daraltici" | "genisletici" | "notr", "onay": true | false, "parametreler": <JSON Schema>, "sonuc": <JSON Schema ya da null>, "bilesik": true | false}]}`; çok iletili işlemler (görev yükleme, parametre kaydetme) tek bileşik komut olarak yer alır. A3-K100/K103 bu dosyayı değiştirmeden döndürür ve doğrulamayı ona göre yapar; A3'ün nihai yanıt olayı kataloğun `sonuc` şemasına uyan bir `veri` nesnesi taşır (parametre değeri, ret nedeni). Panelde sim fırlatıcısını kartı atlayarak tetikleyen uç yoktur. Operatörün atış isteği (`FC_FIRE`) katalogda onaylı genişletici komuttur: karta gider, emniyet zincirini kart yürütür, `FIRE` olayını yalnız çekirdek üretir; bu sürümde atışı başlatan tek kaynak budur (tur içinde netleştirildi; böylece emniyet zinciri sözleşmelerde tanımlanmayan bir atış ölçütüne gerek kalmadan sınanır). Kataloğa hedef, menzil, yön ya da ölçüt taşıyan parametre girmez.

**U17 — Çalıştırma kimliği ve rastgelelik kataloğu (P3-15, P4-20).** A1'in `run_id` (u32) alanı A3/A6'nın metin kimliğinden ayrıdır: kartlı kiplerde metin kimliğinin UTF-8 baytlarının SHA-256'sının ilk dört baytı (küçük uçlu; 0 çıkarsa 1), SIL'de A5 türetimiyle `a1/run_id` etiketinden üretilir. A6 üst verisi ikisini de saklar (`run_id`, `a1_run_id`). A5 Ek D'nin alt akış kataloğu, A1-K114'ün SIL etiketlerini (A1'deki adlarıyla), `a1/run_id`'yi ve kamera gürültüsü için `a2/kamera_gurultu`'yu içerir; değer türetimi (yasak ya da sıfır sonuçta yeniden çekiliş, sayaç tahsisi) A5'te yazılır, A1 ve A2 atıf verir.

**U18 — Sayaç, olay ve kayıt eşlemeleri (P1-14, P4-7…P4-12, P4-15…P4-18).** A3/A6 katalogları aşağıdakilerin her biri için adlandırılmış bir taşıyıcı ve kaynak eşlemesi tanımlar (serbest metin ya da "ham trafikten çözülür" yeterli değildir): A1 Tablo 3.11b'nin 36 bağlantı sayacı, A4'ün 22 çekirdek sayacı ve sekiz sağlık biti, A1'in `kritik_erteleme`, `cagri_izi_eksik`, `sonuc_talebi_atildi` sayaçları ile yapılandırma reddinin parametre ve eşitsizlik numarası; A2'nin halka olay ve sayaçları; A8'in parti başlığı, nihai kare durumu, ret nedeni ve geç yanıt kayıtları (boş partiler dahil); A5'in `atama_celiskisi`, `gevsetme_reddedildi`, `donma_onceki_cikti_yok` olayları, "uygulanmadı" sonucu ve doğrulama raporu (ek olarak). A3'ün zamanlama geçerliliği nedenleri A1'in nedenleriyle bire bir olur (son tarih kaçırma, çağrı çakışması, geç olay, eşzamansız güvenli çıktı, geç yanıt reddi). Ayrıca: çözümlenmiş senaryo eki A5'in kanonik JSON baytlarıdır (`cozumlenmis_senaryo.json`); özetlerin metin gösterimi A3/A6'da `sha256:` + A5'in 64 hex değeridir, kaynak özeti ayrı alandır; A1 şema özeti alanı A1'in tek `dialect_hash`'idir, açılış kaydı `IDA_BOARD_HELLO`'dur; A8 sürümü `major`/`minor` iki alandır; kamera karesi kaydı A2'nin istek kimliği, kare sırası, kamera, çalışma nesli, pozlama sonu, pozlama süresi ve sürüm alanlarını taşır; Ping360 ortak yükü cihaz birimindeki özgün alanları korur (türetilmiş değerleri panel hesaplar, formül A3'te); her zaman çerçevesinin gerçek yazım başlangıcı ve bitişi kaydedilir; kartlı kayıttan yeniden kurma, oturum değerlerini kayıttaki el sıkışma çerçevelerinden alan PC girişini kullanır (A1 bu girişi tek kuralla tanımlar; çok oturumlu kayıtta her oturumda çekirdek kayıttaki açılış alanlarıyla yeniden başlatılır). A5'in kayıt ayarları A3/A6'nın değer kümelerini kullanır: kademe `olaylar` | `telemetri` | `tam` ("kapalı" yok: emniyet kaydı kaldırılamaz), kamera kare kaydı `kapali` | `pencereli` | `surekli`.

**U19 — Çalıştırma nesli ve A1 oturumu (tur içinde eklendi; A8 turunun sorusu üzerine).** Sim zamanının süreksiz olduğu her durum (yeni çalıştırma, sıfırlama, atlama; A2/A8 çalıştırma neslinin her değişimi) A1'de oturumun kapanması ve yeni oturumun kurulmasıdır; aynı oturum içinde zaman çerçevesinin sim zamanı geriye gitmez ve sıçramaz. Karta ayrı bir nesil bildirimi gitmez: algılama akışlarının zaman ilerleme durumu oturum sonu çağrısında sıfırlanır (A1-K107; A4 oturum sonu çağrısı; A8-K17). A6'nın çok oturumlu kayıt tanımı bununla tutarlıdır. A8'in kamera yapılandırma iletisi iç parametreleri A2'deki gibi f64 taşır (çevirme yok).

**Tur içinde kabul edilen yorumlar.** (1) U5a: çağrıdaki bütün genişleticilerin reddi yalnız etkin değerin kısıtlayana döndüğü çağrıda tetiklenir; süregelen kısıtlama kendi girdisinin gevşetilmesine ve kapsadığı izinlere üstün gelir (A4-K45, K116; iskelet §3.2'ye işlendi). (2) U5b: bir çağrıya düşen emniyet örneklerinin kapasiteyi aşmayacağı zamanlamadan bağımsız kanıtlanamadı; sınır aşılırsa örnek atılmaz, oturum kapanır (A1 E20, H52); koşullar W3/W4'te ölçülür. (3) U2: `sifirla` kendi başına artış değildir; A3 kuyruktaki her komutu uygulamadan önce çalıştırma kimliğiyle yeniden denetler (A3-K57). (4) İDA-0 kamera değerleri mimari seçimidir (A5 `ida0.toml`).

## Sözleşme başına işler

Her madde ilgili ortak kararın o sözleşmedeki karşılığıdır; "+" ile başlayanlar yalnız o sözleşmeye özgüdür.

### A1 (`sozlesme/a1-v1`)

U2 (K106, K89), U3 (K85), U4b, U5b, U8, U9, U12 (yeni kapanış nedeni; K120 metni A4 ile aynı kapı), U15, U16, U17.
+ P2-2: `SET_MODE.arg` yalnız A4'ün kabul ettiği hedefler; 0/1 dönüştürücüde `LINK_BAD_FIELD`.
+ P2-3: daraltıcı `FC_DISARM`/`FC_ABORT` bütün tüplere uygulanır; bu iki işlemde `tube` = 255 zorunlu. Mandalın `source` taşımadığı ve bunun bilgi kaybı olmadığı yazılır.
+ P2-4: hata dönüşü dahil her yanıtta `ACTUATOR.batch_seq`, çağrıya verilen zaman çerçevesinin sıra numarasıdır (`out.seq` değil).
+ P2-11: E10, `T_imu_stale` üst sınırı 30 ms ile.
+ P2-20: alım damgasının gerçek alım anından en büyük sapması sayıyla verilir (A4 bu sayıya atıf verecek).
+ P4-18: PC'ye özgü yeniden kurma girişi (U18).
+ `IDA_LOG.code`, `reason`, `inhibit_mask` açıklamaları A4'ün kapalı tablolarına atıf verir (U1).
+ §8.2 "A4'ten beklenenler" listesi bu belgeye göre güncellenir.

### A4 (`sozlesme/a4-v1`)

U1 (kaldırmalar, sıfır argüman, Tablo 3.6a atfı, günlük kodu ve engel biti tabloları, neden aralıkları; P2-19: 128–191 bağlantı katmanınındır, port sonucu olarak üretilmez), U4 (beklenti), U5a, U7, U8, U9, U10, U11, U12.
+ P2-12 ve §6: kamera kapalıyken kayıt sayısı 0'dır, durum başlığı gelen çağrıda parti başlığı sayısı 0 değildir; Ping360 satırı iki kademede de 1200 örneğe kadardır ("sade 600" kalkar).
+ P2-14: algılama partisi atomiktir; bir kayıt geçersizse parti bütünüyle reddedilir (A8-K27/K29).
+ P2-16: algılama akışının bayatlığı: `T_det_stale` (varsayılan 2 s; tasarım seçimi) boyunca başlık gelmeyen akış bayattır; çekirdek o akışın kayıtlarını kullanmaz ve durumu `inhibit_mask`'teki bir bitle bildirir.
+ P2-20: `t_rx` tanımı A1-K48'in damgalama kuralına ve sapma sınırına bağlanır; 34,5 ms sınırının hangi damgadan sayıldığı yazılır.
+ P3-14: parametre kataloğu W3'ündür (O9 açık kalır); A5'in fizikten türetimi 1.0'dan çıkarıldı, A4'ten bu konuda beklenti kalmadı.
+ P4-1 için: `port.h`'deki her birliğin (union) seçici alanı ve etkin üyesi açıkça belgelenir (A6 kodlaması buna dayanacak).

### A5 (`sozlesme/a5-v1`)

U1 (görev öğeleri), U3, U9 (K85), U13, U17, U18 (kayıt ayarları, olaylar).
+ P3-11: varlık kümesi seçimi (küme yolu ve beklenen küme özeti), torpido varlık atfı, statik sahne pozu; `ortam.sahne` A7'nin `statik` türüne eşlenir; taban derinliği denetimi A7 manifestindeki isteğe bağlı `taban_derinligi_m` alanına göre yapılır (alan yoksa uyarı). Örnek senaryoların atıf verdiği varlık kimlikleri A7'nin uygunluk örneğiyle tutar (aşağıda A7).
+ P3-12: malzeme geçersiz kılmalarında A7'nin alan adları, enum değerleri ve aralıkları aynen kullanılır.
+ P3-13: dalga bileşeni ≤ 256; yaşayan gövde bütçesi: 1 (araç) + hedef sayısı + tüp sayısı + diğer hareketli gövdeler ≤ 64, yüklemede doğrulanır.
+ P3-14: "nominal modeli fizikten türet" seçeneği 1.0'dan çıkarılır (K51–K53); otopilot bloğu yalnız açık (kimlik, değer) çiftleri taşır.
+ P3-16: `hedefler.<ad>.sinif` kapalı enum olur ve sayısal tablosu A5'te durur: 0 `bilinmiyor`, 1 `sabit_nesne`, 2 `hareketli_nesne`, 3 `yapi`; 4–255 ayrılmış. Tablo yalnız algılama kaydının sınıf kodlamasıdır; seçim anlamı taşımaz.
+ Şema, örnek senaryolar, Ek E ve altın dosya özeti yeniden üretilir.

### A2 ve A7 (`sozlesme/a2a7-v1`)

A2: U13 (aralıklar, süreç ayarları, optik çerçeve dönüşümü), U14 (beklenti listesinin A8 kurallarına bağlanması), U17 (`gurultu_tohumu` ↔ `a2/kamera_gurultu`).
A7: + P3-11: manifest şemasına statik sahne varlığı için isteğe bağlı `taban_derinligi_m`; `assets/ornek/manifest.json` A5 örneklerinin atıf verdiği bütün varlıkları (araç, havuz, hedef, torpido) içeren bir uygunluk örneğine genişletilir (ikili dosyalar yoktur; bu açıkça yazılır). + P3-12: malzeme alan adlarının ve aralıklarının kaynağı olduğu A5'e karşı açıkça yazılır.

### A8 (`sozlesme/a8-v1`)

U13 (A2'ye atıf), U14 (bu turun en büyük işi).
+ P2-15: akış ve neden başına ret sayaçları çekirdek telemetrisinden beklenmez (çekirdekte tek toplam `det_rejected` vardır); ayrıntılı sayaçlar bileşende ve sim'in kabul kapısında tutulur, A3/A6 üzerinden yayınlanır.
+ P2-17: zaman alanları 0…2⁶² ns (A4-K08); "başlık + 1 s" karşılaştırması taşmasız yazılır.
+ P3-16: `class_id` A5'in sınıf tablosuna atıf verir (U1 sınırıyla).
+ "A4'ten beklenenler": bayatlık eşiği A4'te (P2-16), parti atomikliği A4'te (P2-14).

### A3 ve A6 (`sozlesme/a3a6-v1`)

U2, U3 (A3-K95; A6 atıf), U4a, U6, U10 (tekrar ve `sil_n_bg`), U16, U17 (üst veri), U18 (tamamı).
+ P4-1: A6'nın alan alan kodlamasına `i32` eklenir; birlikler seçici alana göre yalnız etkin üyeyle yazılır; kullanılmayan alan yazılmaz; `PARAM_SAVE` çıktısının blobu `len` (u32) + `len` bayt olarak kodlanır. Kodlama, U1 ile temizlenmiş `port.h`'ye göredir (kaldırılan alanlar kayda girmez; P4-19).
+ Belgeler üreteçten geliyorsa üreteç güncellenir ve iki kanal dosyasındaki ortak yüklerin derin eşitliği yeniden doğrulanır.

## İskelet (mimar; yapıldı)

P1 ve P2 ile P3 ve P4'ün iskelet notları 0.10'a işlendi: emniyet revizyonunun kapalı listesi ve sırası (§3.2), saat alanları ve bariyer kipinin tanımı (§4), atış kontrolün kapsam ifadeleri (§2, §5, §9), kamera karesinin gösterdiği an (§8), yeniden kurma girişi (§3), bant hesabı (%85,5).

## Doğrulama turu (2026-11-20)

sol'un dört parçalı doğrulama incelemesi (`docs/sozlesmeler/inceleme/W1-dogrulama-sol-{1..4}.md`, bütünleştirme dalında) dört parçada da "dondurulamaz" dedi: çapraz incelemenin bulgularının çoğu kapanmış, 19 kritik ve 20 dolayında önemli bulgu kalmıştı (parçalar arasında yinelenenler var). Aynı gün W2'nin ilk işleri sözleşmeleri uygulama gözüyle okudu: çekirdek tasarım notu A4'te 35, tel katmanı uygulayıcısı A1'de 7 belirsizlik bildirdi. Hepsi aşağıdaki mimar kararlarıyla yazarlara verildi; iskelet 0.11 oldu. Kararlar kullanıcı kararı değildir.

**Emniyet zinciri**

- **D-1 Yolda bekleyen genişleticinin üçüncü bekleme noktası.** Sim, A1 gönderim kuyruğunda bekleyen genişletici komutu tele yazmadan hemen önce kabul revizyonuyla yeniden denetler; değiştiyse göndermez (`bayat_yetki`). U4 üç noktalıdır: emülatör çıkışı (A3-K101), sim gönderim kuyruğu (A1-K129), kart erteleme tamponu (A1-K63).
- **D-2 Taşınan emniyet örneği gevşetemez.** Reddedilen (`DUP`/`OLD`) ya da birleştirilen (en yeni dışındaki) partiden sonraki çağrıya taşınan `IDA_SAFETY` örneği `VALID = 0` ile teslim edilir; çekirdek yalnız kısıtlayıcı içeriğini uygular (A4-K130, A1-K31/K124). Gevşetme, periyodik yeniden gönderimle kabul edilen bir partiden gelir.
- **D-3 İzin yenileme güvencesi koşulludur.** Sonuç beklenirken yeni izin olayı yasağı (U12c, A4-K123) korunur; `hold/2` yenileme güvencesi önceki sonucun zamanında alınmasına bağlıdır; gecikirse izin alıcıda süresi dolunca düşer (A4-K107, A1 E9).
- **D-4 `SAFETY_CLOSE` birleştirme.** Bir çekirdek çağrısının kapanmaları birleştirilir: iki tüpü kapsayan neden varsa tek olay, yoksa tüp başına en çok bir (A4-K125); `SAFETY_CLOSE` hiçbir koşulda düşmez.
- **D-5 Hata yanıtı ve olay.** Hata dönüşünde çıkan olay son kabul edilen zaman çerçevesinin numarasını taşır (yoksa 0); sim'de yanıt yuvası olay beklemeden tamamlanır ve olay kendi numarasıyla uygulanır (A4-K57/§4.14, A1-K73/K84).
- **D-6 Emniyet örneği kapasitesi.** En küçük tel boyları kırpmayla hesaplanır: emniyet taşıyan en küçük pencere 54 B, sıkışmada sınır 2,70 ms (A1 E20); güvence koşulludur, aşılırsa oturum kapanır; koşullar W3/W4'te ölçülür.

**Çekirdek çağrı çerçevesi (A4; çekirdek tasarım notu `core/TASARIM.md` üzerine)**

- **D-7** Kurulmamış çekirdeğin çıktısı sabittir (A4-K126); komut başına ret nedenleri kapalı tablodur, yeni neden kodu eklenmedi (A4-K124, §3.17); kip ile `armed` bağımsızdır (A4-K127); ilk kabulden önce IMU bayat, tüp boş, izin kapalı sayılır (A4-K128); çekirdeğin bekleyen olay kümesi 7 + 1'dir (A4-K129).
- **D-8** `cmds[]`'teki daraltıcı komut reddedilen çağrıda da uygulanır; `SESSION_START` reddedilen çağrıda da sıra izini sıfırlar; etkin emniyet değerleri `SESSION_END`'de kısıtlayana döner; `out` NULL çağrısında oluşan kapanma olayı ve günlük sonraki yazılabilir çağrıda verilir; emniyet girdisinin tek gevşeten kaynağı `inst = 0`'dır.

**Bağlantı (A1; tel katmanı uygulaması üzerine)**

- **D-9** Denetim sırası: başlıkta `incompat_flags` → kimlik → `len`; CRC'den sonra kimlik çifti → yön. Başlıkta reddedilen adayda tarama `0xFD`'den sonraki bayttan sürer; CRC hatasında aday bütünüyle tüketilir. `rx_seq_gap` olay sayar. `cnt_rx_error` toplamı `rx_unknown_msgid`'i içerir. A1-K101 işlev adı vermez: durum yalnız örneğin kendi yapısındadır (uygulama kendi ayrıştırıcısını kullanır).
- **D-10** Komut kimliği uzlaştırması doğrudan `last_cmd_id` penceresiyle yapılır (A1-K66). Oturum sonu çağrısının izi en iyi çabayla gönderilir; yeniden kurma ona dayanmaz. Ayna numaralandırma eşlemesi `protocol/ayna.json` dosyasındadır (ad kuralı mekanik değildir).

**Kayıt, tekrar, panel (A3, A6)**

- **D-11** Geçersiz girdi bozuk dosya değildir: tanımsız seçicili girdi başlığıyla kaydedilir, gövdesi kaydedilmez (A6-K52; A4 gövdeyi okumaz). `PARAM_GET` yalnız kimlik taşır; parametre değeri kayıtta ham 32 bittir.
- **D-12** Zaman çerçevesi yazım zamanları yalnız `telemetri` ve `tam` kademelerinde kayda girer; `olaylar` kademesindeki kayıt zamanlamanın yeniden kurulmasına yetmez (A1-K43, A6-K28). Sim bu zamanları her kademede ölçer ve kullanır.
- **D-13** Çelişen atamalar 8 öğelik parçalarla kayıpsız taşınır (A3-K125, A6-K61); A5'e sınır eklenmez. Kart geri okuması yalnız kartın bildirdiği engel maskesinden türetilir; sim'in gönderdiği emniyet değeri kart okuması diye sunulmaz.

**Görsel taraf ve senaryo (A2, A5, A8)**

- **D-14** Çalıştırma nesli yeni çalıştırmada da artar (A2-K80); sim kabul kapısının zaman durumu nesil değişiminde sıfırlanır (A8). Bütün kapanış yollarında eşleme, süren okumalar bittikten sonra kaldırılır (A8).
- **D-15** Kamera kademesi açıkken tam bir etkin kamera ve eksiksiz yapılandırma A5'te yükleme hatasıdır; kamera montajının kuaterniyon kabul kuralı A2'ninkidir; taban derinliği denetimi statik poz uygulanmış geometriyle yapılır.
- **D-16 (P2-15'in kapanışı).** Kamera algılama bileşeninin iç sayaçları dışa açılmaz; ayrıntılı ret, durum ve geç yanıt sayaçları sim kabul kapısında tutulur ve A3/A6'da yayımlanır; çekirdekte tek toplam sayaç vardır. Uyum turundaki ilk ifadeden bilinçli daraltmadır.

**Süreç.** Bu turun değişiklikleri doğrulama incelemesinden sonra yapıldı; dondurmadan önce sol, bütünleştirme dalının doğrulama anındaki hâli (`40b8d63`) ile son hâli arasındaki farkı inceler (fark incelemesi).

## Fark incelemesi turu (2026-11-20)

sol'un iki parçalı fark incelemesi (`docs/sozlesmeler/inceleme/W1-fark-sol-{1,2}.md`, bütünleştirme dalında; `40b8d63..6b63c6e`) iki parçada da "dondurulamaz" dedi: doğrulama turunun 40 bulgusundan 36'sı kapanmış, 4'ü kısmen kapanmıştı; 5 kritik, 5 önemli, 1 küçük yeni bulgu vardı. Tel biçimi, `dialect_hash`, altın çerçeveler ve yapı yerleşimleri sol'un yeniden hesabında da değişmemişti. Aynı tura çekirdek uygulayıcısının A4'te açık bıraktığı 6 okuma ile çekirdek kabul testlerinin 20 belirsizliği ve 2 ifade notu eklendi. Kararlar kullanıcı kararı değildir.

**Emniyet zinciri**

- **D-17 Dördüncü bekleme noktası: kartın açık partisi (P1 bulgu 1).** Kart bir daraltıcı komutu mandala aldığı anda, kendisinden önce alınmış ve henüz hiçbir çekirdek çağrısına girmemiş bütün genişleticileri atar ve `REJECTED` / `NARROWED` yanıtlar: erteleme tamponundakiler, açık partidekiler ve kapanmış ama çağrılmamış partidekiler (A1-K63). Gerekçe: gecikmiş zaman çerçevesinde araya giren ızgara tick'i mandalı tüketiyor, açık partideki eski `ARM` sonraki adıma mandalsız giriyordu. İskelet 0.12 §3.2.
- **D-18 Bekletilen kapanma her yazılabilir çıktıda verilir (P1 bulgu 2).** `out` NULL çağrısında oluşan kapanma, sonraki ilk yazılabilir çıktıda dönüş kodundan bağımsız olarak çıkar (A4 §4.14); kurulmamış çekirdeğin sabit çıktısı hariç.
- **D-19 Komut ret tablosu alan aralıklarını kapsar (P1 bulgu 3).** Maskeyle seçilen alanın aralık ihlali `OUT_OF_RANGE`'dir (A4 §3.17, K50); yeni neden kodu yok.
- **D-20 Daraltıcı hiçbir biçim ya da yineleme denetimiyle reddedilmez (P1 bulgu 4).** Çekirdeğe mandalla ya da yanlışlıkla `cmds[]`'te gelen daraltıcı yineleme penceresinden iki yönde muaftır ve genel alan denetimine girmez (A4-K46 önceliklidir).
- **D-21 Emniyet örneğinin yeniden gönderimi teslim güvencesi değildir (P1 bulgu 6).** 100 ms yeniden üretme aralığıdır; gevşetme ancak kabul edilen bir partinin kendi örneğiyle gelir, koşulsuz kabul süresi türetilmez (A1-K124).

**Bağlantı (A1)**

- **D-22 (P1 bulgu 5).** Komut kimliği seçiminde sıfır bütün dalların sonunda atlanır (A1-K66); 0 "henüz komut yok" gösterimidir.

**Çekirdek çağrı çerçevesi (A4; uygulamadan gelen okumalar)**

- **D-23** `step_miss` tabanını init'ten sonraki ilk çağrı kurar (`out` NULL olsa da); `out` NULL çağrısındaki kapanma `narrow_from_rejected`'a sayılır, iç tutarlılık denetimi sonraki yazılabilir çağrıda yapılır; "ms" birimi yalnız §3.9 tablosundaki parametreleri kapsar; `PARAM_COMMIT`'te komutun işlendiği andaki `armed` kullanılır ve girdideki her `ARM` sayılır; `FAIL` bildirilen `SAVE` de `gen` tabanını yükseltir; nesil değişiminde sıfırlanan sensör kabul durumu kapalı listedir ve emniyetin etkin değerlerini gevşetmez.
- **D-24** Kabul testlerinin belirsizlikleri: init'in `now_w`'si geri adım tabanıdır; günlüğün `t_w`'si olayın saptandığı çağrınındır (init günlüklerinde init'in); API denetim sırası NULL → kurulmamış → bulunamadı; aralık dışı `now_w` geri adım sayılmaz; şemada olmayan kayıt bloğu geçersiz kılar; `window_ns = 0` IMU reddedilir; kabul sonucunda neden 0'dır; `cmd_rejected` çıktıdaki her reddi sayar; ikinci `SESSION_END` olay üretmez; kurulmamış çıktı eşitliği alan alandır; `input_dropped` hata dönüşlerinde de yansır; tanımsız olay sonucu yok sayılır ve olay beklemede kalır; oturum sonu çağrısındaki örnekler olağan kurallarla işlenir; IMU bayatlığının "başladı" günlüğü yalnız taze → bayat geçişinde üretilir.

**Görsel taraf, kayıt, panel (A2, A3, A6)**

- **D-25 (P2 bulgu 1).** Kamera montajında bit eşitliği her bağlantının kendi A5 kaynağına bağlıdır: fizik montajı A2'ye, nominal montaj A8'e; A8 fizik gerçeğini almaz. Ortak iç parametre ve bozulma alanları kayıpsız aktarılır.
- **D-26 (P2 bulgu 2).** Her `kare_sirasi` için düşme kararı, nedeni, kaydı ve sayımı tektir; ilk sonuçlandıran kural geçerlidir. Yuvanın kapatılması düşme kaydından ayrıdır (A2-K81/K64/K96).
- **D-27 (P2 bulgu 3–5).** Çelişki parçalarının olay sıra numaraları artandır, ardışık olmak zorunda değildir (A3-K125, A6-K61); kayıtta gereken kanalın bulunmaması 422'dir (A3-K68; A6 eşitlendi); A6'nın A4'ten karşılanmış beklentisi kapatıldı.

**Süreç.** Bu turun düzeltmeleri sol'a yeniden incelettirilmedi (kullanıcı talimatı: bulgular düzeltilir ve durulur). Dondurma kullanıcı kararını bekler; dondurmadan önce bu turun farkının incelenip incelenmeyeceği de o karardadır.

## Sonraki adım

Altı dal bütünleştirme dalına yeniden birleştirilir; sol **doğrulama incelemesi** yalnız bu belgenin maddelerinin kapanışını ve turun yeni çelişki üretip üretmediğini denetler. Kritik bulgu kalmazsa dondurma: sürüm 1.0, etiketler `sozlesme-A<n>-v1`, PR.
