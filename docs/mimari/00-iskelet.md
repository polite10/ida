# Mimari iskelet

Sürüm: 0.3 (2026-11-13). Durum: taslak; kullanıcı onayı bekliyor.

Sürüm 0.2 değişiklikleri: fizik adımı 1 ms yerine 1,25 ms (kontrol periyodunun tam böleni olması için); ölçüme bağlı yedeklerin kalite kademesi olmadığı notu.

Sürüm 0.3 değişiklikleri (02 ve 04 numaralı bölümlerin sol incelemelerinden çıkan, bölümler arası kararlar): ortak kabuller (3.1); tek seferlik olayların sürekli komutlardan ayrılması (3.2); zaman çerçevesi ve iki ölçütlü tazelik (4); tekrar oynatma için kayıt gereği (A6); torpidonun kütle muhasebesi ve fırlatma modeli kademesi (5).

Bu belge sistemin parçalarını, parçalar arasındaki arayüzleri ve bölümlerin sınırlarını sabitler. Mimari bölümleri (`01`–`09`) bu iskelete göre yazılır. Bir bölüm iskeletle çelişirse iskelet geçerlidir; iskeleti yalnızca mimar oturum değiştirir. Kararların dayanağı [../research/00-duzeltmeler-ve-kararlar.md](../research/00-duzeltmeler-ve-kararlar.md) belgesidir: `K` kesin karar, `Ö` öneri, `M` ölçülecek konu, `Ç` çözülmüş çelişki.

## 1. Amaç ve kapsam

Tek hedef: hibrit insansız deniz aracının simülasyon ortamını kurmak ve otopilot ile atış kontrol yazılımını STM32U5A9J-DK kartında bu ortama bağlı çalıştırmak (K8). Gerçek sensör, ardıl kart ve araç donanımı kapsam dışıdır.

Bunun sonucu: karta giden bütün sensör verisi mesaj olarak enjekte edilir. Sensörlerin kablo protokollerini (UART, I2C, SPI) taklit etmek ek donanım gerektirdiği için bu aşamada yapılmaz; sürücü katmanının sınırı yine de gerçek sürücülerin sonradan eklenebileceği yerde tutulur (A4).

## 2. Parçalar

```
┌────────────── PC (macOS / Windows) ──────────────────────────────┐
│                                                                  │
│  ┌──────────────── Sim sunucusu (C++, başsız) ────────────────┐  │
│  │ fizik · çevre · araç · sensör modelleri · sonar · güç      │  │
│  │ zaman yöneticisi · HIL köprüsü · bağlantı emülatörü        │  │
│  │ senaryo · hata enjeksiyonu · kayıt · API sunucusu          │  │
│  └───┬───────────────┬──────────────────┬─────────────────┬───┘  │
│   A1 │            A2 │               A3 │              A1 │      │
│      │   ┌───────────┴────────┐ ┌───────┴────────┐ ┌──────┴───┐  │
│      │   │ Görselleştirici    │ │ Web panelleri  │ │ SIL      │  │
│      │   │ (Godot, ayrı süreç)│ │ (tarayıcı)     │ │ koşucusu │  │
│      │   │ 3B görünüm, kamera │ │ harita, grafik │ │ çekirdek │  │
│      │   └────────────────────┘ │ senaryo, hata  │ │ PC'de    │  │
│      │                          └────────────────┘ └──────────┘  │
└──────┼───────────────────────────────────────────────────────────┘
       │ USB
┌──────┴──────────── STM32U5A9J-DK ────────────────────────────────┐
│ kart destek katmanı (BSP) · bağlantı · sensör/aktüatör portu (A4)│
│ ortak çekirdek: kestirim · güdüm · kontrol · görev · arıza       │
│ atış kontrol · sonar verisinde hedef algılama                    │
└──────────────────────────────────────────────────────────────────┘
```

