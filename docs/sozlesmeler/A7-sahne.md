# A7 — Sahne ve varlıklar sözleşmesi

| | |
|---|---|
| Sürüm | 1.0-aday (2026-11-19) |
| Görev | `docs/gorevler/W1-A2A7.md`; ortak kurallar `docs/gorevler/W1-ortak.md` |
| Makinece okunur tanım | `assets/manifest.schema.json` (JSON Schema 2020-12); en küçük örnek `assets/ornek/manifest.json` |
| İlgili sözleşmeler | A2 (`docs/sozlesmeler/A2-gorsellestirici.md`), A5 (senaryo; malzeme geçersiz kılma), A6 (kayıt) |
| Dayanak | İskelet 0.8 §3 (A7 satırı), §3.1, §5, §7; glTF 2.0 belirtimi (Khronos); 00 belgesi 12.16; bölüm 04 §2.2, §2.4, §2.9; 05 §2.4.1; 07 §2.8 (taslak; gerekçe için) |

Atıf biçimi: `A7-Knn` bu belgenin kuralı; `A2-Knn` A2 sözleşmesinin kuralı; `04 §n`, `05 §n`, `07 §n` taslak mimari bölüm başlığı (bağlayıcı değildir); `AB-nnn` açık bulgu; `12.n` 00 belgesindeki düzeltme satırı.

glTF'e ilişkin bütün olgular glTF 2.0 belirtiminin Khronos deposundaki kaynağından (`specification/2.0/Specification.adoc` ve `schema/*.schema.json`) doğrulandı; Godot'ya ilişkin olanlar `godotengine/godot` deposunun `4.7.2-stable` etiketindeki sınıf belgelerinden.

---

## 1. Kapsam ve taraflar

**Amaç.** Sahne ve model kaynaklarının **tek kaynaktan** bütün tüketicilere dağıtım biçimini sabitlemek: görsel model, sonar geometrisi, yüzerlik ve çarpışma ağları, bağlantı noktaları ve malzemelerin optik ve akustik varsayılanları (iskelet §3 A7 satırı).

**Taraflar.**

- **Üretici (yazar):** varlık kümesini hazırlayan kişi ya da araç. Kaynak `.glb` dosyalarını üretir (Blender'dan dışa aktararak, betikle ya da elle) ve manifest üretme aracıyla `manifest.json`'u yazar. Bunlar depoya girer.
- **Türetme adımı (derleme):** kaynaktan türetilen ağları (dışbükey ayrıştırma çıktıları) sabitlenmiş araç sürümüyle üretir ve manifestteki özetle doğrular. Çıktıları depoya girmez.
- **Tüketiciler:** sim sunucusu (fizik çekirdeği: yüzerlik ve çarpışma, 04; sonar: akustik ağlar, 05; hedef ve torpido şekilleri) ve görselleştirici (görsel ağlar, malzemeler, ışıklar; 07). Kamera algılama bileşeni (A8) varlık kullanmaz.

**Kapsam dışı.** Senaryonun hangi kümeyi ve hangi varlıkları kullandığı, varlıkların başlangıç pozları ve malzeme değerlerinin senaryodan geçersiz kılınma sözdizimi (A5); İDA-0'ın kütle, atalet, hidrodinamik katsayı ve itici yerleşimi değerleri (A5; 04 §2.6); manifest üretme ve türetme araçlarının kendisi (uygulama işi); açık su tabanının batimetrisi (A5'teki ortam tanımı; A7 yalnız ağ olarak verilen statik sahneyi kapsar); su altı optiği katsayıları (A5, A2 `ortam`).

---

## 2. Terimler ve kabuller

Birim, çerçeve ve kuaterniyon kabulleri iskelet §3.1'dedir. Bu sözleşmeye özgü olanlar:

| Terim | Anlamı |
|---|---|
| Varlık kümesi | `assets/<kume_kimligi>/` dizini: bir `manifest.json` ve onun adını verdiği `.glb` dosyaları. Bir çalıştırma tek bir küme kullanır |
| Varlık | Kümedeki tek bir `.glb` dosyası ve manifestteki girdisi; türü `arac`, `torpido`, `hedef` ya da `statik` |
| Düğüm rolü | Manifestin bir glTF düğümüne verdiği anlam: görsel, yüzerlik, çarpışma, akustik, referans, bağlantı noktası, isabet bölgesi |
| Gövde çerçevesi | Varlığın `REF` düğümünün yerel çerçevesinden A7-K14'teki `M` matrisiyle elde edilen çerçeve (x ileri, y sancak, z aşağı; iskelet §3.1); orijini gövde referans noktası `O_b` |
| Türetilmiş ağ | Kaynak ağdan bir araçla üretilen ağ (bu sürümde yalnız CoACD dışbükey parçaları) |
| Kanonik geometri özeti | Türetilmiş ağın, dosya biçiminden bağımsız tanımlı bayt dizisi üzerinden SHA-256'sı (A7-K42) |
| Manifest özeti | `manifest.json` dosyasının ham baytlarının SHA-256'sı (A7-K05). A2 el sıkışması ve A6 kaydı bu değeri kullanır |
| Platform anahtarı | `macos-arm64` ya da `windows-x64` |

---

## 3. Veri tanımları

### 3.1 Dosya düzeni

```
assets/
  manifest.schema.json            A7 şeması (bu sözleşmenin parçası)
  <kume_kimligi>/
    manifest.json                 küme manifesti (depoda)
    <varlik>.glb                  kaynak varlıklar (depoda)
    kaynak/<ad>.<uzantı>          isteğe bağlı yazım kaynakları (ör. .blend); çalışma zamanında okunmaz
<derleme dizini>/varlik/<kume_kimligi>/<manifest özetinin ilk 16 hanesi>/
    <varlik>.carpisma.glb         türetilmiş dışbükey parçalar (depoya girmez)
```

### 3.2 Manifestin üst düzeyi

Biçim: JSON (RFC 8259), UTF-8, bayt sırası işareti yok, satır sonu yalnız LF. Şema ek alana izin vermez. Bütün alanlar `assets/manifest.schema.json`'dadır; aşağıdaki tablolar anlamı ve şemanın sınayamadığı koşulları verir.

