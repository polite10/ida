# 02 — Ortak çekirdek

Sürüm: 0.2 (2026-11-13). Durum: taslak; ikinci inceleme bekliyor. Sürüm 0.2: iskelet 0.3'e uyum ve [inceleme/02-sol.md](inceleme/02-sol.md) bulgularının giderilmesi; madde madde yanıt [inceleme/02-yanit.md](inceleme/02-yanit.md).

Bu bölüm [00-iskelet.md](00-iskelet.md) belgesine bağlıdır; çelişkide iskelet geçerlidir. Dayanak gösterimi: `K`, `Ö`, `M`, `Ç`, `A` ve `06.6` gibi satırlar [../research/00-duzeltmeler-ve-kararlar.md](../research/00-duzeltmeler-ve-kararlar.md) belgesine; `06 §2.2` ham rapor bölümüne; `D06 E`, `D06 Y5`, `D06 #10` doğrulama maddelerine işaret eder. **(tahmin)** işaretli her sayı ölçülmemiş bir başlangıç değeridir; ilgili ölçüm ya da SIL ayarı çıkmadan kesinleşmez. Ortak kabuller (SI, ns zaman, NED, gövde x ileri / y sancak / z aşağı, kuaterniyon `(w, x, y, z)` Hamilton gövde → dünya) iskelet §3.1'dendir ve burada yinelenmez.

## 1. Amaç ve sorumluluklar

Ortak çekirdek aracın otopilotudur: sensör örneklerinden seyrüsefer durumunu kestirir, görev ve operatör komutlarından setpoint üretir, sekiz iticiyi sürer, arızada aracı güvenli duruma götürür. ISO C17'dir, donanım ve işletim sisteminden bağımsızdır; aynı kaynak karta ve PC'ye (SIL) derlenir (iskelet §2, K3).

Sorumluluklar: port arayüzü (A4), kestirim, güdüm, kontrol, itki tahsisi, kipler, görev yönetimi, arıza güvenliği, sayı tipi ve derleme politikası, SIL koşucusu (A1 ↔ A4 dönüştürücüsünün PC tarafı dahil; iskelet §3).

| Konu | Bölüm | Bu bölümün ondan beklediği |
|---|---|---|
| Algılama, takip, atış çözümü, emniyet zinciri | 03 | `ida_fc_step` ve arka plan işinin içi; atış olay kimliği; 3.7'deki sözleşmeye uyum |
| Başlatma, kesme düzeni, gözetim zamanlayıcısı, flash, kart tarafı dönüştürücü | 01 | 2.2'deki çağrı düzeni; ≥ 100 Hz duvar çağrısı; alım zamanı damgası |
| A1 mesaj kümesi, bariyer, alan eşlemesi | 06 | Zaman çerçevesi, eş zamansız güvenli çıktının kabulü, olay kimliğinin uçtan uca taşınması |
| İDA-0 itici yerleşimi, itki eğrisi, kütle durumları | 04 | 2.5'teki A5 parametre kaynağı (04 §2.6, §2.7) |
| Sensör modelleri, ölçüm zamanı ve kovaryans anlamı | 05 | 3.3'teki alanların anlamıyla üretilmesi |

Temel ilkeler: (1) tek sınır A4, geri çağırma yok (08 §2.6); (2) çekirdek saat okumaz, ölçüm zamanı ve duvar saati ayrı tiplerdir (iskelet §4); (3) bellek ayırma ve küresel değişken yok, bütün durum tek `ida_core_t` içinde (08 §2.5–2.6); (4) simülasyondan habersizlik (3.2).

## 2. İç yapı

### 2.1 Modüller ve veri akışı

| Modül (`core/src/…`) | Girdi | Çıktı |
|---|---|---|
| `ingest` | Çağrı girdisi | Kabul edilmiş, sıralanmış örnekler; tazelik durumu (4.1) |
| `est` | IMU, basınç, DVL, manyetometre, GNSS | Çıkış durumu `ida_nav_t`, sağlık, seyrüsefer geçmişi |
| `fs` | Sağlık, sızıntı, batarya, bağlantı, tazelik | Arıza eylemi isteği, olaylar |
| `mode` | Komutlar, arıza isteği, görev durumu | Etkin kip, güdüm aşaması, atış izni |
| `fcif` | nav, izin, algılamalar, sonar dilimi, arka plan sonucu | 03'ün hizalanma isteği ve atış olayı |
| `mission`, `gd` | Görev, nav, irtifa, hizalanma isteği | Setpoint |
| `ctl` | Setpoint, nav, doyma kalıntısı | İstenen genelleştirilmiş kuvvet `τ_req` |
| `alloc` | `τ_req`, rejim, batarya gerilimi | 8 itici komutu, elde edilen `τ_a`, eksen kalıntısı |
| `param`, `telem`, `math` | — | Parametre tablosu ve işlemleri; telemetri; `mat.h`, kuaterniyon, `ida_math` |

```
 platform (kart: 01 · SIL: koşucu)
   │ ida_step_in_t (IMU ile)          │ ida_tick_in_t (duvar saatiyle, IMU'suz)
   ▼                                  ▼
 ┌ ida_core_step ─────────────────────────────┐  ┌ ida_core_tick ─────────────────┐
 │ ingest ► est(ufuk + çıkış) ► fs ► mode ►   │  │ ingest ► tazelik ► fs ► mode ► │
 │ izin ► fcif(03) ► mission/gd ► ctl ►       │  │ güvenli çıktı (gerekirse)      │
 │ alloc ► çıkış kapısı                       │  └────────────────────────────────┘
 └────────────────────────────────────────────┘
   │ ida_out_t: sürekli itici komutu + tek seferlik olaylar + telemetri + canlılık
   ▼
 platform
```

### 2.2 Çalışma düzeni

