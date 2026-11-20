# A2 — Sim sunucusu ↔ görselleştirici sözleşmesi

| | |
|---|---|
| Sürüm | 1.0-aday (2026-11-19) |
| Görev | `docs/gorevler/W1-A2A7.md`; ortak kurallar `docs/gorevler/W1-ortak.md` |
| Makinece okunur tanım | `sim/include/ida/a2_wire.h` (bayt düzeni; bu belgeyle çelişemez) |
| İlgili sözleşmeler | A7 (`docs/sozlesmeler/A7-sahne.md`), A8 (kamera algılama), A5 (senaryo), A6 (kayıt), A3 (web panelleri) |
| Dayanak | İskelet 0.10 (`docs/mimari/00-iskelet.md`) §2, §3 (A2, A7, A8), §3.1, §4, §6, §8, §10 md.18–21, 25; ölçüm `docs/olcumler/MP6-godot-kamera.md`; bölüm 07 (taslak; iskeletle çeliştiği yerde iskelet geçerlidir) |

Atıf biçimi: `A2-Knn` bu belgenin kuralı; `A7-Knn` A7 sözleşmesinin kuralı; `MP6 §n` ölçüm raporu başlığı; `07 §n`, `04 §n`, `05 §n` taslak mimari bölüm başlığı (gerekçe için; bağlayıcı değildir); `AB-nnn` açık bulgu.

Değerlerin niteliği: bu sözleşmedeki süre, hız ve boyut değerlerinin çoğu **mimari seçimi (tahmin)** olarak etiketlidir. A2 hattının kendisi (soket ve paylaşımlı bellek) hiç ölçülmedi (iskelet §3 A2 satırı). Ölçülmüş tek büyüklükler MP6'nın Mac ön turundaki kamera çizimi ve geri okuma süreleridir; onlar da A2 hattı olmadan, GDScript ile ölçüldü.

---

## 1. Kapsam ve taraflar

**Taraflar.**

- **Sim sunucusu (sunucu, C++):** dünyanın tek yetkili durumunu tutar (iskelet §2). A2 dinleyicisini açar, oturumu yönetir, durum akışını ve kamera isteklerini üretir, kamera kare halkasını oluşturur ve yuvaların sahibidir.
- **Görselleştirici (istemci; Godot 4.7.2 + C++ GDExtension, ayrı süreç):** fizik yapmaz, dünyayı değiştiren hiçbir ileti göndermez. Operatörün 3B görünümünü çizer; kamera isteklerini çizer, geri okur ve halkaya yazar.
- **Kare tüketicileri (yalnız okur):** kamera algılama bileşeni (A8, ayrı süreç) ve sim içindeki kayıt yazıcısı (A6). Bunların halkayla ilişkisi bu sözleşmedeki kare biçimine ve yuva kurallarına tabidir; A8'in kendi soket iletileri A8'dedir.

**Bu sözleşmenin tanımladıkları.** Taşıma ve adlandırma; el sıkışma (A2 sürümü, A7 manifest özeti, kalite kademeleri); durum akışı (pozlar ve hızlar, dalga bileşenleri, sim zamanı, ışık ve su parametreleri); sahne olayları (gövde doğumu ve ölümü, kütle durumu, isabet); kamera isteği, kare teslimi ve kamera karesinin biçimi (A8 buna atıf verir); iki zamanlı çizim gereği; görselleştiricinin çökmesi, gecikmesi ya da hiç çalışmaması durumunda iki tarafın davranışı.

**Kapsam dışı.** Sahne ve varlık dosyalarının biçimi (A7); senaryo dosyasındaki kamera, su optiği ve görselleştirme ayarlarının yazımı (A5); kayıt dosyasının kanalları (A6); panellere giden sayaç ve uyarıların biçimi (A3); algılama kaydı ve algılayıcının soket iletileri (A8); Godot sahnesinin iç tasarımı, gölgelendiriciler, su altı optiğinin gerçekleştirimi, dalga ağının ayrıntı düzeyi (Godot sahnesi uygulama işi). Sahne içi göstergeler (sonar tarama yönü, iz çizgileri) bu sürümde A2'de taşınmaz; bkz. §8.

---

## 2. Terimler ve kabuller

Birimler, çerçeveler, kuaterniyon kabulü, ölçüm zamanı ile alım zamanının ayrılığı ve aralık ölçümlerinin zaman damgası iskelet §3.1'dedir; burada yinelenmez. Bu sözleşmeye özgü olanlar:

| Terim | Anlamı |
|---|---|
| Oturum | Bir TCP bağlantısının ömrü. Her kabul edilen bağlantı yeni bir `oturum_kimligi` (rastgele 64 bit) alır |
| Duvar saati (`*_duvar_ns`) | Aynı makinedeki bütün süreçlerde ortak tekdüze saat: macOS'ta `clock_gettime_nsec_np(CLOCK_UPTIME_RAW)` (Apple Libc `clock_gettime(3)`: değer `mach_absolute_time()` ile aynıdır, uykuda ilerlemez; Godot'nun `Time.get_ticks_usec` kaynağı da `mach_absolute_time`'dır, MP6 §7); Windows'ta `QueryPerformanceCounter` değerinin `QueryPerformanceFrequency` ile nanosaniyeye çevrilmiş hali (Microsoft, "Acquiring high-resolution time stamps": değerler farklı iş parçacıkları ve süreçler arasında tutarlıdır, frekans açılışta sabitlenir). Sim zamanıyla karışmaz (iskelet §4) |
| `t_cizim` | Kamera karesinin çizildiği sim zamanı: pozlama aralığının ortası; her zaman bir fizik adımı sınırıdır |
| `t_olcum` | Kamera ölçümünün ölçüm zamanı: pozlama aralığının **sonu**, `t_olcum = t_cizim + pozlama/2` (iskelet §3.1: aralık ölçümleri aralığın sonunu ve süresini taşır) |
| `t_teslim` | Karenin modellenmiş alım anı (sim zamanı); kamera sensör modelinin (05) ürettiği değer; A2 yalnız taşır ve teslim son tarihi olarak kullanır |
| Teslim adımı `S_d` | `t_teslim`'e eşit ya da ondan büyük ilk fizik adımı sınırı (adım 1 250 000 ns, iskelet §4) |
| Operatör zamanı `t_op` | Operatör görünümünün çizdiği sim zamanı (§4.4) |
| Süreklilik numarası | Gövde başına sayaç; durumun ara değerlenemeyecek biçimde sıçradığı her olayda artar (§4.3) |
| Halka | Bir kamera için sim'in oluşturduğu paylaşımlı bellek nesnesi; `yuva_sayisi` kadar kare yuvası içerir |
| Halka nesli | Halkanın oturum içindeki kimliği; her yeni halka nesnesinde artar |
| Yuva sahipliği | Bir yuvanın kimin elinde olduğu bilgisi yalnız sim'in belleğinde tutulur; paylaşımlı bellekte sahiplik alanı yoktur (§4.6) |

**Çerçeve dönüşümü (görselleştiricinin içi, uygunluk testleri için bağlayıcı).** Godot sağ elli, +Y yukarı, −Z ileri kabul eder (Godot 4.7.2 `Vector3.FORWARD = (0, 0, −1)`). NED'den Godot dünyasına ve gövde çerçevesinden Godot yerel çerçevesine aynı matris uygulanır:

```
C = [[0, 1, 0], [0, 0, −1], [−1, 0, 0]]     det C = +1
x_G = E,  y_G = −D,  z_G = −N               konum_G = C · p_n
taban_G = C · R(q_nb) · Cᵀ
```

A7 varlıklarının glTF → Godot yerel dönüşümü A7-K14'tedir.

---

## 3. Veri tanımları

Bütün çok baytlı alanlar küçük sonludur; `double` IEEE 754 ikili64, `float` kullanılmaz. Yapıların bayt düzeni, boyutu ve alan ofsetleri `sim/include/ida/a2_wire.h` içindedir ve `static_assert` ile sabitlenmiştir; aşağıdaki tablolar anlamı, birimi, geçerli aralığı ve geçersiz değerdeki davranışı verir. "Zorunlu" sütunu, alanın her iletide anlamlı bir değer taşıyıp taşımadığını söyler; dolgu ve ayrılmış alanlar sıfırdır ve alıcı bunları denetlemez.

### 3.1 İleti başlığı (`Baslik`, 16 B)

| Ofset | Alan | Tip | Anlamı | Geçerli | Geçersizse |
|---|---|---|---|---|---|
| 0 | `sihirli` | u32 | `0x32414449` ("IDA2") | sabit | protokol hatası (A2-K06) |
| 4 | `tur` | u16 | ileti türü (§3.2) | tablodaki değerler | protokol hatası |
| 6 | `bayraklar` | u16 | ayrılmış | 0 | protokol hatası |
| 8 | `yuk_bayt` | u32 | başlıktan sonraki bayt sayısı | türün beklenen boyutuna eşit, ≤ 1 048 576 | protokol hatası |
| 12 | `sira` | u32 | yön başına sıra numarası | A2-K07 | protokol hatası |

### 3.2 İleti türleri

| Tür | Kod | Yön | Yük | Beklenen `yuk_bayt` | Sıklık |
|---|---|---|---|---|---|
| `merhaba` | 0x0001 | G→S | `Merhaba` | 144 | oturumda bir kez, ilk ileti |
| `hosgeldin` | 0x0002 | S→G | `Hosgeldin` + `govde_sayisi × GovdeBilgisi` + `kamera_sayisi × KameraYapilandirmasi` + `kamera_sayisi × HalkaBilgisi` | 600 + 16·n + 184·c + 96·c | oturumda bir kez |
| `hazir` | 0x0003 | G→S | `Hazir` | 48 | oturumda bir kez |
| `ret` | 0x0004 | S→G | `Ret` | 128 | en çok bir kez, ardından bağlantı kapanır |
| `dalga_kumesi` | 0x0201 | S→G | `DalgaKumesi` + `bilesen_sayisi × DalgaBileseni` | 72 + 40·m | oturum başında ve küme değişince |
| `ortam` | 0x0202 | S→G | `Ortam` | 208 | oturum başında ve değişince |
| `malzeme_optik` | 0x0203 | S→G | `MalzemeOptik` + `girdi_sayisi × MalzemeOptikGirdisi` | 8 + 56·k | oturum başında ve değişince |
| `durum` | 0x0204 | S→G | `Durum` + `govde_sayisi × GovdeDurumu` | 32 + 120·n | A2-K20, A2-K21 |
| `sahne_olayi` | 0x0205 | S→G | `SahneOlayi` | 120 | olay oldukça |
| `kamera_istegi` | 0x0206 | S→G | `KameraIstegi` + `govde_sayisi × GovdeDurumu` | 96 + 120·n | kamera hızında |
| `kamera_iptal` | 0x0207 | S→G | `KameraIptal` | 16 | A2-K46, A2-K59, A2-K67, A2-K81 |
| `sim_nabzi` | 0x0208 | S→G | `SimNabzi` | 32 | 1 s'de bir (duvar saati) |
| `halka_yenile` | 0x0209 | S→G | `HalkaBilgisi` | 96 | halka emekliye ayrılınca (A2-K86) |
| `govde_tablosu` | 0x020A | S→G | `GovdeTablosu` + `govde_sayisi × GovdeBilgisi` | 24 + 16·n | her `atlama`dan hemen sonra (A2-K90) |
| `kare_hazir` | 0x0101 | G→S | `KareHazir` | 40 | her başarılı kamera isteği için bir kez |
| `kare_dustu` | 0x0102 | G→S | `KareDustu` | 24 | her başarısız kamera isteği için bir kez |
| `goruntuleyici_nabzi` | 0x0103 | G→S | `GoruntuleyiciNabzi` | 48 | 1 s'de bir (duvar saati) |
| `halka_eslendi` | 0x0104 | G→S | `HalkaEslendi` | 16 | her `halka_yenile`ye bir kez |
| `veda` | 0x00FF | iki yön | `Veda` | 8 | kapanıştan hemen önce |

Sınırlar (mimari seçimi): `n ≤ 64` gövde, `m ≤ 256` dalga bileşeni, `c ≤ 1` kamera (iskelet §10 md.25: tek kamera), `k ≤ 256` malzeme girdisi. Sınırı aşan sayı protokol hatasıdır.

### 3.3 El sıkışma yükleri

**`Merhaba`** (görselleştirici → sim)

