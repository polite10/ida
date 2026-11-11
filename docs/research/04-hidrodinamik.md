# 04 — Hidrodinamik ve Araç Dinamiği Modellemesi

> **Durum: ham araştırma raporu (yazıldığı haliyle korunuyor).** Bu rapor bağımsız doğrulamadan geçti ve bilinen hatalar içeriyor. Ayrıca dil kararı C/C++ olmadan önce, Rust varsayımıyla yazıldı; Rust'a özgü öneriler geçersizdir. Doğrulaması: [dogrulama/04-codex.md](dogrulama/04-codex.md). Düzeltmeler ve geçerli kararlar: [00-duzeltmeler-ve-kararlar.md](00-duzeltmeler-ve-kararlar.md). Çelişki halinde 00 numaralı belge geçerlidir.

> Kapsam: Hibrit (su üstü + su altı) tek insansız deniz aracının HIL/SIL simülatörü için fizik çekirdeği. Fossen 6-DOF modeli, yüzey geçişi, dalga/akıntı/rüzgar, itici modelleri, parametreler, sayısal çözüm, torpido, doğrulama ve Rust uygulaması.
> Tarih: 2026-11-10. İşaretler: **[doğrulanmadı]** = birincil kaynakta teyit edilemedi; **[türetim]** = bu raporda yapılan çıkarım veya hesap, literatürden alıntı değil.

---

## 1. Özet

1. **Temel model Fossen'in 6-DOF vektörel modeli olmalı.** Gövde eksenindeki hızlar ν ve NED'deki konum/yönelim η kullanılır. Denklem: `M ν̇_r + C(ν_r)ν_r + D(ν_r)ν_r + g(η) = τ + τ_çevre`. Eklenmiş kütle (added mass) sabit M matrisinin içine konur. Hem MSS hem Python Vehicle Simulator (PVS) bu yapıyı MIT lisansıyla açık kaynak olarak sunuyor ve doğrudan referans alınabilir.
2. **Hibrit araç için tek modelde kalınabilir.** Gerekli olan, g(η) teriminin sabit hacim yerine anlık batmış hacim ∇(t) ve anlık yüzerlik merkezi r_b(t) ile hesaplanması. Hunt & Whitcomb (ICRA 2024) bunu yaptı; yüzeye çıkan bir AUV'de klasik modelin fiziksel olmayan sonuç verdiğini, hibrit modelin ise ölçümlerle tutarlı olduğunu gösterdi.
3. **Hacim ve yüzerlik merkezi için önerilen yöntem: düşük çokgenli hidrostatik ağ (mesh) + üçgen kırpma.** Bu yöntem (Kerner; Stonefish da benzerini kullanıyor) dalgalı yüzeyde Froude–Krylov basıncını da aynı döngüde verir. Eklenmiş kütle ve sönüm ise batıklık oranıyla ölçeklenen toplu katsayılar olarak kalır.
4. **Dalga zinciri:** JONSWAP veya PM spektrumu, sabit tohumlu (seeded) rastgele fazlarla bileşenlerin toplamı, dispersiyon `ω² = gk·tanh(kh)` ve derinlikle `e^{kz}` sönümü. Araç batıkken ağ yerine Morison/FK tipi nokta modeli yeterli. MSS'te spektrum ve batık sferoid için FK yükü fonksiyonları var.
5. **İtici modeli için Blue Robotics'in herkese açık T200 ölçüm tablosu kullanılmalı.** Tablo 10–20 V aralığında PWM'e göre itki, devir (RPM) ve akımı veriyor. Ölçülen değerler: 16 V'ta maksimum 5,25 kgf ileri ve 4,1 kgf geri itki, yaklaşık 1472–1528 µs ölü bölge. Tablo (PWM, V) eksenlerinde 2B interpolasyonla okunur, ardından birinci dereceden RPM dinamiği ve batarya iç direnci modeli eklenir. **İtki tahsisi (allocation) firmware'in işidir.** Simülatör yalnızca her iticinin kuvvetini geometri üzerinden τ'ya çevirir.
6. **Yayımlanmış BlueROV2 katsayıları birbiriyle büyük ölçüde çelişiyor.** Wu (2018) tablosu ölçülmemiş, literatürden ve varsayımlardan türetilmiş. von Benzon (2022) değerleri ise test tankında doğrulanmış ve karesel sönümü 5–8 kat daha yüksek. Varsayılan set olarak von Benzon alınmalı, Wu tablosu kullanılmamalı.
7. **Sayısal çözüm:** Sabit adımlı RK4 (başlangıç önerisi dt = 1 ms) ve quaternion yönelim, her adımda normalize edilerek. Eklenmiş kütle asla "önceki adımın ivmesi × A" gibi dış kuvvet olarak uygulanmamalı; Gazebo bu yöntemin yalnızca koşullu kararlı olduğunu belgeliyor. M sabitse Cholesky ayrıştırması bir kez yapılır, batıklıkla değişiyorsa her adımda yeniden yapılır.
8. **Rapier gibi genel fizik motorları araç dinamiği için uygun değil.** Kütle skaler (f32) ve eylemsizlik 3×3 asal eksenli; 6×6 bağlaşımlı eklenmiş kütle matrisi ifade edilemiyor. Önerilen yol: nalgebra (`Matrix6`, `Vector6`, `UnitQuaternion`, f64) ile yazılmış özel bir çekirdek. Çarpışma sorguları gerekirse parry ayrıca kullanılabilir.
9. **Determinizm koşulları:** sabit dt, sabit tohumlu RNG, sabit işlem sırası, paralel indirgeme (reduction) yok, trigonometrik fonksiyonlarda platformdan bağımsız `libm`. macOS/Windows arasında bit düzeyinde eşitlik CI'da hash ile test edilmeli.
10. **Doğrulama katmanları:** analitik testler (C(ν) çarpık simetri, enerji korunumu, serbest sönüm periyodu, limit hız), PVS/MSS ile yörünge karşılaştırması, von Benzon Tablo 4 hız/itki değerleri ve deneysel veri setleri (Gabl ve ark. 2021, dalgada BlueROV2).

---

## 2. Bulgular

### 2.1 Fossen 6-DOF modeli

**Koordinat sistemleri ve gösterim (SNAME):**
- `{n}` NED (North–East–Down), yerel düz dünya kabulü. Küçük yarışma alanında ECEF/jeodezik dönüşüm yalnızca GNSS sensör modelinde gerekir.
- `{b}` gövde ekseni: x ileri (bow), y sancak, z aşağı. Orijin CO (genellikle CG veya CB).
- η = [x, y, z, φ, θ, ψ]ᵀ (NED konum + Euler açıları) veya quaternion ile [p; q]. ν = [u, v, w, p, q, r]ᵀ gövde hızları. τ = [X, Y, Z, K, M, N]ᵀ.
- Render motorları genellikle Y-yukarı eksen kullanır (ör. Bevy). Dönüşüm yalnızca görselleştirme sınırında yapılmalı, fizik çekirdeği NED'de kalmalı.

**Kinematik** (Fossen 2021, Bölüm 2):
- Euler: `η̇ = J_Θ(η) ν`, `J_Θ = diag(R(Θ), T(Θ))`. θ = ±90°'de tekildir. Dalabilen ve dik yunuslama yapabilecek bir araç için risklidir.
- Quaternion: `ṗⁿ = R(q) v_b`, `q̇ = ½ T(q) ω_b`. MSS'teki `SIMremus100.m` her iki gösterimi de destekliyor. Quaternion modunda MSS hızlar için ileri Euler, konum için yarı-örtük güncelleme, quaternion için "exact discretization" `q ← expm(T(ω)h)·q` ve ardından normalizasyon kullanıyor (kaynak kodda doğrulandı).

**Kinetik (birleşik model, ideal-sıvı + viskoz + hidrostatik):**

```
M_RB ν̇ + C_RB(ν)ν + M_A ν̇_r + C_A(ν_r)ν_r + D(ν_r)ν_r + g(η) + g₀ = τ + τ_rüzgar + τ_dalga
ν_r = ν − ν_c
```

