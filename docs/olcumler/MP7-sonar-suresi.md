# MP7 — Sonar hesap süresi ve ışın yakınsaması (Mac ön turu, W0-MP7a)

| | |
|---|---|
| Görev | `docs/gorevler/W0-MP7a.md` (M11'in Mac ön turu), düzeltme turu 1 dahil |
| Dal | `olcum/mp7-sonar-suresi` |
| Program | `tools/olcum/mp7/` (C++20, CMake + Ninja; atılabilir prototip) |
| Tarih | 2026-11-15 (ilk tur 02:33–02:50; düzeltme turu 12:20–19:52) |
| İnceleme | `docs/olcumler/inceleme/MP7-sol.md`, `docs/olcumler/inceleme/MP7-sol-2.md`, yanıt `docs/olcumler/inceleme/MP7-yanit.md`. sol raporu iki kez inceledi; iki düzeltme turu yapıldı; kapanış mimar kararıyla (görev belgesi, 2026-11-16) |

## Özet (düzeltme turu)

Bu turdaki bütün sonuçlar, mimarın düzeltme turunda tanımladığı hücre kuralı ve dondurduğu yakınsama büyüklükleriyle elde edildi (görev belgesi, "İlk turdan sonra: düzeltme turu"). İlk turun sonuçları geçersizdir; kısa notu §8'de.

| # | Ölçüt | Sonuç |
|---|---|---|
| S1 | Süre tablosu eksiksiz | **Geçti.** Daraltılmış ızgara: 216 tam ve 32 sade hücre, her biri 200 ping; p50, p99 ve en büyük süre kayıtlı (Ek A, `tools/olcum/mp7/sonuc/sure2.csv`) |
| S2 | Yakınsama ölçütünü sağlayan en küçük `N` | **Tam kademe: geçmedi** (7 m ve 50 m). Taranan en büyük adımda (1 280 000 → 2 559 903) `δ_V`'nin %95 GA üst ucu 7 m'de 0,093, 50 m'de 0,175 (eşik 0,05). **Sade kademe (7 m): geçti, `N` = 5 000.** Sade kademe `Σw` ile 05'in kutu başına beneğini kullanıyor (mimarın 2. tur düzeltmesi, ana kod yolu); bu `N`'de tek iş parçacığı p99 7 m'de 0,93 ms, 50 m'de 0,79 ms (§4.3, §4.4) |
| S3 | S2'nin `N`'si, `D` = 2, orta sahne, tek iş parçacığı, 7 m: p99 < kısa menzil ping aralığı | **Geçmedi.** Taranan yapılandırmalarda S2 bulunamadı; `N` = 1 280 000 destek koşusunda p99 = **504,4 ms** (2000 ping). Eşikler 9,33 ms (alt sınır) ve 25,17 ms (model `T_ping` [T]). Bu rakam taranan yapılandırmalar ve bu destek koşusu için geçerlidir; daha büyük `N` için ölçüm yoktur |
| S4 | Aynı tohumla iki koşu tek iş parçacığında bit düzeyinde aynı | **Geçti.** Yeni modelle, `N` = 160 008, `D` = 2, 7 m ve 50 m'de iki ayrı süreç aynı özeti verdi; tek iş parçacığı = 11 iş parçacığı |

**M3 Pro'da ölçülen süreler Windows hedef bilgisayarını temsil etmez.** Windows için süre tahmini yazılmadı.

**Sıra:** Bölüm 1'deki bütçe ilk turun nihai süre koşularından önce yazıldı (öncesinde yalnız 30 pinglik bir düzenek denemesi vardı ve kaba süreleri biliniyordu). Düzeltme turunda bütçe değişmedi. Yakınsama büyüklükleri mimar tarafından bu turun ölçümlerinden önce donduruldu.

<!-- BÖLÜM: BÜTÇE (nihai zamanlama koşularından önce yazıldı) -->

## 1. Bütçe (ölçümden önce yazıldı)

Bu bölüm nihai zamanlama koşularından önce yazıldı. Öncesinde yalnızca düzeneği doğrulamak için 30 pinglik küçük bir deneme koşusu yapıldı; o koşunun sayıları bu raporda sonuç olarak kullanılmaz.

### 1.1 Ping360 ping aralığı (05 §2.4.5)

`T_ping = T_motor(Δaçı) + T_dinleme + T_işlem + T_iletim + T_delay`, `T_dinleme = n · sp · 25 ns`. `T_m0`, `t_grad` ve `T_işlem` [K]'dir. Varsayılan yapılandırmada (50 m, 1200 örnek, `sp` = 2222, 1 gradyan adım, USB) 05, açı başına 82,5 ms (33 s / 400) ve `T_dinleme` = 66,66 ms verir; kalan ≈ 15,84 ms `T_m0 + t_grad + T_işlem + T_iletim` toplamıdır. Modelde bu toplam menzile bağlı değildir: aynı adım, aynı bayt sayısı ve aynı bağlantıda kısa menzile aynen taşınır. `T_delay` = 0 alındı.

| Menzil | `n`, `sp` | `T_s` | Gerçekleşen menzil | `T_dinleme` (alt sınır) | Model `T_ping` (varsayılan uydurmayla) [T] |
|---|---|---|---|---|---|
| Kısa: 7 m | 1200, 311 | 7,775 µs | 6,9975 m | **9,33 ms** (≈ 107 ping/s) | 9,33 + 15,84 = **25,17 ms** |
| Uzun: 50 m | 1200, 2222 | 55,55 µs | 49,995 m | **66,66 ms** (≈ 15 ping/s) | **82,5 ms** |

- **Alt sınır** (`T_dinleme`) bilinmeyen sabitlerden bağımsızdır: ping aralığı bundan kısa olamaz. Daha hızlı bir bağlantı ya da daha küçük `T_işlem`, aralığı bu sınıra yaklaştırır (`T_iletim` kayıpsız baud ölçülmedi; Ç19, M36).
- **Model `T_ping`** 15,84 ms'lik kalanın menzilden bağımsız olduğu varsayımına dayanır [T]; ölçüm değildir.
- **Teslim bütçesi** (sonuç `t_teslim`'de hazır olmalı; 05 §2.4.5, iş `t_tx`'te gönderilir): `t_teslim − t_tx = T_dinleme + T_işlem + T_iletim`. Kalanın `T_motor` ile `T_işlem + T_iletim` arasındaki bölüşümü bilinmediği için 7 m'de 9,33–25,17 ms, 50 m'de 66,66–82,5 ms aralığındadır.
- **Bekleyen iş sınırı:** sonar örneği başına en çok 4 iş (05 §2.4.1). Süreklilik koşulu: ortalama hesap süresi ping aralığından kısa olmalı; tek bir pingin teslim bütçesini aşması o pingi geç yapar (05 §4 politikası).

**S3 için eşik:** kısa menzilin ping aralığı. Bu raporda iki eşik birlikte değerlendirilir: birincil eşik koruyucu alt sınır **9,33 ms**'dir (her bölüşümde teslim bütçesine sığmayı da garanti eder); ikincil eşik model `T_ping` = **25,17 ms**'dir [T].

### 1.2 Dünya adımı ve işçi havuzu (04 §2.11, §2.12)

- Fizik adımı `h` = 1,25 ms; HIL'de (`RealtimeDriver`) adım `n`'in son tarihi `T₀ + (n+1)·h`, adım atlanmaz; `kaçırılan_adım > 0` HIL çalıştırmasını geçersiz kılar (04 §2.11).
- Sonar ışın izlemesi dünya iş parçacığında değil, işçi havuzundadır (04 §2.12, 05 §2.2). 04, işçiler için sayısal bir bütçe vermez; işçilerin CPU, önbellek ve bellek bant genişliği paylaşımı yüzünden dünya adım süresine etkisi "ölçülmeden bilinmez" (M11, M15).
- Bu ölçümden çıkarılabilecek bütçe göstergesi: **tek çekirdek doluluk oranı** = ping başına ortalama hesap süresi / `T_ping`. Dünya iş parçacığı ve diğer iş parçacıkları (HIL G/Ç, kayıt, yayın; 04 §2.12) için en az bir çekirdek boş kalmalıdır. Bu ölçüm dünya iş parçacığını koşturmaz; işçilerin dünya adımına etkisi burada ölçülmedi.

<!-- BÖLÜM SONU: BÜTÇE -->

## 2. Ortam

| Kalem | Değer |
|---|---|
| Makine | Apple M3 Pro (5 P + 6 E çekirdek, `hw.ncpu` = 11), 18 GB |
| OS | macOS 26.5.2 (Darwin 25.5.0) |
| Derleyici | Apple clang 17.0.0 (clang-1700.0.13.3), C++20, `-O2 -ffp-contract=off -fno-fast-math`, Release (09 §2.6 PC politikası) |
| Embree | 4.4.1, Homebrew şişesi (`embree--4.4.1.arm64_tahoe.bottle.1`); şişeyi derleyen: CLANG 21.0.0, Release, `EMBREE_MAX_ISA=NEON2X`, ISPC açık, görev sistemi TBB 2023.2.0 (bağımlılık: hwloc 2.15.0). Seçilen ISA: NEON / 2xNEON; kesişim çekirdeği `BVH8<triangle4>` (Embree'nin `avx2` adlı yolu, NEON üzerinde) |
| Embree cihazı | `threads=1` (BVH kurulumu tek iş parçacıklı, 05 §2.6); sahne kalitesi `RTC_BUILD_QUALITY_MEDIUM`; tekil ışın API'si (`rtcIntersect1`, `rtcOccluded1`), paket/akış yok |
| Kart revizyonu | Yok (kartsız ölçüm) |
| Güç (düzeltme turu) | Süre taraması ve S3 koşusu **prizde** (`pmset`: "AC Power", %65, şarj olmuyor), `caffeinate -dims` altında. Yakınsama koşuları (12:20–12:29) pildeydi; bunlar süre ölçümü değildir |
| Makine yükü (düzeltme turu) | Taramadan önce Spotlight dizinlemesi (`mds_stores` ≈ %66) ve `mobileassetd` sürüyordu; tarama makine %90 boşa düşene kadar bekletildi. Blok başlarında %85,3–97,2 boşta. En düşük değer `tam 1e4 7m` bloğunun başında (%85,3). Bütün blok başlarında Godot süreci yoktu (`pgrep -fl Godot.app`). Ayrıntı: `tools/olcum/mp7/veri/sistem2.txt` (depoya girmez) |

## 3. Ölçüm düzeneği ve bölüm 05 modelinin yorumu

**Sahne** (`kaynak/sahne.cpp`, tohum 20261114, belirlenimci): engebeli taban (110 × 110 m yükseklik alanı, ortalama −8 m), su yüzeyi (z = 0 düzlemi), duvar (x = 6,0 m), 4 silindir, 4 kutu, 3 küre (yarısı 2–6,5 m, yarısı 8–45 m uzakta) ve ayrı alt sahnede hareketli hedef küre (1 m/s). Üçgen sayıları **9 886**, **100 368**, **999 558**. Sonar (0, 0, −6 m), tabandan ≈ 2 m yukarıda, kafa 5° aşağı eğik. Ping `k`'nin kafa açısı `2k` gradyan (200 pingte tam tur), gönderim anı `0,025·k` s.

**Ping hattı** (ping süresi hepsini içerir): işe özel üst sahne kurulumu ve commit, ışın örnekleme, Embree sorguları, yollar, **hücre toplama** (açık adresli tablo, ekleme sırası = ışın sırası), kesirli gecikmeli biriktirme (ince ızgara `T_s/4`), darbe/alıcı çekirdeğiyle evrişim, kutulara indirgeme, gürültü ve yakın alan bandı, log, TVG ve kuantalama. İş kuyruğu, `PoseSet` kopyası ve tam kademenin dalga yaması **ölçülmedi**.

**Hücre kuralı (mimarın tanımı, bu turda uygulandı):**
- Hücreler gövdeye sabit **ortonormal** yüzey koordinatlarında: u = e1 yönü, w = n × u; kenar `Δ_h` = 1 cm. Kimlik `(gövde, üçgen, i, j)`, ışın sayısından bağımsız. Üçgen sınırındaki hücre kırpılır; alanı, hücreye düşen ışınların enerji ağırlıklarıyla örtük olarak hesaplanır.
- Işın `j`'nin her yolu bir enerji ağırlığı taşır: `w_j = a_j²`. Burada `a_j = B_gidiş B_dönüş Π(R_s γ_p) √(σ_bs dA_j) 10^{−αL/20} / (L_g L_d)`, işaretlidir.
- Hücre ve yol türü başına `p_c = s_c √(Σ w_j) g_c e^{iφ_c}`: `s_c = sign(Σ a_j)`, `g_c ~ CN(0,1)` ve `φ_c` düzgün faz; ikisi de (gövde, hücre, yol türü, gerçekleşim) anahtarından türetilir. İkisi de `N`'den bağımsızdır. Hücrenin gecikmesi, ışınların enerji ağırlıklı ortalamasıdır.
- Kutu alanı hücrelerin `p_c` değerlerinin eşevreli toplamıdır (çekirdekle evrişimden sonra).
- Sade kademe (mimarın 2. tur düzeltmesi, ana kod yolu): kutu enerjisi `Σ w_j`, faz yok; 05 §2.4.3'ün kutu başına üstel benek çarpanı korunur. Çarpanın anahtarı gerçekleşim tohumu, ping ve kutudur, `N`'den bağımsızdır. Beneksiz değişke yalnız `--benek 0` ile çalışır; 1. düzeltme turunda tanılama için koşuldu.

**Model tanımı: uygulanan değişke (sol-2 md.1).** Uygulanan değişke mimarın ilk yazımından iki noktada ayrılır; mimar 2026-11-16'da uygulanan değişkeyi tanım olarak kabul etmiştir (görev belgesi, "Kapanış"):
- (a) Hücreler dünya uzayına değil, yüzeyin ait olduğu gövdeye sabittir (gövde yerel koordinatları). Durağan tabanda ikisi aynıdır; hareketli cisimde saçıcılar cisimle birlikte hareket eder.
- (b) Faz ve benek anahtarı (gövde, hücre, yol türü, gerçekleşim) dörtlüsüdür. Aynı hücrenin farklı yol türlerinden gelen katkıları arasındaki faz rastgele kabul edilir (750 kHz'te dalga boyu ≈ 2 mm, yol farkı çok büyük). Aynı kutuya düşen farklı yol türleri enerji olarak, aynı yol türü içindeki hücreler eşevreli toplanır.

Bu raporda ölçülen bütün istatistikler bu değişkeye aittir.

**Diğer yorum seçimleri** [T]:
1. **Yansıma derecesi `D`:** 0 = doğrudan; 1 = + gidişte sekme (dönüşte sekme için genlik ×2, 05 §2.4.4); 2 = + çift sekme `T→M→S→M'→T`. `M'`, `T`'nin `M`'nin teğet düzlemindeki görüntüsüne çizilen doğrunun ilk kesişimidir. `D` = 2'de ışın başına en çok 3 kesişim ve 2 görünürlük sorgusu (05 §5'teki sayım). Her bacakta `tfar` = gerçekleşen menzil.
2. **Desen ve koni:** ayrılabilir sinc, tek yönlü −3 dB 2° × 25°. Tam kademede koni sinc'in ikinci sıfırına kadar (±4,52° × ±56,4°), sade kademede ana loba kadar. Koni dışı elenir; **koni içindeki negatif yan lob genlikleri işaretiyle korunur** ve görünürlük sorguları yapılır (sol md.2).
3. **Işın örnekleme:** açı uzayında katmanlı titreşimli `nh × nv` ızgarası, `dΩ = Δθ Δφ cos φ`; gerçek ışın sayısı `N`'den biraz farklı olabilir, tablolar gerçek sayıyı verir.
4. **Darbe:** `tx_duration` = 4 `T_s` [T]; sıfır fazlı üçgen çekirdek (dikdörtgen ⊗ dikdörtgen). Duvar denetimi: beklenen kutu 1028, yeni modelde yükseliş 1025–1029.
5. **Akustik değerler:** `c` = 1500 m/s, 750 kHz, `α` = 0,1314 dB/m (doğrulanmamış formülün aritmetiği); malzeme `μ`, `R_s`, `γ_p` [K] yer tutucu.
6. **Paralellik:** ping içi, ışınlar 64'lük parçalarla dağıtılır; sonuçlar ışın numarasıyla yazılır, hücre toplama ve kutulara ekleme tek iş parçacığında ışın sırasıyla yapılır. "Tümü" = 11 iş parçacığı (P ve E karışık).

## 4. Sonuçlar (düzeltme turu)

### 4.1 Kurulum ve bellek

| Sahne | Üçgen | BVH kurulumu, tek iş p. (ms) | Sahne RSS artışı (MB) | Embree sahne belleği (MB) | Süreç tepe RSS (MB) |
|---|---|---|---|---|---|
| 1e4 | 9 886 | 1,7–4,4 | 2,9 | 0,8 | 190,6 |
| 1e5 | 100 368 | 14,9–16,9 | 14,2 | 7,8 | 269,9 |
| 1e6 | 999 558 | 163,6–164,1 | 109,6 | 81,2 | 211,0 |

Tepe RSS süreçteki en büyük `N`'nin ışın ve hücre tamponlarını içerir: orta sahnede `N` = 160 000 ve 11 iş parçacığı, 1e4 ve 1e6'da `N` = 80 000. S3 koşusunda (`N` = 1 280 000) tepe RSS 216 MB.

### 4.2 Süre (S1)

Bütün hücreler: Ek A ve `tools/olcum/mp7/sonuc/sure2.csv`. Her hücrede 10 ısınma pingi ve 200 ölçülen ping var; p99 en yakın sıra yöntemiyle (200 pingte 198. değer).

Orta sahne, tam kademe, `D` = 2 (p50 / p99 / en büyük, ms):

| Menzil | `N` | 1 iş p. | 11 iş p. |
|---|---|---|---|
| 7 m | 20 000 | 4,88 / 9,03 / 9,14 | 1,36 / 2,73 / 2,76 |
| 7 m | 80 000 | 18,34 / 34,01 / 34,12 | 4,32 / 9,13 / 9,18 |
| 7 m | 160 008 | 36,10 / 65,24 / 67,47 | 8,00 / 25,23 / 27,48 |
| 50 m | 20 000 | 12,56 / 14,06 / 14,11 | 3,72 / 4,02 / 4,05 |
| 50 m | 80 000 | 48,22 / 53,46 / 55,04 | 13,51 / 14,69 / 15,07 |
| 50 m | 160 008 | 93,39 / 103,68 / 103,81 | 27,47 / 40,36 / 49,49 |

Gözlemler (bu makinede, bu sahnede ve bu açı/hedef dizisinde):
- Süre `N` ile yaklaşık doğrusal. 11 iş parçacığında `N` = 160 008'de p99 kuyruğu kalınlaşıyor (7 m: p99/p50 ≈ 3,2); nedeni incelenmedi.
- **Sahne boyutu etkisi menzile göre değişiyor** (tek iş p., `D` = 2, p99): 7 m'de `N` = 80 000 için 1e4 / 1e5 / 1e6 = 35,6 / 34,0 / 38,2 ms (1e4'e göre +%7,5). 50 m'de 49,9 / 53,5 / 58,7 ms (+%17,6). `N` = 20 000'de 7 m 9,5 / 9,0 / 10,1 ms, 50 m 13,3 / 14,1 / 17,0 ms (+%28). 1e6 7 m `N` = 80 000 hücresinde en büyük süre 61,3 ms, tek bir aykırı değer.
- **Yansıma derecesi etkisi** (1e5, `N` = 80 000, tek iş p., p99): 7 m'de `D` = 0 / 1 / 2 → 11,6 / 22,3 / 34,0 ms; 50 m'de 15,7 / 31,8 / 53,5 ms.
- 7 m'de p99 ≈ 1,8 × p50: süre kafa açısına göre değişiyor. 50 m'de p99/p50 ≈ 1,1.

### 4.3 Yakınsama (S2)

Mimarın dondurduğu tanım uygulandı: orta sahne, `M` = 32 gerçekleşim (tohum 1000–1031), 8 kafa açısı, gürültü kapalı. `N` = 625 · 2^k, 625 … 2 559 903.
- **Kutu kümesi `S`:** bağımsız referanstan, taranan en büyük `N` ve ayrı tohumlarla (5000–5031). Açı başına en yüksek referans ortalama enerjisinin 10⁻³ katından büyük kutular. Ham enerji kullanıldı, TVG yok. Bu tanımla `S` yakın ve parlak kutularda yoğunlaşıyor: |S| 7 m tamda 4 157, 50 m tamda 1 395, 7 m sadede ≈ 1 785 kutu (8 açı toplamı).
- **Büyüklükler:** `δ_E = Σ_S |Ē_2N − Ē_N| / Σ_S Ē_2N`; `δ_V` aynı ifade ham varyansla; `δ_C = |C̄_2N − C̄_N|`.
- **Ölçüt:** nokta değeri ve bootstrap %95 GA üst ucu (B = 1000, 97,5. yüzdelik) ≤ 0,05 olmalı ve bir sonraki ikiye katlamada da sağlanmalı. `V/E²` tanılayıcıdır.

**7 m, tam, `D` = 2** (`sonuc/yakinsama2_7m_tam_D2.*`):

| `N` → `2N` | `δ_E` (üst) | `δ_V` (üst) | `δ_C` (üst) | `V/E²` |
|---|---|---|---|---|
| 5 000 → 9 996 | 0,204 (0,369) | 0,505 (0,984) | 0,001 (0,003) | 1,50 |
| 40 014 → 80 000 | 0,135 (0,252) | 0,451 (0,870) | 0,000 (0,003) | 1,02 |
| 160 008 → 320 000 | 0,057 (0,108) | 0,166 (0,353) | 0,003 (0,005) | 0,98 |
| 320 000 → 640 032 | 0,036 (0,066) | 0,096 (0,173) | 0,000 (0,002) | 0,97 |
| 640 032 → 1 280 000 | 0,023 (0,039) | 0,061 (0,101) | 0,000 (0,002) | 0,97 |
| 1 280 000 → 2 559 903 | 0,015 (0,027) | 0,046 (0,093) | 0,001 (0,002) | 0,97 |

**50 m, tam, `D` = 2:** son iki adımda `δ_E` 0,032 (0,056) ve 0,018 (0,039); `δ_V` 0,067 (0,129) ve 0,098 (0,175); `δ_C` 0,004 (0,008) ve 0,003 (0,008).

**7 m, sade (ana yol: `Σw` + 05'in kutu başına beneği):** `δ_E`, `δ_V` ve `δ_C` 5 000 → 9 996 adımından itibaren her adımda nokta ve üst uçta ≤ 0,05. 5 000 → 9 996 adımında değerler 0,007 (0,012), 0,028 (0,047), 0,022 (0,028); 9 996 → 20 000 adımında da sağlanıyor. **S2: `N` = 5 000, geçti** (`sonuc/yakinsama2_7m_sade_benek.*`). Tanılama için koşulan beneksiz değişkede `δ_V` her adımda 2,1–3,5 kaldı (`sonuc/yakinsama2_7m_sade.*`).

**Yorum:**
- Tam kademede `δ_E` ve `δ_C` 7 m'de `N` ≈ 640 000'den, 50 m'de ≈ 1 280 000'den itibaren ölçütü sağlıyor. `δ_V`'nin üst ucu ise 2 559 903'e kadar 0,05'in altına inmiyor. 7 m'de düzenli azalıyor (0,173 → 0,101 → 0,093), 50 m'de azalmıyor (0,129 → 0,175). Gerekli `N`'yi dışdeğerlemeyle tahmin etmedim.
- Benek kontrastı `V/E²` `N` arttıkça 1'e iniyor (7 m: `N` = 5 000'de 2,17, 20 000'de 1,21, 80 000'den sonra 0,97–1,02). İlk turdaki sınırsız enerji büyümesi kayboldu: `Ē` artık tekdüze yakınsıyor.
- Tam kademede `δ_V`'nin yavaş yakınsamasının nedeni ham varyansın çok parlak birkaç kutunun dördüncü momentine duyarlı olmasıdır. Bu, ölçülen davranışa dayanan bir yorumdur; nedeni ayrıca sınanmadı.
- **Sade kademe tanımı (1. turda bildirildi, 2. turda mimarca düzeltildi):** benek çarpanı olmadan gerçekleşimler arası varyans yalnız ışın titreşiminden gelir ve yaklaşık `1/N` ile küçülür. O durumda `δ_V` her adımda ≈ 1'in üstünde kalır ve ölçüt yapısal olarak sağlanamaz; beneksiz değişkenin ölçümü bunu doğruladı. Mimar sade kademede 05'in kutu başına beneğini korudu; S2 hükmü bu ana yoldan verildi.

### 4.4 Bütçeyle karşılaştırma (S3)

Taranan yapılandırmalarda S2 tam kademede bulunamadı. S3 hücresi bu yüzden destek koşusu olarak `N` = 1 280 000'de koşuldu (S2 taramasında, bir sonraki ikiye katlaması ölçülmüş olan en büyük `N`). Orta sahne, 7 m, `D` = 2, tek iş parçacığı, 2000 ping (`sonuc/sure2_s3.csv`).

| p50 | p99 | en büyük | ortalama (ms) | kesişim / görünürlük sorgusu (ping başına) |
|---|---|---|---|---|
| 291,6 | **504,4** | 544,7 | 304,6 | 2 149 183 / 349 589 |

Eşik 9,33 ms → **geçmedi** (≈ 54 kat); 25,17 ms → **geçmedi** (≈ 20 kat). Bu sonuç taranan yapılandırmalar ve `N` = 1 280 000 destek koşusu için geçerlidir; daha büyük `N` ölçülmedi.

Bütçeye giren yapılandırmalar (orta sahne, p99 < eşik; `N` ızgarası 1 000 … 160 008):

| 7 m | `D` = 0 | `D` = 1 | `D` = 2 |
|---|---|---|---|
| Tek iş p., p99 < 9,33 ms | `N` ≤ 40 014 | `N` ≤ 20 000 | `N` ≤ 20 000 |
| Tek iş p., p99 < 25,17 ms | `N` ≤ 160 008 | `N` ≤ 80 000 | `N` ≤ 40 014 |
| 11 iş p., p99 < 9,33 ms | `N` ≤ 160 008 | `N` ≤ 80 000 | `N` ≤ 80 000 |

50 m (eşikler 66,66 / 82,5 ms): tek iş parçacığı, `D` = 2'de `N` ≤ 80 000; 2 ve daha fazla iş parçacığıyla ızgaranın tamamı (`N` ≤ 160 008). **Sade kademe, S2'nin `N`'si (5 000)**, ana yol (benekli), orta sahne, tek iş parçacığı, 200 ping (`sonuc/sure3_sade.csv`; 19:57, prizde, %96 boşta, Godot yok):

| Menzil | p50 | p99 | en büyük (ms) | Bütçe: alt sınır / model `T_ping` |
|---|---|---|---|---|
| 7 m | 0,54 | **0,93** | 0,93 | 9,33 / 25,17 ms → ikisinin de altında |
| 50 m | 0,69 | **0,79** | 0,82 | 66,66 / 82,5 ms → ikisinin de altında |

Ek A'daki sade satırları 1. düzeltme turunun beneksiz koduyla ölçüldü (fark kutu başına bir üstel çekiliş).

Dünya adımı (04): sonar 05'in mimari seçimiyle işçi havuzundadır. Bu ölçüm, bunu her `N` için gerekli kılan bir sonuç vermez: orta sahnede, `N` = 2 000, tek iş parçacığında en büyük süre 7 m'de `D` = 0/1/2 için 0,43 / 0,70 / 1,03 ms, 50 m'de 0,47 / 1,04 / 1,56 ms. Ağır hücreler (S2'ye yakın `N`) 1,25 ms'yi iki ila üç mertebe aşar. İşçilerin dünya adımına etkisi **ölçülmedi**.

