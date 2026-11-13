# 04 — Simülatör çekirdeği

Sürüm: 0.1 (2026-11-12). Durum: taslak; mimar onayı bekliyor.

Bu bölüm [00-iskelet.md](00-iskelet.md) Bölüm 9, satır 04'ün kapsamını yazar. Dayanak [../research/00-duzeltmeler-ve-kararlar.md](../research/00-duzeltmeler-ve-kararlar.md) belgesidir (bundan sonra "00"). Atıf biçimi: `04.n` = 00 Bölüm 4'teki düzeltme satırı, `Çn` = çelişki çözümü, `Mn` = ölçüm konusu, `Ön`/`Kn` = karar günlüğü, `DNN` = `docs/research/dogrulama/NN-codex.md`, `NN §x` = ham rapor bölümü. Ham rapordan alınan her değer önce 00'daki düzeltmeyle karşılaştırıldı.

Değerlerin niteliği dört etiketle verilir: **ölçülmüş (yayımlanmış)**, **ampirik tahmin (yayımlanmış)**, **mimari varsayımı** (raporlarda yok; bu bölüm seçti, gerekçesi yanında) ve **yayımlanmış, niteliği doğrulanmadı** (kaynakta sayı var ama ölçüm mü tahmin mi olduğu raporlarda ayrılmamış; bkz. Bölüm 8). Bu bölümdeki hiçbir hız, yük ya da süre rakamı ölçülmemiştir; geçtikleri yerde "tahmin" diye işaretlenir.

---

## 1. Amaç ve sorumluluklar

Sim sunucusunun çekirdeği dünyanın tek yetkili durumunu tutar ve sabit adımla ilerletir. Sorumlulukları:

1. Araç, torpido ve hedeflerin 6 serbestlik dereceli (6-SD) hareketi; yüzey, kısmi batma ve dalış tek modelde.
2. Çevre: dalga, akıntı, rüzgar, su özellikleri, havuz ve açık su ortamı.
3. İDA-0 parametre setinin yüklenmesi, doğrulanması ve çalışma sırasında değişmesi (torpido ayrılması).
4. İtici, motor sürücüsü (ESC) ve batarya modeli; elektrik ile itki arasındaki bağlaşım.
5. Torpido fırlatma, uçuşu ve isabet tespiti; hedef hareketi.
6. Çarpışma sorgusu ve özel temas tepkisi.
7. Zaman yöneticisi: tamsayı nanosaniye, sabit adım, alt sistem zamanlaması, üç zaman kipinin sürücüleri, kaçırılan adım sayacı.
8. Sensör eklentilerine (05), HIL köprüsüne (06), görselleştiriciye (07) ve kayıt ile panellere (08) iç arayüzler.
9. Tekrar üretilebilirlik: aynı platformda bit düzeyinde tekrar, platformlar arasında toleranslı eşdeğerlik (Ç7).

Yapmadıkları: sensör ölçümü üretmez (05), mesaj biçimi ve adım bariyerinin protokolünü tanımlamaz (06), görüntü çizmez (07), senaryo dosyasının biçimini tanımlamaz (08). **İtki tahsisi yapmaz:** simülatör yalnızca ileri dönüşümü (PWM → itici kuvveti → gövde kuvveti) uygular; tahsis kart yazılımındadır (04 §2.4 "Mimari not", 02 bölümü).

---

## 2. İç yapı

### 2.1 Alt modüller ve veri akışı

```
sim/core/
  time/       Clock (int64 ns), Scheduler (rasyonel periyot), sürücüler (FreeRun, Realtime, Lockstep)
  world/      World::step(), gövde listesi, olay kuyruğu
  dynamics/   RigidBody6, kütle matrisi + Cholesky, C_RB/C_A, sönüm, RK4, quaternion
  hydro/      hidrostatik ağ kırpma, basınç integrali, yüzey geçişi, yüzey sönümü, havalanma
  env/        WaveField (bileşen listesi), CurrentField, WindField, Medium (ρ, h, sıcaklık)
  actuators/  EscModel, ThrusterModel, Launcher
  power/      Battery (Thevenin), Bus (bağlaşım çözücüsü), güç olayları
  contact/    Jolt sorgu sarmalayıcısı, temas tepkisi, isabet tespiti
  params/     A5'ten gelen parametrelerin doğrulanması, birim ve işaret dönüşümü
  rng/        sayaç tabanlı üreteç ve sürümlü dağılımlar
  math/       ida_math (aşkın işlevler), küçük sabit boyutlu tipler (Eigen)
```

Bir fizik adımının sırası (her adımda aynı, sabit):

1. Gelen aktüatör çerçevesi uygulanır (06'dan; adım başında, adım içinde değişmez).
2. Zamanlanmış olaylar (arıza, fırlatma başlangıcı, senaryo olayları) işlenir.
3. Elektrik bağlaşımı çözülür (2.7.4): her iticinin akımı ve bara gerilimi.
4. Temas sorgusu yapılır (2.9); temas kuvvetleri adım boyunca sabit tutulur.
5. Bütün gövdeler RK4 ile birlikte ilerletilir (2.10). Hidrostatik, dalga, sönüm, itki ve itici dinamiği her RK4 aşamasında yeniden hesaplanır.
6. Quaternion normalleştirilir; batarya durumları ilerletilir; isabet sorgusu yapılır.
7. Zamanlayıcıya göre sensör eklentileri (05), görselleştirici yayını (07), kayıt (08) beslenir.

### 2.2 Koordinat sistemleri ve durum

- **Dünya {n}:** yerel düz NED (kuzey, doğu, aşağı), metre. Jeodezik başlangıç noktası (enlem, boylam, yükseklik) A5'te; jeodezik dönüşümü yalnızca GNSS modeli kullanır (04 §2.1; 05'ten beklenen).
- **Gövde {b}:** x ileri, y sancak, z aşağı (SNAME). Orijin İDA-0'da ağırlık merkezi (CG).
- **Dalga koordinatı:** dalga formülleri yukarı pozitif `z_u = −z_n` ile ve **ortalama** su yüzeyine göre yazılır (04.3'ün koşulu: koordinatın hangi yüzeye göre tanımlandığı açık olmalı). Yön açısı β kuzeyden doğuya ölçülür.
- **Varlık koordinatı:** glTF sağ elli, +Y yukarı, metre (12.16). glTF'den gövdeye dönüşüm yalnız varlık yükleyicide yapılır: `x_b = +Z_gltf, y_b = −X_gltf, z_b = −Y_gltf` (sağ elli olduğu bu bölümde denetlendi; glTF'nin "+Z ileri" kabulüyle eşleşmesi varlık manifestinde sabitlenir — mimari varsayımı, 07 ve A7 ile birlikte doğrulanır).
- **Görselleştirici koordinatı:** Godot'ya dönüşüm A2 sınırında, 07'nin sorumluluğundadır; fizik çekirdeği NED'de kalır (04 §2.1).
- **Durum vektörü (gövde başına):** `p_n` (3), `q_nb` (birim quaternion, 4), `ν = [u v w p q r]` (6); itici başına ESC/itici filtre durumları (3); batarya RC gerilimi ve yük durumu (SoC). Hepsi `double` (Ö3).
- Euler açıları yalnız telemetri ve senaryo girişi içindir; θ = ±90°'de tekil olduğu için çekirdekte kullanılmaz (04 §2.1, §4 md.9).

### 2.3 Hareket denklemleri

Tek model hem yüzeyi hem dalışı kapsar; ortamdan bağımsız terimler (`M_RB`, `C_RB`, itki) sabit, geri getirici kuvvetler anlık batmış geometriden gelir (04 §2.1, Hunt–Whitcomb yaklaşımı; o çalışmanın sayısal sonuçları doğrulanmadı, 04.14).

Su hızı `ν_w` = akıntı + dalga yörünge hızı, gövde çerçevesinde, referans noktada (yüzerlik merkezi); `ν_r = ν − ν_w`.

```
(M_RB + M_A) ν̇ = τ_itici + τ_basınç + τ_W + τ_rüzgar + τ_temas + τ_fırlatma
                 − C_RB(ν) ν − C_A(ν_r) ν_r − D(ν_r, s) ν_r + M_A ν̇_w

M_RB = [[m I₃, −m S(r_g)], [m S(r_g), I_o]]          (04 §2.1)
τ_W  = [Rᵀ f_g ; r_g × Rᵀ f_g],  f_g = [0, 0, m g]ᵀ  (yalnız ağırlık)
ν̇_w = [ −S(ω_b) ν_w,1:3 + Rᵀ v̇_wⁿ ; 0 ]             (MSS oceanCurrents biçimi, 04 §2.3)
```