| Alan | Tip | Anlamı | Geçerli | Geçersizse |
|---|---|---|---|---|
| `surum_ana`, `surum_alt` | u16 | görselleştiricinin A2 sürümü | ana = 1 | ana farklıysa `ret(surum)` (A2-K11) |
| `yetenekler` | u32 | bit 0: kamera çizebilir (A2-K38'deki yol mevcut) | diğer bitler 0 | bilinmeyen bit yok sayılır |
| `platform` | u8 | 1 macOS arm64, 2 Windows x64 | 1–2 | `ret(protokol)` |
| `cizici` | u8 | 1 Forward+ | 1 | `ret(protokol)` |
| `pencere_cizilebilir` | u8 | bağlanma anında pencere çizilebilir mi | 0/1 | 0 sayılır |
| `surec_kimligi` | u32 | işletim sistemi süreç kimliği (tanı için) | herhangi | — |
| `godot_surumu`, `uzanti_surumu`, `gpu_adi` | metin | tanı ve kayıt için | UTF-8, NUL ile biter | NUL yoksa `ret(protokol)` |

**`Hosgeldin`** (sim → görselleştirici)

| Alan | Tip | Birim | Anlamı | Geçerli |
|---|---|---|---|---|
| `oturum_kimligi` | u64 | — | bu bağlantının kimliği | ≠ 0 |
| `surum_ana`, `surum_alt` | u16 | — | oturumda kullanılacak sürüm (A2-K11) | ana = 1 |
| `sim_kipi` | u8 | — | 1 SIL, 2 HIL 1x, 3 HIL 1x + bariyer, 4 tekrar izleme | 1–4 |
| `kademe_3b` | u8 | — | 1 sade, 2 tam (kapalıda A2 çalışmaz, A2-K02) | 1–2 |
| `kademe_kamera` | u8 | — | 0 kapalı, 1 sade, 2 tam | 0–2; `kamera_sayisi = 0` ⇔ 0 |
| `kamera_sayisi` | u8 | — | yapılandırılmış kamera | 0–1 |
| `t_ns`, `adim_no` | i64, u64 | ns, — | gönderim anındaki sim zamanı ve adım | `t_ns = adim_no · adim_ns` |
| `adim_ns` | i64 | ns | fizik adımı | 1 250 000 |
| `durum_hiz_pay`, `durum_hiz_payda` | u32 | Hz | `durum` takviminin hızı | 60 / 1 |
| `manifest_sha256` | 32 B | — | sim'in yüklediği A7 manifestinin özeti (A7-K05) | — |
| `manifest_yolu` | metin (512 B) | — | manifest dosyasının mutlak yolu | UTF-8, NUL ile biter |
| `govde_sayisi` | u16 | — | ardından gelen yaşayan gövde sayısı | ≤ 64 |
| `calisma_nesli` | u32 | — | o anki çalıştırma nesli (A2-K80) | ≥ 1 |

Görselleştirici bu iletideki herhangi bir alanı geçersiz bulursa `hazir.durum = manifest_gecersiz` değil, `veda(protokol_hatasi)` gönderip bağlantıyı kapatır.

**`GovdeBilgisi`**: `govde_kimligi` (u32, oturum boyunca tekil, yeniden kullanılmaz), `sureklilik_no` (u32), `varlik_indeksi` (u16; A7 manifestindeki `varliklar[]` dizisinin indeksi; aralık dışıysa protokol hatası), `govde_turu` (u8: 1 araç, 2 torpido, 3 hedef), `dolu_tupler` (u8; bit k = tüp k+1 dolu; araç dışındaki gövdelerde 0).

**`KameraYapilandirmasi`** (§3.6) ve **`HalkaBilgisi`** (§3.7) kamera başına birer tane.

**`Hazir`** (görselleştirici → sim)

| Alan | Tip | Anlamı | Geçerli |
|---|---|---|---|
| `manifest_sha256` | 32 B | görselleştiricinin `manifest_yolu`ndaki dosyadan kendi hesapladığı özet | — |
| `dalga_kumesi_surumu`, `ortam_surumu` | u32 | uyguladığı son sürümler | — |
| `halka_eslendi_maskesi` | u32 | bit k: kamera k'nın halkası eşlendi ve başlığı doğrulandı (A2-K74) | — |
| `durum` | u32 | 0 başarılı; diğer kodlar `HazirDurumu` (§5) | 0–7 |

**`GovdeTablosu`** (sim → görselleştirici; A2-K90): `t_ns` (i64, ns; atlamadan sonraki yeni zaman, adım sınırı), `adim_no` (u64), `calisma_nesli` (u32; atlama olayındaki yeni nesil), `govde_sayisi` (u16, ≤ 64); ardından `govde_sayisi × GovdeBilgisi`. Geçersiz değer protokol hatasıdır.

**`HalkaEslendi`** (görselleştirici → sim; `halka_yenile` yanıtı): `kamera_kimligi`, `halka_nesli` (yeni halkanın nesli), `durum` (0 başarılı, 6 eşlenemedi, 7 başlık uyuşmuyor).

**`Ret`**: `neden` (1 sürüm, 2 manifest, 3 zaman aşımı, 4 meşgul, 5 protokol, 6 halka) ve UTF-8 açıklama. **`Veda`**: `neden` (0 olağan, 1 protokol hatası, 2 tampon taştı, 3 nabız zaman aşımı, 4 sim kapanıyor, 5 yeni çalıştırma (A2-K96)).

### 3.4 Durum akışı yükleri

**`Durum`** (başlık kısmı)

| Alan | Tip | Birim | Anlamı | Geçerli | Geçersizse |
|---|---|---|---|---|---|
| `t_ns` | i64 | ns | durumun sim zamanı (ölçüm zamanı); bir fizik adımı sınırı | ≥ önceki `durum.t_ns` (sıfırlama ve atlama dışında) | protokol hatası |
| `adim_no` | u64 | — | o adımın numarası | `t_ns = adim_no · adim_ns` | protokol hatası |
| `gonderim_duvar_ns` | i64 | ns | sim'in iletiyi kuyruğa koyduğu duvar saati | > 0 | yok sayılır (yalnız gecikme ölçümü için) |
| `dalga_kumesi_surumu` | u32 | — | bu durumun dalga kümesi | son `dalga_kumesi.surum` | görüntü tutarsız işaretlenir (A2-K28) |
| `govde_sayisi` | u16 | — | yaşayan gövde sayısı | ≤ 64 | protokol hatası |
| `calisma_durumu` | u8 | — | 1 çalışıyor, 2 duraklatıldı, 3 durdu | 1–3 | protokol hatası |
| `bayraklar` | u8 | — | bit 0: takvim dışı (süreklilik sınırı için) | diğer bitler 0 | protokol hatası |

**`GovdeDurumu`** (`durum` ve `kamera_istegi` içinde aynı biçim)

| Alan | Tip | Birim | Anlamı | Geçerli | Geçersizse |
|---|---|---|---|---|---|
| `govde_kimligi` | u32 | — | gövde | yaşayan bir gövde | protokol hatası |
| `sureklilik_no` | u32 | — | §4.3 | ≥ o gövdenin son numarası | protokol hatası |
| `govde_turu` | u8 | — | 1 araç, 2 torpido, 3 hedef | doğumdaki türle aynı | protokol hatası |
| `dolu_tupler` | u8 | — | araçta dolu tüpler; diğerlerinde 0 | — | — |
| `bayraklar` | u16 | — | bit 0: geçerli | — | bit 0 = 0 ise gövde son geçerli pozunda çizilir |
| `p_n` | 3 × f64 | m | gövde referans noktası `O_b`'nin NED konumu (senaryo orijinine göre) | sonlu | gövde son geçerli pozunda kalır, sayaç (A2-K27) |
| `q_nb` | 4 × f64 | — | gövdeden dünyaya dönüş (w, x, y, z), Hamilton | sonlu, `|‖q‖ − 1| ≤ 1e-6` | aynı |
| `v_n` | 3 × f64 | m/s | `O_b`'nin NED hızı | sonlu | aynı |
| `w_b` | 3 × f64 | rad/s | gövdenin dünyaya göre açısal hızı, gövde çerçevesinde | sonlu | aynı |

Gövde durumu itki, itici, fırlatıcı emniyeti ya da atışla ilgili hiçbir alan taşımaz (AB-048; iskelet §5: torpido itkisizdir). `v_n` ve `w_b` yalnız operatör görünümünün ara değerlemesi içindir.

**`SahneOlayi`**

| `tur` | Kod | Dolu alanlar | Anlamı |
|---|---|---|---|
| `govde_dogdu` | 1 | `govde_kimligi`, `varlik_indeksi`, `govde_turu`, `sureklilik_no`, `kaynak_tup` (torpidoda 1 ya da 2, diğerlerinde 0) | gövde `t_ns` anında dünyaya girdi (torpidonun tüpten ayrılması, senaryo hedefinin eklenmesi) |
| `govde_oldu` | 2 | `govde_kimligi`, `neden` (1 isabet, 2 alan dışı, 3 süre, 4 sıfırlama) | gövde `t_ns` anında dünyadan çıktı |
| `kutle_durumu` | 3 | `govde_kimligi`, `eski_tupler`, `yeni_tupler`, `sureklilik_no` | aracın dolu tüp kümesi değişti |
| `isabet` | 4 | `govde_kimligi` (torpido), `diger_govde_kimligi` (hedef), `nokta_n` (m), `normal_n` (birim), `bagil_hiz_n` (m/s) | yalnız görselleştirme içindir (iz, işaret); atış kararını etkilemez |
| `sifirlama` | 5 | `calisma_nesli` (yeni) | senaryo sıfırlandı; bütün gövdeler öldü sayılır; çalıştırma nesli arttı (A2-K80) |
| `duraklat`, `devam` | 6, 7 | — | sim zamanı durdu / yeniden ilerliyor |
| `atlama` | 8 | `calisma_nesli` (yeni) | tekrar oynatmada zaman atlandı; `t_ns` yeni zamandır; çalıştırma nesli arttı (A2-K80) |

Bütün olaylarda `t_ns`, `adim_no` ve `calisma_nesli` (olaydan sonra geçerli nesil) doludur; `t_ns` bir adım sınırıdır ve olay o sınırla başlayan fizik adımının başında uygulanmıştır (A2-K21). Kullanılmayan alanlar sıfırdır.

**`DalgaKumesi`**

| Alan | Tip | Birim | Anlamı | Geçerli |
|---|---|---|---|---|
| `surum` | u32 | — | oturum içinde her yeni kümede artar | ≥ 1 |
| `bilesen_sayisi` | u16 | — | bileşen sayısı | 0–256 |
| `derin_su` | u8 | — | 1 ise `derinlik_m` yok sayılır | 0/1 |
| `tohum` | u64 | — | kümenin üretildiği tohum (yalnız kayıt) | — |
| `g`, `rho`, `derinlik_m` | f64 | m/s², kg/m³, m | 04'ün kullandığı değerler | sonlu, > 0 |
| `ozet_sha256` | 32 B | — | bileşen dizisinin (`bilesen_sayisi × 40` bayt, bu tablodaki sırayla) SHA-256'sı (FIPS 180-4) | alıcı yeniden hesaplar; farklıysa protokol hatası |

`DalgaBileseni`: `a` (m, ≥ 0), `omega` (rad/s, > 0), `k` (rad/m, > 0), `beta` (rad, yayılma yönü, kuzeyden doğuya), `phi` (rad). **Faz kuralı (bağlayıcı; U-17, AB-190):** yerel su yüzeyi yüksekliği, ortalama yüzeye (NED `D = 0` düzlemi) göre yukarı pozitif,

```
ζ(x, y, t) = Σ_i a_i · cos( ω_i·t − k_i·(x·cos β_i + y·sin β_i) + φ_i )
x = N (m), y = E (m), t = t_ns · 10⁻⁹ s
```

Bu, 04 §2.4'teki fizik çekirdeğinin kuralıyla aynıdır. Görselleştirici aynı kümeden, aynı formülle ve kesme yapmadan çizer; rastgele sayı üretmez.

**`Ortam`**

| Alan | Tip | Birim | Anlamı | Geçerli |
|---|---|---|---|---|
| `ortam_surumu` | u32 | — | artan sürüm | ≥ 1 |
| `ortam_turu` | u8 | — | 1 havuz, 2 açık su | 1–2 |
| `statik_varlik_indeksi` | u16 | — | A7 `varliklar[]` içinde `tur = statik` olan varlık; 0xFFFF yok | geçerli indeks ya da 0xFFFF |
| `statik_p_n`, `statik_q_nb` | 3 + 4 × f64 | m, — | statik varlığın `REF` noktasının NED pozu | sonlu, birim kuaterniyon |
| `gunes_yonu_n` | 3 × f64 | — | sahneden güneşe birim vektör, NED | `|‖v‖ − 1| ≤ 1e-6` |
| `gunes_isinimi_w_m2` | f64 | W/m² | yüzeydeki güneş ışınımı | ≥ 0 |
| `ortam_isigi_rgb_dogrusal` | 3 × f64 | — | doğrusal RGB ortam ışığı | ≥ 0 |
| `beta_d_m`, `beta_b_m` | 3 × f64 | 1/m | R, G, B kanallarında zayıflama ve geri saçılım katsayısı (07 §2.5'teki fenomenolojik model; kalibrasyon A5'te) | ≥ 0 |
| `b_sonsuz_rgb_dogrusal` | 3 × f64 | — | sonsuz uzaklıktaki geri saçılım rengi | 0–1 |
| `bulaniklik_olcegi` | f64 | — | ileri saçılım bulanıklığının ölçeği | ≥ 0 |
| `su_optigi_surumu` | u32 | — | A5'teki su optiği kalibrasyon sürümü | — |

Geçersiz alanlı `ortam` iletisi protokol hatasıdır. Işınımın Godot ışık enerjisine çevrilmesi Godot sahnesi uygulama işindedir (§8).

**`MalzemeOptik`**: A5'in optik malzeme geçersiz kılmalarından sonra **etkin** değerler (A7-K30). `malzeme_indeksi` A7 `malzemeler[]` indeksidir; `maske` hangi alanların geçersiz kılındığını söyler (bit 0 `taban_rengi_dogrusal` [4 × f64, 0–1], bit 1 `metaliklik` [0–1], bit 2 `puruzluluk` [0–1]); maskede olmayan alanlar için glTF malzemesinin kendi değeri kullanılır. Girdi yoksa `girdi_sayisi = 0` ile yine gönderilir.

### 3.5 Kamera isteği ve yanıtları

**`KameraIstegi`** (ardından `govde_sayisi × GovdeDurumu`: `t_cizim` anında yaşayan **bütün** gövdelerin durumu)

| Alan | Tip | Birim | Anlamı | Geçerli | Geçersizse (görselleştirici) |
|---|---|---|---|---|---|
| `istek_kimligi` | u64 | — | oturum içinde kesin artan | > önceki | protokol hatası |
| `kare_sirasi` | u32 | — | kamera başına kare sayacı (05'in `kare_seq`'i): sim süreci başlarken 1'dir ve her kamera periyodunda, o periyodun karesi için istek gönderilsin ya da gönderilmesin, 1 artar; yeni çalıştırma, sıfırlama ve atlamada sıfırlanmaz (A2-K49) | > 0, kamera başına kesin artan | protokol hatası |
| `kamera_kimligi` | u8 | — | yapılandırılmış kamera | `hosgeldin`'deki | `kare_dustu(gecersiz_istek)` |
| `yuva`, `halka_nesli` | u16, u32 | — | sim'in bu isteğe ayırdığı yuva | `yuva < yuva_sayisi`, nesil güncel | `kare_dustu(halka_hatasi)` |
| `yapilandirma_surumu` | u32 | — | kamera yapılandırmasının sürümü | `hosgeldin`'deki | `kare_dustu(tutarsiz_surum)` |
| `t_cizim_ns` | i64 | ns | pozlama ortası; çizilecek durumun zamanı | adım sınırı | `kare_dustu(gecersiz_istek)` |
| `t_olcum_ns` | i64 | ns | pozlama sonu | `= t_cizim_ns + pozlama_ns/2` | aynı |
| `pozlama_ns` | i64 | ns | pozlama süresi | > 0, çift, ≤ 1 000 000 000 (§3.6) | aynı |
| `t_teslim_ns` | i64 | ns | modellenmiş alım anı | `≥ t_olcum_ns` | aynı |
| `adim_no` | u64 | — | `t_cizim` anının adımı | `t_cizim_ns = adim_no · adim_ns` | aynı |
| `gurultu_tohumu` | u64 | — | sensör gürültüsünün anahtarı; değeri A5 Ek D'deki `a2/kamera_gurultu` alt akışından, A5'in tanımladığı türetim ve sayaç tahsisiyle bu kare için üretilir (A2-K95) | — | — |
| `dalga_kumesi_surumu`, `ortam_surumu`, `malzeme_optik_surumu` | u32 | — | çizimde kullanılacak sürümler | görselleştiricide mevcut | `kare_dustu(tutarsiz_surum)` |
| `govde_sayisi` | u16 | — | ardından gelen gövde durumu | ≤ 64 | protokol hatası |
| `calisma_nesli` | u32 | — | isteğin ait olduğu çalıştırma nesli (A2-K80) | görselleştiricinin bildiği son nesil | `kare_dustu(tutarsiz_surum)` |

**`KareHazir`**: `istek_kimligi`, `halka_nesli`, `yuva`, `kamera_kimligi`, `kare_sirasi`, `calisma_nesli` (istekteki değerler); `yazim_sayaci` (yuva başlığına yazılan değer; A2-K53); `hazir_duvar_ns` (yuva yazımı bittikten sonraki duvar saati).

**`KareDustu`**: aynı kimlik alanları (`calisma_nesli` dahil) ve `neden`: 1 daha yeni istek, 2 iptal, 3 geri okuma hatası, 4 sahne hazır değil, 5 geçersiz istek, 6 tutarsız sürüm, 7 halka hatası.

**`KameraIptal`**: `istek_kimligi`, `kamera_kimligi`, `neden` (1 geç, 2 çalıştırma nesli değişti, 3 oturum sonu, 4 halka yenilendi).

**`GoruntuleyiciNabzi`**: `gonderim_duvar_ns`; `son_uygulanan_t_ns` (operatör görünümünün son çizdiği sim zamanı); `cizim_hizi_mhz` (saniyedeki ana döngü çizimi × 1000); `elle_cizim_sayaci`, `operator_bekleme_sayaci` (veri gecikince son durumda beklenen çizim sayısı), `uretilen_kare`, `dusen_kare` (oturum başından toplam); `pencere_cizilebilir` (0/1); `gpu_bellek_bayt` (bilinmiyorsa 0). **`SimNabzi`**: `t_ns`, `adim_no`, `gonderim_duvar_ns`, `calisma_durumu`, `sim_kipi`.

### 3.6 Kamera yapılandırması (`KameraYapilandirmasi`, 184 B)

| Alan | Tip | Birim | Anlamı | Geçerli |
|---|---|---|---|---|
| `kamera_kimligi` | u8 | — | 0 | 0 |
| `piksel_bicimi` | u8 | — | 1 = `rgb8_srgb` (§3.8) | 1 |
| `genislik`, `yukseklik` | u16 | piksel | çözünürlük | `kademe_kamera`'nın değeri (§6): tam 640 × 480, sade 320 × 240; genel sınır 16 ≤ g ≤ 640, 16 ≤ y ≤ 480 (iskelet §10 md.25) |
| `hiz_pay`, `hiz_payda` | u32 | Hz | kare hızı = pay / payda | `kademe_kamera`'nın değeri (§6): tam 30/1, sade 15/1; genel sınır pay/payda ≤ 30 ve `1000 · pay / payda` tamsayı (A8 `rate_mhz` kayıpsız taşınır) |
| `yapilandirma_surumu` | u32 | — | sim'in bu çalıştırmada kamera yapılandırmasına verdiği sürüm (1'den başlar; senaryo alanı değildir) | ≥ 1 |
| `kalibrasyon_surumu` | u16 | — | A5'teki kalibrasyon sürümü; A8 `calib_ver` ile aynı genişlik | 1 … 65 535 |
| `ayrilmis_kal` | u16 | — | ayrılmış | 0 |
| `fx`, `fy`, `cx`, `cy` | f64 | piksel | iğne deliği iç parametreleri | sonlu; fx, fy > 0; −0,5 ≤ cx ≤ g − 0,5; −0,5 ≤ cy ≤ y − 0,5 (A8'in sınırı; U13) |
| `bozulma_modeli` | u32 | — | 0 yok, 1 Brown–Conrady (k1 k2 p1 p2 k3) | 0–1 |
| `montaj_govde_kimligi` | u32 | — | kameranın bağlı olduğu gövde (araç) | yaşayan araç |
| `bozulma` | 5 × f64 | — | k1 k2 p1 p2 k3 | sonlu |
| `montaj_l_b` | 3 × f64 | m | kamera merkezinin `O_b`'ye göre konumu, gövde çerçevesi; **fizik gerçeği** montajı | sonlu; her bileşenin mutlak değeri ≤ 10 m (mimari seçimi, tahmin) |
| `montaj_q_bs` | 4 × f64 | — | **S çerçevesinden** (x ileri = optik eksen, y sağ, z aşağı; A5 ve A8 ile aynı) gövdeye dönüş, w x y z; fizik gerçeği montajı. Optik çerçeveye geçiş A2-K94'tedir | sonlu; `|w² + x² + y² + z² − 1| ≤ 1·10⁻⁵`, toplam binary64'te w, x, y, z sırasıyla |
| `pozlama_ns` | i64 | ns | nominal pozlama süresi | > 0, çift, ≤ 1 000 000 000 |
| `yakin_m`, `uzak_m` | f64 | m | kırpma uzaklıkları | 0 < yakın < uzak |

**Kamera çerçevesi ve piksel kabulü (bağlayıcı):** iç parametrelerin ve izdüşümün tanımlandığı **optik çerçeve** x sağa, y aşağı, z optik eksen boyunca ileridir (OpenCV kabulü); montaj ise S çerçevesinde verilir ve ikisi arasındaki sabit dönüşüm A2-K94'tedir. Kamera çerçevesindeki `(X, Y, Z)` noktası, bozulma yokken `u = fx·X/Z + cx`, `v = fy·Y/Z + cy` pikseline düşer; tamsayı `(u, v)` piksel `(u, v)`'nin **merkezidir**; `(0, 0)` sol üst piksel. Geçersiz alanlı yapılandırma görselleştiricide `veda(protokol_hatasi)` ile sonuçlanır. Bu tablonun geçerli aralıkları kamera yapılandırmasının tek kaynağıdır (W1-uyum U13): A8 aynı alanları `CAMERA_CONFIG`'te bu aralıklara atıfla taşır, A5 senaryo değerlerini bunlarla doğrular; bu aralıktaki her değer kayıpsız taşınır. **Bit eşitliği her bağlantının kendi A5 kaynağına bağlıdır:** A2'nin `montaj_l_b` ve `montaj_q_bs` alanları A5'in **fizik gerçeği** montajından, A8 `CAMERA_CONFIG`'in montaj alanları A5'in **nominal** montajından gelir ve her biri kendi A5 kaynağıyla bit düzeyinde aynıdır; A8 fizik gerçeği montajını almaz ve A2'deki montaj bitlerini kopyalamaz (A5-K114, A8-K53). İki bağlantıda da bulunan iç parametre (`fx`, `fy`, `cx`, `cy`) ve bozulma (model ve katsayılar) alanları aynı A5 kaynağından gelir ve iki bağlantıda da binary64 olarak, kaynağın bitleriyle aynen taşınır; çevirme ve yuvarlama yoktur (A8 §3.5). Örnek: A5'te fizik gerçeği montaj konumu `(0,20; 0; 0)` m, nominal montaj konumu `(0,21; 0; 0)` m ise A2 `montaj_l_b` = `(0,20; 0; 0)` taşır (görselleştirici kareyi bu konumdan çizer), A8 `CAMERA_CONFIG` montaj konumu = `(0,21; 0; 0)` taşır (bileşen ve kart bu değeri kullanır); aradaki 1 cm, senaryonun bilerek koyduğu montaj hatasıdır ve hiçbir bağlantı onu ortadan kaldırmaz; iki bağlantıdaki `fx`, `fy`, `cx`, `cy` ve bozulma alanları ise birbirine ve A5'e bit düzeyinde eşittir; piksel biçimi ve bozulma modeli kodlarının A8'deki tamsayı genişlikleri farklıdır ama geçerli kod kümeleri (1; 0–1) iki genişliğe de sığar.

Yapılandırma montaj ve iç parametreleri görselleştiriciye verir, çünkü kareyi fizik gerçeğindeki kamera pozundan çizmek görselleştiricinin işidir. Bu değerler kare halkasına yazılmaz (A2-K55).

### 3.7 Halka bilgisi (`HalkaBilgisi`, 96 B)

| Alan | Tip | Anlamı |
|---|---|---|
| `kamera_kimligi` | u8 | halkanın kamerası |
| `halka_nesli` | u32 | §2 |
| `yuva_sayisi` | u32 | 4–32 (varsayılan 8; mimari seçimi, tahmin; sim yapılandırması A2-K93) |
| `yuva_boyutu` | u64 | `⌈(256 + satir_adimi·yukseklik) / 4096⌉ · 4096` bayt |
| `toplam_boyut` | u64 | `4096 + yuva_sayisi · yuva_boyutu` |
| `ad` | metin (64 B) | işletim sistemi nesne adı (A2-K70–K72) |

Örnek (hesap): 640 × 480 RGB8'de piksel bölgesi 921 600 B, yuva 925 696 B, 8 yuvalı halka 7 409 664 B.

### 3.8 Kamera karesi biçimi (halka düzeni; A8 bu bölüme atıf verir)

**Halka başlığı** (`HalkaBasligi`, ofset 0, 4 096 B'lık bölgenin ilk 128 B'ı):

| Ofset | Alan | Tip | Değer |
|---|---|---|---|
| 0 | `sihirli` | u64 | `0x4B4C414832414449` (baytlar "IDA2HALK") |
| 8 | `duzen_ana` | u16 | A2 ana sürümü (1) |
| 10 | `duzen_alt` | u16 | A2 alt sürümü (0) |
| 12 | `baslik_bolgesi` | u32 | 4096 |
| 16 | `oturum_kimligi` | u64 | `hosgeldin`'deki değer |
| 24 | `halka_nesli` | u32 | `hosgeldin`'deki değer |
| 28 | `yuva_sayisi` | u32 | `hosgeldin`'deki değer |
| 32 | `yuva_boyutu` | u64 | `hosgeldin`'deki değer |
| 40 | `yuvalar_ofseti` | u64 | 4096 |
| 48 | `toplam_boyut` | u64 | `hosgeldin`'deki değer |
| 56 | `genislik` | u16 | kamera çözünürlüğü |
| 58 | `yukseklik` | u16 | kamera çözünürlüğü |
| 60 | `satir_adimi` | u32 | `genislik × 3` |
| 64 | `kamera_kimligi` | u8 | yapılandırmadaki |
| 65 | `piksel_bicimi` | u8 | yapılandırmadaki (1) |
| 66 | `dolgu0` | u16 | 0 |
| 68 | `yuva_baslik_boyutu` | u32 | 256 |
| 72 | `ayrilmis` | u8[56] | 0 |

Halka başlığını yalnız sim, halkayı oluştururken bir kez yazar; sonra değişmez. Bir halkanın boyutu ve düzeni ömrü boyunca değişmez; değişmesi gerekirse (A2-K59) yeni nesil numarasıyla yeni bir halka oluşturulur.

**Yuva `i`** `4096 + i · yuva_boyutu` ofsetindedir. İlk 256 bayt yuva başlığıdır (`YuvaBasligi`), pikseller yuva başından 256 bayt sonra başlar.

| Alan | Tip | Birim | Anlamı |
|---|---|---|---|
| `yazim_sayaci` | u32 | — | bu yuvaya yapılan yazım sayısı; 0 hiç yazılmadı; görselleştirici her yazımda 1 artırır. **Yalnız tanılama içindir** (A2-K53); eşzamanlı erişime karşı koruma sayılmaz ve atomik değildir |
| `calisma_nesli` | u32 | — | karenin ait olduğu çalıştırma nesli (A2-K80) |
| `istek_kimligi`, `halka_nesli`, `kare_sirasi`, `kamera_kimligi` | — | — | karenin ait olduğu istek |
| `piksel_bicimi` | u8 | — | 1 `rgb8_srgb` |
| `kademe` | u8 | — | kamera kademesi (1 sade, 2 tam) |
| `cizim_yolu` | u8 | — | 1 = ana iş parçacığından `force_draw` (A2-K38); başka değer yoktur |
| `genislik`, `yukseklik`, `satir_adimi` | u16, u16, u32 | piksel, bayt | halka başlığıyla aynı |
| `bayraklar` | u32 | — | aşağıdaki tablo |
| `yapilandirma_surumu`, `dalga_kumesi_surumu`, `ortam_surumu` | u32 | — | karenin üretildiği sürümler |
| `kalibrasyon_surumu`, `ayrilmis_kal` | u16, u16 | — | karenin üretildiği kalibrasyon sürümü (1 … 65 535); ayrılmış 0 |
| `t_olcum_ns`, `pozlama_ns` | i64 | ns | ölçüm zamanı (pozlama sonu) ve pozlama süresi; istekten aynen. Kare zamanının birincil gösterimi budur (iskelet §3.1) |
| `t_cizim_ns` | i64 | ns | `t_olcum_ns − pozlama_ns/2`; türetilmiş, kolaylık için yazılır; istekten aynen |
| `t_teslim_ns` | i64 | ns | istekten aynen |
| `istek_alindi_duvar_ns`, `cizim_basladi_duvar_ns`, `hazir_duvar_ns` | i64 | ns | görselleştiricinin gerçek hat süreleri (duvar saati) |
| `piksel_bayt` | u64 | bayt | `satir_adimi × yukseklik` |

| Bayrak biti | Ad | Anlamı |
|---|---|---|
| 0 | bozulma uygulandı | mercek bozulması çizime uygulandı |
| 1 | gürültü uygulandı | sensör gürültüsü uygulandı |
| 2 | dalga görseli | dalga yüzeyi çizildi; 0 ise düz su çizildi (sade 3B kademesi; AB-054) |
| 3 | su altı optiği | zayıflama ve geri saçılım uygulandı |
| 4 | kamera su altında | kamera merkezi `t_cizim` anında yerel dalga yüzeyinin altında |
| 5 | yüzeye yakın | kamera merkezinin yerel yüzeye düşey uzaklığı < 0,1 m (mimari seçimi): kare yüzey geçişindedir; su altı/su üstü ayrımı bütün kare için tek uygulandığından optik yaklaşıktır (AB-054) |
| 6 | pencere çizilebilir | çizim anında operatör penceresi çizilebilirdi (yalnız tanı) |
| 7 | tutarsız | sürüm uyuşmazlığı altında üretildi; kare geçersizdir |

**Piksel biçimi (tek biçim): `rgb8_srgb`.** Piksel başına 3 bayt, sırası R, G, B; değerler sRGB aktarım işleviyle kodlanmış 8 bit (IEC 61966-2-1), ton eşlemesinden sonra; satırlar üstten alta, satır sonunda dolgu yok (`satir_adimi = genislik × 3`); saydamlık kanalı yok.

Gerekçe: (1) Ölçülmüş kamera yolu bu biçimi üretir: saydam olmayan `SubViewport`'ta `get_texture().get_image()` RGB8 döndürür (640 × 480'de 921 600 B; Godot 4.7.2 `texture_storage.cpp:4289` ve `1926–1927`, MP6 §4.4) ve MP6'nın G2/G3 kanıtının tamamı (örtülü, gizli, simge durumlarında 9000/9000 kare, görünür referansla bit düzeyinde aynı) bu yolla alındı; 640 × 480 ilk okuma p99 5,69 ms bu biçime dönüşümü içerir. (2) Dördüncü kanal saydam olmayan hedefte bilgi taşımaz; RGBA8'e göre halka, A8 okuması ve A6 kaydı %25 daha az bayt taşır (hesap). (3) `RenderingDevice.texture_get_data` RGBA8 verir (MP6 §4.4); o yol kullanılırsa görselleştirici RGB8'e kendisi çevirir, biçim değişmez. sRGB kodlamasının Godot'daki kaynağı: `use_hdr_2d = false` olan görüntü alanının çıktısı renk kodlamalı ve 0–1'e kırpılmıştır (Godot 4.7.2 `Viewport.use_hdr_2d` belgesinden **çıkarım; doğrulanmadı**); A2-U11 bunu sınar.

