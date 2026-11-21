# A3 — Sim sunucusu ↔ web panelleri

| | |
|---|---|
| Sözleşme | A3 |
| Sürüm | 1.0 (donduruldu, 2026-11-21; tel sürümü `1.0`) |
| Durum | **Donduruldu (2026-11-21; kullanıcı kararı K20).** Fark incelemesi turunun (D-17…D-27) düzeltmeleri sol'a yeniden incelettirilmedi; kalan hatalar uygulama sırasında bulunur ve küçük sürüm artışıyla (1.0.x) düzeltilir. |
| Tarih | 2026-11-19; düzeltme turu 1: 2026-11-20 (`docs/sozlesmeler/inceleme/A3A6-yanit.md`); düzeltme turu 2: 2026-11-20 (`docs/sozlesmeler/inceleme/A3A6-yanit-2.md`); çapraz uyum turu: 2026-11-20 (`docs/gorevler/W1-uyum.md`, §11) |
| Görev | `docs/gorevler/W1-A3A6.md` |
| Makinece okunur parça | `docs/sozlesmeler/sema/a3-kanallar.json` |
| Dayanak | `docs/mimari/00-iskelet.md` sürüm 0.10 (§2, §3, §3.1, §3.2, §4, §5, §6, §10); `docs/gorevler/W1-uyum.md` (U1–U18); `docs/mimari/08-laboratuvar-uygulamasi.md` (taslak) |
| İlgili sözleşme | A6 (`docs/sozlesmeler/A6-kayit.md`); A1, A2, A5, A8 (ayrı işler) |

Bu belge kural yazar. Gerekçe ve tasarım 08 numaralı bölümdedir; 08'deki bir şey bu belgeye girmediyse bağlayıcı değildir. 08 ile iskelet 0.10 çeliştiğinde iskelet uygulanmıştır (8. başlıkta sapmalar listelenir). Makinece okunur dosya bu belgenin parçasıdır; ikisi çelişirse bu bir hatadır ve düzeltilene kadar dosya geçerli değildir.

Sayıların dayanağı her yerde yazılıdır. "Mimari seçimi (tahmin)" etiketli değerler ölçülmemiştir; 8. başlıkta hangi ölçümle kesinleşecekleri yazılıdır.

---

## 1. Kapsam ve taraflar

**Sunucu (üretici):** sim sunucusunun API bileşeni (`sim/` altında, C++20). Dünyanın tek yetkili durumu sim sunucusundadır (iskelet §2); API bileşeni durumu yalnız okur ve komutları dünyanın komut kuyruğuna verir.

**İstemci (tüketici):** tarayıcıda çalışan web panelleri (TypeScript; K9) ve aynı protokolü konuşan yerel araçlar (test betikleri, komut satırı araçları). Bu sözleşme istemci türleri arasında ayrım yapmaz.

**Kapsam:** taşıma ve erişim koruması; sürüm el sıkışması; ikili telemetri çerçevesi ve kanal kataloğu; abonelik, seyreltme ve yavaş istemci davranışı; HTTP kontrol uçları (çalıştırma kontrolü, senaryo, hata enjeksiyonu, sanal emniyet girdileri, yer istasyonu komutları, kayıt yönetimi ve izleme, yardımcı süreçler); ön denetim raporu; hata yanıtları; geçerlilik ve gerçek zaman bildirimi.

**Kapsam dışı:**
- Panellerin görünümü, bileşenleri, yerleşimi ve ön yüz kitaplıkları (görev belgesi).
- A1 mesajlarının içeriği (A1 sözleşmesi). A3 yalnız A1'den beklediği büyüklükleri listeler (10. başlık).
- A5 dosya biçimi, doğrulama kuralları ve doğrulama raporunun biçimi (A5 sözleşmesi); A3 bunları taşır.
- Kayıt dosyasının biçimi ve tekrar oynatmanın güvenceleri (A6). A3 izleme kipinde kayıttaki veriyi yayınlar.
- Kamera karelerinin panele akışı: 1.0'da yoktur (kareler yalnız A6'ya kaydedilir; 6. başlık).
- Harici yer istasyonu yazılımına açılan MAVLink/UDP uç noktası: ilk sürümde yoktur (iskelet §3 "Yer istasyonu" paragrafı, §10 md.17); A3-K107.
- Foxglove SDK ve benzeri hazır canlı yayın protokolleri: A3 kendi WebSocket protokolüdür; Foxglove SDK kullanılmaz (AB-058; 00 belgesi Ç11, A2).
- Atış kontrolünde algılama, takip, atış ölçütü ve atış çözümü (03 kapsamı ve W1 ortak kurallarındaki atış kontrol sınırı). A3 atışla ilgili yalnız emniyet zincirinin durumunu, olayların sonucunu ve izin/engel kurallarını taşır.

## 2. Terimler ve kabuller

İskelet §3.1 kabulleri (SI birimler, açı radyan, zaman tamsayı nanosaniye, sıcaklık °C, NED dünya çerçevesi, gövde çerçevesi, Hamilton kuaterniyon (w, x, y, z), ölçüm zamanı ile alım zamanının ayrılığı) ve iskelet §4 zaman tanımları (fizik adımı 1,25 ms, kontrol periyodu 5 ms, `L_act` = 10 ms, iki saat alanı) aynen geçerlidir; burada yinelenmez.

| Terim | Anlam |
|---|---|
| Bağlantı | Bir istemcinin tek WebSocket bağlantısı. Bir istemci birden çok bağlantı açabilir |
| İstemci kimliği (`istemci_id`) | `POST /api/v1/istemciler` ile alınan, sim süreci boyunca tekil `u32`; durum değiştiren HTTP isteklerinde `Ida-Istemci` başlığıyla gönderilir |
| Kanal | Ad ve kimlikle tanımlı, tek yük düzeni taşıyan veri akışı (§3.3) |
| Kanal-anahtar | Kanal ile katalogdaki `anahtar` alanlarının değerlerinin birleşimi (ör. `sensor/imu` + `sensor_id` + `inst`); anahtarsız kanalda yalnız kanal |
| Sınıf | Kanalın düşürme kuralı: `durum` (son değer), `dizi` (sınırlı kuyruk, en eski düşer), `olay` (kayıpsız, sıralı) |
| Komut | Durum değiştiren HTTP isteği (katalogda `durum_degistirir: true`) |
| Komut numarası (`komut_no`) | Sim süreci boyunca 1'den boşluksuz artan, komuta sunucunun verdiği numara |
| Adım sınırı | Dünya iş parçacığının iki fizik adımı arasındaki noktası; komutlar burada uygulanır |
| İstemci oturumu | Bir `istemci_id`'nin `POST /api/v1/istemciler` ile alınmasından `DELETE /api/v1/istemciler/{istemci_id}` ya da sim sürecinin bitişine kadarki ömrü |
| Emniyet revizyonu (`emniyet_rev`) | İskelet §3.2'deki tek sayaç: `u32`, 1'den başlar, sıfırlanmaz, `0xFFFFFFFF`'te doyar; A1'deki `safety_rev` ile aynıdır; artıran işlemlerin kapalı listesi A3-K117'dedir |
| Durum revizyonu (`durum_rev`) | Dünya tarafında tutulan, uygulanan her çalıştırma kontrolü komutunda ve her durum geçişinde bir artan sayaç (A3-K122) |
| Çalıştırma kontrolü komutu | `senaryo/yukle`, `calistirma/*` ve `izleme/*` uçlarındaki komutlar (A3-K123) |
| Güncel `run_id` | `baslat` ya da `yeniden_kostur` ile atanan, `sifirla`, `senaryo/yukle` ve izleme başlangıcında `null` olan kimlik (A3-K124) |
| Kısıtlayan / gevşeten komut | Kısıtlayan: emniyeti artıran sanal girdi değişikliği ve izni daraltan yer istasyonu komutu. Gevşeten: emniyeti azaltan sanal girdi değişikliği ve izni genişleten yer istasyonu komutu (A3-K118, K119) |
| Canlı çalıştırma | `hazirlaniyor`, `kosuyor` ya da `duraklatildi` durumundaki çalıştırma |
| Yayın tiki | API bileşeninin duvar saatiyle 50 Hz'te bir çalışan gönderim anı (A3-K33) |
| Yer gerçeği | Fizik ve sensör modellerinin "gerçek" değerleri; karta ve kamera algılama bileşenine hiçbir yolla gitmez (08 §2.1) |

**Koruma kabulü:** A3'ün erişim önlemleri (A3-K02…K12) başka web sayfalarının tarayıcı üzerinden sunucuya erişmesine ve yerel ağdan erişime karşıdır. Aynı kullanıcı hesabında çalışan kötü niyetli bir sürece karşı güvence verilmez (08 §2.1).

---

## 3. Veri tanımları

### 3.1 Tel tipleri

