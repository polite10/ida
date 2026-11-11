# 07 - Hedef Algılama ve Atış Kontrol Sistemi

> **Durum: ham araştırma raporu (yazıldığı haliyle korunuyor).** Bu rapor bağımsız doğrulamadan geçti ve bilinen hatalar içeriyor. Ayrıca dil kararı C/C++ olmadan önce, Rust varsayımıyla yazıldı; Rust'a özgü öneriler geçersizdir. Doğrulaması: [dogrulama/07-codex.md](dogrulama/07-codex.md). Düzeltmeler ve geçerli kararlar: [00-duzeltmeler-ve-kararlar.md](00-duzeltmeler-ve-kararlar.md). Çelişki halinde 00 numaralı belge geçerlidir.

Araştırma tarihi: 2026-11-10. Kapsam: tek bir hibrit (su üstü + dalış) insansız deniz aracında yarışma torpidosu (küçük, patlayıcısız, zararsız görev nesnesi) için hedef algılama, hedef takibi, atış çözümü, atış kontrol durum makinesi, emniyet zinciri, simülasyon ve otopilot arayüzü. Geçici hedef kart: STM32U5A9J-DK (Cortex-M33, 160 MHz, NPU yok).

Okuma notu:
- Kaynak etiketleri [Kxx] biçimindedir. URL'ler Bölüm 6'dadır.
- **"Doğrulanmadı"**: Bu oturumda birincil kaynaktan teyit edilemedi ya da yalnızca forum veya ikincil kaynağa dayanıyor.
- **"Tahmin"**: Kendi mühendislik hesabım veya çıkarımımdır. Ölçümle doğrulanmalıdır. Varsayımlar yanında yazılıdır.
- ST'nin sitesi (st.com) bu oturumda zaman aşımına uğradı. Kart kılavuzu UM2967 için bir ayna kopya kullanıldı [K12]. Kart ve araç zinciri ayrıntıları için kardeş rapor `01-kart-ve-arac-zinciri.md`, otopilot için `06-otopilot.md` esas alınmalı.

---

## 1. Özet

1. **Yarışmalarda baskın çözüm yaylı fırlatıcı + elektromekanik tetik.** Tetik bir servo, solenoid ya da manyetik kilittir. Örnekler: Stanford 2025 (tüp başına en fazla 75 N yay, tek servo) [K3], İTÜ 2024/2025 (yay + solenoid ya da manyetik kilit) [K4][K5], Victoria 2026 (yay + çekmeli solenoid) [K6], Berkeley 2026 (yay + iki su geçirmez servo) [K7]. Pnömatik kullanımı azalıyor. Teknofest 2026 şartnamesi fırlatmada pnömatik, hidrolik ve piroteknik sistemleri açıkça yasaklıyor [K2].
2. **Görev geometrisi küçük ve yakın mesafeli.** RoboSub 2025/2026'da hedef 0.6 m x 0.6 m'lik bir pano; üzerinde iki farklı boyda açıklık var [K1][K1b]. Torpido 51x51x152 mm kutuya sığmalı, en fazla 0.91 kg olmalı, araç en fazla 2 torpido taşıyabilir ve torpidonun hızı "güvenli" olmalı [K1]. Teknofest 2026'da hedefler 3 renkli; en fazla 5 atış yapılabilir, atış başına 20 puan verilir ve görev **manuel** icra edilir [K2].
3. **Bu kartta gerçek kamera ile algılama yapılamaz.** MCU'da DCMI/PSSI birimi var [K13], ama kartta kamera konnektörü yok. UM2967'de genişleme konnektörlerinde DCMI sinyali de listelenmiyor [K12][K14]. USB HS portu yalnızca cihaz (sink) kipinde çalışıyor [K12]. Sonuç: kart üzerinde görüntü işleme ancak HIL'de, PC'nin USB HS üzerinden kare akıtmasıyla denenebilir.
4. **TinyML ile nesne algılama bu sınıfta pratik değil.** ST'nin `st_yoloxn` modeli 192x192 girişte STM32H747 (Cortex-M7, 400 MHz) üzerinde 357.79 ms sürüyor. Aynı model kartında STM32U5 "desteklenmiyor/önerilmiyor" olarak işaretli [K15]. U5'te 1 kare/s'nin altı beklenir (**tahmin**). Klasik renk eşikleme ve kontur, QVGA ve altında birkaç ile onlarca kare/s arasında makul görünüyor (**tahmin**; OpenMV H7 karşılaştırması [K17]).
5. **Sonar tarafı MCU'ya çok uygun.** Ping360 her açı adımında 1-B bir yankı profili üretir. Bu profil üzerinde CA-CFAR, açılar arası kümeleme ve takip M33'te rahatça koşar (**tahmin**). Ancak Ping360'ın 360° taraması 1 m menzilde 3.4-4.3 s sürüyor ve en kısa menzili 0.75 m [K18]. Bu nedenle hizalanma için yavaş kalır; hedefi bulmak ve kaba yaklaşmak için uygundur. Çok ışınlı sonarlar (ör. Oculus M750d: 40 Hz, 512 ışın, Ethernet [K20]) yardımcı bilgisayar işidir.
6. **Hedef takibi için önerilen yapı:** hedef sabit olduğu için hedef konumu dünya (NED) çerçevesinde tutulmalı. Kalman filtresi aracın seyrüsefer verisiyle beslenmeli; bu Quidu vd.'nin yaklaşımıdır [K22]. Veri ilişkilendirme Mahalanobis kapısı + GNN ile, iz onaylama M-of-N ile yapılmalı [K24]. Hareketli hedef için sabit hızlı model ya da alfa-beta filtresi yeterli [K23].
7. **Atış çözümünde balistik kritik.** Örnek hesapla (**tahmin**; 30 mm çap, 0.11 kg etkin kütle, Cd≈0.3, 3 m/s çıkış hızı): torpido 1 m'yi yaklaşık 0.56 s'de, 2 m'yi yaklaşık 2 s'de alır. 2 m'de hız 0.44 m/s'ye düşer. Bu sürede %2'lik yüzerlik hatası 2 m'de yaklaşık 14 cm düşüş, 5 cm/s'lik yan akıntı yaklaşık 10 cm sürüklenme yaratır. Teknofest denizde yapıldığı için akıntı düzeltmesi gerekir.
8. **Atış kontrolü açık bir durum makinesi olarak tasarlanmalı.** Önerilen durumlar: GÜVENLİ → SİLAHLANDIRILIYOR → SİLAHLI → TAKİP → ÇÖZÜM GEÇERLİ → ATIŞ İZNİ → ATIŞ → DEĞERLENDİRME. Ek olarak kilitli ARIZA ve BAKIM/TEST durumları olmalı. Her durumda zaman aşımı ve her girdi için tanımlı bir geçiş bulunmalı (NASA "güvenli durum makinesi" özellikleri [K26]).
9. **Emniyet zinciri için NASA desenleri:** ayrı "arm" ve "fire" komutları, iki adımlı komut (iki adımlı komut tek başına bir "inhibit" sayılmaz), bağımsız inhibit tanımı ve watchdog'un kesmeden beslenmemesi [K26]. Tek MCU'nun kontrol ettiği inhibitler NASA tanımına göre bağımsız sayılmaz. Bu yüzden zincirde en az bir fiziksel ya da başka bir denetleyicide duran inhibit olmalıdır.
10. **İş bölümü önerisi:** Atış kontrolü, emniyet kilitleri, takip ve atış çözümü her zaman MCU'da kalmalı. Ağır algılama (YOLO, stereo/PnP, çok ışınlı sonar) yardımcı bilgisayara gitmeli. Algılamanın yeri sonradan değişebilsin diye sınır **"ölçüm (detection)"** seviyesinde, zaman damgalı ve kovaryanslı, sensörden bağımsız bir mesajla çizilmeli.

---

## 2. Bulgular

### 2.1 Yarışmalarda torpido görevi pratikte nasıl çözülüyor

#### 2.1.1 Kural çerçevesi

**RoboSub 2025 (Tagging - Torpedoes)** [K1]
- Hedef: dikey bir pano, 2 ft x 2 ft (0.6 m x 0.6 m), oluklu plastik zemin üzerinde vinil baskı. Panoda iki açıklık var.
- Puanlama: açıklıktan tam geçiş tam puan alır. Panoya değip geçemeyen atış kısmi puan alır. Seçilen hayvana yakın açıklık ve doğru sıra daha çok puan getirir. Panonun önündeki yatay çubuklarla işaretli "far" mesafesinden atış ek puan alır.
- Puan tablosu: "Tagging: Any, Correct sequence 600 / torpedo (max 2), 1600" ve "'Far' torpedoes 300 / torpedo".
- Torpido kuralları:
  - 51x51x152 mm'lik kutuya sığmalı ve havada en fazla 0.91 kg olmalı.
  - Takım adı, rengi veya amblemi üzerinde bulunmalı.
  - "Safe speed": su altında yakından bir insana çarptığında morartmayacak hız.
  - Araç en fazla 2 torpido taşıyabilir.
  - Dolu torpido takım çadırına alınamaz. Fırlatıcı testleri suda ya da kimseye yönelmeyen açık alanda yapılmalı.
- Kill switch kuralı: dalgıcın kolayca erişebileceği, açıkça işaretli bir anahtar olmalı. Anahtar "bataryaları tüm itki bileşenlerinden **ve cihazlardan**" ayırmalı. Yeniden açıldığında araç güvenli duruma dönmeli (pervaneler kendiliğinden dönmemeli). Fırlatıcının enerji hattı da bu anahtarın arkasında olmalıdır.
- Not: PDF'in sayfa düzeni nedeniyle görev başlıkları ile şekil altyazıları kaymış görünüyor. Pano boyutu ve torpido kuralları ilgili bölümlerde net.

**RoboSub 2026 (Deploy - Torpedoes)** [K1b]
- Aynı 0.6 m x 0.6 m pano, iki farklı boyda açıklık.
- Ek puan için "far (1 ft, 0.3m) and farther (1.5 ft, 0.46m)" mesafeleri verilmiş.
- Açıklıkların ölçüsü sayfada yok. Mesafelerin hangi referansa göre ölçüldüğü (panodan mı, bir çizgiden mi) **doğrulanmadı**.

**Teknofest İnsansız Su Altı Sistemleri 2026** [K2]
- Torpido görevi yalnızca **temel kategoride** var ("Hedefe Müdahale Teması (Yarı Otonom)"). Görev metninde "Bu görev manuel olarak icra edilecektir" deniyor. İleri kategorinin temalarında torpido yok.
- Deniz içinde 3 farklı renkli hedefe toplam en fazla 5 atış yapılır. Hedef rengi önceki görevdeki ipucundan gelir. Her başarılı geçiş 20 puan, tema en fazla 100 puan, süre 5 dakika.
- Yasaklar ve kurallar:
  - Hedefe temas ederek atış yapmak ve gripper ile bırakmak yasak.
  - Fırlatmada "patlayıcı (pyro vb.), pnömatik ve hidrolik sistemler" yasak. Mekanik ya da elektromekanik (ör. yaylı) sistem bekleniyor.
  - Fırlatma mekanizması ağırlık ve boyut ölçümüne dahil. Torpidolar dahil değil.
  - Torpidonun hedeften temaslı ya da temassız geçmesi arasında puan farkı yok.