### 4.5 Tekrar üretilebilirlik (S4)

`NT=160000 tools/olcum/mp7/tekrar.sh`: orta sahne, `N` = 160 008, `D` = 2, 200 ping, gürültü açık; her yapılandırma iki ayrı süreçte (`sonuc/tekrar2.txt`).

| Menzil | Özet (`Ĩ` / `u8`, FNV-1a) | Tek iş p. × 2 | 11 iş p. × 2 |
|---|---|---|---|
| 7 m | `b0130abd498d972e` / `1363c0664220aa49` | aynı | aynı |
| 50 m | `d4b8c7acd407d03b` / `0ed75e0368dd123a` | aynı | aynı |

Kapsam: aynı ikili, aynı makine.

### 4.6 Azaltılmış ışın sayısının bilinen hatası (betimleyici; ölçüt değil)

**Kurulum:** tam kademe, orta sahne, `D` = 2, `M` = 32, 8 açı, gürültü kapalı; tanımlar §4.3'tekilerle aynı, yalnız `2N` yerine referans var. Kutu kümesi `S` §4.3'teki bağımsız referanstan. 2. tur, 19:57–20:03, prizde (`sonuc/hata_7m.*`, `sonuc/hata_50m.*`, `tools/olcum/mp7/hata.sh`).

