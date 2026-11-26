# W5-viz-gorsel — 3B görünümün tanısı, görüntüleri ve kare süresi (Mac)

| | |
|---|---|
| Görev | `docs/gorevler/W5-viz-gorsel.md` |
| Dal | `is/w5-viz-gorsel` |
| Tarih | 2026-11-25 / 2026-11-26 |
| Uygulayan | `ida-hafif-uygulayici` (Sonnet) |
| Makine | Apple M3 Pro, 18 GB, dahili Liquid Retina XDR ekran; macOS 26.5.2; Godot 4.7.2-stable official, Metal 4.0, Forward+. Makine boş değildi (başka agent'ların derleme ve test işleri koşuyordu; yük ortalaması 2,4–3,7) |

## 1. Tanı: görüntü neden siyah ve ışıksızdı

Kullanıcının gözlemi: `viz/calistir.sh --sim scenarios/gorev-havuz-3b.toml` → arka plan düz siyah, taban ışıksız düz beyaz, solda gri bir kavis, "SIL · 3B sade".

**Neden senaryodadır, görselleştirici hatası değildir.** `gorev-havuz-3b.toml` `ortam.su_optigi` ve `ortam.isik` vermiyordu; sim A5-K101 gereği bunları "kalibre edilmedi" değerleriyle çözümler (`docs/sozlesmeler/A5-senaryo.md` Ek E `[kalibre_edilmedi_ortam]`: su optiği katsayıları 0 = berrak su, ortam ışığı 0, güneş tepede, 1000 W/m²) ve A2 `Ortam` bunları aynen taşır. Görselleştiricinin günlüğünden (`ida-viz: ortam (A2): ...`, bu iş sırasında eklendi), değişiklikten önceki senaryoyla ölçülen değerler:

| A2 `Ortam` alanı | Değer | Görüntüdeki sonucu |
|---|---|---|
| `ortam_isigi` (doğrusal RGB) | [0, 0, 0] | su altı ve üst ortam ışığı enerjisi 0: güneş görmeyen her yüzey (duvarlar) siyah |
| `beta_d` (1/m) | [0, 0, 0] | zayıflama yok; sade kademede aşağı inen ışığın soğurması 1 |
| `beta_b` (1/m) | [0, 0, 0] | Godot sisinin yoğunluğu 0: mesafeyle renk yok |
| `b_sonsuz` (doğrusal RGB) | [0, 0, 0] | su altı arka plan rengi ve sis rengi siyah; sade su yüzeyi (`su_ust_sade`) rengi siyah ("su siyah, tabanda araç") |
| `gunes_isinimi` | 1000 W/m² | Godot güneş ışık enerjisi 1,43 (700 W/m² = 1,0 görsel eşlemesi; A2-A06 açık) |
| `gunes_yonu_n` | tepe (NED [0, 0, −1]) | yatay yüzeyler (taban, duvarların üst yüzü) tam aydınlık, düşey yüzeyler (duvarlar) hiç ışık almaz: "ışıksız düz beyaz taban" ve siyah duvarlar |
| tür | 1 (havuz), statik varlık `havuz_ornek` | |

"Solda gri kavis" (**çıkarım; ayrıca doğrulanmadı**): güneşe dönük tek duvar yüzeyi olan duvar ÜST yüzü (0,2 m kalınlık) ya da aracın önündeki iki spot ışığın (A7 `KHR_lights_punctual`, `isik_sancak`/`isik_iskele`) duvarda bıraktığı, sis olmadığı için beyaza taşan leke. Düzeltmeden sonra ikincisi aynı biçimde görünüyor (ışık çarpanı 0,35'e indirildi), bu yüzden leke olasılığı yüksektir.

**Düzeltme (iki katman).**
1. Senaryo: A5-K101 "görüntü gerçekçiliği için senaryo yazarı vermelidir" der. `gorev-havuz.toml`, `gorev-havuz-3b.toml` ve `gorev-havuz-hil.toml`'a (üçü `tests/butunleme/test_sunucu_kucuk.cpp`'nin gövde eşitliği denetimi yüzünden aynı metinle) `[ortam.su_optigi]` (β_d [0,30; 0,055; 0,04], β_b [0,11; 0,065; 0,055], B∞ [0,02; 0,20; 0,26]) ve `[ortam.isik].ortam_isigi_rgb_dogrusal` ([0,30; 0,36; 0,42]) eklendi. Değerler ölçüm değil yer tutucudur. Güneş yönü ve ışınımı KE varsayılanında bırakıldı (`tests/butunleme/test_a2.cpp` ve `sim/sunucu/test/test_a2_baglanti.cpp` bunları 1000 W/m² ve (0,0,−1) olarak bekler).
2. Görselleştirici: senaryo su optiğini ya da ortam ışığını vermemişse ya da sıfır vermişse (A2 bu iki durumu ayırmaz; su optiğinde üç katsayı grubunun hepsi sıfır, ortam ışığında üç kanal sıfır) kendi görsel varsayılanını kullanır (`ana.gd` `GORSEL_*`; yalnız çizim, fiziğe ve sensörlere dokunmaz), günlüğe "görsel varsayılan kullanıldı: ... (sıfır geldi; verilmemiş sayıldı, görsel varsayılan kullanılıyor)" yazar. Bugün `viz/proje`'de kamera sensörü hattı yok. Hat eklendiğinde kamera karesi `GORSEL_*` varsayılanlarını, araç ışık çarpanını ve karo dokusunu değil, A2 değerlerini aynen kullanmalı. Sözleşme değişmedi (W5-K60). `scenarios/sonar-havuz.toml` (optik vermiyor) böyle çalışıyor.

Sim'in okuduğu değerlerde eksiklik bulunmadı: senaryonun verdiği değerler A2 `Ortam`'a aynen ulaşır. Sözleşme: A2 `Ortam` verildi/kalibre edilmedi ayrımı taşımıyor; açıkça verilen 0 (berrak su, karanlık sahne) ayırt edilemiyor. Görselleştirici sıfırı "verilmedi" sayar (makul okuma; W5-K60); kapanış yeri A2 küçük sürümüdür (`docs/mimari/acik-bulgular.md`). Belirsizlik: sade/tam dalga yüzeyi için A2 dalga kümesi sim'de boştur (`sim/sunucu/src/a2_baglanti.cpp`: "fizikte dalga modeli yok"); açık deniz senaryosunda yüzeyin dalgalı görünmesi yalnız gölgelendiricinin ayrıntı normallerindendir, `a2.su_yuksekligi` düzdür (W5-K61).

## 2. Ekran görüntüleri

Dizin: `docs/olcumler/W5-viz-gorsel-ekran/` (1280×720, JPEG). `once_*` değişiklikten önceki görüntü. Koşu: `viz/calistir.sh --sim <senaryo> --hiz 4 --cozunurluk 1280x720 -- --ekran-goruntusu=<dizin> --ekran-plani=<...>`.

| Dosya | İçerik |
|---|---|
| `once_havuz_sade_yuzey.jpg`, `once_havuz_sade_alt.jpg` | önce: siyah su, ışıksız taban (t = 5 s ve 50 s) |
| `havuz_sade_yuzey.jpg` (t = 6 s, su üstü), `havuz_sade_gecis.jpg` (36 s, yüzeye yakın, su altından bakış), `havuz_sade_alt.jpg` (52 s, 1,5 m), `havuz_sade_alt2.jpg` (70 s) | `gorev-havuz-3b.toml`, sade kademe |
| `havuz_tam_yuzey.jpg`, `havuz_tam_gecis.jpg`, `havuz_tam_alt.jpg` | aynı senaryo, `kalite.gorunum = "tam"` (geçici kopya; depoya girmedi) |
| `acik_tam_yuzey.jpg`, `acik_tam_gecis.jpg`, `acik_tam_alt.jpg`, `acik_tam_alt2.jpg` | `gorev-acik-su-3b.toml` (tam kademe) |

## 3. Kare süresi (`--olcum`; kabul 4, kapsam 6)

**Düzenek.** `viz/calistir.sh --sim`'in yaptığı gibi `ida_sim` (`--sil-hiz 4`, SIL) ve Godot (`--resolution 1920x1080 --max-fps 0`, `--olcum=<json>`) başlatılır; kare süresi `ana.gd`'nin ardışık `_process` çağrılarının aralığıdır, pencere sim zamanının 5 s – 116 s arasını (yüzeyde bekleme, yüzeyde seyir, dalış, su altı) kapsar, ekran görüntüsü anları hariç. Çözünürlük 1920×1080, kademe senaryonun `kalite.gorunum`undandır: sade = `gorev-havuz-3b.toml`; tam = aynı senaryonun `kalite.gorunum = "tam"` geçici kopyası (depoya girmedi). **Önce** = `da4192f`/`f8b8f5d` ağacının (`git archive`) temiz kopyası (eski senaryo, eski varlıklar, eski `ana.gd`); **sonra** = bu dalın ucu. Önce ve sonra koşuları art arda, aynı makine durumunda alındı. Kare süreleri ms. `proje` varsayılanı olan 120 fps sınırı kare süresini 8,33 ms'ye kilitlediği için ölçümde `--max-fps 0` verildi (önce, sınırlı: sade ve tam ikisi de 8,33 ms, n = 3 189).

| Kademe | Koşu | n | ortalama | p50 | p95 | p99 | çizim CPU p50 / p99 |
|---|---|---|---|---|---|---|---|
| sade, önce | 1 | 13 422 | 1,98 | 1,94 | 2,54 | 7,22 (aykırı) | 0,105 / 0,357 |
| sade, önce | 2 | 14 467 | 1,84 | 1,71 | 2,44 | 2,61 | 0,112 / 0,346 |
| sade, sonra | 1 | 14 169 | 1,88 | 1,75 | 2,50 | 2,69 | 0,113 / 0,364 |
| sade, sonra | 2 | 14 200 | 1,87 | 1,73 | 2,49 | 2,69 | 0,114 / 0,359 |
| tam, önce | 1 | 5 222 | 5,09 | 4,79 | 6,24 | 6,45 | 0,212 / 0,518 |
| tam, önce | 2 | 5 206 | 5,11 | 4,79 | 6,29 | 6,46 | 0,203 / 0,500 |
| tam, sonra | 1 | 5 158 | 5,15 | 4,76 | 6,36 | 6,52 | 0,211 / 0,529 |
| tam, sonra | 2 | 5 150 | 5,16 | 4,77 | 6,40 | 6,52 | 0,225 / 0,535 |

Yeni sahne (önce yok; bilgi): `gorev-acik-su-3b.toml` tam: ortalama 6,10, p50 5,68, p95 8,24, p99 8,43 (n = 4 354); aynı senaryo sade (geçici kopya): p50 1,69, p95 2,63, p99 2,88 (n = 14 033).

**Sonuç.** Sade kademenin kare süresi değişiklikle artmadı (p50 1,71–1,94 → 1,73–1,75 ms; koşular arası değişim farkın üstünde). Tam kademe ortalamada +0,05–0,07 ms, p95'te +0,1 ms (ölçüm gürültüsü düzeyinde). W5-K63 gereği ek maliyeti tam kademeye taşımaya gerek kalmadı. Açık deniz sahnesi tam kademede havuzdan yaklaşık 1 ms pahalı (daha büyük görünür taban ve sis yolu; neden ayrıca araştırılmadı).

**Sınırlar (ölçülmeyenler).**
- Yalnız bu Mac (Apple M3 Pro, Metal). Hedef Windows makinesi (8 GB, dahili ekran kartı) ölçülmedi; bu rapor onun için rakam tahmin etmez.
- GPU süresi ölçülemedi: `RenderingServer.viewport_get_measured_render_time_gpu` bu Mac'te her kareyi 0,0 verdi (`gpu_ms_*` = 0; nedeni araştırılmadı). Tablodaki "çizim CPU" yalnız CPU tarafının çizim komutu hazırlama süresidir.
- Bu iş sırasında ölçüm makinesi bir süre (art arda yaklaşık on koşu boyunca) `--max-fps 0` iken bile sade kademede ≈6,9 ms'ye kilitlendi (önce ve sonra aynı 6,84–6,90 ms, n ≈ 3 900–4 500; `--always-on-top` ile 8,2 ms; pencerenin gizlenmesi değiştirmedi). Nedeni doğrulanmadı (macOS pencere sunumu ya da güç/ısı durumu olabilir; MP6'daki görünmeyen pencere bulgusuyla ilgili olabilir). O dönemin koşuları kare süresini değil sunum hızını ölçtüğü için tablo dışı bırakıldı; tablodaki sekiz koşu bu durumdan sonra, art arda ve aynı durumda alındı. Bu nedenle kare süresi karşılaştırması yalnız tablodaki koşular için geçerlidir.

## 4. Doğrulanmayanlar

- `acik_tam_gecis.jpg` su yüzeyini zayıf gösteriyor; yüzey geçişi görüntüsü kabul 2'yi ancak kısmen karşılıyor. Dalgalı yüzey ayrıca alınamadı: sim'de dalga modeli yok, A2 dalga kümesi boş, açık deniz yüzeyi düz (§1; W5-K61).
- "Solda gri kavis" açıklaması bir çıkarımdır (§1).
- Hedef Windows makinesi ve GPU süresi ölçülmedi (§3 "Sınırlar").