İkili alanlar katalogdaki sırayla, aralarında doldurma (padding) olmadan, açıkça serileştirilir; bellekteki yapının ham kopyası tele yazılmaz (00 Ç3'teki ders; 08 §3.1). Tipler `x_ida_tipler` ile aynıdır:

| Tip | Tel gösterimi |
|---|---|
| `u8`, `u16`, `u32`, `u64` | işaretsiz tamsayı; çok baytlılar küçük uçlu |
| `i32`, `i64` | ikiye tümleyen işaretli tamsayı, küçük uçlu |
| `f32`, `f64` | IEEE-754 binary32 / binary64, küçük uçlu |
| `T[n]` (katalogda `adet`) | `n` öğe art arda; sayaç yazılmaz |
| `metin(s, m)` | `s` tipinde (`u16` ya da `u32`) bayt uzunluğu + UTF-8 baytlar; sonlandırıcı yok; uzunluk ≤ `m` |
| `bayt_dizisi(s, m)` | `s` tipinde bayt uzunluğu + baytlar; uzunluk ≤ `m` |
| `dizi(s, m)` | `s` tipinde öğe sayısı + öğeler sırayla; sayı ≤ `m`; öğe alanları aynı kurallarla |

**"Yok" değerleri:** bir alanın "bilinmiyor/yok" anlamı katalogdaki `yok_degeri` ile verilir (`NaN`, `INT64_MIN`, `2^64-1`, `0xFFFFFFFF`, `255` …). `yok_degeri` tanımlı olmayan alan her zaman geçerli bir değer taşır.

### 3.2 İkili çerçeve başlığı

Her çerçeve 24 baytlık başlıkla başlar, ardından `yuk_uzunlugu` bayt yük gelir.

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `t_ns` | i64 | ns | [0, 9223372036854775807] | kanalın zaman anlamına göre sim zamanı (A3-K26) |
| `sira` | u32 | - | tipin aralığı | olay kanalında olay sırası; diğer kanallarda kanal üretim sayacı (A3-K27) |
| `yuk_uzunlugu` | u32 | bayt | [0, 65536] | başlıktan sonraki yük uzunluğu |
| `kanal_id` | u16 | - | tipin aralığı | katalogdaki kanal kimliği |
| `kanal_surumu` | u16 | - | tipin aralığı | kanalın yük düzeni sürümü |
| `bayraklar` | u16 | - | tipin aralığı | bit0 kaynak_kayit (izleme), bit1 özet (yeniden eşleme), bit2 oncesinde_dusme, bit3 yer_gercegi; bit4-15 sıfır |
| `ayrilmis` | u16 | - | tipin aralığı | gönderen 0 yazar; alıcı yok sayar |

Bayt konumları: `t_ns` 0, `sira` 8, `yuk_uzunlugu` 12, `kanal_id` 16, `kanal_surumu` 18, `bayraklar` 20, `ayrilmis` 22.

### 3.3 Kanal kataloğu

Sınıflar 2. başlıkta, zaman anlamları A3-K26'da tanımlıdır. "Azami Hz" yalnız `durum` sınıfında anlamlıdır; `dizi` ve `olay` sınıfı seyreltilmez. Bütün hız ve kuyruk değerleri mimari seçimidir (tahmin; 8. başlık md.1).

| `kanal_id` | Ad | Yük | Sınıf | Azami / varsayılan Hz | Kuyruk | Zaman anlamı | Anahtar | Zorunlu | Kaynak | A6'da |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `calistirma/durum` | `yuk_calistirma_durum` | durum | 10 / 10 | — | sim | — | evet | çalıştırma denetçisi (08) | ortak |
| 2 | `sayac/gercek_zaman` | `yuk_sayac_gercek_zaman` | durum | 10 / 10 | — | sim | — | evet | 04 §2.11, 05 §4, 06 §4, iskelet §4 | ortak |
| 3 | `olay` | `yuk_olay` | olay | — | 4096 | sim | — | evet | bütün parçalar | ortak |
| 10 | `fizik/durum` | `yuk_fizik_durum` | durum | 50 / 25 | — | olcum | — | hayır | 04 WorldSnapshot | ortak |
| 11 | `kart/durum` | `yuk_kart_durum` | durum | 50 / 25 | — | alim | — | hayır | A1 HIL düzlemi telemetrisi (emülatörsüz) | ortak |
| 12 | `yer_istasyonu/durum` | `yuk_kart_durum` | durum | 50 / 10 | — | alim | — | hayır | A1 yer istasyonu düzlemi telemetrisi (emülatörden geçmiş) | ortak |
| 13 | `atis/durum` | `yuk_atis_durum` | durum | 10 / 10 | — | sim | — | hayır | sim fırlatıcı modeli (04) + kartın FC durumu (A1) | ortak |
| 14 | `emniyet/girdiler` | `yuk_emniyet_girdiler` | durum | 10 / 10 | — | sim | — | hayır | sim sanal emniyet girdileri (iskelet §5) + kart geri okuması (A1) | ortak |
| 15 | `ariza/etkin` | `yuk_ariza_etkin` | durum | 2 / 2 | — | sim | — | hayır | arıza dağıtıcısı (08) | ortak |
| 20 | `sensor/imu` | `yuk_imu` | durum | 50 / 10 | — | olcum | `sensor_id`, `inst` | hayır | 05 sensör modeli, arıza zinciri sonrası | ortak |
| 21 | `sensor/manyetometre` | `yuk_manyetometre` | durum | 50 / 10 | — | olcum | `sensor_id`, `inst` | hayır | 05 sensör modeli, arıza zinciri sonrası | ortak |
| 22 | `sensor/basinc` | `yuk_basinc` | durum | 50 / 10 | — | olcum | `sensor_id`, `inst` | hayır | 05 sensör modeli, arıza zinciri sonrası | ortak |
| 23 | `sensor/gnss` | `yuk_gnss` | durum | 50 / 10 | — | olcum | `sensor_id`, `inst` | hayır | 05 sensör modeli, arıza zinciri sonrası | ortak |
| 24 | `sensor/dvl` | `yuk_dvl` | durum | 50 / 10 | — | olcum | `sensor_id`, `inst` | hayır | 05 sensör modeli, arıza zinciri sonrası | ortak |
| 25 | `sensor/batarya` | `yuk_batarya` | durum | 50 / 10 | — | olcum | `sensor_id`, `inst` | hayır | 05 sensör modeli, arıza zinciri sonrası | ortak |
| 26 | `sensor/sizinti` | `yuk_sizinti` | durum | 50 / 10 | — | olcum | `sensor_id`, `inst` | hayır | 05 sensör modeli, arıza zinciri sonrası | ortak |
| 27 | `sensor/firlatici` | `yuk_firlatici` | durum | 50 / 10 | — | olcum | `sensor_id`, `inst` | hayır | 05 sensör modeli, arıza zinciri sonrası | ortak |
| 30 | `sonar/ping360` | `yuk_ping360` | dizi | — | 256 | gonderim | `sensor_id`, `inst` | hayır | 05 sonar, arıza zinciri sonrası | ortak |
| 31 | `sonar/ping1d` | `yuk_ping1d` | durum | 50 / 10 | — | gonderim | `sensor_id`, `inst` | hayır | 05 sonar, arıza zinciri sonrası | ortak |
| 41 | `algilama/parti` | `yuk_algilama_parti` | dizi | — | 64 | olcum | — | hayır | A8 parti başlığı ve nihai durum (A8-K63) | ortak |
| 42 | `algilama/sayaclar` | `yuk_algilama_sayaclari` | durum | 2 / 2 | — | sim | — | hayır | A8-K30 sim sayaçları | ortak |
| 54 | `kart/baglanti_sayaclari` | `yuk_kart_baglanti_sayaclari` | durum | 2 / 2 | — | alim | — | hayır | A1 `IDA_LINK_STATS` (Tablo 3.11b) | ortak |
| 55 | `kart/cekirdek_sayaclari` | `yuk_kart_cekirdek_sayaclari` | durum | 2 / 2 | — | alim | — | hayır | A1 `IDA_CORE_COUNTERS` ve `IDA_STATUS.health` (A4) | ortak |
| 56 | `kamera/halka_sayaclari` | `yuk_kamera_halka_sayaclari` | durum | 2 / 2 | — | sim | — | hayır | A2 §5 sayaçları | ortak |
| 40 | `algilama/kayit` | `yuk_algilama_kayit` | dizi | — | 64 | olcum | — | hayır | A8 (kamera) ve A1 (kartın sonar algılaması, taşınırsa) | ortak |
| 50 | `kayit/durum` | `yuk_kayit_durum` | durum | 2 / 2 | — | sim | — | hayır | kayıt yazıcısı (A6) | — |
| 51 | `surec/durum` | `yuk_surec_durum` | durum | 2 / 2 | — | sim | — | hayır | süreç gözetmeni (08) | — |
| 52 | `baglanti/durum` | `yuk_baglanti_durum` | durum | 10 / 10 | — | sim | — | hayır | 06 oturum ve emülatör; A3 bağlantısı | — |
| 53 | `yer_istasyonu/mavlink` | `yuk_yer_istasyonu_mavlink` | dizi | — | 256 | sim | — | hayır | 06 yer istasyonu ağ geçidi | — |

"A6'da: ortak" kanallar A6'ya aynı ad, aynı yük ve aynı `kanal_surumu` ile kaydedilir (A6 §3.3). A3 telemetri çerçevelerinin kendisi kaydedilmez; kayıt yazıcısı aynı tanımdan türeyen veriyi doğrudan yazar (iskelet A6 satırı; U-28).

Sayaç ve tanılama kanalları (W1-uyum U18): `sayac/gercek_zaman` sim tarafının A1 Tablo 3.11a sayaçlarını aynı adlarla ve sim modelinin (04, 05) sayaçlarını taşır; `kart/baglanti_sayaclari` A1 Tablo 3.11b'nin 36 kart sayacını `IDA_LINK_STATS.cnt[]` sırasıyla ve adıyla; `kart/cekirdek_sayaclari` A4'ün 22 çekirdek sayacını (`IDA_CORE_COUNTERS`) ve sekiz sağlık bitini; `kamera/halka_sayaclari` A2 §5'in halka sayaçlarını; `algilama/parti` A8 parti başlığını, bileşenin ve sim'in kararını, nihai kare durumunu (A8-K63) ve geç yanıtı (boş partiler dahil); `algilama/sayaclar` A8-K30'un akış başına ret ve durum sayaçlarını. Bu sayaçlar serbest metinle ya da "ham trafikten çözülür" varsayımıyla taşınmaz; her alanın kaynağı yük tablosunda yazılıdır.

`sensor/<tür>` kanallarının türleri: `imu`, `manyetometre`, `basinc`, `gnss`, `dvl`, `batarya`, `sizinti`, `firlatici` (iskelet §5'teki seyrüsefer ve sistem sensörleri). Kamera sensörünün kareleri A3'te yoktur.

### 3.4 Yük düzenleri

Tablolar `a3-kanallar.json` içindeki `x_ida_yukler` ile aynıdır (aynı tanımdan üretildi). Her alan zorunludur (ikili düzende alan atlanamaz); "yok" anlamı `yok_degeri` ile verilir. Geçersiz değerde davranış A3-K25 ve A3-K24'tedir. `dizi` öğelerinin alanları `ad[].alt_alan` biçiminde gösterilir.

#### Ölçüm başlığı (`yuk_imu` … `yuk_ping1d` yüklerinin ilk 12 alanı)

Aşağıdaki sensör ve sonar yükleri bu 12 alanla, bu sırayla başlar; tablolarında yalnız ardından gelen alanlar gösterilir. JSON katalogunda alanlar her yükte tam listelenir.

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `sensor_id` | u16 | - | tipin aralığı | A5 `sensorler[]` kimliği |
| `inst` | u8 | - | tipin aralığı | aynı türden sensörün örnek numarası |
| `seq` | u32 | - | tipin aralığı | sensörün kendi örnek sayacı; kayıp boşluk bırakır (05 MeasurementRecord.seq) |
| `flags` | u16 | - | tipin aralığı | bit0 geçerli, bit1 doymuş, bit2 sensor_arizasi; bit3-15 türe özgü (A1/A4 tanımlar) |
| `window_ns` | i64 | ns | [0, 1000000000] | aralık boyunca toplanan ölçümde aralık süresi; anlık ölçümde 0 |
| `t_tx_ns` | i64 | ns | yok: INT64_MIN | akustik ölçümde darbe gönderim anı (iskelet §3.1); yoksa INT64_MIN |
| `t_hazir_ns` | i64 | ns | tipin aralığı | modellenmiş veri hazır anı (05) |
| `t_teslim_ns` | i64 | ns | tipin aralığı | modellenmiş teslim anı (05); fiilî teslim parti_step_seq alanındadır |
| `parti_step_seq` | u32 | - | yok: 0xFFFFFFFF | kaydın fiilen girdiği A1 partisinin sıra numarası; teslim edilmediyse 0xFFFFFFFF |
| `fault_mask` | u32 | - | tipin aralığı | o an bu kayda etkiyen arıza türleri bit maskesi (05 §2.7 bileşim sırası numarası = bit) |
| `quality` | f32 | - | [0, 1]; yok: NaN | normalize kalite [0,1]; yoksa NaN |
| `model_version` | u16 | - | tipin aralığı | sensör modelinin sürümü (05 §2.6) |

#### `yuk_calistirma_durum`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `durum` | u8 | - | 0=bos, 1=yuklendi, 2=hazirlaniyor, 3=kosuyor, 4=duraklatildi, 5=bitti, 6=durduruldu, 7=hata, 8=izleme | çalıştırma durumu (A3 Tablo 4.7a) |
| `kip` | u8 | - | 0=yok, 1=sil, 2=hil_1x, 3=hil_bariyer, 4=tekrar_fizik, 5=tekrar_girdi_sil, 6=tekrar_yeniden_kurma | zaman kipi |
| `run_id` | metin(u16, en çok 32) | bayt | tipin aralığı | çalıştırma kimliği (A6 §3.1); çalıştırma yoksa boş |
| `adim_no` | u64 | - | tipin aralığı | tamamlanan fizik adımı sayısı; çalıştırma başında 0 |
| `hedef_carpan` | f32 | - | [0, 100] | istenen sim zamanı / duvar saati oranı; 0 = olabildiğince hızlı; HIL kiplerinde 1 |
| `gerceklesen_carpan` | f32 | - | yok: NaN | son 1 s duvar saati penceresinde gerçekleşen oran; hesaplanmadıysa NaN |
| `gecerli` | u8 | - | 0=gecersiz, 1=gecerli | 1 = çalıştırma geçerli; bir kez 0 olursa o çalıştırmada 1'e dönmez |
| `gercek_zamani_kacirdi` | u8 | - | 0=hayir, 1=evet | iskelet §4 işareti: en az bir yanıt kendi L_act anına yetişmedi ya da HIL'de sim adımı kaçırıldı |
| `gecerlilik_nedenleri` | u32 | - | tipin aralığı | geçersizlik neden bitleri (A3 Tablo 4.8); yalnız bit eklenir, silinmez |
| `tekrar_guvencesi` | u8 | - | 0=yok, 1=izleme, 2=fizik, 3=girdi | bu çalıştırmanın kaydının şu anki tekrar oynatma güvencesi (A6 §4.6) |
| `kademe_gorunum` | u8 | - | 0=kapali, 1=sade, 2=tam | iskelet §6 3B görünüm kademesi |
| `kademe_kamera` | u8 | - | 0=kapali, 1=sade, 2=tam | iskelet §6 kamera kademesi |
| `kademe_sonar` | u8 | - | 0=kapali, 1=sade, 2=tam | iskelet §6 sonar kademesi |
| `kademe_kayit` | u8 | - | 0=olaylar, 1=telemetri, 2=tam | iskelet §6 kayıt kademesi |
| `a1_oturum` | u8 | - | 0=kapali, 1=el_sikisma, 2=hazir, 3=kosuyor, 4=kopuk, 255=uygulanamaz | A1 oturum durumu (06 §2.9; tanım A1) |
| `son_komut_no` | u32 | - | tipin aralığı | son atanan A3 komut numarası; yoksa 0 |
| `durum_rev` | u64 | - | tipin aralığı | durum revizyonu: sim süreci boyunca 1'den başlayan, uygulanan her çalıştırma kontrolü komutunda ve her durum geçişinde bir artan sayaç (A3-K122) |

#### `yuk_sayac_gercek_zaman`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `parti_gonderilen` | u64 | - | tipin aralığı | gönderilen TF sayısı (A1 Tablo 3.11a) |
| `parti_kacirildi` | u64 | - | tipin aralığı | uygulama anında yuvası tamamlanmamış ve kaçırılan parti; birleştirilen dahil (A1 Tablo 3.11a) |
| `parti_kacirildi_birlesik` | u64 | - | tipin aralığı | bunların sonradan gelen bir yanıtın n_merged'i kapsadığı kısmi (A1 Tablo 3.11a) |
| `bariyer_bekleme` | u64 | - | tipin aralığı | bariyerde beklenen yuva sayısı (A1 Tablo 3.11a) |
| `gercek_zaman_kaymasi_ns` | i64 | ns | tipin aralığı | bariyer beklemelerinin toplam süresi (A1 Tablo 3.11a) |
| `gercek_zaman_kaymasi_en_buyuk_ns` | i64 | ns | tipin aralığı | en uzun tek bariyer beklemesi (A1'de `gercek_zaman_kaymasi_ns` adinin "en büyük" değeri; ayrı alan olduğu için sonek aldı) (A1 Tablo 3.11a) |
| `tf_gec_yazim_ns` | i64 | ns | tipin aralığı | TF yazım başlangıcının `W(t_n)`'den en büyük gecikmesi (A1 Tablo 3.11a) |
| `yanit_ret_seq_disi` | u64 | - | tipin aralığı | sıra dışı yanıt reddi (A1 Tablo 3.11a) |
| `yanit_ret_yinelenen` | u64 | - | tipin aralığı | yinelenen yanıt reddi (A1 Tablo 3.11a) |
| `yanit_ret_gec` | u64 | - | tipin aralığı | uygulama anı geçmiş yanıt reddi (A1 Tablo 3.11a) |
| `yanit_ret_oturum` | u64 | - | tipin aralığı | oturum dışı yanıt reddi (A1 Tablo 3.11a) |
| `yanit_iptal_esz` | u64 | - | tipin aralığı | eşzamansız güvenli çıktının iptal ettiği yuva (A1 Tablo 3.11a) |
| `esz_guvenli_uygulanan` | u64 | - | tipin aralığı | uygulanan eşzamansız güvenli çıktı (A1 Tablo 3.11a) |
| `komut_suresi_doldu` | u64 | - | tipin aralığı | kullanım süresi dolup nötre geçiş (A1 Tablo 3.11a) |
| `olay_alinan` | u64 | - | tipin aralığı | yeni olay (A1 Tablo 3.11a) |
| `olay_gec` | u64 | - | tipin aralığı | geç uygulanmaya çalışılan olay (A1 Tablo 3.11a) |
| `olay_eksik` | u64 | - | tipin aralığı | yuvası n_events tamamlanmadan kapanan olay (A1 Tablo 3.11a) |
| `olay_sonuc` | u64[6] | - | tipin aralığı | sonuç kodu başına olay sayacı; sıra `APPLIED`, `DUPLICATE`, `REJ_EXPIRED`, `REJ_SAFE`, `REJ_TUBE_EMPTY`, `REJ_SESSION` (olay_sonuc_<kod>) (A1 Tablo 3.11a) |
| `komut_birlestirildi` | u64 | - | tipin aralığı | sim'de birleştirilen daraltıcı (A1 Tablo 3.11a) |
| `komut_ertelendi` | u64 | - | tipin aralığı | sonraki pencereye kalan toplu komut (A1 Tablo 3.11a) |
| `komut_sonucsuz` | u64 | - | tipin aralığı | oturum bittiği için sonucu gelmeyen komut (A1 Tablo 3.11a) |
| `ping360_seyreltildi` | u64 | - | tipin aralığı | atilan Ping360 satırı (A1 Tablo 3.11a) |
| `ping360_gecikme_en_buyuk` | u64 | - | tipin aralığı | t_hazir'dan teslim partisine en büyük gecikme (periyot) (A1 Tablo 3.11a) |
| `pencere_tasma` | u64 | - | tipin aralığı | kritik ve atanmis üyelerin B_pencere'yi astigi pencere (A1 Tablo 3.11a) |
| `rx_crc_err` | u64 | - | tipin aralığı | sim alıcısının çerçeve hatası (A1 Tablo 3.11a) |
| `rx_resync_bytes` | u64 | - | tipin aralığı | sim alıcısının çerçeve dışı atilan baytı (A1 Tablo 3.11a) |
| `rx_unknown_msgid` | u64 | - | tipin aralığı | sim alıcısının bilinmeyen kimlikli çerçevesi (A1 Tablo 3.11a) |
| `rx_bad_len` | u64 | - | tipin aralığı | sim alıcısının len hatası (A1 Tablo 3.11a) |
| `rx_wrong_sys` | u64 | - | tipin aralığı | sim alıcısının sysid/compid hatası (A1 Tablo 3.11a) |
| `rx_iflag` | u64 | - | tipin aralığı | sim alıcısının incompat_flags hatası (A1 Tablo 3.11a) |
| `rx_mav1` | u64 | - | tipin aralığı | sim alıcısının MAVLink 1 çerçevesi (A1 Tablo 3.11a) |
| `rx_seq_gap` | u64 | - | tipin aralığı | sim alıcısının başlık seq boşluğu (A1 Tablo 3.11a) |
| `oturum_kurulan` | u64 | - | tipin aralığı | kurulan oturum (A1 Tablo 3.11a) |
| `oturum_kopma` | u64 | - | tipin aralığı | kopan oturum (A1 Tablo 3.11a) |
| `beklenmeyen_sifirlama` | u64 | - | tipin aralığı | kartın beklenmeyen sıfırlanması (A1 Tablo 3.11a) |
| `calistirma_gecersiz` | u8 | - | tipin aralığı | çalıştırma zamanlama bakımından geçersiz (A1-K79) (A1 Tablo 3.11a) |
| `neden_bitleri` | u32 | - | tipin aralığı | A1-K79 neden bitleri; bit numaralari A3 Tablo 4.8 (0-11) (A1 Tablo 3.11a) |
| `son_tarih_kacirildi` | u64 | - | tipin aralığı | yuvayı tamamlayan çerçevenin D_n'den sonra alınması (A1-K108) (A1 Tablo 3.11a) |
| `sim_gecikmesi` | u64 | - | tipin aralığı | sim'in fizik adımını planlanan duvar anından sonra hesaplaması (A1-K108) (A1 Tablo 3.11a) |
| `yanit_ret_eski_uretim` | u64 | - | tipin aralığı | out_seq'i artmayan sürekli çıktı (A1 Tablo 3.11a) |
| `olay_rev_ret` | u64 | - | tipin aralığı | safety_rev damgasi güncel olmadığı için REJ_SAFE alan izin acan olay (A1 Tablo 3.11a) |
| `sonuc_kuyrugu_en_buyuk` | u64 | - | tipin aralığı | olay sonucu kuyruğunun en büyük doluluğu (A1 Tablo 3.11a) |
| `kritik_erteleme` | u64 | - | tipin aralığı | B_krit yüzünden kayan kritik üye (A1-K113) (A1 Tablo 3.11a) |
| `kritik_erteleme_en_buyuk` | u64 | - | tipin aralığı | en büyük kritik erteleme (parti) (A1 Tablo 3.11a) |
| `cagri_izi_eksik` | u64 | - | tipin aralığı | gelmeyen IDA_CALL_TRACE (A1-K95) (A1 Tablo 3.11a) |
| `yapilandirma_ret` | u64 | - | tipin aralığı | A1-K104 ile reddedilen yapılandırma; ayrıntı `yapilandirma_reddedildi` olayında (A1 Tablo 3.11a) |
| `cagri_cakismasi` | u64 | - | tipin aralığı | CALL_CONFLICT bayrakli parti (A1-K117) (A1 Tablo 3.11a) |
| `sonuc_talebi_atildi` | u64 | - | tipin aralığı | yer olmadığı için yanıtlanmayan saklı sonuç talebi (A1-K111) (A1 Tablo 3.11a) |
| `komut_kovasi_bekleme` | u64 | - | tipin aralığı | kova yüzünden bekleyen komut (A1 Tablo 3.11a) |
| `emniyet_rev_doydu` | u8 | - | 0=hayir, 1=evet | emniyet revizyonu doygunluğa ulaştı (A1 Tablo 3.11a; iskelet §3.2) |
| `mesaj_retleri` | dizi(u8, en çok 64) | - | tipin aralığı | mesaj başına kart çerçevesi retleri; yalnız sıfırdan büyük olanlar, mesaj kimliği sırasıyla |
| `mesaj_retleri[].mesaj_kimligi` | u32 | - | tipin aralığı | reddedilen kart çerçevesinin A1 mesaj kimliği |
| `mesaj_retleri[].alan_ret` | u64 | - | tipin aralığı | alan_ret_<mesaj> (A1 Tablo 3.11a, Tablo 4.6) |
| `mesaj_retleri[].bag_ret` | u64 | - | tipin aralığı | bag_ret_<mesaj> (A1 Tablo 3.11a, Tablo 4.6) |
| `adim_son_tarih_ihlali` | u64 | - | tipin aralığı | sim fizik adımının son tarihinden geç bitmesi (04 §2.11) |
| `kacirilan_adim` | u64 | - | tipin aralığı | gecikmesi en az bir fizik adımı olan adım (04 §2.11) |
| `azami_adim_gecikmesi_ns` | i64 | ns | tipin aralığı | en büyük fizik adımı gecikmesi (04 §2.11) |
| `sonar_gec` | u64 | - | tipin aralığı | 05 §4 |
| `sonar_kuyruk_dolu` | u64 | - | tipin aralığı | 05 §4 |
| `sensor_kuyruk_tasma` | u64 | - | tipin aralığı | 05 §4 teslim kuyruğu taşması |
| `donma_onceki_cikti_yok` | u64 | - | tipin aralığı | A5 Tablo 3.10-A donma arıza sayacı |
| `atama_celiskisi` | u64 | - | tipin aralığı | A5-K63 atama çelişkisi sayısı |
| `gevsetme_reddedildi` | u64 | - | tipin aralığı | senaryonun reddedilen gevşeten atamasi (W1-uyum U3) |
| `bayat_yetki` | u64 | - | tipin aralığı | emülatörden çıkışta revizyonu değişmiş olduğu için atilan genişletici yer istasyonu komutu (A3-K101) |
| `komut_bayat_yetki` | u64 | - | tipin aralığı | A1 gönderim kuyruğunda, tele yazılmadan önce revizyonu değişmiş olduğu için gönderilmeyen genişletici komut (A1 Tablo 3.11a; A1-K129) |
| `a3_halka_tasma` | u64 | - | tipin aralığı | dünya -> A3 halka tamponunda düşen ileti (A3-K38) |
| `kayit_dusen` | u64 | - | tipin aralığı | dünya -> kayıt halka tamponunda düşen ileti (A6-K30) |

#### `yuk_olay`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `tur` | u16 | - | tipin aralığı | olay türü kodu (x_ida_olay_turleri) |
| `onem` | u8 | - | 0=bilgi, 1=uyari, 2=hata, 3=emniyet | olayın önemi |
| `kaynak` | u8 | - | 0=sim, 1=a3, 2=kart, 3=senaryo, 4=kayit, 5=surec, 6=yer_istasyonu | olayı üreten taraf |
| `adim_no` | u64 | - | yok: 2^64-1 | olayın bağlandığı fizik adımı; adımsız olayda 2^64-1 |
| `komut_no` | u32 | - | tipin aralığı | ilgili A3 komut numarası; yoksa 0 |
| `istemci_id` | u32 | - | tipin aralığı | ilgili istemci; yoksa 0 |
| `ayrinti` | metin(u16, en çok 8192) | bayt | tipin aralığı | UTF-8 JSON nesnesi; şeması olay türüne göre $defs içindedir |

#### `yuk_fizik_durum`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `adim_no` | u64 | - | tipin aralığı | snapshot'ın fizik adımı |
| `govdeler` | dizi(u16, en çok 64) | - | tipin aralığı | yer gerçeği gövde durumları |
| `govdeler[].govde_id` | u16 | - | tipin aralığı | gövde kimliği (04) |
| `govdeler[].tur` | u8 | - | 0=arac, 1=torpido, 2=hedef, 3=diger | gövde türü |
| `govdeler[].kutle_durumu` | u8 | - | 0=iki_torpido, 1=bir_torpido, 2=bos, 255=uygulanamaz | aracın kütle durumu (iskelet §5); araçtan başka gövdede 255 |
| `govdeler[].p_n` | f64[3] | m | tipin aralığı | referans noktasının NED konumu |
| `govdeler[].q_nb` | f64[4] | - | tipin aralığı | gövdeden dünyaya birim kuaterniyon (w,x,y,z) |
| `govdeler[].nu` | f64[6] | m/s, rad/s | tipin aralığı | genelleştirilmiş hız (u,v,w,p,q,r) gövde çerçevesi, O_b |
| `govdeler[].a_n` | f64[3] | m/s^2 | tipin aralığı | O_b'nin NED eylemsiz ivmesi (04) |
| `iticiler` | dizi(u8, en çok 8) | - | tipin aralığı | itici durumları, itici sırasıyla |
| `iticiler[].u` | f32 | - | [-1, 1] | itici isteği (04 snapshot u) |
| `iticiler[].itki_n` | f32 | N | tipin aralığı | üretilen itki |
| `iticiler[].akim_a` | f32 | A | tipin aralığı | itici akımı |
| `bara_gerilimi_v` | f32 | V | tipin aralığı | bara gerilimi |
| `paket_akimi_a` | f32 | A | tipin aralığı | batarya paket akımı |
| `soc` | f32 | - | [0, 1] | şarj durumu |

#### `yuk_kart_durum`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `kart_t_ns` | i64 | ns | tipin aralığı | kestirimin ait olduğu ölçüm zamanı (kartın bildirdiği; A1'den beklenen) |
| `acilis_sayaci` | u32 | - | tipin aralığı | kartın açılış sayacı (iskelet §3.2; A1'den beklenen); bilinmiyorsa 0 |
| `kip` | u8 | - | yok: 255 | otopilot kipi (A1 `IDA_STATUS.mode` = A4 `status.mode`); değer adları A4'te; bilinmiyorsa 255 |
| `silahli` | u8 | - | 0=hayir, 1=evet, 255=bilinmiyor | otopilot arm durumu (atış kontrol silahlandırması değildir) |
| `saglik` | u8 | - | tipin aralığı | sağlık bitleri (A1 `IDAW_HEALTH`): bit0 core_ok, bit1 data_ok, bit2 imu_rejected, bit3 imu_lag, bit4 input_dropped, bit5 narrowed, bit6 sample_conflict, bit7 bg_bad_input |
| `kestirim_gecerli` | u8 | - | 0=hayir, 1=evet, 255=bilinmiyor | kestirim geçerli mi |
| `p_n` | f32[3] | m | yok: NaN | kestirilen NED konum |
| `v_n` | f32[3] | m/s | yok: NaN | kestirilen NED hız |
| `q_nb` | f32[4] | - | yok: NaN | kestirilen yönelim (w,x,y,z) |
| `derinlik_m` | f32 | m | yok: NaN | kestirilen derinlik |
| `batarya_soc` | f32 | - | [0, 1]; yok: NaN | kartın bildirdiği şarj durumu |
| `adim_suresi_us` | u32 | us | yok: 0xFFFFFFFF | tanısal: kartın son adım süresi; bilinmiyorsa 0xFFFFFFFF |

#### `yuk_atis_durum`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `fc_durumu` | u8 | - | 0=SAFE, 1=ARMED, 2=PERMIT_OPEN, 3=FIRE_PENDING, 4=TEST, 5=FAULT_LATCHED; yok: 255 | kartın bildirdiği emniyet zinciri durumu (A1 `IDA_FC_STATUS.state` = A4 `fc.state`); değer adları ve anlamları A4 §3.16; bilinmiyorsa 255 |
| `engel_maskesi` | u32 | - | yok: 0xFFFFFFFF | kartın bildirdiği emniyet engeli bitleri (A1 `IDA_FC_STATUS.inhibit_mask` = A4 `fc.inhibit_mask`); bit adları ve anlamları A4 §3.16; bilinmiyorsa 0xFFFFFFFF |
| `kart_son_olay_kimligi` | u64 | - | yok: 0 | A1 `IDA_FC_STATUS.last_event_id`; yoksa 0 |
| `tupler` | dizi(u8, en çok 2) | - | tipin aralığı | fırlatıcı tüplerinin durumu |
| `tupler[].tup` | u8 | - | [0, 1] | tüp numarası |
| `tupler[].dolu` | u8 | - | 0=bos, 1=dolu | sim'in yetkili durumu: tüpte torpido var mı |
| `tupler[].sureli_izin_gecerli` | u8 | - | 0=hayir, 1=evet | sim tarafında emniyet_ac süreli izni geçerli mi (03 §2.7; iskelet §3.2) |
| `tupler[].izin_kalan_ns` | i64 | ns | yok: -1 | süreli iznin kalan süresi (sim zamanı); izin yoksa -1 |
| `tupler[].son_olay_sonucu` | u8 | - | 0=yok, 1=uygulandi, 2=yineleme, 3=ret_emniyet, 4=ret_suresi_gecmis, 5=ret_diger | bu tüpe ait son tek seferlik olayın sonucu |

#### `yuk_emniyet_girdiler`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `acil_durdurma` | u8 | - | 0=serbest, 1=basili | sanal acil durdurma yetkili durumu (tek) |
| `silahlandirma_fisi` | u8[2] | - | 0=cikarilmis, 1=takili | tüp başına sanal silahlandırma fişi yetkili durumu; dizin 0 = tüp `f1`, dizin 1 = tüp `f2` (A5) |
| `emniyet_pimi` | u8[2] | - | 0=cikarilmis, 1=takili | tüp başına sanal emniyet pimi yetkili durumu; dizin 0 = tüp `f1`, dizin 1 = tüp `f2` (A5) |
| `son_degisim_adimi` | u64 | - | yok: 2^64-1 | son değişimin uygulandığı fizik adımı; hiç değişmediyse 2^64-1 |
| `son_degisim_kaynagi` | u8 | - | 0=baslangic, 1=senaryo, 2=operator | son değişimin kaynağı |
| `kart_acil_durdurma` | u8 | - | yok: 255 | kartın bildirdiği etkin değer: A1 `IDA_FC_STATUS.inhibit_mask` bit 4 `ESTOP` (A4 §3.16; 1 = basili); `IDA_FC_STATUS` alınmadıysa 255 |
| `kart_silahlandirma_fisi` | u8[2] | - | yok: 255 | kartın bildirdiği etkin değer: `inhibit_mask` bit 5+i `PLUG_T<i>` kuruluysa 0 (cikarilmis), değilse 1 (takili) (A4 §3.16); dizin 0 = tüp `f1`, dizin 1 = tüp `f2` (A5); `IDA_FC_STATUS` alınmadıysa 255 |
| `kart_emniyet_pimi` | u8[2] | - | yok: 255 | kartın bildirdiği etkin değer: `inhibit_mask` bit 7+i `PIN_T<i>` kuruluysa 1 (takili), değilse 0 (cikarilmis) (A4 §3.16); dizin 0 = tüp `f1`, dizin 1 = tüp `f2` (A5); `IDA_FC_STATUS` alınmadıysa 255 |
| `geri_okuma_farki` | u8 | - | tipin aralığı | yetkili durum ile kartın bildirdiği etkin değer arasındaki anlık fark bitleri: bit0 acil, bit1 fiş f1, bit2 fiş f2, bit3 pim f1, bit4 pim f2; kartın değeri bilinmiyorsa bit 0 |
| `emniyet_rev` | u32 | - | [1, 4294967295] | emniyet revizyonu (iskelet §3.2; A3-K117): 1'den başlar, sıfırlanmaz, `0xFFFFFFFF`'te doyar |
| `emniyet_rev_doydu` | u8 | - | 0=hayir, 1=evet | revizyon doydu; bundan sonra bütün gevşetmeler reddedilir |

#### `yuk_ariza_etkin`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `arizalar` | dizi(u16, en çok 256) | - | tipin aralığı | etkin arızalar |
| `arizalar[].fault_id` | metin(u16, en çok 32) | bayt | tipin aralığı | arıza kimliği; senaryodan gelende A5 kimliği, elle gelende E<n> |
| `arizalar[].sinif` | u8 | - | 0=sensor, 1=aktuator, 2=guc, 3=baglanti, 4=protokol, 5=sistem | arıza sınıfı |
| `arizalar[].hedef` | metin(u16, en çok 32) | bayt | tipin aralığı | hedef (sensor_id, itici, bağlantı yönü); A5 arıza tanımındaki gösterim |
| `arizalar[].tur` | metin(u16, en çok 32) | bayt | tipin aralığı | arıza türü adı (A5) |
| `arizalar[].kaynak` | u8 | - | 0=senaryo, 1=elle | arızanın kaynağı |
| `arizalar[].baslangic_adimi` | u64 | - | tipin aralığı | uygulandığı fizik adımı |
| `arizalar[].bitis_adimi` | u64 | - | yok: 2^64-1 | planlanan bitiş adımı; süresizse 2^64-1 |

#### `yuk_imu`

Ölçüm başlığının 12 alanı, ardından:

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `ivme_mps2` | f32[3] | m/s^2 | tipin aralığı | özgül kuvvet, sensör çerçevesi |
| `acisal_hiz_radps` | f32[3] | rad/s | tipin aralığı | açısal hız, sensör çerçevesi |
| `sicaklik_c` | f32 | degC | yok: NaN | sensör sıcaklığı |

#### `yuk_manyetometre`

Ölçüm başlığının 12 alanı, ardından:

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `alan_t` | f32[3] | T | tipin aralığı | manyetik alan, sensör çerçevesi |
| `sicaklik_c` | f32 | degC | yok: NaN | sensör sıcaklığı |

#### `yuk_basinc`

Ölçüm başlığının 12 alanı, ardından:

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `basinc_pa` | f32 | Pa | [0, 10000000.0] | mutlak basınç |
| `sicaklik_c` | f32 | degC | yok: NaN | su sıcaklığı |

#### `yuk_gnss`

Ölçüm başlığının 12 alanı, ardından:

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `enlem_rad` | f64 | rad | [-1.5707963267948966, 1.5707963267948966] | enlem |
| `boylam_rad` | f64 | rad | [-3.141592653589793, 3.141592653589793] | boylam |
| `yukseklik_m` | f32 | m | tipin aralığı | elipsoit yüksekliği |
| `hiz_ned_mps` | f32[3] | m/s | tipin aralığı | NED hız |
| `eph_m` | f32 | m | tipin aralığı | yatay konum belirsizliği (05) |
| `epv_m` | f32 | m | tipin aralığı | düşey konum belirsizliği (05) |
| `fix_turu` | u8 | - | 0=yok, 2=iki_boyut, 3=uc_boyut | çözüm türü |
| `uydu_sayisi` | u8 | - | tipin aralığı | kullanılan uydu sayısı |

#### `yuk_dvl`

Ölçüm başlığının 12 alanı, ardından:

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `hiz_sensor_mps` | f32[3] | m/s | yok: NaN | dibe göre hız, sensör çerçevesi |
| `irtifa_m` | f32 | m | yok: NaN | dibe yükseklik |
| `fom_mps` | f32 | m/s | yok: NaN | liyakat sayısı (05) |
| `isin_gecerli` | u8 | - | tipin aralığı | bit0-3: dört ışının geçerliliği |

#### `yuk_batarya`

Ölçüm başlığının 12 alanı, ardından:

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `gerilim_v` | f32 | V | tipin aralığı | ölçülen gerilim |
| `akim_a` | f32 | A | tipin aralığı | ölçülen akım |

#### `yuk_sizinti`

Ölçüm başlığının 12 alanı, ardından:

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `prob_maskesi` | u8 | - | tipin aralığı | sızıntı algılayan prob bitleri |

#### `yuk_firlatici`

Ölçüm başlığının 12 alanı, ardından:

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `acil_durdurma` | u8 | - | 0=serbest, 1=basili | kartın okuyacağı sanal acil durdurma (`IDA_SAFETY.estop`; arızayla bozulabilir) |
| `tup_bitleri` | u8[2] | - | tipin aralığı | kartın okuyacağı tüp başına bitler (`IDA_SAFETY.tube[i]`): bit0 `LOADED`, bit1 `PLUG_IN`, bit2 `PIN_OUT`, bit3 `PERMIT_OPEN` (A4 `IDA_TUBE_*`); dizin 0 = tüp `f1`, dizin 1 = tüp `f2` (A5) |

#### `yuk_ping360`

Ölçüm başlığının 12 alanı, ardından:

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `aci_grad` | u16 | grad | [0, 399] | kafa açısı, cihaz biriminde (A1 `angle_grad`); radyan = 2*pi*aci_grad/400 (A3 §3.4) |
| `n_ornek` | u16 | - | [1, 1200] | örnek sayısı (A1 `n_samples`) |
| `ornek_periyodu` | u16 | 25 ns | [80, 40000] | örnek periyodu, 25 ns biriminde (A1 `sample_period`) |
| `darbe_suresi_us` | u16 | us | tipin aralığı | darbe süresi (A1 `tx_duration_us`) |
| `frekans_khz` | u16 | kHz | tipin aralığı | darbe frekansı (A1 `tx_freq_khz`) |
| `kazanc` | u8 | - | tipin aralığı | cihaz kazanç kodu (A1 `gain`) |
| `kademe` | u8 | - | 1=sade, 2=tam | satırı üreten sonar kademesi |
| `ses_hizi_mps` | f32 | m/s | tipin aralığı | sim'in nominal ses hızı; yalnız panelin menzil dönüşümü için (A3 §3.4); kartın parametresi farklı olabilir |
| `ornekler` | bayt_dizisi(u16, en çok 1200) | bayt | tipin aralığı | kutu şiddetleri, 8 bit (A1 `data[n_samples]`) |

#### `yuk_ping1d`

Ölçüm başlığının 12 alanı, ardından:

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `mesafe_m` | f32 | m | yok: NaN | dibe mesafe; yoksa NaN |
| `guven` | f32 | - | [0, 1] | güven |
| `kademe` | u8 | - | 1=sade, 2=tam | sonar kademesi |
| `profil` | bayt_dizisi(u16, en çok 200) | bayt | tipin aralığı | profil örnekleri, 8 bit |

#### `yuk_algilama_kayit`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `kaynak` | u8 | - | 1=ping360, 2=cam_fwd | A8 `source` |
| `inst` | u8 | - | tipin aralığı | A8 `inst` |
| `batch_seq` | u32 | - | tipin aralığı | kaydın ait olduğu partinin `batch_seq`'i (kamerada `kare_sirasi`; A8-K16, K62) |
| `run_gen` | u32 | - | tipin aralığı | partinin çalıştırma nesli (A8 `run_gen` = A2 `calisma_nesli`, A2-K80) |
| `kayit_indeksi` | u8 | - | [0, 3] | partideki sıra, 0'dan |
| `a8_major` | u16 | - | tipin aralığı | A8 şema ana sürümü (`IDA_DET_SCHEMA_MAJOR`) |
| `a8_minor` | u16 | - | tipin aralığı | A8 şema yan sürümü (`IDA_DET_SCHEMA_MINOR`) |
| `sonuc` | u8 | - | 0=karta_iletildi, 1=parti_reddedildi, 2=gec | kaydın akıbeti |
| `kayit` | bayt_dizisi(u16, en çok 512) | bayt | tipin aralığı | algılama kaydının A8 §3.2'deki tel düzeni, değiştirilmeden; içindeki `class_id` kodlamasi A5-K113'un kapalı tablosudur |

#### `yuk_algilama_parti`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `kaynak` | u8 | - | 1=ping360, 2=cam_fwd | A8 `source` |
| `inst` | u8 | - | tipin aralığı | A8 `inst` |
| `batch_seq` | u32 | - | tipin aralığı | A8 `batch_seq` (kamerada `kare_sirasi`; A8-K16, K62); `NO_FRAME`'de 0 |
| `run_gen` | u32 | - | tipin aralığı | karenin ya da yanıtın çalıştırma nesli (A8 `run_gen`; A2-K80) |
| `kare_sirasi` | u32 | - | tipin aralığı | karenin A2 `kare_sirasi`'i (A2-K49) |
| `window_ns` | u32 | ns | tipin aralığı | A8 `window_ns` |
| `calib_ver` | u16 | - | tipin aralığı | A8 `calib_ver` |
| `producer_ver` | u16 | - | tipin aralığı | A8 `producer_ver` |
| `producer_id` | u8 | - | tipin aralığı | A8 `producer_id` |
| `parti_bayraklari` | u16 | - | tipin aralığı | A8 parti `flags` (bit0 `NO_FRAME`) |
| `bilesen_durumu` | u8 | - | yok: 255 | bileşenin bildirdiği `status`; yanıt yoksa ya da parti sim'de üretildiyse 255 |
| `bilesen_kayit_sayisi` | u8 | - | tipin aralığı | bileşenin yanıtındaki `count`; yanıt yoksa 0 |
| `eski_nesil` | u8 | - | 0=hayir, 1=evet | kare ya da yanıt `M_k` anında güncel nesilde değil (A8-K63 adım 0, K79): karta başlık gitmez, `nihai_durum` 255 |
| `a2_dusme_nedeni` | u8 | - | yok: 0 | A2 kareyi teslim etmediyse özgün `SimKareDusmeNedeni` kodu (1-8, 33-39; A8-K76); etmediyse 0 |
| `nihai_durum` | u8 | - | 0=OK, 1=SKIPPED_BUSY, 2=LATE, 3=COMPONENT_DOWN, 4=FRAME_DROPPED, 5=FRAME_INVALID, 6=CALIB_MISMATCH, 7=CAMERA_OFF, 8=COMPONENT_ERROR; yok: 255 | karta giden nihai kare durumu (A8-K63; A8 §3.4); karta başlık gitmediyse 255 |
| `ret_nedeni` | u8 | - | 0=yok, 1=STRUCT, 2=ECHO, 3=CALIB, 4=LINK, 5=SEQ, 6=TIME, 7=KIND, 8=FLAGS, 9=VALUE, 10=UNCERT | sim'in A8-K27 ret nedeni (`IDA_DET_RJ_*`); ret yoksa 0 |
| `gec` | u8 | - | 0=hayir, 1=evet | yanıt `M_k`'dan sonra alındı (A8-K59) |
| `karta_kayit_sayisi` | u8 | - | tipin aralığı | karta giden partinin `count`'u |
| `a1_batch_seq` | u32 | - | yok: 0xFFFFFFFF | partinin karta iletildiği A1 partisi; iletilmediyse 0xFFFFFFFF |

#### `yuk_algilama_sayaclari`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `akislar` | dizi(u8, en çok 4) | - | tipin aralığı | akış başına sim'in kabul kapısı sayaçları (A8-K30, K65); bileşenin iç sayaçları dışa açılmaz |
| `akislar[].kaynak` | u8 | - | 1=ping360, 2=cam_fwd | A8 `source` |
| `akislar[].inst` | u8 | - | tipin aralığı | A8 `inst` |
| `akislar[].ret` | u64[10] | - | tipin aralığı | ret nedeni başına sayaç; sıra `STRUCT`, `ECHO`, `CALIB`, `LINK`, `SEQ`, `TIME`, `KIND`, `FLAGS`, `VALUE`, `UNCERT` (A8-K30) |
| `akislar[].durum` | u64[9] | - | tipin aralığı | karta giden durum başına sayaç; sıra A8 §3.4 değerleri 0-8 (A8-K30, K65) |
| `akislar[].gec` | u64 | - | tipin aralığı | geç yanıt sayısı (A8-K59) |
| `akislar[].eski_nesil` | u64 | - | tipin aralığı | `eski_nesil` ile reddedilen yanıt (A8-K79) |
| `akislar[].bilinmeyen_dusme_nedeni` | u64 | - | tipin aralığı | A8-K76 tablosunda olmayan A2 kodu (sim hatası) |

#### `yuk_kart_baglanti_sayaclari`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `acilis_sayaci` | u32 | - | tipin aralığı | sayaçların ait olduğu kart açılışı (`boot_count`) |
| `rx_bytes` | u32 | - | tipin aralığı | A1 Tablo 3.11b #0 (`IDA_LINK_STATS.cnt[0]`); 2^32'de sarar |
| `rx_frames` | u32 | - | tipin aralığı | A1 Tablo 3.11b #1 (`IDA_LINK_STATS.cnt[1]`); 2^32'de sarar |
| `rx_crc_err` | u32 | - | tipin aralığı | A1 Tablo 3.11b #2 (`IDA_LINK_STATS.cnt[2]`); 2^32'de sarar |
| `rx_unknown_msgid` | u32 | - | tipin aralığı | A1 Tablo 3.11b #3 (`IDA_LINK_STATS.cnt[3]`); 2^32'de sarar |
| `rx_iflag` | u32 | - | tipin aralığı | A1 Tablo 3.11b #4 (`IDA_LINK_STATS.cnt[4]`); 2^32'de sarar |
| `rx_bad_len` | u32 | - | tipin aralığı | A1 Tablo 3.11b #5 (`IDA_LINK_STATS.cnt[5]`); 2^32'de sarar |
| `rx_wrong_sys` | u32 | - | tipin aralığı | A1 Tablo 3.11b #6 (`IDA_LINK_STATS.cnt[6]`); 2^32'de sarar |
| `rx_mav1` | u32 | - | tipin aralığı | A1 Tablo 3.11b #7 (`IDA_LINK_STATS.cnt[7]`); 2^32'de sarar |
| `rx_overflow` | u32 | - | tipin aralığı | A1 Tablo 3.11b #8 (`IDA_LINK_STATS.cnt[8]`); 2^32'de sarar |
| `rx_resync_bytes` | u32 | - | tipin aralığı | A1 Tablo 3.11b #9 (`IDA_LINK_STATS.cnt[9]`); 2^32'de sarar |
| `rx_seq_gap` | u32 | - | tipin aralığı | A1 Tablo 3.11b #10 (`IDA_LINK_STATS.cnt[10]`); 2^32'de sarar |
| `rx_stale_session` | u32 | - | tipin aralığı | A1 Tablo 3.11b #11 (`IDA_LINK_STATS.cnt[11]`); 2^32'de sarar |
| `rx_outside_session` | u32 | - | tipin aralığı | A1 Tablo 3.11b #12 (`IDA_LINK_STATS.cnt[12]`); 2^32'de sarar |
| `stamp_coarse` | u32 | - | tipin aralığı | A1 Tablo 3.11b #13 (`IDA_LINK_STATS.cnt[13]`); 2^32'de sarar |
| `batch_closed` | u32 | - | tipin aralığı | A1 Tablo 3.11b #14 (`IDA_LINK_STATS.cnt[14]`); 2^32'de sarar |
| `batch_incomplete` | u32 | - | tipin aralığı | A1 Tablo 3.11b #15 (`IDA_LINK_STATS.cnt[15]`); 2^32'de sarar |
| `batch_skipped` | u32 | - | tipin aralığı | A1 Tablo 3.11b #16 (`IDA_LINK_STATS.cnt[16]`); 2^32'de sarar |
| `batch_old` | u32 | - | tipin aralığı | A1 Tablo 3.11b #17 (`IDA_LINK_STATS.cnt[17]`); 2^32'de sarar |
| `batch_dup` | u32 | - | tipin aralığı | A1 Tablo 3.11b #18 (`IDA_LINK_STATS.cnt[18]`); 2^32'de sarar |
| `batch_merged` | u32 | - | tipin aralığı | A1 Tablo 3.11b #19 (`IDA_LINK_STATS.cnt[19]`); 2^32'de sarar |
| `member_overflow` | u32 | - | tipin aralığı | A1 Tablo 3.11b #20 (`IDA_LINK_STATS.cnt[20]`); 2^32'de sarar |
| `ping360_incomplete` | u32 | - | tipin aralığı | A1 Tablo 3.11b #21 (`IDA_LINK_STATS.cnt[21]`); 2^32'de sarar |
| `ping360_bad` | u32 | - | tipin aralığı | A1 Tablo 3.11b #22 (`IDA_LINK_STATS.cnt[22]`); 2^32'de sarar |
| `ping360_orphan` | u32 | - | tipin aralığı | A1 Tablo 3.11b #23 (`IDA_LINK_STATS.cnt[23]`); 2^32'de sarar |
| `cmd_deferred` | u32 | - | tipin aralığı | A1 Tablo 3.11b #24 (`IDA_LINK_STATS.cnt[24]`); 2^32'de sarar |
| `cmd_dropped` | u32 | - | tipin aralığı | A1 Tablo 3.11b #25 (`IDA_LINK_STATS.cnt[25]`); 2^32'de sarar |
| `narrow_coalesced` | u32 | - | tipin aralığı | A1 Tablo 3.11b #26 (`IDA_LINK_STATS.cnt[26]`); 2^32'de sarar |
| `cmd_bad_field` | u32 | - | tipin aralığı | A1 Tablo 3.11b #27 (`IDA_LINK_STATS.cnt[27]`); 2^32'de sarar |
| `grid_missed` | u32 | - | tipin aralığı | A1 Tablo 3.11b #28 (`IDA_LINK_STATS.cnt[28]`); 2^32'de sarar |
| `out_fixed` | u32 | - | tipin aralığı | A1 Tablo 3.11b #29 (`IDA_LINK_STATS.cnt[29]`); 2^32'de sarar |
| `tx_frames` | u32 | - | tipin aralığı | A1 Tablo 3.11b #30 (`IDA_LINK_STATS.cnt[30]`); 2^32'de sarar |
| `tx_dropped` | u32 | - | tipin aralığı | A1 Tablo 3.11b #31 (`IDA_LINK_STATS.cnt[31]`); 2^32'de sarar |
| `tx_crit_stall` | u32 | - | tipin aralığı | A1 Tablo 3.11b #32 (`IDA_LINK_STATS.cnt[32]`); 2^32'de sarar |
| `events_sent` | u32 | - | tipin aralığı | A1 Tablo 3.11b #33 (`IDA_LINK_STATS.cnt[33]`); 2^32'de sarar |
| `events_resent` | u32 | - | tipin aralığı | A1 Tablo 3.11b #34 (`IDA_LINK_STATS.cnt[34]`); 2^32'de sarar |
| `sessions` | u32 | - | tipin aralığı | A1 Tablo 3.11b #35 (`IDA_LINK_STATS.cnt[35]`); 2^32'de sarar |

#### `yuk_kart_cekirdek_sayaclari`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `acilis_sayaci` | u32 | - | tipin aralığı | sayaçların ait olduğu kart açılışı (`boot_count`) |
| `sample_rejected` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.sample_rejected` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `sample_future` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.sample_future` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `sample_conflict` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.sample_conflict` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `sample_dup_dropped` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.sample_dup_dropped` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `narrow_from_rejected` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.narrow_from_rejected` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `bg_bad_input` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.bg_bad_input` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `imu_rejected` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.imu_rejected` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `imu_gap` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.imu_gap` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `bad_input` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.bad_input` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `dup_seq` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.dup_seq` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `old_seq` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.old_seq` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `wall_backstep` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.wall_backstep` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `cmd_rejected` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.cmd_rejected` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `widen_blocked` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.widen_blocked` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `evres_ignored` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.evres_ignored` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `event_unknown` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.event_unknown` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `det_rejected` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.det_rejected` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `ping360_rejected` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.ping360_rejected` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `bg_dropped` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.bg_dropped` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `bg_slice_mismatch` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.bg_slice_mismatch` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `log_dropped` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.log_dropped` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `step_miss` | u32 | - | tipin aralığı | A4 `ida_core_counters_t.step_miss` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar |
| `saglik` | u8 | - | tipin aralığı | A4 `ida_health_t` (A1 `IDA_STATUS.health`, `IDAW_HEALTH`): bit0 core_ok, bit1 data_ok, bit2 imu_rejected, bit3 imu_lag, bit4 input_dropped, bit5 narrowed, bit6 sample_conflict, bit7 bg_bad_input |

#### `yuk_kayit_durum`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `acik` | u8 | - | 0=hayir, 1=evet | kayıt dosyası açık mı |
| `kademe` | u8 | - | 0=olaylar, 1=telemetri, 2=tam | kayıt kademesi |
| `kamera_kareleri` | u8 | - | 0=kapali, 1=pencereli, 2=surekli | kamera kare kaydı bayrağı (A6) |
| `yazilan_bayt` | u64 | bayt | tipin aralığı | dosyaya yazılan bayt |
| `kayit_dusen` | u64 | - | tipin aralığı | düşen kayıt iletisi |
| `tekrar_guvencesi` | u8 | - | 0=yok, 1=izleme, 2=fizik, 3=girdi | kaydın şu anki güvencesi (A6) |
| `dosya` | metin(u16, en çok 64) | bayt | tipin aralığı | kayıt dosyasının adı |

#### `yuk_surec_durum`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `surecler` | dizi(u8, en çok 4) | - | tipin aralığı | yardımcı süreçler |
| `surecler[].surec` | u8 | - | 0=gorsellestirici, 1=algilama | yardımcı süreç |
| `surecler[].durum` | u8 | - | 0=kapali_kademe, 1=baslatiliyor, 2=calisiyor, 3=dustu, 4=durduruldu | süreç durumu |
| `surecler[].yeniden_baslatma` | u32 | - | tipin aralığı | bu çalıştırmadaki yeniden başlatma sayısı |
| `surecler[].son_kare_t_ns` | i64 | ns | yok: INT64_MIN | son kamera karesinin ölçüm zamanı; yoksa INT64_MIN |

#### `yuk_baglanti_durum`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `a1_oturum` | u8 | - | 0=kapali, 1=el_sikisma, 2=hazir, 3=kosuyor, 4=kopuk, 255=uygulanamaz | A1 oturum durumu |
| `sess` | u16 | - | tipin aralığı | A1 oturum numarası; yoksa 0 |
| `acilis_sayaci` | u32 | - | tipin aralığı | kartın açılış sayacı; yoksa 0 |
| `d_asagi_ort_ns` | i64 | ns | yok: INT64_MIN | aşağı yön teslim gecikmesi kestiriminin ortalaması (06 §2.5); yoksa INT64_MIN |
| `emulator` | dizi(u8, en çok 2) | - | tipin aralığı | bağlantı emülatörü |
| `emulator[].yon` | u8 | - | 0=yer_to_arac, 1=arac_to_yer | yön |
| `emulator[].kuyruk_bayt` | u32 | bayt | tipin aralığı | emülatör kuyruğundaki bayt |
| `emulator[].dusen` | u64 | - | tipin aralığı | emülatörün düşürdüğü çerçeve |
| `emulator[].model_tutulamadi` | u64 | - | tipin aralığı | 06 §2.11 "model tutulamadı" işareti |
| `bu_baglanti_ezilen_durum` | u64 | - | tipin aralığı | bu WebSocket bağlantısında gönderilmeden ezilen durum sınıfı çerçeve (A3-K40) |
| `bu_baglanti_dusen_dizi` | u64 | - | tipin aralığı | bu bağlantıda kuyruktan düşen dizi sınıfı çerçeve (A3-K41) |
| `bu_baglanti_olay_kuyrugu` | u32 | - | tipin aralığı | bu bağlantının olay kuyruğundaki çerçeve sayısı |

#### `yuk_yer_istasyonu_mavlink`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `yon` | u8 | - | 0=yer_to_arac, 1=arac_to_yer | yön |
| `cerceve` | bayt_dizisi(u16, en çok 280) | bayt | tipin aralığı | yer istasyonu düzlemindeki MAVLink 2 çerçevesi, emülatör çıktısı (A1) |

#### `yuk_kamera_halka_sayaclari`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `kamera_kimligi` | u8 | - | tipin aralığı | A2 `kamera_kimligi` |
| `dusen_sim` | u64[8] | - | tipin aralığı | sim nedenli düşen kare; sıra `SimKareDusmeNedeni` 1-8 (8 = `halka_emekli_hazir`) |
| `dusen_gorsellestirici` | u64[7] | - | tipin aralığı | görselleştirici nedenli düşen kare; sıra kod 33-39 |
| `gec_hazir` | u64 | - | tipin aralığı | A2-K48 |
| `tuketici_gec` | u64 | - | tipin aralığı | A2-K59 |
| `tuketici_koptu` | u64 | - | tipin aralığı | A2-K59 |
| `halka_emekliye_ayirma` | u64 | - | tipin aralığı | A2-K86 |
| `emekli_halka_yaniti` | u64 | - | tipin aralığı | A2-K87 |
| `tuketici_kamera_kapali` | u64 | - | tipin aralığı | A2-K89 |
| `tutulan_emekli_halka` | u32 | - | tipin aralığı | su an tutulan emekli halka sayısı (A2-K88) |
| `eski_nesil` | u64 | - | tipin aralığı | `eski_nesil` sonuç reddi (A2 §5) |
| `yok_sayilan_birakma` | u64 | - | tipin aralığı | A2 §5 |
| `seyreltilen_durum` | u64 | - | tipin aralığı | A2 §5 |
| `protokol_hatasi` | u64 | - | tipin aralığı | A2 §5 |
| `baglanti` | u64 | - | tipin aralığı | A2 bağlantı sayısı |
| `kopma` | u64 | - | tipin aralığı | A2 kopma sayısı; A2 oturum kapanış nedeni `surec_durumu.a2_veda_nedeni`'nde |
| `istek_hazir_adet` | u64 | - | tipin aralığı | istekten hazıra süresi ölçülen kare (yuva başlığındaki duvar saatlerinden; A2 §5) |
| `istek_hazir_toplam_ns` | u64 | ns | tipin aralığı | bu sürelerin toplamı |
| `istek_hazir_en_buyuk_ns` | u64 | ns | tipin aralığı | en büyük süre |
| `istek_hazir_dagilim` | u64[8] | - | tipin aralığı | dağılım kovaları; sıra <5, <10, <20, <33, <50, <100, <200, >=200 ms (kova sınırları tahmin) |

**Ping360 satırının türetilmiş değerleri.** `yuk_ping360` cihaz birimindeki özgün alanları taşır (A1 §3.4, A4 `ida_ping360_t`); panel türetilmiş değerleri şu formüllerle hesaplar: kafa açısı `aci_rad = 2π · aci_grad / 400`; kutu `i`'nin (0 … `n_ornek − 1`) nominal eğik menzili `r_i = c · (2i + 1) · ornek_periyodu · 25 ns / 4` ve nominal yansıma anı `t_i = t_ns + (2i + 1) · ornek_periyodu · 25 ns / 4` (A1-K52); `c` = `ses_hizi_mps`. Darbe süresi bu formüllere girmez (A1-K52). Kayıtta yalnız özgün alanlar saklanır (A6).

**Geçerlilik neden bitleri** (`gecerlilik_nedenleri`; `x_ida_gecerlilik_nedenleri`): Tablo 4.8.

### 3.5 Olay türleri

`olay` kanalındaki `tur` alanının değerleri. `ayrinti` alanı UTF-8 JSON nesnesidir ve `a3-kanallar.json` içindeki `$defs/olay_<ad>` şemasına uyar.

| Kod | Ad | Varsayılan önem | Ne zaman |
|---|---|---|---|
| 1 | `calistirma_durumu_degisti` | bilgi | Tablo 4.7a'daki her geçişte |
| 2 | `gecerlilik_dustu` | uyarı | Bir geçersiz kılan neden ilk kez oluştuğunda (neden başına bir kez) |
| 3 | `gercek_zaman_kacirildi` | uyarı | `gercek_zamani_kacirdi` 0'dan 1'e geçtiğinde (çalıştırma başına bir kez); `neden` ve `neden_biti` geçersizliği ilk kuran Tablo 4.8 nedenidir (bit 0–11, kapalı) |
| 4 | `parti_kacirildi_ozeti` | uyarı | Kaçırılan partileri en çok saniyede bir özetler |
| 5 | `komut_sonucu` | bilgi | Asenkron komutun nihai sonucu |
| 6 | `ariza_degisti` | bilgi | Arıza uygulandı, kaldırıldı, bitti ya da reddedildi |
| 7 | `emniyet_girdisi_degisti` | emniyet | Sanal emniyet girdisi değişti |
| 8 | `atis_olayi_sonucu` | emniyet | Sim, karttan gelen bir tek seferlik olay için karar verdi |
| 9 | `a1_oturum_degisti` | bilgi | A1 oturum durumu değişti |
| 10 | `kart_sifirlandi` | uyarı | Kartın açılış sayacı değişti |
| 11 | `yer_istasyonu_komutu` | bilgi | Yer istasyonu komutunun iletim aşaması |
| 12 | `yer_istasyonu_yaniti` | bilgi | Kartın yer istasyonu komutuna yanıtı |
| 13 | `senaryo_yuklendi` | bilgi | Senaryo yüklendi |
| 14 | `on_denetim_sonucu` | bilgi | Ön denetim bitti |
| 15 | `kayit_durumu` | bilgi | Kayıt açıldı, kapandı ya da yazılamadı |
| 16 | `kayit_eksik` | uyarı | Kayda yazılamayan ileti oldu (A6 §4.5) |
| 17 | `surec_durumu` | bilgi | Görselleştirici ya da algılama bileşeninin durumu değişti |
| 18 | `olay_kaybi` | hata | A3 halka tamponunda olay düştü (A3-K44) |
| 19 | `erisim_reddedildi` | uyarı | Erişim denetiminde reddedilen istek (A3-K12) |
| 20 | `izleme_durumu` | bilgi | İzleme kipinde konum ya da oynatma durumu değişti |
| 21 | `senaryo_ogesi_uygulandi` | bilgi | Senaryo zaman çizelgesindeki bir öğe uygulandı, reddedildi ya da (çalıştırma sonunda) uygulanmadı; A5 §3.15 "olay sonucu" alanlarıyla |
| 22 | `uyari` | uyarı | Başka türe girmeyen uyarı (kod + ileti) |
| 23 | `tekrar_mudahalesi` | uyarı | Senaryoyu yeniden simüle eden tekrar kipinde ilk müdahale kabul edildi; koşu yeni deney sayılır (A3-K120) |
| 24 | `yapilandirma_reddedildi` | hata | A1-K104: aralık dışı parametre ve sağlanmayan eşitsizlik (E1–E19) |
| 25 | `atama_celiskisi` | uyarı | A5-K63; alanlar A5 §3.15'teki adlarla (değerler RFC 8785 JSON metni olarak); sekizden çok öğeli çelişki parçalara bölünür (A3-K125) |
| 26 | `gevsetme_reddedildi` | emniyet | Senaryonun gevşeten ataması, o sınırda revizyon 1–3. adımlarda arttığı ya da doyduğu için reddedildi (iskelet §3.2 adım 4; W1-uyum U3); alanlar A5 §3.15 |
| 27 | `donma_onceki_cikti_yok` | uyarı | A5 Tablo 3.10-A `donma` arızasında dondurulacak çıktı yok; alanlar A5 §3.15 |
| 28 | `halka_emekliye_ayrildi` | uyarı | A2-K86 |
| 29 | `tuketici_kamera_kapali` | uyarı | A2-K89 |
| 30 | `emniyet_rev_doydu` | emniyet | Emniyet revizyonu `0xFFFFFFFF`'e ulaştı; bundan sonra gevşetme kabul edilmez (iskelet §3.2) |

**Tablo 3.5b — Olay ayrıntısının en kötü durum boyutu.** `ayrinti` alanı en çok 8192 bayttır (`yuk_olay`). Serileştirme politikası A3-K121'dedir: boşluksuz JSON, ASCII dışı karakterler ham UTF-8 (kod noktası başına en çok 4 bayt), `\u` kaçışı yok, `"` ve `\` 2 bayt, denetim karakteri yasak, her tamsayı ±(2⁵³ − 1), her ondalık sayı f32 aralığı içinde sınırlı (ondalık sayının dizilimi en çok 24 bayt). "Hesap" sütunu şemadan çıkarılan üst sınırdır; "Üretilen en kötü örnek" sütunu, denetim betiğinin şemaya uyan en uzun örneği (serbest dizelerde U+1F600, desenli dizelerde desenin en uzun eşleşmesi, tamsayılarda en uzun sınır değeri, dizilerde ve ek nesnelerde en çok öğe) gerçekten üretip bu politikayla serileştirdiği boydur. Hesap bu işte yapıldı (A6 §7.1); ölçüm değildir.

| Olay türü | En kötü durum, hesap (bayt) | Üretilen en kötü örnek (bayt) |
|---|---:|---:|
| `calistirma_durumu_degisti` | 1085 | 1085 |
| `gecerlilik_dustu` | 64 | 64 |
| `gercek_zaman_kacirildi` | 112 | 112 |
| `parti_kacirildi_ozeti` | 125 | 125 |
| `komut_sonucu` | 4966 | 4966 |
| `ariza_degisti` | 1252 | 1252 |
| `emniyet_girdisi_degisti` | 181 | 181 |
| `atis_olayi_sonucu` | 256 | 256 |
| `a1_oturum_degisti` | 133 | 133 |
| `kart_sifirlandi` | 94 | 94 |
| `yer_istasyonu_komutu` | 151 | 151 |
| `yer_istasyonu_yaniti` | 4836 | 4836 |
| `senaryo_yuklendi` | 611 | 611 |
| `on_denetim_sonucu` | 2104 | 2104 |
| `kayit_durumu` | 313 | 313 |
| `kayit_eksik` | 356 | 356 |
| `surec_durumu` | 1150 | 1150 |
| `olay_kaybi` | 33 | 33 |
| `erisim_reddedildi` | 1081 | 1081 |
| `izleme_durumu` | 86 | 86 |
| `senaryo_ogesi_uygulandi` | 185 | 185 |
| `uyari` | 4181 | 4181 |
| `tekrar_mudahalesi` | 81 | 81 |
| `yapilandirma_reddedildi` | 77 | 77 |
| `atama_celiskisi` | 2906 | 2906 |
| `gevsetme_reddedildi` | 554 | 554 |
| `donma_onceki_cikti_yok` | 170 | 170 |
| `halka_emekliye_ayrildi` | 107 | 107 |
| `tuketici_kamera_kapali` | 52 | 52 |
| `emniyet_rev_doydu` | 26 | 26 |

### 3.6 HTTP uçları

Bütün uçlar `/api/v1` altındadır. "Tür": `senkron` uç işlemi bitirip 200 döner; `asenkron` uç eşzamanlı denetimlerden sonra 202 döner, nihai sonuç `komut_sonucu` olayıyla gelir (A3-K56, K57). "Komut" sütunu `evet` olan uçlar durum değiştirir (A3-K50…K54). İstek ve yanıt gövdeleri `a3-kanallar.json` içindeki `$defs` şemalarıdır; bütün ret yanıtları `$defs/yanit_ret` biçimindedir.

| Yöntem | Yol | İstek gövdesi | Yanıt | Tür | Komut |
|---|---|---|---|---|---|
| GET | `/api/v1/surum` | — | `yanit_surum` | senkron | hayır |
| GET | `/api/v1/katalog` | — | bu dosya (`a3-kanallar.json`), değiştirilmeden | senkron | hayır |
| POST | `/api/v1/istemciler` | `istek_istemci` | `yanit_istemci` | senkron | hayır |
| DELETE | `/api/v1/istemciler/{istemci_id}` | — | `yanit_kabul` | senkron | hayır |
| GET | `/api/v1/durum` | — | `yanit_durum` | senkron | hayır |
| GET | `/api/v1/on_denetim` | — | `on_denetim_raporu` | senkron | hayır |
| GET | `/api/v1/on_denetim/{rapor_kimligi}` | — | `on_denetim_raporu` | senkron | hayır |
| GET | `/api/v1/komutlar/{komut_no}` | — | `yanit_komut` | senkron | hayır |
| GET | `/api/v1/olaylar?sonra={sira}&en_cok={n}` | — | `yanit_olaylar` | senkron | hayır |
| GET | `/api/v1/senaryolar` | — | `senaryo_listesi` | senkron | hayır |
| GET | `/api/v1/senaryolar/{ad}` | — | `yanit_senaryo_dosyasi` | senkron | hayır |
| PUT | `/api/v1/senaryolar/{ad}` | `istek_senaryo_kaydet` | `yanit_senaryo_dogrula` | senkron | evet |
| POST | `/api/v1/senaryo/dogrula` | `istek_senaryo_dogrula` | `yanit_senaryo_dogrula` | senkron | hayır |
| POST | `/api/v1/senaryo/yukle` | `istek_senaryo_yukle` | `yanit_senaryo_yukle` | senkron | evet |
| GET | `/api/v1/senaryo/cozumlenmis` | — | `application/json`: A5 kanonik baytlari, degistirilmeden; ozet `Ida-Davranis-Ozeti` basliginda (A3-K80) | senkron | hayır |
| POST | `/api/v1/calistirma/baslat` | `istek_baslat` | `yanit_kuyrukta` | asenkron | evet |
| POST | `/api/v1/calistirma/duraklat` | `istek_duraklat` | `yanit_kuyrukta` | asenkron | evet |
| POST | `/api/v1/calistirma/surdur` | `istek_calistirma` | `yanit_kuyrukta` | asenkron | evet |
| POST | `/api/v1/calistirma/adimla` | `istek_adimla` | `yanit_kuyrukta` | asenkron | evet |
| POST | `/api/v1/calistirma/hiz` | `istek_hiz` | `yanit_kuyrukta` | asenkron | evet |
| POST | `/api/v1/calistirma/durdur` | `istek_calistirma` | `yanit_kuyrukta` | asenkron | evet |
| POST | `/api/v1/calistirma/sifirla` | `istek_sifirla` | `yanit_kuyrukta` | asenkron | evet |
| POST | `/api/v1/calistirma/yeniden_kostur` | `istek_yeniden_kostur` | `yanit_kuyrukta` | asenkron | evet |
| GET | `/api/v1/arizalar/katalog` | — | `ariza_katalogu` | senkron | hayır |
| GET | `/api/v1/arizalar/etkin` | — | `yanit_ariza_etkin` | senkron | hayır |
| POST | `/api/v1/arizalar` | `istek_ariza` | `yanit_kuyrukta_ariza` | asenkron | evet |
| POST | `/api/v1/arizalar/{fault_id}/kaldir` | `istek_ariza_kaldir` | `yanit_kuyrukta` | asenkron | evet |
| GET | `/api/v1/emniyet` | — | `yanit_emniyet` | senkron | hayır |
| POST | `/api/v1/emniyet/onay_jetonu` | `istek_onay_jetonu_emniyet` | `yanit_onay_jetonu` | senkron | hayır |
| POST | `/api/v1/emniyet/degistir` | `istek_emniyet_degistir` | `yanit_kuyrukta` | asenkron | evet |
| GET | `/api/v1/yer_istasyonu/komutlar` | — | `a1_komut_listesi` | senkron | hayır |
| POST | `/api/v1/yer_istasyonu/onay_jetonu` | `istek_onay_jetonu_yer_istasyonu` | `yanit_onay_jetonu` | senkron | hayır |
| POST | `/api/v1/yer_istasyonu/komut` | `istek_yer_istasyonu_komut` | `yanit_kuyrukta` | asenkron | evet |
| GET | `/api/v1/kayitlar` | — | `yanit_kayit_listesi` | senkron | hayır |
| GET | `/api/v1/kayitlar/{run_id}` | — | `yanit_kayit_ust_verisi` | senkron | hayır |
| GET | `/api/v1/kayitlar/{run_id}/dosya` | — | `application/octet-stream` (MCAP dosyası) | senkron | hayır |
| POST | `/api/v1/kayitlar/{run_id}/sil` | `istek_kayit_sil` | `yanit_kabul` | senkron | evet |
| POST | `/api/v1/izleme/baslat` | `istek_izleme_baslat` | `yanit_kuyrukta` | asenkron | evet |
| POST | `/api/v1/izleme/oynat` | `istek_izleme` | `yanit_kuyrukta` | asenkron | evet |
| POST | `/api/v1/izleme/duraklat` | `istek_izleme` | `yanit_kuyrukta` | asenkron | evet |
| POST | `/api/v1/izleme/ara` | `istek_izleme_ara` | `yanit_kuyrukta` | asenkron | evet |
| POST | `/api/v1/izleme/hiz` | `istek_izleme_hiz` | `yanit_kuyrukta` | asenkron | evet |
| POST | `/api/v1/izleme/bitir` | `istek_izleme` | `yanit_kuyrukta` | asenkron | evet |
| PUT | `/api/v1/yayin` | `istek_yayin` | `yanit_kabul` | senkron | evet |
| POST | `/api/v1/surecler/{surec}/baslat` | `istek_surec` | `yanit_kuyrukta` | asenkron | evet |
| POST | `/api/v1/surecler/{surec}/durdur` | `istek_surec` | `yanit_kuyrukta` | asenkron | evet |

**Yoktur:** atış tetikleyen, sim fırlatıcısını doğrudan değiştiren, `ateşle` olayı üreten ya da yer istasyonu komutunu emülatörü atlayarak karta gönderen bir uç (A3-K99, A3-K101). Yukarıdaki liste uçların tamamıdır.

### 3.7 WebSocket metin iletileri

| Yön | İleti | Şema | Ne zaman |
|---|---|---|---|
| sunucu → istemci | `merhaba` | `$defs/ws_merhaba` | Bağlantının ilk iletisi (A3-K19) |
| istemci → sunucu | `abone_ol` | `$defs/ws_abone_ol` | Kanal aboneliği (A3-K30) |
| istemci → sunucu | `abonelik_birak` | `$defs/ws_abonelik_birak` | Abonelik bırakma |
| sunucu → istemci | `abonelik_yaniti` | `$defs/ws_abonelik_yaniti` | Her abonelik iletisine bir yanıt (A3-K31) |
| sunucu → istemci | `hata` | `$defs/ws_hata` | Çözülemeyen ya da bilinmeyen istemci iletisi (A3-K48) |

WebSocket kapanış kodları: 1000, 1001, 1002, 1003, 1007, 1009, 1011 RFC 6455 §7.4.1'deki anlamlarıyla; 4001 (olay kuyruğu taştı), 4002 (erişim jetonu geçersizleşti), 4004 (canlılık yok) bu sözleşmenin özel kodlarıdır (RFC 6455 §7.4.2: 4000–4999 özel kullanım içindir).

### 3.8 Ön denetim raporu

Çalıştırma başlamadan önce sim sunucusunun yaptığı denetimlerin sonucu (AB-056). Şema `$defs/on_denetim_raporu`; her madde `$defs/on_denetim_maddesi`. Tam rapor yalnız HTTP ile alınır; `on_denetim_sonucu` olayı yalnız rapor kimliğini ve sınırlı bir özeti taşır (A3-K62; en kötü durum 2104 bayt, Tablo 3.5b).

| Alan | Tip | Anlam |
|---|---|---|
| `rapor_surumu` | metin, `"1.0"` | rapor biçiminin sürümü |
| `rapor_kimligi` | 16 onaltılık karakter | raporun kimliği; `GET /api/v1/on_denetim/{rapor_kimligi}` |
| `sonuc` | `gecti` \| `kaldi` | en az bir madde `kaldi` ise `kaldi` |
| `kip` | kip adı | denetlenen zaman kipi |
| `olusturma_utc` | ISO 8601 | raporun oluşturulduğu an |
| `maddeler[]` | dizi, en çok 32 öğe | `kod` (en çok 32 karakter, `^[a-z0-9_]{1,32}$`), `durum` (`gecti`/`kaldi`/`uyari`/`atlandi`), `olculen`, `sinir`, `birim`, `ileti`, `dayanak` |

1.0'da tanımlı madde kodları (liste küçük sürümle genişler; istemci bilinmeyen kodu olduğu gibi gösterir):

| Kod | Ne denetlenir | Sınır ve dayanağı |
|---|---|---|
| `senaryo_gecerli` | Yüklü senaryo A5 doğrulamasından geçti mi | A5 |
| `surum_uyumu` | A1 el sıkışmasında sürüm, şema özeti ve kontrol hızı uyumu (HIL ve SIL) | A1 sözleşmesi; 06 §2.9 |
| `kontrol_hizi` | Kartın ve sim'in kontrol hızı | 200 Hz (iskelet §4) |
| `a1_bant_payi` | Senaryonun sensör ve sonar ayarlarından hesaplanan, her yönde kontrol periyodu başına ortalama A1 yükü | ≤ 900 bayt (iskelet §3, A1 kapasitesi paragrafı) |
| `a7_manifest` | A7 manifest özetinin sim, sonar ve görselleştiricide aynı olması | A7 sözleşmesi |
| `ping360_zamanlama` | Ping360 ayarlarının zamanlama modeline uygunluğu | 05 §3 `ping360_timing`; A1 |
| `gorsellestirici` | Kamera ya da 3B görünüm açıksa görselleştiricinin bağlanması | A2 sözleşmesi |
| `algilama_bileseni` | Kamera açıksa algılama bileşeninin bağlanması | A8 sözleşmesi |
| `kayit_disk_alani` | Kayıt dizininde boş alan | ≥ 1 GiB (mimari seçimi, tahmin; 8. başlık md.6) |

### 3.9 Hata yanıtı ve hata kodları

Her ret yanıtı `{"sonuc": "ret", "komut_no": <sayı ya da null>, "hata": {"kod", "ileti", "ayrinti"}}` biçimindedir (`$defs/yanit_ret`). `kod` aşağıdaki listeden biridir; `ileti` Türkçe ve insan içindir, istemci mantığı yalnız `kod`'a bakar. `ayrinti` koda özgü nesnedir (ör. `senaryo_gecersiz` için `{"rapor": <A5 raporu>}`, `beklenen_durum_uyusmuyor` için `{"guncel_run_id", "guncel_durum"}`).

| Kod | HTTP | Anlam |
|---|---|---|
| `erisim_reddedildi` | 403 | jeton yok ya da yanlış; Host ya da Origin izinli değil |
| `bulunamadi` | 404 | uç ya da kaynak yok |
| `yontem_desteklenmiyor` | 405 | uç bu yöntemi desteklemiyor |
| `gecersiz_istek` | 400 | JSON çözülemedi, zorunlu alan yok ya da şema dışı |
| `gecersiz_deger` | 422 | alan şemaya uygun ama değer aralığı ya da anlamı geçersiz |
| `senaryo_gecersiz` | 422 | A5 doğrulaması geçmedi; `ayrinti.rapor` A5 raporu |
| `govde_cok_buyuk` | 413 | istek gövdesi sınırdan büyük |
| `icerik_turu` | 415 | Content-Type application/json değil |
| `gecersiz_durum` | 409 | komut bu çalıştırma durumunda geçersiz |
| `kipte_gecersiz` | 409 | komut bu zaman kipinde geçersiz |
| `beklenen_durum_uyusmuyor` | 409 | `beklenen_run_id` güncel `run_id` değil ya da çalıştırma yokken `null` değil (A3-K124) |
| `onay_gerekli` | 409 | onay ya da onay jetonu eksik |
| `onay_jetonu_gecersiz` | 409 | jeton yok, süresi geçmiş, kullanılmış ya da başka istemci/girdi/komut için |
| `kimlik_cakismasi` | 409 | aynı istemci oturumunda aynı istek_kimligi farklı yöntem, yol ya da gövdeyle (A3-K53) |
| `emniyet_rev_uyusmuyor` | 409 | `beklenen_emniyet_rev` güncel emniyet revizyonu değil; `ayrinti.guncel_emniyet_rev` (A3-K118) |
| `durum_rev_uyusmuyor` | 409 | `beklenen_durum_rev` güncel durum revizyonu değil; `ayrinti.guncel_durum_rev` (A3-K123) |
| `emniyet_rev_doydu` | 409 | emniyet revizyonu doydu; gevşetme kabul edilmez (iskelet §3.2; A3-K117) |
| `kisitlama_hakim` | 409 | aynı adım sınırında aynı girdi için kısıtlayan bir değişiklik var; yalnız `komut_sonucu` olayında kullanılır (A3-K95) |
| `tekrar_kosulu_saglanmiyor` | 409 | sürüm, platform ya da özet koşulu sağlanmıyor; `ayrinti.nedenler` (A3-K68) |
| `tekrar_kipinde_yasak` | 409 | kayıtlı girdiyle beslenen tekrar kipinde girdiyi değiştiren müdahale (A3-K120) |
| `izleme_etkin` | 409 | izleme sürerken canlı çalıştırma komutu |
| `canli_calistirma_yok` | 409 | komut canlı çalıştırma gerektiriyor |
| `komut_listede_yok` | 422 | yer istasyonu komutu A1 listesinde yok |
| `kayit_yok` | 404 | kayıt dosyası yok |
| `kayit_okunamadi` | 422 | kayıt bozuk ya da sürümü uyumsuz (A6 §5) |
| `gecmis_yok` | 410 | istenen olay sırası sunucu geçmişinde artık yok |
| `mesgul` | 503 | sunucu kapanıyor ya da komut kuyruğu dolu |
| `sunucu_hatasi` | 500 | beklenmeyen iç hata |
| `on_denetim_kaldi` | 409 | ön denetim geçmedi; yalnız `komut_sonucu` olayında ve `GET /komutlar` yanıtında kullanılır (A3-K62) |
| `dosya_var` | 409 | senaryo dosyası var ve `uzerine_yaz` `true` değil (A3-K79) |

HTTP 401 kullanılmaz: RFC 9110 §15.5.2 401 yanıtında `WWW-Authenticate` başlığı ister; A3 erişim reddinde 403 döner (RFC 9110 §15.5.4; RFC 6455 §10.2 da kabul edilmeyen Origin için 403 önerir).

---

## 4. Davranış kuralları

Her kural numaralı ve sınanabilirdir. "Sunucu" sim sunucusunun API bileşenidir; "dünya" dünya iş parçacığıdır (04 §2.12).

### 4.1 Taşıma ve erişim

- **A3-K01** Sunucu A3'ü tek bir TCP portunda HTTP/1.1 (RFC 9110, RFC 9112) ve WebSocket (RFC 6455) olarak sunar. Varsayılan port 47100'dür (mimari seçimi); komut satırından değiştirilir; 0 verilirse port işletim sistemince atanır ve atanan port standart çıktıya yazılır.
- **A3-K02** Sunucu yalnız döngü arabirimi adreslerine (`127.0.0.1`, `::1`) bağlanır. Yapılandırmada başka bir adres verilirse sunucu dinlemeye başlamaz ve sim sunucusu hata koduyla çıkar.
- **A3-K03** A3'ün 1.x sürümlerinde döngü arabirimi dışına bağlanma seçeneği bulunmaz. Bu seçenek ancak kimlik doğrulama ve taşıma şifrelemesini tanımlayan yeni bir büyük sürümle ve mimar kararıyla eklenir.
- **A3-K04** Sunucu her başlatmada işletim sisteminin kriptografik rastgele kaynağından 32 baytlık bir erişim jetonu üretir ve onu 64 karakterlik küçük harfli onaltılık metin olarak kullanır. Jeton süreç boyunca değişmez, diske yazılmaz ve standart çıktıya yalnız açılış adresinin parçası olarak bir kez yazılır.
- **A3-K05** Açılış adresi `http://127.0.0.1:<port>/#erisim=<jeton>` biçimindedir. Jeton adresin parça (fragment) kısmındadır; tarayıcı bu kısmı HTTP isteğiyle sunucuya göndermez (RFC 3986 §3.5), panel jetonu buradan okur.
- **A3-K06** `/api/` altındaki her HTTP isteği `Authorization: Bearer <jeton>` başlığını taşır. Başlık yoksa ya da jeton eşleşmezse sunucu isteği işlemeden 403 ve `erisim_reddedildi` döner. Jeton karşılaştırması sabit sürelidir.
- **A3-K07** WebSocket el sıkışması yalnız `/ws` yolunda yapılır. İstemci `Sec-WebSocket-Protocol` başlığında `ida-a3.v1` ve `ida-erisim.<jeton>` değerlerini önerir; tarayıcıda bu `new WebSocket(url, ["ida-a3.v1", "ida-erisim." + jeton])` çağrısıdır (WHATWG WebSockets: kurucu yalnız adres ve alt protokol listesi alır, başka başlık eklenemez). Jeton geçerliyse sunucu yanıtta yalnız `ida-a3.v1` değerini seçer ve jetonu yansıtmaz (RFC 6455 §4.2.2). Jeton yoksa, geçersizse ya da `ida-a3.v1` önerilmemişse sunucu el sıkışmayı 403 ile reddeder.
- **A3-K08** Her HTTP isteğinde ve el sıkışmada `Host` başlığı `127.0.0.1:<port>`, `localhost:<port>` ya da `[::1]:<port>` değilse sunucu 403 döner (DNS yeniden bağlama saldırısına karşı).
- **A3-K09** İstekte `Origin` başlığı varsa değeri `http://127.0.0.1:<port>`, `http://localhost:<port>` ya da `http://[::1]:<port>` olmalıdır; değilse sunucu 403 döner (RFC 6455 §10.2). `Origin` başlığı olmayan istek (tarayıcı dışı araç; RFC 6455 §4.1'e göre tarayıcı her el sıkışmada `Origin` gönderir) yalnız jetonla değerlendirilir.
- **A3-K10** Sunucu hiçbir yanıtta `Access-Control-Allow-*` başlığı göndermez ve `OPTIONS` isteklerini 405 ile yanıtlar.
- **A3-K11** Ön yüzün statik dosyaları `GET /` ve `GET /panel/...` ile jetonsuz sunulur; bu yollar sim verisi içermez ve A3-K08 bunlara da uygulanır. `/api/`, `/ws` ve statik yollar dışındaki her yol 404, statik yollarda GET dışındaki yöntem 405 döner. Gövde taşıyan durum değiştirici istekler POST ya da PUT'tur; DELETE yalnız gövdesiz `DELETE /api/v1/istemciler/{istemci_id}` için kullanılır.
- **A3-K12** Erişim denetiminde reddedilen her istek sayılır ve `erisim_reddedildi` olayı üretir (yol ve neden; gövde ve jeton kaydedilmez). Aynı neden için saniyede en çok bir olay üretilir; aradaki retler bir sonraki olayın `adet` alanında toplanır.

### 4.2 Sürüm ve el sıkışma

- **A3-K13** A3 sürümü `büyük.küçük` biçimindedir; bu sözleşme `1.0`'dır. HTTP taban yolu `/api/v<büyük>`, WebSocket alt protokolü `ida-a3.v<büyük>`'tür.
- **A3-K14** Küçük sürüm artışı yalnız şunları ekleyebilir: yeni kanal; var olan bir yükün **sonuna** alan (o kanalın `kanal_surumu` bir artar); yeni olay türü; yeni hata kodu; yeni ön denetim kodu; JSON nesnelerine isteğe bağlı alan; yeni uç. Başka her değişiklik (alan silme, tip, birim ya da anlam değişikliği, alan sırası, kanal kimliği değişikliği) büyük sürüm artışıdır.
- **A3-K15** Panel WebSocket açmadan önce `GET /api/v1/surum` çağırır. `a3_surumu`'nun büyük kısmı kendi derlendiği sürümden farklıysa bağlanmaz ve kullanıcıya sürüm uyuşmazlığını gösterir; yalnız küçük kısım ya da `katalog_ozeti` farklıysa bağlanır ve farkı uyarı olarak gösterir.
- **A3-K16** `katalog_ozeti`, `sha256:` öneki ile sunucuya gömülü `a3-kanallar.json` dosyasının baytlarının SHA-256 özetinin küçük harfli onaltılık gösterimidir. `GET /api/v1/katalog` aynı baytları değiştirmeden döndürür.
- **A3-K17** WebSocket açıldığında sunucunun ilk iletisi `merhaba` metin iletisidir; sunucu bundan önce ikili çerçeve göndermez.
- **A3-K18** Alıcı bilinmeyen `kanal_id`'li çerçeveyi `yuk_uzunlugu` kadar atlar ve bağlantıyı kapatmaz. JSON yanıtlarında ve olay ayrıntılarında bilinmeyen alanları yok sayar; bilinmeyen olay türünü kodu ve ayrıntısıyla gösterir; bilinmeyen hata kodunu `ileti` alanıyla gösterir.
- **A3-K19** Alıcı, bildiğinden büyük `kanal_surumu`'lu çerçevede bildiği alanları okur ve yükün sonundaki fazla baytları yok sayar; bildiğinden küçük `kanal_surumu`'lu çerçevede yükün sonunda olmayan alanları "yok" sayar.

### 4.3 İkili çerçeve ve zaman

- **A3-K20** Sunucudan istemciye bütün kanal verisi WebSocket ikili iletileriyle (opcode 0x2; RFC 6455 §5.2) gider. Bir ikili ileti bir ya da daha çok A3 çerçevesini art arda taşır, bir çerçeve iki iletiye bölünmez ve bir ikili ileti en çok 1 MiB'tır (mimari seçimi).
- **A3-K21** Çerçeve 24 baytlık başlık (§3.2) ve ardından tam `yuk_uzunlugu` bayt yükten oluşur; `yuk_uzunlugu` en çok 65 536'dır (mimari seçimi; en büyük yük `olay` ayrıntısı ve Ping360 satırıdır). Gönderen `ayrilmis` alanına ve tanımsız bayrak bitlerine 0 yazar; alıcı bunları yok sayar.
- **A3-K22** Yük alanları katalogdaki sırayla, aralarında doldurma olmadan, §3.1'deki tel tipleriyle yazılır.
- **A3-K23** Bir çerçevenin `kanal_id` ve `kanal_surumu` değeri, o yük için katalogdaki değerdir; sunucu bir kanalı tek bir `kanal_surumu` ile yayınlar.
- **A3-K24** Alıcı, katalogdaki aralığın ya da değer listesinin dışında bir değer, geçersiz UTF-8 metin ya da `azami`'yi aşan sayaç gördüğünde çerçeveyi atmaz, o alanı "geçersiz" gösterir ve kendi `istemci_gecersiz_alan` sayacını artırır; `yok_degeri` taşıyan alanı "yok" gösterir.
- **A3-K25** Yükün okunması `yuk_uzunlugu`'nu aşacaksa (bozuk çerçeve) alıcı o çerçeveyi atar, `istemci_bozuk_cerceve` sayacını artırır ve ileti içindeki sonraki çerçeveye `yuk_uzunlugu` ile geçer; ileti sonunda 24 bayttan kısa bir kalan varsa kalanı atar.
- **A3-K26** Başlıktaki `t_ns` kanalın `zaman_anlami` değerine göre yorumlanır: `olcum` = ölçüm zamanı (iskelet §3.1; aralık ölçümünde aralığın sonu); `gonderim` = akustik darbenin gönderim anı (iskelet §3.1, menzil profili); `sim` = durumun ya da olayın ait olduğu adım sınırının sim zamanı; `alim` = verinin sim sunucusuna ulaştığı anda dünyanın sim zamanı (karttan gelen durum). Hiçbir başlık alanı duvar saati taşımaz.
- **A3-K27** `sira` alanı `olay` kanalında olayın sırasıdır: sim süreci boyunca 1'den başlar, boşluksuz artar ve çalıştırmalar arasında sıfırlanmaz. Diğer kanallarda `sira`, sunucunun o kanal-anahtar için ürettiği kayıtların 1'den artan sayacıdır; seyreltme ve düşürme nedeniyle istemciye boşluklu ulaşabilir. `kaynak_kayit` bayraklı (izleme) çerçevelerde `sira` A3-K112'ye göredir ve canlı olay sırasından bağımsızdır.
- **A3-K28** `bayraklar` bitleri: bit0 `kaynak_kayit` (çerçeve izleme kipinde kayıttan üretildi), bit1 `ozet` (abonelik başında ya da yeniden eşlemede gönderilen son değer), bit2 `oncesinde_dusme` (bu kanal-anahtar için bu çerçeveden önce dizi kuyruğundan çerçeve düştü), bit3 `yer_gercegi` (katalogda `yer_gercegi: true` olan kanal). Bit 4–15 sıfırdır.

### 4.4 Abonelik ve seyreltme

- **A3-K29** Zorunlu kanallar (`calistirma/durum`, `sayac/gercek_zaman`, `olay`) her bağlantıda `merhaba`'dan hemen sonra abonelik istenmeden açılır; bunları bırakma isteği `zorunlu_kanal` nedeniyle reddedilir. Böylece her panel penceresi çalıştırmanın geçerliliğini ve gerçek zaman sayaçlarını görür (Ö11; 08 §2.1 durum şeridi).
- **A3-K30** Diğer kanallara `abone_ol` iletisiyle abone olunur. `hz` verilmezse kanalın `varsayilan_hz` değeri kullanılır; verilen `hz` {50, 25, 10, 5, 2, 1} kümesinde değilse kanal `gecersiz_hz` ile reddedilir. `dizi` ve `olay` sınıfında `hz` yok sayılır.
- **A3-K31** Sunucu her `abone_ol` ve `abonelik_birak` iletisine aynı `istek_no` ile tek bir `abonelik_yaniti` döner; yanıtta istenen her kanal için `kabul` (kanal kimliği, kanal sürümü, etkin hız) ya da `ret` ve nedeni bulunur.
- **A3-K32** Durum sınıfı bir kanalın etkin hızı `min(istenen hz, kanalın azami_hz değeri, yayın tavanı)`'dır.
- **A3-K33** Yayın tiki duvar saatiyle 50 Hz'tir (20 ms; mimari seçimi, tahmin). Etkin hızı `h` olan durum sınıfı kanal-anahtar için sunucu her `50/h` tikte bir, son gönderimden sonra yeni bir değer üretilmişse yalnız en son değeri gönderir; yeni değer yoksa göndermez. Yayın tiki sim zamanına değil duvar saatine bağlıdır; hızlandırılmış ya da yavaşlatılmış SIL'de panel akışı duvar saatine göre seyrelir.
- **A3-K34** `dizi` ve `olay` sınıfında sunucu üretilen her çerçeveyi üretim sırasıyla gönderir; seyreltme yapmaz.
- **A3-K35** Bir kanala abone olunduğunda ve her yeni bağlantıda sunucu, abone olunan her durum sınıfı kanal-anahtarın son değerini (varsa) `ozet` bayrağıyla hemen gönderir.
- **A3-K36** Bir bağlantının abone olmadığı kanalın verisi o bağlantı için serileştirilmez ve kuyruğa girmez.
- **A3-K37** Yayın tavanı `PUT /api/v1/yayin` ile {50, 25, 10} Hz değerlerinden birine ayarlanır, bütün bağlantılara uygulanır ve sim süreci başlarken 50'dir. Tavan, `merhaba` iletisinde ve `GET /api/v1/durum` yanıtında bildirilir.

### 4.5 Yavaş istemci ve düşürme

- **A3-K38** Dünya, A3'e veriyi yalnız tek üretici–tek tüketici halka tamponuna yazarak verir. Tampon doluysa dünya iletiyi atar, `a3_halka_tasma` sayacını artırır ve beklemez. A3'ün hiçbir işlemi dünyayı, HIL G/Ç iş parçacığını ya da kayıt yazıcısını bekletmez (08 §1; 04 §2.12). Sim hiçbir kipte paneli beklemez.
- **A3-K39** Yayın iş parçacığı soketlere engellemesiz yazar. Bir bağlantının yazılamayan verisi yalnız o bağlantının kuyruklarında bekler; bir bağlantının yavaşlığı başka bir bağlantıya gönderimi geciktirmez.
- **A3-K40** Durum sınıfı: her bağlantı ve kanal-anahtar için tek bir yuva vardır. Yuvadaki değer gönderilmeden yeni değer gelirse eskisi ezilir ve o bağlantının `bu_baglanti_ezilen_durum` sayacı artar.
- **A3-K41** Dizi sınıfı: her bağlantı ve kanal için kanalın `kuyruk_siniri` kadar çerçevelik kuyruk vardır. Kuyruk doluyken yeni çerçeve gelirse en eski çerçeve atılır, `bu_baglanti_dusen_dizi` artar ve o kanal-anahtarın bundan sonra gönderilen ilk çerçevesinde `oncesinde_dusme` biti 1 olur.
- **A3-K42** Olay sınıfı: her bağlantının 4096 çerçevelik bir olay kuyruğu vardır (mimari seçimi). Olay ezilmez ve atlanmaz; kuyruk doluyken yeni olay gelirse sunucu o bağlantıyı 4001 koduyla kapatır.
- **A3-K43** Kapanan bağlantı çalıştırmayı etkilemez; bütün istemciler kopsa da çalıştırma sürer ve panel bağlantılarının durumu çalıştırmanın geçerliliğini değiştirmez.
- **A3-K44** Dünya–A3 halka tamponunda olay sınıfından ileti düşerse (A3-K38) sunucu, düşen olayların sayısını `olay_kaybi` olayıyla bildirir. Düşen olaylara sıra numarası verilmez; `sira` boşluksuz kalır ve kayıp yalnız bu olayla görünür. Kayıt yazıcısının ayrı halka tamponu bundan etkilenmez (A6 §4.1).
- **A3-K45** Sunucu son 65 536 olayı bellekte tutar (mimari seçimi). Yeniden bağlanan istemci `merhaba`'daki `son_olay_sirasi` ile kendi gördüğü son sırayı karşılaştırır ve aradaki olayları `GET /api/v1/olaylar?sonra=<sıra>&en_cok=<n>` ile alır (`n` ≤ 1000). Yanıttaki her olay `yuk_olay`'ın JSON karşılığıdır: `sira` ve `t_ns` başlıktan, `tur`, `onem` ve `kaynak` adlarıyla, `ayrinti` JSON nesnesi olarak. İstenen sıra bellekteki pencerenin dışındaysa sunucu 410 `gecmis_yok` döner; bu olaylar yalnız kayıttan (A6 `olay` kanalı) okunabilir.
- **A3-K46** Sunucu her bağlantıya 5 saniyede bir WebSocket Ping gönderir; 15 saniye boyunca o bağlantıdan Pong ya da başka bir çerçeve gelmezse bağlantıyı 4004 ile kapatır (süreler mimari seçimi). İstemci Ping'e Pong ile yanıt verir (RFC 6455 §5.5.2; tarayıcı bunu kendisi yapar).
- **A3-K47** İstemciden sunucuya yalnız metin iletileri gider. İkili ileti gelirse sunucu bağlantıyı 1003, geçersiz UTF-8 gelirse 1007 (RFC 6455 §8.1), 64 KiB'tan büyük ileti gelirse 1009 ile kapatır.
- **A3-K48** Çözülemeyen JSON, şemaya uymayan ileti ya da bilinmeyen `tur` için sunucu `hata` metin iletisi gönderir ve bağlantıyı açık tutar.
- **A3-K49** Sim sunucusu olağan kapanırken bütün bağlantıları 1001 ile kapatır.

### 4.6 Komut işleme

- **A3-K50** Durum değiştiren her istek `Ida-Istemci: <istemci_id>` başlığını ve gövdesinde `istek_kimligi` alanını taşır. Başlık yoksa ya da `istemci_id` kayıtlı değilse sunucu 400 `gecersiz_istek` döner.
- **A3-K51** İstek gövdesi `Content-Type: application/json` olmalıdır, değilse 415 `icerik_turu`. Gövde 1 MiB'ı (senaryo uçlarında 4 MiB) aşarsa 413 `govde_cok_buyuk` (sınırlar mimari seçimi): `Content-Length` sınırı aşıyorsa sunucu gövdeden yalnız ilk 4096 baytı okur; `Content-Length` yoksa sınıra kadar okur; her iki durumda sınırın ötesini okumaz ve yanıttan sonra bağlantıyı kapatır. JSON çözülemezse ya da uç şemasına uymazsa 400 `gecersiz_istek`; şemaya uyup anlamca geçersizse 422 `gecersiz_deger`.
- **A3-K52** Erişim denetiminden (A3-K06…K09) geçen ve durum değiştiren her istek, sonucu ne olursa olsun, bir `komut_no` alır ve alındığı anda kayda yazılır (kayıt açıksa; A6 §4.4): **uç şemasından geçen** istek gövdesiyle birlikte A6 `a3/komut` kanalına (sonradan durum, kip, revizyon ya da anlam denetiminde reddedilse de); şemadan geçmeyen her istek (gövdesiz, geçersiz UTF-8, JSON olmayan, sınırı aşan ya da geçerli JSON olup uç şemasına uymayan gövde) A6 `a3/gecersiz_istek` kanalına, okunan gövdenin ilk en çok 4096 baytı, bildirilen ve okunan uzunluk, kesilme ve sınır aşımı bayrakları ve ret nedeniyle. Ret yanıtı da `komut_no` taşır. Tek istisna A3-K53'teki yinelemedir.
- **A3-K53** Yineleme anahtarı (`istemci_id`, HTTP yöntemi, yol, `istek_kimligi`) dörtlüsüdür. Sunucu her anahtarı, ilk isteğin ham gövdesinin SHA-256 özeti ve verdiği yanıtla (ret dahil) saklar. Aynı anahtar ve aynı gövde özetiyle gelen istek yeniden uygulanmaz: saklanan yanıt `yineleme: true` ile döner, yeni `komut_no` atanmaz ve istek A6'ya yeniden yazılmaz. Aynı `istemci_id` ile aynı `istek_kimligi` farklı yöntem, farklı yol ya da farklı gövde özetiyle gelirse sunucu 409 `kimlik_cakismasi` döner ve isteği uygulamaz. Saklanan kayıt, kaydedildiği andaki çalıştırma bittiğinde (`bitti`, `durduruldu`, `hata`, `sifirla` ya da yeni `baslat`) ya da kaydından 10 dakika (duvar saati) sonra, hangisi önce gelirse, silinir; çalıştırma yokken kaydedilen için yalnız süre geçerlidir; istemci oturumu kapanınca o istemcinin kayıtları silinir (süre mimari seçimi). Sunucu en çok 10 000 kayıt tutar; dolu iken gelen yineleme olmayan durum değiştiren istek 503 `mesgul` ile reddedilir; emniyeti kısıtlayan istekler (A3-K119) bu nedenle reddedilmez, yalnız yineleme kaydı tutulmaz (bir kısıtlamanın ikinci kez uygulanması yetkili durumu değiştirmez, yalnız `emniyet_rev`'i artırır; bu tutucu yöndedir; A3-K92).
- **A3-K54** Canlı çalıştırmayı etkileyen istekler (arıza, emniyet, yer istasyonu ve onay jetonu uçları) ile bütün çalıştırma kontrolü komutları (A3-K123) `beklenen_run_id` taşır; değer güncel `run_id`'dir ve güncel `run_id` yokken `null` olmak zorundadır (A3-K124). Değer güncel değilse sunucu 409 `beklenen_durum_uyusmuyor` döner ve `ayrinti`'de güncel `run_id`, durum ve `durum_rev`'i verir; canlı çalıştırma gerektiren uçta `null` gelirse 409 `canli_calistirma_yok` döner. Çalıştırma kontrolü komutları ayrıca zorunlu `beklenen_durum_rev` taşır (A3-K123); emniyeti gevşeten komutlar `beklenen_emniyet_rev` taşır (A3-K118). Böylece aynı revizyonu görmüş iki istemcinin yarışan komutlarından yalnız ilki uygulanır (AB-057).
- **A3-K55** 1.0'da bütün jetonlu istemciler eşit yetkilidir; "operatör" ve "izleyici" rolü ve kontrol sahipliği yoktur. Her komut `istemci_id` ile kaydedilir.
- **A3-K56** Senkron uçlar işlemi bitirir ve 200 döner. Asenkron uçlar (§3.6 "Tür" sütunu) erişim, şema, durum, kip ve onay denetimlerini eşzamanlı yapar; ret varsa 4xx döner; yoksa komutu dünyanın komut kuyruğuna koyar ve 202 ile `{"sonuc": "kuyrukta", "komut_no"}` döner.
- **A3-K57** Dünya kuyruktaki komutları bir sonraki adım sınırında, `komut_no` sırasıyla uygular (A3-K95'teki emniyet sırası saklıdır). `beklenen_run_id` taşıyan her komut, uygulanmadan hemen önce aynı atomik işlemde güncel `run_id` ile yeniden karşılaştırılır; çalıştırma o arada değiştiyse ya da bittiyse (`sifirla`, `durdur`, yeni çalıştırma) komut uygulanmaz ve `reddedildi` / `beklenen_durum_uyusmuyor` sonuçlanır. Emniyet revizyonu çalıştırma değişiminde artmadığı için (A3-K117) eski çalıştırmanın bekleyen gevşetmesini bu karşılaştırma durdurur. Her asenkron komutun nihai sonucu (`uygulandi` ve `uygulama_adimi`, ya da `reddedildi` ve `hata_kodu`) `komut_sonucu` olayıyla yayınlanır ve A6 `a3/komut_sonucu` kanalına yazılır.
- **A3-K58** Çalıştırma duraklatılmışken kuyruğa giren komut sim zamanının ilerlemesini beklemeden uygulanır; `uygulama_adimi` o anki `adim_no`'dur ve etkisi bir sonraki fizik adımının başından geçerlidir.
- **A3-K59** Dünyanın komut kuyruğu 256 komutla sınırlıdır (mimari seçimi). Kuyruk doluyken gelen komut 503 `mesgul` ile reddedilir; emniyeti artıran sanal girdi değişikliği (A3-K92) ve izni daraltan yer istasyonu komutu (A3-K104) bu nedenle reddedilmez, onlar için ayrılmış yuva kullanılır.
- **A3-K60** `GET /api/v1/komutlar/{komut_no}` yalnız güncel sim sürecinin verdiği komut numaralarını kapsar ve son 10 000 komutun güncel durumunu (`kuyrukta`, `uygulandi`, `reddedildi`) döndürür; daha eskisi ya da başka süreçten gelen bir numara için 404 `bulunamadi`. Tekrar koşusunda kayıttan uygulanan komutlar güncel süreçten yeni `komut_no` alır; kaynak kimlikleri ayrı alanlardadır (`kaynak_run_id`, `kaynak_komut_no`; A6-K37).

### 4.7 Çalıştırma durum makinesi ve kipler

**Tablo 4.7a — Durum geçişleri.** Satır başlangıç durumu, hücre komutun götürdüğü durumdur; "—" o komutun bu durumda geçersiz olduğu anlamına gelir. "(iç)" satırları komutsuz geçişlerdir.

| Durum ↓ / Komut → | `senaryo/yukle` | `baslat` | `duraklat` | `surdur` | `adimla` | `hiz` | `durdur` | `sifirla` | `yeniden_kostur` | `izleme/baslat` |
|---|---|---|---|---|---|---|---|---|---|---|
| `bos` | `yuklendi` | — | — | — | — | — | — | — | `hazirlaniyor` | `izleme` |
| `yuklendi` | `yuklendi` | `hazirlaniyor` | — | — | — | — | — | `yuklendi` | `hazirlaniyor` | `izleme` |
| `hazirlaniyor` | — | — | — | — | — | — | `yuklendi` | `yuklendi` | — | — |
| `kosuyor` | — | — | `duraklatildi` | — | — | `kosuyor` | `durduruldu` | `yuklendi` | — | — |
| `duraklatildi` | — | — | — | `kosuyor` | `kosuyor` → `duraklatildi` | `duraklatildi` | `durduruldu` | `yuklendi` | — | — |
| `bitti`, `durduruldu`, `hata` | `yuklendi` | — | — | — | — | — | — | `yuklendi` | `hazirlaniyor` | `izleme` |
| `izleme` | — | — | — | — | — | — | — | — | — | — |

İç geçişler: `hazirlaniyor` → `kosuyor` (ön denetim geçti); `hazirlaniyor` → `yuklendi` (ön denetim kaldı); `kosuyor` → `bitti` (senaryo süresi doldu); `kosuyor` ya da `duraklatildi` → `hata` (04 §4'teki çalıştırmayı durduran hata: NaN durumu vb.); `izleme` → izlemeden önceki durum (`izleme/bitir`).

**Tablo 4.7a′ — Durumlara göre güncel `run_id` ve `beklenen_run_id`'nin kabul kümesi** (A3-K124; şemadaki `$defs/run_id_ya_da_null` ile aynı küme).

| Durum | Güncel `run_id` | Kabul edilen `beklenen_run_id` |
|---|---|---|
| `bos`, `yuklendi`, `izleme` | yok | yalnız `null` |
| `hazirlaniyor`, `kosuyor`, `duraklatildi` | canlı çalıştırmanınki | yalnız o kimlik |
| `bitti`, `durduruldu`, `hata` | biten çalıştırmanınki | yalnız o kimlik |

Böylece `yuklendi` durumundaki `sifirla` (henüz hiç çalıştırma başlamamışken de) `beklenen_run_id: null` ile geçerlidir.

**Tablo 4.7b — Kipe göre çalıştırma komutları.** İskelet §4 kipleri; tekrar kipleri A6 §4.6'dadır.

| Komut | `sil`, `tekrar_fizik`, `tekrar_girdi_sil`, `tekrar_yeniden_kurma` | `hil_1x`, `hil_bariyer` |
|---|---|---|
| `duraklat` | Geçerli; sim zamanı durur; SIL'de duvar saati sim zamanından türediği için emniyet süreleri de durur (iskelet §4) | Yalnız `gecersizligi_onayla: true` ile; aksi 409 `onay_gerekli`. Kabul edilirse çalıştırma `hil_duraklatma` nedeniyle (Tablo 4.8 bit 11) geçersiz olur (06 §2.9) |
| `surdur` | Geçerli | Geçerli; onay istemez (geçersizliğe yol açan istek `duraklat`'tır ve onay orada alınmıştır; `$defs/istek_calistirma`) |
| `adimla` | Geçerli; yalnız `duraklatildi`'dan | 409 `kipte_gecersiz` |
| `hiz` | Geçerli | 409 `kipte_gecersiz` (yalnız 1x) |
| `durdur`, `sifirla` | Geçerli | Geçerli |

- **A3-K61** Durumlar ve geçişler Tablo 4.7a'daki gibidir. Tabloda "—" olan (durum, komut) çifti eşzamanlı denetimde 409 `gecersiz_durum` ile reddedilir ve durum değişmez. Her geçiş `calistirma_durumu_degisti` olayı üretir ve `durum_rev`'i bir artırır (A3-K122). Geçişin önkoşulları (durum, kip, `beklenen_run_id`, `beklenen_durum_rev`) geçişle aynı atomik adımda dünya tarafında denetlenir (A3-K123).
- **A3-K62** `baslat` kabul edildiğinde durum `hazirlaniyor` olur ve sim ön denetimi yapar (HIL'de A1 el sıkışması dahil; 06 §2.9). Ön denetim geçerse durum `kosuyor` olur; kalırsa `yuklendi`'ye döner ve `baslat` komutu `on_denetim_kaldi` koduyla `reddedildi` sonuçlanır. Her rapor bir `rapor_kimligi` (16 küçük harfli onaltılık karakter) alır. `on_denetim_sonucu` olayı yalnız rapor kimliğini, sonucu, kipi, durum sayılarını ve madde başına `kod` ve `durum`'u taşır; tam rapor `GET /api/v1/on_denetim/{rapor_kimligi}` (ve en sonuncusu `GET /api/v1/on_denetim`) ile alınır. Sunucu son 64 raporu saklar (mimari seçimi); daha eskisi için 404 `bulunamadi`. Rapor en çok 32 madde taşır.
- **A3-K63** `run_id`, `baslat` (ya da `yeniden_kostur`) kabul edildiğinde atanır: `YYYYMMDD-hhmmss-xxxxxx` biçiminde, UTC başlangıç anı ve 6 onaltılık rastgele karakter (`$defs/run_id`). Aynı `run_id` kayıt dosyasının adıdır (A6 §3.1). A1'in `run_id` alanı (u32) bu metinden ayrıdır ve `a1_run_id` adıyla taşınır; türetimi A1-K126'dır: kartlı kiplerde metnin UTF-8 baytlarının SHA-256'sının ilk dört baytı (küçük uçlu; 0 çıkarsa 1), SIL'de A5'in türetimiyle a1/run_id (A5 Ek D) etiketinden üretilir (W1-uyum U17); `GET /api/v1/durum` ikisini de verir, A6 üst verisi ikisini de saklar. Güncel `run_id`'nin ömrü A3-K124'tedir.
- **A3-K64** Kip komutları Tablo 4.7b'ye uyar; tabloda reddedilen çift eşzamanlı denetimde belirtilen kodla reddedilir.
- **A3-K65** `adimla`'nın birimi bir kontrol periyodudur (5 ms, dört fizik adımı; iskelet §4). `periyot_sayisi` = n ise sim n periyot ilerler ve `duraklatildi`'ya döner; n, 1–10 000 aralığındadır.
- **A3-K66** `hiz` çarpanı 0 (olabildiğince hızlı) ya da [0,01; 100] aralığındadır; 0 ile 0,01 arası değer 422 `gecersiz_deger` ile reddedilir. Gerçekleşen çarpan `calistirma/durum.gerceklesen_carpan` ile bildirilir; sim, istenen çarpana ulaşamazsa bunu hata saymaz (08 §2.3).
- **A3-K67** `sifirla` aynı çözümlenmiş senaryo ve tohumla `yuklendi`'ye döner, canlı çalıştırmanın kayıt dosyasını kapatır (A6 §4.8), sayaçları sıfırlar, güncel `run_id`'yi `null` yapar ve `durum_rev`'i bir artırır (A3-K122); `sifirla`'nın kendisi `emniyet_rev`'i artırmaz (iskelet §3.2; A3-K117); eski çalıştırmanın jetonları ve bekleyen gevşetmeleri `run_id` bağıyla geçersizleşir (A3-K94, K54, K57). Etkin bir A1 oturumu varken sıfırlama o oturumu kapatır (çalıştırma nesli değişimi = oturum kapanışı + yeni oturum; A1-K128) ve revizyon bu oturum sonu işlemiyle bir artar (iskelet §3.2 md. 4); bu artış oturum sonunundur, `sifirla` için ayrı bir artış yoktur. Yeni `run_id` bir sonraki `baslat`'ta atanır. `yuklendi` durumunda (çalıştırma yokken) `sifirla` `beklenen_run_id: null` ile kabul edilir ve yalnız `durum_rev`'i artırır.
- **A3-K68** `yeniden_kostur` şu sırayla denetlenir: kayıt yoksa 404 `kayit_yok`; okunamıyor ya da `bozuk` ise (A6 §5) 422 `kayit_okunamadi`; istenen düzeyin kademe ve kanal gereği (A6 Tablo 4.6) sağlanmıyorsa 422 `gecersiz_deger` ve `ayrinti.gereken`. **Sürüm, platform ya da özet koşulu sağlanmayan tekrar isteği reddedilir** (409 `tekrar_kosulu_saglanmiyor`, `ayrinti.nedenler`); yalnız istekte açık `guvencesiz: true` varsa kabul edilir ve yeni kayda `tekrar_guvencesi = yok` ile nedenler yazılır (A6-K38 aynı kural). `yeniden_kurma` düzeyi güvence vermediği için bu koşula bağlı değildir. Kabul edilince kaydın çözümlenmiş senaryosu yüklenir ve kip `duzey`'e göre `tekrar_fizik`, `tekrar_girdi_sil` ya da `tekrar_yeniden_kurma` olur.
- **A3-K69** İzleme `bos`, `yuklendi`, `bitti`, `durduruldu` ve `hata` durumlarından başlar; izleme sürerken canlı çalıştırma, arıza, emniyet ve yer istasyonu komutları 409 `izleme_etkin` ile reddedilir.

### 4.8 Geçerlilik ve gerçek zaman bildirimi

**Tablo 4.8 — Geçerlilik nedenleri.** "G": bu kipte çalıştırmayı zamanlama bakımından geçersiz kılar. "T": tanısaldır (bit yazılır, `gecerli` değişmez). "—": bu kipte oluşmaz. Bit 0–11 A1-K79'un nedenleriyle **bire bir**dir ve adları A1 Tablo 3.11a'daki sayaç adlarıdır; bit 16–19 A1 dışı, sim modelinin (05 §4) ve A1-K95'in nedenleridir. Bütün tekrar kiplerinde SIL sütunu uygulanır.

| Bit | Neden | SIL | HIL 1x | HIL bariyer | Dayanak |
|---:|---|:-:|:-:|:-:|---|
| 0 | `parti_kacirildi` | — | G | G | A1-K79, K75 (birleştirilen dahil); iskelet §4 |
| 1 | `son_tarih_kacirildi` | — | G | G | A1-K108 (bariyerde kurtarılanlar dahil) |
| 2 | `sim_gecikmesi` | — | G | G | A1-K108 |
| 3 | `cagri_cakismasi` | — | G | G | A1-K117 (SIL'de oluşmaz, A1-K121) |
| 4 | `bariyer_bekleme` | — | — | G | A1-K75; iskelet §4 |
| 5 | `olay_gec` | — | G | G | A1-K83 |
| 6 | `esz_guvenli_uygulanan` | — | G | G | A1-K78 |
| 7 | `yanit_ret_gec` | — | G | G | A1-K72 |
| 8 | `pencere_tasma` | G | G | G | A1-K34 |
| 9 | `planlanmamis_oturum_sonu` | G | G | G | A1-K79, K23 |
| 10 | `beklenmeyen_sifirlama` | G | G | G | A1-K79, K16 |
| 11 | `hil_duraklatma` | — | G | G | A1-K79; 06 §2.9 |
| 16 | `sonar_gec` | — | T | G | 05 §4 |
| 17 | `sonar_kuyruk_dolu` | T | G | G | 05 §4 |
| 18 | `sensor_kuyruk_tasma` | T | G | G | 05 §4 |
| 19 | `cagri_izi_eksik` | — | T | T | A1-K95 (periyot "zamanlaması doğrulanmamış"; girdi tekrarı "tam değil") |

- **A3-K70** Her çalıştırma `gecerli = 1` ile başlar. Tablo 4.8'deki bir neden oluştuğunda ilgili bit `gecerlilik_nedenleri`'ne yazılır; neden o kip için "G" ise `gecerli` 0 olur ve o neden için bir kez `gecerlilik_dustu` olayı üretilir. Bitler silinmez ve `gecerli` aynı çalıştırmada 1'e dönmez. Arıza enjeksiyonu kendi başına geçerliliği değiştirmez (deneyin parçasıdır; 08 §4); yol açtığı belirtiler Tablo 4.8'deki nedenler üzerinden etkiler.
- **A3-K71** A1-K79'un nedenlerinden biri (Tablo 4.8 bit 0–11) bir kipte "G" olarak oluştuğunda `gercek_zamani_kacirdi` 1 olur, çalıştırma başına bir kez `gercek_zaman_kacirildi` olayı üretilir ve çalıştırma zamanlama bakımından geçersiz olur (iskelet §4 "Zamanlama geçerliliği"). İşaret aynı çalıştırmada 0'a dönmez. A6 kapanış kaydı aynı hükmü ve aynı neden adlarını saklar (A6 §3.5). Olayın `neden` alanı geçersizliği ilk kuran nedenin Tablo 4.8'deki adı, `neden_biti` bit numarasıdır (0–11; kapalı ve eksiksiz: A1-K79'un her nedeni bir değerdir); aynı adımda birden çok neden oluşursa en küçük bit numaralısı yazılır, bütün nedenler `gecerlilik_nedenleri`'ndedir.
- **A3-K72** Kaçırılan her parti `sayac/gercek_zaman.parti_kacirildi`'yi bir artırır ve A6 `hil/bariyer` kanalına her kayıt kademesinde yazılır (A6 §4.4). Sunucu kaçırılan partileri ayrıca en çok saniyede bir `parti_kacirildi_ozeti` olayıyla (ilk ve son `step_seq`, adet) bildirir.
- **A3-K73** Sim tarafının A1 sayaçları (A1 Tablo 3.11a) `sayac/gercek_zaman`'da A1'deki adlarla taşınır: bariyerde beklenen her yuva `bariyer_bekleme`'yi, bekleme süresi `gercek_zaman_kaymasi_ns` toplamını ve `gercek_zaman_kaymasi_en_buyuk_ns`'i, yinelenen yanıt `yanit_ret_yinelenen`'i, uygulama anı geçmiş yanıt `yanit_ret_gec`'i, kullanım süresi dolan komut `komut_suresi_doldu`'yu günceller; `olay_sonuc_<kod>` sayaçları `olay_sonuc` dizisinde, `alan_ret_<mesaj>` ve `bag_ret_<mesaj>` sayaçları `mesaj_retleri` dizisinde durur. Yapılandırma reddinin parametresi ve eşitsizlik numarası `yapilandirma_reddedildi` olayıyla bildirilir (A1-K104).
- **A3-K74** Sim, fizik adımlarının gecikmesini her kipte `adim_son_tarih_ihlali`, `kacirilan_adim` ve `azami_adim_gecikmesi_ns` sayaçlarında sayar (iskelet §6; 04 §2.11); bunlar A1-K108'in `sim_gecikmesi`'nden ayrı, fizik çekirdeğinin tanısal sayaçlarıdır.
- **A3-K75** `sayac/gercek_zaman` alanları çalıştırma boyunca azalmaz; yeni çalıştırmada ve `sifirla`'da sıfırlanır. Sayaçlar her kayıt kademesinde A6'ya yazılır (A6 §4.3).

### 4.9 Senaryo

- **A3-K76** `POST /api/v1/senaryo/dogrula` A5 doğrulayıcısını çalıştırır ve sonucu `rapor` alanında A5'in rapor biçiminde, değiştirmeden döndürür. Bu uç dosyayı ve çalıştırma durumunu değiştirmez ve komut numarası almaz.
- **A3-K77** `POST /api/v1/senaryo/yukle` Tablo 4.7a'daki durumlarda kabul edilir. Senaryo A5 doğrulamasından geçerse çözümlenmiş senaryo ve özeti yüklenir, durum `yuklendi` olur ve `senaryo_yuklendi` olayı üretilir; geçmezse 422 `senaryo_gecersiz` ve `ayrinti.rapor` döner, durum değişmez.
- **A3-K78** Senaryo adları `^[a-z0-9_-]{1,64}\.toml$` kalıbına uyar. Sunucu yalnız yapılandırılmış senaryo dizinini (varsayılan `scenarios/`) listeler, okur ve yazar; kalıba uymayan ad 400 `gecersiz_istek` ile reddedilir.
- **A3-K79** `PUT /api/v1/senaryolar/{ad}` dosyayı yazar; dosya varsa ve `uzerine_yaz: true` değilse 409 `dosya_var` döner. Yazma A5 doğrulamasının sonucundan bağımsızdır; yanıt A5 raporunu içerir. Yazma yüklü senaryoyu değiştirmez.
- **A3-K80** `GET /api/v1/senaryo/cozumlenmis` yüklü çözümlenmiş senaryonun A5 kanonik (RFC 8785) JSON baytlarını gövde olarak, değiştirmeden ve `Content-Type: application/json` ile döndürür; davranış özeti `Ida-Davranis-Ozeti: sha256:<64 hex>` yanıt başlığındadır. Baytlar kayda eklenen `cozumlenmis_senaryo.json` ile aynıdır (A6 §3.6) ve SHA-256'ları A5 davranış özetine eşittir (A5-K02, K17). Özetlerin metin gösterimi A3 ve A6'da her yerde `sha256:` + A5'in 64 küçük onaltılık karakteridir; kaynak dosya özetleri ayrı alandadır (A6 `senaryo_kaynak_ozetleri`).

### 4.10 Hata enjeksiyonu

- **A3-K81** `POST /api/v1/arizalar` gövdesindeki `tanim` A5'in arıza tanımı biçimindedir ve A5 doğrulayıcısıyla denetlenir; geçmezse 422 `senaryo_gecersiz` ve `ayrinti.rapor` döner.
- **A3-K82** Arıza uçları yalnız `kosuyor` ve `duraklatildi` durumlarında kabul edilir; diğer durumlarda 409 `canli_calistirma_yok`. Tekrar kiplerinde ayrıca A3-K120 uygulanır.
- **A3-K83** Elle verilen arızanın başlangıcı yalnız `hemen` (bir sonraki adım sınırı) ya da güncel sim zamanından sonraki bir `sim_zamani` olabilir; koşula bağlı başlangıç 422 `gecersiz_deger` ile reddedilir (koşullu arızalar senaryodadır; 08 §2.5).
- **A3-K84** Elle verilen arızanın kimliği `E<n>`'dir; n çalıştırma içinde 1'den artar ve 202 yanıtında `fault_id` olarak döner.
- **A3-K85** Arıza dağıtıcısı tanımı sınıfına göre ilgili arayüze iletir: sensör arızaları 05 (`IFaultControl`), aktüatör ve güç arızaları 04 (`apply_fault`), bağlantı arızaları bağlantı emülatörü profili, protokol arızaları hata enjektörü (06 §2.11, §2.12), sistem olayları 08 §2.5'teki karşılıkları. Hedef arayüzün parametre reddi komutu `reddedildi` / `gecersiz_deger` sonuçlandırır.
- **A3-K86** Uygulanan, kaldırılan, süresi biten ve reddedilen her arıza (senaryodan ya da elle) `ariza_degisti` olayı üretir ve `ariza/etkin` kanalını günceller; bu kayıtlar kayıt kademesinden bağımsız olarak A6'ya girer.
- **A3-K87** `POST /api/v1/arizalar/{fault_id}/kaldir` arızayı bir sonraki adım sınırında kaldırır; senaryodan gelen arıza da kaldırılabilir. Etkin olmayan kimlik 404 `bulunamadi` döner.
- **A3-K88** Arızanın zaman deseninin rastgeleliği A5 tohumundan ve `fault_id`'den türer; elle arızanın duvar saatine bağlı tek parçası uygulandığı adımdır ve o adım `komut_sonucu` ile A6'ya yazılır (08 §2.5).

### 4.11 Sanal emniyet girdileri

Bu başlık atış kontrol sınırı içindedir: yalnız emniyet zincirinin girdilerini ve onların değiştirilme kurallarını tanımlar.

- **A3-K89** Sanal emniyet girdileri (iskelet §5; W1-uyum U6): tek bir `acil_durdurma` {`serbest`, `basili`}; tüp başına (`f1`, `f2`; A5 adları; A1/A4 tüp dizini `f1` = 0, `f2` = 1) `silahlandirma_fisi` {`cikarilmis`, `takili`} ve `emniyet_pimi` {`cikarilmis`, `takili`}. Fiş ve pim için her istek, onay jetonu, olay ve durum alanı `tup` taşır; acil durdurma için taşımaz (`$defs/girdi_secimi`; aksi 400 `gecersiz_istek`). Bir girdinin iki tüpe birden uygulanması varsayılmaz. **Emniyeti artıran** yön: acil durdurmanın basılması, bir tüpün fişinin çıkarılması, bir tüpün piminin takılması. **Emniyeti azaltan** yön bunların tersidir. `emniyet_girdisi_degisti` olayının ayrıntı şeması da `$defs/girdi_secimi`'ye bağlıdır (fiş ve pimde `tup` zorunlu, acil durdurmada yasak); A6 olay kayıtlarını aynı tanımla doğrular.
- **A3-K90** Girdilerin yetkili durumu yalnız sim sunucusundadır. A1'den gelen hiçbir mesaj yetkili durumu değiştirmez; yetkili durumu yalnız `POST /api/v1/emniyet/degistir` ve senaryonun zaman çizelgesi değiştirir.
- **A3-K91** Emniyet değişikliği yalnız canlı çalıştırmada (`hazirlaniyor`, `kosuyor`, `duraklatildi`) kabul edilir; diğer durumlarda 409 `canli_calistirma_yok`. Başlangıç durumu A5'ten gelir. Tekrar kiplerinde ayrıca A3-K120 uygulanır.
- **A3-K92** Emniyeti artıran (kısıtlayan) değişiklik tek adımlıdır: geçerli bir istek onay, onay jetonu ve `beklenen_emniyet_rev` olmadan kabul edilir; komut kuyruğu doluluğu nedeniyle ve revizyon uyuşmazlığı nedeniyle hiçbir zaman reddedilmez (A3-K119). Girdi o sınırda zaten istenen durumdaysa komut `uygulandi` sonuçlanır ve yetkili durum değişmez, **ama `emniyet_rev` yine bir artar** (A3-K117); böylece sonradan kabul edilen her kısıtlama, daha önce alınmış bütün gevşetme jetonlarını ve bekleyen gevşetme isteklerini geçersizleştirir.
- **A3-K93** Emniyeti azaltan (gevşeten) değişiklik iki adımlıdır: istemci önce `Ida-Istemci` başlıklı `POST /api/v1/emniyet/onay_jetonu` isteğiyle (`beklenen_run_id`, `girdi`, fiş ve pimde `tup`, `hedef_durum`) bir onay jetonu alır; yanıt jetonu ve bağlandığı `run_id` ile `emniyet_rev`'i verir. Sonra `POST /api/v1/emniyet/degistir` isteğini aynı `girdi`, `tup` ve `hedef_durum` ile, `onay: true`, bu jeton ve `beklenen_emniyet_rev` ile gönderir. `onay` ya da jeton yoksa 409 `onay_gerekli`; `beklenen_emniyet_rev` yoksa 400 `gecersiz_istek`; jeton geçersizse (A3-K94) 409 `onay_jetonu_gecersiz`; revizyon doyduysa 409 `emniyet_rev_doydu` döner.
- **A3-K94** Onay jetonu 128 bitlik rastgele değerdir (32 onaltılık karakter) ve şunlara bağlıdır: verildiği andaki `run_id`; verildiği andaki `emniyet_rev`; onu alan istemci oturumu (`istemci_id`); işlemin kendisi (uç ile, sanal girdi için `girdi`, fiş ve pimde `tup` ve `hedef_durum`, yer istasyonu için `komut` ve parametrelerin kanonik JSON özeti; A3-K103). Jeton tek kullanımlıktır ve şu durumların ilkinde geçersizleşir: ilk kullanımı (kabul ya da ret); verilişinden 10 s geçmesi (duvar saati; 08 §3.4; mimari seçimi, tahmin); `emniyet_rev`'in değişmesi (yetkili durumu değiştirmeyen bir kısıtlamanın kabulü dahil; A3-K117); `sifirla`; yeni bir çalıştırmanın başlaması; istemci oturumunun kapanması. Bağlı olduğu değerlerden biri istekteki ya da güncel değerle uyuşmayan jeton reddedilir.
- **A3-K95** Kabul edilen emniyet işlemleri iskelet §3.2'deki **fizik sınırındaki sıra** ile işlenir; A3 bu sırayı aynen yazar (A1-K85 de aynısını yazar; A5 ve A6 atıf verir). Aynı fizik sınırına düşen emniyet işlemleri şu sırayla işlenir; sim duraklatılmışken gelen emniyet işlemleri beklemez, aynı sırayla, sim zamanı ilerlemeden işlenir: (1) oturum sonu ve süresi dolan izinler; (2) kısıtlayan komutlar ve senaryonun kısıtlayan atamaları (hepsi uygulanır); (3) kart olayları: önce `SAFETY_CLOSE`, sonra izin açan olaylar, sonra `FIRE`; her biri önceki adımlardan sonraki emniyet durumuna ve revizyona göre doğrulanır; (4) gevşetmeler: revizyon bu sınırda 1–3. adımlarda arttıysa bu sınırdaki bütün gevşetmeler reddedilir ve yeniden denenmez; artmadıysa önce senaryonun gevşeten atamaları, sonra panel istekleri (her biri taşıdığı revizyona göre) uygulanır; (5) sürekli çıktılar. Bir olay, aynı sınırdaki gevşetmeden yararlanmaz. Panel isteğinin 4. adımdaki reddi `komut_sonucu` olayında `kisitlama_hakim` (revizyon 1–3. adımlarda arttı) ya da `emniyet_rev_uyusmuyor` (taşıdığı revizyon güncel değil) kodlarıyla; senaryo atamasının reddi `gevsetme_reddedildi` olayıyla bildirilir (W1-uyum U3).
- **A3-K96** Her değişiklik (operatörden ya da senaryodan) `emniyet_girdisi_degisti` olayı (önem `emniyet`; fiş ve pimde `tup` ile) üretir, `emniyet/girdiler` kanalını günceller ve kayıt kademesinden bağımsız olarak A6'ya girer. Reddedilen istekler de `a3/komut` ve `a3/komut_sonucu` ile kaydedilir.
- **A3-K97** Kartın emniyet girdilerine dair bildirdiği tek büyüklük atış kontrolünün engel maskesidir (A1 `IDA_FC_STATUS.inhibit_mask` = A4 `fc.inhibit_mask`; A4 §3.16): bit 4 `ESTOP` etkin acil durdurma basılı, bit 5–6 `PLUG_T0/T1` tüpün etkin fiş değeri çıkık, bit 7–8 `PIN_T0/T1` tüpün etkin pim değeri takılı. `emniyet/girdiler`'in `kart_*` alanları yalnız bu bitlerden türetilir; `IDA_FC_STATUS` alınmadıysa 255 (bilinmiyor) gösterilir. Sim'in karta gönderdiği `IDA_SAFETY` (yalnız sim → kart; `sensor/firlatici`) kartın kabul ettiği etkin değer değildir ve kart okuması olarak sunulmaz. Kurulmamış çekirdeğin sabit maskesinde (A4-K126) bu bitler kısıtlayan yöndedir ve öyle gösterilir. `geri_okuma_farki` yetkili durum ile kartın bildirdiği etkin değer arasındaki anlık farktır (bit0 acil, bit1–2 fiş `f1`/`f2`, bit3–4 pim `f1`/`f2`) ve iletim gecikmesi süresince olağan olarak kurulabilir. Sim'in gönderdiği değerin bozulması bir arıza enjeksiyonudur (sınıf `sensor`, hedef fırlatıcı; 05 §2.3.8) ve yetkili durumu değiştirmez.
- **A3-K98** Sim, karttan gelen tek seferlik olay için verdiği her kararı (uygulandı, yineleme, emniyet nedeniyle ret, süresi geçtiği için ret, diğer ret) `atis_olayi_sonucu` olayıyla bildirir ve `atis/durum` kanalını günceller. A3 bu kararı yalnız raporlar; kararın kuralı A1 sözleşmesinde ve 04'ün fırlatıcı modelindedir.
- **A3-K99** A3'te sim fırlatıcısını tetikleyen, `ateşle` olayı üreten, tüp doluluğunu ya da süreli izni doğrudan değiştiren bir uç yoktur; §3.6'daki liste A3 uçlarının tamamıdır ve listede olmayan her `/api/` yolu 404 döner.

### 4.12 Yer istasyonu komutları

İlk sürümde yer istasyonu işlevi web panellerindedir (iskelet §3).

- **A3-K100** Panelden karta giden yer istasyonu komutları yalnız `POST /api/v1/yer_istasyonu/komut` ile verilir. Komut kataloğunun sahibi A1'dir: `protocol/komutlar.json` (A1-K125; W1-uyum U16; biçim: `surum`, `komutlar[]` = `ad`, `a1`, `sinif` ∈ {`daraltici`, `genisletici`, `notr`}, `onay`, `parametreler` (JSON Schema), `sonuc` (JSON Schema ya da null), `bilesik`; bileşik komutta `a1.mesaj` ve `a1.tur` gönderim sırasıyla dizidir ve yinelenen öğe A1-K125'in tablosundadır; `$defs/a1_komut_listesi`). `GET /api/v1/yer_istasyonu/komutlar` bu dosyayı değiştirmeden döndürür. Sunucu `komut`'u kataloğun `ad` alanında, `parametreler`'i o komutun `parametreler` şemasıyla doğrular; katalogda olmayan komut 422 `komut_listede_yok`, şemaya uymayan istek 422 `gecersiz_deger` ile reddedilir. A3 katalog dışı hiçbir komutu kabul etmez. Katalogdaki atış isteği komutu (`FC_FIRE`; mimar kararı) yalnız operatörün atış isteğini karta iletir: emniyet zincirini kart yürütür ve `FIRE` olayını yalnız çekirdek üretir. Bu komut genel yer istasyonu komut yolundan, öteki onaylı genişletici komutlarla aynı kurallarla geçer (onay jetonu A3-K103, revizyon bağı A3-K118, emülatör çıkışında `bayat_yetki` A3-K101); ona özel bir uç ya da parametre yoktur ve A3-K99 geçerlidir: sim fırlatıcısını kartı atlayarak tetikleyen bir uç yoktur. Tekrar kiplerinde ayrıca A3-K120 uygulanır.
- **A3-K101** Kabul edilen komut yer istasyonu ağ geçidinde A1'in yer istasyonu biçimine kodlanır ve bağlantı emülatörüne verilir; komut emülatörün gecikme ve kayıp modelinden geçerek A1 üzerinden karta gider (iskelet §3). A3 hiçbir komutu emülatörü atlayarak karta göndermez. **Genişletici** (kataloğun `sinif: genisletici`) komut kabul edildiği andaki `emniyet_rev`'e bağlanır ve üç bekleme noktasında düşürülebilir: (a) bağlantı emülatöründen çıkıp A1'e verileceği anda revizyon farklıysa komut atılır, karta gitmez, `sayac/gercek_zaman.bayat_yetki` artar ve emülatör kararı A6 `yer_istasyonu/emulator`'a `bayat_yetki` olarak yazılır (iskelet §3.2; W1-uyum U4a); (b) sim'in A1 gönderim kuyruğunda, tele yazılmadan hemen önce revizyon farklıysa gönderilmez ve `sayac/gercek_zaman.komut_bayat_yetki` artar (A1-K129; A1 Tablo 3.11a); (c) kartın ertelenmiş komut kuyruğunda bekleyen genişletici, platform bir daraltıcıyı mandala aldığında atılır ve kart bunu ret sonucuyla bildirir (W1-uyum U4b; A1). (a) ve (b) `yer_istasyonu_komutu` olayıyla `bayat_yetki` durumunda, (c) `yer_istasyonu_yaniti` olayında kartın ret sonucuyla bildirilir. Kabul edilen her **daraltıcı** komut, işlendiği fizik sınırında (A3-K95 adım 2) revizyonu bir artırır (iskelet §3.2 md. 1).
- **A3-K102** 202 yanıtı yalnız komutun kuyruğa alındığını söyler. Sonraki aşamalar `yer_istasyonu_komutu` olayıyla (`emulatore_verildi`, `karta_iletildi`, `emulator_dusurdu`, `kuyruk_tasti`, `oturum_yok`, `bayat_yetki`), kartın A1 yanıtı `yer_istasyonu_yaniti` olayıyla bildirilir. Kartın `PARAM_GET`/`PARAM_SET` sonucunda `type` = 0 ise (`ACCEPTED` olmayan sonuç; A4 §3.14, K43) değer yoktur: ağ geçidi `veri`'ye `param_type` ve `value` yazmaz, yalnız `ack`, `reason` ve `param_id` yazar. Panel sonucu ve kart günlüğünü gösterirken ret nedenleri için A4 §3.17'nin, günlük kodları ve önem düzeyleri için A4 §3.15'in kapalı tablolarına atıf verir, anlamlarını yinelemez. Nihai yanıt olayı, kataloğun o komut için verdiği `sonuc` şemasına uyan bir `veri` nesnesi taşır (ör. parametre kimliği, türü ve değeri; ret nedeni; A1 `IDA_CMD_RESULT`, A4 `ida_cmd_result_t` alanlarından); `veri` ayrıca `$defs/sinirli_nesne` sınırlarına uyar (A3-K121). Sunucu emülatörde kaybolan ya da atılan komutu kendiliğinden yeniden göndermez; yeniden gönderme kullanıcının yeni komutudur.
- **A3-K103** Katalogda `onay: true` olan komutlar (A1'in sınıflamasıyla genişletici komutların hepsi) A3-K93'teki gibi iki adımlıdır: onay jetonu `Ida-Istemci` başlıklı `POST /api/v1/yer_istasyonu/onay_jetonu` isteğiyle (`beklenen_run_id`, `komut`, `parametreler`) alınır ve A3-K94'teki değerlere bağlıdır (işlem = `komut` + `parametreler`'in kanonik JSON özeti: anahtarları sıralanmış, boşluksuz UTF-8 metnin SHA-256'sı); jetonun bağları, ömrü ve geçersizleşmesi A3-K94'teki gibidir. Genişletici komut ayrıca `beklenen_emniyet_rev` taşır (A3-K118); adım sınırındaki sıra A3-K95'tir; emülatör çıkışındaki denetim A3-K101'dedir.
- **A3-K104** Katalogda `sinif: daraltici` olan komutlar (ör. silahsızlandırma, atış iptali) tek adımlıdır; onay ve `beklenen_emniyet_rev` istemez; komut kuyruğu doluluğu ve revizyon uyuşmazlığı nedeniyle reddedilmez (A3-K119).
- **A3-K105** Katalogda `bilesik: true` olan komutlar (birden çok A1 iletisi gerektiren işlemler; ör. görev yükleme, parametre kaydetme) ağ geçidince A1-K125'teki sırayla ve yinelenen öğe tablosuyla (A1-K125) yürütülür; panel bunları tek komut olarak gönderir ve sonucu tek `yer_istasyonu_yaniti` olayında, kataloğun `sonuc` şemasına uyan `veri` ile alır.
- **A3-K106** Yer istasyonunun gördüğü telemetri `yer_istasyonu/durum` (çözülmüş) ve `yer_istasyonu/mavlink` (ham çerçeve) kanallarından gelir ve emülatörden geçmiştir. `kart/durum` emülatörsüz laboratuvar görüşüdür; ikisi ayrı kanaldır ve sunucu birinin verisini diğerine koymaz.
- **A3-K107** Sim sunucusu A3 dışında hiçbir yerden yer istasyonu girişi kabul etmez; harici yer istasyonu yazılımına açılan MAVLink/UDP uç noktası yoktur (iskelet §3, §10 md.17).

### 4.13 Kayıt yönetimi ve izleme

- **A3-K108** Her canlı ve tekrar çalıştırması kaydedilir. Kayıt `baslat` ya da `yeniden_kostur` kabul edildiğinde açılır ve çalıştırma `bitti`, `durduruldu`, `hata` olduğunda ya da `sifirla` ile kapanır (A6 §4.8). Kaydı çalıştırma ortasında başlatan ya da durduran bir uç yoktur.
- **A3-K109** `baslat` gövdesindeki `kayit_kademesi` ve `kamera_kareleri` A5'teki değerleri o çalıştırma için geçersiz kılar; verilmezlerse A5 değerleri kullanılır.
- **A3-K110** `GET /api/v1/kayitlar` kayıt dizinindeki `.mcap` dosyalarını A6 üst verisinden çıkarılan özetle listeler; üst verisi okunamayan dosya `kapanmis: false` ve `tekrar_guvencesi: yok` ile listelenir.
- **A3-K111** `POST /api/v1/kayitlar/{run_id}/sil` yalnız `onay: true` ile ve dosya etkin çalıştırmanın kaydı değilse uygulanır; etkin kayıt için 409 `gecersiz_durum` döner.
- **A3-K112** `izleme/baslat` kaydı açar ve durumu `izleme` yapar. Sunucu kayıttaki ortak kanalları (A6 §3.3 "A3 ortak") aynı kanal kimliği, aynı yük ve `kaynak_kayit` bayrağıyla, iletilerin `log_time` değerlerine göre ve `izleme/hiz` çarpanıyla yayınlar. Kayıtta bulunmayan kanallar yayınlanmaz; aboneliği reddedilmez, yalnız veri gelmez. Bu çerçevelerin `sira` alanı kayıttaki iletinin MCAP `sequence` değeridir (A6-K09).
- **A3-K113** İzleme sürerken dünya fizik adımı koşmaz; izlemenin `calistirma/durum` çerçevelerinde `durum = izleme`'dir ve `t_ns` kayıttaki konumdur.
- **A3-K114** `izleme/ara` verilen `t_ns`'e gider ve abone olunan her durum sınıfı kanal-anahtarın o andaki son değerini `ozet` bayrağıyla gönderir.
- **A3-K115** Kayıt sonuna gelindiğinde oynatma durur ve `izleme_durumu` olayı `kayit_sonu` ile üretilir; `izleme/bitir` durumu izlemeden önceki duruma döndürür.

### 4.14 Yardımcı süreçler

- **A3-K116** `POST /api/v1/surecler/{surec}/baslat|durdur` görselleştiriciyi ya da algılama bileşenini başlatır veya durdurur (`{surec}` = `gorsellestirici` | `algilama`). Kamera kademesi kapalı değilken canlı bir çalıştırmada durdurma isteği `onay: true` ister, aksi 409 `onay_gerekli`; durdurmanın kamera verisine etkisi A2 ve A8 kurallarıyla arıza olarak akar. Her değişiklik `surec_durumu` olayı üretir ve `surec/durum` kanalını günceller.

### 4.15 Emniyet ve durum revizyonları, tekrar kiplerinde müdahale, olay boyutu (düzeltme turları 1 ve 2)

- **A3-K117** `emniyet_rev` iskelet §3.2'deki tek sayaçtır: dünya tarafında (sim sunucusunda) tutulur; işaretsiz 32 bit (A3/A6 yüklerinde `u32`, JSON'da 1…4294967295); süreç başında 1; çalıştırmalar ve oturumlar arasında sıfırlanmaz; `0xFFFFFFFF`'te doyar ve doyduktan sonra süreç yeniden başlayana kadar bütün gevşetmeler reddedilir (409 `emniyet_rev_doydu`; `emniyet_rev_doydu` olayı bir kez). A1'deki `safety_rev` aynı sayaçtır (A1-K106 aynı listeyi yazar). Revizyonu **yalnız** şu altı işlem artırır; her işlem tam bir artırır, işlemin kaç tüpü etkilediği ya da durumu değiştirip değiştirmediği artışı değiştirmez; artış, işlemin işlendiği fizik sınırında, A3-K95'teki sıradaki yerinde yapılır: (1) kabul edilen her kısıtlayan komut: sanal emniyet girdisini kısıtlayan yönde değiştiren panel isteği ya da A4'ün daraltıcı sınıfındaki yer istasyonu komutu (komut başına); (2) bir fizik sınırında uygulanan, senaryo kaynaklı kısıtlayan atamaların tümü (tek işlem); (3) uygulanan her `SAFETY_CLOSE` kart olayı; (4) her bağlantı oturumunun sonu (nedeni ne olursa olsun); (5) bir fizik sınırında süresi dolan izinlerin tümü (tek işlem); (6) uygulanan her gevşetme: panelin gevşeten sanal girdi isteği (istek başına) ya da bir fizik sınırında uygulanan senaryo kaynaklı gevşeten atamaların tümü (tek işlem). Artırmayanlar: uygulanan `SAFETY_OPEN` (izin açma ve yenileme), `FIRE`, reddedilen her girdi, sürekli çıktılar. Gevşeten (A4'ün genişletici sınıfındaki) yer istasyonu komutu kabul edildiği andaki revizyona bağlanır; bağlantı emülatöründen çıkıp A1'e verileceği anda revizyon değişmişse atılır (A3-K101). Güncel değer `emniyet/girdiler.emniyet_rev` alanında, `GET /api/v1/emniyet` ve `GET /api/v1/durum` yanıtlarında, onay jetonu yanıtında ve `emniyet_girdisi_degisti` olayında yayınlanır. (Düzeltme turu 2'deki "yeni çalıştırma ve `sifirla` revizyonu artırır" kuralı W1-uyum U2 ile kaldırıldı; çalıştırma değişiminde jetonlar ve bekleyen gevşetmeler `run_id` bağıyla geçersizleşir: A3-K94, K54.)
- **A3-K118** Emniyeti **gevşeten** her komut — emniyeti azaltan sanal girdi değişikliği (A3-K93) ve izni genişleten yer istasyonu komutu (A3-K103) — `beklenen_run_id` yanında `beklenen_emniyet_rev` taşır. Sunucu eşzamanlı denetimde onu güncel değerle karşılaştırır; uyuşmazsa 409 `emniyet_rev_uyusmuyor` ve `ayrinti.guncel_emniyet_rev` döner. Revizyon doyduysa 409 `emniyet_rev_doydu` döner. Uyuşursa komutu kuyruğa alır ve dünya karşılaştırmayı adım sınırında, uygulamayla aynı atomik işlemde yeniden yapar (A3-K95); orada tutmazsa komut `komut_sonucu` olayında `reddedildi` / `emniyet_rev_uyusmuyor` ve `sonuc_ayrinti.guncel_emniyet_rev` ile sonuçlanır.
- **A3-K119** Emniyeti **kısıtlayan** komutlar — emniyeti artıran sanal girdi değişikliği (A3-K92) ve izni daraltan yer istasyonu komutu (A3-K104) — revizyon eşleşmesi istemez; istekte `beklenen_emniyet_rev` varsa yok sayılır. Bir kısıtlama bayat olduğu gerekçesiyle hiçbir zaman reddedilmez.
- **A3-K120** Tekrar kiplerinde müdahaleler Tablo 4.15'e uyar. Kayıtlı girdiyle beslenen kiplerde (`tekrar_girdi_sil`, `tekrar_yeniden_kurma`) girdiyi değiştiren her müdahale (arıza enjeksiyonu ve kaldırılması, sanal emniyet girdisi değişimi, yer istasyonu komutu ve onunla yapılan parametre değişimi) eşzamanlı denetimde 409 `tekrar_kipinde_yasak` ile reddedilir. Senaryoyu yeniden simüle eden kipte (`tekrar_fizik`) arıza ve sanal emniyet girdisi müdahaleleri kabul edilir; ilk kabul edilen müdahaleyle birlikte koşu yeni bir deney sayılır: sunucu `tekrar_mudahalesi` olayını bir kez üretir, kayda ilk müdahalenin uygulandığı adım (`canli_mudahale_adimi`) ve `guvence_nedenleri = canli_mudahale` yazılır ve kaydın `tekrar_guvencesi` `yok` olur (A6-K53).
- **A3-K121** Olay ayrıntısının serileştirme politikası sabittir: boşluksuz JSON; ASCII dışı karakterler ham UTF-8 olarak yazılır ve `\u` kaçışı üretilmez; yalnız `"` ve `\` kaçışlıdır (2 bayt); ayrıntıdaki dizeler denetim karakteri (U+0000–U+001F) içeremez ve her serbest dize şemada `^[^\u0000-\u001f]*$` deseni ve `maxLength` taşır; her tamsayı alanı şemada `minimum` ve `maximum` taşır ve sınırlar ±(2⁵³ − 1) içindedir; her dizi `maxItems`, her ek nesne `maxProperties` taşır. Bu politikayla her olay türünün en kötü durum boyutu 8192 baytı aşmaz (Tablo 3.5b; kod noktası başına en çok 4 bayt). Denetim betiği şemada sınırsız bir sayısal alan, dize, dizi ya da nesne gördüğünde başarısız olur ve her olay türü için şemanın izin verdiği en kötü örneği üretip boyunu ölçer. Yeni olay türü ancak bu koşulları sağlayan şemayla eklenir. Ondalık sayılar yalnız `minimum`/`maximum` ile f32 aralığına (|x| ≤ 3.4028234663852886·10³⁸) sınırlı alanlarda bulunur ve ECMAScript Number dizilimiyle (RFC 8785 §3.2.2.3) yazılır; böyle bir sayının dizilimi en çok 24 bayttır. Yer istasyonu yanıtının `veri` nesnesi (`$defs/sinirli_nesne`) bu kuralla f32 parametre değerini taşır.
- **A3-K122** `durum_rev` dünya tarafında tutulan, sim sunucusu süreci boyunca 1'den başlayıp yalnız artan bir `u64` sayaçtır; yeni çalıştırmada sıfırlanmaz. Uygulanan her çalıştırma kontrolü komutunda (durumu değiştirmeyen `hiz`, `izleme/ara` gibi komutlar ve aynı duruma dönen `sifirla` ile `senaryo/yukle` dahil) ve her iç geçişte (Tablo 4.7a: `hazirlaniyor` → `kosuyor` ya da `yuklendi`, `kosuyor` → `bitti`, → `hata`, `adimla`'nın `duraklatildi`'ya dönüşü) tam bir artar. Güncel değer `calistirma/durum.durum_rev` alanında ve `GET /api/v1/durum` yanıtında yayınlanır.
- **A3-K123** Çalıştırma kontrolü komutları — `senaryo/yukle`, `calistirma/baslat`, `duraklat`, `surdur`, `adimla`, `hiz`, `durdur`, `sifirla`, `yeniden_kostur` ve `izleme/*` — zorunlu `beklenen_durum_rev` taşır. Sunucu eşzamanlı denetimde onu güncel değerle karşılaştırır; uyuşmazsa 409 `durum_rev_uyusmuyor` ve `ayrinti.guncel_durum_rev` döner. Uyuşursa komutu kuyruğa alır; dünya (ya da senkron uçlarda çalıştırma denetçisi) `beklenen_durum_rev`, `beklenen_run_id`, durum (Tablo 4.7a) ve kip (Tablo 4.7b) önkoşullarını geçişle aynı atomik adımda yeniden denetler; biri tutmazsa komut `komut_sonucu` olayında `reddedildi` ve ilgili kodla (`durum_rev_uyusmuyor`, `beklenen_durum_uyusmuyor`, `gecersiz_durum`, `kipte_gecersiz`) ve `sonuc_ayrinti.guncel_durum_rev` ile sonuçlanır. Uygulanan her komut `durum_rev`'i artırdığından aynı `beklenen_durum_rev` ile gönderilmiş komutlardan yalnız ilki uygulanır; iki istemcinin aynı çalıştırmaya gönderdiği iki `sifirla` da buna dahildir.
- **A3-K124** Güncel `run_id` `baslat` ve `yeniden_kostur` kabulünde atanır; çalıştırma `bitti`, `durduruldu` ya da `hata` olunca biten çalıştırmanın kimliği olarak kalır; `sifirla`, `senaryo/yukle` ve `izleme/baslat` uygulandığında `null` olur; `bos`, `yuklendi` ve `izleme` durumlarında `null`'dur (Tablo 4.7a′). `beklenen_run_id` güncel `run_id` `null` iken `null` olmak zorundadır, değilken güncel kimliğe eşit olmak zorundadır; şema (`$defs/run_id_ya_da_null`) ve Tablo 4.7a′ aynı kabul kümesini tanımlar.
- **A3-K125** (W1 doğrulama turu, Y5.) Bir `atama_celiskisi` olayı en çok 8 öğe (`degerler[i]` ile `olay_kimlikleri[i]` aynı öğenin değeri ve kimliğidir) taşır. A5'in izin verdiği daha büyük bir çelişki kayıpsız olarak ardışık olaylara bölünür: bütün parçalar aynı `celiski_kimligi`'ni (çalıştırma içinde 1'den artan, u32), aynı `t_ns`, `alan` ve `uygulanan_deger`'i taşır; `parca_no` 1'den `parca_sayisi`'na artar (en çok 512 parça = 4096 öğe, A5'in olay sınırı), yalnız sonuncuda `son_parca` doğrudur; öğeler A5 dosyasındaki sırayla, parça sırasıyla dağıtılır ve her parça (sonuncusu hariç) tam 8 öğe taşır. Parçalar aynı adım sınırında, parça sırasıyla yayınlanır. Parçaların olay sıra numaraları artandır ama ardışık olmak zorunda değildir (araya başka olaylar girebilir); birleştirme yalnız `celiski_kimligi`, `parca_no` ve `son_parca` ile yapılır. İstemci ve A6 okuyucusu parçaları böyle birleştirir (A6-K61).

**Tablo 4.15 — Tekrar kiplerinde müdahaleler.**

| Müdahale | `tekrar_fizik` (senaryoyu yeniden simüle eder) | `tekrar_girdi_sil`, `tekrar_yeniden_kurma` (kayıtlı girdiyle beslenir) |
|---|---|---|
| `duraklat`, `surdur`, `durdur`, `sifirla`, `adimla`, `hiz` | Serbest (Tablo 4.7a, 4.7b) | Serbest (Tablo 4.7a, 4.7b) |
| Kanal aboneliği, `GET` uçları, izleme okuması | Serbest | Serbest |
| Arıza ekleme ve kaldırma | Kabul; koşu yeni deney (A3-K120) | 409 `tekrar_kipinde_yasak` |
| Sanal emniyet girdisi değişimi | Kabul; koşu yeni deney (A3-K120) | 409 `tekrar_kipinde_yasak` |
| Yer istasyonu komutu (parametre değişimi dahil) | 409 `kipte_gecersiz` (bu kipte kart ya da çekirdek yoktur) | 409 `tekrar_kipinde_yasak` |
| Görselleştirici ve algılama süreci başlatma/durdurma | Serbest (kamera çıktısı bu kipte tüketilmez) | Serbest |
| Senaryo yükleme | Tablo 4.7a (canlı çalıştırmada geçersiz) | Tablo 4.7a |

---

## 5. Hata durumları

| Durum | Algılama | Sunucunun davranışı | İstemcinin davranışı |
|---|---|---|---|
| Jeton yok ya da yanlış; `Host` ya da `Origin` izinli değil | A3-K06…K09 | 403 `erisim_reddedildi` ya da el sıkışma reddi; `erisim_reddedildi` olayı (A3-K12) | Kullanıcıya erişim hatasını gösterir; yeniden denemez |
| A3 büyük sürüm uyuşmazlığı | `GET /surum` | — | Bağlanmaz (A3-K15) |
| Bilinmeyen kanal, olay ya da alan | Katalogda yok | — | Atlar ya da yok sayar (A3-K18, K19) |
| Bozuk ikili çerçeve | `yuk_uzunlugu` aşımı | — | Çerçeveyi atar, sayar (A3-K25) |
| Yavaş istemci, durum sınıfı | Yuva dolu | Eskiyi ezer, sayar (A3-K40) | — |
| Yavaş istemci, dizi sınıfı | Kuyruk dolu | En eskiyi atar, sayar, `oncesinde_dusme` (A3-K41) | Boşluğu gösterir |
| Yavaş istemci, olay sınıfı | Olay kuyruğu dolu | 4001 ile kapatır (A3-K42) | Yeniden bağlanır, eksik olayları ister (A3-K45) |
| Dünya–A3 halka tamponu taştı | Yazma başarısız | Dünya beklemez; `a3_halka_tasma`; olay düştüyse `olay_kaybi` (A3-K38, K44) | Kaybı gösterir; olayların tamamı A6'dadır |
| İstenen olay geçmişi bellekte yok | `sonra` penceredışı | 410 `gecmis_yok` (A3-K45) | Kullanıcıyı kayda yönlendirir |
| Canlılık kaybı | 15 s Pong yok | 4004 ile kapatır (A3-K46) | Yeniden bağlanır |
| Bütün istemciler koptu | Bağlantı yok | Çalıştırma sürer (A3-K43) | — |
| Çözülemeyen istek gövdesi, şema dışı, büyük gövde, yanlış içerik türü | A3-K51 | 400 / 413 / 415; komut numarası ve A6 kaydı (A3-K52) | Hatayı gösterir |
| Geçersiz durum ya da kip | Tablo 4.7a, 4.7b | 409; durum değişmez (A3-K61, K64) | Hatayı gösterir |
| Yarışan komut | `beklenen_*` uyuşmuyor | 409 `beklenen_durum_uyusmuyor` (A3-K54) | Durumu yeniler |
| Aynı istek yeniden gönderildi | Yineleme anahtarı ve gövde özeti | Saklanan yanıt, `yineleme: true` (A3-K53) | — |
| Aynı `istek_kimligi` başka yöntem, yol ya da gövdeyle | Yineleme anahtarı | 409 `kimlik_cakismasi`; uygulanmaz (A3-K53) | Yeni kimlik üretir |
| Bozuk, JSON olmayan ya da sınırı aşan gövde | A3-K51 | 400 / 413 / 415; `a3/gecersiz_istek` kaydı (A3-K52) | Hatayı gösterir |
| Çalıştırma kontrolünde bayat `durum_rev` (yarışan ikinci komut dahil) | A3-K123 | 409 `durum_rev_uyusmuyor` + güncel revizyon; sınırda ise `komut_sonucu` reddedildi | Durumu yeniler |
| Çalıştırma yokken dolu ya da çalıştırma varken `null` `beklenen_run_id` | A3-K124 | 409 `beklenen_durum_uyusmuyor` (canlı çalıştırma gerektiren uçta `null` için `canli_calistirma_yok`) | Durumu yeniler |
| Gevşeten komutta bayat `emniyet_rev` | A3-K118 | 409 `emniyet_rev_uyusmuyor` + güncel revizyon; sınırda ise `komut_sonucu` reddedildi | Durumu yeniler, yeniden onay ister |
| Aynı sınırda kısıtlama ve gevşetme | A3-K95 | Kısıtlama uygulanır, gevşetme `kisitlama_hakim` | Gösterir |
| Sürüm/platform/özet koşulu sağlanmayan tekrar | A3-K68 | 409 `tekrar_kosulu_saglanmiyor`; `guvencesiz: true` ile güvencesiz başlar | Nedenleri gösterir |
| Kayıtlı girdiyle beslenen tekrarda müdahale | A3-K120 | 409 `tekrar_kipinde_yasak` | Gösterir |
| Emniyet revizyonu doydu | `emniyet_rev` = `0xFFFFFFFF` | Bütün gevşetmeler 409 `emniyet_rev_doydu`; olay bir kez (A3-K117) | Sim sürecinin yeniden başlatılmasını önerir |
| Genişletici komut emülatördeyken revizyon değişti | Emülatör çıkışında karşılaştırma | Komut atılır, `bayat_yetki` (A3-K101) | Gösterir; yeniden onay ister |
| Fiş ya da pim isteğinde `tup` yok; acil durdurmada `tup` var | `$defs/girdi_secimi` | 400 `gecersiz_istek` (A3-K89) | Düzeltir |
| Komut kuyruğu dolu | 256 komut | 503 `mesgul`; emniyeti artıran ve izni daraltan istisna (A3-K59) | Kullanıcıya bildirir |
| Ön denetim kaldı | Ön denetim raporu | `yuklendi`'ye dönüş, rapor (A3-K62) | Raporu gösterir |
| Senaryo ya da arıza tanımı A5'ten geçmedi | A5 doğrulayıcısı | 422 `senaryo_gecersiz` + A5 raporu (A3-K77, K81) | Raporu yol ve satırla gösterir |
| Hedef arayüz arızayı reddetti | 04/05/06 parametre denetimi | `komut_sonucu` reddedildi (A3-K85) | Gösterir |
| Onay eksik ya da jeton geçersiz (süre, kullanım, `emniyet_rev` değişimi, yeni çalıştırma, `sifirla`, başka oturum ya da işlem) | A3-K93, K94, K103 | 409 | Kullanıcıdan yeniden onay ister |
| Yer istasyonu komutu emülatörde kayboldu | Emülatör kararı | `yer_istasyonu_komutu` olayı; yeniden göndermez (A3-K102) | Gösterir |
| Kayıt okunamıyor | A6 §5 | 422 `kayit_okunamadi` (A3-K68) | Gösterir |
| Sim sunucusu çöktü | Bağlantı kapanır | Yeni süreç yeni jeton ve yeni `run_id` üretir; kapanmamış kayıt A6 §5'e göre okunur | Eski jetonla bağlanamaz; yeni açılış adresi gerekir |
| Sunucu kapanıyor | Olağan kapanış | 1001 ile kapatır (A3-K49); yeni istek 503 `mesgul` | — |

## 6. Kalite kademelerinin etkisi

- Fizik, emniyet ve sayaç kanalları (`fizik/durum`, `emniyet/girdiler`, `atis/durum`, `sayac/gercek_zaman`, `olay`, `calistirma/durum`) her kademede aynı içerikle yayınlanır; emniyet kabulü kademeden bağımsızdır (iskelet §6).
- **Kamera kapalı:** `algilama/kayit` kanalında kamera kaynaklı çerçeve olmaz; `surec/durum` algılama bileşenini `kapali_kademe` gösterir. Ön denetimde `algilama_bileseni` maddesi `atlandi` olur.
- **3B görünüm ve kamera kapalı:** görselleştirici başlatılmaz; `surec/durum` `kapali_kademe` gösterir; `gorsellestirici` ön denetim maddesi `atlandi` olur.
- **Sonar kademesi:** `sonar/ping360` ve `sonar/ping1d` her kademede yayınlanır; satırı üreten kademe yükteki `kademe` alanındadır.
- **Kayıt kademesi:** izleme kipinde yayınlanabilecek kanal kümesini belirler (A6 §4.3); canlı yayını değiştirmez.
- **Panel yükü:** iskelet §6'da panel yayını ayrı bir kademe özelliği değildir. Düşük donanımda yük, yayın tavanı (A3-K37) ve istemcinin abonelik hızlarıyla kısılır. Düşük donanımlı Windows bilgisayarında hangi tavanın gerektiği ölçülmedi (M20, MP8; 8. başlık md.1).
- Kamera kareleri 1.0'da panele akmaz. Kamera kademe değerleri A2'de tanımlıdır (tam 640 × 480 / 30 Hz, sade 320 × 240 / 15 Hz; tahmin); tam kademede RGB8 kareler ≈ 27,6 MB/s eder (hesap, ölçülmedi). Kare görüntüleme gerekirse yeni kanal küçük sürümle eklenir.

## 7. Uygunluk

Test kodu bu işte yazılmaz. Test türleri: **birim** (tek modül), **sözleşme** (sunucu ile istemci kitaplığı arasında, sahte dünya ile), **SIL** (gerçek sim sunucusu ve SIL koşucusu), **kartlı** (HIL).

| Kurallar | Test türü | Sınama yöntemi ve gözlenecek büyüklük |
|---|---|---|
| K01–K03 | sözleşme | Varsayılan ve 0 portla başlatma; yapılandırmaya `0.0.0.0` ve LAN adresi verilince sürecin hata koduyla çıkması; başka arabirimden bağlantının reddi |
| K04–K12 | sözleşme | Jetonsuz, yanlış jetonlu, yanlış `Host`, yanlış `Origin`, `Origin`'siz ama jetonlu istekler; WS el sıkışmasında alt protokol seçimi ve jetonun yansıtılmaması; `OPTIONS` 405; CORS başlığının yokluğu; red olayının hız sınırı ve `adet` |
| K13–K19 | birim + sözleşme | `GET /surum` ve `GET /katalog` ile özet eşitliği; büyük sürümü değiştirilmiş sahte sunucuda panelin bağlanmaması; sonuna alan eklenmiş (`kanal_surumu` + 1) ve alan eksik çerçevelerin okunması; bilinmeyen kanal ve olay atlanması |
| K20–K28 | birim | C++ kodlayıcı ile TypeScript çözücünün (ve tersinin) depodaki altın çerçevelerde bayt bayt aynı sonuç vermesi; bozuk `yuk_uzunlugu`, aralık dışı değer, geçersiz UTF-8; `sira` sürekliliği; bayrak bitleri |
| K29–K37 | sözleşme | Zorunlu kanalların aboneliksiz gelmesi ve bırakılamaması; 50/25/10/5/2/1 Hz aboneliklerde gözlenen hızın `min(istenen, azami, tavan)` ile ±1 tik uyumu; yeni değer yoksa gönderim olmaması; abone olunmayan kanalın gelmemesi; özet bayrağı |
| K38–K49 | SIL | Bir istemci okumayı keser, diğeri 50 Hz tüketir: dünyanın adım süresi dağılımı istemcisiz koşudan ayırt edilemez (eşik M20'de), HIL'de `kacirilan_adim = 0`; sayaçların artışı; okuma kesen istemcinin olay kuyruğu dolunca 4001; yeniden bağlanınca `olaylar?sonra=` ile boşluksuz olay dizisi; 410; Ping/Pong zaman aşımı; ikili ve büyük ileti kapanış kodları |
| K50–K60 | sözleşme | Başlıksız, kayıtsız istemci, yanlış içerik türü, büyük gövde; `Content-Length` sınırı aşan istekte sunucunun 4096 bayttan fazla okumaması; her durum değiştiren isteğin `komut_no` ile A6'da `a3/komut` ya da `a3/gecersiz_istek`'te bulunması (geçersiz UTF-8, JSON olmayan ve sınırı aşan gövde dahil); aynı anahtar ve gövdede yinelemenin yeniden uygulanmaması ve `yineleme: true` (senaryo kaydetme dahil bütün uçlarda); aynı `istek_kimligi`'nin başka yol, yöntem ya da gövdeyle `kimlik_cakismasi`'na yol açması; farklı istemcilerin aynı `istek_kimligi`'nin çakışmaması; saklama ömrünün sonunda kaydın silinmesi; geçerli JSON olup uç şemasına uymayan isteğin (ör. 65 karakterlik ya da nesne tipinde `istek_kimligi`) `a3/gecersiz_istek`'e yazılması; iki istemcinin aynı `beklenen_durum_rev` ile yarışan iki `sifirla`'sından — aynı sınırda ve ardışık sınırlarda — yalnız birinin uygulanması ve diğerinin `durum_rev_uyusmuyor` alması (A3-K123); hiç çalıştırma başlamamışken `yuklendi`'de `beklenen_run_id: null` ile `sifirla`'nın kabulü ve dolu kimlikle reddi (A3-K124); dolu kuyrukta 503 ve emniyet istisnası |
| K61–K69 | sözleşme + SIL | Her (durum, komut) çifti için Tablo 4.7a/4.7b'deki geçiş ya da ret; HIL'de onaysız `duraklat`'ın reddi ve onaysız `surdur`'un kabulü; `adimla n` sonrası `adim_no` artışının `4n` olması; `hiz` aralığı; ön denetim kaldığında `yuklendi`'ye dönüş, olayda yalnız özet ve kimlik, tam raporun kimlikle alınması; koşulu sağlamayan tekrarın `guvencesiz`siz reddi ve `guvencesiz: true` ile güvencesiz kaydı |
| K70–K75 | SIL + kartlı | Hata enjektörüyle geciktirilen yanıtlarla: her neden için bit, `gecerli` ve olayın Tablo 4.8'e uyması; `gercek_zamani_kacirdi`'nın tek olayla 1 olması; `parti_kacirildi` sayacının A6 `hil/bariyer` kayıtlarındaki `kacirildi` sayısına eşitliği; Tablo 4.8 bit 0–11'in A1-K79 nedenleriyle ve A6 kapanış kaydındaki adlarla aynı olması; özet olayının saniyede en çok bir kez gelmesi |
| K76–K80 | sözleşme | A5'in geçersiz dosya takımıyla doğrulama ve yükleme; ad kalıbı ve dizin dışına çıkma denemeleri; `uzerine_yaz`; çözümlenmiş senaryonun A6 ekiyle bayt eşitliği |
| K81–K88 | SIL | Her sınıftan en az bir elle arıza; `E<n>` kimlikleri; koşullu başlangıcın reddi; reddedilen parametrenin `komut_sonucu`; aynı senaryo, tohum ve kayıtlı elle arıza adımlarıyla iki koşuda aynı arıza dizisi |
| K89–K99, K117–K119 | sözleşme + SIL | Artıran yönün tek adımda, azaltan yönün yalnız geçerli jetonla uygulanması; süresi geçmiş, kullanılmış, başka istemciye ya da girdiye ait jetonun reddi; aynı adımda pim takma + atış olayında olayın `ret_emniyet` sonuçlanması, aynı adımda pim çıkarma + atış olayında olayın eski (kısıtlı) durumla değerlendirilmesi; A1'den yetkili durumu değiştirme denemesinin etkisizliği; uç listesi denetimi (listede olmayan yolların 404'ü); `emniyet_rev`'in A3-K117'deki her olayda tam bir artması; bayat `beklenen_emniyet_rev`'li gevşetmenin hem eşzamanlı denetimde hem adım sınırında reddi; aynı sınıra gelen eski bir "pimi çıkar" ile yeni bir "pimi tak"ta, geliş sırası iki yönde de denenerek, pimin takılı kalması ve çıkarmanın `kisitlama_hakim` ile reddi; incelemedeki ardışık sınır sırasının (pim takılı, `emniyet_rev` = 7 iken "pimi çıkar" jetonu alınır; "pimi tak" N sınırında uygulanır ve yetkili durumu değiştirmeden `emniyet_rev`'i 8 yapar; eski çıkarma N+1 sınırına ulaşır) 409 `emniyet_rev_uyusmuyor` (istek N'den sonra gönderildiyse eşzamanlı denetimde) ya da `komut_sonucu` reddedildi / `emniyet_rev_uyusmuyor` (istek N'den önce kuyruğa girdiyse) ile sonuçlanması ve pimin takılı kalması; kısıtlayan komutun bayat revizyonla ve dolu kuyrukta kabulü; jetonun yeni çalıştırmada, `sifirla`'dan sonra, `emniyet_rev` değişiminden sonra, başka oturumda ve başka işlemde reddi |
| K100–K107 | SIL | Listede olmayan komutun reddi; emülatör L2 profilinde kaybolan komut için `emulator_dusurdu` olayı ve yeniden gönderim olmaması; genişleten komutun jetonsuz reddi, daraltan komutun dolu kuyrukta kabulü; `kart/durum` ile `yer_istasyonu/durum` gecikme farkının emülatör gecikmesiyle uyumu; UDP portu açılmadığının denetimi |
| K108–K115 | SIL | Her çalıştırmada dosya açılıp kapanması; kademe geçersiz kılması; izlemede kanalların `kaynak_kayit` ile ve kayıttaki `log_time` aralıklarıyla yayınlanması; arama sonrası özet; izlemede canlı komutların reddi |
| K116 | SIL | Kamera açıkken onaysız durdurmanın reddi; durdurmanın `surec_durumu` olayı |
| K89–K97 (tüp başına) | sözleşme + SIL | `f1` fişi takılı, `f2` fişi çıkık durumunun durum yükünde, HTTP yanıtında ve A6'da kayıpsız temsili; `tup`'suz fiş isteğinin ve `tup`'lu acil durdurma isteğinin 400'ü; bir tüp için alınan jetonun öteki tüpte reddi |
| K95, K117 | SIL | İskelet §3.2'nin altı artıran işleminin her birinde tam bir artış, artırmayanlarda (izin açma, `FIRE`, ret, sürekli çıktı, yeni çalıştırma, `sifirla`) artış olmaması; iki tüpü kapatan tek acil durdurmanın tek artışı; aynı sınırda adım 1–3'te artış olduğunda bütün gevşetmelerin reddi; yapay olarak `0xFFFFFFFE`'den başlatılan sayaçla doyma ve doyma sonrası her gevşetmenin reddi |
| K100–K105 | SIL | `GET /yer_istasyonu/komutlar` yanıtının `protocol/komutlar.json` ile bayt eşitliği; katalog dışı komutun reddi; genişletici komutun emülatördeyken araya giren daraltıcıdan sonra `bayat_yetki` ile atılması ve karta hiç ulaşmaması; `PARAM_GET` yanıt olayında `veri`'nin kataloğun `sonuc` şemasına uyması |
| §3.3 sayaç kanalları | SIL + kartlı | Kartlı koşuda `kart/baglanti_sayaclari`'nın 36 alanının `IDA_LINK_STATS.cnt[]` ile, `kart/cekirdek_sayaclari`'nın 22 alanının `IDA_CORE_COUNTERS` ile ada göre eşitliği; `algilama/parti`'de boş (`count` = 0) ve geç partilerin bulunması ve `algilama/sayaclar` toplamlarının A8-K65'in kare sayısıyla eşitliği; Tablo 4.8 bitlerinin A1-K79 nedenleriyle eşlenmesi |
| K120 | SIL | Tablo 4.15'in her hücresi: kayıtlı girdili kiplerde yasak müdahalelerin 409'u; `tekrar_fizik`'te ilk müdahaleden sonra `tekrar_mudahalesi` olayı, kayıtta `canli_mudahale_adimi` ve `tekrar_guvencesi = yok` |
| K121, §3.5 | birim | Denetim betiğinin her olay şeması için şemanın izin verdiği en kötü örneği üretmesi, A3-K121 politikasıyla serileştirip boyunu ölçmesi ve hesapla karşılaştırması; şemada sınırsız sayısal alan, dize, dizi ya da nesne kalırsa ve boy 8192'yi aşarsa başarısız olması; sunucunun kodlayıcısının aynı örneklerde aynı boyu üretmesi ve `\u` kaçışı yazmaması |
| K122–K124 | sözleşme + SIL | Her çalıştırma kontrolü komutunda ve iç geçişte `durum_rev`'in tam bir artması; aynı `beklenen_durum_rev` ile gönderilmiş her komut çiftinde yalnız birinin uygulanması; eşzamanlı denetimden geçip sınırda önkoşulu bozulan komutun `komut_sonucu` reddi; Tablo 4.7a′'nın her satırında `beklenen_run_id`'nin kabul kümesi |

## 8. Açık kalanlar

1. **Yayın ve kuyruk değerleri (tahmin):** yayın tiki 50 Hz, kanal azami hızları, dizi kuyruk sınırları (Ping360 256, algılama 64, MAVLink 256), olay kuyruğu 4096, olay geçmişi 65 536, istek kimliği ve komut geçmişi 10 000, komut kuyruğu 256, ikili ileti 1 MiB, yük 65 536 bayt. Ölçülmedi; panel yükünün dünya adım süresine etkisi ve düşük donanımdaki tavan M20 (MP8, Windows) ölçümüyle sabitlenir. Değişiklik bu değerlerin küçük sürümle güncellenmesidir.
2. **08'den sapma — yayın tabanı 60 Hz yerine 50 Hz (mimari seçimi):** 50 Hz'in periyodu (20 ms) fizik adımının (16 adım) ve kontrol periyodunun (4 periyot) tam katıdır; 60 Hz 13/14 adım dönüşümü gerektirir (04 §2.11 tablosu). Mimar onayı bekler.
3. **08'den sapma — kontrol sahipliği yok:** 08 §2.1'in "kontrol sahipliği (kiralama)" önerisi yerine eşit yetkili istemciler ve `beklenen_*` ile iyimser eşzamanlılık (A3-K54, K55). Gerekçe: paneller aynı kullanıcının ayrı pencereleridir; yarışan komutların tekrar üretilebilirliği her komutun uygulama adımının kaydıyla zaten sağlanır. Mimar onayı bekler; rol gerekirse büyük sürüm değişikliği değil, küçük sürümle isteğe bağlı başlık olarak eklenir.
4. **Onay jetonu ömrü 10 s** (08 §3.4'te de tahmin), **yineleme kaydı ömrü 10 dakika**, **saklanan ön denetim raporu sayısı 64** ve **varsayılan port 47100**: mimari seçimi.
5. **WebSocket/HTTP kitaplığı** (Boost.Beast ya da IXWebSocket; 08 §6): seçilen kitaplığın sunucu tarafında `Sec-WebSocket-Protocol` seçimini, Ping/Pong'u ve engellemesiz yazmayı desteklediği doğrulanmadı; W5 uygulama işinde doğrulanır.
6. **Kayıt diski eşiği 1 GiB** (`kayit_disk_alani`): tahmin; kayıt yazıcı verimi ölçümüyle (A6 §8) birlikte değerlendirilir.
7. **A1'e bağlı yük alanları:** `kart/durum`, `yer_istasyonu/durum`, `atis/durum` ve `emniyet/girdiler.kart_*` alanları A1'in taşıyacağı büyüklüklere bağlıdır (10. başlık). A1 bir büyüklüğü taşımazsa alan `yok_degeri` ile gider; A1 dondurulunca bu kanalların `kanal_surumu` 1 düzeni kesinleşir.
8. **A8 algılama kaydının tel boyutu:** `algilama/kayit.kayit` için 512 bayt üst sınırı tahmindir; A8 sözleşmesi dondurulunca kesinleşir.
9. **Tarayıcı dışı istemcilerin tarayıcı ile aynı alt protokol yolunu kullanması** (A3-K07) varsayılmıştır; komut satırı araçları için ayrı bir kimlik yolu tanımlanmamıştır.
10. **`emniyet_rev`'i artıran işlemlerin listesi** iskelet §3.2'nin kapalı listesidir (A3-K117); A3 bu listeyi değiştiremez.
12. **A1-K85 ile sıra farkı** W1-uyum U3 ile kapandı: iki sözleşme iskelet §3.2'nin beş adımını aynen yazar (A3-K95).
11. **3. başlık md.3'teki sapma (kontrol sahipliği yok)** düzeltme turu 1'de `emniyet_rev` ile tamamlandı; mimar kararı (2026-11-20) A3A6-yanit.md'dedir.

## 9. Açık bulgu kapanış tablosu

`docs/mimari/acik-bulgular.md` "A3 — Sim sunucusu ↔ web panelleri" başlığındaki bulguların tamamı:

| Bulgu | Durum | Sözleşmede nerede | Gerekçe |
|---|---|---|---|
| AB-056 | kapandı | §3.8; A3-K62, K121; Tablo 3.5b | Ön denetim raporunun şeması (`$defs/on_denetim_raporu`, en çok 32 madde) ve madde kodları tanımlandı; olay yalnız rapor kimliği ve sınırlı özet taşır (en kötü durum 2104 bayt ≤ 8192), tam rapor kimlikle HTTP'den alınır. |
| AB-057 | kapandı | A3-K53…K55, K92…K95, K117…K119, K122…K124; Tablo 4.7a′; 8. başlık md.3, md.11 | Kontrol sahipliği yerine eşit yetkili istemciler; çalıştırmalar arası yarış `beklenen_run_id` ve `beklenen_durum_rev` ile, aynı çalıştırma içindeki emniyet ve yetki yarışları dünya tarafında tutulan `emniyet_rev` ile çözülür: gevşeten her komut `beklenen_emniyet_rev` taşır ve revizyon adım sınırında uygulamayla atomik olarak yeniden karşılaştırılır; aynı sınırda kısıtlama hâkimdir; kısıtlama hiçbir zaman bayat sayılmaz; onay jetonu çalıştırmaya, revizyona, oturuma ve işleme bağlıdır; durumu değiştirmeyen kısıtlama da `emniyet_rev`'i artırdığından önceden alınmış jetonlar ve bekleyen gevşetmeler geçersizleşir (incelemedeki ardışık sınır sırası dahil); çalıştırma kontrolü yarışları dünya tarafındaki `durum_rev` ile atomik olarak çözülür ve aynı revizyonla yarışan komutlardan yalnız ilki uygulanır; çalıştırma yokken `beklenen_run_id` `null`'dur (mimar kararları, 2026-11-20). Davranış küçük bir yürütülebilir modelle bütün kısa geliş sıralarında sınandı (A3A6-yanit-2.md). |
| AB-058 | kapandı | 1. başlık (kapsam dışı); §3.2–3.4 | A3 canlı yayını kendi WebSocket protokolüdür; Foxglove SDK kullanılmaz, bu yüzden 00 A2 sorusunun (Rust bağımlılığı) yanıtı A3'ü etkilemez. |

Uyumsuzluk tablosunda A3'e doğrudan değinen satır yoktur. A3'ü ilgilendiren ama başka başlıkta duran bulgular (bilgi için; tabloyu mimar işler):

| Bulgu | Önerilen durum | Nerede | Gerekçe |
|---|---|---|---|
| AB-206 | kapsam dışı | A3-K107; 1. başlık | İskelet §3 ve §10 md.17 (0.8'den beri; 0.10'da aynı): harici yer istasyonu uç noktası ilk sürümde yok; UDP ucunun kimlik denetimi sorusu düşer. |
| AB-221 | açık (ölçüm) | 8. başlık md.1; §6 | Panel yoğunluğu ve tavan değerleri M20/MP8 ölçümünü bekler. |
| U-12 (sim tarafı) | kısmen | A3-K95 | Aynı adım sınırında sanal emniyet değişikliğinin atış olayına göre sırası tanımlandı; kart tarafı A1/A4'tedir. |

Kritik önemde açık bırakılan A3 bulgusu yoktur.

## 10. Başka sözleşmelerden beklenenler

**A1'den beklenen:**

0. (W1-uyum) `protocol/komutlar.json` kataloğu (U16): her komut için `sinif`, `onay`, `parametreler` ve `sonuc` şemaları; `sonuc` şemaları `$defs/sinirli_nesne` sınırlarına (en çok 16 alan, skaler değerler, dizeler ≤ 64 karakter, tamsayılar ±(2⁵³ − 1) ve `minimum`/`maximum` ile sınırlı, ondalık sayılar f32 aralığında) uymalı; atış isteği komutu yalnız onaylı genişletici olarak ve karta iletilen istek anlamıyla bulunmalı. A1-K106 ve K85'in iskelet §3.2'yi aynen yazması (U2, U3); Tablo 3.11a ve 3.11b sayaç adlarının bu sözleşmedeki yüklerle aynı kalması (U18); `run_id` (u32) türetimi (U17; A1-K126); çalıştırma nesli değişiminin oturum kapanışı ve yeni oturum olması (A1-K128); `IDAW_` önekli tel enum adları (U15; bu belge enum değerlerine adla değil değerle bağlıdır).
1. Kart → sim telemetrisinde, `kart/durum` yükündeki büyüklüklerin taşınması: kestirimin ait olduğu ölçüm zamanı, açılış sayacı, otopilot kipi (değer adlarıyla), otopilot arm durumu, sağlık bitleri, kestirim geçerliliği, NED konum, NED hız, yönelim, derinlik, şarj durumu ve (tanısal) adım süresi; aynı büyüklüklerin yer istasyonu düzlemindeki telemetride de bulunması.
2. Atış kontrol emniyet zinciri durumunun (`IDA_FC_STATUS.state`, `inhibit_mask`, `last_event_id`) telemetride taşınması; kartın emniyet girdilerine dair bildirdiği tek büyüklük `inhibit_mask`'in emniyet bitleridir (A3-K97); A3 yeni bir A1 alanı istemez; değer ve bit anlamları A4 §3.16'dadır, A3 yinelemez (`atis/durum.fc_durumu`, `engel_maskesi`).
3. (W1-uyum U16 ile 0. maddeye taşındı) Kartın yanıt kodları (`IDA_CMD_RESULT.ack`, `reason`) A4'ün kapalı tablolarındandır; A3 bunları `veri` içinde değerle taşır.
4. Bariyer kararlarının (zamanında, geç bekletildi, kaçırıldı, yinelenen ret, geç ret) ve Tablo 3.11a/3.11b sayaçlarının tanımları; bu sözleşmedeki adlar A1'in adlarıdır (W1-uyum U18).
5. Bekleme sınırı, sürekli komut kullanım süresi ve kart bağlantı zaman aşımı değerleri (iskelet §4); A3 bunları göstermez, A6 üst verisine yazılır.
6. Tek seferlik olay türlerinin adları ve sim'in olay kararlarının (uygulandı, yineleme, emniyet reddi, süre reddi, diğer ret) A1'deki karşılıkları.
7. A1 oturum durumlarının (06 §2.9) ve geçiş nedenlerinin adları.
8. Yer istasyonu komut sınıflamasının (`daraltici` / `genisletici` / `notr`, katalogda) `emniyet_rev` ve A3-K95 sırası için kullanılması.
9. A1'deki `safety_rev` A3'teki `emniyet_rev` ile aynı sayaçtır; artış kuralı dünyanındır ve iskelet §3.2'nin kapalı listesidir (A1-K106 ve A3-K117 aynen yazar; A1-K89 ayrı artış tanımlamaz). A1-K85 iskelet §3.2'nin beş adımını aynen yazar (W1-uyum U2, U3).

**A5'ten beklenen:**
1. Doğrulama raporunun JSON biçimi (yol, satır, kural kimliği, ileti) ve şeması; A3 bunu `rapor` alanında değiştirmeden taşır.
2. Arıza tanımının (`arizalar[]` öğesi) JSON karşılığı ve doğrulayıcının tek öğeyi doğrulayabilmesi; `baslangic` alanında `hemen` ve `sim_zamani` türlerinin bulunması.
3. Kayıt ayarlarının A3/A6 değer kümelerini kullanması (W1-uyum U18): kademe `olaylar` | `telemetri` | `tam` ("kapalı" yok: emniyet kaydı kaldırılamaz), kamera kare kaydı `kapali` | `pencereli` | `surekli`.
4. Çalıştırma kipi değerleri (`sil`, `hil_1x`, `hil_bariyer`) ve sanal emniyet girdilerinin başlangıç durumu ile zaman çizelgesi biçimi.
5. Çözümlenmiş senaryonun kanonik JSON baytları (A5-K17) ve davranış özeti (64 küçük onaltılık); A3/A6 bunu `sha256:` önekiyle gösterir. Olayların (`atama_celiskisi`, `gevsetme_reddedildi`, `donma_onceki_cikti_yok`, olay sonucu) A5 §3.15 alan adlarının kararlı kalması (A3 ayrıntı şemaları bu adlarla yazıldı); zamanlı ve koşullu emniyet atamalarının A3-K95 adım 2 ve 4'te işlenmesi (W1-uyum U3).

**A2'den beklenen:** görselleştiricinin süreç durumu, A2 §5 sayaçları (`kamera/halka_sayaclari`) ve `SimKareDusmeNedeni` kodlarının (1–8, 33–39) kararlı kalması; kamera kademe değerleri (tam 640 × 480 / 30 Hz, sade 320 × 240 / 15 Hz; tahmin) A2'de tanımlıdır.

**A4'ten beklenen:** §3.16'daki `fc.state` ve `inhibit_mask` tablolarının ve §3.15 günlük kodlarının kapalı ve kararlı kalması (A3 değerle taşır, anlamı A4'e bırakır); 22 çekirdek sayacının ve sekiz sağlık bitinin adlarının `kart/cekirdek_sayaclari` ile aynı kalması. A3-K95'teki sim tarafı sıra, A4-K45'teki "daraltma çağrıya hâkimdir" ilkesiyle aynı yöndedir; A4'ün kartta daraltıcı mandalları genişleticilerden önce uygulaması (A4-K45, K46) bu sözleşmenin varsayımıdır.

**A8'den beklenen:** algılama kaydının tel düzeni (80 bayt) ve en büyük boyutu (`algilama/kayit.kayit`, 512 bayt sınırı); sürümün `IDA_DET_SCHEMA_MAJOR`/`MINOR` iki alan olarak kalması; parti durumu (0–8) ve ret nedeni (`IDA_DET_RJ_*` 1–10) kodlarının kararlı kalması (`algilama/parti`, `algilama/sayaclar`).

**A7'den beklenen:** manifest özetinin metin biçimi (ön denetim `a7_manifest` maddesi).

**Uygulama işlerinden (sözleşme değil) beklenen:** 04'ün çalıştırmayı durduran hata listesi (`hata` durumuna geçiş); 05 ve 06'nın sayaç adlarının bu sözleşmedeki adlarla aynı tutulması; arıza dağıtıcısının 04, 05 ve 06 arayüzleriyle eşlemesi (AB-093, AB-094 A5 başlığında).

## 11. W1 çapraz uyum turu (2026-11-20)

Bu turda `docs/gorevler/W1-uyum.md`'nin A3'e düşen maddeleri işlendi; kural numaraları korundu, yeni kural eklenmedi.

| Madde | Yer |
|---|---|
| U2 — emniyet revizyonu `u32`, doyma, altı işlemlik kapalı liste | A3-K117 (iskelet §3.2 aynen), K93, K118; `yuk_emniyet_girdiler.emniyet_rev` (`u32`); JSON alanları 1…4294967295; `emniyet_rev_doydu` hata kodu ve olayı; A3-K67 |
| U3 — fizik sınırındaki sıra | A3-K95 (iskelet §3.2'nin beş adımı aynen); `gevsetme_reddedildi` olayı |
| U4a — yolda bekleyen genişletici | A3-K101, K102, K103; `bayat_yetki` (olay, sayaç, A6 emülatör kararı) |
| U6 — fiş ve pim tüp başına | A3-K89, K93, K94, K96, K97; `yuk_emniyet_girdiler`, `yuk_firlatici`; `$defs/girdi_secimi`, `tup_adi` |
| U16 — komut kataloğu | A3-K100, K102, K103, K104, K105; `$defs/a1_komut_listesi`; `yer_istasyonu_yaniti.veri` |
| U17 — çalıştırma kimliği | A3-K63; `yanit_durum.a1_run_id` |
| U18 — sayaç, olay, geçerlilik, özet, Ping360 | §3.3 sayaç kanalları (`sayac/gercek_zaman` A1 Tablo 3.11a adlarıyla, `kart/baglanti_sayaclari`, `kart/cekirdek_sayaclari`, `kamera/halka_sayaclari`, `algilama/parti`, `algilama/sayaclar`); olaylar 24–30; Tablo 4.8 (A1-K79 ile bire bir), A3-K71…K74; A3-K80 (kanonik JSON baytları, `sha256:` gösterimi); `yuk_ping360` cihaz birimleri ve §3.4 dönüşüm formülleri; `yuk_algilama_kayit` (A8 sürümü iki alan) |

## 12. W1 doğrulama turu (2026-11-20)

`docs/sozlesmeler/inceleme/W1-dogrulama-sol-4.md` (Y1–Y13) ve `W1-dogrulama-sol-1.md` (bulgu 5) üzerine mimar kararları işlendi; A1 ve A4'ün bu turdaki değişiklikleri karşılandı. Yeni kural: A3-K125.

| Madde | Yer |
|---|---|
| Y4 | A5 kimlik deseni `^[A-Za-z][A-Za-z0-9_]{0,31}$` olay şemalarında değiştirilmeden (`atama_celiskisi`, `gevsetme_reddedildi`, `donma_onceki_cikti_yok`, `senaryo_ogesi_uygulandi`) |
| Y5 | A3-K125; `$defs/olay_atama_celiskisi` (`celiski_kimligi`, `parca_no`, `parca_sayisi`, `son_parca`); fark incelemesi F3: sıra numaraları artan, ardışık değil |
| Y6 | A3-K71; `$defs/olay_gercek_zaman_kacirildi.neden` (Tablo 4.8 bit 0–11), `neden_biti`; Tablo 4.7b'de `hil_duraklatma` |
| Y10 | A3-K97; `yuk_emniyet_girdiler.kart_*` (A4 §3.16 engel bitleri) |
| Y13 | `yuk_algilama_kayit.batch_seq`, `yuk_algilama_parti.batch_seq` açıklamalarında `kare_sirasi` |
| Parça 1, bulgu 5 | A3-K89; `$defs/olay_emniyet_girdisi_degisti` → `$defs/girdi_secimi` |
| A1-K129 | A3-K101 (üç bekleme noktası); `sayac/gercek_zaman.komut_bayat_yetki` |
| A1-K125 | A3-K100, K105 (yinelenen öğe tablosu) |
| A4 §3.15, §3.17 | A3-K102 (panel gösterimi atıf verir) |
| A4 §3.14, K43 | A3-K102 (`type` = 0 "değer yok"; `veri`'de `param_type` ve `value` yazılmaz) |