**Referans seçimi (yorum):** Bu eşli tablo, ortak benek alanında, sonlu referansa göre sayısal farktır. Referans `N` = 2 559 903, sınanan `N` ile **aynı gerçekleşim tohumlarıyla** (eşli). Böylece fark yalnız ışın sayısından gelir. Bağımsız tohumlu bir referans, `M` = 32 gerçekleşimin benek gürültüsünü de farka katardı. Buna karşılık, gözlenen iki bağımsız kümenin farkı ayrıca verildi: aynı `N` = 2 559 903'te bağımsız tohumlu referansa göre (kutu kümesi karşılaştırılan kümelerden biriyle seçildi ve iki küme aynı bootstrap indisleriyle yeniden örneklendi; genel bir belirsizlik tabanı değildir). Sayılar nokta değeri, parantezde bootstrap %95 GA üst ucu.

| Menzil | `N` (gerçek) | `δ_E` | `δ_V` | `δ_C` | `V/E²` |
|---|---|---|---|---|---|
| 7 m | 20 000 | 0,146 (0,308) | 0,732 (1,661) | 0,006 (0,008) | 1,21 |
| 7 m | 40 014 | 0,122 (0,235) | 0,305 (0,670) | 0,004 (0,006) | 1,09 |
| 7 m | 80 000 | 0,110 (0,185) | 0,263 (0,512) | 0,003 (0,005) | 1,02 |
| 7 m | 160 008 | 0,057 (0,101) | 0,159 (0,303) | 0,004 (0,005) | 0,98 |
| 7 m | gözlenen iki bağımsız küme farkı (2 559 903, bağımsız tohum) | 0,154 (0,322) | 0,491 (1,029) | 0,000 (0,004) | 0,97 |
| 50 m | 20 000 | 0,107 (0,273) | 0,427 (0,744) | 0,001 (0,007) | 1,04 |
| 50 m | 40 014 | 0,074 (0,266) | 0,206 (0,902) | 0,002 (0,007) | 0,97 |
| 50 m | 80 000 | 0,071 (0,178) | 0,257 (0,383) | 0,006 (0,010) | 1,00 |
| 50 m | 160 008 | 0,055 (0,125) | 0,095 (0,368) | 0,005 (0,011) | 0,98 |
| 50 m | gözlenen iki bağımsız küme farkı (2 559 903, bağımsız tohum) | 0,148 (0,397) | 0,424 (1,496) | 0,004 (0,009) | 0,99 |