Halka hiçbir koşulda poz, montaj, iç parametre, hedef konumu ya da başka bir yer gerçeği taşımaz (A2-K55).

---

## 4. Davranış kuralları

### 4.1 Taşıma

- **A2-K01.** Sim, durum kanalını yalnız `127.0.0.1` adresinde bir TCP dinleyicisi olarak açar; varsayılan port 47210'dur (mimari seçimi: IANA kayıt defterinde 47101–47556 aralığı atanmamış; macOS ve Windows'un geçici port aralığı 49152–65535'in dışında; Microsoft KB 929851). Port sim yapılandırmasından değiştirilir (A2-K93); başka bir arabirime bağlanılmaz.
- **A2-K02.** 3B görünüm kademesi kapalıysa sim A2 dinleyicisini açmaz ve kamera kademesi kapalı olmak zorundadır (A5 doğrulaması; §6).
- **A2-K03.** Sim aynı anda en çok bir görselleştirici bağlantısı tutar; ikinci bir bağlantıya `ret(mesgul)` gönderir ve onu kapatır, mevcut oturumu etkilemez.
- **A2-K04.** İki taraf bağlantı kurulur kurulmaz sokette `TCP_NODELAY` ayarlar.
- **A2-K05.** Dinleyici açılamazsa (port kullanımda) sim çalışmayı A2'siz sürdürür, durumu A3'e bildirir ve her kamera periyodunu `gorsellestirici_yok` nedeniyle düşen kare sayar.
- **A2-K06.** Alıcı; sihirli sayı yanlış, tür bilinmiyor, `bayraklar ≠ 0`, `yuk_bayt` türün beklenen boyutundan (§3.2) farklı ya da 1 048 576'dan büyük, metin alanında NUL yok, sayı alanı §3'teki sınırı aşıyor ise `veda(protokol_hatasi)` gönderir ve bağlantıyı kapatır.
- **A2-K07.** Her yönde ilk iletinin `sira` değeri 0'dır ve gönderen her iletide 1 artırır. Alıcı kesin artışı denetler; azalan ya da yinelenen sıra protokol hatasıdır. Sim → görselleştirici yönünde boşluk yalnız A2-K25 ve A2-K26'daki `durum` seyreltmesinden doğar; görselleştirici boşlukları sayar, hata saymaz. Görselleştirici → sim yönünde boşluk protokol hatasıdır.
- **A2-K08.** `*_duvar_ns` alanlarının hepsi §2'deki duvar saatiyle doldurulur.

### 4.2 El sıkışma, sürüm ve uyumluluk

- **A2-K10.** Görselleştirici bağlantı kurulunca ilk ileti olarak `merhaba` gönderir. Sim bağlantıyı kabul ettikten sonra 5 s (duvar saati; mimari seçimi) içinde `merhaba` almazsa bağlantıyı kapatır.
- **A2-K11.** `merhaba.surum_ana ≠ 1` ise sim `ret(surum)` gönderir ve kapatır. Ana sürüm aynıysa oturum alt sürümü `min(sim alt, görselleştirici alt)`'tır ve `hosgeldin`'de bildirilir; hiçbir taraf oturum alt sürümünden sonra eklenmiş ileti ya da alan göndermez. Ana sürüm yalnız bayt düzeni ya da bir kuralın anlamı değişince artar; alt sürüm yalnız yeni ileti türü ya da ayrılmış alanın kullanıma alınmasıyla artar.
- **A2-K12.** Sim `merhaba`ya `hosgeldin`, ardından sırasıyla `dalga_kumesi`, `ortam` ve `malzeme_optik` ile yanıt verir. Bu dört iletiden sonra sim `durum`, `sahne_olayi` ve `sim_nabzi` akışını başlatır; `kamera_istegi`ni yalnız başarılı `hazir`dan sonra gönderir.
- **A2-K13.** Görselleştirici `manifest_yolu`ndaki dosyayı okur, baytlarının SHA-256'sını hesaplar (A7-K05), sahneyi A7 kurallarıyla kurar (A7-K53), her kameranın halkasını eşleyip başlığını doğrular (A2-K74) ve `hazir` gönderir. Herhangi bir adım başarısızsa `hazir.durum` ilgili hata kodunu taşır (§5).
- **A2-K14.** Sim `hazir.manifest_sha256`yı kendi özetiyle bayt bayt karşılaştırır. Özetler farklıysa ya da `hazir.durum ≠ 0` ise sim `ret(manifest)` (halka hatasında `ret(halka)`) gönderir, bağlantıyı kapatır ve o oturumda hiçbir kamera isteği göndermez.
- **A2-K15.** `hosgeldin`'den sonra 120 s (duvar saati; mimari seçimi: sahne yükleme ve ilk gölgelendirici derlemesi için; Metal derleme süresi ayrıca ölçülecek, 12.9) içinde `hazir` gelmezse sim `ret(zaman_asimi)` gönderir ve kapatır.
- **A2-K16.** Görselleştirici `ret(surum)` ya da `ret(manifest)` aldıktan sonra kendiliğinden yeniden bağlanmaz ve nedeni kullanıcıya gösterir; diğer ret ve kapanmalardan sonra A2-K63 geçerlidir.
- **A2-K17.** El sıkışma sırasında ve sonrasında sim hiçbir adımı görselleştirici yüzünden bekletmez (A2-K47).

### 4.3 Durum akışı ve sahne olayları

- **A2-K20.** Sim, `durum` iletisini sim zamanında 60 Hz takvimle gönderir (mimari seçimi; 07 §3.2); takvim 04'ün rasyonel zamanlayıcısıyla en yakın adım sınırına bağlanır (ardışık iletiler arası 13 ya da 14 adım, en çok 0,625 ms sapma; hesap: 16,67 ms / 1,25 ms = 13⅓).
- **A2-K21.** **Süreklilik sınırı (tek tanım).** `T` anındaki bir olay, `T` ile başlayan fizik adımının başında uygulanır; `T` zamanlı her durum (`durum` iletisi, kamera isteğindeki gövde durumu) olaydan **sonraki** durumu taşır. Doğan bir gövdenin ilk `sureklilik_no` değeri 0'dır; yaşayan bir gövdenin numarası, `T` adımının başında darbe (fırlatma geri tepmesi), kütle durumu değişimi ya da tekrar oynatmada atlama varsa, aynı adımda birden çok neden olsa da, `T` zamanlı durumda tam 1 artmış olarak görünür. Bir gövdenin sürekliliği `T`'de değiştiyse sim, takvimde değilse takvim dışı bayrağıyla, iki `durum` iletisi gönderir: `T − 1 250 000 ns` zamanlı (eski parçanın son durumu, eski numarayla) ve `T` zamanlı (olay sonrası ilk durum, yeni numarayla).
- **A2-K22.** Sim, `T` anındaki bir `sahne_olayi`nı, `T` zamanlı `durum` iletisinden önce gönderir.
- **A2-K23.** `durum` iletisinin gövde listesi o anda yaşayan bütün gövdeleri içerir: `T`'de doğan gövde `T` zamanlı durumda ilk kez yer alır; `T`'de ölen gövde `T` zamanlı durumda yer almaz. Gövde kimliği oturum içinde yeniden kullanılmaz.
- **A2-K24.** Sim duraklatıldığında `duraklat` olayını gönderir ve devam edene kadar `durum` göndermez; `sim_nabzi` duvar saatine göre 1 s aralıkla sürer.
- **A2-K25.** SIL'de sim, takvimli `durum` iletilerini duvar saatinin her saniyesinde en çok 120 olacak biçimde seyreltir (mimari seçimi); takvim dışı iletiler seyreltilmez.
- **A2-K26.** Sim gönderimde hiçbir zaman bloklanmaz. Bağlantı başına gönderme kuyruğu 8 MiB ile sınırlıdır (mimari seçimi). Kuyruk sınıra ulaşınca sim gönderilmemiş takvimli `durum` iletilerini atar (her biri `sira` boşluğu olarak görünür ve sayılır); diğer iletiler için yer kalmazsa `veda(tampon_tasti)` gönderir ve bağlantıyı kapatır.
- **A2-K27.** Görselleştirici her `GovdeDurumu`nu §3.4'teki geçerlilik koşuluyla denetler; geçersiz gövde son geçerli pozunda çizilir ve sayılır. Bu gövdeyi içeren kamera isteği `kare_dustu(gecersiz_istek)` ile yanıtlanır.
- **A2-K28.** `durum.dalga_kumesi_surumu` görselleştiricinin son aldığı `dalga_kumesi` sürümüyle uyuşmazsa görselleştirici o durum geçerli kalana kadar operatör görünümünü "tutarsız" işaretler; bu sürede çizilen kamera kareleri `tutarsiz` bayrağı taşır.
- **A2-K29.** `sifirlama` olayıyla birlikte bütün gövdeler ölmüş sayılır ve çalıştırma nesli artar (A2-K80); görselleştirici gövde geçmişini siler. Sim sıfırlamadan sonra yaşayan gövdeler için `govde_dogdu` olaylarını yeni sim zamanında gönderir. `atlama` olayında da çalıştırma nesli artar; görselleştirici geçmişi siler ve sahneyi A2-K90–K92'deki gövde tablosundan yeniden kurar.