- Ön eleme videosunda temel kategori en az 3 torpidonun hedeften geçtiğini göstermeli. Araç atış sırasında hedefe değmemeli.
- Emniyet ve operasyon:
  - Bataryalı araçlarda erişilebilir bir acil durdurma butonu olmalı. Butona basıldığında "bütün motorların durup sistemin kapandığı" gösterilmeli.
  - Gerilim en fazla 50 VDC olabilir.
  - Kablosuz araçlara en az 50 m'lik emniyet halatı bağlanmalı.
  - Kontrol masasındaki üyeler denizi ve aracı göremez. Operatör onaylı atış kipi tamamen araç kamerasına ve telemetriye dayanır.
- 2024 yarışmasında finalist bir takımın "Otonom Torpido Atışı" görevinden söz ettiği haber kaynaklarında geçiyor [K2b]. Bunun hangi kategoride ve hangi kurallarla yapıldığı **doğrulanmadı**. Kurallar yıldan yıla açıkça değişiyor; tasarım tek bir yılın kuralına kilitlenmemeli.

#### 2.1.2 Fırlatma mekanizmaları (takım raporları)

| Takım / yıl | Mekanizma | Tetik / arayüz | Not |
|---|---|---|---|
| Stanford 2025 [K3] | Çift tüp, yay; tüp başına en fazla 75 N | Tek servo; kapak-tetik-çekiç zinciri servoya mekanik avantaj sağlıyor ve mili radyal/eksenel yükten koruyor | Torpido elipsoidal, büyük kanatlı |
| İTÜ 2024 [K4] | Yay + elektrikli solenoid kilit | Solenoid | Manyetik ve pnömatik sistemler "olası teknik arıza ve bakım güçlüğü" gerekçesiyle elenmiş |
| İTÜ 2025 [K5] | Dikey istifli çift tüp, yay | Komutla açılan manyetik kilit | Ana kart torpido sistemine 12 V/5 V sağlıyor; kill switch durumu CAN ile izleniyor |
| Victoria 2026 [K6] | Yay | Çekmeli solenoid. Ölçülen açma kuvveti ~8 N; O-ring sürtünmesi için daha güçlü solenoid ve çekme çubuğu eklenmiş | Delrin namlu, su giriş delikleri; neredeyse nötr yüzerlik; spin için yivli kanatlar |
| Berkeley 2026 [K7] | Yay | İki su geçirmez servo ile mandal; servolar Pixhawk pin başlığına bağlı | Nötr yüzerliğin en iyi isabeti verdiği belirtiliyor |
| METU 2026 [K8] | Yay | Solenoid. Step motordan solenoide geçilmiş; 50+ kuru atış testinde sürtünmeden kaynaklı sıkışma bulunmuş | IBVS ile hizalanma; "far" bonusu hedefleniyor |
| Sherbrooke 2026 [K9] | Yay + kılavuzlu piston | Dört kamlı tek kam mili: 2 kam bırakıcı, 2 kam torpido | Myring gövde formu; Simulink'te yörünge benzetimi |
| Brasília 2026 [K10] | CO2 kartuşu | Solenoid bir pimi kartuşa sürüyor | Teknofest kuralına göre pnömatik sayılır ve yasak olur |
| NTU Mecatron 2025 [K11] | Kendi itkili torpido: çekirdeksiz DC motor + 6 V/5 F süperkapasitör (~15 s itki) | Reed anahtar + tristörle aktivasyon; Qi kablosuz şarj | Kanatsız denemelerde yörünge kararsız çıkmış; kanatlar kararlılığı belirgin artırmış |
| UC Boulder 2020 [K21] | Pnömatikten solenoide geçiş (bırakıcılarda) | - | Algılama: anahtar nokta eşleme + `solvePnP` ile hedef pozu; tüp ekseni hedef düzlemine izdüşürülüyor; kamera-tüp ofseti hesaba katılıyor |

Çıkarımlar:
- **Kontrol arayüzü pratikte iki türlü.**
  1. Hobi servosu: 50 Hz, 1-2 ms darbe genişlikli PWM [K29].
  2. Solenoid ya da manyetik kilit: MOSFET ile sürülen GPIO darbesi.
  - Ek olarak birkaç takımda tüp başına ayrı çıkış var. İTÜ'de kill switch durumu da izleniyor [K5].
- **Mekanik güvenilirlik algoritmadan daha belirleyici.** METU bunu açıkça yazıyor: "An actuator that fires reliably scores higher than an actuator that achieves..." [K8]. Victoria da ilk prototiplerde güvenilmez açılma ve yetersiz kuvvet sorununu not etmiş [K6].
- **Torpido tasarımında ortak noktalar:** nötr ya da nötre yakın yüzerlik, kanat ya da spin ile kararlılık, kısa ve düz yörünge [K6][K7][K11].
- **Tipik atış mesafeleri kısa.** RoboSub 2026'da bonus mesafeleri 0.3 m ve 0.46 m [K1b]. Teknofest mesafe tanımlamıyor; temas yasak olduğundan pratikte "temassız en yakın" mesafe [K2]. Takımların gerçek atış mesafelerine dair sayısal veri raporlarda bulunamadı (**doğrulanmadı**).

#### 2.1.3 Algılama ve hizalama yaklaşımları (takımlar)

- **Neredeyse tüm takımlar algılamayı Jetson sınıfı bir yardımcı bilgisayarda YOLO ile yapıyor.**
  - İTÜ 2025: YOLOv11, stereo nokta bulutu ile hedef yönelimi [K5].
  - Victoria 2026: TensorRT üzerinde YOLO, Jetson Orin Nano [K6].
  - METU 2026: YOLO11-Pose + `solvePnP` + hibrit görsel servo (IBVS) [K8].
  - Sherbrooke 2026: YOLOv8n; Luxonis OAK kameralarında kamera üstü işlem [K9].
  - Stanford 2025: Jetson Orin AGX + OAK-D [K3].
- **MCU tarafı her raporda "gerçek zamanlı donanım ve eyleyici" katmanı olarak kalıyor.** Victoria bu ayrımı açıkça yazıyor: "the MCU owns all real-time hardware interaction and low-level actuation, while the Jetson handles compute-intensive workloads" [K6]. İTÜ'de CAN, Berkeley'de Pixhawk kullanılıyor [K5][K7].
- **Görev mantığı genelde davranış ağacı ya da durum makinesi.** METU, iz kaybında torpido görevini güvenle iptal etmeyi bir test maddesi olarak doğrulamış (TS-SW-03) [K8]. NTU, `LaunchTorpedo` hizmetini ROS üzerinden davranış ağacına açıyor [K11].
- **Kamera-tüp ofseti hesaba katılmalı.** Boulder'ın yöntemi bu proje için doğrudan model alınabilir. Hedef düzleminin pozu kestirilir, tüp ekseni bu düzleme izdüşürülür ve araç bu izdüşüm hedef açıklığa denk gelecek şekilde konumlandırılır. Böylece açıklık görüş alanından çıksa bile atış yapılabilir [K21].

### 2.2 Mikrodenetleyicide yapılabilir hedef algılama

#### 2.2.1 Kartın gerçek kısıtları

- **MCU kaynakları:** STM32U5A9: Cortex-M33, 160 MHz, 2.5 MB SRAM, 4 MB flash. Birimler: DCMI, PSSI, NeoChrom GPU2D, Chrom-ART (DMA2D), MIPI DSI host, LTDC [K13].
- **Kart (UM2967 [K12]):**
  - 480x480 yuvarlak MIPI DSI ekran.
  - 512 Mbit Octo-SPI flash, 512 Mbit Hexadeca-SPI PSRAM, 4 GB eMMC [K14].
  - İki adet 2.54 mm genişleme konnektörü. Bu konnektörlerde USART, LPUART, iki SPI, SAI, üç I2C, SDMMC, ADC, zamanlayıcı ve GPIO sinyalleri var. **DCMI ve kamera konnektörü yok.** İndirilen UM2967 kopyasında "DCMI/PSSI" geçmiyor.
  - USB Type-C, USB 2.0 HS, "sink only".
  - Kart yalnızca VDD = 1.8 V için tasarlanmış.
- **Sonuç: bu kartla gerçek kamera ile kart üstü görüntü işleme mümkün değil.** USB yalnızca cihaz kipinde olduğu için UVC kamera bağlamak da mümkün görünmüyor (çıkarım).
- **HIL'de denemek mümkün.** PC, sentetik kareleri USB HS CDC ile karta akıtabilir.
  - Bant genişliği **tahmini**: QVGA RGB565 15 fps ≈ 2.3 MB/s; 160x120 gri 15 fps ≈ 0.29 MB/s.
  - Bu, HIL sensör akışıyla aynı bağlantıyı paylaşır. Kardeş rapor 01'e göre Rust/Embassy'de U5 USB HS desteği yeni ve donanımda doğrulanmamış.
- **ST'de gerçek kamera ile DCMI kare hızı örneği (farklı kart):** STM32U575I-EV + OV5640, 320x240 RGB565. Piksel saati 12 MHz'ten 24 MHz'e çıkarılınca hız 7.5 fps'ten 15 fps'e çıkıyor [K16]. Bu bir topluluk makalesi, resmi veri değil. AN5020'ye göre DCMI'da taşma koruması yok; AHB veri hızını karşılayamazsa veri üzerine yazılır [K16b].

#### 2.2.2 Klasik görüntü işleme (M33, NPU yok)

- **Yapılabilir iş hattı:**
  1. Düşük çözünürlük (160x120 ya da 320x240).
  2. RGB565'ten HSV'ye ya da normalize renge dönüşüm.
  3. Renk eşikleme.
  4. Morfolojik açma.
  5. Bağlı bileşen etiketleme ve kontur.
  6. Şekil ölçütleri: alan, dairesellik, en-boy oranı, dışbükey gövde ve delik sayısı.
  7. Panodaki açıklıklar için "delik" topolojisi: dış kontur içinde iç kontur aranır.
  8. Bilinen pano boyutu ile `solvePnP` benzeri poz ya da basit pinhole menzil kestirimi.