Okuma: eşli farklar, `M` = 32 ile gözlenen iki bağımsız küme farkından küçük ya da onunla aynı mertebede. Bu karşılaştırma yalnız gözlenen iki kümeyle sınırlıdır; genel bir örnekleme belirsizliği ya da gürültü tabanı iddiası değildir. Benek kontrastı `N` ≥ 40 000'de 1'e %10 yakın.

## 5. Komutlar ve veri

```
tools/olcum/mp7/derle.sh                                  # Embree: EMBREE_ONEK ya da `brew --prefix`
tools/olcum/mp7/yakinsama.sh hepsi                        # S2 -> veri/yakinsama2_*.csv|txt
NT=160000 tools/olcum/mp7/tekrar.sh                       # S4 -> veri/tekrar2.txt
NS3=1280000 caffeinate -dims tools/olcum/mp7/sure.sh      # S1, S3 -> veri/sure2.csv, veri/sure2_s3.csv, veri/ham2/, veri/sistem2.txt
caffeinate -dims tools/olcum/mp7/hata.sh                  # 2. tur: hata tablosu ve sade N = 5 000 süresi -> veri/hata_*.csv, veri/sure3_sade.csv
python3 -I tools/olcum/mp7/ozet.py tools/olcum/mp7/veri/sure2.csv tools/olcum/mp7/veri/sure2_s3.csv
```