### 4.4 Operatör görünümü ve iki zamanlı çizim

- **A2-K30.** Operatör zamanı `t_op` hiçbir zaman alınmış son `durum.t_ns`'den büyük olmaz; görselleştirici ekstrapolasyon yapmaz, veri gecikirse son durumda bekler ve `operator_bekleme_sayaci`nı artırır.
- **A2-K31.** `t_op = t_son − D_op`'tur; `D_op` varsayılanı 33 333 000 ns'dir (yaklaşık iki `durum` periyodu, mikrosaniyeye yuvarlanmış; mimari seçimi, tahmin: bilgisayar tarafında 10–20 ms'lik seyrek gecikmeler görüldü, iskelet §8) ve sim yapılandırmasından (A2-K93) 0–100 ms aralığında ayarlanır.
- **A2-K32.** Görselleştirici iki durum arasında yalnız aynı `govde_kimligi` ve aynı `sureklilik_no`yu taşıyan durumlarla ara değerler: konumda `v_n` ile kübik Hermite, yönelimde kısa yoldan küresel doğrusal ara değerleme (slerp). Bir gövdenin sürekliliği `T`'de değiştiyse (A2-K21): `t_op < T − 1 250 000 ns` için eski parçanın içinde ara değerlenir; `T − 1 250 000 ns ≤ t_op < T` için eski parçanın son durumu (`T − h` zamanlı) değiştirilmeden çizilir; `t_op ≥ T` için yeni parça kullanılır ve sınırda (`t_op = T`) olay sonrası durum çizilir. Olayın iki yanı arasında ara değerleme yapılmaz; sınırdaki konum hatası en çok bir fizik adımı boyunca yer değiştirmedir.
- **A2-K33.** `T`'de doğan gövde `t_op < T` için çizilmez, `t_op ≥ T` için çizilir; `T`'de ölen gövde `t_op < T` için çizilir, `t_op ≥ T` için çizilmez.
- **A2-K34.** Görselleştirici her gövde için en az son 500 ms'lik (sim zamanı; mimari seçimi) durum geçmişini tutar.
- **A2-K35.** `t_op` sıfırlama ve atlama dışında azalmaz.
- **A2-K36.** **İki zaman.** Kamera karesi yalnız kendi isteğinin `t_cizim` durumunu gösterir: gövdeler istekteki `GovdeDurumu` listesinden, ara değerleme yapılmadan; dalga yüzeyi istekteki sürümün kümesinden `t = t_cizim`'de; ortam ve malzeme istekteki sürümlerden. Operatör görünümünün durumu, `durum` akışı ve başka bir isteğin durumu kamera karesine girmez.
- **A2-K37.** Sahne içi göstergeler ve hata ayıklama çizimleri (çarpışma, yüzerlik ve akustik ağlar, A7) kamera karesinde görünmez.
- **A2-K38.** **Kamera çizim yolu (iskelet §8).** Görselleştirici kamera karesini yalnız şu yolla üretir: ana iş parçacığından `RenderingServer.force_draw(false, dt)` çağrısı, kamera `SubViewport`'u `UPDATE_ONCE` kipinde, ardından aynı iş parçacığında eşzamanlı geri okuma. Olağan çizim döngüsüne bağlı kare üretimi ve asenkron geri okuma (`texture_get_data_async`) kamera için kullanılmaz (MP6 §6: olağan döngü örtülü, gizli ve simge durumunda kare üretmedi; asenkron okuma gizli pencerede p99 70,52 ms). Yuva başlığında `cizim_yolu = 1` yazılır.
- **A2-K39.** Kamera karesi kendinden önceki çizimlere bağlı (zamansal birikimli) etki kullanmaz. Aynı istek, aynı makinede, öncesinde hangi operatör ya da kamera çizimi yapılmış olursa olsun, ortalama mutlak farkı ≤ 2/255 ve hareketli bir nesnenin piksel konumu ≤ 1 piksel farkla aynı kareyi üretir (eşikler 07 §7 P5'ten; mimari seçimi. MP6'da aynı yol bit düzeyinde aynı kare üretti, MP6 §5 G3).
- **A2-K40.** Görselleştirici bir ana döngü yinelemesinde önce bekleyen kamera isteklerini çizer, sonra operatör görünümünü günceller. Pencere çizilebilir değilken operatör görünümü çizilmeyebilir; kamera üretimi sürer.

### 4.5 Kamera isteği ve kare teslimi

- **A2-K41.** Sim, `t_cizim` anının durumunu üreten fizik adımı tamamlanınca o isteği gönderir; istek `t_cizim`'den önce gönderilmez. Kare zamanının birincil gösterimi ölçüm zamanı `t_olcum` (pozlama sonu) ve `pozlama` süresidir (iskelet §3.1); `t_cizim = t_olcum − pozlama/2` bunlardan türer ve bir adım sınırıdır; `pozlama` çift sayıdır; `t_teslim ≥ t_olcum`'dur. Tek bir geçerlilik anı gereken her tüketici (algılama kaydının yakalama zamanı dahil, A8) bu türetilmiş anı kullanır. Kamera isteğindeki gövde durumları A2-K21 gereği `t_cizim` adımının başındaki olaylardan sonraki durumdur.
- **A2-K42.** Sim her istek için o anda **boş** durumdaki bir yuva ayırır (A2-K56). Boş yuva yoksa istek gönderilmez, o periyodun karesi üretilmez ve `halka_dolu` nedeniyle düşmüş sayılır (sensör arızası olarak akar, A2-K49); sim boş yuva oluşturmak için hiçbir yuvayı geri almaz.
- **A2-K43.** Görselleştirici her `kamera_istegi`ne, bağlantı açık kaldıkça, tam bir kez `kare_hazir` ya da `kare_dustu` ile yanıt verir.
- **A2-K44.** Görselleştirici bekleyen istekleri `t_cizim` sırasıyla işler. Bir kamera için çizimine başlanmamış birden çok istek bekliyorsa yalnız en yenisini çizer, eskilerini `kare_dustu(daha_yeni_istek)` ile yanıtlar.
- **A2-K45.** Çizimine başlanmamış bir istek için `kamera_iptal` gelirse görselleştirici onu çizmez ve `kare_dustu(iptal)` gönderir. Çizimi başlamış bir istek iptal edilmez; görselleştirici onu tamamlayıp olağan yanıtı gönderir. (Eşzamanlı geri okuma ana iş parçacığında tek yinelemede bittiği için iptalden sonra gelen bir geri çağrı yoktur; AB-195.)
- **A2-K46.** **Teslim son tarihi.** Sim, sim zamanı teslim adımı `S_d`'ye ulaştığında, o adımın sonuçlarını işlemeye başlamadan önce A2 soketinden okunmuş iletilere bakar. Kare yalnız şu üç koşul birlikte sağlanırsa teslim edilmiş sayılır ve tüketicilere verilir (A2-K61): o isteğin `kare_hazir`'ı alınmıştır, isteğin çalıştırma nesli hâlâ geçerlidir ve isteğin halkası hâlâ **etkin**dir (A2-K86). Yanıt alınmamışsa kare `kamera_gec` nedeniyle düşmüş sayılır ve istek henüz yanıtlanmamışsa sim `kamera_iptal(gec)` gönderir; halka emekliye ayrılmışsa kare emeklilik anında A2-K86'ya göre zaten düşmüştür.
- **A2-K47.** **Sim beklemez.** Sim hiçbir kipte (SIL, HIL 1x, HIL 1x + bariyer, tekrar izleme) bir fizik adımını, bir sensör örneğini ya da bir karta gidecek partiyi A2 yüzünden geciktirmez; görselleştiricinin gecikmesi yalnız düşen kare olarak görünür.
- **A2-K48.** `S_d`'den sonra gelen `kare_hazir` reddedilir: sim yuvayı hemen boşaltır (yuvada henüz tüketici yoktur) ve `gec_hazir` sayacını artırır; kare hiçbir tüketiciye verilmez.
- **A2-K49.** **Kamera karesi yoksa sensör arızası.** Düşen her kare (neden ne olursa olsun, istek hiç gönderilmemiş olsa da) kamera sensörünün o `kare_sirasi`'ndaki kareyi üretemediği anlamına gelir: sim düşmeyi `kare_sirasi` ve nedeniyle A6'ya yazar, nedene göre sayar ve A3'e bildirir, A8 yoluna "kare yok" bilgisini verir (biçimi A8'de). Her `kare_sirasi` için bu kayıt, sayım ve bildirim A2-K97'ye göre **tektir**. `kare_sirasi` sim süreci başlarken 1'dir ve her kamera periyodunda, istek gönderilsin ya da gönderilmesin, 1 artar; yeni çalıştırma, sıfırlama ve atlamada sıfırlanmaz. Sim kameranın yerine bayat kare, başka bir karenin kopyası ya da yer gerçeğinden türetilmiş bir kayıt koymaz.
- **A2-K50.** **Deney geçerliliği.** A5'te `kamera_gerekli` doğru olan bir çalıştırma, çalışma sırasında görselleştirici bağlantısı koparsa ya da çalıştırma boyunca düşen kare oranı %1'i aşarsa (eşik mimari seçimi; 07 §2.4) gerekçesiyle geçersiz işaretlenir. `kamera_gerekli` yanlışsa düşen kareler çalıştırmayı geçersiz kılmaz, yalnız sayılır. Sim her iki durumda da çalışmayı sürdürür.
- **A2-K51.** SIL'de kamera karelerinin teslim edilip edilmemesi duvar saatine bağlıdır; bu yüzden kamera açık bir SIL çalıştırması için tekrarlanabilirlik güvencesi verilmez. Kamera karelerinden türetilen algılama kayıtları A6'ya yazılır ve tekrar oynatma onlardan yapılır (A6'dan beklenen).

### 4.6 Kare halkası: yazma, okuma, sahiplik

- **A2-K52.** **Yazma.** Görselleştirici yalnız istekte kendisine ayrılmış yuvaya, istekteki `halka_nesli`'ne sahip halkada yazar; yuva başlığının bütün alanlarını ve pikselleri yazar, `yazim_sayaci`'nı yuvadaki önceki değerin 1 fazlası yapar, bir serbest bırakma çiti (`std::atomic_thread_fence(std::memory_order_release)`) koyar ve ancak bundan sonra `kare_hazir` gönderir. Yuva başka bir isteğe ayrılmış ya da **tüketicilerde** iken görselleştirici ona yazmaz; sim bunu A2-K57 ile güvenceye alır.
- **A2-K53.** **Okuma ve bellek görünürlüğü.** Bir okuyucu (A8, A6 kayıt yazıcısı) bir yuvayı yalnız sim o kareyi kendisine verdikten sonra ve kendi bırakmasını göndermeden önce okur. Sim, `kare_hazir`'ı aldıktan sonra ve kareyi bir tüketiciye bildirmeden önce bir edinme çiti (`std::atomic_thread_fence(std::memory_order_acquire)`) ardından serbest bırakma çiti koyar; tüketici bildirimi aldıktan sonra, yuvayı okumadan önce bir edinme çiti koyar. Yuva protokolü (A2-K56–K61) bir yuvaya aynı anda yazan ve okuyan bulunmamasını sağladığı için piksel ve başlık erişimleri sıradan (atomik olmayan) bellek erişimleridir ve paylaşımlı bellekte atomik alan yoktur. **Bağlanan varsayım:** desteklenen derleyiciler (clang, MSVC) ve işlemciler (arm64, x86-64) için, bir süreçte serbest bırakma çitinden sonra yapılan soket gönderimi ile öteki süreçte o iletinin alımından sonraki edinme çiti, çitten önceki yazmaları alıcıya görünür kılar. C++20 bu süreçler arası ilişkiyi tanımlamaz (`[atomics.lockfree]`'deki adresten bağımsızlık ifadesi bir "should" notudur); varsayım arm64'te çitlerin iç paylaşılabilir alanı kapsayan `dmb ish` engellerine, x86-64'te işlemcinin güçlü bellek sıralamasına ve iki işletim sisteminde soket sistem çağrısının çekirdek geçişine dayanır, **doğrulanmadı** ve A2-U13 ile sınanır. Okuyucu okumadan önce ve sonra yuva başlığındaki `yazim_sayaci`, `istek_kimligi`, `calisma_nesli` ve `halka_nesli`'nin kendisine bildirilenle aynı olduğunu denetler; farklılık bir protokol ihlalidir, kare kullanılmaz ve olay A3'e bildirilir. Bu denetim **yalnız tanılama içindir**; veri yarışına karşı koruma sayılmaz.
- **A2-K54.** Okuyucular halkayı salt okunur eşler ve paylaşımlı belleğe hiç yazmaz. Yuvaların sahipliğini ve her karenin bekleyen tüketici kümesini yalnız sim tutar; paylaşımlı bellekte sahiplik, okuyucu sayacı ya da atomik alan yoktur (AB-049).
- **A2-K55.** Halka (halka başlığı, yuva başlıkları, pikseller) poz, montaj, iç parametre, hedef konumu ya da başka bir yer gerçeği taşımaz. Kare tüketicisinin ihtiyaç duyduğu kalibrasyon kendi bağlantısından gelir (A8).
- **A2-K56.** Bir yuvanın durumları sim'in belleğinde şunlardır: **boş** → (istek gönderildi) **görselleştiricide** → (`kare_hazir` `S_d`'den önce geldi) **hazır** → (`S_d`; nesil geçerli, halka etkin) **tüketicilerde** (bekleyen tüketici kümesi = o karenin verildiği tüketiciler) → (küme boşaldı) **boş**. `kare_dustu`, geç `kare_hazir` ya da nesli geçersiz bir isteğin yanıtı **görselleştiricide** durumundan doğrudan **boş**a götürür. **Hazır** durumdaki bir kare `S_d`'de tüketicisi yoksa ya da nesli geçersizse doğrudan **boş**a geçer. Yuva **boş** dışındaki hiçbir durumdan, hiçbir zaman aşımıyla **boş**a çekilmez. Halka emekliye ayrılınca yuvalarının durumları §4.10'daki tabloya göre değişir; emekli bir halkanın hiçbir yuvası bir daha **boş**a dönüp yeni bir isteğe ayrılmaz.
- **A2-K57.** Sim **boş** olmayan bir yuvayı yeni bir isteğe ayırmaz. **Görselleştiricide** durumundaki yuva, o isteğe `kare_hazir` ya da `kare_dustu` gelene kadar (iptal edilmiş olsa bile; görselleştirici o sırada yazıyor olabilir) ve **tüketicilerde** durumundaki yuva bekleyen tüketici kümesi boşalana kadar yeniden kullanılmaz.
- **A2-K58.** **Bırakma tüketici başına bir kezdir.** Sim her teslim edilen kare için bekleyen tüketici kümesini tutar. Tüketici kimliği sim'in tarafında tüketicinin bağlantısından türer (süreç içi kayıt yazıcısının sabit bir kimliği vardır); bırakma iletisindeki bir alandan alınmaz. Bir bırakma `(halka_nesli, yuva, istek_kimligi)` üçlüsünü taşır ve yalnız **gönderen** tüketicinin o karedeki bekleyen başvurusunu kaldırır. Üçlü yuvanın o anki karesiyle uyuşmuyorsa ya da gönderen tüketici o karenin bekleyen kümesinde değilse (yinelenen bırakma dahil) bırakma yok sayılır ve sayılır; hiçbir bırakma başka bir tüketicinin başvurusunu kaldıramaz ve aynı başvuruyu ikinci kez kaldıramaz (AB-050).
- **A2-K59.** **Bırakılmayan yuva: halka emekliye ayrılır.** Bir tüketici **etkin** bir halkadan kendisine verilen bir kareyi 1 s (duvar saati; mimari seçimi) içinde bırakmazsa ya da bir tüketicinin bağlantısı, etkin halkada bekleyen başvurusu varken kapanırsa sim yuvayı geri **almaz**; o kameranın halkasını A2-K86–K89'a ve §4.10'daki durum tablosuna göre emekliye ayırır. Yalnız etkin halkadaki bir kare emekliliği tetikler; emekli bir halkadaki bırakma gecikmesi ya da kopma yeni bir emeklilik başlatmaz, yalnız sayılır. Olay `tuketici_gec` ya da `tuketici_koptu` olarak sayılır ve A6'ya yazılır.
- **A2-K60.** Bir tüketicinin bağlantısının kapanması hiçbir başvuruyu kaldırmaz ve hiçbir emekli halkanın serbest bırakılması sayılmaz: o tüketicinin etkin halkada bekleyen başvurusu yoksa başka bir şey yapılmaz; varsa A2-K59 uygulanır (AB-051). O tüketici adına tutulan emekli halkalar oturum sonuna kadar tutulur (A2-K88). Bir tüketici süreci kapandıktan sonra gelen ileti olmayacağı için aynı başvuru ikinci kez azaltılamaz.
- **A2-K61.** Sim teslim edilen kareyi `S_d`'de etkin tüketicilere (o kameranın güncel **etkin** halkasıyla el sıkışmış ve kamera yolu kapalı olmayan A8 bileşeni, A2-K89; kayıt kademesi kameralıysa A6 kayıt yazıcısı) aynı anda verir ve bekleyen kümeyi bu tüketicilerle kurar; tüketicilerden biri olmasa da kare teslim edilmiş sayılır.

### 4.7 Nabız, kopma, çökme ve yeniden bağlanma

