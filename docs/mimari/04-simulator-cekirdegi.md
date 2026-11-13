# 04 — Simülatör çekirdeği

Sürüm: 0.2 (2026-11-13). Durum: taslak; ikinci inceleme bekliyor. Sürüm 0.2: iskelet 0.3'e uyum (1,25 ms adım, ortak kabuller, yük muhasebesi, darbe modeli) ve [inceleme/04-sol.md](inceleme/04-sol.md) bulgularının giderilmesi; madde madde yanıt [inceleme/04-yanit.md](inceleme/04-yanit.md).

Bu bölüm [00-iskelet.md](00-iskelet.md) Bölüm 9, satır 04'ün kapsamını yazar. Dayanak [../research/00-duzeltmeler-ve-kararlar.md](../research/00-duzeltmeler-ve-kararlar.md) belgesidir ("00"). Atıf: `04.n` = 00 Bölüm 4 düzeltme satırı, `Çn` çelişki çözümü, `Mn` ölçüm, `Ön`/`Kn` karar günlüğü, `DNN` = `docs/research/dogrulama/NN-codex.md`, `NN §x` = ham rapor bölümü. Ham rapordan alınan her değer önce 00'daki düzeltmeyle karşılaştırıldı.

Değerlerin niteliği: **ölçülmüş (yayımlanmış)**, **ampirik tahmin (yayımlanmış)**, **CAD hesabı (yayımlanmış)**, **mimari varsayımı** (raporlarda yok; bu bölüm seçti, gerekçesi yanında). Bu bölümde hiçbir hız, yük ya da süre ölçülmemiştir; geçtikleri yerde "tahmin" ya da "işlem sayımı" diye işaretlidir.

---

## 1. Amaç ve sorumluluklar

Sim sunucusunun çekirdeği dünyanın tek yetkili durumunu tutar ve sabit adımla ilerletir:

1. Araç, torpido ve hedeflerin 6 serbestlik dereceli (6-SD) hareketi; yüzey, kısmi batma ve dalış tek modelde.
2. Çevre: dalga, akıntı, rüzgar, su özellikleri, havuz ve açık su.
3. İDA-0 parametre seti ve üç kütle durumu (iskelet §5).
4. İtici, motor sürücüsü (ESC) ve batarya; elektrik–itki bağlaşımı.
5. Torpido fırlatma (darbe modeli), uçuşu ve isabet tespiti; hedef hareketi.
6. Çarpışma sorgusu ve özel temas tepkisi.
7. Zaman yöneticisi: tamsayı nanosaniye, 1,25 ms sabit adım, alt sistem zamanlaması, üç kipin sürücüleri, son tarih sayaçları.
8. Sensörlere (05), HIL köprüsüne (06), görselleştiriciye (07), kayıt ve panellere (08) iç arayüzler.
9. Tekrar üretilebilirlik (Ç7).

Yapmadıkları: sensör ölçümü (05), mesaj biçimi ve adım bariyeri protokolü (06), görüntü (07), senaryo ve kayıt dosyası biçimi (08). **İtki tahsisi yapmaz:** yalnız ileri dönüşüm (PWM → itici kuvveti → gövde kuvveti) uygular; tahsis kart yazılımındadır (04 §2.4, 02).

---

## 2. İç yapı

### 2.1 Alt modüller ve adım sırası

```
sim/core/
  time/       Clock (int64 ns), Scheduler (rasyonel periyot), sürücüler (FreeRun, Realtime, Lockstep)
  world/      World::step(), gövde listesi, kütle durumları, olay kuyruğu, snapshot halkası
  dynamics/   RigidBody6, kütle matrisi + Cholesky, C_RB/C_A, sönüm, RK4, kuaterniyon
  hydro/      hidrostatik ağ kırpma, basınç integrali, nokta modeli, yüzey sönümü, havalanma
  env/        WaveField, CurrentField, WindField, Medium
  actuators/  EscModel, ThrusterModel, Launcher
  power/      Battery, Bus (bağlaşım çözücüsü), güç olayları
  contact/    Jolt sorgu sarmalayıcısı, temas tepkisi, isabet tespiti
  params/     A5 parametrelerinin doğrulanması, birim ve işaret dönüşümü
  rng/, math/ sayaç tabanlı üreteç, sürümlü dağılımlar, ida_math, Eigen sabit boyutlu tipler
```

Bir fizik adımının sabit sırası:

