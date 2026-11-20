# A5 — Senaryo ve araç tanımı dosyası sözleşmesi

| | |
|---|---|
| Sürüm | 1.0-aday, W1-uyum turu (2026-11-20) |
| İş | `W1-A5` (`docs/gorevler/W1-A5.md`; ortak kurallar `docs/gorevler/W1-ortak.md`); çapraz uyum `docs/gorevler/W1-uyum.md` |
| İnceleme | `docs/sozlesmeler/inceleme/A5-sol.md`, `A5-sol-2.md`; yanıtlar `A5-yanit.md`, `A5-yanit-2.md`; çapraz inceleme `W1-capraz-sol-{1..4}.md` (bütünleştirme dalında) |
| Dayanak | iskelet 0.8 (`docs/mimari/00-iskelet.md`) §3, §3.1, §4, §5, §6, §8, §10; bölümler 02, 04, 05, 06, 08 (taslak); `docs/gorevler/W0-MP7a.md`; `docs/olcumler/MP7-sonar-suresi.md`; `docs/research/00-duzeltmeler-ve-kararlar.md`; mimarın W1-A5 düzeltme turu kararları |
| Makinece okunur tanım | `scenarios/sema/a5.schema.json` (JSON Schema 2020-12), `scenarios/ida0.toml` (araç), `scenarios/ornek-havuz.toml` (en küçük geçerli senaryo), `scenarios/ornek-havuz.cozumlenmis.json` (altın çözümlenmiş senaryo, kanonik baytlar) |

Bu belge kural yazar. Şema dosyası ve örnek dosyalar sözleşmenin parçasıdır; belgeyle çelişemez. Şema yapısal kuralları (anahtar, tip, aralık, zorunluluk, çözümlenmiş biçimin zorunlulukları) taşır; dosyalar arası ve anlamsal kurallar yalnız bu belgededir (4. başlık). Belge ile şema çelişirse belge geçerlidir ve şema düzeltilir.

---

## 1. Kapsam ve taraflar

**Konu.** Bir simülasyon çalıştırmasını baştan sona tanımlayan dosyaların biçimi: araç (İDA-0) parametreleri, ortam, hedefler, görev, sensör ve aktüatör ayarları, arıza ve olay zaman çizelgesi, bağlantı emülatörü, çalışma kipi, kalite kademeleri, rastgelelik, başlangıç koşulları ve sanal emniyet girdilerinin başlangıç durumu. Aynı davranış özeti (A5-K17), aynı yazılım ikilileri ve aynı platform aynı çalıştırmayı tanımlar (sınırları A5-K85, A5-K112).

| Taraf | Rol |
|---|---|
| Senaryo yazarı (insan ya da araç) | Dosyayı yazar |
| Laboratuvar uygulaması (08; sim sunucusunun içinde, C++) | **Doğrulayıcı ve çözümleyici:** ayrıştırır, birleştirir, doğrular, çözümlenmiş senaryoyu, davranış özetini ve doğrulama raporunu üretir; A6'ya yazar |
| Sim sunucusunun modelleri (04 fizik, 05 sensör ve sonar, 06 HIL köprüsü, bağlantı emülatörü ve hata enjektörü) | **Tüketici:** yalnız çözümlenmiş senaryoyu okur |
| SIL koşucusu (02) | **Tüketici:** yalnız `otopilot` bloğunu, platform parametre bloğu olarak (02 §3.6) |
| Web panelleri (A3) | Senaryo formunu şemadan üretir; doğrulama raporunu gösterir |
| Kayıt (A6) | Çözümlenmiş senaryonun kanonik baytlarını, davranış özetini ve kaynak özetlerini saklar |

**Kapsam dışı.** TOML ayrıştırıcısının seçimi ve kodu; modellerin içi (04, 05); A1 mesajlarının biçimi; A7 manifestinin içi (sahneye, varlıklara ve malzemelere yalnız kimlikle atıf verilir); A4 parametre tablosunun içeriği; A3 HTTP uçları; kamera kademe değerleri (A2 §6); görselleştirici ve kamera hattının süreç ayarları (port, halka yuva sayısı, `D_op`: A2-K93 sim yapılandırması); kontrol hızı, fizik adımı ve `L_act` (iskelet §4; A5-K77); HIL geçersizlik eşiği (A5-K77); ölçüme bağlı yedekler (iskelet §6); yarışma kuralları, ardıl kart, gerçek sensör tedariki.

**Atış kontrol sınırı.** Torpido ve fırlatıcı alanları iskelet §5'teki itkisiz, kısa menzilli basit modelin parametreleriyle sınırlıdır ve 1.0'da değiştirilemez (A5-K29; mimar onayladı). İtki, güdüm, arayıcı, menzil hedefi ya da atış çözümü alanı yoktur. Zaman çizelgesinde atış komutu yoktur (A5-K73; mimar onayladı).

---

## 2. Terimler ve kabuller

İskelet §3.1'in ortak kabulleri geçerlidir (SI, NED dünya çerçevesi, gövde x ileri / y sancak / z aşağı, gövde referans noktası `O_b`, kuaterniyon (w, x, y, z) Hamilton, gövdeden dünyaya). Bu sözleşmeye özgü olanlar:

| Terim | Anlamı |
|---|---|
| Araç dosyası | `meta.tur = "arac"`; fizik gerçeğini (`arac`) ve isteğe bağlı otopilot parametre çiftlerini (`otopilot`) taşır |
| Senaryo dosyası | `meta.tur = "senaryo"`; tek bir araç dosyasına başvurur (`arac.dosya`) ve araç değerlerini alan bazında geçersiz kılabilir |
| Çözümlenmiş senaryo | `meta.tur = "cozumlenmis_senaryo"`; araç birleşmiş, kaynak alanları çıkarılmış, bütün varsayılanlar açık, çözümleyici üst verisi eklenmiş biçim (A5-K16). Modellerin okuduğu tek biçim |
| Kanonik biçim | Çözümlenmiş senaryonun RFC 8785 (JCS) JSON dizilimi (A5-K17) |
| Davranış özeti | Kanonik baytların SHA-256'sı, 64 küçük onaltılık karakter; kaynak dosya baytlarından bağımsızdır (A5-K17) |
| Kaynak özetleri | Araç ve senaryo dosyalarının ham baytlarının SHA-256'sı; davranış özetine girmez, ayrı üst veridir |
| Anahtarlı koleksiyon | Girdileri adla anahtarlanan tablo: `arac.iticiler`, `arac.firlaticilar`, `arac.sensorler`, `hedefler`, `baglanti.profiller`, `kalibrasyon.malzeme` |
| Ürün profili | Sensör modelinin bu belgede sabitlenen varsayılan değer kümesi (3.6); `ayar` ile geçersiz kılınır |
| Kalibre edilmedi | Profilde ölçülmüş ya da yayımlanmış değeri olmayan parametrenin ideal (hata katkısı sıfır) değerle çözümlenmesi; çözümlenmiş senaryoda `meta.kalibre_edilmedi` listesinde görünür (A5-K101) |
| Olay çeşitleri | **Kalıcı atama** (değeri atar, değer sonraki atamaya kadar kalır), **anlık komut** (bir kez uygulanır), **geçici arıza** (başlar, süresi dolunca kalkar) (A5-K109) |
| Değer etiketleri | Örnek dosyalarda: [Y] yayımlanmış ölçüm, [CAD] yayımlanmış CAD hesabı, [AMP] yayımlanmış ampirik tahmin, [Ü] üretici belgesi, [T] tahmin, [K] kalibrasyon parametresi, [C] yapılandırma, [G] çevresel girdi, [MV] mimari varsayım (bölümlerde değer yoktu; mimar belirledi, K10) |

**Birim ve adlandırma kabulleri (A5-K11).** Sayısal değerler birim eki almaz; birim anahtarın tanımındadır. Anahtar adı birimi yalnız şu istisnalarda taşır: `_ns` tamsayı nanosaniye (bütün süreler, zaman sabitleri, periyotlar ve sim zamanı anları), `_c` santigrat derece, `_us` mikrosaniye (ESC darbe genişliği; iskelet §3 A1 istisnası), `_hz` hertz, `_db` ve `_dbm` logaritmik oranlar. Açılar radyandır; enlem ve boylam dahil. Tuzluluk kütle oranıdır (kg/kg). Kuvvet newtondur: üretici kgf değerleri 9,80665 m/s² (standart yerçekimi, tanım değeri) ile çevrilmiştir.

**Gövde referans noktası (İDA-0).** `O_b` çerçevenin geometrik merkezidir; İDA-0'da boş aracın ağırlık merkezi bu noktada kabul edilir (`r_g = 0`, 04 §2.6) [MV]. Gövdeye sabittir ve yük ayrılınca yerinde kalır (iskelet §3.1).

---

## 3. Veri tanımları

**Tablo sütunları.** *Tip:* `tamsayı` (TOML tamsayısı zorunlu), `sayı` (TOML tamsayısı ya da kayan noktalı; binary64'e A5-K96 ile çevrilir), `mantıksal`, `dizgi`, `dizi[n]`, `tablo`, `matris 3×3` (üç satırlık dizi dizisi), `kuaterniyon` (`dizi[4]` sayı, w, x, y, z). *Zorunlu:* E = evet, H = hayır, K = koşullu (koşul anlamda). *Varsayılan:* koşulsuz varsayılan şemada `default` ile durur; koşullu varsayılanlar A5-K98'dedir; "—" varsayılan yoktur. **Bütün varsayılanların normatif listesi Ek E'dedir** (makinece okunur TOML). Belgedeki her sayısal varsayılan, çözümlemede kullanılan binary64 değerin en kısa gidiş-dönüş ondalığıdır; varsayılan tablolarında yuvarlatılmış gösterim kullanılmaz (Türkçe metinde ondalık ayıracı virgül, `kod` içinde ve Ek E'de noktadır; basamaklar aynıdır). *Besler:* değeri okuyan bölüm. **Geçersiz değerde davranış** bütün tablolar için aynıdır: dosya, ilgili kural numarasıyla reddedilir ve A5-K92 biçiminde bulgu üretilir; uyarı olarak tanımlananlar (A5-K95) yüklemeyi durdurmaz. "Desteklenmiyor" işaretli anahtar ya da tür verilirse A5-K102 ile reddedilir.

### 3.1 Dosya türleri ve `meta`

| Tablo | Araç dosyası | Senaryo dosyası | Çözümlenmiş senaryo |
|---|---|---|---|
| `meta` | E | E | E (+ çözümleyici üst verisi) |
| `arac` | E (tam) | E (`dosya` + kısmi geçersiz kılma) | E (tam; `dosya` yok) |
| `otopilot` | H | H | E |
| `calistirma`, `ortam`, `rastgelelik`, `varliklar` | yok | E | E |
| `hedefler`, `olaylar`, `emniyet_girdileri`, `baglanti`, `kalibrasyon`, `kalite`, `baslangic` | yok | H | E (A5-K99) |
| `gorev` | yok | H | yalnız senaryoda varsa |

| Anahtar | Tip | Aralık | Zorunlu | Anlam |
|---|---|---|---|---|
| `meta.sema_surumu` | dizgi | `^1\.[0-9]{1,3}$` | E | Sözleşme sürümü `major.minor`; bu belge `"1.0"` |
| `meta.tur` | dizgi | `arac`, `senaryo`, `cozumlenmis_senaryo` | E | Dosya türü |
| `meta.ad` | dizgi | ≤ 128 | E | Ad |
| `meta.aciklama` | dizgi | ≤ 4096 | H | Açıklama |
| `meta.kalibre_edilmedi` | dizi dizgi, sıralı | anahtar yolları | yalnız çözümlenmişte E | A5-K101 |
| `meta.dogrulanmamis_varsayimlar` | dizi dizgi, sıralı | anahtar yolları | yalnız çözümlenmişte E | A5-K108 |
| `meta.olcum_kanitlari[]` | dizi tablo `{yol, kaynak}` | — | yalnız çözümlenmişte E | Ölçüm sınırına bağlı seçimin kanıtı (A5-K79) |

Çözümleyici üst verisi (son üç satır) girdi dosyasında bulunursa reddedilir (A5-K06).

### 3.2 `arac`: genel, kütle ve hidrodinamik (besler: 04)

| Anahtar | Tip | Birim | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|---|
| `arac.ad` | dizgi | — | ≤ 64 | — | E | Araç adı |
| `arac.varlik` | dizgi | — | A7 kimliği (`^[a-z0-9_]{1,32}$`; A7 §3.3) | — | E | A7 `arac` türündeki varlık (A5-K19) |
| `arac.referans_noktasi` | dizgi | — | ≤ 256 | — | E | `O_b`'nin sözlü tanımı (bilgi) |
| `arac.kutle.kutle` | sayı | kg | > 0 | — | E | Boş araç kütlesi (trim hariç) |
| `arac.kutle.hacim` | sayı | m³ | > 0 | — | E | Boş araç deplasmanı ∇ |
| `arac.kutle.r_g` | dizi[3] | m | — | — | E | Ağırlık merkezi, `O_b`'ye göre |
| `arac.kutle.r_b` | dizi[3] | m | — | — | E | Tam batık yüzerlik merkezi |
| `arac.kutle.atalet` | matris 3×3 | kg·m² | A5-K21 | — | E | `O_b`'ye göre atalet matrisi; köşegen dışı terim matris elemanıdır (`I_yz = −Σ m y z`) |
| `arac.kutle.trim_kutlesi` | sayı | kg | ≥ 0 | 0 | H | Hacimsiz nokta kütle (A5-K24) |
| `arac.kutle.trim_konumu` | dizi[3] | m | — | — | K (A5-K24) | Trim kütlesinin konumu |
| `arac.hidrodinamik.isaret_kurali` | dizgi | — | `"pozitif_buyukluk"` | — | E | Katsayılar pozitif büyüklüktür; Fossen işaret dönüşümü yükleyicidedir |
| `arac.hidrodinamik.eklenmis_kutle` | dizi[6] | kg, kg·m² | ≥ 0 | — | E | `[X_u̇, Y_v̇, Z_ẇ, K_ṗ, M_q̇, N_ṙ]`; köşegen dışı 0 |
| `arac.hidrodinamik.dogrusal_sonum` | dizi[6] | N·s/m, N·m·s/rad | ≥ 0 | — | E | `[X_u, Y_v, Z_w, K_p, M_q, N_r]` |
| `arac.hidrodinamik.karesel_sonum` | dizi[6] | N·s²/m², N·m·s²/rad² | ≥ 0 | — | E | `[X_u|u|, …, N_r|r|]` |
| `arac.hidrodinamik.yuzey_sonum_orani` | dizi[3] | — | ≥ 0 | — | E | Otter yüzey sönümü ζ (heave, roll, pitch) |
| `arac.hidrodinamik.ruzgar_direnc_katsayisi` | sayı | — | > 0 | — | E | Rüzgâr `C_D` (04 §2.5) |
| `arac.hidrodinamik.belirsizlik.oteleme` / `.donme` | sayı | bağıl | [0, 1] | — | E | Belirsizlik bandı; A5-K26'nın sınırı |
| `arac.hidrodinamik.sapma.eklenmis_kutle` / `.dogrusal_sonum` / `.karesel_sonum` | dizi[6] | çarpan | A5-K26 | `[1, 1, 1, 1, 1, 1]` | H | Yalnız fizik tarafına uygulanan çarpanlar |

### 3.3 `arac`: iticiler, itici modeli, ESC (besler: 04; tahsis matrisi kaynağı olarak 02)

`arac.iticiler` tam olarak `t1`…`t8` anahtarlarını taşır (iskelet §5).

| Anahtar | Tip | Birim | Aralık | Zorunlu | Anlam |
|---|---|---|---|---|---|
| `arac.iticiler.<tN>.kanal` | tamsayı | — | 0–7, tekil (A5-K25) | E | Kartın gördüğü itici sırası |
| `arac.iticiler.<tN>.konum` | dizi[3] | m | — | E | `r_i`, `O_b`'ye göre |
| `arac.iticiler.<tN>.yon` | dizi[3] | — | birim (A5-K32) | E | `ε_i`: pozitif komutun itki yönü; dikeyde pozitif komut +z (aşağı) |
| `arac.iticiler.<tN>.rol` | dizgi | — | ≤ 64 | H | Bilgi |
| `arac.itici_modeli.tur` | dizgi | — | `"t200_gecici"` | E | 04 §2.7.2 geçici model |
| `arac.itici_modeli.sekil_polinomu` | dizi[5] | — | A5-K104 | E | `F(u) = Σ c_i u^(2i+1)`, `[c₀ … c₄]` = `[u¹, u³, u⁵, u⁷, u⁹]` katsayıları |
| `arac.itici_modeli.gerilim_noktalari` | dizi[2–8] | V | > 0, kesin artan | E | Doğrulanmış uç değer gerilimleri |
| `arac.itici_modeli.azami_itki_ileri` / `azami_itki_geri` | dizi[2–8] | N | > 0 | E | Her gerilimde azami itki (geri: pozitif büyüklük) |
| `arac.itici_modeli.tam_gaz_akimi` | dizi[2–8] | A | > 0 | E | Her gerilimde tam gaz akımı |
| `arac.itici_modeli.akim_ussu` | sayı | — | > 0 | E | `I = I_maks (|F|/F_maks)^p` |
| `arac.itici_modeli.transfer_pay` / `transfer_payda` | dizi | — | 1–4 / 2–5 eleman; A5-K105 | E | `K(s)` katsayıları, yüksek dereceden düşüğe; `F = K(s) F_ss` (04 §2.7.3) |
| `arac.itici_modeli.pervane_yaricapi` | sayı | m | > 0 | E | Havalanma için `R` |
| `arac.itici_modeli.havalanma_h_bolu_r` / `havalanma_beta` | dizi[2–32] | — | kesin artan / [0, 1] | E | `β(h/R)` tablosu, doğrusal ara değerleme |
| `arac.esc.pwm_notr_us`, `olu_bant_us`, `pwm_min_us`, `pwm_max_us` | tamsayı | µs | 800–2200 (ölü bant 0–200); A5-K31 | E | ESC darbe değerleri |
| `arac.esc.baslatma_suresi_ns` | tamsayı | ns | ≥ 0 | E | Açılışta ve bara geri gelişinde nötr bekleme |

PWM kaybı davranışı bir anahtar değildir (A5-K27).

### 3.4 `arac.batarya` ve `arac.temas` (besler: 04)

| Anahtar | Tip | Birim | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|---|
| `kip` | dizgi | — | `ideal`; `b0`, `b1` desteklenmiyor (A5-K28) | — | E | Batarya modeli |
| `v_sabit` | sayı | V | (0, 26] | — | K (ideal) | İdeal kaynak gerilimi |
| `seri`, `paralel` | tamsayı | — | 1–16, 1–32 | — | E | Hücre düzeni |
| `kapasite` | sayı | C | > 0 | — | E | `Q` |
| `r_kablo` | sayı | Ω | ≥ 0 | — | E | Kablo direnci; ideal kipte `R_top` |
| `aviyonik_guc` | sayı | W | ≥ 0 | — | E | Sabit aviyonik ve sensör yükü |
| `surekli_akim_siniri`, `darbe_akim_siniri` | sayı | A | > 0 | — | H | Aşılınca olay; kesme değildir |
| `darbe_suresi_ns` | tamsayı | ns | > 0 | — | H | Darbe akımı penceresi |
| `asgari_guvenli_gerilim` | sayı | V | > 0 | — | H | Altında olay |
| `sigorta_i2t` | sayı | A²·s | — | — | desteklenmiyor | Sigorta modeli bu sürümde yok |
| `hucre.*` (`sicakliklar_c`, `soc`, `ocv`, `r0`, `r1`, `c1`) | | | | — | desteklenmiyor | B0/B1 haritaları (AB-088); yapı şemadadır |
| `arac.temas.yay_sabiti` | sayı | N/m | > 0 | — | E | `k_n` |
| `arac.temas.sonum_orani` | sayı | — | ≥ 0 | — | E | `c_n`'nin kritik sönüme oranı |
| `arac.temas.surtunme_katsayisi` | sayı | — | ≥ 0 | — | E | `μ` |
| `arac.anten_konumu` | dizi[3] | m | — | — | desteklenmiyor | Telsiz anteni modeli bu sürümde yok |

### 3.5 `arac.torpido` ve `arac.firlaticilar` (besler: 04; mimari varsayım, iskelet §5)

Bütün sayısal değerler sabittir: şemada `const` olarak durur ve başka değer reddedilir (A5-K29). Tek değişken alan `arac.torpido.varlik`'tir (dizgi, A7 kimliği, zorunlu): A7 `torpido` türündeki varlık (A5-K19); sayısal bir yetenek taşımaz.

| Anahtar | Tip | Birim | Sabit değer |
|---|---|---|---|
| `arac.torpido.geometri` | dizgi | — | `"prolate_sferoid"` |
| `boy`, `cap` | sayı | m | 0,20; 0,04 |
| `kutle` | sayı | kg | 0,175 |
| `r_g` | dizi[3] | m | (0; 0; 0,002), torpido merkezine göre |
| `kanat_alani_duzlem` | sayı | m² | 0,0018 |
| `kanat_en_boy_orani` | sayı | — | 2,0 |
| `kanat_x` | sayı | m | −0,09 |
| `surukleme_katsayisi` | sayı | — | 0,42 |
| `ayrilma_hizi` | sayı | m/s | 2,0 (bağıl ayrılma hızı `v₀`) |
| `arac.firlaticilar.f1.tup_konumu` / `f2.tup_konumu` | dizi[3] | m | (0; +0,10; 0,10) / (0; −0,10; 0,10) |
| `arac.firlaticilar.<f>.tup_ekseni` | dizi[3] | — | (1; 0; 0) |
| `arac.firlaticilar.<f>.tup_su_kutlesi` | sayı | kg | 0,0 |