Akıntı NED'de sabit ve dönümsüz (irrotational) ise model tamamen ν_r cinsinden yazılabilir: `M ν̇_r + C(ν_r)ν_r + D(ν_r)ν_r + g(η) = τ`. Bunun koşulu, C_RB'nin doğrusal hızdan bağımsız parametrelenmesidir. von Benzon ve ark. (2022, Denk. 20) aynı yapıyı kullanıyor; gz-sim Hydrodynamics de `M ẍ + C(ẋ_r)ẋ_r + D(ẋ_r)ẋ_r = F` biçimini belgeliyor.

- **Rijit cisim kütlesi:** `M_RB = [[m I₃, −m S(r_g)], [m S(r_g), I_o]]`.
- **Eklenmiş kütle:** `M_A = −[X_u̇ … N_ṙ]` (6×6). Üç düzlemde simetrik bir gövdede yaklaşık diyagonaldir. Fossen işaret kuralında türevler negatiftir (X_u̇ < 0, dolayısıyla M_A pozitif). M = M_RB + M_A simetrik ve pozitif tanımlı olmalıdır.
- **Coriolis/merkezcil terim:** Kirchhoff formülasyonuyla M'den türetilir. PVS `m2c()`:
  `C(ν) = [[0₃, −S(M₁₁ν₁ + M₁₂ν₂)], [−S(M₁₁ν₁ + M₁₂ν₂), −S(M₂₁ν₁ + M₂₂ν₂)]]`.
  Bu yapı çarpık simetriktir, dolayısıyla `νᵀC(ν)ν = 0` sağlanır. Enerji testi bu özelliğe dayanır. C_A, ince gövdelerde **Munk momentini** üretir; bu moment kararsızlaştırıcıdır. PVS'nin `remus100.py` modeli, dönme sönümü eksikse bu terimlerin modeli kararsızlaştırdığını not edip bazı C_A terimlerini sıfırlıyor (kaynak kodda doğrulandı).
- **Sönüm:** `D(ν_r) = D_L + D_NL(ν_r)`, `D_NL = −diag(X_{u|u|}|u|, Y_{v|v|}|v|, …, N_{r|r|}|r|)`. Düşük hızlı ROV'larda diyagonal kabulü standarttır (von Benzon Denk. 10–11). PVS'nin REMUS modelinde doğrusal sönüm hızla `exp(−3U_r)` biçiminde sönümlendiriliyor; yüksek hızda karesel sürükleme ve kaldırma (lift) baskın hale geliyor.
- **Geri getirici kuvvetler (her yönelimde geçerli vektör formu, quaternion ile uyumlu):**
  ```
  g(η) = −[ Rᵀ(f_gⁿ + f_bⁿ) ;  r_g × Rᵀ f_gⁿ + r_b × Rᵀ f_bⁿ ],   f_gⁿ = [0,0,W]ᵀ, f_bⁿ = −[0,0,B]ᵀ
  ```
  Euler açılı açık formu PVS `gvect()` içindedir; örneğin ilk satır `(W−B)·sinθ`. Su üstü gemilerinde doğrusal karşılık `G = diag(0, 0, ρgA_wp, ρg∇GM_T, ρg∇GM_L, 0)` (PVS `otter.py`: G33, G44, G55).

**Su üstü ve su altı tek modelde nasıl birleşir:**
Klasik ayrım şöyledir. Gemiler için frekansa bağlı A(ω), B(ω), yani Cummins denklemi ve sıvı belleği (fluid memory) kullanılır. Batık araçlar için sabit katsayılar yeterlidir. Fossen'in "unified" modeli manevra ve denizcilik (seakeeping) teorisini birleştirir. Küçük bir yarışma aracında pratik yaklaşım şudur:
- M_RB, C_RB ve itici terimleri ortamdan bağımsızdır.
- g(η) **anlık batmış geometriden** hesaplanır (bkz. 2.2). Hunt & Whitcomb'un g_h formu: `[(ρ∇_h(t) − m)g Rᵀe₃ ; ρg∇_h(t)(r_h(t) × Rᵀe₃)]`.
- M_A ve D batıklık oranı s ∈ [0, 1] ile ölçeklenir.
- Su üstündeyken eksik kalan dalga yapma (radiation) sönümü, heave/roll/pitch'e eklenen doğrusal sönümle karşılanır. Otter modeli bu sönümü göreli sönüm oranıyla tanımlıyor: `Z_w = −2·0,3·ω₃·M₃₃`, `K_p = −2·0,2·ω₄·M₄₄`, `M_q = −2·0,4·ω₅·M₅₅` (PVS kaynak kodu).

### 2.2 Hibrite özgü konular: yüzey geçişi, kısmi batıklık, balast

**Literatürdeki yaklaşımlar:**

| Yaklaşım | Kaynak | Ne yapıyor | Artı / eksi |
|---|---|---|---|
| Nokta bulutu, Monte Carlo | Hunt & Whitcomb, ICRA 2024 | Gövde hacmine üniform rastgele noktalar yerleştirilir. Su altındaki noktaların oranı ∇_h, ortalamaları r_h verir. M_A ve D sabit kalır. | Basit. Iver3 AUV'de yüzeydeki CG derinliğini ölçümle tutarlı buldu (≈0,13 m; klasik model −0,2 m, yani suyun üstünde). Tam batıkken klasik modelle farkı < 1e-11. Dezavantajı: örnekleme gürültüsü var ve dalgalı yüzey test edilmemiş (düz yüzey varsayılmış). |
| Doğrusal ağırlıklandırma | Wang & Shang, arXiv 2507.03869 (hava-su hibrit) | `C = 0,5 − z/H` (−H/2 < z < H/2) ile eklenmiş kütle, yüzerlik ve sürükleme alanı ölçeklenir. İtki katsayısı log-doğrusal karıştırılır. | Çok ucuz ve sürekli. Ancak yönelime duyarsız ve küçük açı varsayımına dayanıyor; dalga içermiyor. |
| Üçgen ağ kırpma (mesh clipping) | Kerner (Game Developer, 2015); Stonefish | Her üçgen su çizgisinde kesilir, basınç kuvveti batmış parçalar üzerinden toplanır. Stonefish yüzerliği yüzey üzerindeki hidrostatik kuvvetlerin toplamı olarak hesaplıyor ve "geometrik dalgalı veya dalgasız" yüzeyde gerçekçi yüzerlik sağladığını belirtiyor. | Dalgalı yüzeyde de doğru; FK basıncı aynı döngüde hesaplanır. Kerner'in uyarıları: kenar başına tek kesişim varsayımı "teorik olarak yanlış"; üçgenler en kısa dalga boyuna göre küçük olmalı; yük uygulama noktası üçgen merkezi değil basınç merkezi olmalı, aksi halde düz suda sahte devrilme momenti oluşur. |
| Katman/dilim | gz-sim Buoyancy, "graded" mod | Çarpışma ağını su çizgisine göre dilimler. | Gazebo'ya bağlı; yalnızca hidrostatik. |
| Strip theory | Klasik gemi teorisi | Kesit başına eklenmiş kütle, sönüm ve yüzerlik. | İnce (slender) gövde gerektirir. Gabl ve ark. ROV tipi gövdeler için uygun olmadığını belirtiyor. |