Özet veri depoda: `tools/olcum/mp7/sonuc/` (bu tur) ve `tools/olcum/mp7/sonuc/tur1/` (ilk tur, geçersiz). Ham veri (`veri/`: ping başına süreler, sistem anlık görüntüleri) depoya girmez.

## 6. Doğrulanmayanlar ve sınırlar

- Windows süresi; düşük donanımlı hedefte süre ve bellek (K16). M3 Pro sonucu Windows'u temsil etmez.
- İşçilerin dünya iş parçacığı adım süresine etkisi (04 §2.12; M11, M15).
- Paylaşılan alt sahnelerin **eşzamanlı** olarak birden çok üst sahnede örneklenmesi: burada aynı anda tek ping koştu; Embree belgesinden de doğrulanmadı.
- Tam kademenin dalga yaması (üretim süresi, üçgen sayısı); su yüzeyi düz düzlem.
- Tam kademede S2'yi sağlayan `N`: taranan yapılandırmalarda bulunamadı (taranan en büyük `N` 2 559 903); bunun ötesi ölçülmedi. `Δ_h`'ye, sahneye ve malzeme değerlerine duyarlılık.
- Sonlu referansın (N ≈ 2,56 milyon) kendisi dondurulmuş ölçüte göre yakınsamamıştır; referansa göre ölçülen farklar bu sınırla okunmalıdır.
- Sade kademede S2 yalnız 7 m'de ölçüldü (50 m'de yalnız süre).
- **p99 değerleri bu tarama iş yüküne aittir:** 200 pingte tam tur yapan kafa açısı dizisi, bu sahne ve hedef hareketi. Başka bir senaryonun ya da işletim sistemi gecikmelerinin genel p99'u değildir.
- **Bootstrap aralıkları koşulludur:** `S` sabitken yalnız gerçekleşimler yeniden örneklendi; `S`'nin seçimi bağımsız referanstan geldiği için seçim yanlılığı yoktur, ama referansın kendi belirsizliği aralığa girmez.
- İlk turun model sapmaları (sol md.2–4) bu turda giderildi; giderilmiş modelin fiziği (desen, soğurma, malzeme, `tx_duration`) doğrulanmadı.
- Embree paket/akış API'leriyle olası hızlanma; Embree'nin kaynak derlemesi (Homebrew şişesi kullanıldı).
- OceanSim "512 ışın" iddiasının yeniden ölçümü ve açık veri kümesiyle karşılaştırma (M11'in diğer parçaları) bu turda yok.

## 7. Bölüm 05 için düzeltme önerisi (05 dosyasına dokunulmadı)

**§2.4.3 saçıcı hücresi kuralı.** 05'in yazımında aynı hücreye düşen `m` ışının `√(σ_bs dA_j)` genlikleri eşevreli toplanıyor. Bu, enerjiyi `σ A m` yapar ve ışın sayısıyla sınırsız büyütür (sol, bağımsız türetme). İlk turda ölçüldü: 7 m'de enerji her ikiye katlamada +%8'den +%64'e kadar arttı (`sonuc/tur1/yakinsama_7m_D2_k0.csv`).

Mimarın önerdiği kural şudur: hücre içinde enerji ağırlıkları `w_j = σ_j dA_j G_j` toplanır, `p_c = s_c √(Σ w_j) g_c e^{iφ_c}`; `g_c` ve `φ_c` hücre kimliği ve gerçekleşim tohumundan gelir, hücreler arası toplam eşevrelidir. Hücreler ortonormal yüzey koordinatlarındadır ve negatif yan loblar korunur. Bu kuralla:
- Kutu ortalama enerjisi tekdüze yakınsıyor: 7 m'de `δ_E` 0,204 → 0,015. Benek kontrastı `N` ≥ 80 000'de ≈ 1 (0,97–1,02).
- Sade kademe (`Σ w_j` + 05'in kutu başına beneği) `N` = 5 000'de ölçütü sağlıyor.

**§6 yakınsama ölçütü.** Mimarın dondurduğu tanım (`δ_E`, ham varyansla `δ_V`, `δ_C`; bağımsız referanstan `S`; nokta ve GA üst ucu ≤ 0,05, kararlılık) 05 §6'ya taşınabilir. İki not:
- Sade kademede 05'in kutu başına beneği korunmalı; beneksiz tanımla `δ_V` yapısal olarak sağlanamaz (ölçüldü, §4.3).
- Tam kademede ham varyans ölçütü taranan aralıkta (≤ 2,56·10⁶ ışın) sağlanmıyor. Mimarın seçenekleri: ölçütü `V/E²` gibi ölçeksiz bir büyüklükle desteklemek, `M`'yi artırmak ya da tam kademeyi yalnız SIL'de tutmak (iskelet §8 yedeği). Karar mimarındır.

## 8. Önceki tur (ilk tur, geçersiz)

İlk turda (02:33–02:50, pilde) model 05'in hücre kuralının "birleşik" bir değişkenini, eğik hücre koordinatlarını kullanıyordu; negatif yan lobları atıyordu, sade kademede alan ağırlığı yanlıştı. Yakınsama ise `V/E²` ve geometrik enerji oranıyla ölçüldü (sol md.1–5). O turun "S2 biçimsel geçti (`N` = 80 000)" hükmü geri çekildi. Süre tablosu (672 hücre) ve S3 (p99 = 36,6 ms, `N` = 80 000) eski modele aittir. Veri: `tools/olcum/mp7/sonuc/tur1/`.

## 9. İskelet §8 sonar kararı için girdi

Bu paragraf M3 Pro'da, orta sahnede, `D` = 2 için ölçülenleri özetler; Windows için tahmin değildir.
- **(i)** Tam kademe, dondurulmuş yakınsama ölçütünü taranan aralıkta (`N` ≤ 2 559 903) sağlamıyor. Taranan yapılandırmalarda S2 bulunamadı; `N` = 1 280 000 destek koşusunda tek iş parçacığı p99 504,4 ms, 7 m ping aralığının (9,33 ms alt sınır; 25,17 ms model değeri [T]) çok üstünde.
- **(ii)** Gerçek zamanlı bütçeye giren `N` değerleri ve bu `N`'lerdeki bilinen hata (§4.4, §4.6; eşli referansa göre `δ_E` / `δ_V`):
  - 7 m, tek iş parçacığında 9,33 ms için `N` ≤ 20 000: 0,146 / 0,732.
  - 7 m, tek iş parçacığında 25,17 ms için `N` ≤ 40 014: 0,122 / 0,305.
  - 7 m, 11 iş parçacığında 9,33 ms için `N` ≤ 80 000: 0,110 / 0,263.
  - 50 m, tek iş parçacığında `N` ≤ 80 000: 0,071 / 0,257.
  - 50 m, iki ve daha fazla iş parçacığında `N` ≤ 160 008: 0,055 / 0,095.
  - Bu hatalar, gözlenen iki bağımsız kümenin farkıyla (7 m'de 0,154 / 0,491) aynı mertebede (genel bir belirsizlik tabanı değildir; §4.6).
- **(iii)** Sade kademe `N` = 5 000'de ölçütü sağlıyor. Tek iş parçacığı p99 7 m'de 0,93 ms, 50 m'de 0,79 ms: iki menzilin de iki eşiğinin altında.

## Ek A — Bütün süre hücreleri (S1, düzeltme turu)

Hücre sayısı: tam 216, sade 32; ping sayıları: 200

| Sahne | Üçgen | BVH kurulumu (ms) | Sahne RSS artışı (MB) | Embree sahne belleği (MB) | Süreç tepe RSS (MB) |
|---|---|---|---|---|---|
| 1e4 | 9886 | 1,70–4,40 | 2,90 | 0,80 | 190,60 |
| 1e5 | 100368 | 14,90–16,90 | 14,20 | 7,80 | 269,90 |
| 1e6 | 999558 | 163,60–164,10 | 109,60 | 81,20 | 211,00 |

#### Kademe: tam (hücre başına p50 / p99 / en büyük, ms)

| Sahne | Menzil | D | N (gerçek) | 1 iş p. | 2 iş p. | 4 iş p. | 11 iş p. | kesişim/ping | görünürlük/ping |
|---|---|---|---|---|---|---|---|---|---|
| 1e4 | 7 m | 0 | 20000 | 3,00 / 3,84 / 3,88 | — | — | — | 20000 | 0 |
| 1e4 | 7 m | 0 | 80000 | 11,01 / 13,91 / 14,03 | — | — | — | 80000 | 0 |
| 1e4 | 7 m | 1 | 20000 | 4,18 / 6,75 / 6,83 | — | — | — | 29563 | 1351 |
| 1e4 | 7 m | 1 | 80000 | 15,77 / 25,27 / 25,59 | — | — | — | 118251 | 5404 |
| 1e4 | 7 m | 2 | 20000 | 4,73 / 9,48 / 9,59 | — | — | — | 32739 | 4526 |
| 1e4 | 7 m | 2 | 80000 | 17,92 / 35,58 / 36,07 | — | — | — | 130954 | 18105 |
| 1e4 | 50 m | 0 | 20000 | 3,84 / 4,06 / 4,16 | — | — | — | 20000 | 0 |
| 1e4 | 50 m | 0 | 80000 | 14,51 / 15,33 / 15,42 | — | — | — | 80000 | 0 |
| 1e4 | 50 m | 1 | 20000 | 7,83 / 8,82 / 8,84 | — | — | — | 39081 | 7601 |
| 1e4 | 50 m | 1 | 80000 | 29,99 / 33,48 / 33,52 | — | — | — | 156322 | 30400 |
| 1e4 | 50 m | 2 | 20000 | 12,03 / 13,26 / 13,52 | — | — | — | 56647 | 25165 |
| 1e4 | 50 m | 2 | 80000 | 46,43 / 49,90 / 50,89 | — | — | — | 226585 | 100656 |
| 1e5 | 7 m | 0 | 999 | 0,20 / 0,25 / 0,25 | 0,14 / 0,18 / 0,18 | 0,13 / 0,16 / 0,18 | 0,15 / 0,18 / 0,36 | 999 | 0 |
| 1e5 | 7 m | 0 | 2002 | 0,33 / 0,42 / 0,43 | 0,23 / 0,29 / 0,30 | 0,19 / 0,24 / 0,24 | 0,19 / 0,25 / 0,25 | 2002 | 0 |
| 1e5 | 7 m | 0 | 5000 | 0,70 / 0,95 / 0,98 | 0,46 / 0,61 / 0,63 | 0,35 / 0,48 / 0,48 | 0,32 / 0,45 / 0,46 | 5000 | 0 |
| 1e5 | 7 m | 0 | 9996 | 1,29 / 1,64 / 1,70 | 0,81 / 1,10 / 1,12 | 0,58 / 0,84 / 0,85 | 0,54 / 0,78 / 0,98 | 9996 | 0 |
| 1e5 | 7 m | 0 | 20000 | 2,45 / 3,14 / 3,17 | 1,50 / 2,06 / 2,07 | 1,04 / 1,55 / 1,57 | 0,85 / 1,45 / 1,46 | 20000 | 0 |
| 1e5 | 7 m | 0 | 40014 | 4,74 / 6,67 / 7,01 | 2,80 / 3,88 / 3,93 | 1,84 / 2,76 / 2,79 | 1,48 / 2,38 / 2,41 | 40014 | 0 |
| 1e5 | 7 m | 0 | 80000 | 9,13 / 11,62 / 11,84 | 5,26 / 7,23 / 7,47 | 3,35 / 4,99 / 5,08 | 2,56 / 4,28 / 4,31 | 80000 | 0 |
| 1e5 | 7 m | 0 | 160008 | 17,91 / 22,12 / 22,70 | 10,02 / 13,62 / 13,98 | 6,28 / 9,23 / 9,25 | 4,67 / 7,32 / 7,47 | 160008 | 0 |
| 1e5 | 7 m | 1 | 999 | 0,26 / 0,39 / 0,40 | 0,19 / 0,27 / 0,28 | 0,15 / 0,21 / 0,22 | 0,17 / 0,22 / 0,31 | 1478 | 77 |
| 1e5 | 7 m | 1 | 2002 | 0,47 / 0,69 / 0,70 | 0,31 / 0,47 / 0,48 | 0,23 / 0,35 / 0,38 | 0,23 / 0,32 / 0,33 | 2963 | 154 |
| 1e5 | 7 m | 1 | 5000 | 1,04 / 1,58 / 1,60 | 0,64 / 1,01 / 1,02 | 0,45 / 0,73 / 0,73 | 0,37 / 0,60 / 0,60 | 7400 | 386 |
| 1e5 | 7 m | 1 | 9996 | 1,97 / 3,07 / 3,13 | 1,18 / 1,88 / 1,91 | 0,79 / 1,28 / 1,30 | 0,61 / 0,99 / 1,00 | 14794 | 770 |
| 1e5 | 7 m | 1 | 20000 | 3,79 / 5,97 / 5,99 | 2,23 / 3,61 / 3,65 | 1,44 / 2,41 / 2,42 | 1,07 / 1,81 / 1,83 | 29600 | 1541 |
| 1e5 | 7 m | 1 | 40014 | 7,35 / 11,57 / 11,75 | 4,22 / 6,97 / 7,13 | 2,70 / 4,52 / 4,54 | 1,90 / 3,42 / 3,48 | 59221 | 3081 |
| 1e5 | 7 m | 1 | 80000 | 14,20 / 22,33 / 22,60 | 8,05 / 12,91 / 13,59 | 4,90 / 8,26 / 8,32 | 3,39 / 6,00 / 6,03 | 118400 | 6161 |
| 1e5 | 7 m | 1 | 160008 | 28,00 / 43,91 / 45,49 | 15,40 / 24,71 / 24,94 | 9,28 / 15,39 / 15,48 | 6,36 / 10,86 / 11,07 | 236812 | 12322 |
| 1e5 | 7 m | 2 | 999 | 0,33 / 0,55 / 0,56 | 0,22 / 0,36 / 0,36 | 0,18 / 0,29 / 0,29 | 0,19 / 0,27 / 0,28 | 1670 | 268 |
| 1e5 | 7 m | 2 | 2002 | 0,58 / 1,03 / 1,03 | 0,37 / 0,65 / 0,68 | 0,28 / 0,49 / 0,50 | 0,26 / 0,44 / 0,45 | 3346 | 537 |
| 1e5 | 7 m | 2 | 5000 | 1,33 / 2,38 / 2,44 | 0,81 / 1,49 / 1,50 | 0,57 / 1,06 / 1,06 | 0,46 / 0,82 / 0,85 | 8358 | 1343 |
| 1e5 | 7 m | 2 | 9996 | 2,54 / 4,65 / 4,73 | 1,52 / 2,83 / 2,92 | 1,01 / 1,97 / 2,00 | 0,77 / 1,46 / 1,50 | 16709 | 2684 |
| 1e5 | 7 m | 2 | 20000 | 4,88 / 9,03 / 9,14 | 2,91 / 5,48 / 5,52 | 1,88 / 3,70 / 3,74 | 1,36 / 2,73 / 2,76 | 33432 | 5371 |
| 1e5 | 7 m | 2 | 40014 | 9,42 / 17,46 / 18,01 | 5,43 / 10,45 / 10,60 | 3,45 / 6,94 / 6,98 | 2,45 / 5,06 / 5,17 | 66885 | 10744 |
| 1e5 | 7 m | 2 | 80000 | 18,34 / 34,01 / 34,12 | 10,38 / 19,58 / 19,65 | 6,29 / 12,75 / 12,90 | 4,32 / 9,13 / 9,18 | 133724 | 21481 |
| 1e5 | 7 m | 2 | 160008 | 36,10 / 65,24 / 67,47 | 19,88 / 37,20 / 37,33 | 12,33 / 23,74 / 23,90 | 8,00 / 25,23 / 27,48 | 267463 | 42966 |
| 1e5 | 50 m | 0 | 999 | 0,24 / 0,28 / 0,29 | 0,19 / 0,21 / 0,21 | 0,17 / 0,18 / 0,18 | 0,19 / 0,33 / 0,37 | 999 | 0 |
| 1e5 | 50 m | 0 | 2002 | 0,42 / 0,47 / 0,47 | 0,31 / 0,35 / 0,35 | 0,26 / 0,27 / 0,28 | 0,26 / 0,30 / 0,33 | 2002 | 0 |
| 1e5 | 50 m | 0 | 5000 | 0,99 / 1,09 / 1,10 | 0,64 / 0,71 / 0,72 | 0,50 / 0,53 / 0,54 | 0,46 / 0,51 / 0,52 | 5000 | 0 |
| 1e5 | 50 m | 0 | 9996 | 1,82 / 2,05 / 2,16 | 1,19 / 1,28 / 1,31 | 0,91 / 0,95 / 0,95 | 0,78 / 0,84 / 0,84 | 9996 | 0 |
| 1e5 | 50 m | 0 | 20000 | 3,46 / 3,73 / 3,75 | 2,24 / 2,38 / 2,44 | 1,70 / 1,77 / 1,77 | 1,41 / 1,53 / 1,54 | 20000 | 0 |
| 1e5 | 50 m | 0 | 40014 | 6,91 / 7,52 / 7,59 | 4,41 / 4,76 / 4,89 | 3,16 / 3,29 / 3,30 | 2,61 / 3,03 / 3,06 | 40014 | 0 |
| 1e5 | 50 m | 0 | 80000 | 13,44 / 15,69 / 16,36 | 8,44 / 9,05 / 9,16 | 5,87 / 6,26 / 6,29 | 4,77 / 6,87 / 7,35 | 80000 | 0 |
| 1e5 | 50 m | 0 | 160008 | 25,55 / 27,79 / 28,84 | 15,91 / 17,25 / 17,42 | 11,18 / 12,05 / 12,15 | 8,98 / 9,88 / 10,26 | 160008 | 0 |
| 1e5 | 50 m | 1 | 999 | 0,48 / 0,55 / 0,56 | 0,31 / 0,35 / 0,36 | 0,25 / 0,29 / 0,29 | 0,23 / 0,27 / 0,28 | 1952 | 375 |
| 1e5 | 50 m | 1 | 2002 | 0,87 / 1,02 / 1,04 | 0,55 / 0,63 / 0,64 | 0,41 / 0,46 / 0,47 | 0,36 / 0,40 / 0,40 | 3913 | 752 |
| 1e5 | 50 m | 1 | 5000 | 2,06 / 2,35 / 2,38 | 1,24 / 1,49 / 1,51 | 0,86 / 0,96 / 0,97 | 0,67 / 0,76 / 0,76 | 9772 | 1881 |
| 1e5 | 50 m | 1 | 9996 | 4,05 / 4,58 / 4,66 | 2,34 / 2,66 / 2,68 | 1,62 / 1,80 / 1,82 | 1,20 / 1,36 / 1,37 | 19536 | 3759 |
| 1e5 | 50 m | 1 | 20000 | 7,79 / 8,81 / 8,87 | 4,53 / 5,12 / 5,14 | 3,05 / 3,44 / 3,46 | 2,26 / 2,54 / 2,54 | 39088 | 7522 |
| 1e5 | 50 m | 1 | 40014 | 15,04 / 16,89 / 16,90 | 8,73 / 9,84 / 9,88 | 5,83 / 6,59 / 8,00 | 4,23 / 4,79 / 4,81 | 78202 | 15044 |
| 1e5 | 50 m | 1 | 80000 | 28,77 / 31,78 / 31,92 | 17,02 / 19,12 / 19,68 | 11,12 / 12,60 / 12,61 | 8,03 / 9,08 / 9,40 | 156349 | 30081 |
| 1e5 | 50 m | 1 | 160008 | 55,88 / 63,53 / 66,34 | 33,10 / 36,64 / 36,94 | 21,55 / 24,48 / 24,57 | 15,27 / 17,42 / 17,54 | 312714 | 60165 |
| 1e5 | 50 m | 2 | 999 | 0,73 / 0,83 / 0,84 | 0,50 / 0,57 / 0,57 | 0,37 / 0,42 / 0,42 | 0,33 / 0,36 / 0,37 | 2836 | 1259 |
| 1e5 | 50 m | 2 | 2002 | 1,35 / 1,55 / 1,56 | 0,91 / 1,03 / 1,03 | 0,63 / 0,67 / 0,68 | 0,53 / 0,63 / 0,82 | 5684 | 2523 |
| 1e5 | 50 m | 2 | 5000 | 3,22 / 3,63 / 3,70 | 2,06 / 2,30 / 2,33 | 1,38 / 1,51 / 1,53 | 1,06 / 1,16 / 1,18 | 14195 | 6303 |
| 1e5 | 50 m | 2 | 9996 | 6,42 / 7,25 / 7,52 | 3,90 / 4,40 / 4,42 | 2,61 / 2,87 / 2,88 | 1,98 / 2,13 / 2,14 | 28379 | 12602 |
| 1e5 | 50 m | 2 | 20000 | 12,56 / 14,06 / 14,11 | 7,56 / 8,34 / 8,38 | 5,03 / 5,45 / 5,47 | 3,72 / 4,02 / 4,05 | 56780 | 25212 |
| 1e5 | 50 m | 2 | 40014 | 24,02 / 26,52 / 26,61 | 14,51 / 16,01 / 16,09 | 9,72 / 10,53 / 10,63 | 7,08 / 7,66 / 8,06 | 113601 | 50439 |
| 1e5 | 50 m | 2 | 80000 | 48,22 / 53,46 / 55,04 | 28,52 / 31,13 / 31,23 | 18,62 / 20,42 / 20,58 | 13,51 / 14,69 / 15,07 | 227122 | 100845 |
| 1e5 | 50 m | 2 | 160008 | 93,39 / 103,68 / 103,81 | 55,99 / 62,03 / 62,29 | 37,57 / 54,02 / 54,85 | 27,47 / 40,36 / 49,49 | 454268 | 201700 |
| 1e6 | 7 m | 0 | 20000 | 2,82 / 3,55 / 3,58 | — | — | — | 20000 | 0 |
| 1e6 | 7 m | 0 | 80000 | 10,31 / 13,01 / 13,23 | — | — | — | 80000 | 0 |
| 1e6 | 7 m | 1 | 20000 | 4,38 / 6,71 / 6,72 | — | — | — | 29601 | 1533 |
| 1e6 | 7 m | 1 | 80000 | 17,04 / 26,92 / 36,64 | — | — | — | 118402 | 6132 |
| 1e6 | 7 m | 2 | 20000 | 5,51 / 10,11 / 10,46 | — | — | — | 33431 | 5363 |
| 1e6 | 7 m | 2 | 80000 | 21,27 / 38,24 / 61,33 | — | — | — | 133724 | 21450 |
| 1e6 | 50 m | 0 | 20000 | 3,84 / 4,08 / 4,10 | — | — | — | 20000 | 0 |
| 1e6 | 50 m | 0 | 80000 | 14,27 / 14,92 / 15,06 | — | — | — | 80000 | 0 |
| 1e6 | 50 m | 1 | 20000 | 8,90 / 10,32 / 10,37 | — | — | — | 39088 | 7463 |
| 1e6 | 50 m | 1 | 80000 | 33,24 / 38,86 / 40,64 | — | — | — | 156352 | 29856 |
| 1e6 | 50 m | 2 | 20000 | 14,72 / 16,99 / 17,10 | — | — | — | 56768 | 25141 |
| 1e6 | 50 m | 2 | 80000 | 53,66 / 58,67 / 59,44 | — | — | — | 227071 | 100566 |

#### Kademe: sade (hücre başına p50 / p99 / en büyük, ms)

| Sahne | Menzil | D | N (gerçek) | 1 iş p. | 11 iş p. | kesişim/ping | görünürlük/ping |
|---|---|---|---|---|---|---|---|
| 1e5 | 7 m | 0 | 999 | 0,14 / 0,18 / 0,18 | 0,08 / 0,10 / 0,10 | 999 | 0 |
| 1e5 | 7 m | 0 | 2002 | 0,25 / 0,32 / 0,33 | 0,10 / 0,14 / 0,16 | 2002 | 0 |
| 1e5 | 7 m | 0 | 5000 | 0,57 / 0,74 / 0,75 | 0,17 / 0,21 / 0,22 | 5000 | 0 |
| 1e5 | 7 m | 0 | 9996 | 1,10 / 1,46 / 1,47 | 0,26 / 0,32 / 0,33 | 9996 | 0 |
| 1e5 | 7 m | 0 | 20000 | 2,05 / 3,25 / 3,61 | 0,44 / 0,55 / 0,58 | 20000 | 0 |
| 1e5 | 7 m | 0 | 40014 | 4,04 / 5,60 / 5,77 | 0,84 / 1,09 / 1,12 | 40014 | 0 |
| 1e5 | 7 m | 0 | 80000 | 8,38 / 11,57 / 11,61 | 1,52 / 2,01 / 2,05 | 80000 | 0 |
| 1e5 | 7 m | 0 | 160008 | 16,34 / 22,77 / 23,12 | 3,06 / 4,08 / 4,32 | 160008 | 0 |
| 1e5 | 50 m | 0 | 999 | 0,18 / 0,20 / 0,21 | 0,09 / 0,11 / 0,13 | 999 | 0 |
| 1e5 | 50 m | 0 | 2002 | 0,32 / 0,36 / 0,37 | 0,12 / 0,14 / 0,15 | 2002 | 0 |
| 1e5 | 50 m | 0 | 5000 | 0,75 / 0,85 / 0,87 | 0,21 / 0,26 / 0,31 | 5000 | 0 |
| 1e5 | 50 m | 0 | 9996 | 1,44 / 1,63 / 1,66 | 0,31 / 0,34 / 0,35 | 9996 | 0 |
| 1e5 | 50 m | 0 | 20000 | 2,80 / 3,18 / 3,21 | 0,55 / 0,64 / 0,64 | 20000 | 0 |
| 1e5 | 50 m | 0 | 40014 | 5,58 / 6,24 / 6,42 | 1,10 / 1,30 / 1,36 | 40014 | 0 |
| 1e5 | 50 m | 0 | 80000 | 11,04 / 12,39 / 12,54 | 2,00 / 2,31 / 2,38 | 80000 | 0 |
| 1e5 | 50 m | 0 | 160008 | 21,42 / 24,78 / 25,58 | 4,14 / 4,82 / 4,98 | 160008 | 0 |

#### S3 uzun koşusu (1e5, 7 m, D = 2, tek iş parçacığı)

| N (gerçek) | ping | p50 | p99 | en büyük | ortalama (ms) |
|---|---|---|---|---|---|
| 1280000 | 2000 | 291,62 | 504,45 | 544,68 | 304,60 |
