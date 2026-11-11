# 11 — Haberleşme Bağlantıları ve Güç Sistemi Modellemesi

> **Durum: ham araştırma raporu (yazıldığı haliyle korunuyor).** Bu rapor bağımsız doğrulamadan geçti ve bilinen hatalar içeriyor. Doğrulaması: [dogrulama/11-codex.md](dogrulama/11-codex.md). Düzeltmeler ve geçerli kararlar: [00-duzeltmeler-ve-kararlar.md](00-duzeltmeler-ve-kararlar.md). Çelişki halinde 00 numaralı belge geçerlidir.

> Kapsam: Hibrit tek insansız deniz aracı için HIL/SIL laboratuvarında (a) yer istasyonu–araç haberleşme bağlantısının ve (b) batarya, ESC, itici elektrik ve güç dağıtımının modellenmesi, ayrıca hata enjeksiyonu kataloğu.
> Etiketler: **[doğrulandı]** = bu oturumda birincil kaynaktan okundu veya hesaplandı; **[türetim]** = kaynaklı değerlerden hesaplandı; **[doğrulanmadı]** = kaynak okunamadı / tahmin / bellekten.
> Önceki raporlarla ilişki: T200 tablo modeli ve ArduSub failsafe listesi için 04 ve 06 raporlarına, modem ve batarya sensörü için 05 raporuna bakınız; burada tekrarlanmadı.

## 1. Özet

1. **Radyo suda pratikte yok sayılmalı.** Deniz suyunda (σ≈4 S/m varsayımıyla) iletken-ortam yaklaşımı 433 MHz'de ~700 dB/m, 2,4 GHz'de ~1700 dB/m verir **[türetim, yaklaşık üst sınır]**; yani anten sudan çıktığı anda bağlantı kopar, yeniden bağlanma yüzeye çıkış anında olur. Modelde "derinlik > anten yüksekliği (ıslak anten) ⇒ radyo kalitesi 0" ikili bir kural yeterli, dalga ile aralıklı ıslanma ise bir geçiş bölgesi olarak eklenir.
2. **Kablo (tether) yarışmada en yaygın "su altı bağlantısı".** Fathom-X HomePlugAV ile tek bükümlü çift üzerinden Ethernet taşır, ≥300 m çalışır **[doğrulandı, üretici GitHub]**; gecikme/kayıp değerleri yayımlanmamış. Model: sabit düşük gecikme + küçük patlamalı kayıp.
3. **Akustik modem çok yavaş ve gecikmeli:** Water Linked M64: 64 bit/s yarı çift yönlü, 200 m, tipik ~500 ms gecikme **[doğrulandı, ikincil kaynak + 05 raporu]**. Ses yayılma gecikmesi (≈ r/1500 m/s) ayrıca eklenmeli. Komuta-kontrol için değil, durum/abort mesajı için.
4. **MAVLink'te bağlantı kaybı kararı heartbeat'e dayanır:** heartbeat tipik 1 Hz, 4–5 kayıp = bağlantı kopuk (MAVLink önerisi); ArduSub `FS_GCS_TIMEOUT` varsayılanı 5 s; eylemler: warn / disarm / ALT_HOLD / SURFACE **[doğrulandı]**. Bu nedenle bağlantı emülatörü heartbeat'i gerçekten düşürebilmeli, yalnızca "ortalama kayıp" vermemeli.
5. **Bağlantı emülatörü HIL'de yer istasyonu ile kart arasında, bayt akışı (stream) düzeyinde durmalı;** gerçek UART/USB'nin üzerine gecikme kuyruğu + token-bucket bant sınırı + Gilbert–Elliott kayıp (çerçeve/paket düzeyinde, MAVLink çerçeve sınırlarını bilerek) bindirilir. Gerçek fiziksel bağlantının kendi gecikmesi ölçülüp çıkarılmalı.
6. **T200 elektrik verisi okundu ve doğrulandı.** Üreticinin XLSX dosyası bu oturumda ayrıştırıldı: 6 gerilim sayfası (10–20 V), 4 µs PWM adımı, RPM, akım, güç, kuvvet. 16 V'ta sıfır-itki aralığı 1472–1528 µs, 1900 µs'de 5,25 kgf @ 23,83 A, 1100 µs'de −4,07 kgf @ 24,3 A **[doğrulandı]**. 04 doğrulamasındaki "XLSX okunamadı" açığı kapandı.
7. **Batarya için 1-RC Thevenin + SoC–OCV tablosu + Coulomb sayımı + sıcaklığa bağlı R₀** başlangıç için yeterli; 2-RC ve yaşlanma sonraki kademe. Tipik küçük araç paket parametreleri için sağlam kaynaklı sayı bulunamadı (açık soru); hücre HPPC verisi veya kendi ölçümünüz gerekir.
8. **Gerilim çökmesi bağlaşımı kritik:** 4 iticili tam gaz akımı 100 A'i aşabilir (T200 başına 16 V'ta ~24 A **[doğrulandı]**), paket iç direnci × akım = gerilim düşümü → ESC/itki azalması ve kontrol kartında brownout riski. Basic ESC'de programlanabilir alçak-gerilim kesmesi yok (forum, personel yanıtı) **[ikincil]**.
9. **ArduSub batarya failsafe'i gerçek bir şablon:** `BATT_LOW_VOLT` varsayılan 10,5 V, 10 s boyunca eşik altı, ikinci katman `BATT_CRT_*`, eylem 0/1/2 (hiçbir şey/disarm/SURFACE) **[doğrulandı]**. Otopilotun ihtiyacı: gerilim, akım, tüketilen mAh, sag-düzeltmeli gerilim seçeneği.
10. **Hata enjeksiyonu iki katmanlı olmalı:** (a) bağlantı katmanı parametreleri (gecikme, kayıp, kesinti, bant), (b) güç katmanı parametreleri (SoC, R₀ çarpanı, ESC/motor arıza durumu, sensör ofseti). Kataloğu Bölüm 2.B.10'da.