- **A2-K62.** Görselleştirici `goruntuleyici_nabzi`ni, sim `sim_nabzi`ni duvar saatine göre 1 s aralıkla gönderir. Bir taraf karşıdan 3 s (duvar saati; mimari seçimi) boyunca hiçbir ileti almazsa karşıyı ölü sayar, `veda(nabiz_zaman_asimi)` gönderir ve bağlantıyı kapatır (AB-202).
- **A2-K63.** Görselleştirici sim'e bağlanamazsa ya da bağlantı koparsa son sahneyi dondurur, "bağlantı yok" gösterir, hiçbir kare üretmez ve hiçbir şeyi simüle etmez, halkayı eşlemeden çıkarır ve 1 s aralıkla yeniden bağlanmayı dener.
- **A2-K64.** Görselleştirici bağlantısı herhangi bir nedenle kapanınca A2 oturumu biter ve sim: yanıtlanmamış ve **henüz sonuçlandırılmamış** (A2-K97) istekleri `gorsellestirici_yok` nedeniyle düşürür; daha önce sonuçlandırılmış isteklere (ör. A2-K81 ile `calisma_nesli_degisti`, A2-K86 ile `halka_yenilendi` ya da A2-K46 ile `kamera_gec` nedeniyle düşmüş) yeni bir düşme, kayıt ya da sayım üretmez, yalnız yuvalarını kapatır; o oturumun bütün halkalarını (etkin ve emekli) teslime kapatır, henüz sonuçlandırılmamış **hazır** karelerini `gorsellestirici_yok` nedeniyle düşürür, adlarını kaldırır ve oturum sonu olduğu için kendi eşlemelerini ve tutamaklarını bırakır (A2-K88; tüketicilerin kendi eşlemeleri geçerli kalır, okudukları karelere kimse yazmaz); tüketicilere oturum sonunu A8 yoluyla bildirir; bağlantı yokken her kamera periyodunu `gorsellestirici_yok` nedeniyle düşen kare sayar; fizik, HIL, sonar ve diğer sensörler çalışmayı sürdürür (A2-K47); yeni bağlantıyı kabul etmeye hazırdır.
- **A2-K65.** Her yeni oturumda sim yeni bir `oturum_kimligi` ve her kamera için yeni bir halka nesnesi (yeni ad, yeni nesil) oluşturur ve tam durumu yeniden gönderir (`hosgeldin` ile yaşayan gövdeler, `dalga_kumesi`, `ortam`, `malzeme_optik`). Eski oturumun halkası yeni oturumda kullanılmaz.
- **A2-K66.** Sıfırlama ve atlamada sim A2-K81–K84'ü uygular.
- **A2-K67.** Taraflardan biri olağan biçimde kapanırken `veda(olagan)` ya da `veda(sim_kapaniyor)` gönderir; sim bundan önce yanıtlanmamış her istek için `kamera_iptal(oturum_sonu)` gönderir. `veda`dan sonra gelen iletiler yok sayılır.
- **A2-K68.** Görselleştirici hiç çalışmıyorsa (3B kademesi açık ama bağlantı yok) sim A2-K64'teki "bağlantı yok" davranışıyla çalışır; sim görselleştiriciyi başlatmayı beklemez.

### 4.8 Paylaşımlı bellek adlandırması ve yaşam döngüsü

- **A2-K70.** Halka nesnesinin adı `ida2-<oturum>-c<kamera>-g<nesil>` biçimindedir: `<oturum>` oturum kimliğinin düşük 32 bitinin 8 haneli küçük harf onaltılık yazımı, `<kamera>` kamera kimliğinin onluk yazımı, `<nesil>` halka neslinin 8 haneli küçük harf onaltılık yazımı.
- **A2-K71.** **macOS.** Ad başına `/` eklenir (örnek `/ida2-1a2b3c4d-c0-g00000001`, 27 karakter). xnu'da POSIX paylaşımlı bellek adı en çok `PSHMNAMLEN` = 31 bayttır (`bsd/sys/posix_shm.h`; `bsd/kern/posix_shm.c` adı `PSHMNAMLEN + 1` baytlık tampona NUL dahil kopyalar). Sim nesneyi `shm_open(ad, O_CREAT | O_EXCL | O_RDWR, 0600)` ile oluşturur ve boyutunu tek bir `ftruncate` ile verir (xnu bir nesnenin ömründe yalnız bir `ftruncate`'e izin verir: `posix_shm.c`, "We only allow one ftruncate() per lifetime"); halkanın boyutu nesnenin ömrü boyunca değişmez. Görselleştirici `O_RDWR`, okuyucular `O_RDONLY` ile açar.
- **A2-K72.** **Windows.** Ad başına `Local\` eklenir (oturum ad alanı; örnek `Local\ida2-1a2b3c4d-c0-g00000001`). Sim nesneyi `CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, …)` ile sayfa dosyası destekli olarak oluşturur; görselleştirici yazma, okuyucular okuma erişimiyle `OpenFileMappingW` kullanır. Nesne son tutamak ve son görünüm kapanınca işletim sistemince kaldırılır (Microsoft, `CreateFileMappingA` belgesi).
- **A2-K73.** Oluşturma sırasında ad zaten varsa (macOS `EEXIST`, Windows `ERROR_ALREADY_EXISTS`) sim mevcut nesneyi kullanmaz, kendi tutamağını kapatır ve nesli artırıp yeni adla yeniden dener; 16 denemede başaramazsa kamera o oturum için kullanılamaz ve her kare `halka_hatasi` nedeniyle düşer.
- **A2-K74.** Görselleştirici ve okuyucular halkayı eşledikten sonra halka başlığının `sihirli`, `duzen_ana`, `oturum_kimligi`, `halka_nesli`, `yuva_sayisi`, `yuva_boyutu`, `toplam_boyut`, `genislik`, `yukseklik`, `satir_adimi`, `piksel_bicimi` alanlarının kendilerine bildirilen değerlerle aynı olduğunu ve eşlenen boyutun `toplam_boyut`'a eşit olduğunu denetler; farklıysa halkayı kullanmaz (görselleştirici `hazir.durum` ya da `halka_eslendi.durum` = `halka_duzeni_uyusmaz`).
- **A2-K75.** **Kalmış nesneler (yalnız macOS).** POSIX paylaşımlı bellek nesneleri `shm_unlink` edilene kadar süreç çöktüğünde de kalır ve macOS'ta bunları listelemenin belgelenmiş bir yolu bilinmiyor (doğrulanmadı; Linux'taki `/dev/shm` dizininin karşılığı yok). Bu yüzden 07 §3.4'teki "önekle eşleşen eski nesneleri sil" yöntemi uygulanamaz. Sahiplik kilidinin sahibi (A2-K77), oluşturduğu her halka adını kullanıcının geçici dizinindeki `ida/a2_halkalar.txt` kayıt dosyasında tutar. Kayıt dosyası yalnız kilidin sahibi tarafından ve her zaman atomik olarak güncellenir: yeni içerik aynı dizinde geçici bir dosyaya yazılıp diske gönderilir (`fsync`) ve `rename` ile kayıt dosyasının üzerine taşınır (macOS `rename(2)`: yeni ad, sistem işlem ortasında çökse bile her zaman var olur).
- **A2-K76.** Sim halkanın bütün yuva başlıklarını oluşturma sırasında sıfırlar (`yazim_sayaci = 0`); Windows'ta sayfa dosyası destekli yeni nesnenin içeriği zaten sıfırdır (Microsoft belgesi), macOS'ta sim açıkça sıfır yazar.
- **A2-K77.** **Sahiplik kilidi.** Sim süreci başlarken, A2 dinleyicisini açmadan ve herhangi bir halka oluşturmadan önce, kullanıcının geçici dizinindeki `ida/a2.kilit` dosyasında işletim sisteminin özel dosya kilidini bloklamadan almayı dener: macOS'ta `flock(fd, LOCK_EX | LOCK_NB)`, Windows'ta `LockFileEx(…, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, …)`. Kilit süreç ömrü boyunca tutulur. Paylaşımlı bellek nesnelerini yalnız kilidin sahibi oluşturur ve kaldırır; A2 dinleyicisini yalnız kilidin sahibi açar. Temizlik yetkisi porttan bağımsızdır: hangi portta dinlendiği kilidi etkilemez. Kilit süreç sonlanınca işletim sistemince bırakılır (Windows: Microsoft `LockFileEx` belgesi, "bırakma süresi sistem kaynaklarına bağlıdır"; macOS: `flock(2)` kilitleri dosyaya bağlıdır, son tanımlayıcı kapanınca bırakılması BSD davranışıdır, kılavuzda açıkça yazmıyor, **doğrulanmadı**, A2-U21 sınar).
- **A2-K78.** **Oluşturma, kaldırma ve çökme penceresi (macOS).** Kilidin sahibi bir halkayı şu sırayla oluşturur: (1) adı kayıt dosyasına atomik olarak ekler, (2) `shm_open(O_CREAT | O_EXCL)` ile oluşturur. Kaldırırken: (1) `shm_unlink`, (2) adı kayıt dosyasından atomik olarak siler. Böylece var olan her nesnenin adı kayıt dosyasındadır; kayıt dosyasında olup nesnesi bulunmayan ad yalnız (1) ile (2) arasındaki bir çökmeden kalır. **Açılıştaki kurtarma:** kilidi alan sim, kayıt dosyasındaki her ad için `shm_unlink` çağırır (`ENOENT` hata sayılmaz) ve ardından kayıt dosyasını atomik olarak boşaltır. Kilidi alabilmek, kayıttaki adları oluşturan önceki sahibin sonlanmış olduğunu gösterir; bu yüzden yaşayan başka bir sim sürecinin nesnesi hiçbir zaman kaldırılmaz. Önceki sahibin halkalarını hâlâ eşlemiş tüketiciler (eski A8 ya da görselleştirici süreci) etkilenmez: ad kaldırılır, eşlenmiş görünüm geçerli kalır. Windows'ta adlandırılmış eşleme nesneleri son tutamakla birlikte kalktığı için kayıt dosyası ve açılış kurtarması yoktur.
- **A2-K79.** **Aynı makinede ikinci sim süreci.** İzinlidir, ama kilidi alamayan sim A2'siz çalışır: A2 dinleyicisini açmaz, paylaşımlı bellek nesnesi oluşturmaz ve kaldırmaz, kayıt dosyasına dokunmaz; kamera kademesi kapalı değilse her kare `gorsellestirici_yok` nedeniyle düşer ve durum A3'e "A2 sahiplik kilidi başka bir sim sürecinde" uyarısıyla bildirilir. Fizik, HIL ve diğer sensörler etkilenmez.

### 4.9 Çalıştırma nesli: yeni çalıştırma, sıfırlama ve atlama

- **A2-K80.** **Çalıştırma nesli.** Sim bir zaman çizgisi kimliği olarak 32 bitlik bir çalıştırma nesli tutar. Sim süreci başlarken nesil 1'dir ve süreçteki ilk çalıştırma bu nesille başlar. Nesli artıran olayların **kapalı listesi** şudur ve başka hiçbir olay nesli değiştirmez: (1) **yeni çalıştırma** — aynı sim sürecinde bir çalıştırmanın bitip yenisinin başlaması (A3 çalıştırma kontrolü; senaryo yeniden yüklensin ya da yüklenmesin; A2-K96); (2) **sıfırlama** (`sifirlama` olayı); (3) **atlama** (`atlama` olayı, tekrar izleme). Her birinde nesil, olayın uygulandığı adımda tam 1 artar; sıfırlama ve atlamada yeni değer olayın `calisma_nesli` alanında, yeni çalıştırmada yeni A2 oturumunun `hosgeldin.calisma_nesli` alanında bildirilir; A8 bileşenine bildirim A8-K78'deki iletiyle gider. Çalıştırma nesli sıfırlama ve atlamaya ek olarak aynı sim sürecindeki her yeni çalıştırmada da tam 1 artar ve aynı olay/bildirim sırasıyla bildirilir; önceki çalıştırmanın kareleri A2-K81–K84 ile geçersizleşir (A8-K17, K78, K79 bu artışa atıf verir). Bu üç olay sim zamanının süreksiz olduğu durumların tamamıdır ve her nesil değişimi A1'de oturumun kapanması ve yeni oturumun kurulmasıdır (A1-K128; W1-uyum U19); karta ayrı bir nesil bildirimi gitmez. `hosgeldin`, her `kamera_istegi`, `kare_hazir`, `kare_dustu` ve yuva başlığı ait oldukları neslin değerini taşır.
- **A2-K81.** Nesil arttığında sim, eski nesle ait yanıtlanmamış ve henüz sonuçlandırılmamış (A2-K97) bütün istekler için `kamera_iptal(calisma_nesli_degisti)` gönderir ve onları `calisma_nesli_degisti` nedeniyle **sonuçlandırır** (tek düşme, tek kayıt, tek sayım); bu isteklerin yuvaları yanıt gelene ya da A2 oturumu bitene kadar **görselleştiricide** kalır (A2-K57); gelen yanıt (`kare_hazir` ya da `kare_dustu`) ya da oturum sonu yalnız yuvayı kapatır, yeni bir düşme üretmez ve kare hiçbir tüketiciye verilmez.
- **A2-K82.** Nesil arttığında **hazır** durumdaki (yanıtı alınmış, `S_d`'yi bekleyen) eski nesil kareleri teslim edilmez: sim yuvalarını hemen **boş**a döndürür ve kareleri `calisma_nesli_degisti` nedeniyle sonuçlandırır (A2-K97).
- **A2-K83.** **Tüketiciye verilmiş karenin yaşam döngüsü.** Nesil arttığında **tüketicilerde** durumundaki eski nesil kareleri için: sim bu karelerden türetilen sonuçları (algılama kayıtları) geçersiz sayar, kabul etmez, karta iletmez ve `eski_nesil` nedeniyle sayar; tüketici, A8 sözleşmesindeki nesil bildirimini alınca eski nesle ait sonuçları göndermez, okumayı bitirir ve yuvayı A2-K58'e göre **yine bırakır** (bırakma nesilden bağımsız kabul edilir); yuva ancak bekleyen kümesi boşalınca **boş**a döner. A6 kayıt yazıcısı zaten kaydettiği eski nesil karelerini eski nesil etiketiyle bırakır; tekrar oynatmada bu kareler yeni zaman çizgisine taşınmaz.
- **A2-K84.** Sim, nesli `calisma_nesli`'nden farklı olan bir `kamera_istegi` yanıtını, yuva başlığını ya da tüketici sonucunu hiçbir zaman güncel neslin verisi olarak kullanmaz. Görselleştirici, bildiği son nesilden eski nesilli bir istek alırsa (iletiler sıralı olduğu için olmaması gerekir) `kare_dustu(tutarsiz_surum)` ile yanıtlar.
- **A2-K85.** Nesil değişimi `kare_sirasi`'nı, `istek_kimligi`'ni ve halka neslini değiştirmez ve halkayı kendiliğinden emekliye ayırmaz; sıfırlama ve atlama A2 oturumunu da değiştirmez (yeni çalıştırma için A2-K96). Emekli bir halkada **tüketicilerde** durumunda kalmış eski nesil kareleri için A2-K83 aynen uygulanır (sonuçlar reddedilir, bırakma kabul edilir, yuva yeniden kullanılmaz).
- **A2-K96.** **Yeni çalıştırmanın sırası.** Aynı sim sürecinde yeni bir çalıştırma başlarken sim, sırasıyla: (1) önceki çalıştırmanın son fizik adımından sonra nesli artırır (A2-K80) ve A2-K81–K84'ü uygular: yanıtlanmamış ve henüz sonuçlandırılmamış istekler `kamera_iptal(calisma_nesli_degisti)` ile iptal edilir ve bu nedenle **sonuçlandırılır**, sonuçlandırılmamış hazır kareler teslim edilmeden aynı nedenle sonuçlandırılır (A2-K82), tüketicideki karelerin sonuçları reddedilir; (2) A8 bileşenine nesil bildirimini gönderir (A8-K78); (3) görselleştiriciye `veda(yeni_calistirma)` gönderip A2 oturumunu kapatır ve A2-K64'ü uygular: (1)'de sonuçlandırılmış isteklerin ve karelerin yuvaları yalnız kapatılır, yeni düşme, kayıt ya da sayım üretilmez (A2-K97); halkalar serbest bırakılır, A8'e oturum sonu bildirilir (A8-K75); (4) yeni çalıştırmanın ilk fizik adımından önce yeni bağlantıyı kabul etmeye hazırdır; görselleştirici A2-K63'e göre yeniden bağlanır ve yeni oturum, yeni çalıştırmanın senaryosuyla (manifest, dalga kümesi, ortam, kamera yapılandırması, yaşayan gövdeler) ve yeni nesille tam el sıkışmayla kurulur (A2-K65). Önceki çalıştırmanın hiçbir isteği, karesi ya da sonucu yeni çalıştırmada kullanılmaz; yeni çalıştırmanın fiziği A2 bağlantısını beklemez (A2-K47).
- **A2-K97.** **Kare sonucunun tekliği.** Her `kare_sirasi` için sim tam bir **nihai sonuç** üretir: ya teslim (A2-K46, K61) ya da tek bir nedenle düşme (A2-K49). Kare, bu sonuçlardan birini ilk üreten kuralın uygulandığı anda **sonuçlandırılmış** olur; düşme nedeni, A6 kaydı, sayaç artışı ve A8'e "kare yok" bildirimi o kuralın ürettiği tek değerdir ve sonradan değişmez. Sonraki hiçbir kural (A2-K42, K46, K48, K59, K64, K81, K82, K86, K96 dahil) sonuçlandırılmış bir kareyi yeniden düşürmez, ikinci bir neden, kayıt, sayım ya da bildirim üretmez. **Yuvanın kapatılması ayrı bir işlemdir:** bir isteğin ya da karenin yuvası, kare sonuçlandırıldıktan sonra (geç yanıt, oturum sonu, emekliliğin bitişi ile) kapatılabilir ve bu kapatma hiçbir düşme üretmez. `gec_hazir`, `emekli_halka_yaniti`, yok sayılan bırakma gibi sayaçlar yanıt ve ileti sayaçlarıdır, düşme sayacı değildir; A2-K50'deki düşen kare oranı yalnız nihai sonuçlardan hesaplanır. Örnek (yeni çalıştırma): `kare_sirasi` = 1000 olan isteğe yanıt gelmemişken yeni çalıştırma başlar; A2-K81 kareyi `calisma_nesli_degisti` ile sonuçlandırır (bir kayıt, bir sayım); A2-K96 (3) ile oturum kapanınca A2-K64 bu isteğin yuvasını yalnız kapatır, `gorsellestirici_yok` nedeniyle hiçbir şey saymaz.

### 4.10 Halka emekliliği

Bu başlıktaki "taraf", bir halkaya bağlı olan görselleştirici ya da bir tüketicidir (A8 bileşeni bağlantısı; süreç içi A6 kayıt yazıcısı). Bir taraf, bir halkayla el sıkışmışsa (görselleştirici: `hosgeldin` ya da `halka_eslendi(durum = 0)`; A8 tüketicisi: A8 sözleşmesindeki halka el sıkışması; kayıt yazıcısı: halka oluşturulunca kendiliğinden) o halkaya **bağlı**dır. Halkanın üç durumu vardır: **etkin** (yuva ayrılır, teslim yapılır), **emekli** (ne yuva ayrılır ne teslim yapılır; sim eşlemesini tutar), **serbest** (sim kendi eşlemesini ve tutamağını bırakmıştır). Bir kamerada aynı anda en çok bir etkin halka vardır.

- **A2-K86.** **Emeklilik anı.** Halka `H` emekliye ayrıldığı adımda sim, sırasıyla ve aynı adımda: (1) `H`'yi **emekli** yapar: `H`'den yeni yuva ayrılmaz, `H`'den hiçbir kare bir daha teslim edilmez; (2) `H`'nin **hazır** yuvalarındaki kareleri teslim etmeden düşürür, `halka_emekli_hazir` nedeniyle sayar ve A6'ya yazar (`SimKareDusmeNedeni` = 8); (3) `H`'deki yanıtlanmamış istekler için `kamera_iptal(halka_yenilendi)` gönderir ve bunlardan henüz sonuçlandırılmamış olanları `halka_yenilendi` nedeniyle sonuçlandırır (A2-K97; örneğin `S_d`'de `kamera_gec` ile düşmüş bir istek yeniden sayılmaz); (4) **tüketicilerde** yuvalarının bekleyen kümelerini değiştirmez: o kareler teslim edilmiş geçerli karelerdir, tüketiciler okumayı bitirebilir ve sonuçlarını gönderebilir (nesil geçerliyse kabul edilir), bırakmalar A2-K58'e göre kaydedilir; (5) `H`'nin adını kaldırır (macOS `shm_unlink`; Windows'ta ad sim'in tutamağı açık kaldıkça çözülebilir kalır, yeni bir taraf ona yönlendirilmez); (6) yeni bir nesille yeni halka `H'`'yi oluşturur (A2-K70–K73) ve **etkin** yapar; (7) görselleştiriciye `halka_yenile(H')` gönderir; (8) A2-K89'a göre izinli her tüketiciye A8 yoluyla `H'`'yi bildirir. `H'` için `halka_eslendi(durum = 0)` gelene kadar o kameranın kareleri `halka_yenileniyor` nedeniyle düşer.
- **A2-K87.** **Emekli halkaya atıf yapan yanıtlar.** Emeklilikten sonra gelen ve emekli bir halkanın nesliyle yuva ya da isteğe atıf yapan `kare_hazir` ve `kare_dustu` yalnız ilgili isteği kapatmak için işlenir: istek yanıtlanmış sayılır, hiçbir teslim üretilmez, yuva hiçbir isteğe ayrılmaz ve yanıt `emekli_halka_yaniti` sayacında sayılır. Emekli halkaya ait bırakmalar A2-K58'e göre kaydedilir ama hiçbir yuvayı kullanıma açmaz. Emekli halkada bir tüketicinin bırakma süresini aşması ya da bağlantısının kopması yeni emeklilik başlatmaz, yalnız sayılır.
- **A2-K88.** **Eski eşlemenin tutulması ve serbest bırakılması.** Sim emekli halka `H`'nin kendi eşlemesini ve tutamağını, emeklilik anında `H`'ye bağlı olan **her** taraf `H`'den daha yeni bir halkayla el sıkışana (görselleştirici: `halka_eslendi(durum = 0)` ve `halka_nesli` > `H`'nin nesli; A8 tüketicisi: A8'deki yeni halka el sıkışması; kayıt yazıcısı: yeni halkanın oluşturulması) ya da A2 oturumu bitene (A2-K64) kadar bırakmaz; ikisinden biri olunca `H` **serbest** olur. Bir tarafın bağlantısının kopması el sıkışma yerine geçmez; bağlantısı kopan tüketicinin bağlı olduğu emekli halkalar oturum sonuna kadar tutulur.
- **A2-K89.** **Tüketici başına emekli halka sınırı.** Bir tüketici adına aynı anda tutulan emekli halka sayısı en çok 2'dir (mimari seçimi, tahmin; `kTuketiciBasinaAzamiEmekliHalka`). Bir emeklilik bu sayıyı bir tüketici için 2'nin üstüne çıkaracaksa yeni halka yine kurulur (görselleştirici ve diğer tüketiciler için), ama o tüketiciye bildirilmez ve verilmez: o tüketici için kamera yolu **kapalı**dır, olay `tuketici_kamera_kapali` olarak A3'e bildirilir ve A6'ya yazılır. Kamera yolu kapalı tüketici, yalnız yeni bir bağlantı ve A8'deki tam el sıkışmayla, yeni bir tüketici kimliği olarak geri döner; eski kimliği adına tutulan emekli halkalar oturum sonuna kadar tutulur. Not: A2-K61 (tüketiciye yalnız el sıkıştığı etkin halkadan kare verilir) ve A2-K88 (daha yeni halkayla el sıkışma eski halkayı o tüketici için bırakır) birlikte uygulandığında, kurallara uyan bir akışta bir tüketici adına en çok bir emekli halka birikir (yürütülebilir durum modeliyle sınandı; A2-U24); sınır, bu iki kuraldan birinin uygulamada çiğnenmesine karşı savunmadır ve her emeklilikte denetlenir.