| Parça | Dil | Sorumluluk | Bölüm |
|---|---|---|---|
| Kart yazılımı | C17 | Donanımı başlatır, bağlantıyı yürütür, portu (A4) uygular, çekirdeği zamanında çağırır | 01 |
| Ortak çekirdek | C17 | Kestirim, güdüm, kontrol, görev yönetimi, arıza güvenliği. Donanıma ve işletim sistemine bağımlı değildir; hem karta hem PC'ye derlenir | 02 |
| Atış kontrol | C17 | Hedef algılama (sonar verisi), takip, atış çözümü, durum makinesi, emniyet zinciri. Ortak çekirdeğin parçasıdır, ayrı modüldür | 03 |
| Sim sunucusu | C++20 | Dünyanın tek yetkili durumu: fizik, çevre, araç, zaman. Diğer bütün parçalar ona bağlanır | 04 |
| Sensör modelleri ve sonar | C++20 | Sim sunucusunun içinde eklenti olarak çalışır; fizik durumundan ölçüm üretir | 05 |
| HIL köprüsü ve protokol | C (kart), C++ (PC) | Sim ile kart arasındaki mesajlaşma, adım eşleme, bağlantı emülasyonu | 06 |
| Görselleştirici | C++ (GDExtension), Godot | 3B operatör görünümü ve kamera sensörü görüntüsü. Fizik yapmaz; durumu sim sunucusundan alır | 07 |
| Web panelleri ve laboratuvar işlevleri | TypeScript (ön yüz), C++ (sunucu tarafı) | Harita, grafikler, senaryo, hata enjeksiyonu, kayıt ve tekrar oynatma | 08 |
| SIL koşucusu | C17 + C++ | Ortak çekirdeği PC'de, kartın yerine çalıştırır; sim sunucusuna kartla aynı mesajlarla bağlanır | 02, 06 |

Kart yazılımı simülasyonda olduğunu bilmez: sensör örneklerini porttan alır, aktüatör komutlarını porta verir. Portun arkasında bugün HIL bağlantısı vardır.

## 3. Arayüzler

Her arayüz bir sözleşmedir. Sözleşmeler mimari onaylandıktan sonra ayrı bir işte, kodlamadan önce yazılır ve dondurulur; bölümler burada yalnızca içeriğini ve gereklerini tanımlar.

| # | Arasında | Taşıma | İçerik | Durum |
|---|---|---|---|---|
| A1 | Sim sunucusu ↔ kart (ve SIL koşucusu) | Kartta USB yüksek hız CDC; yedek ST-LINK sanal seri port. SIL'de yerel soket | MAVLink 2 + özel dialect. Sim → kart: zaman damgalı sensör örnekleri, sonar profilleri, algılama sonuçları, yer istasyonu komutları. Kart → sim: aktüatör komutları, atış komutu, durum ve telemetri | Ö2; taşıma M1–M3, biçim M9 ile kesinleşir |
| A2 | Sim sunucusu ↔ görselleştirici | Yerel soket (durum), paylaşımlı bellek (kamera kareleri) | Sim → Godot: pozlar, dalga bileşenleri, sim zamanı, ışık ve su parametreleri. Godot → sim: zaman damgalı kamera kareleri | Ö5, Ö5-k; M12, M13, M28, M30 |
| A3 | Sim sunucusu ↔ web panelleri | WebSocket (ikili telemetri akışı), HTTP (kontrol) | Telemetri, sonar görüntüsü, olaylar; senaryo yükleme, çalıştırma kontrolü, hata enjeksiyonu, kayıt yönetimi | K9, Ö10; M20 |
| A4 | Ortak çekirdek ↔ çevresi | C işlev arayüzü (`port`) | Zaman, birimli ve zaman damgalı sensör örnekleri, aktüatör çıkışı, bağlantı, kalıcı parametreler. Kartta BSP ve HIL bağlantısı, PC'de SIL koşucusu uygular | Kesin tasarım ilkesi |
| A5 | Senaryo ve araç tanımı | Dosya (TOML) | Araç parametreleri, ortam, hedefler, görev, arıza zaman çizelgesi, rastgelelik tohumu, kalite kademesi | — |
| A6 | Kayıt | Dosya (MCAP) | Bütün A1 ve A3 trafiği, fizik durumu, olaylar; tekrar oynatma bu dosyadan yapılır. Tekrar oynatılabilir bir kayıt, mesaj içeriğine ek olarak her komutun uygulandığı fizik adımını, bariyer zaman aşımı kararlarını ve düşürülen verileri içerir; "yalnızca olaylar" kademesinde tekrar oynatma güvencesi yoktur | Ö7 |
| A7 | Sahne ve varlıklar | glTF + manifest | Tek kaynaktan görsel model, sonar geometrisi, yüzerlik ve çarpışma ağları, malzemelerin akustik ve optik özellikleri | 12 numaralı rapor; D12 |