## 2. Bulgular

### A. Haberleşme

#### A.1 Bağlantı türleri

| Bağlantı | Tipik veri hızı | Menzil | Gecikme | Kayıp davranışı | Kaynak/durum |
|---|---|---|---|---|---|
| Telemetri radyosu 868/915 MHz (RFD900x, SiK) | Hava hızı 64 kbit/s varsayılan; veri sayfalarında 224–500 kbit/s'ye kadar (kaynaklar çelişkili) | 40 km+ (hat görüşü, en iyi durum; düşük hızda); 500 m–1 km kapalı alan | **Sayı bulunamadı** [doğrulanmadı]; TDM + FEC ayarına bağlı | Menzil kenarında kademeli düşüş, FHSS ile patlamalı kayıp; hassasiyet −111 dBm @ 12 kbit/s | RFD900x veri sayfası (Bölüm 6, [K12]) |
| Wi-Fi 2,4/5 GHz | Onlarca Mbit/s | onlarca–yüzlerce m (anten/aralığa bağlı) | birkaç ms, yüksek jitter | RSSI'ye bağlı, patlamalı | Genel bilgi, **bu oturumda kaynaklanmadı** |
| 433 MHz telemetri (SiK 3DR sınıfı) | ~57,6 kbit/s seri, hava hızı 64 kbit/s | ~km | **doğrulanmadı** | — | RFD900x'e benzer, **doğrulanmadı** |
| Bükümlü çift Ethernet (Fathom-X, HomePlugAV/IEEE 1901) | 100 Mbit/s Ethernet (GitHub); "80 Mbit/s" (mağaza ilanı) — kaynaklar çelişkili | ≥300 m (Fathom tether); 200 m Slim tether | Yayımlanmamış [doğrulanmadı] | HomePlug, hat gürültüsüne göre hız adaptasyonu yapar [bilgi: standart gereği, doğrulanmadı] | [K1][K2] |
| Fathom-E (4 çift Ethernet) | Ethernet standardı | ≤100 m | düşük | — | [K1] |
| RS-485 / seri tether | 115,2 kbit/s–Mbit/s | yüzlerce m | düşük | Bit hataları, kesinti | **bu oturumda kaynaklanmadı** |
| Akustik modem Water Linked M64 | 64 bit/s, yarı çift yönlü, 8 bayt yük (05 raporu) | 200 m; 300 m derinlik | tipik ~500 ms (+ r/c) | Çok yollu, gürültü ve menzile bağlı paket kaybı | [K10][K11], 05 raporu |
| Akustik modem M16 | 10 bit/s, 1000 m, paket 1,6 s (05 raporu) | 1 km | — | — | 05 raporu |
| Yüzeye çıkan anten/şamandıra | Radyo bağlantısının hızı | Radyo menzili, anten yüksekliği ile kısıtlı | + şamandıra kablosu | Dalgalanma ile aralıklı | Mimari tasarım, kaynak yok |

Yarışma pratiği (kablolu/tam otonom): bu oturumda yarışma kuralı kaynağı toplanmadı **[doğrulanmadı]**. Genel pratik olarak (bilgi, kaynaksız): ROV sınıfında kablolu, otonom sınıfta bağlantı yok veya yüzeyde telemetri; kural metni yarışma duyurusundan alınmalı.

#### A.2 Radyonun suda sönümü

- İletken ortamda yatay dalga sayısı için kayıp (iyi iletken yaklaşımı): α = √(π f μ₀ σ) Np/m, dB/m = 8,686·α. σ = 4 S/m (deniz suyu, tipik) **[türetim]**:
  - 433 MHz → α ≈ 83 Np/m ≈ 720 dB/m
  - 915 MHz → α ≈ 120 Np/m ≈ 1040 dB/m
  - 2,4 GHz → α ≈ 195 Np/m ≈ 1690 dB/m
  - Not: 2,4 GHz'de σ/(ωε) ≈ 0,4 olduğu için "iyi iletken" yaklaşımı sınırda; gerçek değer farklı olabilir (aynı büyüklük mertebesi). Formül kaynağı bir forum yazısı; birincil kaynakla doğrulanmadı ([K5]). Doğrulama için ITU-R P.527 veya Debye modeli kullanılmalı.
- Ölçümlü destek: 2,4 GHz'de deniz suyunda iletişim menzili ~25 cm'ye düşüyor (tez, [K6]); 433 MHz'de yalıtımlı kablo üzerinde 32–47 dB/m [K7] (bu, çıplak deniz suyu değeri değil). PMC derlemesi radyonun su altında < 1 m taşıdığını özetliyor [K8].
- **Tatlı su (havuz) farklı:** iletkenlik çok daha düşük, kayıp çok daha az; ancak sayısal değer bu oturumda kaynaklanmadı **[doğrulanmadı]**. Havuz yarışmalarında anten 5–10 cm bile batsa 2,4 GHz kopabilir: yarışma ortamının iletkenliği (S/m) senaryo parametresi olmalı.
- **Sonuç (model kuralı):** Anten fazı merkezi suyun altına girdiği an (anten yüksekliği h_a − dalga yüksekliği − araç derinliği < 0) radyo bağlantısı kopar. Kopma süresi bir paket süresinden (ms) kısa; yani "bağlantı kademeli zayıflar" modeli gereksiz, bir geçiş bölgesi (±birkaç cm, dalga ıslanması) yeterli. Yeniden bağlanma: anten çıktıktan sonra radyo eşleşmesi (SiK/FHSS senkronizasyonu) sn mertebesi alabilir **[doğrulanmadı, tahmin]**; parametre olarak `reacquire_time` (varsayılan 1–3 s) konmalı ve ölçümle belirlenmeli.
- Dalga ve anten yüksekliği: deniz durumu dalga yüksekliği (04 raporu) ve araç trim açısı ile anten sudan çıkma olasılığı hesaplanır; anten kısa ise yüzeyde seyirde bile aralıklı kopma olur. Şamandıralı çözümde şamandıra kablo uzunluğu derinlik sınırıdır.

