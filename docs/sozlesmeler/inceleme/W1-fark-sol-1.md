# sol incelemesi: sozlesmeler

## Hüküm

**Dondurulamaz.** Üç kritik, üç önemli bulgu var. Önceki düzeltmelerin çoğu kapanmış; ancak kartın açık partisinde bekleyen genişletici için emniyet boşluğu kalıyor. Yeni çıktı ve ret kuralları da bütün durumlarda tek anlamlı değil.

İnceleme `40b8d63..6b63c6e` farkıyla, A1/A4 ve bunları etkileyen karşı hükümlerle sınırlandı. Öncelik kullanıcı kararları → iskelet 0.11 → `W1-uyum.md` olarak uygulandı. Dosya değiştirilmedi, kurulum yapılmadı.

Aşağıda A1 ve A4, sırasıyla `docs/sozlesmeler/A1-kart-baglantisi.md` ve `docs/sozlesmeler/A4-port.md` belgeleridir.

**Önceki doğrulama bulgularının kapanışı**

| Bulgu | Hüküm | Baktığım yer | Kalan eksik |
|---|---|---|---|
| sol-1 / 1 — Bekleyen genişletici | KISMEN | A1-K129/K63/K117; A3-K101; A4-K18/K46; iskelet §3.2 | Sim gönderim kuyruğu kapanmış. Kartın açık partisindeki bekleme için bulgu 1 kalıyor. |
| sol-1 / 2 — Komut kimliği uzlaştırması | KISMEN | A1-K66/K123, §9.2 | Yarım uzay karşı örneği düzelmiş. İlk seçimde sıfırın atlanması bütün dallarda sağlanmıyor; bulgu 5. |
| sol-1 / 3 — E20 pencere boyu | KAPANDI | A1 E20; `protocol/ida.xml` | 54 B ve 2,70 ms hesabı doğru. Kapasite güvencesi koşullu; ölçüm kapanmış sayılmadı. |
| sol-1 / 4 — Taşınan emniyet örneği | KAPANDI | A1-K31/K124; A4-K33/K34/K115/K116/K130 | Eski karşı örnek artık gevşetemiyor. Yeniden teslim süresine ilişkin yeni iddia bulgu 6’da. |
| sol-1 / 5 — Olayda tüp zorunluluğu | KAPANDI | A3 şeması: `olay_emniyet_girdisi_degisti`, `girdi_secimi`; A3-K96 | `allOf` bağı fiş/pimde tüpü zorunlu, acil durdurmada yasak yapıyor. |
| sol-1 / 6 — Eski alan ve atıflar | KAPANDI | A1 §3.7, Ek A, §8.2 | Kaldırılan alanlar artık mevcut üyeler gibi sunulmuyor. |
| sol-2 / Y1 — Hata yanıtı–olay bağı | KAPANDI | A1-K73/K84, Tablo 4.6 V3 ve 4.6b; A4-K57, §4.14 | Eski parti numaralı olay yeni yanıt yuvasını kilitlemiyor. Bekletilmiş olayların çıktı tablosundaki ayrı sorunu bulgu 2. |
| sol-2 / Y2 — E20 | KAPANDI | A1 E20/K124 | sol-1 / 3 ile aynı kapanış. |
| sol-2 / Y3 — `PARAM_GET` seçicisi | KAPANDI | A4 §3.14/K43; `port.h`; A6-K52 | GET yalnız `id` taşıyor; kullanılmayan `type/v` okunmuyor. |
| sol-2 / Y4 — Sonuç tablosu | KAPANDI | A4 §3.5/K67/K106; A1-K80/K120 | Kova boşluğu ve `apply_by + 95 ms` ayrımı eşitlenmiş. |
| sol-2 / Y5 — Eski süre aralıkları | KAPANDI | A1 §3.7/§3.9/K104; A4-K58/K66 | Ortak aralıklar aynı. |
| sol-2 / Y6 — Oturum sonu izi | KAPANDI | A1-K11/K22/K95/K127 | Gönderim sırası, en iyi çaba ve kayıp istisnası tanımlı. |
| sol-2 / Y7 — Kaldırılmış alanlar ve `SELECT` sınaması | KAPANDI | A1 §3.7/Ek A; A4 §7/K119 | Eski kabul edilebilir `SELECT` işlemi sınamadan çıkarılmış. |
| sol-2 / Y8 — Bileşen sayaçları | KAPANDI | `W1-uyum.md` D-16; A8-K30 | Daraltma açık mimar kararı olarak kaydedilmiş. |
| sol-2 / M1 — Yenileme ve sonuç bekleme | KAPANDI | A4-K107/K123 ve §7; A1 E9/§3.9 | Kesintisiz yenileme iddiası koşullandırılmış; gecikmede güvenli süre dolması tanımlı. |
| sol-3 / D8 — Etiket parametreleri | KAPANDI | A1-K114; A5-K118/Ek D.7 | Başlangıç, artış, yeniden gönderim ayrımı ve ömür tanımlı; vektörler eşleşiyor. |
| sol-3 / D9 — Eski `SELECT` sınaması | KAPANDI | A4 §7/K119 | Kapalı listeyle uyumlu. |
| sol-4 / Y9 — Komut kataloğu | KAPANDI | A1-K125; `protocol/komutlar.json` | Yinelenen öğeler ve katalogdaki negatif derinlik/irtifa sorunu düzelmiş. Yeni A4 ret tablosuyla çelişki bulgu 3. |
| sol-4 / Y8 — A1 payı | KAPANDI | A1-K43/§6; A6-K28 | Ölçme ile kayda yazma ayrılmış; `olaylar` istisnası açık. |

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | A1-K129/K63/K28/K117 ↔ A4-K18/K46/K45; iskelet §3.2 | **Dördüncü bekleme noktası: kartın açık partisi.** Son TF kartta 0 ms’de alınsın. Ardından `ARM` 1 ms’de açık partiye girsin, `DISARM` 3 ms’de mandala alınsın. Sonraki TF gecikirse K117 uyarınca 6 ms’de ızgara tick’i başlayabilir; bu tick `DISARM` mandalını uygular ve temizler. TF 7 ms’de, tick bittikten sonra gelsin: açık partideki `ARM` şimdi mandalsız çağrıya girer ve yeniden silahlandırabilir. FIFO korunmuştur, fakat **uygulama sırası korunmamıştır**. K63’ün “açık partideki genişleticiler mandalla aynı çağrıya girer” varsayımı burada yanlış. | Daraltıcı mandala alınırken açık ve henüz çağrılmamış partilerdeki eski genişleticilerin de iptalini tanımlayın. Yalnız erteleme tamponunu temizlemek yeterli değil. Bu sıra kabul sınamasına eklenmeli; tel biçimi değişmesi gerekmiyor. | kritik |
| 2 | A4 §3.11 (b2), §4.15, K63/K125 ↔ §4.14 ve K28 | **Bekletilmiş kapanmanın hata çıktısında bulunması iki türlü okunuyor.** Bir `out = NULL` çağrısında `SESSION_END` kapanması oluşsun. Sonraki yazılabilir çağrı yeni daraltma taşımadan `DUP_SEQ` dönsün. (b2), K63 ve K125 kapanmanın bu çağrıda verilmesini ister. §4.14 ise `DUP_SEQ` için `BAD_INPUT` satırına gider; o satır yalnız çağrının “(b) daraltmasından doğan” kapanmayı listeler. K28 çıktının geri kalanını bu tabloya bağlar. Bir uygulayıcı bekletilmiş olayı çıkarırken diğeri hata satırında olay olmadığını okuyabilir. | Hata satırlarının olay hücresine **önceki (b2) çağrılarından bekletilen kapanmaları** açıkça ekleyin. Bunların birleştirme, `seq/t_decide` ve tek teslim kuralları aynı yerde bağlansın. | kritik |
| 3 | Yeni A4 §3.17/K124 ↔ A4 §3.4/K50; A1-K125 ve `protocol/komutlar.json` | **Kapalı ret tablosu geçersiz sayısal girdiyi kabul ettiriyor.** `GUIDED` kipinde, yeni kimlikli `GUIDED_SP`, `mask = DEPTH`, `depth = -1` verilsin. Maske sıfır değil, değer sonlu, kip doğru: §3.17’ye göre hiçbir ret koşulu tutmaz ve `ACCEPTED` çıkar. Oysa §3.4 ve yeni K125 derinliği ≥0 ister. Aynı sorun `altitude = -1` ve `YAW` maskesiyle `yaw = 4` için var. A1 Tablo 3.6 bu mesajda dönüşüm reddi tanımlamıyor; katalog denetimi çekirdek sınırını tamamlamıyor. Yeni K124 ayrıca tabloda olmayan koşuldan ret üretmeyi yasaklıyor. | §3.17 ve K50’yi mevcut alan aralıklarının tamamına bağlayın; maskeli alanın aralık ihlalini mevcut `OUT_OF_RANGE` sonucuna dahil edin. Yeni neden kodu gerekmiyor. | kritik |
| 4 | Yeni A4-K46 ↔ §3.17 giriş sırası ve `FC` ortak satırı; K48/K119 | **Daraltıcıların savunmacı işlenmesinde sonuç önceliği eşitlenmemiş.** K46, yanlışlıkla `cmds[]`’te gelen daraltıcıyı yineleme penceresinden muaf tutup `ACCEPTED` yanıtlar. §3.17’nin ilk adımı ise her yinelenen kimliğe `DUPLICATE` verir. Ayrıca `FC_ABORT, tube = 255` yanlışlıkla `cmds[]`’teyse K46 mandal gibi uygulayıp kabul ederken §3.17’nin “FC her işlem” satırı tüpü 0–1 dışında diye reddeder. | (b)’de işlenen daraltıcıların sonuçlarını genel yineleme ve FC alan denetiminden açıkça ayırın. §3.17’de K46 istisnasının önceliğini belirtin. | önemli |
| 5 | A1-K66 ↔ §9.2 kapanış iddiası ve `BOARD_HELLO.last_cmd_id` açıklaması | **İlk seçimde sıfır hâlâ çıkabiliyor.** `L = 0xFFFFFFFF`, `c = 0` için fark 1’dir; ilk dal `x = c = 0` seçer. Sıfırı atlama yalnız diğer dalda ve sonraki komutlarda var. `c`’nin ilk değerinin sıfır olamayacağı önkoşulu da yazılmamış. Dolayısıyla §9.2’deki “ilk komutta da 0 atlanır” kapanış iddiası sağlanmıyor; 0 ayrıca “henüz komut yok” gösterimidir. | `c` için sıfırdan farklı başlangıç/ömrü tanımlayın veya sıfır denetimini bütün seçim dallarının sonuna uygulayın; sonuç penceresini bundan sonra doğrulayın. | önemli |
| 6 | A1-K124 son paragraf ↔ K56; A4-K102/K130 | **100 ms yeniden gönderim, 100 ms içinde kabul edilmiş teslim güvencesi değil.** Düşen gevşetmenin 100 ms’de yeniden gönderildiği parti de daha yeni partiyle birleşebilir; örnek yeniden `VALID = 0` olur. Bu her periyodik örnekte tekrarlanabilir. Taşıma gecikmesi de ayrıca sınırsızlaştırılmamış. “En geç bir `T_emniyet` sonra … `VALID = 1` ile gelir” sonucu çıkmıyor. | 100 ms’yi **yeniden üretme/gönderme aralığı** olarak ifade edin. Çekirdekte kabul için örneğin en yeni, kabul edilen partide teslim edilmesi koşulunu yazın; mevcut kurallardan koşulsuz kabul süresi türetmeyin. | önemli |