A1 ile A4 arasındaki dönüştürücü: A1 mesajlarını port örneklerine, port çıkışlarını A1 mesajlarına çeviren ince katman. Kartta kart yazılımının (01), PC'de SIL koşucusunun (02) parçasıdır; mesaj alanları ile port alanları arasındaki eşlemeyi 06 numaralı bölüm tanımlar.

A1 üzerindeki sonar verisi: tek bir tarama satırı MAVLink'in 255 baytlık yük sınırını aşar; profil parçalanarak taşınır (06 numaralı bölüm tasarlar, M9 ölçer).

Kamera ile hedef algılama kartta yapılmaz. PC'de sim sunucusuna bağlı ayrı bir algılama bileşeni kamera karelerini işler ve sonucu A1 üzerinden karta "algılama" mesajı olarak gönderir. Mesaj, sonar tabanlı algılamanın kart içinde ürettiği kayıtla aynı biçimdedir (yakalama zamanı, konum, belirsizlik, kaynak); böylece algılamanın yeri atış kontrol mantığını etkilemez.

### 3.1 Ortak kabuller

Bütün bölümler ve bütün arayüzler aynı kabulleri kullanır; bir bölüm başka bir kabul kullanamaz.

- **Birimler:** SI. Açı radyan, zaman tamsayı nanosaniye, basınç pascal, gerilim volt, akım amper.
- **Dünya çerçevesi:** NED (kuzey, doğu, aşağı); orijin senaryoda tanımlanır.
- **Gövde çerçevesi:** x ileri, y sancak, z aşağı. Gövde referans noktası A5 dosyasında tanımlıdır; genelleştirilmiş hızlar, kuvvetler ve momentler bu noktaya göre yazılır.
- **Yönelim:** birim kuaterniyon, bileşen sırası (w, x, y, z), Hamilton çarpımı; gövdeden dünyaya dönüşü temsil eder.
- **Zaman damgası:** her ölçüm iki ayrı zaman taşır: ölçümün geçerli olduğu an (ölçüm zamanı) ve alıcıya ulaştığı an (alım zamanı). Bunlar birbirinin yerine kullanılmaz.

### 3.2 Sürekli komutlar ve tek seferlik olaylar

A1 ve A4 üzerinde iki tür çıkış ayrı taşınır:

- **Sürekli komut** (itici çıkışları): her adımda yenilenir; yeniden gönderilmesi zararsızdır; en son geçerli olan uygulanır ve kullanım süresi sınırlıdır.
- **Tek seferlik olay** (atış tetiği, silahlandırma değişikliği): benzersiz bir olay kimliği taşır; alıcı aynı kimliği yalnızca bir kez uygular. Yeniden gönderilen ya da önbellekten tekrarlanan bir çıkış olay üretmez.

Silahlandırmayı kaldıran ve izni daraltan komutlar, aynı adımda atış kontrol modülünden önce uygulanır; modül her adımda güncel izni görür.

## 4. Zaman

