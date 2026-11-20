# sol incelemesi: sozlesmeler

## Hüküm

Uyum turu önemli eksikleri gidermiş; ancak nesil değişimi, halka eşlemesinin kaldırılması ve senaryo doğrulamasında kritik boşluklar kalmış. Dosya değiştirilmedi, kurulum yapılmadı.

Aşağıdaki kısaltmalar `docs/sozlesmeler/` altındaki şu belgeleri gösterir: A1 `A1-kart-baglantisi.md`, A2 `A2-gorsellestirici.md`, A4 `A4-port.md`, A5 `A5-senaryo.md`, A7 `A7-sahne.md`, A8 `A8-kamera-algilama.md`. `P3-n`, `docs/sozlesmeler/inceleme/W1-capraz-sol-3.md` bulgusudur. Düzeltme numaraları sonraki tablodadır.

**Önceki 17 bulgunun kapanışı**

| Bulgu | Hüküm | Baktığım yer | Kalan eksik |
|---|---|---|---|
| P3-1 — Kopma ve zaman aşımı | KISMEN | A2-K59–K60, K86–K88; A8-K43, K45, K50 | Sim başvuruyu koruyor; bileşenin okumayı bitirmeden kendi eşlemesini kaldırması yasaklanmamış. D3. |
| P3-2 — Halka yenileme protokolü | KISMEN | A8 §3.5, K70–K77; A2 “A8’den beklenen” | Yeni iletiler ve onay sırası mevcut. A2’nin beklenti listesi güncel A8 kurallarına bağlanmamış; artık yanlış olan eksiklik notları duruyor. D7. |
| P3-3 — Çalıştırma nesli ve sayaç ömrü | KISMEN | A2-K49, K80–K85; A8-K15–K17, K78–K80 | Eski nesil sonucu reddediliyor, kare sayacı korunuyor. Sim kabul kapısının zaman durumu ve yeni çalıştırmada nesil artırımı eksik. D1–D2. |
| P3-4 — Geçersiz kare | KAPANDI | A8-K48; A2 §3.8 | Bit 7, yapılandırma ve kalibrasyon sürümü denetleniyor; geçersiz kareden kayıt üretilmiyor. |
| P3-5 — Düşme kodları | KAPANDI | A8-K63, K65, K76; `sim/include/ida/a2_wire.h` `SimKareDusmeNedeni` | 1–8 ve 33–39 eksiksiz. Kod 4’te eski nesil nedeniyle başlık gönderilmemesi ayrıca tanımlı. |
| P3-6 — Canlı SIL eşitliği | KAPANDI | A1-K102/K121; A5-K85; A2-K51; A8-K60/K61; iskelet §4 | A1/A5 güvencesi kamera kapalı koşula çekilmiş. A8 hiçbir kipte bekleme güvencesi vermiyor. |
| P3-7 — Kamera alanlarının senaryoda taşınması | KAPANDI | A5 §3.6, §3.9, §3.12, K114, Ek E; şemanın `sensor`/`sensor_cozum` kamera dalları | İç parametreler, iki montaj, pozlama, kırpma, gecikmeler, optik ve ışık alanları mevcut. Süreç ayarları A2-K93’e taşınmış. |
| P3-8 — Kamera tip ve sınırları | KAPANDI | A2 §3.6; A8 §3.5/K36; iki başlık dosyası | Eski pozlama, kalibrasyon genişliği ve ana nokta uyuşmazlıkları giderilmiş. Yeni A5 kuaterniyon tutarsızlığı D6’da. |
| P3-9 — Optik çerçeve dönüşümü | KAPANDI | A2-K94; A5-K89/K114; A8 §2/K53 | Sabit dönüşüm ve iki örnek doğru. |
| P3-10 — Kamera sayısı ve etkinlik | KISMEN | A5-K80/K114; A2 §3.3/§3.6; A8-K35 | En çok bir etkin kamera ve `inst=0` tamam. Kamera açıkken sıfır etkin kameranın yalnız uyarıyla kabulü karşılanmıyor. D5. |
| P3-11 — Varlık kümesi ve sahne atıfları; A5+/A7+ | KISMEN | A5 §3.15, K19/K36/K115, Ek C; A7 §3.7, K56; manifest | Kimlikler, özet, statik poz ve taban alanı tamam. Derinlik denetimi statik pozu kullanmıyor. D4. |
| P3-12 — Malzeme adları ve aralıkları; A5+/A7+ | KAPANDI | A5 §3.11/K18; A7 §3.4, K30/K57; iki şema | Akustik adlar, `lambert_cos2`/`cos1` ve sınırlar aynı; optik geçersiz kılmalar mevcut. |
| P3-13 — Kapasiteler; A5+ | KAPANDI | A5-K116; şema `hedefler` ve dalga sınırları; A2 §3.2; `a2_wire.h` | Dalga ≤256; gövde bütçesi araç ve iki tüp dahil ≤64, dolayısıyla hedef ≤61. |
| P3-14 — Fizikten nominal parametre türetimi; A5+ | KAPANDI | A5-K16, K51–K53; A4 O9 | Türetim 1.0’dan kaldırılmış; yalnız açık parametre çiftleri var. W3 kataloğu açıkça bekleniyor. |
| P3-15 — Rastgelelik kataloğu | KISMEN | A5-K84/K118, Ek D.7; A1-K114/K126; A2-K95 | Etiketler ve Philox sayaçları tamam; etiket içindeki deneme/oturum sıra numaralarının tahsisi tanımsız. D8. |
| P3-16 — Sınıf kodları; A5+ | KAPANDI | A5 §3.10/K113; A8-K23; `algilama_kaydi.h` `IDA_DET_CLASS_*` | 0–3 tablosu aynı; 4–255 ayrılmış. Seçim anlamı eklenmemiş. |
| P3-17 — Kapsam dışı görev ve opak alanlar | KISMEN | A5-K55 ve görev şeması; A1 Tablo 3.6a/K122; A4-K119–K121, §7 | Etkin arayüzler temizlenmiş; A4 uygunluk satırında `SELECT` hâlâ sınanacak işlem olarak bulunuyor. D9. |