**Geçişe özgü fiziksel etkiler ve başlangıç kararları:**
- **Değişken eklenmiş kütle:** M_A(t) değişirken momentum korunumunda `d/dt(M_A ν) = M_A ν̇ + Ṁ_A ν` ortaya çıkar. `M ν̇` formu Ṁ_A·ν terimini atar. Bu terim suya giriş (slamming) etkisinin kaynağıdır. Kerner 2. bölümde ayrı bir "slamming force" ekliyor. **Öneri [türetim]:** düşük hızlı dalma/çıkışta bu terimi başlangıçta ihmal et, ama M_A(s)'i yumuşak (C¹) bir fonksiyonla değiştir. Sert geçiş integratörde sıçrama yaratır.
- **Pervane havalanması (ventilation):** Yüzeyde seyirde iticiler yüzeye yakın kalır. Califano & Steen ve Kozlowska & Steen, h/R oranına bağlı itki kaybı modelleri veriyor. Bir formülasyonda h/R > 3,4 "derin", yani havalanma yok kabul ediliyor. T200 için R = 38 mm olduğundan bu eşik yaklaşık 13 cm şaft derinliğine denk gelir **[türetim]**. Model katsayıları doğrulanmadı; başlangıç için h/R ile itki çarpanı β(h/R) ∈ [0, 1] olan bir tablo önerilir.
- **Değişken yüzerlik / balast:** m(t), W(t) ve r_g(t) zamanla değişen parametreler olarak modellenir. Pistonlu balastta hacim değişim hızı sınırlı bir aktüatör dinamiği eklenir (birinci derece + hız limiti). Su tanklı balastta tanktaki serbest yüzey etkisi GM'yi düşürür; ilk sürümde ihmal edilip açık soru olarak tutulmalı. Kütle akışının momentum terimleri küçük debilerde ihmal edilebilir **[türetim]**.
- **Yüzeye yakın dalga etkisi:** Aracri ve ark. (CNR, 2021) sığ derinlikteki bir ROV'da Linear Wave Theory tabanlı düşük mertebeli bir yük modelini dalga tankında doğruladı. Ortalama korelasyon 0,91; surge/heave kuvveti ve pitch momenti için normalize ortalama hata sırasıyla 0,16 / 0,27 / 0,34. Bu, LWT + Morison/FK tipi bir modelin yarışma gerçekçiliği için yeterli olduğunu destekliyor.

### 2.3 Çevre modelleri

**Dalga spektrumları (MSS `waveSpectrum.m` ve SINTEF SIMA belgesinden doğrulandı):**
- Pierson–Moskowitz (Hs ile): `S(ω) = a ω⁻⁵ exp(−b/ω⁴)`, a = 0,0081 g², b = 3,11/Hs².
- JONSWAP (Hs, ω_p, γ; DNV düzeltmeli):
  ```
  S(ω) = A_γ · (5/16) Hs² ω_p⁴ ω⁻⁵ · exp(−1,25 (ω_p/ω)⁴) · γ^{exp(−(ω−ω_p)²/(2σ²ω_p²))}
  A_γ = 1 − 0,287 ln γ ;  σ = 0,07 (ω ≤ ω_p), 0,09 (ω > ω_p) ;  γ tipik 3,3
  ```
  SIMA'daki α = 5,061 Hs²/Tp⁴ (1 − 0,287 ln γ) ile aynıdır, çünkü (5/16)(2π)⁴/g² ≈ 5,06 **[türetim]**. MSS, γ geçersizse DNV kuralını uygular: Tp/√Hs ≤ 3,6 için γ = 5; 3,6–5 aralığında `exp(5,75 − 1,15 Tp/√Hs)`; > 5 için γ = 1.
- Yarışma havuzu veya kıyı için önerilen aralık: Hs 0–0,5 m, Tp 1,5–4 s. Bu bir tahmin; yarışma ortamına göre ayarlanmalı **[doğrulanmadı]**.

**Yüzey üretimi:**
- *Bileşen toplamı (Airy, doğrusal):* `ζ(x,y,t) = Σ A_i cos(ω_i t − k_i(x cosβ_i + y sinβ_i) + φ_i)`, `A_i = √(2 S(ω_i) D(β_i) Δω Δβ)`. Dispersiyon `ω² = g k tanh(k h)`, derin suda `k = ω²/g` (MSS `spheroidRAO.m` aynı ilişkiyi kullanıyor). Periyodik tekrarı önlemek için frekanslar kutu (bin) içinde rastgele seçilmeli ve tohum sabit olmalı.
- *Gerstner (trochoidal):* Tepe şekli daha gerçekçi görünür. Fizik çekirdeğinde ise Airy ile aynı kinematikler kullanılmalı; Gerstner'i yalnızca render için kullanmak tutarsızlık yaratır **[türetim]**.
- *FFT (Tessendorf 2001):* Izgarada (ör. 64×64 veya 128×128) spektral genlikler + FFT ile periyodik bir yükseklik alanı üretilir. Tessendorf, Phillips spektrumunun yüksek k'da yakınsama sorunu olduğunu ve küçük dalga boylarının üstel bir çarpanla bastırılması gerektiğini not ediyor. Fizik için Phillips yerine JONSWAP genlikleri FFT ızgarasına konulabilir.

**Dalga kuvvetlerinin araca uygulanması:**
- *Su üstünde veya yarı batıkken (ağ tabanlı):* Her batık üçgende basınç `p = ρg(−z_rel) + ρg Σ A_i e^{k_i z} cos(·)` (hidrostatik + birinci mertebe FK). Kuvvet `dF = −p n dA`, moment basınç merkezine göre alınır (Kerner). Su yüzeyi üstündeki kinematik için "Wheeler stretching" gibi düzeltmeler gerekir **[doğrulanmadı, standart offshore pratiği]**.
- *Tam batıkken (nokta/eleman modeli):* Morison tipi `F = ρ∇ a_w + A a_w + ½ρ C_D A_p (u_w − v)|u_w − v|`. Burada ilk terim FK (basınç gradyanı), ikincisi difraksiyon/eklenmiş kütle katkısıdır. **Önemli ayrıntı [türetim]:** Fossen'in göreli hız formu (M_A ν̇_r) yalnızca eklenmiş kütle kısmını içerir. Akış ivmeli olduğunda (dalga yörüngesel hızları) FK terimi ρ∇·a_w ayrıca eklenmelidir. Morison'un geçerlilik sınırı D/λ küçük olmalıdır; DNV-RP-C205'te sıkça verilen değer λ > 5D **[doğrulanmadı]**.
- *Analitik referans:* MSS `spheroidRAO.m` tam batık prolate sferoid için kapalı formda FK yükü verir (ör. `P = ρ g k V Φ(λ) e^{−k z_n}`) ve isteğe bağlı düşük frekanslı difraksiyon içerir. Torpido benzeri gövde için doğrulama referansı olarak kullanılabilir.
- *Derinlikle azalma:* Derin suda yörüngesel hız ve dinamik basınç `e^{kz}` (z yukarı pozitif) ile azalır. z = −λ/2'de çarpan e^{−π} ≈ 0,043 **[türetim]**. Sonlu derinlikte `cosh(k(z+h))/cosh(kh)` kullanılır.
- *İkinci mertebe (drift) kuvvetleri:* ilk sürümde ihmal edilebilir; yüzeyde uzun süreli seyirde konum kaymasına yol açar.

**Akıntı:**
- MSS `oceanCurrents.m` (2026 sürümü): uniform ve dönümsüz akıntı. `v_cⁿ = V_c[cosγ cosβ, cosγ sinβ, sinγ]`, `ν_c = [Rᵀ v_cⁿ; 0]`, `ν̇_c = [−S(ω)ν_c,1:3 + Rᵀ v̇_cⁿ; 0]`. Sınırlı Gauss–Markov stokastik mod da destekleniyor (kaynak kodda doğrulandı).
- Derinliğe bağlı profil: DNV-RP-C205'te gelgit akıntısı için 1/7 kuvvet yasası `v(z) = v₀((d+z)/d)^{1/7}`, rüzgârla oluşan akıntı için belirli bir derinlikte sıfıra inen doğrusal profil verildiği biliniyor **[doğrulanmadı, DNV metni okunmadı]**. Profil kullanılırken Fossen'in "uniform akıntı" varsayımı bozulur. Pratik çözüm: akıntı ağ elemanlarında veya birkaç örnek noktada ayrı ayrı değerlendirilip sürükleme terimine sokulur **[türetim]**. Stonefish de yüzey elemanı başına yerel akış hızı kullanarak "gövdenin bir kısmına etkiyen akıntı"yı modelleyebildiğini belirtiyor.

**Rüzgâr:** Fossen'in standart formu `τ_rüzgar = ½ ρ_a V_rw² [C_X(γ_rw) A_FV, C_Y A_LV, …, C_N A_LV L_oa]`. Katsayılar Isherwood benzeri tablolardan gelir; MSS'te `exWindForce.m` mevcut. Hibrit araç için alanlar su üstündeki projeksiyonla ölçeklenmeli (batıklık oranı). Ani rüzgâr (gust) için Harris veya NORSOK spektrumları kullanılabilir **[doğrulanmadı]**.

### 2.4 İtici ve aktüatör modelleri

**T200 verisi** (Blue Robotics ürün sayfası ve `T200-Public-Performance-Data-10-20V-September-2019.xlsx`, tablo indirilip incelendi):