- Sim zamanı tamsayı nanosaniyedir. Fizik sabit adımla ilerler; adım 1,25 ms (800 Hz).
- Kontrol döngüsü hedefi 400 Hz (M5 ile kesinleşir); yedeği 200 Hz. Kartta döngüyü yeni IMU örneğinin gelişi tetikler.
- Kontrol periyodu fizik adımının tam katıdır: 400 Hz'te iki, 200 Hz'te dört fizik adımı. Kontrolü tetikleyen IMU örneği her zaman bir fizik adımının sınırına denk gelir. (Sürüm 0.1'deki 1 ms adım 400 Hz ile tam bölünmüyordu; 04 numaralı bölümün bildirimiyle düzeltildi.)
- Diğer sensörler kendi hızlarında örneklenir; periyodu fizik adımının tam katı olmayanlar (ör. kamera) rasyonel zamanlamayla en yakın adım sınırına bağlanır ve gerçek örnekleme zamanını taşır.
- İki saat alanı vardır ve karışmaz: **sim zamanı** (sensör örneklerinin zaman damgası; kestirim ve kontrol hesapları bunu kullanır) ve **duvar saati** (bağlantı zaman aşımı, gözetim zamanlayıcısı, emniyet süreleri). Çekirdek simülasyonu bilmediği için sim zamanını **ölçüm zamanı** adıyla görür.
- Arıza koşullarında verinin tazeliği ölçüm zamanıyla, koşulun ne kadar sürdüğü duvar saatiyle değerlendirilir. SIL'de duvar saati sim zamanından türetilir; böylece hızlandırılmış koşuda emniyet süreleri de ölçeklenir.
- IMU örnekleri kesilirse çekirdek duvar saatiyle çağrılmaya devam eder; arıza güvenliği sensör akışına bağımlı kalmaz.
- **Zaman çerçevesi:** sim sunucusu her kontrol periyodunda, IMU örneği üretilsin ya da üretilmesin, sim zamanını ve sıra numarasını taşıyan bir zaman çerçevesi gönderir. Kartta duvar çağrılarını donanım zamanlayıcısı, SIL'de bu çerçeve sürer. Böylece IMU'nun kesildiği bir senaryoda SIL'de zaman ilerlemeye devam eder ve adım bariyeri kilitlenmez; kullanıcının duraklatması bundan ayrı bir durumdur.
- **Tazelik iki ölçütle denetlenir:** ölçüm zamanının ilerlemesi ve alım zamanı. Yinelenen, eski ya da geçersiz örnekler veri akışını canlı saymaz.

| Kip | Kim koşar | Zaman | Kullanım |
|---|---|---|---|
| SIL | Çekirdek PC'de | Sanal; hızlandırılabilir, duraklatılabilir, yavaş koşabilir | Geliştirme, otomatik test, ağır modeller |
| HIL 1x | Çekirdek kartta | Gerçek zaman; iki taraf serbest koşar | Gerçek zaman davranışının sınanması |
| HIL 1x + adım bariyeri | Çekirdek kartta | Gerçek zaman; sim her sensör çerçevesinden sonra aynı sıra numaralı aktüatör yanıtını bekler | Tekrarlanabilir kartlı testler |

Adım bariyerinde yanıt bütçe içinde gelmezse sim durmaz: son komutla ilerler, kaçırılan adımı sayar ve çalıştırmayı geçersiz işaretler (Ö11). Bariyer, kart yazılımının bütün zaman davranışını sim zamanına bağlamaz (Ç1).

## 5. Araç ve donanım seti

Araç parametrelerini mimar oturum belirler (K10). Başlangıç aracı **İDA-0**:

- BlueROV2 Heavy sınıfı açık çerçeveli araç; sekiz itici (dördü yatay düzlemde vektörlü, dördü dikey).
- Hafif pozitif yüzerlik: yüzeyde kısmen batık seyreder, dikey iticilerle dalar; balast sistemi yoktur.
- Hidrodinamik parametreler yayımlanmış ve test tankında ölçülmüş sönüm değerlerine dayanır (von Benzon 2022; 00 belgesi, 04 düzeltmeleri). Bütün parametreler A5 dosyasında durur ve değiştirilebilir.
- **Yük muhasebesi:** araç üç kütle durumunda tanımlanır: iki torpido yüklü, bir torpido yüklü, boş. Torpido tüpten ayrılana kadar aracın bağlı kütlesidir; ayrılma anında ayrı bir gövdeye dönüşür ve aracın kütlesi, kütle merkezi, ataleti ve deplasmanı aynı fizik adımında güncellenir. Hiçbir anda torpidonun kütlesi iki gövdede birden sayılmaz.
- **Fırlatma modeli:** ilk kademe, ayrılmayı tek bir anda çözen darbe modelidir (araç ve torpido etkin kütleleriyle birlikte; 00 belgesi, 04 düzeltmeleri). Tüp boyunca sonlu süreli hareketi çözen model sonraki kademedir ve ilk sürümün kapsamında değildir.

| Simüle edilen | Öğeler |
|---|---|
| Seyrüsefer sensörleri | IMU, manyetometre, basınç/derinlik, GNSS (yalnızca yüzeyde), DVL |
| Sonar | Ping1D (dibe mesafe), Ping360 (mekanik taramalı) |
| Görüntü | Öne bakan kamera |
| Sistem sensörleri | Batarya gerilimi ve akımı, sızıntı |
| Aktüatörler | Sekiz itici (motor sürücü modeliyle), iki torpido fırlatıcı |
| Çevre | Dalga, akıntı, rüzgar, havuz ve açık su ortamı, hedefler |
| Bağlantı | Yer istasyonu bağlantısının gecikmesi ve kaybı (telsiz, kablo) |

## 6. Kalite kademeleri

Windows hedef bilgisayarı düşük donanımlıdır (Ö11). Ağır özelliklerin her biri A5 dosyasından ayrı ayrı kısılır ya da kapatılır.

| Özellik | Kapalı | Sade | Tam |
|---|---|---|---|
| 3B görünüm | Görselleştirici çalışmaz | Düz su, basit ışık | Dalga yüzeyi, su altı optiği |
| Kamera sensörü | Kare üretilmez | Düşük çözünürlük ve hız | Tam çözünürlük, optik etkiler |
| Sonar | — | Az ışın, temel gürültü | Çok ışın, çok yollu yansıma |
| Kayıt | Yalnızca olaylar | Telemetri | Telemetri, sonar, kamera |

Fizik, kamera dışı sensörler, otopilot ve HIL her kademede tam çalışır. Sim sunucusu gerçek zamanı kaçırdığı adımları her kipte sayar ve A3 üzerinden bildirir.

Kalite kademesi kullanıcının seçtiği bir ayardır. 8. bölümdeki ölçüme bağlı yedekler kademe değildir: bir ölçüm sonucunda tasarımın kendisi yedeğe geçer ve bu bütün kademeler için geçerli olur.

## 7. Depo düzeni (planlanan)

```
core/         ortak çekirdek (C): kestirim, güdüm, kontrol, görev, atış kontrol, port arayüzü
firmware/     kart yazılımı (C): bsp, bağlantı, uygulama
protocol/     MAVLink dialect tanımı ve üretilmiş başlıklar
sim/          sim sunucusu (C++): fizik, çevre, sensörler, sonar, hil, kayıt, api
viz/          Godot projesi ve GDExtension
web/          web panelleri
scenarios/    senaryo ve araç tanım dosyaları
assets/       sahne ve model kaynakları
tests/        bütünleme ve senaryo testleri
tools/        yardımcı betikler
docs/mimari/  bu belgeler
```

## 8. Ölçüme bağlı kararlar

Aşağıdaki kararlar ilgili ölçüm yapılmadan kesinleşmez. Bölümler birincil seçeneği ve yedeğini birlikte tanımlar.

| Karar | Birincil | Yedek | Ölçüm |
|---|---|---|---|
| A1 taşıması | USB yüksek hız CDC (TinyUSB) | ST-LINK sanal seri port; ST'nin USB yığını | M1, M2, M3 |
| Kontrol döngüsü | RTOS'suz, 400 Hz | Daha düşük hız; FreeRTOS | M5 |
| A1 biçimi | MAVLink 2 + özel dialect | Açık serileştirmeli özel biçim | M9 |
| Kamera sensörü | Godot, pencere görünürlüğünden bağımsız çizim | Pencere görünür tutulur; kamera yalnızca o koşulda | M13, M28 |
| Sonar ayrıntısı | Gerçek zamanlı çok ışınlı model | Az ışın; ayrıntılı model yalnızca SIL'de | M11 |
| Kalite kademesi tabanı | 6. bölümdeki sade kademe | Özellik kapatma | M12, M20, M21, M30 |

## 9. Bölümler

| # | Dosya | Kapsam | Sınır (yazmaz) |
|---|---|---|---|
| 01 | `01-kart-yazilimi.md` | Başlatma, saat, bağlantı sürücüsü, döngü zamanlaması, portun kart tarafı, bellek ve kesme düzeni, gözetim zamanlayıcısı, karttaki ekranın kullanımı | Kestirim ve kontrol algoritmaları (02), mesaj içerikleri (06) |
| 02 | `02-ortak-cekirdek.md` | Port arayüzü (A4), kestirim, güdüm, kontrol, itki tahsisi, kipler, görev yönetimi, arıza güvenliği, sayı tipleri, SIL koşucusu | Atış kontrol (03), donanım (01) |
| 03 | `03-atis-kontrol.md` | Sonar verisinde hedef algılama, takip, atış çözümü, durum makinesi, emniyet zinciri, algılama mesajı, torpidonun simülasyondaki modeli için gerekler | Torpido fiziğinin kendisi (04), kamera görüntüsünün üretimi (07) |
| 04 | `04-simulator-cekirdegi.md` | Fizik (6 serbestlik dereceli model, yüzerlik, dalga, akıntı, rüzgar), İDA-0 parametre seti, iticiler ve güç modeli, torpido ve hedef fiziği, çarpışma, zaman yöneticisi, iç yapı ve iş parçacıkları, tekrar üretilebilirlik | Sensör modelleri (05), mesajlaşma (06) |
| 05 | `05-sensorler-ve-sonar.md` | Sensör eklenti arayüzü, her sensörün modeli ve hata kaynakları, zamanlama, sonar (ışın izleme, kademeler), arıza modelleri | Kamera görüntüsünün çizimi (07), taşıma (06) |
| 06 | `06-protokol-ve-hil.md` | A1'in mesaj kümesi ve gerekleri, sonar profilinin parçalanması, adım bariyeri, iki saat alanı, bağlantı kurma ve kopma, bağlantı emülatörü, yer istasyonu bağlantısı | Mesajların üretildiği modeller (04, 05), kart sürücüsü (01) |
| 07 | `07-gorsellestirme.md` | A2, Godot süreci, sahne, kamera sensörü hattı, dalga tutarlılığı, A7 sahne hattı, kamera ile algılama bileşeninin yeri, kalite kademelerinin görsel tarafı | Fizik ve çarpışma (04), sonar (05) |
| 08 | `08-laboratuvar-uygulamasi.md` | A3, web panelleri, A5 senaryo biçimi, hata enjeksiyonu, A6 kayıt ve tekrar oynatma, çalıştırma kontrolü | Modellerin içi (04, 05) |
| 09 | `09-test-ve-is-akisi.md` | Test katmanları, kabul ölçütleri, ölçüm işlerinin tanımı, otomatik derleme ve test, dal ve inceleme düzeni, derleme araç zinciri (Mac ve Windows) | Parçaların iç tasarımı |

### Her bölümün yapısı

1. Amaç ve sorumluluklar
2. İç yapı (alt parçalar, veri akışı, zamanlama)
3. Arayüzler (iskeletteki `A` numaralarına atıfla; alan listeleri ve işlev imzaları düzeyinde)
4. Hata durumları ve davranışı
5. Kalite kademelerinin etkisi (varsa)
6. Ölçüme bağlı kararlar: birincil seçenek, yedek, hangi ölçüm
7. Test yaklaşımı ve kabul ölçütleri
8. Açık noktalar ve iskelete öneriler

### Bölüm yazarları için kurallar

- Kod yazılmaz. Arayüz tanımı için alan listesi ve işlev imzası yeterlidir.
- Dayanak 00 numaralı belgedir. Ham rapordan alınan her değer için önce 00'daki düzeltme tablosuna bakılır; düzeltilmiş hali kullanılır.
- Ölçülmemiş hız, gecikme ve yük rakamları tahmin olarak işaretlenir.
- Başka bölümün kapsamına giren konu yazılmaz; ihtiyaç "o bölümden beklenen" olarak belirtilir.
- İskeletle çelişen bir gereklilik bulunursa bölüm iskelete uyar ve çelişkiyi 8. başlık altında bildirir.
- Ardıl kart, gerçek sensör tedariki ve yarışma kuralları konu edilmez (K7, K8).
