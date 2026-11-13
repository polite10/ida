# 02 — Ortak çekirdek

Sürüm: 0.1 (2026-11-12). Durum: taslak; mimar denetimi ve kullanıcı onayı bekliyor.

Bu bölüm [00-iskelet.md](00-iskelet.md) belgesine bağlıdır; iskeletle çeliştiği yerde iskelet geçerlidir. Dayanak gösterimi: `K`, `Ö`, `M`, `Ç`, `A` ve `06.6` gibi satır numaraları [../research/00-duzeltmeler-ve-kararlar.md](../research/00-duzeltmeler-ve-kararlar.md) belgesine; `06 §2.2` ham rapor bölümüne; `D06 E`, `D06 Y5`, `D06 #10` doğrulama maddelerine işaret eder. **(tahmin)** işaretli her sayı ölçülmemiş bir başlangıç değeridir; ilgili ölçüm ya da SIL ayarı çıkmadan kesinleşmez.

## 1. Amaç ve sorumluluklar

Ortak çekirdek, aracın otopilotudur: sensör örneklerinden seyrüsefer durumunu kestirir, görevi ve operatör komutlarını izleyerek setpoint üretir, sekiz iticiyi sürer ve arıza durumunda aracı güvenli duruma götürür. ISO C17 ile yazılır, donanıma ve işletim sistemine bağımlı değildir; aynı kaynak hem STM32U5A9J-DK'ya hem PC'ye (SIL) derlenir (iskelet §2, K3).

Sorumluluklar: port arayüzü (A4), kestirim, güdüm, kontrol, itki tahsisi, kipler, görev yönetimi, arıza güvenliği, sayı tipi ve derleme politikası, SIL koşucusu.

Yazmadıkları ve beklenen yer:

| Konu | Bölüm | Bu bölümün ondan beklediği |
|---|---|---|
| Hedef algılama, takip, atış çözümü, emniyet zinciri | 03 | `ida_fc_step` ve arka plan işinin içi; algılama kaydının kesin alanları (3.6) |
| Başlatma, saat, kesme düzeni, gözetim zamanlayıcısı, parametrelerin flash'ta saklanması | 01 | Çekirdeği 2.2'deki düzende çağırması; girdi biriktirme; IMU yokken duvar saati çağrısı |
| A1 mesaj kümesi, adım bariyeri, bağlantı emülatörü, MAVLink eşlemesi | 06 | A1 ↔ A4 dönüşümü; sıra numarasının uçtan uca korunması (Ç1) |
| İDA-0'ın itici yerleşimi, itici eğrisi, yüzerlik, sönüm | 04 | 2.5'teki araç girdileri |
| Sensör modelleri, örnekleme hızları ve zaman damgalarının anlamı | 05 | 3.3'teki alanların modelden üretilmesi |

Temel ilkeler:
1. **Tek sınır A4.** Çekirdek dış dünyayı yalnız bir adım çağrısının girdisi ve çıktısı olarak görür; geri çağırma, işletim sistemi, donanım başlığı yoktur (08 §2.6).
2. **Saat okumaz.** Ölçüm zamanı örneklerin damgasında, duvar saati girdide gelir; iki alan tip düzeyinde ayrıdır (iskelet §4, D06 E).
3. **Bellek ayırmaz, küresel değişken tutmaz.** Bütün durum, platformun statik ayırdığı tek `ida_core_t` içindedir; boyutlar derleme zamanında sabittir (08 §2.5, 08 §2.6).
4. **Simülasyondan habersizdir** (3.2).

## 2. İç yapı

### 2.1 Modüller ve veri akışı

| Modül (`core/src/…`) | Girdi | Çıktı |
|---|---|---|
| `port` | — | A4 tipleri, sabitler, `ida_core_step` |
| `ingest` | Adım girdisi | Ölçüm zamanına göre sıralanmış örnekler, tazelik bilgisi |
| `est` | IMU, basınç, DVL, manyetometre, GNSS | `ida_nav_t`, `ida_est_health_t`, seyrüsefer geçmişi |
| `fs` (arıza güvenliği) | Sağlık, sızıntı, batarya, bağlantı olayları, adım bilgisi | Arıza eylemi isteği, olaylar |
| `mode` | Komutlar, arıza isteği, görev durumu, atış hizalanma isteği | Etkin kip, rejim (yüzey/dalış) |
| `mission` | Görev listesi, nav durumu | Güdüm hedefi, görev ilerlemesi |
| `gd` (güdüm) | Hedef, nav, irtifa | Setpoint (rota, derinlik/irtifa, hız, yönelim) |
| `ctl` (kontrol) | Setpoint, nav, doyma bayrakları | Genelleştirilmiş kuvvet `τ` (6) |
| `alloc` (tahsis) | `τ`, batarya gerilimi | 8 itici komutu, doyma bayrakları, gerçekleşen `τ` |
| `fcif` | nav, izin, algılamalar, Ping360 profili | 03'ün hizalanma isteği ve tetik çıktısı |
| `param`, `telem` | Parametre komutları; bütün modüller | Parametre tablosu; telemetri kayıtları |
| `math` | — | `mat.h` (sabit boyutlu matris), kuaterniyon, `ida_math` sarmalayıcıları |

```
 platform (kart: 01 · SIL: koşucu)
   │ ida_step_in_t: IMU tetik örneği + biriken örnekler + komutlar + duvar saati
   ▼
 ┌ ida_core_step ───────────────────────────────────────────────────────┐
 │ ingest ─► est (ESKF15) ─► nav, sağlık, geçmiş ──────────► fcif (03)   │
 │              │                                         hizalanma ist. │
 │              ▼                                              │         │
 │             fs ─► mode ◄─ komutlar, mission ◄───────────────┘         │
 │                    │                                                   │
 │                    ▼                                                   │
 │                   gd ─► ctl ─► alloc ─► 8 itici komutu                 │
 │                          ▲       │ doyma bayrakları (sonraki adım)     │
 │                          └───────┘                                     │
 └───────────────────────────────────────────────────────────────────────┘
   │ ida_step_out_t: itici komutları, 2 tetik, telemetri, olaylar, canlılık
   ▼
 platform
```

### 2.2 Çalışma düzeni

**Çağrılma.** Her yeni IMU örneği bir adım üretir (iskelet §4; ArduSub "metronom" deseni, 06 §2.1). Platform, son adımdan beri gelen diğer örnekleri ve komutları biriktirir ve IMU örneğiyle birlikte tek girdide verir. Bir IMU örneği tam olarak bir adım ve bir aktüatör çıktısı üretir; çıktı IMU'nun sıra numarasını taşır. Bu, adım bariyerinin istediği koşuldur (Ç1). Yinelenen ya da eski sıra numaralı IMU örneği adım çalıştırmaz (3.1). IMU akışı kesilirse platform duvar saatiyle `ida_core_tick_wall` çağırır (Bölüm 4).

**Döngü hızları.** Hız grupları adım sayacına değil ölçüm zamanına bağlanır; IMU örneği kaybolsa da gruplar kaymaz. Ağır gruplar faz kaydırılarak aynı adıma düşürülmez (öneri; en kötü durum adım süresini düşürmek için).