#### A.3 Bağlantı modelleri (kademeli)

Parametre kümesi (her bağlantı için): `latency_base`, `jitter` (dağılım), `bandwidth` (bit/s), `queue_limit`, `loss` (bağımsız ya da Gilbert–Elliott), `outage` (zamanlanmış/olaya bağlı), `quality(range, depth)`.

**Gilbert–Elliott:** iki durumlu Markov zinciri; G (iyi) durumunda doğru iletim olasılığı k (genelde 1), B (kötü) durumunda h; geçiş olasılıkları p(G→B), r(B→G) **[doğrulandı, Wikipedia "Burst error"]**. Ortalama kötü durum süresi 1/r, ortalama iyi durum süresi 1/p; kararlı hâl kayıp oranı = π_B·(1−h) + π_G·(1−k), π_B = p/(p+r). Paket başına bir kez ilerletilir (hesaplama maliyeti ihmal edilebilir).

| Kademe | Model | İçerik | Maliyet | Ne zaman |
|---|---|---|---|---|
| L0 | İdeal | Hiç gecikme/kayıp, yalnızca "bağlı/kopuk" bayrağı | ~0 | SIL başlangıcı |
| L1 | Sabit gecikme + bant sınırı + bağımsız (Bernoulli) kayıp + jitter | Yaygın "netem" benzeri | çok düşük | HIL ilk sürüm |
| L2 | L1 + Gilbert–Elliott patlamalı kayıp + `quality(range, depth)` + radyo için su altı ikili kural | Gerçekçi heartbeat kayıp davranışı | düşük | Hedef sürüm |
| L3 | Akustik: ray/çok yollu gecikme dağılımı, Doppler, yarı çift yönlü çakışma, SNR→paket kaybı eğrisi | Menzile bağlı hata, gürültü | orta | Yalnızca modem kullanılacaksa |

**Akustik kanal (L2–L3):**
- Yayılma gecikmesi = r/c, c ≈ 1500 m/s (ses hızı sıcaklık/tuzluluğa bağlı; 05 raporundaki ses hızı modeli kullanılabilir). 100 m için ≈ 67 ms; 200 m için ≈ 133 ms. Bu, paket süresine (M64: 8 bayt yük = 64 bit / 64 bit/s ≈ 1 s) göre küçük ama ses hızı/gidiş-dönüş protokollerinde etkili [türetim].
- Çok yollu yayılım: sığ suda doğrudan yol + yüzey ve dip yansımaları; gecikme yayılımı (delay spread) eşitleme yoksa simge süresinin alt sınırını belirler [K13]. Waymark modeli yol başına gecikme + kazanç ile dürtü yanıtı kurar [K14]. Pekeris kılavuzu basit sığ su modelidir [K13].
- Pratik hesaplı-ucuz yaklaşım **[öneri]:** paket başarı olasılığı = f(SNR), SNR = SL − TL(r) − NL; TL = 20·log₁₀(r) + α_a·r (küresel + soğurma), sığ suda silindirik yayılım için 10·log₁₀(r); havuzda yansıma sıçramaları bağımsız bir "çok yollu bozulma" olayı olarak eklenir (rastgele paket kaybı bursts). Parametreler M64 için yayımlanmış SNR eşiği bulunmadığından ölçüm gerektirir **[doğrulanmadı]**.

#### A.4 MAVLink ve yer istasyonu kaybı

