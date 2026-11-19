# A5 — Senaryo ve araç tanımı dosyası sözleşmesi

| | |
|---|---|
| Sürüm | 1.0-aday, 1. düzeltme turu (2026-11-19) |
| İş | `W1-A5` (`docs/gorevler/W1-A5.md`; ortak kurallar `docs/gorevler/W1-ortak.md`) |
| İnceleme | `docs/sozlesmeler/inceleme/A5-sol.md`; yanıt `docs/sozlesmeler/inceleme/A5-yanit.md` |
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

**Kapsam dışı.** TOML ayrıştırıcısının seçimi ve kodu; modellerin içi (04, 05); A1 mesajlarının biçimi; A7 manifestinin içi (sahneye, varlıklara ve malzemelere yalnız kimlikle atıf verilir); A4 parametre tablosunun içeriği; A3 HTTP uçları; kamera kademe değerleri (A8); kontrol hızı, fizik adımı ve `L_act` (iskelet §4; A5-K77); HIL geçersizlik eşiği (A5-K77); ölçüme bağlı yedekler (iskelet §6); yarışma kuralları, ardıl kart, gerçek sensör tedariki.

**Atış kontrol sınırı.** Torpido ve fırlatıcı alanları iskelet §5'teki itkisiz, kısa menzilli basit modelin parametreleriyle sınırlıdır ve 1.0'da değiştirilemez (A5-K29; mimar onayladı). İtki, güdüm, arayıcı, menzil hedefi ya da atış çözümü alanı yoktur. Zaman çizelgesinde atış komutu yoktur (A5-K73; mimar onayladı).

---

## 2. Terimler ve kabuller

İskelet §3.1'in ortak kabulleri geçerlidir (SI, NED dünya çerçevesi, gövde x ileri / y sancak / z aşağı, gövde referans noktası `O_b`, kuaterniyon (w, x, y, z) Hamilton, gövdeden dünyaya). Bu sözleşmeye özgü olanlar:

| Terim | Anlamı |
|---|---|
| Araç dosyası | `meta.tur = "arac"`; fizik gerçeğini (`arac`) ve isteğe bağlı nominal otopilot bloğunu (`otopilot`) taşır |
| Senaryo dosyası | `meta.tur = "senaryo"`; tek bir araç dosyasına başvurur (`arac.dosya`) ve araç değerlerini alan bazında geçersiz kılabilir |
| Çözümlenmiş senaryo | `meta.tur = "cozumlenmis_senaryo"`; araç birleşmiş, kaynak alanları çıkarılmış, bütün varsayılanlar açık, çözümleyici üst verisi eklenmiş biçim (A5-K16). Modellerin okuduğu tek biçim |
| Kanonik biçim | Çözümlenmiş senaryonun RFC 8785 (JCS) JSON dizilimi (A5-K17) |
| Davranış özeti | Kanonik baytların SHA-256'sı, 64 küçük onaltılık karakter; kaynak dosya baytlarından bağımsızdır (A5-K17) |
| Kaynak özetleri | Araç ve senaryo dosyalarının ham baytlarının SHA-256'sı; davranış özetine girmez, ayrı üst veridir |
| Anahtarlı koleksiyon | Girdileri adla anahtarlanan tablo: `arac.iticiler`, `arac.firlaticilar`, `arac.sensorler`, `hedefler`, `baglanti.profiller`, `kalibrasyon.malzeme`, `otopilot.parametreler` |
| Ürün profili | Sensör modelinin bu belgede sabitlenen varsayılan değer kümesi (3.6); `ayar` ile geçersiz kılınır |
| Kalibre edilmedi | Profilde ölçülmüş ya da yayımlanmış değeri olmayan parametrenin ideal (hata katkısı sıfır) değerle çözümlenmesi; çözümlenmiş senaryoda `meta.kalibre_edilmedi` listesinde görünür (A5-K101) |
| Olay çeşitleri | **Kalıcı atama** (değeri atar, değer sonraki atamaya kadar kalır), **anlık komut** (bir kez uygulanır), **geçici arıza** (başlar, süresi dolunca kalkar) (A5-K109) |
| Değer etiketleri | Örnek dosyalarda: [Y] yayımlanmış ölçüm, [CAD] yayımlanmış CAD hesabı, [AMP] yayımlanmış ampirik tahmin, [Ü] üretici belgesi, [T] tahmin, [K] kalibrasyon parametresi, [C] yapılandırma, [G] çevresel girdi, [MV] mimari varsayım (bölümlerde değer yoktu; mimar belirledi, K10) |

**Birim ve adlandırma kabulleri (A5-K11).** Sayısal değerler birim eki almaz; birim anahtarın tanımındadır. Anahtar adı birimi yalnız şu istisnalarda taşır: `_ns` tamsayı nanosaniye (bütün süreler, zaman sabitleri, periyotlar ve sim zamanı anları), `_c` santigrat derece, `_us` mikrosaniye (ESC darbe genişliği; iskelet §3 A1 istisnası), `_hz` hertz, `_db` ve `_dbm` logaritmik oranlar. Açılar radyandır; enlem ve boylam dahil. Tuzluluk kütle oranıdır (kg/kg). Kuvvet newtondur: üretici kgf değerleri 9,80665 m/s² (standart yerçekimi, tanım değeri) ile çevrilmiştir.

**Gövde referans noktası (İDA-0).** `O_b` çerçevenin geometrik merkezidir; İDA-0'da boş aracın ağırlık merkezi bu noktada kabul edilir (`r_g = 0`, 04 §2.6) [MV]. Gövdeye sabittir ve yük ayrılınca yerinde kalır (iskelet §3.1).

---

## 3. Veri tanımları