| İş | Hız | Dayanak |
|---|---|---|
| Adım, nominal durum yayılımı, açısal hız kontrolü, tahsis | 400 Hz (IMU ile) | iskelet §4; M5 |
| ESKF kovaryans yayılımı | 100 Hz, biriken Δθ, Δv ile | 06 §3.2; PX4 tahmin adımı 10 ms (D06 #3) |
| Basınç güncellemesi | Geldikçe; sensör ≤ ~25 Hz | 05 §2.8 |
| DVL güncellemesi | Geldikçe; 2–15 Hz (irtifaya bağlı) | 05 §2.8 |
| Manyetometre (yalnız yön) | 10 Hz'e seyreltilmiş (tahmin); sensör 10–100 Hz | 05 §2.8 (doğrulanmadı) |
| GNSS | Geldikçe; 5 Hz | 05 §2.8 (PX4 varsayılanı) |
| Açı, derinlik, hız, konum döngüleri | 100 Hz (tahmin) | 06 §3.2 ölçeklendi |
| Güdüm, görev yürütücüsü, kip yöneticisi | 10 Hz (tahmin) | 06 §3.2 |
| Arıza değerlendirmesi | 50 Hz (tahmin); sızıntı her adımda | 06 §3.2 |
| Telemetri kayıtları | Akış başına 1–10 Hz (tahmin) | 06 §3.2 |

**Adım içindeki sıra.**
1. `ingest`: örnekler ölçüm zamanına göre sıralanır; zamanı IMU'dan büyük olan örnek sonraki adıma kalır. Aynı zaman damgasında sabit sıra uygulanır: IMU, basınç, DVL, manyetometre, GNSS, Ping1D, batarya, sızıntı, algılama, Ping360 (D06 E: sensör sıralaması protokolün parçası olmalı).
2. Tazelik: her sensörün son örnek yaşı, ölçüm zamanında hesaplanır.
3. `est`: nominal yayılım; zamanı geldiyse kovaryans yayılımı; ölçüm güncellemeleri (adım başına en çok `IDA_EST_MAX_SCALAR_UPD` skaler güncelleme, varsayılan 8 (tahmin); artanlar sonraki adıma kalır, böylece adım süresi sınırlı olur).
4. `fs`: arıza koşulları ve süreleri; eylem isteği.
5. `fcif`: 03'ün hızlı kısmı (durum makinesi) çağrılır; hizalanma isteği ve tetik çıktısı alınır.
6. `mode`: komutlar, arıza isteği, görev ve hizalanma isteği önceliğe göre birleştirilir.
7. `mission` ve `gd` (zamanı geldiyse), ardından `ctl` dış döngüler (zamanı geldiyse) ve iç döngü.
8. `alloc`: itici komutları ve doyma bayrakları; bayraklar bir sonraki adımın integral güncellemesine girer (PX4 deseni, D06 #10).
9. Çıktı: itici komutları, tetikler (03'ten aynen), telemetri, olaylar, parametre kayıt isteği, canlılık bayrağı.

03'ün sonar verisinde hedef algılama işi 400 Hz adımın içinde koşmaz; platformun düşük öncelikte çağırdığı ayrı bir arka plan girişidir (03'ten ve 01'den beklenen).

**Hesap bütçesi (tahmin; M5 ölçer).** Periyot 2,5 ms. ESKF15'in kovaryans yayılımı ve 200 skaler güncelleme/s için bağımsız işlem sayısı ≈ 855.000 MAC/s; MAC başına 3–6 çevrim varsayımıyla 160 MHz'te %1,6–3,2; Joseph formu genel yoğun çarpımlarla uygulanırsa %3,8–7,6. Bu sayılar nominal entegrasyonu, Jacobian'ları, kapılamayı ve bellek erişimini kapsamaz ve kapasite garantisi değildir (06.5, D06 Y5). Tahsis, önceden hesaplanmış 8×6 sözde ters ile adım başına 48 çarpma düzeyindedir. Tasarım hedefi: çekirdek adımının en kötü durum süresi ≤ 1,0 ms, ortalama işlemci payı ≤ %15 (hedef, ölçüm değil); kalan pay USB servisi, 03 ve platform içindir. Ortalama yük en kötü durum yanıtını kanıtlamaz (08.12); karar M5'in DWT ölçümüyle verilir. Statik bellek tahmini < 64 KB (tahmin; kartta yaklaşık 2,5 MB SRAM var, 00 Bölüm 6).

### 2.3 Kestirim

**Filtre.** Hata durumlu (çarpımsal) genişletilmiş Kalman filtresi, 15 hata durumu (06 §2.2; D06 N1: ESKF15 ve kademeli PID başlangıcı savunulabilir). Referans: MSS `ins_mekf` 15×15 hata kovaryansı (D06 #24), Solà ESKF notu (06 §2.2).

| | İçerik |
|---|---|
| Nominal durum | `p` (yerel NED, m), `v` (NED, m/s), `q` (gövde → NED), `b_g`, `b_a` |
| Hata durumu | `δp, δv, δθ, δb_g, δb_a` = 15 |
| İsteğe bağlı (ilk sürümde yok) | Basınç ofseti (1), su akıntısı (2, yalnız su takibiyle), manyetometre sapması (3). SIL'de gerekçelendirilirse eklenir; boyut derleme zamanı sabitidir |

**Öngörü.** Nominal durum her IMU örneğinde entegre edilir (dönme vektörü üstel eşlemesi, kuaterniyon normalleştirmesi). Kovaryans 100 Hz'te, birikmiş aralıkla `P = F P Fᵀ + Q_d` ile yayılır; yoğunlukları parametredir. Dünya dönüşü ihmal edilir (düşük maliyetli MEMS gürültüsü ve kısa dalış süresi varsayımı; 05'in IMU modeli dünya dönüşü içeriyorsa SIL'de etkisi ölçülür).

**Ölçüm modelleri.** Güncellemeler sıralı skalerdir, matris tersi gerekmez; Joseph formu, simetrikleştirme ve varyans alt sınırı uygulanır (06 §2.2).

| Sensör | Model | Kapı ve koşul |
|---|---|---|
| Basınç | `d = (P − P_atm)/(ρ g)`; ölçüm `d = p_z + (R l_p)_z` (montaj kolu) | `P_atm` yüzeyde açılışta kalibre edilir; `ρ` parametre. Yüzeye yakın dalga basıncı için `R` derinliğe göre büyütülür (tahmin; dalga etkisinin derinlikle `e^{-kd}` sönümü, 04.10) |
| DVL | `v_dvl = R_dvlᵀ (R_nbᵀ v + ω × l_dvl)` (06 §2.2) | Yalnız dip takibi; `velocity_valid`, FOM ve en az irtifa koşulu. Yüzeyde ve yüzeye çok yakınken kapalı (06 §2.2 md.4, tahmin) |
| Manyetometre | Yalnız yön açısı; sapma açısı parametre | İtici akımı kaynaklı bozulma riski nedeniyle üç eksen yerine yön (06 §4 md.5) |
| GNSS | Yatay konum (2) ve yatay hız (2); anten kolu | Yalnız `SURFACE_GNSS` kaynak kipinde; 3B fix, `eph` eşiğin altında |
| Ping1D | Filtreye girmez; irtifa güdümde kullanılır (2.4) | Dip düz değildir; ESKF durumu değil |

**Gecikme.** Ölçüm zamanı (DVL'de `time_of_validity`, yani yansıma anı; 05.3) adım zamanından eskidir. İlk sürüm: yenilik, seyrüsefer geçmişindeki ölçüm anına en yakın durumla hesaplanır, düzeltme şimdiki duruma uygulanır. Bu, düşük hızlı araç için yeterli olabilir ama doğrulanmamıştır (06 §2.2, kendi tahmini); yedek PX4 tipi gecikmeli ufuktur (06 §2.1, PX4 EKF2). Karar SIL ölçümüyle (Bölüm 6).

**Yenilik kapısı.** Kapı `P`'den değil `S = H P Hᵀ + R` üzerinden hesaplanır (D06 E, 00 Bölüm 6). Vektör ölçümler (GNSS konumu 2, GNSS hızı 2, DVL 3) önce bütün olarak Mahalanobis uzaklığıyla (`yᵀ S⁻¹ y < γ`, en çok 3×3 ters) kapıdan geçirilir, sonra skaler güncellenir; kısmi kabul olmaz. Sensörün verdiği `R` köşegen değilse skaler güncellemeden önce Cholesky ile beyazlatılır; eksenleri bağımsız saymak ancak böyle gerekçelenir (D06 E). `γ` sensör başına parametredir; test oranı `NIS/γ` sağlık metriğidir.

**Hata enjeksiyonu ve kovaryans sıfırlaması.** Güncellemeden sonra `δx` nominal duruma eklenir (`q ← q ⊗ Exp(δθ)`), `δx = 0` yapılır ve kovaryans `P ← G P Gᵀ`, `G = blokköşegen(I, I, I − [½δθ]×, I, I)` ile dönüştürülür (Solà; D06 E). `G ≈ I` kısaltması kullanılmaz; yalnız 3×3 blok değiştiği için maliyeti küçüktür.

**Yüzey ve dalış geçişi.** Kaynak kipleri `SURFACE_GNSS`, `TRANSITION`, `SUBMERGED_DR` derinlik histerezisiyle değişir: dalış > 0,5 m, yüzey < 0,2 m (06 §2.2, tahmin; anten yüksekliğine göre parametre). Dalışta GNSS füzyonu kapanır, konum kovaryansının büyümesine izin verilir; DVL kilidi de yoksa "konum güveni düşük" bayrağı kalkar. GNSS'in su altında ne zaman kesildiği sabit bir derinlikle varsayılmaz; fix kalitesi ve derinlik birlikte kullanılır (05.4).

**Yüzeye çıkışta konum düzeltmesi.** Fix ancak N ardışık iyi örnekten sonra kabul edilir (N = 5, 5 Hz'te ≈ 1 s; tahmin). Dalıştan sonra fix'in belirli sürede geleceği varsayılmaz (05.5). İlk fix kapıdan geçerse normal güncelleme yapılır. Geçmez ama kalite M ardışık örnek boyunca iyiyse kontrollü konum sıfırlaması yapılır: `p_NE` GNSS'e eşitlenir, ilgili `P` bloğu `R`'den kurulur, çapraz terimler sıfırlanır, `reset_counter` artar, dalış boyunca biriken ölü hesap hatası olay olarak kaydedilir ve güdüm yolu yeni konumdan yeniden kurulur (06 §2.2 md.3).

**Kestirim güveninin düşmesi.** Sağlık çıktısı: konum ve hız σ'sı, yön σ'sı, sensör başına test oranı ve ardışık ret sayısı, DVL kilit yaşı, `reset_counter`. Üç düzey:

| Düzey | Koşul (eşikler parametre; tahmin) |
|---|---|
| `EST_OK` | Aşağıdakilerin hiçbiri |
| `EST_DEGRADED` | Yatay konum σ > σ_max ya da DVL kilidi yok ve GNSS yok |
| `EST_BAD` | Hız ya da tutum test oranı sürekli > 1; tutum σ eşik üstü; durumda NaN/Inf; `P` köşegeninde negatif değer |

Süre koşulları Bölüm 4'te duvar saatindedir.

**Tek duyarlık: gerekçe ve sınırlar.** Kartın FPU'su yalnız tek duyarlıklıdır (D06 #8, D09 #10); `double` yazılımla mümkündür ama yoğun filtre aritmetiğinde `float` performans tercihidir, zorunluluk değildir (06.6). Konum yerel NED'de, orijine göre tutulur. 10 km'de `float` çözünürlüğü `2^(13−23)` m ≈ 0,977 mm'dir (06.7; ham rapordaki 0,6 mm yanlış); temsil çözünürlüğü kestirim doğruluğu demek değildir. Coğrafi orijin `int32` 1e-7 derece olarak tutulur; GNSS farkı tamsayı çıkarmayla alınıp `float` ile ölçeklenir, böylece çekirdekte `double` gerekmez (düz dünya yaklaşımı birkaç km için; sınır SIL'de ölçülür). Asıl sınırlar temsil değil birikimdir: (a) mutlak zaman `float` saniye tutulamaz; 1 saatte ULP `2^(11−23)` s ≈ 0,24 ms, adım periyodunun onda biri. Zaman `int64` ns tutulur, yalnız farklar `float`'a çevrilir. (b) `float` kovaryans pozitif tanımlılığını yitirebilir; Joseph formu, simetrikleştirme ve varyans alt sınırı bu yüzden zorunludur. (c) Varyansların dinamik aralığı geniştir (konum ile sapma); SIL'de aynı filtrenin yalnız test için derlenmiş `double` referansıyla karşılaştırılır (Bölüm 7).

### 2.4 Güdüm

- **Rota noktası ve yol takibi.** Bacak boyunca LOS; akıntıya karşı kalıcı çapraz hatayı sıfırlamak için ILOS (Caharija vd. 2016; Fossen ve Pettersen 2014; 06 §2.3). Kabul dairesi, bakış uzaklığı ve integral kazancı parametredir. Araç tam itkili olduğu için yol takibinde yön rotaya hizalanır; yanal hata ILOS ile yön düzeltmesine çevrilir.
- **Derinlik ve irtifa.** Dikey kanal yataydan ayrıdır (06 §2.3). Derinlik setpoint'i hız ve ivme sınırlı ikinci/üçüncü dereceden referans modelden geçer (`refModel3`, 06 §2.3). İrtifa tutmada setpoint `d_ref = d + (a − a_ref)` filtrelenerek üretilir; irtifa Ping1D mesafesinin eğim düzeltmeli değeri, yedeği DVL irtifasıdır. En az irtifa sınırı arıza güvenliğine girer.
- **İstasyon tutma.** NED konum hatası gövde çerçevesinde hız setpoint'ine çevrilir, yön ayrıca tutulur. Akıntıya burun tutma enerji için seçenektir (06 §2.3).
- **Sonar tarama deseni.** Ping360 mekanik taramalıdır; tam tur süresi menzil, örnek sayısı, açı adımı ve bağlantı hızıyla değişir, 50 m'de 33 s'ye kadar çıkabilir (05.2). İki desen: (a) **çizgi taraması**: biçerdöver deseni, hat aralığı = kapsama genişliği × (1 − örtüşme); hız, sektör tarama süresiyle sınırlanır; dönüşler önceden hesaplı yaylarla (06 §2.3). (b) **noktada tur**: ızgara noktalarında istasyon tutma ve tam tur tarama. Desen, görev yürütücüsünde sabit bellekle bacak bacak üretilir. Sonar verisi yalnız düz, sabit hız ve derinlikli bacaklarda geçerli sayılır; çekirdek bunu `scan_leg_active` bayrağıyla 03'e ve telemetriye bildirir.
- **Yüzey seyri ile dalış arasındaki geçiş.** Güdüm aşamaları: `SURFACE_TRANSIT` (derinlik kontrolü kapalı, araç pozitif yüzerlikle yüzeyde; LOS ve GNSS), `DIVE` (konum ve yön tutulurken derinlik referansı hız sınırlı rampayla iner; kaynak kipi `SUBMERGED_DR` olana dek bekler), `SUBMERGED`, `ASCENT` (hız sınırlı yükselme; yüzey eşiğinde derinlik kontrolü bırakılır). Aşamalar kestiricinin kaynak kipiyle aynı histerezisi kullanır (06 §2.4).

### 2.5 Kontrol ve itki tahsisi

**Kademeli yapı** (06 §2.4; ArduSub ve PX4 deseni, 06 §2.1):

| Kanal | Dış döngü (100 Hz, tahmin) | İç döngü (400 Hz) | Çıkış |
|---|---|---|---|
| Tutum | Kuaterniyon hatası → açısal hız setpoint'i (P, sınırlı) | Açısal hız PID | `K, M, N` |
| Derinlik | `z` hatası → dikey hız setpoint'i (P, sınırlı) | PI + net yüzerlik ileri beslemesi | `Z` |
| Yatay hız | `u_sp, v_sp` | PI + sönüm eğrisi ileri beslemesi | `X, Y` |
| Konum (istasyon tutma) | NED hata → gövde hız setpoint'i | — | yatay hız kanalına |

**Doyma ve integral birikimi.** Her PID kendi çıkış sınırını bilir. Tahsis katmanı her eksen için pozitif ve negatif doyma bayrağı döndürür; integral yalnız doymayı artıran yönde dondurulur, ters yönde çözülmesine izin verilir (PX4, D06 #10). Kip ve rejim geçişlerinde integral, mevcut çıkışı koruyacak biçimde başlatılır (06 §2.4).

**Yüzey ve dalış kontrolcüsü.** Her rejimin kendi kazanç seti vardır (06 §2.4). Yüzey rejiminde kontrol edilen eksenler `X, Y, N`'dir; roll ve pitch hidrostatiğe bırakılır, dikey iticiler havalanma riski nedeniyle sınırlı yetkiyle kullanılır (06 §2.4; sınır SIL'de ayarlanır). Dalış rejimi altı eksenlidir. Geçiş kestiricinin kaynak kipine bağlıdır, kazançlar T_blend (1 s, tahmin) boyunca karıştırılır ve integral sarsıntısız aktarılır.

**Sekiz itici için tahsis.** `τ = B u`, `B` 6×8, `u` itici kuvvetleri (N). Ağırlıklı sözde ters `B⁺` başlatmada ve parametre değişince (yalnız devre dışıyken) hesaplanır. Doyma olursa öncelikli doyma giderme uygulanır: önce düşük öncelikli eksenler ölçeklenir; öncelik sırası roll/pitch > derinlik > yön > ileri/yanal (öneri; ArduSub'ın dikey ve yatay grupları ayrı normalize etmesi ve PX4'ün sıralı doyma giderme yöntemi örnek alındı, 06 §2.4, 06 §2.1). Kuvvetten komuta geçişte itici eğrisi tersine çevrilir (`T ∝ n|n|` → `komut ≈ işaret·√|T|`), ileri/geri asimetrisi ve ölü bölge telafi edilir (06 §2.4); azami itki batarya gerilimine göre ölçeklenir (04.13). Çıktı `[-1, 1]` aralığında normalleştirilmiş sürücü komutudur; platform bunu `PWM = 1500 + 400·u` µs ile darbe genişliğine çevirir (04 bölümü taslağındaki normalizasyonun tersi). ESC'nin belgeli dur ölü bandı 1500 ± 25 µs olduğundan sıfırdan farklı komut en az `|u| = 25/400` olarak verilir (11.6, Ç12). Gerilime bağlı sıfır-itki aralığı ve eğri değerleri doğrulanmamış adaydır (Ç12, M22); hepsi parametredir.

**04'ten beklenen araç girdisi:** sekiz iticinin gövde çerçevesindeki konumu ve yönü (`B` bunlardan ve CG referansından kurulur; 04 taslağında yerleşim tablosu mimari varsayımı olarak var ve pervane tepki torku ihmal ediliyor), ileri/geri azami itki ve gerilime bağımlılığı, itici eğrisi, net yüzerlik ve CB–CG ofseti, ileri besleme için yaklaşık sönüm katsayıları, kazanç tasarımı için kütle ve eylemsizlik, kartın gördüğü itici sırası. Çekirdek bu değerleri kendi parametresi olarak taşır; fizikteki gerçek değerden kasıtlı olarak farklı verilebilmeleri gerekir (8. başlık).

**Pasif yüzeye çıkışın hızı.** 04 taslağındaki İDA-0 değerleriyle (tatlı suda net yüzerlik ≈ +1,96 N; 120 N dikey kuvvette uç hız ≈ 0,71 m/s) karesel sönüm yaklaşımı pasif yükselme hızını ≈ 0,1 m/s verir (kaba tahmin, doğrusal sönüm ihmal edildi). Bu nedenle `PASSIVE_SURFACE` yalnız kontrolün güvenilmez olduğu durumlar içindir; diğer arızalarda kontrollü `SURFACE` seçilir.

### 2.6 Kipler ve görev yönetimi

**Kipler** (06 §2.5'ten uyarlandı):

| Kip | Ön koşul | Davranış |
|---|---|---|
| `INIT` | — | Parametre doğrulama (CRC, aralık), kestirici hizalama |
| `DISARMED` | — | Motorlar nötr; arm komutu ön koşullar sağlanırsa kabul |
| `MANUAL` | Arm | Operatör `τ` doğrudan tahsise |
| `STABILIZE` | Tutum geçerli | Tutum tutulur, öteleme operatörden |
| `DEPTH_HOLD` | Derinlik geçerli | Derinlik ve yön tutulur |
| `STATION_KEEP` | `EST_OK` | Konum, derinlik ve yön tutulur |
| `GUIDED` | `EST_OK` | Yer istasyonundan setpoint; 3 s yeni hedef gelmezse son hedefte kalır (ArduSub, 06 §2.1) |
| `AUTO` | `EST_OK`, geçerli görev | Görev yürütücüsü setpoint üretir |
| `SURFACE` | — | Kontrollü yükselme, yön tutulur |
| `PASSIVE_SURFACE` | — | Motorlar nötr, arm durumu korunur; İDA-0 pozitif yüzerlikle yüzer (iskelet §5) |

Geçiş önceliği: arıza eylemi > operatör komutu > görev. Ön koşulu düşen kip bir alt kipe iner (`AUTO`/`STATION_KEEP` → `DEPTH_HOLD`, 06 §2.5 tablosu). Arıza eylemleri kalıcıdır (mandal); açık bir sıfırlama komutuyla ve koşul geçmişse kalkar (11.4'teki "kendiliğinden toparlanma ile açık sıfırlama ayrıdır" dersi). Durum makinesi tam geçiş tablosu olarak yazılır; her durum × olay çifti tanımlıdır (06 §2.5'in Rust `match` gerekçesinin C karşılığı: tablo ve birim testle eksiksizlik).

**Görev durum makinesi:** `IDLE → LOADED → RUNNING(öğe i, aşama) ⇄ PAUSED → COMPLETE | ABORTED`. Her öğenin başlangıcı, setpoint üretimi, bitiş ölçütü (kabul dairesi + derinlik toleransı + bekleme süresi, ya da süre) ve zaman aşımı vardır; zaman aşımı görevi durdurur ve `SURFACE` ister. Yürütücü yalnız setpoint üretir, kip ve arıza katmanını atlayamaz (06 §2.5).

**Görev öğeleri** (sabit kapasiteli dizi, `IDA_MISSION_MAX` = 128 (tahmin)): `WAYPOINT` (konum, derinlik ya da irtifa kipi, hız, kabul yarıçapı), `DIVE_TO`, `SURFACE`, `HOLD` (süre), `SCAN_LINES` (dikdörtgen alan, yön, hat aralığı, derinlik/irtifa), `SCAN_POINT` (tur sayısı), `WAIT_GNSS`, `TARGET_ALIGN` (03'ün hizalanma aşaması, zaman aşımıyla).

**Görevin çekirdeğe verilişi.** Görev bağlantı komutlarıyla gelir: `MISSION_BEGIN(n)`, `MISSION_ITEM(i, öğe)`, `MISSION_END`. Platform bunları MAVLink görev protokolünden çözer (06; özel öğeler `MAV_CMD_USER_*` aralığında olabilir, 06 §2.5). SIL'de senaryodaki görev (A5) sim sunucusunun yer istasyonu yolundan aynı komutlarla gelir. Görev atomik yüklenir: bütün öğeler doğrulanmadan yürürlüğe girmez; çalışan görev yalnız `PAUSED` ya da `HOLD` iken değiştirilebilir.

### 2.7 Sayı tipleri ve derleme politikası

- **Standart:** C17, `-std=c17 -pedantic-errors` (GCC, Clang), MSVC `/std:c17`; VLA, `complex`, `<stdatomic.h>`, `<threads.h>` ve GNU uzantıları yok (08 §2.6; Ç10; kullanıcı onayı A17 açık).
- **Tipler:** `<stdint.h>` sabit genişlikli tamsayılar; kayan nokta yalnız `float`; çekirdekte `double` yoktur (2.3'teki tamsayı orijin sayesinde gerekmez). Sabitler `f` sonekli; matematik işlevleri `ida_math` sarmalayıcısıyla (`sinf`, `sqrtf` …), ileride kendi uygulamasına geçilebilsin diye; bu tek başına platformlar arası eşitlik vermez (Ç7).
- **Kayan nokta derleme politikası:** GCC/Clang `-ffp-contract=off -fno-fast-math`; Apple Clang'de daralma varsayılanı `on` olduğu için açıkça kapatılır (09.6); MSVC `/fp:precise`, daralma açılmaz; clang-cl MSVC ile özdeş sayılmaz (09.6). FMA'yı kapatmak bit eşitliği sağlamaz (08.14, 09.5). Kartın FPSCR ayarı (yuvarlama, sıfıra yuvarlama) 01'de sabitlenip kaydedilir; subnormal davranışı iki tarafta farklı olabilir (09.5).
- **SIL'in kartla aynı politikayı kullanması:** çekirdek tek bir CMake hedefinden, tek bir FP politikası arayüz hedefiyle derlenir; SIL kartın tiplerini ve üretimdeki firmware politikasını korur, fizik ise `double` kalır (Ö3, 09.5, D09 N3).
- **Bellek ayırma yasağının denetimi:** (1) çekirdek kütüphanesi CI'da `-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free` ile ve sarmalayıcı tanımlanmadan bağlanır; başvuru varsa bağlantı hatası olur (08.13: tanımsız sarmalayıcı hata verir, "boş malloc" vermez). (2) Nesne dosyalarında `nm` ile `malloc`, `calloc`, `realloc`, `free`, `_malloc_r` ve `printf` ailesi aranır (08.13: yeniden girişli libc girişleri ayrıca denetlenmeli). (3) Kendi havuz ayırıcısı yazmak da yasaktır (kod incelemesi kuralı). (4) Özyineleme yasak; `-fstack-usage` ve `-Wstack-usage` ile yığın sınırı raporlanır. (5) Yapı boyutları `_Static_assert` ile sabitlenir.
- **Statik analiz:** `-Wall -Wextra -Wconversion -Wdouble-promotion -Wfloat-conversion -Wshadow -Wvla`, GCC `-fanalyzer`, clang-tidy (`bugprone-*`, `cert-*`, `clang-analyzer-*`), cppcheck; MISRA'dan seçilmiş yazılı proje kural alt kümesi; "MISRA uyumlu" iddia edilmez (08 §2.6). Çekirdek hedefinin içerme yolunda HAL, CMSIS ve RTOS başlıkları yoktur.

### 2.8 SIL koşucusu

Çekirdeği PC'de kartın yerine çalıştıran ince katmandır (iskelet §2: C17 + C++).

- **Bağlantı:** sim sunucusuna A1 üzerinden yerel soketle bağlanır; el sıkışma ve mesajlar kartınkiyle aynıdır (06). Sim sunucusu için kart ile SIL arasındaki fark yalnız taşımadır.
- **Adımlama:** SIL'de bariyer hep açıktır. Koşucu soketten çerçeveleri okur; IMU çerçevesi gelince o ana dek gelen diğer örnekleri ve komutları `ida_step_in_t`'ye koyar, `ida_core_step` çağırır, aynı sıra numaralı aktüatör çerçevesini gönderir. Sim yanıtı bekler; zaman sanaldır, hızlandırılabilir, duraklatılabilir (iskelet §4).
- **Duvar saati:** varsayılan olarak son alınan çerçevenin sim zamanından türetilen sanal saattir; duraklatma bağlantı arızası üretmez, SIL tekrarlanabilir kalır. Gerçek zamanlı davranışı sınamak için gerçek duvar saati seçeneği vardır.
- **Kartla ortak olanlar:** çekirdek kaynağı ve derleme politikası; A1 kodlayıcısı (`protocol/`); A1 ↔ A4 dönüştürücüsü (öneri: kartla SIL'in paylaştığı tek C modülü; kartın simülasyondan haberdar olan tek parçası budur).
- **Ortak olmayanlar:** taşıma (USB ↔ soket), zamanlama (kesme ↔ engelleyen döngü), gözetim zamanlayıcısı (SIL'de yok), parametre deposu (flash ↔ A5 dosyası; dönüştürücü C++), DWT ölçümü. Süre aşımı, kaçırılan adım ve IMU kesilmesi SIL'de koşucu tarafından enjekte edilebilir.
- Küresel durum olmadığı için bir süreçte birden çok çekirdek koşabilir (Monte Carlo; 08 §2.6).

## 3. Arayüzler

### 3.1 A4: adım çağrısı

```c
void         ida_core_init(ida_core_t *c, const ida_params_t *p, ida_wtime_t now_w);
ida_step_rc  ida_core_step(ida_core_t *c, const ida_step_in_t *in, ida_step_out_t *out);
void         ida_core_tick_wall(ida_core_t *c, ida_wtime_t now_w, ida_step_out_t *out);
ida_rc       ida_core_nav_at(const ida_core_t *c, ida_stime_t t, ida_nav_t *nav);
size_t       ida_core_sizeof(void);   /* platform statik ayırma doğrulaması için */
```

`ida_step_rc`: `OK`, `DUP_SEQ` (aynı IMU sıra numarası; adım çalışmaz, platform önceki çıktıyı yeniden gönderebilir), `OLD_SEQ`, `BAD_INPUT`. Çekirdek yeniden girişli değildir; aynı `ida_core_t` üzerinde çağrıları platform sıralar. Çekirdek geri çağırma kullanmaz: girdi ve çıktı yalnız veridir. Böylece adım girdisinin kaydı, aynı platformda adımın bit düzeyinde yeniden üretilmesine yeter (Ç7).

**Zaman tipleri.** `ida_stime_t` (ölçüm zamanı; HIL ve SIL'de sim zamanı) ve `ida_wtime_t` (duvar saati) ikisi de `int64_t` ns taşıyan ayrı yapılardır; derleyici karıştırmayı reddeder. Ölçüm zamanı ns seçildi çünkü sim zamanı tamsayı ns'dir (iskelet §4).

**`ida_step_in_t`:** `imu` (tetik örneği), `n_samples` + `samples[IDA_IN_MAX]` (etiketli birlik; varsayılan 16, tahmin), `ping360` (isteğe bağlı tek profil; platform tamponuna `const` işaretçi, çağrı süresince geçerli), `n_cmds` + `cmds[8]`, `now_w`, `plat` (platform durumu: önceki adım süre aşımı bayrağı ve süresi, kuyruk taşması sayacı, bağlantı olayları).

**`ida_step_out_t`:** `seq` (IMU sıra numarası), `t_meas` (IMU zamanı), `armed`, `thr_cmd[8]` (`float`, [-1, 1]), `fire[2]` (03'ün tetik çıktısı; her adım sıfırla başlar), `n_telem` + `telem[8]`, `n_events` + `events[8]`, `param_save` (kalıcı kayıt isteği), `alive_ok` (gözetim zamanlayıcısı besleme koşulu, Bölüm 4).

### 3.2 Simülasyondan habersizlik

Kart yazılımı simülasyonda olduğunu bilmez (iskelet §2); A4'te bu şöyle sağlanır:
1. A4'te sim kipi, sim zamanı ya da sim kaynağı belirten hiçbir alan ve parametre yoktur; çekirdekte koşullu derleme (`#ifdef SIL` benzeri) yasaktır ve otomatik denetimle aranır.
2. Örnekler gerçek bir sürücünün üretebileceği biçimdedir: derinlik değil basınç, DVL için geçerlilik bayrakları ve FOM, GNSS için fix türü ve `eph`. Gerçek sürücü geldiğinde aynı yapıyı doldurur (06 §2.8; HAL sınırı "SI birimli, zaman damgalı, çerçevesi tanımlı örnek").
3. Ölçüm zamanı nötr bir alandır: HIL'de sim zamanı, gerçek donanımda sensörün veri hazır anındaki kart saati.
4. Çıktılar sürücüye gidecek isteklerdir: normalleştirilmiş itici komutu ve tetik isteği.
5. Yer gerçeği (ground truth) çekirdeğe hiçbir yoldan girmez; değerlendirme sim tarafında A3/A6 ile yapılır.
6. Algılama kaydı, algılamanın kartta, PC'de ya da simülatörde yapılmasından bağımsız aynı biçimdedir (iskelet §3).

### 3.3 Sensör örnekleri

Ortak başlık `ida_hdr_t`: `t_meas` (`ida_stime_t`), `seq` (`uint32_t`), `inst` (`uint8_t`, sensör örneği), `flags` (`uint16_t`: geçerli, doymuş, sensör arızası). Birimler SI, `float`; çerçeveler: gövde FRD, yerel NED, sensör çerçeveleri montaj parametreleriyle. Derinlik aşağı pozitiftir.

| Yapı | Alanlar |
|---|---|
| `ida_imu_t` | `gyro[3]` rad/s, `accel[3]` m/s² (özgül kuvvet), `interval_ns` (örnek aralığı; değerler aralık ortalaması, 05 §2.8 md.3), `temp_c` |
| `ida_mag_t` | `field[3]` T |
| `ida_baro_t` | `p_abs` Pa, `temp_c` |
| `ida_gnss_t` | `lat_e7`, `lon_e7` (`int32_t`), `alt_mm` (`int32_t`), `vel_ned[3]` m/s, `eph`, `epv` m, `fix_type`, `n_sv` |
| `ida_dvl_t` | `t_meas` = `time_of_validity` (yansıma anı, 05.3), `vel[3]` m/s (DVL çerçevesi), `fom` m/s, `cov[9]` (varsa), `vel_valid`, `beam_valid` (4 bit), `beam_dist[4]` m (çözülemeyen ışın −1, 00 Bölüm 6), `altitude` m, `track` (dip / su) |
| `ida_ping1d_t` | `distance` m, `confidence` %, `ping_number` |
| `ida_ping360_t` | `angle_grad` (0–399), `n_samples` (≤ 1200), `sample_period` (25 ns birim), `tx_duration`, `tx_freq_khz`, `gain`, `data[1200]` (`uint8_t`) (05 §2.1). Donanımda yakalama damgası yoktur (00 Bölüm 6); `t_meas` platformun verdiği ping zamanıdır |
| `ida_batt_t` | `voltage` V, `current` A, `temp_c` |
| `ida_leak_t` | `mask` (prob başına bit) |
| `ida_detection_t` | 3.6 |

Yüksek hızlı IMU'nun 400 Hz'e indirgenmesi (ortalama, FIFO) ve paketlemenin getirdiği bekleme (05.6) 05 ve 06'nın işidir; çekirdek adım başına bir IMU örneği alır.

### 3.4 Bağlantı

Çekirdek bayt ya da MAVLink görmez; platform çözülmüş komut verir ve telemetri kaydını kodlar. Böylece A1 biçimi (M9) değişse çekirdek değişmez.

- **Gelen `ida_cmd_t`:** `HEARTBEAT` (bağlantı canlılığı, duvar saati damgalı), `ARM`/`DISARM`, `SET_MODE`, `MANUAL_INPUT` (6 eksen, normalleştirilmiş), `GUIDED_SETPOINT`, `MISSION_BEGIN/ITEM/END/START/PAUSE/CLEAR`, `PARAM_SET`/`PARAM_GET`, `FAILSAFE_RESET`, `FC_CMD` (03'e aynen geçen atış komutları).
- **Giden `ida_telem_t`** (etiketli birlik): nav durumu, sağlık, kip ve arıza durumu, itici komutları ve doyma, görev ilerlemesi, parametre değeri, komut yanıtı, olay metni.

QGC'de hibrit kiplerin ve görev yüklemenin çalışması temel telemetriden ayrı kabul ölçütüdür (06.12, 09.15, M18).

### 3.5 Kalıcı parametreler

`ida_params_t`: `version`, `size`, `crc32` ve gruplar: araç (itici geometrisi, eğriler, yüzerlik), sensör (montajlar, `ρ`, sapma açısı), kestirim (gürültü yoğunlukları, kapılar, histerezis), kontrol (iki rejimin kazançları), güdüm, arıza (eşikler, eylemler, süreler), görev varsayılanları. Statik `const` tanım tablosu her parametrenin kimliğini, adını (≤ 16 karakter, MAVLink parametre adı sınırı), tipini, en küçük, en büyük ve varsayılan değerini tutar. Platform açılışta bloğu yükler (kart: flash, 01; SIL: A5 dosyasından). Çalışırken `PARAM_SET` aralık denetiminden geçer; kalıcı kayıt yalnız devre dışıyken `param_save` ile istenir. CRC ya da sürüm hatasında varsayılanlar yüklenir ve arm reddedilir.

### 3.6 Atış kontrol modülüne sağlanan arayüz (içi 03'te)

- **Seyrüsefer durumu `ida_nav_t`:** `t`, `p_ned[3]`, `v_ned[3]`, `q_nb[4]`, `omega_b[3]`, `depth`, `altitude`, `p_sigma[3]`, `v_sigma[3]`, `att_sigma[3]`, kaynak kipi, `reset_counter`, `est_level`.
- **Geçmiş:** `ida_core_nav_at(t)` son 2 s'yi (100 Hz, 200 kayıt; tahmin, 03'ün gerek duyduğu ufukla düzeltilir) ara değerleyerek verir; sonar profili ve algılamaların yakalama anındaki pozu için (07.7: sensör ölçümünden dünya konumu poz gerektirir).
- **İzin `ida_fc_permit_t`:** `armed`, `mode_ok` (`AUTO`'da `TARGET_ALIGN` öğesi ya da `GUIDED`), `est_ok`, `failsafe_clear`. Tetik kararı 03'ündür; çekirdek tetik çıktısını değiştirmez.
- **Hizalanma isteği `ida_align_req_t`:** `valid`, `yaw_ref`, `depth_ref`, `hold_ne[2]` (isteğe bağlı), `max_speed`, `tol_yaw`, `tol_pos`, `dwell`. Kip yöneticisi yalnız izin koşulları sağlanırken uygular; arıza eylemi her zaman önceliklidir. Yanıt `ida_align_status_t`: `accepted`, `aligned`, hatalar.
- **03'ün uygulayacağı imzalar:** `void ida_fc_step(ida_fc_t *fc, const ida_fc_in_t *in, ida_fc_out_t *out)` (adım içinde, hızlı) ve `void ida_fc_background(ida_fc_t *fc, ...)` (arka plan, algılama). `ida_fc_in_t`: nav, izin, adımdaki algılamalar, Ping360 profili, `FC_CMD`'ler, `now_w`. `ida_fc_out_t`: hizalanma isteği, `fire[2]`, telemetri.
- **Algılama kaydı `ida_detection_t`:** 07 §3.2 "Detection v1" alan listesi (00 Bölüm 3 bu listeyi geçerli sayar): `seq`, yakalama zamanı (`ida_stime_t`), kaynak, çerçeve, tür ve değerleri (yön, menzil-yön, menzil, poz), `cov`, sınıf, güven, üretici ve sürüm. Kesin alanlar 03'ün, A1 karşılığı 06'nındır.

## 4. Hata durumları ve davranışı

**Saat alanı kuralı** (iskelet §4): bir koşulun *verisi* ölçüm zamanında değerlendirilir (örnek yaşı, ölçüm tutarlılığı); koşulun *ne kadar sürdüğü* ve zaman aşımları duvar saatindedir. SIL'de duvar saati koşucunun sanal saatidir (2.8); bu yüzden süreler SIL'de de tekrarlanabilir.

Eylem sırası: `WARN < HOLD < SURFACE < PASSIVE_SURFACE < DISARM`; aynı anda birden çok arızada en ağırı uygulanır. `HOLD` ve üstü 03'e `failsafe_clear = false` verir.

| Arıza | Tetik (veri, ölçüm zamanı) | Süre (duvar saati) | Eylem (varsayılan; parametre) | Dayanak |
|---|---|---|---|---|
| Sızıntı | Sızıntı biti, 2 ardışık örnekte (tahmin) | Bekleme yok | Arm'lıysa `SURFACE` ve yüzeyde konum bildirimi; devre dışıysa uyarı. Mandallı | 06 §2.5; ArduSub'da eylem seçime ve arm durumuna bağlı (06.10) |
| Bağlantı kaybı | — (son `HEARTBEAT`'in duvar saati damgası) | 3 s ve 60 s (tahmin) | `MANUAL`/`STABILIZE`/`GUIDED`: 3 s → `HOLD`, 60 s → `SURFACE`. `AUTO`'da su altında kayıp beklenir: eylem yok; görev süresi ve coğrafi sınır denetlenir | 06 §2.5 |
| Düşük batarya | Filtrelenmiş gerilim ve tüketilen enerji; iki düzey | 10 s sürekli | Düşük: uyarı + görev iptali + `SURFACE`; kritik: `SURFACE`, sürerse `PASSIVE_SURFACE` | Süre 10 s doğru (11.4). ArduSub'ın 10,5 V'u Copter varsayılanıdır, Sub'da gerilim tetiği varsayılan kapalıdır (11.4); eşikler İDA-0 paketinden (04) gelir. Kritik eşik paketin üretici en düşük güvenli geriliminin altına inmez (11.12'deki paket için 12 V) |
| Derinlik sınırı | `d > d_max − pay` (pay 0,5 m, tahmin); irtifa < `a_min` | Anında | İniş setpoint'i kırpılır, yükselme setpoint'i verilir; tekrar ya da `d > d_max` → `SURFACE` | 06 §2.5; `d_max` araç parametresi (04) |
| Kestirim güveni | `EST_DEGRADED` / `EST_BAD` (2.3) | `DEGRADED` anında alt kip; 30 s sürerse (tahmin) `SURFACE`. `BAD` 2 s sürerse | `DEGRADED`: `AUTO`/`STATION_KEEP` → `DEPTH_HOLD` + yön tutma. `BAD`: tutum güvenilmez sayılır → `PASSIVE_SURFACE` | 06 §2.5; ArduSub "EKF kötü" 2 s (06 §2.1) |
| Adım kaçırma | IMU sıra boşluğu ya da `t_meas` aralığı > 1,5 periyot; platformun bildirdiği süre aşımı | 1 s içinde > 4 kaçırma (tahmin) | `WARN` + telemetri; sürerse `HOLD` | Ç1, iskelet §4; HIL'de sim de sayar ve çalıştırmayı geçersiz işaretler |
| IMU akışı yok | — | > 10 ms (4 periyot; tahmin): platform `ida_core_tick_wall` çağırır | Motorlar nötr (`PASSIVE_SURFACE`); `alive_ok = false` uzun sürerse gözetim zamanlayıcısı 01'in tanımıyla resetler | Ana döngü kilitlenmesi 2 s (06 §2.1); IWDG aralığı 07.10 |
| Sensör bayatlığı | Sensörün son örnek yaşı > sınır | Sınır sensör başına | İlgili güncelleme kapanır; kestirim düzeyine yansır | D08 Y8 (bayat veri davranışı tanımlanmalı) |
| Parametre hatası | CRC/sürüm | Açılışta | Varsayılanlar, arm reddi | — |

**Gözetim zamanlayıcısı beslemesi.** `alive_ok` yalnız 400 Hz adımın çalıştığını değil, IMU tazeliğini, son başarılı bağlantı servisini (platformun bildirdiği) ve kestirimde NaN olmadığını birlikte gösterir; yalnız ISR'den besleme yetmez (00 Bölüm 6, D08 E). IWDG süresi ve reset boyunca çıkışların elektriksel güvenliği 01'in ve M19'un konusudur.

**Girdi hataları.** NaN/Inf içeren örnek, sınır dışı değer ya da kapasiteyi aşan girdi atılır ve sayılır; adım her durumda tanımlı bir çıktı üretir. Ping360 profilinde `n_samples > 1200` reddedilir.

**HIL'de duraklatma.** Sim duraklarsa kartın duvar saati ilerler ve bağlantı/IMU arızaları tetiklenir; bu istenen davranıştır (Ç1: bariyer bütün firmware zamanını sim zamanına bağlamaz). 06'dan beklenen: HIL'de duraklatmanın ya desteklenmemesi ya da çalıştırmanın geçersiz sayılması.

## 5. Kalite kademelerinin etkisi

Otopilot her kademede tam çalışır (iskelet §6); çekirdekte kademe parametresi yoktur. Dolaylı etkiler: sonar kademesi Ping360 profilinin içeriğini değiştirir, biçimini değiştirmez; kamera kapalıysa dışarıdan algılama gelmez ve çekirdek ile 03 sıfır algılamayla çalışabilmelidir. SIL gerçek zamandan yavaş koşarsa sanal zaman nedeniyle çekirdek davranışı değişmez.

## 6. Ölçüme bağlı kararlar

| Karar | Birincil | Yedek | Ölçüm |
|---|---|---|---|
| Kontrol adımı hızı | 400 Hz | 200 Hz (06 §3.2) | M5 |
| Kovaryans yayılımı | 100 Hz, sabit boyutlu `mat.h` döngüleri | Seyrekliği açık elle açılmış çarpım; 50 Hz; CMSIS-DSP | M5 |
| Ölçüm güncelleme formu | Optimize skaler Joseph | Genel Joseph (sayısal sorun çıkarsa) | M5 + SIL `double` referans karşılaştırması |
| Gecikme telafisi | Geçmiş durumla yenilik | Gecikmeli ufuk (PX4) | SIL ölçümü (yeni ölçüm önerisi, 8. başlık) |
| Adım başına skaler güncelleme sınırı | 8 | Dağıtılmış ölçüm kuyruğu | M5 |
| Kartla SIL arasındaki fark eşikleri | 7. başlıktaki yöntemle belirlenir | — | M10 |
| Geciken/yinelenen adım | `DUP_SEQ` ile önceki çıktı | Güvenli duruma geçiş | M17, A8 (06 karar verir) |
| Kontrolcü | Kademeli PID | LQR (kazançlar SIL'de çevrimdışı; 06 §2.4) | SIL ayar sonuçları |

## 7. Test yaklaşımı ve kabul ölçütleri

**Birim testleri** (Unity; aynı testler kartta da koşar, 08 §2.6): kuaterniyon normu ve açı sarma; tahsisin doymasızken `τ`'yu geri üretmesi ve doymada sınır içinde kalması; integral dondurma yönü; kip geçiş tablosunun her durum × olay çifti; görev doğrulama ve atomik yükleme; parametre aralık denetimi; zaman tiplerinin karışmadığı (derleme hatası testi). Rastgele testler sabit tohumla, tekrarlanabilir koşar.

**Yayımlanmış referansla karşılaştırma.** ESKF: MSS `ins_mekf` (D06 #24); ILOS: MSS `ILOSpsi.m`; referans modeli: PythonVehicleSimulator `refModel3` (MIT) (06 §2.10). Referans çıktıları sabitlenmiş sürümle üretilip altın dosya olarak depoda tutulur; Python/Matlab bağımlılığı A16'ya bağlıdır. Referansla karşılaştırma tek başına yetmez (D06 E): kovaryans sıfırlama dönüşümü ve `S` tabanlı kapı ayrı testlerle sınanır; SIL Monte Carlo koşularında sim yer gerçeğiyle NEES/NIS tutarlılık testi yapılır.

**SIL senaryoları** (ArduSub autotest örnek alınır, 06 §2.10): derinlik tutma; akıntıda istasyon tutma; akıntıda ILOS ile rota; dalış–ölü hesap–yüzeye çıkış–GNSS ile konum sıfırlama; DVL kesintisi; GNSS gecikmesi; her kipte bağlantı kaybı; sızıntı; düşük batarya; derinlik sınırı; kestirim bozulması; kaçırılan adım ve IMU kesilmesi; tarama deseninin kapsaması. Her senaryonun geçti/kaldı ölçütü (çapraz hata RMS, derinlik hatası, aşım, arıza eylemi ve süresi) SIL'de ilk ayardan sonra sayıyla yazılır; şimdiden sayı verilmez.

**Kontrolcü ayarı simülasyonda yapılır** (06 §2.10): basamak yanıtlarıyla sistem tanılama; maliyet (IAE, aşım, aktüatör eforu) üzerinde ızgara ya da Nelder–Mead araması, SIL'de gerçek zamandan hızlı; her rejim için ayrı kazanç seti parametre dosyasına; sonuç HIL'de doğrulanır; model farkı için kazanç payı bırakılır.

**Kart ile SIL arasındaki sayısal fark (M10).** Adım girdileri kaydedilir (A6). (1) Aynı ikilide iki koşu bit düzeyinde aynı olmalı. (2) macOS-arm64 ile Windows-x64 ve (3) kart ile SIL, kayıt açık çevrimde yeniden oynatılarak karşılaştırılır; kapalı çevrimde küçük farklar yörüngeyi ayırır, bu yüzden açık çevrim esastır. Her çıktı alanı için en büyük mutlak ve göreli fark ile ilk ayrışma adımı raporlanır; eşikler bu ilk ölçümden sonra konur (Ç7: platformlar arası bit eşitliği beklenmez).

**Kabul ölçütleri:** birim testleri iki işletim sisteminde ve kartta geçer; bellek ayırma denetimi (2.7) ve statik analiz temiz; aynı ikilide tekrar üretim bit düzeyinde; M5 ölçümüyle en kötü durum adım süresi bütçe içinde; SIL senaryoları yazılı ölçütlerle geçer; M10 raporu çıkmıştır.

## 8. Açık noktalar ve iskelete öneriler

### 8.1 Açık noktalar

1. **Gecikme telafisi ve ESKF tutarlılığı** için ölçüm numarası yok; 00 belgesinin Bölüm 7'sine "ESKF gecikme telafisi ve NEES/NIS tutarlılığı (SIL)" maddesi eklenmesi önerilir.
2. **Çekirdeğin araç parametreleri ile fiziğin gerçek değerleri ayrı tutulmalı:** A5 araç tanımı (08) "fizik gerçeği" ile "otopilot parametreleri"ni ayırmalı; aksi halde model hatasına dayanıklılık sınanamaz.
3. **Zaman birimi:** 07'nin algılama kaydı `t_capture_us` kullanıyor; A4'te ns seçildi. 03 ve 06'ya ns önerilir.
4. **Geciken adım politikası** (A8, M17) 06'da kesinleşir; çekirdek yinelenen sıra numarasını tanır ve üç politikayı da destekler.
5. **İç basınç ve iç sıcaklık** ArduSub'da arıza girdisidir (06 §2.1) ama iskeletin sensör setinde yok; eklenip eklenmeyeceği mimarın kararıdır.
6. **Batarya eşikleri** İDA-0 paketine (04) ve açık soru A19'a bağlıdır.
7. **A16:** referans karşılaştırmasında Python/Matlab ve tasarım betiklerinde SymPy kullanımı kullanıcı onayı bekliyor.

### 8.2 İskelete öneriler

1. **§4 iki saat alanı:** çekirdek için "sim zamanı" yerine "ölçüm zamanı (HIL/SIL'de sim zamanı)" denmesi; gerçek sensörde aynı alanın kart saatine geçtiği açık olur.
2. **§4 emniyet süreleri:** "koşulun verisi ölçüm zamanında, süresi duvar saatinde" ayrımı ve SIL'de duvar saatinin sanal saat olduğu iskelete eklenmeli; yoksa SIL'de süreler bilgisayar hızına bağlı kalır.
3. **§4:** IMU akışı kesildiğinde platformun duvar saatiyle çekirdeği çağırdığı (`ida_core_tick_wall`) yazılmalı; adımı yalnız IMU'nun tetiklemesi, IMU kaybında arıza güvenliğini durdurur.
4. **§2 tablosu:** A1 ↔ A4 dönüştürücüsünün kart ve SIL koşucusunca paylaşılan C modülü olduğu ve sahibinin 06 mı 02 mi olduğu belirtilmeli.

İskelete aykırı bir gereklilik bulunmadı. Ham rapor 06'nın 200 Hz IMU önerisi yerine iskeletin 400 Hz hedefi kullanıldı; 200 Hz yedektir.