**Çağrılar.** Her yeni IMU örneği bir `ida_core_step` çağrısı üretir (iskelet §4). Platform ayrıca duvar saatiyle `ida_core_tick` çağırır: kartta donanım zamanlayıcısı ≥ 100 Hz (01'den beklenen), SIL'de sim sunucusunun her kontrol periyodunda gönderdiği zaman çerçevesi (iskelet §4). SIL'de bir periyotta IMU varsa `step`, yoksa `tick` çağrılır; bariyer yanıtı her durumda o periyodun sıra numarasını taşır (2.8). İki çağrı aynı `ida_core_t` üzerinde eş zamanlı çalışmaz; sıralamayı platform yapar.

**Zaman ilişkileri.** Fizik adımı 1,25 ms; 400 Hz kontrolde periyot 2,5 ms = iki fizik adımı, 200 Hz yedekte 5 ms = dört fizik adımı (iskelet §4). Bu belgedeki süre eşikleri periyot sayısıyla değil süreyle verilir; iki hızda da aynı kalır.

**Tek hız tablosu.** Hız grupları ölçüm zamanına bağlanır; ağır gruplar faz kaydırılarak aynı adıma düşürülmez.

| İş | Hız | Dayanak |
|---|---|---|
| Çıkış durumu yayılımı, ufuk ilerletmesi, açısal hız ve öteleme hızı döngüleri, tahsis | Her adım (400 Hz) | iskelet §4; M5 |
| Kovaryans yayılımı | Biriken aralık 10 ms'ye ulaşınca ya da ufukta bir füzyondan hemen önce (2.3.2) | PX4 tahmin adımı 10 ms (D06 #3) |
| Ölçüm füzyonu (basınç ≤ 25, DVL 2–15, GNSS 5 Hz; manyetometre 10 Hz'e seyreltilmiş) | Ufuk ölçüm zamanına ulaştıkça | 05 §2.8; seyreltme tahmin |
| Tutum, derinlik, konum dış döngüleri | 100 Hz (tahmin) | 06 §3.2 ölçeklendi |
| Güdüm, görev yürütücüsü | 10 Hz (tahmin) | 06 §3.2 |
| Kip yöneticisi, izin, arıza değerlendirmesi | Her çağrı (step ve tick) | iskelet §3.2: izin her adımda güncel |
| Telemetri | Akış başına 1–10 Hz (tahmin) | 06 §3.2 |
| Duvar çağrısı (`tick`) | Kart ≥ 100 Hz; SIL her IMU'suz periyot | iskelet §4 |

**Adım içindeki sıra** (`step`):
1. `ingest`: örnek ve komutlar kabul denetiminden geçer (4.1); kabul edilmeyen ne kuyruğa ne tazeliğe girer.
2. `est`: ufuk ilerletilir ve bekleyen ölçümler füzyonlanır (2.3.2), çıkış durumu yeni IMU ile yayılır; sayısal çökme denetimi (2.3.6).
3. `fs`: arıza koşulları; sayısal çökmede eylem bu adımda devreye girer.
4. `mode`: bütün komutlar uygulanır. Silahlandırmayı kaldıran ve izni daraltan komutlar (`DISARM`, izin dışı kipe geçiş, `FC_ABORT`) ve arıza eylemi atış modülünden önce uygulanır (iskelet §3.2).
5. İzin hesaplanır (3.7); `fcif`: 03'ün hızlı kısmı bu adımın güncel izniyle çağrılır.
6. `mission`, `gd` (zamanı geldiyse), `ctl`, `alloc`; doyma kalıntısı integral güncellemesine bir sonraki adımda girer.
7. Çıkış kapısı: itici komutları sonlu ve sınır içinde mi denetlenir; atış olayı yalnız izin çıkış anında hâlâ geçerliyse yazılır, değilse atılır ve olay kaydı üretilir.

03'ün sonar verisinde algılama işi adım içinde koşmaz; platformun düşük öncelikte çağırdığı arka plan işidir (3.7).

**Hesap bütçesi (tahmin; M5 ölçer).** MAC eşdeğeri sayılarıdır, `-ffp-contract=off` altında çarpma ve toplama ayrı komuttur; MAC başına 3–6 çevrim varsayımı doğrulanmamıştır (D06 Y5).
- *Ortalama:* 100 Hz kovaryans yayılımı ≈ 675.000; skaler güncellemeler: sensör hızlarının üst değerleriyle basınç 25 + DVL 3×15 + manyetometre 10 + GNSS 4×5 = 100/s, iki kat payla 200/s × 900 ≈ 180.000; 100 Hz LDLᵀ denetimi ≈ 56.000; adım başına nominal entegrasyon, kontrol ve tahsis ≈ 340.000. Toplam ≈ 1,25 milyon/s → 160 MHz'te %2,3–4,7. Ortalama işlemci payı hedefi, 03'ün hızlı kısmı dahil ≤ %15 (hedef).
- *En kötü yığılma (tek adım):* kovaryans yayılımı 6.750 + 8 optimize skaler güncelleme 7.200 + vektör kapısı ve beyazlatma ≈ 1.000 + sıfırlama ≈ 540 + LDLᵀ ≈ 560 + nominal, kontrol, tahsis ≈ 850 ≈ 16.900 → 0,32–0,63 ms. Hedef ≤ 1,0 ms; kesme gecikmeleri ve USB servisi 01'in bütçesindedir. Genel yoğun Joseph formu adım başına 8 güncellemeyle 60.750 → 1,14–2,28 ms olur ve hedefi aşar; bu yüzden yedek ancak güncelleme sınırı adım başına 1'e indirilerek kullanılır (≈ 0,31–0,62 ms; 400 adım/s kapasite 200 skaler/s'yi karşılar).
- Ortalama yük en kötü durum yanıtını kanıtlamaz (08.12); karar M5'in DWT ölçümüyle. Statik bellek < 64 KB (tahmin; kartta ≈ 2,5 MB SRAM, 00 Bölüm 6).

### 2.3 Kestirim

#### 2.3.1 Filtre ve öngörü

Hata durumlu (çarpımsal) genişletilmiş Kalman filtresi, 15 hata durumu (06 §2.2; D06 N1). Nominal durum `p` (NED, gövde referans noktası `O_b`), `v`, `q` (gövde → NED), `b_g`, `b_a` (16 sayı); hata durumu `δp, δv, δθ, δb_g, δb_a` (15). Tutum hatası sağdan yereldir: `q = q̂ ⊗ Exp(δθ)` (Solà; D06 #24 MSS `ins_mekf` aynı boyut). İsteğe bağlı durumlar (basınç ofseti, akıntı, manyetometre sapması) ilk sürümde yoktur.

`R = R(q)`, `f = a_m − b_a`, `ω = ω_m − b_g`, `g_n = (0, 0, g)`:
- Nominal: `ṗ = v`, `v̇ = R f + g_n`, `q̇ = ½ q ⊗ (0, ω)`; IMU örneği aralık ortalaması olduğundan aralık içinde sabit alınır, kuaterniyon `q ← q ⊗ Exp(ω Δt)` ve normalleştirme.
- Hata dinamiği: `δṗ = δv`, `δv̇ = −R[f]× δθ − R δb_a − R n_a`, `δθ̇ = −[ω]× δθ − δb_g − n_g`, sapmalar rastgele yürüyüş (`δḃ = n_b`). Sürekli zaman Jacobian'ı `A`, ayrık geçiş `Φ = I + A Δt`, `Q_d = L Q_c Lᵀ Δt` (birinci mertebe; `Φ` ve `Q_d` aynı birikmiş `Δt` için kurulur). `P⁻ = Φ P⁺ Φᵀ + Q_d`.
- Geçerlilik: birinci mertebe yaklaşım `Δt ≤ 10 ms` ve `|ω| Δt ≤ 0,05 rad` (yani `|ω| ≤ 5 rad/s`, tahmin) için kullanılır; daha büyük açısal hızda yayılım bölünür.
- Dünya dönüşü ihmal edilir (düşük maliyetli MEMS ve kısa dalış varsayımı; 05'in IMU modeli içeriyorsa etkisi SIL'de ölçülür).
- Başlangıç hizalaması: durağan 1 s (tahmin) ivme ortalamasından roll/pitch, eğim telafili manyetometreden yön; `P₀` parametre.

#### 2.3.2 Zaman ufku ve gecikmeli füzyon

**Seçilen yöntem: sabit gecikmeli füzyon ufku + çıkış öngörücüsü** (PX4 EKF2 yöntemi: gecikmeli ufuk, FIFO tamponları, tamamlayıcı ileri yayılım; 06 §2.1). Geçmiş yeniliğe güncel kovaryansın kazancını uygulamak yanlış güven üretir (inceleme hesabı: `P₀ = Q = R = 1` için doğru `P⁺ = 1,5`, yanlış yöntem `2/3`); bu yüzden 0.1'deki "geçmiş durumla yenilik, şimdiki duruma düzeltme" kaldırıldı.

- **Ufuk.** Filtre (durum ve `P`) `t_h = t_imu − D` anında yaşar; `D = 150 ms` (parametre, tahmin: en büyük beklenen gecikme GNSS ≈ 120 ms, 05 §2.8 PX4 varsayılanı; DVL gecikmesi `r/c` + işleme + iletim, 05.3). IMU tamponu `(D + 50 ms)/periyot` = 80 kayıt (400 Hz).
- **İlerletme ve zaman eşlemesi.** Her adımda ufuk, tampondaki IMU örnekleriyle `t_imu − D`'ye kadar ilerler. Füzyon bekleyen bir ölçümün zamanı `t_m` bir IMU aralığının içine düşerse aralık `t_m`'de bölünür (örnek aralık ortalaması olduğundan iki parça aynı değerle entegre edilir). `t_m`'ye varınca birikmiş aralıkla kovaryans yayılır, ölçüm Jacobian'ı o anki ufuk durumunda kurulur ve füzyon yapılır: durum, Jacobian ve `P` aynı `t_m`'de buluşur. Kalan aralık füzyondan sonra entegre edilir ve yeni birikime başlar.
- **Ölçüm sırası.** Bekleyen ölçümler `t_m`'ye göre sıralıdır; eşit zamanlarda sabit sıra: basınç, DVL, manyetometre, GNSS (D06 E). Ufka göre geç kalan ölçüm (`t_m < t_h` iken gelen) füzyonlanmaz, "geç örnek" sayacı artar.
- **Güncelleme sınırı.** Adım başına en çok 8 skaler güncelleme (tahmin). Sınır dolarsa ufuk o adımda daha fazla ilerlemez; sonraki adımlarda birden çok IMU örneği tüketerek yetişir. Ufuk gecikmesi `D + 50 ms`'yi aşarsa en eski bekleyen ölçüm atılır ve sayılır; böylece tampon taşmaz ve davranış deterministiktir.
- **Çıkış öngörücüsü.** Kontrol, güdüm ve 03 ufuk durumunu değil şimdiki zamandaki çıkış durumunu `x_o` kullanır. `x_o` her yeni IMU ile aynı nominal denklemlerle ve ufkun sapma kestirimleriyle yayılır; geçmişi tamponda tutulur. Ufuk ilerledikçe hata `e = x_h ⊖ x_o(t_h)` (konum, hız ve `Log(q_o(t_h)⁻¹ ⊗ q_h)`) hesaplanır ve `x_o(şimdi)` ile tampondaki kayıtlara `k = T/τ_o` kazancıyla eklenir; `τ_o = 0,25 s` (tahmin).
- **Geçerlilik sınırları.** (a) Ölçüm gecikmesi `D`'yi aşmamalıdır; geç örnek oranı sağlık metriğidir. (b) `τ_o ≥ 10 T`; düzeltme `τ_o` ile yumuşar, kontrol sıçrama görmez. (c) Sağlık metrikleri `D` kadar gecikir; 4. başlıktaki 2 s pencereler buna göre seçildi. (d) Konum sıfırlaması (2.3.4) yumuşatılmaz, `x_o` ve geçmişe aynı adımda sıçrama olarak uygulanır.

#### 2.3.3 Ölçüm modelleri

Ham değerler sensör çerçevesindedir; montaj dönüşümlerini çekirdek parametrelerden uygular (3.3).

| Sensör | Model `h(x)` | Kapı ve koşul |
|---|---|---|
| Basınç | `d = (P − P_atm)/(ρ g)`; `h = p_z + (R l_p)_z`; `H = [e₃ᵀ, 0, −e₃ᵀ R [l_p]×, 0, 0]` | `P_atm` yüzeyde açılışta kalibre; `ρ` parametre. Yüzeye yakın dalga basıncı için `R` derinliğe göre büyütülür (tahmin; sönüm `e^{-kd}`, 04.10) |
| DVL | `h = R_dvlᵀ (Rᵀ v + ω × l_dvl)`; kollar `O_b`'den | Dip takibi, `vel_valid`, FOM ve en az irtifa; yalnız fiziksel rejim `DALIŞ` iken (06 §2.2 md.4) |
| Manyetometre | Eğim telafili yön; yenilik `(−π, π]` aralığına sarılır | Alan normu beklenenin ±%20'si (tahmin); bozulma riskinden ötürü üç eksen yerine yön (06 §4 md.5) |
| GNSS | Anten konumu `p + R l_a`, hızı `v + R(ω × l_a)` (yatay 2 + 2) | Yalnız GNSS kullanılabilirken (2.3.5); 3B fix, `eph` eşik altı |
| Ping1D | Filtreye girmez; irtifa güdümde | Dip düz değildir |

`R` kaynakları: sensör kovaryans verirse o (DVL `cov`), vermezse parametre; `eph` ve FOM bir ölçekle `σ`'ya çevrilir. Verilen kovaryans sonlu ve pozitif tanımlı değilse parametre `R` kullanılır ve bayrak kalkar. Ayrıntılar 8.1'de.

#### 2.3.4 Kapı, güncelleme ve sıfırlama

- **Kapı:** vektör ölçüm bütün olarak `yᵀ S⁻¹ y < γ_m`, `S = H P Hᵀ + R` (D06 E); `γ_m` ölçüm boyutuna göre ki-kare eşiğidir. Kısmi kabul yoktur.
- **Beyazlatma:** `R = L Lᵀ`, `y' = L⁻¹ y`, `H' = L⁻¹ H` (D06 E: ilişkili gürültüde eksenleri bağımsız saymanın gerekçesi).
- **Sıralı skaler güncelleme:** her skaler için `u = P hᵀ`, `s = h u + r`, `k = u/s`, `P ← P − k uᵀ − u kᵀ + s k kᵀ` (Joseph formunun açılmış hali, karesel maliyet); yenilik önceki skalerlerin düzeltmesini içerir: `y_j ← y_j − h_j δx`, `δx ← δx + k (y_j − h_j δx)`. Simetrikleştirme ve varyans alt sınırı ardından.
- **Enjeksiyon ve sıfırlama:** `δx` nominal duruma eklenir (`q ← q ⊗ Exp(δθ)`); `G = blokköşegen(I, I, I − ½[δθ]×, I, I)` **sıfırlamadan önceki** `δθ` ile kurulur, `P ← G P Gᵀ`, sonra `δx = 0` (Solà; D06 E).
- **Yüzeye çıkışta konum düzeltmesi.** Fix, 5 ardışık iyi örnekten sonra kabul edilir (5 Hz'te ilk ile son örnek arası 0,8 s; tahmin); fix'in belli sürede geleceği varsayılmaz (05.5). İlk fix kapıdan geçerse normal güncelleme. Geçmez ama kalite M = 10 ardışık örnek (tahmin) boyunca iyiyse sıfırlama: anten konumu kolla `O_b`'ye taşınır (`p = p_a − R l_a`), `p_NE` ona eşitlenir; GNSS hızı da kapıyı geçemiyorsa `v_NE` aynı biçimde sıfırlanır; sıfırlanan blokların çapraz kovaryansı sıfırlanır, köşegen `R` + kol belirsizliğiyle kurulur; `reset_counter` artar; biriken ölü hesap hatası olay olarak kaydedilir; güdüm yolu yeni konumdan kurulur (06 §2.2 md.3).
- **GNSS farkı:** `lat_e7`, `lon_e7` farkı `int64` alınır, boylam farkı ±180°'ye sarılır, sonra `float` ile ölçeklenir.

#### 2.3.5 Fiziksel rejim, sensör kullanılabilirliği ve güdüm aşaması

Üçü ayrı durumdur; biri diğerinin yerine kullanılmaz.

| Durum | Değerler | Belirleyen | Kullanan |
|---|---|---|---|
| Fiziksel rejim | `YÜZEY`, `GEÇİŞ`, `DALIŞ` | Yalnız basınçtan derinlik, histerezis: `DALIŞ` girişi `d > 0,5 m`, `YÜZEY` girişi `d < 0,2 m`, arası `GEÇİŞ` (06 §2.2, tahmin; anten yüksekliğine göre parametre) | DVL kapısı, `SURFACE` eyleminin bitişi, arıza anlamları |
| Sensör kullanılabilirliği | GNSS, DVL kilidi, manyetometre, basınç | Fix kalitesi ve rejim (GNSS yalnız `YÜZEY`; derinlikle sabit bir kesme varsayılmaz, 05.4), bayraklar, tazelik | Kestirim, sağlık |
| Güdüm aşaması | `SURFACE_TRANSIT`, `DIVE`, `SUBMERGED`, `ASCENT` | Görev ve kip | Kontrol rejimi ve dikey yetki |

Yüzeyde GNSS kaybı dalış değildir: rejim `YÜZEY` kalır, kestirim ölü hesapla sürer ve sağlık düzeyi düşer (4. başlık). Dalış ve yükselme GNSS beklemez.

#### 2.3.6 Sağlık ve sayısal çökme

- `EST_OK`; `EST_DEGRADED`: yatay konum σ > σ_max (parametre) ya da GNSS ve DVL kilidi yok; `EST_BAD`: hız ya da tutum kapı oranı (`NIS/γ`) sürekli > 1, tutum σ eşik üstü. Bunların süre koşulları duvar saatindedir (4.3).
- **Sayısal çökme ayrıdır ve beklemesizdir:** ufuk ya da çıkış durumunda NaN/Inf, kovaryansın 100 Hz LDLᵀ ayrıştırmasında pivot ≤ ε (pozitif köşegen pozitif tanımlılığı kanıtlamaz), ya da sonlu olmayan kontrol çıktısı. Aynı çağrıda: atış izni kalkar, kontrol yolu atlanır, sekiz itici nötr (`PASSIVE_SURFACE`), `EST_FAULT` olayı. Filtre yeniden hizalanır (2.3.1); arıza mandallı kalır, `FAILSAFE_RESET` ve 2 s `EST_OK` ile kalkar.

#### 2.3.7 Tek duyarlık: gerekçe ve sınırlar

Kartın FPU'su yalnız tek duyarlıklıdır (D06 #8, D09 #10); `float` yoğun filtre aritmetiğinde performans tercihidir, zorunluluk değildir (06.6). 10 km'de `float` aralığı `2^(13−23)` m = 0,977 mm (06.7); temsil çözünürlüğü doğruluk değildir. Coğrafi orijin `int32` 1e-7 derecedir; çekirdekte `double` gerekmez (düz dünya yaklaşımı birkaç km için; sınır SIL'de ölçülür). Asıl sınırlar birikimdir: (a) mutlak zaman `float` saniye tutulamaz (1 saatte aralık 0,244 ms, periyodun onda biri); zaman `int64` ns, yalnız farklar `float`; (b) `float` kovaryans pozitif tanımlılığını yitirebilir: Joseph formu, simetrikleştirme, alt sınır ve LDLᵀ denetimi bu yüzden; (c) geniş dinamik aralık: aynı filtre yalnız test için `double` derlenip karşılaştırılır (Bölüm 7).

### 2.4 Güdüm

- **Rota noktası ve yol takibi:** LOS; akıntıya karşı kalıcı çapraz hatayı sıfırlamak için ILOS (Caharija vd. 2016; Fossen ve Pettersen 2014; 06 §2.3). Yön rotaya hizalanır; yanal hata ILOS ile yön düzeltmesine çevrilir.
- **Derinlik ve irtifa:** dikey kanal ayrıdır; derinlik referansı hız ve ivme sınırlı referans modelden geçer (`refModel3`, 06 §2.3). İrtifa tutma `d_ref = d + (a − a_ref)` (filtreli); irtifa Ping1D'nin eğim düzeltmeli değeri, yedeği DVL irtifası.
- **İstasyon tutma:** NED konum hatası → gövde hız setpoint'i; yön ayrıca tutulur.
- **Sonar taraması:** Ping360'ın tam turu menzil, örnek sayısı, açı adımı ve bağlantı hızıyla değişir, 50 m'de 33 s'ye çıkabilir (05.2). (a) Çizgi taraması: hat aralığı = kapsama × (1 − örtüşme), hız sektör tarama süresiyle sınırlı, dönüşler hazır yaylarla (06 §2.3); (b) noktada tur: ızgara noktasında istasyon tutma ve tam tur. Desen sabit bellekle bacak bacak üretilir; `scan_leg_active` yalnız düz, sabit hız ve derinlikli bacaklarda doğrudur ve 03'e `ida_fc_in_t` ile, telemetriye kayıtla gider.
- **Güdüm aşamaları:** `SURFACE_TRANSIT` (yüzey kontrol rejimi, dikey kapalı); `DIVE` (dalış rejimi aşamanın başında açılır: dikey yetki tam, derinlik referansı hız sınırlı rampayla; konum ve yön tutulur; fiziksel rejim `DALIŞ` olunca `SUBMERGED`); `SUBMERGED`; `ASCENT` (dalış rejimi, yükselme hızı `v_asc = 0,25 m/s` (tahmin); fiziksel rejim `YÜZEY` olunca dikey kapanır, `SURFACE_TRANSIT`). Kontrol rejimi fiziksel rejime değil aşamaya bağlıdır; böylece "dalmak için dalış kontrolü, dalış kontrolü için derinlik" döngüsü oluşmaz.

### 2.5 Kontrol ve itki tahsisi

**Kademeli yapı** (06 §2.4, 06 §2.1):

| Kanal | Dış döngü, 100 Hz (tahmin) | İç döngü, 400 Hz | Çıkış |
|---|---|---|---|
| Tutum | Kuaterniyon hatası → açısal hız setpoint'i (P, sınırlı) | Açısal hız PID | `K, M, N` |
| Derinlik | `d` hatası → NED dikey hız setpoint'i (P, sınırlı) | Dikey hız PI + yüzerlik ileri beslemesi | NED `F_z` |
| Konum | NED hata → yatay hız setpoint'i (yön çerçevesinde) | Yatay hız PI + sönüm ileri beslemesi | Yön çerçevesinde `F_x, F_y` |

Öteleme kuvvetleri yön (yalnız yaw ile döndürülmüş NED) çerçevesinde hesaplanır ve gövdeye `[X Y Z]ᵀ = Rᵀ R_ψ [F_x F_y F_z]ᵀ` ile çevrilir. Yüzerlik ileri beslemesi NED aşağı yönde `+(B − W)` (araç pozitif yüzer); `B − W` üç kütle durumu için ayrı parametredir (tatlı suda iki yüklü +1,82, bir yüklü +1,89, boş +1,96 N; 04 §2.6) ve kütle durumu çekirdeğin kabul ettiği her atış olayında bir alt duruma geçer.

**İntegral yönetimi.** Tahsis, eksen başına kalıntıyı `r = τ_req − τ_a` döndürür; `|r_j| > ε_j` ise eksen o işaretle doymuş sayılır ve ilgili integral yalnız doymayı artıran yönde dondurulur, ters yönde çözülebilir (PX4, D06 #10). Kalıntı gövde eksenindedir; yön çerçevesindeki öteleme integralleri için aynı dönüşümün devriğiyle geri taşınır. Kazanç değişiminde çıktı sürekliliği `I_yeni = I_eski + (K_p,eski − K_p,yeni) e` ile korunur; yeni etkinleşen kanalın integrali, o eksende elde edilen çıktıdan P terimi çıkarılarak başlatılır (06 §2.4).

**Rejime göre kontrolcü.** Yüzey rejimi: denetlenen eksenler `X, Y, N`; `Z, K, M` hidrostatiğe bırakılır, dikey iticiler kapalıdır (havalanma, 06 §2.4; 04 havalanmayı modeller). Dalış rejimi altı eksenlidir. Rejim güdüm aşamasıyla değişir (2.4); kazançlar 1 s (tahmin) boyunca karıştırılır.

**Tahsis matrisi.** `B` (6×8), 04 §2.6'nın A5 kaynağından kurulur: sütun `i` = `[ε_i; r_i × ε_i]`, referans `O_b` (yük durumundan bağımsız), dikey iticilerde pozitif komut `+z` (aşağı) itkisi; sıra kartın gördüğü itici sırasıdır (04 §2.6). Yatay iticilerin `z = 0,085 m` ofseti `K = −0,085 Y`, `M = 0,085 X` çapraz momentlerini üretir; dalış rejiminde dikey grup bunları dengeler. Tam `B`'nin rankı 6'dır (inceleme hesabı; 04 denetledi).

**Tahsis yöntemi.**
1. Başlatmada ve geometri parametresi değişince (yalnız devre dışıyken) her rejim için ağırlıklı en küçük norm tersi `B⁺ = W⁻¹ Bᵀ (B W⁻¹ Bᵀ)⁻¹` hesaplanır; `W = diag(1/F_maks,i²)`. Yüzey rejiminde yalnız `X, Y, N` satırları ve yatay iticiler kullanılır. Koşul sayısı `κ(B W⁻¹ Bᵀ) > κ_maks` ise parametre reddedilir.
2. Her adımda öncelik grupları sırasıyla `[K, M] > [Z] > [N] > [X, Y]` (dalış; yüzeyde `[N] > [X, Y]`): `u ← u + α_g B⁺ τ_g`; `α_g ∈ [0, 1]`, her itici sınırından doğrusal olarak hesaplanan en büyük uygun değer. Yineleme sayısı grup sayısıyla sınırlıdır (en çok 4); sonuç her zaman sınır içindedir (ArduSub grup normalizasyonu ve PX4 sıralı doyma giderme örnek alındı, 06 §2.1, §2.4).
3. Sınırlar: `F_maks,ileri/geri(V)` 04 ile aynı gerilim yasasıyla, filtrelenmiş batarya geriliminden (04 §2.7.2).
4. Elde edilen `τ_a = B u` ve kalıntı integral yönetimine döner.

**İtki eğrisinin tersi.** Çekirdek 04'ün A5'teki geçici modelini aynı sürümle kullanır: `F = F_şekil(u) · F_maks,yön(V)` (04 §2.7.2). `F_şekil`'in monoton kırpılmış biçiminden başlatmada 33 noktalı tablo kurulur; ters, ikili arama ve doğrusal ara değerlemeyle alınır. Daha önceki `√|T|` tersi kaldırıldı: `T ∝ n|n|` devir hızına aittir, PWM'e değil. ESC dur ölü bandı 1500 ± 25 µs sınır dahil sıfır itkidir (04 §2.7.1; 11.6); PWM 1 µs nicemlidir, bu yüzden sıfırdan farklı en küçük komut `|PWM − 1500| = 26 µs`, yani `|u| = 0,065`. İstenen itki `F_şekil(0,065)` değerinin yarısından küçükse komut 0, değilse en az 0,065. Platform `PWM = 1500 + round(400 u)` µs ile çevirir. Eğri sürümü 04'ünkinden farklıysa bu ancak bilinçli model hatası olarak `model_farkı` bayrağıyla kabul edilir.

**Pasif yükselme hızı.** 04 §2.6 değerleriyle tam batık, durgun tatlı suda `33 v + 190 v² = B − W` çözümü: boş 0,047, iki yüklü 0,044 m/s (doğrusal sönüm bu hızda baskın; yalnız karesel terim 0,10 m/s verir ve yanlıştır). Yüzeye yaklaşma, yönelim ve dalga bu denge hesabının dışındadır. 10 m'den ≈ 3,5 dakika (tahmin). Bu yüzden `PASSIVE_SURFACE` yalnız kontrolün güvenilmez olduğu durumlar içindir.

### 2.6 Kipler ve görev yönetimi

| Kip | Ön koşul | Davranış |
|---|---|---|
| `INIT` | — | Parametre doğrulama, kestirici hizalama |
| `DISARMED` | — | Motorlar nötr; arm, ön koşullar ve parametre geçerliliğiyle |
| `MANUAL` | Arm | Operatör altı eksen girdisi doğrudan `τ_req` |
| `STABILIZE` | Tutum geçerli | Tutum tutulur, öteleme operatörden |
| `DEPTH_HOLD` | Derinlik geçerli | Derinlik ve yön tutulur |
| `STATION_KEEP` | `EST_OK` | Konum, derinlik, yön |
| `GUIDED` | `EST_OK` | Yer istasyonu setpoint'i; 3 s yeni hedef yoksa son hedefte kalır (06 §2.1) |
| `AUTO` | `EST_OK`, geçerli görev | Görev yürütücüsü |

Arıza eylemleri (`HOLD`, `SURFACE`, `PASSIVE_SURFACE`, `DISARM`; tanımları 4.2) kipin üstünde çalışır. Öncelik: arıza > operatör > görev. Ön koşulu düşen kip bir alt kipe iner (`AUTO`/`STATION_KEEP` → `DEPTH_HOLD`, 06 §2.5). Durum makinesi tam geçiş tablosudur; her durum × olay çifti tanımlıdır ve birim testle doğrulanır.

**Görev durum makinesi:** `IDLE → LOADED → RUNNING(öğe, aşama) ⇄ PAUSED → COMPLETE | ABORTED`. Her öğenin başlangıcı, setpoint üretimi, bitiş ölçütü ve zaman aşımı vardır; zaman aşımı görevi durdurur ve `SURFACE` ister. Davranış süreleri (bekleme, tur) ölçüm zamanında; zaman aşımları ve toplam görev süresi duvar saatindedir (iskelet §4). Yürütücü yalnız setpoint üretir (06 §2.5).

**Öğeler** (sabit dizi, `IDA_MISSION_MAX = 128`, tahmin): `WAYPOINT`, `DIVE_TO`, `SURFACE`, `HOLD`, `SCAN_LINES`, `SCAN_POINT`, `WAIT_GNSS`, `TARGET_ALIGN`.

**Görevin verilişi:** `MISSION_BEGIN(n)`, `MISSION_ITEM(i, öğe)`, `MISSION_END` komutları; platform MAVLink görev protokolünden çözer (06; özel öğeler `MAV_CMD_USER_*`, 06 §2.5). SIL'de senaryodaki görev (A5) sim sunucusunun yer istasyonu yolundan aynı komutlarla gelir. Yükleme atomiktir; çalışan görev yalnız `PAUSED` ya da `HOLD` iken değişir.

### 2.7 Sayı tipleri ve derleme politikası

- **Standart:** C17, `-std=c17 -pedantic-errors`, MSVC `/std:c17`; VLA, `complex`, `<stdatomic.h>`, `<threads.h>`, GNU uzantıları yok (08 §2.6; Ç10; A17 açık).
- **Tipler:** sabit genişlikli tamsayılar; kayan nokta yalnız `float`, çekirdekte `double` yok; sabitler `f` sonekli; aşkın işlevler `ida_math` sarmalayıcısından (Ç7: tek başına platform eşitliği vermez).
- **FP politikası:** GCC/Clang `-ffp-contract=off -fno-fast-math` (Apple Clang varsayılanı `on`, açıkça kapatılır; 09.6); MSVC `/fp:precise`; clang-cl MSVC ile özdeş sayılmaz. FMA'yı kapatmak bit eşitliği sağlamaz (08.14, 09.5). Kartın FPSCR ayarı 01'de sabitlenip kaydedilir; subnormal davranışı farklı olabilir (09.5).
- **SIL ile kart aynı politika:** çekirdek tek CMake hedefinden, tek FP politikası arayüz hedefiyle derlenir; SIL kartın tiplerini ve üretim politikasını korur, fizik `double` kalır (Ö3, 09.5).
- **Matematik arka ucu:** `mat.h` taşınabilir C ile uygulanır. CMSIS-DSP hızlandırması seçilirse çekirdeğin içine değil, aynı `mat.h` arayüzünü uygulayan ve kart platformunda bağlanan ayrı arka uca girer (CMSIS başlıkları çekirdeğin içerme yolunda yasak kalır); SIL taşınabilir arka ucu kullanır ve aradaki fark M10'da ölçülür.
- **Bellek ayırma yasağının denetimi:** (1) GNU ld ile bağlanan yapılandırmada (kart, arm-none-eabi) çekirdek `--wrap=malloc,calloc,realloc,free` ile sarmalayıcısız bağlanır; başvuru bağlantı hatası verir (08.13). (2) Her platformda çekirdek nesne dosyalarının sembolleri (`nm`; MSVC'de `dumpbin /symbols`) `malloc`, `calloc`, `realloc`, `free`, `_malloc_r` ve `printf` ailesi için taranır; macOS ld64 ve MSVC'de denetim budur. (3) Özel havuz ayırıcı yasak. (4) Özyineleme yasak; `-fstack-usage`, `-Wstack-usage`. (5) Yapı boyutları `_Static_assert` ile.
- **Statik analiz:** `-Wall -Wextra -Wconversion -Wdouble-promotion -Wfloat-conversion -Wshadow -Wvla`, `-fanalyzer`, clang-tidy, cppcheck, yazılı proje kural alt kümesi; "MISRA uyumlu" iddia edilmez (08 §2.6). Çekirdekte koşullu derleme (`#ifdef SIL` vb.) yoktur ve otomatik taranır.

### 2.8 SIL koşucusu

Çekirdeği PC'de kartın yerine çalıştıran ince katman (iskelet §2: C17 + C++); A1 ↔ A4 dönüştürücüsünün PC tarafı da onun parçasıdır, alan eşlemesini 06 tanımlar (iskelet §3).

- **Bağlantı:** sim sunucusuna A1 ile yerel soketten bağlanır; el sıkışma ve mesajlar kartınkiyle aynı. Sim için fark yalnız taşımadır.
- **Adımlama:** SIL'de bariyer hep açıktır. Sim her kontrol periyodunda zaman çerçevesi gönderir (iskelet §4). Koşucu periyodun bütün çerçevelerini toplar; IMU varsa `ida_core_step`, yoksa `ida_core_tick` çağırır ve periyodun sıra numarasıyla tek aktüatör yanıtı döner. IMU kesilse de zaman ilerler, arıza çıktısı alınır, bariyer kilitlenmez.
- **Duvar saati:** zaman çerçevesinin sim zamanından türetilir (iskelet §4); kullanıcı duraklatması bundan ayrıdır: duraklatılmış simde çerçeve gelmez, sanal duvar saati de durur, arıza üretilmez. Gerçek duvar saati seçeneği gerçek zaman davranışını sınamak içindir.
- **Kartla ortak:** çekirdek kaynağı ve derleme politikası, A1 kodlayıcısı (`protocol/`). Dönüştürücü mantığı tek C modülü olarak paylaşılabilir (öneri; sahiplik iskelet §3'te 01 ve 02).
- **Ortak olmayan:** taşıma, zamanlama (kesme ↔ engelleyen döngü), gözetim zamanlayıcısı, parametre deposu (flash ↔ A5; dönüştürücü C++), DWT. Süre aşımı, adım kaçırma ve IMU kesilmesi koşucuda enjekte edilebilir.
- Küresel durum olmadığından bir süreçte birden çok çekirdek koşabilir (Monte Carlo; 08 §2.6).

## 3. Arayüzler

### 3.1 A4 çağrıları

```c
void        ida_core_init(ida_core_t *c, const ida_params_t *p, uint32_t boot_id, ida_wtime_t now_w);
ida_step_rc ida_core_step(ida_core_t *c, const ida_step_in_t *in, ida_out_t *out);
void        ida_core_tick(ida_core_t *c, const ida_tick_in_t *in, ida_out_t *out);
ida_rc      ida_core_nav_at(const ida_core_t *c, ida_stime_t t, ida_nav_t *nav);
```

- `ida_core_t` kamuya açık tam tiptir (`ida_core_types.h`); platform `static ida_core_t` ayırır, boyut ve hizalama `_Static_assert` ile sabitlenir.
- `ida_stime_t` (ölçüm zamanı) ve `ida_wtime_t` (duvar saati) `int64_t` ns taşıyan ayrı yapılardır; derleyici karıştırmayı reddeder; iki alan hiçbir yerde birbirinden çıkarılmaz.
- `ida_step_rc`: `OK`; `DUP_SEQ`, `OLD_SEQ`, `BAD_INPUT` (adım çalışmaz; çıktı 3.4'teki yeniden gönderim kuralına göre üretilir; tazelik yenilenmez).
- `ida_step_in_t`: `imu`, `n_samples` + `samples[16]` (etiketli birlik, tahmin), `sonar` (isteğe bağlı Ping360 dilimi, `const` işaretçi; çekirdek kopyalar, 3.8), `n_cmds` + `cmds[8]`, `time_frame` (varsa `t`, `seq`), `bg_result` (03 arka plan sonucunun tüketim jetonu, 3.7), `now_w`, `plat` (önceki çağrının süre aşımı, kuyruk taşması sayaçları, bağlantı servis durumu).
- `ida_tick_in_t`: `now_w`, `time_frame` (varsa), `n_samples` + `samples[16]` (IMU dışı), `n_cmds` + `cmds[8]`, `plat`. Çalıştırdıkları: `ingest`, tazelik, `fs`, `mode` (komutlar dahil), izin. Kestirim, güdüm, kontrol ve 03 çalışmaz. IMU tazeyse itici çıktısı üretmez (`thr_valid = false`, sürekli komut adımın elindedir); IMU bayatsa, sayısal çökme ya da `DISARM` varsa nötr güvenli çıktı üretir (`thr_valid = true`, `src = TICK`).

### 3.2 Simülasyondan habersizlik

1. A4'te sim kipi, sim kaynağı ya da gerçeklik bilgisi taşıyan hiçbir alan ve parametre yoktur; koşullu derleme yasaktır (2.7).
2. Örnekler gerçek sürücünün üreteceği anlamdadır: derinlik değil basınç; DVL geçerlilik bayrakları ve FOM; GNSS fix türü ve `eph` (06 §2.8).
3. Ölçüm zamanı, ölçümün geçerli olduğu andır ve ortak zaman alanındadır; HIL'de sim zamanı, gerçek donanımda sürücünün sensör saatini bu alana eşlediği değer (eşleme 01 ve 05'in işi). Alım zamanı ayrı bir alandır (iskelet §3.1).
4. Çıktılar sürücü istekleridir: normalleştirilmiş itici komutu ve atış olayı.
5. Yer gerçeği çekirdeğe girmez; değerlendirme A3/A6 ile sim tarafında.
6. Algılama kaydı, algılamanın nerede yapıldığından bağımsız aynı biçimdedir (iskelet §3).

### 3.3 Sensör örnekleri

Ortak başlık `ida_hdr_t`: `t_meas` (`ida_stime_t`, geçerlilik anı), `t_rx` (`ida_wtime_t`, alım anı; platform damgalar), `seq` (`uint32_t`), `inst` (`uint8_t`), `flags` (`uint16_t`: geçerli, doymuş, sensör arızası, isteğe bağlı alanların varlık bitleri). Değerler `float`, SI, sensör çerçevesinde; montaj dönüşümleri çekirdek parametrelerindedir.

| Yapı | Alanlar ve zaman anlamı |
|---|---|
| `ida_imu_t` | `gyro[3]`, `accel[3]` (özgül kuvvet), `interval_ns`; değerler `[t_meas − interval, t_meas]` aralığının ortalaması (05 §2.8 md.3), `temp_c` |
| `ida_mag_t` | `field[3]` T |
| `ida_baro_t` | `p_abs` Pa, `temp_c`; `t_meas` dönüşüm ortası |
| `ida_gnss_t` | `lat_e7`, `lon_e7`, `alt_mm` (`int32_t`), `vel_ned[3]`, `eph`, `epv`, `fix_type`, `n_sv` |
| `ida_dvl_t` | `t_meas` = `time_of_validity` (yansıma anı, 05.3), `vel[3]`, `fom`, `cov[9]` (varlık biti ile), `vel_valid`, `beam_valid`, `beam_dist[4]` (çözülemeyen −1, 00 Bölüm 6), `altitude`, `track` |
| `ida_ping1d_t` | `distance`, `confidence`, `ping_number` |
| `ida_ping360_t` | `angle_grad` (0–399), `n_samples` ≤ 1200, `sample_period` (25 ns), `tx_duration`, `tx_freq_khz`, `gain`, `data[1200]` (05 §2.1); donanımda yakalama damgası yok (00 Bölüm 6), `t_meas` platformun ping zamanı |
| `ida_batt_t` | `voltage`, `current`, `temp_c` |
| `ida_leak_t` | `mask` |
| `ida_detection_t` | 07 §3.2 "Detection v1" alan listesi (00 Bölüm 3 geçerli sayar), yakalama zamanı `ida_stime_t`; kesin alanlar 03'ün |

Yüksek hızlı IMU'nun kontrol hızına indirgenmesi 05 ve 06'nın işidir; çekirdek çağrı başına bir IMU örneği alır.

### 3.4 Çıkışlar: sürekli komut ve tek seferlik olay

`ida_out_t` (iskelet §3.2):
- **Sürekli komut:** `thr_valid`, `src` (`STEP`/`TICK`), `seq` (IMU ya da zaman çerçevesi sıra numarası), `t_meas`, `thr_cmd[8]` (`float`, [−1, 1]), `armed`, `valid_for_ns` (kullanım süresi; 04'ün `komut_ömrü` ile aynı parametre, varsayılan 50 ms).
- **Tek seferlik olaylar:** `n_events` + `events[4]`; her olay `event_id` (`boot_id` << 32 | sayaç), tür (`FIRE` tüp, `ARM_CHANGE`), değer. `FIRE` olayı yalnız 3.7'deki kapıdan geçerse yazılır.
- `n_telem` + `telem[8]`, `param_save`, `alive_ok`.
- **Yeniden gönderim:** platform önbellekteki çıktıyı ham olarak yeniden göndermez. `DUP_SEQ`/`OLD_SEQ` çağrısında çekirdek yalnız sürekli komutu yeniden üretir ve onu güncel güvenlik durumuyla kapılar: arm yoksa, arıza eylemi `PASSIVE_SURFACE` ya da üstüyse ya da bir duvar çağrısı güvenli çıktı vermişse nötr. Olay hiçbir zaman yeniden üretilmez. Alıcı (sim ya da gerçek sürücü) aynı `event_id`'yi bir kez uygular, kullanım süresi geçmiş sürekli komutu uygulamaz (06'dan ve 04'ten beklenen).

### 3.5 Bağlantı

Çekirdek bayt ya da MAVLink görmez; A1 biçimi (M9) değişse çekirdek değişmez.
- **Gelen `ida_cmd_t`** (`cmd_id` ile; yanıt aynı kimlikle): `HEARTBEAT`, `ARM`/`DISARM`, `SET_MODE`, `MANUAL_INPUT`, `GUIDED_SETPOINT`, `MISSION_*`, `PARAM_SET`/`PARAM_COMMIT`/`PARAM_GET`, `FAILSAFE_RESET`, `FC_CMD` (03'e), `FC_ABORT`.
- **Giden `ida_telem_t`:** nav, sağlık, kip ve arıza, itici ve kalıntı, görev ilerlemesi, `scan_leg_active`, hizalanma durumu, parametre değeri, komut yanıtı, olay metni.
- QGC'de hibrit kiplerin ve görev yüklemenin çalışması ayrı kabul ölçütüdür (06.12, 09.15, M18).

### 3.6 Kalıcı parametreler

- **Biçim:** açık kodlanmış (küçük uçlu, sabit genişlikli, sürümlü) kayıt; CRC32 bu kodlanmış biçim üzerindedir, ham `struct` üzerinde değil (08.11). Statik tanım tablosu: kimlik, ad (≤ 16 karakter, MAVLink sınırı), tip, en küçük, en büyük, varsayılan, sınıf.
- **Sınıflar:** (A) çalışırken değişebilir: kazançlar, eşikler, süreler. (B) yalnız devre dışıyken: geometri (`B`), itki eğrisi, montajlar, gürültü yoğunlukları, kütle durumu tabloları; arm'lıyken reddedilir.
- **İşlem:** `PARAM_SET` değeri bekletir; `PARAM_COMMIT` ilişkili grubu birlikte doğrular (aralık; `B` rankı ve koşul sayısı; `Q`, `R` köşegenlerinin pozitifliği; eğri monotonluğu), türetilmiş değerleri (`B⁺`, eğri tablosu) yeniden kurar ve bir adım sınırında atomik uygular. Geçersiz grup tamamen reddedilir.
- **Açılış:** platform bloğu yükler (kart: flash, 01; SIL: A5). CRC ya da sürüm hatasında derlenmiş varsayılanlar yüklenir ve arm engellenir; engel ancak geçerli bir parametre kümesi `PARAM_COMMIT` ile uygulanıp `param_save` ile kalıcılaştırılınca kalkar.

### 3.7 Atış kontrol modülüne sağlanan arayüz (içi 03'te)

- **`ida_nav_t`:** çıkış durumu (`t`, `p_ned`, `v_ned`, `q_nb`, `omega_b`, `depth`, `altitude`, σ'lar, `reset_counter`, `est_level`).
- **Geçmiş:** `ida_core_nav_at(t)` çıkış geçmişinden son 2 s'yi (100 Hz, 200 kayıt; tahmin) ara değerler (07.7: sensör ölçümünden dünya konumu poz gerektirir). Sorgu bir sıfırlama sınırına düşerse ara değerleme yapmaz, `IDA_RESET_BOUNDARY` ile sınırın doğru tarafındaki kaydı döner; ufuk dışı sorgu `IDA_OUT_OF_RANGE` döner.
- **İzin `ida_fc_permit_t`:** `armed`, `mode_ok` (`AUTO`'da `TARGET_ALIGN` ya da `GUIDED`), `est_ok`, `failsafe_clear`. Adımın 4. sırasından sonra hesaplanır (2.2); 03 hep güncel izni görür.
- **Çıkış kapısı:** 03'ün atış isteği (03'ün verdiği benzersiz `shot_id` ile) çekirdekte izin çıkış anında da geçerliyse `FIRE` olayına dönüşür; değilse atılır ve kayda geçer. Kabul edilen her `FIRE` olayında kütle durumu güncellenir (2.5).
- **Hizalanma:** 03 `ida_align_req_t` (`valid`, `yaw_ref`, `depth_ref`, `hold_ne[2]`, `max_speed`, `tol_yaw`, `tol_pos`, `dwell`) verir; kip yöneticisi izin varken uygular; yanıt `ida_align_status_t` (`accepted`, `aligned`, hatalar) bir sonraki adımda `ida_fc_in_t` ile döner.
- **03'ün imzaları:** `void ida_fc_step(ida_fc_t *fc, const ida_fc_in_t *in, ida_fc_out_t *out)` ve `void ida_fc_background(const ida_fc_bg_in_t *in, ida_fc_bg_out_t *out)`. Arka plan işi `ida_core_t`'ye ve `ida_fc_t`'ye dokunmaz: girdisi çekirdeğin kopyaladığı değişmez sonar dilimi ve poz özeti, çıktısı platformun sahip olduğu sonuç yapısıdır. Sonuç çekirdeğe yalnız bir adımın `bg_result` jetonuyla girer; tüketim adımı böylece kayda geçer.

### 3.8 Kapasiteler ve sahiplik (tahmin)

| Tampon | Kapasite | Taşma |
|---|---|---|
| Çağrı başına örnek / komut | 16 / 8 | Fazlası atılır, sayılır; platform sonraki çağrıya taşıyabilir |
| Bekleyen ölçüm (sensör başına) | Basınç 8, DVL 4, manyetometre 4, GNSS 4 | Aynı sensörün en eskisi atılır, sayılır |
| IMU ve çıkış geçmişi | 80 kayıt | Yapı gereği taşmaz (2.3.2) |
| Seyrüsefer geçmişi | 200 kayıt | Halka |
| Sonar dilimi | 2 yuva; çekirdek kopyalar | Arka plan bitmeden yeni dilim gelirse eski yuva bırakılır, sayılır |
| Görev | 128 öğe | Yükleme reddedilir |

### 3.9 Tekrar üretim için kayıt

Aynı ikilide bit düzeyinde tekrar için kaydedilenler (iskelet A6'nın çekirdek tarafı): kodlanmış parametre bloğu ve `boot_id`; `init`, `step`, `tick` çağrılarının sıralı izi ve her birinin tam girdisi (işaretçiyle gelen sonar dilimi içeriğiyle); her `bg_result` jetonu ve içeriği; `ida_out_t` (karşılaştırma için). Arka plan işi deterministik olduğundan tekrar oynatmada yeniden hesaplanabilir ama tüketim adımı kayıttan alınır.

## 4. Hata durumları ve davranışı

### 4.1 Kabul ve tazelik

- **Kabul:** örnek sonlu, `flags` geçerli, `seq` ve `t_meas` o sensörün son kabul edileninden kesin büyük olmalıdır. Yinelenen, eski ya da geçersiz örnek atılır, sayılır ve tazeliği yenilemez (iskelet §4).
- **İki ölçüt** (iskelet §4): (a) alım: son kabul edilen örneğin `t_rx`'inden bu yana geçen duvar süresi; (b) ölçüm zamanının ilerlemesi: aynı sensörün art arda kabul edilen örneklerinde `Δt_meas` ile `Δt_rx` artışları karşılaştırılır (farklı alanlar birbirinden çıkarılmaz). 1 s'lik pencerede oran HIL'de [0,9; 1,1] dışındaysa akış "gecikiyor", 0,5'in altındaysa "durmuş" sayılır (tahmin; SIL'de oran yapı gereği 1). Ayrıca zaman çerçevesi ilerlediği halde IMU'nun `t_meas`'ı ilerlemiyorsa IMU ölçüm zamanında da bayattır.
- **Sınır:** sabit bir teslim gecikmesi (akış hızı doğru ama hep geride) kart tarafından ölçülemez; ortak saat olmadan bu 06'nın gidiş-dönüş ölçümünün işidir.
- **Tazelik sınırları** (alım; tahmin): IMU 20 ms, basınç 200 ms, manyetometre 500 ms, DVL 1 s, GNSS 1 s (yalnız `YÜZEY`), batarya 2 s, sızıntı 2 s.

### 4.2 Eylemler

Sıra `WARN < HOLD < SURFACE < PASSIVE_SURFACE < DISARM`; en ağırı uygulanır. `HOLD` ve üstü `failsafe_clear = false`.
- `HOLD`: derinlik (yüzeyde dikey kapalı) ve yön tutulur; `EST_OK` ise konum tutulur, değilse yatay itki sıfır.
- `SURFACE`: güdüm `ASCENT` (`v_asc`, yön tutma, yatay `HOLD` kuralı); fiziksel rejim `YÜZEY` olunca dikey kapanır, yatay `HOLD` kuralı sürer. GNSS beklenmez.
- `PASSIVE_SURFACE`: sekiz itici nötr, arm korunur; araç ≈ 0,044–0,047 m/s ile yükselir (2.5).
- `DISARM`: nötr ve devre dışı.

### 4.3 Kip × arıza matrisi

Saat kuralı (iskelet §4): koşulun verisi ölçüm zamanında, süresi duvar saatinde. Süreler tahmindir ve parametredir. Sütunlar: **M** = `MANUAL`/`STABILIZE`, **T** = `DEPTH_HOLD`/`STATION_KEEP`, **G** = `GUIDED`, **A** = `AUTO`, **D** = `DISARMED`.

| Arıza (tetik; süre) | M | T | G | A | D | Temizleme | Dayanak |
|---|---|---|---|---|---|---|---|
| Sızıntı (2 ardışık örnek) | SURFACE | SURFACE | SURFACE | SURFACE | WARN, arm reddi | Devre dışı + `FAILSAFE_RESET` | 06 §2.5; 06.10 |
| Bağlantı kaybı (`HEARTBEAT`; 3 s / 60 s) | HOLD / SURFACE | WARN / SURFACE | HOLD / SURFACE | Su altında yok; yüzeyde 60 s SURFACE sürer | — | Bağlantı geri gelince otomatik | 06 §2.5 |
| Düşük batarya (10 s) | WARN | WARN | WARN | Görev iptali + SURFACE | WARN | Gerilim eşik + histerezis üstünde ve `FAILSAFE_RESET` | 11.4 (süre 10 s doğru) |
| Kritik batarya (10 s; 60 s) | SURFACE / PASSIVE | aynı | aynı | aynı | Arm reddi | Aynı | 11.4, 11.12 |
| Derinlik sınırı (`d > d_max − 0,5 m`) | Aşağı girdi kırpılır | Setpoint kırpılır | aynı | aynı | — | Otomatik; 60 s içinde ikinci ihlal → SURFACE, mandallı | 06 §2.5 |
| En az irtifa (`a < a_min`) | Aşağı girdi kırpılır | Yükselme | aynı | aynı | — | Otomatik | 06 §2.3 |
| Coğrafi sınır (yatay yarıçap; toplam görev süresi) | WARN | WARN | SURFACE | SURFACE | — | `FAILSAFE_RESET` | 06 §2.5 |
| `EST_DEGRADED` (anında; 30 s) | — | `STATION_KEEP` → `DEPTH_HOLD`; 30 s SURFACE | HOLD; 30 s SURFACE | HOLD (görev durur); 30 s SURFACE | — | `EST_OK` 2 s | 06 §2.5 |
| `EST_BAD` (2 s) | `MANUAL`: WARN; `STABILIZE`: PASSIVE | PASSIVE | PASSIVE | PASSIVE | Arm reddi | `FAILSAFE_RESET` + `EST_OK` 2 s | 06 §2.1 (EKF kötü 2 s) |
| Sayısal çökme (beklemesiz) | PASSIVE, aynı çağrıda | aynı | aynı | aynı | Arm reddi | 2.3.6 | inceleme #6 |
| IMU bayat (20 ms ya da akış durmuş) | PASSIVE (`tick` ile) | aynı | aynı | aynı | — | Taze IMU 1 s + `FAILSAFE_RESET` | iskelet §4 |
| IMU akışı gecikiyor (4.1 b; 2 pencere) | WARN | HOLD | HOLD | HOLD | — | Oran 2 s normal | iskelet §4 |
| Adım kaçırma (1 s'de > 4) | WARN | HOLD | HOLD | HOLD | — | 5 s kaçırmasız | Ç1 |
| Sensör bayat (4.1) | İlgili füzyon kapanır; sağlık düzeyine yansır; sızıntı bayatsa WARN ve dalış reddi | | | | | Taze örnek | D08 Y8 |
| Parametre hatası | — | — | — | — | Arm reddi | 3.6 | 08.11 |

Batarya eşikleri İDA-0 paketinden gelir (04); ArduSub'ın 10,5 V'u Copter varsayılanıdır, Sub'da gerilim tetiği varsayılan kapalıdır (11.4); kritik eşik paketin üretici en düşük güvenli geriliminin altına inmez (11.12'deki paket için 12 V).

### 4.4 Gözetim, girdi hataları, duraklatma

- **`alive_ok`:** taze IMU, son başarılı bağlantı servisi (platform bildirir), sayısal çökme yokluğu ve son `tick`'in zamanında çalışması birlikte; yalnız ISR'den besleme yetmez (00 Bölüm 6, D08 E). IWDG süresi ve reset boyunca çıkışların elektriksel güvenliği 01 ve M19'un konusudur.
- **Girdi hataları:** sınır dışı ya da kapasiteyi aşan girdi atılır, sayılır; her çağrı sonlu ve tanımlı bir çıktı üretir; `n_samples > 1200` sonar dilimi reddedilir.
- **HIL'de duraklatma:** kartın duvar saati ilerler, IMU ve bağlantı arızaları tetiklenir; bu istenen davranıştır (Ç1). 06'dan beklenen: HIL'de duraklatmanın desteklenmemesi ya da çalıştırmanın geçersiz sayılması.

## 5. Kalite kademelerinin etkisi

Otopilot her kademede tam çalışır (iskelet §6); çekirdekte kademe parametresi yoktur. Sonar kademesi dilimin içeriğini değiştirir, biçimini değiştirmez; kamera kapalıysa dış algılama gelmez, çekirdek ve 03 sıfır algılamayla çalışır. SIL'in gerçek zamandan yavaş koşması sanal zaman nedeniyle davranışı değiştirmez. Ölçüme bağlı yedekler kademe değildir (iskelet §6).

## 6. Ölçüme bağlı kararlar

| Karar | Birincil | Yedek | Ölçüm |
|---|---|---|---|
| Kontrol hızı | 400 Hz | 200 Hz (iskelet §4) | M5 |
| Kovaryans yayılımı | 10 ms birikimle, sabit boyutlu `mat.h` | Elle açılmış seyrek çarpım; CMSIS-DSP arka ucu (2.7) | M5 |
| Güncelleme formu | Açılmış skaler Joseph, adım başına ≤ 8 | Genel yoğun Joseph, adım başına ≤ 1 (2.2) | M5 + `double` referans karşılaştırması |
| Füzyon ufku `D` | 150 ms | Sensör gecikmeleri 05'te ölçülünce yeniden seçilir | 05'in gecikme modeli; SIL geç örnek oranı |
| Çıkış öngörücüsü `τ_o` | 0,25 s | Ufuktan tam yeniden yayılım (adım başına ≈ 60 entegrasyon) | M5 + SIL |
| Kartla SIL farkı | 7. başlıktaki dondurulmuş sınırlar | — | M10 |
| Bariyerde geciken adım | İskelet §4: sim son komutla ilerler, çalıştırmayı geçersiz sayar; çekirdek `DUP_SEQ` ile kapılanmış yeniden üretim | — | M17 |
| Kontrolcü | Kademeli PID | LQR (kazançlar çevrimdışı; 06 §2.4) | SIL ayarı |

## 7. Test yaklaşımı ve kabul ölçütleri

**Birim testleri** (Unity, PC ve kartta; 08 §2.6): kuaterniyon normu ve açı sarma; `B`'nin 04 tablosundan kurulumu ve rankı; tahsisin doymasızken `τ`'yu geri üretmesi, doymada sınır içinde kalması ve kalıntı işareti; integral dondurma yönü ve kazanç değişiminde süreklilik; eğri tersinin 04'ün ileri modeliyle gidiş-dönüşü ve ölü bant sınırı; kip geçiş tablosunun her durum × olay çifti; görev ve parametre işlemi doğrulaması; zaman tiplerinin karışmaması (derleme hatası testi). Rastgele testler sabit tohumla koşar.

**Kritik yol testleri:**
1. *Kovaryans zaman eşlemesi:* IMU aralığının içine düşen ölçümün füzyonu, aralığı bölmeden ince adımlı referansla aynı sonucu verir.
2. *Gecikmeli füzyon eşdeğerliği:* ufuk yöntemi, her ölçümde geçmişten itibaren tam yeniden işleyen kaba kuvvet referansıyla karşılaştırılır.
3. *Sayısal çökme:* NaN enjeksiyonunda aynı çağrının çıktısı nötr ve izin kapalıdır.
4. *Aynı adımda `DISARM` + atış isteği:* `FIRE` olayı üretilmez.
5. *Tetik tekrarı:* `DUP_SEQ` ve yeniden bağlanma olay üretmez; alıcı aynı `event_id`'yi bir kez uygular.
6. *Duvar arızasından sonra yeniden gönderim* eski itki komutunu geri getirmez.
7. *IMU'suz SIL:* IMU kesilince zaman ilerler, 20 ms + bir çağrı periyodu içinde nötr çıktı, bariyer kilitlenmez.
8. *Gecikmiş ve duran HIL akışı:* yinelenen ya da eski IMU tazeliği yenilemez; oran ölçütü tetiklenir.
9. *Sıfırlama sınırı:* `nav_at` sınır üzerinden ara değerleme yapmaz.

**Yayımlanmış referanslar:** ESKF için MSS `ins_mekf` (D06 #24), ILOS için `ILOSpsi.m`, referans modeli için PythonVehicleSimulator `refModel3` (MIT) (06 §2.10); altın dosyalar sabitlenmiş sürümle üretilir (A16 açık). Referans karşılaştırması yetmez (D06 E): SIL Monte Carlo koşularında yer gerçeğiyle NEES/NIS tutarlılığı ölçülür.

**SIL senaryoları** (ArduSub autotest örnek, 06 §2.10): derinlik tutma; akıntıda istasyon tutma ve ILOS; dalış, ölü hesap, yüzeye çıkış ve GNSS sıfırlaması; DVL kesintisi; GNSS gecikmesi; her kipte bağlantı kaybı; sızıntı; batarya; derinlik ve coğrafi sınır; kestirim bozulması; tarama kapsaması.

**Kontrolcü ayarı simülasyonda** (06 §2.10): sistem tanılama, IAE + aşım + efor maliyetinde ızgara ya da Nelder–Mead araması (SIL, gerçek zamandan hızlı), rejim başına kazanç seti, HIL'de doğrulama, model farkı için pay.

**Kabul sınırlarının dondurulması.** Ayar senaryoları ve tohumları ile kabul senaryoları ve tohumları ayrıdır; ayar verisi kabulde kullanılmaz. Kabul sınırları ilk kabul koşusundan önce yazılır ve dondurulur; sonuç görüldükten sonra gevşetmek mimar kararı ve kayıt gerektirir. Bu bölümün dondurduğu emniyet sınırları: sayısal çökmede ve `DISARM`'da güvenli çıktı aynı çağrıda; IMU bayatlığında nötr çıktı ≤ 20 ms + bir duvar çağrısı periyodu; yinelenen atış olayı sıfır; arıza tablosu eylemleri tablodaki sürelerde. M10 açık çevrim tekrar oynatma sınırları (tahmin, ilk ölçümden önce donmuş): 60 s'de konum farkı ≤ 1 mm, tutum ≤ 1e-4 rad, itici komutu ≤ 1e-3. Doğruluk sınırları (çapraz hata, derinlik hatası) 09'un sahipliğinde, aynı kuralla ayar koşularından önce dondurulur.

**Kart ile SIL farkı (M10):** (1) aynı ikilide iki koşu bit düzeyinde aynı; (2) macOS-arm64 ile Windows-x64 ve (3) kart ile SIL, 3.9'daki kayıt açık çevrimde yeniden oynatılarak karşılaştırılır (kapalı çevrimde küçük farklar yörüngeyi ayırır). Çıktı alanı başına en büyük mutlak ve göreli fark ile ilk ayrışma adımı raporlanır (Ç7).

**Kabul ölçütleri:** birim ve kritik yol testleri iki işletim sisteminde ve kartta geçer; bellek denetimi (2.7) ve statik analiz temiz; aynı ikilide bit düzeyinde tekrar; M5 ile en kötü durum adım süresi ≤ 1,0 ms; SIL senaryoları donmuş sınırlarla geçer; M10 donmuş sınırlar içinde.

## 8. Açık noktalar ve iskelete öneriler

### 8.1 Sözleşme aşamasında tamamlanacaklar

Mimari karar bu bölümde verildi; aşağıdaki tanımlar A4 sözleşmesi yazılırken tamamlanır (iskelet §3):
1. `Q_c` bloklarının IMU gürültü yoğunluklarından ve sapma rastgele yürüyüşünden kuruluşu, `L` matrisi, `P₀` değerleri ve başlangıç hizalamasının kabul ölçütü (inceleme #1).
2. Manyetometre yön ölçümünün Jacobian'ı, eğim telafisi formülü, sapma açısı ve alan normu/eğim açısı geçerlilik koşulları (#4).
3. GNSS anten konumu ve hızı Jacobian'ları; `eph`'in `σ`'ya dönüşüm ölçeğinin anlamı (05 ile: HDOP tabanlı mı, 1σ mı) (#4).
4. DVL kovaryansının sensörden gövdeye dönüşümü `R_b = R_dvl R_s R_dvlᵀ` ve FOM → `σ` ölçeği (#4).
5. Bütün `ida_cmd_t` türlerinin yük alanları: `MANUAL_INPUT`'un altı ekseninin `τ_maks,manuel` ile kuvvete ölçeklenmesi, `STABILIZE`'da girdinin anlamı, `GUIDED_SETPOINT` türleri (konum, hız, derinlik, irtifa) ve çerçeveleri (#13).
6. `ida_telem_t` ve olay kayıtlarının alanları; `plat` alanının tam listesi; her hata dönüşünün çıktı anlamı (#13).
7. `ida_fc_in_t`, `ida_fc_out_t`, `ida_fc_bg_in_t`, `ida_fc_bg_out_t` alanları (03 ile birlikte) (#13, #15).
8. Kapasitelerin son değerleri ve yapı boyut/hizalama `_Static_assert` değerleri (#15).
9. Parametre tablosunun tam listesi ve sınıfları; kodlanmış kalıcı biçimin bayt düzeni (#23).

### 8.2 Açık noktalar

1. **Fizik gerçeği ile otopilot parametreleri ayrı tutulmalı:** A5 (08) iki kümeyi ayırmalı; aksi halde model hatasına dayanıklılık sınanamaz.
2. **İç basınç ve iç sıcaklık** ArduSub'da arıza girdisidir (06 §2.1), iskeletin sensör setinde yok; mimarın kararı.
3. **Batarya eşikleri** İDA-0 paketine (04) ve A19'a bağlı.
4. **A16:** referans karşılaştırmasında Python/Matlab, tasarım betiklerinde SymPy kullanımı kullanıcı onayı bekliyor.
5. **Sabit teslim gecikmesi** kart tarafından ölçülemez (4.1); 06'nın gidiş-dönüş ölçümüyle tamamlanmalı.

### 8.3 Komşu bölümlere notlar

- **03:** açılış içinde benzersiz `shot_id` (yeniden başlatmalar arası benzersizliği çekirdeğin `event_id`'sindeki `boot_id` sağlar), arka plan işinin `ida_core_t`'ye dokunmaması, geçmiş sorgu dönüş kodlarına uyum.
- **06:** zaman çerçevesinin alanları; `src = TICK` eş zamansız güvenli çıktının ve `event_id` tekrar elemesinin A1'deki karşılığı; sürekli komutun kullanım süresinin alıcıda uygulanması.
- **01:** ≥ 100 Hz `tick`, `t_rx` damgası, `alive_ok` ile IWDG beslemesinin bağlanması, FPSCR kaydı.
- **04:** itki eğrisi ve `B` kaynağının aynı A5 sürümünden okunması; kütle durumu geçişinin atış olayıyla eşlenmesi.

İskelete aykırı bir gereklilik bulunmadı.