- **Hesap yükü (tahmin):** QVGA'da 76.800 piksel var. Piksel başına onlarca komutluk 2-3 geçiş 160 MHz'te kare başına birkaç ile yirmi ms arasında tutar. Bu, bellek erişimi ve önbellek davranışına bağlıdır ve ölçülmelidir.
- **Karşılaştırma:** OpenMV H7 (Cortex-M7, 480 MHz) için ikincil kaynaklar QVGA'da basit algoritmalarla 25-50 fps, tek renk takibinde ~40 fps bildiriyor [K17] (**doğrulanmadı**, forum ve satıcı verisi). M33 @160 MHz bunun birkaç katı yavaş olmalı (**tahmin**).
- **Çözünürlük yeterliliği (tahmin):** 60° yatay görüş, 320 piksel genişlik ve 2 m mesafede piksel başına ~6.5 mm düşer. 0.6 m'lik pano ~90 piksel görünür. Renk ve şekil ayrımı için yeterli. Açıklık boyutu bilinmediği için açıklık kenarı hassasiyeti ayrıca hesaplanmalı.
- **Su altı renk bozulması klasik eşiklemenin en zayıf noktası.** Kırmızı hızla sönümlenir; bulanıklık ve geri saçılma vardır. Stanford 2026, kuru görüntülerle eğitilen modelin su altında kötü genellediğini raporluyor [K19]. Klasik eşikler için de benzer kırılganlık beklenmeli. Teknofest hedeflerinin kırmızı, yeşil ve mavi olması [K2] bu açıdan önemli bir risk.

#### 2.2.3 TinyML (STM32Cube.AI / ST Edge AI, TFLite Micro)

- **ST model hayvanat bahçesi ölçümleri [K15]:**
  - `st_yoloxn d033_w025` modeli 192x192 int8 ile STM32H747I-DISCO (M7, 400 MHz) üzerinde 357.79 ms sürüyor. 256x256'da 641.66 ms.
  - Ağırlıklar ~891 KiB flash, aktivasyonlar ~185 KiB RAM tutuyor. Kartın belleğine sığar.
  - Aynı model STM32N6570-DK NPU'sunda 6.63 ms.
  - Model kartında STM32U5 "desteklenmiyor/önerilmiyor" olarak işaretli. `mobilenetv2_pt` kartında da U5 ve H7 için destek işareti yok [K15b].
- **U5 için tahmin:** saat oranı (160/400) ve M33'ün M7'ye göre çevrim başına daha düşük başarımı birlikte düşünüldüğünde aynı model 1-2 s/kare civarında koşar. Bu, hizalanma döngüsü için kullanışsız.
- **TinyML'in bu kartta makul kullanımı:** 64x64 ya da 96x96 küçük bir **doğrulama sınıflandırıcısı**. Klasik iş hattının bulduğu aday bölgeyi "hedef / değil" ya da "renk sınıfı" olarak doğrular. MLPerf Tiny'de görsel uyandırma kelimesi (96x96, MobileNetV1 0.25x) STM32L4R5 @120 MHz üzerinde ~230 ms ölçülmüş [K17b]. U5'te bunun altında olması beklenir (**tahmin**).
- **TFLite Micro:** yorumlayıcı tabanlıdır. Dinamik bellek yerine geliştiricinin verdiği sabit bir "arena" kullanır [K27]. STM32Cube.AI ise modeli C koduna derler. Rust tarafında ikisi de C FFI ile bağlanır. Kardeş rapor 01'de FFI karması varsayılan olarak önerilmiyor; bu nedenle TinyML ilk sürümde ertelenmeli.
- **Kart ileride değişirse:** STM32N6 sınıfı (NPU + kamera girişi) bir kartta `st_yoloxn` ~150 çıkarım/s veriyor [K15]. Bu, "MCU'da gerçek algılama" seçeneğini açar. Arayüz bu değişime şimdiden hazır tasarlanmalı (Bölüm 3.2).

#### 2.2.4 Sonar tabanlı algılama

| Sonar | Ne verir | Ne algılanabilir | MCU uygunluğu |
|---|---|---|---|
| Ping2 / Ping (1-B yankı ölçer, 115 kHz, 25° ışın, UART 115200) [K18b] | Tek ışın boyunca yankı profili ve mesafe | Önündeki en güçlü yansıtıcıya mesafe; açı bilgisi yok | Çok uygun. Öne bakarsa hizalandıktan sonra **menzil ölçer** olarak kullanılır |
| Ping360 (mekanik taramalı, 750 kHz, 2° x 25°, 0.75-50 m) [K18] | Her açı adımında 1-B yankı profili; açılar gradyan cinsinden (0-399) [K18c] | Pano, direk, duvar gibi yapılar küme olarak. Açıklıkları ayırt etmesi kuşkulu: 2° ışın 2 m'de ~7 cm | Uygun (UART/RS485). Ama yavaş: 360° tarama 1 m'de 3.4-4.3 s, 50 m'de 33 s. ±30°'lik sektör 1 m'de ~0.6-0.7 s (**tahmin**, spec'ten oranlama) |
| Çok ışınlı görüntüleme sonarı (ör. Oculus M750d: 130° x 20°, 512 ışın, en fazla 40 Hz, Ethernet) [K20] | 2-B görüntü (açı x menzil) | Hedef geometrisi, hareket; yakın mesafede açıklık kenarları muhtemelen | Bu kartta Ethernet yok (kardeş rapor 01). Veri hızı ve işlem yükü nedeniyle **yardımcı bilgisayar** işi |

