# sol incelemesi: sozlesmeler

## Hüküm

**Dondurulamaz.** Dar kapsamlı W1 çapraz incelemesinde kapsam sınırını aşan alanlar, emniyet kısıtlamasının kaybolabildiği yollar ve aynı girdiye farklı sonuç veren kurallar bulundu.

Öncelik kullanıcı kararları → iskelet **0.9** → görev belgeleri olarak uygulandı. Eski 01–09 bölümleriyle farklar bulgu sayılmadı. Hiçbir dosya değiştirilmedi, kurulum yapılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `docs/sozlesmeler/A4-port.md` §3.4, §3.8, K104, K91; `core/include/ida/port.h` — `IDA_FC_SELECT`, `ida_fc_cmd_t`, `ida_event_t`, `ida_fc_status_t` ↔ `docs/sozlesmeler/A1-kart-baglantisi.md` §3.6–3.7, K64 | A4 hâlâ hedef seçimini, anlamı uygulamaya bırakılmış `arg0/arg1`, `fc_ref` ve `ext_len/ext[64]` alanlarını kabul ediyor. A1 bunları özellikle kaldırmış veya sıfıra kapatmış. Ayrıca `docs/sozlesmeler/A6-kayit.md` §3.4, K52/K54 bütün port çıktısını kaydettiğinden opak FC alanları A1’i kullanmadan kayda giriyor. | A4 belgesini ve başlığını A1’in kapalı listesine eşitleyin; hedef seçimini ve opak FC alanlarını kaldırın, ayrılmış argümanları doğrulanabilir biçimde sıfıra kapatın. A6 kodlamasını bu temizlenmiş tanımdan üretin. | kritik |
| 2 | A4 §3.4, K51; `port.h` — `IDA_MI_TARGET_ALIGN`, `ida_mission_item_t` ↔ A1 Tablo 3.6a, K122 ↔ `docs/sozlesmeler/A5-senaryo.md` §3.10, K55–K57; `scenarios/sema/a5.schema.json` — `$defs/gorev`, `$defs/gorev_cozum` | `TARGET_ALIGN` A4 ve A5’te duruyor, A1’de reddediliyor. A4’ün `p[4]` anlamları uygulamaya bırakılmış; A1 her türü kapatmış. A5’in bazı geçerli görevleri A1’in zorunlu alanlarını sağlayamıyor; tür bazındaki farklar aşağıda listelendi. | A4 ve A5’i A1’in kapalı görev listesine eşitleyin. Her seyir öğesinin alan eşlemesini, varsayılanlarını, kullanılmayan alanlarını ve sınırlarını açıkça tanımlayın; A1’de temsil edilemeyen girdiyi senaryo doğrulamasında reddedin. | kritik |
| 3 | `docs/sozlesmeler/A3-web.md` K94–K95, K101, K118 ↔ A1 K59, §3.6; `protocol/ida.xml` — komut mesajları ↔ `port.h` — `ida_cmd_t` | Revizyon denetimi komut emülatöre verilmeden yapılıyor. Emülatörde bekleyen gevşetici komutun revizyonu A1/A4’e taşınmıyor. Örneğin onaylanmış `ARM` geciktirilirken yeni `DISARM` uygulanabilir; eski `ARM` daha sonraki çağrıda yeniden silahlandırabilir. A4’ün aynı çağrıdaki daraltma kuralı bu ardışık çağrı durumunu çözmez. | Bekleyen gevşetmenin yetki bağını emülatör ve ertelenmiş komut kuyrukları boyunca koruyun; araya giren kısıtlama eski gevşetmeyi kalıcı olarak iptal etsin. Denetimin hangi teslim/uygulama sınırında yapıldığı A1–A3–A4’te aynı olmalı. | kritik |
| 4 | A1 K106; `ida.xml` — `IDA_SAFETY.safety_rev`, `IDA_EVENT.safety_rev` ↔ A3 §3.4, K117–K118; `docs/sozlesmeler/sema/a3-kanallar.json` — `yuk_emniyet_girdiler`, revizyon alanları | Aynı sayaç A1’de doyan `uint32`, A3’te artan `u64`. A3 şemaları `4294967296` değerini kabul ediyor; A1 bunu taşıyamıyor. A3’te `0xFFFFFFFF` sonrası gevşetmeleri kalıcı reddeden kural da yok. | W1-çapraz ve A1-K106’daki tanımı kaynak alın: başlangıç 1, `uint32`, süreç boyunca sıfırlanmama, doygunluk ve doygunluk sonrası bütün gevşetmelerin reddi. A3/A6 ortak yüklerini ve JSON sınırlarını birlikte değiştirin. | kritik |
| 5 | A1 K89/K106 ↔ A3 K117 ↔ A5 K62–K63 | Artış listesi tek değil. A1-K106 A5 kaynaklı kısıtlamayı sayıyor, fakat fiilî gevşetmeyi yalnız A3 kaynağıyla yazıyor; A3-K117(c) senaryo değişimini de sayıyor. Ayrıca A1-K89’un tüp başına kapanmada artış ifadesi, tek komutun K106’daki artışına ek mi olduğu konusunda belirsiz. İki tüpü kapatan tek acil durdurma farklı uygulamalarda farklı revizyon üretebilir. | Tek olay/artış tablosu oluşturun. Zamanlı ve koşullu A5 atamalarını, durumu değiştirmeyen kısıtlamayı, iki tüpü etkileyen tek girdiyi ve örtüşen kapanış nedenlerini açıkça sayın; K89 bağımsız ikinci artış tanımlamasın. | kritik |
| 6 | A1 K85 ↔ A3 K95 | A1 bütün sanal girdi değişikliklerini olaylardan önce; A3 gevşetmeleri olaylardan sonra işler. Somut fark: revizyon 7 ve acil durdurma basılıyken aynı sınıra `SAFETY_CLOSE` ile revizyon 7’ye bağlı bırakma gelsin. A1 sırasıyla bırakma uygulanır, sonra kapama gelir: girdi serbest, revizyon 9. A3 sırasıyla kapama revizyonu 8 yapar, bırakma reddedilir: girdi basılı kalır. | Tek fizik sınırı sırası yazın. A3’ün kısıtlayanlar → olaylar → revizyonu yeniden denetlenen gevşetenler sırasını A1’e işleyin; sürekli çıktılar ve izin süre dolumlarının bu sıradaki yeri de açık olsun. | kritik |
| 7 | A5 K62–K63, K71, K109 ↔ A3 K95/K118 ↔ A1 K85/K106 | A5’in çelişki kuralı senaryo atamalarını çözüyor; panel değişikliğiyle ortak hakemliği ve otomatik gevşetmenin revizyon bağını tanımlamıyor. Koşullu bir “acil durdurmayı bırak” ataması ile panelin aynı sınırdaki “bas” komutunun hangi ortak kuralla sonuçlanacağı kapalı değil. A5 gevşetmesinin hangi anda revizyon aldığı da yok. | A5 atamalarını ortak emniyet işlemine bağlayın. Panel–senaryo çakışmasını, gevşetmenin revizyon bağını, reddini ve sayaç etkisini tanımlayın. Yalnız dosya içindeki `atama_celiskisi` çözümü yeterli değil. | kritik |
| 8 | A3 §3.4 `yuk_emniyet_girdiler`, `yuk_firlatici`, K89/K93; A3 şeması — `girdi_adi`, emniyet istek/yanıtları ↔ A1 `IDA_SAFETY.tube[2]`, A4 `ida_safety_t`, A5 §3.11/K65 | A1/A4/A5 fiş ve pimi tüp başına tanımlıyor. A3 bunları tek skaler değerle gösteriyor; değiştirme ve onay isteklerinde tüp seçimi yok. `f1` pimi takılı, `f2` pimi çıkık durumu temsil edilemiyor. A6 aynı ortak yükü kullandığından değişim kaydı da eksik kalıyor. | A3’ün durum, komut, jeton bağları ve olaylarına tüp kimliğini ekleyin; A6 ortak yüklerini birlikte eşitleyin. Tek değerin iki tüpe nasıl uygulanacağı varsayılmamalı. | kritik |
| 9 | A1 K31/K40/K46 ↔ A4 K102/K113/K116 | Kısıtlayıcı örnek çekirdeğe ulaşmadan kaybolabiliyor. A1 eski/yinelenen TF’nin bütün üyelerini atıyor; emniyet örneği için istisna yok. Birleştirmede de ilk partinin 16 örneği kapasiteyi doldurursa sonraki partinin acil durdurma örneği A4-K102 ile düşebilir. K116 yalnız kendisine ulaşan örnekleri korur. | Geçerli oturumdaki, yapısal olarak okunabilir emniyet kısıtlamasını parti reddi ve kapasite düşürmesinden önce koruyun; sonraki çağrıya kayıpsız aktarılmasını A1 ve A4 birlikte tanımlasın. CRC/oturum denetimlerinin aşılması gerekmez. | kritik |
| 10 | A4 K116/K113/K45; `docs/sozlesmeler/inceleme/A4-yanit-2.md` “Daraltıcı içeriğin tanımı” ↔ iskelet §3.2 | Son turdaki geçiş tanımı ilkeyi daraltmış. Son kabul edilen örnek `estop=1` olsun. Aynı çağrıda bunun bayat yinelemesi ile yeni `estop=0` örneği gelsin. K116 yalnız son kabul edilen değere göre **0→1** aradığından bayat `1` kısıtlama sayılmaz; bayat örnek reddedilir, yeni `0` kabul edilir. Reddedilen kısıtlayıcı içerik kaybolur. | Aynı çağrıdaki kısıtlayıcı içerik ile “önceki kabul edilen değere göre yeni kapanış olayı” kavramlarını ayırın. Yinelenen kapanış için yeniden olay üretmeme, aynı çağrıdaki gevşetmeye üstün gelmesini kaldırmamalı. | kritik |
| 11 | A4 §3.8, K66/K107; `port.h` — `IDA_APPLY_BY_*`, `IDA_HOLD_*` ↔ A1 §3.9, K104 ve olay doğrulaması | A4, `apply_by=1 ms` ve `hold=1 ms` değerlerini geçerli çıktı sayıyor. A1 sırasıyla en az 16 ms ve 60 ms istiyor. `hold=500 ms` de A4’te geçerli, A1’de geçersiz. Bağımsız yazılmış uygun bir çekirdek, taşımanın reddettiği olay üretebilir. | A4 parametre/çıktı doğrulamasını A1’in bağlayıcı aralıklarına eşitleyin: `apply_by` 16–1000 ms, `hold` 60–450 ms. Genel depolama kapasitesiyle bu uygulamanın geçerli aralığını ayırın. | kritik |
| 12 | A1 K31/K66 ↔ A4 Bölüm 2, K25 | A1 “boşluğun büyüklüğüne üst sınır yoktur” diyor; kullanılan 32 bit seri karşılaştırma bütün farkları sıralayamaz. `a=0x80000000`, `b=0` tam yarım uzay farkıdır; tanımlı büyük/küçük sonucu yoktur. [RFC 1982 §3.2](https://www.rfc-editor.org/info/rfc1982/) bunu açıkça sınırlar. | Karşılaştırılabilir pencereyi ve yarım uzay/büyük sıçrama davranışını tanımlayın; “üst sınır yok” ifadesini kaldırın. Oturum ve komut kimliği uzlaştırması aynı sınırı kullansın. | kritik |
| 13 | `docs/sozlesmeler/A2-gorsellestirici.md` §3.6/§3.8; `sim/include/ida/a2_wire.h` — `kalibrasyon_surumu`, `pozlama_ns` ↔ A8 §3.2/§3.5, K27 — `calib_ver`, `window_ns` | A2 kalibrasyon sürümünü `u32`, A8/A1 algılama kaydı `u16` taşıyor; `65536` için eşleme yok. A2 pozlamayı pozitif çift `i64` kabul ediyor; A8 en çok 1 s kabul ediyor. Örneğin A2’de geçerli 2 s pozlama A8’de `TIME` reddi doğurur. | Ortak kamera yapılandırmasının geçerli aralığını üretimden önce daraltın veya sürümlü bir eşleme tanımlayın. Sessiz tamsayı daraltması yapılmamalı. | kritik |
| 14 | A6 §3.5, §10 “A1’den beklenen”; A6 şeması — `a1_sema_ozeti` ↔ A1 §3.2/K14; ayrıca A3/A6 `yuk_algilama_kayit.a8_surumu` ↔ A8 el sıkışması | A6 zorunlu üst verisi A1’de bulunmayan `hash_base/hash_full` bekliyor; A1 tek `dialect_hash` tanımlıyor. Algılama yükündeki tek `u16 a8_surumu` için de A8’in `major/minor` çiftinin kodlama kuralı yok. | A6’yı gerçek A1 özet alanına ve gösterimine bağlayın. A3/A6’daki A8 sürüm alanının hangi sürümü, nasıl kodladığını açıkça tanımlayın. | önemli |
| 15 | A4 §3.8/K90 — `ida_log_t.code/arg0/arg1`, FC’ye ayrılan `reason ≥ 0x100` ↔ A1 §3.7 `IDA_LOG`, `IDA_CMD_RESULT` ↔ A6 K54 | Opak FC uzantıları dışında da semantiği sonraki uygulamaya bırakılmış yollar var: günlük kodu/argümanları ve FC ret kodları. Bunların FC kullanımlarının yalnız emniyet tanılamasıyla sınırlı olduğu kapalı bir katalog veya açık yasakla bağlanmamış. Yasak içeriğin mevcut olduğuna dair kanıt değil, kapsam sınırının açık kalan kanalıdır. | FC kullanımında izin verilen emniyet tanılarını ve alan anlamlarını kapatın; hedef seçimi, hedefleme çözümü ve atış ölçütü taşınamayacağını bu kanallarda da bağlayıcı yapın. | önemli |

## Yeniden hesaplananlar

**Geliş sıraları.** A1/A3/A5 dünyanın yetkili durumunu, A4 bunun çekirdeğe teslim edilen karşılığını düzenler. “Tanımsız” hücreler, eşit sonucun sözleşmeden çıkarılamadığını gösterir.

| Sıra | A1 sonucu | A3 sonucu | A4 sonucu | A5 sonucu |
|---|---|---|---|---|
| 1. Aynı sınırda kısıtla + gevşet; iki geliş sırası da denenir | K85 bunların kendi iç sırasını kapatmıyor; birleşik sıra bulgu 6’ya bağlı. | Kısıtlama uygulanır, gevşetme reddedilir. | Daraltıcı mandal + genişletici aynı çağrıdaysa genişletici reddedilir. | İki senaryo atamasından en az biri koşulluysa kısıtlayan değer kazanır; iki zamanlı çelişki yüklemede reddedilir. |
| 2. Revizyon 7’de gevşetme alınır; durumu değiştirmeyen kısıtlama N’de revizyonu 8 yapar; eski gevşetme N+1’de gelir | Revizyon 7 damgalı izin olayı reddedilir. | Eski istek/jeton reddedilir. | Komut revizyonu yoktur; eski komut yeni çağrıya ulaşırsa bu bayatlık tanınmaz. | Atamanın revizyon bağı tanımsızdır. |
| 3. Son durum açık; bayat örnekte kapanma, yeni örnekte açılma; aynı çağrı | Eski TF’ye bağlı kapanma çekirdeğe ulaşmadan atılabilir. | Yapısal olarak geçerli kısıtlama bayat revizyon nedeniyle reddedilmez. | İki örnek ulaşırsa K116 kapanmayı korur, genişletme engellenir. | Koşullu senaryo çelişkisinde kapanma kazanır. |
| 4. Son durum zaten kapalı; bayat kapalı örnek + yeni açık örnek | İletim sonrası sonuç A4’e bağlıdır. | Durumu değiştirmeyen kısıtlama da gevşetmeyi düşürür. | K116’nın geçiş tanımı nedeniyle açık duruma geçebilir: bulgu 10. | Aynı alanın koşullu atama çelişkisinde kapalı değer kazanır. |
| 5. Koşullu senaryo bırakması + panelin aynı sınırdaki acil durdurması | Ortak alt sıra/arbitraj kapalı değildir. | Kısıtlama üstün olmalı; senaryo gevşetmesinin revizyon girdisi tanımlanmamış. | Kapanma ve açılma aynı çağrıya ulaşırsa kısıtlama korunmalı; önceki durum kapalıysa bulgu 10 geçerlidir. | K63 panel komutuyla ortak uygulamayı tanımlamaz: bulgu 7. |
| 6. Revizyon 7, acil durdurma basılı; kart `SAFETY_CLOSE` olayı + panel bırakması | Bırakma önce: **serbest, revizyon 9**. | Kapama önce: bırakma reddi, **basılı, revizyon 8**. | Farklı dünya durumu sonraki emniyet örneğine yansır. | Yeni atama yok; bu sıra için ek çözüm sağlamaz. |
| 7. Onaylı `ARM` emülatörde bekler; sonra `DISARM` uygulanır; bekleyen `ARM` teslim edilir | Eski yetki damgası taşınmadığından komut iletilebilir. | İlk kuyruk denetimi geçmiştir; emülatör içindeki iptal tanımsızdır. | Ayrı çağrıda gelen `ARM`, aynı çağrıdaki daraltma kuralıyla engellenmez. | Bağlantı gecikmesi bu sırayı mümkün kılar; yetki iptali kuralı eklemez. |

**Görev öğeleri arasındaki farklar:**

| Tür | A1’in kapalı tanımı ↔ A4/A5 farkı |
|---|---|
| Ortak | A4 bütün `p[]` anlamlarını erteliyor; A1 tür başına anlam, aralık, izin verilen `frame`, kullanılmayan konumların sıfırlanması ve `depth` sınırını bağlamış. A5’te bunların tamamına ait dönüşüm yok. |
| `WAYPOINT` | A5 `hiz=4 m/s` değerini kabul eder; A1 üst sınırı 3 m/s. Kabul yarıçapı ve konum sınırları da eşit değil. |
| `DIVE_TO` | A5 yalnız derinliği zorunlu tutar; A1 ayrıca dikey hız ve tolerans ister. A5’te derinlik 0 geçerli, A1’de geçersizdir. |
| `SURFACE` | A1’in zorunlu pozitif çıkış hızının A5 kaynağı/varsayılanı yoktur. |
| `HOLD` | A5 süreyi ns ve ≥0 kabul eder; A1 saniye cinsinden >0 ve ≤3600 ister. Konum toleransının kaynağı tanımlı değildir. |
| `SCAN_LINES` | A5 iki köşe, hat aralığı ve hız taşır; A1 başlangıç, hat boyu, hat sayısı ve rota taşır. Dönüşüm ve A5 hızının karşılığı tanımlanmamıştır. |
| `SCAN_POINT` | A5 tur sayısı, A1 süre ve konum toleransı taşır; eşleme yoktur. |
| `WAIT_GNSS` | A5 zorunlu ek alan istemez; A1 pozitif bekleme süresi ve en az fix türü ister. |
| `TARGET_ALIGN` | A4/A5’te vardır; A1’de kapalı listenin dışındadır ve reddedilir. |

**Sayısal denetimler:**

- `2⁶² ns = 4 611 686 018 427 387 904 ns ≈ 146,14 yıl`. A5’in JCS için seçtiği `2⁵³−1 ns ≈ 104,25 gün` dosya sınırı daha dardır; aynı sayı tipi kapasitesi değildir. JSON’un binary64 sınırı birincil [RFC 8785 §3.1](https://www.rfc-editor.org/rfc/rfc8785.html#section-3.1) ile doğrulandı.
- Emniyet sayacının son değeri **4 294 967 295**. A3’ün kabul ettiği bir sonraki değer A1’in `uint32` alanına sığmaz.
- A1 E9: `60/2 − (10+5+14,5) = 0,5 ms` pay. Aritmetik doğru; 14,5 ms gözlenen gecikmedir, garantili üst sınır değildir.
- A1 E8: `2×35 + 5 + 14,5 + 0,86 = 90,36 ms ≤ 95 ms < 100 ms`. Aritmetik doğru.
- A1 uçtan uca bütçe toplamı **9,7333 ms**, 10 ms’ye kalan pay **0,2667 ms**. Bu hesap ölçüm değildir.
- `event_id = (5 << 32) | 3 = 21 474 836 483`. A1 XML ve A4’ün 32+32 bit düzeni uyumludur.
- `IDA_EVENT` yükü **37 B**, imzasız MAVLink çerçevesi **49 B**. MAVLink alan sıralaması ve 12 B ek yük birincil [serileştirme belgesiyle](https://mavlink.io/en/guide/serialization.html) doğrulandı.
- 32 bit seri uzayın yarısı **2 147 483 648**; 200 Hz’te yaklaşık **124,28 gün**. Bu fark için sınırsız sıralama iddiası geçerli değildir.
- A8’in `2³²/(30×4)` hesabı **414,25 gün**: aritmetik doğru; sarma davranışının yerine geçmez.
- A2/A7 matrisleri yeniden çarpıldı: `C·M = diag(−1,1,−1)`; iki dönüşümün determinantı +1.

**Çelişki bulunmayan denetimler:**

| Denetim | Sonuç ve dayanak |
|---|---|
| Kısıtlayan yönler | **uyumlu** — A3 K89, A5 K63 ve A1/A4 bit anlamlarında acil durdurma basılı, fiş çıkık, pim takılı yönleri aynı. |
| Sayacın sahibi ve başlangıcı | **uyumlu** — A1 K106 ve A3 K117 dünya sahipliği, başlangıç 1 ve yeni çalıştırmada sıfırlanmama konusunda aynı; genişlik ve artış farkları bulgu 4–5’te. |
| Kartın izin yenilemesi | **uyumlu** — A1 K106 ve A3 K117, `SAFETY_OPEN` uygulamasının sayacı artırmadığını söylüyor. |
| İki saat ve izin yenileme ilkesi | **uyumlu** — A1 §3.9/K86 ile A4 K66/K107 üreticide duvar saati, alıcıda uygulama anından sim zamanı ve en geç `hold/2` yenileme düzeninde aynı; geçerli aralıklar bulgu 11’de. |
| Zaman tipi ve birimi | **uyumlu** — A1/A2/A3/A4/A6/A8 zaman damgalarında işaretli 64 bit ns kullanıyor; A5 dosya tamsayılarını ayrıca sınırlandırıyor. A7’nin ilgili geometri alanlarında zaman damgası yok. Bütün alanların aynı üst sınırı kullandığı söylenemez. |
| Çerçeveler ve fiziksel birimler | **uyumlu** — NED, gövde ileri/sancak/aşağı, Hamilton `(w,x,y,z)`, A8 sensör çerçevesi ve A2/A7 dönüşümleri aynı zinciri oluşturuyor. Godot/glTF eksen kabulleri birincil [Godot](https://docs.godotengine.org/en/latest/tutorials/assets_pipeline/importing_3d_scenes/model_export_considerations.html) ve [Khronos](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html) belgeleriyle karşılaştırıldı. |
| Olay kimliği ve daraltıcı mandallar | **uyumlu** — A1/A4 olay kimliği 64 bit; açılış ve olay sayaçları 32’şer bit. Dört komut mandalının bitleri aynı. Emniyet örneklerinin kaybı ayrı olarak bulgu 9’da. |
| A3/A6 ortak kodlama | **uyumlu** — 19 ortak yük tanımı derin eşit; emniyet ve atış durumu yükleri de aynı. Bulgu 8’in temsil eksikliğini ikisi birlikte taşıyor. |
| A2/A7/A8 kapsam sınırı | **uyumlu** — A2’nin sahne/isabet verisi görselleştirme amaçlı; A7 geometri sağlar. A8 ölçüm kaydı taşır ve K52–K55 ile hedef listesi, kart durumu ve yer gerçeği girdisini dışlar. Bu kısımlarda yeni hedef seçimi/atış çözümü alanı saptanmadı. |
| Kalite kademesinde emniyet | **uyumlu** — iskelet §6, A3 §6, A5 K81 ve A6 K18 emniyet davranışı/kaydının kademeyle kaldırılmasına izin vermiyor. |

## İskelete ya da diğer bölümlere notlar

**İskelet kapsamı — kritik:** `docs/mimari/00-iskelet.md` §2’deki atış kontrol sorumluluğu hâlâ “takip, atış çözümü” içeriyor; §9’daki 03 satırı atış ölçütü tanımlıyor. §5’teki “Bilinen sınır” açıklaması da hedefe yaklaşma davranışı tarif ediyor. Bunlar bu incelemenin bağlayıcı emniyet/arayüz sınırıyla uyumlu değil. İskelet bu içerikleri uygulama gereği olarak taşımamalı; hedefleme ayrıntısı önerilmedi.

**İskelet sayaç ifadesi — önemli:** §3.2 “emniyet yetkisini değiştiren her olay” için artış söylüyor. A1 K106/A3 K117 ise izin açma ve yenilemeyi özellikle hariç tutuyor. Bu istisna iskelette açıkça yazılmalı; mevcut genel ifade iki farklı uygulamaya izin veriyor.

**İskelet saat ifadesi — önemli:** §4 emniyet sürelerini genel olarak duvar saatine bağlıyor; A1/A4 alıcıdaki izin ömrünü sim zamanında sayıyor. Üretici ve alıcının ayrı saat alanları iskelette de ayrılmalı.

`docs/gorevler/W1-ortak.md` hâlâ iskelet 0.8’e atıf veriyor. Bu incelemede kullanıcının açık talimatıyla 0.9 esas alındı.

Yerel inceleme kaynaklarına erişildi. C17 N2176 taslağının birincil PDF’si web aracıyla açılamadı; C17 tamsayı dönüşüm davranışı doğrulanmış sayılmadı. Seri aritmetik bulgusu bağımsız olarak erişilen RFC 1982’ye dayanıyor.