**Ortak kararlar ve kalan “+” maddeleri**

| Madde | Hüküm | Baktığım yer | Kalan eksik |
|---|---|---|---|
| U13 — Tek kamera yapılandırması kaynağı | KISMEN | A2 §3.6/K93/K94; A5-K80/K114; A8-K36/K53 | Alan aktarımı tamam; etkin kamera bulunmaması ve kuaterniyon kabul kümeleri tutarsız. D5–D6. |
| U14 — Halka ve nesil yaşam döngüsü | KISMEN | A2-K49, K53, K56–K65, K74, K80–K92; A8-K43–K50, K70–K80 | D1–D3 ve güncellenmemiş karşılıklı atıflar D7. |
| U17 — Kimlik ve rastgelelik kataloğu | KISMEN | A5 Ek D.7/K118; A1-K114/K126; A2-K95 | Etiketler harfi harfine aynı; etiket parametrelerinin tahsisi eksik. D8. |
| U19 — Nesil değişimi ve A1 oturumu | KISMEN | A1-K128; A4-K122; A8-K17/K78/K79; A2-K80 | A1 oturum yenilemesi ve f64 yapılandırma tamam. A2 yeni çalıştırmayı saymıyor; sim’in zaman kabul durumu sıfırlanmıyor. D1–D2. |
| A5+ — Şema, örnekler, Ek E ve altın özet | KAPANDI | `scenarios/`, A5 Ek E/K17 | Bağımsız çözümlemede altın nesne aynı; kanonik baytlar ve özet eşit. Standart şema doğrulayıcısı bulunmadığına ilişkin sınır aşağıda. |
| A8+ P2-15 — Ayrıntılı ret sayaçları | KISMEN | A8-K30, §3.5 | Çekirdekten yalnız toplam beklenmesi tamam. Uyum belgesinin istediği bileşen sayaçlarının yayımlanması açıkça dışlanmış. D10. |
| A8+ P2-17 — Zaman aralığı ve taşma | KAPANDI | A8-K13/K27/K81; `IDA_DET_TIME_MAX_NS` | Aralık önce denetleniyor; fark i64’e sığıyor. |
| A8+ P3-16 — Sınıf tablosuna atıf | KAPANDI | A8-K23; A5-K113 | Tablo ve ayrılmış aralık aynı. |
| A8+ — A4 parti atomikliği ve bayatlık beklentileri | KAPANDI | A8 §8.2 “A4’ten beklenen” md.2/5; A4-K41/K118 | Atomiklik ve `T_det_stale` karşılanıyor; eski madde numarası atıfları D7’de. |