**Tablo sütunları.** *Tip:* `tamsayı` (TOML tamsayısı zorunlu), `sayı` (TOML tamsayısı ya da kayan noktalı; binary64'e A5-K96 ile çevrilir), `mantıksal`, `dizgi`, `dizi[n]`, `tablo`, `matris 3×3` (üç satırlık dizi dizisi), `kuaterniyon` (`dizi[4]` sayı, w, x, y, z). *Zorunlu:* E = evet, H = hayır, K = koşullu (koşul anlamda). *Varsayılan:* koşulsuz varsayılan şemada `default` ile durur; koşullu varsayılanlar A5-K98'dedir; "—" varsayılan yoktur. *Besler:* değeri okuyan bölüm. **Geçersiz değerde davranış** bütün tablolar için aynıdır: dosya, ilgili kural numarasıyla reddedilir ve A5-K92 biçiminde bulgu üretilir; uyarı olarak tanımlananlar (A5-K95) yüklemeyi durdurmaz. "Desteklenmiyor" işaretli anahtar ya da tür verilirse A5-K102 ile reddedilir.

### 3.1 Dosya türleri ve `meta`

| Tablo | Araç dosyası | Senaryo dosyası | Çözümlenmiş senaryo |
|---|---|---|---|
| `meta` | E | E | E (+ çözümleyici üst verisi) |
| `arac` | E (tam) | E (`dosya` + kısmi geçersiz kılma) | E (tam; `dosya` yok) |
| `otopilot` | H | H | E |
| `calistirma`, `ortam`, `rastgelelik` | yok | E | E |
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
| `arac.varlik` | dizgi | — | A7 kimliği (`^[A-Za-z0-9_.:/-]{1,128}$`) | — | E | A7 varlığı: `BUOY_*`, `COL_*`, `SON_*` ağları, itici ve fırlatıcı bağlantı düğümleri |
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

Bütün değerler sabittir: şemada `const` olarak durur ve başka değer reddedilir (A5-K29).

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

**Ürün profilleri.** Her anahtar verilmezse aşağıdaki değerle çözümlenir. **KE** = kalibre edilmedi: ideal değer, `meta.kalibre_edilmedi` listesine girer (A5-K101). **DY** = doğrulanmamış varsayılan: değer yazılmaz, model 05'in formülünü kullanır, uyarı zorunludur (A5-K108). **DS** = desteklenmiyor: verilirse reddedilir (A5-K102). Bütün `_ns` anahtarları ≥ 0; zaman sabitleri ve periyotlar > 0'dır (A5-K48).

| Tür (profil; `hiz_hz`) | Anahtar (birim; profil değeri) |
|---|---|
| `imu` (`icm42688p`; hız yok, kontrol periyodu) | `jiroskop_yogunluk` (rad/s/√Hz; 4,887e-5), `ivme_yogunluk` (dizi[3], m/s²/√Hz; 6,374e-4, 6,374e-4, 6,865e-4), `jiroskop_acilis_bias_siniri` (rad/s; 8,727e-3), `ivme_acilis_bias_siniri` (m/s²; 0,1961), `jiroskop_sicaklik_kats_siniri` (rad/s/°C; 8,727e-5), `ivme_sicaklik_kats_siniri` (m/s²/°C; 1,471e-3), `jiroskop_olcek_siniri` (0,005), `jiroskop_eksenler_arasi_siniri` (0,0125), `ivme_olcek_siniri` (0,005), `ivme_eksenler_arasi_siniri` (0,01), `jiroskop_dogrusalsizlik` (0,001), `ivme_dogrusalsizlik` (0), `jiroskop_gm_tau_ns` (1 000 s), `ivme_gm_tau_ns` (300 s), `jiroskop_gm_sigma` (rad/s; 4,848e-5), `ivme_gm_sigma` (m/s²; 1e-3), `jiroskop_aralik` (rad/s; 8,7266), `ivme_aralik` (m/s²; 39,2266), `bit` (16), `suzgec_etkin` (false), `suzgec_tau_ns` (yazılmaz; süzgeç etkinse zorunlu, A5-K46), `dunya_donusu_etkin` (true) |
| `manyetometre` (`rm3100`; 100 Hz) | `gurultu_sigma` (T; 2e-8), `yumusak_demir` (birim matris), `sert_demir` (T; 0, 0, 0), `sicaklik_kats` (T/°C; 0), `itici_girisim_orani` (0,2), `teslim_gecikmesi_ns` (1 ms) |
| `basinc` (`ms5837_30ba`; 25 Hz) | `gurultu_sigma` (Pa; 20), `mutlak_dogruluk_siniri` (Pa; 5 000), `sicaklik_kats` (Pa/°C; 0), `kuantalama` (Pa; 10), `donusum_penceresi_ns` (18,75 ms), `okuma_suresi_ns` (1 ms), `dinamik_basinc_kats` (0), `sicaklik_ofset_siniri` (°C; 4) |
| `gnss` (`neo_m9n`; 5 Hz) | `cep` (m; 2,0), `gm_tau_ns` (60 s), `gm_varyans_payi` (0,9), `dusey_carpan` (1,5), `hiz_sigma` (m/s; 0,05), `teslim_gecikmesi_ns` (120 ms), `uydu_sayisi` (20), `n_sv_ref` (12), `w50` (m; 0,030), `w_gecis` (m; 0,005), `t_kisa_ns` (10 s), `t_efemeris_ns` (2 saat; 05'in 2–4 saat aralığının alt ucu, mimar onayladı), `hizli_medyan_ns` (1 s), `sicak_medyan_ns` (2 s), `soguk_medyan_ns` (24 s), `t_edinim_ns` (0; KE), `yakinsama_baslangic_hatasi` (m; 0; KE), `yakinsama_tau_ns` (yazılmaz; `yakinsama_baslangic_hatasi > 0` ise zorunlu), `rapor_carpani` (1) |
| `dvl` (`a50`; hız yok, mod tablosu) | `isin_acisi` (rad; 0,39270), `yari_guc_genisligi` (rad; 0,07679), `azimut_ofseti` (rad; 0,78540), `alt_isin_sayisi` (7), `gelis_acisi_siniri` (rad; 1,04720), `sigma0` (m/s; 1e-3), `k_r` (1/s; 2e-4), `k_u` (1e-3), `uzun_donem_olcek_siniri` (0,0101), `kappa` (1), `c_ayar` (m/s; yazılmaz, KE: ortamın ses hızına eşit), `islem_suresi_ns` (5 ms), `uc_isinla_cozum` (false), `su_takibi` (false), `baglanti` (`seri`), `seri_baud` (115 200), `mod_histerezis_ping` (3), `kilit_arama_ping` (3), `yakin_mod_deneme_ns` (10 s) |
| `ping1d` (`ping1d`; hız yok) | `kutu_sayisi` (200), `ping_araligi_ns` (50 ms), `oto_menzil_carpani` (1,5), `islem_suresi_ns` (0; KE), `sogurma` (dB/m; DY), `algilama_esigi` (DS) |
| `ping360` (`ping360`; hız yok, kartın yapılandırması) | `baglanti` (`usb`), `baud` (yazılmaz, KE: iletim süresi 0), `t_m0_ns`, `t_grad_ns`, `islem_suresi_ns` (0; KE), `gurultu_tabani` (0; KE), `yakin_alan_genligi` (0; KE), `tvg_a` (dB; 20), `sogurma` (dB/m; DY), `kazanc_ofsetleri`, `l_min`, `l_maks` (DS) |
| `kamera` (`kamera_on`; hız yok, A8 kademesi) | `l_sonrasi_ns` (0; KE) |
| `batarya_olcer` (`psm`; 10 Hz) | `gerilim_bolucu` (11,0), `akim_ofseti` (V; 0,330), `akim_olcegi` (A/V; 37,8788), `analog_gurultu_sigma` (V; 2e-3), `adc_bit` (12), `adc_referans` (V; 3,3), `sicaklik_kats` (A/°C; 0), `teslim_gecikmesi_ns` (1 ms) |
| `sizinti` (`sos_leak`; 10 Hz) | `prob_sayisi` (2 [MV]), `algilama_gecikmesi_ns` (1 s), `sahte_pozitif_orani` (0) |
| `firlatici_geri_okuma` (`firlatici_io`; 10 Hz) | `anahtar_gecikmesi_ns` (10 ms), `teslim_gecikmesi_ns` (1 ms). Okunan alanlar: tüp başına dolu, emniyet açık, fiş takılı, pim çıkık; araç başına acil durdurma. "Bakım" alanı yoktur |

Profil değerlerinin kaynakları 05 §2.3–2.5'tir (etiketleri orada); bu tablo onları değiştirmez, yalnız sabitler.

### 3.7 `otopilot` (besler: 02 SIL koşucusu; HIL'de A1/A4 yoluyla)

| Anahtar | Tip | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|
| `otopilot.nominal` | dizgi | `fizik`, `acik` | `fizik` | E (tablo varsa) | A5-K51, K52 |
| `otopilot.parametreler.<AD>` | sayı ya da mantıksal | A4 tablosundaki aralık | `{}` | H | A4 parametre adı (`^(?!x_)[A-Za-z0-9_]{1,16}$`; 02 §3.6) → değer |

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
| `sahne` | dizgi | — | A7 kimliği | — | E | A7 sahnesi |
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
| `dalga.bilesen_sayisi` | tamsayı | — | 1–1024 | 64 | H | Bileşen sayısı (04 §2.5) |
| `akinti.hiz_ned`, `ruzgar.hiz_ned` | dizi[3] | m/s | rüzgârda d = 0 (A5-K39) | (0, 0, 0) | H | Düzgün akıntı / rüzgâr |
| `akinti.gauss_markov.*`, `ruzgar.gauss_markov.*` | `etkin` (false); `tau_ns` > 0; `sigma` ≥ 0 (m/s); `sinir` > 0 (m/s) | | | | K (A5-K38) | Sınırlı Gauss–Markov bileşeni |

### 3.10 `hedefler`, `gorev`, `olaylar`

**`hedefler.<ad>`** (en çok 64; besler 04 §2.8, 05, 07):

| Anahtar | Tip | Birim | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|---|
| `kimlik` | tamsayı | — | 0–65535, tekil | — | E | Hedef kimliği (04 isabet olayında `hedef_id`) |
| `sinif` | dizgi | — | `^[a-z][a-z0-9_]{0,31}$` | — | H | Senaryo etiketi; yalnız A6 ve değerlendirme |
| `varlik` | dizgi | — | A7 kimliği | — | E | Şekil, isabet bölgesi ve malzeme A7'dedir |
| `konum` | dizi[3] | m | — | — | E | Başlangıç, NED |
| `yonelim` | kuaterniyon | — | birim | — | E | Gövdeden NED'e |
| `yonelim_rastgele` | mantıksal | — | — | false | H | true ise başlangıç yaw'ı Ek D.4'teki çekilişle değiştirilir |
| `hareket.tur` | dizgi | — | `sabit`, `sabit_hiz`, `rastgele_yuruyus`, `sinus` | `sabit` | H | Kinematik hareket |
| `hareket.hiz_ned` | dizi[3] | m/s | — | — | K (`sabit_hiz`) | |
| `hareket.yuruyus_yogunlugu`, `hareket.azami_hiz` | sayı | m/s/√s, m/s | ≥ 0, > 0 | — | K (`rastgele_yuruyus`) | |
| `hareket.genlik_ned`, `hareket.periyot_ns`, `hareket.faz` | dizi[3], tamsayı, sayı | m, ns, rad | —, > 0, — | — | K (`sinus`; üçü de) | `konum + genlik · sin(2π t / periyot + faz)` |

**`gorev`** (besler 02 §2.6 yer istasyonu yolundan):

| Anahtar | Tip | Birim | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|---|
| `gorev.yukleme_zamani_ns` | tamsayı | ns | ≥ 0 | 0 | H | A5-K57 |
| `gorev.ogeler[i].tur` | dizgi | — | `WAYPOINT`, `DIVE_TO`, `SURFACE`, `HOLD`, `SCAN_LINES`, `SCAN_POINT`, `WAIT_GNSS`, `TARGET_ALIGN` | — | E | 02 §2.6 |
| `konum_ne` | dizi[2] | m | — | — | K | Kuzey, doğu |
| `derinlik` | sayı | m | ≥ 0 | — | K | 0 = yüzey |
| `hiz` | sayı | m/s | > 0 | — | K | |
| `kabul_yaricapi` | sayı | m | > 0 | — | K | |
| `sure_ns` | tamsayı | ns | ≥ 0 | — | K | `HOLD` süresi |
| `kose_a`, `kose_b` | dizi[2] | m | — | — | K | `SCAN_LINES` dikdörtgeninin karşı köşeleri |
| `hat_araligi` | sayı | m | > 0 | — | K | |
| `tur_sayisi` | tamsayı | — | 1–100 | — | K | `SCAN_POINT` |
| `zaman_asimi_ns` | tamsayı | ns | > 0 | — | H | Duvar saati (02 §2.6) |

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
| | `donma` | — | Yük, arıza başlamadan önceki son arızalı çıktıyla tekrarlanır; `t_meas` ve `seq` ilerler |
| | `donma_zaman_damgasi` | — | Yük, `t_meas` ve `window_ns` donar; `seq` ilerler |
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

**Bileşim.** Aynı kayda birden çok sensör arızası etkiliyorsa 05 §2.7'nin sırası uygulanır (ofset → ölçek → sürüklenme → gürültü → sıçrama → donma → yanlış geçerlilik → kendini bildiren → kayıp → geç teslim); aynı türden birden çok olay dosya sırasıyla. Zaman deseni: `surekli` her örnekte etkin; `bernoulli` her örnekte bağımsız `p` olasılıkla; `gilbert_elliott` sim zamanında iki durumlu süreç, kötü durumda etkin, geçiş her fizik adımında `1 − exp(−h/T)` olasılıkla (05 §2.7).

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
| `kalibrasyon.malzeme.<A7 malzeme kimliği>.akustik.*` | `geri_sacilim` (≥ 0), `gelis_yasasi` (`lambert_cos2`, `lambert_cos`), `spekuler_yansima` ([−1, 1]), `puruzluluk` ([0, 1]) | — | | A7 değeri | H | A7 varsayılanını geçersiz kılar |

### 3.12 `kalite` (iskelet §6)

| Anahtar | Tip | Aralık | Varsayılan | Zorunlu | Anlam |
|---|---|---|---|---|---|
| `kalite.gorunum` | dizgi | `kapali`, `sade`, `tam` | `sade` | H | 3B görünüm (A2) |
| `kalite.kamera` | dizgi | `kapali`, `sade`, `tam` | `sade` | H | Kamera sensörü; değerleri A8 |
| `kalite.sonar` | dizgi | `sade`, `tam` | `sade` | H | A5-K78, K79 |
| `kalite.kayit` | dizgi | `kapali` (= yalnızca olaylar), `sade` (= telemetri), `tam` | `sade` | H | A6 düzeyi |
| `kalite.kamera_kare_kaydi` | mantıksal | — | false | H | Kare kaydı ayrı bayrak |
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
- **A5-K11** Birimler 2. başlıktaki kabullere uyar; değerin yanında birim dizgisi yazılmaz.
- **A5-K102** "Desteklenmiyor" işaretli anahtar (`arac.batarya.sigorta_i2t`, `arac.batarya.hucre`, `arac.anten_konumu`, `baglanti.profiller.<ad>.telsiz`, `ping1d` `ayar.algilama_esigi`, `ping360` `ayar.kazanc_ofsetleri`, `ayar.l_min`, `ayar.l_maks`), desteklenmeyen batarya kipi (`b0`, `b1`; A5-K28) ve desteklenmeyen olay türü (3.10) verilirse senaryo aşama 2'de "bu sürümde desteklenmiyor" iletisiyle reddedilir. Bunların desteklenmesi minor sürümle eklenir.

### 4.2 Dosya düzeni, birleştirme, çözümleme ve özet

- **A5-K12** Senaryo dosyası tam olarak bir araç dosyasına `arac.dosya` ile başvurur. Yol, senaryo dosyasının dizinine göre göreli, `/` ayırıcılıdır; mutlak yol ve `..` bileşeni reddedilir. Başvurulan dosya yoksa ya da `meta.tur` `arac` değilse senaryo reddedilir. Araç dosyası başka dosyaya başvuramaz.
- **A5-K13** Öncelik sırası (düşükten yükseğe): bu sözleşmedeki varsayılan → araç dosyası → senaryonun `[arac]` alt ağacı. `otopilot` için: varsayılan → araç dosyası → senaryo. Malzeme için: A7 varsayılanı → `kalibrasyon.malzeme`.
- **A5-K14** Birleştirme tablolarda anahtar anahtar özyinelemelidir; yaprak değerler (diziler dahil) bütün olarak değiştirilir; diziler eleman eleman birleştirilmez.
- **A5-K15** Senaryodaki geçersiz kılma anahtarlı koleksiyonlara (`arac.iticiler`, `arac.firlaticilar`, `arac.sensorler`) araç dosyasında bulunmayan girdi ekleyemez ve girdi silemez (`x_` anahtarları girdi değildir, A5-K07). Bir sensörü kapatmanın tek yolu `etkin = false`'tur.
- **A5-K16** Çözümleyici şu sırayla tek bir çözümlenmiş senaryo üretir: (1) A5-K13/K14 birleştirmesi; (2) kaynak alanlarının çıkarılması (A5-K100); (3) eksik tabloların üretimi (A5-K99); (4) koşullu varsayılanlar (A5-K98) ve profil değerleri (3.6, A5-K101); (5) şemadaki koşulsuz `default` değerlerinin yazılması; (6) `otopilot` türetimi (A5-K51); (7) çözümleyici üst verisi (`meta.tur = "cozumlenmis_senaryo"`, `meta.kalibre_edilmedi`, `meta.dogrulanmamis_varsayimlar`, `meta.olcum_kanitlari`; listeler sözlük sırasıyla). Sonuç şemadaki `$defs/cozumlenmis_senaryo` tanımına uyar. Altın örnek: `scenarios/ornek-havuz.cozumlenmis.json`.
- **A5-K17** Kanonik biçim çözümlenmiş senaryonun RFC 8785 (JSON Canonicalization Scheme) dizilimidir: nesne anahtarları UTF-16 kod birimlerine göre sıralanır, belirteçler arasında boşluk yazılmaz, sayılar ECMAScript Number dizilimiyle yazılır (RFC 8785 §3.2; A5-K96 tablosu). **Davranış özeti** kanonik baytların SHA-256'sıdır (64 küçük onaltılık karakter); kaynak dosya özetleri bu nesneye girmez, A6'ya ve doğrulama raporuna ayrı üst veri olarak yazılır. Bu yüzden anahtar sırası, yorum, boşluk, tablo ya da satır içi tablo yazımı, sayıların yazılış biçimi ve açıkça yazılmış varsayılanlar davranış özetini değiştirmez; olayların dosya sırası anlamlıdır (A5-K62) ve özeti değiştirir. Çalıştırma A6'da davranış özetiyle tanımlanır. (RFC 8785 Informational yayındır; seçim bu sözleşmenindir.) Altın örneğin davranış özeti: `209f758fad29d6fe15a9087cfc2357b50f1b468f8e8254b1df8238e28cebf1d2`.
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
  | `olaylar[i].sure_ns` | geçici arıza ve yoksa | 0 |
  | `olaylar[i].zaman_deseni.tur`, `sira_boz`, `kanal_maskesi` | `sensor` sınıfı ve yoksa | `surekli`; false; 4294967295 |
- **A5-K99** Senaryoda olmayan tablolar çözümlenmiş biçimde şöyle üretilir: `hedefler` → `{}`; `olaylar` → `[]`; `emniyet_girdileri` → `{acil_durdurma: false, fis: {f1: false, f2: false}, pim: {f1: true, f2: true}}`; `baglanti` → `{etkin_profil: "ideal", profiller: {ideal: {kademe: "L0", tasima: "datagram", yukari: {gecikme_taban_ns: 0, titresim_ns: 0, kayip_modeli: "yok"}, asagi: (aynı)}}}`; `kalibrasyon` → `{malzeme: {}}`; `kalite` → bütün koşulsuz varsayılanlar; `baslangic` → bütün varsayılanlar; `otopilot` → `{nominal: "fizik", parametreler: {}}` ve A5-K51 türetimi; `gorev` → yazılmaz (görev yok). İç tablolar için de aynı ilke geçerlidir: varsayılanı olan her anahtar yazılır.
- **A5-K100** Kaynak alanları çözümlenmiş biçimde bulunmaz: `arac.dosya` çıkarılır; `meta.tur` `cozumlenmis_senaryo` olur; kaynak dosya özetleri yazılmaz (A5-K17).
- **A5-K18** `kalibrasyon.malzeme` anahtarı A7 manifestinde bulunmayan bir malzeme kimliğiyse senaryo reddedilir; geçersiz kılınan değerler çözümlenmiş senaryoda durur. Bilinmeyen malzemeye varsayılan verilmez (05 §4).
- **A5-K19** `arac.varlik`, `ortam.sahne` ve `hedefler.<ad>.varlik` A7 manifestinde aynı türde (varlık ya da sahne) bulunmuyorsa senaryo aşama 2'de reddedilir.

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
- **A5-K89** Sensör çerçevesi kabulleri (montaj yorumunda): IMU, manyetometre, basınç, GNSS, batarya ölçer, sızıntı ve fırlatıcı geri okuması için sensör eksenleri gövde eksenlerine paraleldir (birim yönelimde); DVL ve Ping1D ışın ekseni sensör +z'dir; Ping360 dönme ekseni sensör +z, tarama düzlemi sensör x–y'dir ve 0 gradyan sensör +x'tir; kamera optik ekseni sensör +x, görüntü sağı sensör +y'dir. Bu kabullerle İDA-0'ın bütün montaj yönelimleri birim dönüştür.
- **A5-K33** `kimlik` tekildir ve `(tur, inst)` çifti tekildir.
- **A5-K34** Yükleyici temas doğruluk ölçütünü (`ω_c h ≤ 0,2`; 04 §2.9) her kütle durumunda hesaplar; aşılırsa `uyari` üretir.

### 4.4 Ortam

- **A5-K35** Su yoğunluğu ve tuzluluğu A5-K98'e göre yazılır (04 §2.5; açık su değeri mimari varsayımıdır).
- **A5-K36** `ortam.tur = "havuz"` iken `ortam.derinlik`, A7 sahnesinin taban derinliğinden 1 mm'den fazla farklıysa senaryo reddedilir. `acik_su` iken `ortam.derinlik` yalnız dalga dispersiyonunun sabit `h`'sidir.
- **A5-K37** `dalga.spektrum` `yok` değilse `hs`, `tepe_periyodu_ns` ve `yon` zorunludur.
- **A5-K38** `gauss_markov.etkin = true` iken `tau_ns`, `sigma` ve `sinir` zorunludur.
- **A5-K39** `ortam.ruzgar.hiz_ned`'in d bileşeni 0'dan farklıysa reddedilir.

### 4.5 Sensörler

- **A5-K40** `ayar` altında verilmeyen anahtar 3.6'daki profil değeriyle çözümlenir; `yakinsama_baslangic_hatasi > 0` iken `yakinsama_tau_ns` zorunludur.
- **A5-K41** `imu`, `dvl`, `kamera`, `ping1d` ve `ping360` için `hiz_hz` verilirse reddedilir: IMU kontrol periyodunda örneklenir (iskelet §4, 200 Hz); DVL hızı mod tablosundan, kamera hızı A8 kademesinden, Ping1D ve Ping360 zamanlaması cihaz modelinden ve kartın yapılandırmasından gelir. `imu` için `saat_kaymasi_ppm` verilirse reddedilir.
- **A5-K42** `basinc` sensöründe `hiz_hz > 1 / (2 · donusum_penceresi + okuma_suresi)` ise reddedilir (profil değerleriyle 25,974026 Hz).
- **A5-K43** `hiz_hz` (0, 800] Hz aralığındadır; zamanlayıcı onu en yakın adım sınırı kuralıyla bağlar (04 §2.11).
- **A5-K44** Etkin bir `manyetometre` varken `ortam.manyetik_alan` yoksa reddedilir.
- **A5-K45** Etkin bir `gnss`, etkin bir `manyetometre` ya da `dunya_donusu_etkin = true` olan etkin bir `imu` varken `ortam.orijin` yoksa senaryo reddedilir (IMU'nun dünya dönüşü modeli enlemi kullanır, 05 §2.3.1).
- **A5-K46** `imu` ayarında `suzgec_etkin = true` iken `suzgec_tau_ns` zorunludur.
- **A5-K47** `dvl` `baglanti = "seri"` iken `seri_baud` kullanılır; `"tcp"` iken 05 §2.3.5'in TCP gecikme dağılımı kullanılır.
- **A5-K48** A5'te sonar menzili ve örnek sayısı anahtarı yoktur: menzil kartın yapılandırmasıdır (A1, 05 §2.4.5). Zaman sabitleri, periyotlar ve Gilbert–Elliott süreleri sıfır olamaz (şemada `exclusiveMinimum: 0`).
- **A5-K49** `etkin = false` sensör kayıt üretmez ve karta veri göndermez.
- **A5-K101** Profilde ölçülmüş ya da yayımlanmış değeri olmayan parametreler (3.6'da KE) ideal değerle çözümlenir: süreler 0, hata büyüklükleri 0, DVL `c_ayar` ortamın ses hızı, Ping360 `baud` yokken iletim süresi 0. Çözümleyici, kullanıcının vermediği ve etkin bir sensöre ait her KE anahtarının yolunu `meta.kalibre_edilmedi` listesine yazar; "yazılmaz" işaretli KE anahtarları (`c_ayar`, `baud`) çözümlenmiş biçimde bulunmaz ve anlamları bu kuraldaki ideal değerdir.
- **A5-K108** `ping1d` ya da `ping360` sensörü etkin ve `ayar.sogurma` verilmemişken soğurma 05 §2.2.1'in doğrulanmamış formülünden (tatlı suda yalnız saf su terimi) hesaplanır; çözümleyici her böyle yolu `meta.dogrulanmamis_varsayimlar` listesine yazar ve doğrulama raporunda her biri için `uyari` üretir (AB-089).

### 4.6 Otopilot

- **A5-K50** `arac` ile `otopilot` ayrı tablolardır; fizik değeri `otopilot`'a örtük olarak kopyalanmaz.
- **A5-K51** `otopilot.nominal = "fizik"` iken çözümleyici, A4 parametre tablosunda fizik kaynağı tanımlı her parametreyi çözümlenmiş `arac`'tan türetir ve `otopilot.parametreler`'e açıkça yazar; açıkça verilen değer türetileni geçersiz kılar ve "bilinçli model farkı" `uyari`'sı üretir. A4 tablosu yokken türetilen küme boştur.
- **A5-K52** `otopilot.nominal = "acik"` iken yalnız verilen değerler kullanılır. Her iki kipte de parametre adı A4 tablosunda yoksa ya da değer A4'teki aralığın dışındaysa senaryo aşama 2'de reddedilir.
- **A5-K53** SIL'de çözümlenmiş `otopilot` bloğu platform parametre bloğudur (02 §3.6). HIL kiplerinde A5 değerleri yalnız taşır; karttaki blokla eşitleme ya da farkın ön denetimde bildirilmesi A1/A4'ün kuralıdır.

### 4.7 Görev ve zaman çizelgesi

- **A5-K55** Görev öğesinde zorunlu alanlar: `WAYPOINT` → `konum_ne`, `derinlik`, `hiz`, `kabul_yaricapi`; `DIVE_TO` → `derinlik`; `HOLD` → `sure_ns`; `SCAN_LINES` → `kose_a`, `kose_b`, `derinlik`, `hat_araligi`, `hiz`; `SCAN_POINT` → `konum_ne`, `derinlik`, `tur_sayisi`; `SURFACE`, `WAIT_GNSS`, `TARGET_ALIGN` → yok. `zaman_asimi_ns` her öğede isteğe bağlıdır. Öğe türüne ait olmayan alan reddedilir.
- **A5-K56** Görev en çok 128 öğe taşır (02 §3.8).
- **A5-K57** Görev `yukleme_zamani_ns` anında GCS ağ geçidi → bağlantı emülatörü → A1 yolundan kartın görev komutlarıyla gönderilir; SIL ve HIL'de yol aynıdır.
- **A5-K109** Olay çeşitleri: **kalıcı atama** (`emniyet_girdisi`, `baglanti/profil_degisimi`, `sistem/su_girisi`), **anlık komut** (`gcs_komutu`, `sistem/kart_sifirlama`), **geçici arıza** (diğer bütün desteklenen türler). Kalıcı atama ve anlık komut `sure_ns` taşıyamaz; kalıcı atama değeri bir sonraki atamaya kadar kalır ("süreli" emniyet değişimi iki atamayla yazılır). `emniyet_girdisi` yalnız `zaman_ns` ile verilir (`kosul` yasak).
- **A5-K60** `olaylar[].kimlik` tekildir; `^E[0-9]+$` elle enjeksiyona ayrılmıştır ve reddedilir.
- **A5-K61** Her olay tam olarak birini taşır: `zaman_ns` ya da `kosul`.
- **A5-K62** `zaman_ns` taşıyan olay sim zamanı `t ≥ zaman_ns` olan ilk fizik adım sınırında başlar. Aynı adım sınırında önce bitişler, sonra başlangıçlar uygulanır; her grubun içinde dosyadaki sıra geçerlidir. Aynı kalıcı atama hedefine (aynı `girdi` ve `hedef`; aynı bağlantı profili ataması; aynı prob ataması) aynı adım sınırına düşen, farklı değerli iki atama doğrulama hatasıdır.
- **A5-K63** `kosul` taşıyan olayın koşulu her fizik adımı sonunda değerlendirilir; ilk sağlandığı adımı izleyen adım sınırında olay bir kez başlar ve yeniden kurulmaz; uygulama adımı A6'ya yazılır, tekrar oynatmada koşul yeniden değerlendirilmez (08 §2.2). Koşullu iki kalıcı atama aynı sınırda çelişirse dosyada sonraki geçerli olur ve çelişki A6'ya olay olarak yazılır.
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

- **A5-K78** `kalite` anahtarlarının varsayılanı `sade`'dir. Sonar `sade` kademesi ping başına 5 000 ışın kullanır (iskelet §6; MP7 S2) ve ışın sayısı A5'ten değiştirilmez. Ping360 örnek sayısının üst sınırı iki kademede de 1 200'dür (MP7'de ölçülen yapılandırma; mimar kararı); sade kademeyi sınırlayan ışın sayısıdır. Örnek üst sınırı A5 anahtarı değildir.
- **A5-K79** `kalite.sonar = "tam"` iken `kalite.sonar_tam.isin_sayisi` zorunludur. Kartlı kiplerde (`hil_1x`, `hil_bariyer`) ışın sayısı yalnız ölçülmüş kümeden seçilir: 20 000, 40 000, 80 000, 160 000 (MP7 §4.6; ölçümde gerçekleşen ışın sayıları 20 000, 40 014, 80 000, 160 008); başka değer reddedilir ve geçerli seçim `uyari` üretir. SIL'de 1 … 10⁷ serbesttir; kümede olmayan değer "ölçülmemiş ışın sayısı" `uyari`'sı üretir. Çözümleyici `meta.olcum_kanitlari`'na sonar kademesinin kanıtını yazar: sade → `docs/olcumler/MP7-sonar-suresi.md §4.3–4.4 (sade, N = 5 000)`; tam ve kümedeki değer → `docs/olcumler/MP7-sonar-suresi.md §4.6`; tam ve kümede olmayan değer → kanıt yazılmaz. Bu sınırlar ölçüm sınırıdır, donanım sınırı değildir; yeni ölçüm kümeyi minor sürümle genişletir.
- **A5-K80** `kalite.gorunum = "kapali"` iken `kalite.kamera` `kapali` değilse reddedilir: kamera karelerini görselleştirici süreci üretir (iskelet §8, A2), görünüm kapalıyken o süreç çalışmaz. `kalite.kamera = "kapali"` iken etkin bir `kamera` sensörü varsa reddedilir. `kalite.kamera` `kapali` değilken etkin kamera sensörü yoksa `uyari` üretilir.
- **A5-K81** Kalite kademesi emniyet girdilerini, emniyet olaylarını ve bu belgedeki emniyet kurallarını değiştirmez (iskelet §6).

### 4.10 Rastgelelik ve tekrar üretilebilirlik (Ek D: rastgelelik sözleşmesi sürüm 1)

- **A5-K82** `rastgelelik.ana_tohum` 0 … 2⁵³−1 aralığında bir tamsayıdır.
- **A5-K83** Üreteç Philox4x32-10'dur (Random123). Her alt akışın anahtarı `K = SHA-256("IDA-A5-RNG-1" ‖ 0x00 ‖ LE64(ana_tohum) ‖ UTF8(alt_sistem_adı) ‖ 0x00)`'dır; Philox anahtar sözcükleri `k0 = LE32(K[0..3])`, `k1 = LE32(K[4..7])`'dir; kalan baytlar kullanılmaz.
- **A5-K84** Alt sistem adları: `fizik/dalga`, `fizik/akinti`, `fizik/ruzgar`, `hedef/<ad>`, `sensor/<ad>/<akış>` (akış: `acilis`, `olcum`, `zamanlama`, `ariza`, `sonar_isin`, `sonar_hucre`), `ariza/<olay kimliği>` (elle enjeksiyonda `ariza/E<n>`), `baglanti/<profil>/<yukari|asagi>`, `protokol/<olay kimliği>`. Bir alt sistemin çekiliş sayısının değişmesi başka alt sistemin çekilişlerini değiştirmez.
- **A5-K110** Sayaç düzeni: `c0 = n mod 2³²`, `c1 = ⌊n / 2³²⌋` (`n` alt akış içindeki 64 bitlik sıra), `c2 = (kanal << 24) | (bilesen << 16) | cekilis` (8 + 8 + 16 bit), `c3 = 0` (sürüm 1'de ayrılmış). Bu sözleşmenin tanımladığı çekilişlerin `n` ve `c2` atamaları Ek D.4'tedir; model içi çekilişlerin atamaları 04 ve 05'in uygulama belgelerindedir ve aynı düzeni kullanır.
- **A5-K111** Dönüşümler: düzgün `u(x) = (x + 0,5) · 2⁻³²` (binary64'te tam, (0, 1) açık aralık); Bernoulli `u < p`; [0, m) tamsayı `⌊u · m⌋`; normal (Box–Muller) `r = sqrt(−2,0 · log(u_a))`, `t = 6,283185307179586 · u_b`, `z₀ = r · cos(t)`, `z₁ = r · sin(t)`; üstel `−log(u)`; karmaşık normal CN(0, 1) `sqrt(−log(u_a)) · (cos(t), sin(t))`. İşlem sırası budur, FMA kullanılmaz; `u_a`, `u_b` aynı çıktının ardışık sözcükleridir (0–1 ya da 2–3).
- **A5-K112** Philox sözcükleri ve düzgün dağılım her platformda bit düzeyinde aynıdır. `log`, `cos`, `sin` doğru yuvarlanmış olmadığından normal, üstel ve karmaşık normal dönüşümler için platformlar arası bit eşitliği garanti edilmez (sınır); aynı ikili ve platformda tekrar garanti edilir.
- **A5-K85** Aynı davranış özeti, aynı ikililer ve aynı platform SIL'de bit düzeyinde aynı fizik ve sensör izini üretir (Ç7); HIL kiplerinde güvence yalnız sim tarafı içindir ve kayıtlı uygulama adımlarıyla verilir (iskelet A6, Ç1). Platformlar arası eşdeğerlik toleranslıdır (04 §2.13).

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
| A7 kimliği, malzeme ya da havuz derinliği uyuşmuyor | aşama 2 | `K18`, `K19`, `K36` hata | Yükleme yok |
| A4 parametre adı ya da aralığı uyuşmuyor | aşama 2 | `K52` hata | Yükleme yok |
| Ön denetim tutmadı | aşama 3 | A1'in bulgusu, aynı biçimle | Çalıştırma başlamaz (06 §4) |
| Uyarı (dışdeğerleme, temas ölçütü, emniyeti azaltan atama, doğrulanmamış soğurma, tam sonar kartlı kipte, ölçülmemiş ışın sayısı, `x_` alanı, model farkı) | aşama 1–2 | `uyari` | Yüklenir; uyarılar A6'ya |
| Yeniden yüklenen çözümlenmiş senaryonun özeti uyuşmuyor | aşama 1 | `K02` hata | Yükleme yok |
| Koşullu iki atama çalışma anında çelişti | çalıştırma | — | Sonraki geçerli; A6'ya olay (A5-K63) |
| Koşullu olayın koşulu hiç sağlanmadı | çalıştırma sonu | — | Uygulanmamış olarak A6'ya; çalıştırma geçersiz sayılmaz |

---

## 6. Kalite kademelerinin etkisi

Kademe seçimi 3.12'deki anahtarlarla yapılır; etkisi iskelet §6 ve 05 §5'tedir. A5'e özgü etkiler: `sonar = sade` iken `kalite.sonar_tam` çözümlenmiş biçimde bulunmaz (A5-K98); kartlı kipte tam sonar yalnız ölçülmüş ışın sayılarıyla seçilir (A5-K79); `kayit` düzeyi tekrar oynatma güvencesini belirler (`kapali` → güvence yok, iskelet A6); `kamera = kapali` kamera sensörünün kapatılmasını, `gorunum = kapali` kameranın kapatılmasını gerektirir (A5-K80); emniyet kuralları kademeden bağımsızdır (A5-K81). Kamera kademe değerleri A8'de, görünüm kademe değerleri A2'dedir.

---

## 7. Uygunluk

| Kurallar | Sınama türü | Yöntem ve gözlenecek büyüklük |
|---|---|---|
| K01–K11, K96, K97, K102 | birim (doğrulayıcı) | Tablo güdümlü bozuk dosya takımı; her kural için en az bir dosya; beklenen `kural`, `asama`, `yol`. K96 için 4.1'deki sınır vektörlerinin her satırı |
| K12–K16, K98–K100 | birim + altın dosya | `ornek-havuz.toml`'un çözümlenmiş kanonik baytları `scenarios/ornek-havuz.cozumlenmis.json` ile bayt bayt eşit; her koşullu varsayılan satırı için koşulun iki tarafı |
| K17 | birim | Anahtar sırası, yorum, satır içi tablo, sayı yazımı (`3` / `3.0`, `0.30` / `3e-1`, `-0.0`) ve açıkça yazılmış varsayılanlar değiştirilmiş eşdeğer TOML aynı davranış özetini verir, kaynak özeti farklıdır; olay sırası değiştirilmiş dosya farklı özet verir; bir yaprağın değişmesi özeti değiştirir |
| K18, K19, K36 | sözleşme (A7 ile) | A7 örnek manifestiyle var olan / olmayan kimlik; taban derinliği farkı 0,5 mm ve 2 mm |
| K20–K34, K89, K104, K105 | birim (fizik yükleyici) | Atalet, `M` pozitif tanımlılığı, Ek A test vektörleri (trimli dahil; 1e-4 bağıl), `B` rankı, sıfır polinom, sıfır önde gelen payda, kararsız payda, DC kazancı 1 olmayan pay, norm toleransı 1e-6'nın iki yanı |
| K35–K49, K101, K108 | birim + SIL | Profil değerleri çözümlenmiş biçimde; basınç 25,9 Hz kabul, 26,0 Hz ret; orijinsiz senaryoda IMU dünya dönüşü açık/kapalı; `meta.kalibre_edilmedi` ve `meta.dogrulanmamis_varsayimlar` içerikleri; `etkin = false` sensörün A1'de kaydı yok |
| K50–K53 | sözleşme (A4 ile) + SIL | Türetilen blok; otopilot bloğunu değiştirmek fizik izini değiştirmez (aynı tohum, bit eşit) |
| K55–K57 | SIL | Görev öğeleri kartın görev yürütücüsüne aynı sırayla ulaşır |
| K60–K73, K109 | birim + SIL | Başlangıç ve bitiş adım sınırları; aynı sınırda bitişlerin başlangıçlardan önce uygulanması; çelişen atamanın reddi; her desteklenen türün Tablo 3.10-A etkisi; titreşimli geç teslimde `t_teslim' ≥ t_teslim`; desteklenmeyen türün reddi |
| K74–K77 | birim + SIL | Profil zorunlulukları; HIL'de SIL alanlarının reddi |
| K78–K81 | birim + SIL | Kartlı kipte 30 000 ışının reddi, 40 000'in kabulü ve kanıt kaydı; SIL'de ölçülmemiş değerin uyarısı; emniyet testleri her kademede aynı |
| K82–K85, K110–K112 | birim | Ek D'deki bilinen yanıt vektörleri (Philox sözcükleri bit eşit; normal değerler aynı platformda bit eşit); bir sensörün çekiliş sayısını değiştirmek diğer akışları değiştirmez; aynı özetle iki SIL koşusu bit eşit |
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
| 8 | Havuz derinliğinin A7 sahnesiyle eşlenmesi (A5-K36); örnek sahne `havuz_ornek` 25 m × 12,5 m × 3,0 m düz taban | A7 bekliyor | A7 örnek sahnesi |
| 9 | Optik malzeme, su optiği, kamera kalibrasyonu ve `L_alg` geçersiz kılmaları 1.0'da yok | A7/A8 bekliyor | Minor sürüm |
| 10 | 04'ün sayısal ayarları (nokta modeli eşikleri, temas uyarı eşiği, torpido süresi sınırı) A5'te yok | mimari seçimi | Ölçüm (M15) gerekirse minor sürüm |
| 11 | Kabul denetimleri (08 `meta.checks`) ve Monte Carlo tohum aileleri A5'te yok | açık | 09 test altyapısı |
| 12 | A4 parametre tablosu yokken `otopilot.parametreler` türetimi boş; A4 dondurulunca altın çözümlenmiş dosya ve davranış özeti değişir | A4 bekliyor | A4 sözleşmesi |
| 13 | MP7'nin gerçekleşen ışın sayıları (40 014, 160 008) izinli kümenin adlarından (40 000, 160 000) farklı; tam kademenin ışın örnekleme ızgarası ad ile gerçekleşen arasındaki eşlemeyi sabitlemeli | ölçülecek | Sonar modeli uygulaması (W4) |
| 14 | Normal ve üstel dönüşümlerin platformlar arası bit eşitliği yok (A5-K112) | sınır | — |
| 15 | `O_b` tanımı: mimar "çerçevenin geometrik merkezi" dedi, 04 "boş aracın ağırlık merkezi"; İDA-0'da ikisinin çakıştığı kabul edildi (`r_g = 0`) | mimari varsayım | Gerçek varlık (A7) gelince yeniden değerlendirme |

---

## 9. Açık bulgu kapanış tablosu

`docs/mimari/acik-bulgular.md` "A5 — Senaryo ve araç tanımı" başlığındaki on bir bulgu:

| Bulgu | Durum | Sözleşmede | Gerekçe |
|---|---|---|---|
| AB-085 | kapandı | A5-K27, A5-K06 | A5'te PWM kaybı seçimi yoktur; kullanım süresi dolunca nötr (iskelet §4), kullanım süresi A1'de |
| AB-086 | kapandı | 3.7; A5-K50, K51, K26 | Fizik gerçeği ve nominal model ayrı tablolardır; örtük kopya yok, sapmalar yalnız fiziğe |
| AB-087 | açık | 3.7 | Batarya eşikleri A4 parametreleridir ve `otopilot.parametreler` ile taşınır; değerleri A4 tablosunu ve mimarı bekliyor |
| AB-088 | açık | A5-K28, K102; 8. başlık md. 1 | Harita yapısı şemada; değerler aktarılmadığı için yalnız ideal kaynak destekleniyor |
| AB-089 | uygulamaya devredildi | A5-K108; 8. başlık md. 6 | Formülün geçerlilik alanının doğrulanması sensör modeli işidir (05 §2.4.2). A5 doğrulanmamış kullanımı görünür kılar: çözümlenmiş senaryoda `meta.dogrulanmamis_varsayimlar`, raporda zorunlu uyarı |
| AB-090 | kapandı | A5-K66, K48; şemada `exclusiveMinimum` | Titreşim gecikmeyi aşamaz; zaman sabitleri ve GE süreleri sıfırdan büyüktür |
| AB-091 | kapandı | A5-K40, K48, K101; şema `ayar` tabloları | Sensör geçersiz kılmaları türe göre şemada; süreç süreleri pozitif; sonar menzili kartın yapılandırması, örnek üst sınırı A5-K78 |
| AB-092 | kapandı | A5-K08; Ek B | Anahtarlar ASCII; ad eşlemesi dondurulmuş tabloda |
| AB-093 | açık | Tablo 3.10-A; A5-K102; 8. başlık md. 2 | Desteklenen aktüatör ve güç türlerinin (`itki_carpani`, `esc_sinyal_kaybi`, `ic_direnc_artisi`, `bara_kopmasi`) alanları ve etkileri tanımlandı; `tutukluk`, `dolanma`, `ani_cokme`, `dusuk_batarya` tanımlanamadı ve reddediliyor. 04'ün `FaultSpec` uygulaması ayrıca 04 işidir |
| AB-094 | açık | 3.11; Tablo 3.10-A; A5-K74, K75, K102 | Emülatör profil alanları ve desteklenen enjektör işlemleri tanımlandı; telsiz marj modeli, `gecikme_sicramasi` ve `cop_bayt` tanımlanamadı ve reddediliyor |
| AB-095 | kapandı | 3.7; A5-K51–K53 | Otopilot bloğunun yeri ve biçimi tanımlandı; ad tablosu ve `PARAM_*` dönüştürücüsü A4/A1'den (Ek C) |

Uyumsuzluk tablosunda A5'e değinen satırlar:

| Satır | Durum | Sözleşmede | Gerekçe |
|---|---|---|---|
| U-11 | A5 payı kapandı | A5-K27 | ESC sinyal kaybı seçimi kaldırıldı; komut ömrü A1'de |
| U-13 | A5 payı kapandı | 2. başlık; 3.2 | Sabit geometrik `O_b`, yük ayrılınca yerinde kalır; 02/05'in CG referansı A4'te |
| U-16 | A5 payı kapandı | 3.3; A5-K51, K104 | İtki eğrisi ve `B` tek A5 kaynağında |
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

## Ek C — Başka sözleşmelerden beklenenler

**A7'den beklenen.**
1. Kimlik biçimi: A5 `^[A-Za-z0-9_.:/-]{1,128}$` desenini geçici kabul eder; A7 kendi desenini sabitler, A5 minor sürümle uyar.
2. Manifestte varlık ve sahne türünün ayrımı (A5-K19); varlıktan `BUOY_*`, `COL_*`, `SON_*` ağlarına, itici ve fırlatıcı bağlantı düğümlerine erişim.
3. İDA-0 yer tutucu varlığı (`ida0`): 04 §2.6'daki yüzerlik kutusu (0,40 × 0,48 × 0,07135 m, merkez `r_b`) ve çarpışma kutusu (0,46 × 0,58 × 0,38 m, merkez `O_b`); sensör montaj noktaları 3.6 / `ida0.toml` ile tutarlı.
4. Örnek sahne `havuz_ornek`: 25 m × 12,5 m, düz taban, derinlik 3,0 m (A5-K36); örnek hedef varlığı `hedef_ornek` (şekil ve isabet bölgesi).
5. Malzeme kimlikleri ve akustik alan adları (`geri_sacilim`, `gelis_yasasi`, `spekuler_yansima`, `puruzluluk`); farklıysa eşleme tablosu.
6. Optik malzeme alanlarının listesi (8. başlık md. 9).

**A1'den beklenen.**
1. Sürekli komutun kullanım süresi (A5-K27) ve bekleme sınırı.
2. El sıkışmada `max_ping360_samples = 1200` (A5-K78; iki kademede aynı).
3. HIL'de `otopilot` bloğunun karta yüklenmesi ya da karşılaştırılması (A5-K53) ve ön denetim raporunun A5-K92 biçimiyle uyumu (aşama 3).
4. GCS ağ geçidinin `gcs_komutu` olaylarını ve görev yüklemesini taşıyan mesajları.
5. Protokol enjektörünün `duzlem = hil` ve `gcs` için uygulandığı katman ve `mesaj_turu` adları.
6. Algılama ve sensör kayıtlarında Tablo 3.10-A'nın değiştirdiği bayrak bitlerinin (`geçerli`, `sensör_arızası`) adları.

**A3'ten beklenen.** Doğrulama ucunun A5-K92 nesnesini değiştirmeden döndürmesi; formun şemadan üretilmesi; elle enjeksiyonlarda `E<n>` kimlikleri ve `ariza/E<n>` akışı.

**A4'ten beklenen.** Parametre tablosu: ad, tip, aralık, varsayılan, sınıf ve her parametre için fizik kaynağı (A5 anahtar yolu ya da "yok"; A5-K51); batarya eşikleri (AB-087); görev öğesi alanları (çelişirse A4 geçerli); komut ve kip adları.

**A6'dan beklenen.** Çözümlenmiş senaryonun kanonik baytlarının, davranış özetinin ve kaynak özetlerinin ayrı ayrı saklanması; uyarıların, koşullu olayların uygulama adımlarının ve çelişen koşullu atamaların yazılması.

**A8'den beklenen.** Kamera kademelerinin çözünürlük ve hız değerleri.

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

**D.4 Bu sözleşmenin tanımladığı çekilişler.**

| Çekiliş | Alt sistem | `n` | `c2` | Dönüşüm |
|---|---|---|---|---|
| Hedef başlangıç yaw'ı (`yonelim_rastgele`) | `hedef/<ad>` | 0 | 0 | `ψ = π · (2 u(w0) − 1)`; başlangıç yönelimi `q_z(ψ) ⊗ yonelim` |
| `gurultu_artisi` | `ariza/<kimlik>` | sensör kaydının `seq`'i | `kanal << 24` | `z` = `z0` (`w0`, `w1`) |
| `sicrama` | `ariza/<kimlik>` | `seq` | `(kanal << 24) | (1 << 16)` | olur: `u(w0) < p`; işaret: `w1 < 2³¹` ise +1, değilse −1 |
| `bernoulli` zaman deseni | `ariza/<kimlik>` | `seq` | `0xFF << 24` | etkin: `u(w0) < p` |
| `gilbert_elliott` zaman deseni | `ariza/<kimlik>` | fizik adım numarası | `0xFE << 24` | geçiş: `u(w0) < 1 − exp(−h/T)` |
| `gec_teslim` titreşimi | `ariza/<kimlik>` | `seq` | `(kanal << 24) | (2 << 16)` | `j = ⌊u(w0) · (2 · titresim + 1)⌋ − titresim` |
| Protokol işlemleri | `protokol/<kimlik>` | olayın yönündeki çerçeve sırası (0'dan) | 0 | olur: `u(w0) < p`; konum ya da uzunluk `⌊u(w1) · m⌋` (bayt bozmada `m = uzunluk`; kesmede uzunluk `1 + ⌊u(w1) · (uzunluk − 1)⌋`); XOR değeri `1 + ⌊u(w2) · 255⌋` |
| Bağlantı emülatörü kaybı | `baglanti/<profil>/<yon>` | çerçeve sırası | 0 | Bernoulli `u(w0) < p`; GE süreleri ters dağılımla `−log(u)/λ` (06 §2.11) |
