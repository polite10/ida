# 07 — Görselleştirme

Sürüm: 0.1 (2026-11-13). Durum: taslak; inceleme ve kullanıcı onayı bekliyor.

Bu bölüm [00-iskelet.md](00-iskelet.md) (sürüm 0.5) Bölüm 9, satır 07'nin kapsamını yazar: A2, Godot süreci, sahne, kamera sensörü hattı, dalga tutarlılığı, A7 sahne hattı, kamera ile algılama bileşeninin yeri ve kalite kademelerinin görsel tarafı. İskeletle çeliştiği yerde iskelet geçerlidir. Dayanak [../research/00-duzeltmeler-ve-kararlar.md](../research/00-duzeltmeler-ve-kararlar.md) belgesidir ("00").

Atıf biçimi: `12.n`, `10.n` = 00 Bölüm 4'teki düzeltme satırı; `Kn`, `Ön`, `Mn`, `Çn`, `An` = 00'daki karar, öneri, ölçüm, çelişki ve açık soru; `D12 #k`, `D12 Yk`, `D12 Nk`, `D12 E:…` = [../research/dogrulama/12-codex.md](../research/dogrulama/12-codex.md) maddeleri (D10 aynı biçimde); `R12 §x`, `R10 §x` = ham rapor bölümü; `B03 §x`, `B04 §x`, `B05 §x`, `B06 §x` = komşu mimari bölüm. Ham rapordan alınan her değer önce 00'daki düzeltmeyle karşılaştırıldı.

**Değerlerin niteliği.** Bu bölümde hiçbir kare hızı, gecikme, GPU süresi ya da işlemci yükü ölçülmemiştir; bu makinede hiçbir şey derlenmedi ya da çalıştırılmadı. Sayılar üç türdür: **hesap** (aritmetik; ölçüm değil), **mimari varsayımı** (bu bölümün seçtiği başlangıç değeri; parametredir) ve **tahmin** (ölçülene kadar geçersiz).

---

## 1. Amaç ve sorumluluklar

Görselleştirici, sim sunucusunun yetkili durumunu çizen ayrı bir Godot sürecidir (Ö5; R12 §3 md.1). Sorumlulukları:

1. **Operatörün 3B görünümü:** aracın, torpidoların, hedeflerin, su yüzeyinin ve ortamın çizimi; sahne içi göstergeler (sonar tarama yönü, izler, durum yazıları).
2. **Kamera sensörü görüntüsü:** sim sunucusunun istediği anda, istenen pozla kare üretmek, geri okumak ve paylaşımlı belleğe yazmak.
3. **Su altı optiği ve dalga yüzeyinin görüntüsü:** fizik çekirdeğinin dalga bileşen listesinden, rastgele sayı üretmeden.
4. **A2'nin görselleştirici tarafı:** el sıkışma, durum akışının alınması, kare ve durum bildirimlerinin gönderilmesi.
5. **A7 sahne hattı:** tek kaynaktan (Blender → glTF) bütün tüketicilerin türetildiği hattın tanımı ve görselleştiricinin payı.
6. **Kamera ile algılama bileşeninin yeri ve arayüzü** (algoritmanın içi değil).

**Yapmadıkları:** fizik, çarpışma, dalga hesabı ve zaman yönetimi (B04); kamera sensörünün zamanlaması, teslim gecikmesi modeli ve sonar (B05); A1 mesajlarının karta taşınması (B06); harita, grafik ve form panelleri, A5 biçimi, A6 kaydı (B08). Harita ve grafikler web'de, ayrı penceredir (K9, Ö10); Godot yalnız 3B görünüm, kamera sensörü ve sahne içi göstergelerden sorumludur.

**İlkeler:**

- **Godot fizik yapmaz.** Godot'nun fizik düğümleri ve fizik enterpolasyonu kullanılmaz; düğümlere sim sunucusunun pozları yazılır (R12 §3 md.3). Görselleştirici dünyayı değiştiren hiçbir komut göndermez; A2'de görselleştiriciden sim'e yalnız kare ve durum bildirimi gider.
- **Tek sahne, ayrı görünümler:** operatör görünümü ile kamera sensörü aynı `World3D`'yi paylaşan ayrı viewport'lardan üretilir; ikinci bir ayrıntılı 3B motor kurulmaz (Ö5; D10 Ç; Ç2).
- **Süreç izolasyonu ile deney geçerliliği ayrıdır.** Görselleştirici çökse ya da gizlense de fizik, HIL ve kamera dışı sensörler sürer; fakat kamera girdisini kullanan bir deney kare kaybından etkilenir ve geçerliliği ayrı değerlendirilir (12.17; D12 Y12).

---

## 2. İç yapı

### 2.1 Süreçler ve veri akışı

```
Sim sunucusu (C++, B04/B05)                     Görselleştirici (Godot 4.7.2 + ince GDExtension)
 dünya ── VizState 60 Hz, olaylar ──┐            ┌─ A2 istemcisi ─ durum tamponu (son iki durum, olay listesi)
 kamera modeli (B05) ─ CameraRequest┼─ TCP ─────►├─ sahne uygulayıcı ─ düğüm pozları, dalga zamanı, ışık
 WaveSpectrumSet, ortam ────────────┘ (loopback) ├─ operatör viewport'u ─ HUD, sahne içi göstergeler
          ▲  FrameReady, ViewerStatus            ├─ kamera SubViewport'u ─ su altı optiği ─ geri okuma
          │                                      └─ kare halkası yazarı ──┐
          │                                                               ▼
          │                         paylaşımlı bellek: kare halkası (sim oluşturur)
          │                                                               │ salt okunur
          └──── algılama kayıtları ── kamera algılama bileşeni (C++, ayrı süreç) ◄┘
                (B06 üzerinden karta A1 IDA_DETECTION)
```