**A2’nin “A8’den beklenen” listesindeki 16 maddenin tek tek denetimi**

| Madde | Hüküm | Güncel A8 karşılığı | Sonuç |
|---|---|---|---|
| 1 — Salt okunur eşleme | KAPANDI | K38 | Yazma yasak. |
| 2 — Kare bildirimi alanları | KAPANDI | §3.5 `FRAME_AVAILABLE`, K72 | İstenen kimlik, nesil, sayaç ve zaman alanları mevcut. |
| 3 — Çit ve başlık denetimleri | KAPANDI | K48/K73 | Okuma öncesi ve sonrası denetimler mevcut; görünürlük varsayımı doğrulanmış sayılmıyor. |
| 4 — Geçersiz kare/sürüm | KAPANDI | K48 | `FRAME_INVALID`/`CALIB_MISMATCH`; kayıt yok. |
| 5 — Bırakma üçlüsü | KAPANDI | K45/K74, `FRAME_RELEASE` | Kimlik bağlantıdan; üçlü eşleşmesi ve yinelenen bırakma tanımlı. |
| 6 — Kopma/zaman aşımı | KISMEN | K43/K45/K50 | Sim sahipliği doğru; bileşenin kendi eşlemesi için D3 açık. |
| 7 — Yenileme ve yeni nesli onaylama | KAPANDI | K70/K71 | Eski okuma, sonuç ve bırakma tamamlanmadan başarılı ACK yok. |
| 8 — Kamera yolu kapanan tüketici | KAPANDI | K77 | Yeni bağlantı ve tam el sıkışma gerekiyor. |
| 9 — A2 oturum sonu | KAPANDI | K75 | Okuma bitiriliyor, eşlemeler sonra kaldırılıyor; yeni halka yeniden teklif ediliyor. |
| 10 — Çalıştırma nesli | KISMEN | K17/K78/K79 | Eski sonuç reddi var; sim zaman durumunun yeniden başlangıcı eksik. D1–D2. |
| 11 — Kare sayacının ömrü | KAPANDI | K80 | Süreç boyunca sürüyor. |
| 12 — Düşme kodları | KAPANDI | K76 | 15 kodun tamamı tanımlı. |
| 13 — Geçerlilik anı | KAPANDI | K10–K12 | Pozlama ortası doğru türetiliyor. |
| 14 — Yapılandırma ve nominal montaj | KAPANDI | §3.5 `CAMERA_CONFIG`, K36/K53 | İç parametreler f64; nominal montaj ayrı. A2’deki f32 açıklaması yanlış kalmış: D7. |
| 15 — Kademe/tek kamera | KAPANDI | K35/K68 | A8 tarafında tek kamera ve A2 kademe değerleri geçerli; senaryo boşluğu D5’te. |
| 16 — Halka adı | KAPANDI | §3.5 `RING_OFFER.ring_name`, §8.2 | 64 baytlık alan A2 adlarını NUL dahil taşıyor. |

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| D1 | A8-K17/K27/K78/K79 ↔ A1-K128, A4-K122 | Nesil değişiminde **kartın** son kabul edilen ölçüm zamanı sıfırlanıyor; sim’in K27 doğrulayıcısındaki karşılığı için kural yok. Örneğin son kabul edilen zaman 10 s iken geri atlanıp yeni nesilde 1 s’lik geçerli parti gelirse `run_gen` geçer, K17 zaman ilerleme denetimi reddeder. | A8’de sim kabul kapısının zaman durumunu da nesil değişiminde sıfırla; `batch_seq` ve `det_seq` durumlarının korunmasını ayrı belirt. | kritik |
| D2 | A2-K80/K85 ↔ A8-K17, A1-K128 ve `docs/gorevler/W1-uyum.md` U19 | A2 nesli yalnız `sifirlama` ve `atlama` olaylarında artırıyor. Aynı sim sürecindeki **yeni çalıştırma** için artış ve bildirim yok. A2 oturumu açık kalabildiğinden önceki çalıştırmanın yoldaki sonucu aynı nesli taşıyabilir; K79 onu ayıramaz. | A2-K80 ve ilgili olay/bildirim sırasına yeni çalıştırmayı ekle; önceki çalıştırmanın karelerini K81–K84 ile geçersizleştir. | kritik |
| D3 | A8-K43 ↔ K71/K75; A2-K59/K60/K88 | K43, bağlantı kapanınca eşlemenin bırakılmasını söylüyor; K71/K75’teki “önce okumayı bitir” sırası burada yok. İzin verilen çok iş parçacıklı uygulamada soket iş parçacığı eşlemeyi kaldırırken işçi piksel okumaya devam edebilir. Sim’in halkayı tutması bileşenin kaldırdığı sanal adresi korumaz. | Bütün kapanış yollarında yeni okumayı durdur, mevcut okumaların bittiğini kesinleştir, sonra eşlemeyi kaldır. Bekleyen sim başvurularını koruyan mevcut kural sürsün. Eşleme kaldırmanın adres aralığını geçersizleştirdiği [Microsoft belgesinde](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-unmapviewoffile) açık. | kritik |
| D4 | A5 §3.9/K36 ↔ A7 §3.3/K56; A2 `Ortam.statik_*` | A7 taban derinliği statik varlığın **REF noktasına göre yerel** uzaklığı; A5 bunu dünya su derinliğiyle doğrudan karşılaştırıyor. Yeni statik poz uygulanmıyor. `taban_derinligi_m=3`, statik NED konumu `(0,0,1)`, birim yönelim ve `ortam.derinlik=3` denetimi geçer; geometrik taban dünya D=4 m’dedir. | A5 derinlik denetimini sahne pozu uygulanmış geometriye bağla; yerel beyanı dünya su derinliğiyle dönüşümsüz karşılaştırma. | kritik |
| D5 | A5-K49/K80/K114 ↔ A2 §3.3/§3.6, A8-K35 | Kamera kademesi açıkken etkin kamera bulunmaması yalnız uyarı. Böyle bir dosyada iç parametreler, pozlama ve kırpma alanları da zorunlu değil. A2 ise açık kademede `kamera_sayisi=1` ve geçerli yapılandırma istiyor; A8 de tam bir kamera yapılandırması bekliyor. Kabul edilen A5 girdisinin karşılığı kurulamayabiliyor. | Kamera kademesi açıkken tam bir etkin kamera ve `inst=0` koşulunu A5’te yükleme hatası olarak doğrula. | kritik |
| D6 | A5-K32/K114 ve kamera montaj şeması ↔ A2 §3.6, A8-K36 | Kamera için tek geçerli aralık kaynağı denmesine rağmen A5 `abs(norm(q)−1)≤1e-6`, A2/A8 `abs(norm²(q)−1)≤1e-5` kullanıyor. `q=(0,7071096096080154;0;0;0,7071096096080154)` A2/A8’den geçer, A5-K32’den kalır. | Kamera montajlarının kabul denetiminde A2’nin kuralını kullan; A5-K32’nin kamera için nasıl uygulanacağını ve normalleştirme sırasını açıkça ayır. | önemli |
| D7 | A2 “A8’den beklenen”, §3.6 ve §6 ↔ A8 §3.5/K48/K70–K81; A8 §5/§8.1 ↔ §8.2 | A2 listesi hâlâ “alan yok”, “yeni ileti eklenir” gibi artık yanlış notlar taşıyor; güncel kural numaralarına bağlanmamış. A2 §3.6 hâlâ A8’in f32’ye çevirmesinden söz ediyor; A8 artık f64. A8’in bayatlık atıfları “A4’ten beklenen 3” diyor; güncel md.3 ret sayacı, bayatlık md.5. | Beklenti listesini yukarıdaki 16 karşılıkla güncelle; f32 ve eski iskelet/kademe açıklamalarını düzelt; bayatlık atıflarını md.5’e bağla. A5 §8 md.18’deki “A1-K114 bunu yazmıyor” notu da artık yanlış. | önemli |
| D8 | A5 Ek D.7/K118 ↔ A1-K114 | Ek D.7, `<deneme>` ve `<oturum_sırası>` anlamlarını A1-K114’e bırakıyor; K114 bunları yalnız etiket içinde tekrar ediyor. İlk değer, artırılma olayı, yeniden gönderimle yeni denemenin ayrımı ve sayaçların ömrü tanımlı değil. Philox’un etiket içindeki `n` sayacı bunu tamamlamıyor. | A1 bu etiket parametrelerinin başlangıç, artış ve ömür kurallarını tanımlasın; A5 doğrudan o kurala bağlansın. | önemli |
| D9 | A4 §7, K44/K45/K104/K111 uygunluk satırı ↔ A1 Tablo 3.6, A4-K119 | Genişletici işlemleri sınayan satır hâlâ `FC_ARM/SELECT/FIRE/RESET/TEST_ENTER` istiyor. Güncel kapalı listede `SELECT` yok. | Uygunluk satırından `SELECT` işlemini kaldır; ayrılmış op değerinin reddi için mevcut K119 sınamasını kullan. Yeni alan veya işlem ekleme. | önemli |
| D10 | A8-K30/§3.5 ↔ `docs/gorevler/W1-uyum.md`, A8+ P2-15 | Uyum görevi ayrıntılı sayaçların bileşende ve sim kabul kapısında tutulup A3/A6 üzerinden yayımlanmasını istiyor. K30 ise bileşenin iç sayaçlarını açıkça dışa kapatıyor; §3.5’te taşıyıcı da yok. | A8’i verilen sayaç yayımlama yükümlülüğüne tamamla. Çekirdeğin tek toplam `det_rejected` sayacıyla sınırlı olması korunsun. | önemli |

