# A7 — Sahne ve varlıklar sözleşmesi

| | |
|---|---|
| Sürüm | 1.0-aday (2026-11-19) |
| Görev | `docs/gorevler/W1-A2A7.md`; ortak kurallar `docs/gorevler/W1-ortak.md` |
| Makinece okunur tanım | `assets/manifest.schema.json` (JSON Schema 2020-12); uygunluk örneği `assets/ornek/manifest.json` (A5 örneklerinin atıf verdiği dört varlık; ikili `.glb` dosyaları yoktur, §3.7) |
| İlgili sözleşmeler | A2 (`docs/sozlesmeler/A2-gorsellestirici.md`), A5 (senaryo; malzeme geçersiz kılma), A6 (kayıt) |
| Dayanak | İskelet 0.10 §3 (A7 satırı), §3.1, §5, §7; glTF 2.0 belirtimi (Khronos); 00 belgesi 12.16; bölüm 04 §2.2, §2.4, §2.9; 05 §2.4.1; 07 §2.8 (taslak; gerekçe için) |

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
| `taban_derinligi_m` | sayı | m | yalnız `statik`te, isteğe bağlı | **Yerel** uzunluk: statik varlığın kendi gövde çerçevesinde (A7-K14, K20), `REF` noktasından o çerçevenin +z ekseni boyunca taban üst yüzeyine (ilk kesişime) uzaklık. Dünya çerçevesindeki su derinliği **değildir**; dünyadaki taban derinliği ancak A5'teki statik poz uygulanarak bulunur | 0 < x ≤ 11 000; A7-K56; başka türde bulunursa `SEMA` |

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

`SEMA` şema ihlali · `BIRIM` birim ya da ölçek hatası · `EKSEN` eksen kabulü uyuşmuyor · `MANIFEST_BICIM` CR, BOM ya da geçersiz UTF-8 · `EKSIK_VARLIK` dosya yok · `OZET_UYUSMAZ` dosya özeti ya da boyutu tutmuyor · `GLTF` glTF/GLB kuralı ihlali · `UZANTI` izinsiz glTF uzantısı · `DUGUM` düğüm tablosu ihlali · `MALZEME` malzeme ataması eksik ya da bilinmeyen kimlik · `KAPALI_DEGIL` yüzerlik ağı kapalı ve manifold değil · `HACIM` hacim, merkez ya da kenar manifestle tutmuyor · `DISBUKEY_DEGIL` `yok` ayrıştırmalı çarpışma ağı dışbükey değil · `ARAC` türetme aracı sürümü uyuşmuyor · `TURETILMIS_EKSIK` türetilmiş dosya yok · `TURETILMIS_OZET` kanonik özet tutmuyor ya da platform için yok · `ORTU` türetilmiş parçalar kaynağı örtmüyor · `A5_TUTARSIZ` A5 değerleriyle tutarsızlık · `TUR` varlık türünün gerektirdiği rol eksik · `TABAN` beyan edilen taban derinliği geometriyle tutmuyor · `ERISILEMEZ` rol düğümü seçili sahneden erişilemiyor ya da Godot'da sahne düğümü üretilmedi.


### 3.7 Uygunluk örneği