## Yeniden hesaplananlar

| Denetim | Bağımsız sonuç |
|---|---|
| `ida.xml` farkı | Açıklamalar çıkarıldığında eski ve yeni XML yapıları eşit. Mesaj, alan, ad, değer ve numaralandırma değişikliği yok. Yalnız iki neden girdisine açıklama eklenmiş. |
| Kanonik özet | **565 satır**, `dialect_hash = 0x3939788984A5FF80`; A1 ile eşit. |
| A1 Ek B | `40b8d63` ile metin olarak aynı. **38/38** çerçevenin uzunluk ve CRC denetimi geçti. |
| Başlıkların korunması | `port.h` yorumlar ve boşluklar çıkarıldığında aynı. `algilama_kaydi.h` değişmemiş. `a2_wire.h` farkı yalnız `VedaNedeni::yeni_calistirma = 5`; yapı yerleşimi değişmemiş. |
| Ayna dosyası | **21 numaralandırma, 136 girdi**. İlgili XML girdileri eksiksiz ve tekil; bütün `ida` adları başlıkta var, değerler eşit. `IDAW_REASON` kapsamı yalnız **0–15**. |
| `NOT_INIT` engel maskesi | K126’da sayılan bitlerin toplamı **`0x00026FFF`**; sabit doğru. |
| Süre parametreleri | `10000 ms × 1000000 = 10000000000 ns`; `uint32_t` ns’ye sığmaz, belirtilen `int64_t` dönüşümü uygun. |
| Parametre kapasitesi | `(8000 − 24) / 8 = 997`; K125 üst sınırı doğru. |
| E8 | `2×35 + 5 + 14,5 + 0,86 = 90,36 ms ≤ 95 ms < 100 ms`. |
| E9 | `60/2 − (10 + 5 + 14,5) = 0,5 ms`. Aritmetik doğru; sonuç bekleme ve taşıma koşullarını garanti etmez. |
| E10 | `30 + 14,5 = 44,5 ms < 60 ms`. |
| K66 eski karşı örneği | `L=10, c=2147483658` artık **11** seçiyor; fark 1. Üst geçerli sınır `c=2147483657` korunuyor; fark `2³¹−1`. `L=0xFFFFFFFF` geri dönüş dalında **1**, fark 2. `c=0` dalı bulgu 5’te. |
| A5 Ek D.7 | `board_tohum = 0xb1536801d0cf436c`; `run_id = 1059443490`; ilk sim nonce’u **3869573587**; kart nonce’u **220998108**; ilk `sess` **18101**, yasaksa sonraki çekiliş **6699**. Anahtarlar ve değerler eşleşti. |