Türetilenler (hacim, ataletler, Lamb katsayıları, eklenmiş kütleler, `C_Lα`) dosyada yazılmaz; 04 §2.8'deki formüllerle yükleyici hesaplar.

### 3.6 `arac.sensorler` (besler: 05)

Anahtar sensör adıdır (`^(?!x_)[a-z][a-z0-9_]{0,31}$`), en çok 64 sensör.

| Anahtar | Tip | Birim | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|---|
| `tur` | dizgi | — | `imu`, `manyetometre`, `basinc`, `gnss`, `dvl`, `ping1d`, `ping360`, `kamera`, `batarya_olcer`, `sizinti`, `firlatici_geri_okuma` | — | E | Sensör türü (iskelet §5 seti; iç basınç/sıcaklık ve "bakım" girdisi yok) |
| `kimlik` | tamsayı | — | 0–65535, tekil | — | E | `sensor_id` (05 §3) |
| `inst` | tamsayı | — | 0–255; (tür, inst) tekil | — | E | Aynı türden örnek numarası |
| `profil` | dizgi | — | türe göre tek değer: `icm42688p`, `rm3100`, `ms5837_30ba`, `neo_m9n`, `a50`, `ping1d`, `ping360`, `kamera_on`, `psm`, `sos_leak`, `firlatici_io` | — | E | Ürün profili |
| `etkin` | mantıksal | — | — | true | H | A5-K49 |
| `hiz_hz` | sayı | Hz | (0, 800] | profil (aşağıda) | H; `imu`, `dvl`, `kamera`, `ping1d`, `ping360` için yasak (A5-K41) | Örnekleme hızı |
| `saat_kaymasi_ppm` | sayı | ppm | — | 0 (koşullu, A5-K98) | H; `imu` için yasak | Saat kayması |
| `montaj.konum` | dizi[3] | m | — | — | E | `l`, `O_b`'ye göre |
| `montaj.yonelim` | kuaterniyon | — | birim (A5-K32) | — | E | `R_bs`: sensörden gövdeye dönüş; sensör çerçevesi kabulleri A5-K89 |
| `ayar` | tablo | — | türe göre (aşağıda) | profil | H | Profil değerlerinin geçersiz kılınması |

**Ürün profilleri.** Her anahtar verilmezse aşağıdaki değerle çözümlenir; değerler TOML değişmezi olarak yazılmıştır ve Ek E'nin `[profil.*]` tablolarıyla aynıdır (süreler tamsayı ns). **KE** = kalibre edilmedi: ideal değer, `meta.kalibre_edilmedi` listesine girer (A5-K101). **DY** = doğrulanmamış varsayılan: değer yazılmaz, model 05'in formülünü kullanır, uyarı zorunludur (A5-K108). **DS** = desteklenmiyor: verilirse reddedilir (A5-K102). Bütün `_ns` anahtarları ≥ 0; zaman sabitleri ve periyotlar > 0'dır (A5-K48).