- `τ_basınç`: hidrostatik ve birinci mertebe Froude–Krylov (FK) basıncının ıslak yüzey üzerindeki integrali (2.4). Yüzerlik bu terimin içindedir; klasik `g(η)` ayrıca eklenmez.
- **Eklenmiş kütle tek kez sayılır (04.5):** ivmeli akıştaki eklenmiş kütle katkısı yalnız `M_A ν̇_r = M_A ν̇ − M_A ν̇_w` üzerinden girer. FK kuvveti (`ρ∇ u̇_w` eşdeğeri) ya ağ üzerindeki basınç integralinden ya da nokta modelinden gelir, ikisinden birden asla. Tam `M ν̇_r` sadeleştirmesi (`C_RB`'yi göreli hızla yazmak) kullanılmaz; çünkü sabit akıntı varsayımı ivmeli dalgada geçerli değildir (04.5).
- `C_A(ν_r)`: `M_A`'dan Kirchhoff biçimiyle (PVS `m2c`), çarpık simetrik; `νᵀCν = 0` (04 §2.1). Munk momenti korunur; PVS'nin bazı `C_A` terimlerini sıfırlayan sadeleştirmesi taşınmaz (04.11).
- `D(ν_r, s) = s_D(s) · (D_L + D_NL(ν_r)) + D_yüzey(s)`, köşegen (04 §2.1; von Benzon Denk. 10–11). `s` batıklık oranıdır (2.4).
- Eklenmiş kütle asla "önceki adımın ivmesi × A" biçiminde dış kuvvet olarak uygulanmaz; İDA-0'da `Z_ẇ = 18,68 kg > m = 13,5 kg` olduğundan bu yol kararsızdır (04 §2.6; Gazebo bağlamında doğru, D04 #4).
- Kütle matrisi simetrik ve pozitif tanımlı olmalıdır; yükleyici denetler (Bölüm 4).

### 2.4 Hidrostatik, dalga basıncı ve yüzey geçişi

**Ağ.** Yüzerlik için A7'den gelen kapalı, manifold "BUOY_*" ağları kullanılır (12 §2.7). Ağ, aracın **gerçek su deplasmanını** temsil eder: basınç gövdeleri ve köpük kapalı hacim, açık çerçevenin su dolan boşlukları değil (04.17). Torpido ayrılınca araç kütlesi, CG'si, ataleti ve deplasmanı güncellenir (04.17).

**Kırpma.** Her üçgen, köşelerindeki anlık yüzey yüksekliği `ζ(x, y, t)` ile su çizgisinde kesilir; batmış parçalar üzerinde `dF = −p n dA` toplanır, moment basınç merkezine göre alınır (Kerner; 04 §2.2). Üçgenler en kısa önemli dalga boyuna göre küçük olmalıdır (Kerner uyarısı, 04 §2.2); varlık hattı bunu manifestte kaydeder (07'den beklenen).

**Basınç (04.3'ün düzeltilmiş hali, anlık derinlik biçimi):**

```
p − p_atm = ρ g (ζ − z_u) + ρ g Σ_i a_i [F_p,i(z_u) − 1] cos θ_i        (z_u ≤ ζ)
θ_i = ω_i t − k_i (x cos β_i + y sin β_i) + φ_i,     ζ = Σ_i a_i cos θ_i
```

Islak yüzeyin kırpılması ile basınç referansının seçimi ayrı işlemlerdir; ilk terim `ρgζ`'yi zaten içerdiği için FK katkısı yalnız `[F_p − 1]` ile eklenir (04.3). Ortalama yüzeyin üstündeki noktalarda (`0 < z_u ≤ ζ`) doğrusal teorinin uzatılması (Wheeler germe vb.) doğrulanmadı (04 §2.3); ilk sürümde formül olduğu gibi uygulanır ve açık nokta olarak kalır (Bölüm 8).

**Sonlu derinlik fonksiyonları (04.4; her büyüklüğün kendi fonksiyonu):**

```
F_p(z_u) = cosh k(z_u + h) / cosh kh                                   (basınç)
u_yatay  = a ω · cosh k(z_u + h) / sinh kh · cos θ   (β yönünde)
w_yukarı = a ω · sinh k(z_u + h) / sinh kh · sin θ
ivmeler: zamana göre türev (u̇ = −a ω² … sin θ, ẇ = a ω² … cos θ)
dispersiyon: ω² = g k tanh(kh)   (04 §2.3)
```

Faz işaretleri konvansiyona bağlıdır (04.4); kod bu bölümdeki konvansiyonu kullanır. Derin suda üç fonksiyon `e^{k z_u}`'ya iner. `kh > 10` iken taşmayı önlemek için üstel biçim kullanılır (mimari varsayımı: eşik sayısal güvenlik içindir, fiziksel fark ihmal edilebilir düzeyde olduğu için seçildi). Yeni dalga bileşeni eklendiğinde dispersiyon denklemi sabit sayıda Newton yinelemesiyle çözülür (tekrar üretilebilirlik için yineleme sayısı sabittir).

**Nokta modeline geçiş.** Araç tamamen batık ve şu ölçüt bütün bileşenler için sağlanıyorsa ağ yerine nokta modeli kullanılır (04.10'un düzeltilmiş ölçütü):

```
d ≥ (λ_max / 2π) · ln(1/ε)        d: gövdenin en üst noktasının ortalama yüzeyden derinliği
```

`λ_min` tabanlı eşik kullanılmaz; uzun dalgalar derine işler (örnek: `d = 0,5 m`'de 20 m dalga boyunun çarpanı ≈ 0,855; 04.10). `ε = 0,01` ve geri dönüşte %10 histerezis mimari varsayımıdır (amaç kipler arasında titreşimi önlemek; M15'te sınanır). Nokta modelinde: `∇`, `r_b` sabit (tam batık değerler), FK kuvveti `[ρ∇ Rᵀ a_wⁿ ; r_b × ρ∇ Rᵀ a_wⁿ]`; `a_wⁿ` referans noktadaki su ivmesidir. Nokta modeli ayrıca gövde boyunca akış değişiminin küçük olmasını gerektirir (04.10); Morison'un λ > 5D koşulu ince elemanlar içindir, hacimli araçta doğruluk garantisi değildir (D04 E:Morison).

**Batıklık oranı.** `s = ∇_batık / ∇_tam`, ağ integralinden. 

**Eklenmiş kütle ve sönümün geçişi.** Birincil: `M_A` sabit (tam batık değer); Cholesky bir kez yapılır ve `Ṁ_A` terimi doğmaz. Yedek: `M_A(s)` C¹ yumuşak ölçekleme; bu durumda kütle matrisi her RK4 aşamasında yeniden ayrıştırılır. Her iki seçenek de **deneysel olarak doğrulanmamış yaklaşımdır**; yumuşatma sayısal süreklilik sağlar, enerji korunumunu veya doğru giriş/çıkış fiziğini sağlamaz (04.9). `Ṁ_A ν` ya da Lagrange kaynaklı `½A'(z)ż²` gibi terimler ilk sürümde yoktur; suya giriş çarpması (slamming), sıçrama ve hava sürüklenmesi modelin dışındadır (04 §4 md.3). Seçim M15'teki enerji ve momentum testleriyle yapılır (Bölüm 6). Sürükleme `s_D(s) = s` ile ölçeklenir (mimari varsayımı: havada sürükleme ihmal edilir, ara durumda batmış oranla doğrusal; Wang–Shang doğrusal ağırlığı bu kabulün kanıtı değildir, 04.15).

**Yüzey sönümü.** Su yüzeyini kesen gövdede eksik kalan dalga yapma sönümü heave, roll ve pitch'e doğrusal terimle eklenir (Otter yaklaşımı, 04 §2.1): `Z_w = 2ζ₃ω₃M₃₃`, `K_p = 2ζ₄ω₄M₄₄`, `M_q = 2ζ₅ω₅M₅₅`. `ζ₃,₄,₅ = 0,3 / 0,2 / 0,4` Otter'in değerleridir (PVS kaynak kodu, 04 §2.1); İDA-0 için kullanılması mimari varsayımıdır (açık çerçeveli araca ait değer yok). Doğal frekanslar yüklemede, yüzeydeki denge konumunda ağdan hesaplanır (`ω₃ = √(ρ g A_wp / (m + A₃₃))`, 04 §2.8). Terim, su çizgisi gövdeyi kesiyorsa tam, tamamen batıkken sıfırdır; arada su çizgisinin üst noktaya uzaklığıyla C¹ geçiş yapar (mimari varsayımı).

**Pervane havalanması.** Her iticinin itkisi `β(h/R)` ile çarpılır; `h` itici ekseninin yerel yüzeyden derinliği, `R = 38 mm` (T200, 04 §2.2). `h/R ≥ 3,4` "derin" kabul edilir (literatür eşiği; katsayılar doğrulanmadı, 04 §2.2). `β`: `h/R ≤ 0`'da 0, `3,4`'te 1, arada doğrusal (mimari varsayımı: T200'e özgü veri yok, 04 §4 md.4; tablo A5'te değiştirilebilir).

### 2.5 Çevre

**Dalga spektrumu.** Pierson–Moskowitz ve JONSWAP (DNV düzeltmeli `A_γ = 1 − 0,287 ln γ`, `σ = 0,07 / 0,09`, γ geçersizse DNV kuralı; D04 #6 DOĞRU). Yönsel yayılım `D(β)` A5'te seçilir.

**Bileşen listesi.** Senaryo tohumundan, çekirdekteki RNG ile üretilir (12 §2.5). Her bileşen: `{a_i, ω_i, k_i, β_i, φ_i}`, `a_i = √(2 S(ω_i) D(β_i) Δω Δβ)`; frekanslar kutu içinde rastgele seçilir, periyodik tekrar önlenir (04 §2.3). Varsayılan bileşen sayısı 64'tür (04 §3.2'deki 32–64 aralığının üst ucu; mimari varsayımı: fizik her kademede tam çalıştığı için yüksek uç seçildi; hesap süresi ölçülmedi, 04.16, M15). **Fizik listedeki bütün bileşenleri toplar; kesme yapmaz.** Aynı liste, sürüm numarası ve özetiyle A2 üzerinden görselleştiriciye verilir; görselleştirici rastgele sayı üretmez (12 §2.5). Görselleştirici FFT ızgarası, LOD ya da enterpolasyon kullanırsa fark kayan noktadan ibaret değildir; atılan bileşenler için `|Δη| ≤ Σ|a_i|` (12.14, 10.7). Örnekleme, enterpolasyon, yatay öteleme ve zaman sözleşmesi 07'den beklenen; karşılaştırma için çekirdek referans yükseklik sorgusu sunar (Bölüm 3).

**Akıntı.** Birincil: NED'de düzgün, dönümsüz akıntı `v_cⁿ = V_c [cos γ cos β, cos γ sin β, sin γ]`, isteğe bağlı sınırlı Gauss–Markov değişimi (MSS `oceanCurrents.m`, 04 §2.3). Derinliğe bağlı profil (ör. 1/7 kuvvet yasası) DNV metni okunmadığı için doğrulanmadı (04 §2.3); sonraki aşamada sürüklemeye ağ elemanı ya da örnek nokta başına yerel hız olarak girer; o zaman Fossen'in düzgün akıntı varsayımı bozulur (04 §2.3). DVL su takibi akıntı alanını sorgular (00 Bölüm 6); sorgu arayüzü Bölüm 3'te.

**Rüzgar.** Yalnız su üstündeki kısım etkilenir: `τ_rüzgar = ½ ρ_a C_D A_üst(t) |V_rw| V_rw`, kırpılmış ağın su üstü izdüşüm alanının merkezine uygulanır. Fossen'in yönlü katsayı tabloları (04 §2.3) İDA-0 için yok; `C_D = 1,0` ve `ρ_a = 1,225 kg/m³` mimari varsayımıdır (küt cisim için yer tutucu ve standart hava). Ani rüzgar spektrumları doğrulanmadı (04 §2.3); ilk sürümde sabit rüzgar + isteğe bağlı Gauss–Markov. İDA-0 yüzeyde neredeyse tamamen batık seyrettiği için (2.6) rüzgar etkisi küçüktür.

**Ortamlar (A5'ten seçilir):**

| Ortam | ρ (kg/m³) | Derinlik | Sınırlar | Dalga |
|---|---|---|---|---|
| Havuz | 1000 (tatlı su; von Benzon setiyle aynı, 04 §2.5) | sabit `h`, A5 | duvar ve taban: A7'deki statik çarpışma şekilleri | varsayılan yok (`H_s = 0`) |
| Açık su | 1025 (mimari varsayımı: tipik deniz suyu, raporlarda yok) | batimetriden (03, A5) | taban; isteğe bağlı engeller | spektrum A5'ten |

Su sıcaklığı ve tuzluluk ortam parametresidir; ses hızı ve sensör etkileri 05'te kullanılır.

### 2.6 İDA-0 parametre seti

Araç parametrelerini mimar belirler (K10). Bütün değerler A5 dosyasından okunur ve değiştirilebilir; dosya her katsayı için değer, birim, işaret kuralı, kaynak ve belirsizlik alanlarını taşır (04 §2.5, §3.4). İç temsilde tek işaret kuralı vardır (büyüklük pozitif); Fossen negatif türev biçiminden dönüşüm yükleyicide yapılır (04 §3.4). von Benzon seti hedef aracın kalibrasyonu değil, benchmark'tır (D04 N4).

**Kütle, yüzerlik, sönüm** (kaynak: von Benzon ve ark. 2022, Tablo A1, 04 §2.5 aktarımıyla; düzeltme 04.1):

| Parametre | Değer | Birim | Kaynak / düzeltme | Nitelik |
|---|---|---|---|---|
| Kütle `m` | 13,5 | kg | Tablo A1; D04 #1 | yayımlanmış, niteliği doğrulanmadı |
| Deplasman `∇` | **0,0137** | m³ | Yayımlanan 0,0134 (D04 #1); aşağıdaki not | mimari varsayımı |
| `I_x, I_y, I_z` | 0,26 / 0,23 / 0,37 | kg·m² | Tablo A1 (04 §2.5; D04'te ayrıca denetlenmedi) | yayımlanmış, niteliği doğrulanmadı |
| Atalet çarpımları | 0 | kg·m² | kaynakta yok | mimari varsayımı (simetrik gövde) |
| `r_g` | (0, 0, 0) | m | Tablo A1 | yayımlanmış, niteliği doğrulanmadı |
| `r_b` (tam batık) | (0, 0, −0,01) | m | Tablo A1 | yayımlanmış, niteliği doğrulanmadı |
| `g` | 9,82 | m/s² | Tablo A1 (Tablo 4 regresyonunu bozmamak için korunur) | yayımlanmış |
| `X_u̇, Y_v̇, Z_ẇ` | 6,36 / 7,12 / 18,68 | kg | Tablo A1; Eidsvik yöntemi, öteleme hatası %10–20, geçerlilik koşulu ihlal (04.1, 04.18) | ampirik tahmin (yayımlanmış) |
| `K_ṗ, M_q̇, N_ṙ` | 0,189 / 0,135 / 0,222 | kg·m² | aynı; dönme hatası %30–100 | ampirik tahmin (yayımlanmış) |
| `M_A` köşegen dışı | 0 | — | von Benzon köşegen kabulü | mimari varsayımı |
| `X_u, Y_v, Z_w` | 13,7 / 0 / 33,0 | N·s/m | Tablo A1, deneysel (04.1) | ölçülmüş (yayımlanmış) |
| `K_p, M_q, N_r` | 0 / 0,8 / 0 | N·m·s/rad | aynı | ölçülmüş (yayımlanmış) |
| `X_u|u|, Y_v|v|, Z_w|w|` | 141,0 / 217,0 / 190,0 | N·s²/m² | aynı; D04 #1 | ölçülmüş (yayımlanmış) |
| `K_p|p|, M_q|q|, N_r|r|` | 1,19 / 0,47 / 1,5 | N·m·s²/rad² | aynı | ölçülmüş (yayımlanmış) |
| Belirsizlik bandı | öteleme ±%30, dönme ±%100 | — | 04 §2.5 önerisi | mimari varsayımı (Monte Carlo için) |

**Hafif negatif yüzerlik sorununun çözümü.** Yayımlanan `∇ = 0,0134 m³`, `ρ = 1000` ile 13,4 kg yer değiştirir; `m = 13,5 kg` ile araç ≈ 0,1 kg negatif yüzer (04 §2.5, türetim). İskelet hafif pozitif yüzerlik istediği için (iskelet §5) İDA-0'da kütle korunur, deplasman **0,0137 m³** alınır: tatlı suda net yüzerlik `+0,2 kg` (≈ +1,96 N), deniz suyunda (1025) `+0,54 kg`. Gerekçe: kütle ve atalet setin geri kalanıyla tutarlı kalır; değişiklik deplasmanın %2,2'sidir, sönüm ve eklenmiş kütle belirsizlik bandının çok altında. İskelet balast sistemi istemediğinden (iskelet §5) ince ayar için A5'te sabit `trim_kütlesi` ve konumu vardır (varsayılan 0; balast aktüatörü değil, kurulumda takılan ağırlık). Yüzeyde su üstünde kalan hacim `(ρ∇ − m)/ρ ≈ 0,2 L`'dir; araç yüzeyde neredeyse tamamen batık seyreder ve dikey iticilerle dalar.

**Yüzerlik ağı (yer tutucu).** Gerçek İDA-0 varlığı A7 hattıyla gelene kadar (07) tek kapalı kutu kullanılır: taban 0,40 × 0,48 m (genişliğin boyu %20 aşması von Benzon'daki Heavy oranından, 04 §2.5), yükseklik `∇ / (0,40·0,48) = 0,0714 m`, merkezi `r_b`'de. Bu ağ mimari varsayımıdır; tam batıkta `∇` ve `r_b`'yi tam verir ama yüzeydeki su çizgisi alanını ve devrilme sertliğini gerçek dağılımlı geometriden farklı üretir. Varlık geldiğinde yükleyici, ağın tam batık hacmi ve merkezinin bu tablodaki `∇` ve `r_b` ile tutarlılığını denetler (Bölüm 4).

**İtici yerleşimi** (sekiz T200; dördü yatay düzlemde vektörlü, dördü dikey; iskelet §5). von Benzon Tablo A1 itici konumlarını ve yönlerini içerir (04 §2.4), ancak değerler raporlara aktarılmadı ve bağımsız doğrulanmadı. Aşağıdaki tablonun tamamı **mimari varsayımıdır**; Tablo A1 okunduğunda karşılaştırılıp gerekirse değiştirilir (Bölüm 8).

| # | Konum `r_i` (m, gövde) | Yön `ε_i` (pozitif komut) | Rol |
|---|---|---|---|
| 1 | ( 0,156,  0,111, 0,085) | (1, −1, 0)/√2 | yatay, ön sancak |
| 2 | ( 0,156, −0,111, 0,085) | (1,  1, 0)/√2 | yatay, ön iskele |
| 3 | (−0,156,  0,111, 0,085) | (1,  1, 0)/√2 | yatay, arka sancak |
| 4 | (−0,156, −0,111, 0,085) | (1, −1, 0)/√2 | yatay, arka iskele |
| 5 | ( 0,120,  0,218, 0) | (0, 0, 1) | dikey, ön sancak |
| 6 | ( 0,120, −0,218, 0) | (0, 0, 1) | dikey, ön iskele |
| 7 | (−0,120,  0,218, 0) | (0, 0, 1) | dikey, arka sancak |
| 8 | (−0,120, −0,218, 0) | (0, 0, 1) | dikey, arka iskele |

Bu yerleşim yatay iticilerle surge, sway ve yaw'ı, dikeylerle heave, roll ve pitch'i bağımsız üretir (geometriden denetlendi: yatay iticilerin yaw momentleri 1 ve 3'te negatif, 2 ve 4'te pozitif). Pervane tepki torku ihmal edilir (mimari varsayımı: T200 tork verisi raporlarda yok). Gövde kuvveti `τ = Σ_i [ε_i F_i ; r_i × ε_i F_i]` (04 §2.4). Pervane dönüş yönü ve kablolama, kartın gördüğü itici sırasıyla birlikte A5'te tutulur; tahsis matrisi 02'nin işidir.

### 2.7 İticiler, ESC ve güç

**2.7.1 Komut ve ölü bant.** Kart her itici için zaman damgalı PWM darbe genişliği (µs) verir (06'dan beklenen; 50–400 Hz, 11 §2.B.7). ESC üretici değerleri doğrulanmıştır: 1100 µs azami geri, 1500 µs dur, 1900 µs azami ileri, **dur ölü bandı 1500 ± 25 µs**, güncelleme 400 Hz'e kadar, 7–26 V, ısıl kısma basamakları ≥140/145/150/155 °C'de %75/50/25/0 (D11 #6). Normalize komut: `u = clamp((PWM − 1500)/400, −1, 1)`, ölü bant içinde 0. Ölçülmüş gerilime bağlı sıfır-itki aralığı (16 V'ta 1472–1528 µs) **doğrulanmadı**; ESC'nin komut ölü bandının yerine geçmez (11.6, Ç12). ESC açılışta birkaç saniye nötr komut ister (D11 #6); `esc_başlatma_süresi = 2 s` mimari varsayımıdır ("birkaç saniye"nin alt ucu).

**2.7.2 Dinamik.** von Benzon'un birim DC kazançlı üçüncü derece transfer fonksiyonu normalize komuta uygulanır: `K(s) = (6136 s + 108700)/(s³ + 89 s² + 9258 s + 108700)` (D04 #1 DOĞRU; kutuplar `−13,16` ve `−37,92 ± 82,60i`, 04.2). Durumları RK4 durum vektörüne girer. ESC'nin kendi rampa gecikmesi yayımlanmadı (11 §2.B.7).

**2.7.3 Durağan itki eğrisi.** Doğrulanmış olan yalnız uç değerlerdir: azami ileri/geri itki 12 V'ta 3,71/2,92 kgf, 16 V'ta 5,25/4,1 kgf, 20 V'ta 6,7/5,05 kgf; tam gaz akımı 17/24/32 A (D04 #3, D11 #1). Ham tablo (PWM × V; RPM, akım, kuvvet) iki doğrulamada da okunamadı; 11 §2.B.7 tablosu **aday değerdir** (Ç12). Bu nedenle:

- **Birincil (M22 sonrası):** üreticinin ham tablosundan (PWM, V) iki boyutlu enterpolasyon; itki ve akım aynı tablodan.
- **Geçici model (M22'ye kadar):** şekil von Benzon itki polinomundan (`F(V) = −140,3V⁹ + 389,9V⁷ − 404,1V⁵ + 176,0V³ + 8,9V`, normalize giriş, 04 §2.5), ölçek doğrulanmış uç değerlerden: `F = F_şekil(u_f) · F_maks,yön(V_bara)`; `F_şekil = F(u)/max F`, ileri ve geri ayrı ölçeklenir (T200 asimetrik). Polinom `u ≈ 0,97` ile `1` arasında hafifçe azalır (bu bölümde hesaplandı, < %1); şekil monoton kırpılır. `F_maks(V)` doğrulanmış noktalar arasında kuvvet yasasıyla enterpole edilir (üsler 12→16 V 1,207; 16→20 V 1,093; D04 Y12; genel pervane yasası değildir). 12 V altı aynı üsle dışdeğerlenir ve "dışdeğerleme" bayrağı kaldırılır (mimari varsayımı). Akım: `I = I_maks(V) · (|F| / F_maks,yön(V))^{3/2}` (mimari varsayımı; gerekçe: ideal itki diski kuramında güç itkinin 3/2 kuvvetiyle artar; ham tablo okununca kalkar), `I_maks(V)` 17/24/32 A arasında doğrusal. Ölü bantta akım 0; ESC boşta tüketimi aviyonik yüküne dahildir (11 §2.B.7).
- İlerleme oranına bağlı itki düşüşü `K_T(J)` için T200 eğrisi yok (04 §2.4); ilk sürümde bollard itkisi kullanılır, A5'te `K_T(J)` tablosu için yer ayrılır. İtici–gövde ve itici–itici etkileşimi modellenmez (04 §4 md.5).

**2.7.4 ESC davranışları ve arıza kancaları.** 30 A sürekli çalışma derecesidir, akım sınırı ya da kesme eşiği değildir (11.7); model bu değerde kısma yapmaz. 7 V alt sınırı kesme eşiği değildir; gerilim çökerken kapanma ve yeniden başlama davranışı bilinmiyor (11.7, M23). Model şu durumları parametreyle taşır: frenleme ve hızlı yön değiştirme, başarısız kalkışta nötr komut bekleme (BLHeli_S belgeli davranış, 11.17), PWM kaybında `nötr` ya da `son_değer` (`pwm_kaybı_davranışı`, doğrulanmadı, 11 §2.B.7), ısıl kısma kancası (ESC sıcaklık modeli ilk sürümde yok; mimari varsayımı: ısıl veri yok, kısma yalnız arıza enjeksiyonuyla). T200'ün içinde su olması normaldir; "su alma" arızası tanımlanmaz, yalıtım hasarı ve kuru muhafaza sızıntısı ayrı olaylardır (11.8). Arıza kancaları: itki çarpanı, tutukluk (itki 0, akım parametresi), dolanma, ESC sinyal kaybı. Arızanın zaman çizelgesi 08'in A5 biçimindedir.

**2.7.5 Batarya.** Tek kollu Thevenin eşdeğer devresi (D11 #13 DOĞRU):

```
V_t = OCV(SoC, T) − I R₀(SoC, T) − V₁,      dV₁/dt = I/C₁ − V₁/(R₁C₁),      dSoC/dt = −I/Q
```

Hücre parametreleri: Samsung 18650-30Q için X-57 çalışmasının eklerindeki OCV, R₀ ve R_Th haritaları (0/20/30/45 °C), C_Th = 2000 F; ör. 20 °C'de haritanın geniş bölümünde R₀ = 0,025 Ω (D11 E:30Q; 11.11). Bu veri paket ölçümünün yerine geçmez ve 0 °C altını doğrulamaz. Haritalar bu bölüme aktarılmadı; A5'e kaynağından aktarılana kadar yükleyici B1 modelini reddeder ve yalnız sabit gerilimli "ideal kaynak" kipine izin verir. Chen'in 30 mV doğruluğu iki RC'li modele aittir, bu modele aktarılmaz (11.10).

| Paket parametresi | Değer | Kaynak | Nitelik |
|---|---|---|---|
| Referans paket | 4S, 18 Ah, 266,4 Wh; 60 A sürekli, 132 A / 10 s; 12 V asgari güvenli gerilim | Blue Robotics üretici tablosu (D11 Y9) | yayımlanmış |
| Hücre düzeni | 4S6P 30Q | 18 Ah'yi 30Q hücreyle eşlemek için | mimari varsayımı (hücre kapasitesi X-57'den doğrulanmalı) |
| Paket R₀ | hücre R₀ × 4/6 + kablo direnci | seri/paralel düzen | mimari varsayımı (düzen) |
| Kablo + bağlantı direnci | 0,01 Ω | yer tutucu | mimari varsayımı |
| Aviyonik + sensör yükü | 20 W sabit güç | yer tutucu; ölçüm yok | mimari varsayımı |
| Batarya sıcaklığı | su sıcaklığına eşit, sabit | ısıl model ilk sürümde yok | mimari varsayımı |

60 A ve 132 A / 10 s değerleri model tarafından kesme olarak uygulanmaz; aşıldığında olay üretilir. Akım ölçer (PSM) modeli 05'tedir; PSM'nin 100 A değeri sürekli değildir (11.12).

**2.7.6 Elektrik–itki bağlaşımı.** Bara gerilimi itici akımlarına, akımlar gerilime bağlıdır: `V_bara = OCV − V₁ − R_top · Σ I_i(V_bara) − …`. Bir adım gecikmeli çözüm koşulsuz kararlı değildir; basit yinelemenin yerel yakınsama koşulu `|R · dI/dV| < 1` (11.18, doğrudan türetim). Birincil: her fizik adımının başında adım içi sabit nokta yinelemesi, en çok 8 yineleme, kalıntı eşiği 1 mV (mimari varsayımı: 1 mV ADC ölçüm çözünürlüğünün altında kalsın diye). Yinelemede `|R · dI/dV|` tahmini kaydedilir; 1'e yaklaşırsa ya da kalıntı eşiği aşılırsa olay ve sayaç (Bölüm 4). Yedek: bir adım gecikmeli bağlaşım ve aynı denetim (M26). Bulunan `V_bara` adım boyunca sabittir.

**2.7.7 Güç olayları.** `bara_bağlı` bayrağı (acil kesme / sigorta: V_bara = 0, itki 0, geri gelişte ESC başlatma süresi), sigorta I²t (eşik A5'te), "kart resetlendi" olayı. HIL'de kart USB'den beslendiği için gerçek brownout oluşmaz; sanal batarya çökmesi, kart reseti ve gerçek besleme çökmesi üç ayrı testtir ve BOR eşikleri batarya hattına değil MCU beslemesine aittir (11.19). Çekirdek yalnız olayı üretir; karta iletimi 06'dan beklenen.

### 2.8 Torpido ve hedefler

**Torpido gövdesi.** Aynı integratörü kullanan ayrı 6-SD gövde; kendi `M_RB`, `M_A`, `D`, yüzerlik ve kanat modeli vardır (04 §2.7). Eklenmiş kütle Lamb k-faktörleriyle, **yer değiştiren sıvı kütlesi** ölçeğiyle hesaplanır (04.6). Geometri parametriktir; yarışma kuralları kullanılmaz (K7). Varsayılan torpido (hepsi A5'te):

| Parametre | Varsayılan | Nitelik ve gerekçe |
|---|---|---|
| Boy / çap | 0,20 m / 0,04 m (L/D = 5) | mimari varsayımı: 04 §2.5'te hesaplanmış L/D = 5 k-faktörleri kullanılabilsin diye |
| Hacim / kütle | 2,51·10⁻⁴ m³ / 0,26 kg | mimari varsayımı: hafif negatif yüzerlik (04 §2.7 "hafif negatif veya nötr") |
| k₁, k₂, k′ | 0,059 / 0,894 / 0,700 | 04 §2.5 türetimi (Lamb, L/D = 5); D04 itiraz etmedi |
| `C_D` (cephe alanına göre) | 0,42 | mimari varsayımı: REMUS değeri (04.8) yalnız yer tutucu; yeni geometrinin katsayısını belirlemez; 0,3–1,0 aralığı doğrulanmadı ve tolerans olarak kullanılmaz |
| Çıkış hızı `v₀` (bağıl ayrılma hızı) | 2 m/s | mimari varsayımı: 04.7'deki örnek değer |
| Tüp boyu | 0,20 m | mimari varsayımı: 04.7 örneği |
| Fırlatıcı konumları | (0,10, ±0,10, 0,10) m, eksen +x | mimari varsayımı: gövdenin altı, ön yarı |

Sürükleme `X = −½ ρ C_D A_ön u|u|` (04.8); çapraz akış sönümü şerit integrali ve Hoerner katsayılarıyla (04 §2.7). Kanat kuvvetleri yerel bağıl akışa göre (PVS `forceLiftDrag` biçimi, 04 §2.4).

**Kararlılık koşulu.** Yükleyici statik yön kararlılığını hesaplar: `½ ρ S_f C_Lα |x_f| > A₂₂ − A₁₁` (04.11; DOĞRU ama yalnız küçük açı, sabit katsayı, sıfır yaw hızı ve simetrik gövdede statik moment koşulu). Varsayılan gövde için `A₂₂ − A₁₁ = (k₂ − k₁) ρ∇ ≈ 0,21 kg`. Ayrıca doğrusal sway–yaw sisteminin özdeğerleri denetlenir (`tr(A) < 0`, `det(A) > 0`; 04.11). Statik marj oranı (`sol/sağ`) 1,5'in altındaysa yükleyici uyarır (mimari varsayımı: değer kanat varsayılanlarının seçiminde hedeftir, tolerans değildir). PVS `torpedo.py` Munk momentini sıfırladığı için aynen taşınmaz (04.11).

**Fırlatma (çok gövdeli olay, 04.7).** `v₀` atış sonrası **bağıl ayrılma hızıdır**. Araç ve torpido birlikte çözülür; aracın namlu noktasındaki etkin ters kütlesi dönmeyi de içerir:

```
w = [ε ; r_n × ε]                        (ε: tüp ekseni, r_n: namlu noktası, gövde çerçevesi)
1/M_e = wᵀ M⁻¹ w                         (M = M_RB + M_A, araç)
1/m_e = 1/(m_t + A₁₁,t + m_su,tüp)       (m_su,tüp: tüpte ivmelenen su, A5; varsayılan 0)
J = v₀ / (1/M_e + 1/m_e)                 (tek eksende J = M_e m_e/(M_e + m_e) · v₀ ile aynı)
```

`J = m_e v₀` yalnız geri tepmenin ihmal edildiği sınırdır (04.7). Torpido, namlu noktası hızı `v + ω × r_n` ile başlar. Darbe tek adıma sıkıştırılmaz: sabit ivme kabulüyle süre `t = 2L/v₀` (varsayılanlarla 200 ms, 04.7) boyunca eşit kuvvet olarak iki gövdeye zıt yönde uygulanır; bu sürede torpido tüp ekseni dışında araca bağlı (kinematik kılavuz) hareket eder (mimari varsayımı: mekanizma bilinmiyor, en basit tutarlı model). Çıkışta araç kütlesi, CG, atalet ve deplasmanı güncellenir (04.17). Etkin su kütlesi modeli A5'te açıkça kaydedilir (04.7). Fırlatıcı türleri: `darbe` (yay benzeri) ve `sürekli_itki` (motorlu nesne; itki ve süre A5'te); sürekli itkili nesne ayrı model ister (00 Bölüm 6). Namlu su üstündeyse olay üretilir ve atış "model dışı" işaretlenir: suya giriş fiziği yok (04 §2.7).

**Hedefler.** Statik ya da senaryo yörüngesini izleyen kinematik gövdeler; şekilleri A7'den. Hedefin dinamiği yoktur (ilk sürüm).

**İsabet tespiti.** Her adımda torpidonun o adımdaki hareket parçası boyunca şekil süpürmesi (Jolt `CastShape`) yapılır; küçük ve hızlı nesnede ayrık sorgu tünelleme yapar (12 §2.8). İsabet olayı alanları: `t_ns, torpido_id, hedef_id, nokta_n, normal_n, bağıl_hız_n`. İsabetten sonra torpido durdurulur ve dünyadan çıkarılır (A5'te `sek` seçeneği sonraya). Sürekli çarpışma gereksinimi açık sorudur (A25).

### 2.9 Çarpışma ve temas

**Kütüphane.** Jolt Physics v5.6.0, MIT, **yalnız sorgu** olarak (Ö9, öneri; D12 #6a). Jolt'un dinamik çözücüsü adımlanmaz (`PhysicsSystem::Update` çağrılmaz); havuz duvarları, taban, engeller ve hedefler statik ya da kinematik şekillerdir; araç şekli A7'de çevrimdışı üretilmiş dışbükey parçalardan bileşik şekildir (CoACD, 12 §2.7). Sürüm sabitlenir (D12 #15).

**Sorgu.** Her fizik adımının başında araç ve torpidolar için `NarrowPhaseQuery::CollideShape`. Koordinat uyarıları (12.7, D12 E:Jolt temas verisi):

- `CollideShape` gövde kökenini değil **şeklin kütle merkezi dönüşümünü** ister; dönüşüm sarmalayıcıda tek yerde yapılır.
- Temas noktaları taban ofsetine göre dönebilir; sarmalayıcı dünya NED koordinatına çevirir.
- Penetrasyon ekseni dünya uzayındadır ve **büyüklüğü anlamsızdır**; yalnız yön olarak normalleştirilip kullanılır, derinlik `mPenetrationDepth`'ten alınır.
- Sonuç sırası garanti değildir; sonuçlar (gövde kimliği, alt şekil kimliği, derinlik) anahtarıyla sıralanır (12 §2.8).
- Statik sensör kısıtı ve sensör olayları sorgu düzeninde kendiliğinden üretilmez (12.6); olaylar sorgu sonuçlarından çekirdekte türetilir.

**Tepki (özel).** Temas noktası başına normal kuvvet `F_n = max(0, k_n d + c_n v_n)`, Coulomb sürtünme `|F_t| ≤ μ F_n`; gövde çerçevesine dönüştürülüp `τ_temas = [Rᵀ F ; (r_c − r_ref) × Rᵀ F]` olarak denkleme girer (12 §2.8 md.3). Eklenmiş kütle Fossen tarafında olduğundan kütüphanenin kütlesine gerek yoktur. Kararlılık için `ω_c · dt ≤ 0,2`, `ω_c = √(k_n / m_eff)` sınırı uygulanır (mimari varsayımı: açık yöntemde yay frekansını adımın çok altında tutmak için seçildi; M15'te sınanır). İDA-0 varsayılanları: `k_n = 1·10⁵ N/m` (`m_eff = m + A₃₃ = 32,18 kg` ile `ω_c dt ≈ 0,056`), `c_n` kritik sönüm (`2√(k_n m_eff) ≈ 3,6·10³ N·s/m`), `μ = 0,5`. Üçü de mimari varsayımıdır (malzeme verisi yok). Temas kuvveti adım boyunca sabit tutulur; bu, temas sırasında RK4 mertebesini düşürür (04 §2.6 "süreksizlikler"); gerekirse temas alt adımlaması yedektir (Bölüm 6).

**Determinizm.** `CROSS_PLATFORM_DETERMINISTIC` açık, tek iş parçacıklı iş sistemi, sabit FP bayrakları (`-ffp-contract=off`, `/fp:precise`; 12 §2.8). Bu Jolt'un kendi kapsamı içindir; bütün simülatörün platformlar arası bit eşitliğini sağlamaz (Ç7 güncellemesi).

### 2.10 Sayısal çözüm

- **İntegratör:** klasik RK4, sabit `dt = 1 ms` (iskelet §4; D04 N3). Kararlılık gerekçesi yalnız incelenen doğrusal modlar içindir: itici kutuplarında `|R(hλ)| ≈ 0,98693 / 0,96279`, surge ve roll yerel zaman sabitleri 91,6 ms ve 37,7 ms (04.2). Bu değerler değişken batıklık, temas ve bağlaşımlı sistem için kararlılık ispatı değildir; kararlılık ile doğruluk ayrı koşullardır (04.2). Karşılaştırma: aynı itici modelinde ileri Euler'in sınırı 9,18 ms'dir; 20 ms'de kararsızdır (04.2).
- **İvme çözümü:** `ν̇ = M⁻¹(…)`, Cholesky ile; `M` sabitse ayrıştırma bir kez yapılır, yalnız torpido ayrılmasında yenilenir (04 §2.6).
- **Yönelim:** RK4 aşamalarında `q̇ = ½ q ⊗ [0, ω]`, adım sonunda normalleştirme (04 §2.6). Yedek: hızlar RK4 ile, yönelim üstel haritayla (MSS yaklaşımı).
- **Çok oranlı güncelleme:** birincil çözümde hidrostatik ve dalga kuvvetleri her RK4 aşamasında yeniden hesaplanır. Kuvvetleri 200–250 Hz'de hesaplayıp aşamalarda sabit tutmak tekrar üretilebilir olsa da tam sistemin RK4 mertebesini düşürebilir; 16 kat yakınsama beklenmemeli (04.16). Bu yalnız M15 ölçümü gerektirirse yedek olarak açılır (Bölüm 6). Sabit tutulan terimler: aktüatör komutu, bara gerilimi, temas kuvveti (adım başında hesaplanır; bilinçli sıfırıncı derece tutma).
- **Süreksizlikler:** ölü bant, doyum, ağ kırpması ve temas türevi süreksiz terimlerdir; mertebeyi düşürür. Olay tespiti yapılmaz; sabit `dt` ile kabul edilir, gerekiyorsa smoothstep yumuşatma (04 §2.6).
- **Kararlılık hesabının sınırları:** sert alt sistemler (temas, ileride halat) eklenirse alt adımlama ya da yarı örtük yöntem gerekir (04 §2.6). Hesap yükü ölçülmedi; 500 köşe × 64 bileşen × 1000 Hz ≈ 3,2·10⁷ `cos`/s işlem sayısı bir tahmindir, süre karşılığı doğrulanmadı (04.16, M15).

### 2.11 Zaman yöneticisi

- **Saat:** sim zamanı `int64` nanosaniye; çekirdek duvar saati okumaz (09 §2.4). `World::step()` yalnız bir sonraki adıma ilerler. Duvar saati sürücülere aittir; iki saat alanı karışmaz (iskelet §4).
- **Zamanlayıcı:** her görev `{periyot_pay_ns, periyot_payda, faz_ns}` ile rasyonel periyotla kaydedilir; n. olayın ideal zamanı tamsayı aritmetiğiyle `faz + ⌊n · pay / payda⌋` olarak hesaplanır, birikimli kayma olmaz. Olay, ideal zamana eşit ya da ondan sonraki ilk fizik adımında çalışır; örnek o adımın zaman damgasını taşır. 30 Hz kamera (33⅓ ms) 33/34 adım dönüşümüyle ifade edilir (00 Bölüm 6, D09 E). Aynı adımdaki görevlerin sırası kayıt sırasına göre sabittir; aynı sim zamanındaki sensör örneklerinin karta sırası 06'nın sözleşmesidir (00 Bölüm 6).
- **Sensörlerin kendi hızları:** her sensör eklentisi (05) kendi periyodu ve fazıyla zamanlayıcıya kaydolur; ölçüm zamanı ile teslim zamanı ayrıdır, teslim gecikmesi 05'in modelidir (00 Bölüm 6).
- **Üç kip, tek çekirdek:** çekirdek kipten habersizdir; kipi sürücü belirler (09 §2.4, Ö8).

| Kip | Sürücü | Davranış |
|---|---|---|
| SIL | `FreeRunDriver(hız, duraklat)` | sanal zaman; hızlandırılabilir, duraklatılabilir; ağır modeller beklenebilir |
| HIL 1x | `RealtimeDriver` | adım n'in son tarihi `T₀ + (n+1)·dt` (duvar saati); fizik adımı asla atlanmaz |
| HIL 1x + adım bariyeri | `LockstepDriver` | sensör çerçevesinden sonra aynı sıra numaralı aktüatör yanıtını bekler; bariyer protokolü 06'nın |

- **Kaçırılan adım sayacı:** gerçek zamanlı iki kipte bir adım son tarihinden tolerans kadar geç biterse sayaç artar (tolerans A5'te; varsayılan `1 dt`, mimari varsayımı: tek adımlık gecikme kartın 400 Hz döngüsünde gözlenebilir düzeydedir). Bariyerde yanıt bütçe içinde gelmezse sim durmaz: son komutla ilerler, sayaç artar (iskelet §4, Ö11). Sayaç sıfırdan büyükse HIL çalıştırması geçersiz işaretlenir ve A3'ten bildirilir (Ö11). Fizik adımları atlanmaz; geride kalan sim zamanı duvar saatinin gerisine düşer (tekrar üretilebilirlik bunu gerektirir).

### 2.12 İş parçacığı düzeni

- **Dünya iş parçacığı (tek):** zaman yöneticisi, çevre, dinamik, hidro, aktüatör, güç, temas sorgusu, sensör eklentilerinin CPU tarafı. Bütün indirgemeler sabit sırayla; paralel indirgeme yok (04 §2.6, 09 §2.4).
- **İşçi havuzu (ağır işler):** sonar ışın izleme (Embree, 05) gibi ağır sensör işleri. Dünya iş parçacığı işi sabit bir sim zamanında gönderir; sonuç, modellenmiş teslim zamanında ve sabit sırada tüketilir. SIL'de sonuç hazır değilse beklenir; gerçek zamanlı HIL'de geç sonuç için düşürme ya da bayat veri politikası 05 ve 06'dan beklenen (00 Bölüm 6, D09 E).
- **Diğer iş parçacıkları:** HIL G/Ç (06), kayıt yazıcısı (08), WebSocket ve HTTP (08), görselleştirici yayını (07). Dünya ile aralarında sınırlı, tek üretici–tek tüketici halka tamponları; hızlı yolda kilit yok (09 §2.4). Dünya iş parçacığı bunlardan yalnız bariyer kipinde aktüatör yanıtını bekler.
- Monte Carlo ve regresyon için bağımsız dünyalar paralel koşabilir (09 §2.4).

### 2.13 Tekrar üretilebilirlik

- **Hedef (Ç7):** aynı ikili, aynı platform, aynı girişle bit düzeyinde tekrar; macOS-arm64 ile Windows-x64 arasında toleranslı eşdeğerlik. Platformlar arası bit eşitliği garanti edilmez.
- **Kayan nokta politikası:** fizik `double` (Ö3); `-ffp-contract=off` (clang), `/fp:precise` (MSVC; VS 2022'de daralma kapalı), hiçbir yerde fast-math (09 §3.1). Daralma varsayılanları derleyici sürümüne bağlıdır; bayraklar açıkça verilir (09.6). Referans derlemede `EIGEN_DONT_VECTORIZE` ile birlikte `EIGEN_FAST_MATH` de kapatılır (09.7). Aşkın işlevler tek `ida_math` katmanından çağrılır; başlangıçta `std::`, platform farkı ölçülürse sürümlü kendi uygulama (09 §2.3). Fizik yolunda sırasız kap yinelemesi yok (04 §4 md.6).
- **Rastgele sayı:** sayaç tabanlı Philox (09.4: Philox sayaç tabanlıdır; PCG değildir); akış kimliği `(senaryo_tohumu, alt_sistem, örnek)` ile türetilir, böylece bir alt sistemin çektiği sayı diğerini kaydırmaz. `<random>` dağılımları kullanılmaz; normal dağılım kendi kodumuzdadır ve algoritma sürümü kayda yazılır. Kendi Box–Muller kodu da `log`, `sqrt` ve trigonometri nedeniyle platformlar arası bit eşitliği vermez (09.4). Fizikte RNG kullananlar: dalga fazları ve frekansları, akıntı ve rüzgar Gauss–Markov süreçleri.
- **Kayda girenler:** tohum, RNG ve dağılım sürümü, parametre dosyasının özeti, derleyici ve bayraklar, bağımlılık sürümleri (Eigen, Jolt), dalga listesi özeti (A6, 08'den beklenen).

---

## 3. Arayüzler

Biçimler C++ imza ve alan listesi düzeyindedir; sözleşmeler ayrı işte dondurulur (iskelet §3).

**Çekirdeğin kendi yüzü**

```
void       World::reset(const ScenarioConfig& cfg, uint64_t seed);   // A5 içeriği 08'in ayrıştırıcısından
StepResult World::step();                                          // tam bir dt; StepResult: t_ns, adım_no, olaylar, sayaçlar
void       World::apply_actuators(const ActuatorFrame& f);          // bir sonraki step() başında uygulanır
void       World::apply_fault(const FaultSpec& f);                 // arıza kancaları (2.7.4, 2.7.7)
const WorldSnapshot& World::snapshot() const;                      // salt okunur, adım sonu
```

**ActuatorFrame** (06'dan beklenen, A1): `seq`, `t_sim_ns` (uygulanacağı adım), `pwm_us[8]`, `fırlatıcı_komutu[2]` (`hazır`, `ateşle`, `emniyet`), `geçerlilik_maskesi`.

**WorldSnapshot** (sensörlere 05, kayda 08, panellere 08): `t_ns`, `adım_no`; gövde başına `id, tür, p_n, q_nb, ν, ν̇, s (batıklık), ∇_batık, r_b`; itici başına `u_komut, u_filtreli, F, I, β_havalanma`; güç `V_bara, I_paket, V₁, SoC, sıcaklık, bara_bağlı`; temas listesi `{gövde_id, nokta_n, normal_n, derinlik}`; fırlatıcı durumları; olaylar; sayaçlar (`kaçırılan_adım`, `elektrik_yakınsamadı`, `kip_geçişi`).

**IEnvironmentView** (05 sensörleri ve 07 karşılaştırma testi için; yalnız okuma, her sim zamanı için deterministik):

```
double surface_elevation(double x_n, double y_n, int64_t t_ns) const;   // ζ, ortalama yüzeye göre, yukarı pozitif
Vec3   water_velocity(const Vec3& p_n, int64_t t_ns) const;             // akıntı + dalga, NED
Vec3   water_acceleration(const Vec3& p_n, int64_t t_ns) const;
double water_density() const;   double water_temperature() const;   double depth_at(double x_n, double y_n) const;
Vec3   wind_velocity(int64_t t_ns) const;
```

**Zamanlayıcı ve sensör eklentisi** (05 ile sınır): `TaskId Scheduler::add(uint64_t period_num_ns, uint32_t period_den, int64_t phase_ns, Callback cb)`. Eklenti arayüzünün kendisi (`ISensorModel`) 05'te tanımlanır; çekirdek ona `WorldSnapshot`, `IEnvironmentView` ve sabit RNG akış kimliği verir. Sonar ışın izlemesi için gereken sahne geometrisi A7'den gelir; çekirdek yalnız gövde pozlarını sağlar.

**Görselleştiriciye (07, A2):**
- `VizState` (her yayında): `t_ns`, gövde başına `id, p_n, q_nb`, fırlatıcı ve torpido durumları, `dalga_listesi_sürümü`.
- `WaveSpectrumSet` (senaryo başında ve değiştiğinde): `sürüm, özet, tohum, g, ρ, h, t₀ konvansiyonu, faz işaret konvansiyonu, bileşenler[]{a, ω, k, β, φ}`. Yayın hızı ve taşıma 07'den beklenen.
- Çekirdek, 07'nin prob noktası testine referans yükseklik verir (`surface_elevation`).

**Kayda ve panellere (08, A3/A6):** `IRecorderSink::on_step(const WorldSnapshot&)`, `on_event(const Event&)`; çalıştırma geçerlilik bayrağı ve sayaçlar. Hangi alanların hangi kademede kaydedildiği 08'in kararıdır.

**Senaryo ve araç tanımı (A5, biçim 08'de):** çekirdeğin beklediği gruplar — `ortam` (tür, ρ, h, sıcaklık, jeodezik başlangıç), `dalga` (spektrum, H_s, T_p, γ, yayılım, bileşen sayısı), `akıntı`, `rüzgar`, `araç` (2.6'daki tablolar, işaret kuralı, belirsizlik), `itici[8]`, `esc`, `batarya`, `torpido`, `fırlatıcı[2]`, `hedefler[]`, `temas`, `sayısal` (dt, M_A kipi, çok oranlı kip), `tohum`, `arızalar[]`.

**Varlıklar (A7):** `BUOY_*` (kapalı, manifold yüzerlik ağları), `COL_*` (dışbükey çarpışma parçaları), itici ve fırlatıcı bağlantı düğümleri, manifest özeti. Yükleyici manifest özetini görselleştiricininkiyle karşılaştırır (12 §2.7; manifest içeriği 07'den beklenen).

---

## 4. Hata durumları ve davranışı

| Durum | Tespit | Davranış |
|---|---|---|
| Parametre hatası (M simetrik/pozitif tanımlı değil, birim/işaret eksik, B1 haritası boş) | yüklemede | senaryo reddedilir, alan adıyla hata |
| Yüzerlik ağı açık / manifold değil; tam batık hacim ya da merkezi tabloyla tutarsız | yüklemede | reddedilir; tutarsızlık eşiği A5'te (varsayılan hacimde %1, merkezde 2 mm; mimari varsayımı) |
| Torpido statik kararlılık marjı < 1,5 ya da özdeğer koşulu sağlanmıyor | yüklemede | uyarı (reddetmez; kararsız torpido deneyi meşrudur) |
| NaN / sonsuz durum | her adım | çalıştırma durur, son iki adımın durumu ve girişleri kayda yazılır, çalıştırma geçersiz |
| Quaternion normu sapması > 1e-9 normalleştirmeden önce | her adım | olay (integratör sorununun göstergesi) |
| Elektrik bağlaşımı yakınsamadı ya da `|R·dI/dV|` ≥ 0,9 | her adım | son yinelemeyle devam, olay ve sayaç |
| Temas derinliği > eşik (varsayılan 5 cm; mimari varsayımı) | her adım | olay; tünelleme ya da temas parametresi sorunu |
| Torpido alan dışına çıktı ya da 120 s'yi aştı (mimari varsayımı) | her adım | dünyadan çıkarılır, olay |
| Su üstünde atış | fırlatma anı | atış yapılır, "model dışı" olayı |
| Kaçırılan adım / bariyer zaman aşımı | sürücü | 2.11; HIL'de çalıştırma geçersiz |
| Dalga listesi sürümü görselleştiricinin bildirdiğiyle uyuşmuyor | A2 el sıkışması | olay; görüntü tutarsız işaretlenir (fizik etkilenmez) |
| Görselleştirici ya da kayıt tamponu dolu | halka tampon | dünya beklemez; kayıp sayacı (Godot kaybı fiziği durdurmaz, ama kamera kullanan deneyin geçerliliği 06/07'nin kararıdır; 12.17) |

---

## 5. Kalite kademelerinin etkisi

İskelet §6 gereği fizik her kademede tam çalışır: bileşen listesi, ağ kırpması, itici ve güç modeli, temas ve 1 ms RK4 kademeden bağımsızdır. Kademeler çekirdeği yalnız dolaylı etkiler:

- **3B görünüm "kapalı" ya da "sade":** görselleştirici dalga listesini kullanmaz ya da düz su çizer; fizik aynı listeyle çalışır. Tutarlılık testi yalnız "tam"da anlamlıdır.
- **Kayıt kademesi:** `WorldSnapshot` hangi sıklıkla ve hangi alanlarla kayda gider (08).
- **Sonar kademesi:** işçi havuzundaki iş miktarı (05); dünya iş parçacığının adım süresini etkilemez, teslim zamanlamasını etkileyebilir.
- Düşük donanımlı Windows'ta 1x gerçek zamanın tutulup tutulamayacağı ölçülmedi; tutulamazsa sayaç artar ve çalıştırma geçersiz olur (Ö11). Fizik doğruluğunu düşüren seçenekler (çok oranlı kuvvet, su yaması ızgarası) kademe değil, Bölüm 6'daki ölçüme bağlı yedeklerdir (iskeletle ilişkisi Bölüm 8'de).

---

## 6. Ölçüme bağlı kararlar

| Karar | Birincil | Yedek | Ölçüm |
|---|---|---|---|
| İntegratör ve adım | RK4, 1 ms, bütün kuvvetler her aşamada | temas alt adımlaması; 0,5 ms | M15 |
| Hidrostatik/dalga kuvvetinin hesap sıklığı | her RK4 aşamasında, 64 bileşen, tam ağ | adım başına sıfırıncı derece tutma ya da 200–250 Hz (mertebe kaybı ölçülerek); araç çevresinde dalga yaması ızgarası + bilineer enterpolasyon (04 §3.3) | M15 (hesap süresi ve yakınsama) |
| Yüzey geçişinde eklenmiş kütle | sabit `M_A` | `M_A(s)` C¹ ölçekleme | M15 (enerji/momentum), M16 |
| Nokta modeline geçiş eşiği | `d ≥ (λ_max/2π) ln(1/ε)`, ε = 0,01 | daima ağ | M15 |
| İtici durağan modeli | ham T200 tablosu (PWM × V) | yayımlanmış uç değerler + von Benzon şekli (2.7.3) | M22, M16 |
| ESC davranışı (frenleme, tutukluk, PWM kaybı, gerilim çökmesi) | parametreli durum makinesi | yalnız durağan model | M23 |
| Elektrik bağlaşımı | adım içi sabit nokta yinelemesi | bir adım gecikmeli + `|R·dI/dV|` denetimi | M26 |
| Batarya modeli | 1-RC Thevenin, 30Q X-57 haritaları | sabit R₀ (B0) | M26 |
| Çarpışma sorgusu | Jolt v5.6.0 yalnız sorgu | havuz için kutu/düzlem ilkel şekilleri ve kendi sorgumuz | M31 |
| Torpido sürüklemesi ve fırlatma | 2.8 varsayılanları | — (ölçümle değiştirilir: düşürme testinde uç hız, pasif fırlatmada yavaşlama eğrisi, mekanizma süresi) | M16 |
| Platformlar arası eşdeğerlik toleransı | 1e-6 bağıl, kısa ufukta (09 §2.3 örneği) | ilk ıraksayan adım raporuyla senaryo başına tolerans | M10 |

---

## 7. Test yaklaşımı ve kabul ölçütleri

Bütün sayısal eşikler, aksi belirtilmedikçe mimari varsayımıdır ve 09 bölümünde kesinleşir.

**Analitik çözümlü durumlar**

1. **Çarpık simetri:** rastgele ν ve simetrik pozitif tanımlı M için `|νᵀC(ν)ν| < 1e-12 · |ν|²·‖M‖` (04 §2.8).
2. **Enerji:** `D = 0`, yerçekimi ve yüzerlik kapalı, `τ = 0` iken `½νᵀMν`'nin 600 s'deki bağıl sürüklenmesi < 1e-6; Munk kararsızlığı torpido gövdesinde nitel olarak gözlenir (04 §2.8).
3. **Hidrostatik denge:** analitik yüzme derinliği bilinen kutu ve silindir; İDA-0'nın yüzeyde su üstünde kalan hacmi `(ρ∇ − m)/ρ` (± ağ çözünürlüğü).
4. **Klasik modelle özdeşlik:** düz su, tam batık araçta ağ integrali ile klasik vektör biçimli `g(η)` farkı < 1e-9 bağıl (Hunt–Whitcomb'un 1e-11 sonucu doğrulanmadığı için kendi eşiğimiz; 04.14).
5. **Roll salınımı (tam batık):** `T = 2π√((I_x + K_ṗ)/(W·BG))`; İDA-0'da `W·BG = 13,5·9,82·0,01 = 1,326 N·m`, `T ≈ 3,66 s` (sönüm kapalı, küçük genlik). Yüzeyde heave: `ω₃ = √(ρgA_wp/(m + A₃₃))`.
6. **Uç hız:** dış kuvvet doğrudan uygulanarak `X_u|u|u² + X_u u = T`. 85 N → 0,729 m/s; sway 85 N → 0,626 m/s; heave 120 N → 0,713 m/s (bu bölümde yeniden hesaplandı); von Benzon Tablo 4'teki 0,72 / 0,63 / 0,71 m/s ile uyum (04 §2.5; D04 #1). Bu test itici yerleşiminden bağımsızdır.
7. **Serbest sönüm:** doğrusal sönümde analitik zarf; karesel sönümde enerji–genlik ilişkisi.
8. **Dalga:** üretilen `ζ(t)` için Welch spektrumu `S(ω)` ile karşılaştırılır, `H_s ≈ 4√m₀`; sonlu derinlik fonksiyonları ve dispersiyon analitik değerlerle; ivmeli akışta eklenmiş kütle ve FK'nin tek sayıldığı tek eksenli test: `(m + A) v̇ = (ρ∇ + A) u̇_w` (04.5).
9. **Fırlatma:** sürtünmesiz, durgun suda araç + torpido toplam (eklenmiş kütleli) momentumu korunur; ayrılma hızı `v₀`'a eşit; pasif yatay uçuşta `1/u(t) = 1/u₀ + (c/m_e)t` (04.8); kanatlar kaldırılınca statik koşul ve simülasyon birlikte kararsızlık gösterir.
10. **Batarya:** dengelenmiş 1-RC'ye sabit akım basamağında `ΔV(t)/I = R₀ + R₁(1 − e^{−t/(R₁C₁)})` (11.10).
11. **Temas:** tabanda duran araçta penetrasyon kararlı, enerji artmaz; düşürme testinde mekanik enerji tekdüze azalır.

**Sayısal doğrulama:** `dt` yarıya inince düzgün bölgede hata ≈ 16 kat azalır; temas, kırpma ve ölü bant içeren senaryolarda mertebe raporlanır, kabul ölçütü yapılmaz (04.16, M15). Çok oranlı yedek açılırsa aynı test mertebe kaybını ölçer.

**Tekrar üretilebilirlik:** aynı platformda iki koşuda her N adımda durum özeti bit düzeyinde eşit; macOS-arm64 ve Windows-x64'te aynı altın iz, bağıl tolerans bandı ve ilk ıraksayan adım raporu; derleyici sürümü değişince altın iz yeniden üretilir (09 §2.3, Ç7).

**Yayımlanmış referanslarla karşılaştırma:** PVS/MSS `remus100` ve `otter` aynı parametre ve girişlerle (PVS ileri Euler kullandığı için MSS'in RK4 kipi referans; 04 §2.8); batık sferoid FK yükü MSS `spheroidRAO` ile; dalgada sabitlenmiş BlueROV2 yükleri Gabl ve ark. (2021) ile. Aracri/Walker çalışması sabitlenmiş ROV içindir, yüzey geçişini doğrulamaz (04.18). Yüzey–kısmi batma–dalış geçişi ayrı kabul testidir (00 Bölüm 6): yüzeyden dalış, yüzeye çıkış ve dalgada yüzeyde bekleme senaryolarında enerji dengesi ve fiziksel olmayan sıçramanın yokluğu.

---

## 8. Açık noktalar ve iskelete öneriler

**İskeletle çelişki ve öneriler**

1. **400 Hz ile 1 ms adım uyuşmuyor.** İskelet §4 fizik adımını 1 ms, kontrol döngüsünü 400 Hz ve IMU örneği tetiklemeli koyuyor. 400 Hz periyodu (2,5 ms) 1 ms adımın tam katı değildir; bu bölümdeki rasyonel zamanlayıcıyla IMU örnekleri 2 ms / 3 ms aralıklarla gelir, oysa gerçek IMU düzenli saatle örnekler. Bölüm iskelete uydu (1 ms). Öneri: başlangıç adımını **0,5 ms** yapmak (hesap yükü iki katı, tahmin) ya da IMU hızını 1 ms'nin katı seçmek (ör. 500 Hz). Karar M5 ve M15 ile birlikte verilmeli.
2. **"Fizik her kademede tam çalışır" ile ölçülmemiş hesap yükü.** Düşük donanımlı Windows'ta 1 kHz × 64 bileşen × ağ maliyeti bilinmiyor (04.16). Ölçüm yetmezse fizik doğruluğunu azaltan yedekler (Bölüm 6) gerekecek. Öneri: iskelet §6'ya "fizik çözüm ayarları kademe değildir; yalnız M15 sonucuyla ve kayda yazılarak değişir" notu eklenmesi.
3. **Nitelik etiketi.** Görev tanımı üç etiket istiyordu; `m`, `I`, `r_g`, `r_b` için kaynakta sayı var ama ölçüm mü tahmin mi olduğu raporlarda ayrılmamış. Dördüncü etiket ("yayımlanmış, niteliği doğrulanmadı") kullanıldı. von Benzon Tablo A1 doğrudan okunduğunda bu satırlar ve itici yerleşimi kesinleşir.
4. **Torpido ve fırlatıcı parametreleri** iskelet §5'te "iki torpido fırlatıcı" olarak duruyor; varsayılan geometri ve konumların mimari varsayımı olduğu iskelet §5'e not düşülebilir (K10 kapsamı).

**Açık noktalar**

- von Benzon Tablo A1'deki itici konum ve yönleri, atalet değerleri ve itki polinomunun hangi gerilimde ölçüldüğü raporlara aktarılmadı; okunup 2.6 ile karşılaştırılmalı.
- 30Q OCV, R₀, R_Th haritaları ve hücre kapasitesi X-57 ekinden A5'e aktarılmalı; aktarılana kadar B1 kullanılamaz.
- Ortalama yüzey üstünde dalga kinematiği (Wheeler germe ya da başka düzeltme) doğrulanmadı.
- İkinci mertebe sürüklenme kuvvetleri, frekansa bağlı eklenmiş kütle ve sıvı belleği yok; yüzeyde uzun seyirde konum kayması eksik kalır (04 §2.3, §4 md.2).
- Derinliğe bağlı akıntı profili ve DVL su takibi için akıntı alanının ayrıntısı (05 ile).
- Torpido sürekli çarpışma gereksinimi (A25) ve isabet sonrası davranış.
- Tek sahnede operatör görünümü ile sensör yakalama zamanının farklı olması (00 Bölüm 6) 07'nin konusu; çekirdek her ikisi için de `t_ns` etiketli poz verir.
- Batarya, ESC ve itici ısıl modelleri ilk sürümde yok.