**E20’nin tel sırasından yeniden kurulması**

`IDA_SAFETY` yükünde bayt konumları şöyledir:

| Alan | Başlangıç | Boy |
|---|---:|---:|
| `t_meas_ns` | 0 | 8 |
| `seq` | 8 | 4 |
| `safety_rev` | 12 | 4 |
| `flags` | 16 | 2 |
| `inst` | 18 | 1 |
| `estop` | 19 | 1 |
| `tube[2]` | 20 | 2 |

`safety_rev = 1`, sonraki alanlar sıfırken yük **13 B**, imzasız çerçeve **25 B** olur. `VALID = 1` olduğunda yük **17 B**, çerçeve **29 B** olur. TF’nin son alanı `run_mode` sıfır olamadığından TF **29 B** kalır.

Böylece en küçük pencere **29 + 25 = 54 B**; `300 B/ms` kapasitede **0,18 ms**’dir. E20’nin kendi modeliyle kalan 15 pencere için sınır **15×0,18 = 2,70 ms** çıkar. Bu koşullu hesap doğrudur; **3,5 ms çağrı bütçesi bu sınırı aşar**. Taşmada oturum kapatma yolu korunmuş; köprü biriktirmesine ilişkin ölçüm yapılmış sayılmadı. Alan sıralaması ve kırpma, canlı [MAVLink serileştirme belgesi](https://mavlink.io/en/guide/serialization.html) ve [pymavlink 2.4.50 sıralama kaynağıyla](https://raw.githubusercontent.com/ArduPilot/pymavlink/v2.4.50/generator/mavparse.py) karşılaştırıldı.

**İstenen sıraların yeniden yürütülmesi**

- **sol-1 / 4:** Son kabul edilen emniyet örneği `seq=10, t=1 s, estop=1` iken DUP penceresinden gelen `seq=11, t=1 s, estop=0`, artık `VALID=0` teslim ediliyor. K33 reddeder; etkin `estop=1`, kabul izi `seq=10` kalır. K115’in yeni istisnasıyla bu örnek eşit anahtar çatışmasına da girmez. Sonraki kabul edilmiş partinin kendi geçerli örneği gevşetebilir.
- **sol-2 / Y1:** Son kabul edilen TF=10 iken TF=11 çağrısı hata ve kapanma üretiyorsa yanıt **11**, olay **10** taşır. K73 artık yuva 11’i olay beklemeden tamamlar. Olay kendi TF bağı ve K84 süreleriyle değerlendirilir. Son geçerli TF yoksa `0,0` kapanması V3’ten geçer ve süre istisnasıyla yalnız kapatır.
- **sol-1 / 1:** `ARM` sim gönderim kuyruğundayken araya `DISARM` girerse K129 gönderimi engeller. Telden geçmiş `ARM` için ise bulgu 1’deki açık parti–ızgara tick sırası kalır.
- **M1:** `hold=60 ms`, sonuç 40 ms’de geliyorsa ilk 30 ms’de yenileme zorunluluğu artık koşulsuz değil. K123 beklemeyi koruyor; yenileme olmazsa izin süre sonunda kapanıyor.

**Tel alım kuralları**

K02/K05–K08 ve §3.11 aynı denetim sırasını veriyor: başlıkta uyumsuzluk bayrakları → mesaj kimliği → uzunluk; ardından CRC → kimlik çifti → yön. Başlık reddinde yeniden tarama ile CRC reddinde adayı tüketme ayrılmış. `cnt_rx_error` toplamı A4 §3.6’da aynı beş bileşeni içeriyor. `rx_seq_gap` kayıp paket miktarını değil olay sayısını sayıyor; örneğin kabul edilen `255,0,0,2` dizisi **iki** olay üretir.

K101’in belirli yardımcı işlev adlarından ayrılması uygun. Başlık reddinde doğrudan boşta durumuna dönüş ve uzunluk makrosunun davranışı, canlı [pymavlink 2.4.50 yardımcı kaynak dosyasında](https://raw.githubusercontent.com/ArduPilot/pymavlink/v2.4.50/generator/C/include_v2.0/mavlink_helpers.h) doğrulandı.

## İskelete ya da diğer bölümlere notlar

**İskelet 0.11 §3.2:** A1-K129 ile A3-K101 sim gönderim kuyruğundaki revizyon denetiminde eşleşiyor. A4-K130, A1-K31/K124’ün taşınan örneği gevşetmeme kararını karşılıyor. Üç bekleme noktasının bütün zinciri kapsadığı sonucu ise bulgu 1 nedeniyle sağlanmıyor. Mimarın kısıtlama hâkimiyeti kararı değiştirilmeden uygulama sınırı tamamlanmalı.

**Mimar kararlarının iç tutarlılığı ve kapsamı:** D-2’nin `VALID=0` yöntemi kısıtlayıcı içeriği koruyor. D-3’ün koşullu yenilemesi ve D-16’nın sayaç daraltması açıkça kaydedilmiş. Bu kararlarda ayrıca hedefleme kapsamını genişleten bir değişiklik saptamadım.

Yeni A4 §3.17; hedef seçimi, hedefleme çözümü, menzil/yön ölçütü veya opak veri kanalı içermiyor. FC satırları yayımlanmış emniyet durumları ve ret kodlarıyla sınırlı. `fc_fire` kataloğu yalnız zorunlu `tube:0..1` parametresini kabul ediyor ve ek özelliklere kapalı. Kaldırılan FC alanlarının aktif tanımlara geri eklendiği bir yer bulunmadı.

Birincil web kaynaklarına erişildi. Gerçek üreteç başlıkları yeniden üretilip derlenmedi; kart, Windows ve uçtan uca zamanlama ölçümü yapılmadı. Sayısal denetimler bellek içinde, standart kitaplıkla yapıldı; çalışma ağacı temiz kaldı.

dondurulamaz