| Gerilim | Maks. ileri / geri itki | Maks. akım (güç) |
|---|---|---|
| 12 V | 3,71 / 2,92 kgf | 17 A (205 W) |
| 16 V | 5,25 / 4,1 kgf | 24 A (390 W) |
| 20 V | 6,7 / 5,05 kgf | 32 A (645 W) |

- Pervane çapı 76 mm. PWM 1100–1900 µs, 4 µs adımlarla ölçülmüş.
- **Ölü bölge** (tabloda itkinin sıfır olduğu PWM aralığı): 12 V'ta 1464–1536 µs, 16 V'ta 1472–1528 µs **[türetim: tablodan okundu]**.
- **Statik itki katsayısı [türetim]:** F = k·n|n| (n rps) uydurulduğunda 16 V ileri yönde k ≈ 0,0139–0,0148 N/rps², geri yönde ≈ 0,0109–0,0111. K_T = k/(ρD⁴) ≈ 0,44 ileri, 0,33 geri. İleri/geri asimetrisi açık.
- **Gerilim etkisi [türetim]:** Tam gazda devir 12 V'ta ≈ 2995, 16 V'ta 3533, 20 V'ta 3907 rpm. İtki gerilimle yaklaşık V^1,1–1,2 oranında artıyor. Bu yüzden gerilimi bir parametre olarak alan 2B tablo (PWM × V → itki, akım) en sağlam çözüm.
- **İtici dinamiği:** von Benzon ve ark., MathWorks verisinden birim DC kazançlı 3. derece bir transfer fonksiyonu tanıladı: `K(s) = (6136 s + 108700)/(s³ + 89 s² + 9258 s + 108700)`. Kutuplar yaklaşık −13,2 ve −37,9 ± 82,6j; baskın zaman sabiti ≈ 76 ms **[türetim: kökler hesaplandı]**. Daha basit alternatif: devir için birinci derece `ṅ = (n_c − n)/T_n` (PVS REMUS: T_n = 1 s; büyük bir pervane için). Klasik referanslar: Yoerger, Cooke & Slotine (1990), iticiyi itki seviyesine bağlı hızla tepki veren "yavaş doğrusal olmayan filtre" olarak modelliyor. Healey ve ark. (1995) dört bölgeli (four-quadrant) model kurup hızlı komut değişiminde itki aşımını öngörüyor.
- **Akış etkisi:** Bollard (statik) verisi araç hareket halindeyken itkiyi fazla gösterir. REMUS modeli K_T'yi ilerleme oranı J ile doğrusal düşürüyor (K_T0 = 0,4566 → K_T,max = 0,1798, Wageningen B serisi tabanlı). T200 için yayımlanmış K_T(J) eğrisi bulunamadı **[doğrulanmadı]**.
- **Batarya:** `V = OCV(SoC) − I·R_iç`. Toplam akım tablodan okunur. Gerilim düşümü tüm iticilerin itkisini birlikte etkiler, bu da gerçekçi ve firmware'in gerilim kompanzasyonunu test etmeye değer bir bağlaşım.
- **Tahsis (allocation) matrisi:** `τ = Σ_i [ε_i F_i ; r_i × ε_i F_i] = T F`. von Benzon, BlueROV2 Heavy'nin 8 iticisinin r_i ve ε_i değerlerini Tablo A1'de veriyor. **Mimari not:** HIL'de tahsis (T⁺ = Tᵀ(TTᵀ)⁻¹ veya doyuma duyarlı QP) firmware'de çalışır. Simülatör yalnızca ileri dönüşümü (PWM → F_i → τ) yapmalıdır; aksi halde firmware'in tahsis hataları gizlenir.
- **Dümen ve kanat** (PVS `forceLiftDrag`): `C_L = C_Lα α`, `C_Lα = π AR/(1 + √(1 + (AR/2)²))`, `C_D = C_D0 + C_L²/(π e AR)`, e = 0,7. Stall için `C_L = (1−σ) C_Lα α + σ·2 sgn(α) sin²α cosα` ile karıştırma yapılır. REMUS kuyruk kanatları için kullanılan değerler: C_Lδ,r = 0,5, C_Lδ,s = 0,7, kanat alanı 0,00665 m², servo zaman sabiti T_δ = 0,1 s, ±15° doyum.

### 2.5 Parametreler