`assets/ornek/manifest.json` bir **uygunluk örneğidir**: A5 örnek senaryolarının atıf verdiği bütün varlıkları içerir (araç `ida0`, torpido `torpido_ida0`, hedef `hedef_ornek`, statik havuz `havuz_ornek`; üç malzeme). **İkili dosyalar yoktur:** atıf verilen `.glb` dosyaları depoda bulunmaz; `sha256` ve `bayt` alanları yer tutucudur; bu küme çalışma zamanında yüklenemez (A7-K06, K51). Örnek şema doğrulaması, düğüm ve malzeme tablolarının iç tutarlılığı, A5 ile çapraz tutarlılık (A7-K29, K52, K56) ve kimlik eşlemesi için kullanılır (A7-U19). Geometri değerleri 04'ün yer tutucu ağlarındandır; akustik değerler tahmindir. Örneğin ham bayt özeti (A7-K05) bu sürümde `29063c2d7f89a80e452dfbb66504e9470d5c928d0fcf7dd295497e2b22a22ca4`'tür; A5 örneği beklenen küme özeti olarak bunu kullanabilir, dosya her değiştiğinde özet de değişir.

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
- **A7-K44.** **Örtü denetimi (katı cisme göre iki tek yönlü ölçüt).** Her `coacd` çarpışma düğümü için manifest açık bir `ortu_toleransi_m` (`τ`, varlık ve düğüm başına; 0 < τ ≤ 1 m) taşır. Dışbükey ayrıştırmanın kaynağın içbükeyliğini bilerek doldurduğu düğümlerde `τ` bu doldurmayı kapsayacak büyüklükte seçilir ve gerekçesi manifest üretme aracının çıktısına yazılır; varsayılan değer yoktur. Tanımlar: `S` kaynak düğümün üçgen yüzeyi; `K` `S`'nin sınırladığı kapalı katı (A7-K26 gereği `S` kapalı, manifold ve dışa doğru tutarlı yönlüdür; sağlamayan manifest `KAPALI_DEGIL` ile reddedilir); `P_i` türetilmiş dışbükey parçalar (her biri kapalı dışbükey çokyüzlü; A7-K26'daki dışbükeylik denetimi her parçaya uygulanır, sağlamayan parça `DISBUKEY_DEGIL`); `D = ∪ P_i` kapalı katı; `δ = τ / 5` örnek aralığı (mimari seçimi). Türetme adımı şu denetimi yapar:
  1. **Örnekleme (δ-örtü).** Her üçgen, kenarları `n = ⌈L_maks / δ⌉` eşit parçaya bölünerek `n²` alt üçgene ayrılır (`L_maks` üçgenin en uzun kenarı) ve alt üçgenlerin bütün köşeleri örnek olur. Alt üçgenlerin her kenarı ≤ `δ` olduğundan yüzeyin her noktası en yakın örneğe en çok `δ` uzaklıktadır ve örnek yoğunluğu alanla orantılıdır. `S`'nin bütün üçgenleri ve her `P_i`'nin **bütün** yüzleri örneklenir; hiçbir örnek atılmaz.
  2. **Ölçüt 1: eksik yok.** `S`'nin her örneği `s` için `d(s, D) ≤ τ − δ`. Burada `d(s, D) = min_i d(s, P_i)` ve `d(s, P_i)`, `s` `P_i`'nin içindeyse ya da üstündeyse (bütün yüz düzlemlerine göre işaretli uzaklık ≤ 1e-9 m) 0, değilse `s`'den `P_i`'nin üçgenlerine en kısa uzaklıktır.
  3. **Ölçüt 2: taşma yok.** Her parçanın yüzeyindeki her örnek `g` için `d(g, K) ≤ τ − δ`. Burada `d(g, K)`, `g` `K`'nın içindeyse ya da `S`'nin üstündeyse (`S`'ye uzaklığı ≤ 1e-9 m) 0, değilse `g`'den `S`'nin üçgenlerine en kısa uzaklıktır.
  4. **İçeride olma sınaması (tek yöntem): sarım sayısı.** `S`'ye uzaklığı 1e-9 m'den büyük bir `g` için sarım sayısı `w(g) = (1/4π) Σ Ω_t(g)` hesaplanır; `Ω_t`, `S`'nin `t` üçgeninin `g`'den görülen işaretli katı açısıdır (Van Oosterom–Strackee: `tan(Ω/2) = a·(b × c) / (|a||b||c| + (a·b)|c| + (a·c)|b| + (b·c)|a|)`, `a, b, c` köşelerin `g`'ye göre konumları, `atan2` ile). Kapalı ve dışa yönlü `S` için `w` içeride 1, dışarıda 0'dır; `g` içeride sayılır ⇔ `round(w) ≠ 0`. Işın atılmadığı için dejenere ışın (kenardan ya da köşeden geçen ışın) durumu yoktur; sınır üstündeki noktalar sarım sayısına gelmeden önce uzaklık eşiğiyle (≤ 1e-9 m) içeride sayılır.
  **Güvence.** `d(·, D)` ve `d(·, K)` 1-Lipschitz işlevlerdir. `S`'nin her noktası bir örneğe ≤ `δ` uzaklıkta olduğundan ölçüt 1, `S`'nin her noktasının `D`'ye en çok `τ` uzakta olduğunu; her `∂P_i`'nin her noktası bir örneğe ≤ `δ` uzaklıkta olduğundan ölçüt 2, her `∂P_i` noktasının `K`'ya en çok `τ` uzakta olduğunu garanti eder. Ölçüt katı cisimlere göre tanımlandığı için bitişik parçaların ortak iç yüzleri (`K`'nın içinde, uzaklık 0) hata üretmez ve örnek silmek gerekmez. **Sınamadığı şey:** ölçüt `D`'nin iç boşluklarını (parçaların arasında kalan, `S` içindeki kapalı boşlukları) sınamaz; dıştan çarpışma için bunlar önemsizdir ve yüzerlik ile kütle değerleri çarpışma geometrisinden değil A5'ten ve yüzerlik ağlarından gelir. Ölçüt sağlanmazsa derleme `ORTU` ile başarısız olur ve en büyük uzaklık, hangi ölçüt olduğu ve örneğin yeri raporlanır. Yalnız köşelerde ölçülen uzaklık bu kuralı karşılamaz (sol incelemesinin karşı örneği; A7-U12).
- **A7-K45.** Sim çalışma zamanında türetilmiş dosyayı §3.1'deki yoldan okur, her düğümün kanonik özetini yeniden hesaplar ve manifestteki kendi platform anahtarının değeriyle karşılaştırır; dosya yoksa `TURETILMIS_EKSIK`, özet farklıysa `TURETILMIS_OZET` ile çalıştırmayı başlatmaz.
- **A7-K46.** Çalışma zamanında hiçbir tüketici CoACD ya da başka bir türetme aracı çalıştırmaz.
- **A7-K47.** Kaynak `.glb` dosyalarının üretimi (Blender'dan dışa aktarma dahil) derlemenin parçası değildir; derleme ve otomatik test yalnız depodaki `.glb` ve `manifest.json`'u, iskelet §7'nin izin verdiği araçlarla (C++ ve derleme zamanında Python) kullanır. Blender bir derleme bağımlılığı değildir (AB-103).

### 4.6 Çalışma zamanında doğrulama

- **A7-K50.** Sim, çalıştırma başında A5'in gösterdiği kümenin manifestini **bir kez** okur, A7-K04–K06, K10–K14, K19–K29, K45, K56 denetimlerini yapar ve manifest özetini hesaplar. Fizik (yüzerlik ve çarpışma), sonar (akustik ağlar ve malzemeler) ve hedef/torpido şekilleri bu tek yüklemeden kurulur; hiçbir modül kümeyi kendi başına yeniden okumaz. Her modül kurulduğu manifest özetini kaydeder; çalıştırma başlamadan önce sim bu özetlerin hepsinin aynı olduğunu denetler.
- **A7-K51.** Sim'in herhangi bir A7 denetimi başarısızsa çalıştırma başlamaz; sim hata kodunu, kümeyi, varlığı ve düğümü A3'e bildirir. Sonar bu durumda etkinleşmez (05 §4) ve hiçbir tüketici yarım yüklenmiş kümeyle çalışmaz.
- **A7-K52.** Sim, A5'teki kütle durumlarının deplasmanını ve yüzerlik merkezini manifestteki yüzerlik ağlarıyla karşılaştırır: her kütle durumu için `her_zaman` ağları ile o durumda dolu olan tüplerin `tup_k_dolu` ağlarının toplam hacmi ve hacimle ağırlıklı merkezi, A5 değerinden hacimde %1'den, merkezde 2 mm'den (04 §4; mimari seçimi) fazla saparsa çalıştırma `A5_TUTARSIZ` ile reddedilir.
- **A7-K53.** Görselleştirici A2 el sıkışmasında aldığı manifest yolundaki dosyayı kendisi okur, özetini A2 `hazir` iletisiyle bildirir (A2-K13, A2-K14), her `.glb`'yi A7-K06 ile doğrular ve A7-K04, K10–K14, K19–K23, K28 denetimlerini yapar. Sim ile görselleştirici aynı manifest özetini bildirmedikçe görselleştirici sahne kurmaz ve kamera kullanılmaz.
- **A7-K54.** Görselleştirici yalnız görsel düğümleri ve glTF ışıklarını operatör ve kamera görünümünde çizer; yüzerlik, çarpışma, akustik ve isabet düğümlerini yalnız hata ayıklama katmanında ve kamera karesine girmeyecek biçimde (A2-K37) gösterir.
- **A7-K55.** A6 kaydı kullanılan kümenin manifest özetini, `kume_kimligi` ve `kume_surumu`yla, her türetilmiş düğümün kullanılan kanonik özetini ve etkin malzeme değerlerini taşır (A6'dan beklenen).
- **A7-K56.** **Taban derinliği.** Statik varlıkta `taban_derinligi_m` verilmişse sim, yüklemede `REF` noktasından gövde +z (aşağı) yönünde varlığın çarpışma kaynak ağlarına bir ışın atar; ilk kesişimin uzaklığı `taban_derinligi_m`'den en çok 1 mm (mimari seçimi) farklıdır, değilse `TABAN` ile reddedilir; ışın hiçbir ağa çarpmazsa da `TABAN`. Alan yalnız beyan ve denetim içindir: fizik ve sonar tabanı geometriden alır. Alan varlığın kendi gövde çerçevesinde ölçülen yerel bir uzunluktur ve dünya su derinliğiyle dönüşümsüz karşılaştırılmaz: dünya çerçevesinde taban derinliği, A5'teki statik poz (konum ve yönelim) bu yerel geometriye uygulanarak bulunur; A5'in `ortam.derinlik` denetimi bu dönüştürülmüş değere göre yapılır, alan yoksa A5 uyarı verir (A5'in kuralı; W1-uyum P3-11, doğrulama D4).
- **A7-K57.** **Malzeme alanlarının kaynağı.** Malzeme özelliklerinin alan adları (`geri_sacilim_mu`, `gelis_yasasi`, `ayna_yansima_rs`, `puruzluluk_gamma_p`; optikte glTF metalik-pürüzlülük çarpanları), enum değerleri (`lambert_cos2`, `cos1`) ve geçerli aralıkları (§3.4; `geri_sacilim_mu` > 0) A7'dedir. A5'in geçersiz kılmaları bunları **aynen** kullanır: başka ad, başka enum değeri ya da daha geniş aralık bir uyumsuzluktur ve geçersiz kılma reddedilir (W1-uyum P3-12; A7-K30).

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
| Örtü ölçütü aşıldı (eksik ya da taşma) | A7-K44 | — | — | `ORTU` (hangi ölçüt, en büyük uzaklık, yer) |
| A5 ile tutarsızlık | A7-K29, K52 | `A5_TUTARSIZ` | — | — |
| Taban derinliği geometriyle tutmuyor | A7-K56 | `TABAN` | — | — |
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
| A7-U12 | A7-K44 | D | Dört sınama vektörü (yeniden hesaplandı ve yürütülebilir bir modelle sınandı; rapor): (1) **delikli prizma:** `S` dış kutu `[−2, 2]² × [−1, 1]` m, içinden geçen delik `(−1, 1)² × [−1, 1]` (hacim 24 m³); tek parça deliği dolduran kutu `[−2, 2]² × [−1, 1]`. Ölçüt 1: 0 (`S` tamamen `D` içinde); ölçüt 2: delik ağzındaki üst yüz noktası `(0, 0, 1)`'in `K`'ya uzaklığı 1 m → `ORTU` (`τ ≤ 1 m` için eşik `0,8 τ < 1 m`). (2) **Birim küpün iki kusursuz yarısı** `[0, 0,5] × [0, 1]²` ve `[0,5, 1] × [0, 1]²`: ortak yüz merkezi `(0,5; 0,5; 0,5)` `K`'nın içinde, uzaklık 0; iki ölçüt de 0 → kabul (`τ = 0,01 m`). (3) **Büyütülmüş küp:** `S` kenarı 1 m küp, tek parça eş merkezli ve her yönde 2,5 mm büyük küp. Ölçüt 1: 0; ölçüt 2'de en büyük uzaklık köşede `2,5·√3 = 4,330127` mm (yüz merkezi 2,5 mm, kenar ortası 3,535534 mm). Beklenen: `τ = 0,01` (eşik 8 mm) kabul; `τ = 0,0055` (eşik 4,4 mm) kabul; `τ = 0,005` (eşik 4 mm) **ret** (gerçek uzaklık 4,33 mm < 5 mm olsa da örnek payı `δ` yüzünden ölçüt tutucudur); `τ = 0,004` (eşik 3,2 mm) ret. (4) **Eksik parça:** `S` birim küp, tek parça `[0, 0,5] × [0, 1]²`; ölçüt 1'de `(1; 0,5; 0,5)`'in `D`'ye uzaklığı 0,5 m → `ORTU`; ölçüt 2: 0. Ayrıca alt üçgen kenarlarının ≤ `δ` olduğu ve hiçbir parça örneğinin atılmadığı denetlenir |
| A7-U13 | A7-K45, K46, K50, K51 | SIL | Türetilmiş dosya silinir ya da bir baytı değiştirilir: sim çalıştırmayı başlatmaz; fizik, sonar ve görselleştirici tek bir manifest özeti bildirir |
| A7-U14 | A7-K52 | SIL | A5'te ∇ %2 değiştirilince `A5_TUTARSIZ`; üç kütle durumunda hacim ve merkez karşılaştırması |
| A7-U15 | A7-K30 | SIL | A5 akustik geçersiz kılması sonar profilini değiştirir, manifest özetini değiştirmez, A6'ya yazılır; bilinmeyen kimliğe geçersiz kılma A5 doğrulamasında reddedilir |
| A7-U16 | A7-K53 | S | Görselleştiriciye farklı bir manifest verilince A2 `ret(manifest)` ve sahne kurulmaz |
| A7-U17 | A7-K07, K08 | D | Değişen kümede `kume_surumu` artmadıysa depo denetimi reddeder; `assets/` altında türetilmiş dosya yok |
| A7-U18 | A7-K56 | B | `taban_derinligi_m` = 3,0 m ve tabanı 3,0 m'de olan havuz kabul; 3,002 m'de olan `TABAN`; tabansız statik varlık `TABAN`; araçta alan `SEMA` |
| A7-U19 | A7-K57, §3.7 | D | Uygunluk örneği (`assets/ornek/manifest.json`): şemaya uyar; A5 örnek senaryolarının atıf verdiği her varlık kimliği (`ida0`, `torpido_ida0`, `hedef_ornek`, `havuz_ornek`) örnekte vardır ve türü A5'teki rolüyle aynıdır; A7-K52 hacim/merkez karşılaştırması A5'in üç kütle durumuyla geçer; bağlantı noktaları A5 montaj ve tüp konumlarıyla aynıdır; A5 malzeme geçersiz kılmalarının adları ve enumları §3.4'teki adlarla aynıdır |

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

**A5'ten beklenen:** varlık kümesinin seçimi: küme yolu (`assets/` altındaki `kume_kimligi`) ve **beklenen küme özeti** (A7-K05'teki manifest özeti; isteğe bağlı olarak `kume_surumu`); araç, torpido, hedef ve statik sahne için varlık kimlikleri (uygunluk örneğinde `ida0`, `torpido_ida0`, `hedef_ornek`, `havuz_ornek`; örnek senaryolar bu kimliklerle tutar); `ortam.sahne`'nin A7'nin `statik` türüne eşlenmesi ve statik sahnenin pozu; `ortam.derinlik` denetiminin A7-K56'daki yerel `taban_derinligi_m`'ye statik poz uygulanarak bulunan dünya derinliğine göre yapılması (alan yoksa uyarı); kütle durumlarının ∇ ve `r_b` değerleri (A7-K52); sensör montajı, itici ve tüp konumları (A7-K29; tüp `f<k>` ↔ `TUBE_<k>`); malzeme geçersiz kılmalarında A7-K57'deki adların, enumların ve aralıkların aynen kullanılması.

**A6'dan beklenen:** A7-K55'teki alanlar.

**W2 depo iskeletinden beklenen (uygulama):** `.gitattributes` içinde `assets/**` için satır sonu çevirisinin kapatılması (`-text`) — aksi halde Windows'ta `core.autocrlf` manifestin baytlarını değiştirir ve A7-K04/K05 ihlal edilir; derleme dizininin `.gitignore`'da olması; A7-K01, K07, K08 depo denetimleri; türetme adımının CMake'e bağlanması.

---

## 9. Açık bulgu kapanış tablosu

### 9.1 `acik-bulgular.md` "A7" başlığı

| Bulgu | Durum | Sözleşmede | Gerekçe |
|---|---|---|---|
| AB-102 | kapandı | A7-K08, K40–K46, §3.5 | Türetilmiş ağlar depoya girmez (iskelet A7); manifest her türetilmiş düğümün platform başına kanonik özetini taşır; özet tutmazsa derleme durur ve çalışma zamanı reddeder; platformlar arası farklılık ayrı anahtarla karşılanır, belirlenimcilik ölçümü A7-A01'de; örtü denetimi katı cisme göre iki tek yönlü ölçütle tanımlı ve güvencesi kanıtlı (A7-K44) |
| AB-103 | kapandı | A7-K47 | Blender dışa aktarması derlemenin parçası değildir; derleme yalnız depodaki `.glb` ve manifesti iskelet §7'nin izin verdiği araçlarla kullanır, bu yüzden A7 hattı A16'nın yanıtına bağlı değildir (iskelet §7 ve §10 md.25 derleme zamanında Python'a zaten izin verir) |

### 9.2 Uyumsuzluk tablosunda A7'ye değinen satır

| Satır | Durum | Sözleşmede | Gerekçe |
|---|---|---|---|
| U-20 | kapandı | §3.4, A7-K30 | Akustik varsayılanlar A7 manifestinde, senaryoya özgü değerler A5'te aynı kimlikle; kuralın A7 tarafı burada, sözdizimi A5'te |