**Emeklilik durum tablosu.** "Halka" sütunu yuvanın ait olduğu halkanın durumudur; "—" her durumda.

| # | Halka | Yuva | Olay | Sonuç |
|---|---|---|---|---|
| E1 | etkin | boş | kamera periyodu, taraf bağlı | yuva ayrılır, `kamera_istegi`; yuva **görselleştiricide** |
| E2 | etkin | (boş yuva yok) | kamera periyodu | istek yok; kare `halka_dolu` (A2-K42) |
| E3 | etkin | görselleştiricide | `kare_hazir`, `S_d`'den önce, nesil geçerli | **hazır** |
| E4 | etkin | görselleştiricide | `kare_dustu` | **boş**; görselleştirici nedeniyle düşme |
| E5 | etkin | görselleştiricide | `S_d`, yanıt yok | `kamera_gec` düşme, `kamera_iptal(gec)`; yuva **görselleştiricide** kalır |
| E6 | etkin | görselleştiricide (iptal edilmiş) | geç `kare_hazir` ya da `kare_dustu` | **boş**; `gec_hazir` |
| E7 | etkin | hazır | `S_d`, nesil geçerli | tüketici varsa **tüketicilerde** (küme = el sıkışmış izinli tüketiciler, A2-K61), yoksa **boş** |
| E8 | etkin | tüketicilerde | kümedeki tüketiciden bırakma | küme küçülür; boşalırsa **boş** |
| E9 | — | tüketicilerde | yinelenen ya da yabancı bırakma | yok sayılır, sayılır (A2-K58) |
| E10 | etkin | tüketicilerde | bırakma süresi aşıldı ya da tüketici bekleyen başvuruyla koptu | **emeklilik** (E11–E14) |
| E11 | etkin → emekli | hazır | emeklilik | düşer: `halka_emekli_hazir`; teslim yok (A2-K86 (2)) |
| E12 | etkin → emekli | görselleştiricide | emeklilik | `kamera_iptal(halka_yenilendi)`; düşme `halka_yenilendi` |
| E13 | etkin → emekli | tüketicilerde | emeklilik | küme korunur; okuma ve geçerli sonuçlar sürer |
| E14 | etkin → emekli | boş | emeklilik | bir daha ayrılmaz |
| E15 | emekli | görselleştiricide | `kare_hazir` ya da `kare_dustu` | istek kapanır; teslim yok; `emekli_halka_yaniti` (A2-K87) |
| E16 | emekli | — | kamera periyodu | bu halkadan yuva ayrılmaz; yeni halka eşlenmişse oradan (E1), değilse `halka_yenileniyor` |
| E17 | emekli | — | `S_d` | teslim yok (A2-K46) |
| E18 | emekli | tüketicilerde | bırakma | küme küçülür; yuva yeniden kullanılmaz |
| E19 | emekli | tüketicilerde | bırakma süresi aşıldı ya da tüketici koptu | yeni emeklilik yok; sayılır |
| E20 | emekli | — | emeklilik anında bağlı her taraf daha yeni halkayla el sıkıştı | **serbest**: sim eşlemesini ve tutamağını bırakır (A2-K88) |
| E21 | emekli | — | A2 oturumu bitti | **serbest** |
| E22 | etkin | — | A2 oturumu bitti | henüz sonuçlandırılmamış hazır kareler ve istekler `gorsellestirici_yok` ile düşer, sonuçlandırılmış olanların yuvası yalnız kapanır (A2-K97); teslim yok; **serbest** (A2-K64) |
| E23 | — | — | çalıştırma nesli değişti | A2-K81–K85; halka durumu değişmez |
| E24 | etkin → emekli | — | emeklilik bir tüketicinin tuttuğu emekli halka sayısını 2'nin üstüne çıkarıyor | yeni halka kurulur; o tüketiciye bildirilmez; tüketicinin kamera yolu kapalı (A2-K89) |

### 4.11 Atlama sonrası sahnenin yeniden kurulması

- **A2-K90.** **Gövde tablosu.** Sim her `atlama` olayından hemen sonra, yeni zamandaki ilk `durum` ve ilk `kamera_istegi` iletisinden önce, tek bir `govde_tablosu` iletisi gönderir: başlık (`t_ns` yeni zaman, `adim_no`, `calisma_nesli` yeni nesil, `govde_sayisi`) ve ardından yeni zamanda yaşayan **bütün** gövdeler için `hosgeldin`'deki gövde tablosuyla aynı biçimdeki `GovdeBilgisi` girdileri (`govde_kimligi`, `sureklilik_no`, `varlik_indeksi`, `govde_turu`, `dolu_tupler`). Blok tek iletidir; başlangıcı `atlama` olayı, sonu `govde_tablosu` iletisidir. Bloğun içinde `govde_dogdu` ve `govde_oldu` olayları gönderilmez.
- **A2-K91.** **Alıcının davranışı.** Görselleştirici `atlama` olayını alınca gövde geçmişini siler ve `govde_tablosu` gelene kadar sahneyi değiştirmez. `govde_tablosu` gelince: tabloda olmayan her gövdeyi ölmüş sayar ve sahneden kaldırır; tablodaki her gövdeyi varlık indeksi ve türüyle (gerekirse yeniden yükleyerek) sahneye koyar; süreklilik numaralarını tablodaki değerlerle başlatır. `atlama` ile `govde_tablosu` arasında gelen `durum` ya da `kamera_istegi`, `t_ns`'i atlama zamanından farklı bir `govde_tablosu` ve tablodaki `calisma_nesli`'nin atlama olayındakinden farklı olması protokol hatasıdır (A2-K06). `govde_tablosu`'ndan sonraki ilk `durum` iletisinin `t_ns`'i tablonun `t_ns`'ine eşittir ve gövde listesi tablodaki kimliklerle birebir aynıdır.
- **A2-K92.** **Kimlik eşlemesi.** `govde_kimligi` oturum boyunca tek bir gövdeyi gösterir ve hiçbir zaman başka bir gövdeye verilmez: bir kimlik oturum içinde ilk görüldüğü varlık indeksi ve türle her zaman birlikte gelir; farklı bir varlık indeksi ya da türle gelmesi protokol hatasıdır. Tekrar izleme kipinde kimlik kayıttaki gövdenin kimliğidir; atlamadan sonra bir kimliğin yeniden görünmesi (ör. geri atlamada daha önce ölmüş bir hedef) aynı gövdenin başka bir zamandaki halidir ve yeni bir gövde değildir. Varlık indeksi (`varlik_indeksi`) bir kimlik değildir: aynı A7 varlığını birden çok gövde kullanabilir (ör. iki hedef); gövdeler yalnız `govde_kimligi` ile ayırt edilir. Sıfırlama (A2-K29) kimlikleri yeniden kullanmaz: sıfırlamadan sonra doğan gövdeler yeni kimlik alır.

### 4.12 Sim yapılandırması, kamera çerçevesi ve gürültü anahtarı (W1-uyum U13, U17)

- **A2-K93.** **Sim yapılandırması.** Aşağıdaki süreç ayarları senaryo alanı **değildir** (A5'te yer almaz, çözümlenmiş senaryoya ve özetine girmez, simülasyonun sonucunu değiştirmez); sim sunucusunun ve görselleştiricinin komut satırı seçenekleriyle verilir, A6 üst verisine yazılır (A6'dan beklenen):

  | Ayar | Seçenek (süreç) | Geçerli | Varsayılan | Kural |
  |---|---|---|---|---|
  | A2 portu | `--a2-port <ondalık>` (sim ve görselleştirici) | 1024 … 65 535 | 47210 | A2-K01 |
  | Halka yuva sayısı | `--a2-yuva <ondalık>` (sim) | 4 … 32 | 8 (tahmin) | §3.7 |
  | Operatör gecikmesi `D_op` | `--a2-operator-gecikme-us <ondalık>` (görselleştirici) | 0 … 100 000 µs | 33 333 µs (tahmin) | A2-K31 |

  Paylaşımlı bellek adı ayar değildir: biçimi A2-K70–K72 ile sabittir. Sahiplik kilidi ve kayıt dosyasının yeri (A2-K75, K77) de sabittir. Geçersiz bir seçenek değeriyle sim ya da görselleştirici başlamaz ve nedenini bildirir.
- **A2-K94.** **S çerçevesinden optik çerçeveye sabit dönüşüm.** Kamera montajı (`montaj_l_b`, `montaj_q_bs`) S çerçevesinde verilir: x ileri (optik eksen), y sağ, z aşağı; bu, A5'in sensör montaj kabulü ve A8'in sensör çerçevesiyle aynıdır. İç parametrelerin ve izdüşümün tanımlandığı optik çerçeve (§3.6: x sağ, y aşağı, z ileri) ile S arasındaki dönüşüm sabittir:

  ```
  v_S = R_SO · v_O,   R_SO = [[0, 0, 1], [1, 0, 0], [0, 1, 0]],   det R_SO = +1
  (z_O → x_S,  x_O → y_S,  y_O → z_S)
  Optik çerçeveden gövdeye: R_bO = R(montaj_q_bs) · R_SO
  ```

  Görselleştirici kamerayı `R_bO` ve `montaj_l_b` ile konumlar (Godot'ya geçiş A2 §2'deki `C` ile); `montaj_q_bs` hiçbir tarafça optik çerçeveden gövdeye dönüş gibi yorumlanmaz. **Uygunluk örnekleri** (hesap, betikle yeniden üretildi): (a) `montaj_q_bs = (1, 0, 0, 0)` → optik eksen (`z_O`) gövde +x'e (ileri), görüntünün sağı (`x_O`) gövde +y'ye (sancak), görüntünün aşağısı (`y_O`) gövde +z'ye (aşağı) bakar; (b) `montaj_q_bs = (cos 45°, 0, −sin 45°, 0)` (gövde y ekseni etrafında −90°) → optik eksen gövde +z'ye (aşağı bakan kamera), görüntünün sağı gövde +y'ye, görüntünün aşağısı gövde −x'e (geriye) bakar. Sabit `a2_wire.h`'de `kR_SO`'dur.
- **A2-K95.** **Gürültü anahtarı.** `kamera_istegi.gurultu_tohumu`, A5 Ek D'nin rastgelelik kataloğundaki `a2/kamera_gurultu` alt akışından, A5'in yazdığı türetim (anahtar, sayaç tahsisi; yasak ya da sıfır sonuçta yeniden çekiliş) ile o kamera karesi için üretilen değerdir (W1-uyum U17). Türetimin kendisi A5'tedir; A2 onu yinelemez. Sim değeri isteğe yazar; görselleştirici kamera gürültüsünü yalnız bu değeri anahtar alarak üretir ve kendi rastgele üretecini kullanmaz. Aynı ana tohum ve aynı `kare_sirasi` aynı `gurultu_tohumu`'nu verir.

---

## 5. Hata durumları