- Heartbeat: bileşenler tipik 1 Hz yayınlar; sistem 4–5 ardışık heartbeat kaçırılınca bağlantı kopuk sayılır; MAVLink bu değerleri tanımlamaz, kanala bağlıdır **[doğrulandı, [K15]]**.
- ArduSub GCS failsafe: `FS_GCS_TIMEOUT` varsayılan 5 s; hiçbir GCS hiç bağlanmadıysa etkin değil; `FS_GCS_ENABLE`: 0 kapalı, 1 uyarı, 2 disarm, 3 ALT_HOLD, 4 SURFACE; failsafe sonrası heartbeat dönse bile mod otomatik geri dönmez **[doğrulandı, [K3]]**.
- Pilot girişi failsafe'i (RC veya MAVLink RC override kaybı): `FS_PILOT_INPUT` 0/1/2 (kapalı/uyarı/disarm), `FS_PILOT_TIMEOUT` varsayılanı sayfada belirtilmemiş; RC failsafe'i etkinse bu hiç tetiklenmez **[doğrulandı, [K9]]**.
- Copter için `FS_GCS_TIMEOUT` 5 s varsayılan, eylemler RTL/SmartRTL/Land/Brake [K4]; aracın hibrit olması nedeniyle yüzey modunda Rover/Boat tarzı bir davranış seçilmeli (kendi otopilotumuzun failsafe'i, 06 raporu).
- Düşük bant genişliğinde MAVLink: bant genişliği tasarımı/stream hızları (SR parametreleri) ve paket boyutu bu oturumda kaynaklanmadı **[doğrulanmadı]**. Hesap için: MAVLink v2 çerçeve ek yükü 12 bayt (başlık 10 + CRC 2, imzasız) **[bellekten, doğrulanmadı]**; ATTITUDE 28 bayt yük → ~40 bayt çerçeve. 64 kbit/s hava hızında (≈ 8 kB/s) ayrılan akışlar paylaşılmalı; M64'te (64 bit/s) MAVLink taşınamaz, özel kompakt şema gerekir.
- Otonom görev (AUTO) sırasında su altında GCS kaybı **beklenen** durumdur (06 raporu): failsafe'i GCS kaybına değil görev süresi/geofence'e bağla; yüzeye çıkınca GCS dönerse yeni komut kabulü kuralı gerekir.

#### A.5 Simülatör mimarisi: bağlantı emülatörü

```
Yer İstasyonu (laboratuvar uygulaması)
      │  TCP/UDP/seri (PC tarafı)
 ┌────▼────────────────────────┐
 │  Bağlantı Emülatörü (C++)   │  ← LinkModel (L0–L3), sim zamanı ile sürülür
 │  - bayt akışı → MAVLink     │
 │    çerçeve ayrıştırıcı      │
 │  - gecikme kuyruğu, token   │
 │    bucket, GE kayıp, kesinti│
 └────┬────────────────────────┘
      │ gerçek USB CDC / UART (HIL)  veya  sanal soket (SIL)
   STM32 kartı (otopilot)
```
- **Yer:** GCS ile kartın arasında, PC'de, simülatör sürecinin içinde (ya da ayrı iş parçacığında) bir proxy olarak. Hem HIL'de (gerçek seri port) hem SIL'de (soket) aynı `LinkModel` kullanılır; yalnızca "iç uç" farklıdır.
- **Zaman:** Gecikme/jitter/kesinti simülasyon zamanıyla (sabit adımlı fizik saati) sürülmeli, duvar saatiyle değil; aksi halde hızlandırılmış/yavaşlatılmış koşularda bağlantı davranışı bozulur. HIL'de zaman gerçek zamanlı olduğundan ikisi aynıdır; SIL hızlandırılmışsa sim zamanı zorunlu.
- **Gerçek bağlantının üstüne bindirme:** Kart ile PC arasındaki gerçek USB/UART bağlantısının kendi gecikmesi (USB CDC için ms mertebesi, **ölçülmeli**) çıkarılarak emülatör gecikmesi ayarlanır; yani `hedef_gecikme − ölçülen_taban`. Yalnızca eklenebilir, çıkarılamaz: taban gecikmeden düşük gecikme istenemez.
- **Kayıp uygulama birimi:** Bayt değil MAVLink çerçevesi (STX 0xFD, uzunluk alanı) düzeyinde düşür; yoksa çerçeve ortasında bayt kaybı CRC hatasına döner ve test edilen şey bağlantı kaybı değil çerçeve senkronizasyonu olur. İstenirse ayrı bir "bit hatası" kipi (CRC testi için) sunulur.
- **Radyo/akustik/kablo ayrı profil:** Her fiziksel bağlantı için ayrı `LinkProfile` (JSON/YAML) ve kalite fonksiyonu giriş olarak araç durumunu (menzil, derinlik, anten yüksekliği, dalga) alır. Aynı anda birden çok bağlantı (ör. telemetri + akustik) bağımsız örnekler.
- HIL mesaj kapsamı (DVL, sızıntı, batarya yok) için 06 raporunun özel ikili protokolü kullanılır; bağlantı emülatörü **yalnızca GCS–otopilot (MAVLink) yolunu** etkiler, sensör/aktüatör (HIL) yolu etkilenmez (aksi halde HIL bütünlüğü bozulur).

### B. Güç sistemi

#### B.6 Batarya modelleri

- **Thevenin eşdeğer devresi:** OCV(SoC, T) kaynağı + seri R₀ + n adet paralel RC; V_t = OCV − I·R₀ − Σ V_RCi. 1-RC'de R₀ pulsun anlık düşümünden, R₀+R₁ ~10 s noktasından (HPPC), C₁ ise zaman sabitinden bulunur **[doğrulandı, [K16] özeti]**. Parametreler SoC, sıcaklık, akıma göre değişir ve tablo/interpolasyonla verilir (X-57 çalışması 18650-30Q için 1-RC haritaları kullanıyor, [K17]).
- **Chen & Rincón-Mora (2006):** Thevenin tabanlı runtime + I–V modeli; raporlanan runtime hatası < %0,4, azami gerilim hatası 30 mV; kapasitenin akım, sıcaklık, çevrim sayısı ve saklama süresine bağlılığı modelleniyor (NiMH ve polimer Li-ion ile doğrulama) **[doğrulandı, özet; tam metindeki denklemler ve parametre değerleri bu oturumda okunamadı — PDF sıkıştırılmış çıktı verdi]** [K18].
- **Kaynaklı küçük-araç parametresi:** Samsung 30Q veya benzeri hücre/paket için R₀, R₁, C₁ tablosu bu oturumda **bulunamadı** [doğrulanmadı]. Öneri: (i) üretici hücre veri sayfasından DC-IR (genelde 1 kHz AC ve DC değerleri farklıdır) ve deşarj eğrisinden OCV(SoC) çıkar; (ii) paketin seri/paralel düzeninde ölçülen toplam iç direnç ve Kontrol Kartı'nın gördüğü gerilim–akım kayıtlarından 1-RC parametrelerini kendi paketinden uydur.
- **Sıcaklık/yaşlanma:** R₀ düşük sıcaklıkta belirgin artar (soğuk su!), kapasite azalır; X-57 ve Panasonic 18650PF çalışmaları −20…+25 °C'de parametre değişimini gösteriyor [K17 özeti]. Sayısal çarpanlar **doğrulanmadı**; model arayüzü: `R0(SoC, T) = R0_ref(SoC)·k_T(T)·k_age(N_cycle)`, `Q(T, age)`.

| Kademe | İçerik | Maliyet | Doğruluk beklentisi |
|---|---|---|---|
| B0 | V = OCV(SoC) − I·R₀ (sabit R₀), Coulomb sayımı | ihmal edilebilir | Failsafe eşik testi için yeterli; dinamik toparlanma yok |
| B1 | 1-RC Thevenin + OCV(SoC) tablosu + R₀(SoC) | düşük | Chen: ~30 mV sınıfı (kendi paketinde uydurulursa) |
| B2 | 2-RC + R(T) + kapasite(T, akım) + termal 1. derece | düşük-orta | Soğuk su ve darbeli yük (itici PWM) dinamikleri |
| B3 | Yaşlanma, hücre dengesizliği, BMS koruma kesmeleri | orta | Uzun görev ve arıza senaryoları |

- Batarya çıkışı iticilerle bağlaşık: toplam akım I = Σ I_i (T200 tablosu, aşağıda) → V_bus = OCV − I·R_toplam → her itici V'sine göre tabloda yeniden okunur (sabit-nokta iterasyonu veya bir adım gecikmeli). Tek adım gecikme (önceki adımın akımı) çoğunlukla yeterli ve kararlı; ESC tepki zamanı ms mertebesinden büyük olduğunda.

#### B.7 İtici ve ESC elektrik tarafı

**T200 (üretici XLSX'i bu oturumda ayrıştırıldı [doğrulandı, [K19]]):**

| V | Sıfır-itki PWM (µs) | 1100 µs: RPM / A / kgf | 1900 µs: RPM / A / kgf |
|---|---|---|---|
| 10 | 1460–1540 | 2662 / 13,6 / −2,31 | 2668 / 13,6 / +2,93 |
| 12 | 1464–1536 | 2976 / 17,0 / −2,90 | 2995 / 16,9 / +3,71 |
| 14 | 1468–1532 | 3233 / 20,7 / −3,52 | 3279 / 20,4 / +4,52 |
| 16 | 1472–1528 | 3465 / 24,3 / −4,07 | 3533 / 23,8 / +5,25 |
| 18 | 1472–1528 | 3643 / 28,2 / −4,59 | 3742 / 27,6 / +6,02 |
| 20 | 1476–1528 | 3783 / 32,2 / −5,04 | 3931 / 31,2 / +6,72 |

- Ürün sayfası: 12 V 17 A/205 W, 16 V 24 A/390 W, 20 V 32 A/645 W (tam gaz); önerilen 12–16 V, çalışma aralığı 7–20 V; sensörsüz fırçasız ESC şart [K20] **[doğrulandı]**. (Tablo değerleri sayfa değerleriyle tutarlı.)
- Yorum: Akım–PWM ilişkisi yaklaşık kuadratik/kübik; ölü bölgede akım 0 (tabloda 0 A, tüketim yok—ESC'nin boşta tüketimi tabloya dahil değil, **ayrı modellenmeli** [doğrulanmadı]). Reverse'de akım ileriye yakın ama itki daha düşük (verim farkı).
- **ESC (Blue Robotics Basic ESC / BESC30-R3) [K21, doğrulandı]:** 7–26 V (2–6S), 30 A sürekli (soğutmaya bağlı), sinyal 3,3–5 V, 1100 µs maks geri / 1500 µs dur / 1900 µs maks ileri, dur ölü bandı 1500 ± 25 µs, güncelleme hızı 400 Hz'e kadar, ısıl sınırlama: ≥140 °C'de %75, ≥145 °C'de %50, ≥150 °C'de %25, ≥155 °C'de %0 güç. Başlatma: birkaç saniye 1500 µs gönderilir, iki ton duyulur. Sayfada düşük-gerilim davranışı **yok**; forumda "Basic ESC'de programlanabilir alçak gerilim kesmesi yok" denmiş (ikincil, eski olabilir) [K22].
- Diğer protokoller (DShot, OneShot, UAVCAN/DroneCAN) bu oturumda araştırılmadı **[doğrulanmadı]**; PWM 1100–1900 µs hedef ESC için yeterli. Sim girişi: kartın PWM çıkışının zaman damgalı darbe genişliği (µs), 50–400 Hz.
- **ESC tepki dinamiği:** üretici yayımlamamış; 04 raporundaki 3. derece TF (baskın ≈76 ms) veya birinci derece devir dinamiği (T_n) kullanılır [04 raporu]. ESC'nin kendi rampa/sürücü gecikmesi **[doğrulanmadı]**.
- **Akım sınırı:** ESC 30 A; 20 V'ta T200 ~31–32 A çekebilir, sürekli çalışma sınırda (forum: ~30 A [K22]); tam gaz-ters-tam gaz geçişlerinde akım zirvesi ve ısınma modellenmeli. Basic ESC'nin aşırı-akım davranışı (kesme mi, kısma mı) **doğrulanmadı**.
- **Arıza kipleri (model önerisi, kaynak yok → tamamı [doğrulanmadı]):**
  - *Senkron kaybı / stall:* PWM var ama RPM→0, akım ESC sınırına kadar artar, ısıl kısıtlama, ESC bazen kapanıp yeniden başlatma (forumda "ESC turns off when motor stalls" başlığı var, içerik doğrulanmadı).
  - *Pervane dolanması:* yük torku artar, devir düşer, akım yükselir; itki ~0.
  - *Su alma (motor içi):* kısa devre/sızıntı akımı, ESC koruması, kalıcı kayıp; olay tabanlı.
  - *Kablo kopması/PWM kaybı:* ESC 1500 µs'e dönmeli mi, son değerde mi kalmalı? Basic ESC için **doğrulanmadı**; seçenek olarak parametre (`pwm_loss_behavior`).

#### B.8 Güç dağıtımı ve ölçüm

- Blue Robotics Power Sense Module: gerilim 11,0 V/V, akım 37,8788 A/V (0,330 V ofset), 100 A'e kadar (05 raporu, [doğrulandı]); ürün sayfası bu oturumda ayrıca alınamadı (404). Hata özellikleri (ofset, kazanç hatası, gürültü) ADC kuantizasyonu ile modellenir; sayısal doğruluk bildirilmedi **[doğrulanmadı]**.
- ArduSub batarya failsafe'i bir Power Module gerektirir [K23]. `BATT_FS_VOLTSRC` ham veya sag-düzeltmeli gerilim seçer [K23] — simülatörün gerilimi gerçekçi (yük altında çökmüş) vermesi bu seçimi test edilebilir kılar.
- **Brownout:** Kontrol kartı için düzenleyici (buck) giriş çökmesinde 3,3 V rayını düşürebilir; STM32U5 BOR eşikleri veri sayfasından alınmalı (bu oturumda **doğrulanmadı**). Kart, itici gücünden ayrı regülatörle beslenmeli; HIL'de kartın kendisi USB ile beslendiğinden **brownout fiziksel olarak oluşmaz**, bu yüzden sim, kartın görmesi gereken "batarya geriliminin çökmesi" sinyalini ve "kart resetlendi" olayını (yazılım tarafından kart yeniden başlatılması, GPIO reset) enjekte etmeli.
- **Sigorta/acil kesme:** Eylem: bus gerilimi 0'a düşer (sensör ve ESC'ler kaybolur), kart ayrı güç kaynağındaysa ayakta kalır. Model: `bus_connected` bayrağı (kill switch) → V_bus 0, itki 0; geri gelince ESC yeniden başlatma (1500 µs + birkaç sn) süresi. Sigorta atması: akım > eşik × süre (I²t) → kalıcı açık devre.

#### B.9 Enerji bütçesi ve batarya failsafe için gerekli ölçümler

- Enerji: E = ∫ V·I dt = Σ P_itici(t) + P_aviyonik + P_sensör. T200 tablo gücü doğrudan P(V, PWM) verir (örn. 16 V'ta tam gaz 390 W). Pratik görev profillerinde ortalama güç tam gazın küçük bir kesridir; verimli bölgede (örn. 1336 µs, 10 V: 15 W, −0,58 kgf) çok daha düşüktür **[türetim: tablo]**.
- Paket enerjisi örneği: 4S (14,8 V nominal) × 18 Ah ≈ 266 Wh (BlueROV2 sınıfı paket tipik değeri **bellekten, doğrulanmadı**; ürün sayfası 404 verdi). 4 itici × 100 W ortalama → ~35 dk, eğer sürekli olsaydı **[türetim; varsayımlara bağlı]**.
- Otopilotun ihtiyaçları: (1) paket gerilimi (sag'lı), (2) paket akımı, (3) tüketilen mAh/Wh (Coulomb), (4) mümkünse sıcaklık, (5) hücre başına gerilim (BMS varsa). ArduSub: düşük seviye `BATT_LOW_VOLT` 10,5 V (4S için 2,6 V/hücre — LiPo için fazla düşük; kullanıcı değeri ayarlamalı), 10 s eşik altı kalma süresi; mAh eşiği ~%20 kapasite önerisi; ikinci katman `BATT_CRT_*`; tetiklenince reboot'a kadar sıfırlanmaz [K23].
- Eşik önerileri (kendi tahminim, **doğrulanmadı**): uyarı ~%30, kritik ~%15 SoC; gerilim eşiği yük altında değil, düşük akımda tahmin edilmeli (sag düzeltmesi); failsafe tetiği SoC tahmincisi (Coulomb + OCV rölanti düzeltmesi) ile beslenmeli.

#### B.10 Hata enjeksiyonu kataloğu

| # | Arıza | Bağlantı/Güç | Sim temsili | Beklenen otopilot tepkisi |
|---|---|---|---|---|
| 1 | GCS bağlantı kaybı (tam) | Haberleşme | `outage(t0, süre)` → tüm çerçeveler düşer | `FS_GCS_TIMEOUT` sonrası eylem (HOLD/SURFACE/disarm) |
| 2 | Aralıklı bağlantı (radyo kenarı) | Haberleşme | GE: p, r, h parametreleri; heartbeat kaybı serileri | Eşiği geçmeden geçen/çevrilen failsafe; histerezis testi |
| 3 | Radyo derinlikte kopma | Haberleşme | anten sudaysa quality=0 (A.2) | Dalış sırasında beklenen; yüzeyde yeniden bağlanma |
| 4 | Yüksek gecikme/jitter | Haberleşme | `latency`, `jitter` artır | Komut gecikmesi, kontrol çevrimi etkilenmemeli (otopilot yerel) |
| 5 | Bant genişliği daralması | Haberleşme | token-bucket oranı düşür | Stream hızı düşer; kritik mesajlar önceliklenmeli |
| 6 | Akustik kanal bozulması | Haberleşme | SNR düşüşü, çok yollu burst | Paket kaybı, yeniden gönderim |
| 7 | Tether kopması / kablo gürültüsü | Haberleşme | Ethernet için outage veya hız düşüşü | ROV modunda failsafe |
| 8 | Çerçeve bozulması (CRC) | Haberleşme | bit-flip kipi | CRC reddi; sayaç artışı |
| 9 | Düşük batarya | Güç | SoC başlangıcını düşür veya `drain_rate` çarpanı | Uyarı → kritik → SURFACE |
| 10 | Batarya iç direnç artışı (soğuk/yaşlı) | Güç | R₀ × k | Aşırı gerilim çökmesi; sag-düzeltmeli eşik testi |
| 11 | Batarya ani çökmesi / hücre arızası | Güç | OCV kayması, kapasite kaybı | Hızlı failsafe |
| 12 | İtici arızası (stall, dolanma) | Güç | itki çarpanı 0, akım ESC sınırına | Tahsis yeniden düzenleme / SURFACE |
| 13 | ESC sinyal kaybı | Güç | ESC çıkışı son değerde veya nötr (parametre) | Failsafe motor çıkışı doğru mu |
| 14 | Kill switch / sigorta atması | Güç | `bus_connected=false` | Disarm algısı; yeniden başlatma |
| 15 | Brownout / kart resetlenmesi | Güç | kart GPIO reset, V_bus çökmesi sinyali | Reboot sonrası güvenli durum, arm kilidi |
| 16 | Akım/gerilim sensör hatası | Güç | ofset/kazanç kayması, takılma | Sensör sağlık denetimi |
| 17 | Sızıntı + kısa devre | Güç | leak olayı + `bus_connected=false` | SURFACE (zaten kesilmişse) |

Her arıza `{id, başlangıç koşulu (zaman/olay/konum), süre, şiddet parametresi}` ile tanımlanıp senaryo dosyasında (06 raporu SIL senaryoları) yer almalı.

## 3. Mimari için öneriler

1. **Bağlantı emülatörünü ilk günden ayrı bir bileşen olarak kur (C++, `LinkModel` arayüzü), L1 ile başla, L2'ye (Gilbert–Elliott + radyo-su altı kuralı) hızlıca geç.** Gerekçe: failsafe testleri heartbeat kaybının zaman yapısına (ardışık kayıp) duyarlıdır; Bernoulli kayıp bunu yansıtmaz. L3 (akustik çok yollu) ancak yarışmada modem kullanılacaksa.
2. **MAVLink çerçeve sınırı düzeyinde çalış, sim zamanıyla sür, HIL'de gerçek bağlantının taban gecikmesini ölç ve çıkar.**
3. **Güç tarafında B1 (1-RC Thevenin + tablo OCV) ile başla, B0'ı hemen sağla.** Gerekçe: failsafe testi için SoC, sag ve dinamik toparlanma yeterli; 2-RC/yaşlanma parametre verisi yokken yanıltıcı doğruluk üretir.
4. **T200 modelini doğrudan üreticinin tablosundan (PWM, V) üret.** Tablo bu oturumda ayrıştırıldı: 6 sayfa × 201 satır; birleştirip C/C++ başlığa gömülebilir. Akım toplamı batarya modeline bağlansın (bir adım gecikmeli).
5. **Kill switch / bus kesilmesi ve kart reseti ayrı "güç olayı" arayüzü** olsun; HIL'de brownout fiziksel olmadığından sim tarafında enjekte et.
6. **Arıza enjeksiyonu hem zamanlanmış hem olaya bağlı** (derinlik > x, SoC < y) olsun; senaryolar versiyonlanmış dosya olarak saklansın.
7. **Radyo `reacquire_time`, anten yüksekliği, iletkenlik (tatlı/tuzlu) ve ESC `pwm_loss_behavior` kalibrasyon parametreleri** olarak açıkça işaretlensin ve ölçümle doldurulsun.

## 4. Riskler ve açık sorular

- Radyo sönümü formülü forum kaynaklı, iyi-iletken yaklaşımı 2,4 GHz'de sınırda; tatlı su değeri bulunmadı.
- RFD900x gecikmesi, Fathom-X gecikme/kayıp, M64 SNR–paket kaybı eğrisi bulunamadı; HIL'de gerçek donanımla ölçmek gerekir.
- Batarya parametreleri (R₀, R₁, C₁, sıcaklık çarpanı) için kaynaklı küçük-araç değerleri yok; Chen & Rincón-Mora tam metni okunamadı.
- BlueROV2 batarya özellikleri doğrulanmadı (ürün sayfası 404).
- Basic ESC düşük gerilim ve aşırı akım davranışı belgelenmemiş; ESC değiştiğinde (BlueESC, farklı sürücü) davranış değişir.
- Yarışma kuralları (kablo/otonom zorunluluğu, bağlantı kısıtları) bilinmiyor.
- MAVLink bant genişliği hesapları kaynaksız; DroneCAN/DShot araştırılmadı.
- STM32U5 BOR/brownout eşikleri doğrulanmadı.
- Ses hızı ve akustik gürültü modeli 05 raporuyla birleştirilmeli.

## 5. Bağımsız doğrulanması gereken kritik iddialar

1. T200 16 V'ta sıfır-itki aralığı 1472–1528 µs; 1900 µs'de 5,25 kgf @ 23,83 A; 1100 µs'de −4,07 kgf @ 24,3 A — kaynak [K19] (XLSX, bu oturumda ayrıştırıldı); ayrıca [K20].
2. ArduSub `FS_GCS_TIMEOUT` varsayılan 5 s; `FS_GCS_ENABLE` 0–4 seçenekleri; failsafe sonrası mod geri dönmez — [K3].
3. ArduSub `BATT_LOW_VOLT` varsayılan 10,5 V, eşik altı >10 s, `BATT_FS_LOW_ACT` 0/1/2, tetiklendikten sonra reboot'a kadar sıfırlanmaz — [K23].
4. MAVLink heartbeat tipik 1 Hz, 4–5 kaçırma = kopuk — [K15].
5. Fathom-X HomePlugAV, 100 Mbit/s (GitHub) / 80 Mbit/s (ilan), ≥300 m — [K1][K2].
6. Basic ESC: 7–26 V, 30 A, 1100/1500/1900 µs, ±25 µs ölü band, ısıl sınır basamakları — [K21].
7. Water Linked M64: 64 bit/s, yarı çift yönlü, 200 m, tipik ~500 ms gecikme — [K10][K11].
8. Deniz suyunda radyo sönümü ~10²–10³ dB/m mertebesinde (433 MHz–2,4 GHz) — [K5][K6][K8]; hesap bu rapordaki türetim, ITU-R P.527 ile yeniden hesaplanmalı.
9. Gilbert–Elliott iki durumlu Markov, G'de doğru iletim olasılığı k (genelde 1), B'de h — [K24].
10. Chen & Rincón-Mora modeli runtime hatası <%0,4, azami gerilim hatası 30 mV — [K18].

## 6. Kaynaklar

- [K1] Fathom-X GitHub: https://github.com/bluerobotics/fathom-x
- [K2] Fathom-X ilan/arama sonuçları: https://bluerobotics.com/store/electronics/fathom-x-tether-interface-board/ ; https://www.robotshop.com/products/fathom-x-tether-interface-board-set
- [K3] ArduSub GCS Failsafe: https://ardupilot.org/sub/docs/gcs-failsafe.html
- [K4] ArduCopter GCS Failsafe: https://ardupilot.org/copter/docs/gcs-failsafe.html
- [K5] PhysicsForums (zayıf kaynak, formül): https://www.physicsforums.com/threads/how-is-radio-wave-propagation-modelled-in-seawater.824664
- [K6] Tez (2,4 GHz deniz suyu menzili): https://core.ac.uk/download/11805041.pdf
- [K7] Radioengineering 2018 (433 MHz yalıtımlı kablo): https://www.radioeng.cz/fulltexts/2018/18_01_0127_0133.pdf
- [K8] PMC su altı iletişimi derlemesi: https://pmc.ncbi.nlm.nih.gov/articles/PMC4934316
- [K9] ArduSub Pilot Input Failsafe: https://ardupilot.org/sub/docs/pilot-control-failsafe.html
- [K10] Water Linked M64 datasheet (okunamadı, arama özeti): https://bluerobotics.com/wp-content/uploads/2019/02/W-MK-19009-1_Modem-M64_Datasheet.pdf
- [K11] Water Linked M64 dokümanı / geo-matching: https://waterlinked.github.io/modem-m64/modem-m64 ; https://geo-matching.com/products/modem-m64
- [K12] RFD900x veri sayfası: https://cdn.hackaday.io/files/276031180966752/RFD900x DataSheet.pdf
- [K13] Al-Aboosi & Sha'ameri (Pekeris): https://ijeecs.iaescore.com/index.php/IJEECS/article/download/407/280 ; Falahati: https://ijeee.iust.ac.ir/article-1-40-en.pdf
- [K14] Waymark: https://www.york.ac.uk/media/physics-engineering-technology/research/engineering-research/commtech/communicationsystemsandprotocols/waymark_model_description_v2.pdf ; Morozs 2020: https://eprints.whiterose.ac.uk/165430/8/09146662.pdf
- [K15] MAVLink heartbeat: https://mavlink.io/en/services/heartbeat.html
- [K16] Thevenin model: https://www.batterydesign.net/modelling/thevenin-model/
- [K17] X-57 batarya modeli: https://openmdao.org/pubs/chin_battery_performance_x57_2019.pdf
- [K18] Chen & Rincón-Mora: https://rincon-mora.gatech.edu/publicat/jrnls/tec05_batt_mdl.pdf
- [K19] T200 performans verisi (XLSX): https://cad.bluerobotics.com/T200-Public-Performance-Data-10-20V-September-2019.xlsx
- [K20] T200 ürün sayfası: https://bluerobotics.com/store/thrusters/t100-t200-thrusters/t200-thruster-r2-rp/
- [K21] Basic ESC (BESC30-R3): https://bluerobotics.com/store/thrusters/speed-controllers/besc30-r3/
- [K22] Blue Robotics forum (düşük gerilim, 30 A): https://discuss.bluerobotics.com/t/low-voltage-cut-off/19236 ; https://discuss.bluerobotics.com/t/t200-with-5s-21v-lipo-battery/5518/2
- [K23] ArduSub Battery Failsafe: https://ardupilot.org/sub/docs/failsafe-battery.html ; Leak: https://ardupilot.org/sub/docs/internal-leak-failsafe.html
- [K24] Burst error / Gilbert–Elliott: https://en.wikipedia.org/wiki/Burst_error