1. Aktüatör çerçevesi ve tek seferlik olaylar uygulanır (3.2'deki kurallarla). Emniyeti artıran olaylar atıştan önce işlenir (iskelet §3.2).
2. Zamanlanmış olaylar (arıza, senaryo) işlenir. Ateşleme olayı varsa ayrılma bu adımda çözülür (2.8).
3. Elektrik bağlaşımı çözülür (2.7.6): itici akımları ve bara gerilimi.
4. Temas geometrisi sorgulanır (2.9).
5. Bütün gövdeler RK4 ile ilerletilir (2.10).
6. Kuaterniyon normalleştirilir; batarya durumları ilerletilir; isabet sorgusu yapılır.
7. Snapshot üretilir; zamanlayıcıya göre sensörler (05), görselleştirici (07), kayıt (08) beslenir.

### 2.2 Kabuller, çerçeveler ve durum

İskelet §3.1'deki ortak kabuller kullanılır: SI, açı radyan, zaman `int64` ns; dünya NED; gövde x ileri, y sancak, z aşağı; kuaterniyon (w, x, y, z), Hamilton çarpımı, gövdeden dünyaya dönüş.

- **Gövde referans noktası `O_b`:** A5'te tanımlıdır; genelleştirilmiş hız, kuvvet ve moment buna göre yazılır. İDA-0'da `O_b` **boş aracın** ağırlık merkezidir; yük durumu değişince `O_b` gövdede sabit kalır, `r_g` değişir (2.6).
- **Kinematik:** `ṗ_n = R(q) ν₁:₃`, `q̇ = ½ q ⊗ [0, ω_b]`. Euler açıları yalnız telemetri ve senaryo girişi içindir (04 §2.1).
- **Dalga koordinatı:** formüller yukarı pozitif `z_u = −z_n`, **ortalama** su yüzeyine göre yazılır (04.3). β kuzeyden doğuya.
- **Varlıklar:** glTF sağ elli, +Y yukarı (12.16). Dönüşüm yalnız varlık yükleyicide: `x_b = +Z_gltf, y_b = −X_gltf, z_b = −Y_gltf` (determinant +1; glTF "+Z ileri" eşleşmesi manifestte sabitlenir, mimari varsayımı). Godot dönüşümü A2 sınırında, 07'de.
- **Durum (gövde başına):** `p_n`, `q_nb`, `ν = [u v w p q r]`, kütle durumu kimliği; itici başına üç filtre durumu; batarya `V₁`, SoC. Hepsi `double` (Ö3).

### 2.3 Hareket denklemleri

Tek model yüzeyi ve dalışı kapsar; geri getirici kuvvetler anlık batmış geometriden gelir (04 §2.1; Hunt–Whitcomb'un sayısal sonuçları doğrulanmadı, 04.14).

```
(M_RB + M_A) ν̇ = τ_itici + τ_basınç + τ_W + τ_rüzgar + τ_temas
                 − C_RB(ν) ν − C_A(ν_r) ν_r − D(ν_r, s) ν_r + M_A ν̇_w,      ν_r = ν − ν_w

M_RB = [[m I₃, −m S(r_g)], [m S(r_g), I_o]]      (I_o: O_b'ye göre; 04 §2.1)
τ_W  = [Rᵀ f_g ; r_g × Rᵀ f_g],  f_g = [0, 0, m g]ᵀ
```

**Su hızı ve türevi.** `ν_w = [Rᵀ v_wⁿ(p_O, t) ; 0₃]`; `v_wⁿ` = akıntı + dalga yörünge hızı, `O_b` noktasında (`p_O`). Hareketli noktada gövde çerçevesindeki türev:

```
ν̇_w = [ −S(ω_b) Rᵀ v_wⁿ + Rᵀ( ∂_t v_wⁿ + (∇v_wⁿ) ṗ_n ) ; 0₃ ]
```

İlk terim çerçeve dönüşünden, son terim uzaysal taşınımdan gelir (düzgün akıntıda sıfır; MSS `oceanCurrents` biçimi bu durumun özelidir, 04 §2.3). Su açısal hızı ve gövde boyunca hız değişimi bu terimlerde yoktur; o değişim ağ üzerindeki basınç integralinde (2.4) ya da, nokta modelinde, 2.4'teki uzaysal ölçütle sınırlanır.

- **Tek sayım (04.5):** eklenmiş kütle katkısı yalnız `M_A ν̇_r` içindedir. Froude–Krylov (FK) kuvveti ya ağdaki basınç integralinden ya da nokta modelinin `ρ∇ ∂_t v_w` teriminden gelir, ikisinden birden asla. FK ivmesi Euler (yerel) ivmedir; göreli hız türevi yukarıdaki gövde boyunca türevdir; ikisi ayrı tanımlıdır. Tam `M ν̇_r` sadeleştirmesi kullanılmaz (sabit akıntı varsayımı ivmeli dalgada geçersiz, 04.5).
- `C_A(ν_r)`: Kirchhoff biçimi (PVS `m2c`), çarpık simetrik; Munk momenti korunur (04.11).
- `D(ν_r, s) = s_D(s)(D_L + D_NL(ν_r)) + D_yüzey(s)`, köşegen (04 §2.1).
- Eklenmiş kütle hiçbir zaman önceki ivmeden dış kuvvet olarak uygulanmaz (İDA-0'da `Z_ẇ > m`; 04 §2.6, D04 #4).

### 2.4 Hidrostatik, dalga basıncı ve yüzey geçişi

**Ağ.** A7'den kapalı, manifold "BUOY_*" ağları; gerçek su deplasmanını temsil eder (basınç gövdeleri, köpük; açık çerçevenin su dolan boşlukları değil; 04.17). Kütle durumu değişince ağ kümesi de değişir (torpido hacmi, 2.6).

**Kırpma ve kuvvet.** Her üçgen köşelerindeki anlık `ζ` ile su çizgisinde kesilir (Kerner, 04 §2.2). Batmış parça başına `dF = −p n dA` kuvveti parçanın basınç merkezinde uygulanır; moment **gövde referans noktasına** göre toplanır: `dτ = [Rᵀ dF ; r_P × Rᵀ dF]`, `r_P` uygulama noktasının `O_b`'ye göre konumu. Üçgenler en kısa önemli dalga boyuna göre küçük olmalıdır (04 §2.2); varlık hattı bunu manifestte kaydeder (07'den beklenen).

**Basınç (04.3'ün düzeltilmiş hali):**

```
p − p_atm = ρ g (ζ − z_u) + ρ g Σ_i a_i [F_p,i(z_u) − 1] cos θ_i        (z_u ≤ ζ)
θ_i = ω_i t − k_i (x cos β_i + y sin β_i) + φ_i,      ζ = Σ_i a_i cos θ_i
```

Bu ifade `−ρ g z_u + ρ g Σ a_i F_p,i cos θ_i` ile özdeştir; klasik yüzerlik ayrıca eklenmediği için dalga yüksekliği iki kez sayılmaz (04.3). Ortalama yüzeyin üstünde (`0 < z_u ≤ ζ`) doğrusal teorinin uzatılması doğrulanmadı (04 §2.3); formül olduğu gibi uygulanır (Bölüm 8).

**Sonlu derinlik alanı (04.4).** Hız potansiyeli `Φ = −(g a/ω) · cosh k(z_u+h)/cosh kh · sin θ` ile türetildi; yüzey kinematik koşulu `∂_t ζ = w(0)` ve süreklilik `∂_x u + ∂_z w = 0` bu bölümde denetlendi:

```
F_p(z_u) = cosh k(z_u + h) / cosh kh                                        (basınç)
u_yatay  =  a ω · cosh k(z_u + h)/sinh kh · cos θ      ∂_t u_yatay  = −a ω² · cosh(…)/sinh kh · sin θ
w_yukarı = −a ω · sinh k(z_u + h)/sinh kh · sin θ      ∂_t w_yukarı = −a ω² · sinh(…)/sinh kh · cos θ
ω² = g k tanh kh                                                            (04 §2.3)
```

Yatay hız β yönündedir; NED'e geçiş `w_n = −w_yukarı` ile yapılır. `∇v_w` analitik türevlerle hesaplanır. `kh > 10` iken taşmayı önlemek için üstel biçim kullanılır (mimari varsayımı: sayısal güvenlik eşiği). Dispersiyon sabit sayıda Newton yinelemesiyle çözülür.

**Nokta modeline geçiş.** Ağ yerine nokta modeli yalnız iki koşul birlikte sağlanınca kullanılır (04.10'un düzeltmesinin sonlu derinliğe ve gövde boyuna genişletilmesi):

1. *Yüzey kesişmesi olanaksız:* gövdenin en üst noktası `d_üst ≥ Σ_i |a_i|` derinlikte (ζ'nin üst sınırı).
2. *Her bileşen için* ya `F_p,i(−d_üst) ≤ ε` (bileşen gövdenin en üstünde ihmal edilebilir; sonlu derinlik fonksiyonuyla, derin su biçimi kullanılmaz) ya da `k_i L_maks ≤ 2π/5` (dalga boyu gövdenin en büyük boyutunun en az 5 katı: alan gövde boyunca yaklaşık düzgün).

`ε = 0,01`, `2π/5` eşiği ve geri dönüşte %10 histerezis mimari varsayımıdır (λ > 5L, Morison'un ince eleman sınırının benzeri; hacimli araçta doğruluk garantisi değil, D04 E:Morison; M15'te sınanır). Nokta modelinde `∇`, `r_b` tam batık değerdir; FK kuvveti `[ρ∇ Rᵀ ∂_t v_wⁿ ; r_b × ρ∇ Rᵀ ∂_t v_wⁿ]`, ivme yüzerlik merkezinde.

**Batıklık oranı:** `s = ∇_batık / ∇_tam`, ağ integralinden.

**Eklenmiş kütle ve sönümün geçişi.** Birincil: `M_A` sabit (tam batık değer); `Ṁ_A` terimi doğmaz. Yedek: `M_A(s)` C¹ ölçekleme (kütle matrisi her aşamada yeniden ayrıştırılır). İkisi de deneysel olarak doğrulanmamış yaklaşımdır; yumuşatma enerji korunumunu ya da doğru giriş/çıkış fiziğini sağlamaz (04.9). `Ṁ_A ν`, `½A'(z)ż²`, çarpma ve sıçrama yoktur (04 §4 md.3). Sürükleme `s_D(s) = s` (mimari varsayımı; Wang–Shang doğrusal ağırlığı kanıt değildir, 04.15).

**Yüzey sönümü.** Otter yaklaşımı (04 §2.1): `Z_w = 2ζ₃ω₃M₃₃`, `K_p = 2ζ₄ω₄M₄₄`, `M_q = 2ζ₅ω₅M₅₅`, `ζ = 0,3 / 0,2 / 0,4` (PVS Otter değerleri; İDA-0'a uygulanması mimari varsayımı). Doğal frekanslar yüzeydeki denge konumunda ağdan hesaplanır. Terim su çizgisi gövdeyi kesiyorsa tam, tamamen batıkta sıfırdır; arada C¹ geçiş (mimari varsayımı).

**Pervane havalanması.** İtki `β(h/R)` ile çarpılır; `h` itici ekseninin yerel yüzeyden derinliği, `R = 38 mm` (04 §2.2). `h/R ≥ 3,4` derin (literatür eşiği, katsayılar doğrulanmadı); `β` 0'da 0, 3,4'te 1, arada doğrusal (mimari varsayımı; tablo A5'te). Yer tutucu ağla tatlı suda yüzeyde dikey iticiler için `h/R ≈ 1,17`, `β ≈ 0,35` çıkar (inceleme hesabı); bu, ölçülmüş T200 davranışı değil, varsayımların sonucudur.

### 2.5 Çevre

**Dalga spektrumu:** Pierson–Moskowitz ve DNV düzeltmeli JONSWAP (D04 #6). Yönsel yayılım A5'te.

**Bileşen listesi:** senaryo tohumundan çekirdekte üretilir (12 §2.5): `{a_i, ω_i, k_i, β_i, φ_i}`, `a_i = √(2 S(ω_i) D(β_i) Δω Δβ)`, frekanslar kutu içinde rastgele (04 §2.3). Varsayılan 64 bileşen (04 §3.2'deki aralığın üst ucu; mimari varsayımı). **Fizik bütün bileşenleri toplar, kesme yapmaz.** Liste sürüm ve özetiyle A2'den görselleştiriciye gider; görselleştirici rastgele sayı üretmez. Görselleştirici FFT, LOD ya da enterpolasyon kullanırsa fark kayan noktadan ibaret değildir: atılan bileşenler için `|Δη| ≤ Σ|a_i|` (12.14, 10.7). Örnekleme ve zaman sözleşmesi 07'den beklenen.

**Akıntı:** NED'de düzgün, dönümsüz akıntı, isteğe bağlı sınırlı Gauss–Markov (MSS `oceanCurrents`, 04 §2.3). Derinlik profili (1/7 yasası) doğrulanmadı; sonraki aşamada ağ elemanı başına yerel hız olarak girer. DVL su takibi akıntı alanını sorgular (00 Bölüm 6).

**Rüzgar:** `τ_rüzgar = ½ ρ_a C_D A_üst(t) |V_rw| V_rw`, kırpılmış ağın su üstü izdüşüm alanının merkezinde; moment `O_b`'ye göre. `C_D = 1,0`, `ρ_a = 1,225 kg/m³` mimari varsayımı (yönlü katsayı tablosu yok, 04 §2.3). Sabit + isteğe bağlı Gauss–Markov.

| Ortam | ρ (kg/m³) | Derinlik | Sınırlar | Dalga |
|---|---|---|---|---|
| Havuz | 1000 (04 §2.5) | sabit `h`, A5 | A7'deki statik duvar ve taban | varsayılan yok |
| Açık su | 1025 (mimari varsayımı: tipik deniz suyu) | batimetri (A5) | taban, engeller | spektrum A5'ten |

### 2.6 İDA-0 parametre seti ve kütle durumları

Parametreleri mimar belirler (K10). Hepsi A5'ten okunur ve değiştirilebilir; her katsayı değer, birim, işaret kuralı, kaynak ve belirsizlik taşır (04 §3.4). İç temsilde büyüklük pozitif; Fossen negatif türev biçiminden dönüşüm yükleyicide. von Benzon seti hedef aracın kalibrasyonu değil, benchmark'tır (D04 N4).

**Boş araç** (kaynak: von Benzon ve ark. 2022, Tablo A1, 04 §2.5; nitelik ayrımı inceleme kaydına göre, bu bölümde kaynak PDF'inden bağımsız okunamadı):

| Parametre | Değer | Birim | Kaynak / düzeltme | Nitelik |
|---|---|---|---|---|
| `m` | 13,5 | kg | Tablo A1; D04 #1 | ölçülmüş (yayımlanmış) |
| `∇` | **0,0137** | m³ | yayımlanan 0,0134; aşağıdaki not | mimari varsayımı |
| `I_x, I_y, I_z` (O_b'ye göre) | 0,26 / 0,23 / 0,37 | kg·m² | Tablo A1 | CAD hesabı (yayımlanmış) |
| Atalet çarpımları | 0 | kg·m² | kaynakta yok | mimari varsayımı |
| `r_g` | 0 | m | `O_b` tanımı | tanım |
| `r_b` | (0, 0, −0,01) | m | Tablo A1 | CAD hesabı (yayımlanmış) |
| `g` | 9,82 | m/s² | Tablo A1 (Tablo 4 regresyonu için) | yayımlanmış |
| `X_u̇, Y_v̇, Z_ẇ` | 6,36 / 7,12 / 18,68 | kg | Eidsvik yöntemi; geçerlilik koşulu ihlal (04.1, 04.18) | ampirik tahmin (yayımlanmış) |
| `K_ṗ, M_q̇, N_ṙ` | 0,189 / 0,135 / 0,222 | kg·m² | aynı | ampirik tahmin (yayımlanmış) |
| `M_A` köşegen dışı | 0 | — | köşegen kabul | mimari varsayımı |
| `X_u, Y_v, Z_w` | 13,7 / 0 / 33,0 | N·s/m | deneysel (04.1) | ölçülmüş (yayımlanmış) |
| `K_p, M_q, N_r` | 0 / 0,8 / 0 | N·m·s/rad | aynı | ölçülmüş (yayımlanmış) |
| `X_u|u|, Y_v|v|, Z_w|w|` | 141,0 / 217,0 / 190,0 | N·s²/m² | aynı; D04 #1 | ölçülmüş (yayımlanmış) |
| `K_p|p|, M_q|q|, N_r|r|` | 1,19 / 0,47 / 1,5 | N·m·s²/rad² | aynı | ölçülmüş (yayımlanmış) |
| Belirsizlik bandı | öteleme ±%30, dönme ±%100 | — | 04 §2.5 önerisi | mimari varsayımı |

Boş araçta `M = diag(19,86; 20,62; 32,18; 0,449; 0,365; 0,592)`, pozitif tanımlı; atalet üçgen eşitsizlikleri sağlanır (inceleme hesabı, bu bölümde denetlendi).

**Hafif pozitif yüzerlik.** Yayımlanan `∇` tatlı suda ≈ 0,1 kg negatif yüzerlik verir (04 §2.5). Kütle korunur, deplasman 0,0137 m³ alınır (değişim %2,24): boş araç tatlı suda `B − W = 1,964 N` (su üstü hacim 0,200 L), deniz suyunda `5,327 N`. Bu, yüzey sertliğinin de az değiştiği anlamına gelmez (yüzey davranışı ağ geometrisine bağlıdır). İnce ayar için A5'te sabit `trim_kütlesi` (varsayılan 0; balast aktüatörü değil).

**Yer tutucu ağlar** (gerçek varlık A7 ile gelene kadar; ikisi de mimari varsayımı):
- *Yüzerlik:* tek kapalı kutu, taban 0,40 × 0,48 m (genişlik/boy 1,20, bağımsız seçim), yükseklik 0,07135 m, merkezi `r_b`'de. Tatlı suda serbest borda ≈ 1,04 mm (inceleme hesabı).
- *Çarpışma:* `O_b` merkezli kutu 0,46 × 0,58 × 0,38 m (boy × genişlik × yükseklik; Heavy dış ölçüleri, von Benzon Tablo 5'ten inceleme kaydına göre).

**Kütle durumları (iskelet §5).** Torpido tüpten ayrılana kadar aracın bağlı kütlesidir; hiçbir anda iki gövdede birden sayılmaz. Bağlı torpidonun katkısı nokta kütle + kendi ataleti (paralel eksen) ve kendi hacmiyle eklenir; aracın `M_A` ve sönümü yük durumundan bağımsız kabul edilir (mimari varsayımı: torpido tüpün içinde, dış akışı değiştirmez). Torpido verileri 2.8'de; yüklü konumlar `r_t = (0, ±0,10, 0,10)` m.

| Durum | `m` (kg) | `r_g` (m) | `∇` (m³) | `r_b` (m) | `I_x / I_y / I_z`; `I_yz` (kg·m²) | Tatlı suda `B − W` |
|---|---|---|---|---|---|---|
| İki yüklü | 13,85 | (0, 0, 0,00253) | 0,014035 | (0, 0, −0,00737) | 0,26706 / 0,23423 / 0,37423; 0 | +1,82 N |
| Bir yüklü (y = +0,10) | 13,675 | (0, 0,00128, 0,00128) | 0,013868 | (0, 0,00121, −0,00867) | 0,26353 / 0,23211 / 0,37211; I_yz = −0,00175 | +1,89 N |
| Boş | 13,5 | 0 | 0,0137 | (0, 0, −0,01) | 0,26 / 0,23 / 0,37; 0 | +1,96 N |

(`I_yz` atalet matrisindeki köşegen dışı terimdir, `−Σ m y z`. Değerler bu bölümde hesaplandı.) Hangi tüpün önce ateşlendiği bir yüklü durumun aynalı halini seçer. Başlangıç durumu A5'te, varsayılan iki yüklü.

**İtici yerleşimi** (sekiz T200; iskelet §5). von Benzon Tablo A1 konum ve yönleri içerir (04 §2.4) ama raporlara aktarılmadı; tablonun tamamı **mimari varsayımıdır**. Dikey iticilerde kaynağın pozitif yönü `−z`'dir (inceleme kaydı); burada pozitif komut `+z` (aşağı) itkisidir ve dönüşüm A5'te kayıtlıdır.

| # | `r_i` (m, `O_b`'ye göre) | `ε_i` (pozitif komut) | Rol |
|---|---|---|---|
| 1 | ( 0,156,  0,111, 0,085) | (1, −1, 0)/√2 | yatay, ön sancak |
| 2 | ( 0,156, −0,111, 0,085) | (1,  1, 0)/√2 | yatay, ön iskele |
| 3 | (−0,156,  0,111, 0,085) | (1,  1, 0)/√2 | yatay, arka sancak |
| 4 | (−0,156, −0,111, 0,085) | (1, −1, 0)/√2 | yatay, arka iskele |
| 5–8 | (±0,120, ±0,218, 0) (5: ön sancak, 6: ön iskele, 7: arka sancak, 8: arka iskele) | (0, 0, 1) | dikey |

`τ = Σ_i [ε_i F_i ; r_i × ε_i F_i] = B F` (04 §2.4). Tam `B` matrisinin rankı 6'dır; altı eksen tahsis edilebilir (inceleme hesabı, denetlendi). Gruplar bağımsız değildir: yatay iticilerin `z = 0,085 m` ofseti `K = −0,085 Y`, `M = 0,085 X` çapraz momentleri üretir; saf surge/sway için dikey grup bunları dengeler. Rank, doyum altında her istenen kuvvetin üretilebileceğini göstermez. `B` matrisi, pozitif yönler ve yük durumuna göre değişmeyen `O_b` A5'ten 02'ye aynı kaynakla verilir (02'den beklenen: kendi matrisini bu kaynaktan kurması). Pervane tepki torku ihmal (mimari varsayımı: tork verisi yok).

### 2.7 İticiler, ESC ve güç

**2.7.1 Komut.** Kart her itici için zaman damgalı PWM (µs) gönderir (06'dan beklenen). Bu bir sürekli komuttur (iskelet §3.2): en son geçerli olan uygulanır, ömrü `komut_ömrü` (A5; varsayılan 50 ms, mimari varsayımı: 400 Hz'te 20 periyot) aşılınca ESC "PWM kaybı" davranışına geçer. ESC üretici değerleri doğrulanmış: 1100/1500/1900 µs, **dur ölü bandı 1500 ± 25 µs**, 400 Hz'e kadar, 7–26 V, ısıl kısma basamakları (D11 #6). `u = clamp((PWM − 1500)/400, −1, 1)`, ölü bantta 0. 16 V'taki 1472–1528 µs sıfır-itki aralığı doğrulanmadı ve ölü bandın yerine geçmez (11.6, Ç12). Açılışta nötr bekleme `esc_başlatma_süresi = 2 s` (mimari varsayımı; üretici "birkaç saniye" diyor).

**2.7.2 Durağan itki isteği.** Doğrulanmış olan yalnız uç değerlerdir: azami ileri/geri itki 12 V'ta 3,71/2,92, 16 V'ta 5,25/4,1, 20 V'ta 6,7/5,05 kgf; tam gaz akımı 17/24/32 A (D04 #3, D11 #1). Ham tablo okunamadı (Ç12).

- **Birincil (M22 sonrası):** üretici tablosundan `F_ss = T(PWM, V)` ve `I = T_I(PWM, V)`.
- **Geçici (M22'ye kadar):** `F_ss = F_şekil(u) · F_maks,yön(V_bara)`. Şekil von Benzon polinomundan (`F(u) = −140,3u⁹ + 389,9u⁷ − 404,1u⁵ + 176,0u³ + 8,9u`, 04 §2.5) azami değerine bölünerek; polinom `u ≈ 0,972`'den sonra %0,72 azalır, şekil monoton kırpılır. İleri ve geri ayrı ölçeklenir. `F_maks(V)` doğrulanmış noktalar arasında kuvvet yasasıyla (üsler 1,207 ve 1,093; D04 Y12).

**2.7.3 Dinamik ve işlem sırası.** von Benzon modelindeki sıra korunur: durağan itki isteği eğriden üretilir, birim DC kazançlı üçüncü derece transfer fonksiyonu **itkiye** uygulanır: `F = K(s) F_ss`, `K(s) = (6136 s + 108700)/(s³ + 89 s² + 9258 s + 108700)` (D04 #1; sıra inceleme kaydının Denk. 13–19 okumasına dayanır, bu bölümde kaynak bağımsız okunamadı). Ters sıra (`F(K(s)u)`) küçük sinyalde kazancı yaklaşık 0,29'a indirir ve aynı model değildir. Filtre durumları RK4 vektörüne girer; ESC'nin kendi rampası yayımlanmadı. Havalanma çarpanı `β` filtre çıkışına uygulanır.

**2.7.4 Akım ve geçerlilik alanı.** Geçici akım modeli `I = I_maks(V) · (|F| / F_maks,yön(V))^{3/2}` (mimari varsayımı: ideal itki diskinde güç itkinin 3/2 kuvvetiyle artar; tablo okununca kalkar), `I_maks` 17/24/32 A arasında doğrusal. Akım ilk sürümde negatif olmaz: frenlemede rejeneratif akım, bataryaya dönen enerji ve bara gerilimi yükselmesi **modellenmez** (BLHeli_S frenlemesi belgeli, değerleri yok; 11.17, M23). Gerilim alanı: 12–20 V doğrulanmış uç değerler arası; 7–12 V aynı yasalarla dışdeğerleme ve `dışdeğerleme` bayrağı; 20–26 V'ta 20 V değerleri ve bayrak (T200 verisi 20 V'ta biter); 7 V altında ESC çalışma alanı dışıdır, itki ve akım 0 alınır ve `model_dışı` olayı üretilir (mimari varsayımı; 7 V bir kesme eşiği değildir, gerçek davranış bilinmiyor, 11.7). 30 A bir akım sınırı değildir; kısma yapılmaz (11.7). İlerleme oranına bağlı itki (`K_T(J)`) ve itici etkileşimi yok (04 §2.4, §4 md.5).

**2.7.5 ESC davranışları ve arıza kancaları.** Parametreli durum makinesi: başarısız kalkışta nötr komut bekleme (11.17), PWM kaybında `nötr` ya da `son_değer` (doğrulanmadı, 11 §2.B.7), ısıl kısma yalnız arıza enjeksiyonuyla (ısıl model yok; mimari varsayımı). T200 içinde su normaldir; "su alma" arızası yok (11.8). Kancalar: itki çarpanı, tutukluk, dolanma, ESC sinyal kaybı. Zaman çizelgesi 08'in A5 biçiminde.

**2.7.6 Batarya.** Üç kip; hepsi A5'te seçilir:

| Kip | Denklem | Zorunlu girdi | İzin |
|---|---|---|---|
| İdeal kaynak | `E = V_sabit`, `R_top = R_kablo` | `V_sabit` | her zaman (testler) |
| B0 | `E = OCV_p(SoC)`, `R_top = R₀,p + R_kablo` | OCV ve R₀ haritaları, `Q` | haritalar A5'te dolu ise |
| B1 | `E = OCV_p(SoC) − V₁`, `dV₁/dt = I/C₁,p − V₁/(R₁,p C₁,p)` | B0 + R₁, C₁ | aynı |

`dSoC/dt = −I/Q`. Tek kollu Thevenin devresi D11 #13 ile doğrulanmıştır; Chen'in 30 mV doğruluğu iki RC'li modele aittir (11.10). Hücre verisi: Samsung 30Q, X-57 ekleri (OCV, R₀, R_Th haritaları 0/20/30/45 °C; C_Th = 2000 F; 20 °C'de geniş bölgede R₀ = 0,025 Ω; D11 E:30Q). Haritalar bu bölüme aktarılmadı; aktarılana kadar yalnız ideal kaynak kipine izin verilir. Paket dönüşümleri (4S6P):

```
OCV_p = 4 OCV_h,   R₀,p = (4/6) R₀,h,   R₁,p = (4/6) R₁,h,   C₁,p = (6/4) C₁,h   (zaman sabiti korunur)
Q = 18 Ah = 64 800 C   (üretici paketi)
```

| Paket parametresi | Değer | Kaynak | Nitelik |
|---|---|---|---|
| Referans paket | 4S, 18 Ah, 266,4 Wh; 60 A sürekli, 132 A / 10 s; 12 V asgari güvenli | Blue Robotics (D11 Y9) | yayımlanmış |
| Hücre düzeni | 4S6P 30Q | 18 Ah ile eşleşme; hücre kapasitesi X-57'den doğrulanmalı | mimari varsayımı |
| `R_kablo` | 0,01 Ω | yer tutucu | mimari varsayımı |
| Aviyonik + sensör yükü | 20 W sabit güç | yer tutucu | mimari varsayımı |
| Başlangıç | SoC = 1,0; `V₁ = 0` (dinlenmiş) | A5'te değiştirilebilir | mimari varsayımı |
| Batarya sıcaklığı | su sıcaklığı, sabit | ısıl model yok | mimari varsayımı |

60 A ve 132 A / 10 s kesme olarak uygulanmaz; aşılınca olay üretilir. PSM ölçer modeli 05'te (100 A sürekli değil, 11.12).

**2.7.7 Elektrik–itki bağlaşımı.** Her fizik adımının başında, o andaki itici filtre durumları `F_i` ile bara gerilimi çözülür:

```
f(V) = V − E + R_top · ( Σ_i I_i(V) + P_aux / V ) = 0,     V ∈ [V_alt, E],   V_alt = 7 V
```

Birincil: sabit nokta yinelemesi `V ← E − R_top I_top(V)`, en çok 8 yineleme, kalıntı eşiği 1 mV (mimari varsayımı: çözücü toleransı, modelin kendi doğruluğundan çok küçük seçildi; M26'da gözden geçirilir). Yerel yakınsama koşulu `|R_top · dI_top/dV| < 1` (11.18); inceleme örneğinde 16 V civarında sekiz tam komutlu iticiyle 0,37. Yakınsama olmazsa sağlam yedek: `[V_alt, E]` aralığında sabit 40 adımlı ikiye bölme (deterministik). `f(V_alt) > 0` ise aralıkta kök yoktur: **güç çökmesi** olayı, ESC'ler kapanır (itici akımı 0), gerilim yalnız aviyonik yüküyle `V² − EV + R_top P_aux = 0`'ın büyük kökünden; `E² < 4 R_top P_aux` ise `V = 0` ve aviyonik de kapalı. Sayısal yakınsamama (yedekle çözüldü) çalıştırmayı geçersiz kılmaz, sayaçla bildirilir; güç çökmesi gerçek bir senaryo sonucudur, olay olarak kaydedilir ve çalıştırma `model_dışı_durum` bayrağı alır. Bulunan `V_bara` adım boyunca sabittir.

**2.7.8 Güç olayları.** `bara_bağlı` (acil kesme / sigorta: V_bara = 0, itki 0, geri gelişte ESC başlatma süresi), sigorta I²t (eşik A5'te), "kart resetlendi" olayı. HIL'de kart USB'den beslenir; sanal batarya çökmesi, kart reseti ve gerçek besleme çökmesi üç ayrı testtir; BOR eşikleri MCU beslemesine aittir (11.19). Karta iletim 06'dan beklenen.

### 2.8 Torpido ve hedefler

**Torpido gövdesi.** Ayrıldıktan sonra aynı integratörü kullanan ayrı 6-SD gövde (04 §2.7). Geometri tek seçimdir: **prolate sferoid**, Lamb k-faktörleri bu geometriye aittir ve kütle ölçeği yer değiştiren su kütlesidir (04.6). Yarışma kuralları kullanılmaz (K7). Varsayılanlar (A5'te; sayısal hepsi mimari varsayımı, türetilenler bu bölümde hesaplandı):

| Parametre | Varsayılan | Not |
|---|---|---|
| Boy / çap | 0,20 m / 0,04 m (a = 0,10, b = 0,02) | L/D = 5: 04 §2.5'te k-faktörleri hesaplanmış oran |
| Hacim | 1,6755·10⁻⁴ m³ | sferoid, `4/3 π a b²` |
| Kütle | 0,175 kg | hafif negatif (tatlı suda net −0,073 N; 04 §2.7 "hafif negatif veya nötr") |
| Atalet (düzgün yoğunluk) | `I_x = 2,80·10⁻⁵`, `I_y = I_z = 3,64·10⁻⁴` kg·m² | `I_x = 2/5 m b²`, `I_y = 1/5 m (a² + b²)` |
| `r_g` (CB merkezde) | (0, 0, 0,002) m | roll kararlılığı için CG altta |
| k₁, k₂, k′ | 0,0591 / 0,8943 / 0,6999 | Lamb, L/D = 5 (04 §2.5; inceleme yeniden hesabı) |
| `A₁₁ / A₂₂ = A₃₃ / A₅₅ = A₆₆` | 0,00991 kg / 0,1498 kg / 2,44·10⁻⁴ kg·m² | `k₁ρ∇`, `k₂ρ∇`, `k′·1/5 ρ∇(a² + b²)`, ρ = 1000 |
| Kanatlar | dört adet, her biri 0,03 × 0,03 m; düzlem başına `S_f = 1,8·10⁻³ m²`, AR = 2, `x_f = −0,09 m` | `C_Lα = πAR/(1 + √(1 + (AR/2)²)) = 2,60 /rad` (04 §2.4 formülü) |
| `C_D` (cephe, `A = 1,257·10⁻³ m²`) | 0,42 | REMUS değeri yalnız yer tutucu (04.8); 0,3–1,0 aralığı doğrulanmadı, tolerans değildir |
| Bağıl ayrılma hızı `v₀` | 2 m/s | 04.7 örneği |
| Tüp ekseni, yüklü konum | +x; `r_t = (0, ±0,10, 0,10)` m | gövdenin altı |

Sürükleme `X = −½ ρ C_D A u|u|` (04.8), çapraz akış şerit integrali ve Hoerner katsayılarıyla, kanat kuvveti yerel bağıl akışla (04 §2.4, §2.7).

**Kararlılık.** Statik koşul `½ ρ S_f C_Lα |x_f| > A₂₂ − A₁₁` (04.11; küçük açı, sabit katsayı, sıfır yaw hızı). Varsayılanlarla sol taraf 0,2108 kg, sağ taraf 0,1399 kg, oran 1,51. Yükleyici ayrıca doğrusal sway–yaw sisteminin özdeğerlerini (`tr(A) < 0`, `det(A) > 0`) hesaplar; oran 1,5'in altında ya da özdeğer koşulu bozuksa uyarır (reddetmez; mimari varsayımı hedef oran). PVS `torpedo.py` Munk momentini sıfırladığı için taşınmaz (04.11).

**Fırlatma: darbe modeli (iskelet §5, ilk kademe).** Ateşleme tek seferlik olaydır (iskelet §3.2): olay kimliği bir kez uygulanır; tüp dolu ve emniyet kapalı değilse ayrılma o adımın başında, tek anda çözülür:

1. Araç bir alt kütle durumuna geçer (aynı adımda `m`, `r_g`, `I_o`, ağ ve `∇` güncellenir; `ν` değişmez). Torpido ayrı gövde olarak `r_t` konumunda, aracın o noktadaki rijit hızıyla (`v + ω × r_t`, `ω`) başlar. Torpidonun kendi eklenmiş kütlesinin akış momentumu bu anda doğar; bu darbe idealizasyonunun bilinen eksiğidir.
2. Tüp ekseni `ε` boyunca zıt yönlü darbe `J` uygulanır; araç tarafında etkin ters kütle dönmeyi içerir (04.7):

```
w = [ε ; r_t × ε],   1/M_e = wᵀ M_v⁻¹ w   (M_v: ayrılma sonrası aracın M_RB + M_A'sı)
1/m_e = 1/(m_t + A₁₁,t + m_su,tüp)       (m_su,tüp A5'te, varsayılan 0)
J = v₀ / (1/M_e + 1/m_e)
Δν_araç = −M_v⁻¹ w J,      Δu_torpido = +J / m_e (ε yönünde)
```

`J = m_e v₀` yalnız geri tepmenin ihmal edildiği sınırdır (04.7). Etkin su kütlesi modeli A5'te kaydedilir.
3. Torpido ile kendi aracı arasındaki çarpışma, şekilleri ayrışana kadar süzülür (2.9). Tüp içindeki kısa yol serbest su gibi çözülür (darbe modelinin sınırı).

**Sonraki kademe (ilk sürümde yok):** sonlu süreli tüp modeli; tüp koordinatı ve kısıt kuvvetleriyle araç ve torpidoyu birlikte çözer, etkin kütleyi ve dış yükleri (sürükleme, değişen yönelim) içerir, çıkışı süreye değil tüp sonuna ulaşmaya bağlar; sürekli itkili nesneler de bu kademededir (00 Bölüm 6). Namlu su üstündeyse olay üretilir ve atış "model dışı" işaretlenir (suya giriş fiziği yok, 04 §2.7).

**Hedefler.** Statik ya da senaryo yörüngesini izleyen kinematik gövdeler; şekilleri A7'den; dinamikleri yok.

**İsabet tespiti.** Her adımda torpido şekli, hedefe göre **bağıl** hareket parçası boyunca süpürülür (Jolt `CastShape`; hedef pozu adım başında, yer değiştirme `Δp_torpido − Δp_hedef` hedef çerçevesinde); doğrusal harekette kesin, dönmede yaklaşık. Ayrık sorgu tünelleme yapar (12 §2.8). Olay: `t_ns, torpido_id, hedef_id, nokta_n, normal_n, bağıl_hız_n`. İsabetten sonra torpido dünyadan çıkarılır. Sürekli çarpışma gereksinimi açık (A25).

### 2.9 Çarpışma ve temas

**Kütüphane.** Jolt Physics v5.6.0 (MIT), yalnız sorgu (Ö9; D12 #6a); dinamik çözücü adımlanmaz. Statik ya da kinematik sahne şekilleri; araç şekli A7'de çevrimdışı dışbükey parçalar (12 §2.7). Sürüm sabit (D12 #15).

**Sorgu** (her adımın başında `NarrowPhaseQuery::CollideShape`). Koordinat kuralları (12.7): şeklin kütle merkezi dönüşümü verilir; temas noktaları taban ofsetinden dünya NED'e çevrilir; penetrasyon ekseni yalnız yön olarak kullanılır (büyüklüğü anlamsız), derinlik `mPenetrationDepth`'ten; sonuçlar (gövde, alt şekil, derinlik) anahtarıyla sıralanır. Sarmalayıcı normali **engelden araca** yönlendirir (Jolt'un eksen işareti M31'de doğrulanır). Süzgeçler: bağlı torpido ayrı gövde olmadığı için sorgulanmaz; ayrılan torpido kendi aracıyla, ilk örtüşmesiz adıma kadar sorgulanmaz. Sensör olayları sorgu düzeninde kendiliğinden üretilmez (12.6).

**Tepki.** Temas noktası `r_c` (`O_b`'ye göre), birim normal `n` (gövde çerçevesine çevrilmiş), derinlik `d ≥ 0`, yaklaşma hızı `v_y = −(v_c,araç − v_c,engel)·n` (yaklaşırken pozitif):

```
F_n = max(0, k_n d + c_n v_y),   |F_t| ≤ μ F_n (Coulomb),   τ_temas = Σ [F ; r_c × F]
```

Temas geometrisi (`r_c`, `n`) adım başında sabitlenir; `d` ve `v_y` her RK4 aşamasında temas noktasının o aşamadaki hareketinden doğrusal olarak güncellenir. Böylece temas yayı RK4'ün içinde çözülür (yalnız adım başında sabit kuvvet tutmak, sönümsüz yayda adım başına `det = 1 + (ω h)²/2` ile enerji ekler; bu bölümde türetildi). Kütüphanenin kütlesi kullanılmaz; eklenmiş kütle Fossen tarafındadır.

Her temas için etkin kütle `1/m_eff = wᵀ M⁻¹ w`, `w = [n ; r_c × n]` (köşe temasında dönme etkin kütleyi düşürür). Doğruluk ölçütü `ω_c h ≤ 0,2`, `ω_c = √(k_n/m_eff)`, `h = 1,25 ms` (mimari varsayımı: periyot başına ≥ 31 adım). Yükleyici bu ölçütü çarpışma şeklinin bütün köşelerinde, tekil temas ve dört alt köşenin eşzamanlı teması için denetler, aşılırsa uyarır. İDA-0 varsayılanları (mimari varsayımı, malzeme verisi yok): `k_n = 3·10⁴ N/m`, `c_n` her temas için kritik sönüm `2√(k_n m_eff)`, `μ = 0,5`. Yer tutucu çarpışma kutusunda (boş araç): merkezden düşey temas `m_eff = 32,18 kg`, `ω_c h = 0,038`; alt köşede düşey temas `m_eff ≈ 2,75 kg`, `ω_c h ≈ 0,13`; dört alt köşe eşzamanlı temasında roll kipi `ω h ≈ 0,19`, pitch kipi `≈ 0,16` (bu bölümde hesaplandı). RK4'ün sönümsüz salınım kararlılık sınırı `ω h < 2,83`'tür; ölçüt doğruluk içindir.

**Determinizm.** `CROSS_PLATFORM_DETERMINISTIC`, tek iş parçacıklı iş sistemi, sabit FP bayrakları (12 §2.8); Jolt'un kapsamıyla sınırlıdır, bütün simülatörün platformlar arası eşitliğini sağlamaz (Ç7).

### 2.10 Sayısal çözüm

- **İntegratör:** klasik RK4, `h = 1,25 ms` (iskelet §4). İtici kutuplarında (`−13,160`, `−37,920 ± 82,596i`) `|R(hλ)| = 0,98368 / 0,95371`: incelenen doğrusal modlar kararlı (inceleme hesabı). Surge (`u = 0,72 m/s`) ve roll (`p = 5 rad/s`) yerel zaman sabitleri 91,6 ms ve 37,7 ms (≈ 73 ve 30 adım; 04.2). Bunlar bağlaşımlı sistemin kararlılık ispatı değildir (04.2). İleri Euler'in aynı modeldeki sınırı 9,18 ms'dir.
- **Her RK4 aşamasında yeniden hesaplananlar:** hidrostatik ve dalga basıncı, nokta modeli FK, sönüm, Coriolis, rüzgar, itici filtreleri, temas yayı (2.9), `ν̇_w`.
- **Adım başında tutulanlar (sıfırıncı derece):** aktüatör komutu (gerçekten adım boyunca sabit girdi), bara gerilimi, temas geometrisi. Batarya durumları RK4'ün dışında, tutulan akımla kesin ayrıklaştırmayla ilerler: `V₁⁺ = V₁ e^{−h/τ} + R₁ I (1 − e^{−h/τ})`, `SoC⁺ = SoC − I h / Q`.
- **Mertebe:** elektrik bağlaşımı ayrık (birinci mertebeden) olduğu için bağlaşık sistemin genel mertebesi birdir; dördüncü mertebe ve yarıya inen adımda ≈ 16 kat hata azalması yalnız gerçekten RK4 içinde çözülen düzgün alt sistemlerde beklenir (ideal kaynak, temas yok, ölü bant dışı, su çizgisi gövdeyi kesmiyor). Ölü bant, doyum, kırpma ve temas mertebeyi düşürür (04 §2.6).
- **Kütle matrisi:** `ν̇ = M⁻¹(…)`, Cholesky; birincil kipte yalnız kütle durumu değişince yeniden ayrıştırılır.
- **Yönelim:** RK4 aşamalarında `q̇ = ½ q ⊗ [0, ω]`, adım sonunda normalleştirme (04 §2.6).
- **Alt adımlama (yedek):** eşit iki alt adım 0,625 ms; yakınsama dizisi 1,25 → 0,625 → 0,3125 ms.
- **Çok oranlı yedek:** kuvvetleri seyrek hesaplamak mertebeyi düşürebilir (04.16). Açılırsa periyot fizik adımının tam katı seçilir (400 Hz = 2, 200 Hz = 4 adım); 250 Hz düzgün oturmaz.
- **Hesap yükü:** 500 köşe × 64 bileşen × 800 Hz × 4 RK aşaması ≈ 1,0·10⁸ köşe–bileşen değerlendirmesi/s. Bu bir işlem sayımıdır; süre ölçülmedi (04.16, M15).

### 2.11 Zaman yöneticisi

- **Saat:** sim zamanı `int64` ns; adım 1 250 000 ns; çekirdek duvar saati okumaz. `World::step()` tam bir adım ilerler.
- **Zamanlayıcı:** görev `{periyot_pay_ns, periyot_payda, faz_ns}`; n. olayın ideal zamanı tamsayı aritmetiğiyle `faz + ⌊n · pay / payda⌋`. Olay **en yakın** adım sınırına bağlanır (eşitlikte önceki adım; iskelet §4), en çok 0,625 ms sapma; örnek bağlandığı adımın zamanını gerçek örnekleme zamanı olarak taşır. Aynı adımdaki görevlerin sırası kayıt sırasıdır.

| Hız | Periyot | 800 Hz tabanda |
|---|---|---|
| Kontrol / IMU 400 Hz (zaman çerçevesi dahil) | 2,5 ms | 2 adım |
| Kontrol yedeği 200 Hz | 5 ms | 4 adım |
| 100 Hz / 50 Hz | 10 / 20 ms | 8 / 16 adım |
| Kamera 30 Hz | 33⅓ ms | 27 / 26 adım dönüşümlü |
| Yayın 60 Hz | 16⅔ ms | 13 / 14 adım |

- **İskelet §4 zaman çerçevesi:** her kontrol periyodunda, IMU örneği üretilsin ya da üretilmesin, çekirdek zamanlayıcısı bir "kontrol sınırı" görevi tetikler; çerçevenin içeriği ve gönderimi 06'nındır.
- **Ölçüm zamanı:** snapshot ve sensör örnekleri ölçüm zamanını (sim zamanı) taşır; alım zamanı 05 ve 06'nın modelidir (iskelet §3.1).
- **Üç kip, tek çekirdek:** çekirdek kipten habersizdir (09 §2.4, Ö8).

| Kip | Sürücü | Davranış |
|---|---|---|
| SIL | `FreeRunDriver(hız, duraklat)` | sanal zaman; ağır modeller beklenebilir |
| HIL 1x | `RealtimeDriver` | adım n'in son tarihi `T₀ + (n+1)·h`; fizik adımı atlanmaz |
| HIL 1x + bariyer | `LockstepDriver` | iki fizik adımında bir (400 Hz) aktüatör yanıtını bekler; bariyer bütçesi, fizik süresi ve giriş/çıkış fazı 06 ile eşlenir |

- **Sayaçlar (her kipte, iskelet §6):** `son_tarih_ihlali` (adım son tarihinden herhangi bir miktar geç bitti), `azami_gecikme` ve gecikme dağılımı, `kaçırılan_adım` (gecikme ≥ 1 adım). Geçersizlik eşiği ayrıdır: HIL kiplerinde `kaçırılan_adım > 0` ya da bariyer zaman aşımı çalıştırmayı geçersiz kılar (Ö11; eşik A5'te değiştirilebilir, mimari varsayımı). SIL'de ihlal "1x hesap bütçesi aşıldı" anlamında tanısaldır, çalıştırmayı geçersiz kılmaz; düşük donanımda gerçek zamanlı koşunun mümkün olup olmadığını gösterir. Fizik adımı hiçbir kipte atlanmaz.

### 2.12 İş parçacığı düzeni

- **Dünya iş parçacığı (tek):** zaman, çevre, dinamik, hidro, aktüatör, güç, temas, sensör eklentilerinin CPU tarafı; sabit sıralı indirgemeler (04 §2.6, 09 §2.4).
- **İşçi havuzu:** ağır sensör işleri (Embree sonar, 05). İş sabit bir sim zamanında gönderilir, sonuç modellenmiş teslim zamanında sabit sırayla tüketilir. Dünya iş parçacığı sonucu beklemez (SIL hariç); ancak işçiler CPU, önbellek ve bellek bant genişliğini paylaştığı için dünya adım süresine etkileri ölçülmeden bilinmez (M11, M15). Geç sonuç politikası 05 ve 06'dan beklenen.
- **Diğerleri:** HIL G/Ç (06), kayıt (08), WebSocket/HTTP (08), görselleştirici yayını (07); dünya ile tek üretici–tek tüketici halka tamponları. Dünya iş parçacığı yalnız bariyer kipinde aktüatör yanıtını bekler.

### 2.13 Tekrar üretilebilirlik

- **Hedef (Ç7):** aynı ikili ve platformda bit düzeyinde tekrar; macOS-arm64 ile Windows-x64 arasında toleranslı eşdeğerlik.
- **FP:** fizik `double`; `-ffp-contract=off`, `/fp:precise`, fast-math yok (09 §3.1, 09.6); referans derlemede `EIGEN_DONT_VECTORIZE` ve `EIGEN_FAST_MATH` kapalı (09.7); aşkın işlevler `ida_math` katmanından; fizik yolunda sırasız kap yinelemesi yok.
- **RNG:** sayaç tabanlı Philox (09.4); akış kimliği `(tohum, alt_sistem, örnek)`; `<random>` dağılımları yok; kendi normal dağılımı sürümlü (Box–Muller da platformlar arası bit eşitliği vermez, 09.4). Fizikte: dalga fazları ve frekansları, akıntı ve rüzgar süreçleri.
- **Gerçek zamanlı kiplerde "aynı giriş":** mesaj içeriği yetmez; her komutun **uygulandığı fizik adımı**, uygulanan tek seferlik olay kimlikleri, ömrü dolan komutlar ve elektrik/güç olayları adım sonucu olarak üretilir ve A6'ya yazılır (iskelet A6). Bariyer zaman aşımı ve düşürülen veri 06'nın kaydıdır. Tekrar oynatma bu kayıtla bit düzeyinde aynı fiziği üretir.
- **Kayda girenler:** tohum, RNG ve dağılım sürümü, parametre dosyası özeti, kütle durumu geçişleri, derleyici ve bayraklar, Eigen ve Jolt sürümleri, dalga listesi özeti.

---

## 3. Arayüzler

İmza ve alan listesi düzeyinde; sözleşmeler ayrı işte dondurulur (iskelet §3).

```
void       World::reset(const ScenarioConfig& cfg, uint64_t seed);       // A5 içeriği 08'in ayrıştırıcısından
StepResult World::step();
void       World::apply_actuators(const ActuatorFrame& f);              // sürekli komut; sonraki step() başında
void       World::apply_event(const OneShotEvent& e);                   // tek seferlik; kimlik başına bir kez
void       World::apply_fault(const FaultSpec& f);
std::shared_ptr<const WorldSnapshot> World::snapshot(int64_t t_ns) const;   // halkadaki değişmez kopya
```

- **ActuatorFrame** (06'dan, A1): `seq`, `t_ölçüm_ns`, `pwm_us[8]`, `geçerlilik_maskesi`. Ömür 2.7.1'de.
- **OneShotEvent** (06'dan, A1; iskelet §3.2): `olay_id` (benzersiz), `tür` (`ateşle`, `emniyet_aç`, `emniyet_kapat`), `fırlatıcı`, `t_ölçüm_ns`. Yinelenen kimlik yok sayılır ve sayılır. Emniyeti artıran olaylar aynı adımda ateşlemeden önce uygulanır.
- **StepResult** (A6 için): `t_ns, adım_no`, uygulanan `ActuatorFrame.seq`, uygulanan ve yok sayılan `olay_id` listeleri, ömrü dolan komutlar, olaylar, sayaçlar, `kütle_durumu`.
- **WorldSnapshot** (değişmez; paylaşımlı sahiplik; dünya son K adımın snapshot'ını halkada tutar, varsayılan K = 64 = 80 ms, mimari varsayımı; işçi elindeki kopyayı istediği kadar tutabilir): `t_ns` (ölçüm zamanı), `adım_no`; gövde başına `id, tür, kütle_durumu, p_n, q_nb (w,x,y,z), ν (gövde, O_b), ν̇ (gövde çerçevesinde zaman türevi), a_n (O_b'nin NED eylemsiz ivmesi = R(ν̇₁:₃ + ω × v)), ω̇, r_g, r_b, s, ∇_batık`; itici başına `u, F_ss, F, I, β`; güç `V_bara, I_paket, V₁, SoC, kip, bara_bağlı`; temaslar `{gövde_id, nokta_n, normal_n, derinlik}`; fırlatıcılar `{dolu, emniyet}`; olaylar; sayaçlar. Özgül kuvvet (IMU) 05'in `a_n` ve `g`'den hesabıdır. Ara zaman için yardımcı: `interpolate(t_ns)` iki komşu snapshot arasında konumda kübik Hermite, yönelimde slerp; yalnız halkadaki aralıkta geçerli.
- **IEnvironmentView** (05 ve 07 testi; salt okunur, deterministik):

```
double surface_elevation(double x_n, double y_n, int64_t t_ns) const;   // ζ, ortalama yüzeye göre, yukarı pozitif
Vec3   water_velocity(const Vec3& p_n, int64_t t_ns) const;             // akıntı + dalga, NED
Vec3   water_acceleration_local(const Vec3& p_n, int64_t t_ns) const;   // ∂_t v (Euler), NED
Mat3   water_velocity_gradient(const Vec3& p_n, int64_t t_ns) const;    // ∇v, NED
double water_density() const;  double water_temperature() const;  double depth_at(double x_n, double y_n) const;
Vec3   wind_velocity(int64_t t_ns) const;
```

- **Zamanlayıcı (05 ile sınır):** `TaskId Scheduler::add(uint64_t period_num_ns, uint32_t period_den, int64_t phase_ns, Callback cb)`. `ISensorModel` 05'te tanımlanır; çekirdek ona snapshot, `IEnvironmentView` ve RNG akış kimliği verir. Sensör montaj noktaları `O_b`'ye göre A5'te.
- **Görselleştirici (07, A2):** `VizState {t_ns, gövdeler[]{id, kütle_durumu, p_n, q_nb}, fırlatıcı ve torpido durumları, dalga_listesi_sürümü}`; `WaveSpectrumSet {sürüm, özet, tohum, g, ρ, h, faz konvansiyonu, bileşenler[]{a, ω, k, β, φ}}`. Operatör çizim zamanı ile kamera yakalama zamanının ayrımı 07'nin; çekirdek her ikisi için `t_ns` etiketli snapshot verir.
- **Kayıt ve paneller (08, A3/A6):** `IRecorderSink::on_step(const StepResult&, std::shared_ptr<const WorldSnapshot>)`, `on_event(const Event&)`; geçerlilik bayrakları ve sayaçlar. "Yalnızca olaylar" kademesinde tekrar oynatma güvencesi yoktur (iskelet A6).
- **A5 grupları (biçim 08'de):** `ortam`, `dalga`, `akıntı`, `rüzgar`, `araç` (2.6 tabloları, `O_b`, kütle durumları, işaret kuralı, belirsizlik), `itici[8]`, `esc`, `batarya` (kip, haritalar), `torpido`, `fırlatıcı[2]`, `hedefler[]`, `temas`, `sayısal`, `tohum`, `arızalar[]`.
- **A7:** `BUOY_*`, `COL_*`, itici ve fırlatıcı bağlantı düğümleri, manifest özeti (12 §2.7).

---

## 4. Hata durumları ve davranışı

| Durum | Tespit | Davranış |
|---|---|---|
| Parametre hatası (M simetrik/pozitif tanımlı değil, birim/işaret eksik, kip için zorunlu girdi yok) | yüklemede | senaryo reddedilir |
| Yüzerlik ağı açık / manifold değil; hacim ya da merkez tabloyla tutarsız | yüklemede | reddedilir (eşik %1 hacim, 2 mm merkez; mimari varsayımı) |
| Temas ölçütü `ω_c h > 0,2`; torpido kararlılık marjı < 1,5 | yüklemede | uyarı |
| NaN / sonsuz durum | her adım | çalıştırma durur, son iki adım kayda yazılır, geçersiz |
| Kuaterniyon normu sapması > 1e-9 (normalleştirmeden önce) | her adım | olay |
| Elektrik yinelemesi yakınsamadı | her adım | ikiye bölme yedeği; sayaç (2.7.7) |
| Güç çökmesi (kök yok) ya da `V < 7 V` | her adım | ESC'ler kapalı, olay, `model_dışı_durum` |
| Temas derinliği > 5 cm (mimari varsayımı) | her adım | olay (tünelleme ya da parametre sorunu) |
| Torpido alan dışında ya da 120 s'yi aştı (mimari varsayımı) | her adım | dünyadan çıkarılır |
| Su üstünde atış; boş tüpe ateşleme; yinelenen olay kimliği | olay anı | sırasıyla: model dışı olayı; yok sayılır ve olay; yok sayılır ve sayılır |
| Son tarih ihlali, kaçırılan adım, bariyer zaman aşımı | sürücü | 2.11 |
| Dalga listesi sürümü görselleştiricininkiyle uyuşmuyor | A2 el sıkışması | olay; görüntü tutarsız işaretlenir |
| Kayıt ya da görselleştirici tamponu dolu | halka tampon | dünya beklemez; kayıp sayacı (12.17) |

---

## 5. Kalite kademelerinin etkisi

Fizik her kademede tam çalışır (iskelet §6). Kademeler çekirdeği dolaylı etkiler: görünüm kapalı ya da sade iken dalga listesi görselleştiricide kullanılmaz (fizik aynı); kayıt kademesi snapshot ve adım sonuçlarının kayda giden kısmını belirler (tekrar oynatma "yalnızca olaylar"da güvenceli değildir); sonar kademesi işçi havuzunun yükünü belirler, dünya adım süresine etkisi ölçülmeden bilinmez (M11, M15). Bölüm 6'daki yedekler kademe değildir; ölçüm sonucunda bütün kademeler için geçerli olur (iskelet §6).

---

## 6. Ölçüme bağlı kararlar

| Karar | Birincil | Yedek | Ölçüm |
|---|---|---|---|
| İntegratör ve adım | RK4, 1,25 ms | temas için iki eşit alt adım (0,625 ms) | M15 |
| Hidrostatik/dalga kuvvetinin hesap sıklığı | her RK4 aşamasında, 64 bileşen, tam ağ | 400 ya da 200 Hz çok oranlı (mertebe kaybı ölçülerek); araç çevresinde dalga yaması ızgarası (04 §3.3) | M15 |
| Yüzey geçişinde eklenmiş kütle | sabit `M_A` | `M_A(s)` C¹ | M15, M16 |
| Nokta modeline geçiş | 2.4'teki iki koşul | daima ağ | M15 |
| İtici durağan modeli | ham T200 tablosu | uç değerler + von Benzon şekli (2.7.2) | M22, M16 |
| ESC davranışı | parametreli durum makinesi | yalnız durağan model | M23 |
| Elektrik bağlaşımı | sabit nokta + ikiye bölme yedeği | RK4 aşamalarında bara çözümü (mertebe artışı gerekirse) | M26 |
| Batarya | B1 (30Q haritaları) | B0 | M26 |
| Çarpışma sorgusu | Jolt v5.6.0 yalnız sorgu | ilkel şekiller ve kendi sorgumuz | M31 |
| Torpido sürüklemesi ve fırlatma | 2.8 varsayılanları, darbe modeli | sonlu süreli tüp modeli (sonraki kademe) | M16 |
| Platformlar arası tolerans | 1e-6 bağıl, kısa ufuk (09 §2.3 örneği) | senaryo başına tolerans ve ilk ıraksama raporu | M10 |

---

## 7. Test yaklaşımı ve kabul ölçütleri

Eşikler aksi belirtilmedikçe mimari varsayımıdır; 09'da kesinleşir. Testler boş araç durumunda yapılır.

1. **Çarpık simetri:** `|νᵀC(ν)ν| < 1e-12 · |ν|² ‖M‖` (04 §2.8).
2. **Enerji:** `D = 0`, ağırlık ve yüzerlik kapalı, `τ = 0`: `½νᵀMν`'nin 600 s'deki (480 000 adım) bağıl sürüklenmesi < 1e-6.
3. **Hidrostatik denge:** analitik yüzme derinliği bilinen kutu ve silindir; İDA-0'nın su üstü hacmi `(ρ∇ − m)/ρ`.
4. **Klasik modelle özdeşlik:** düz su, tam batık araçta ağ integrali ile vektör biçimli `g(η)` farkı < 1e-9 bağıl (Hunt–Whitcomb'un 1e-11 sonucu doğrulanmadı, 04.14).
5. **Roll salınımı (tam batık, sönüm kapalı, küçük genlik):** `T = 2π√((I_x + K_ṗ)/(B·BG))`, `B = ρg∇`; tatlı suda 3,630 s, deniz suyunda 3,585 s. **Yüzey heave:** yer tutucu kutuda `ω₃ = √(ρgA_wp/(m + A₃₃)) = 7,654 rad/s`.
6. **Uç hız (ağırlık ve yüzerlik kapalı, kuvvet doğrudan):** 85 N → 0,7294 m/s, sway 85 N → 0,6259 m/s, heave 120 N → 0,7126 m/s; von Benzon Tablo 4'teki 0,72 / 0,63 / 0,71 m/s ile uyum (04 §2.5). Net yüzerlik açıkken heave 120 N aşağı → ≈ 0,706 m/s (inceleme hesabı).
7. **Serbest sönüm:** doğrusal sönümde analitik zarf; karesel sönümde enerji–genlik ilişkisi.
8. **Dalga:** Welch spektrumu `S(ω)` ile, `H_s ≈ 4√m₀`; sonlu derinlik alanında yüzey kinematik koşulu ve süreklilik sayısal türevle; tek eksende `(m + A) v̇ = (ρ∇ + A) u̇_w` (04.5); uzaysal taşınım terimi açık/kapalı karşılaştırması.
9. **Darbe modeli referans durumu:** dış kuvvet yok, ağırlık/yüzerlik kapalı, sabit kütle matrisleri, başlangıçta durgun; bir yüklüden boşa geçiş. Beklenen: `M_e = 10,566 kg`, `m_e = 0,18491 kg`, `J = 0,36345 N·s`; araçta `Δu = −0,01830 m/s`, `Δq = −0,09958 rad/s`, `Δr = +0,06139 rad/s`; torpido `+1,96561 m/s`; bağıl ayrılma hızı 2 m/s (1e-9 bağıl); toplam doğrusal ve açısal momentum (eklenmiş kütleli) korunur. Kütle muhasebesi: ayrılmadan önce ve sonra toplam `m` ve `∇` aynı.
10. **Torpido uçuşu:** pasif yatay uçuşta `1/u(t) = 1/u₀ + (c/m_e)t` (04.8); kanatlar kaldırılınca statik koşul ve simülasyon birlikte kararsızlık gösterir.
11. **Batarya:** dengelenmiş B1'e akım basamağında `ΔV(t)/I = R₀ + R₁(1 − e^{−t/(R₁C₁)})` (11.10); `Q` birimi testi (18 Ah, 1 A, 1 h → ΔSoC = 1/18).
12. **Elektrik:** yapay yükle `|R·dI/dV| ≥ 1` durumunda ikiye bölme yedeğinin kökü bulması; kök olmayan durumda güç çökmesi olayı.
13. **Temas:** tabanda duran araçta penetrasyon kararlı, enerji artmaz; düşürme testinde mekanik enerji tekdüze azalır; tek köşe ve dört köşe temasında 2.9'daki `ω h` değerlerinde salınım büyümez.
14. **Zamanlayıcı:** 30 Hz için en yakın adıma bağlama, en çok 0,625 ms sapma, birikimli kayma yok; 400 Hz için her olay adım sınırında.
15. **Yakınsama:** `h = 1,25 → 0,625 → 0,3125 ms`; düzgün alt sistemde (2.10'daki koşullar) ≈ 16 kat azalma; diğer senaryolarda mertebe raporlanır, kabul ölçütü yapılmaz (04.16, M15).
16. **Tekrar üretilebilirlik:** aynı platformda iki koşu ve A6 kaydından tekrar oynatma bit düzeyinde eşit; macOS-arm64 ile Windows-x64'te tolerans bandı ve ilk ıraksayan adım raporu (09 §2.3, Ç7).
17. **Yayımlanmış referanslar:** PVS/MSS `remus100` ve `otter` (MSS RK4 kipi referans; 04 §2.8); batık sferoid FK yükü MSS `spheroidRAO` ile; dalgada sabitlenmiş BlueROV2 yükleri Gabl ve ark. (2021). Aracri/Walker yüzey geçişini doğrulamaz (04.18). Yüzey–kısmi batma–dalış geçişi ayrı kabul testidir (00 Bölüm 6): dalış, yüzeye çıkış ve dalgada yüzeyde bekleme senaryolarında enerji dengesi ve fiziksel olmayan sıçramanın yokluğu.

---

## 8. Açık noktalar ve iskelete öneriler

**İskelete ve komşu bölümlere**

1. **02:** tahsis matrisini 2.6'daki A5 kaynağından (çapraz momentler, dikey iticilerde `+z` pozitif yön, sabit `O_b`) kurmalı. 02'nin kontrolcü içindeki ters itki eğrisi 04'ün polinom/tablo modeliyle aynı değilse bunun bilinçli model hatası mı geçici yaklaşım mı olduğunu belirtmeli.
2. **03:** atış komutunun tek seferlik olay alanları (3'teki `OneShotEvent`) ve fırlatıcının `dolu/emniyet` durumu 03'ün emniyet zinciriyle eşlenmeli.
3. **05:** IMU özgül kuvveti `a_n` ve `g`'den hesaplar; montaj noktaları `O_b`'ye göre; geçmiş durum için snapshot halkası ve `interpolate` yeterli mi, 05 belirtmeli.
4. **06:** bariyerin iki fizik adımında bir kurulması, bariyer bütçesi ile fizik adım süresinin ve giriş/çıkış fazının eşlenmesi; `komut_ömrü` değerinin kart tarafındaki çıkış ömrüyle uyumu.
5. **İskelet §5:** torpido ve fırlatıcı varsayılanlarının (2.8) mimari varsayımı olduğu ve yük durumlarında aracın `M_A`'sının değişmediği kabulü iskelete not düşülebilir.

**Açık noktalar**

- von Benzon Tablo A1'deki itici konum ve yönleri raporlara aktarılmadı; okunup 2.6 ile karşılaştırılmalı. Bu bölümde kaynak PDF'i bağımsız okunamadığı için itici işlem sırası (2.7.3) ve nitelik etiketleri (2.6) inceleme kaydına dayanıyor.
- 30Q haritaları ve hücre kapasitesi X-57'den A5'e aktarılmalı; aktarılana kadar yalnız ideal kaynak.
- Ortalama yüzey üstünde dalga kinematiği (Wheeler germe) doğrulanmadı.
- İkinci mertebe sürüklenme, frekansa bağlı eklenmiş kütle ve sıvı belleği yok (04 §4 md.2).
- Frenlemede rejeneratif akım ve ESC/itici/batarya ısıl modelleri yok.
- Derinliğe bağlı akıntı profili ve DVL su takibi ayrıntısı (05 ile).
- Torpido sürekli çarpışma gereksinimi (A25); sonlu süreli tüp modeli ve sürekli itkili nesneler sonraki kademe.