## Yeniden hesaplananlar

**Halka yaşam döngüsünün dört yürütümü**

| Senaryo | Yürütüm ve sonuç |
|---|---|
| Bileşen okurken kopma | Sim başvuruyu kaldırmaz; etkin H halkasını emekliye ayırır, H’ye yeniden yazım için yuva ayırmaz; kopan tüketici adına H’yi A2 oturum sonuna kadar tutar. **Sim sahipliği doğru.** Bileşenin K43 ile kendi eşlemesini okumadan önce kaldırabilmesi D3’tür; iki sözleşme birlikte koşulsuz bellek güvenliği vermiyor. |
| 1 s zaman aşımı | K50 yuvayı bırakmaz, H emekliye ayrılır ve H′ teklif edilir. K71 eski okuma, bırakma ve gönderilecek sonuç tamamlanmadan H′ için başarılı ACK’yi yasaklar. Eski içerik yeniden kullanılmaz. **Sahiplik bakımından güvenli.** |
| Halka yenilenirken eski kare okunması | A2-K86 eski tüketici kümesini korur; yeni halka ayrı nesnedir. A8-K71/K74 eski okumayı bitirir, bırakır ve ancak sonra yeni halkayı onaylar. A2-K88 bütün bağlı tarafların geçişini bekler. **Sahiplik bakımından güvenli.** |
| Sıfırlama sırasında yoldaki sonuç | Eski `run_gen` taşıyan sonuç K79’da doğrulamadan reddedilir; bırakma nesilden bağımsız işlenir. Yeni sonuçların sim zaman denetiminde takılması ise D1’dir. **Eski sonucun sızması engellenmiş; yeni neslin kabulü eksik.** |