| # | Durum | Algılama koşulu | Sim | Görselleştirici |
|---|---|---|---|---|
| H01 | Protokol hatası | A2-K06, A2-K07 | `veda(protokol_hatasi)`, kapatır; A2-K64 | `veda(protokol_hatasi)`, kapatır; A2-K63 |
| H02 | Sürüm uyuşmazlığı | `merhaba.surum_ana ≠ 1` | `ret(surum)`, kapatır | yeniden bağlanmaz, nedeni gösterir (A2-K16) |
| H03 | Manifest özeti uyuşmuyor | A2-K14 | `ret(manifest)`; kamera kullanılamaz; çalıştırma sürer (kamera gerekliyse geçersiz) | sahneyi kurmaz; yeniden bağlanmaz; nedeni gösterir |
| H04 | Manifest okunamadı ya da A7 kuralı ihlal edildi | A7-K53 | `hazir.durum` 1–5 → `ret(manifest)` | `hazir.durum` = 1 okunamadı, 2 geçersiz, 3 varlık eksik, 4 varlık özeti uyuşmaz, 5 varlık kuralı |
| H05 | Halka eşlenemedi ya da başlığı uyuşmuyor | A2-K74 | `hazir.durum` 6–7 → `ret(halka)` | `hazir.durum` = 6 ya da 7 |
| H06 | `merhaba` ya da `hazir` zamanında gelmedi | A2-K10, A2-K15 | kapatır / `ret(zaman_asimi)` | A2-K63 |
| H07 | İkinci görselleştirici | A2-K03 | `ret(mesgul)` yeni bağlantıya | A2-K63 |
| H08 | Görselleştirici çöktü, kapandı ya da donuk | soket kapandı ya da A2-K62 | A2-K64; kamera gerekliyse çalıştırma geçersiz (A2-K50) | — |
| H09 | Sim yok ya da çöktü | bağlanamama, soket kapandı ya da A2-K62 | — | A2-K63 |
| H10 | Kare geç | A2-K46 | `kamera_gec` düşmesi; `kamera_iptal(gec)`; A2-K49 | A2-K45 |
| H11 | Geç `kare_hazir` | A2-K48 | yuvayı boşaltır, `gec_hazir` sayar | — |
| H12 | Halka dolu | A2-K42 | istek göndermez, `halka_dolu` sayar | — |
| H13 | Geri okuma başarısız | görselleştirici | — | `kare_dustu(geri_okuma_hatasi)`; ardışık 30 hatada (mimari seçimi) A3'e "kamera sağlıksız" gider (sim bu sayacı `kare_dustu`'lardan tutar) |
| H14 | Geçersiz istek ya da tutarsız sürüm | §3.5 | düşen kare sayar | `kare_dustu(gecersiz_istek | tutarsiz_surum)` |
| H15 | Gönderme kuyruğu doldu | A2-K26 | `durum` seyreltir; gerekirse `veda(tampon_tasti)` | `sira` boşluklarını sayar |
| H16 | Gövde durumu geçersiz (NaN, birim dışı kuaterniyon) | A2-K27 | — | son geçerli poz, sayaç; kamera isteği düşer |
| H17 | Dalga kümesi özeti tutmuyor | `ozet_sha256` yeniden hesapta farklı | — | protokol hatası (H01) |
| H18 | Tüketici etkin halkadaki bir yuvayı süresinde bırakmadı ya da bekleyen başvurusu varken bağlantısı koptu | A2-K59, K60 | yuvayı geri almaz; halkayı A2-K86'ya göre emekliye ayırır; `tuketici_gec` / `tuketici_koptu` sayar | yeni halkayı eşler, `halka_eslendi`; eski halkayı bırakır |
| H19 | Okuyucu yuva başlığında beklenmeyen kimlik ya da sayaç gördü | A2-K53 | protokol ihlali olarak A3'e bildirir (tanılama) | — (okuyucu kareyi kullanmaz; A8) |
| H23 | Yinelenen ya da yabancı bırakma | A2-K58 | yok sayar, `yok_sayilan_birakma` sayar | — |
| H24 | Sıfırlama ya da atlama (nesil değişti) | A2-K80–K84 | eski nesil isteklerini iptal eder, hazır kareleri teslim etmez, eski nesil sonuçlarını reddeder | `kamera_iptal`'leri yanıtlar; geçmişi siler |
| H25 | Sahiplik kilidi alınamadı | A2-K77, K79 | A2'siz çalışır, A3 uyarısı | bağlanamaz; A2-K63 |
| H27 | Emekli halkaya atıf yapan yanıt | A2-K87 | isteği kapatır, teslim üretmez, `emekli_halka_yaniti` sayar | — |
| H28 | Tüketici başına emekli halka sınırı aşılıyor | A2-K89 | o tüketicinin kamera yolunu kapatır, olayı A3 ve A6'ya bildirir | — |
| H29 | Atlama bloğu ihlali (`govde_tablosu`'ndan önce `durum` ya da `kamera_istegi`; tablo zamanı ya da nesli uyuşmuyor; kimlik başka varlıkla geliyor) | A2-K91, K92 | — | `veda(protokol_hatasi)`, A2-K63 |
| H30 | Aynı süreçte yeni çalıştırma | A2-K96 | nesli artırır, K81–K84, A8 nesil bildirimi, `veda(yeni_calistirma)` ve oturum sonu | A2-K63 ile yeniden bağlanır |
| H26 | Yeni halka eşlenemedi | `halka_eslendi.durum ≠ 0` ya da 5 s (mimari seçimi) içinde yanıt yok | o kameranın kareleri `halka_yenileniyor` ile düşer; görselleştirici bağlantısını kapatır, görselleştirici yeni oturumda yeniden bağlanır | A2-K63 |
| H20 | Port kullanımda | dinleyici açılamadı | A2-K05 | bağlanamaz; A2-K63 |
| H21 | Halka adı çakıştı | A2-K73 | yeni nesille yeniden dener | — |
| H22 | Pencere çizilemez oldu | görselleştirici | — | kamera üretimi sürer (A2-K38, A2-K40); `pencere_cizilebilir = 0` bildirir |

Sim'in tuttuğu ve A3/A6'ya verdiği A2 sayaçları (biçim A3 ve A6'da): düşen kare (nedene göre: `halka_dolu`, `kamera_gec`, `gorsellestirici_yok`, `calisma_nesli_degisti`, `halka_yenilendi`, `halka_yenileniyor`, `halka_hatasi`, görselleştiricinin `kare_dustu` nedenleri), `gec_hazir`, `tuketici_gec`, `tuketici_koptu`, halka emekliye ayırma sayısı, `halka_emekli_hazir`, `emekli_halka_yaniti`, `tuketici_kamera_kapali`, tutulan emekli halka sayısı, `eski_nesil` sonuç reddi, yok sayılan bırakma, seyreltilen `durum`, protokol hatası, bağlantı sayısı ve kopma nedeni, istekten hazıra duvar saati süresi dağılımı (yuva başlığındaki duvar saatlerinden).

---

## 6. Kalite kademelerinin etkisi

| Kademe (iskelet §6) | A2'deki etkisi |
|---|---|
| 3B görünüm kapalı | A2 dinleyicisi açılmaz (A2-K02); kamera kademesi kapalı olmak zorundadır |
| 3B görünüm sade | Görselleştirici düz su (`D = 0` düzlemi) çizer; `dalga_kumesi` yine gönderilir; kamera kareleri `dalga görseli = 0` taşır (§3.8) |
| 3B görünüm tam | Dalga yüzeyi §3.4'teki kuralla çizilir |
| Kamera kapalı | `kamera_sayisi = 0`; halka oluşturulmaz; `kamera_istegi` gönderilmez |
| Kamera tam | 640 × 480, 30 Hz (iskelet §10 md.25'teki üst sınır) |
| Kamera sade | 320 × 240, 15 Hz (**tahmin; Windows hedef bilgisayarındaki ölçümle (MP8) sabitlenecek**) |

Bu sürümde tek kamera vardır. Kamera kademe değerleri kareyi üreten taraf olarak A2'dedir (iskelet 0.10 §10 md.25; W1-uyum U13); A8 ve A5 bunlara atıf verir. A5 yalnız kademeyi seçer; çözünürlük ve hız kademeden gelir, iç parametreler, pozlama ve `L_sonrası` A5'tedir. Değerler `a2_wire.h`'de `kTam*` ve `kSade*` sabitleridir.

Kademe bir çalıştırma boyunca değişmez; `hosgeldin`'de bildirilir ve yuva başlığına yazılır. Fizik, kamera dışı sensörler, otopilot ve HIL her kademede A2'den bağımsız çalışır (A2-K47).

---

## 7. Uygunluk

Test kodu bu işte yazılmaz. Tür: **B** birim, **S** sözleşme (iki taraftan biri sahte uçla), **SIL** kartsız bütünleşik, **K** kartlı.

| Test | Sınadığı kurallar | Tür | Yöntem ve gözlenecek büyüklük |
|---|---|---|---|
| A2-U01 | başlık ve tüm yapılar | B | `a2_wire.h` derlenir (`static_assert`); her ileti için kodlama → çözme gidiş-dönüşünde bayt eşitliği; bilinen bir örnek iletinin onaltılık dökümü tabloyla karşılaştırılır |
| A2-U02 | A2-K06, K07 | S | Sahte sim ve sahte görselleştirici; bozuk sihirli sayı, bilinmeyen tür, yanlış uzunluk, NUL'suz metin, azalan sıra gönderilir; beklenen: `veda(protokol_hatasi)` ve kapanış |
| A2-U03 | A2-K10–K16 | S | Ana sürüm farkı → `ret(surum)`; yanlış manifest özeti → `ret(manifest)` ve hiç `kamera_istegi` yok; `hazir` gecikmesi → 120 s'de `ret(zaman_asimi)`; alt sürüm farkında oturum alt sürümü = en küçüğü |
| A2-U04 | A2-K03, K05 | S | İkinci bağlantı → `ret(mesgul)`, ilk oturum etkilenmez; port dolu iken sim çalışır, A3'e bildirim |
| A2-U05 | A2-K20–K23, K29 | SIL | 16 gövdeli senaryoda 60 Hz takvim (13/14 adım); süreklilik `T`'de değişince `T − h` (eski numara, olay öncesi) ve `T` (yeni numara, olay sonrası: fırlatmada araç hızı değişmiş, torpido var) takvim dışı iletileri; olayın `T` zamanlı `durum`dan önce gelmesi; `T`'de doğan gövde ilk kez `T` durumunda, ölen gövde `T` durumunda yok; sıfırlama sonrası doğumlar |
| A2-U06 | A2-K30–K35 | B + SIL | Yer gerçeğiyle karşılaştırma: operatör konum hatası ≤ 1 fizik adımı (1,25 ms) boyunca yer değiştirme; `[T − h, T)` aralığında eski parçanın son durumu, `t_op = T`'de olay sonrası durum çizilir; sınırı aşan eğri yok; doğan gövde `t_op < T`'de, ölen gövde `t_op ≥ T`'de çizilmez; `t_op` azalmaz |
| A2-U07 | A2-K36, K37, K39 | SIL (Godot) | Adım başına > 1 piksel yer değiştiren hedef: kamera karesinde `t_cizim` konumunda ≤ 1 piksel (A2-K36); operatör durumu sızmaz; göstergeler görünmez; aynı istek farklı çizim geçmişlerinden sonra ortalama mutlak fark ≤ 2/255 |
| A2-U08 | A2-K38 | SIL (Godot) | Pencere görünür, tamamen örtülü, gizli, simge durumunda (macOS) ve simge durumunda, örtülü (Windows) en az 30 dk: istek, üretilen, düşen kare ve nedeni; `cizim_yolu = 1`; içerik görünür referansla ortalama mutlak fark ≤ 2/255 (MP6'nın G2/G3 ölçütü, A2 hattı üzerinden) |
| A2-U09 | A2-K41–K48 | S | Sahte görselleştirici gecikmesi ayarlanarak: zamanında yanıt teslim edilir; `S_d`'den sonra gelen reddedilir ve yuva boşalır; iptal edilmiş istek tam bir yanıt alır; aynı kamerada iki bekleyen istekte eskisi `daha_yeni_istek` |
| A2-U10 | A2-K47 | SIL ve K | Görselleştirici donduruldu (SIGSTOP / süreç askıya alma), öldürüldü, hiç başlatılmadı: sim'in `kaçırılan_adım` sayacı ve HIL bariyer bekleme süreleri görselleştiricili referans koşudan farklı değil; kamera düşen kare sayacı artar |
| A2-U11 | §3.8 piksel biçimi | SIL (Godot) | Işıksız (unshaded) düz renkli bir yüzey, sRGB taban rengi `(64, 128, 192)`, doğrusal ton eşleme, pozlama 1: kare piksel değerleri ±1 içinde; satır adımı `genislik × 3`; ilk satır sahnenin üstü |
| A2-U12 | §3.6 kamera kabulü, A2-K94 | B ve SIL (Godot) | Birim testinde A2-K94'ün iki uygunluk örneği (birim kuaterniyon → optik eksen gövde +x; y etrafında −90° → optik eksen gövde +z) ve `det R_SO = +1`; Godot'da bilinen konumdaki işaret noktası `u = fx·X/Z + cx` izdüşümünün ≤ 1 piksel içinde (A7-U06 ile birlikte); birim montajla kamera karesinin merkezinde aracın önündeki işaret görünür |
| A2-U13 | A2-K52–K59 | B (sim, yazıcı ve iki okuyucu süreci) | 10⁶ kare, arm64 ve x86-64'te: her karenin piksellerine gömülü kare kimliği ve içerik özeti okuyucunun gördüğüyle aynı (bellek görünürlüğü varsayımının sınaması, A2-K53); okuyucu A bir kareyi iki kez bırakır → B bırakana kadar yuva yeniden ayrılmaz; okuyucu 1 s'den uzun bekletilir → yuva geri alınmaz, halka emekliye ayrılır, yavaş okuyucunun eski halkada okuduğu kare üzerine yazılmaz; süreç içi benzetimde (iki iş parçacığı) ThreadSanitizer yuva verisine eşzamanlı erişim raporlamaz |
| A2-U14 | A2-K59, K60, K64, K65, K88 | SIL | Bekleyen başvurusu olan okuyucu süreci öldürülür → başvuru azaltılmaz, halka emekliye ayrılır, `halka_yenile`/`halka_eslendi` sonrası kare akışı sürer, emekli halka oturum sonuna kadar tutulur; başvurusu olmayan okuyucu kapanır → halka değişmez; görselleştirici öldürülüp yeniden başlatılır → yeni oturum kimliği, yeni halka adı ve nesli, eski nesil hiç kullanılmaz, 5 s içinde (mimari seçimi) kare akışı yeniden başlar |
| A2-U15 | A2-K62 | S | Görselleştirici 3 s ileti göndermeyince sim kapatır; sim 3 s sessiz kalınca görselleştirici kapatır |
| A2-U16 | A2-K70–K76 | B (iki işletim sistemi) | Ad uzunluğu ve biçimi; macOS'ta 31 bayt sınırı; çakışmada yeni nesil; macOS'ta çöken sim'den kalan nesnenin sonraki açılışta silinmesi; kayıt dosyasına ekleme ile `shm_open` arasında ve `shm_unlink` ile kayıttan silme arasında öldürülen sim'den sonra kurtarmanın hatasız tamamlanması; Windows'ta son tutamak kapanınca nesnenin kalkması; yeni halkada bütün `yazim_sayaci` = 0 |
| A2-U21 | A2-K77–K79 | B (iki işletim sistemi) | İki sim süreci farklı portlarla başlatılır: yalnız biri kilidi alır; ikincisi A2'siz çalışır, hiçbir halka adını kaldırmaz ve kayıt dosyasına yazmaz; kilit sahibi öldürülünce yeni süreç kilidi alır ve yalnız ölen sürecin adlarını kaldırır; ölen sürecin halkasını eşlemiş okuyucu okumayı hatasız bitirir |
| A2-U22 | A2-K80–K85 | S | Sahte görselleştirici ve sahte tüketiciyle: sıfırlama ve atlamada yanıtlanmamış, hazır ve tüketicideki kareler için sırasıyla iptal, teslim etmeme ve sonuç reddi; geç yanıt yalnız yuvayı boşaltır; tüketicinin eski nesil sonucu karta gitmez; `kare_sirasi` sıfırlanmaz; emekli halkada kalan eski nesil karesi için A2-K85 |
| A2-U23 | A2-K29, K90–K92 | S ve SIL (tekrar izleme) | Üç atlama: (a) bir torpidonun doğumunun üzerinden ileri: tabloda torpido var, `govde_dogdu` gönderilmez, görselleştirici torpidoyu tablodan kurar; (b) bir hedefin ölümünün üzerinden ileri: tabloda hedef yok, görselleştirici onu kaldırır; (c) geri atlama ölümden önceye: aynı kimlikli hedef aynı varlık indeksiyle yeniden görünür. Her üçünde `atlama` → `govde_tablosu` → `durum` sırası; tablodan önce gönderilen bir `durum` (sahte sim) protokol hatası; aynı kimliğin farklı varlık indeksiyle gelmesi protokol hatası; iki hedef aynı varlık indeksini paylaşır |
| A2-U24 | A2-K86–K89, §4.10 | S (sahte görselleştirici ve iki sahte tüketici) | Durum tablosunun her satırı (E1–E24) için ayrı senaryo; özellikle: emeklilik anında hazır olan kare, yeni halkayla el sıkışmış tüketiciye hiç verilmez ve `halka_emekli_hazir` sayılır; emeklilikten sonra gelen `kare_hazir` teslim üretmez; sim eski halkayı bütün bağlı taraflar yeni halkayla el sıkışana kadar tutar, bağlantısı kopan tüketici varken oturum sonuna kadar tutar; aynı tüketici üç ardışık emekliliğe yol açınca üçüncüde kamera yolu kapanır, yeni bağlantıyla döner; emekli halkanın hiçbir yuvası yeniden ayrılmaz (yuva başlığı `yazim_sayaci` emeklilikten sonra değişmez) |
| A2-U25 | A2-K93, §3.6 | B | Sim yapılandırması seçeneklerinin aralık dışı değerlerinde süreç başlamaz; seçeneklerin çözümlenmiş senaryo özetini değiştirmediği; §3.6'nın sınır değerleri (pozlama 1 000 000 000 ns kabul, 1 000 000 002 ret; tek sayı ret; `cx` = −0,5 ve g − 0,5 kabul, dışı ret; `kalibrasyon_surumu` 0 ret, 65 535 kabul; kuaterniyon norm farkı 1·10⁻⁵ sınırı) |
| A2-U26 | A2-K95 | S | Aynı ana tohum ve `kare_sirasi` ile iki koşuda `gurultu_tohumu` aynı; değerin A5 Ek D'nin `a2/kamera_gurultu` vektörleriyle (A5'in yayımladığı) bit düzeyinde aynı olduğu |
| A2-U27 | A2-K80, K96 | S ve SIL | Aynı sim sürecinde iki ardışık çalıştırma: ikinci çalıştırmada nesil 1 artar; önceki çalıştırmanın yanıtlanmamış isteği iptal edilir, hazır karesi teslim edilmez, A8 bileşeninin geç gelen sonucu (eski nesil) reddedilir ve karta gitmez; görselleştirici `veda(yeni_calistirma)` alır, yeniden bağlanır ve yeni `hosgeldin` yeni nesli taşır; A1 oturumu kapanır ve yeniden kurulur (A1-K128); `kare_sirasi` sıfırlanmaz; nesli değiştiren başka bir olay (duraklatma, kademe, A8 yeniden bağlanması) nesli artırmaz |
| A2-U28 | A2-K97, K49, K64, K81, K96 | S | Sahte görselleştiriciyle, yanıtlanmamış bir istek ve hazır bir kare varken yeni çalıştırma başlatılır: her `kare_sirasi` için A6'da tam bir düşme kaydı (`calisma_nesli_degisti`), düşme sayaçlarında tam bir artış, A8'e tam bir "kare yok" bildirimi; oturum sonunda `gorsellestirici_yok` sayacı değişmez; aynı yoklama emeklilik (K86 sonra K64) ve geç teslim (K46 sonra K48) bileşimleri için; 100 periyotta tek düşmede A2-K50 oranı %1'dir, %2 değil |
| A2-U17 | A2-K25, K26 | S | Yavaş okuyan görselleştirici: sim bloklanmaz (dünya adım süresi değişmez); `durum` seyreltilir ve `sira` boşlukları sayılır; olay iletileri kaybolmaz; kuyruk taşınca `veda(tampon_tasti)` |
| A2-U18 | A2-K50 | SIL | `kamera_gerekli` çalıştırmada bağlantı kopunca ve %1 eşiği aşılınca geçersiz işareti; gerekmeyende yalnız sayaç |
| A2-U19 | §3.4 dalga kuralı | SIL (Godot) | En az 25 noktada (bir kısmı ağ köşelerinin arasında), hem `t_cizim` hem `t_op` geçişinde GPU'dan geri okunan yükseklik ile 04'ün `surface_elevation` değeri arasındaki fark ≤ max(1 mm, %1 `H_s`) (07 §7 P7; mimari seçimi); sıfırdan farklı fazlı bileşenle |
| A2-U20 | A2-K08, durum gecikmesi | SIL | `gonderim_duvar_ns` ile uygulama anı arasındaki fark, 60 Hz, 16 gövde, 1 saat: p50 ve p99 raporlanır (eşik yok; ölçüm AB-217) |

---

## 8. Açık kalanlar

| # | Konu | Etiket | Neyle kapanacak |
|---|---|---|---|
| A2-A01 | Sade kamera kademesinin çözünürlük ve hızı (şimdilik 320 × 240, 15 Hz) | tahmin, ölçülecek | MP8 (Windows hedef bilgisayarı); değer A2 §6'da düzeltilir |
| A2-A10 | Süreçler arası bellek görünürlüğü varsayımı (A2-K53). sol'un notu: C++20'de çit eşzamanlaması atomik işlemler üzerinden tanımlanır (`[atomics.fences]`), soket gönderimi ve alımı bu ilişkiye girmez; Arm'ın bariyer kuralları da tek başına uçtan uca soket güvencesini kanıtlamaz. Atomik alanların kaldırılması bir yeterlilik kanıtı değildir | açık, doğrulanmadı | A2-U13 (arm64 ve x86-64; Windows'ta MSVC ile) |
| A2-A11 | macOS `flock` kilidinin süreç sonlanınca bırakılması | doğrulanmadı | A2-U21 |
| A2-A02 | A2 hattının kendisi (soket ve paylaşımlı bellek) üzerinden uçtan uca kamera gecikmesi; `t_teslim − t_cizim` bütçesinin yeterliliği | ölçülecek | Godot sahnesinin ilk işi (A2-U08, A2-U20) ve MP8. Bilinen tek veri: MP6'da A2'siz istekten hazıra p99 7,13–9,06 ms (Mac, durumlara göre). A5'in `L_sonrası` değeri bunun altında seçilirse kareler sürekli düşer |
| A2-A03 | Windows'ta `force_draw` yolu (simge ve örtülü durum) | ölçülecek | MP8; geçmezse iskelet §8'deki yedek (pencere görünür tutulur) tasarımın kendisi olur ve A2-K38 yalnız çizim yolunun koşulunu değiştirir, biçimi değiştirmez |
| A2-A04 | Tüketici başına emekli halka sınırı (2), varsayılan yuva sayısı (8), D_op (33,3 ms), nabız (1 s / 3 s), bırakma zaman aşımı (1 s; aşılınca halka emekliye ayrılır), `halka_eslendi` bekleme (5 s), `merhaba`/`hazir` zaman aşımları, 60 Hz takvim, 8 MiB kuyruk | tahmin | A2-U13, A2-U14, A2-U17 ve ilk SIL koşularının gözlemi; değişirse alt sürüm artışı gerekmez (sabitler `a2_wire.h`'de) ama sözleşme düzeltmesi gerekir |
| A2-A05 | `use_hdr_2d = false` görüntü alanının sRGB kodlu çıktı verdiği | doğrulanmadı | A2-U11 |
| A2-A06 | Güneş ışınımının Godot ışık enerjisine ve pozlamaya çevrimi | açık | Godot sahnesi uygulama işi; değerler A5'te kalibrasyon [K] |
| A2-A07 | Sahne içi göstergeler (sonar tarama yönü, iz, durum yazıları) | açık | A2 1.1'de yeni ileti türü olarak eklenebilir (alt sürüm artışı); kaynak A3 telemetrisi |
| A2-A08 | Kamera karelerinin tekrar oynatmada yeniden çizilmesi | açık | A6 (tekrar izleme kipinde istek üretilip üretilmeyeceği) |
| A2-A09 | Godot'da düğüm pozlarının `force_draw`'dan önce RenderingServer'a ulaştığı (AB-191) ve aydınlatmanın iki zamanlı çizimde doğru ilerlediği | ölçülecek | Godot sahnesi uygulama işi; A2-U07, A2-U08 sınar |

**A5'ten beklenen** (yalnız senaryo alanları; süreç ayarları A2-K93'tedir ve senaryoda yer almaz):

1. Kamera kademesinin seçimi (çözünürlük ve hız kademeden gelir; kademe değerlerinin kaynağı A2 §6'dır, A8 değil).
2. Kamera yapılandırmasının senaryodan gelen alanları, A2 §3.6'nın aralıklarıyla doğrulanmış olarak: `fx`, `fy`, `cx`, `cy`; bozulma modeli ve katsayıları; `pozlama_ns` (pozitif, çift, ≤ 1 s); `kalibrasyon_surumu` (1 … 65 535); **fizik gerçeği** montajı (`l_b`, S çerçevesinden gövdeye `q_bs`; görselleştiriciye bu gider) ve ondan ayrı **nominal** montaj (A8'e ve karta giden); `yakin_m`, `uzak_m`; `L_sonrası` (`t_teslim − t_olcum`); `L_alg` (A8). Varsayılanlar A5 Ek E'de.
3. `kamera_gerekli` (A2-K50).
4. Su optiği (`beta_d_m`, `beta_b_m`, `b_sonsuz_rgb_dogrusal`, `bulaniklik_olcegi`, `su_optigi_surumu`), güneş yönü ve ışınımı, ortam ışığı (§3.4 `Ortam`).
5. Statik sahnenin A7 varlık kimliği ve pozu (`Ortam.statik_*`).
6. A7 malzemelerinin optik ve akustik geçersiz kılmaları (A7-K30).
7. Doğrulama kuralları: en çok bir etkin kamera ve `inst = 0` (`kamera_kimligi` 0); kamera kademesi kapalı değilse 3B kademesi kapalı olamaz (A2-K02); A2 §3.6 aralıkları; yaşayan gövde bütçesi ≤ 64 ve dalga bileşeni ≤ 256 (§3.2 sınırları).
8. Ek D alt akış kataloğunda `a2/kamera_gurultu` ve onun kare başına türetim ve sayaç tahsisi kuralı (A2-K95; W1-uyum U17).

**A8'den beklenen** (halka ve nesil yaşam döngüsünün kaynağı A2'dir, W1-uyum U14; ileti adları ve kodları A8'indir). Doğrulama incelemesindeki (W1-doğrulama, parça 3) karşılık tablosuna ve A8'in bu turdaki metnine göre on altı maddenin hepsi karşılanmıştır; köşeli parantezde A8'in karşılayan kuralı:

1. **Salt okunur eşleme** (A2-K54): karşılandı [A8-K38].
2. **Kare bildirimi** `halka_nesli`, `yuva`, `istek_kimligi`, `kare_sirasi`, `calisma_nesli`, `yazim_sayaci`, `t_olcum_ns`, `pozlama_ns`, `yapilandirma_surumu` ve A2 oturum kimliğini taşır (A2-K53, K58, K80): karşılandı [A8 §3.5 `FRAME_AVAILABLE`, K72].
3. **Edinme çiti ve başlık denetimi** okumadan önce ve sonra (A2-K53, K74): karşılandı [A8-K73, K48]; görünürlük varsayımı iki tarafta da doğrulanmadı (A2-A10).
4. **Geçersiz kare** (`tutarsiz` bayrağı, yapılandırma ya da kalibrasyon sürümü uyuşmazlığı): kayıt üretilmez (A2 §3.8, K28): karşılandı [A8-K48].
5. **Bırakma** `(halka_nesli, yuva, istek_kimligi)` ile, tüketici kimliği bağlantıdan, kare başına bir kez (A2-K58): karşılandı [A8-K74, K45].
6. **Kopma ve zaman aşımı** yuvayı bırakmaz; sim halkayı emekliye ayırır (A2-K59, K60, K86); bileşen bütün kapanış yollarında önce süren okumayı bitirir, sonra eşlemeyi kaldırır: karşılandı [A8-K45, K50, K82].
7. **Halka yenileme ve yeni nesli onaylama**; eski okuma, sonuç ve bırakma bitmeden başarılı onay yok (A2-K86, K88): karşılandı [A8-K70, K71].
8. **Kamera yolu kapatılan tüketicinin dönüşü** yalnız yeni bağlantı ve tam el sıkışmayla (A2-K89): karşılandı [A8-K77, K63 adım 2].
9. **A2 oturum sonu** bildirimi; okuma bitirilir, eşleme sonra kaldırılır, yeni oturumun halkası yeniden teklif edilir (A2-K64, K96): karşılandı [A8-K75, K70 (b)].
10. **Çalıştırma nesli** bildirimi ve eski nesil sonuçlarının reddi; nesli artıran olayların kapalı listesi A2-K80'dedir (yeni çalıştırma, sıfırlama, atlama) (A2-K80–K84, K96); sim kabul kapısının zaman ilerleme durumu nesil değişiminde sıfırlanır, `batch_seq` ve `det_seq` korunur: karşılandı [A8-K78, K79, K17].
11. **Kare sayacının ömrü** süreç boyunca, sıfırlanmaz (A2-K49): karşılandı [A8-K80, K16].
12. **Düşme nedenlerinin kapalı eşlemesi** (1–8, 33–39; özgün kod A6'da): karşılandı [A8-K76, K63 adım 1].
13. **Geçerlilik anı** `t_olcum_ns − pozlama_ns/2` (A2-K41): karşılandı [A8-K10–K12].
14. **Yapılandırma ve nominal montaj** (A2 §3.6 aralıkları; A8 `CAMERA_CONFIG` iç parametre ve bozulma alanlarını A5'teki bitleriyle aynen binary64 taşır (A2'deki karşılıklarıyla bit eşit), montaj alanlarını ise A5'in **nominal** montajından taşır; A2'nin montajı A5'in fizik gerçeği montajıdır ve A8'e gitmez; çevirme ve yuvarlama yoktur; `calib_ver` u16; nominal montaj S çerçevesinde) (A2-K55, K94): karşılandı [A8-K36, K52, K53].
15. **Kademe ve tek kamera** (A2 §6): karşılandı [A8-K35, K68].
16. **Halka adı** A2-K70–K72'deki biçimdedir (en çok 32 ASCII karakter, Windows `Local\` önekiyle; macOS'ta 27); A8'in 64 baytlık alanına NUL dahil sığar: karşılandı [A8 §3.5 `RING_OFFER.ring_name`, K70].

**A6'dan beklenen:** düşen her karenin `kare_sirasi`, nedeni ve `S_d`'si; çalıştırma nesli değişimleri; halka emekliye ayırma olayları; kaydedilen karelerin çalıştırma nesli etiketi (A2-K83); A2 sayaçları (§5); oturumların açılış ve kapanış nedenleri; manifest ve dalga kümesi özetleri; `tam` kayıt kademesinde kare kaydı (sim içi tüketici olarak, A2-K61); kameradan türetilen algılama kayıtlarının kaydı ve tekrar oynatmanın onlardan yapılması (A2-K51).

**A3'ten beklenen:** A2 bağlantı durumu, sayaçlar, "kamera sağlıksız", "dinleyici açılamadı", "A2 sahiplik kilidi başka bir sim sürecinde", "halka emekliye ayrıldı" ve yuva başlığı protokol ihlali uyarıları, `kamera_gerekli` çalıştırmanın geçersizlik gerekçesi.

---

## 9. Açık bulgu kapanış tablosu

### 9.1 `acik-bulgular.md` "A2" başlığı

| Bulgu | Durum | Sözleşmede | Gerekçe |
|---|---|---|---|
| AB-048 | kapandı | §3.4 `GovdeDurumu` notu | Gövde durumu ve olaylar itki, itici ya da atışla ilgili alan taşımaz; iskelet 0.7 öncesi alanlar yok |
| AB-049 | kapandı | A2-K53, K54, K56–K58, K61 | Çoklu okuyucu: her karenin bekleyen tüketici kümesi sim'in belleğinde; bir yuva, kümedeki her tüketici bırakmadan yeniden kullanılmaz; okuyucular salt okunur eşler, paylaşımlı bellekte atomik ya da sahiplik alanı yok |
| AB-050 | kapandı | A2-K42, K57, K58 | Yuvayı yalnız sim ayırır; bırakma yalnız gönderen tüketicinin (kimliği bağlantıdan) o karedeki başvurusunu bir kez kaldırır; yinelenen ve yabancı bırakma yok sayılır; iptal edilen yuva yanıt gelene kadar yeniden ayrılmaz |
| AB-051 | kapandı | A2-K59, K60, K64, K65, K77–K79, K86–K89 | Bırakılmayan ya da sahibi kopan yuva geri alınmaz; halka emekliye ayrılır ve yeni nesille yenisi kurulur; görselleştirici yeniden bağlanınca yeni oturum ve yeni halka; kalmış nesnelerin temizliği porttan bağımsız sahiplik kilidine bağlı |
| AB-052 | kapandı | A2-K21, K22, K31–K34 | Süreklilik sınırı tek tanımlı (olay `T` adımının başında; `T` durumu olay sonrası); `T − h` ve `T` durumları takvim dışı gönderilir; görselleştirici ≥ 500 ms geçmiş tutar; sınırda çizilecek durum kurallı |
| AB-053 | kapandı | §2 (`t_cizim`, `t_olcum`, `t_teslim`), A2-K41, §3.8 | Ölçüm zamanı pozlama sonu, çizim zamanı ortası ve tek geçerlilik anı olarak kullanılır (iskelet §3.1); `t_teslim ≥ t_olcum`; üçü de kareyle ayrı taşınır. Algılama kaydının alanına eşlenmesi A8'den beklenen olarak yazıldı; kart alım anı A1/A8'in konusudur |
| AB-054 | kapandı | §3.8 bayrak 2, 4, 5; §6 | `dalga görseli`, `kamera su altında` ve `yüzeye yakın` bayrakları karede taşınır |
| AB-055 | kapandı | §3.4, A2-K21, K32 | `v_n`, `w_b`, `sureklilik_no` akışta; olay öncesi son durum (`T − h`) ve olay sonrası ilk durum (`T`) tek tanımla gönderilir |

### 9.2 Uyumsuzluk tablosunda A2'ye değinen satırlar

| Satır | Durum | Sözleşmede | Gerekçe |
|---|---|---|---|
| U-05 | kapandı | §2, A2-K41, §3.8 | Kamera ölçüm zamanı iskelet §3.1'e göre pozlama sonu ve pozlama süresi (birincil); tek geçerlilik anı olan orta bunlardan türer |
| U-17 | kapandı | §3.4 faz kuralı | Dalga fazı 04'ün `cos(ωt − k·x + φ)` kuralıyla tek tanımlı |
| U-18 | kapandı | A2-K21, K32, K90–K92 | Ara değerleme süreklilik sınırını aşmaz; sınırın tanımı ve iki yanı tek kuralda |
| U-19 | kapandı | §3.4 | Torpido itki alanı yok |

### 9.3 Bilgi için: başka başlıklarda durup bu sözleşmenin kural verdiği bulgular

Bu satırlar kendi gruplarında kapatılır; durum değerlendirmesi o işlerindir.

| Bulgu | Grup | Bu sözleşmenin katkısı |
|---|---|---|
| AB-236 | A8 | Kamera yolu A2-K38'de kural |
| AB-237 | A8 | Piksel biçimi tek: `rgb8_srgb` (§3.8) |
| AB-238 | A8 | Asenkron geri okuma kamera için kullanılmaz (A2-K38) |
| AB-190 | Godot sahnesi | Faz kuralı §3.4'te bağlayıcı |
| AB-192 | Godot sahnesi | İstek kimliği, yuva ve sıra A2-K43–K45, K52'de; komut sırası uygulamada |
| AB-194 | Godot sahnesi | Asenkron kuyruk kullanılmadığı için kuyruk gecikmesi sorunu düşer; uçtan uca bütçe A2-A02 |
| AB-195 | Godot sahnesi | İptal kuralları A2-K45, K46, K48 |
| AB-202 | Godot sahnesi | Geçersizlik kuralı A2-K50, nabız zaman aşımı A2-K62 |
| AB-203 | Godot sahnesi | İki okuyuculu halka, okuyucu çökmesi, iptal, yeniden başlatma testleri A2-U13, U14, U09 |
| AB-217 | Ölçüm | Durum akışı gecikmesi A2-U20'de ölçülür |
