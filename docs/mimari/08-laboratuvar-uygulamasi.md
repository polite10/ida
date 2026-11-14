# 08 — Laboratuvar uygulaması

Sürüm 0.1 (2026-11-14). Durum: taslak; kullanıcı onayı yok.

Bu bölüm [00-iskelet.md](00-iskelet.md) (sürüm 0.7) Bölüm 9, satır 08'in kapsamını yazar: A3, web panelleri, A5 senaryo biçimi, hata enjeksiyonu, A6 kayıt ve tekrar oynatma, çalıştırma kontrolü. Dayanak [../research/00-duzeltmeler-ve-kararlar.md](../research/00-duzeltmeler-ve-kararlar.md) belgesidir ("00"). Atıf: `Kn`/`Ön`/`Çn`/`Mn` = 00'daki karar, öneri, çelişki, ölçüm; `NN.n` = 00 Bölüm 4 düzeltme satırı; `NN §x` = ham rapor bölümü; `0X §y` = bu dizindeki mimari bölüm. Ham rapor değerleri önce 00'daki düzeltmeyle karşılaştırıldı; 03 numaralı raporun Rust'a özgü önerileri (Rust tipinden JSON Schema, `mcap` Rust crate'i) kullanılmadı (K3). Bu bölümde ölçülmüş hız ya da yük rakamı yoktur; rakamlar `[T]` (tahmin) ya da kaynağın kendi hesabıdır.

---

## 1. Amaç ve sorumluluklar

Laboratuvar uygulaması, sim sunucusunun dışarıya bakan yüzüdür: operatörün koşuyu kurduğu, yönettiği, gözlediği, bozduğu ve sonradan incelediği yer. Fizik, sensör, protokol ve görsel model içermez.

1. **A3:** sim sunucusunun içindeki API bileşeni; WebSocket ikili telemetri akışı ve HTTP kontrol arayüzü (iskelet A3).
2. **Web panelleri** (K9): tarayıcıda çalışan harita, grafik, sonar, durum, atış kontrol göstergesi, senaryo, hata enjeksiyonu, kayıt panelleri.
3. **A5:** senaryo ve araç tanım dosyasının biçimi, sürümlemesi ve doğrulaması.
4. **Hata enjeksiyonu:** arıza kataloğunun ve zaman çizelgesinin sahibi; uygulama 04, 05, 06'nın arayüzlerindendir.
5. **A6:** MCAP kaydı ve iki anlamlı tekrar oynatma.
6. **Çalıştırma kontrolü:** çalıştırma durum makinesi, yardımcı süreçlerin (Godot, kamera algılama bileşeni) başlatılması (07 §2.1), kalite kademesinin uygulanması.

**Yazmadıkları:** modellerin içi (04, 05); A1 mesajlarının karta taşınması, bağlantı emülatörünün içi ve protokol hata enjektörü (06); 3B görünüm ve kamera hattı (07); test ve iş akışı (09). Beklentiler ilgili bölümlere "X'ten beklenen" olarak 8. başlıkta ve satır içinde yazılmıştır.

**Dayanak ilkeler:**
- **Kapsam kısıtı:** panel ve ön yüz kütüphaneleri öneridir (Ö6/Ö10: MapLibre GL JS, uPlot); web olması karardır (K9). Ayrı pencere yerleşimi Ö10'dur, teknik zorunluluk gibi sunulmaz (Ö10 dayanağı, D12 Ç).
- **Sim sunucusu tek yetkilidir.** Paneller durumu yalnız okur ve komutu A3'e verir; fizik ve zaman kararı panelde verilmez (iskelet §2).
- **Yavaş istemci simülasyonu yavaşlatamaz** (09 §2.7 "UI yavaşlarsa sunucu kare atar, HIL durmaz").

---

## 2. İç yapı

### 2.1 Süreçler ve yerleşim

```
 Sim sunucusu (tek süreç)
  ├ dünya iş parçacığı (04 §2.12) ── halka tampon ──┬─► API bileşeni (A3): yayın iş parçacığı + HTTP/WS G/Ç
  │                                                  ├─► kayıt yazıcısı (A6): tek iş parçacığı, MCAP
  │                                                  └─► (A2) görselleştirici yayını (07)
  ├ senaryo motoru ve hata dağıtıcısı (dünya iş parçacığının adım sınırında çağırdığı küçük kod)
  └ süreç gözetmeni: Godot ve kamera algılama bileşenini başlatır/izler (07 §2.1)
 Tarayıcı pencereleri (her panel ayrı pencere olabilir)  ◄── A3 ──►   Godot penceresi (3B, A2)
```

- **API bileşeni** sim sunucusunun içinde ayrı iş parçacıklarında koşar. Dünya iş parçacığı yalnız tek üretici–tek tüketici halka tamponuna yazar; API'yi, ağı ya da diski hiçbir koşulda beklemez (04 §2.12). Kitaplık seçimi açık: Boost.Beast ya da IXWebSocket (09 §2.7 önerisi; ikisi de bu oturumda ölçülmedi). Statik ön yüz dosyaları (`web/`) aynı sunucudan servis edilir; ayrı bir uygulama kabuğu ilk sürümde yoktur.
- **Ön yüz** TypeScript, harita MapLibre GL JS (çevrimdışı döşeme: tek dosya PMTiles, 03 §2.4), grafik uPlot (öneri; 09 §2.6). WebGL2 tabandır (MapLibre GL JS 6 WebGL1'i kaldırdı; 03.4). Web'de ikinci bir 3B sahne kurulmaz (Ç2, Ö5).
- **Paneller bağımsız pencerelerdir;** hepsi aynı A3 bağlantı kitaplığını kullanır. Her pencerenin üstünde ortak bir **durum şeridi** vardır: çalıştırma durumu, zaman kipi, sim zamanı, geçerlilik bayrağı ve kaçırılan adım sayısı. Böylece operatör hangi pencerede olursa olsun geçersiz çalıştırmayı görür (Ö11).
- **3B görünüm Godot'da kalır** (07). Paneller Godot'ya gömülmez; ikisi yalnızca sim zamanı ile ortak ilişkilidir (A2 ve A3 aynı `t_ns` etiketli snapshot'ı yayınlar). Panel ile Godot arasında doğrudan bağ yoktur; "3B pencereyi aç/kapat" komutu A3'ten gelir, süreç gözetmeni yürütür.
- **Çoklu istemci:** sınırsız değil, ama birden çok istemci eş zamanlı bağlanabilir (`[T]` bir elin sayısı; üst sınır M20'de ölçülür). Her istemci kendi aboneliğine sahiptir. Çalıştırmayı değiştiren komutlar tek bir **komut kuyruğundan** geçer; her komut `istemci_id` ve komut sırası taşır ve A6'ya yazılır. Durumu değiştiren komutlar için **kontrol sahipliği** (kiralama) önerilir: ilk bağlanan "operatör" olur, diğerleri izleyicidir; devir elle yapılır. Gerekçe: iki pencerenin aynı anda "sıfırla" demesinin tekrar üretilemez kayda yol açması (mimari öneri, karar değil).
- **Erişim koruması (basit, yerel):** sunucu yalnız döngü arabirimine (loopback) bağlanır; yerel ağa açma seçeneği bu aşamada yoktur. Her başlatmada rastgele bir erişim jetonu üretilir ve süreç gözetmeni ile açılan tarayıcı bağlantısına eklenir; jetonsuz HTTP/WS isteği reddedilir. `Host` ve `Origin` başlıkları denetlenir (tarayıcı sayfasından yapılan çapraz kaynak ve DNS yeniden bağlama saldırılarına karşı). Bu koruma yerel kötü niyetli sürece karşı değildir. **Yer istasyonu UDP uç noktası** (QGC, 06 §2.11) kimliksiz bir MAVLink komut kanalıdır (QGC `sysid 255` komutu karta ulaşır); varsayılan kapalıdır, açılınca yalnız loopback'e bağlanır ve durum şeridinde görünür.
- **Koordinatlar:** paneller dünya çerçevesini NED yerel düzlemde gösterir (iskelet §3.1); coğrafi altlık yalnızca senaryo `ortam.orijin` (enlem, boylam) verdiğinde açılır. Derinlik altlığı (batimetri) kendi dahili yüksek haritası biçimindedir; GEBCO ve EMODnet yalnız örnek "gerçek deniz" senaryolarına içe aktarılır (03 §2.4, 03.8).
- **Yer gerçeği ile kart görüşü ayrı katmandır.** Harita ve grafikler yer gerçeğini ve kartın kestirimini ayrı katman olarak çizer; yer gerçeği hiçbir yolla karta ya da kamera algılama bileşenine gitmez (07 §8). A3, yer gerçeği kanallarını "gerçek" etiketiyle ayırır.

### 2.2 Senaryo motoru ve hata dağıtıcısı

Senaryo motoru, dünya iş parçacığının **adım sınırında** çalışır (05: arıza "bir sonraki adım sınırında etkin"). Zaman çizelgesi öğeleri (arıza, emniyet girdisi değişimi, ortam olayı, görev) sim zamanıyla tetiklenir; koşula bağlı öğeler (ör. "derinlik > x", "görev öğesi i") `StepResult` ve snapshot üzerinden değerlendirilir. Koşul bir kez sağlanınca **çözümlenmiş uygulama adımı** A6'ya yazılır; tekrar oynatma koşulu yeniden değerlendirmez, o adımı kullanır (böylece koşul değerlendirmesi ile fizik arasındaki sıra duyarlılığı tekrarı bozmaz).

### 2.3 Zaman kipleri ve çalıştırma durum makinesi

Çalıştırma durumları: `BOŞ` → (senaryo yükle, doğrula) `YÜKLENDİ` → (ön denetim; HIL'de ayrıca 06 §2.9'daki `HAZIR`) `HAZIR` → `KOŞUYOR` ⇄ `DURAKLATILDI` → `BİTTİ` | `DURDURULDU` | `HATA`. `sıfırla` herhangi bir durumdan `YÜKLENDİ`'ye döner (aynı senaryo, aynı tohum; yeni `run_id`). Durum makinesi ile **geçerlilik** dikey iki ayrı eksendir: geçerlilik koşu içinde yalnız düşer (`geçerli` → `geçersiz`, nedenler listesi), bir kez düşünce o çalıştırmada geri dönmez (Ö11; 04 §2.11; 06 §4). Oturum alt durumları (`EL_SIKIŞMA`, `KOŞUYOR`, `KOPUK`; 06 §2.9) A3'te aynen gösterilir, 08 yeniden tanımlamaz.

| İşlem | SIL | HIL 1x | HIL 1x + bariyer |
|---|---|---|---|
| Duraklat / sürdür | Evet; sim zamanı durur, çekirdek ve duvar saati sim zamanından türediği için emniyet süreleri de durur (iskelet §4) | Teknik olarak mümkün; TF ve sensör akışı durur, kart IMU bayatlığını görür. Çalıştırma **geçersiz** olur; 08 duraklatmadan önce uyarır ve onay ister (06 §2.9) | Aynı |
| Hız çarpanı | Evet: yavaş, 1x, hızlı, "olabildiğince hızlı". Gerçekleşen çarpan ayrıca gösterilir; ağır modellerde istenen hıza ulaşılamayabilir (04 §2.11). Üst sınır ölçülmemiştir | Yok; yalnız 1x | Yok; yalnız 1x |
| Adımla | Evet. Birim **bir kontrol periyodu** önerilir (400 Hz'te iki fizik adımı); tek fizik adımı kontrol sınırında bölünürdü (açık nokta 8.2) | Yok (kart duvar saatiyle koşar) | Yok (kart yanıtını bekleyen bariyer adım sahibi değil; adımı kart değil sim ilerletir) |
| Çalışma sırasında değiştirilebilen | Arıza enjeksiyonu, sanal emniyet girdileri (onaylı), hız, duraklat | Arıza enjeksiyonu, emniyet girdileri (onaylı) | Aynı |
| Diğer her değişiklik | `sıfırla` ve yeniden yükleme gerekir | Aynı | Aynı |

Kartlı kiplerde "gerçek zaman" kart tarafının sınırıdır; sim tarafında gerçek zamanı kaçıran adım her kipte sayılır (iskelet §6). Kartsız kipte kaçırma tanısaldır, kartlı kipte çalıştırmayı geçersiz kılar (04 §2.11).

### 2.4 Paneller

Her panel tek pencere olabilir; ortak durum şeridi hepsinde vardır. Kullanılan kanallar §3.1'deki adlarla verilmiştir.

- **Harita ve görev planlama.** Aracın yer gerçeği izi, kart kestirimi, hedefler, sanal engeller, yol noktaları ve görev öğeleri NED yerel düzlemde (ve varsa coğrafi altlıkta) çizilir. Görev iki yoldan düzenlenir: *senaryo görevi* (`gorev` tablosuna yazılır, SIL'de yer istasyonu yolundan aynı komutlarla gelir, 02 §2.6) ve *çalışma anı yüklemesi* (A3 → GCS ağ geçidi → emülatör, 06 §2.11; bağlantı kaybı bunu da etkiler). Hibrit kipler ve atış komutları QGC'nin kapsamı dışında olduğu için bu panelin işidir (06 §2.11, 09.15). Kanallar: `durum`, `kontrol`, `yer_istasyonu`.
- **Canlı grafikler.** Seçilen kanalların zaman serileri (uPlot, öneri), pencere uzunluğu ve çizilen nokta sayısı istemcide sınırlanır; ham 400–800 Hz akış tarayıcıya gitmez, sunucu abonelik hızına indirir (özet yayını, ham veri MCAP'e; 09 §2.7). Yer gerçeği, sensör ve arızalı/arızasız temel kayıt karşılaştırılabilir (05 temel ve arızalı kayıt). Kanallar: `durum`, `sensör/*`, `güç`, `kontrol`.
- **Sonar görüntüsü.** Ping360 kutu dizilerini kutupsal tarama, Ping1D profilini zaman–menzil şeridi olarak çizer; satır zamanı darbe gönderim anıdır (iskelet §3.1). Veri A1'deki profilden (parçalama kart tarafında çözülür; panel kendi `sonar` kanalından, parçalanmamış alır). Kanal: `sonar`; sonar kademesi `sayaç` içinde.
- **Araç ve sistem durumu.** Araç durumu (kip, arm, derinlik, kütle durumu, güç, batarya), kart sağlığı, oturum durumu (`HAZIR`/`KOŞUYOR`/`KOPUK`), bağlantı emülatörü durumu ve ön denetim raporu. Kanallar: `durum`, `güç`, `kontrol`, `sayaç`, `süreç`.
- **Atış kontrol ve emniyet durumu.** Atış durum makinesinin durumu (03), `FIRE_AUTHORIZED`, sanal acil durdurma/fiş/pim ve fırlatıcı (dolu/boş) durumu, kartın geri okuması ile yetkili durumun yan yana gösterimi (fark = bozulmuş geri okuma işareti), olay onayları (uygulandı, yineleme, `REJECTED_SAFE`, `REJECTED_EXPIRED`), hedef takip kaydı ve atış değerlendirme raporu (03 §2.8 isabet sınıfı: temiz, temaslı, çarpma, ıska). **Yalnız gösterir;** emniyet girdisini değiştirme ayrı, onaylı bir eylemdir (§3.4); atış tetikleyen hiçbir denetim yoktur. Kanallar: `atış`, `olay`.
- **Senaryo.** Dosya listesi, form tabanlı editör (şemadan üretilir), kuru çalıştırma doğrulaması ve hata listesi (yol/satır), çözümlenmiş senaryo görüntüsü, yükle/sıfırla. Fizik gerçeği ve otopilot kümeleri ayrı sekmelerdedir (3.3). Kanallar yok; HTTP.
- **Hata enjeksiyonu.** Katalogdan seçim, parametre formu, anlık etkinleştirme, etkin arızalar listesi ve kaldırma; zamanlanmış arıza çizelgesi senaryodan salt okunur gösterilir. Kanallar: `arıza`, `olay`.
- **Kayıt ve tekrar oynatma.** Kayıt başlat/durdur, kademe ve hacim, `kayıt_düşen` ve "tekrar oynatma güvencesi" göstergesi, kayıt listesi ve üst bilgisi; seçilen kaydı izleme ya da yeniden koşturma olarak açma (2.6.2). Kanallar: `kayıt`, `olay`.
- **Gerçek zamanı kaçıran adım göstergesi** (Ö11). Durum şeridindeki kalıcı alan ve ayrıntı bölümü: `son_tarih_ihlali`, `azami_gecikme` ve gecikme dağılımı, `kaçırılan_adım`, bariyer sayaçları (`parti_eksik`, `bulk_drop`, `ping360_eksik`, geç olay, eş zamansız güvenli çıktı; 06 §4), geçersizlik nedenleri. HIL kiplerinde sıfırdan farklı değer kırmızıdır ve çalıştırma geçersizdir; SIL'de gri, tanısaldır (04 §2.11). Kanal: `sayaç`.

### 2.5 Hata enjeksiyonu

**Katalog** üç kaynaktan derlenir; 08 yalnızca kataloğu, zamanlamayı ve kaydı sahiplenir, uygulama ilgili bölümün arayüzündedir:

| Sınıf | Kaynak | Örnekler | Uygulandığı iç arayüz |
|---|---|---|---|
| Sensör arızaları | 05 §2.7, 11 §B.10 #16 | ofset, ölçek, sürüklenme, gürültü artışı, sıçrama, donma, yanlış geçerlilik, NaN, kayıp, geç teslim; durumlu: Ping360 motor takılması, DVL ışın kaybı, sensör güç çevrimi; geri okuma (fırlatıcı, emniyet) bozulması | `IFaultControl::arm/disarm/active` (05 §3), `SensorFaultSpec` |
| Aktüatör ve güç arızaları | 04 §2.7.5, 11 §B.10 #9–#14, #17 | itici itki çarpanı/tutukluk/dolanma, ESC sinyal kaybı ve PWM kaybı davranışı, düşük batarya, iç direnç, ani çökme, bara kopması | `World::apply_fault(FaultSpec)` (04 §3) |
| Haberleşme ve protokol | 11 §B.10 #1–#8, 06 §2.11–2.12 | GCS kaybı, aralıklı bağlantı (Gilbert–Elliott), gecikme/jitter, bant daralması, akustik bozulma, kablo kopması; protokol düzeyinde çerçeve düşürme/çoğaltma/bayt bozma | Emülatör profili (06 §2.11); `FaultInjector::configure` (06 §2.12). **İkisi ayrı profildir** (11.13): modelin kendisi ile protokolün sınanması karışmaz |
| Sistem olayları | 11 #15, 11.19, 05 §2.3.7 | kart sıfırlama (planlı), "su girişi" yer gerçeği olayı | Kart sıfırlama: platformun yeniden başlatma kancası (06 §2.9); su girişi: 05'in sızıntı yer gerçeği girdisi |

Kart sıfırlaması **sanal batarya çöküşünün, kart reseti ve gerçek besleme çöküşünün karşılığı değildir**; üçü ayrı testtir (11.19). Fiziksel USB üzerinde enjeksiyon yapılmaz (06 §2.12).

**Tek tanım şeması:** `ArızaTanımı {fault_id, sınıf, hedef (sensor_id/itici/bağlantı yönü), tür, başla {zaman | olay | koşul}, süre (0 = süresiz), şiddet{…}, zaman_deseni {sürekli | Bernoulli(p) | GE(T_iyi, T_kötü)}, kaynak {senaryo | elle}}`. Alt alanlar 04, 05, 06 şemalarına birebir eşlenir; 08 üç ayrı arayüzü bu tek şemanın arkasında dağıtır (arıza dağıtıcısı). Parametre denetimi `arm` anında ve doğrulamada yapılır (05 §2.7); reddedilen tanım olay olarak kaydedilir.

**Zamanlanmış ve elle:**
- *Zamanlanmış:* `arizalar[]` içinde sim zamanı ya da koşul; adım sınırında uygulanır (2.2).
- *Elle:* panelden `POST` ile gelir; sunucu isteği **bir sonraki adım sınırına** bağlar ve o adımı yanıtta döndürür. `fault_id` `E<n>` olarak koşu sayacından atanır. Elle enjeksiyon kontrol sahibi istemcidendir.
- *Kaldırma:* süresiz arıza `disarm` ile kalkar; durumlu arızalar bittikten sonra temel diziyle bit eşitliği beklenmez (05 §2.7).

**Kayıt ve tekrar üretilebilirlik.** Her enjeksiyon, kayıt kademesi ne olursa olsun `olay/arıza` kanalına yazılır: `{fault_id, kaynak, istemci, sınıf, tanım, uygulandığı_adım, sonuç (kabul/ret ve neden), bitiş_adımı, çözümlenmiş koşul adımı}`. Arıza RNG akışı sim zamanından değil `(tohum, alt_sistem, fault_id)` ile anahtarlanır (05 §2.6); elle arıza duvar saatinden etkilenmez, yalnız **hangi adımda uygulandığı** duvar saatine bağlıdır ve o adım kayıttadır. Dolayısıyla: aynı senaryo + tohum + kayıtlı elle enjeksiyon adımları = aynı arıza dizisi. Gerçek zamanlı kiplerde enjeksiyonun kendisi tekrar üretilebilir; kartın tepkisi ise kartın zamanlamasına bağlı olduğundan tekrar üretilebilirlik iddiası yalnız sim tarafı içindir (Ç1).

### 2.6 Kayıt ve tekrar oynatma

#### 2.6.1 Kayıt düzeyleri (iskelet §6, kayıt satırı)

| Kademe | Kaydedilen | Tekrar oynatma güvencesi |
|---|---|---|
| **Kapalı = "yalnızca olaylar"** | `olay/*`, `a3/komut`, ekler, koşu sonu sayaç özeti | **Yoktur.** Yalnız olay zaman çizelgesi izlenebilir (iskelet A6) |
| **Sade = "telemetri"** | Yukarıdakilere ek: `fizik/adim`, `hil/bariyer`, A1 `kart_sim` çerçeveleri ve düşürülen veriler, `fizik/durum`, `sensor/*`, yer gerçeği | Fizik yeniden koşturma (2.6.2 b) için gereken bütün girdiler vardır |
| **Tam = "telemetri, sonar, kamera"** | Ek: A1 `sim_kart` çerçevelerinin tamamı (sonar profilleri dahil), `sonar/*`, algılama kayıtları; kamera kareleri ayrı bayrakla | Kart ya da çekirdeğe girdi tekrarı (2.6.2 c) de mümkün |

Kamera kareleri sıkıştırılmamış 640 × 480 × 30 Hz'te 36,9 MB/s'dir (07, hesap), yani saatte yüzlerce GB; bu yüzden kare kaydı **ayrı bayrak ve olay çevresi penceresi** olarak tasarlanır, sıkıştırma yöntemi ve gerçek maliyet ölçüme bağlıdır (6. başlık). Ham iki yön A1 trafiği 06'ya göre ≈ 151–293 kB/s'dir (sıkıştırmasız tahmin; saatte ≈ 0,5–1 GB hesap).

#### 2.6.2 Tekrar oynatmanın iki anlamı (ve bir alt ayrımı)

**(a) Kaydı izlemek.** Sunucu kayıtlı kanalları aynı A3 çerçeveleriyle ve `kaynak = kayıt` bayrağıyla yayınlar; paneller canlıyla aynı kodla çalışır. Fizik koşmaz; oynat, duraklat, hız, arama (MCAP indeksi), döngü. İzlenebilen yalnızca kayıt kademesinin içerdiğidir. Düşük donanımda canlı çalıştırma ile aynı anda izleme önerilmez (tahmin).

**(b) Aynı girdilerle yeniden koşturmak (fizik).** Senaryo + tohum ile `World::reset`; kart yazılımı **yerine** kayıtlı `ActuatorFrame` ve tek seferlik olaylar **kayıtlı fizik adımında** uygulanır (iskelet A6 gereği: komutun uygulandığı fizik adımı, bariyer kararları, düşürülen veriler; 04 §2.13). Sonuç: aynı ikili ve platformda aynı fizik izi (hedef, Ç7; macOS-arm64 ile Windows-x64 arasında yalnız toleranslı eşdeğerlik). Kayıtlı arıza adımları ve elle enjeksiyonlar aynı biçimde uygulanır. **Senaryo, tohum ya da ikili değişirse kayıtlı komutlar açık döngü olur ve sonuç farklı çıkar;** bu bir hata değil, beklenen davranıştır, ama "tekrar" sayılmaz. Bu düzey en az "telemetri" kademesi gerektirir.

**(c) Girdi tekrarı** (alt ayrım): kayıtlı `sim_kart` çerçevelerini SIL çekirdeğine ya da karta besleyip kartın davranışını incelemek. Bu, fizik koşturmaz ve yalnız "tam" kademede mümkündür, çünkü sonar profilleri ve algılama kayıtları ancak orada kayıtlıdır. SIL'de bit düzeyinde eşitlik beklenir (03 §7); karttaki koşuda yalnız toleranslı karşılaştırma (09.5).

**Sonar tekrarının sınırı:** (b) düzeyinde sonar profilleri kayıttan değil tohum ve girdilerden yeniden üretilir (05 §2.6, 04 §2.12'deki sabit sıralı tüketim). Embree sonuçlarının platformlar arası ya da aynı platformda sürümler arası bit eşitliği garantisi bulunamadı (Ö4 dayanağı, D10 #17); aynı ikili ve platformda eşitlik de M11/M15 ile doğrulanmadı. Bu yüzden telemetri düzeyinde sonar kaynaklı algılama farkı **doğrulanmadan güvence sayılmaz**.

**HIL'de.** Bariyerli ve bariyersiz HIL kaydı (b) için fizik tekrarı verir; kartın tepkisi tekrar üretilmez. Bariyersiz HIL'de komutun uygulandığı adım kayıttan gelir, kartın zamanlaması yeniden üretilmez (Ç1). Kayıt "geçersiz" çalıştırmadan geldiyse (kaçırılan adım, planlanmamış duraklatma) kaydın üst bilgisi bunu taşır ve yeniden koşturmada uyarı çıkar.

#### 2.6.3 Kayıt yönetimi

Kayıtlar `kayitlar/` (derleme çıktısı gibi depoya girmez, `.gitignore`) altındadır. Kayıt listesi `run_id`, senaryo özeti, kademe, geçerlilik, süre ve boyutu gösterir; silme açık onaylıdır. Bir kaydın çözümlenmiş senaryosu tek tıkla yeniden yüklenebilir.

---

## 3. Arayüzler

İmza ve alan listesi düzeyinde; sözleşme ayrı işte dondurulur (iskelet §3).

### 3.1 A3 telemetri akışı (WebSocket, ikili)

- **Çerçeve:** `{şema_sürümü u16, kanal_id u16, sıra u32, t_ns i64 (sim, ölçüm zamanı), kaynak_bayrağı (canlı|kayıt), yük}`. Sabit genişlikli, küçük uçlu, **açık serileştirme**; ham `struct` kopyası yok (00 Ç3'teki ders). Kodlayıcı/çözücü (C++ ve TypeScript) tek şema dosyasından üretilir, 06'nın biçim sözleşmesiyle aynı yöntem (06 yedek biçim). Kontrol ve abonelik iletileri metin (JSON) çerçevedir.
- **Kanallar** (liste düzeyi): `durum` (araç pozu, hız, tutum; yer gerçeği ve kart kestirimi ayrı), `sensör/<id>`, `kontrol` (kip, görev, aktüatör çıkışı, kart sağlığı), `güç`, `sonar` (satır ya da profil; zaman damgası darbe gönderim anı, iskelet §3.1), `atış` (durum makinesi, emniyet girdileri, fırlatıcı, olay onayları, değerlendirme), `sayaç` (kaçırılan adım, bariyer, `bulk_drop`, geçerlilik nedenleri, kayıt taşması), `olay` (mod geçişleri, arıza, kayıt, oturum), `arıza` (etkin arıza listesi), `yer_istasyonu` (06 §2.11 ağ geçidi), `kayıt` (durum, hacim), `süreç` (Godot, algılama bileşeni).
- **Abonelik:** istemci `{kanal, azami_hız_hz, alan_alt_kümesi}` ister. Yayın görevi 60 Hz'tir (04 §2.11 zamanlayıcı tablosu); hızlar yalnız bunun bölenleri olabilir. Abone olunmayan kanal serileştirilmez ve kuyruğa girmez.
- **Hız düşürme ve geri basınç** (iki sınıf): (a) *durum sınıfı* kanallar (poz, sensör, sonar, güç) istemci başına "son değer" kuralıyla çalışır: yavaş istemci için eski kare atılır, yeni kare gönderilir; (b) *olay sınıfı* kanallar (`olay`, `sayaç`, `atış` olay onayları, `arıza`) sırayla ve düşmeden gider, istemci başına sınırlı kuyrukla. Kuyruk taşan istemci kopar; yeniden bağlanınca sunucu **tam durum özeti** gönderir (yeniden eşleme) ve olaylar `sıra` boşluğuyla kayıttan tamamlanabilir. Kuyruk sınırı ve yayın CPU payı M20'de belirlenir. Hiçbir istemci dünya iş parçacığına geri basınç uygulayamaz.
- **Gerçek zamanlı olmayan veriler** (senaryo dosyaları, kayıt listesi, kayıt indirme) HTTP ile gider; WebSocket yalnız akıştır.

### 3.2 A3 kontrol (HTTP, JSON, sürümlü: `/api/v1`)

| Grup | İşlemler |
|---|---|
| Senaryo | listele, getir, doğrula (kuru çalıştırma), yükle, kaydet (form editörü), çözümlenmiş halini getir |
| Çalıştırma | başlat, duraklat, sürdür, adımla(n), hız, durdur, sıfırla; HIL'de "duraklatma geçersiz kılar" onayı |
| Durum | çalıştırma durumu, geçerlilik nedenleri, sayaçlar, kademe, süreçler, sürümler |
| Hata enjeksiyonu | kataloğu getir, etkinleştir (`ArızaTanımı`), kaldır, etkinleri listele |
| Sanal emniyet girdileri | durumu oku; değiştir (iki adımlı onaylı, §3.4) |
| Yer istasyonu | operatör komutları (mod, arm, görev yükleme) 06 §2.11 ağ geçidi üzerinden |
| Kayıt | başlat/durdur, listele, meta bilgi, indir, sil, tekrar oynatma başlat (izleme) ya da yeniden koşturma (2.6.2) |
| Süreç | 3B görselleştiriciyi başlat/durdur, algılama bileşeni durumu |

**Yoktur:** atış tetikleyen bir uç. `ateşle` olayı yalnız karttan gelir (06 §2.4, A1); A3 atışı başlatamaz, yalnız yetki ve emniyet girdilerini yönetir. Yanıt sözleşmesi: her kontrol çağrısı kabul ya da ret + neden döner; ret de A6'ya yazılır.

### 3.3 A5 senaryo ve araç tanımı

TOML dosyası (iskelet A5; 03 §2.6). Üst düzey tablolar, 04 ve 05'in A5'ten beklediği gruplarla birleşik liste (alan ayrıntıları sözleşme aşamasında):

| Tablo | İçerik | Kaynak beklenti |
|---|---|---|
| `meta` | `schema_version` (major.minor), ad, açıklama, süre, çalıştırma kipi (SIL/HIL/bariyer), kabul denetimleri (`checks`) | 03 §2.6 |
| `arac` | İDA-0 parametreleri: gövde, `O_b`, kütle durumları, hidrodinamik, `itici[8]`, `esc`, `batarya`, `torpido`, `firlatici[2]`, belirsizlik/sapma | 04 §3 |
| `ortam` | orijin, derinlik/havuz, `dalga`, `akinti`, `ruzgar`, `temas`, su özellikleri | 04 §3 |
| `hedefler[]` | hedef sınıfı, hareket, isabet bölgesi | 03 §2.8 |
| `gorev` | görev öğeleri (02 `MISSION_*`, özel öğeler `MAV_CMD_USER_*`) | 02 §2.6 |
| `sensorler[]` | `sensor_id`, tür, `inst`, ürün profili, montaj (`O_b`'ye göre), hız, bağlantı profili, etkin | 05 §3 |
| `baglanti` | yön başına emülatör profili L0–L2 (gecikme, bant, kayıp, kesinti, telsiz marjı), protokol enjektörü profili | 06 §2.11–2.12 |
| `arizalar[]` | arıza zaman çizelgesi (2.5) | 04, 05, 06 |
| `emniyet_girdileri` | sanal acil durdurma, fiş, pim için başlangıç durumu ve zaman çizelgesi | 03 §2.7, 05 §3 |
| `kalibrasyon` | A7 varsayılanlarını geçersiz kılan akustik/optik değerler, `su_optigi`, `kamera[]`, `L_alg` | iskelet A7, 07 |
| `tohum` | ana tohum `u64`; alt akışlar 04/05'in Philox anahtarlarından türer | 04 §2.13, 05 §2.6 |
| `kalite` | özellik bazlı kademe (5. başlık) ve kayıt düzeyi; HIL geçersizlik eşikleri (04 §2.11) | iskelet §6 |
| `otopilot` | kartın bildiği parametre kümesi (aşağıda) | 02 §3.6, 02 §8.2 md.1 |

**Fizik gerçeği ile otopilot parametreleri ayrıdır.** `arac` ve `ortam` fizik gerçeğidir (yalnız sim okur); `otopilot` kartın nominal modelidir (itki eğrisi tersi, tahsis matrisi `B`, kazançlar, kestirim parametreleri; SIL'de platform bloğu, kartta flash; 02 §3.6). Kurallar: (1) ayrıştırıcı fizik değerini otopilot kümesine örtük kopyalamaz; (2) varsayılan `otopilot.nominal = "fizik"` yazılırsa kopya **çözümlenmiş senaryoda açıkça yazılır** ve A6'ya girer; (3) `arac.belirsizlik` ve sapmalar yalnız fizik tarafına uygulanır; böylece model hatasına dayanıklılık sınanabilir (02 §8.2 md.1).

**Sürümleme ve doğrulama:**
- `schema_version` major uyuşmazlığı ret; minor yeni isteğe bağlı alan ekler. **Bilinmeyen anahtar ret'tir** (yazım hatası sessiz geçmesin); deneysel alanlar `x_` önekiyle uyarıyla geçer.
- Anahtarlar **ASCII**'dir (TOML 1.0 çıplak anahtarları ASCII ile sınırlar; belirtim bu oturumda yeniden açılmadı). Belgelerdeki Türkçe alan adları (ör. `akıntı`) dosyada `akinti`'dir; uyumsuzluk 8.2'de.
- Doğrulama üç aşamadır: **(1) sözdizimi ve şema** (tip, aralık, SI birim zorunlu; birim eki yok); **(2) anlamsal** (başvurulan `sensor_id`, hedef, A7 varlık kimliği var mı; sensör hızı ve zamanlama sınırı, ör. 05'in MS5837 için ≈ 25,97 Hz sınırı; kademe-özellik tutarlılığı, ör. `kamera_gerekli` ile kamera kapalı kademesi çelişemez; arıza parametre denetimi, 05 `arm`); **(3) ön denetim** (06 ön denetim raporu; HIL'de kart uyumu). Hatalar yol ve satırla toplu raporlanır; ön denetim tutmazsa çalıştırma başlamaz (06 §4).
- Ayrıştırıcı C++'tadır; şema bir kez tanımlanır, C++ ayrıştırıcı ve editör formu aynı tanımdan türer (03 §2.6'daki "Rust tipinden JSON Schema" yolu kullanılmaz). TOML ayrıştırma kitaplığı seçimi açık; `toml++` aday, bu oturumda doğrulanmadı.
- **Çözümlenmiş senaryo** (varsayılanlar ve araç dosyası dahil, kanonik biçim) ve özeti (SHA-256) A6'ya girer; koşu bu özetle tanımlanır. Araç tanımı ayrı dosyadır (`scenarios/` altında, senaryo ona başvurur ve alan bazlı geçersiz kılar); araç dosyasının sürümü de özete girer.

### 3.4 Sanal emniyet girdilerinin operatör eylemi

Atış kontrol paneli yalnızca **gösterir** (03 §2.7; 2.4). Sanal acil durdurma, silahlandırma fişi ve emniyet pimi (iskelet §5) senaryo zaman çizelgesinden ya da operatör eyleminden değişir; kart bunları değiştiremez. Operatör eylemi panelin sıradan düğmesi değildir:
- **Emniyeti artıran yön** (acil durdurmaya bas, pimi tak, fişi çıkar) tek adımda uygulanır.
- **Emniyeti azaltan yön** (acil durdurmayı kaldır, pimi çıkar, fişi tak) iki adımlıdır: önce sunucudan kısa ömürlü tek kullanımlık onay jetonu istenir (hedef ve yön jetona bağlı), sonra onay kutusu ve jetonla gönderilir. Ömür 10 s [T]. Jeton başka istemcide ya da başka girdi için kullanılamaz.
- Kontrol sahibi olmayan istemci azaltan yönü gönderemez. Her eylem (istemci, yön, adım, ret) `olay` kanalına ve A6'ya yazılır. Senaryo zaman çizelgesi aynı etkiyi yaratabilir; bu, senaryo yüklenirken doğrulama özetinde ayrıca listelenir.
- Kartın geri okumasını bozmak bir **hata enjeksiyonudur** (05 §2.3.8) ve emniyet girdisini değiştirmekten ayrı kayıtlanır.

### 3.5 A6 kayıt (MCAP)

- **Biçim:** MCAP, C++'ta `mcap` kitaplığı doğrudan (Ö7, Ç11, 09 §2.8). Foxglove SDK kullanılmaz kararı K3 kapsamına bağlı açık sorudur (00 A2). Bir koşu bir dosyadır; dosya adı `run_id`.
- **Zaman:** `log_time` sim zamanı ns, `publish_time` duvar saatinin monoton değeri ns (öneri). Aynı sim zamanındaki kayıtlar `sıra` ile sıralanır.
- **Kanallar** (tek yazıcı iş parçacığı; yazıcının iş parçacığı güvenliği belgelenmediği için tek parçacık, 09 §2.8): `a1/sim_kart`, `a1/kart_sim` (çerçeveler olduğu gibi, gönderim/alım duvar saatiyle), `fizik/durum` (snapshot'lar), `fizik/adim` (`StepResult`: uygulanan `ActuatorFrame.seq`, uygulanan/yok sayılan `olay_id`, ömrü dolan komutlar, kütle durumu, sayaçlar; 04 §3), `hil/bariyer` (06 §2.13), `sensor/*` (temel ve arızalı kayıt, yer gerçeği), `sonar/*`, `kamera/*`, `a3/komut` (A3 kontrol çağrıları ve retleri), `olay/*` (arıza, emniyet girdisi, atış, mod, kayıt), `ek/*` (bkz. aşağıdaki ekler).
- **Ekler ve üst veri:** çözümlenmiş senaryo ve özeti, tohum, RNG ve dağılım sürümü, derleyici ve bayraklar, Eigen/Jolt/Embree sürümleri, dalga listesi özeti, kart yazılımı özeti, `(T₀, t₀)` eşlemesi, kayıt şema tanımları (04 §2.13; 06 §2.13).
- **Şema gereği:** MCAP geçerli dosya yapar, içeriği otomatik anlamlandırmaz (03.5). Her kanal şema adı ve kodlama etiketi (`ida-binary-v<n>`) taşır; şema tanımı ekte durur. A1 çerçeveleri MAVLink'tir ama **özel mesajlar** için dialect XML özeti ve sürümü ekte kayıtlıdır. Harici araçlar (Foxglove, PlotJuggler vb.) için `tools/` altında MCAP → JSON/CSV dışa aktarıcı yazılır; bu araçların kayıtları doğrudan çizeceği **iddia edilmez**.
- **Kayıt sağlamlığı:** yazıcı bölümleri düzenli aralıkla kapatır (`[T]` yaklaşık 1 s) ve dosya özeti kapanışta yazılır; kesik dosyada özet olmayabilir ve baştan sıralı okunur (kurtarma belirtimde tanımlı değil, 03 §2.5, çıkarım). Dünya iş parçacığından yazıcıya halka tamponu taşarsa kayıt düşer: sayaç `kayıt_düşen` artar, dosyada "kayıt eksik" işareti kalır, `tekrar oynatma güvencesi` bayrağı düşer, A3 uyarır; çalıştırma **geçersiz sayılmaz** (kayıt koşunun fiziği değil, izidir), ama o kayıttan yeniden koşturma güvencesi verilmez.

---

## 4. Hata durumları ve davranışı

| Durum | Davranış |
|---|---|
| İstemci yavaş / tıkandı | Durum sınıfında eski kare atılır; olay sınıfında kuyruk taşarsa istemci kopar, yeniden bağlanınca tam özet; sim etkilenmez; sayılır |
| Tüm istemciler koptu | Çalıştırma sürer (HIL'de duramaz); olaylar kayıtta; yeniden bağlanan istemci özetle eşlenir |
| Yetkisiz ya da yanlış `Origin`/`Host` isteği | Ret, sayılır, kayıt ve A3 olayı; komut uygulanmaz |
| İki istemci aynı anda komut gönderdi | Kuyruğa giren ilk komut geçer; kontrol sahibi olmayanın komutu ret + neden |
| Senaryo doğrulaması ya da ön denetim tutmadı | Çalıştırma başlamaz; hata listesi A3'te; durum `YÜKLENDİ`'de kalır |
| HIL'de duraklatma | Önceden uyarı ve onay; onaylanırsa çalıştırma geçersiz (06 §2.9) |
| Kayıt yazıcısı yetişmiyor / disk dolu | `kayıt_düşen`; güvence bayrağı düşer; çalıştırma geçersiz sayılmaz; disk dolmak üzereyse başlamadan uyarı |
| Çalışma sırasında enjeksiyon reddi (parametre, hedef yok) | Ret + neden, kayıtta; sim etkilenmez |
| Sim sunucusu çöktü | Yeni süreç `run_id`; kayıt dosyası kapanmamış işaretli, sıralı okunur; kart `REBOOT` ister (06 §2.9) |
| Godot / algılama bileşeni düştü | Süreç olayı; kamera verisi geçerliliği 07'nin kuralıyla; panel uyarır; fizik ve HIL sürer |
| Elle enjekte edilen arıza çalıştırmayı geçersiz kılar mı? | Hayır; arıza deneyin parçasıdır. Geçersizlik yalnız altyapı ihlalidir (kaçırılan adım, bariyer, planlanmamış sıfırlama; 06 §4) |

---

## 5. Kalite kademelerinin etkisi

Kademeler bilgisayar başına seçilir (Ö11; iskelet §6). Fizik, otopilot ve HIL tam çalışır; paneller kısılır. **Aşağıdakiler tahmindir; Windows hedef bilgisayarı bilinmiyor ve hiçbir rakam ölçülmedi** (M20, M21).

- **Yayın:** `kalite` tablosundaki "panel profili" varsayılan yayın hızını (ör. 60 → 30 → 15 Hz [T]), sonar satır hızını ve grafik pencere uzunluğunu düşürür; abone olunmayan kanal bedeli yoktur.
- **Kamera ve 3B kapalı** kademelerde `süreç` ve kamera paneli ögeleri gizlenir; sim sunucusunun A2 yayını ve kamera kayıtları kesilir.
- **Kayıt düzeyi** kademeyle birlikte iner (2.6.1); düşük donanımda "tam" önerilmez.
- **Tarayıcı + sim + Godot aynı makinede CPU paylaşır** (04 §2.12'deki çekişme uyarısıyla aynı sınıf risk). Varsayılan: tek açık panel penceresi; ek pencere kullanıcı tercihidir. Yerel ağa açma yok (2.1), dolayısıyla panelleri başka cihazda açma bu aşamada çözüm değildir.
- Panel yoğunluğu ile kaçırılan adım arasındaki ilişki ölçülmeden bilinmez; ölçüm M20'ye bağlanır.

---

## 6. Ölçüme bağlı kararlar

| Karar | Birincil | Yedek | Ölçüm |
|---|---|---|---|
| Panel yayın hızı ve ham veri | Özet yayın 60 Hz [T], ham veri yalnız MCAP'e (09 §2.7) | 30 / 15 Hz; sonar yalnız talep halinde | M20 (1 kHz telemetriyi yerel WebSocket'in kaldırması dahil, 03.9 doğrulanamadı) |
| Ön yüz taşıması | Sistem tarayıcısında MapLibre GL JS + uPlot | Sarmalayıcı (WKWebView/WebView2) ya da C++ yerel panel (09 §2.6 karşılaştırması) | M20: en eski desteklenen WebKit/WebView2, WebGL2, uzun akış |
| WebSocket/HTTP kitaplığı | Boost.Beast ya da IXWebSocket | diğeri | M20, M11 (CPU payı) |
| Kayıt yazıcı verimi ve taşma eşiği | Tek iş parçacığı + zstd sıkıştırma | Sıkıştırmasız; "telemetri"de sonar kapalı | Yeni ölçüm; 00'da M numarası yok (öneri 8.1 md.4) |
| Kamera kare kaydı | Pencereli, sıkıştırılmış | Yalnız algılama kayıtları | M30 (kare hızı/kopya), yeni: sıkıştırma maliyeti |
| SIL hız üst sınırı | "Olabildiğince hızlı"; sınır sonar ve ağır modellere bağlı | Hız tavanı sabit | M11, M15 |

---

## 7. Test yaklaşımı ve kabul ölçütleri

1. **Şema gidiş-dönüş:** A3 ve A5 şemasından üretilen C++ ve TypeScript kodlayıcı/çözücüler aynı altın (golden) çerçeveleri üretir/çözer; boyut ve sürüm denetimi.
2. **Yavaş istemci:** bir istemci okumayı keser, bir diğeri 60 Hz tüketir. Kabul: dünya iş parçacığının adım süresi dağılımı istemcisizle istatistiksel olarak ayırt edilemez (eşik M20'de, bu bölümde sayı yok); HIL'de `kaçırılan_adım = 0`; kopan istemci yeniden bağlanıp tam özetle eşlenir.
3. **Erişim:** jetonsuz, yanlış `Origin`/`Host` istekleri ret; loopback dışından bağlanılamaz; UDP uç noktası varsayılan kapalı.
4. **Emniyet girdisi eylemi:** azaltan yöndeki tek adımlı istek, süresi geçmiş ya da başka girdiye ait jetonlu istek ve kontrol sahibi olmayan istemcinin isteği ret; her eylem A6'da; A3'te tetik ucu bulunmaz (uç listesi denetimi).
5. **Senaryo doğrulayıcı:** tablo güdümlü geçersiz dosya takımı (bilinmeyen anahtar, aralık dışı, çelişkili kademe, eksik başvuru, major uyuşmazlığı); her biri beklenen yol/satırla rapor eder. Fizik ve otopilot kümeleri bağımsız: otopilot bloğunu değiştirmek fizik izini değiştirmez (aynı tohum, bit eşit).
6. **Hata enjeksiyonu:** aynı senaryo+tohum iki SIL koşusunda aynı arıza dizisini ve aynı kaydı üretir; elle enjeksiyon kayıttan aynı adımda yeniden koşar; her enjeksiyon "yalnızca olaylar" kademesinde de kayıtlıdır; üç arayüze dağıtım her sınıf için en az bir örnekle sınanır.
7. **Kayıt:** üretilen MCAP bağımsız bir okuyucuyla açılır ve şema ekleriyle çözülür; yapay yavaş disk `kayıt_düşen`'i artırır, güvence bayrağını düşürür, sim adım süresini bozmaz.
8. **Tekrar oynatma:** (a) kayıttan izleme aynı panelleri sürer ve canlı akışla aynı çerçeveleri üretir; (b) SIL koşusu yeniden koşturulur → FC çıktıları ve olaylar bit aynı (aynı ikili ve platform; 03 §7); bariyerli HIL kaydından fizik yeniden koşturma aynı fizik izini verir (04 §2.13).
9. **Durum makinesi:** her durum × komut çifti beklenen geçiş ya da retle; HIL'de duraklatma uyarı + geçersizlik; geçerlilik geri dönmez.
10. **Ön yüz taban kabulü:** WebGL2 olan en eski hedef tarayıcıda harita, grafik, sonar ve durum şeridi açılır (M20 ölçer).

Kabul ölçütü olarak yük, gecikme ya da hız rakamı bu bölümde yoktur; M20 ve yeni kayıt ölçümü bu rakamları belirler.

---

## 8. Açık noktalar ve iskelete öneriler

**11.1 İskelete öneriler (mimar karar verir):**
1. **A6 "Bütün A1 ve A3 trafiği":** A3 telemetri çerçeveleri sim durumunun türevidir; kaydedilmez, A6 ve tekrar oynatmaya yaramaz. Öneri: "A1 trafiği, A3 komutları ve olayları".
2. **A6 iki ya da üç düzey tekrar oynatma:** iskeletteki tek "güvence" ifadesi yerine (a) izleme, (b) fizik yeniden koşturma ("telemetri"), (c) girdi tekrarı ("tam", sonar profilleri) ayrımı önerilir; 06 §2.13 "bütün A1 çerçeveleri" ile kademe tablosundaki "sonar yalnız Tam" arasındaki gerilim böyle çözülür.
3. **A5 anahtarları ASCII:** 04'ün A5 grup adları (`akıntı`, `rüzgar`, `fırlatıcı`, `araç`) dosyada ASCII karşılığıyla yazılır; sözleşme aşamasında ad eşlemesi dondurulur.
4. **Yeni ölçüm:** kayıt yazıcı verimi ve kamera kare sıkıştırma maliyeti 00 §7'de yok; M20 yanına eklenmesi önerilir.

**11.2 Komşu bölümlerle uyumsuzluklar ve beklentiler:**
- **03 ↔ iskelet:** 03 §2.7 sanal emniyet girdileri arasında "bakım girdisi" sayıyor; iskelet §5 üç girdi (acil durdurma, fiş, pim) tanımlıyor. Bu bölüm iskelete uyar (üç girdi); bakım girdisi iskelete eklenirse A3 ve A5 güncellenir.
- **04'ten beklenen:** (i) `World::step()` bir fizik adımıdır; SIL "adımla" birimi kontrol periyodudur: koşucunun iki fizik adımını bir arada çağırması ya da 04'ün kontrol sınırı görevinin bölünebilir olması gerekir. (ii) `apply_fault`'un `FaultSpec` alan listesi henüz yazılı değil (05'in `SensorFaultSpec`'i var); 08'in `ArızaTanımı` eşlemesi buna bağlı. (iii) A6 için `StepResult` sürümlü ve donmuş olmalı.
- **05'ten beklenen:** "su girişi" yer gerçeği olayının girişi (05 §2.3.7) 08'in `sistem olayları` sınıfından gelir; 04 su girişini modellemez, bu 05 ile ortak sahiplik gerektirir.
- **06'dan beklenen:** (i) `FaultInjector` ile emülatör profil alanlarının A5 biçimi (06'nın 08'den beklenenleri) bu bölümde yalnız tablo satırıdır; alan listesi 06 ile sözleşme aşamasında. (ii) QGC UDP uç noktasının kimlik denetimsiz doğası (2.1) 06'nın QGC kapsam kararına etkir. (iii) Ön denetim raporunun A3 şeması.
- **07'den beklenen:** kare kaydı ve algılama için yer gerçeği maskesi (07 §8) bu bölümde yalnız "A6'ya, algılama bileşenine hiçbir koşulda verilmeden" olarak düşünülür; kapsama alınması kararı mimara bağlı.
- **02'den beklenen:** otopilot parametre bloğunun A5'teki yeri (parametre deposu, 02 §3.6) ve `PARAM_*` dönüştürücüsü.

**11.3 Çözülmemiş:**
- Windows hedef bilgisayarının özellikleri; panel yoğunluğu ve kademe sınırları buna bağlı (Ö11).
- Çoklu istemci için kontrol sahipliğinin devri ve "izleyici" rolünün kapsamı (bu bölümde öneri, kullanıcı kararı yok).
- Uygulama kabuğu (sarmalayıcı) gerekip gerekmediği: M20 sonrasına bırakıldı.
- Foxglove SDK'nın canlı yayın için kullanılıp kullanılmayacağı (00 A2, K3 kapsamı).
- Hedef senaryo aileleri (Monte Carlo tohum taraması) A5'in dışında, komut satırı aracı olarak 09'dadır; ayar ve kabul tohumlarının ayrımı (03 §2.8 beklentisi) 09'a bırakıldı.
- QGC'nin hedef mi prova mı olduğu (00 A6) yanıtsız; UDP uç noktasının kapsamı buna bağlı.