| Tür (profil; `hiz_hz`) | Anahtar (birim; profil değeri) |
|---|---|
| `imu` (`icm42688p`; hız yok, kontrol periyodu) | `jiroskop_yogunluk` (rad/s/√Hz; `4.887e-5`), `ivme_yogunluk` (dizi[3], m/s²/√Hz; `[0.0006374, 0.0006374, 0.0006865]`), `jiroskop_acilis_bias_siniri` (rad/s; `0.008727`), `ivme_acilis_bias_siniri` (m/s²; `0.1961`), `jiroskop_sicaklik_kats_siniri` (rad/s/°C; `8.727e-5`), `ivme_sicaklik_kats_siniri` (m/s²/°C; `0.001471`), `jiroskop_olcek_siniri` (`0.005`), `jiroskop_eksenler_arasi_siniri` (`0.0125`), `ivme_olcek_siniri` (`0.005`), `ivme_eksenler_arasi_siniri` (`0.01`), `jiroskop_dogrusalsizlik` (`0.001`), `ivme_dogrusalsizlik` (`0.0`), `jiroskop_gm_tau_ns` (`1000000000000`), `ivme_gm_tau_ns` (`300000000000`), `jiroskop_gm_sigma` (rad/s; `4.848e-5`), `ivme_gm_sigma` (m/s²; `0.001`), `jiroskop_aralik` (rad/s; `8.7266`), `ivme_aralik` (m/s²; `39.2266`), `bit` (`16`), `suzgec_etkin` (`false`), `suzgec_tau_ns` (yazılmaz; süzgeç etkinse zorunlu, A5-K46), `dunya_donusu_etkin` (`true`) |
| `manyetometre` (`rm3100`; `100.0` Hz) | `gurultu_sigma` (T; `2e-8`), `yumusak_demir` (matris 3×3; `[[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]`), `sert_demir` (T; `[0.0, 0.0, 0.0]`), `sicaklik_kats` (T/°C; `0.0`), `itici_girisim_orani` (`0.2`), `teslim_gecikmesi_ns` (`1000000`) |
| `basinc` (`ms5837_30ba`; `25.0` Hz) | `gurultu_sigma` (Pa; `20.0`), `mutlak_dogruluk_siniri` (Pa; `5000.0`), `sicaklik_kats` (Pa/°C; `0.0`), `kuantalama` (Pa; `10.0`), `donusum_penceresi_ns` (`18750000`), `okuma_suresi_ns` (`1000000`), `dinamik_basinc_kats` (`0.0`), `sicaklik_ofset_siniri` (°C; `4.0`) |
| `gnss` (`neo_m9n`; `5.0` Hz) | `cep` (m; `2.0`), `gm_tau_ns` (`60000000000`), `gm_varyans_payi` (`0.9`), `dusey_carpan` (`1.5`), `hiz_sigma` (m/s; `0.05`), `teslim_gecikmesi_ns` (`120000000`), `uydu_sayisi` (`20`), `n_sv_ref` (`12`), `w50` (m; `0.03`), `w_gecis` (m; `0.005`), `t_kisa_ns` (`10000000000`), `t_efemeris_ns` (`7200000000000`; 2 saat, 05'in 2–4 saat aralığının alt ucu, mimar onayladı), `hizli_medyan_ns` (`1000000000`), `sicak_medyan_ns` (`2000000000`), `soguk_medyan_ns` (`24000000000`), `t_edinim_ns` (`0`; KE), `yakinsama_baslangic_hatasi` (m; `0.0`; KE), `yakinsama_tau_ns` (yazılmaz; `yakinsama_baslangic_hatasi > 0` ise zorunlu), `rapor_carpani` (`1.0`) |
| `dvl` (`a50`; hız yok, mod tablosu) | `isin_acisi` (rad; `0.39269908169872414`), `yari_guc_genisligi` (rad; `0.07679448708775051`), `azimut_ofseti` (rad; `0.7853981633974483`), `alt_isin_sayisi` (`7`), `gelis_acisi_siniri` (rad; `1.0471975511965976`), `sigma0` (m/s; `0.001`), `k_r` (1/s; `0.0002`), `k_u` (`0.001`), `uzun_donem_olcek_siniri` (`0.0101`), `kappa` (`1.0`), `c_ayar` (m/s; yazılmaz, KE: ortamın ses hızına eşit), `islem_suresi_ns` (`5000000`), `uc_isinla_cozum` (`false`), `su_takibi` (`false`), `baglanti` (`"seri"`), `seri_baud` (`115200`), `mod_histerezis_ping` (`3`), `kilit_arama_ping` (`3`), `yakin_mod_deneme_ns` (`10000000000`) |
| `ping1d` (`ping1d`; hız yok) | `kutu_sayisi` (`200`), `ping_araligi_ns` (`50000000`), `oto_menzil_carpani` (`1.5`), `islem_suresi_ns` (`0`; KE), `sogurma` (dB/m; DY), `algilama_esigi` (DS) |
| `ping360` (`ping360`; hız yok, kartın yapılandırması) | `baglanti` (`"usb"`), `baud` (yazılmaz, KE: iletim süresi 0), `t_m0_ns` (`0`; KE), `t_grad_ns` (`0`; KE), `islem_suresi_ns` (`0`; KE), `gurultu_tabani` (`0.0`; KE), `yakin_alan_genligi` (`0.0`; KE), `tvg_a` (dB; `20.0`), `sogurma` (dB/m; DY), `kazanc_ofsetleri` (DS), `l_min` (DS), `l_maks` (DS) |
| `kamera` (`kamera_on`; hız yok, A2 §6 kademesi) | `l_sonrasi_ns` (`0`; KE), `l_alg_ns` (`50000000`), `bozulma_modeli` (`"yok"`), `bozulma` (dizi[5]: k1 k2 p1 p2 k3; `[0.0, 0.0, 0.0, 0.0, 0.0]`), `kalibrasyon_surumu` (`1`), `ic_tam`, `ic_sade` (`{fx, fy, cx, cy}`, piksel; yazılmaz; seçilen kademeninki A5-K114'e göre zorunlu), `pozlama_ns` (yazılmaz; A5-K114'e göre zorunlu), `yakin_m` (yazılmaz; A5-K114'e göre zorunlu), `uzak_m` (yazılmaz; A5-K114'e göre zorunlu), `nominal_montaj` (yoksa `montaj`'ın kopyası, A5-K98). Çözünürlük ve hız kamera kademesindendir (A2 §6); aralıklar A2 §3.6 |
| `batarya_olcer` (`psm`; `10.0` Hz) | `gerilim_bolucu` (`11.0`), `akim_ofseti` (V; `0.33`), `akim_olcegi` (A/V; `37.8788`), `analog_gurultu_sigma` (V; `0.002`), `adc_bit` (`12`), `adc_referans` (V; `3.3`), `sicaklik_kats` (A/°C; `0.0`), `teslim_gecikmesi_ns` (`1000000`) |
| `sizinti` (`sos_leak`; `10.0` Hz) | `prob_sayisi` (`2`; [MV]), `algilama_gecikmesi_ns` (`1000000000`), `sahte_pozitif_orani` (`0.0`) |
| `firlatici_geri_okuma` (`firlatici_io`; `10.0` Hz) | `anahtar_gecikmesi_ns` (`10000000`), `teslim_gecikmesi_ns` (`1000000`). Okunan alanlar: tüp başına dolu, emniyet açık, fiş takılı, pim çıkık; araç başına acil durdurma. "Bakım" alanı yoktur |

Profil değerlerinin kaynakları 05 §2.3–2.5'tir (etiketleri orada); bu tablo onları değiştirmez, yalnız sabitler.

**Kamera alanları ve A2 §3.6 eşlemesi** (A5-K114; geçerli aralıkların tek kaynağı A2 §3.6'dır, burada yinelenmez):

| A5 | A2 §3.6 / A8 | Not |
|---|---|---|
| `ayar.ic_tam.{fx, fy, cx, cy}`, `ayar.ic_sade.{…}` (sayı, piksel) | `fx`, `fy`, `cx`, `cy` | Kademeye göre seçilir (tam 640 × 480, sade 320 × 240; A2 §6) |
| `ayar.bozulma_modeli` (`yok`, `brown_conrady`), `ayar.bozulma` (dizi[5]) | `bozulma_modeli` (0, 1), `bozulma` | Model yokken katsayılar 0 |
| `ayar.pozlama_ns` (tamsayı, ns) | `pozlama_ns` | |
| `ayar.yakin_m`, `ayar.uzak_m` (sayı, m) | `yakin_m`, `uzak_m` | |
| `ayar.kalibrasyon_surumu` (tamsayı) | `kalibrasyon_surumu`; A8 `calib_ver` | |
| `montaj` (fizik gerçeği) | `montaj_l_b`, `montaj_q_bs` | S çerçevesi; optik çerçeve dönüşümü A2-K94 |
| `ayar.nominal_montaj.{konum, yonelim}` | A8 `mount_l_b`, `mount_q_bs` (A8-K53) | Yoksa fizik montajının kopyası |
| `ayar.l_sonrasi_ns` (tamsayı, ns) | `t_teslim − t_olcum` | KE: 0 |
| `ayar.l_alg_ns` (tamsayı, ns; 5 ms–500 ms) | A8 §6.1 `L_alg` | Varsayılan 50 ms (A8 §6.1) |
| `kalite.kamera`, `kalite.kamera_gerekli` | A2 §6 kademe, A2-K50 | A5-K80 |

Yapılandırma sürümü, port, halka yuva sayısı ve `D_op` senaryo alanı değildir (A2 §3.6, A2-K93).

### 3.7 `otopilot` (besler: 02 SIL koşucusu; HIL'de A1/A4 yoluyla)

| Anahtar | Tip | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|
| `otopilot.parametreler[]` | dizi tablo | ≤ 1024 eleman; `kimlik` tekil | `[]` | H | Açık (kimlik, değer) çiftleri (A5-K52) |
| `otopilot.parametreler[].kimlik` | tamsayı | 0–65535 | — | E | A4 parametre kimliği (`ida_param_value_t.id`) |
| `otopilot.parametreler[].tip` | dizgi | `i32`, `u32`, `f32` | — | E | A4 `IDA_PT_*` |
| `otopilot.parametreler[].deger` | sayı | tipin aralığı (A5-K52) | — | E | Değer |

### 3.8 `calistirma` ve `rastgelelik`

| Anahtar | Tip | Birim | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|---|
| `calistirma.kip` | dizgi | — | `sil`, `hil_1x`, `hil_bariyer` | — | E | Zaman kipi (iskelet §4) |
| `calistirma.sure_ns` | tamsayı | ns | > 0 | — | E | Çalıştırmanın sim zamanı süresi |
| `calistirma.sil_hiz_carpani` | sayı | — | > 0 | SIL'de 1,0 (A5-K98) | H; HIL'de yasak | Başlangıç hız çarpanı |
| `calistirma.sil_azami_hiz` | mantıksal | — | — | SIL'de false (A5-K98) | H; HIL'de yasak | Olabildiğince hızlı |
| `rastgelelik.ana_tohum` | tamsayı | — | 0 … 2⁵³−1 | — | E | Ana tohum (A5-K82) |

### 3.9 `ortam` (besler: 04 çevre, 05 akustik, IMU ve GNSS)

| Anahtar | Tip | Birim | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|---|
| `tur` | dizgi | — | `havuz`, `acik_su` | — | E | Ortam |
| `sahne` | dizgi | — | A7 kimliği | — | E | A7 `statik` türündeki varlık (A5-K19) |
| `sahne_poz.konum`, `sahne_poz.yonelim` | dizi[3], kuaterniyon | m, — | birim | (0, 0, 0), (1, 0, 0, 0) | H | Statik varlığın `REF` noktasının NED pozu (A2 §3.4 `statik_p_n`, `statik_q_nb`) |
| `su_optigi.beta_d_m`, `su_optigi.beta_b_m` | dizi[3] | 1/m | ≥ 0 | KE: 0 (A5-K101) | H | R, G, B zayıflama ve geri saçılım (A2 §3.4) |
| `su_optigi.b_sonsuz_rgb_dogrusal` | dizi[3] | — | [0, 1] | KE: 0 | H | Sonsuz uzaklık geri saçılım rengi |
| `su_optigi.bulaniklik_olcegi` | sayı | — | ≥ 0 | KE: 0 | H | İleri saçılım bulanıklığı |
| `su_optigi.surum` | tamsayı | — | 1 … 2³²−1 | 1 | H | A2 `su_optigi_surumu` |
| `isik.gunes_yonu_n` | dizi[3] | — | birim | KE: (0, 0, −1) | H | Sahneden güneşe birim vektör, NED (A2 §3.4) |
| `isik.gunes_isinimi_w_m2` | sayı | W/m² | ≥ 0 | KE: 1000 | H | Yüzeyde güneş ışınımı |
| `isik.ortam_isigi_rgb_dogrusal` | dizi[3] | — | ≥ 0 | KE: 0 | H | Doğrusal RGB ortam ışığı |
| `derinlik` | sayı | m | > 0 | — | E | Sabit su derinliği `h`; A5-K36 |
| `orijin.enlem`, `orijin.boylam` | sayı | rad | [−π/2, π/2], [−π, π] | — | K (A5-K45) | NED orijininin WGS-84 konumu |
| `orijin.yukseklik` | sayı | m | — | — | K | Elipsoit yüksekliği |
| `yercekimi` | sayı | m/s² | > 0 | 9,82 | H | `g` (04 §2.6) |
| `atmosfer_basinci` | sayı | Pa | > 0 | 101 325 | H | `p_atm` (standart atmosfer) |
| `hava_yogunlugu` | sayı | kg/m³ | > 0 | 1,225 | H | `ρ_a` (04 §2.5) |
| `su.yogunluk` | sayı | kg/m³ | [900, 1100] | havuz 1000, açık su 1025 (A5-K98) | H | `ρ` |
| `su.sicaklik_c` | sayı | °C | [−2, 40] | — | E | Su sıcaklığı [G] |
| `su.tuzluluk` | sayı | kg/kg | [0; 0,05] | havuz 0, açık su 0,035 (A5-K98) | H | Ses hızı ve soğurma |
| `su.ph` | sayı | — | [6, 9] | 8 | H | Soğurma |
| `su.ses_hizi` | sayı | m/s | [1300, 1700] | formülden | H | Sabit `c` geçersiz kılması |
| `manyetik_alan` | dizi[3] | T | — | — | K (A5-K44) | `B_n` (NED) |
| `dalga.spektrum` | dizgi | — | `yok`, `pierson_moskowitz`, `jonswap` | `yok` | H | Spektrum (04 §2.5) |
| `dalga.hs` | sayı | m | > 0 | — | K (A5-K37) | `H_s` |
| `dalga.tepe_periyodu_ns` | tamsayı | ns | > 0 | — | K | `T_p` |
| `dalga.gamma` | sayı | — | ≥ 1 | jonswap'ta 3,3 [MV] (A5-K98) | H | JONSWAP tepe çarpanı |
| `dalga.yon` | sayı | rad | — | — | K | `β`, kuzeyden doğuya, dalganın ilerlediği yön |
| `dalga.yayilim` | dizgi | — | `tek_yon`, `cos2s` | `cos2s` [MV] | H | Yönsel yayılım; `cos2s`: `D(β) ∝ cos^(2s)((β − β₀)/2)` |
| `dalga.yayilim_s` | sayı | — | > 0 | 10 [MV] | H | cos-2s üssü `s` |
| `dalga.bilesen_sayisi` | tamsayı | — | 1–256 (A2 §3.4; A5-K116) | 64 | H | Bileşen sayısı (04 §2.5) |
| `akinti.hiz_ned`, `ruzgar.hiz_ned` | dizi[3] | m/s | rüzgârda d = 0 (A5-K39) | (0, 0, 0) | H | Düzgün akıntı / rüzgâr |
| `akinti.gauss_markov.*`, `ruzgar.gauss_markov.*` | `etkin` (false); `tau_ns` > 0; `sigma` ≥ 0 (m/s); `sinir` > 0 (m/s) | | | | K (A5-K38) | Sınırlı Gauss–Markov bileşeni |

### 3.10 `hedefler`, `gorev`, `olaylar`

**`hedefler.<ad>`** (en çok 61; A5-K116; besler 04 §2.8, 05, 07):

| Anahtar | Tip | Birim | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|---|
| `kimlik` | tamsayı | — | 0–65535, tekil | — | E | Hedef kimliği (04 isabet olayında `hedef_id`) |
| `sinif` | dizgi | — | `bilinmiyor`, `sabit_nesne`, `hareketli_nesne`, `yapi` | `bilinmiyor` | H | Algılama kaydının sınıf kodu (A5-K113; tablo aşağıda); seçim anlamı yok |
| `varlik` | dizgi | — | A7 kimliği | — | E | A7 `hedef` türündeki varlık; şekil, isabet bölgesi ve malzeme A7'dedir |
| `konum` | dizi[3] | m | — | — | E | Başlangıç, NED |
| `yonelim` | kuaterniyon | — | birim | — | E | Gövdeden NED'e |
| `yonelim_rastgele` | mantıksal | — | — | false | H | true ise başlangıç yaw'ı Ek D.4'teki çekilişle değiştirilir |
| `hareket.tur` | dizgi | — | `sabit`, `sabit_hiz`, `rastgele_yuruyus`, `sinus` | `sabit` | H | Kinematik hareket |
| `hareket.hiz_ned` | dizi[3] | m/s | — | — | K (`sabit_hiz`) | |
| `hareket.yuruyus_yogunlugu`, `hareket.azami_hiz` | sayı | m/s/√s, m/s | ≥ 0, > 0 | — | K (`rastgele_yuruyus`) | |
| `hareket.genlik_ned`, `hareket.periyot_ns`, `hareket.faz` | dizi[3], tamsayı, sayı | m, ns, rad | —, > 0, — | — | K (`sinus`; üçü de) | `konum + genlik · sin(2π t / periyot + faz)` |

**Sınıf tablosu (A5-K113; A8 `class_id`).** Yalnız algılama kaydının sınıf kodlamasıdır; hedef seçimi ya da ölçüt anlamı taşımaz.

| `class_id` | `sinif` |
|---|---|
| 0 | `bilinmiyor` |
| 1 | `sabit_nesne` |
| 2 | `hareketli_nesne` |
| 3 | `yapi` |
| 4–255 | ayrılmış |

**`gorev`** (besler 02 §2.6 yer istasyonu yolundan; öğeler A1 Tablo 3.6a'nın alanlarıdır, A5-K55):

| Anahtar | Tip | Birim | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|---|
| `gorev.yukleme_zamani_ns` | tamsayı | ns | ≥ 0 | 0 | H | A5-K57 |
| `gorev.ogeler[i].item_type` | dizgi | — | `WAYPOINT`, `DIVE_TO`, `SURFACE`, `HOLD`, `SCAN_LINES`, `SCAN_POINT`, `WAIT_GNSS` | — | E | A1 Tablo 3.6a `Ad` (item_type 1–7) |
| `frame` | dizgi | — | `NONE`, `LOCAL`, `GLOBAL` | — | E | A1 Tablo 3.6a |
| `p` | dizi[4] sayı | A1 Tablo 3.6a | A1 Tablo 3.6a | — | E | `p[0]`…`p[3]` |
| `lat_e7`, `lon_e7` | tamsayı | 10⁻⁷ derece | A1 Tablo 3.6a | — | E | |
| `north_m`, `east_m` | sayı | m | A1 Tablo 3.6a | — | E | |
| `depth` | sayı | m | A1 Tablo 3.6a | — | E | Aşağı pozitif |

**`olaylar[i]`** (arıza ve olay zaman çizelgesi; en çok 4096):

| Anahtar | Tip | Birim | Aralık | Zorunlu | Anlam |
|---|---|---|---|---|---|
| `kimlik` | dizgi | — | `^[A-Za-z][A-Za-z0-9_]{0,31}$`, tekil, `^E[0-9]+$` değil | E | `fault_id` (A5-K60) |
| `sinif` | dizgi | — | `sensor`, `aktuator`, `guc`, `baglanti`, `protokol`, `sistem`, `emniyet_girdisi`, `gcs_komutu` | E | Sınıf |
| `zaman_ns` | tamsayı | ns | ≥ 0 | K (A5-K61) | Başlangıç anı |
| `kosul.buyukluk` / `.karsilastirma` / `.deger` | dizgi / dizgi / sayı | — | `derinlik` (yer gerçeği, `O_b`, m), `gorev_ogesi` (kartın telemetrideki etkin öğe indisi); `>`, `>=`, `<`, `<=`, `==` | K | A5-K63 |
| `sure_ns` | tamsayı | ns | ≥ 0 | yalnız geçici arıza (A5-K109) | 0 = süresiz |
| `tur` | dizgi | — | Tablo 3.10-A | K | |
| `hedef` | dizgi | — | A5-K65 | K | |
| `siddet.*` | sayı / tamsayı | kanal birimi | `ofset`, `olcek`, `oran`, `son_deger`, `tau_ns` (> 0), `k` (≥ 1), `p` ([0, 1]), `genlik` (≥ 0), `gecikme_ns`, `titresim_ns`, `carpan` (≥ 0), `soc` | K (A5-K68) | Türün kullandığı alanlar Tablo 3.10-A |
| `kanal_maskesi` | tamsayı | — | A5-K69 | yalnız `sensor` | |
| `sira_boz` | mantıksal | — | — | yalnız `sensor` | Geç teslimde sıra korunmaz |
| `zaman_deseni.tur` / `.p` / `.t_iyi_ns` / `.t_kotu_ns` | dizgi / sayı / tamsayı / tamsayı | —, —, ns, ns | `surekli`, `bernoulli`, `gilbert_elliott`; [0, 1]; > 0; > 0 | yalnız `sensor` (A5-K67) | |
| `yon` | dizgi | — | `yukari`, `asagi`, `iki_yon` | K (`baglanti`, `protokol`) | `yukari` = karta doğru |
| `duzlem` | dizgi | — | `hil`, `gcs` | K (`protokol`) | 06 §2.12 |
| `mesaj_turu` | dizgi | — | A1 mesaj adı `^[A-Z][A-Z0-9_]{0,63}$` | H (`protokol`) | Yoksa bütün mesajlar |
| `profil` | dizgi | — | `baglanti.profiller` anahtarı | K (`profil_degisimi`) | |
| `girdi`, `deger` | dizgi, mantıksal | — | `acil_durdurma`, `fis`, `pim`; true = basılı / takılı | K (`emniyet_girdisi`) | |
| `komut`, `kip` | dizgi | — | `ARM`, `DISARM`, `SET_MODE`; `MANUAL`, `STABILIZE`, `DEPTH_HOLD`, `STATION_KEEP`, `GUIDED`, `AUTO` | K (`gcs_komutu`) | Araç (itici) silahlandırması ve kip; atış komutu değildir |
| `problar` | dizi tamsayı | — | 0–15, prob sayısından küçük | K (`su_girisi`) | |

**Tablo 3.10-A — desteklenen türler ve etkileri.** Bu sürüm yalnız etkisi burada tam tanımlanan türleri destekler; diğerleri A5-K102 ile reddedilir. `y` maskelenen kanalın arıza zincirindeki değeri, `t_b` olayın uygulandığı adım sınırı, `t_meas` kaydın ölçüm zamanıdır (ns). Rastgele çekilişler Ek D'deki akış ve sayaç kurallarıyla yapılır.

| Sınıf | `tur` | Zorunlu `siddet` | Etki |
|---|---|---|---|
| `sensor` | `ofset` | `ofset` | `y ← y + ofset` |
| | `olcek` | `olcek` | `y ← olcek · y` |
| | `suruklenme_dogrusal` | `oran` | `y ← y + oran · (t_meas − t_b) · 10⁻⁹` |
| | `suruklenme_ustel` | `son_deger`, `tau_ns` | `y ← y + son_deger · (1 − exp(−(t_meas − t_b)/tau_ns))` |
| | `gurultu_artisi` | `k` | `y ← y + σ_nom √(k² − 1) z`, `z ~ N(0, 1)`; `σ_nom` kanalın profildeki örnek başına σ'sı, yoğunlukla tanımlı kanalda `ASD/√(2Δt)` (05 §2.3). Yalnız `imu`, `manyetometre`, `basinc`, `gnss`, `dvl`, `batarya_olcer` |
| | `sicrama` | `genlik`, `p` | Her örnekte `p` olasılıkla `y ← y + genlik · s`, `s ∈ {−1, +1}` eşit olasılıklı |
| | `donma` | — | Yük, arıza başlamadan önceki son arızalı çıktıyla tekrarlanır; `t_meas` ve `seq` ilerler. Dondurulacak önceki çıktı yoksa (arıza sensörün ilk kaydından önce ya da `zaman_ns = 0`'da başladıysa) sensör arıza bitene kadar hiç kayıt üretmez; bu durum `donma_onceki_cikti_yok` sayacını artırır ve olay olarak A6'ya yazılır; başlangıç reddedilmez |
| | `donma_zaman_damgasi` | — | Yük, `t_meas` ve `window_ns` donar; `seq` ilerler. Önceki çıktı yoksa `donma` ile aynı kural |
| | `yanlis_gecerlilik` | — | `geçerli` biti tersine çevrilir; yük değişmez |
| | `kendini_bildiren` | — | `sensör_arızası` biti 1 olur; yük değişmez |
| | `kayip` | — | Kayıt teslim edilmez; `seq` boşluğu kalır |
| | `gec_teslim` | `gecikme_ns`; `titresim_ns` isteğe bağlı | `t_teslim' = t_teslim + gecikme_ns + j`, `j` [−titresim, +titresim] düzgün tamsayı ns (A5-K66); `sira_boz = false` iken `t_teslim'_n = max(t_teslim'_n, t_teslim'_{n−1})` |
| | `motor_takilmasi` | — | Yalnız `ping360`: kafa açısı ilerlemez, pingler o açıda sürer; bitince kaldığı açıdan devam eder |
| | `dvl_isin_kaybi` | — (maske zorunlu) | Yalnız `dvl`: maskedeki ışınlar (bit 0–3) geçersiz, çözüm kalan ışınlarla yeniden hesaplanır |
| | `guc_cevrimi` | — (`sure_ns > 0`) | Süre boyunca kayıt yok; bitişte açılış hataları açılış akışından yeniden çekilir (sayaç = çevrim no) |
| `aktuator` | `itki_carpani` | `carpan` ∈ [0, 1] | `F_i ← carpan · F_i` (süzgeç ve `β` sonrası); akım arızasız itkiden hesaplanır |
| | `esc_sinyal_kaybi` | — | ESC darbe almaz: `F_ss = 0` (nötr); bitişte `baslatma_suresi_ns` nötr, sonra olağan |
| `guc` | `ic_direnc_artisi` | `carpan` ≥ 1 | `R_top ← carpan · R_top` |
| | `bara_kopmasi` | — | `bara_bağlı = false`: `V_bara = 0`, bütün itici itkisi ve akımı 0, aviyonik yük 0; bitişte bara bağlanır, ESC'ler `baslatma_suresi_ns` nötr (04 §2.7.8) |
| `baglanti` | `kesinti` | — | Yalnız GCS düzlemi, `yon` yönünde: `datagram` profilinde çerçeveler düşer; `stream` profilinde akış durur ve 06 §2.11 stream kuralları uygulanır |
| | `profil_degisimi` | — | Kalıcı atama: uygulama sınırından sonra gönderilen çerçeveler `profil`'in modeline tabidir; yoldaki çerçeveler etkilenmez |
| `protokol` | `dusurme` | `p` | `duzlem` ve `yon`'daki her uygun çerçeve `p` olasılıkla atılır |
| | `cogaltma` | `p` | Çerçeve `p` olasılıkla art arda iki kez yazılır |
| | `yer_degistirme` | `p` | Çerçeve `p` olasılıkla aynı yöndeki bir sonraki çerçeveyle yer değiştirir; pencere içinde sonraki yoksa etkisiz |
| | `bayt_bozma` | `p` | `p` olasılıkla bir bayt (konum düzgün [0, uzunluk−1]) düzgün [1, 255] değerle XOR'lanır |
| | `kesme` | `p` | `p` olasılıkla çerçeve düzgün [1, uzunluk−1] bayta kısaltılır |
| `sistem` | `kart_sifirlama` | — | Anlık: platformun yeniden başlatma kancası (06 §2.9) |
| | `su_girisi` | — | Kalıcı atama: `problar` uygulama sınırından itibaren ıslak (sızıntı yer gerçeği, 05 §2.3.7) |
| `emniyet_girdisi` | (yok) | — | Kalıcı atama: `girdi` (ve `hedef`) değeri `deger`'e atanır |
| `gcs_komutu` | (yok) | — | Anlık: komut GCS ağ geçidi → emülatör → A1 yolundan gönderilir |

**Desteklenmeyen türler (bu sürümde reddedilir):** `nan`, `aralik_disi`, `kismi` (sensör); `tutukluk`, `dolanma` (aktüatör); `ani_cokme`, `dusuk_batarya` (güç); `gecikme_sicramasi`, `cop_bayt` (protokol). Etkileri sözleşmede tam tanımlanamadı (8. başlık).

**Bileşim.** Aynı kayda birden çok sensör arızası etkiliyorsa 05 §2.7'nin sırası uygulanır (ofset → ölçek → sürüklenme → gürültü → sıçrama → donma → yanlış geçerlilik → kendini bildiren → kayıp → geç teslim); aynı türden birden çok olay dosya sırasıyla. Zaman deseni: `surekli` her örnekte etkin; `bernoulli` her örnekte bağımsız `p` olasılıkla; `gilbert_elliott` olay zamanlı iki durumlu süreçtir: olayın başladığı sınırda "iyi" durumda başlar, kalış süreleri ortalaması `t_iyi_ns` ve `t_kotu_ns` olan üstel dağılımdan Ek D.5'in geçiş dizisiyle çekilir, kayıt `t_meas` anında "kötü" durumdaysa arıza etkindir. (05 §2.7'nin adım başına `1 − exp(−h/T)` biçimi yerine olay zamanlı biçim seçildi; ortalama kalış süreleri aynıdır.)

### 3.11 `emniyet_girdileri`, `baglanti`, `kalibrasyon`

| Anahtar | Tip | Birim | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|---|
| `emniyet_girdileri.acil_durdurma` | mantıksal | — | — | false (basılı değil) | H | Sanal acil durdurma |
| `emniyet_girdileri.fis.f1` / `.f2` | mantıksal | — | — | false (çıkık) | H | Silahlandırma fişi takılı mı |
| `emniyet_girdileri.pim.f1` / `.f2` | mantıksal | — | — | true (takılı) | H | Emniyet pimi takılı mı |
| `baglanti.etkin_profil` | dizgi | — | `profiller` anahtarı | — | E (tablo varsa) | Başlangıç profili |
| `baglanti.profiller.<ad>.kademe` | dizgi | — | `L0`, `L1`, `L2` | — | E | L0 ideal; L1 gecikme + bant + Bernoulli; L2 zamana bağlı Gilbert–Elliott (06 §2.11) |
| `.tasima` | dizgi | — | `datagram`, `stream` | `datagram` | H | |
| `.yukari.*`, `.asagi.*` | `gecikme_taban_ns`, `titresim_ns` (≥ 0; 0); `bant_genisligi` (bit/s > 0; yoksa sınırsız); `kuyruk_siniri` (bayt; yoksa sınırsız); `kayip_modeli` (`yok`, `bernoulli`, `gilbert_elliott`; `yok`); `kayip_p`; `ge_lambda_gb`, `ge_lambda_bg` (1/s > 0); `ge_k`, `ge_h` ([0, 1]); `hava_paketi` (bayt); `bayt_bozma_orani` ([0, 1]) | | | | K (A5-K74) | Yön başına model |
| `.stream.*` | `rto0_ns`, `rto_maks_ns`, `t_kopma_ns`, `t_yeniden_ns` (> 0, hepsi zorunlu) | | | — | K (stream) | |
| `.telsiz.*` | | | | — | desteklenmiyor | Telsiz marj modeli bu sürümde yok |
| `kalibrasyon.malzeme.<A7 malzeme kimliği>.akustik.*` | `geri_sacilim_mu`, `gelis_yasasi`, `ayna_yansima_rs`, `puruzluluk_gamma_p` | A7 §3.4 | A7 §3.4 ve A7-K57 (aynen) | A7 değeri | H | A7 varsayılanını geçersiz kılar (A5-K18) |
| `kalibrasyon.malzeme.<A7 malzeme kimliği>.optik.*` | `taban_rengi_dogrusal` (dizi[4]), `metaliklik`, `puruzluluk` | — | A2 §3.4 `MalzemeOptik` ve A7 §3.4 (0–1) | glTF değeri | H | Optik geçersiz kılma (A7-K30) |

### 3.12 `kalite` (iskelet §6)

| Anahtar | Tip | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|
| `kalite.gorunum` | dizgi | `kapali`, `sade`, `tam` | `sade` | H | 3B görünüm (A2) |
| `kalite.kamera` | dizgi | `kapali`, `sade`, `tam` | `sade` | H | Kamera sensörü; değerleri A2 §6 |
| `kalite.kamera_gerekli` | mantıksal | — | false | H | Çalıştırma kamerayı gerektirir (A2-K50; A5-K80) |
| `kalite.sonar` | dizgi | `sade`, `tam` | `sade` | H | A5-K78, K79 |
| `kalite.kayit` | dizgi | `olaylar`, `telemetri`, `tam` | `telemetri` | H | A6 kayıt kademesi; emniyet kaydı her kademede (A5-K78) |
| `kalite.kamera_kareleri` | dizgi | `kapali`, `pencereli`, `surekli` | `kapali` | H | A6 kamera kare kaydı (A6-K19) |
| `kalite.sonar_tam.isin_sayisi` | tamsayı | 1 … 10⁷; kartlı kipte A5-K79 kümesi | — | K (tam) | Ping başına ışın |
| `kalite.sonar_tam.azami_sekme` | tamsayı | 0–2 | tamda 2 (A5-K98) | H | Speküler sekme sayısı |
| `kalite.sonar_tam.hucre_kenari` | sayı, m | > 0 | tamda 0,01 | H | Saçıcı hücresi `Δ_h` (W0-MP7a kuralı) |
| `kalite.sonar_tam.yuzey_yamasi_araligi` | sayı, m | > 0 | tamda 0,05 | H | `Δ_y` |

### 3.13 `baslangic`

| Anahtar | Tip | Birim | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|---|
| `konum` | dizi[3] | m | — | (0, 0, 0) | H | `O_b`, NED |
| `yonelim` | kuaterniyon | — | birim | (1, 0, 0, 0) | H | Gövdeden NED'e |
| `hiz` | dizi[6] | m/s, rad/s | — | sıfır | H | `ν`, gövde, `O_b` |
| `yuklu_tupler` | dizi dizgi | — | `f1`, `f2`; tekrarsız | `["f1", "f2"]` | H | A5-K86 |
| `batarya_soc` | sayı | — | [0, 1] | 1,0 | H | 04 §2.7.6 |
| `batarya_v1` | sayı | V | — | 0,0 | H | RC kolu gerilimi |

### 3.14 Doğrulama raporu (A5-K92)

| Alan | Tip | Anlam |
|---|---|---|
| `gecerli` | mantıksal | Hiç `hata` yoksa true |
| `davranis_ozeti` | dizgi \| null | Geçerliyse A5-K17 özeti |
| `kaynak_ozetleri` | tablo dosya → dizgi | Okunan her dosyanın ham bayt SHA-256'sı |
| `bulgular[]` | dizi | Bulgular |
| `bulgular[].kural` | dizgi | `A5-K` + 2–3 basamak |
| `bulgular[].onem` | dizgi | `hata` \| `uyari` |
| `bulgular[].asama` | tamsayı | 1, 2, 3 (A5-K90) |
| `bulgular[].dosya` | dizgi | Depo köküne göre göreli yol |
| `bulgular[].yol` | dizgi | A5-K93 sözdiziminde anahtar yolu; kök ya da bilinmeyen için boş dizgi |
| `bulgular[].satir` | tamsayı \| null | A5-K94 |
| `bulgular[].ileti` | dizgi | İnsan okur açıklama |

### 3.15 `varliklar` (A5-K115) ve A5'in ürettiği olay ve kayıtlar (A5-K117)

| Anahtar | Tip | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|
| `varliklar.kume` | dizgi | A7 `kume_kimligi` (`^[a-z0-9_]{1,32}$`) | — | E | Küme yolu `assets/<kume>/manifest.json` (A7-K01) |
| `varliklar.kume_surumu` | tamsayı | ≥ 1 | — | H (yoksa uyarı) | Beklenen `kume_surumu` (A7-K07) |
| `varliklar.manifest_ozeti` | dizgi | 64 küçük onaltılık | — | H (yoksa uyarı) | Beklenen manifest özeti (A7-K05) |

A5'in tanımladığı olay ve kayıt içerikleri (taşıyıcı ve kanal adları A3/A6'nındır; W1-uyum U18). Bütün zamanlar sim zamanı ns'dir.

| Ad | Ne zaman | Alanlar |
|---|---|---|
| `atama_celiskisi` | Aynı fizik sınırında aynı alana farklı değer düşen atamalar (A5-K63) | `t_ns` (sınır), `alan` (anahtar yolu, ör. `emniyet_girdileri.pim.f1`), `degerler[]`, `olay_kimlikleri[]` (dosyadaki sırayla), `uygulanan_deger` |
| `gevsetme_reddedildi` | Senaryonun gevşeten emniyet ataması o sınırda revizyon arttığı için reddedildi (A5-K63; iskelet §3.2 adım 4) | `t_ns`, `olay_kimligi`, `alan`, `istenen_deger`, `revizyon_adim_basi` (u32), `revizyon_adim_3_sonu` (u32) |
| `donma_onceki_cikti_yok` | `donma`/`donma_zaman_damgasi` başladığında dondurulacak önceki çıktı yok (Tablo 3.10-A) | `t_ns`, `olay_kimligi`, `sensor` (ad), `sayac` (bu çalıştırmadaki toplam, u32) |
| Olay sonucu | Her zaman çizelgesi öğesi için bir kez | `olay_kimligi`, `sonuc` ∈ {`uygulandi`, `reddedildi`, `uygulanmadi`}, `t_ns` (uygulandığı ya da reddedildiği sınır; `uygulanmadi`'da çalıştırma sonu), `neden` (`reddedildi`'de: `gevsetme_reddedildi`, `desteklenmiyor`, `hedef_yok`; `uygulanmadi`'da: `kosul_saglanmadi`) |
| Doğrulama raporu | Yüklemede bir kez | §3.14'teki JSON nesnesi, değiştirilmeden; A6'ya ek olarak (ön denetim raporundan ayrı) |

---

## 4. Davranış kuralları

### 4.1 Dosya, sürüm, tip ve sayılar

- **A5-K01** Dosya UTF-8 kodlu, TOML 1.0.0 belirtimine uygun ve `.toml` uzantılıdır; ayrıştırılamayan dosya reddedilir. (TOML 1.0.0: geçerli UTF-8 zorunlu; bir anahtarı ya da tabloyu iki kez tanımlamak geçersizdir; https://toml.io/en/v1.0.0)
- **A5-K02** Okuyucu `arac` ve `senaryo` türlerini girdi olarak, `cozumlenmis_senaryo` türünü yalnız A6'dan yeniden yükleme için (JSON, kanonik baytlar) kabul eder; yeniden yüklenen baytların SHA-256'sı kayıttaki davranış özetiyle eşit değilse reddedilir.
- **A5-K03** `meta.sema_surumu`'nun major bölümü 1 değilse dosya reddedilir; senaryo ile başvurduğu araç dosyasının major bölümü aynıdır.
- **A5-K04** Dosyanın minor bölümü okuyucunun minor bölümünden büyükse reddedilir; küçük ya da eşitse kabul edilir. Minor artış yalnız isteğe bağlı yeni anahtar, yeni enum değeri ya da desteklenmeyen bir anahtarın ya da türün desteklenmesini ekler; anahtar silme, anlam ya da birim değişikliği, zorunlu anahtar ekleme ve varsayılan değişikliği major artıştır.
- **A5-K05** Zorunlu (E) anahtarın eksik olduğu dosya reddedilir; koşullu (K) anahtar koşulu sağlandığında eksikse reddedilir.
- **A5-K06** Bu sözleşmede tanımlanmamış her anahtar reddedilir; A5-K07 dışında istisna yoktur. "Bakım" emniyet girdisi, iç basınç ve iç sıcaklık sensörü, PWM kaybı seçimi, kontrol hızı, HIL geçersizlik eşiği, torpido itki alanları ve çözümleyici üst verisi girdi dosyasında bu kuralla reddedilir.
- **A5-K07** `x_` önekli anahtar her tabloda (anahtarlı koleksiyonlar ve dizi tabloları dahil) kabul edilir; anahtarlı koleksiyonda girdi sayılmaz (ör. `arac.sensorler.x_not` bir sensör değildir); bir `uyari` üretir; çözümlenmiş senaryoya aynen yazılır, davranış özetine girer ve hiçbir model tarafından okunmaz. Kullanıcı adları (sensör, hedef, profil, malzeme, parametre) `x_` ile başlayamaz.
- **A5-K08** Sözleşmenin tanımladığı anahtarlar ASCII çıplak anahtardır (TOML 1.0: `A-Za-z0-9_-`); kullanıcı adları tablolardaki desenlere uyar; yalnız A7 kimlikleri tırnaklı anahtar olabilir. Türkçe grup adlarının dosyadaki karşılığı Ek B'dedir.
- **A5-K09** Tip `tamsayı` olan anahtarda TOML tamsayısı zorunludur; `3.0` gibi kayan noktalı yazım reddedilir. Tip `sayı` olan anahtar TOML tamsayısını ve kayan noktalı sayıyı kabul eder. Mantıksal alan yalnız `true`/`false`, dizgi yalnız TOML dizgisidir; örtük dönüşüm yapılmaz. (JSON Schema 2020-12'de `integer` kesir kısmı sıfır olan sayıyı da kabul ettiğinden doğrulayıcı TOML tipini ayrıca denetler.)
- **A5-K10** Her değer şemadaki aralık, değer kümesi, desen ve eleman sayısı sınırlarına uyar; her tamsayı [−(2⁵³−1), 2⁵³−1] aralığındadır (A5-K17'nin kayıpsızlığı için; RFC 8785 Ek B'deki öneri).
- **A5-K96** Ondalık ve kayan noktalı TOML yazımı IEEE 754 binary64'e en yakına, eşitlikte çift mantisli değere yuvarlanarak çevrilir. Sonucu sonsuz olan (taşma), `inf` ya da `nan` yazılan ve sıfırdan farklı olup sıfıra yuvarlanan (alt taşma) değer reddedilir; alt normal sonuç kabul edilir; −0 değeri +0'a çevrilir. `sayı` alanındaki TOML tamsayısı binary64'e tam çevrilir (A5-K10 sınırı içinde kayıpsızdır). Sınır vektörleri:

  | TOML yazımı | binary64 (onaltılık) | Sonuç | JCS dizilimi |
  |---|---|---|---|
  | `0.1` | `0x3FB999999999999A` | kabul | `0.1` |
  | `15.6` | `0x402F333333333333` | kabul | `15.6` |
  | `-0.0` | `0x0000000000000000` | kabul, +0 | `0` |
  | `4.9e-324` | `0x0000000000000001` | kabul (alt normal) | `5e-324` |
  | `2.4703282292062328e-324` | `0x0000000000000001` | kabul | `5e-324` |
  | `2.4703282292062327e-324` | 0 | ret (alt taşma) | — |
  | `1e-400` | 0 | ret (alt taşma) | — |
  | `1.7976931348623157e308` | `0x7FEFFFFFFFFFFFFF` | kabul | `1.7976931348623157e+308` |
  | `1.7976931348623159e308` | +∞ | ret (taşma) | — |
  | `9007199254740993.0` | `0x4340000000000000` | kabul (eşitlikte çifte: 9007199254740992) | `9007199254740992` |
  | `inf`, `nan` | — | ret | — |
- **A5-K97** `x_` alanlarının değerleri dizgi, tamsayı, sayı, mantıksal, dizi ve tablodan oluşur; TOML tarih ve saat türleri (yerel ya da ofsetli) hiçbir anahtarda kabul edilmez.
- **A5-K11** Birimler 2. başlıktaki kabullere uyar; değerin yanında birim dizgisi yazılmaz. Tek istisna görev öğeleridir: A1 Tablo 3.6a'nın alanlarını ve birimlerini (s, m, m/s, rad; `lat_e7`, `lon_e7` 10⁻⁷ derece) dönüşümsüz taşırlar (A5-K55).
- **A5-K102** "Desteklenmiyor" işaretli anahtar (`arac.batarya.sigorta_i2t`, `arac.batarya.hucre`, `arac.anten_konumu`, `baglanti.profiller.<ad>.telsiz`, `ping1d` `ayar.algilama_esigi`, `ping360` `ayar.kazanc_ofsetleri`, `ayar.l_min`, `ayar.l_maks`), desteklenmeyen batarya kipi (`b0`, `b1`; A5-K28) ve desteklenmeyen olay türü (3.10) verilirse senaryo aşama 2'de "bu sürümde desteklenmiyor" iletisiyle reddedilir. Bunların desteklenmesi minor sürümle eklenir.

### 4.2 Dosya düzeni, birleştirme, çözümleme ve özet

- **A5-K12** Senaryo dosyası tam olarak bir araç dosyasına `arac.dosya` ile başvurur. Yol, senaryo dosyasının dizinine göre göreli, `/` ayırıcılıdır; mutlak yol ve `..` bileşeni reddedilir. Başvurulan dosya yoksa ya da `meta.tur` `arac` değilse senaryo reddedilir. Araç dosyası başka dosyaya başvuramaz.
- **A5-K13** Öncelik sırası (düşükten yükseğe): bu sözleşmedeki varsayılan → araç dosyası → senaryonun `[arac]` alt ağacı. `otopilot` için: varsayılan → araç dosyası → senaryo. Malzeme için: A7 varsayılanı → `kalibrasyon.malzeme`.
- **A5-K14** Birleştirme tablolarda anahtar anahtar özyinelemelidir; yaprak değerler (diziler dahil) bütün olarak değiştirilir; diziler eleman eleman birleştirilmez.
- **A5-K15** Senaryodaki geçersiz kılma anahtarlı koleksiyonlara (`arac.iticiler`, `arac.firlaticilar`, `arac.sensorler`) araç dosyasında bulunmayan girdi ekleyemez ve girdi silemez (`x_` anahtarları girdi değildir, A5-K07). Bir sensörü kapatmanın tek yolu `etkin = false`'tur.
- **A5-K16** Çözümleyici şu sırayla tek bir çözümlenmiş senaryo üretir: (1) A5-K13/K14 birleştirmesi; (2) kaynak alanlarının çıkarılması (A5-K100); (3) eksik tabloların üretimi (A5-K99; Ek E `[eksik_tablolar]`); (4) koşulsuz varsayılanların yazılması (Ek E `[kosulsuz]`, şemadaki `default` değerleriyle aynı); (5) koşullu varsayılanlar (A5-K98; Ek E `[[kosullu]]`, koşul 4. adımdan sonraki değere bakar), ürün profilleri ve kalibre edilmeyen değerler (3.6, A5-K101; Ek E `[profil.*]`, `[kalibre_edilmedi]`); (6) `otopilot` bloğu: dosyadaki açık çiftler aynen, yoksa `parametreler = []` (A5-K52; türetim yok, W1-uyum P3-14); (7) çözümleyici üst verisi (`meta.tur = "cozumlenmis_senaryo"`, `meta.kalibre_edilmedi`, `meta.dogrulanmamis_varsayimlar`, `meta.olcum_kanitlari`; listeler sözlük sırasıyla). Sonuç şemadaki `$defs/cozumlenmis_senaryo` tanımına uyar. Altın örnek: `scenarios/ornek-havuz.cozumlenmis.json`; yalnız bu belgeden (Ek E ve kurallar) yazılmış bağımsız bir çözümleyici iki örnekte de aynı baytları üretir.
- **A5-K17** Kanonik biçim çözümlenmiş senaryonun RFC 8785 (JSON Canonicalization Scheme) dizilimidir: nesne anahtarları UTF-16 kod birimlerine göre sıralanır, belirteçler arasında boşluk yazılmaz, sayılar ECMAScript Number dizilimiyle yazılır (RFC 8785 §3.2; A5-K96 tablosu). **Davranış özeti** kanonik baytların SHA-256'sıdır (64 küçük onaltılık karakter); kaynak dosya özetleri bu nesneye girmez, A6'ya ve doğrulama raporuna ayrı üst veri olarak yazılır. Bu yüzden anahtar sırası, yorum, boşluk, tablo ya da satır içi tablo yazımı, sayıların yazılış biçimi ve açıkça yazılmış varsayılanlar davranış özetini değiştirmez; olayların dosya sırası anlamlıdır (A5-K62) ve özeti değiştirir. Çalıştırma A6'da davranış özetiyle tanımlanır. (RFC 8785 Informational yayındır; seçim bu sözleşmenindir.) A3 ve A6 bu baytları değiştirmeden `cozumlenmis_senaryo.json` eki olarak taşır; özetlerin metin gösterimi orada `sha256:` + bu 64 hane, kaynak özetleri ayrı alandır (W1-uyum U18). Altın örneğin davranış özeti: `d1735f8b7eafabede9350c591bb4db7aa3a563b8ac4f45500200d33d4427cd92`.
- **A5-K98** Koşullu varsayılanlar:

  | Anahtar | Koşul | Yazılan değer |
  |---|---|---|
  | `ortam.su.yogunluk` | yoksa | havuz 1000, açık su 1025 |
  | `ortam.su.tuzluluk` | yoksa | havuz 0, açık su 0,035 |
  | `ortam.dalga.gamma` | `spektrum = jonswap` ve yoksa | 3,3; diğer spektrumlarda yazılmaz |
  | `calistirma.sil_hiz_carpani`, `sil_azami_hiz` | `kip = sil` ve yoksa | 1,0; false. HIL'de yazılmaz (ve verilemez, A5-K76) |
  | `kalite.sonar_tam.azami_sekme`, `hucre_kenari`, `yuzey_yamasi_araligi` | `sonar = tam` ve yoksa | 2; 0,01; 0,05. `sade`'de `sonar_tam` tablosu yazılmaz; verilmişse çözümlenmiş biçimden çıkarılır |
  | `arac.sensorler.<ad>.hiz_hz` | hızı olan türde ve yoksa | profil hızı (3.6) |
  | `arac.sensorler.<ad>.saat_kaymasi_ppm` | `imu` dışında ve yoksa | 0 |
  | `arac.sensorler.<ad>.ayar.<k>` | yoksa | profil değeri (3.6); "yazılmaz" işaretliler yazılmaz |
  | `arac.sensorler.<ad>.ayar.nominal_montaj` | `tur = kamera` ve yoksa | aynı sensörün `montaj` değerinin kopyası (Ek E `[[kopya]]`) |
  | `ortam.su_optigi.*`, `ortam.isik.*` | yoksa | A5-K101 değerleri (Ek E `[kalibre_edilmedi_ortam]`) |
  | `olaylar[i].sure_ns` | geçici arıza ve yoksa | 0 |
  | `olaylar[i].zaman_deseni.tur`, `sira_boz`, `kanal_maskesi` | `sensor` sınıfı ve yoksa | `surekli`; false; 4294967295 |
- **A5-K99** Senaryoda olmayan tablolar çözümlenmiş biçimde şöyle üretilir: `hedefler` → `{}`; `olaylar` → `[]`; `emniyet_girdileri` → `{acil_durdurma: false, fis: {f1: false, f2: false}, pim: {f1: true, f2: true}}`; `baglanti` → `{etkin_profil: "ideal", profiller: {ideal: {kademe: "L0", tasima: "datagram", yukari: {gecikme_taban_ns: 0, titresim_ns: 0, kayip_modeli: "yok"}, asagi: (aynı)}}}`; `kalibrasyon` → `{malzeme: {}}`; `kalite` → bütün koşulsuz varsayılanlar; `baslangic` → bütün varsayılanlar; `otopilot` → `{parametreler: []}`; `gorev` → yazılmaz (görev yok). `varliklar` senaryoda zorunludur (A5-K115). İç tablolar için de aynı ilke geçerlidir: varsayılanı olan her anahtar yazılır.
- **A5-K100** Kaynak alanları çözümlenmiş biçimde bulunmaz: `arac.dosya` çıkarılır; `meta.tur` `cozumlenmis_senaryo` olur; kaynak dosya özetleri yazılmaz (A5-K17).
- **A5-K18** `kalibrasyon.malzeme` anahtarı A7 manifestinde bulunmayan bir malzeme kimliğiyse senaryo reddedilir; geçersiz kılınan değerler çözümlenmiş senaryoda durur. Bilinmeyen malzemeye varsayılan verilmez (05 §4). Geçersiz kılma alanlarının adları, enum değerleri ve aralıkları A7 §3.4 ve A7-K57'dekinin aynısıdır (akustik: `geri_sacilim_mu`, `gelis_yasasi` ∈ {`lambert_cos2`, `cos1`}, `ayna_yansima_rs`, `puruzluluk_gamma_p`; optik: A2 §3.4 `MalzemeOptik`'in `taban_rengi_dogrusal`, `metaliklik`, `puruzluluk`); başka ad ya da daha geniş aralık reddedilir.
- **A5-K19** Varlık atıfları A7 manifestinde (A5-K115'in kümesinde) şu türdeki bir varlığa eşlenir; eşlenmezse senaryo aşama 2'de reddedilir: `arac.varlik` → `arac`; `arac.torpido.varlik` → `torpido`; `hedefler.<ad>.varlik` → `hedef`; `ortam.sahne` → `statik`. Tüp eşlemesi: A5 `f<k>` ↔ A7 `TUBE_<k>`; sensör montajı `<ad>` ↔ A7 `MNT_<ad>`; itici `t<k>` ↔ A7 `THR_<k>` (karşılaştırma A7-K29'dadır).

### 4.3 Araç

- **A5-K20** Araç konumları ve yönleri gövde çerçevesinde ve `O_b`'ye göredir; dünya konumları NED'dedir.
- **A5-K21** `arac.kutle.atalet` simetrik değilse (`|I_ij − I_ji| > 1e-12` kg·m²), pozitif tanımlı değilse ya da asal momentleri üçgen eşitsizliğini sağlamıyorsa reddedilir.
- **A5-K22** Üç kütle durumunun her biri için `M_RB + M_A` (04 §2.3) simetrik pozitif tanımlı değilse araç reddedilir.
- **A5-K23** Kütle durumları dosyada yazılmaz; yükleyici şu sırayla türetir: (1) boş araç; (2) trim kütlesi (A5-K24); (3) her dolu tüp için torpido: kütlesi tüp konumunda nokta kütle, kendi merkezine göre düzgün yoğunluklu sferoid ataleti paralel eksen kuralıyla, hacmi (`4/3 π a b²`) yüzerlik merkezine katkısıyla; torpidonun kendi `r_g` ofseti bağlı durumda ihmal edilir. Aracın `M_A`'sı ve sönümü yük durumundan bağımsızdır (iskelet §5). Türetilen değerler Ek A'daki test vektörleriyle 1e-4 bağıl hata içinde eşleşir.
- **A5-K24** `trim_kutlesi > 0` iken `trim_konumu` zorunludur. Trim kütlesi verilen konumda hacimsiz nokta kütledir: toplam kütleye, kütle merkezine ve `O_b`'ye göre ataletine (`m (|r|² I − r rᵀ)`) katılır; deplasmanı ve yüzerlik merkezini değiştirmez; torpidolardan önce eklenir.
- **A5-K25** `kanal` değerleri 0–7 arasında ve tekildir; `B = [ε_i; r_i × ε_i]` matrisinin rankı 6 değilse araç reddedilir. Kart tahsis matrisini bu kaynaktan kurar (02 §2.5).
- **A5-K26** `sapma` çarpanlarının ilk üç elemanı `[1 − oteleme, 1 + oteleme]`, son üç elemanı `[1 − donme, 1 + donme]` aralığındadır. Çarpanlar yalnız fizik tarafına uygulanır, `otopilot`'a geçmez.
- **A5-K27** A5'te PWM kaybı davranışı seçimi yoktur: sürekli komutun kullanım süresi dolunca aktüatör modeli nötr duruma geçer (iskelet §4); kullanım süresi A1'de tanımlanır.
- **A5-K28** 1.0'da `arac.batarya.kip = "ideal"` dışındaki değer A5-K102 ile reddedilir; `ideal` kipte `v_sabit` zorunludur; `v_sabit` 12–20 V dışındaysa dışdeğerleme uyarısı üretilir (04 §2.7.4).
- **A5-K29** `arac.torpido` ve `arac.firlaticilar` değerleri 3.5'teki sabit değerlere eşittir; farklı değer reddedilir. Değişiklik sözleşme değişikliğidir. Bu tablolara itki, güdüm, arayıcı, yakıt, menzil ya da yörünge anahtarı eklenemez (A5-K06).
- **A5-K30** `gerilim_noktalari`, `azami_itki_ileri`, `azami_itki_geri` ve `tam_gaz_akimi` aynı uzunluktadır ve `gerilim_noktalari` kesin artandır; `havalanma_h_bolu_r` ile `havalanma_beta` aynı uzunluktadır ve `havalanma_h_bolu_r` kesin artandır. Polinom ve transfer fonksiyonu dizilerinin uzunluğu bu kurala girmez (A5-K104, K105).
- **A5-K104** `sekil_polinomu` için `F(u_j)`, `u_j = j/1000`, `j = 0 … 1000` noktalarında hesaplanır; normalleştirme böleni `N = max_j F(u_j)` sonlu ve pozitif değilse ya da herhangi bir `j ≥ 1` için `F(u_j) ≤ 0` ise reddedilir. Şekil `F_şekil(u) = max_{v ≤ u} F(v) / N` ile monoton kırpılır (04 §2.7.2).
- **A5-K105** `transfer_payda`'nın ilk ve son katsayısı sıfırdan farklıdır; `len(transfer_pay) < len(transfer_payda)` (kesin uygun); DC kazanç `pay[son]/payda[son]` 1'den en çok 1e-12 sapar (04: birim DC kazanç); payda Routh–Hurwitz ölçütüne göre kararlıdır (bütün kökler sol yarı düzlemde). Aksi halde reddedilir.
- **A5-K31** `pwm_min_us < pwm_notr_us − olu_bant_us` ve `pwm_notr_us + olu_bant_us < pwm_max_us` sağlanmazsa reddedilir.
- **A5-K32** Birim vektör ve kuaterniyon olarak tanımlı her değerin normu 1'den en çok 1e-6 sapar; daha büyük sapma reddedilir; yükleyici kabul ettiği değeri normalleştirerek kullanır, çözümlenmiş senaryoya dosyadaki değeri yazar.
- **A5-K89** Sensör çerçevesi kabulleri (montaj yorumunda): IMU, manyetometre, basınç, GNSS, batarya ölçer, sızıntı ve fırlatıcı geri okuması için sensör eksenleri gövde eksenlerine paraleldir (birim yönelimde); DVL ve Ping1D ışın ekseni sensör +z'dir; Ping360 dönme ekseni sensör +z, tarama düzlemi sensör x–y'dir ve 0 gradyan sensör +x'tir; kamera optik ekseni sensör +x, görüntü sağı sensör +y'dir (S çerçevesi; A2 §3.6 ve A8 ile aynı). Kameranın optik çerçeveye (z ileri) dönüşümü A2-K94'tedir; A5 yinelemez. Bu kabullerle İDA-0'ın bütün montaj yönelimleri birim dönüştür.
- **A5-K33** `kimlik` tekildir ve `(tur, inst)` çifti tekildir.
- **A5-K34** Yükleyici temas doğruluk ölçütünü (`ω_c h ≤ 0,2`; 04 §2.9) her kütle durumunda hesaplar; aşılırsa `uyari` üretir.

### 4.4 Ortam

- **A5-K35** Su yoğunluğu ve tuzluluğu A5-K98'e göre yazılır (04 §2.5; açık su değeri mimari varsayımıdır).
- **A5-K36** `ortam.tur = "havuz"` iken `ortam.sahne`'nin A7 statik varlığı `taban_derinligi_m` taşıyorsa (A7-K56) `ortam.derinlik` ondan 1 mm'den fazla farklı olamaz, farklıysa senaryo reddedilir; alan yoksa `uyari` üretilir. `acik_su` iken `ortam.derinlik` yalnız dalga dispersiyonunun sabit `h`'sidir.
- **A5-K37** `dalga.spektrum` `yok` değilse `hs`, `tepe_periyodu_ns` ve `yon` zorunludur.
- **A5-K38** `gauss_markov.etkin = true` iken `tau_ns`, `sigma` ve `sinir` zorunludur.
- **A5-K39** `ortam.ruzgar.hiz_ned`'in d bileşeni 0'dan farklıysa reddedilir.

### 4.5 Sensörler

- **A5-K40** `ayar` altında verilmeyen anahtar 3.6'daki profil değeriyle çözümlenir; `yakinsama_baslangic_hatasi > 0` iken `yakinsama_tau_ns` zorunludur.
- **A5-K41** `imu`, `dvl`, `kamera`, `ping1d` ve `ping360` için `hiz_hz` verilirse reddedilir: IMU kontrol periyodunda örneklenir (iskelet §4, 200 Hz); DVL hızı mod tablosundan, kamera hızı A2 §6 kademesinden, Ping1D ve Ping360 zamanlaması cihaz modelinden ve kartın yapılandırmasından gelir. `imu` için `saat_kaymasi_ppm` verilirse reddedilir.
- **A5-K42** `basinc` sensöründe `hiz_hz > 1 / (2 · donusum_penceresi + okuma_suresi)` ise reddedilir (profil değerleriyle 25,974026 Hz).
- **A5-K43** `hiz_hz` (0, 800] Hz aralığındadır; zamanlayıcı onu en yakın adım sınırı kuralıyla bağlar (04 §2.11).
- **A5-K44** Etkin bir `manyetometre` varken `ortam.manyetik_alan` yoksa reddedilir.
- **A5-K45** Etkin bir `gnss`, etkin bir `manyetometre` ya da `dunya_donusu_etkin = true` olan etkin bir `imu` varken `ortam.orijin` yoksa senaryo reddedilir (IMU'nun dünya dönüşü modeli enlemi kullanır, 05 §2.3.1).
- **A5-K46** `imu` ayarında `suzgec_etkin = true` iken `suzgec_tau_ns` zorunludur.
- **A5-K47** `dvl` `baglanti = "seri"` iken `seri_baud` kullanılır; `"tcp"` iken 05 §2.3.5'in TCP gecikme dağılımı kullanılır.
- **A5-K48** A5'te sonar menzili ve örnek sayısı anahtarı yoktur: menzil kartın yapılandırmasıdır (A1, 05 §2.4.5). Zaman sabitleri, periyotlar ve Gilbert–Elliott süreleri sıfır olamaz (şemada `exclusiveMinimum: 0`).
- **A5-K49** `etkin = false` sensör kayıt üretmez ve karta veri göndermez.
- **A5-K101** Profilde ölçülmüş ya da yayımlanmış değeri olmayan parametreler (3.6'da KE) ideal değerle çözümlenir: süreler 0, hata büyüklükleri 0, DVL `c_ayar` ortamın ses hızı, Ping360 `baud` yokken iletim süresi 0. Çözümleyici, kullanıcının vermediği ve etkin bir sensöre ait her KE anahtarının yolunu `meta.kalibre_edilmedi` listesine yazar; "yazılmaz" işaretli KE anahtarları (`c_ayar`, `baud`) çözümlenmiş biçimde bulunmaz ve anlamları bu kuraldaki ideal değerdir. Ortamın su optiği ve ışığı da aynı kurala tabidir (Ek E `[kalibre_edilmedi_ortam]`): verilmeyen su optiği katsayıları 0 (berrak su), güneş yönü tepe (NED `[0, 0, −1]`), güneş ışınımı 1000 W/m² (yer tutucu; kaynağı doğrulanmadı), ortam ışığı 0 yazılır ve yolları `meta.kalibre_edilmedi`'ye girer. Işık değerleri ölçüm değildir; mimar bunları "kalibre edilmedi" yer tutucusu olarak kabul etti (W1-uyum); görüntü gerçekçiliği için senaryo yazarı vermelidir.
- **A5-K108** `ping1d` ya da `ping360` sensörü etkin ve `ayar.sogurma` verilmemişken soğurma 05 §2.2.1'in doğrulanmamış formülünden (tatlı suda yalnız saf su terimi) hesaplanır; çözümleyici her böyle yolu `meta.dogrulanmamis_varsayimlar` listesine yazar ve doğrulama raporunda her biri için `uyari` üretir (AB-089).

### 4.6 Otopilot

- **A5-K50** `arac` ile `otopilot` ayrı tablolardır; fizik değeri `otopilot`'a örtük olarak kopyalanmaz.
- **A5-K51** Kaldırıldı (W1-uyum P3-14): "nominal modeli fizikten türet" seçeneği 1.0'da yoktur; `otopilot.nominal` anahtarı A5-K06 ile reddedilir.
- **A5-K52** `otopilot.parametreler` yalnız açık `{kimlik, tip, deger}` çiftleri taşır: `kimlik` A4 parametre kimliği (`ida_param_value_t.id`, u16) ve dizide tekil; `tip` A4 `IDA_PT_*` (`i32`, `u32`, `f32`); `deger` `i32`/`u32`'de tipin aralığında tamsayı değerli, `f32`'de binary32'ye en yakına çevrildiğinde sonlu. Aksi senaryo aşama 2'de reddedilir. Parametre kataloğu W3'ündür (A4 O9); katalog dondurulunca kimliğin katalogda bulunması, tipinin ve aralığının katalogla uyuşması aşama 2'de denetlenir. Çözümleyici fizikten hiçbir değer türetmez.
- **A5-K53** SIL'de çözümlenmiş `otopilot.parametreler` platform parametre bloğudur (02 §3.6); verilmeyen parametre A4'ün derlenmiş varsayılanıyla kalır. HIL kiplerinde A5 değerleri yalnız taşır; karttaki blokla eşitleme ya da farkın ön denetimde bildirilmesi A1/A4'ün kuralıdır.

### 4.7 Görev ve zaman çizelgesi

- **A5-K55** Görev öğesi A1 Tablo 3.6a'nın alanlarını bire bir taşır: `item_type` (A1 tablosunun `Ad` sütunu: `WAYPOINT`, `DIVE_TO`, `SURFACE`, `HOLD`, `SCAN_LINES`, `SCAN_POINT`, `WAIT_GNSS`), `frame` (`NONE`, `LOCAL`, `GLOBAL`), `p` (dört sayı), `lat_e7`, `lon_e7`, `north_m`, `east_m`, `depth`; hepsi zorunludur, ayrı gösterim ve dönüşüm yoktur. Her öğe A1-K122'nin bütün koşullarını sağlar (türün izin verdiği `frame`, kullanılmayan konum alanlarının 0 olması, `depth` ve her `p[i]`'nin türün aralığında olması, "0" yazan `p[i]`'nin tam 0 olması); sağlamazsa senaryo aşama 2'de reddedilir. Anlam, birim ve aralıklar A1 Tablo 3.6a'dadır ve burada yinelenmez. `TARGET_ALIGN` yoktur (W1-uyum U1).
- **A5-K56** Görev en çok 128 öğe taşır (02 §3.8).
- **A5-K57** Görev `yukleme_zamani_ns` anında GCS ağ geçidi → bağlantı emülatörü → A1 yolundan kartın görev komutlarıyla gönderilir; SIL ve HIL'de yol aynıdır.
- **A5-K109** Olay çeşitleri: **kalıcı atama** (`emniyet_girdisi`, `baglanti/profil_degisimi`, `sistem/su_girisi`), **anlık komut** (`gcs_komutu`, `sistem/kart_sifirlama`), **geçici arıza** (diğer bütün desteklenen türler). Kalıcı atama ve anlık komut `sure_ns` taşıyamaz; kalıcı atama değeri bir sonraki atamaya kadar kalır ("süreli" emniyet değişimi iki atamayla yazılır). Kalıcı atama `zaman_ns` ya da `kosul` ile verilebilir. Emniyet atamaları iskelet §3.2 "Fizik sınırındaki sıra"ya bağlıdır (A5-K63). Doğrulamada saptanabilen çelişki (zamanlı atamalar) doğrulama hatasıdır (A5-K62); çalışma anında ortaya çıkan çelişki (en az biri koşullu, ya da panel ve oturum işlemleriyle aynı sınır) A5-K63'teki sırayla çözülür.
- **A5-K60** `olaylar[].kimlik` tekildir; `^E[0-9]+$` elle enjeksiyona ayrılmıştır ve reddedilir.
- **A5-K61** Her olay tam olarak birini taşır: `zaman_ns` ya da `kosul`.
- **A5-K62** `zaman_ns` taşıyan olay sim zamanı `t ≥ zaman_ns` olan ilk fizik adım sınırında başlar. Aynı adım sınırında önce bitişler, sonra başlangıçlar uygulanır; her grubun içinde dosyadaki sıra geçerlidir. Yükleme doğrulaması: aynı fizik sınırına düşen iki **zamanlı** emniyet atamasından biri kısıtlayan (acil durdurma basılı, fiş çıkık, pim takılı) öteki gevşetense, alanları farklı olsa da senaryo reddedilir (W1-uyum U3); aynı bağlantı profili alanına aynı sınırda farklı değer atayan iki zamanlı atama da reddedilir.
- **A5-K63** `kosul` taşıyan olayın koşulu her fizik adımı sonunda değerlendirilir; ilk sağlandığı adımı izleyen adım sınırında olay bir kez başlar ve yeniden kurulmaz; uygulama adımı A6'ya yazılır, tekrar oynatmada koşul yeniden değerlendirilmez (08 §2.2). Zamanlı ve koşullu emniyet atamaları sim'de iskelet §3.2 "Fizik sınırındaki sıra"ya göre işlenir (A1-K85, A3-K95 aynı sırayı yazar; A5 yinelemez): kısıtlayan atamalar (acil durdurma basılı, fiş çıkık, pim takılı) 2. adımda hepsi uygulanır; gevşeten atamalar 4. adımda, panel isteklerinden önce uygulanır ancak emniyet revizyonu o sınırda 1–3. adımlarda arttıysa (senaryonun kendi kısıtlayan ataması dahil) reddedilir, yeniden denenmez ve `gevsetme_reddedildi` olayı yazılır (A5-K117). Revizyon artışları iskelet §3.2'nin listesindedir. Aynı emniyet alanına aynı sınırda iki değer düşerse (ör. koşullu "bas" ile zamanlı "bırak") bu sıra kısıtlayanı uygular ve ayrıca `atama_celiskisi` olayı yazılır. Bağlantı profili alanında (emniyet dışı) aynı sınırda çelişen atamalardan dosya sırasında sonraki uygulanır ve `atama_celiskisi` yazılır.
- **A5-K64** Geçici arıza, başladığı sınır `t_b` ise `t_b + sure_ns`'e eşit ya da ondan sonraki ilk adım sınırında biter ve etkisi kalkar (durumlu arızaların bitiş davranışı Tablo 3.10-A); `sure_ns = 0` süresizdir ve yalnız elle kaldırılır. Aynı hedefe çakışan geçici arızaların hepsi uygulanır (3.10 bileşim kuralı).
- **A5-K65** `hedef` sınıfa uyar: `sensor` → `arac.sensorler` anahtarı; `aktuator` → `t1`…`t8` ya da `tumu`; `emniyet_girdisi` ile `fis`/`pim` → `f1` ya da `f2`, `acil_durdurma` → `hedef` yok; `guc`, `baglanti`, `protokol`, `sistem`, `gcs_komutu` → `hedef` yok. `baglanti` ve `protokol`'de `yon`, `protokol`'de `duzlem` zorunludur; `profil_degisimi`'nde `profil` mevcut bir profildir; `su_girisi`'nde `problar` prob sayısından küçüktür; `motor_takilmasi` yalnız `ping360`, `dvl_isin_kaybi` yalnız `dvl` hedefinde, `gurultu_artisi` yalnız Tablo 3.10-A'daki türlerde geçerlidir.
- **A5-K66** `gec_teslim`'de `titresim_ns ≤ gecikme_ns` sağlanmazsa olay reddedilir. Böylece arızalı teslim modellenmiş teslim anından önce olmaz ve `t_meas ≤ t_hazır ≤ t_teslim` korunur (05 §2.2).
- **A5-K67** `zaman_deseni`, `sira_boz` ve `kanal_maskesi` yalnız `sensor` sınıfında bulunur; `bernoulli` iken `p`, `gilbert_elliott` iken `t_iyi_ns` ve `t_kotu_ns` zorunludur.
- **A5-K68** Her desteklenen tür Tablo 3.10-A'daki zorunlu `siddet` alanlarını taşır ve başka `siddet` alanı taşıyamaz (`gec_teslim` için `titresim_ns` isteğe bağlıdır). `itki_carpani`'nda `carpan` [0, 1], `ic_direnc_artisi`'nda `carpan` ≥ 1'dir.
- **A5-K69** `kanal_maskesi` bu sürümde `dvl_isin_kaybi` dışında yalnız 4294967295 (bütün kanallar) olabilir; `dvl_isin_kaybi`'nda 1 … 15 (bit `i` = ışın `i`) olur.
- **A5-K70** Durumlu arızalar (`motor_takilmasi`, `dvl_isin_kaybi`, `guc_cevrimi`) bittikten sonra temel ölçüm dizisiyle bit eşitliği beklenmez; diğer sensör arızalarında arıza penceresi dışındaki çıktı arızasız koşuyla bit düzeyinde aynıdır (05 §2.7).
- **A5-K71** Emniyeti azaltan her zaman çizelgesi ataması (acil durdurmayı kaldırma, fişi takma, pimi çıkarma) ve emniyeti azaltan her başlangıç durumu doğrulama raporunda ayrı bir `uyari` olarak listelenir (08 §3.4).
- **A5-K72** `komut = "SET_MODE"` iken `kip` zorunludur; diğer komutlarda yasaktır.
- **A5-K73** Zaman çizelgesinde atış kontrol komutu (silahlandırma ya da ateşleme) yoktur; `ateşle` olayı yalnız karttan gelir (08 §3.2).

### 4.8 Bağlantı emülatörü ve çalışma kipi

- **A5-K74** `baglanti` yoksa A5-K99'daki L0 nesnesi yazılır. `etkin_profil` `profiller` içinde bulunur. `kayip_modeli = "bernoulli"` → `kayip_p`; `gilbert_elliott` → `ge_lambda_gb`, `ge_lambda_bg`, `ge_k`, `ge_h` zorunludur; `tasima = "stream"` → `stream` tablosu zorunlu, `bayt_bozma_orani` ve `hava_paketi` yasaktır; `kademe = "L0"` iken yön tablolarında yalnız varsayılan değerler bulunabilir; `L1` iken `gilbert_elliott`, `L2` iken `bernoulli` yasaktır.
- **A5-K75** `protokol` olayları 06'nın hata enjektörünü, `baglanti` olayları emülatörü sürer; ikisi aynı olayda birleştirilmez (06 §2.12).
- **A5-K76** `calistirma.kip` `hil_1x` ya da `hil_bariyer` iken `sil_hiz_carpani` ya da `sil_azami_hiz` verilirse reddedilir.
- **A5-K77** Kontrol hızı (200 Hz), fizik adımı (1,25 ms) ve `L_act` A5'ten ayarlanmaz (iskelet §4). HIL kiplerinde tek bir kaçırılan parti çalıştırmayı zamanlama bakımından geçersiz kılar; bu eşiği değiştiren anahtar yoktur.

### 4.9 Kalite kademeleri

- **A5-K78** `kalite.gorunum`, `kalite.kamera` ve `kalite.sonar`'ın varsayılanı `sade`'dir; kayıt ayarları A3/A6'nın değer kümelerini kullanır: `kalite.kayit` ∈ {`olaylar`, `telemetri`, `tam`} (varsayılan `telemetri`; "kapalı" yoktur, emniyet kaydı kaldırılamaz), `kalite.kamera_kareleri` ∈ {`kapali`, `pencereli`, `surekli`} (varsayılan `kapali`; anlamları A6-K19). Sonar `sade` kademesi ping başına 5 000 ışın kullanır (iskelet §6; MP7 S2) ve ışın sayısı A5'ten değiştirilmez. Ping360 örnek sayısının üst sınırı iki kademede de 1 200'dür (MP7'de ölçülen yapılandırma; mimar kararı); sade kademeyi sınırlayan ışın sayısıdır. Örnek üst sınırı A5 anahtarı değildir.
- **A5-K79** `kalite.sonar = "tam"` iken `kalite.sonar_tam.isin_sayisi` zorunludur. Kartlı kiplerde (`hil_1x`, `hil_bariyer`) ışın sayısı yalnız ölçülmüş kümeden seçilir: 20 000, 40 000, 80 000, 160 000 (MP7 §4.6; ölçümde gerçekleşen ışın sayıları 20 000, 40 014, 80 000, 160 008); başka değer reddedilir ve geçerli seçim `uyari` üretir. SIL'de 1 … 10⁷ serbesttir; kümede olmayan değer "ölçülmemiş ışın sayısı" `uyari`'sı üretir. Çözümleyici `meta.olcum_kanitlari`'na sonar kademesinin kanıtını yazar: sade → `docs/olcumler/MP7-sonar-suresi.md §4.3–4.4 (sade, N = 5 000)`; tam ve kümedeki değer → `docs/olcumler/MP7-sonar-suresi.md §4.6`; tam ve kümede olmayan değer → kanıt yazılmaz. Bu sınırlar ölçüm sınırıdır, donanım sınırı değildir; yeni ölçüm kümeyi minor sürümle genişletir.
- **A5-K80** `kalite.gorunum = "kapali"` iken `kalite.kamera` `kapali` değilse reddedilir: kamera karelerini görselleştirici süreci üretir (iskelet §8, A2), görünüm kapalıyken o süreç çalışmaz. `kalite.kamera = "kapali"` iken etkin bir `kamera` sensörü varsa reddedilir. `kalite.kamera` `kapali` değilken etkin kamera sensörü yoksa `uyari` üretilir. `kalite.kamera_gerekli = true` iken kamera kademesi `kapali` ya da etkin kamera yoksa reddedilir (anlamı A2-K50). Kamera kademesinin sayısal değerleri (çözünürlük, hız) A2 §6'dadır.
- **A5-K81** Kalite kademesi emniyet girdilerini, emniyet olaylarını ve bu belgedeki emniyet kurallarını değiştirmez (iskelet §6).

### 4.10 Rastgelelik ve tekrar üretilebilirlik (Ek D: rastgelelik sözleşmesi sürüm 1)

- **A5-K82** `rastgelelik.ana_tohum` 0 … 2⁵³−1 aralığında bir tamsayıdır.
- **A5-K83** Üreteç Philox4x32-10'dur (Random123). Her alt akışın anahtarı `K = SHA-256("IDA-A5-RNG-1" ‖ 0x00 ‖ LE64(ana_tohum) ‖ UTF8(alt_sistem_adı) ‖ 0x00)`'dır; Philox anahtar sözcükleri `k0 = LE32(K[0..3])`, `k1 = LE32(K[4..7])`'dir; kalan baytlar kullanılmaz.
- **A5-K84** Alt sistem adları: `fizik/dalga`, `fizik/akinti`, `fizik/ruzgar`, `hedef/<ad>`, `sensor/<ad>/<akış>` (akış: `acilis`, `olcum`, `zamanlama`, `ariza`, `sonar_isin`, `sonar_hucre`), `ariza/<olay kimliği>` (elle enjeksiyonda `ariza/E<n>`), `ariza/<olay kimliği>/gecis` (Gilbert–Elliott geçiş dizisi), `baglanti/<profil>/<yukari|asagi>/gecis` (geçiş dizisi), `baglanti/<profil>/<yukari|asagi>/cerceve` (çerçeve dizisi), `protokol/<olay kimliği>`; A1-K114'ün SIL etiketleri A1'deki adlarıyla `ida/a1/nonce_sim/<deneme>`, `ida/a1/nonce_board/<boot_count>/<deneme>`, `ida/a1/sess/<oturum_sırası>`; `a1/run_id`; `a1/board_tohum`; `a2/kamera_gurultu`. Değer türetimi A5-K118 ve Ek D.7'dedir. Bir alt sistemin çekiliş sayısının değişmesi başka alt sistemin çekilişlerini değiştirmez.
- **A5-K110** Sayaç düzeni: `c0 = n mod 2³²`, `c1 = ⌊n / 2³²⌋` (`n` alt akış içindeki 64 bitlik sıra), `c2 = (kanal << 24) | (bilesen << 16) | cekilis` (8 + 8 + 16 bit), `c3 = 0` (sürüm 1'de ayrılmış). Bu sözleşmenin tanımladığı çekilişlerin `n` ve `c2` atamaları Ek D.4'tedir; model içi çekilişlerin atamaları 04 ve 05'in uygulama belgelerindedir ve aynı düzeni kullanır.
- **A5-K111** Dönüşümler: düzgün `u(x) = (x + 0,5) · 2⁻³²` (binary64'te tam, (0, 1) açık aralık); Bernoulli `u < p`; [0, m) tamsayı `⌊u · m⌋`; normal (Box–Muller) `r = sqrt(−2,0 · log(u_a))`, `t = 6,283185307179586 · u_b`, `z₀ = r · cos(t)`, `z₁ = r · sin(t)`; üstel `−log(u)`; karmaşık normal CN(0, 1) `sqrt(−log(u_a)) · (cos(t), sin(t))`. İşlem sırası budur, FMA kullanılmaz; `u_a`, `u_b` aynı çıktının ardışık sözcükleridir (0–1 ya da 2–3).
- **A5-K112** Philox sözcükleri ve düzgün dağılım her platformda bit düzeyinde aynıdır. `log`, `cos`, `sin` doğru yuvarlanmış olmadığından normal, üstel ve karmaşık normal dönüşümler için platformlar arası bit eşitliği garanti edilmez (sınır); aynı ikili ve platformda tekrar garanti edilir.
- **A5-K85** Aynı davranış özeti, aynı ikililer ve aynı platformla iki **canlı** SIL koşusu bit düzeyinde aynı fizik ve sensör izini yalnız kamera kapalıyken (`kalite.kamera = "kapali"`) üretir: kamera karesinin üretimi ve algılama sonucunun dönüşü bilgisayarın zamanlamasına bağlıdır (iskelet §4 "Canlı koşular arası eşitlik"). Kamera açık koşularda güvence, kayıtlı çekirdek çağrı girdilerinin tekrarıdır (A6-K34, K56) (Ç7); HIL kiplerinde güvence yalnız sim tarafı içindir ve kayıtlı uygulama adımlarıyla verilir (iskelet A6, Ç1). Platformlar arası eşdeğerlik toleranslıdır (04 §2.13).

### 4.11 Başlangıç

- **A5-K86** `baslangic.yuklu_tupler` kütle durumunu seçer: iki tüp → iki yüklü; yalnız `f1` → bir yüklü (y = +0,10); yalnız `f2` → bir yüklü, aynalı; boş dizi → boş. Yinelenen tüp reddedilir.
- **A5-K87** `emniyet_girdileri` verilmezse başlangıç durumu acil durdurma basılı değil, iki fiş çıkık, iki pim takılıdır.
- **A5-K88** Sim zamanı 0'dan başlar; `baslangic.konum`, `yonelim` ve `hiz` `t = 0` anındaki `O_b` durumudur; araç `t = 0`'da `esc.baslatma_suresi_ns` boyunca nötr bekler.

### 4.12 Doğrulama

- **A5-K90** Doğrulama üç aşamadır: (1) sözdizimi, sayısal dönüşüm, tip ve şema, her dosya için ayrı; (2) birleştirme, çözümleme, anlamsal kurallar (A7 manifesti ve A4 parametre tablosu gerekir) ve çözümlenmiş biçimin şemaya uyumu; (3) ön denetim (06; HIL'de kart uyumu; A1 sözleşmesi). Bir aşamada `hata` varsa sonraki aşama koşmaz.
- **A5-K91** Bir aşamanın bütün bulguları toplanır; doğrulayıcı ilk hatada durmaz.
- **A5-K92** Doğrulama sonucu 3.14'teki JSON nesnesidir; bulgular `(dosya, satir, yol, kural)` sırasıyla dizilir; `gecerli = true` ise `davranis_ozeti` doludur.
- **A5-K93** `yol` TOML anahtar yolu sözdizimindedir: noktayla ayrılmış anahtarlar, dizi elemanı `[i]` (0 tabanlı), çıplak anahtar olamayan ad TOML temel dizgisi olarak tırnaklı.
- **A5-K94** `satir` bulgunun ilgili olduğu anahtarın 1 tabanlı satırıdır; değer varsayılandan ya da türetimden geliyorsa ya da ayrıştırıcı konum vermiyorsa `null`'dur. A5-K01 sözdizimi hatasında `satir` zorunludur.
- **A5-K95** `uyari` bulguları yüklemeyi durdurmaz ve A6'ya yazılır; `hata` bulgusu olan senaryo yüklenmez (08 §2.3).

### 4.13 Çapraz uyum kuralları (W1-uyum)

- **A5-K113** `hedefler.<ad>.sinif` kapalı bir enum'dur ve yalnız algılama kaydının sınıf kodlamasıdır (A8-K23 `class_id`); hedef seçimi, öncelik ya da atış ölçütü anlamı taşımaz. Sayısal tablo: 0 `bilinmiyor`, 1 `sabit_nesne`, 2 `hareketli_nesne`, 3 `yapi`; 4–255 ayrılmıştır. Varsayılan `bilinmiyor`. Tabloya yeni değer sözleşme revizyonu ister.
- **A5-K114** Kamera: en çok bir `kamera` sensörü etkin olabilir ve onun `inst` değeri 0'dır (A2 `kamera_kimligi` = 0; A8-K35); aksi reddedilir. Etkin kamera varken ve `kalite.kamera` `kapali` değilken seçilen kademenin iç parametreleri (`ayar.ic_tam` ya da `ayar.ic_sade`: `fx`, `fy`, `cx`, `cy`), `pozlama_ns`, `yakin_m` ve `uzak_m` zorunludur. Bütün kamera alanları A2 §3.6'nın geçerli aralıklarıyla doğrulanır (tek kaynak; A2-K93–K95): iç parametreler, bozulma modeli ve katsayıları (model yokken katsayılar 0), pozlama, kalibrasyon sürümü, kırpma uzaklıkları ve iki montaj. **Fizik gerçeği montajı** sensörün `montaj`'ıdır (görselleştiriciye gider); **nominal montaj** `ayar.nominal_montaj`'dır (A8'e ve karta gider; A8-K53), yoksa fizik montajının kopyasıdır (A5-K98); ikisi de S çerçevesindedir (A5-K89). Kameranın süreç ayarları (port, halka yuva sayısı, `D_op`) senaryo alanı değildir (A2-K93); yapılandırma sürümü sim'indir (A2 §3.6).
- **A5-K115** `varliklar.kume` A7 kümesini seçer (`assets/<kume>/manifest.json`; A7-K01). `varliklar.kume_surumu` verilmişse manifestin `kume_surumu`'na, `varliklar.manifest_ozeti` verilmişse manifestin ham baytlarının SHA-256'sına (A7-K05) eşit olmalıdır; değilse senaryo aşama 2'de reddedilir. Biri verilmemişse `uyari` üretilir ve sim hesapladığı değeri A6'ya yazar (A7-K55). Bütün varlık atıfları bu kümededir (A5-K19).
- **A5-K116** Yaşayan gövde bütçesi: 1 (araç) + hedef sayısı + tüp sayısı (İDA-0'da 2) + diğer hareketli gövdeler ≤ 64 (A2 §3.4 `govde_sayisi`); yüklemede doğrulanır. Bu sürümde diğer hareketli gövde yoktur ve tüp sayısı sabittir, dolayısıyla en çok 61 hedef; şema bunu `hedefler` için `maxProperties = 61` ile taşır ve aşan dosya aşama 1'de A5-K10 ile reddedilir. Dalga bileşen sayısı ≤ 256'dır (A2 §3.4; şemada `maximum`).
- **A5-K117** A5'in ürettiği olay ve kayıtlar §3.15'tedir (`atama_celiskisi`, `gevsetme_reddedildi`, `donma_onceki_cikti_yok`, olay sonucu, doğrulama raporu). Taşıyıcıları ve kanal adları A3/A6'nındır (W1-uyum U18); A5 içeriklerini tanımlar.
- **A5-K118** Kimlik ve tohum değerleri (Ek D.7): etiketin anahtarı A5-K83 ile, `tohum` = `ana_tohum` (A1'in `ida/a1/nonce_board/…` etiketlerinde `a1/board_tohum`'dan türetilen 64 bitlik değer). `n = 0, 1, 2, …` sayacıyla (`c2 = c3 = 0`) çekilir: u32 değer `w0`; u16 değer `w0 & 0xFFFF`; u64 değer `w0 | (w1 << 32)`. Değer 0 ya da çağıranın yasak listesindeyse (ör. `sess` için `prev_sess` ve son 16 oturum) bir sonraki `n` denenir; ilk uygun değer alınır. `a2/kamera_gurultu`'da yeniden çekiliş yoktur: kare başına `n = kare_sirasi` (A2 §3.5), `c2 = kamera_kimligi << 24`, değer u64 (`gurultu_tohumu`; A2-K95).

---

## 5. Hata durumları

| Durum | Algılama | Doğrulayıcının davranışı | Tüketicinin davranışı |
|---|---|---|---|
| TOML sözdizimi hatası, UTF-8 değil | aşama 1 | `K01` hata, satırla | Yükleme yok |
| Sayısal taşma, alt taşma, `inf`/`nan`; `x_` alanında tarih | aşama 1 | `K96`, `K97` hata | Yükleme yok |
| Desteklenmeyen sürüm | aşama 1 | `K03`/`K04` hata | Yükleme yok |
| Eksik, bilinmeyen anahtar; tip ya da aralık hatası | aşama 1 | `K05`, `K06`, `K09`, `K10` hata | Yükleme yok |
| Araç dosyası yok, tür yanlış, yol dizin dışında | aşama 1 | `K12` hata | Yükleme yok |
| Geçersiz kılma yeni girdi ekliyor | aşama 2 | `K15` hata | Yükleme yok |
| Desteklenmeyen anahtar, kip ya da olay türü | aşama 2 | `K102` hata | Yükleme yok |
| Anlamsal tutarsızlık (fizik, sensör, görev, olay, kalite) | aşama 2 | İlgili kural hata | Yükleme yok |
| A7 kümesi, kimliği, türü, malzemesi ya da havuz derinliği uyuşmuyor | aşama 2 | `K115`, `K18`, `K19`, `K36` hata (derinlik alanı yoksa uyarı) | Yükleme yok |
| Görev öğesi A1 Tablo 3.6a'ya uymuyor | aşama 2 | `K55` hata | Yükleme yok |
| Birden çok etkin kamera, kamera `inst ≠ 0`, eksik ya da A2 §3.6 dışı kamera alanı | aşama 2 | `K114` hata | Yükleme yok |
| Gövde bütçesi aşıldı (62 ve üstü hedef), 256'dan çok dalga bileşeni | aşama 1 | `K10` hata (A5-K116) | Yükleme yok |
| Aynı sınırda kısıtlayan ve gevşeten zamanlı emniyet ataması | aşama 2 | `K62` hata | Yükleme yok |
| Otopilot parametre çifti geçersiz ya da (katalog varken) katalogla uyuşmuyor | aşama 2 | `K52` hata | Yükleme yok |
| Senaryonun gevşeten emniyet ataması, revizyon o sınırda arttığı için uygulanamadı | çalıştırma | — | Reddedilir, yeniden denenmez; `gevsetme_reddedildi` olayı (A5-K63, K117) |
| Ön denetim tutmadı | aşama 3 | A1'in bulgusu, aynı biçimle | Çalıştırma başlamaz (06 §4) |
| Uyarı (dışdeğerleme, temas ölçütü, emniyeti azaltan atama, doğrulanmamış soğurma, tam sonar kartlı kipte, ölçülmemiş ışın sayısı, `x_` alanı, model farkı) | aşama 1–2 | `uyari` | Yüklenir; uyarılar A6'ya |
| Yeniden yüklenen çözümlenmiş senaryonun özeti uyuşmuyor | aşama 1 | `K02` hata | Yükleme yok |
| Koşullu bir atama aynı sınırda başka bir atamayla çelişti | çalıştırma | — | Emniyet alanında iskelet §3.2 sırası kısıtlayanı uygular, bağlantı profilinde sonraki geçerli; `atama_celiskisi` olayı A6'ya (A5-K63) |
| `donma` arızasında dondurulacak önceki çıktı yok | çalıştırma | — | Arıza boyunca kayıt yok; `donma_onceki_cikti_yok` sayacı ve olay A6'ya (Tablo 3.10-A) |
| Koşullu olayın koşulu hiç sağlanmadı | çalıştırma sonu | — | Uygulanmamış olarak A6'ya; çalıştırma geçersiz sayılmaz |

---

## 6. Kalite kademelerinin etkisi

Kademe seçimi 3.12'deki anahtarlarla yapılır; etkisi iskelet §6 ve 05 §5'tedir. A5'e özgü etkiler: `sonar = sade` iken `kalite.sonar_tam` çözümlenmiş biçimde bulunmaz (A5-K98); kartlı kipte tam sonar yalnız ölçülmüş ışın sayılarıyla seçilir (A5-K79); `kayit` düzeyi tekrar oynatma güvencesini belirler (`olaylar` → güvence yok, iskelet A6; A6 §4.6); `kamera = kapali` kamera sensörünün kapatılmasını, `gorunum = kapali` kameranın kapatılmasını gerektirir (A5-K80); kamera açıkken canlı SIL koşuları arasında eşitlik güvencesi yoktur (A5-K85); emniyet kuralları kademeden bağımsızdır (A5-K81). Kamera ve görünüm kademe değerleri A2 §6'dadır.

---

## 7. Uygunluk

| Kurallar | Sınama türü | Yöntem ve gözlenecek büyüklük |
|---|---|---|
| K01–K11, K96, K97, K102 | birim (doğrulayıcı) | Tablo güdümlü bozuk dosya takımı; her kural için en az bir dosya; beklenen `kural`, `asama`, `yol`. K96 için 4.1'deki sınır vektörlerinin her satırı |
| K12–K16, K98–K100 | birim + altın dosya | `ornek-havuz.toml`'un çözümlenmiş kanonik baytları `scenarios/ornek-havuz.cozumlenmis.json` ile bayt bayt eşit; her koşullu varsayılan satırı için koşulun iki tarafı |
| K17 | birim | Anahtar sırası, yorum, satır içi tablo, sayı yazımı (`3` / `3.0`, `0.30` / `3e-1`, `-0.0`) ve açıkça yazılmış varsayılanlar değiştirilmiş eşdeğer TOML aynı davranış özetini verir, kaynak özeti farklıdır; olay sırası değiştirilmiş dosya farklı özet verir; bir yaprağın değişmesi özeti değiştirir |
| K18, K19, K36, K115 | sözleşme (A7 ile) | A7 uygunluk örneği (`assets/ornek/manifest.json`) ile: var olan / olmayan / yanlış türde kimlik; A7 dışı malzeme alanı adı ve `gelis_yasasi = lambert_cos` reddi; taban derinliği farkı 0,5 mm ve 2 mm; yanlış `kume_surumu` ve `manifest_ozeti` reddi, verilmemişse uyarı |
| K113–K118 | birim + sözleşme | Sınıf kodu tablosu (A8 ile); tek kamera ve `inst = 0`; A2 §3.6 sınırlarının iki yanı (pozlama tek ya da > 1 s, `cx = g − 0,5` kabul / `g − 0,4` ret, kalibrasyon sürümü 0 ve 65 536); nominal montajın yokken fizik montajına eşit yazılması; 61 hedef kabul, 62 ret; 257 dalga bileşeni ret; Ek D.7 vektörleri; A5 olaylarının §3.15 alanlarıyla A6'ya ulaşması |
| K55 | birim (A1 ile) | A1-K122 sınama tablosundaki her tür ve koşul için aynı sonuç (A5 doğrulayıcısı reddeder, A1 `LINK_BAD_FIELD` verir) |
| K62–K63 | SIL (A1, A3 ile) | Aynı sınırda: senaryo "pim çıkar" + panel "pim tak" → pim takılı, `gevsetme_reddedildi`; senaryo koşullu "acil durdurmayı bırak" + oturum sonu → ret; zamanlı kısıtlayan + zamanlı gevşeten → yükleme reddi |
| K20–K34, K89, K104, K105 | birim (fizik yükleyici) | Atalet, `M` pozitif tanımlılığı, Ek A test vektörleri (trimli dahil; 1e-4 bağıl), `B` rankı, sıfır polinom, sıfır önde gelen payda, kararsız payda, DC kazancı 1 olmayan pay, norm toleransı 1e-6'nın iki yanı |
| K35–K49, K101, K108 | birim + SIL | Profil değerleri çözümlenmiş biçimde; basınç 25,9 Hz kabul, 26,0 Hz ret; orijinsiz senaryoda IMU dünya dönüşü açık/kapalı; `meta.kalibre_edilmedi` ve `meta.dogrulanmamis_varsayimlar` içerikleri; `etkin = false` sensörün A1'de kaydı yok |
| K50, K52, K53 | birim + SIL | Kimlik tekilliği, tip aralıkları; otopilot bloğunu değiştirmek fizik izini değiştirmez (aynı tohum, bit eşit); `otopilot.nominal` reddi |
| K56–K57 | SIL | Görev öğeleri kartın görev yürütücüsüne aynı sırayla ve aynı alan değerleriyle ulaşır |
| K60–K73, K109 | birim + SIL | Başlangıç ve bitiş adım sınırları; aynı sınırda bitişlerin başlangıçlardan önce uygulanması; çelişen iki zamanlı atamanın reddi; koşullu çelişkide emniyet alanında kısıtlayan değerin, profil alanında sonrakinin uygulanması ve `atama_celiskisi` olayı; her desteklenen türün Tablo 3.10-A etkisi; `zaman_ns = 0`'da başlayan `donma` ve `donma_zaman_damgasi`'nda arıza boyunca kayıt olmaması, sayaç ve olay; titreşimli geç teslimde `t_teslim' ≥ t_teslim`; desteklenmeyen türün reddi |
| K16, Ek E | belge tutarlılığı | Yalnız Ek E ve kurallardan yazılmış bağımsız çözümleyici iki örnek için altın dosyayla aynı davranış özetini üretir; Ek E `[kosulsuz]` şemadaki `default` değerleriyle, `[profil.*]` 3.6 tablosuyla bire bir eşit |
| K74–K77 | birim + SIL | Profil zorunlulukları; HIL'de SIL alanlarının reddi |
| K78–K81 | birim + SIL | Kartlı kipte 30 000 ışının reddi, 40 000'in kabulü ve kanıt kaydı; SIL'de ölçülmemiş değerin uyarısı; emniyet testleri her kademede aynı |
| K82–K85, K110–K112 | birim | Ek D'deki bilinen yanıt vektörleri (Philox sözcükleri bit eşit; normal değerler aynı platformda bit eşit); bir sensörün çekiliş sayısını değiştirmek diğer akışları değiştirmez; kamera kapalıyken aynı özetle iki canlı SIL koşusu bit eşit (kamera açıkken bu sınama yapılmaz; güvence A6 girdi tekrarıdır) |
| K86–K88 | birim | Kütle durumu seçimi; varsayılan emniyet durumu |
| K90–K95 | birim | Aşama sırası; bulguların toplanması; rapor biçimi ve sıralama; `yol` sözdizimi |

---

## 8. Açık kalanlar

| # | Konu | Etiket | Neyle kapanır |
|---|---|---|---|
| 1 | 30Q hücre haritaları (B0/B1) aktarılmadı; `kip` yalnız `ideal` (AB-088) | değer bekleniyor | X-57 eklerinin aktarılması; minor sürüm |
| 2 | Desteklenmeyen olay türleri: `nan`, `aralik_disi`, `kismi`, `tutukluk`, `dolanma`, `ani_cokme`, `dusuk_batarya`, `gecikme_sicramasi`, `cop_bayt`; etkileri (zorlanan değer, kanal seçimi, akım davranışı, bayt sayısı, saat alanı) tanımlanamadı (AB-093, AB-094) | açık | 04, 05, 06 uygulamalarının tanımı ve mimar kararı; minor sürüm |
| 3 | Sigorta modeli, telsiz marj modeli ve anten konumu bu sürümde yok | mimar kararı | Model tanımı; minor sürüm |
| 4 | Ping360 u8 eşlemesi (`L_min`, `L_maks`), kazanç ofsetleri ve Ping1D algılama eşiği A5'ten ayarlanamaz; değerleri sensör modeli uygulaması sabitler ve sürümüyle A6'ya yazar | uygulamaya | W3/W4 sensör ve sonar modeli |
| 5 | KE (kalibre edilmedi) parametrelerin ideal değerleri gerçekçi değildir (ör. Ping360 tarama süresi bağlantı ve motor süreleri 0 iken üreticinin 33 s'sinden kısadır) | tahmin | Kalibrasyon (05 §6, M36) |
| 6 | Soğurma formülünün geçerlilik alanı doğrulanmadı; uyarıyla kullanılıyor (AB-089) | uygulamaya | Sensör modeli işi (05 §2.4.2) |
| 7 | `B_n` değerleri (25,6; 2,7; 40,3 µT) WMM'den doğrulanmadı; orijin 41,0° K, 29,0° D | mimari varsayım | WMM hesabı (gerekirse) |
| 8 | A7 uygunluk örneğinin `.glb` dosyaları yoktur (manifest yalnız); örnek senaryonun A7 denetimi manifest düzeyindedir | A7 | Gerçek varlık dosyaları |
| 9 | İDA-0 kamera ayarları (`ida0.toml`: iç parametreler su içinde 80° yatay görüş açısı kabulünden, bozulmasız, pozlama 8 ms, kırpma 0,05–40 m, kalibrasyon sürümü 1) mimari seçimidir; ölçüm ya da kalibrasyon değildir. `ornek-havuz` kamerayı senaryo düzeyinde kapatır (A5-K85 sınaması) | mimari seçimi | Kamera kalibrasyonu (gerekirse) |
| 10 | 04'ün sayısal ayarları (nokta modeli eşikleri, temas uyarı eşiği, torpido süresi sınırı) A5'te yok | mimari seçimi | Ölçüm (M15) gerekirse minor sürüm |
| 11 | Kabul denetimleri (08 `meta.checks`) ve Monte Carlo tohum aileleri A5'te yok | açık | 09 test altyapısı |
| 12 | Otopilot parametre kataloğu W3'ündür (A4 O9); katalog gelene kadar kimliklerin varlığı ve aralıkları denetlenemez | W3 bekliyor | Parametre kataloğu |
| 13 | MP7'nin gerçekleşen ışın sayıları (40 014, 160 008) izinli kümenin adlarından (40 000, 160 000) farklı; tam kademenin ışın örnekleme ızgarası ad ile gerçekleşen arasındaki eşlemeyi sabitlemeli | ölçülecek | Sonar modeli uygulaması (W4) |
| 14 | Normal ve üstel dönüşümlerin platformlar arası bit eşitliği yok (A5-K112) | sınır | — |
| 15 | `O_b` tanımı: mimar "çerçevenin geometrik merkezi" dedi, 04 "boş aracın ağırlık merkezi"; İDA-0'da ikisinin çakıştığı kabul edildi (`r_g = 0`) | mimari varsayım | Gerçek varlık (A7) gelince yeniden değerlendirme |
| 16 | Sensör arızası Gilbert–Elliott deseni olay zamanlı tanımlandı (Ek D.5); 05 §2.7'nin adım başına geçiş biçiminden farklıdır, ortalama kalışlar aynıdır | sözleşme seçimi | 05 uygulaması bu tanıma uyar |
| 17 | Işık varsayılanları (güneş tepede, 1000 W/m², ortam ışığı 0) kalibrasyon değeri değil yer tutucudur; "kalibre edilmedi" listesinde görünür (A5-K101); mimar yer tutucu olarak kabul etti | mimari seçimi (yer tutucu) | Senaryo yazarı |
| 18 | A1'in `--a1-seed` değerinin `a1/board_tohum` etiketinden türetilmesi A5'in tanımıdır; A1-K114 bunu yazmıyor | A1 bekliyor | A1 atfı (Ek C) |

---

## 9. Açık bulgu kapanış tablosu

`docs/mimari/acik-bulgular.md` "A5 — Senaryo ve araç tanımı" başlığındaki on bir bulgu:

| Bulgu | Durum | Sözleşmede | Gerekçe |
|---|---|---|---|
| AB-085 | kapandı | A5-K27, A5-K06 | A5'te PWM kaybı seçimi yoktur; kullanım süresi dolunca nötr (iskelet §4), kullanım süresi A1'de |
| AB-086 | kapandı | 3.7; A5-K50, K52, K26 | Fizik gerçeği ve otopilot parametreleri ayrı tablolardır; örtük kopya ve fizikten türetim yok (W1-uyum P3-14), sapmalar yalnız fiziğe |
| AB-087 | açık | 3.7 | Batarya eşikleri A4 parametreleridir ve `otopilot.parametreler` ile taşınır; değerleri A4 tablosunu ve mimarı bekliyor |
| AB-088 | açık | A5-K28, K102; 8. başlık md. 1 | Harita yapısı şemada; değerler aktarılmadığı için yalnız ideal kaynak destekleniyor |
| AB-089 | uygulamaya devredildi | A5-K108; 8. başlık md. 6 | Formülün geçerlilik alanının doğrulanması sensör modeli işidir (05 §2.4.2). A5 doğrulanmamış kullanımı görünür kılar: çözümlenmiş senaryoda `meta.dogrulanmamis_varsayimlar`, raporda zorunlu uyarı |
| AB-090 | kapandı | A5-K66, K48; şemada `exclusiveMinimum` | Titreşim gecikmeyi aşamaz; zaman sabitleri ve GE süreleri sıfırdan büyüktür |
| AB-091 | kapandı | A5-K40, K48, K101; şema `ayar` tabloları | Sensör geçersiz kılmaları türe göre şemada; süreç süreleri pozitif; sonar menzili kartın yapılandırması, örnek üst sınırı A5-K78 |
| AB-092 | kapandı | A5-K08; Ek B | Anahtarlar ASCII; ad eşlemesi dondurulmuş tabloda |
| AB-093 | açık | Tablo 3.10-A; A5-K102; 8. başlık md. 2 | Desteklenen aktüatör ve güç türlerinin (`itki_carpani`, `esc_sinyal_kaybi`, `ic_direnc_artisi`, `bara_kopmasi`) alanları ve etkileri tanımlandı; `tutukluk`, `dolanma`, `ani_cokme`, `dusuk_batarya` tanımlanamadı ve reddediliyor. 04'ün `FaultSpec` uygulaması ayrıca 04 işidir |
| AB-094 | açık | 3.11; Tablo 3.10-A; A5-K74, K75, K102 | Emülatör profil alanları ve desteklenen enjektör işlemleri tanımlandı; telsiz marj modeli, `gecikme_sicramasi` ve `cop_bayt` tanımlanamadı ve reddediliyor |
| AB-095 | kapandı | 3.7; A5-K52, K53 | Otopilot bloğunun yeri ve biçimi tanımlandı (açık kimlik–değer çiftleri); katalog W3'te, `PARAM_*` taşıması A1/A4'te (Ek C) |

Uyumsuzluk tablosunda A5'e değinen satırlar:

| Satır | Durum | Sözleşmede | Gerekçe |
|---|---|---|---|
| U-11 | A5 payı kapandı | A5-K27 | ESC sinyal kaybı seçimi kaldırıldı; komut ömrü A1'de |
| U-13 | A5 payı kapandı | 2. başlık; 3.2 | Sabit geometrik `O_b`, yük ayrılınca yerinde kalır; 02/05'in CG referansı A4'te |
| U-16 | A5 payı kapandı | 3.3; A5-K104 | İtki eğrisi ve `B` tek A5 kaynağında; kartın nominal değerleri açık parametre çiftleriyle verilir (A5-K52) |
| U-20 | A5 payı kapandı | A5-K13, K18; 3.11 | A7 varsayılan, A5 geçersiz kılar; bilinmeyen malzeme reddedilir |

Başka gruplarda olup A5'e değinen bulgular (bilgi): AB-001 (A5 payı A5-K27 ile kapandı), AB-023 (basınç hız sınırı A5-K42'de), AB-155 (hedef sınıfı `hedefler.<ad>.sinif`; isabet bölgesi A7 varlığında).

Özet: 11 bulgudan 6'sı kapandı, 1'i uygulamaya devredildi (AB-089), 4'ü açık (AB-087, AB-088, AB-093, AB-094; hepsi önem belirtilmemiş). Kritik bulgu yok.

---

## Ek A — Türetilmiş kütle durumları (A5-K23, K24 test vektörleri)

`ida0.toml` değerleriyle (1e-4 bağıl tolerans; `I` `O_b`'ye göre):

| Durum | `m` (kg) | `r_g` (m) | `∇` (m³) | `r_b` (m) | `I_xx / I_yy / I_zz` | `I_xz` | `I_yz` |
|---|---|---|---|---|---|---|---|
| Boş | 13,5 | 0 | 0,0137 | (0; 0; −0,01) | 0,26 / 0,23 / 0,37 | 0 | 0 |
| Bir yüklü (`f1`) | 13,675 | (0; 0,0012797; 0,0012797) | 0,0138676 | (0; 0,0012082; −0,0086709) | 0,263528 / 0,232114 / 0,372114 | 0 | −0,00175 |
| İki yüklü | 13,85 | (0; 0; 0,0025271) | 0,0140351 | (0; 0; −0,0073736) | 0,267056 / 0,234228 / 0,374228 | 0 | 0 |

**Trimli vektörler** (`trim_kutlesi = 0,5` kg, `trim_konumu = (0,10; 0; 0,05)` m; yalnız test için, `ida0.toml`'da trim yok):

| Durum | `m` (kg) | `r_g` (m) | `∇` (m³) | `r_b` (m) | `I_xx / I_yy / I_zz` | `I_xz` | `I_yz` |
|---|---|---|---|---|---|---|---|
| Boş + trim | 14,0 | (0,0035714; 0; 0,0017857) | 0,0137 | (0; 0; −0,01) | 0,26125 / 0,23625 / 0,375 | −0,0025 | 0 |
| Bir yüklü (`f1`) + trim | 14,175 | (0,0035273; 0,0012346; 0,0029982) | 0,0138676 | (0; 0,0012082; −0,0086709) | 0,264778 / 0,238364 / 0,377114 | −0,0025 | −0,00175 |
| İki yüklü + trim | 14,35 | (0,0034843; 0; 0,0041812) | 0,0140351 | (0; 0; −0,0073736) | 0,268306 / 0,240478 / 0,379228 | −0,0025 | 0 |

Torpido: `a = 0,10`, `b = 0,02` m; `∇_t = 1,67552·10⁻⁴` m³; `I_x,t = 2/5 m b² = 2,80·10⁻⁵`, `I_y,t = I_z,t = 1/5 m (a² + b²) = 3,64·10⁻⁴` kg·m². Köşegen dışı terimler matris elemanıdır (`I_xz = −Σ m x z`).

## Ek B — Ad eşlemesi (AB-092)

| 04/05/08'deki ad | Dosyadaki anahtar |
|---|---|
| `araç` | `arac` |
| `dalga` / `akıntı` / `rüzgar` | `ortam.dalga` / `ortam.akinti` / `ortam.ruzgar` |
| `itici[8]` | `arac.iticiler.t1` … `t8` |
| `esc` / `batarya` / `torpido` / `temas` | `arac.esc` / `arac.batarya` / `arac.torpido` / `arac.temas` |
| `fırlatıcı[2]` | `arac.firlaticilar.f1`, `f2` |
| `hedefler[]` | `hedefler.<ad>` |
| `sensörler[]` | `arac.sensorler.<ad>` |
| `sayısal` | yok (8. başlık md. 10) |
| `tohum` | `rastgelelik.ana_tohum` |
| `arızalar[]` | `olaylar[]` |
| `görev` / `bağlantı` | `gorev` / `baglanti` |
| `emniyet_girdileri`, `kalibrasyon`, `kalite`, `otopilot`, `ortam` | aynı |
| `meta.schema_version` | `meta.sema_surumu` |
| A6 kayıt kademesi, `kamera_kareleri` | `kalite.kayit`, `kalite.kamera_kareleri` (aynı değer kümeleri) |
| A1 Tablo 3.6a görev öğesi alanları | `gorev.ogeler[i]` aynı adlarla |
| A7 malzeme alanları | `kalibrasyon.malzeme.<id>.akustik.*` aynı adlarla |

## Ek C — Başka sözleşmelerden beklenenler

**A7'den beklenen.** (W1-uyum turunda A7'nin yeni metniyle karşılandı; bilgi için) A7 kimlik deseni (`^[a-z0-9_]{1,32}$`); `assets/ornek/manifest.json` uygunluk örneğinin `ida0` (`arac`), `torpido_ida0` (`torpido`), `hedef_ornek` (`hedef`), `havuz_ornek` (`statik`, `taban_derinligi_m = 3,0`) varlıklarını, `MNT_kamera0`, `TUBE_1`, `TUBE_2` bağlantı noktalarını taşıması (A5 değerleriyle tutarlı: kamera (0,21; 0; −0,02) m, tüpler (0; ±0,10; 0,10) m); A7-K56 (taban derinliği) ve A7-K57 (malzeme alanlarının kaynağı). Kalan beklenti: `.glb` dosyaları üretildiğinde aynı kimliklerin korunması.

**A1'den beklenen.**
1. Sürekli komutun kullanım süresi (A5-K27) ve bekleme sınırı.
2. El sıkışmada `max_ping360_samples = 1200` (A5-K78; iki kademede aynı).
3. HIL'de `otopilot` parametre çiftlerinin karta yüklenmesi ya da karşılaştırılması (A5-K53) ve ön denetim raporunun A5-K92 biçimiyle uyumu (aşama 3).
4. GCS ağ geçidinin `gcs_komutu` olaylarını ve görev yüklemesini (Tablo 3.6a alanları, A5-K55) taşıyan mesajları.
5. Protokol enjektörünün `duzlem = hil` ve `gcs` için uygulandığı katman ve `mesaj_turu` adları.
6. Algılama ve sensör kayıtlarında Tablo 3.10-A'nın değiştirdiği bayrak bitlerinin (`geçerli`, `sensör_arızası`) adları.
7. A1-K85'in fizik sınırı sırasını iskelet §3.2'deki gibi yazması (A5-K63 ona atıf verir).
8. A1-K114'te `--a1-seed` değerinin A5 `a1/board_tohum` etiketinden (Ek D.7) türetildiğinin ve `run_id`'nin SIL'de `a1/run_id`'den üretildiğinin yazılması.

**A2'den beklenen.** A2-K93–K95 ve §3.6'nın bugünkü metni; A2'nin "A5'ten beklenen" sekiz maddesi bu sürümde karşılanmıştır (3.6 kamera eşleme tablosu, 3.9, 3.11, 3.12, A5-K80, K113–K118, Ek D.7).

**A3'ten beklenen.** Doğrulama ucunun A5-K92 nesnesini değiştirmeden döndürmesi; formun şemadan üretilmesi; elle enjeksiyonlarda `E<n>` kimlikleri ve `ariza/E<n>` akışı; A3-K95'in fizik sınırı sırası (A5-K63); §3.15'teki A5 olayları için adlandırılmış taşıyıcılar (W1-uyum U18).

**A4'ten beklenen.** Batarya eşikleri ve diğer parametrelerin kataloğu (W3; A4 O9); görev öğesi alanlarının A1 Tablo 3.6a'ya atfı; komut ve kip adları. (Fizik kaynağı sütunu beklentisi W1-uyum P3-14 ile kalktı.)

**A6'dan beklenen.** Çözümlenmiş senaryonun kanonik baytlarının `cozumlenmis_senaryo.json` eki olarak, davranış özetinin `sha256:<hex>` ve kaynak özetlerinin ayrı alanlarda saklanması; kayıt kademesi ve `kamera_kareleri` değer kümeleri (A5-K78); §3.15'teki olayların (`atama_celiskisi`, `gevsetme_reddedildi`, `donma_onceki_cikti_yok`, olay sonucu) ve doğrulama raporunun (ön denetim raporundan ayrı ek) kanal eşlemesi; A7 manifest özetinin ve küme sürümünün kaydı (A5-K115).

**A8'den beklenen.** `class_id` için A5-K113 tablosuna atıf; nominal montajın A5 `ayar.nominal_montaj`'dan geldiği (A8-K53); `L_alg` aralığı (A8 §6.1; A5 aynı sayıları şemada taşır); tek kamera (A8-K35).

**04, 05 ve 06 uygulamalarından beklenen (sözleşme değil).** Tablo 3.10-A etkilerinin uygulanması; KE parametrelerin ideal anlamı; model içi rastgele çekilişlerin A5-K110 düzenine bağlanması; desteklenmeyen türlerin tanımı (8. başlık md. 2).

## Ek D — Rastgelelik sözleşmesi, sürüm 1

**D.1 Üreteç.** Philox4x32-10 (Salmon, Moraes, Dror, Shaw 2011; Random123). Çarpanlar `M0 = 0xD2511F53`, `M1 = 0xCD9E8D57`; anahtar artışları `W0 = 0x9E3779B9`, `W1 = 0xBB67AE85`. On turun her birinde: `p0 = M0 · c0`, `p1 = M1 · c2` (64 bit); `c ← (hi(p1) ⊕ c1 ⊕ k0, lo(p1), hi(p0) ⊕ c3 ⊕ k1, lo(p0))`; ardından `k0 += W0`, `k1 += W1` (mod 2³²). Çıktı dört 32 bitlik sözcüktür.

**D.2 Bilinen yanıt vektörleri (Random123 `tests/kat_vectors`).**

| Sayaç (c0 c1 c2 c3) | Anahtar (k0 k1) | Beklenen çıktı |
|---|---|---|
| `00000000 00000000 00000000 00000000` | `00000000 00000000` | `6627e8d5 e169c58d bc57ac4c 9b00dbd8` |
| `ffffffff ffffffff ffffffff ffffffff` | `ffffffff ffffffff` | `408f276d 41c83b0e a20bc7c6 6d5451fd` |
| `243f6a88 85a308d3 13198a2e 03707344` | `a4093822 299f31d0` | `d16cfe09 94fdcceb 5001e420 24126ea1` |

Doğrulama: bu belgenin başvuru uygulaması (Python) ve depodaki MP7 ölçüm prototipinin üreteci (`tools/olcum/mp7/kaynak/rng.hpp`, Apple clang ile derlendi) üç vektörü de bit bit üretti. MP7 prototipinin anahtar türetimi (splitmix64) bu sözleşmeninkinden (SHA-256) farklıdır; MP7'nin düzgün dönüşümü A5-K111 ile aynıdır, karmaşık normal dönüşümü A5-K111'deki CN(0, 1) biçimidir.

**D.3 A5 anahtar ve sayaç vektörleri.** `n` ve `c2` A5-K110'a göre; `u0 = u(w0)`; `z0, z1` Box–Muller (`w0`, `w1`). Philox sözcükleri ve `u0` bit düzeyinde normatiftir; `z0`, `z1` bilgi amaçlıdır (A5-K112; macOS arm64'te Python 3.12 ve Apple clang libm ile aynı çıktı).

| `ana_tohum`, alt sistem | SHA-256 anahtar özeti | `k0 k1` | `n`, `c2` | Çıktı `w0 w1 w2 w3` | `u0` | `z0`; `z1` |
|---|---|---|---|---|---|---|
| 1, `fizik/dalga` | `570da964f86c050a90b5ea1c153ea64195bd3a09e29e8f7b38c8213608a24ad3` | `64a90d57 0a056cf8` | 0, 0 | `a62dda3b ec8d7d6b 9fb8419a 83ee6b35` | 0.6491371531737968 | 0.8257344949355085; −0.42706563087197097 |
| 1, `fizik/dalga` | aynı | aynı | 1, 0 | `32db07b0 1952207c 160a5668 9d2d9ea3` | 0.19865463313180953 | 1.4617232135179496; 1.0467760958628956 |
| 1, `fizik/dalga` | aynı | aynı | 2³² + 5, `0x03010007` | `14bdf9e0 202cc9a5 dd5ee3bf c181b39f` | 0.08102380495984107 | 1.5784267388341056; 1.5920406993290706 |
| 1, `sensor/imu0/olcum` | `0f8822c8ff5b86074ba3bd29d2829757a4a0a1eace1aefe3cc1f73e7fedb62e6` | `c822880f 07865bff` | 0, 0 | `9f4e8d20 8607e250 c2b93430 adc365d1` | 0.6222923473687842 | −0.9633570170026116; −0.14364479602771965 |
| 0, `fizik/akinti` | `2231a977ad3d7fd128caa299fbc7812d722eeb8467def13dd850521339115494` | `77a93122 d17f3dad` | 0, 0 | `ac35a29e 36a3f2d5 349cfeef efb49abe` | 0.6726934086764231 | 0.20276240209932794; 0.8670747562427242 |

**D.4 Bu sözleşmenin tanımladığı çekilişler.** `kanal` sensör kaydının kanal numarasıdır (0–254; 05 §2.7 numaralandırması); `c2`'nin üst baytındaki 255 (`0xFF`) zaman deseni çekilişlerine ayrılmıştır.

| Çekiliş | Alt sistem | `n` | `c2` | Dönüşüm |
|---|---|---|---|---|
| Hedef başlangıç yaw'ı (`yonelim_rastgele`) | `hedef/<ad>` | 0 | 0 | `ψ = π · (2 u(w0) − 1)`; başlangıç yönelimi `q_z(ψ) ⊗ yonelim`, `q_z(ψ) = (cos(ψ/2), 0, 0, sin(ψ/2))` (NED z ekseni) |
| `gurultu_artisi` | `ariza/<kimlik>` | sensör kaydının `seq`'i | `kanal << 24` | `z` = `z0` (`w0`, `w1`) |
| `sicrama` | `ariza/<kimlik>` | `seq` | `(kanal << 24) | (1 << 16)` | olur: `u(w0) < p`; işaret: `w1 < 2³¹` ise +1, değilse −1 |
| `bernoulli` zaman deseni | `ariza/<kimlik>` | `seq` | `0xFF << 24` | etkin: `u(w0) < p` |
| `gilbert_elliott` zaman deseni | `ariza/<kimlik>/gecis` | kalış sırası `i` | 0 | D.5 geçiş dizisi; ortalamalar `t_iyi_ns`, `t_kotu_ns`; süreç olayın başladığı sınırda "iyi" başlar |
| `gec_teslim` titreşimi (kayıt başına) | `ariza/<kimlik>` | `seq` | `2 << 16` | `j = ⌊u(w0) · (2 · titresim + 1)⌋ − titresim` |
| Protokol işlemleri | `protokol/<kimlik>` | olay başladıktan sonra o yönde `mesaj_turu`'na uyan çerçevelerin sırası (0'dan) | `yön << 24` (yukarı 0, aşağı 1) | olur: `u(w0) < p`; konum ya da uzunluk `⌊u(w1) · m⌋` (bayt bozmada `m = uzunluk`; kesmede uzunluk `1 + ⌊u(w1) · (uzunluk − 1)⌋`); XOR değeri `1 + ⌊u(w2) · 255⌋` |
| Bağlantı emülatörü, GE kalış süreleri | `baglanti/<profil>/<yon>/gecis` | kalış sırası `i` | 0 | D.5 geçiş dizisi; ortalamalar `1/λ_GB` ("iyi"), `1/λ_BG` ("kötü") saniye |
| Bağlantı emülatörü, çerçeve başına | `baglanti/<profil>/<yon>/cerceve` | o profil ve yöndeki çerçeve sırası (0'dan, gönderim sırası) | 0 | `w0`: L1'de kayıp `u(w0) < kayip_p`; L2'de başarı `u(w0) < k` ("iyi") ya da `u(w0) < h` ("kötü"), durum gönderim anındaki kalıştan. `w1`: titreşim `⌊u(w1) · (2J + 1)⌋ − J` ns (`J = titresim_ns`), gecikme `max(0, gecikme_taban_ns + titreşim)`. Bayt bozma (`bayt_bozma_orani`) aynı `n` ile `c2 = 1` (çekiliş 1) sayacından: `u(w0) < oran` ise tek bayt bozulur, konum `⌊u(w1) · uzunluk⌋`, XOR değeri `1 + ⌊u(w2) · 255⌋` |

**D.5 Gilbert–Elliott süreçleri (sürüm 1).** Her GE süreci başlangıç anında "iyi" durumdadır: bağlantı emülatörünün her profil × yön süreci sim zamanı `t = 0`'da (profil o anda etkin olsun ya da olmasın, süreç `t = 0`'dan kesintisiz ilerler); sensör arızası zaman deseni süreci olayın başladığı adım sınırında. Her sürecin iki ayrı sayaç dizisi vardır ve diziler **anahtar türetiminde ayrı alt sistem adı** taşır (A5-K84: `…/gecis`, `…/cerceve`; adın UTF-8 baytları A5-K83 girdisine girer):

1. **Geçiş dizisi.** Kalış `i` (`i = 0, 1, 2, …`; 0. kalış "iyi", tek sıralı kalışlar "kötü") için sayaç `n = i`, `c2 = 0`, `c3 = 0`; süre `d_i = max(1, ⌈−log(u(w0)) · m_i⌉)` ns, `m_i` o durumun ortalama kalışı ns cinsinden binary64 (`1e9 / λ` ya da `t_iyi_ns`, `t_kotu_ns`); çarpım tek binary64 işlemidir. Geçiş zamanları `T_0` = başlangıç, `T_{i+1} = T_i + d_i`. Her kalış kendi sayaç konumunu tüketir; iki çerçeve ya da iki kayıt arasında kaç geçiş olursa olsun her biri sıradaki konumu kullanır. Kalış `[T_i, T_{i+1})` yarı açık aralıktır.
2. **Çerçeve dizisi** (yalnız emülatör). Sayaç `n` = çerçeve sırası; çerçeve başına çekilişler D.4'teki sözcüklerle. Sensör arızasında çerçeve dizisi yoktur: kayıt `t_meas` "kötü" kalıştaysa arıza etkindir.

`log` kullanıldığından kalış süreleri A5-K112 sınırına tabidir (platformlar arası eşitlik garanti edilmez); Philox sözcükleri normatiftir.

**D.6 Gilbert–Elliott bilinen sonuç vektörü.** `ana_tohum = 1`, profil `ornek`, yön `asagi`, `λ_GB = 0,5 s⁻¹`, `λ_BG = 5 s⁻¹`, `k = 0,99`, `h = 0,25`.

| Dizi | Anahtar `k0 k1` | `n` | `w0` | `u(w0)` | Sonuç |
|---|---|---|---|---|---|
| geçiş (`baglanti/ornek/asagi/gecis`) | `e3e9c071 5496e939` | 0 | `6f12e7a8` | 0.43388221610803157 | "iyi" kalış `d_0 = 1669964347` ns |
| geçiş | aynı | 1 | `d7a09977` | 0.8422943035839126 | "kötü" kalış `d_1 = 34325160` ns |
| geçiş | aynı | 2 | `7e6c6057` | 0.49384119163732976 | "iyi" kalış `d_2 = 1411082576` ns |
| geçiş | aynı | 3 | `0576bb6b` | 0.02134295820724219 | "kötü" kalış `d_3 = 769406685` ns |

İlk üç geçiş zamanı: `T_1 = 1669964347`, `T_2 = 1704289507`, `T_3 = 3115372083` ns.

| Çerçeve (`baglanti/ornek/asagi/cerceve`, anahtar `6e26fc32 e31136e7`) | Gönderim anı (ns) | Durum | `w0` | `u(w0)` | Sonuç |
|---|---|---|---|---|---|
| 0 | 0 | iyi | `c71e071a` | 0.7778019370744005 | geçti (< 0,99) |
| 1 | 1000000000 | iyi | `bedd1223` | 0.7455607735319063 | geçti |
| 2 | 1680000000 | kötü | `ca3644a1` | 0.7898905653273687 | kayıp (≥ 0,25) |
| 3 | 1700000000 | kötü | `6ef0178d` | 0.4333510132273659 | kayıp |
| 4 | 3200000000 | kötü | `f64ef78b` | 0.9621424403740093 | kayıp |

Philox sözcükleri (`6f12e7a8` ve `ca3644a1` dahil) MP7 prototipinin derlenmiş üreteciyle de aynı çıktı.

**D.7 Kimlik, nonce ve kamera gürültüsü (A5-K118; W1-uyum U17).** Alt akış kataloğuna eklenen etiketler, sahipleri ve türetimleri:

| Etiket | Kullanan | `tohum` | Değer | Yasak / yeniden çekiliş |
|---|---|---|---|---|
| `a1/run_id` | A1 `run_id` (u32), SIL (W1-uyum U17) | `ana_tohum` | `w0` | 0 → sonraki `n` |
| `a1/board_tohum` | SIL koşucusunun `--a1-seed` değeri (A1-K114) | `ana_tohum` | `w0 | (w1 << 32)` (u64) | yok |
| `ida/a1/nonce_sim/<deneme>` | A1-K114, `IDA_SIM_HELLO.nonce` (u32) | `ana_tohum` | `w0` | 0 → sonraki `n` |
| `ida/a1/nonce_board/<boot_count>/<deneme>` | A1-K114, `IDA_BOARD_HELLO.nonce` (u32) | `a1/board_tohum` değeri | `w0` | 0 → sonraki `n` |
| `ida/a1/sess/<oturum_sırası>` | A1-K114/K15, `sess` (u16) | `ana_tohum` | `w0 & 0xFFFF` | 0, `prev_sess` ve son 16 oturum → sonraki `n` |
| `a2/kamera_gurultu` | A2-K95, `gurultu_tohumu` (u64) | `ana_tohum` | `w0 | (w1 << 32)`; sayaç `n = kare_sirasi`, `c2 = kamera_kimligi << 24` | yok (kare başına tek çekiliş) |

Etiketlerdeki sayılar başında sıfır olmayan onluk ASCII'dir (`<deneme>`, `<boot_count>`, `<oturum_sırası>` anlamları A1-K114'te). `tohum` 64 bittir; `ana_tohum`'un 2⁵³ sınırı (A5-K82) yalnız dosya alanınındır. Etiketler katalogdaki diğer adlarla çakışmaz (önekleri farklıdır).

| Etiket (`ana_tohum = 1`) | Anahtar `k0 k1` | Sonuç |
|---|---|---|
| `a1/board_tohum` | `ca3d4094 4d0ec9ef` | `0xb1536801d0cf436c` |
| `a1/run_id` | `fbda3b40 8ef52f96` | `n = 0`: 1059443490 (`0x3f25d322`) |
| `ida/a1/nonce_sim/0` | `ec125d0e faf5b70a` | `n = 0`: 3869573587 (`0xe6a501d3`) |
| `ida/a1/nonce_board/1/0` (`tohum = 0xb1536801d0cf436c`) | `6c316b29 d1b8de2f` | `n = 0`: 220998108 (`0x0d2c29dc`) |
| `ida/a1/sess/0` | `1dac5271 c7f5c672` | `n = 0`: 18101; 18101 yasaksa `n = 1`: 6699 |
| `a2/kamera_gurultu`, kamera 0 | `c7bd3f98 cc0aef21` | kare 1: `0x8bacb11bdc5f81f9`; kare 2: `0xfbfa969d1162aba6` |

## Ek E — Normatif varsayılanlar (A5-K16; makinece okunur)

Bu blok geçerli bir TOML belgesidir ve çözümleyicinin kullandığı bütün varsayılanları taşır. Belgenin tablolarındaki varsayılan sütunları bu bloğu gösterir; çelişirse bu blok geçerlidir ve tablo düzeltilir. Sayılar, çözümlemede kullanılan binary64 değerlerin en kısa gidiş-dönüş ondalığıdır (A5-K96'nın JCS dizilimiyle aynı basamaklar). Uygulama sırası A5-K16'dadır. `profil.imu.saat_kaymasi_ppm = "yok"` anahtarın yazılmadığını belirtir.

```toml
# Ek E — normatif varsayılanlar (A5-K16). Sayılar binary64 değerlerinin en kısa gidiş-dönüş ondalığıdır.
surum = "1.0"

# K99: senaryoda yoksa üretilen kök tablolar (adım 3)
[eksik_tablolar]
hedefler = {}
olaylar = []
emniyet_girdileri = {}
baglanti = {etkin_profil = "ideal", profiller = {ideal = {kademe = "L0"}}}
kalibrasyon = {}
kalite = {}
baslangic = {}
otopilot = {}

# Koşulsuz varsayılanlar (adım 4): yol yoksa ve ebeveyni varsa yazılır; '*' anahtarlı koleksiyonun,
# '[]' dizinin her elemanı; sıra ebeveyn önce.
[kosulsuz]
"otopilot.parametreler" = []
"ortam.sahne_poz" = {}
"ortam.sahne_poz.konum" = [0.0, 0.0, 0.0]
"ortam.sahne_poz.yonelim" = [1.0, 0.0, 0.0, 0.0]
"ortam.su_optigi" = {}
"ortam.su_optigi.surum" = 1
"ortam.isik" = {}
"ortam.yercekimi" = 9.82
"ortam.atmosfer_basinci" = 101325.0
"ortam.hava_yogunlugu" = 1.225
"ortam.su.ph" = 8.0
"ortam.dalga" = {}
"ortam.dalga.spektrum" = "yok"
"ortam.dalga.yayilim" = "cos2s"
"ortam.dalga.yayilim_s" = 10.0
"ortam.dalga.bilesen_sayisi" = 64
"ortam.akinti" = {}
"ortam.akinti.hiz_ned" = [0.0, 0.0, 0.0]
"ortam.akinti.gauss_markov" = {}
"ortam.akinti.gauss_markov.etkin" = false
"ortam.ruzgar" = {}
"ortam.ruzgar.hiz_ned" = [0.0, 0.0, 0.0]
"ortam.ruzgar.gauss_markov" = {}
"ortam.ruzgar.gauss_markov.etkin" = false
"hedefler.*.sinif" = "bilinmiyor"
"hedefler.*.yonelim_rastgele" = false
"hedefler.*.hareket" = {}
"hedefler.*.hareket.tur" = "sabit"
"gorev.yukleme_zamani_ns" = 0
"emniyet_girdileri.acil_durdurma" = false
"emniyet_girdileri.fis" = {}
"emniyet_girdileri.fis.f1" = false
"emniyet_girdileri.fis.f2" = false
"emniyet_girdileri.pim" = {}
"emniyet_girdileri.pim.f1" = true
"emniyet_girdileri.pim.f2" = true
"baglanti.profiller.*.tasima" = "datagram"
"baglanti.profiller.*.yukari" = {}
"baglanti.profiller.*.yukari.gecikme_taban_ns" = 0
"baglanti.profiller.*.yukari.titresim_ns" = 0
"baglanti.profiller.*.yukari.kayip_modeli" = "yok"
"baglanti.profiller.*.asagi" = {}
"baglanti.profiller.*.asagi.gecikme_taban_ns" = 0
"baglanti.profiller.*.asagi.titresim_ns" = 0
"baglanti.profiller.*.asagi.kayip_modeli" = "yok"
"kalibrasyon.malzeme" = {}
"kalite.gorunum" = "sade"
"kalite.kamera" = "sade"
"kalite.sonar" = "sade"
"kalite.kayit" = "telemetri"
"kalite.kamera_kareleri" = "kapali"
"kalite.kamera_gerekli" = false
"baslangic.konum" = [0.0, 0.0, 0.0]
"baslangic.yonelim" = [1.0, 0.0, 0.0, 0.0]
"baslangic.hiz" = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
"baslangic.yuklu_tupler" = ["f1", "f2"]
"baslangic.batarya_soc" = 1.0
"baslangic.batarya_v1" = 0.0
"arac.kutle.trim_kutlesi" = 0.0
"arac.hidrodinamik.sapma" = {}
"arac.hidrodinamik.sapma.eklenmis_kutle" = [1.0, 1.0, 1.0, 1.0, 1.0, 1.0]
"arac.hidrodinamik.sapma.dogrusal_sonum" = [1.0, 1.0, 1.0, 1.0, 1.0, 1.0]
"arac.hidrodinamik.sapma.karesel_sonum" = [1.0, 1.0, 1.0, 1.0, 1.0, 1.0]
"arac.sensorler.*.etkin" = true

# Koşullu varsayılanlar (adım 5). kosul.yol, yol '[]' içeriyorsa dizi elemanına göredir; deger ya da icinde;
# sil = true ise yol çıkarılır. Koşul, adım 4'ten sonraki değere bakar.
[[kosullu]]
yol = "ortam.su.yogunluk"
kosul = {yol = "ortam.tur", deger = "havuz"}
deger = 1000.0
[[kosullu]]
yol = "ortam.su.yogunluk"
kosul = {yol = "ortam.tur", deger = "acik_su"}
deger = 1025.0
[[kosullu]]
yol = "ortam.su.tuzluluk"
kosul = {yol = "ortam.tur", deger = "havuz"}
deger = 0.0
[[kosullu]]
yol = "ortam.su.tuzluluk"
kosul = {yol = "ortam.tur", deger = "acik_su"}
deger = 0.035
[[kosullu]]
yol = "ortam.dalga.gamma"
kosul = {yol = "ortam.dalga.spektrum", deger = "jonswap"}
deger = 3.3
[[kosullu]]
yol = "calistirma.sil_hiz_carpani"
kosul = {yol = "calistirma.kip", deger = "sil"}
deger = 1.0
[[kosullu]]
yol = "calistirma.sil_azami_hiz"
kosul = {yol = "calistirma.kip", deger = "sil"}
deger = false
[[kosullu]]
yol = "kalite.sonar_tam"
kosul = {yol = "kalite.sonar", deger = "sade"}
sil = true
[[kosullu]]
yol = "kalite.sonar_tam"
kosul = {yol = "kalite.sonar", deger = "tam"}
deger = {}
[[kosullu]]
yol = "kalite.sonar_tam.azami_sekme"
kosul = {yol = "kalite.sonar", deger = "tam"}
deger = 2
[[kosullu]]
yol = "kalite.sonar_tam.hucre_kenari"
kosul = {yol = "kalite.sonar", deger = "tam"}
deger = 0.01
[[kosullu]]
yol = "kalite.sonar_tam.yuzey_yamasi_araligi"
kosul = {yol = "kalite.sonar", deger = "tam"}
deger = 0.05
[[kosullu]]
yol = "olaylar[].sure_ns"
kosul = {yol = "sinif", icinde = ["sensor", "aktuator", "guc", "protokol"]}
deger = 0
[[kosullu]]
yol = "olaylar[].sure_ns"
kosul = {yol = "tur", deger = "kesinti"}
deger = 0
[[kosullu]]
yol = "olaylar[].zaman_deseni"
kosul = {yol = "sinif", deger = "sensor"}
deger = {}
[[kosullu]]
yol = "olaylar[].zaman_deseni.tur"
kosul = {yol = "sinif", deger = "sensor"}
deger = "surekli"
[[kosullu]]
yol = "olaylar[].sira_boz"
kosul = {yol = "sinif", deger = "sensor"}
deger = false
[[kosullu]]
yol = "olaylar[].kanal_maskesi"
kosul = {yol = "sinif", deger = "sensor"}
deger = 4294967295

# Ürün profilleri (adım 5): arac.sensorler.<ad> için tur'a göre. hiz_hz yalnız verilmişse yazılır;
# saat_kaymasi_ppm imu dışında 0; ayar anahtarları yoksa yazılır.
[profil.imu]
profil = "icm42688p"
saat_kaymasi_ppm = "yok"
ayar = {jiroskop_yogunluk = 4.887e-5, ivme_yogunluk = [0.0006374, 0.0006374, 0.0006865], jiroskop_acilis_bias_siniri = 0.008727, ivme_acilis_bias_siniri = 0.1961, jiroskop_sicaklik_kats_siniri = 8.727e-5, ivme_sicaklik_kats_siniri = 0.001471, jiroskop_olcek_siniri = 0.005, jiroskop_eksenler_arasi_siniri = 0.0125, ivme_olcek_siniri = 0.005, ivme_eksenler_arasi_siniri = 0.01, jiroskop_dogrusalsizlik = 0.001, ivme_dogrusalsizlik = 0.0, jiroskop_gm_tau_ns = 1000000000000, ivme_gm_tau_ns = 300000000000, jiroskop_gm_sigma = 4.848e-5, ivme_gm_sigma = 0.001, jiroskop_aralik = 8.7266, ivme_aralik = 39.2266, bit = 16, suzgec_etkin = false, dunya_donusu_etkin = true}
[profil.manyetometre]
profil = "rm3100"
hiz_hz = 100.0
saat_kaymasi_ppm = 0.0
ayar = {gurultu_sigma = 2e-8, yumusak_demir = [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]], sert_demir = [0.0, 0.0, 0.0], sicaklik_kats = 0.0, itici_girisim_orani = 0.2, teslim_gecikmesi_ns = 1000000}
[profil.basinc]
profil = "ms5837_30ba"
hiz_hz = 25.0
saat_kaymasi_ppm = 0.0
ayar = {gurultu_sigma = 20.0, mutlak_dogruluk_siniri = 5000.0, sicaklik_kats = 0.0, kuantalama = 10.0, donusum_penceresi_ns = 18750000, okuma_suresi_ns = 1000000, dinamik_basinc_kats = 0.0, sicaklik_ofset_siniri = 4.0}
[profil.gnss]
profil = "neo_m9n"
hiz_hz = 5.0
saat_kaymasi_ppm = 0.0
ayar = {cep = 2.0, gm_tau_ns = 60000000000, gm_varyans_payi = 0.9, dusey_carpan = 1.5, hiz_sigma = 0.05, teslim_gecikmesi_ns = 120000000, uydu_sayisi = 20, n_sv_ref = 12, w50 = 0.03, w_gecis = 0.005, t_kisa_ns = 10000000000, t_efemeris_ns = 7200000000000, hizli_medyan_ns = 1000000000, sicak_medyan_ns = 2000000000, soguk_medyan_ns = 24000000000, t_edinim_ns = 0, yakinsama_baslangic_hatasi = 0.0, rapor_carpani = 1.0}
[profil.dvl]
profil = "a50"
saat_kaymasi_ppm = 0.0
ayar = {isin_acisi = 0.39269908169872414, yari_guc_genisligi = 0.07679448708775051, azimut_ofseti = 0.7853981633974483, alt_isin_sayisi = 7, gelis_acisi_siniri = 1.0471975511965976, sigma0 = 0.001, k_r = 0.0002, k_u = 0.001, uzun_donem_olcek_siniri = 0.0101, kappa = 1.0, islem_suresi_ns = 5000000, uc_isinla_cozum = false, su_takibi = false, baglanti = "seri", seri_baud = 115200, mod_histerezis_ping = 3, kilit_arama_ping = 3, yakin_mod_deneme_ns = 10000000000}
[profil.ping1d]
profil = "ping1d"
saat_kaymasi_ppm = 0.0
ayar = {kutu_sayisi = 200, ping_araligi_ns = 50000000, oto_menzil_carpani = 1.5, islem_suresi_ns = 0}
[profil.ping360]
profil = "ping360"
saat_kaymasi_ppm = 0.0
ayar = {baglanti = "usb", t_m0_ns = 0, t_grad_ns = 0, islem_suresi_ns = 0, gurultu_tabani = 0.0, yakin_alan_genligi = 0.0, tvg_a = 20.0}
[profil.kamera]
profil = "kamera_on"
saat_kaymasi_ppm = 0.0
ayar = {l_sonrasi_ns = 0, l_alg_ns = 50000000, bozulma_modeli = "yok", bozulma = [0.0, 0.0, 0.0, 0.0, 0.0], kalibrasyon_surumu = 1}
[profil.batarya_olcer]
profil = "psm"
hiz_hz = 10.0
saat_kaymasi_ppm = 0.0
ayar = {gerilim_bolucu = 11.0, akim_ofseti = 0.33, akim_olcegi = 37.8788, analog_gurultu_sigma = 0.002, adc_bit = 12, adc_referans = 3.3, sicaklik_kats = 0.0, teslim_gecikmesi_ns = 1000000}
[profil.sizinti]
profil = "sos_leak"
hiz_hz = 10.0
saat_kaymasi_ppm = 0.0
ayar = {prob_sayisi = 2, algilama_gecikmesi_ns = 1000000000, sahte_pozitif_orani = 0.0}
[profil.firlatici_geri_okuma]
profil = "firlatici_io"
hiz_hz = 10.0
saat_kaymasi_ppm = 0.0
ayar = {anahtar_gecikmesi_ns = 10000000, teslim_gecikmesi_ns = 1000000}

# Kopya varsayılanlar (adım 5): koleksiyon elemanında kosul sağlanır ve hedef yoksa kaynak kopyalanır.
[[kopya]]
koleksiyon = "arac.sensorler"
kosul = {yol = "tur", deger = "kamera"}
hedef = "ayar.nominal_montaj"
kaynak = "montaj"

# Ortamın kalibre edilmemiş değerleri (A5-K101): yol yoksa yazılır ve listelenir.
[kalibre_edilmedi_ortam]
"ortam.su_optigi.beta_d_m" = [0.0, 0.0, 0.0]
"ortam.su_optigi.beta_b_m" = [0.0, 0.0, 0.0]
"ortam.su_optigi.b_sonsuz_rgb_dogrusal" = [0.0, 0.0, 0.0]
"ortam.su_optigi.bulaniklik_olcegi" = 0.0
"ortam.isik.gunes_yonu_n" = [0.0, 0.0, -1.0]
"ortam.isik.gunes_isinimi_w_m2" = 1000.0
"ortam.isik.ortam_isigi_rgb_dogrusal" = [0.0, 0.0, 0.0]

# Kalibre edilmedi (A5-K101): verilmemişse ideal değer yazılır ve etkin sensörde yolu listelenir;
# yazilmaz listesindekiler yazılmaz, yalnız listelenir.
[kalibre_edilmedi]
gnss = {deger = {t_edinim_ns = 0, yakinsama_baslangic_hatasi = 0.0}, yazilmaz = []}
dvl = {deger = {}, yazilmaz = ["c_ayar"]}
ping1d = {deger = {islem_suresi_ns = 0}, yazilmaz = []}
ping360 = {deger = {t_m0_ns = 0, t_grad_ns = 0, islem_suresi_ns = 0, gurultu_tabani = 0.0, yakin_alan_genligi = 0.0}, yazilmaz = ["baud"]}
kamera = {deger = {l_sonrasi_ns = 0}, yazilmaz = []}

# Doğrulanmamış varsayılanlar (A5-K108): etkin sensörde verilmemişse yolu listelenir, değer yazılmaz.
[dogrulanmamis]
ping1d = ["sogurma"]
ping360 = ["sogurma"]

# Ölçüm kanıtları (A5-K79)
[olcum_kanitlari]
sade = {yol = "kalite.sonar", kaynak = "docs/olcumler/MP7-sonar-suresi.md §4.3–4.4 (sade, N = 5 000)"}
tam = {yol = "kalite.sonar_tam.isin_sayisi", kaynak = "docs/olcumler/MP7-sonar-suresi.md §4.6"}
tam_olculmus = [20000, 40000, 80000, 160000]
```