**BlueROV2 Heavy, von Benzon ve ark. (2022), Tablo A1 (önerilen varsayılan set):**
- Rijit cisim: m = 13,5 kg; ∇ = 0,0134 m³ (ρ = 1000 ile yer değiştiren kütle 13,4 kg; tablo değerleri alındığı gibi kullanılırsa araç ≈ 0,1 kg **negatif** yüzer [türetim]. Bu, gerçek BlueROV2'nin hafif pozitif ayarıyla çelişebilir ve kontrol edilmeli); I_x, I_y, I_z = 0,26 / 0,23 / 0,37 kg·m²; r_g = 0; r_b = (0, 0, −0,01) m; g = 9,82; ρ = 1000.
- Eklenmiş kütle: X_u̇ = 6,36; Y_v̇ = 7,12; Z_ẇ = 18,68 kg; K_ṗ = 0,189; M_q̇ = 0,135; N_ṙ = 0,222 kg·m² (pozitif büyüklük olarak raporlanmış).
- Doğrusal sönüm: X_u = 13,7; Y_v = 0; Z_w = 33,0; K_p = 0; M_q = 0,8; N_r = 0.
- Karesel sönüm: X_{u|u|} = 141,0; Y_{v|v|} = 217,0; Z_{w|w|} = 190,0; K_{p|p|} = 1,19; M_{q|q|} = 0,47; N_{r|r|} = 1,5.
- İtki eğrisi (normalize giriş V ∈ [−1, 1]): `F(V) = −140,3V⁹ + 389,9V⁷ − 404,1V⁵ + 176,0V³ + 8,9V`.
- Tablo 4'teki maksimum hızlar: 0,72 / 0,63 / 0,71 m/s (x/y/z); maksimum kuvvetler 85 / 85 / 120 N.
- Eklenmiş kütle Eidsvik'in ampirik yöntemiyle hesaplanmış. Bu yöntemde öteleme hatası %10–20, dönme hatası %30–100. Ayrıca Heavy'nin genişliği boyunu %20 aştığından yöntemin geçerlilik koşulu ihlal edilmiş; makalede bu açıkça belirtiliyor. Sönüm katsayıları deneysel; surge/sway/heave hatası grafiklerde veriliyor, pitch ve yaw farkı %10 ve %12.
- **Tutarlılık kontrolü [türetim]:** `X_{u|u|}u² + X_u u = 85` denkleminden u = 0,729 m/s çıkıyor; Tablo 4'teki 0,72 m/s ile uyumlu.

**Wu (2018) tablosu, yaygın biçimde kopyalanıyor:** m = 11,5; X_u̇ = −5,5; Y_v̇ = −12,7; Z_ẇ = −14,57; X_{u|u|} = −18,18; Y_{v|v|} = −21,66; Z_{w|w|} = −36,99; dönme katsayıları hepsi aynı (−0,12, −0,07, −1,55). **Tezin kendi ifadesine göre** araç teslim edilmediği için deney yapılamamış. Değerler BlueROV1 üzerine yapılmış Sandøy (2016) çalışmasından alınmış. Dönme eksenlerinin değerleri yaw değerinin kopyası; CG–CB mesafesi (0,02 m) "temsili" olarak seçilmiş. Aynı 85 N itkiyle bu set ≈ 2,05 m/s limit hız veriyor **[türetim]**, yani gerçekçi değil.

**Diğer kaynaklar:**
- REMUS 100 (Prestero 2001, MIT tezi; PVS/MSS'te kodlanmış): torpido biçimli gövde için referans.
- Iver3 (Hunt & Whitcomb Tablo I): M = diag(50,5; 70,5; 69,5; 19,8; 109,2; 109,9); D = diag(0,16; 2,44; 12,1; 0,70; 4,04; 0,61)·10²·|v|.

**Gerçek araç yokken kestirim yöntemleri (öncelik sırasıyla):**
1. **Geometriden analitik ve ampirik hesap:**
   - Sferoid ve silindir için Lamb k-faktörleri: `M_A = diag(m k₁, m k₂, m k₂, r₄₄ I_x, k′ I_y, k′ I_y)`. Hesaplanan değerler [türetim]: L/D = 5 için k₁ = 0,059, k₂ = 0,894, k′ = 0,700; L/D = 8 için k₁ = 0,029, k₂ = 0,945.
   - Kutu ve çerçeve gövdeler için Eidsvik & Schjølberg (DNV verisine dayanan) yöntemi.
   - Sürükleme için Hoerner katsayıları. PVS `Hoerner()` 2B çapraz akış eğrisini sayısallaştırılmış olarak içeriyor.
2. **Panel yöntemi:** Capytaine (açık kaynak, BEM). gz-sim belgesi ROV eklenmiş kütlesi için "dalga frekansı ve serbest yüzey sonsuz" ayarını öneriyor. Yüzey aracı için frekansa bağlı A(ω) ve B(ω) da aynı araçla elde edilir.
3. **CFD:** Sönüm katsayıları için RANS; tek tek doğrulama gerektirir.
4. **Sistem tanılama (araç gelince):** serbest sönüm (pendulum) testi, sabit itkide limit hız testleri (von Benzon yöntemi: %20–100 itki, %10 adımlarla), bollard testi, en küçük kareler. Gabl ve ark. 8 halatla sabitlenmiş BlueROV2 üzerinde akıntı (1 m/s'ye kadar) ve düzenli dalga yük verisi yayımladı; doğrulama için kullanılabilir.

**Makul varsayılan seçimi [türetim]:**
- Ölçekleme yaklaşımı: BlueROV2 Heavy (von Benzon) katsayılarını aracın boyutlarına göre ölçekle. Eklenmiş kütle ∝ ρ·hacim; karesel sönüm ∝ ½ρC_D A_proj; dönme katsayıları ∝ uzunluğun 5. kuvveti (karesel) ve 4. kuvveti (eklenmiş atalet).
- Her katsayı için bir belirsizlik aralığı tanımla (ör. öteleme ±%30, dönme ±%100). Kontrolcüyü bu aralıkta Monte Carlo ile test et.
- Parametre dosyası (TOML/RON) her katsayı için değer, birim, kaynak ve belirsizlik alanlarını taşımalı.

### 2.6 Sayısal çözüm

- **Referans uygulamaların seçimleri** (kaynak kodda doğrulandı):
  - PVS: ileri Euler, h = 0,02 s.
  - MSS SIMremus100: h = 0,05 s. 2024-07 itibarıyla Euler açısı modu için RK4'e geçilmiş ("improved numerical accuracy"). Quaternion modunda yukarıda anlatılan karma şema var.
  - Stonefish (Bullet tabanlı): "velocity-based dynamics with semi-implicit Euler".
  - von Benzon: halat (tether) modeli nedeniyle sistem katı (stiff) olduğundan örtük (implicit) çözücü öneriyor (Simulink ode15s).
- **Katılık (stiffness) tahmini [türetim]:**
  - BlueROV2 roll ekseni: τ = (I_x + K_ṗ)/(2K_{p|p|}|p|). p = 5 rad/s'de ≈ 38 ms.
  - Surge: 0,72 m/s'de ≈ 92 ms.
  - İtici modeli: ≈ 76 ms ve ω_n ≈ 91 rad/s.
  - Sonuç: dt = 1 ms'de RK4 için tüm modlar rahatça kararlı; dt = 20 ms'de ileri Euler sınırda kalır. Halat veya temas (contact) gibi gerçekten katı alt sistemler eklenirse ya o alt sistem için alt adımlama (sub-stepping) ya da yarı-örtük yöntem kullanılmalı.
- **Eklenmiş kütle tuzağı:** gz-sim belgesi, eski plugin'in eklenmiş kütleyi "son adımın ivmesinden tahmin edip dış kuvvet olarak uyguladığını" ve açık integrasyonun yalnızca koşullu kararlı olduğunu söylüyor. Önerilen çözüm, eklenmiş kütleyi motorun kütle matrisine koymak (DART `<fluid_added_mass>`, "unconditionally stable, full 6×6, implicit Coriolis"). Bullet ve MuJoCo bunu desteklemiyor. Özellikle hafif gövdelerde (A ≳ m; ör. Z_ẇ 18,68 > m 13,5) kararsızlık kaçınılmaz. **Kural:** `ν̇ = M⁻¹(τ − Cν − Dν − g)`, M = M_RB + M_A. M sabitse Cholesky ayrıştırması önceden yapılır.
- **Yönelim:** RK4 içinde `q̇ = ½ q ⊗ [0, ω]`, ardından her adım sonunda normalizasyon. Alternatif olarak hız RK4 ile, yönelim üstel harita (exponential map) ile güncellenir (MSS yaklaşımı).
- **Süreksizlikler:** Doyumlar, ölü bölge ve ağ kırpma kuvveti (konumda sürekli ama türevi süreksiz) RK4'ün mertebesini düşürür. Hatayı sabit dt ile kabul et, gerekirse yumuşatma (smoothstep) uygula. Olay tespiti (event detection) gerekmiyor.
- **Determinizm:**
  - Sabit dt ve gerçek zamandan bağımsız fizik saati. HIL gerçek zaman eşlemesi ayrı bir zamanlayıcı katmanında yapılmalı.
  - Seeded RNG (ör. ChaCha). Sabit toplama sırası; paralel indirgeme yok.
  - Rust'ta temel aritmetik IEEE-754'tür ve derleyici FMA birleştirmesini varsayılan olarak yapmaz **[doğrulanmadı — resmi Rust/LLVM belgesiyle teyit edilmeli]**.
  - `sin`, `cos`, `exp` platform libm'ine gider ve macOS ile Windows'ta farklı sonuç verebilir. Rapier belgesi aşkın (transcendental) fonksiyonların platformlar arası deterministik olmadığını açıkça yazıyor. Çözüm: `libm` crate'i (saf Rust C math kütüphanesi), simba/num-traits'in `libm` özelliği veya kendi sarmalayıcın.
  - `enhanced-determinism` gibi garantiler yalnızca IEEE 754-2008'e sıkı uyan platformlar için geçerli.

### 2.7 Torpido dinamiği

**Model (6-DOF, aynı çekirdeğin ikinci bir örneği):** Torpido kendi M_RB, M_A (Lamb k-faktörleriyle ince sferoid veya silindir), D ve g değerlerine sahip, aynı integratörü kullanan ayrı bir "body" olmalı. PVS'deki `torpedo.py` (REMUS tabanlı, keyfi yerleştirilebilen kanat sınıfı, yerel bağıl akış hızına göre kanat kuvveti) iyi bir şablon.
- **Sürükleme:** `X = −½ρ C_D A_ön u|u|`. REMUS için C_D = 0,42 (Allen ve ark. 2000; PVS'de cephe alanına göre). Kısa, 3D baskı, kör burunlu yarışma torpidoları için C_D muhtemelen 0,3–1,0 aralığında **[doğrulanmadı; Hoerner tabanlı tahmin]**. Basit bir düşürme veya fırlatma testiyle limit hızdan ölçülmeli.
- **Çapraz akış:** PVS `crossFlowDrag` (şerit/strip integrasyonu + Hoerner C_D) sway ve yaw sönümünü verir.
- **Yüzerlik:** Hafif negatif veya nötr. CG–CB ofseti roll kararlılığını belirler.
- **Yön kararlılığı [türetim — bağımsız kontrol edilmeli]:** Kanatsız ince gövdede Munk momenti yaklaşık (A₂₂ − A₁₁)·u·v büyüklüğünde ve kararsızlaştırıcı. Arka kanatların düzelttiği moment ≈ |x_f|·½ρU²S_f C_Lα·β. Her ikisi de U² ile ölçeklendiğinden kaba statik kararlılık koşulu hızdan bağımsızdır: `½ρ S_f C_Lα |x_f| > (A₂₂ − A₁₁)` (gövde kaldırması ihmal edilmiş). Simülasyon, kanat boyutu değiştirildiğinde bu sınırın gözlenip gözlenmediğini göstermeli.
- **Fırlatma ve araca tepki [türetim]:** Namludan çıkış süresi (L_tüp/v₀) ms mertebesindedir. Pratik model: atış anında torpido, namlu ucundaki aracın hızı (`v + ω × r_namlu`) artı tüp ekseni boyunca v₀ başlangıç durumuyla oluşturulur (spawn). Araca eşit ve zıt itme `J = −(m_t + A₁₁,t)·v₀·ε_tüp` ve buna karşılık gelen açısal itme `r_namlu × J` tek adımda uygulanır. Eklenmiş kütlenin itmeye dahil edilip edilmemesi tüpün içinde suyun da ivmelenip ivmelenmediğine bağlı; açık soru. Daha gerçekçi seçenek: yay veya pnömatik itici için F(t) profili verilir, ateşleme alt adımlarla çözülür.
- **Yüzeydeki atış:** Araç su üstündeyken atış yapılıyorsa torpidonun suya giriş anı (Ṁ_A, slamming, hava kabarcığı) basit modelin dışında kalır **[açık soru]**.

### 2.8 Doğrulama

**Analitik ve özellik testleri (birim/özellik testi olarak CI'da):**
1. Rastgele ν ve SPD M için `|νᵀC(ν)ν| < ε` (çarpık simetri).
2. D = 0, g = 0, τ = 0 iken ½νᵀMν sabit kalmalı. Uzun koşudaki sürüklenme integratör kalitesinin ölçüsüdür. Aynı koşulda sferoid eksen boyunca hareketinde Munk kararsızlığı nitel olarak gözlenmeli.
3. **Hidrostatik denge:** Kutu veya silindir gövdenin yüzme derinliği analitik olarak bilinir. Batık araçta CG altta iken küçük roll salınım periyodu `T = 2π√((I_x + A₄₄)/(W·BG))`. Su üstünde `ω₃ = √(ρgA_wp/(m + A₃₃))` (Otter yaklaşımıyla aynı).
4. **Serbest sönüm:** Doğrusal sönümde analitik zarf. Karesel sönümde enerji–genlik ilişkisi.
5. **Limit hız:** `X_{u|u|}u² + X_u u = T`. von Benzon Tablo 4 değerleri regresyon testi olarak kullanılır.
6. **Kısmi batıklık:** Hunt & Whitcomb'un "tam batıkken klasik modelle fark < 1e-11" testi aynen uygulanabilir: hibrit g(η) ile klasik g(η) derinde özdeş olmalı.
7. **Dalga:** Üretilen ζ(t) zaman serisinden Welch yöntemiyle spektrum tahmin edilir ve S(ω) ile karşılaştırılır; `Hs ≈ 4√m₀` kontrolü yapılır. Batık sferoid FK yükü MSS `spheroidRAO` ile karşılaştırılır.
8. **Yakınsama:** dt yarıya indirildiğinde hata ~16 kat azalmalı (RK4, düzgün bölgede).
9. **Determinizm:** N adım sonrası durum vektörünün hash'i macOS ve Windows CI'da bit düzeyinde eşit olmalı.

**Referans koda karşı karşılaştırma:**
- PVS `remus100`, `otter`, `torpedo` ve MSS `remus100.m`, `otter.m` (MIT lisanslı) aynı parametre ve girişlerle Rust'a taşınıp yörüngeler karşılaştırılmalı. PVS ileri Euler kullandığı için karşılaştırmada PVS küçük h ile koşturulmalı veya MSS'in RK4 modu referans alınmalı.
- Fossen'in Handbook 3. baskısı (2027) MSS kaynak yorumlarında atıf olarak geçiyor; bölüm ve denklem numaraları 2. baskıdan farklı olabilir.

**Deneysel ve dış veri:**
- Gabl ve ark. (2021, Ocean Engineering, doi:10.1016/j.oceaneng.2021.109279): sabitlenmiş BlueROV2 üzerinde akıntı ve dalga yükleri.
- Aracri ve ark. (CNR, 2021): dalga tankı ROV yükleri.
- Hunt & Whitcomb: yüzeye çıkış davranışı (nitel karşılaştırma).
- Araç üretildikten sonra en değerli doğrulama kendi havuz verisi olacak.

### 2.9 Rust tarafı

- **nalgebra** (sürüm 0.35.0 docs.rs'de görüldü): sabit boyutlu, yığında (stack) tutulan `Matrix6<f64>`, `Vector6<f64>`; Cholesky ve LU ayrıştırmaları; `UnitQuaternion` (`from_scaled_axis`, `to_rotation_matrix`, `euler_angles`, `from_euler_angles` doğrulandı). Normalizasyon `Unit` sarmalayıcısında **[`renormalize` metodunun varlığı bu sayfada doğrulanmadı]**. 6×6 çözüm mikro saniye mertebesinde olduğundan çekirdek gerçek zamanın çok üzerinde koşabilir.
- **Rapier / parry:** `MassProperties` alanları `local_com`, `inv_mass: f32`, `inv_principal_inertia`, `principal_inertia_local_frame` (parry3d 0.31.1). Kütle skaler, eylemsizlik 3×3, varsayılan tip f32. **Bu yapı 6×6 bağlaşımlı eklenmiş kütleyi ifade edemez.** Eklenmiş kütle dış kuvvet olarak eklenirse Gazebo'nun belgelediği koşullu kararlılık sorunu ortaya çıkar. Rapier'in determinizm özelliği (`enhanced-determinism`, `simd8` ile birlikte kullanılamaz) faydalı ama tek başına araç dinamiği için gerekçe değil. **Sonuç:** araç ve torpido dinamiği özel çekirdekte yazılmalı. Havuz duvarı, dip ve hedef çarpışmaları için parry (f64 varyantı) yalnızca geometri sorgusu olarak kullanılabilir; temas kuvveti (penalty/yay-sönüm) özel çekirdekte uygulanır.
- **Açık kaynak referanslar:**
  - MSS (MATLAB, MIT) ve PVS (Python, MIT): denklemler ve araç modelleri için birincil referans.
  - Stonefish (C++, GPL-3.0): geometri tabanlı yüzerlik ve sürükleme. Lisansı nedeniyle kod kopyalanmamalı, yalnızca yöntem referansı olarak kullanılmalı.
  - gz-sim Hydrodynamics ve Buoyancy (Apache-2.0): parametre adlandırması (`xDotU`, `xUabsU` …) ve işaret kuralı farkları (plugin negatif, `<fluid_added_mass>` pozitif).
  - Kerner makaleleri: ağ kırpma algoritması.
  - Rust'ta olgun bir Fossen uygulaması bulunamadı. `marine_vessel_simulator` 0.1.0 erken aşamada ve içeriği doğrulanmadı.

---

## 3. Mimari için öneriler

**3.1 Katmanlı çekirdek (crate ayrımı)**
- `hydro-core`: render ve IO'dan bağımsız, saf fonksiyonlar. `State { p_n, q, nu, actuator_states }`, `Params`, `fn derivatives(state, inputs, env, t) -> dState`, `fn step_rk4(dt)`. f64 ve nalgebra kullanır; `libm` üzerinden trigonometri yapar.
- `hydro-env`: dalga (spektrum → bileşenler veya FFT ızgarası), akıntı profili, rüzgâr. Tümü seeded ve zaman fonksiyonu olarak sorgulanabilir: `wave.elevation(x, y, t)`, `wave.kinematics(x, y, z, t)`.
- `hydro-actuators`: T200 tablo modeli (PWM × V), RPM dinamiği, ölü bölge, havalanma çarpanı, servo/kanat, balast, batarya.
- `hydro-bodies`: araç ve torpido örnekleri; çok gövdeli dünya, sabit sıralı döngü.
- Sensör modelleri ayrı bir rapor konusu. Çekirdek yalnızca "gerçek durum" (ground truth) ve türevleri sağlar.

**3.2 Doğruluk aşamaları**

| Aşama | İçerik | Gerekçe |
|---|---|---|
| **A0 (ilk hafta)** | Fossen 6-DOF, sabit M (Cholesky), C_RB + C_A (m2c), diyagonal D_L + D_NL, vektör formunda g(η) ile **sabit** ∇ ve r_b; quaternion, RK4, dt = 1 ms; uniform akıntı; T200 tablosu + 1. derece dinamik + ölü bölge; ileri tahsis (geometri) | PVS/MSS ile birebir karşılaştırılabilir, SIL hemen başlar. |
| **A1** | Düşük çokgenli (≤ 500 üçgen) hidrostatik ağ + üçgen kırpma ile ∇(t), r_b(t); düz yüzeyde yüzme/dalma geçişi; M_A(s), D(s) batıklık oranıyla yumuşak ölçekleme; yüzeyde heave/roll/pitch ek doğrusal sönüm (ζ ≈ 0,2–0,4, Otter tarzı); havalanma çarpanı | Hibrit davranışın temeli. Hunt & Whitcomb testleri uygulanabilir. |
| **A2** | Düzensiz dalga (JONSWAP + yönsel yayılım, 32–64 bileşen, seeded) + ağ üzerinde FK basıncı; batıkken Morison/FK nokta modeli ve e^{kz} sönümü; rüzgâr (su üstü alanıyla ölçekli); derinliğe bağlı akıntı profili | Yarışma koşullarına yakın bozucu etkiler. |
| **A3** | Torpido gövdesi (Lamb k-faktörleri, kanat, çapraz akış), fırlatma itmesi; balast aktüatörü; batarya SoC ve gerilim düşümü | Görev senaryoları. |
| **A4 (araç gelince)** | Sistem tanılama ile katsayı güncelleme, Capytaine ile A(ω)/B(ω) (isteğe bağlı), FFT ızgara dalgası, ikinci mertebe drift, slamming/Ṁ_A | Gerçek veriyle kalibrasyon. |

**3.3 Performans bütçesi [türetim]:** Bileşen toplamıyla dalga hesabı ağ başına pahalıdır: 500 köşe × 64 bileşen × 1000 Hz ≈ 3,2·10⁷ cos/s, yani `libm` ile kabaca 0,3–0,6 s hesap süresi başına simülasyon saniyesi. Gerçek zamandan hızlı koşmak için şu seçenekler var:
- Araç derindeyken (ör. z < −λ_min/2) ağ yerine nokta modeline geçmek.
- Dalga alanını adım başına araç çevresindeki küçük bir ızgarada (ör. 16×16) bir kez hesaplayıp köşelere bilineer interpolasyon yapmak (Kerner'in "water patch" yaklaşımı).
- Hidrostatik/dalga kuvvetini 200–250 Hz'de hesaplayıp RK4 alt adımlarında sabit tutmak (multi-rate). Bu da deterministiktir, ancak yakınsama testleriyle doğrulanmalı.

**3.4 Arayüz kuralları**
- Firmware'e giden ve gelen her şey PWM, ham sensör ve zaman damgasıdır. Tahsis, kontrol ve kestirim simülatörde yer almaz.
- Parametre dosyalarında her katsayının birimi, işareti (Fossen negatif / büyüklük pozitif), kaynağı ve belirsizliği tutulur. İç temsilde tek işaret kuralı (büyüklük pozitif) kullanılır, dönüşüm yükleyicide yapılır.
- Fizik saati sabit dt ile ilerler. HIL'de gerçek zaman eşlemesi zamanlayıcıdadır; çekirdek "bir adım ilerle" dışında zamandan habersizdir.

---

## 4. Riskler ve açık sorular

1. **Katsayı belirsizliği baskın hata kaynağı.** Yayımlanmış iki BlueROV2 seti arasında karesel sönümde 5–8 kat fark var. Eidsvik yöntemi dönme eksenlerinde %30–100 hatalı. Araç geometrisi belli olmadan hiçbir katsayı "doğru" sayılmamalı; kontrolcü belirsizlik aralığında test edilmeli.
2. **Yüzeyde hidrodinamik eksik kalıyor:** dalga yapma sönümü (radiation), frekansa bağlı eklenmiş kütle, difraksiyon ve ikinci mertebe kuvvetler. Ağ tabanlı FK + ayarlı sönüm nitel olarak doğru ama nicel doğruluğu bilinmiyor. Gerekirse Capytaine ile A(ω)/B(ω) hesaplanıp Cummins/sıvı belleği yaklaşımına geçilebilir; bu karmaşıklığı artırır.
3. **Geçiş anı fiziği:** Ṁ_A, slamming, sıçrama ve hava sürüklenmesi modelde yok. Yüksek hızlı dalma/çıkışta sonuçlar iyimser olacak.
4. **Pervane havalanması ve yüzeye yakın itki kaybı** için T200'e özgü veri yok; literatür modelleri büyük pervaneler için.
5. **T200 tablosu statik (bollard) ölçüm.** Seyir hızında itki düşüşü (K_T(J)), itici–gövde ve itici–itici etkileşimi modellenmiyor. Wu, bunların önemli kayıplara yol açabildiğini not ediyor.
6. **Determinizm tuzakları:** platform libm'i, `f64::mul_add` kullanımı, HashMap iterasyon sırası, paralel koşum, platforma bağlı SIMD. CI'da çapraz platform hash testi yoksa sessizce kırılır.
7. **Torpido:** Yarışma kuralları (boyut, kütle, fırlatma mekanizması, atış derinliği veya yüzeyden atış) bilinmiyor. C_D ve kanat etkinliği tahmini. Fırlatma itmesinde eklenmiş kütlenin rolü açık soru.
8. **Kısmi batıklıkta M_A ölçeklemesi** için fiziksel olarak doğrulanmış basit bir yasa bulunamadı. Hunt & Whitcomb sabit tutuyor, Wang & Shang doğrusal ölçekliyor; hangisinin bizim gövdeye uyduğu deneyle belirlenmeli.
9. **Euler açısı tekilliği:** Dışa aktarılan telemetri Euler kullanıyorsa ±90° pitch yakınında firmware ile uyumsuzluk görülebilir. Çekirdek quaternion kullanmalı.
10. **Fossen Handbook 3. baskı (2027)** MSS'te atıf olarak geçiyor. Bölüm ve denklem numaraları değişebilir, kodda atıf verirken baskı belirtilmeli.

---

## 5. Bağımsız doğrulanması gereken kritik iddialar

1. **BlueROV2 Heavy parametreleri:** m = 13,5 kg, ∇ = 0,0134 m³, X_{u|u|} = 141,0, Y_{v|v|} = 217,0, Z_{w|w|} = 190,0 Ns²/m², Z_ẇ = 18,68 kg; itici transfer fonksiyonu `K(s) = (6136s + 108700)/(s³ + 89s² + 9258s + 108700)`; maksimum surge hızı 0,72 m/s ve 85 N. Kaynak: von Benzon ve ark., JMSE 10(12):1898, 2022, Tablo 4 ve Tablo A1, Denk. 18–19.
2. **Wu (2018) BlueROV2 Heavy tablosu ölçülmemiştir:** değerler Sandøy (2016) ve üretici verisinden türetilmiş, dönme eksenleri yaw değerinin kopyası. Kaynak: C.-J. Wu, *6-DoF Modelling and Control of a Remotely Operated Vehicle*, Flinders Üniv. yüksek lisans tezi, 2018, Bölüm 5.2, Tablo 5.1–5.3.
3. **T200 maksimum itki:** 12 V'ta 3,71/2,92 kgf, 16 V'ta 5,25/4,1 kgf, 20 V'ta 6,7/5,05 kgf. Herkese açık tabloya göre 16 V'ta sıfır itki aralığı 1472–1528 µs. Kaynak: Blue Robotics T200 ürün sayfası ve `T200-Public-Performance-Data-10-20V-September-2019.xlsx`.
4. **Eklenmiş kütleyi önceki adımın ivmesinden dış kuvvet olarak uygulamak koşullu kararlıdır;** DART'ın `<fluid_added_mass>` ile örtük entegrasyonu koşulsuz kararlıdır; Bullet ve MuJoCo yerel eklenmiş kütleyi desteklemez. Kaynak: Gazebo Sim API belgesi, "Hydrodynamics" teori sayfası (gz-sim 8/9).
5. **Hibrit g(η) yaklaşımı:** Monte Carlo ile ∇_h(t) ve r_h(t); tam batıkken fark < 1e-11; yüzeyde CG derinliği ≈ 0,13 m (deneyle tutarlı), klasik modelde −0,2 m. Kaynak: Hunt & Whitcomb, "A Hybrid Dynamical Model for Robotic Underwater Vehicles when Submerged or Surfaced", ICRA 2024 (NSF PAR 10565587).
6. **JONSWAP (DNV) formu:** `A_γ = 1 − 0,287 ln γ`, σ = 0,07/0,09, β = 1,25; α = 5,061 Hs²/Tp⁴ (1 − 0,287 ln γ). Kaynak: SINTEF SIMA belgesi; MSS `waveSpectrum.m` (DNV-RP-C205 atfıyla).
7. **Rapier/parry kütle modeli** skaler `inv_mass: f32` ve asal eksenli 3×3 eylemsizlikten oluşur, 6×6 eklenmiş kütleyi desteklemez. Rapier belgesine göre aşkın fonksiyonlar platformlar arası deterministik değildir. Kaynak: docs.rs parry3d `MassProperties` (0.31.1); rapier.rs "Determinism" sayfası.
8. **MSS SIMremus100 sayısal şeması:** h = 0,05 s; Euler modu RK4; quaternion modunda ileri Euler (hız), `expm(T(ω)h)` ile quaternion güncellemesi ve normalizasyon. PVS ise ileri Euler ve h = 0,02 s kullanıyor. Kaynak: github.com/cybergalactic/MSS `CRAFT/AUV/SIMremus100.m`; github.com/cybergalactic/PythonVehicleSimulator `main.py`, `lib/gnc.py`.
9. **Eidsvik ampirik eklenmiş kütle yöntemi:** öteleme hatası %10–20, dönme hatası %30–100; boyutlar arası fark %10'u geçmemeli. Kaynak: von Benzon ve ark. 2022, Bölüm 4.3 (Eidsvik & Schjølberg'e atıfla).
10. **[türetim] Kanatlı torpido statik yön kararlılığı koşulu** `½ρ S_f C_Lα |x_f| > (A₂₂ − A₁₁)` (gövde kaldırması ihmal). Bu raporda türetildi, literatürle (ör. Prestero 2001'deki Munk momenti ve kanat terimleri) karşılaştırılarak kontrol edilmeli.

---

## 6. Kaynaklar

**Kitap / toolbox (birincil)**
- T. I. Fossen, *Handbook of Marine Craft Hydrodynamics and Motion Control*, 2nd ed., Wiley, 2021. (MSS kaynak yorumlarında 3rd ed., 2027'ye atıf var.) — https://www.fossen.biz/wiley
- Marine Systems Simulator (MSS), MIT lisansı — https://github.com/cybergalactic/MSS (incelenen dosyalar: `LIBRARY/environment/waveSpectrum.m`, `oceanCurrents.m`, `spheroidRAO.m`, `CRAFT/AUV/SIMremus100.m`)
- Python Vehicle Simulator, MIT lisansı — https://github.com/cybergalactic/PythonVehicleSimulator (incelenen dosyalar: `main.py`, `lib/mainLoop.py`, `lib/gnc.py`, `vehicles/remus100.py`, `vehicles/otter.py`, `vehicles/torpedo.py`)

**Makaleler / tezler**
- M. von Benzon ve ark., "An Open-Source Benchmark Simulator: Control of a BlueROV2 Underwater Robot", *J. Mar. Sci. Eng.* 10(12):1898, 2022 — https://www.mdpi.com/2077-1312/10/12/1898 ; PDF: https://vbn.aau.dk/ws/files/505520780/jmse_10_01898.pdf
- J. E. Hunt, L. L. Whitcomb, "A Hybrid Dynamical Model for Robotic Underwater Vehicles when Submerged or Surfaced: Approach and Preliminary Evaluation", ICRA 2024 — https://par.nsf.gov/servlets/purl/10565587
- C.-J. Wu, *6-DoF Modelling and Control of a Remotely Operated Vehicle*, Flinders Univ., 2018 — https://flex.flinders.edu.au/file/27aa0064-9de2-441c-8a17-655405d5fc2e/1/ThesisWu2018.pdf
- Wang & Shang, "Modeling and control of a low-cost multirotor hybrid aerial underwater vehicle", arXiv:2507.03869, 2025 — https://arxiv.org/html/2507.03869v1
- R. Gabl ve ark., "Hydrodynamic loads on a restrained ROV under waves and current", *Ocean Engineering*, 2021, doi:10.1016/j.oceaneng.2021.109279 — https://ira.lib.polyu.edu.hk/bitstream/10397/92240/1/Gabl_Hydrodynamic_Loads_Restrained.pdf
- S. Aracri ve ark. (CNR), "Experimental Validation of Unsteady Wave Induced Loads on a Stationary ROV", 2021 — https://iris.cnr.it/handle/20.500.14243/448967 (yalnızca özet incelendi)
- O. A. N. Eidsvik, I. Schjølberg, "Determination of hydrodynamic parameters for remotely operated vehicles" — https://ntnuopen.ntnu.no/ntnu-xmlui/handle/11250/2468120 (yalnızca kayıt incelendi)
- T. Prestero, *Verification of a Six-Degree of Freedom Simulation Model for the REMUS AUV*, MIT S.M. tezi, 2001 — https://dspace.mit.edu/handle/1721.1/65068 (içerik doğrudan okunmadı; PVS/MSS üzerinden)
- D. R. Yoerger, J. G. Cooke, J.-J. E. Slotine, "The influence of thruster dynamics on underwater vehicle behavior…", *IEEE JOE* 15:167–178, 1990 (künye arama sonucundan)
- A. J. Healey ve ark., "Toward an Improved Understanding of Thruster Dynamics for Underwater Vehicles", *IEEE JOE* 20(4), 1995 — https://calhoun.nps.edu/entities/publication/e3ba4e30-318c-418e-9bba-b634181b572e/full
- Kozlowska & Steen; Califano & Steen — pervane havalanması (ikincil özet): https://www.mdpi.com/2077-1312/8/1/31
- J. Tessendorf, "Simulating Ocean Water/Surfaces", SIGGRAPH Course Notes, 2001 — https://jtessen.people.clemson.edu/reports/papers_files/waterslides2001.pdf
- J. Kerner, "Water interaction model for boats in video games" (Bölüm 1 ve 2), Game Developer, 2015 — https://www.gamedeveloper.com/programming/water-interaction-model-for-boats-in-video-games ; https://www.gamedeveloper.com/programming/water-interaction-model-for-boats-in-video-games-part-2

**Üretici / standart / belge**
- Blue Robotics T200 ürün sayfası — https://bluerobotics.com/store/thrusters/t100-t200-thrusters/t200-thruster-r2-rp/
- T200 performans verisi — https://cad.bluerobotics.com/T200-Public-Performance-Data-10-20V-September-2019.xlsx
- SINTEF SIMA, dalga spektrumları — https://sima.sintef.no/docs/5.0/simo/modelling/environment/waves.html
- DNV-RP-C205, *Environmental Conditions and Environmental Loads* (metin doğrudan okunmadı; MSS ve SIMA üzerinden)
- Gazebo Sim, Hydrodynamics teori sayfası — https://gazebosim.org/api/sim/9/theory_hydrodynamics.html ; Buoyancy sistemi — https://gazebosim.org/api/sim/9/classgz_1_1sim_1_1systems_1_1Buoyancy.html
- Stonefish belgesi (teori) — https://stonefish.readthedocs.io/en/v1.5/theory.html ; P. Cieślak, "Stonefish: An Advanced Open-Source Simulation Tool Designed for Marine Robotics, With a ROS Interface", OCEANS 2019

**Rust**
- nalgebra `UnitQuaternion` (0.35.0) — https://docs.rs/nalgebra/latest/nalgebra/geometry/type.UnitQuaternion.html
- parry3d `MassProperties` (0.31.1) — https://docs.rs/parry3d/latest/parry3d/mass_properties/struct.MassProperties.html
- Rapier determinizm — https://rapier.rs/docs/user_guides/rust/determinism/
- simba özellikleri (`libm`, `libm_force`) — https://docs.rs/crate/simba/latest/features
- libm crate (0.2.16) — https://docs.rs/crate/libm/latest
- marine_vessel_simulator (0.1.0, içerik doğrulanmadı) — https://lib.rs/crates/marine_vessel_simulator