Bu yürütümler A2-K53’ün süreçler arası görünürlük varsayımını kanıtlamaz. A8-K73 de bunu doğrulanmadı diye işaretliyor. C++ çit eşzamanlamasının atomik işlemler üzerinden tanımlandığı [standart taslağında](https://eel.is/c++draft/atomics.fences) doğrulandı; soket üzerinden uçtan uca görünürlük bu incelemede doğrulanmış sayılmadı.

**Yeni A8 iletileri ve A2 başlığıyla alan karşılaştırması**

| İleti | Bağımsız boyut hesabı | Alan karşılaştırması |
|---|---:|---|
| `RING_OFFER` | 112 B | Oturum u64; nesil/yuva sayısı u32; yuva/toplam boyut u64; boyutlar u16; satır adımı u32; kamera/biçim u8; ad 64 B: `HalkaBilgisi` ve `HalkaBasligi` karşılıklarıyla uyumlu. |
| `RING_ACK` | 16 B | Oturum u64, nesil u32. A8 durumları 0/1/2; A2 `HalkaEslendi` durumları 0/6/7’dir. Ayrı protokollerin kodlarıdır; doğrudan yapı kopyası değildir. Başarılı onayın anlamı uyumlu. |
| `A2_SESSION_END` | 8 B | A2 oturum kimliği u64, kayıpsız. |
| `RUN_EPOCH` | 8 B | Çalıştırma nesli u32, A2 ile aynı genişlikte. |
| `FRAME_RELEASE` | 16 B | İstek u64, halka nesli u32, yuva u16: A2-K58 üçlüsüyle aynı. |
| `FRAME_AVAILABLE` | 56 B | İstek/oturum u64; kare/halka/çalıştırma nesli/yazım/yapılandırma u32; yuva u16; kamera u8 uyumlu. Pozlama A2’de i64, A8’de u32: ≤1.000.000.000 ns sınırında kayıpsız. |
| `CAMERA_CONFIG` | 152 B | İç parametreler ve bozulma f64; kalibrasyon u16. Piksel kodu A2 u8 → A8 u16; bozulma kodu A2 u32 → A8 u8, geçerli 0–1 kümesinde kayıpsız. Hız mHz’ye tam dönüşüyor. Montaj A5’in nominal montajıdır. |

`CAMERA_CONFIG`, A2’nin 184 baytlık yapısının bire bir kopyası değildir. Pozlama kare bildiriminde taşınır; yakın/uzak kırpma alanları A8 yapılandırmasında bulunmaz ve görselleştiricide kullanılır. `L_alg` ile `L_sonrası` sim’in zamanlama girdileridir. Ortak taşınan alanlarda sayısal daraltma bulmadım; D6 kabul kümesi, D7 açıklama tutarsızlığıdır.

**Kamera ve sayısal denetimler**

| Denetim | Bağımsız sonuç |
|---|---|
| İDA-0 tam kamera | A5 `fx=fy=381,36`, `cx=319,5`, `cy=239,5` → A2’nin 640×480 sınırlarında → A8’de aynı f64 değerler. |
| İDA-0 pozlama/kırpma | 8.000.000 ns pozitif, çift ve 1 s altında; 0,05 < 40 m. Kalibrasyon 1, u16’ya sığıyor. Fizik ve nominal montajlar ayrı alanlarda mevcut. |
| Odak hesabı | `320/tan(40°)=381,3611496301472 px`; dosyadaki yaklaşık 381,36 değeriyle yatay açı ≈80,0001701°. |
| İDA-0 zaman zinciri | `t_cizim=1.000.000.000`; `t_olcum=1.004.000.000`; varsayılan `L_sonrası=0` ile teslim zamanı aynı, `S_d=1.005.000.000`; `L_alg=50 ms` ile `t_alg=1.054.000.000`, `P_k` kapanışı 1.055.000.000 ns. Bunlar modellenmiş zamanlar; gerçek hattın yetiştiğini kanıtlamaz. |
| A2-K94 örnek a | Birim montajda optik sağ → gövde +y, optik aşağı → +z, optik ileri → +x. Doğru. |
| A2-K94 örnek b | `q=(cos45°,0,−sin45°,0)` için `R_bO=[[0,−1,0],[1,0,0],[0,0,1]]`. Optik ileri +z, sağ +y, aşağı −x. Doğru. |
| Tam halka | Piksel 921.600 B; yuva 925.696 B; sekiz yuvalı halka 7.409.664 B. |
| Sade halka | Piksel 230.400 B; yuva 233.472 B; sekiz yuvalı halka 1.871.872 B. |
| Düşme eşlemesi | 1–8 ve 33–39: 15 kodun tamamı mevcut. Kod 4 eski nesil nedeniyle başlıksız; diğer 14 kod `FRAME_DROPPED`. Kod 8 meşgul/yok durumuna çevrilmiyor. |
| Algılama bütçesi | `39+4×90=399 B`; `30×399=11.970 B/s`; 200 Hz’de 59,85 B/periyot. |
| Sayaç ömürleri | Kamera u32 sayacı 30 Hz’de ≈4,5366 yıl; kayıt sayacı 120 kayıt/s’de ≈414,252 gün. Belgelerdeki yaklaşık hesaplar doğru. |

**Senaryo, varlıklar ve altın dosya**

| Denetim | Sonuç |
|---|---|
| Varlık atıfları | `ida0/arac`, `torpido_ida0/torpido`, `hedef_ornek/hedef`, `havuz_ornek/statik` manifestte mevcut. |
| Düğüm atıfları | `MNT_kamera0`, `TUBE_1`, `TUBE_2` mevcut; konumlar A5 ile aynı, yönelimler birim. Gerçek GLB düğümleri mevcut olmadığından geometri üzerinden doğrulanamadı. |
| Malzemeler | Atamalar `govde_plastik`, `hedef_boyali_metal`, `havuz_beton` kimliklerine çözülüyor. Örnek senaryoda malzeme geçersiz kılması yok; şemadaki akustik/optik ad ve sınırlar karşılaştırıldı. |
| Üç yüzerlik durumu | Manifest toplam hacimleri boş/bir/iki yük için 0,0136992 / 0,01386675 / 0,0140343 m³. Merkezler A5 Ek A değerlerinin 2 mm, hacimler %1 toleransı içinde. Bu, bulunmayan ağların geometrik doğrulaması değildir. |
| Gövde/dalga sınırı | En çok `1+61+2=64` gövde; dalga en çok 256 bileşen. A2 ve başlık sabitleriyle aynı. |
| Görev: WAYPOINT | LOCAL; global alanlar sıfır; N/E=(2,0) m; derinlik 0; yarıçap 0,3 m ve hız 0,3 m/s geçerli; kullanılmayan `p[2:4]` sıfır. A1-K122’den geçer. |
| Görev: SURFACE | NONE; bütün konum alanları ve derinlik sıfır; çıkış hızı 0,25 m/s; diğer parametreler sıfır. A1-K122’den geçer. |
| Manifest SHA-256 | 6.030 ham bayt: `29063c2d7f89a80e452dfbb66504e9470d5c928d0fcf7dd295497e2b22a22ca4`. A7 §3.7, senaryo ve altın dosyayla aynı. |
| Altın dosya SHA-256 | 10.527 ham bayt: `d1735f8b7eafabede9350c591bb4db7aa3a563b8ac4f45500200d33d4427cd92`. A5-K17 ile aynı. |
| Kanonik biçim | UTF-16 sıralaması ve ECMAScript sayı dizilimiyle yeniden üretilen baytlar ham dosyayla aynı. Bu seçim [RFC 8785](https://www.rfc-editor.org/rfc/rfc8785) ile uyumlu. |
| Bağımsız çözümleme | Araç/senaryo birleştirmesi, Ek E varsayılanları, nominal montaj kopyası ve üst veri listeleri bağımsız yeniden kuruldu; altın JSON nesnesiyle aynı çıktı. |

Şema dosyaları ve örnekler ayrıştırıldı; şemalarda kullanılan yapısal koşulları değerlendiren bağımsız bellek içi denetimde dört örnek için hata çıkmadı. Makinede `jsonschema` ve AJV bulunmadığından standart JSON Schema 2020-12 doğrulayıcısıyla tam doğrulama yapılmadı.

**Ek D.7 yeniden hesabı**

A1-K114/K126 ve A2-K95’teki etiket yazımları A5 kataloğuyla aynı. `ana_tohum=1` için yeniden hesap:

| Çekiliş | Sonuç |
|---|---|
| `a1/board_tohum`, n=0 | `0xb1536801d0cf436c` |
| `a1/run_id`, n=0 | `1059443490`, `0x3f25d322` |
| `ida/a1/nonce_sim/0`, n=0 | `3869573587`, `0xe6a501d3` |
| `ida/a1/nonce_board/1/0`, türetilmiş kart tohumu | `220998108`, `0x0d2c29dc` |
| `ida/a1/sess/0` | n=0: 18101; bu yasaksa n=1: 6699 |
| `a2/kamera_gurultu`, kamera 0 | Kare 1: `0x8bacb11bdc5f81f9`; kare 2: `0xfbfa969d1162aba6` |

Hepsi belgeyle aynı. Temel Philox vektörleri de [Random123’ün birincil vektör dosyasıyla](https://raw.githubusercontent.com/DEShawResearch/random123/main/tests/kat_vectors) örtüşüyor. Bu aritmetik doğrulama, D8’deki çağıran sayaçlarının tahsis eksikliğini kapatmıyor.

## İskelete ya da diğer bölümlere notlar

Önceki iskelet notları 0.10’da karşılanmış: kamera karesinin gösterdiği an pozlama ortasına bağlanmış; bariyerli HIL için tekrarlanabilirlik iddiası daraltılmış; sorumluluk tablosundaki atış çözümü görevi kaldırılmış. Eski 01–09 metinleriyle farklar bulgu sayılmadı.

**Kapsam sınırı — önemli:** A8 §6.2 hâlâ iskelet §5’e dayanarak yakın bölgede hedefin “konumunun kamera algılamasıyla sürdürüldüğünü” söylüyor. Güncel iskelet yalnız bu bölgede kamera algılama kayıtlarının bulunduğunu belirtiyor; konum sürdürme davranışı tanımlamıyor. A8’deki ifade bu dar sınıra çekilmeli.

`protocol/komutlar.json` içindeki `fc_fire` yalnız zorunlu `tube` 0–1 parametresi taşıyor; ek özellikler yasak, onay gerekli ve komut genişletici. Hedef, menzil, yön veya atış ölçütü parametresi yok. A1-K125, A4-K64/K67 ve A5-K73 birlikte incelendi: istek karta gidiyor, `FIRE` olayını çekirdek üretiyor; platform olay üretmiyor. İncelenen etkin arayüzlerde hedef seçimi, hedefleme çözümü veya bunları taşıyacak yeni opak kanal bulmadım. D9 eski uygunluk metninde kalan işlemdir.

Canlı SIL eşitliğinin kamera kapalı koşulu A1-K102/K121, A5-K85 ve iskelet §4’te açıkça aynı. A2-K51 kamera açık bütün SIL koşularının sınırını yazıyor. A8-K60 bütün kiplerde beklemeyi yasaklıyor; K61 ve §6.3 açıklamaları hızlandırılmış SIL’i öne çıkarsa da yavaş/1x koşulara ek eşitlik güvencesi vermiyor.

Canlı birincil kaynak denetiminde Godot 4.7.2’nin [`force_draw` ana iş parçacığı koşulu](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/doc/classes/RenderingServer.xml), [`get_scene_node` için null davranışı](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/modules/gltf/doc_classes/GLTFState.xml), [glTF eksen ve hiyerarşi kabulleri](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html), Apple’ın [31 bayt ad sınırı](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/sys/posix_shm.h) ve [tek başarılı boyutlandırma kısıtı](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/kern/posix_shm.c) doğrulandı. CoACD’nin 17 API ayarı ve iki sabit iç ayarı [etiketli API](https://raw.githubusercontent.com/SarahWeiii/CoACD/1.0.14/public/coacd.h) ile [Params kaynağından](https://raw.githubusercontent.com/SarahWeiii/CoACD/1.0.14/src/config.h) denetlendi.

CoACD etiketinin GitHub API uç noktasına erişilemedi; tam 40 haneli commit bu yoldan doğrulanamadı. [Resmî sürüm listesi](https://github.com/SarahWeiii/CoACD/releases) 1.0.14 ve `1401ce2` kısa özetini gösteriyor. Gerçek GLB dosyaları, iki platformdaki halka çalışması ve Windows kamera zamanlaması bu incelemede doğrulanmış sayılmadı.

dondurulamaz