- **Sim sunucusu** sunucudur; görselleştirici ve algılama bileşeni ona bağlanan istemcilerdir. Süreçleri 08'in çalıştırma kontrolü başlatır; görselleştirici elle de başlatılıp bağlanabilir.
- **GDExtension ince tutulur:** yalnız IPC, poz uygulama, kamera hattı, dalga ve optik parametreleri. Embree, Jolt ve Eigen eklentiye girmez (R12 §1 md.5; R12 §4 R9).
- **A2 serileştirme kitaplığı** (C++, başlık tabanlı) sim sunucusu, eklenti ve algılama bileşeninde aynı kaynaktan derlenir. Biçim açık serileştirmedir; ham `struct` kopyası değildir (00 Ç3'teki ders).
- **Duvar saati:** üç süreç aynı makinededir; duvar saati işletim sisteminin monoton saatidir (macOS `CLOCK_MONOTONIC`, Windows `QueryPerformanceCounter`), süreçler arasında ortaktır. Sim zamanı her zaman `int64` ns olarak taşınır (iskelet §3.1, §4).

### 2.2 Godot sahnesi

| Düğüm | İçerik |
|---|---|
| `Dünya` | Senaryo orijini; NED → Godot dönüşümü burada uygulanır (2.3) |
| `Gövde[id]` | Araç, torpidolar, hedefler; her biri A7'den yüklenmiş görsel model + sabit glTF düzeltmesi (2.3) |
| `Su` | Dalga yüzeyi ağı ve gölgelendiricisi (2.7); su altı hacmi |
| `Işıklar` | Güneş/ortam ya da havuz lambaları (A7'deki `KHR_lights_punctual` düğümleri; A5 geçersiz kılar) |
| `Operatör` | Root penceredeki 2B tuvalde gösterilen operatör `SubViewport`'u ve `Camera3D`'si; serbest, araç takip ve kamera önizleme kipleri |
| `KameraSensörü[k]` | Sabit çözünürlüklü `SubViewport` + `Camera3D` + su altı optiği `CompositorEffect`'i (2.5) |
| `Göstergeler` | Sahne içi göstergeler; ayrı görünürlük katmanında |

- Operatör görünümü de bir `SubViewport`'tur; böylece iki görünümün güncelleme kipi ayrı ayrı denetlenir (2.4).
- **Göstergeler kamera görüntüsüne girmez:** göstergeler, çarpışma ve sonar ağlarının hata ayıklama çizimleri ayrı görünürlük katmanındadır; kamera sensörünün `cull_mask`'i bu katmanı içermez.
- **Sayı tipi:** Godot ve godot-cpp tek duyarlıklı (`single`) derlenir (2.10). Konumlar senaryo orijinine göredir; birkaç km'lik açık su sahnesinde `float` göreli hatası mm altıdır (hesap: 2⁻²⁴ × 1000 m ≈ 0,06 mm). Durum A2'de `double` gelir, dönüşüm eklentide yapılır.

### 2.3 Çerçeve dönüşümleri

B04 §2.2 Godot dönüşümünü A2 sınırına, bu bölüme bırakır. Godot sağ elli, +Y yukarı, −Z ileri kabul eder.

- **Dünya:** `x_G = E`, `y_G = −D`, `z_G = −N`. Matris `C = [[0,1,0],[0,0,−1],[−1,0,0]]`, `det C = +1` (hesap).
- **Gövde:** gövde x ileri, y sancak, z aşağı (iskelet §3.1); Godot yerel ekseninde ileri −Z, sağ +X, yukarı +Y. Aynı `C` matrisi gövde → Godot yerel dönüşümüdür (`x_l = y_b`, `y_l = −z_b`, `z_l = −x_b`).
- **Düğüm pozu:** `konum_G = C p_n`, `taban_G = C R(q_nb) Cᵀ`. Kuaterniyonun (w, x, y, z) sırası (iskelet §3.1) Godot'nun bileşen sırasına yalnız eklentide çevrilir.
- **glTF → Godot yerel:** B04'ün varlık dönüşümü `x_b = +Z_gltf`, `y_b = −X_gltf`, `z_b = −Y_gltf` ile `C` birleşince `(X, Y, Z)_l = (−X, Y, −Z)_gltf` çıkar: Y ekseni etrafında 180° (hesap, `det = +1`). Godot glTF'yi kendi ekseninde yüklediği için bu sabit dönüşüm görsel modelin altına konur ve manifeste yazılır (2.8).
- **Gövde referans noktası:** düğümün orijini A5'teki gövde referans noktası `O_b`'dir; torpido ayrılınca yerinde kalır (iskelet §3.1). Görsel model `REF` düğümüne göre yerleştirilir.

### 2.4 Zaman, iki görünüm ve ara değerleme

**Sorun.** Operatör görünümü pürüzsüzlük için gecikmeli ara değerlenmiş bir zamanı, kamera ise B05'in istediği pozlama ortası `t_meas`'ı çizer. Paylaşılan `World3D` aynı anda iki zamanın durumunu taşıyamaz; kameranın pozunu değiştirmek hedefleri, suyu ve ışığı geçmişe götürmez (D12 E:Tek sahnede iki zaman; 00 Bölüm 6).

**Kamera zamanı.** `t_meas` bir fizik adımı sınırındadır (iskelet §4; B05 §2.5). `CameraRequest` o adımın bütün gövde pozlarını ve dalga zamanını taşır; kamera çiziminde **ara değerleme yapılmaz**.

**Operatör zamanı.** Operatör sahne zamanı `t_op = t_son − D_op`; `D_op` = bir yayın periyodu (60 Hz'te 16,7 ms; mimari varsayımı). Saat, her `VizState` ile duvar saatine yavaşça eşlenir; SIL'de sim hızıyla, duraklatmada durur. İki durum arasında konumda kübik Hermite (hızlarla), yönelimde slerp kullanılır (B04 §3'teki `interpolate` ile aynı kural). Son durumun ötesine ekstrapolasyon yapılmaz: veri gecikirse son durumda beklenir ve gecikme sayılır.

**Çözüm (birincil): sıralı uygulama, ayrık çizim.** Bekleyen kamera isteği olan her Godot karesinde:

1. Sahneye `t_meas` durumu uygulanır (gövdeler, dalga zamanı, ışık); kamera viewport'ları `UPDATE_ONCE`, operatör viewport'u `UPDATE_DISABLED`.
2. Ana iş parçacığından `RenderingServer.force_draw(false, dt)` çağrılır (2.6); geri okuma kuyruğa alınır (2.5).
3. Sahneye `t_op` durumu uygulanır; kamera viewport'ları kapatılır, operatör viewport'u olağan çizimle çizilir. Pencere çizilemiyorsa bu adım kendiliğinden atlanır; operatör zaten görmüyordur.

Gerekçe: görünürlükten bağımsız çizim için zaten `force_draw` gerekiyor (2.6); kamera geçişi her zaman aynı yoldan geçerse bu yol sürekli sınanır. Bilinen risk: Godot hareket vektörleri ve zamansal birikim için önceki dönüşümleri tutar; iki zaman arasında gidip gelmek bunları bozabilir (çıkarım, doğrulanmadı). Bu yüzden **ilk sürümde kamera görünümünde geçmiş kareye dayanan etkiler kapalıdır** (TAA, SDFGI, zamansal hacimsel sis; mimari varsayımı). Bu, #97658 türü elle çizim sorunlarını da sensör yolundan uzak tutar (D12 E:#97658). Ek GPU maliyeti ölçülmedi (M12, M30).

**Yedek: ortak yakalama zamanı.** Kamera isteği olan karede bütün viewport'lar tek çizimde `t_meas` durumuyla çizilir; operatör görünümü o karede aynı zamanı gösterir, ara karelerde en son alınan durumu (`D_op = 0`) gösterir. Akış sim zamanı sırasıyla geldiği için `t_meas`, operatörün daha önce gösterdiği zamandan geride kalmaz; operatör görünümü daha az pürüzsüzdür ama zaman geriye gitmez. Seçim 6. başlıktaki ölçümle yapılır.

**Ara değerleme olay sınırlarını aşmaz** (B04 inceleme 2, bulgu 7). Her gövde durumu bir `süreklilik_no` taşır; şu olaylarda artar: darbe (fırlatma geri tepmesi), kütle durumu değişimi, gövde doğumu (torpido ayrılması), gövde ölümü (isabet, alan dışı), senaryo sıfırlaması ve tekrar oynatmada atlama. Kurallar:

- Ara değerleme yalnız aynı gövde kimliği ve aynı `süreklilik_no` taşıyan iki durum arasında yapılır.
- Sınırı kapsayan aralıkta `t_op` olay adımından önceyse eski parçanın son durumu, sonraysa yeni parçanın ilk durumu çizilir; iki parça arasında eğri kurulmaz.
- Doğan gövde ilk durumundan önce çizilmez; ölen gövde ölüm olayından sonra çizilmez.
- **B04'ten beklenen:** olay olan adımda, olağan 60 Hz takviminin dışında da bir `VizState` gönderilmesi; böylece her sınırın iki yanı akışta bulunur.

Dalga yüzeyi ara değerlenmez: yükseklik, sahne zamanında bileşen listesinden doğrudan hesaplanır (2.7).

**Kameranın ölçüm zamanı ve alım zamanı** (iskelet §3.1). Ölçüm zamanı `t_meas` = pozlama ortası; alım zamanı `t_teslim = t_meas + L_kam` B05'in modelidir (B05 §2.5). Görselleştirici her ikisini istekten alır ve kare üst verisine yankılar; kendi gerçek süresini ayrıca duvar saatiyle kaydeder (`istek_alındı_duvar_ns`, `çizim_duvar_ns`, `hazır_duvar_ns`). Modellenen gecikme ile gerçek hat gecikmesi birbirinin yerine kullanılmaz.

**Kuyruk gecikmesi.** Asenkron geri okumanın geri çağrısı `frame_queue_size` kare gecikebilir (12.12). Bu kare sayısı kamera hızıyla değil **gerçek çizim hızıyla** ilerler: 30 çizim/s'te 1–3 kare 33,3–100 ms, 60 çizim/s'te 16,7–50 ms (hesap). Bu yüzden görselleştirici döngüsü kamera hızından hızlı koşar (dikey eşzamanlama kapalı, `max_fps` = 120; mimari varsayımı) ve `frame_queue_size` prototipte 1–3 arasında denenir (R12 §6). Pencere çizilemezken kuyruğun `force_draw` ile ilerleyip ilerlemediği bilinmiyor; M28 bunu ayrıca ölçer (2.6).

**Geç kare politikası.**

| Kip | Davranış |
|---|---|
| SIL | Sim, kareyi `t_teslim` adımında bekler (B05 §2.5). `t_meas` ile `t_teslim` arasında dünya ilerlemeye devam eder; çizim bununla örtüşür. Etkin hız çizim hızıyla sınırlanır (R10 §2.9) |
| HIL 1x | Kare `t_teslim`'e karşılık gelen duvar saati son tarihine kadar halkada hazır değilse düşürülür, sayılır ve A6'ya yazılır; teslim zamanı kaydırılmaz (B05 §2.5; 00 Bölüm 6). Görselleştirici de istekteki son tarihi geçmiş isteği çizmez, daha yeni istek gelince eskisini düşürür ("en yeni önce") ve nedenini bildirir |
| HIL 1x + bariyer | HIL 1x ile aynı; düşen her kare modellenen sensör akışını değiştirdiği için çalıştırma tekrarlanamaz işaretlenir (B05 §4'teki sonar kuralıyla aynı) |

Kamera kullanan bir deneyde (A5'te `kamera_gerekli`) HIL 1x'te düşen kare oranı %1'i aşarsa çalıştırma geçersiz işaretlenir (eşik mimari varsayımı; R12 §6'daki hedefle aynı).

### 2.5 Kamera sensörü hattı

```
t_meas durumu ─► kamera SubViewport (Forward+) ─► su altı optiği (CompositorEffect, compute)
   ─► [tam] sensör modeli: bozulma, gürültü, pozlama ─► renk dokusu (RGBA8, sRGB)
   ─► texture_get_rd_texture ─► texture_get_data_async (render iş parçacığı)
   ─► geri çağrı: PackedByteArray ─► halka yuvasına kopya ─► FrameReady (TCP)
```

- **Görünüm:** kamera başına sabit çözünürlüklü `SubViewport`, operatörle aynı `World3D` (R12 §2.3). İç parametreler (`fx, fy, cx, cy`) `Camera3D` görüş açısı ve ana nokta kaydırmasına çevrilir; mercek bozulması kendi son işlem geçişimizdir. Montaj B05'in **fizik gerçeği** montajıdır (B05 §3 `MountTransform`).
- **Geri okuma:** `RenderingServer.texture_get_rd_texture` → `RenderingDevice.texture_get_data_async`, `RenderingServer.call_on_render_thread` ile (R12 §2.3). Eşzamanlı `get_texture().get_image()` yedektir: kuyruk beklemesi yoktur ama GPU'yu bekletir (6. başlık).
- **Biçim:** RGBA8, sRGB kodlu, satır sırası üstten alta, satır arası dolgu yok (`satır_adımı = genişlik × 4`; RD'nin döndürdüğü satır hizası M30'da doğrulanır). Dördüncü kanal kullanılmaz; dönüşüm kopyası eklememek için atılmaz.
- **Kopya sayısı:** GPU → aktarım tamponu (sürücü), aktarım tamponu → `PackedByteArray` (Godot, 1 CPU kopyası), `PackedByteArray` → halka yuvası (1 CPU kopyası). Sim sunucusu piksele dokunmaz, yalnız üst veriyi okur; algılama bileşeni yuvayı yerinde okur (0 kopya). A6'ya kare kaydı (B08) ayrı bir okuyucudur. Paylaşımlı bellek GPU geri okumasını ve `PackedByteArray` aktarımını kopyasız yapmaz (12.11); toplam maliyet M30'da ölçülür.

**Veri hızı** (hesap; protokol ve tampon maliyeti hariç; 12.11, Tablo 12.0):

| Yapılandırma | Kare | Hız |
|---|---|---|
| 640 × 480 RGBA8, 30 Hz (tam, mimari varsayımı) | 1.228.800 B | 36,864 MB/s |
| Aynısı, iki kamera | — | 73,728 MB/s |
| 320 × 240 RGBA8, 15 Hz (sade, mimari varsayımı) | 307.200 B | 4,608 MB/s |
| 1280 × 720 RGBA8, 30 Hz (M12'nin deney noktası) | 3.686.400 B | 110,592 MB/s |

İskelet §5'te tek öne bakan kamera vardır; çözünürlük, hız ve kamera sayısı A24'e bağlıdır.

**Su altı optiği.** Yaklaşım, derinlik tamponundan alınan menzil `z` ile kanal başına Akkaynak–Treibitz biçimidir (R10 §2.5.1):

```
I_c = J_c · exp(−β_c^D · z) + B_c^∞ · (1 − exp(−β_c^B · z)),   c ∈ {R, G, B}
```

- **Zayıflama** (`β_c^D`), **geri saçılım** (`β_c^B`, `B_c^∞`) ve **bulanıklık** (ileri saçılım için menzille büyüyen bulanıklaştırma çekirdeği) `CompositorEffect` içinde compute ile uygulanır (R12 §2.5). Sınıf deneysel etiketlidir ve yalnız Forward+/Mobile'da çalışır; etki tek dosyada yalıtılır (R12 §4 R4). Mobile renderer'daki #120019 nedeniyle Forward+ kullanılır (12.13).
- **Fiziksel spektral doğruluk değildir.** RGB render'a kanal başına ya da bantlı son işlem uygulamak spektral ışık taşınımı yapmaz; kayıp spektral bilgi RGB'den geri getirilemez (10.6; D10 #14). Model fenomenolojiktir; katsayılar **kalibrasyon gerektirir** ve görüntüler arasında aktarılamaz (R10 §2.5.1).
- **Kalibrasyon parametreleri A5'ten gelir:** `su_optiği {β_D[3], β_B[3], B_inf[3], bulanıklık_ölçeği, kaynak, sürüm}` ve `kamera[k] {çözünürlük, hız, iç parametreler, bozulma, pozlama, gürültü, kalibrasyon_sürümü}` (biçim B08'in). Başlangıç değerleri **[K]**dir; açık veri kümeleriyle (ör. SQUID renk kartları; R10 §2.8) uydurulur. Gerçek kamera kaydı K8 kapsamında değildir; M14'ün kamera kısmı bu aşamada açık veriyle sınırlıdır.
- **Yüzey geçişi:** ilk sürümde kamera merkezinin yerel dalga yüzeyine göre konumu bütün karenin su altı/su üstü durumunu belirler; piksel başına maske (R10 §2.6 md.4) tam kademede sonraki adımdır.
- **Sensör modeli (tam kademe):** mercek bozulması, okuma ve atış gürültüsü, sabit pozlama. Gürültü, sim'in verdiği `(tohum, kamera_id, kare_seq)` ile karma tabanlı üretilir; Godot kendi rastgele üretecini kullanmaz. Hareket bulanıklığı ve kayan perde ilk sürümde yoktur (8. başlık).

### 2.6 Görünürlükten bağımsız çizim (Ö5-k)

**Neden olağan çizim döngüsüne güvenilmez.** Godot 4.7.2'de `Main::iteration()`, çizilebilir pencere yoksa (ek çıktı ve bekleyen kaynak işi istisnaları dışında) `RenderingServer::draw`'ı çağırmaz; olağan otomatik çizimin sürmesine güvenilemez (12.1; D12 #1a). `UPDATE_ALWAYS` bunu çözmez; güncelleme kipi ancak çizim işlevine girildikten sonra değerlendirilir (D12 #1d; Ç14). Varsayılan kip `UPDATE_WHEN_VISIBLE`'dır (12.4).

| İşletim sistemi | Çizim ne zaman kesilir | Kaynak |
|---|---|---|
| macOS | Pencere başka pencerelerce **tamamen örtülürse**, gizlenirse ya da simge durumuna küçültülürse (`NSWindowOcclusionStateVisible` ve `isVisible` birlikte gerekir) | 12.1; D12 #1b |
| Windows | **Bütün** pencereler simge durumuna küçültülürse. Yalnız örtülme aynı durum değildir; Godot çizmeye devam eder, ancak DWM ve GPU davranışı ölçülmedi | 12.1; D12 #1c |

Ayrıca işletim sisteminin arka plan kısıtlamaları ayrı bir risktir: macOS'ta App Nap (M13), Windows'ta güç kısıtlaması. Eklenti macOS'ta etkinlik bildirimiyle App Nap'ı, Windows'ta süreç güç kısıtlamasını kapatmayı dener; ikisinin etkisi doğrulanmadı (M13, M28).

**Birincil: elle çizim.** Kamera geçişi her zaman ana iş parçacığından `RenderingServer.force_draw(false, dt)` ile çizilir (2.4). 4.7.2 kaynağında bu çağrı doğrudan `RenderingServer::draw`'a bağlanır ve `Main::iteration()`'ın görünürlük kapısından geçmez; uygun `ONCE`/`ALWAYS` viewport'ları dokuya çizilebilir (12.3; D12 E:force_draw). Ancak 4.7.2'de macOS örtülü ve Windows simge durumunda **kesintisiz** üretim doğrulanmadı (D12 #2b); başarılı tek kare sürekli akışı, zamanında geri okumayı ve aydınlatmanın doğru ilerlediğini kanıtlamaz (D12 E:force_draw, E:#97658). `--disable-render-loop` (yalnız açıkça istendiğinde çizim) SIL için ikinci bir seçenek olarak prototipte denenir (R12 §2.2); birincil yol değildir.

**Yedek: pencere çizilebilir tutulur, kamera yalnız o koşulda geçerlidir.** Koşul "pencere var" değil "pencere çizilebilir"dir: küçük ya da köşedeki bir pencere macOS'ta tamamen örtülebilir (12.2; D12 Y3). Görselleştirici her karede çizilebilirliği (Godot'nun çizilebilir pencere durumu ve çizilen kare sayacının ilerlemesi) kare üst verisine `çizilebilir` bayrağı olarak yazar. Yedek kipte bu bayrak düşükken kare üretilmez ya da üretilen kare geçersiz işaretlenir; düşen kareler "görünürlük" nedeniyle sayılır, HUD'da ve A3 üzerinden uyarı verilir. Yedeğin seçilmesi, deney kapsamının yalnız görünür pencereyle sınırlı olduğunun açıkça kabulü demektir (Ö5-k; A23 kullanıcıya sorulur).

**Başsız kip sensör yolu değildir.** `--headless` altında RenderingDevice yoktur; gizli pencereyle GPU çizimi ile başsız kip farklı çalışma biçimleridir (D12 #12; 00 Bölüm 6). Kamera sensörü hiçbir kipte `--headless` ile planlanmaz.

**İlk kabul kapısı.** Godot prototipinin ilk kapısı, macOS'ta tamamen örtülü ve gizli, Windows'ta simge durumundaki pencerede sürekli kamera üretimidir (Ö5-k). Ölçüm M28 (M13'ün somut hali); kabul ölçütleri 7. başlıkta. Sonuç işletim sistemi başına kayda geçer. Kapı bir işletim sisteminde geçilemezse o işletim sisteminde yedek tasarımın kendisi olur; bu bir kalite kademesi değildir (iskelet §6).

### 2.7 Dalga tutarlılığı

**Girdi.** B04'ün `WaveSpectrumSet`'i: sürüm, özet, tohum, `g`, `ρ`, `h`, faz konvansiyonu ve bileşenler `{a, ω, k, β, φ}`; varsayılan 64 bileşen, fizik hepsini toplar (B04 §2.5). Görselleştirici rastgele sayı üretmez; dalgayı yalnız bu listeden çizer (R12 §2.5).

**Yöntem: doğrudan toplam, kesmesiz.** Su ağının her köşesinde yükseklik `ζ(x, y, t_s) = Σ a_i cos(k_i (x cos β_i + y sin β_i) − ω_i t_s + φ_i)` köşe gölgelendiricisinde **bütün** bileşenlerle toplanır (faz işaretleri B04'ün konvansiyonuna göre). FFT ve ayrı bir `h0` ızgarası kullanılmaz; böylece fizikle görüntü aynı seridir ve bileşen kesmesi sıfırdır (12.14'teki `Σ|a_i|` farkı doğmaz).

Kalan fark kaynakları ve sınırları:

| Kaynak | Kural | Sınır |
|---|---|---|
| Kayan nokta | Zaman terimi `(φ_i − ω_i t_s) mod 2π` CPU'da `double` hesaplanıp gölgelendiriciye verilir; uzun koşuda `float` zaman hassasiyeti kaybı olmaz | GPU `float`; fark test noktalarında ölçülür |
| Ağ ara değerlemesi | Köşeler arası doğrusal ara değerleme farklı bir işlemdir (10.7). Tutarlılık yarıçapı `R_tutarlı` içinde ağ adımı `Δx ≤ λ_min/16` | Dalga yönünde bileşen başına `a_i (k_i Δx)²/8` ≤ `a_i`'nin %1,93'ü (doğrudan türetim, tek boyutlu; üçgen ağda yönle değişir, test ölçer). 10.7'nin `kΔx = π/2` örneğindeki 0,207 A farkı bu sınırla (0,308 A) tutarlıdır |
| Ayrıntı düzeyi | `R_tutarlı` (varsayılan 25 m; mimari varsayımı) araç ve kamera çevresinde tam ağ; dışında kabalaşan halkalar | Dışarıda sınır verilmez; yalnız görsel |
| Yatay öteleme | Fizik doğrusal (Airy) yüzey kullanır; görselde choppy yatay öteleme **yoktur** (R10 §2.6 md.2) | 0 |
| Görsel zamanlama | Yükseklik her geçişte kendi sahne zamanında (`t_meas` ya da `t_op`) hesaplanır; kademeli ya da yük dengelemeli güncelleme yok (D12 E:Dalga eşleşmesi) | 0 |
| Yalnız görsel ayrıntı | Köpük ve ince dalgacık yalnız gölgelendirme normalinde; geometriyi değiştirmez | Geometri farkı 0; görüntü farkı fizikte karşılıksızdır ve böyle işaretlenir |

**Maliyet** (hesap, GPU süresi ölçülmedi): 256 × 256 = 65.536 köşe × 64 bileşen ≈ 4,2·10⁶ değerlendirme/çizim; operatör 60 + kamera 30 çizim/s'te ≈ 3,8·10⁸ değerlendirme/s. Yetmezse yedek, fizik çekirdeğinin ürettiği aynı bileşenlerden kurulmuş FFT ızgarasıdır (R12 §2.5 seçenek B); o durumda kesme, ızgara ve LOD farkı yeniden sınırlanır (M32).

**Hazır okyanus eklentileri.** GodotOceanWaves (MIT; ana dalın son commit'i 27 Eylül 2024; Godot 4.3) ve godot4-oceanfft (MIT; son commit 19 Kasım 2025; "erken aşama") uzun süredir hareketsizdir; bu bakım riskidir, terk edilmişlik kanıtı değildir (12.15). İkisinde de dışarıdan bileşen ya da `h0` girdisi yoktur ve kademe güncellemesi yük dengelidir (R12 §2.5; D12 E). Swell'in lisansı ve sürümü doğrulanmadı (R12 §2.5). Bu yüzden **bağımlılık olarak kullanılmaz**: dalga geometri gölgelendiricisi, su altı optiği ve yüzey geçişi kendimizce yazılır. Köpük, Fresnel ve gökyüzü yansıması gibi gölgelendirme teknikleri MIT lisansıyla, kaynak belirtilerek uyarlanabilir. Okyanus eklentileri hazır sensör modeli sayılmaz (D12 N7).

### 2.8 A7 sahne hattı

**Akış** (R12 §2.7, §3 md.7):

```
assets/kaynak/*.blend ─► dışa aktarma betiği ─► sahne.glb + manifest
                                              ├─► Godot görsel model (GLTFDocument + IDA uzantısı; bu bölüm)
                                              ├─► Embree sonar geometrisi (SON_* ya da COL_*'dan türetilmiş; B05)
                                              ├─► yüzerlik ağları BUOY_* (B04)
                                              └─► çarpışma: COL_* → CoACD dışbükey parçalar, çevrimdışı (B04)
```

**Düğüm adlandırma** (ad kuralı; ayrıca `extras` bayrağı):

| Önek | Anlamı | Tüketen |
|---|---|---|
| (öneksiz) | Görsel ağ | 07 |
| `COL_` | Çarpışma kaynağı; çevrimdışı dışbükey parçalara bölünür | B04 |
| `BUOY_` | Kapalı, manifold yüzerlik ağı (gerçek deplasman) | B04 |
| `SON_` | Akustik ağ (yoksa `COL_`'dan türetilir) | B05 |
| `REF` | Gövde referans noktası `O_b` | hepsi |
| `MNT_<sensör>`, `THR_<k>`, `TUBE_<k>` | Sensör montajı, itici, fırlatıcı bağlantı noktaları | B04, B05 |
| `HIT_` | Hedefin isabet bölgesi (B03 §2.8) | B04, B08 |

**Malzeme.** Her üçgen bir `malzeme_id` taşır; aynı kimlik görsel (glTF standart PBR), akustik (geri saçılım katsayısı, geliş açısı yasası, speküler katsayı, pürüzlülük) ve optik (kanal yansıtıcılığı) özellikleri bağlar (R10 §2.7; B05 §2.4.1). Standart dışı veri adlı bir vendor uzantısında (`IDA_materials`) durur, `extensionsUsed` içinde bildirilir, `extensionsRequired` içine **konmaz**; böylece uzantıyı bilmeyen okuyucu dosyayı yine yükler. "Diğer araçlar yok sayar" koşulsuz değildir (12.16). Sunucuda `cgltf` (C, MIT), Godot'da `GLTFDocumentExtension` aynı JSON şemasını okur (R12 §2.7). Godot sahneyi çalışma anında aynı `.glb` dosyasından yükler; böylece özet karşılaştırması anlamlıdır. A7 değerleri varsayılandır; A5 aynı kimliklerle geçersiz kılabilir ve geçersiz kılma A6'ya yazılır.

**Manifest içeriği** (yalnız dosya özeti değil; 12.16, D12 E:Varlık manifesti):

- şema sürümü, varlık sürümü, `.glb` ve kaynak `.blend` SHA-256 özetleri;
- dışa aktarıcı: Blender sürümü, dışa aktarma ayarları, betik sürümü;
- koordinat dönüşümleri: glTF → gövde (B04 §2.2) ve glTF → Godot yerel (2.3); birim metre, ölçeğin uygulanmış olduğu;
- üçgenleme ayarı; düğüm tablosu (ad, önek, gövde kimliği, malzeme);
- `BUOY_` başına hacim, hacim merkezi, kapalılık sonucu ve en uzun kenar (B04 §2.4'ün dalga boyu kuralı için);
- çarpışma üretimi: CoACD sürümü, parametreler, tohum, iş parçacığı sayısı (R12 §4 R8);
- `SON_` türetme ayarları (B05 §2.4.1);
- malzeme tablosu özeti, `extensionsUsed` listesi;
- **manifest özeti:** kanonik manifest metninin SHA-256'sı. B04, B05 ve bu bölüm bu tek değeri karşılaştırır.

**Tutarlılık denetimi.**

1. *Hat sırasında:* `BUOY_` kapalı ve manifold, hacim ve merkez A5 ile tutarlı (B04 §4: %1 hacim, 2 mm merkez); `COL_` parçaları dışbükey ve görsel ağı örter (Hausdorff uzaklığı eşiği; mimari varsayımı 1 cm); her `malzeme_id` tabloda; ad kuralı.
2. *Çalışma başında:* A2 el sıkışmasında manifest özeti sim'inkiyle karşılaştırılır; uyuşmazlıkta görselleştirici sahneyi kurmaz (3. başlık), B05 sonarı etkinleştirmez (B05 §4).
3. *Koordinat testi:* bilinen işaret düğümünün (`MNT_kamera`) Godot'daki dünya konumu ile sim'deki konumu 1 mm içinde (7. başlık).

**Görselleştiricinin payı:** görsel ağların, malzemelerin ve ışıkların yüklenmesi; `COL_`, `BUOY_`, `SON_` düğümleri yalnız hata ayıklama katmanında çizilir. Çarpışma ve sonar tarafının kullanımı B04 ve B05'indir.

### 2.9 Kamera ile algılama bileşeni

**Yer.** Kamera ile hedef algılama kartta yapılmaz; PC'de sim sunucusuna bağlı ayrı bir bileşendir (iskelet §3; Ç5). Bu bölüm onu **ayrı bir C++ süreci** (`camdet`) olarak tanımlar: çökmesi sim'i durdurmaz, algoritması bağımsız değiştirilir, sürümü kayda girer. Kamera kapalıyken başlatılmaz.

**Kareleri nereden alır.** Görselleştiricinin yazdığı halkayı salt okunur eşler; sim sunucusu her hazır kare için `FrameAvailable` bildirir. Bileşen kareye hazır olur olmaz başlayabilir; sonucun karta ulaşacağı an ise modellenir (aşağıda). Bileşene **yalnız** piksel ve kare üst verisi (`kamera_id`, `kare_seq`, `t_meas_ns`, `pozlama_ns`, iç parametreler, bozulma, `calib_ver`, kademe) verilir. Araç pozu, seyrüsefer ve yer gerçeği verilmez (B03 §2.8: algılama seyrüsefer ya da yer gerçeği pozu kullanmaz).

**Ürettiği kayıt.** B03 §3.2'deki `ida_detection_t` biçimindedir:

| Alan | Bileşenin doldurduğu |
|---|---|
| `t_capture` | Karenin `t_meas_ns`'i, **değiştirilmeden** (pozlama ortası; B03 §2.2 md.3) |
| `t_sigma_ns` | Algoritmanın verdiği zaman belirsizliği |
| `source`, `inst` | `CAM_FWD`, kamera örneği |
| `frame`, `kind` | `SENSOR`; tek kamerada `B2` (yön, yükseliş), bilinen hedef boyutundan menzil çıkarılıyorsa `RB3` |
| `z`, `cov` | Sensör çerçevesinde ölçüm ve yalnız ölçüm belirsizliği |
| `class`, `confidence` | Algoritmanın çıktısı |
| `calib_ver` | Karede kullanılan kamera kalibrasyon sürümü |
| `producer_id`, `producer_ver` | Algoritma ve sürümü |

Algılama olmayan kare için de boş bir yanıt döner; böylece "işlenmedi" ile "hedef yok" ayrılır.

**Teslim.** Sonuç sim sunucusuna gider; kartın alım zamanı `t_teslim_alg = t_teslim + L_alg` olarak modellenir (`L_alg` A5'te, [K]). SIL'de sim bu ana kadar bekler; HIL'de sonuç o ana karşılık gelen duvar saatine kadar gelmezse düşürülür, `algılama_geç` sayılır, A6'ya yazılır. Kayıt karta B06 üzerinden A1 `IDA_DETECTION` olarak gider. Bu kural B06'nın bu bölümden beklediği düşürme kuralıdır (B06 §3). Tekrar oynatmada kareler yeniden çizilebilir ama bit düzeyinde aynı çıkmaz; karta giden algılama kayıtları A6'dan oynatılır.

**Algoritmanın içi bu bölümün kapsamında değildir.** Arayüz, SIL'de yer gerçeğinden gürültülü kayıt üreten anlamsal algılama üretecinin de aynı yuvaya takılmasına izin verir (`producer_id` farklı); bunun kapsama alınması B03 ve B05 ile birlikte kararlaştırılır (B03 §8.4, B05 §8).

### 2.10 Derleme ve sürüm

- **Sürümler sabittir:** Godot **4.7.2-stable** (18 Ağustos 2026; güncel kararlı; 4.8 beta kararlı değil), godot-cpp **10.0.0-stable** (15 Eylül 2026), `api_version = 4.7`, `.gdextension` içinde `compatibility_minimum = 4.7` (D12 #11, #4a, #4b; R12 §1 md.1). Eski minör için derlenen uzantı sonraki minörde çalışır, tersi beklenmez; bu sınırsız garanti değildir (D12 #4b). Godot yükseltmesi M28, M29 ve M30'un yeniden koşulmasını gerektirir (R12 §4 R3).
- **Renderer:** Forward+ (2.5). Sürücüler: macOS'ta Metal (macOS 26+ Metal 4, eskilerde Metal 3; A26), Windows'ta D3D12 (yeni projelerde varsayılan; D12 #24).
- **Yapı:** CMake birincil (godot-cpp CMake desteği etkin ama SCons'a göre ikincil; D12 #4c), SCons yedek. godot-cpp diğer bağımlılıklardan önce eklenir; MSVC çalışma zamanı değişkeni **`CMAKE_MSVC_RUNTIME_LIBRARY`** (tekil) bütün bağımlılıklarda aynıdır (12.10). `GODOTCPP_PRECISION = single`, kullanılan Godot derlemesiyle aynı. macOS hedefi yalnız arm64 (K4). Sıcak yeniden yükleme yalnız hata ayıklama derlemesinde (R12 §2.1).
- **macOS imzası koşulludur:** laboratuvar içinde yerel çalıştırmada ad-hoc imza yeterlidir; Godot belgesi GDExtension ve ad-hoc imzada "Disable Library Validation" yetkisini önerir, fakat aynı Team ID ile imzalı kütüphaneler Apple'ın koşulunu zaten karşılar; yetki her dağıtımda zorunlu değildir (12.8). Notarization yalnız dağıtım gerekirse ve Developer ID ile yapılır. Hepsi M33'te sınanır.
- **Metal kararlılığı ve shader derleme süresi ayrı ölçülür;** "4.6.x'e dönmek sorunu çözer" varsayılmaz (12.9; M29).
- **Proje ayarları sürümlenir:** renderer, `frame_queue_size`, dikey eşzamanlama, `max_fps`, düşük işlemci kipi kapalı, Godot fizik düğümü yok.

---

## 3. Arayüzler

Alan listesi ve imza düzeyindedir; sözleşmeler ayrı işte dondurulur (iskelet §3).

### 3.1 A2 taşıması, el sıkışma, sürüm, yeniden bağlanma

- **Durum kanalı:** yerel TCP, yalnız `127.0.0.1`'e bağlanır; iki işletim sisteminde aynı kod (R12 §2.2, §3 md.2). Unix domain soket ve adlandırılmış boru, ölçülmüş bir kazanç olmadan seçilmez (R12 §2.2). Çerçeve: `{tür u16, sürüm u16, uzunluk u32, seq u32}` + açık serileştirilmiş yük, little-endian.
- **Kare kanalı:** paylaşımlı bellek halkası (3.4); uyandırma TCP üzerindeki `FrameReady` ile yapılır, yoklama yok.
- **El sıkışma:**
  1. Görselleştirici → `Hello {a2_sürümü (major.minor), godot_sürümü, eklenti_sürümü, platform, renderer, sürücü, yetenekler}`.
  2. Sim → `Welcome {oturum_id, sim_kipi, kalite_kademesi, a7_manifest_özeti, WaveSpectrumSet, ortam, kamera_listesi[], halka_adı, halka_nesli}`.
  3. Görselleştirici sahneyi yükler, manifest ve dalga özetini hesaplar, halkayı eşler → `Ready {a7_manifest_özeti, dalga_özeti, halka_eşlendi}`.
  4. Sim özetleri karşılaştırır; uyuşmazlıkta `Reject {neden}` ve bağlantı kapanır.
- **Sürüm:** major farkı reddedilir; minor farkında ortak yetenek kümesi kullanılır.
- **Yeniden bağlanma:** bağlantı koparsa sim beklemez (3.5). Görselleştirici 1 s aralıkla yeniden bağlanmayı dener (mimari varsayımı). Yeni oturumda tam durum yeniden gönderilir: dalga seti, ortam, son `VizState`, yaşayan gövdelerin listesi; halka yeni `halka_nesli` ile sıfırlanır, eski nesildeki yuvalar geçersizdir.

### 3.2 Sim → görselleştirici

| İleti | Alanlar | Sıklık |
|---|---|---|
| `VizState` | `t_ns`, `adım_no`; gövde başına `{id, tür, kütle_durumu, süreklilik_no, p_n, q_nb (w,x,y,z), v_n, ω_b}`; fırlatıcı başına `{dolu, emniyet}`; torpido itki durumu; `dalga_listesi_sürümü`; `kip`, `duraklatıldı`, `geçerli` | 60 Hz sim zamanında (B04 §2.11) + her olay adımında |
| `WaveSpectrumSet` | B04 §3'teki alanlar | oturum başında ve değişince |
| `Environment` | ortam türü (havuz/açık su), ortalama su seviyesi, güneş yönü ve ışınımı, ışık geçersiz kılmaları, `su_optiği` (2.5), `sürüm` | oturum başında ve değişince |
| `SceneEvent` | `t_ns`, `adım_no`, `tür` ve yükü: `GÖVDE_DOĞDU {id, tür, kaynak_fırlatıcı}` (torpidonun ayrılması), `GÖVDE_ÖLDÜ {id, neden}`, `KÜTLE_DURUMU {id, eski, yeni}`, `İSABET {torpido_id, hedef_id, nokta_n, normal_n, bağıl_hız_n}` (B04 §2.8), `SIFIRLAMA`, `DURAKLAT/DEVAM`, `KADEME`, `ATLAMA` (tekrar oynatma) | olay oldukça |
| `CameraRequest` | `istek_id`, `kamera_id`, `kare_seq`, `t_meas_ns`, `pozlama_ns`, `t_teslim_ns`, `son_tarih_duvar_ns` (HIL'de), `gürültü_tohumu`, `t_meas` anındaki bütün gövde durumları (`VizState` gövde listesiyle aynı biçim), `kamera_yapılandırma_sürümü` | kamera hızında (B05) |
| `Overlay` | gösterge listesi: `{kimlik, tür, çerçeve, konum/yön, etiket, ömür}`; kaynak B08'in telemetrisi | ≤ 10 Hz (mimari varsayımı) |

`CameraConfig` (Welcome içinde): `kamera_id`, çözünürlük, hız, `fx, fy, cx, cy`, bozulma, montaj (`l`, `q_bs`), `calib_ver`, kademe.

**Durum akışının veri hızı** (hesap): gövde başına `4 + 1 + 1 + 2 + 24 + 32 + 24 + 24 = 112 B`; 16 gövde + 64 B başlık ≈ 1,86 KB/ileti; 60 Hz'te ≈ 0,11 MB/s. `CameraRequest` aynı gövde listesiyle 30 Hz'te ≈ 0,06 MB/s. Durum kanalı kamera kanalının (36,9 MB/s) yaklaşık %0,5'idir; bu yüzden durum için soket yeterlidir. Gecikmesi ölçülür (7. başlık).

### 3.3 Görselleştirici → sim

| İleti | Alanlar |
|---|---|
| `FrameReady` | `istek_id`, `kamera_id`, `kare_seq`, `yuva`, `halka_nesli` |
| `FrameDropped` | `istek_id`, `kamera_id`, `kare_seq`, `neden` (`SON_TARİH`, `DAHA_YENİ_İSTEK`, `GÖRÜNÜRLÜK`, `HALKA_DOLU`, `GERİ_OKUMA_HATASI`, `SAHNE_HAZIR_DEĞİL`) |
| `ViewerStatus` (1 Hz) | çizim hızı, `çizilebilir`, elle çizim sayacı, `frame_queue_size`, operatör gecikme sayacı, son uygulanan `t_ns`, GPU belleği (varsa) |

Görselleştirici dünyayı değiştiren ileti göndermez.

### 3.4 Kare halkası (paylaşımlı bellek)

- **Ad ve sahiplik:** sim oluşturur; ad `/ida-cam-<oturum 8 hex>` (17 karakter; macOS POSIX sınırı 31; R12 §2.2). Sim başlarken kendi önekiyle kalmış nesneleri siler (macOS); Windows'ta eşleme son tutamaç kapanınca kalkar. Görselleştirici yazar, algılama bileşeni ve kayıt (B08) salt okur.
- **Başlık:** `sihirli_sayı`, `şema_sürümü`, `halka_nesli`, `kamera_sayısı`, kamera başına `yuva_sayısı` (varsayılan 4; mimari varsayımı: 3 kuyruk + 1 okunan), `yuva_boyutu`.
- **Yuva:** atomik `durum` (`BOŞ → YAZILIYOR → HAZIR → OKUNUYOR → BOŞ`; her geçişin tek yazarı vardır, sahiplik alanla devredilir), `okuyucu_sayısı`, **kare üst verisi** ve piksel verisi.
- **Kare üst verisi** (B05 §2.5'in istediği alanlar, bu bölüm doldurur): `kamera_id`, `kare_seq`, `istek_id`, `t_meas_ns`, `pozlama_ns`, `t_teslim_ns` (istekten yankı), genişlik, yükseklik, `satır_adımı`, piksel biçimi (`RGBA8_SRGB`), iç parametreler, bozulma, montaj, `calib_ver`, kalite kademesi, `çizilebilir`, `çizim_yolu` (`ELLE`/`OLAĞAN`), `istek_alındı_duvar_ns`, `çizim_duvar_ns`, `hazır_duvar_ns`, `düşürüldü` ve nedeni.
- Görselleştirici yalnız `BOŞ` yuvaya yazar; boş yuva yoksa kare `HALKA_DOLU` nedeniyle düşer, okunan yuvanın üzerine yazılmaz.

### 3.5 Kamera algılama bileşeni ↔ sim

```
sim → camdet:  FrameAvailable {halka_nesli, yuva, kamera_id, kare_seq, t_meas_ns, son_tarih_duvar_ns}
camdet → sim:  DetectionBatch {kamera_id, kare_seq, t_meas_ns (yankı), hazır_duvar_ns,
                               n ≤ 8, ida_detection_t[n]}          // B03 §3.2
               SlotRelease   {halka_nesli, yuva}

class ICameraDetector {                          // camdet içindeki algoritma eklentisi
  DetectorInfo info() const;                     // producer_id, producer_ver
  void configure(const CameraConfig& cam, const DetectorParams& p);
  void process(const FrameView& f, DetectionSink& out);   // f: salt okunur piksel + üst veri; poz yok
};
```

Taşıma A2 ile aynı kitaplık, ayrı TCP bağlantısıdır. İskelet bu bağlantıyı adlandırmaz (8. başlık).

### 3.6 GDExtension sınıfları

```
IdaVizClient      : Node            connect(host, port); poll(); state_at(t_ns); events()
IdaSceneLoader    : RefCounted      load(glb_yolu, manifest_yolu) -> Ok | ManifestMismatch | LoadError
IdaGltfExtension  : GLTFDocumentExtension   IDA_materials okur; düğüm öneklerini ayırır
IdaWorldApplier   : Node            apply(const SceneState&)    // 2.3 dönüşümü, süreklilik kuralı
IdaWaveSurface    : MeshInstance3D  set_spectrum(const WaveSpectrumSet&); set_time(int64 t_ns)
IdaCameraSensor   : Node            configure(const CameraConfig&); render(const CameraRequest&)
IdaUnderwaterFx   : CompositorEffect set_params(const WaterOptics&)
IdaFrameRing      : RefCounted      open(ad, nesil); acquire(kamera_id) -> yuva; commit(yuva, üst_veri)
```

### 3.7 Komşu bölümlerden beklenenler

- **B04:** `VizState`'e `v_n`, `ω_b`, `süreklilik_no` ve torpido itki durumu eklenmesi; olay adımında takvim dışı `VizState`; `SceneEvent` türlerinin olay kaynağı; dalga faz konvansiyonunun yazılı tanımı; `IEnvironmentView::surface_elevation`'ın test noktası sorgusu için kullanılabilmesi (B04 §3'te var).
- **B05:** `CameraRequest`'in üretimi (`t_meas`, `t_teslim`, `pozlama`, gürültü tohumu); halkadaki karenin `t_teslim` adımında teslimi ya da düşürülmesi; sade kademe hızında (15 Hz) zamanlama; algılama kaydının `t_teslim + L_alg` teslim yolu; manifest özeti karşılaştırması (B05 §2.4.1).
- **B06:** `IDA_DETECTION`'ın B03 §3.2 alanlarıyla taşınması; kamera algılamasının A1 bütçesi.
- **B08:** süreçlerin başlatılması; A5'te `kamera[]`, `su_optiği`, `kamera_gerekli`, `L_alg`, görselleştirme ayarları; A6'ya düşen kare, geç algılama ve görünürlük olaylarının yazılması; A3'te uyarılar; isteğe bağlı kare kaydı.

---

## 4. Hata durumları ve davranışı

| Durum | Tespit | Davranış |
|---|---|---|
| Sim sunucusu yok ya da bağlantı koptu | TCP | Görselleştirici son sahneyi dondurur, "bağlantı yok" gösterir, 1 s aralıkla yeniden dener; kare üretmez, hiçbir şeyi simüle etmez |
| Görselleştirici çöktü ya da kapandı | sim: TCP kapandı, `ViewerStatus` kesildi | Sim **beklemez**; fizik, HIL ve diğer sensörler sürer. HIL'de kamera istekleri `görselleştirici_yok` nedeniyle düşen kare sayılır; `kamera_gerekli` ise çalıştırma geçersiz. SIL'de sim duraklar ve yeniden bağlanmayı bekler; 60 s (duvar saati; mimari varsayımı) içinde bağlanmazsa çalıştırma "kamera kaynağı yok" nedeniyle sonlanır |
| Algılama bileşeni çöktü | TCP | Kareler üretilmeye devam eder; algılama kaydı gelmez, `algılama_geç` sayılır; `kamera_gerekli` ise HIL çalıştırması geçersiz. Atış kontrolü kamera kaydı olmadan çalışır (B03 §5) |
| A2 major sürüm farkı | el sıkışma | `Reject`; kamera kullanılamaz |
| A7 manifest özeti uyuşmuyor | el sıkışma | Sahne kurulmaz, `Reject`; kamera kullanılamaz, B05 sonarı etkinleştirmez (B05 §4) |
| Dalga listesi sürümü ya da özeti uyuşmuyor | el sıkışma ya da `VizState` | Görüntü "tutarsız" işaretlenir; bu sürede üretilen kareler geçersiz (B04 §4) |
| Pencere çizilemez oldu | `çizilebilir` bayrağı | Birincilde elle çizim sürer, kare `çizim_yolu = ELLE` işaretlenir. Yedekte kare düşer (`GÖRÜNÜRLÜK`), HUD ve A3 uyarısı |
| Kare son tarihe yetişmedi | görselleştirici ya da sim | 2.4'teki tablo |
| Geri okuma geri çağrısı gelmedi | istek başına zaman aşımı (`son_tarih` + 1 kamera periyodu) | `GERİ_OKUMA_HATASI` ile düşer; ardışık 30 hatada (mimari varsayımı) kamera "sağlıksız" bildirilir |
| Halka dolu | yazar | `HALKA_DOLU` ile düşer; okunan yuvanın üzerine yazılmaz |
| Kalmış paylaşımlı bellek nesnesi | sim başlangıcı | Önekle eşleşen eski nesneler silinir; yeni nesil numarası |
| Metal donması ya da uzun shader derlemesi (#119436, #123481) | çizim hızı sıfıra düşer; `ViewerStatus` kesilir | Çökme gibi işlenir; shader önbelleği ilk açılışta ısıtılır ve süresi ayrı kaydedilir (M29) |
| Veri gecikmesi (operatör) | son durum `t_op`'un gerisinde | Son durumda beklenir, ekstrapolasyon yok; sayaç |
| NaN ya da birim dışı kuaterniyon | uygulayıcı | Gövde son geçerli pozda kalır, olay; kamera karesi geçersiz |

Fizik sunucusunun çalışmaya devam etmesi, kamera kullanan deneyin geçerli olduğu anlamına gelmez; geçerlilik düşen kare oranı, görünürlük olayları ve algılama gecikmesiyle ayrıca değerlendirilir (12.17).

---

## 5. Kalite kademelerinin etkisi

Kademe A5'ten seçilir (iskelet §6, Ö11). Fizik, kamera dışı sensörler, otopilot ve HIL her kademede tam çalışır.

| Kademe | 3B görünüm | Kamera sensörü | Algılama bileşeni |
|---|---|---|---|
| Kapalı | Görselleştirici çalışmaz | Kare üretilmez (B05 §5) | Çalışmaz; atış kontrol yalnız sonarla (B03 §5) |
| Sade | Düz su (ortalama yüzey düzlemi), basit ışık, gölge basit; dalga görseli yok | 320 × 240, 15 Hz (mimari varsayımı); optik yalnız zayıflama + geri saçılım; bulanıklık, sensör gürültüsü ve kostik yok | Çalışır |
| Tam | Dalga yüzeyi (2.7), su altı optiği, gölgeler | 640 × 480, 30 Hz (mimari varsayımı; A24 açık); zayıflama, geri saçılım, bulanıklık, sensör modeli | Çalışır |

- **Sade kademede dalga varsa** kamera görüntüsü su yüzeyine yakın bölgede fizikle tutarsızdır (görüntüde düz su, fizikte dalga). Kare üst verisi `dalga_görseli = yok` taşır; bu karelerle yapılan algılama değerlendirmesi buna göre yorumlanır (8. başlık).
- **Görselleştirici kapalıyken** sim, HIL, sonar, web panelleri ve kayıt tam çalışır; A2 dinlenmez.
- **Düşük donanımlı Windows bilgisayarı** (tahmin; hiçbiri ölçülmedi). Taban donanım tahmini 4 çekirdek, 8 GB bellek, DirectX 12 destekli dahili grafiktir (Ö11). Beklenti: sade kademede tek kamera 15 Hz ile operatör görünümünün birlikte koşabileceği; tam kademenin yetmeyebileceği. Görselleştirici sim sunucusundan düşük işletim sistemi önceliğiyle koşar; sim gerçek zamanı kaçırırsa bunu B04 sayar. Kamera kareleri yetişmezse düşer ve sayılır; `kamera_gerekli` HIL çalıştırması geçersiz olur. Tabanı M12, M30 ve M20 belirler.
- **Kayıt kademesi** (B08): "tam"da kareler halkadan kayda alınabilir; 640 × 480 × 30 Hz için sıkıştırılmamış veri 36,9 MB/s'tir (hesap). Sıkıştırma B08'in kararıdır.

---

## 6. Ölçüme bağlı kararlar

| Karar | Birincil | Yedek | Ölçüm |
|---|---|---|---|
| Kamera sensörü (iskelet §8) | Görünürlükten bağımsız çizim: `force_draw` ile elle kamera geçişi; App Nap ve güç kısıtlaması kapatılır | Pencere çizilebilir tutulur; kamera yalnız `çizilebilir` iken geçerli; deney kapsamı buna göre kabul edilir (A23) | M13, M28 |
| Tek sahnede iki zaman | Sıralı uygulama, ayrık çizim; kamerada zamansal etkiler kapalı | Ortak yakalama zamanı (tek çizim, `D_op = 0`) | M28 kapsamında (8. başlık), M12 |
| Geri okuma | `texture_get_data_async`, `frame_queue_size` en küçük kararlı değer, döngü 120 Hz üst sınırlı | Eşzamanlı `get_image()` (kuyruk yok, GPU bekler); gizli pencerede kuyruk ilerlemiyorsa yalnız o durumda | M30, M12 |
| Durum taşıması | TCP loopback | Paylaşımlı bellek, sürüm sayaçlı poz tamponu (R12 §3 md.2) | Poz akışı gecikmesi (7. başlık; yeni ölçüm önerisi, 8. başlık) |
| Dalga görseli | Köşe gölgelendiricisinde bütün bileşenlerin doğrudan toplamı | Çekirdeğin bileşenlerinden FFT ızgarası, sınırı yeniden hesaplanır | M32, M15 |
| Su altı optiği | `CompositorEffect` (Forward+) | Yerleşik ortam sisi ve hacimsel sis (yalnız zayıflama yaklaşımı; R12 §2.5) | Kare süresi artışı (R12 §6), M12 |
| Uzantı yapısı | CMake | SCons | M33 |
| Godot sürümü | 4.7.2 | Bir sonraki kararlı yama sürümü; 4.6.x'e dönüş otomatik çözüm sayılmaz (12.9) | M29 |
| Kalite tabanı | Sade kademe (iskelet §6) | Kamera kapatılır | M12, M20, M21, M30 |

Ölçüme bağlı yedekler kalite kademesi değildir; ölçüm sonucunda tasarım yedeğe geçer ve bütün kademeler için geçerli olur (iskelet §6).

---

## 7. Test yaklaşımı ve kabul ölçütleri

**Kabul ölçütü görüntünün güzel görünmesi değildir.** Her ölçüt sayıyla ya da evet/hayır tablosuyla kaydedilir; renk kartı benzerliği tek başına yetmez (00 Bölüm 6: bulanıklık, kontrast, yüzey kırılması, pozlama, hareket, yanlış pozitifler ve zamansal davranış). Eşikler aksi belirtilmedikçe hedeftir, ölçüm değildir (R12 §6); B09'da kesinleşir. Her test iki işletim sisteminde koşar; donanım ve sürücü sürümleri kayda yazılır.

**İki haftalık prototipin ölçütleri** (R12 §6 planının bu bölüme uyarlanmışı):

| # | Test | Ölçüt |
|---|---|---|
| P1 | Uzantı derlemesi ve yüklenmesi (CMake, gerekirse SCons) | İki işletim sisteminde yüklenir; yapılandırma kaydedilir (M33) |
| P2 | Poz akışı: sahte sim, 60 Hz `VizState`, 16 gövde, 1 saat | Yayından uygulamaya gecikme p50/p99 ölçülür ve raporlanır; hedef p99 < 16,7 ms. Kayıp ya da yinelenen ileti = 0 |
| P3 | Ara değerleme ve olay sınırları | Operatör zamanı hatası < 1 fizik adımı (1,25 ms). Fırlatmada torpido ilk durumundan önce görünmez, isabetten sonra kaybolur; kütle durumu değişiminde sınırı aşan eğri kurulmaz (yer gerçeğiyle karşılaştırma) |
| P4 | Kamera hattı: 640 × 480 RGBA8, 30 Hz, 30 dk | Düşen kare < %1. İstekten hazıra gecikme p50/p99 duvar saatinde, gerçek çizim hızı ve `frame_queue_size` 1–3 için ayrı ayrı. Kopya sayısı ve CPU süresi (M30) |
| P5 | **Görünmeyen pencere (ilk kabul kapısı; M28, M13).** macOS: tamamen örtülü, gizli, simge durumunda; Windows: simge durumunda ve örtülü; iki işletim sisteminde başka uygulama tam ekranda, ekran koruyucu, ekran kapalı | Her durumda en az 30 dk: istek sayısı, üretilen kare sayısı, düşen kare ve nedeni; teslim gecikmesi p50/p99; **kare içeriği**: aynı `t_meas` durumunun görünür pencerede çizilmiş referansıyla ortalama mutlak piksel farkı ≤ 2/255 (mimari varsayımı); hareketli hedefin kare içindeki konumu yer gerçeği izdüşümüyle ≤ 1 piksel; ışık, gölge ve su efektlerinin varlığı. Tek kare kanıt sayılmaz. Sonuç durum × işletim sistemi evet/hayır tablosu |
| P6 | İki zamanlı çizim | Operatör görünümü `t_op`'u, kamera `t_meas`'ı çizerken, adım başına 1 pikselden fazla yer değiştiren hareketli hedef kamera karesinde `t_meas`'taki konumunda (≤ 1 piksel); operatör durumu kamera karesine sızmaz. Birincil ve yedek için kare süresi karşılaştırılır |
| P7 | Dalga tutarlılığı | En az 25 test noktasında GPU'dan geri okunan yükseklik ile `IEnvironmentView::surface_elevation` farkı ≤ max(1 mm, %1 `H_s`) (mimari varsayımı), hem `t_meas` hem `t_op` geçişinde kendi zamanında; noktaların bir kısmı köşelerin arasında |
| P8 | Su altı optiği | Kare süresi artışı ölçülür. Bilinen `β`'larla sentetik sahnede menzile göre kanal yoğunluğu formülle %2 içinde; açık veri kümesine uydurulan parametreler ve uydurma hatası kaydedilir |
| P9 | A7 hattı | `cgltf` ve `GLTFDocumentExtension` aynı malzeme tablosunu okur; manifest özeti uyuşmazlığı reddedilir; işaret düğümü Godot ile sim arasında ≤ 1 mm; `BUOY_` ve `COL_` denetimleri |
| P10 | Algılama bileşeni arayüzü | `t_capture` ile karenin `t_meas`'ı bit düzeyinde eşit; `calib_ver` ve `producer_*` taşınır; HIL'de geç sonuç düşer ve sayılır; bileşen arayüzünde poz ve yer gerçeği alanı bulunmaz |
| P11 | Uzun koşu (M29) | macOS'ta 2 saat kesintisiz, "timeout waiting for fence" yok; ilk açılıştaki shader derleme süresi ayrı kaydedilir |
| P12 | Çökme ve yeniden bağlanma | HIL 1x'te görselleştirici öldürülünce B04'ün `kaçırılan_adım` sayacı değişmez; kamera düşen kare sayacı artar; yeniden başlatılınca 5 s içinde (mimari varsayımı) kare akışı yeniden başlar ve eski nesil yuvalar kullanılmaz |
| P13 | Kademeler | Kapalı kademede bütün B04, B05, B06 testleri görselleştiricisiz geçer; sade ve tam kademelerde P4 ve P5 tekrarlanır |

**Sürekli testler** (otomatik derleme ve test hattında): A2 serileştirme gidiş-dönüş testi; çerçeve dönüşümü birim testleri (`det C = +1`, bilinen pozlar); süreklilik kuralı birim testleri; halka yuva durum makinesi testi (iki süreç); manifest üretiminin aynı girdide aynı özeti verdiği (CoACD çıktısı hariç; 8. başlık).

---

## 8. Açık noktalar ve iskelete öneriler

**Komşu bölümlerle ve kurallarla uyumsuzluklar** (bölüm iskelete uydu):

1. **Varlık çıktısının depoya girmesi:** R12 §4 R8, CoACD çıktısının tekrar üretilemeyebileceği için depoda saklanmasını önerir; `AGENTS.md` "derleme çıktıları depoya girmez" der. Öneri: dışbükey parçalar ve manifest, üretim ayarlarıyla birlikte sürümlü **kaynak varlık** olarak `assets/` altında tutulsun; mimarın kararı gerekir.
2. **B04 `VizState`** hız, `süreklilik_no` ve olay adımı durumu içermiyor; torpido B04 sürüm 0.2'de hâlâ itkisiz, iskelet 0.5'te itkili. Görüntü için itki durumu gerekir (3.7).
3. **Akustik malzeme değerlerinin yeri:** B05 §2.4.1 değerleri A7 malzeme kaydında, B05 §3 ise A5 tablosunda gösteriyor. Bu bölüm "A7 varsayılan, A5 aynı kimlikle geçersiz kılar" kuralını önerir; B05 ile eşlenmeli.
4. **B05 zamanlama tablosu** kamera için 30 Hz'i sabit yazıyor; sade kademede 15 Hz için 53/54 adım dizisi gerekir. B05 hızı bu bölüme bıraktığı için çelişki değil, tablo notu gerekir.
5. **B06 `IDA_DETECTION` alanları** B03 §3.2 ile uyuşmuyor (B03 §8.1 md.6'da bildirilmiş); bu bölüm B03'ün biçimini kullanır.
6. **Sade kademede düz su ile fizikteki dalga** kamera görüntüsünde tutarsızlık yaratır (5. başlık). İskelet §6 "Sade: düz su" der; bölüm buna uydu.

**İskelete öneriler:**

1. **A2 satırı:** içerik "pozlar ve hızlar, dalga bileşenleri, sim zamanı, ışık ve su parametreleri, sahne olayları (gövde doğumu ve ölümü, kütle durumu, isabet), kamera istekleri; el sıkışmada A2 sürümü ve A7 manifest özeti" olarak genişletilsin.
2. **Kamera algılama bileşeni ↔ sim** bağlantısı (3.5) iskelette adsız; A2'nin alt kanalı ya da ayrı bir `A8` olarak adlandırılsın.
3. **§8 kamera satırına** "tek sahnede iki zaman: ayrık çizim / ortak yakalama zamanı" kararı eklensin ya da M28'in kapsamına alınsın.
4. **Yeni ölçüm önerisi (00 Bölüm 7):** "A2 poz akışı gecikmesi ve kaybı" (P2); M12 ile M30 kamera hattını kapsıyor, durum akışını kapsamıyor.
5. **§6 sade kademesi** için seçenek: dalga varsa sade kademede de kaba ağla, kesmesiz doğrudan toplam. Maliyeti ölçülmeden önerilmez; M12 ile birlikte değerlendirilsin.
6. **B03 §8.3'ün bağlantısı:** iskelet §5'e göre yakın bölgede hedef konumu kamera algılamasıyla sürdürülür. Ö5-k yedeğe düşerse kör bölgedeki atış yeteneği pencere görünürlüğüne bağlı kalır; bu iskelete not edilsin.

**Açık noktalar:**

- A23 (pencerenin her zaman görünür tutulması kabul mü), A24 (kamera çözünürlüğü, hızı, sayısı), A26 (macOS sürümü), A27 (iki pencere mi, LibGodot ile gömme mi; LibGodot kütüphane desteği 4.6'dan beri var, mevcut yüzeye gömme ayrı prototip ister: 12.5, Ç15) kullanıcı yanıtı bekliyor.
- A16: Blender dışa aktarma betiği Blender'ın içinde Python'la çalışır; yapı zamanında Python kabul edilmezse A7 hattının dışa aktarma adımı yeniden düşünülmeli.
- Hareket bulanıklığı, kayan perde, araç lambası (R10 §2.5.2 K2), kostik ve parçacıklar ilk sürümde yok.
- Kameranın yarı batık olduğu durumda piksel başına su altı/su üstü maskesi sonraki kademe.
- `force_draw(false, dt)` ile olağan çizimin aynı Godot karesinde art arda kullanılmasının iş parçacıklı render kipinde güvenli olup olmadığı doğrulanmadı; P5 ve P6 bunu da sınar.
- Gizli pencerede asenkron geri okuma kuyruğunun ilerleyip ilerlemediği bilinmiyor; ilerlemiyorsa eşzamanlı okuma yalnız o durumda kullanılır (6. başlık).
- Algılama değerlendirmesi için kamera görüntüsüyle hizalı yer gerçeği maskesi (hedef kimliği) yalnız A3/A6 için düşünülebilir; algılama bileşenine hiçbir koşulda verilmez. Kapsama alınması B08 ile birlikte kararlaştırılmalı.