| Alan | Tip | Birim | Zorunlu | Anlamı | Geçerli | Geçersizse |
|---|---|---|---|---|---|---|
| `sema_surumu` | metin | — | evet | A7 şema sürümü | `"1.0"` | küme reddedilir (`SEMA`) |
| `kume_kimligi` | metin | — | evet | küme adı | `^[a-z0-9_]{1,32}$`, dizin adıyla aynı | `SEMA` |
| `kume_surumu` | tamsayı | — | evet | küme içeriği her değiştiğinde artar (A7-K07) | ≥ 1 | `SEMA` |
| `aciklama` | metin | — | hayır | serbest açıklama | ≤ 1000 karakter | `SEMA` |
| `birimler` | nesne | — | evet | `uzunluk: "m"`, `aci: "rad"`, `kutle: "kg"`, `zaman: "s"` | sabit | `BIRIM` |
| `eksenler` | nesne | — | evet | A7-K14'teki sabit dönüşümler | sabit | `EKSEN` |
| `uretim.disa_aktarici` | nesne | — | evet | `arac` (`blender`, `betik`, `elle`), `surum`, isteğe bağlı `ayarlar` (metin, sayı, mantıksal değerli düz nesne) | — | `SEMA` |
| `uretim.kaynaklar[]` | dizi | — | hayır | `kaynak/` altındaki yazım dosyaları: `yol`, `sha256` | yol `kaynak/` altında | `SEMA`; özet uyuşmazlığı yalnız manifest üretme aracında hata |
| `araclar.coacd` | nesne | — | herhangi bir çarpışma girdisi `coacd` ise evet | `surum` (ör. `1.0.14`), `kaynak_isleme` (40 haneli git işleme özeti) | — | `ARAC` |
| `malzemeler[]` | dizi | — | evet, ≥ 1 | §3.4 | kimlikler tekil | `SEMA` / `MALZEME` |
| `varliklar[]` | dizi | — | evet, 1–64 | §3.3 | kimlikler ve dosya adları tekil | `SEMA` |

### 3.3 Varlık girdisi

| Alan | Tip | Birim | Zorunlu | Anlamı | Geçerli |
|---|---|---|---|---|---|
| `kimlik` | metin | — | evet | varlık kimliği; A5 buna atıf verir | `^[a-z0-9_]{1,32}$`, kümede tekil |
| `tur` | metin | — | evet | `arac`, `torpido`, `hedef`, `statik` | A7-K24 |
| `dosya` | metin | — | evet | kümenin dizinindeki `.glb` | `^[a-z0-9_]{1,48}\.glb$` |
| `sha256` | metin | — | evet | dosyanın ham baytlarının SHA-256'sı, küçük harf onaltılık | 64 hane |
| `bayt` | tamsayı | B | evet | dosya boyutu | 20 – 20 971 520 (20 MiB; mimari seçimi) |
| `referans` | nesne | — | evet | `{dugum, ad: "REF"}`: gövde referans noktası düğümü | A7-K20 |
| `sinir_kutusu_govde_m` | nesne | m | evet | `en_kucuk`, `en_buyuk`: görsel ve çarpışma ağlarının gövde çerçevesindeki eksen hizalı sınır kutusu | A7-K23 |
| `malzeme_atamasi[]` | dizi | — | evet, ≥ 1 | `{gltf_malzeme: indeks, malzeme: kimlik}` | A7-K28 |
| `gorsel[]` | dizi | — | evet, ≥ 1 | `{dugum, ad, kosul}`; ad ayrılmış öneklerle başlamaz | A7-K21 |
| `yuzerlik[]` | dizi | — | türe bağlı (A7-K24) | `{dugum, ad: BUOY_*, kosul, hacim_m3, hacim_merkezi_govde_m, en_uzun_kenar_m}` | A7-K25 |
| `carpisma[]` | dizi | — | evet, ≥ 1 | `{dugum, ad: COL_*, ayristirma: yok \| coacd, coacd?, turetilmis_ozet?, ortu_toleransi_m?}`; son üçü yalnız ve zorunlu olarak `coacd`'de | A7-K26, A7-K40–K46 |
| `akustik[]` | dizi | — | hayır | `{dugum, ad: SON_*}`; boşsa akustik ağ çarpışma kaynağından alınır | A7-K27 |
| `baglanti_noktalari[]` | dizi | — | hayır | `{dugum, ad: MNT_<ad> \| THR_<1–8> \| TUBE_<1–2>, tur: montaj \| itici \| tup, konum_govde_m, q_govde}` | A7-K29 |
| `isabet_bolgeleri[]` | dizi | — | hayır | `{dugum, ad: HIT_*}` | A7-K24 |

**`kosul`:** `her_zaman`, `tup_1_dolu`, `tup_2_dolu`. Koşullu bir düğüm yalnız aracın o tüpü doluyken etkindir: görsel düğüm çizilir (A2 `dolu_tupler`), yüzerlik ağı deplasmana katılır (04 §2.4: kütle durumu değişince ağ kümesi değişir).

**Yüzerlik girdisinin sayısal alanları:** `hacim_m3` (> 0) kapalı ağın hacmi; `hacim_merkezi_govde_m` hacim merkezi, gövde çerçevesinde, `O_b`'ye göre (m); `en_uzun_kenar_m` ağdaki en uzun üçgen kenarı (m; 04 §2.4'teki üçgen boyutu–dalga boyu kuralı için).