**MCU'da önerilen sonar iş hattı:**
1. **Ön işleme.** Zamanla değişen kazanç (TVG) düzeltmesi yapılır. Yakın alan ve kör bölge (Ping360'ta < 0.75 m) maskelenir.
2. **Algılama.** Her ışın için menzil yönünde **CA-CFAR** uygulanır. Eşik T = α·Pn; α = N·(Pfa^(-1/N) − 1). Burada N eğitim hücresi sayısıdır; CUT'ın iki yanında koruma hücreleri bırakılır [K25]. Kenar ve duvar yankılarında maskelenmeyi azaltmak için OS-CFAR alternatiftir.
3. **Kümeleme.** Ardışık açılardaki algılamalar polar ızgarada bağlı bileşen ya da Kartezyende DBSCAN ile kümelenir. Her kümeden merkez, uzanım ve toplam yankı şiddeti çıkarılır.
4. **Takip.** Bölüm 2.3'teki takipçi kullanılır. Sonar tarama hızı yavaş olduğu için her ölçüm kendi açı ve zaman damgasıyla işlenmeli; tüm tarama tek anlık bir görüntü gibi ele alınmamalı.
5. **Literatür.**
   - Karoui, Quidu ve Legris, ileriye bakan sonarda sıralı algılama + debiased converted measurement Kalman + JPDA ile sabit ve hareketli engelleri izliyor [K22b].
   - Quidu vd., hedefin sonardaki görünür hareketini seyrüsefer verisiyle Kalman durum modeline katıyor [K22].
   - Sheng vd., mekanik taramalı sonarda C-means kümeleme ile çok hedefli ilişkilendirme yapıyor [K22c].

**Dürüst değerlendirme:**
- Oluklu plastik bir panonun akustik yansıtıcılığı (hedef kuvveti) bilinmiyor; **doğrulanmadı**. Panonun kendisi değil, taşıyıcı çerçevesi ve ağırlıkları daha iyi yansıtıyor olabilir. Bu, havuzda ölçülmeden sonar tabanlı hedef sınıflandırmaya güvenilmemesi gerektiği anlamına gelir.
- Sonarın işi: hedefi uzaktan bulmak, yaklaşma rotası vermek ve son aşamada menzil ölçmek. Açıklığa nişan almak kamera işidir.

#### 2.2.5 Neyi nerede yapmalı (özet tablo)

| İş | Bu kart (U5A9J-DK) | Yardımcı bilgisayar (Jetson / RPi) | Gerekçe |
|---|---|---|---|
| Atış kontrol durum makinesi, emniyet kilitleri, eyleyici sürme | **Evet, her zaman** | Hayır | Emniyet kritik; deterministik zamanlama gerekir |
| Hedef takibi (Kalman, ilişkilendirme), atış çözümü | **Evet** | İsteğe bağlı yedek | Hafif hesap; seyrüsefer verisiyle aynı yerde olmalı |
| 1-B sonar algılama (Ping2/Ping360 + CFAR + kümeleme) | **Evet** | - | Düşük veri hızı, basit hesap |
| Çok ışınlı sonar görüntü işleme | Hayır | **Evet** | Ethernet, yüksek veri hızı |
| Klasik renk/kontur algılama | Yalnızca HIL'de (kamera girişi yok) | Evet | Gerçek kamera bu karta bağlanamıyor |
| CNN ile nesne algılama (YOLO vb.) | Hayır (U5'te ~1-2 s/kare, tahmin) | **Evet** | NPU/GPU gerekir |
| Pano pozu (PnP), stereo | Hayır (ilk sürümde) | **Evet** | Kamera orada |

### 2.3 Hedef takibi

#### 2.3.1 Durum modeli ve çerçeve seçimi

- **Sabit hedefler (pano, renkli hedef).** Durum: dünya (yerel NED) çerçevesinde p = (x, y, z). Süreç gürültüsü çok küçük tutulur; hedef "neredeyse sabit" kabul edilir.
  - Ölçümler aracın seyrüsefer kestirimiyle dünya çerçevesine taşınır.
  - Böylece aracın kendi hareketi takip filtresine gürültü olarak girmez.
  - Seyrüsefer hatası ölçüm kovaryansına eklenir.
  - Bu, Quidu vd.'nin "navigational data" yaklaşımıdır [K22].
- **Hareketli hedef (eğitim ve test senaryoları için).** Sabit hızlı model kullanılır: durum (p, v).
  - Alternatif α-β filtresidir. Kalata'nın izleme indeksi Λ, manevra belirsizliğinin ölçüm belirsizliğine oranıyla α ve β'yı tek parametreye bağlar [K23].
  - α-β, sabit kazançlı Kalman'ın kararlı hâl çözümüne denktir. MCU'da en ucuz seçenektir.
  - Ölçüm aralığı düzensizse (sonar taraması, kare düşmesi) tam Kalman daha doğru olur. Kalman, kovaryans da verdiği için kapılama ve atış izni için ayrıca gereklidir.
- **Ölçüm modelleri.**
  - Kamera: piksel (u, v) → yön (azimut, yükseklik). Pano boyutundan ya da PnP'den menzil.
  - Ping360: (menzil, azimut); yükseklik belirsiz (25° dikey ışın).
  - Ping2: yalnız menzil.
  - Polar ölçümleri Kartezyene çevirirken taraflılık (bias) oluşur. Karoui vd. bu yüzden "debiased converted measurement" kullanıyor [K22b]. Alternatif: EKF ile polar ölçüm doğrudan işlenir.
- **Kamera-tüp ve sonar-tüp dış kalibrasyonu (extrinsics)** filtrede değil atış çözümünde kullanılır. Ama hatası bütçeye girer (Bölüm 2.4).

#### 2.3.2 Veri ilişkilendirme ve iz yönetimi

- **Kapılama:** Mahalanobis uzaklığı d² = νᵀ S⁻¹ ν ile yapılır. Eşik ki-kare dağılımından seçilir. Örneğin %99 için 2 serbestlik derecesinde ≈ 9.21, 3 serbestlik derecesinde ≈ 11.34 (standart tablo değerleri).
- **İlişkilendirme:**
  - Az hedef (≤ 5) ve az ölçüm için **GNN** (Hungarian/Munkres ya da açgözlü eşleme) yeterli. MathWorks belgeleri Munkres'in optimal ama en yavaş seçenek olduğunu belirtiyor [K24].
  - Yoğun yankı ortamında (havuz duvarları, çok yollu yayılım) JPDA daha sağlamdır [K22b]. MCU'da hipotez sayısı sınırlanmalıdır.
- **İz yaşam döngüsü:** aday → onaylı (M-of-N, ör. 3/5) → bakım → silme (K ardışık kaçırma). trackerGNN'de de izler "tentative" başlar ve yeterli algılamadan sonra "confirmed" olur [K24].
- **Atış kontrolü yalnız onaylı ve kovaryansı yeterince küçük izlere atış izni vermeli.** Ölçütü 3σ elipsinin açıklık içinde kalması olabilir.

#### 2.3.3 Hedefin araca göre konumu

- **Çıktı:** hedefin araç gövde çerçevesinde ve **tüp çerçevesinde** göreli konumu. Bu, dünya çerçevesindeki iz ile o andaki seyrüsefer kestiriminden hesaplanır.
- **İzin "geçerlilik zamanı" atış anına ileri kestirilmeli.** Sabit hedef için bu, aracın t_atış anındaki pozunun kestirimi demektir. Bu da otopilotun kestiricisinden alınır.

### 2.4 Atış çözümü

#### 2.4.1 Hizalanma ölçütleri (önerilen)

Atış izni için tüm koşullar aynı anda ve en az T_dwell süresince (ör. 0.3-0.5 s) sağlanmalı:
1. **Tahmini isabet noktası açıklığın içinde.** Tüp ekseni ve balistik modelle hesaplanan düşme ve sürüklenme sonrası isabet noktası p_hit için:
   - |e_yatay| ve |e_dikey| < r_açıklık − r_torpido − pay.
   - Pay, iz kovaryansı ve balistik dağılımdan gelir (3σ).
2. **Menzil kapısı:** R_min ≤ R ≤ R_max.
   - R_min: Teknofest'te temas yasak [K2]; RoboSub'ta "far" bonusu için alt sınır [K1b]; ayrıca torpidonun tüpten çıkıp kararlı hâle geçme mesafesi.
   - R_max: balistik dağılımın açıklığa sığdığı en büyük menzil. Simülasyonda Monte Carlo ile bulunup MCU'ya tablo olarak yüklenmeli.
3. **Açısal hızlar küçük:** |ω_yaw|, |ω_pitch| < eşik. Tüpün dönme hızı torpidoya yanal hız olarak geçer: v = ω × r_tüp.
4. **Yalpa ve baş-kıç açıları sınırlar içinde.**
5. **Göreli öteleme hızı küçük** (istasyon tutuluyor).
6. **İz onaylı, ölçüm tazeliği < T_max** (ör. 200 ms).

#### 2.4.2 Basit balistik model

- **Denklem:** m_e · dv/dt = −½ ρ C_d A |v_r| v_r + (ρV − m) g ẑ + (eyleyici itkisi, yalnız tüpte)
  - v_r = v − v_akıntı (suya göre hız).
  - m_e = m + m_a (eklenen kütle). Narin gövdede eksenel eklenen kütle küçük, yanal olanı büyüktür (Fossen, bölüm 6 [K28]).
  - Sürükleme denklemi ve C_d'nin referans alana bağımlılığı için [K30].
- **Kapalı form (yalnız eksenel sürükleme, tahmin):** L = 2 m_e / (ρ C_d A) olmak üzere v(x) = v0 · e^(−x/L) ve t(x) = (L / v0) · (e^(x/L) − 1).
- **Sayısal örnek (tahmin; varsayımlar: çap 30 mm → A = 7.07e-4 m², m_e = 0.11 kg, C_d = 0.3, v0 = 3 m/s):**
  - L ≈ 1.04 m.
  - x = 1 m: t ≈ 0.56 s, v ≈ 1.15 m/s.
  - x = 2 m: t ≈ 2.0 s, v ≈ 0.44 m/s.
  - Çıkarım: torpido birkaç metrede ciddi yavaşlar. Uçuş süresi uzadıkça yüzerlik ve akıntı hataları büyür. Kısa menzil açıkça avantajlıdır.
- **Yüzerlik hatası (tahmin):** ağırlığın %2'si kadar net batma kuvveti ≈ 0.02 N. Yanal sürüklemeyle uç hız ≈ 9 cm/s olur (C_d ≈ 1, yan alan 30x150 mm). Düşüş 1 m'de (0.56 s) ≈ 1.5 cm, 2 m'de (2 s) ≈ 14 cm. Takımların nötr yüzerliği vurgulaması [K6][K7] bununla tutarlı.
- **Akıntı (tahmin):** torpido suyla birlikte sürüklenir, hedef ise zemine bağlıdır. Yan sapma ≈ v_akıntı,yan · t_uçuş. 5 cm/s akıntıda 1 m'de ≈ 2.8 cm, 2 m'de ≈ 10 cm.
  - **Düzeltme:** nişan noktası akıntının geldiği yöne v_akıntı · t_uçuş kadar kaydırılır.
  - v_akıntı kestirimi otopilottan gelir: DVL dip/su izi farkı ya da ILOS integral terimi (kardeş rapor 06).
  - Havuzda bu terim ihmal edilebilir; Teknofest denizde yapıldığı için [K2] gereklidir.
- **Aracın kendi hareketi:** fırlatma anındaki torpido hızı v0·tüp_ekseni + v_araç + ω × r_tüp olur. 5 cm/s yanal kayma, 3 m/s çıkış hızında ≈ 1° sapma ve 1 m'de ≈ 1.7 cm hata demektir. 2° baş-kıç hatası 1 m'de ≈ 3.5 cm hata yaratır (tahmin).
- **Çıkış hızı:** yay enerjisi E = ½ k x² (doğrusal yay), v0 ≈ √(2 η E / m_e). η verimi namlu sürtünmesi ve suyun direnci nedeniyle 1'den çok küçüktür; **ölçülmeli**. Stanford'un 75 N üst sınırı [K3] tek başına çıkış hızını belirlemez. RoboSub'ın "safe speed" kuralı [K1] üst sınırı fiilen kısıtlar.
- **MCU'da kullanım:** tam 3-B entegrasyon ucuzdur (ör. 1 ms adımla 2 s = 2000 adım). Ama atış çözümü için simülasyonda üretilmiş **arama tablosu** (menzil → t_uçuş, düşüş) yeterli ve deterministiktir. Tablo, havuz atışlarıyla kalibre edilmelidir.

#### 2.4.3 Algılamadan atışa toplam gecikmenin etkisi

- **Gecikme zinciri:** τ = poz alma ve kare aktarımı + algılama + takip güncellemesi + karar (durum makinesi periyodu) + (operatör kipinde) insan onayı + tetik gecikmesi (servo/solenoid mekanik) + namlu içi süre.
- **Hata katkısı (tahmin):**
  - Ölçülmemiş göreli hız Δv için e ≈ Δv · τ. Örnek: 5 cm/s, 300 ms → 1.5 cm.
  - Açısal hız ω için e ≈ ω · τ · R. Örnek: 5°/s, 300 ms, 1 m → 2.6 cm.
- **Azaltma yolları:**
  1. Ölçüm, sensörün **yakalama zamanı** ile damgalanır; işlem zamanı kullanılmaz.
  2. İz, t_atış'a ileri kestirilir.
  3. Atış, açısal hız düşükken verilir.
  4. Tetik gecikmesi ölçülüp sabit bir ofset olarak modele katılır.
  5. Operatör kipinde onay "bu iz ve bu çözüm için" verilir. Karar ve tetik yine MCU'da, koşullar o an geçerliyse yapılır. Onay bir **zaman penceresi** açar; tetik zamanını belirlemez.
- Servo ve solenoid tetik gecikmeleri için yayımlanmış takım ölçümü bulunamadı; **doğrulanmadı**, ölçülmeli. METU'nun kuru tezgâh "tetik zamanlaması" testi (TS-HW-05) bunun yapılabileceğini gösteriyor [K8].

### 2.5 Atış kontrol durum makinesi

**Kaynak desenleri (NASA-GB-8719.13 [K26]):**
- Durum makinesinde her durumun her olası girdi için tanımlı bir davranışı olmalı.
- Her durumdan çıkış mümkün olmalı. Girdi gelmezse zaman aşımı geçişi tanımlı olmalı.
- Aralık dışı girdiye karşı davranış tanımlı olmalı.
- Kritik işlevler için ayrı yetkilendirme ve ayrı kontrol işlevi kullanılmalı: ayrı "arm" ve "fire" komutları.
- Tehlikeli diziler tek tuşla başlatılmamalı.
- Sıra dışı mesaj algılanırsa bilinen güvenli duruma dönülmeli.
- Yazılım her zaman bilinen güvenli durumda başlatılmalı.
- Testte kaldırılan kilitlerin test sonunda yeniden devreye girdiği yazılımca doğrulanmalı.
- Tehlikeli işlemin her adımı arasında güncellenen özel bir durum bayrağı tutulmalı.
- Karar mantığında "hep 0" ya da "hep 1" değerleri kullanılmamalı; özel bit desenleri kullanılmalı.

Önerilen durum makinesi Bölüm 3.3'te.

**Operatör onaylı ve tam otonom kip:**
- **Operatör onaylı kip** Teknofest 2026'nın gerçek ihtiyacı: görev manuel ve operatör denizi göremiyor [K2]. Operatör, kamera görüntüsü ve "çözüm geçerli" göstergesiyle karar verir.
- **Tam otonom kip** RoboSub'ın ihtiyacı: dış kontrol yok [K1].
- **İki kip arasındaki tek fark "atış izni"nin kaynağı olmalı.** Emniyet kilitleri, çözüm ölçütleri ve donanım zinciri iki kipte de aynı kalır.
- Otonom kipte izin, görev planındaki **görev kapsamlı bir ön izin** ile verilir. Bu iznin sınırları: görev adımı kimliği, hedef sınıfı, menzil kapısı, en fazla atış sayısı ve zaman penceresi.

### 2.6 Emniyet zinciri

#### 2.6.1 Kavramlar (NASA-GB-8719.13 [K26])

- **Inhibit:** enerji kaynağı ile işlev arasında **fiziksel kesinti** sağlayan tasarım öğesi (röle, transistör vb.). "Software inhibit" tek başına bu anlamda bir inhibit değildir.
- **Bağımsız inhibit:** Tek bir arıza, hata, olay ya da ortam koşulu iki inhibiti birden ortadan kaldıramıyorsa iki inhibit bağımsızdır. "Must not work" işlevlerinde bağımsızlık genellikle **farklı işlemciler ve farklı yazılımlar** gerektirir.
- **İki adımlı komut** ek bir önlemdir ama bir inhibit değildir. Arıza toleransı için ne gerekli ne yeterlidir.
- **Kilitlerin durumu** operatöre görünür olmalı. Her yazılım inhibitinin benzersiz kimliği olmalı. Ön koşullar sağlanmazsa komut reddedilmeli ve operatör uyarılmalı.
- **Watchdog** asla bir kesme içinden beslenmemeli. Görevler kilitlenmişken kesme çalışmaya devam edebilir; bu durumda watchdog anlamsızlaşır.

#### 2.6.2 Yarışma kuralları

- RoboSub kill switch'i itkinin yanında "cihazlar"ın gücünü de kesmeli. Yeniden açılışta güvenli duruma dönülmeli [K1].
- Teknofest acil durdurma butonuna basıldığında motorlar durmalı ve sistem kapanmalı [K2].

#### 2.6.3 STM32U5 olanakları

- **IWDG:** 32 kHz LSI'den saatlenir, yani ana saatten bağımsızdır. Pencere işlevi ve erken uyarı kesmesi var. Zaman aşımı aralığı 16 ms ile 4096 s arası [K31].
- **WWDG:** APB saatinden türer ve bir zaman penceresi uygular [K31].
- **X-CUBE-STL:** ST'nin IEC 61508 SIL2/3 öz-test kütüphanesi. STM32U5'i desteklediği ST blogunda belirtiliyor [K32]. U5 için TÜV sertifikasının kapsamı **doğrulanmadı**.
- **Yarışma bağlamı:** sertifika gerekmiyor. Yine de CPU, RAM ve flash öz-test desenleri açılışta kullanılabilir.

#### 2.6.4 Engelleme koşulları (hibrit araç için)

- **Atış yasak:**
  - Araç yüzeyde ya da yüzey kipindeyse.
  - Derinlik < d_min ise (tüp ve hedef en az bir torpido çapı kadar su içinde olmalı).
  - Araç yüzeyden dalışa ya da dalıştan yüzeye geçiş halindeyse.
- **Yönelim ve hareket:** yalpa ve baş-kıç sınır dışındaysa ya da açısal hız yüksekse atış yasak.
- **Seyrüsefer:** kestirici "geçersiz" ya da "yeniden başlatıldı" durumundaysa atış yasak. Kardeş rapor 06'daki konum sıfırlama sayacı izlenmeli.
- **Haberleşme:**
  - Operatör kipinde heartbeat süresi aşıldıysa SİLAHLI durumdan GÜVENLİ duruma düşülür.
  - Otonom kipte bağlantı kaybı tek başına iptal sebebi olmayabilir. Ancak görev zaman penceresi ve otonom ön izin süresi sınırlı olmalı.
- **Sağlık:** sızıntı sensörü, batarya gerilimi, eyleyici hattı gerilimi, sıcaklık, öz-test sonucu, watchdog sıfırlama nedeni.
- **Fırlatıcı durumu:** tüpte torpido var mı (NTU'nun reed anahtarına benzer bir sensör [K11]), mekanizma kurulu mu, emniyet pimi takılı mı.
- **Hedef:** iz onaylı mı, ölçüm taze mi, menzil kapı içinde mi, hedef sınıfı ve renk doğru mu.

### 2.7 Simülasyon tarafı

#### 2.7.1 Hedef modelleri

**Geometri**
- RoboSub panosu: 0.6 m x 0.6 m düzlem, açıklıklar çokgen olarak [K1][K1b]. Açıklık ölçüleri resmi CAD dosyalarından alınmalı.
- Teknofest: 3 renkli hedef. Nihai teknik çizimler sonradan paylaşılıyor [K2].
- Hedefler eğimli zemine konabilir [K2]. Bu nedenle pano yönelimi rastgeleleştirilmeli.

**Kamera imzası**
- Renk ve doku, su sönümlemesi (kanal başına), geri saçılma, bulanıklık ve ışık değişimi. Ayrıntılar kardeş rapor 05'te.

**Sonar imzası**
- Pano kenarları, çerçeve ve ağırlık noktalarında nokta saçıcılar (hedef kuvveti parametreli).
- Benek (speckle, Rayleigh), yankılanma ve çok yollu yayılım (duvar, yüzey).
- Ping360'ın 2° x 25° ışın deseni ve tarama zamanlaması.

**Hareketli hedef**
- Sabit hız, rastgele yürüyüş ve sinüs yörünge modelleri. Takipçinin doğrulanması için gerekli; yarışmada hareketli hedef yok.

**Hazır simülatörler**
- Stonefish: ileriye bakan sonar doğruluğu iyileştirilmiş; otomatik etiketleme aracı var [K33].
- HoloOcean: görüntüleme, profil, yan taramalı sonar ve yankı ölçer var; gürültü modelleri "küçük sim-gerçek farkı" iddiasında [K34]. HoloOcean yalnız Linux ve Windows destekliyor, **macOS yok** [K34]. Bu, macOS geliştirme hedefiyle çelişir.

#### 2.7.2 Torpido yörüngesi ve isabet değerlendirmesi

- **Model:** Bölüm 2.4.2'deki denklem, 3 serbestlik dereceli nokta kütle olarak. Gerekirse 6 serbestlik dereceli gövde (kanat kararlılığı, spin) eklenir.
- **Başlangıç koşulları:**
  - Fırlatma anındaki araç durumu.
  - Tüp ofseti.
  - Fırlatıcı modeli: v0 dağılımı, açısal sapma, tetik gecikmesi dağılımı. Havuz ölçümleriyle kalibre edilmeli.
- **İsabet testi:** her adımda torpido segmenti ile pano düzleminin kesişimi kontrol edilir (sürekli çarpışma; ince pano içinden atlamayı önler). Kesişim noktası açıklık çokgeninin içinde mi diye bakılır. Torpido yarıçapı hesaba katılarak üç sonuç ayrılır:
  - **temiz geçiş**,
  - **temaslı geçiş**,
  - **panoya çarpma** (RoboSub'ta kısmi puan [K1]).
- **Puanlama:** kural tablosu veri olarak tutulur (yarışma ve yıl başına bir dosya). Örnek: RoboSub 2025 tablosu [K1], Teknofest 2026 tablosu [K2]. Kurallar her yıl değişiyor.

#### 2.7.3 Tekrarlanabilirlik

- **Senaryo dosyası** sürümlü olmalı ve şunları içermeli: hedef yerleşimi, akıntı, sensör gürültü parametreleri, fırlatıcı dağılımları, başlangıç durumu ve tohum.
- **RNG akışları ayrı olmalı:** her alt sistem (sonar gürültüsü, kamera gürültüsü, fırlatıcı, akıntı) kendi tohumlanmış akışını kullanmalı. Böylece bir modeli değiştirmek diğerlerinin rastgele dizisini kaydırmaz.
- **Kayıt:** her koşuda firmware sürümü, simülatör sürümü, senaryo özeti ve tohum kaydedilmeli.
- **SIL:** lockstep ile deterministik çalışır.
- **HIL:** zamanlama deterministik değildir. Kart tarafı zaman damgaları kaydedilmeli ve kayıttan tekrar oynatma (replay) desteklenmeli.
- **Monte Carlo** çıktısı isabet olasılığı haritası olmalı: menzil x yanal hata x akıntı. Bu harita R_max tablosunu ve hizalanma toleranslarını belirler.

#### 2.7.4 Algılama algoritmasının sentetik veriyle sınanması

- **Etiketler:** simülatör kusursuz etiket (ground truth) verir.
- **Ölçütler:**
  - Menzile göre algılama olasılığı (Pd) ve yanlış alarm oranı (Pfa).
  - Konum hatası.
  - İz onaylama süresi, iz sürekliliği ve iz kopması.
  - Uçtan uca: isabet oranı ve yanlış atış sayısı (yanlış atış sıfır olmalı).
- **Sentetik-gerçek farkı:**
  - Stanford, kuru görüntülerle eğitilen modelin su altında kötü genellediğini ve karışık alan eğitiminin gölgeli görüntülerde hassasiyet kaybettiğini raporluyor. Çözüm olarak fizik tabanlı su altı stil aktarımı (JTA-GAN) kullanmışlar [K19].
  - Yan taramalı sonarda sentetik + gerçek veriyle Mask R-CNN AP'sinin belirgin arttığı raporlanıyor [K35]. Atıfta yazar ve tarih bilgisi farklı kaynaklarda çelişiyor; **doğrulanmalı**.
- **Pratik ilkeler:**
  1. Eşikler (CFAR Pfa, renk eşikleri) yalnız sentetik veriyle ayarlanmamalı.
  2. Gerçek havuz kayıtları (Ping360 ham verisi, kamera) erken toplanmalı. Simülatör gürültü modelleri bu kayıtlarla kalibre edilmeli.
  3. Gerçek verinin bir kısmı hiç dokunulmayan test seti olarak ayrılmalı.
  4. Alan rastgeleleştirme (renk, bulanıklık, ışık, yansıtıcılık) yapılmalı.
  5. HIL'de "gerçek kayıt oynatma" kipi bulunmalı: sentetik yerine kaydedilmiş sensör verisi karta akıtılır.

### 2.8 Otopilot ile arayüz

- **Atış kontrolünün otopilottan istediği veriler (her biri zaman damgalı ve geçerlilik bayraklı):**
  - poz (yerel NED konum, tutum),
  - hız (gövde ve dünya),
  - açısal hızlar,
  - derinlik,
  - yüzey/dalış kipi,
  - kestirici sağlık durumu ve sıfırlama sayacı,
  - akıntı kestirimi,
  - kontrol doygunluk durumu.
- **Atış kontrolünün otopilota gönderdiği komutlar (öneri):**
  - `AlignRequest { track_id, hedef_göreli_poz (R*, azimut*=0, yükseklik*=0), toleranslar, en_fazla_hızlar, tutma_süresi, son_geçerlilik }`
  - `HoldStation` (atış penceresinde istasyon tutma).
  - `ReleaseControl` / `Abort`.
- **Otopilottan geri bildirim:** `AlignStatus { yakınsadı mı, kalan hata, doygunluk }`.
- **Örnek uygulamalar:** METU'da görsel servo çıktısı doğrudan itki karıştırıcısına gidiyor [K8]. NTU'da davranış ağacı (x, y, z, r) komutları gönderiyor [K11]. MAVLink tabanlı bir otopilotta karşılığı `SET_POSITION_TARGET_LOCAL_NED`; ArduSub'da bu komut Guided kip ve yeterince sık akış (bir kullanıcıda 25 Hz) gerektiriyor [K36] (forum, **doğrulanmadı**).
- **Aynı kartta birlikte çalışmanın zamanlama etkisi:**
  - Kardeş rapor 06, kontrol hattını en yüksek öncelikli `InterruptExecutor`'a koymayı öneriyor.
  - Atış kontrolü orta öncelikte 20-50 Hz yeterli (tahmin). İşlemi kısa ve sınırlı süreli.
  - Kart üstü görüntü işleme (HIL'de) en düşük öncelikte ve kesilebilir olmalı. Kare başına onlarca ms sürer (tahmin) ve kontrol döngüsünü asla geciktirmemeli.
  - DMA (USB, DCMI) ile bellek veri yolu çekişmesi kontrol döngüsünün jitter'ını etkileyebilir. DWT çevrim sayacıyla ölçülmeli.
  - **Tetik anı yüksek öncelikli bağlamda verilmeli.** Darbe donanım zamanlayıcısının tek darbe kipiyle üretilmeli; yazılım yalnız başlatır. Tetik zamanı ve o anki seyrüsefer durumu aynı kayıtta saklanmalı.

---

## 3. Mimari için öneriler

### 3.1 İş bölümü (kart ve yardımcı bilgisayar)

**Aşama 1: şimdi, yalnız bu kart ve PC (HIL/SIL)**
- **Kartta:**
  - atış kontrol durum makinesi,
  - emniyet kilitleri ve inhibit yönetimi,
  - eyleyici sürücüleri (servo PWM ya da solenoid darbesi, tüp başına),
  - takipçi (Kalman + GNN),
  - atış çözümü (balistik tablo),
  - Ping360/Ping2 algılayıcı (CFAR + kümeleme),
  - isteğe bağlı olarak HIL'de akıtılan düşük çözünürlüklü karelerde klasik renk/kontur algılayıcı (deneysel, en düşük öncelik).
- **PC simülatöründe:** sensör üretimi ve isteğe bağlı bir "sanal yardımcı bilgisayar" süreci. Bu süreç, simülasyonda kameradan YOLO benzeri ya da doğrudan gürültülü kusursuz-etiket algılamaları üretip karta gönderir.

**Aşama 2: gerçek araç**
- Kamera ve ağır algılama yardımcı bilgisayara (Jetson/RPi) gider. Takım raporlarının tamamı bu ayrımı kullanıyor [K3][K5][K6][K8][K9].
- Kart; atış kontrolü, takip, çözüm, sonar ve emniyetin sahibi olarak kalır.
- Yardımcı bilgisayar çökerse kart, sonarla hedef konumunu korur ama **atış izni vermez**. Görsel doğrulama atış izni için zorunlu bir koşul olmalı.

**Aşama 3: kart değişirse (NPU + kamera girişli MCU)**
- Aynı algılama arayüzüyle algılayıcı karta taşınır. Atış kontrolü değişmez.

**Gerekçe**
- Emniyet kritik karar, seyrüsefer verisiyle aynı saat alanında ve deterministik bir ortamda olmalı.
- Ağır algılama donanım bağımlıdır ve en çok değişecek parçadır.

### 3.2 Algılama arayüzü (yer değiştirebilir olsun diye)

Sınır, **ölçüm (detection) seviyesinde** çizilmeli: ham görüntü ya da iz değil, tek bir sensör gözlemi. Önerilen mesaj (sürümlü, ikili kodlama; ör. `postcard` + COBS, USB/UART/CAN-FD üzerinde):

```
Detection v1 {
  seq: u32,                    // sıra numarası; sıra dışı/kayıp tespiti
  t_capture_us: u64,           // ortak zaman tabanında YAKALAMA zamanı
  source: SensorId,            // cam_front, ping360, ping2, sim_truth...
  frame: FrameId,              // sensör çerçevesi (extrinsics kartta tanımlı)
  kind: Bearing{az, el} | RangeBearing{r, az, el?} | Range{r} | Pose6D{...},
  cov: [f32; N],               // ölçüm kovaryansı (algılayıcı söyler)
  class: TargetClass,          // board, hole_large, hole_small, red, green, blue...
  confidence: f32,
  producer: ProducerId + version   // hangi algoritma/model sürümü
}
```

- **Takip ve atış kontrolü yalnız bu mesajı tüketir.** Algılamanın kartta, yardımcı bilgisayarda ya da simülatörde (kusursuz etiket + gürültü) yapılması fark etmez. Bu, HIL'de aşamalı test sağlar:
  1. Kusursuz-etiket algılamaları + gürültü.
  2. Sentetik ham veri → kart üstü algılayıcı.
  3. Kayıt oynatma.
  4. Gerçek yardımcı bilgisayar.
- **Zaman eşlemesi zorunlu.** Ortak zaman tabanı ve basit bir NTP benzeri ofset kestirimi kullanılmalı. Yakalama zamanı yoksa takip ileri kestirimi bozulur (Bölüm 2.4.3).
- **Kovaryansı algılayıcı vermeli.** Atış izni kovaryansa bağlı olduğundan "güven" puanı tek başına yetmez.
- **Rust uygulaması:** kartta `trait DetectionSource` arayüzü. Uygulamaları: `Ping360Detector`, `HilFrameDetector`, `LinkDetector` (dış kaynak).

### 3.3 Durum makinesi önerisi

**Durumlar**

| Durum | Anlamı | Enerji hattı | Zaman aşımı |
|---|---|---|---|
| `SAFE` | Açılış ve her sıfırlama sonrası varsayılan. Tüm tetik çıkışları pasif | Kapalı | - |
| `ARMING` | Ön kontroller: donanım silahlandırma anahtarı takılı mı, kill switch normal mi, öz-test (çıkış takılı mı), sensör tazeliği, yüzey/derinlik, envanter | Kapalı | ör. 2 s → `SAFE` |
| `ARMED` | Yazılım inhibiti #1 kaldırıldı, hedef yok | Kapalı | Görev penceresi → `SAFE` |
| `TRACKING` | Seçili hedefte onaylı iz var; otopilota `AlignRequest` gönderildi | Kapalı | İz kaybı ya da hizalanma süresi aşımı → `ARMED` |
| `SOLUTION_VALID` | Bölüm 2.4.1 ölçütleri T_dwell boyunca sağlandı. Operatöre "çözüm geçerli" gösterilir | Kapalı | Ölçüt bozulursa → `TRACKING` |
| `FIRE_AUTHORIZED` | İzin alındı (operatör nonce'u ya da otonom ön izin). Enerji hattı açılır, hat gerilimi geri okunur | **Açık** | ör. 300 ms → `ARMED` (enerji kesilir) |
| `FIRING` | Tüp N için donanım zamanlayıcısıyla tek darbe | Açık | Darbe sonu → `ASSESS` |
| `ASSESS` | Enerji kesilir. Torpido çıktı mı (varlık sensörü, solenoid akım imzası), sonuç kaydedilir, envanter azaltılır, otopilota `HoldStation`/`Release` gönderilir | Kapalı | ör. 3 s → `ARMED` ya da `SAFE` |
| `FAULT_LATCHED` | Herhangi bir arıza. Enerji kapalı, durum kilitli | Kapalı | Yalnız açık "reset" komutu + koşullar → `SAFE` |
| `MAINTENANCE` | Yalnız `SAFE`'ten, fiziksel bakım jumper'ı takılıyken ve tüpler boşken. Kuru atış ve eyleyici testi | Testte açık | Çıkışta kilitlerin yeniden etkin olduğu doğrulanır [K26] |

**Evrensel geçişler** (her durumdan `SAFE` ya da `FAULT_LATCHED` durumuna):
- disarm komutu,
- kill switch,
- silahlandırma anahtarının çıkması,
- yüzey kipine geçiş ya da derinlik < d_min,
- yönelim veya açısal hız sınır dışı,
- seyrüsefer geçersiz,
- sızıntı,
- (operatör kipinde) heartbeat kaybı,
- sıra dışı ya da CRC hatalı komut,
- watchdog sıfırlaması (açılışta sıfırlama nedeni okunur; **otomatik yeniden silahlanma yok**).

**Komut kuralları (NASA desenleri [K26]):**
- `ARM` ve `FIRE` ayrı komutlardır. `ARM` için sabit, özel bir 32-bit desen kullanılır; 0x00000000 ya da 0xFFFFFFFF kullanılmaz.
- `FIRE`, o an geçerli olan `track_id`, `tube_id` ve tek kullanımlık bir `nonce` taşır. Nonce, `SOLUTION_VALID` durumuna girerken kart tarafından üretilip operatöre gönderilir. Böylece bayat bir onay yeni bir çözümde kullanılamaz.
- Her komut sıra numarası ve CRC taşır. Ön koşul sağlanmazsa komut reddedilir ve sebebi bildirilir.
- Tüm inhibitlerin durumu (benzersiz kimlikle) telemetride sürekli yayınlanır.

**Kipler:**
- `OperatorConfirm`: `SOLUTION_VALID → FIRE_AUTHORIZED` geçişi yalnız geçerli nonce'lu `FIRE` ile olur.
- `Autonomous`: aynı geçiş, görev planındaki ön iznin sınırları içinde (görev adımı, hedef sınıfı, menzil kapısı, kalan atış hakkı, zaman penceresi) otomatik olur.
- Diğer tüm kilitler iki kipte aynıdır.

**Rust notu:**
- Durumlar `enum` olarak tanımlanır ve `match` kapsamlı olmak zorunda olduğu için "her durum her girdiyi işler" kuralı derleyiciyle zorlanır.
- Tetik sürücüsü bir "typestate" tutamacı olur: `fire()` metodu yalnız `Authorized` belirteciyle elde edilen türde bulunur. Belirteç yalnız durum makinesinin tek bir fonksiyonunda üretilir. Bu, çalışma zamanı kontrollerinin yerine değil, yanına konur.
- Durum makinesi `no_std`, donanımdan bağımsız ayrı bir crate olmalı. Böylece SIL'de PC'de birebir aynı kod koşar ve özellik tabanlı testlerle tüm geçiş tablosu taranabilir.

### 3.4 Emniyet zinciri önerisi (donanım + yazılım)

Eyleyici enerji yolunda seri öğeler, kaynaktan tetiğe doğru:
1. **Kill switch / acil durdurma:** itki ve eyleyici hattını keser. RoboSub ve Teknofest bunu zorunlu tutuyor [K1][K2].
2. **Fiziksel silahlandırma fişi ya da anahtarı** (eyleyici hattında). Bakım ve taşımada çıkarılır. RoboSub'ın "çadırda dolu torpido yok" kuralıyla uyumlu bir işletme prosedürü [K1].
3. **Mekanik emniyet pimi** (fırlatıcıda, "suya girmeden çıkar" etiketli). Elektrik ne yaparsa yapsın mekanizmayı kilitler.
4. **MCU kontrollü yüksek taraf anahtarı (yazılım kontrollü inhibit).** Sürekli bir GPIO seviyesiyle değil, zamanlayıcının ürettiği **dinamik bir sinyalle** (ör. şarj pompası ya da kondansatör kuplajlı sürücü) açık tutulmalı. MCU kilitlenirse ya da pin takılı kalırsa anahtar kendiliğinden kapanır. Bu, NASA'nın "hep 0 / hep 1 değerine güvenme" ilkesinin donanım karşılığıdır (öneri; **kaynakla doğrulanmış bir standart devre değildir**).
5. **Tüp başına alçak taraf sürücüsü (tetik).**
   - Ayrı bir GPIO portundan sürülmeli.
   - Darbe süresi donanım zamanlayıcısının tek darbe kipiyle sınırlanmalı.
   - Solenoid için serbest dönüş diyodu kullanılmalı.
   - NASA, kritik ve kritik olmayan işlevler için aynı I/O portunun kullanılmamasını öneriyor [K26].
6. **Geri okumalar:** hat gerilimi (ADC), solenoid ya da servo akımı, tüpte torpido varlığı, mekanizmanın kurulu olduğu.

**Önemli kısıtlar**
- Madde 4 ve 5 aynı MCU'dan sürüldüğü için NASA tanımına göre **bağımsız değildir** [K26]. Gerçek bağımsızlık 1-3 numaralı fiziksel öğelerden gelir.
- Operatör kipinde ek bağımsız bir inhibit olarak kontrol masasında fiziksel bir "atış izni" anahtarı önerilir. Teknofest temel kategoride kablolu araca izin veriyor [K2].
- Kart 1.8 V mantık seviyesinde çalışıyor (UM2967 [K12], kardeş rapor 01). Servo PWM ve MOSFET kapı sürücüsü için seviye çevirici gerekli.

**Watchdog**
- IWDG (LSI) kullanılır. Yalnız bir "monitör" görevi besler; o da her kritik görevin (kontrol, atış kontrol, haberleşme) kalp atışı bayrağı tazeyse besler. Kesmeden asla beslenmez [K26].
- Açılışta RCC sıfırlama bayrakları okunur. IWDG kaynaklı sıfırlamada `SAFE` durumunda kalınır, olay kaydedilir ve operatör bilgilendirilir.

**Test ve bakım kipi**
- Yalnız fiziksel jumper ile girilir.
- Tüplerin boş olduğu sensörle doğrulanır.
- Kuru atış sayaçları tutulur. METU 50+ kuru atış döngüsüyle sıkışma sorununu bu yolla bulmuş [K8].
- Kipten çıkarken tüm inhibitlerin yeniden etkin olduğu yazılımca doğrulanır [K26].

### 3.5 Simülasyon ve test önerisi

1. **Önce SIL:** durum makinesi, takipçi ve balistik crate'leri PC'de lockstep ile koşar. Senaryo dosyaları ve Monte Carlo ile R_max ve tolerans tabloları üretilir.
2. **Sonra HIL:** kart, PC'den sonar ham verisi (Ping protokolü benzeri) ve/veya `Detection` mesajları alır. Atış komutu simülatöre geri gider. Simülatör torpidoyu uçurup puanlar.
3. **Arıza enjeksiyonu senaryoları (HIL'de zorunlu test seti):**
   - bağlantı kopması,
   - seyrüsefer sıfırlaması,
   - ani yüzeye çıkma,
   - sensör donması (aynı sıra numarası),
   - sıra dışı komut,
   - bayat nonce,
   - watchdog tetiklenmesi,
   - eyleyici geri okuma hatası.
   - Kabul ölçütü: **sıfır yanlış atış**, her durumda `SAFE`/`FAULT_LATCHED` durumuna dönüş.
4. **Simülatör seçimi:** HoloOcean macOS'u desteklemiyor [K34]. Atış görevine özgü hedef ve balistik modelinin, simülatörden bağımsız küçük bir modül olarak yazılması önerilir. Sensör üretimi kardeş raporlar 02 ve 05 ile uyumlu olmalı.

---

## 4. Riskler ve açık sorular

| # | Risk / soru | Etki | Durum / öneri |
|---|---|---|---|
| R1 | Kartta kamera girişi yok (DCMI dışarı çıkmıyor, USB yalnız cihaz) | Kart üstü görsel algılama yalnız HIL'de denenebilir | Gerçek araçta yardımcı bilgisayar ya da kart değişimi şart |
| R2 | U5'te CNN algılama ~1-2 s/kare (tahmin) | TinyML ile hizalanma döngüsü kurulamaz | Klasik yöntem + yardımcı bilgisayar; TinyML yalnız doğrulama sınıflandırıcısı |
| R3 | Kurallar yıldan yıla değişiyor (Teknofest 2026: torpido yalnız temel kategoride ve manuel; pnömatik yasak) | Tasarım bir yıla kilitlenirse boşa gider | Kural tablosunu veri yap; iki kip (operatör/otonom) baştan |
| R4 | Pano malzemesinin sonar yansıtıcılığı bilinmiyor | Sonarla hedef bulma başarısız olabilir | Havuzda Ping360 kaydı al; simülatör hedef kuvvetini buna göre ayarla |
| R5 | Su altı renk bozulması; Teknofest hedefleri kırmızı/yeşil/mavi | Renk sınıflandırma hataları, yanlış hedefe atış | Renk sabitliği, çoklu ipucu (şekil + konum), görsel doğrulama zorunlu |
| R6 | Tek MCU'da inhibitler bağımsız değil | Tek yazılım hatası istenmeyen atışa yol açabilir | Fiziksel inhibitler + dinamik enable + operatör anahtarı |
| R7 | Tetik gecikmesi ve çıkış hızı dağılımı bilinmiyor | Atış çözümü hatalı | Tezgâhta ve havuzda ölç; tabloyu kalibre et |
| R8 | Yüzerlik ve akıntı hassasiyeti (2 m'de ~10-14 cm, tahmin) | Uzak atışta ıska | Kısa menzil tercih; torpidoyu nötr yüzerliğe ayarla; akıntı düzeltmesi |
| R9 | Zaman eşlemesi (yardımcı bilgisayar ↔ kart) | İz ileri kestirimi bozulur | Yakalama zamanı damgası + ofset kestirimi; test senaryosu |
| R10 | Embassy'de U5 USB HS desteği yeni ve donanımda doğrulanmamış (kardeş rapor 01) | HIL kare akışı gecikebilir | Spike; yedek: düşük çözünürlük ya da yalnız `Detection` akışı |
| R11 | Sentetik-gerçek farkı [K19] | Simülasyonda iyi, havuzda kötü | Erken gerçek veri toplama, kayıt oynatma, ayrılmış test seti |
| R12 | Hibrit araçta yüzey/dalış geçişinde kestirici sıfırlaması | Yanlış göreli konumla atış | Sıfırlama sayacı değişirse iz yeniden onaylanana kadar atış yasak |

**Açık sorular**
1. Hedef yarışma hangisi ve hangi yıl? Teknofest 2026 torpidosu yalnız temel kategoride ve manuel [K2]. Proje ileri kategoriye yönelikse torpido o kategoride hiç olmayabilir.
2. Açıklık ölçüleri ve Teknofest hedef çizimleri (henüz paylaşılmadı [K2]).
3. Fırlatıcı türü: servo mandal mı, solenoid mi? Bu seçim sürücü devresini ve gecikmeyi belirler.
4. Kaç tüp? RoboSub en fazla 2, Teknofest en fazla 5 torpido taşınmasına izin veriyor.
5. Gerçek araçta yardımcı bilgisayar olacak mı ve hangisi? Kamera ona bağlanacak.
6. Sonar seçimi: Ping360 mı, çok ışınlı mı? Çok ışınlı seçilirse algılama yardımcı bilgisayara kayar.

---

## 5. Bağımsız doğrulanması gereken kritik iddialar

1. **RoboSub 2025 torpido kuralları:** her torpido 51x51x152 mm kutuya sığmalı, havada en fazla 0.91 kg olmalı, araç en fazla 2 torpido taşıyabilir, torpido "safe speed" ile gitmeli. Kill switch bataryaları "all propulsion components and devices"tan ayırmalı. Kaynak: https://robonation.org/app/uploads/sites/4/2025/07/2025-RoboSub_Team-Handbook-07_21.pdf (Bölüm 5.3.1, 5.4.2)
2. **Teknofest 2026 torpido görevi:** yalnız temel kategoride, manuel icra; 3 renkli hedef, en fazla 5 atış, atış başına 20 puan; fırlatmada patlayıcı, pnömatik ve hidrolik sistemler yasak. Kaynak: https://cdn.teknofest.org/media/upload/userFormUpload/2026_%C4%B0NSANSIZ_SU_ALTI_S%C4%B0STEMLER%C4%B0_YARI%C5%9EMASI_-_T%C3%9CRK%C3%87E_-_13.07.2026_jOTfB.pdf (Bölüm 6.2)
3. **STM32U5A9J-DK'de kamera konnektörü yok ve genişleme konnektörlerinde DCMI sinyali listelenmiyor; USB HS "sink only"; kart yalnız 1.8 V VDD için tasarlanmış.** Kaynaklar: https://evo.net.ua/content/files/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics-12836994.pdf (UM2967 ayna kopya), https://docs.zephyrproject.org/latest/boards/st/stm32u5a9j_dk/doc/index.html
4. **`st_yoloxn d033_w025` 192x192 int8: STM32H747I-DISCO (400 MHz) üzerinde 357.79 ms, STM32N6570-DK NPU'da 6.63 ms; STM32U5 desteklenmiyor/önerilmiyor.** Kaynak: https://huggingface.co/STMicroelectronics/st_yoloxn/blob/main/README.md
5. **Ping360: 2° yatay x 25° dikey ışın, en kısa menzil 0.75 m, 360° tarama 1 m'de 3.42-4.26 s, 50 m'de 33 s.** Kaynak: https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/
6. **NASA-GB-8719.13:** ayrı "arm" ve "fire" komutları; iki adımlı komut bir inhibit değildir; bağımsız inhibit tanımı (tek arıza ikisini birden kaldıramaz) ve "must not work" işlevlerinde farklı işlemci/yazılım gereği; watchdog kesmeden beslenmemeli. Kaynak: https://s3vi.ndc.nasa.gov/ssri-kb/static/resources/nasa-gb-8719.13.pdf (Bölüm 6.5.3, 7.4, Ek E, Sözlük)
7. **Stanford 2025 fırlatıcısı:** iki torpido, her biri en fazla 75 N yayla; tek servo, kapak-tetik-çekiç zinciri. Kaynak: https://robonation.org/app/uploads/sites/4/2025/07/RS25_TDR_Stanford-University-AUV-compressed.pdf (Bölüm 2.1.7)
8. **STM32U575I-EV + OV5640, 320x240 RGB565: piksel saati 12 → 24 MHz ile 7.5 → 15 fps** (topluluk makalesi). Kaynak: https://community.st.com/stm32-mcus-60/how-to-improve-dcmi-frame-rate-an-example-using-a-ov5640-camera-module-127871
9. **HoloOcean yalnız Linux ve Windows destekliyor (macOS yok).** Kaynak: https://byu-holoocean.github.io/holoocean-docs/v2.0.0/index.html
10. **Stanford 2026: kuru görüntülerle eğitilen modeller su altında kötü genelleşmiş; JTA-GAN ile fizik tabanlı stil aktarımı kullanılmış.** Kaynak: https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_StanfordAUV.pdf

---

## 6. Kaynaklar

Yarışma kuralları ve takım raporları
- [K1] RoboNation, 2025 RoboSub Team Handbook (07-21). https://robonation.org/app/uploads/sites/4/2025/07/2025-RoboSub_Team-Handbook-07_21.pdf
- [K1b] RoboNation, RoboSub 2026 Team Handbook, 3.2 Task Descriptions. https://robonation.gitbook.io/robosub-resources/section-3-autonomy-challenge/3.2-task-descriptions
- [K2] TEKNOFEST, 2026 İnsansız Su Altı Sistemleri Yarışması Şartnamesi (Türkçe, 13.07.2026). https://cdn.teknofest.org/media/upload/userFormUpload/2026_%C4%B0NSANSIZ_SU_ALTI_S%C4%B0STEMLER%C4%B0_YARI%C5%9EMASI_-_T%C3%9CRK%C3%87E_-_13.07.2026_jOTfB.pdf ; yarışma sayfası: https://www.teknofest.org/tr/yarismalar/insansiz-su-alti-sistemleri-yarismasi/
- [K2b] Haber: TEKNOFEST Mavi Vatan'da İnsansız Su Altı Sistemleri Yarışması (2024 görev başlıkları). https://uzmanpara.milliyet.com.tr/uzmanpara/teknofest-mavi-vatanda-insansiz-su-alti-sistemleri-yarismasi-7645793
- [K3] Stanford University, RoboSub 2025 TDR. https://robonation.org/app/uploads/sites/4/2025/07/RS25_TDR_Stanford-University-AUV-compressed.pdf
- [K4] Istanbul Technical University, RoboSub 2024 TDR. https://robonation.org/app/uploads/sites/4/2024/07/RS24_TDR_IstanbulTechU.pdf
- [K5] Istanbul Technical University, RoboSub 2025 TDR. https://robonation.org/app/uploads/sites/4/2025/07/RS25_TDR_Istanbul-Technical-University-AUV-compressed.pdf
- [K6] University of Victoria, RoboSub 2026 TDR. https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_Univ-of-Victoria.pdf
- [K7] UC Berkeley, RoboSub 2026 TDR. https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_UCBerkeley.pdf
- [K8] Middle East Technical University, RoboSub 2026 TDR. https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_MiddleEastTechnicalUniversity.pdf
- [K9] Université de Sherbrooke, RoboSub 2026 TDR. https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_UniversitedeSherbrooke.pdf
- [K10] Universidade de Brasília (All Blue), RoboSub 2026 TDR. https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_UniversidadeBrasilia_AllBlue.pdf
- [K11] Nanyang Technological University (Mecatron), RoboSub 2025 TDR. https://robonation.org/app/uploads/sites/4/2025/07/RS25_TDR_Nanyang-Technological-University-Mecatron.pdf
- [K19] Stanford AUV, RoboSub 2026 TDR. https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_StanfordAUV.pdf
- [K21] University of Colorado Boulder, RoboSub 2020 TDR. https://robonation.org/app/uploads/sites/4/2020/08/RS20_TDR_UC-Boulder.pdf

Donanım ve üretici belgeleri
- [K12] ST, UM2967 Discovery kit with STM32U5A9NJ MCU (ayna kopya; asıl adres: https://www.st.com/resource/en/user_manual/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics.pdf). https://evo.net.ua/content/files/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics-12836994.pdf
- [K13] ST, STM32U5A9NJ ürün sayfası (DCMI, PSSI, NeoChrom GPU2D, 2.5 MB SRAM, 160 MHz). https://www.st.com/en/microcontrollers-microprocessors/stm32u5a9nj.html
- [K14] ST, STM32U5A9J-DK ürün sayfası ve Zephyr kart belgesi. https://www.st.com/en/evaluation-tools/stm32u5a9j-dk.html ; https://docs.zephyrproject.org/latest/boards/st/stm32u5a9j_dk/doc/index.html
- [K15] STMicroelectronics, st_yoloxn model kartı (Hugging Face). https://huggingface.co/STMicroelectronics/st_yoloxn/blob/main/README.md
- [K15b] STMicroelectronics, mobilenetv2_pt model kartı. https://huggingface.co/STMicroelectronics/mobilenetv2_pt/blob/main/README.md
- [K16] ST Community, "How to improve DCMI frame rate: an example using a OV5640 camera module". https://community.st.com/stm32-mcus-60/how-to-improve-dcmi-frame-rate-an-example-using-a-ov5640-camera-module-127871
- [K16b] ST, AN5020 Introduction to DCMI for STM32 MCUs. https://www.st.com/resource/en/application_note/an5020-introduction-to-digital-camera-interface-dcmi-for-stm32-mcus-stmicroelectronics.html
- [K17] SparkFun, OpenMV Cam H7 Plus ürün sayfası (QVGA'da 25-50 fps ifadesi); OpenMV forumu. https://www.sparkfun.com/openmv-cam-h7-plus.html ; https://forums.openmv.io/t/openmv-cam-h7-plus-resolutions/1729
- [K17b] MLPerf Tiny 1.0 sonuçları üzerine Plumerai yazısı (STM32L4R5 VWW 230.5 ms). https://www.edge-ai-vision.com/?p=39779 ; MLPerf Tiny makalesi: https://arxiv.org/abs/2106.07597
- [K18] Blue Robotics, Ping360 ürün ve teknik özellik sayfası. https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/
- [K18b] Blue Robotics, Ping Sonar (echosounder) sayfası. https://bluerobotics.com/store/sonars/echosounders/ping-sonar-r2-rp/
- [K18c] Blue Robotics, Ping Protocol - Ping360 mesajları. https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/
- [K20] Blueprint Subsea, Oculus M-series teknik özellikleri. https://www.blueprintsubsea.com/oculus/oculus-m-series
- [K29] OpenMV, Servo control eğitimi (50 Hz, 1-2 ms darbe). https://docs.openmv.io/openmvcam/tutorial/hardware/pwm/servos.html
- [K31] RM0456 (STM32U5) IWDG/WWDG bölümleri (ikincil HTML) ve ST HAL IWDG kılavuzu. https://stm32.dirba.io/rm0456-stm32u5/61-independent-watchdog-iwdg/ ; https://stm32.dirba.io/rm0456-stm32u5/62-system-window-watchdog-wwdg/ ; https://dev.st.com/stm32cube-docs/stm32u5-hal2/2.0.0-beta.1.1/docs/drivers/hal_drivers/iwdg/hal_iwdg_how_to_use.html
- [K32] ST blog, X-CUBE-STL'nin STM32U5 desteği; X-CUBE-STL veri özeti. https://blog.st.com/?p=43097 ; https://www.st.com.cn/resource/en/data_brief/x-cube-stl.pdf

Algılama, takip ve emniyet literatürü
- [K22] I. Quidu, L. Jaulin, A. Bertholom, Y. Dupas, "Robust Multitarget Tracking in Forward-Looking Sonar Image Sequences Using Navigational Data," IEEE J. Oceanic Eng., 37(3), 2012. https://hal-ensta-bretagne.archives-ouvertes.fr/hal-00773673
- [K22b] I. Karoui, I. Quidu, M. Legris, "Automatic Sea-Surface Obstacle Detection and Tracking in Forward-Looking Sonar Image Sequences," IEEE TGRS 53(8), 2015, DOI 10.1109/TGRS.2015.2467396. https://hal.archives-ouvertes.fr/hal-01203532
- [K22c] Sheng vd., "Clustering Cloud-Like Model-Based Targets Underwater Tracking for AUVs," Sensors 19(2):370, 2019. https://www.mdpi.com/1424-8220/19/2/370
- [K23] P. R. Kalata, "The Tracking Index: A Generalized Parameter for α-β and α-β-γ Target Trackers," IEEE TAES AES-20(2), 1984, DOI 10.1109/TAES.1984.310438. https://researchdiscovery.drexel.edu/esploro/outputs/journalArticle/The-Tracking-Index-A-Generalized-Parameter/991019173672304721
- [K24] MathWorks, trackerGNN ve atama yöntemlerine giriş. https://www.mathworks.com/help/fusion/ref/trackergnn-system-object.html ; https://www.mathworks.com/help/fusion/ug/introduction-to-assignment-methods-in-tracking-systems.html
- [K25] MathWorks, Constant False Alarm Rate (CFAR) Detection. https://www.mathworks.com/help/phased/ug/constant-false-alarm-rate-cfar-detection.html
- [K26] NASA-GB-8719.13, NASA Software Safety Guidebook (2004). https://s3vi.ndc.nasa.gov/ssri-kb/static/resources/nasa-gb-8719.13.pdf
- [K27] R. David vd., "TensorFlow Lite Micro: Embedded Machine Learning for TinyML Systems," MLSys 2021. https://proceedings.mlsys.org/paper_files/paper/2021/hash/6c44dc73014d66ba49b28d483a8f8b0d-Abstract.html
- [K28] T. I. Fossen, Handbook of Marine Craft Hydrodynamics and Motion Control, 2. baskı, Wiley, 2021 (Bölüm 6, eklenen kütle). https://www.wiley-vch.de/en/areas-interest/engineering/handbook-of-marine-craft-hydrodynamics-and-motion-control-978-1-119-57505-4
- [K30] NASA Glenn, Drag Coefficient. https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/drag-coefficient/

Simülasyon ve sentetik-gerçek farkı
- [K33] M. Grimaldi vd., "Stonefish: Supporting Machine Learning Research in Marine Robotics," ICRA 2025. https://arxiv.org/abs/2502.11887
- [K34] HoloOcean belgeleri v2.0.0. https://byu-holoocean.github.io/holoocean-docs/v2.0.0/index.html
- [K35] "Syn2Real Domain Generalization for Underwater Mine-like Object Detection Using Side-Scan Sonar," arXiv 2410.12953. https://arxiv.org/abs/2410.12953v1

Otopilot arayüzü
- [K36] Blue Robotics forumu, Pymavlink SET_POSITION_TARGET ile hız; MAVLink Offboard Control. https://discuss.bluerobotics.com/t/pymavlink-set-position-target-with-velocity/12839 ; https://mavlink.io/en/services/offboard_control.html

Kardeş raporlar (bu depo)
- `docs/research/01-kart-ve-arac-zinciri.md` (kart kısıtları, 1.8 V, USB HS, Embassy durumu)
- `docs/research/06-otopilot.md` (executor öncelikleri, kestirici, ILOS)