**Bağlantı noktası:** `konum_govde_m` düğümün gövde çerçevesindeki konumu (m), `q_govde` düğüm çerçevesinden gövde çerçevesine dönüş (w, x, y, z). `MNT_<ad>` bir sensör montajıdır (`<ad>` A5'teki sensör kimliği), `THR_<k>` itici k, `TUBE_<k>` fırlatma tüpü k.

### 3.4 Malzeme girdisi

| Alan | Tip | Birim | Zorunlu | Anlamı | Geçerli |
|---|---|---|---|---|---|
| `kimlik` | metin | — | evet | malzeme kimliği; A5 geçersiz kılmaları buna atıf verir | `^[a-z0-9_]{1,32}$`, tekil |
| `aciklama` | metin | — | hayır | — | ≤ 200 karakter |
| `optik.model` | metin | — | evet | optik özelliklerin kaynağı: bu malzemeye atanmış glTF malzemelerinin metalik-pürüzlülük parametreleri (`pbrMetallicRoughness`: taban rengi çarpanı ve dokusu, metaliklik, pürüzlülük; glTF 2.0 belirtimi "Materials") | `"gltf_metalik_puruzluluk"` |
| `akustik.geri_sacilim_mu` | sayı | birimsiz | evet | geri saçılım katsayısı `μ` (05 §2.4.1) | 0 < μ ≤ 1 |
| `akustik.gelis_yasasi` | metin | — | evet | `lambert_cos2` (`σ_bs = μ cos²θ`) ya da `cos1` (`σ_bs = μ cos θ`) | — |
| `akustik.ayna_yansima_rs` | sayı | birimsiz | evet | speküler basınç yansıma katsayısı `R_s` | −1 ≤ R_s ≤ 1 |
| `akustik.puruzluluk_gamma_p` | sayı | birimsiz | evet | pürüzlülük çarpanı `γ_p` | 0 ≤ γ_p ≤ 1 |
| `akustik.kaynak` | metin | — | evet | değerin niteliği: `tahmin`, `literatur`, `olcum` | — |
| `akustik.dayanak` | metin | — | hayır | kaynak ya da gerekçe | ≤ 300 karakter |

Akustik alanların varsayılanı yoktur: her malzeme dört değeri de taşır. Bugün bilinen bütün değerler `tahmin`dir (05 §2.4.1: 750 kHz'te hedef ve karo yansıtıcılığı bilinmiyor).

### 3.5 Türetilmiş çarpışma dosyası

`<derleme dizini>/varlik/<kume>/<özet16>/<varlik>.carpisma.glb`: GLB sürüm 2. Her `coacd` ayrıştırmalı `COL_*` kaynak düğümü için aynı adlı bir düğüm ve ona bağlı bir ağ; ağın her temel öğesi (primitive) bir dışbükey parçadır (`mode` = 4, `POSITION` float32, indeksli). Parça köşeleri kaynak düğümün **yerel** glTF koordinatlarındadır.

### 3.6 Hata kodları

`SEMA` şema ihlali · `BIRIM` birim ya da ölçek hatası · `EKSEN` eksen kabulü uyuşmuyor · `MANIFEST_BICIM` CR, BOM ya da geçersiz UTF-8 · `EKSIK_VARLIK` dosya yok · `OZET_UYUSMAZ` dosya özeti ya da boyutu tutmuyor · `GLTF` glTF/GLB kuralı ihlali · `UZANTI` izinsiz glTF uzantısı · `DUGUM` düğüm tablosu ihlali · `MALZEME` malzeme ataması eksik ya da bilinmeyen kimlik · `KAPALI_DEGIL` yüzerlik ağı kapalı ve manifold değil · `HACIM` hacim, merkez ya da kenar manifestle tutmuyor · `DISBUKEY_DEGIL` `yok` ayrıştırmalı çarpışma ağı dışbükey değil · `ARAC` türetme aracı sürümü uyuşmuyor · `TURETILMIS_EKSIK` türetilmiş dosya yok · `TURETILMIS_OZET` kanonik özet tutmuyor ya da platform için yok · `ORTU` türetilmiş parçalar kaynağı örtmüyor · `A5_TUTARSIZ` A5 değerleriyle tutarsızlık · `TUR` varlık türünün gerektirdiği rol eksik · `ERISILEMEZ` rol düğümü seçili sahneden erişilemiyor ya da Godot'da sahne düğümü üretilmedi.

---

## 4. Davranış kuralları

### 4.1 Dosyalar, adlandırma ve özetler

- **A7-K01.** Bir varlık kümesi `assets/<kume_kimligi>/` dizinidir; içinde tam bir `manifest.json`, manifestte adı geçen bütün `.glb` dosyaları ve isteğe bağlı `kaynak/` alt dizini bulunur. Manifestte adı geçmeyen `.glb` dosyası dizinde bulunmaz (manifest üretme aracı ve depo denetimi reddeder).
- **A7-K02.** `kume_kimligi` dizin adıyla aynıdır; değilse küme `SEMA` ile reddedilir.
- **A7-K03.** Tüketici `.glb` dosyalarını yalnız manifestin bulunduğu dizinden, `dosya` alanındaki adla açar; alt dizin, `..` ya da mutlak yol kullanılmaz (şema deseni bunu da sınar).
- **A7-K04.** Manifest UTF-8'dir, bayt sırası işareti ve CR (0x0D) baytı içermez; içerirse tüketici `MANIFEST_BICIM` ile reddeder. Manifest şemaya uymazsa `SEMA` ile reddedilir.
- **A7-K05.** Manifest özeti, `manifest.json` dosyasının ham baytlarının SHA-256'sıdır (FIPS 180-4); metin gösterimi 64 haneli küçük harf onaltılıktır. Dosya hiçbir biçimde normalleştirilmeden özetlenir. (Satır sonu çevirisi özeti değiştireceği için A7-K04 CR'yi yasaklar.)
- **A7-K06.** Her `.glb`'nin özeti ham baytlarının SHA-256'sı, boyutu bayt sayısıdır; tüketici dosyayı ayrıştırmadan önce ikisini manifestle karşılaştırır, uyuşmazsa `OZET_UYUSMAZ` ile reddeder.
- **A7-K07.** Kümedeki herhangi bir dosya ya da manifest alanı değişince `kume_surumu` artırılır (manifest üretme aracı önceki manifestle karşılaştırarak zorlar; depo denetimi bir dalda değişen kümede sürümün artmadığını reddeder).
- **A7-K08.** Türetilmiş çıktılar yalnız derleme dizini altında, §3.1'deki yolda üretilir; `assets/` ya da depoda izlenen başka bir yola yazılmaz (iskelet §3 A7 satırı).

### 4.2 glTF kuralları

- **A7-K10.** Her varlık dosyası GLB sürüm 2'dir: başlık `magic = 0x46546C67` ("glTF"), `version = 2`, ilk parça JSON (`0x4E4F534A`), varsa ikinci parça BIN (`0x004E4942`) (glTF 2.0 belirtimi "Binary glTF Layout"); `asset.version = "2.0"`. Dışarıya başvuran URI kullanılmaz: bütün tamponlar ve görüntüler GLB'nin içindedir.
- **A7-K11.** `extensionsRequired` yoktur ya da boştur. `extensionsUsed` yalnız `KHR_lights_punctual`, `KHR_materials_emissive_strength` ve `KHR_texture_transform` içerebilir. Aksi `UZANTI` ile reddedilir. (Belirtim: gereken uzantılar `extensionsRequired`'da listelenir ve bu liste `extensionsUsed`'ın alt kümesidir.) IDA'ya özgü veri glTF dosyasında değil manifesttedir; dosyada özel uzantı ya da `extras` kullanılmaz ve tüketiciler `extras` okumaz.
- **A7-K12.** Bütün ağ temel öğeleri `mode = 4` (üçgen listesi; şema varsayılanı 4) kullanır, `POSITION` özniteliği float32 VEC3'tür. Seyrek erişimci (sparse accessor), biçim hedefi (morph target), deri (skin), animasyon ve kamera nesnesi bulunmaz. Aksi `GLTF` ile reddedilir.
- **A7-K13.** Dosyada tam bir sahne (`scenes` uzunluğu 1, `scene = 0`) vardır. Her düğümün ölçeği yoktur ya da her bileşeni 1'dir (|s − 1| ≤ 1e-6); `matrix` kullanan düğümde matris birim ölçekli bir dönüş ve ötelemedir (dikgenlik ve determinant +1, 1e-6 içinde). Aksi `BIRIM` ile reddedilir (ölçek uygulanmış olmalıdır; 12.16).
- **A7-K14.** **Birim ve eksenler.** glTF uzunluğu metre, açısı radyandır; glTF sağ elli, +Y yukarı, varlığın önü +Z, sol yanı +X'tir (glTF 2.0 belirtimi §3.4). glTF'ten gövde çerçevesine dönüş sabittir:

  ```
  M = [[0, 0, 1], [−1, 0, 0], [0, −1, 0]]     x_b = +Z_gltf,  y_b = −X_gltf,  z_b = −Y_gltf,   det M = +1
  ```

  Godot içe aktarılan varlıkların önünü +Z kabul eder (Godot 4.7.2 `Vector3.MODEL_FRONT = (0, 0, 1)`) ve glTF koordinatlarını eksen dönüşümü yapmadan yükler; gövdeden Godot yereline dönüş A2 §2'deki `C` olduğundan glTF'ten Godot yereline dönüş `C · M = diag(−1, 1, −1)`'dir (Y ekseni etrafında 180°). Manifestin `eksenler` alanı bu iki matrisi aynen taşır; farklıysa `EKSEN` ile reddedilir.

### 4.3 Düğüm tablosu ve roller

- **A7-K19.** **Erişilebilirlik.** Bütün rol ve bağlantı noktası düğümleri seçili sahnenin (`scenes[0]`) kök düğümlerinden `children` bağlarıyla erişilebilirdir; ağ taşıyan ama `scenes[0]`'dan erişilemeyen düğüm de bulunmaz. glTF düğüm hiyerarşisi ayrık katı ağaçlardan oluşur, döngü içermez ve her düğümün en çok bir ebeveyni vardır (glTF 2.0 belirtimi, "Nodes and Hierarchy"); doğrulayıcı bunu da denetler. Bir düğümün sahne kökündeki dönüşümü, kökten düğüme giden tek yol boyunca yerel dönüşümlerin çarpımıdır. Aksi `ERISILEMEZ`.
- **A7-K20.** Her varlıkta tam bir `REF` düğümü vardır: sahnenin kök düğümlerinden biridir, ağı yoktur, dönüşü birim, ölçeği 1'dir; ötelemesi gövde referans noktası `O_b`'nin glTF konumudur. Bir glTF noktasının gövde çerçevesindeki konumu `p_b = M · (p_gltf − t_REF)`'tir; `p_gltf` noktanın sahne kökündeki konumudur. Aksi `DUGUM`.
- **A7-K21.** Manifestin rol listelerindeki her `{dugum, ad}` girdisi için dosyadaki `nodes[dugum]` vardır ve `name` alanı `ad`'a eşittir; görsel, yüzerlik, çarpışma, akustik ve isabet rollerindeki düğümler bir `mesh` taşır; referans ve bağlantı noktası düğümleri ağ taşımaz. Ağ taşıyan ama hiçbir rolde listelenmeyen düğüm bulunmaz. Bir düğüm en çok bir rolde listelenir. Ad önekleri rolle uyumludur: `REF`, `BUOY_`, `COL_`, `SON_`, `MNT_`, `THR_`, `TUBE_`, `HIT_`; görsel düğümlerin adı bu öneklerle başlamaz. Aksi `DUGUM`.
- **A7-K22.** Tüketiciler düğümleri manifestteki indeksle bulur, ada göre arama yapmaz. Görselleştirici glTF düğüm indeksinden üretilen Godot düğümüne `GLTFState.get_scene_node(indeks)` ile ulaşır (Godot 4.7.2 `GLTFState` sınıf belgesi: "glTF düğümüyle aynı indekse karşılık gelen Godot sahne düğümünü döndürür"; aynı belgeye göre her glTF düğümü için sahne düğümü üretilmeyebilir ve o zaman `null` döner). Manifestte rolü olan herhangi bir düğüm için `get_scene_node` `null` dönerse görselleştirici varlığı kurmaz, sahneyi kurmaz ve A2 `hazir.durum = varlik_kurali` ile bildirir (hata kodu `ERISILEMEZ`); bir rol düğümünü atlayarak ya da adla arayarak devam etmez.
- **A7-K23.** Tüketici görsel ve çarpışma ağlarının bütün köşelerini gövde çerçevesine çevirip eksen hizalı sınır kutusunu hesaplar; her bileşen manifestteki `sinir_kutusu_govde_m` değerinden en çok 1 mm (mimari seçimi) farklıdır. Aksi `BIRIM` (santimetre ya da milimetre ile dışa aktarılmış bir varlık burada yakalanır).
- **A7-K24.** **Türe göre zorunlu roller.** `arac`: `REF`, ≥ 1 görsel, ≥ 1 `her_zaman` koşullu yüzerlik, ≥ 1 çarpışma; aracın A5'te tanımlı her tüpü için o tüpe koşullu en az bir yüzerlik düğümü ve bir `TUBE_<k>` bağlantı noktası. `torpido`: `REF`, ≥ 1 görsel, ≥ 1 çarpışma (isabet taraması için, 04 §2.8); yüzerlik ağı kullanılmaz (torpidonun hidrostatiği 04'teki sferoid modelindedir). `hedef`: `REF`, ≥ 1 görsel, ≥ 1 çarpışma; isabet bölgesi isteğe bağlıdır. `statik`: `REF`, ≥ 1 çarpışma; görsel en az 1 (şema). Koşullu (`tup_k_dolu`) düğümler yalnız `arac` türünde bulunur. Aksi `TUR`.
- **A7-K25.** **Yüzerlik ağı.** Her `BUOY_*` ağı kapalı ve manifolddur: her kenar tam iki üçgen tarafından, karşıt yönlerde paylaşılır; yönelim dışa doğrudur (gövde çerçevesinde işaretli hacim > 0). Tüketici hacmi, hacim merkezini ve en uzun kenarı ağdan yeniden hesaplar; manifest değerleriyle fark hacimde bağıl 1e-6, merkezde 1e-6 m, kenarda 1e-6 m'yi aşamaz. Aksi `KAPALI_DEGIL` ya da `HACIM`.
- **A7-K26.** **Çarpışma ağı.** Her `COL_*` kaynak ağı kapalı ve manifolddur (A7-K25'teki tanımla). `ayristirma = yok` ise ağ dışbükeydir: her üçgenin düzlemine göre bütün köşeler en çok 1e-6 m önde kalır; tüketici ağı tek bir dışbükey şekil olarak kullanır. `ayristirma = coacd` ise tüketici kaynak ağı değil türetilmiş parçaları kullanır (A7-K40–K46). Aksi `KAPALI_DEGIL` ya da `DISBUKEY_DEGIL`.
- **A7-K27.** **Akustik ağ.** Varlığın `akustik[]` listesi boş değilse sonar yalnız bu düğümlerin ağlarını kullanır; boşsa sonar `carpisma[]` düğümlerinin **kaynak** ağlarını (dışbükey parçaları değil) birebir kullanır. Bu türetme bir araç gerektirmez ve dosya üretmez.
- **A7-K28.** **Malzeme ataması.** Görsel, akustik ve çarpışma kaynak ağlarının her temel öğesi bir glTF `material` indeksi taşır. Kullanılan her glTF malzeme indeksi `malzeme_atamasi`'nda tam bir kez geçer ve atadığı kimlik `malzemeler[]` içinde vardır. Üçgenin malzemesi, ait olduğu temel öğenin malzemesidir. Aksi `MALZEME`; bilinmeyen malzemeye varsayılan verilmez (05 §4).
- **A7-K29.** **Bağlantı noktaları.** Tüketici her bağlantı noktasının gövde çerçevesindeki konumunu ve dönüşünü düğümden yeniden hesaplar; manifestle fark konumda 1e-6 m, açıda 1e-6 rad'ı aşamaz (aksi `DUGUM`). Montaj, itici ve tüp konumlarının fiziksel yetkili kaynağı A5'tir (iskelet §3.1); A7'deki nokta A5'teki karşılığından konumda 1 mm'den, yönde 1 mrad'dan (mimari seçimi) fazla saparsa sim çalıştırmayı `A5_TUTARSIZ` ile reddeder. A7'de karşılığı olmayan A5 noktası hata değildir.

### 4.4 Malzeme değerleri ve geçersiz kılma

- **A7-K30.** Manifestteki akustik değerler ve glTF malzemelerinin optik değerleri **varsayılandır**. Senaryo bunları A5 sözleşmesindeki geçersiz kılma kuralıyla, malzeme kimliğine göre değiştirir (iskelet §3 A7 satırı; sözdizimi ve öncelik A5'te). A7'nin bu mekanizmaya koyduğu koşullar: geçersiz kılma yalnız manifestte bulunan bir malzeme kimliğine ve yalnız §3.4'teki alanlara (optikte taban rengi, metaliklik, pürüzlülük) yapılır; birim ve geçerli aralık §3.4'tekiyle aynıdır; geçersiz kılma manifest özetini değiştirmez; etkin değerler ve geçersiz kılmalar A6'ya yazılır; görselleştiriciye etkin optik değerler A2 `malzeme_optik` iletisiyle gider.

### 4.5 Türetilmiş ağların üretimi

- **A7-K40.** Bu sürümde türetilen tek çıktı `ayristirma = coacd` olan çarpışma düğümlerinin dışbükey parçalarıdır. Araç CoACD'dir (MIT lisanslı; bu yazımda son sürüm 1.0.14, 28 Ağustos 2026, GitHub sürüm listesi). Kullanılan sürüm ve git işleme özeti manifestin `araclar.coacd` alanındadır (1.0.14 etiketinin işlemesi `1401ce2a7ae1ed89c65ab958b48d489350c233c7`, GitHub API); türetme adımı bağlı olduğu CoACD'nin sürüm ve işleme özeti manifesttekinden farklıysa `ARAC` ile durur.
- **A7-K41.** **Ayarların tamamı.** CoACD 1.0.14'ün algoritma ayarları `Params` yapısındaki 19 alandır (`src/config.h`, 1.0.14 etiketi): `threshold`, `real_metric`, `apx_mode`, `preprocess_mode`, `prep_resolution`, `resolution`, `mcts_nodes`, `mcts_iteration`, `mcts_max_depth`, `merge`, `max_convex_hull`, `decimate`, `max_ch_vertex`, `extrude`, `extrude_margin`, `pca`, `seed`, `rv_k`, `dmc_thres`. Manifestin `coacd` nesnesi bunların hepsini taşır: sırasıyla `esik`, `gercek_metrik`, `yaklasim`, `on_isleme`, `on_isleme_cozunurlugu`, `ornekleme_cozunurlugu`, `mcts_dugum`, `mcts_yineleme`, `mcts_derinlik`, `birlestirme`, `azami_parca`, `kose_siniri`, `azami_parca_kosesi`, `disari_uzatma`, `disari_uzatma_payi`, `pca`, `tohum`, `rv_k`, `dmc_esigi`. Türetme adımı CoACD'yi komut satırı aracıyla değil **kitaplık API'siyle** (`public/coacd.h`, `CoACD(...)` ya da `CoACD_run(...)`) çağırır ve API'nin aldığı 17 ayarın hepsini açıkça verir; hiçbir API parametresi varsayılan değerine bırakılmaz. API'nin açmadığı iki ayar (`rv_k` ve `dmc_thres`) 1.0.14'te `Params` kurucusunun değerleriyle sabittir (0,3 ve 0,55; `src/config.h`); şema bu iki alanı bu değerlere sabitler ve CoACD sürümü değiştiğinde (A7-K40) bu iki değer kaynaktan yeniden denetlenir. Komut satırı aracı kullanılmaz: 1.0.14'te `-dt` seçeneği hem `max_ch_vertex`'e hem `dmc_thres`'e bağlıdır (`main.cpp`), bu yüzden komut satırından iki ayar birbirinden bağımsız verilemez. `tohum` sabittir ve açıkça verilir (CoACD README: algoritma rastgeledir; aynı sonucun yeniden üretimi için tohum verilir; kitaplık API'sinin varsayılan tohumu 0, komut satırınınki o anki zamandır).
- **A7-K42.** **Kanonik geometri özeti.** Bir çarpışma düğümünün türetilmiş parçaları için özet, şu bayt dizisinin SHA-256'sıdır: ASCII `IDA7GEO1` (8 B); parça sayısı (u32); her parça için, üretim sırasıyla, köşe sayısı (u32), köşeler (her biri x, y, z; IEEE 754 ikili32, küçük sonlu, kaynak düğümün yerel glTF koordinatlarında), üçgen sayısı (u32), üçgen indeksleri (her biri üç u32, küçük sonlu). Dosya biçimi, sıkıştırma ya da GLB yazıcısının JSON düzeni özeti etkilemez.
- **A7-K43.** Manifest her `coacd` düğümü için platform anahtarına göre beklenen kanonik özeti (`turetilmis_ozet`) taşır. Türetme adımı parçaları üretir, kanonik özeti hesaplar ve çalıştığı platformun anahtarıyla karşılaştırır; özet farklıysa ya da o platformun anahtarı manifestte yoksa derleme `TURETILMIS_OZET` ile başarısız olur. Manifest üretme aracı yeni özeti ancak açık bir "yeniden özetle" komutuyla yazar ve bu değişiklik A7-K07 gereği küme sürümünü artırır.
- **A7-K44.** **Örtü denetimi (yüzey örneklemeli, iki yönlü).** Her `coacd` çarpışma düğümü için manifest açık bir `ortu_toleransi_m` (`τ`, varlık ve düğüm başına; 0 < τ ≤ 1 m) taşır. Dışbükey ayrıştırmanın kaynağın içbükeyliğini bilerek doldurduğu düğümlerde `τ` bu doldurmayı kapsayacak büyüklükte seçilir ve gerekçesi manifest üretme aracının çıktısına yazılır; varsayılan değer yoktur. Türetme adımı şu denetimi yapar (örnek aralığı `δ = τ / 5`; mimari seçimi):
  1. **Örnekleme.** Kaynak yüzey `S` (düğümün bütün üçgenleri) ve parça yüzeyleri ayrı ayrı örneklenir: her üçgen, kenarları `n = ⌈L_maks / δ⌉` eşit parçaya bölünerek `n²` alt üçgene ayrılır (`L_maks` üçgenin en uzun kenarı) ve alt üçgenlerin bütün köşeleri örnek olur. Böylece örnek yoğunluğu alanla orantılıdır, alt üçgenlerin her kenarı ≤ `δ`'dir ve yüzeyin her noktası en yakın örneğe en çok `δ` uzaklıktadır.
  2. **Birleşimin sınırı.** Bir parça yüzeyi örneği, başka bir parçanın içinde (o parçanın bütün yüz düzlemlerinin en az 1e-9 m gerisinde) kalıyorsa birleşimin sınırında değildir ve atılır; kalan örnekler `B_D` kümesidir. `S`'nin örnekleri `B_S` kümesidir.
  3. **Ölçüt.** `B_S`'deki her örneğin `B_D`'deki en yakın örneğe uzaklığı ve `B_D`'deki her örneğin `B_S`'deki en yakın örneğe uzaklığı en çok `τ − δ`'dir.
  Bu ölçüt sağlanırsa kaynak yüzey ile parçaların birleşiminin sınırı arasındaki iki yönlü Hausdorff uzaklığı en çok `τ`'dur (yüzeyin her noktası bir örneğe ≤ `δ` uzaklıkta olduğu için üçgen eşitsizliğinden). Ölçüt sağlanmazsa derleme `ORTU` ile başarısız olur ve en büyük uzaklık ile yeri raporlanır. Yalnız köşelerde ölçülen uzaklık bu kuralı karşılamaz: delikli bir cismin deliğini dolduran bir dışbükey kutu köşe denetimini 0 m hatayla geçebilir (sol incelemesinin karşı örneği; A7-U12).
- **A7-K45.** Sim çalışma zamanında türetilmiş dosyayı §3.1'deki yoldan okur, her düğümün kanonik özetini yeniden hesaplar ve manifestteki kendi platform anahtarının değeriyle karşılaştırır; dosya yoksa `TURETILMIS_EKSIK`, özet farklıysa `TURETILMIS_OZET` ile çalıştırmayı başlatmaz.
- **A7-K46.** Çalışma zamanında hiçbir tüketici CoACD ya da başka bir türetme aracı çalıştırmaz.
- **A7-K47.** Kaynak `.glb` dosyalarının üretimi (Blender'dan dışa aktarma dahil) derlemenin parçası değildir; derleme ve otomatik test yalnız depodaki `.glb` ve `manifest.json`'u, iskelet §7'nin izin verdiği araçlarla (C++ ve derleme zamanında Python) kullanır. Blender bir derleme bağımlılığı değildir (AB-103).

### 4.6 Çalışma zamanında doğrulama

- **A7-K50.** Sim, çalıştırma başında A5'in gösterdiği kümenin manifestini **bir kez** okur, A7-K04–K06, K10–K14, K19–K29, K45 denetimlerini yapar ve manifest özetini hesaplar. Fizik (yüzerlik ve çarpışma), sonar (akustik ağlar ve malzemeler) ve hedef/torpido şekilleri bu tek yüklemeden kurulur; hiçbir modül kümeyi kendi başına yeniden okumaz. Her modül kurulduğu manifest özetini kaydeder; çalıştırma başlamadan önce sim bu özetlerin hepsinin aynı olduğunu denetler.
- **A7-K51.** Sim'in herhangi bir A7 denetimi başarısızsa çalıştırma başlamaz; sim hata kodunu, kümeyi, varlığı ve düğümü A3'e bildirir. Sonar bu durumda etkinleşmez (05 §4) ve hiçbir tüketici yarım yüklenmiş kümeyle çalışmaz.
- **A7-K52.** Sim, A5'teki kütle durumlarının deplasmanını ve yüzerlik merkezini manifestteki yüzerlik ağlarıyla karşılaştırır: her kütle durumu için `her_zaman` ağları ile o durumda dolu olan tüplerin `tup_k_dolu` ağlarının toplam hacmi ve hacimle ağırlıklı merkezi, A5 değerinden hacimde %1'den, merkezde 2 mm'den (04 §4; mimari seçimi) fazla saparsa çalıştırma `A5_TUTARSIZ` ile reddedilir.
- **A7-K53.** Görselleştirici A2 el sıkışmasında aldığı manifest yolundaki dosyayı kendisi okur, özetini A2 `hazir` iletisiyle bildirir (A2-K13, A2-K14), her `.glb`'yi A7-K06 ile doğrular ve A7-K04, K10–K14, K19–K23, K28 denetimlerini yapar. Sim ile görselleştirici aynı manifest özetini bildirmedikçe görselleştirici sahne kurmaz ve kamera kullanılmaz.
- **A7-K54.** Görselleştirici yalnız görsel düğümleri ve glTF ışıklarını operatör ve kamera görünümünde çizer; yüzerlik, çarpışma, akustik ve isabet düğümlerini yalnız hata ayıklama katmanında ve kamera karesine girmeyecek biçimde (A2-K37) gösterir.
- **A7-K55.** A6 kaydı kullanılan kümenin manifest özetini, `kume_kimligi` ve `kume_surumu`yla, her türetilmiş düğümün kullanılan kanonik özetini ve etkin malzeme değerlerini taşır (A6'dan beklenen).

---

## 5. Hata durumları

| Durum | Algılama koşulu | Sim | Görselleştirici | Türetme adımı (derleme) |
|---|---|---|---|---|
| Şema ihlali | şemaya uymayan manifest | çalıştırma başlamaz (`SEMA`) | `hazir.durum = manifest_gecersiz` | durur |
| CR, BOM, UTF-8 hatası | A7-K04 | `MANIFEST_BICIM` | `manifest_gecersiz` | durur |
| Birim, ölçek | A7-K13, K23 | `BIRIM` | `varlik_kurali` | durur |
| Eksen kabulü | A7-K14 | `EKSEN` | `manifest_gecersiz` | durur |
| Eksik varlık | dosya yok | `EKSIK_VARLIK` | `varlik_eksik` | durur |
| Özet ya da boyut uyuşmazlığı | A7-K06 | `OZET_UYUSMAZ` | `varlik_ozeti_uyusmaz` | durur |
| glTF kuralı, izinsiz uzantı | A7-K10–K12 | `GLTF` / `UZANTI` | `varlik_kurali` | durur |
| Düğüm tablosu | A7-K20–K22, K29 | `DUGUM` | `varlik_kurali` | durur |
| Rol düğümü erişilemiyor | A7-K19, K22 | `ERISILEMEZ` | `varlik_kurali` (`get_scene_node` `null` dahil) | durur |
| Malzeme | A7-K28 | `MALZEME` | `varlik_kurali` | durur |
| Kapalı değil, hacim, dışbükey değil | A7-K25, K26 | `KAPALI_DEGIL` / `HACIM` / `DISBUKEY_DEGIL` | denetlemez (fizik kullanmaz) | durur |
| Türe göre rol eksik | A7-K24 | `TUR` | `varlik_kurali` | durur |
| Türetme aracı sürümü | A7-K40 | — | — | `ARAC` |
| Türetilmiş özet yok ya da farklı | A7-K43, K45 | `TURETILMIS_EKSIK` / `TURETILMIS_OZET` | — | `TURETILMIS_OZET` |
| Örtü eşiği aşıldı | A7-K44 | — | — | `ORTU` |
| A5 ile tutarsızlık | A7-K29, K52 | `A5_TUTARSIZ` | — | — |
| Sim ile görselleştirici farklı manifest | A2-K14 | `ret(manifest)`; kamera yok | sahne kurulmaz | — |

Bütün hata iletileri kod, küme kimliği, varlık kimliği, düğüm indeksi ve adı ile beklenen/bulunan değeri içerir.

---

## 6. Kalite kademelerinin etkisi

A7'nin içeriği kademeden bağımsızdır; kademeler yalnız tüketicinin neyi kullandığını değiştirir:

| Kademe (iskelet §6) | Etkisi |
|---|---|
| 3B görünüm kapalı | Görselleştirici varlık yüklemez; sim A7 denetimlerinin hepsini yine yapar (fizik ve sonar her kademede tam çalışır) |
| 3B görünüm sade / tam | Görsel düğümler aynıdır; dalga ve su altı optiği A7'nin konusu değildir |
| Sonar sade / tam | İkisi de aynı akustik ağları ve malzemeleri kullanır; sade kademe hücre fazı kullanmaz (05 §2.4.3), malzeme alanları değişmez |

---

## 7. Uygunluk

Tür: **B** birim, **D** derleme/depo denetimi, **S** sözleşme, **SIL** kartsız bütünleşik.

| Test | Sınadığı kurallar | Tür | Yöntem ve gözlenecek büyüklük |
|---|---|---|---|
| A7-U01 | şema, A7-K04 | D | Depodaki her `manifest.json` şemaya uyar; CR ve BOM yok. Bu iş sırasında `assets/ornek/manifest.json` standart kitaplıkla yazılmış bir yapısal denetleyiciyle doğrulandı ve 14 bilerek bozulmuş türev ile CoACD ayarları ve örtü toleransına ilişkin 5 bozuk türev reddedildi; CoACD ayrıştırmalı geçerli bir türev geçti (rapor) |
| A7-U02 | A7-K05, K06 | B | Bilinen bayt dizilerinin SHA-256'sı FIPS 180-4 test vektörleriyle; tek bayt değişen `.glb` reddedilir |
| A7-U03 | A7-K10–K13 | B | Elle hazırlanmış bozuk GLB'ler: yanlış sihirli sayı, sürüm 1, dış URI, `extensionsRequired` dolu, izinsiz uzantı, `mode = 1`, sparse, morph, skin, animasyon, ölçek 0,01, yansıtıcı matris: her biri beklenen kodla reddedilir |
| A7-U04 | A7-K14, K20 | B | Önü +Z'ye bakan bilinen bir kutu: gövde çerçevesinde x ileri, y sancak, z aşağı; `det M = +1`; `C · M = diag(−1, 1, −1)` |
| A7-U05 | A7-K19, K21–K24 | B | Ad–indeks uyuşmazlığı, ağsız görsel düğüm, listelenmemiş ağlı düğüm, iki rolde düğüm, yanlış önek, türe göre eksik rol: her biri reddedilir; `scenes[0]`'dan erişilemeyen rol düğümü ve iki ebeveynli düğüm `ERISILEMEZ`; Godot'da `get_scene_node` `null` dönen rol düğümü varlığı kurdurmaz |
| A7-U06 | A7-K14, K22, K29 | SIL (Godot) | İşaret düğümü (`MNT_kamera_on`) Godot'daki dünya konumu ile sim'in hesapladığı konum arasında ≤ 1 mm (07 §2.8; A2-U12 ile birlikte) |
| A7-U07 | A7-K23 | B | Santimetreyle dışa aktarılmış aynı varlık `BIRIM` ile reddedilir |
| A7-U08 | A7-K25 | B | Analitik hacmi bilinen kutu ve silindir: hacim, merkez, en uzun kenar manifestle 1e-6 içinde; tek üçgeni ters çevrilmiş ve deliği olan ağ `KAPALI_DEGIL` |
| A7-U09 | A7-K26, K27 | B | Dışbükey olmayan `yok` ağı `DISBUKEY_DEGIL`; akustik liste boşken sonarın kullandığı üçgen sayısı çarpışma kaynak ağınınkine eşit |
| A7-U10 | A7-K28 | B | Atanmamış glTF malzemesi ve tabloda olmayan kimlik `MALZEME` |
| A7-U11 | A7-K40–K43 | D | Aynı girdi ve aynı tohumla ardışık iki türetme, aynı makinede, 1 ve N iş parçacığıyla (CoACD 1.0.14 sürüm notu: aynı girdi ve tohumla sonuç iş parçacığı sayısından bağımsızdır; doğrulanacak) aynı kanonik özeti verir. Tohumun araca iletildiği ayrıca sınanır: türetme adımının CoACD'ye verdiği parametreler kaydedilir (ya da API çağrısı bir sınama kancasıyla yakalanır) ve `seed` değeri manifestteki `tohum`'a eşittir. Farklı tohumun farklı geometri vermesi şart değildir ve sınanmaz. CoACD sürümü farklı bağlanınca `ARAC`; platform anahtarı yoksa `TURETILMIS_OZET` |
| A7-U12 | A7-K44 | D | **Karşı örnek (sol):** kaynak dış kutu `[−2, 2]² × [−1, 1]` m, içinden geçen delik `(−1, 1)² × [−1, 1]`; "parça" olarak deliği dolduran tek dışbükey kutu `[−2, 2]² × [−1, 1]` verilir. Köşe denetimi 0 m hata verir; A7-K44 denetimi kaynak iç duvarındaki `(1, 0, 0)` örneğinin parça sınırına uzaklığını ≈ 1 m bulur ve `τ = 0,01 m` ile `ORTU` verir; ölçüt `τ − δ = 0,8 τ ≥ 1 m` gerektirdiği için (hesap) şemanın izin verdiği her `τ ≤ 1 m` ile de `ORTU` verir. Ayrıca kaynağı kenarı 1 m olan bir küp, parçası onunla eş merkezli ve kenarı 5 mm uzun bir küp olan durum (yüzeyler arası uzaklık 2,5 mm, köşeler arası ≈ 4,33 mm; hesap) `τ = 0,01 m` ile geçer, `τ = 0,004 m` ile kalır; örnek aralığının `δ = τ/5` olduğu ve her alt üçgen kenarının ≤ `δ` olduğu denetlenir |
| A7-U13 | A7-K45, K46, K50, K51 | SIL | Türetilmiş dosya silinir ya da bir baytı değiştirilir: sim çalıştırmayı başlatmaz; fizik, sonar ve görselleştirici tek bir manifest özeti bildirir |
| A7-U14 | A7-K52 | SIL | A5'te ∇ %2 değiştirilince `A5_TUTARSIZ`; üç kütle durumunda hacim ve merkez karşılaştırması |
| A7-U15 | A7-K30 | SIL | A5 akustik geçersiz kılması sonar profilini değiştirir, manifest özetini değiştirmez, A6'ya yazılır; bilinmeyen kimliğe geçersiz kılma A5 doğrulamasında reddedilir |
| A7-U16 | A7-K53 | S | Görselleştiriciye farklı bir manifest verilince A2 `ret(manifest)` ve sahne kurulmaz |
| A7-U17 | A7-K07, K08 | D | Değişen kümede `kume_surumu` artmadıysa depo denetimi reddeder; `assets/` altında türetilmiş dosya yok |

---

## 8. Açık kalanlar

| # | Konu | Etiket | Neyle kapanacak |
|---|---|---|---|
| A7-A01 | CoACD 1.0.14 çıktısının aynı girdi ve tohumla aynı makinede ve iş parçacığı sayısından bağımsız olarak aynı olduğu (1.0.14 sürüm notu bunu söylüyor: birleştirme aşamasının örneklemesi aday çift başına tohumlanıyor ve sonuçlar sabit sırayla toplanıyor; bu depoda sınanmadı) ve macOS arm64 ile Windows x64 arasında aynı olup olmadığı | ölçülecek | Varlık hattı uygulama işi (A7-U11; iki platform). Aynı makinede belirlenimci değilse mimar kararı gerekir. Platformlar arasında farklıysa iki platform anahtarı ayrı özet taşır (A7-K43 bunu zaten öngörür) |
| A7-A02 | Örtü toleransı `τ` için varlık başına seçilecek değerler (ilk örnek 0,01 m) ve örnek aralığı `δ = τ/5`, sınır kutusu toleransı 1 mm, A5 nokta toleransı 1 mm / 1 mrad, varlık boyutu üst sınırı 20 MiB | tahmin | İlk gerçek İDA-0 varlığının hattan geçirilmesi; çarpışma doğruluğu 04'ün temas testleriyle (04 §7 md.13) |
| A7-A03 | Gerçek İDA-0, torpido, hedef ve havuz varlıkları | açık | Varlık hattı uygulama işi; o zamana kadar 04 §2.6'daki yer tutucu kutular kullanılır |
| A7-A04 | Godot'nun glTF düğüm adlarını değiştirmeden koruduğu ve `get_scene_node` eşlemesinin GDExtension'dan kullanılabildiği | doğrulanmadı (yalnız sınıf belgesi) | Godot sahnesi uygulama işi (A7-U06) |
| A7-A05 | 750 kHz akustik malzeme değerleri | tahmin | Açık veri kümesiyle uydurma (05 §7; M14'ün daraltılmış kapsamı); değerler A5 geçersiz kılmasıyla girer |
| A7-A06 | Büyük ikili dosyaların depoda tutulma biçimi (doğrudan git ya da Git LFS) | açık | W2 depo iskeleti; bu sözleşme yalnız boyut üst sınırını koyar |

**A5'ten beklenen:** kullanılan kümenin yolu (`assets/` altındaki `kume_kimligi`) ve beklenen `kume_surumu`; araç, torpido, hedef ve statik sahne için varlık kimlikleri; kütle durumlarının ∇ ve `r_b` değerleri (A7-K52); sensör montajı, itici ve tüp konumları (A7-K29); malzeme geçersiz kılmalarının sözdizimi ve A7-K30'daki koşulların doğrulanması.

**A6'dan beklenen:** A7-K55'teki alanlar.

**W2 depo iskeletinden beklenen (uygulama):** `.gitattributes` içinde `assets/**` için satır sonu çevirisinin kapatılması (`-text`) — aksi halde Windows'ta `core.autocrlf` manifestin baytlarını değiştirir ve A7-K04/K05 ihlal edilir; derleme dizininin `.gitignore`'da olması; A7-K01, K07, K08 depo denetimleri; türetme adımının CMake'e bağlanması.

---

## 9. Açık bulgu kapanış tablosu

### 9.1 `acik-bulgular.md` "A7" başlığı

| Bulgu | Durum | Sözleşmede | Gerekçe |
|---|---|---|---|
| AB-102 | kapandı | A7-K08, K40–K46, §3.5 | Türetilmiş ağlar depoya girmez (iskelet A7); manifest her türetilmiş düğümün platform başına kanonik özetini taşır; özet tutmazsa derleme durur ve çalışma zamanı reddeder; platformlar arası farklılık ayrı anahtarla karşılanır, belirlenimcilik ölçümü A7-A01'de |
| AB-103 | kapandı | A7-K47 | Blender dışa aktarması derlemenin parçası değildir; derleme yalnız depodaki `.glb` ve manifesti iskelet §7'nin izin verdiği araçlarla kullanır, bu yüzden A7 hattı A16'nın yanıtına bağlı değildir (iskelet §7 ve §10 md.25 derleme zamanında Python'a zaten izin verir) |

### 9.2 Uyumsuzluk tablosunda A7'ye değinen satır

| Satır | Durum | Sözleşmede | Gerekçe |
|---|---|---|---|
| U-20 | kapandı | §3.4, A7-K30 | Akustik varsayılanlar A7 manifestinde, senaryoya özgü değerler A5'te aynı kimlikle; kuralın A7 tarafı burada, sözdizimi A5'te |
