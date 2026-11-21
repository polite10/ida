# Mimari iskelet

Sürüm: 0.12 (2026-11-20). Durum: kullanıcı 2026-11-14'te bilinen açık bulgularla onayladı (K15); 0.8, W0 ölçümlerinin sonuçlarını ve K17'yi işledi; 0.9, 0.10, 0.11 ve 0.12, sözleşme incelemelerinden (W1) çıkan iskelet düzeyindeki kararları işler.

Sürüm 0.12 değişikliği (W1 fark incelemesi üzerine; mimar kararı D-17; ayrıntı `docs/gorevler/W1-uyum.md` "Fark incelemesi turu"): yolda bekleyen genişletici komutun kart tarafındaki bekleme noktası ertelenmiş komut kuyruğundan, kartta çekirdek çağrısına girmeden bekleyen her yere (açık ve bekleyen parti dahil) genişletildi (3.2).

Sürüm 0.11 değişiklikleri (W1 doğrulama incelemesinin dört parçası ve W2'nin ilk işleri üzerine; mimar kararları; ayrıntı `docs/gorevler/W1-uyum.md` "Doğrulama turu"): yolda bekleyen genişletici komutun üçüncü bekleme noktası (sim sunucusunun A1 gönderim kuyruğu) ve reddedilen ya da birleştirilen partiden taşınan emniyet örneğinin gevşetememesi yazıldı (3.2); otomatik derlemenin kota düzeni ve belge denetiminin koşucusu açıklandı (7).

Sürüm 0.10 değişiklikleri (W1 çapraz uyum incelemesinin dört parçası üzerine; mimar kararları; ayrıntı `docs/gorevler/W1-uyum.md`): emniyet revizyonunun genişliği, doyması ve onu artıran işlemlerin kapalı listesi yazıldı; fizik sınırındaki işlem sırası tekleştirildi; bekleyen genişletici komutların iptali eklendi (3.2); sıra numaralarının karşılaştırma kuralı ortak kabul oldu (3.1); süreli izinlerde iki tarafın saat alanı ayrıldı, canlı SIL koşuları arası eşitlik güvencesi kamera kapalı koşullarla sınırlandı, bariyer kipinin kullanım tanımı düzeltildi (4); kartlı kayıttan yeniden kurmanın girişi tanımlandı (3, A6 satırı); atış kontrolün kapsam ifadeleri sözleşmelerin dar sınırına çekildi (2, 5, 9); kamera karesinin gösterdiği an düzeltildi (8).

Sürüm 0.9 değişiklikleri (W1 sözleşmelerinin sol incelemeleri ve yazar önerileri üzerine; mimar kararları): A1 bant ölçütü olağan profil ve bağlayıcı üst sınır olarak ikiye ayrıldı (3); girdi tekrarının sınırı ve güvencesi tanımlandı (3, A6 satırı; 4); emniyette "kısıtlama hâkimdir" ilkesi ve emniyet revizyonu eklendi (3.2); sürekli çıktıların üretim sırası, eşzamansız güvenli çıktı, olayların zamanlaması, kullanım süresinin başlangıcı ve oturum sonunun önceliği tanımlandı (4); `L_act` payının sözleşme hesabı ve çağrı çakışması eklendi (4); kamera ile 3B görünümün bağı ve kamera kademe değerlerinin yeri düzeltildi (6, 10).

Sürüm 0.8 değişiklikleri (W0 ölçümleri ve kullanıcı kararı K17 üzerine; ayrıntı 8. ve 10. bölümlerde): A1 taşıması yalnız ST-LINK sanal seri portudur, USB yüksek hız yolu kaldırıldı (K17; 3, 8); kontrol hızının tasarım tabanı 200 Hz oldu (4, 8); sabit uygulama gecikmesi ve geç kalan yanıtın kuralı tanımlandı (4); kamera çizim yolu seçildi, sonar kademesi kararı ölçümle kesinleşti (8); bölümlerin iskelete önerileri karara bağlandı (10); depo düzeni güncellendi (7).

Sürüm 0.7 değişiklikleri: 0.5'te alınan "torpido itkilidir, yaklaşık 3 m/s ve 10 m menzil hedefi" kararı geri alındı (5). Torpido ilk tanımındaki gibi itkisiz, kısa menzilli basit bir simülasyon nesnesidir; atış kontrol bölümünün kapsamı emniyet zinciri ve arayüzlerle sınırlandı (9).

Sürüm 0.6 değişiklikleri (07 numaralı bölümün bildirdikleri üzerine): kamera algılama bileşeninin bağlantısı A8 olarak adlandırıldı; malzeme özelliklerinin yeri ve türetilmiş ağların depoya girmemesi (A7).

Sürüm 0.5 değişiklikleri (03 numaralı bölümün bildirdikleri üzerine): torpido itkili olarak tanımlandı ve menzil hedefi verildi (5); sanal emniyet girdileri ve fırlatıcı durum geri okuması donanım setine eklendi (5); olay kimliğinin kapsamı (3.2); menzil profili taşıyan akustik ölçümlerin zaman damgası (3.1).

Sürüm 0.4 değişiklikleri (05 numaralı bölümün bildirdiği uyumsuzluklar üzerine): gövde referans noktasının kütle merkezi olmadığı ve yük ayrılınca değişmediği; bir aralık boyunca toplanan ölçümlerin zaman damgası (3.1).

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
       │ tek kablo: ST-LINK portu (sanal seri port; K17)
┌──────┴──────────── STM32U5A9J-DK ────────────────────────────────┐
│ kart destek katmanı (BSP) · bağlantı · sensör/aktüatör portu (A4)│
│ ortak çekirdek: kestirim · güdüm · kontrol · görev · arıza       │
│ atış kontrol · sonar verisinde hedef algılama                    │
└──────────────────────────────────────────────────────────────────┘
```

| Parça | Dil | Sorumluluk | Bölüm |
|---|---|---|---|
| Kart yazılımı | C17 (platform kodu `gnu17`; derleyici uzantıları yalnız `firmware/` içinde) | Donanımı başlatır, bağlantıyı yürütür, portu (A4) uygular, çekirdeği zamanında çağırır | 01 |
| Ortak çekirdek | C17 | Kestirim, güdüm, kontrol, görev yönetimi, arıza güvenliği. Donanıma ve işletim sistemine bağımlı değildir; hem karta hem PC'ye derlenir | 02 |
| Atış kontrol | C17 | Sonar verisinden algılama kaydı üretimi, algılama kayıtlarının alınması, durum makinesi, emniyet zinciri ve atış olayının arayüzü; kapsamı 9. bölümdeki sınırla dardır. Ortak çekirdeğin parçasıdır, ayrı modüldür | 03 |
| Sim sunucusu | C++20 | Dünyanın tek yetkili durumu: fizik, çevre, araç, zaman. Diğer bütün parçalar ona bağlanır | 04 |
| Sensör modelleri ve sonar | C++20 | Sim sunucusunun içinde eklenti olarak çalışır; fizik durumundan ölçüm üretir | 05 |
| HIL köprüsü ve protokol | C (kart), C++ (PC) | Sim ile kart arasındaki mesajlaşma, adım eşleme, bağlantı emülasyonu | 06 |
| Görselleştirici | C++ (GDExtension), Godot | 3B operatör görünümü ve kamera sensörü görüntüsü. Fizik yapmaz; durumu sim sunucusundan alır | 07 |
| Web panelleri ve laboratuvar işlevleri | TypeScript (ön yüz), C++ (sunucu tarafı) | Harita, grafikler, senaryo, hata enjeksiyonu, kayıt ve tekrar oynatma | 08 |
| SIL koşucusu | C17 + C++ | Ortak çekirdeği PC'de, kartın yerine çalıştırır; sim sunucusuna kartla aynı mesajlarla bağlanır | 02, 06 |

Kart yazılımı simülasyonda olduğunu bilmez: sensör örneklerini porttan alır, aktüatör komutlarını porta verir. Portun arkasında bugün HIL bağlantısı vardır.

## 3. Arayüzler

Her arayüz bir sözleşmedir. Sözleşmeler ölçüm prototiplerinden (W0) sonra, ürün kodundan önce ayrı işlerde (W1) yazılır ve dondurulur (`docs/sozlesmeler/`); bölümler burada yalnızca içeriğini ve gereklerini tanımlar. Bir bölüm ile bu iskelet çelişirse sözleşme iskelete göre yazılır.

| # | Arasında | Taşıma | İçerik | Durum |
|---|---|---|---|---|
| A1 | Sim sunucusu ↔ kart (ve SIL koşucusu) | Kartta ST-LINK sanal seri portu (USART1), 3 000 000 baud, 8N1; başka taşıma yok (K17). SIL'de yerel soket | MAVLink 2 + özel dialect. Sim → kart: zaman çerçevesi, zaman damgalı sensör örnekleri, sonar profilleri, algılama sonuçları, yer istasyonu komutları. Kart → sim: aktüatör komutları (itici komutu ESC darbe genişliği olarak, mikrosaniye; normalleştirilmiş itki isteğinden çeviri kartın platform katmanındadır), atış olayı, durum ve telemetri | K17; taşıma MP3 ile ölçüldü (8); biçim Ö2, maliyeti M9 |
| A2 | Sim sunucusu ↔ görselleştirici | Yerel soket (durum), paylaşımlı bellek (kamera kareleri) | Sim → Godot: pozlar ve hızlar, dalga bileşenleri, sim zamanı, ışık ve su parametreleri, sahne olayları (gövde doğumu ve ölümü, kütle durumu, isabet), kamera istekleri. Godot → sim: zaman damgalı kamera kareleri. El sıkışmada A2 sürümü ve A7 manifest özeti doğrulanır | Ö5; kamera çizim yolu için Mac'te ön kanıt (MP6; 8); A2 hattının kendisi ölçülmedi; Windows M12, M28, M30 |
| A3 | Sim sunucusu ↔ web panelleri | WebSocket (ikili telemetri akışı), HTTP (kontrol) | Telemetri, sonar görüntüsü, olaylar; senaryo yükleme, çalıştırma kontrolü, hata enjeksiyonu, kayıt yönetimi | K9, Ö10; M20 |
| A4 | Ortak çekirdek ↔ çevresi | C işlev arayüzü (`port`) | Zaman, birimli ve zaman damgalı sensör örnekleri, aktüatör çıkışı, bağlantı, kalıcı parametreler. Kartta BSP ve HIL bağlantısı, PC'de SIL koşucusu uygular | Kesin tasarım ilkesi |
| A5 | Senaryo ve araç tanımı | Dosya (TOML) | Araç parametreleri, ortam, hedefler, görev, arıza zaman çizelgesi, rastgelelik tohumu, kalite kademesi | — |
| A6 | Kayıt | Dosya (MCAP) | A1 trafiği, A3 komutları ve olayları, fizik durumu; tekrar oynatma bu dosyadan yapılır ve üç düzeyi ayrıdır: izleme (kayıttaki durumu gösterme), fizik yeniden koşturma (kayıtlı komutlarla), girdi tekrarı (kayıtlı çekirdek çağrı girdilerini PC'deki çekirdeğe verme). Girdi tekrarının sınırı A4 port sınırıdır: bit düzeyinde aynı sonuç güvencesi yalnız SIL kaydından ve aynı yapı, platform ve özet koşulunda verilir; kartlı bir koşunun kaydından yapılan tekrar, çağrı girdilerinin A1 bayt akışından PC'de yeniden kurulmasıdır ve güvence taşımaz (dönüştürücü bunun için, oturum değerlerini kayıttaki el sıkışma çerçevelerinden alan ve yalnız PC derlemesinde bulunan bir giriş sunar); kayıtlı akışın karta yeniden verilmesi ilk sürümde yoktur. Tekrar oynatılabilir bir kayıt, mesaj içeriğine ek olarak her komutun uygulandığı fizik adımını, bariyer zaman aşımı kararlarını ve düşürülen verileri içerir; "yalnızca olaylar" kademesinde tekrar oynatma güvencesi yoktur | Ö7 |
| A7 | Sahne ve varlıklar | glTF + manifest | Tek kaynaktan görsel model, sonar geometrisi, yüzerlik ve çarpışma ağları, malzemelerin akustik ve optik özellikleri. Malzemelerin varsayılan değerleri A7'de durur; senaryoya özgü kalibrasyon değerleri A5'te bunları geçersiz kılar. Kaynaktan türetilen ağlar (ör. dışbükey ayrıştırma çıktıları) depoya girmez; derlemede sabitlenmiş araç sürümüyle üretilir ve manifestteki özetle doğrulanır | 12 numaralı rapor; D12 |
| A8 | Sim sunucusu ↔ kamera algılama bileşeni | Paylaşımlı bellek (kareler), yerel soket (kayıtlar) | Sim → bileşen: zaman damgalı kamera kareleri. Bileşen → sim: 03 numaralı bölümdeki biçimde algılama kayıtları; sim bunları A1 üzerinden karta iletir. Bileşen poz ya da yer gerçeği almaz | 07 numaralı bölüm; M8 |

A1 ile A4 arasındaki dönüştürücü: A1 mesajlarını port örneklerine, port çıkışlarını A1 mesajlarına çeviren ince katman. Kartta kart yazılımının (01), PC'de SIL koşucusunun (02) parçasıdır; mesaj alanları ile port alanları arasındaki eşlemeyi 06 numaralı bölüm tanımlar.

A1 üzerindeki sonar verisi: tek bir tarama satırı MAVLink'in 255 baytlık yük sınırını aşar; profil parçalanarak taşınır (06 numaralı bölüm tasarlar, M9 ölçer).

A1'in kapasitesi (hesap; ölçülen hız üzerinden): 3 000 000 baud 8N1 yön başına 300 000 bayt/s taşır; 200 Hz'lik kontrol periyodu başına 1 500 bayt. Sözleşme iki ölçüt uygular: **olağan çalışma profilinde** her yönde periyot başına ortalama yük kapasitenin %60'ının (900 bayt) altındadır; **bağlayıcı üst sınırda** (izin verilen her yapılandırmada, bütün sınıfların aynı anda en yüksek hızda aktığı durum) yük kapasitenin %90'ını (1 350 bayt) aşmaz ve kritik çıktının gecikme bütçesi sağlanır. Ölçümdeki %80 yük kayıpsız geçti (MP3); %90 sınırı ölçülmedi, W4'te A1 üzerinde sınanır. (0.8'de tek ölçüt olarak yazılan %60, zorunlu sınıfın atılamadığı en kötü durumda karttan PC'ye sağlanamıyor: A1 sözleşmesinin hesabı ≈ %85,5.)

Yer istasyonu: ilk sürümde yer istasyonu işlevini web panelleri (A3) görür; panelden verilen komutlar sim sunucusunda bağlantı emülatöründen (gecikme, kayıp) geçirilip A1 üzerinden karta iletilir. Harici bir yer istasyonu yazılımına (ör. QGroundControl) açılan ayrı bir MAVLink/UDP uç noktası ilk sürümün kapsamında değildir; A1'in MAVLink 2 olması bunu sonradan eklemeyi açık tutar.

Algılama kaydının çerçevesi: algılama kayıtları (sonar ya da kamera kaynaklı) ölçümü sensör çerçevesinde taşır; dünya çerçevesine dönüşüm yalnız kartta, kartın kendi seyrüsefer çözümüyle yapılır.

Kamera ile hedef algılama kartta yapılmaz. PC'de sim sunucusuna bağlı ayrı bir algılama bileşeni kamera karelerini işler ve sonucu A1 üzerinden karta "algılama" mesajı olarak gönderir. Mesaj, sonar tabanlı algılamanın kart içinde ürettiği kayıtla aynı biçimdedir (yakalama zamanı, konum, belirsizlik, kaynak); böylece algılamanın yeri atış kontrol mantığını etkilemez.

### 3.1 Ortak kabuller

Bütün bölümler ve bütün arayüzler aynı kabulleri kullanır; bir bölüm başka bir kabul kullanamaz.

- **Birimler:** SI. Açı radyan, zaman tamsayı nanosaniye, basınç pascal, gerilim volt, akım amper. İstisna: sıcaklık santigrat derece (°C) olarak taşınır.
- **Dünya çerçevesi:** NED (kuzey, doğu, aşağı); orijin senaryoda tanımlanır.
- **Gövde çerçevesi:** x ileri, y sancak, z aşağı. Gövde referans noktası A5 dosyasında tanımlıdır; genelleştirilmiş hızlar, kuvvetler ve momentler bu noktaya göre yazılır. Referans noktası gövdeye sabit geometrik bir noktadır; kütle merkezi değildir ve torpido ayrıldığında yerinde kalır. Sensör montaj konumları ve itici konumları da bu noktaya göre verilir.
- **Yönelim:** birim kuaterniyon, bileşen sırası (w, x, y, z), Hamilton çarpımı; gövdeden dünyaya dönüşü temsil eder.
- **Zaman damgası:** her ölçüm iki ayrı zaman taşır: ölçümün geçerli olduğu an (ölçüm zamanı) ve alıcıya ulaştığı an (alım zamanı). Bunlar birbirinin yerine kullanılmaz.
- **Sıra numaraları:** 32 bitlik sıra numaraları ve sayaç kimlikleri seri aritmetikle karşılaştırılır: `a`, `b`'den yenidir ancak ve ancak `(a − b) mod 2³²` değeri 1 ile 2³¹ − 1 arasındaysa. Fark 0 ise yinelemedir; 2³¹ ya da daha büyükse `a` eski sayılır. Hiçbir sözleşme "boşluğun üst sınırı yoktur" diyemez.
- **Aralık boyunca toplanan ölçümler** (ör. IMU'nun örnekleme aralığı, basınç sensörünün dönüşüm penceresi) aralığın sonunu ve süresini taşır. Tek bir geçerlilik anı gereken yerde aralığın ortası kullanılır; "dönüşümün bittiği an" ya da "verinin hazır olduğu an" ölçüm zamanı sayılmaz. Akustik ölçümlerde ölçüm zamanı yansımanın gerçekleştiği andır. Menzil profili taşıyan ölçümlerde (sonar tarama satırı) darbenin gönderildiği an taşınır; her menzil kutusunun zamanı, gönderim anına o menzilin doğrudan yoldaki gidiş süresi eklenerek bulunur. Bu, kutunun **nominal** zamanıdır: çok yollu yansımayla aynı kutuya düşen enerji fiziksel olarak başka bir anda ve yerde yansımıştır; profil bunu ayırmaz ve alıcı kutu zamanını nominal değer olarak kullanır.

### 3.2 Sürekli komutlar ve tek seferlik olaylar

A1 ve A4 üzerinde iki tür çıkış ayrı taşınır:

- **Sürekli komut** (itici çıkışları): her adımda yenilenir; yeniden gönderilmesi zararsızdır; en son geçerli olan uygulanır ve kullanım süresi sınırlıdır.
- **Tek seferlik olay** (atış tetiği, silahlandırma değişikliği): benzersiz bir olay kimliği taşır; alıcı aynı kimliği yalnızca bir kez uygular. Yeniden gönderilen ya da önbellekten tekrarlanan bir çıkış olay üretmez. Olay kimliği, kartın açılış sayacı ile olay sayacından oluşur; böylece kart yeniden başladığında ya da bağlantı yeniden kurulduğunda eski bir olay yeni sayılmaz. Her olayın bir son geçerlilik süresi vardır; süresi geçmiş olay uygulanmaz. İzin veren durumlar (silahlandırma, atış izni) sürelidir: belirli aralıkla yenilenmezse kendiliğinden düşer; izni kaldıran komut ise süresizdir. Olayın oturum kapsamı, alıcının yanıtı ve taşma davranışı A1 sözleşmesinde tanımlanır.

Silahlandırmayı kaldıran ve izni daraltan komutlar, aynı adımda atış kontrol modülünden önce uygulanır; modül her adımda güncel izni görür.

**Kısıtlama hâkimdir.** Emniyeti kısıtlayan ile gevşeten girdiler aynı adıma, aynı çekirdek çağrısına ya da aynı fizik sınırına düşerse geliş sıralarından bağımsız olarak kısıtlayan uygulanır ve gevşeten reddedilir. Bayat, yinelenen ya da başka nedenle reddedilen bir girdinin kısıtlayıcı içeriği kaybolmaz; gevşetici içeriği yalnız kabul edilen girdiden uygulanır. "Kısıtlayıcı içerik" bir girdinin kısıtlayan yönde taşıdığı her değerdir; önceki kabul edilen değere göre bir değişiklik olması gerekmez (yinelenen bir kısıtlama yeni olay üretmez ama aynı emniyet girdisinin aynı çağrıdaki gevşetilmesine yine üstün gelir). Çağrıdaki bütün genişletici komutların reddi ise yalnız etkin değerin kısıtlayana döndüğü çağrıda tetiklenir; süregelen bir kısıtlama (ör. takılı duran pim) kendi kapsadığı izinleri engeller, ilgisiz komutları reddetmez. Araya giren kısıtlama, yolda bekleyen gevşetmeyi de düşürür: bağlantı emülatöründe, sim sunucusunun A1 gönderim kuyruğunda ya da kartta çekirdek çağrısına girmeden bekleyen (açık ya da bekleyen partide, ertelenmiş komut kuyruğunda) genişletici bir komut, kendisinden sonra kabul edilmiş bir daraltıcı komuttan sonra uygulanamaz; zincirin her bekleme noktası aynı denetimi yapar. Kartta bu, mandala alınan daraltıcının kendisinden önce alınmış ve henüz çağrılmamış bütün genişleticileri düşürmesidir: daraltıcıyı araya giren başka bir çağrı uygulayıp tüketse de eski genişletici sonraki çağrıya giremez. Reddedilen ya da başka bir partiyle birleştirilen bir partiden sonraki çağrıya taşınan emniyet örneği gevşetemez: yalnız kısıtlayıcı içeriği taşınır, gevşetme kabul edilen bir partinin kendi örneğiyle gelir. Bu ilke A1, A3, A4 ve A5'te aynı sonucu verir.

**Emniyet revizyonu.** Dünya tarafı (sim sunucusu) tek bir emniyet revizyonu tutar: işaretsiz 32 bit; süreç başında 1; çalıştırmalar ve oturumlar arasında sıfırlanmaz; `0xFFFFFFFF`'te doyar ve doyduktan sonra süreç yeniden başlayana kadar bütün gevşetmeler reddedilir. 0 değeri yalnız "platform henüz emniyet örneği görmedi" anlamında kullanılır. Emniyeti gevşeten her istek ve izin açan her olay, karar verildiği andaki revizyonu taşır ve yalnız revizyon hâlâ güncelse uygulanır; araya giren her artış bekleyen yetkileri kalıcı olarak düşürür, eski yetki kendiliğinden geri gelmez. Sayaç A1 ve A3'te aynıdır.

Revizyonu **yalnız** aşağıdaki işlemler artırır; her işlem tam bir artırır, işlemin kaç tüpü etkilediği ya da durumu değiştirip değiştirmediği artışı değiştirmez. Artış, işlemin işlendiği fizik sınırında, aşağıdaki sıradaki yerinde yapılır:

1. kabul edilen her kısıtlayan komut: sanal emniyet girdisini kısıtlayan yönde değiştiren panel isteği ya da A4'ün daraltıcı sınıfındaki yer istasyonu komutu (komut başına);
2. bir fizik sınırında uygulanan, senaryo kaynaklı kısıtlayan atamaların tümü (tek işlem);
3. uygulanan her `SAFETY_CLOSE` kart olayı;
4. her bağlantı oturumunun sonu (nedeni ne olursa olsun);
5. bir fizik sınırında süresi dolan izinlerin tümü (tek işlem);
6. uygulanan her gevşetme: panelin gevşeten sanal girdi isteği (istek başına) ya da bir fizik sınırında uygulanan senaryo kaynaklı gevşeten atamaların tümü (tek işlem).

Artırmayanlar: uygulanan `SAFETY_OPEN` (izin açma ve yenileme), `FIRE`, reddedilen her girdi, sürekli çıktılar. Gevşeten (A4'ün genişletici sınıfındaki) yer istasyonu komutu kabul edildiği andaki revizyona bağlanır; bağlantı emülatöründen çıkarken ve sim sunucusunun A1 gönderim kuyruğundan tele yazılmadan hemen önce yeniden denetlenir; revizyon değişmişse atılır.

**Fizik sınırındaki sıra.** Aynı fizik sınırına düşen emniyet işlemleri şu sırayla işlenir; A1 ve A3 bu sırayı aynen yazar, A5 ve A6 ona atıf verir. Sim duraklatılmışken gelen emniyet işlemleri beklemez; aynı sırayla, sim zamanı ilerlemeden işlenir.

1. oturum sonu ve süresi dolan izinler;
2. kısıtlayan komutlar ve senaryonun kısıtlayan atamaları (hepsi uygulanır);
3. kart olayları: önce `SAFETY_CLOSE`, sonra izin açan olaylar, sonra `FIRE`; her biri önceki adımlardan sonraki emniyet durumuna ve revizyona göre doğrulanır;
4. gevşetmeler: revizyon bu sınırda 1–3. adımlarda arttıysa bu sınırdaki bütün gevşetmeler reddedilir ve yeniden denenmez; artmadıysa önce senaryonun gevşeten atamaları, sonra panel istekleri (her biri taşıdığı revizyona göre) uygulanır;
5. sürekli çıktılar.

Bir olay, aynı sınırdaki gevşetmeden yararlanmaz.

## 4. Zaman

- Sim zamanı tamsayı nanosaniyedir. Fizik sabit adımla ilerler; adım 1,25 ms (800 Hz).
- Kontrol döngüsünün tasarım tabanı **200 Hz**'tir (periyot 5 ms). Dayanak iki ölçümdür: yer tutucu çekirdek adımı kartta ağır yükte 400 Hz periyoduna sığmadı, 200 Hz'te hiç aşım olmadı (MP4a); A1'in 256 baytlık gidiş-dönüşü 3 000 000 baud'da yaklaşık 2,2 ms sürüyor (MP3). Sözleşmeler 200 Hz'e göre yazılır. 400 Hz bir hedef değildir; yalnız gerçek çekirdeğin karttaki ölçümü (W3) açık pay gösterirse ayrı bir kararla yeniden ele alınır.
- Kartta döngüyü, yeni IMU örneğini içeren **adım partisinin kapanışı** tetikler: sim sunucusu bir kontrol periyoduna ait bütün mesajları (zaman çerçevesi, IMU örneği, o periyotta hazır olan diğer sensör örnekleri) bir parti olarak gönderir ve partinin bittiğini işaretler; çekirdek adımı parti kapandığında koşar. Tetik, taşımanın bayt akışındaki bir ara noktaya bağlanmaz.
- Kontrol periyodu fizik adımının tam katıdır: 200 Hz'te dört fizik adımı. Partiyi kapatan an her zaman bir fizik adımının sınırına denk gelir. (Sürüm 0.1'deki 1 ms adım 400 Hz ile tam bölünmüyordu; 04 numaralı bölümün bildirimiyle düzeltildi.)
- Diğer sensörler kendi hızlarında örneklenir; periyodu fizik adımının tam katı olmayanlar (ör. kamera) rasyonel zamanlamayla en yakın adım sınırına bağlanır ve gerçek örnekleme zamanını taşır.
- İki saat alanı vardır ve karışmaz: **sim zamanı** (sensör örneklerinin zaman damgası; kestirim ve kontrol hesapları bunu kullanır) ve **duvar saati** (bağlantı zaman aşımı, gözetim zamanlayıcısı, emniyet süreleri). Çekirdek simülasyonu bilmediği için sim zamanını **ölçüm zamanı** adıyla görür. Süreli izinlerde iki taraf ayrı sayar: izni üreten taraf (kart) yenileme aralığını duvar saatiyle, izni uygulayan taraf (sim) iznin ömrünü uygulama anından başlayarak sim zamanıyla sayar.
- Arıza koşullarında verinin tazeliği ölçüm zamanıyla, koşulun ne kadar sürdüğü duvar saatiyle değerlendirilir. SIL'de duvar saati sim zamanından türetilir; böylece hızlandırılmış koşuda emniyet süreleri de ölçeklenir.
- IMU örnekleri kesilirse çekirdek duvar saatiyle çağrılmaya devam eder; arıza güvenliği sensör akışına bağımlı kalmaz.
- **Zaman çerçevesi:** sim sunucusu her kontrol periyodunda, IMU örneği üretilsin ya da üretilmesin, sim zamanını ve sıra numarasını taşıyan bir zaman çerçevesi gönderir. Kartta duvar çağrılarını donanım zamanlayıcısı, SIL'de bu çerçeve sürer. Böylece IMU'nun kesildiği bir senaryoda SIL'de zaman ilerlemeye devam eder ve adım bariyeri kilitlenmez; kullanıcının duraklatması bundan ayrı bir durumdur.
- **Tazelik iki ölçütle denetlenir:** ölçüm zamanının ilerlemesi ve alım zamanı. Yinelenen, eski ya da geçersiz örnekler veri akışını canlı saymaz.

| Kip | Kim koşar | Zaman | Kullanım |
|---|---|---|---|
| SIL | Çekirdek PC'de | Sanal; hızlandırılabilir, duraklatılabilir, yavaş koşabilir | Geliştirme, otomatik test, ağır modeller |
| HIL 1x | Çekirdek kartta | Gerçek zaman; iki taraf serbest koşar | Gerçek zaman davranışının sınanması |
| HIL 1x + adım bariyeri | Çekirdek kartta | Gerçek zaman; sim her kontrol periyodunun partisinden sonra aynı sıra numaralı aktüatör yanıtını bekler | Kartlı işlevsel testler: kabul edilen komutların uygulandığı fizik adımı belirlidir. Beklemeli bir çalıştırma için tekrarlanabilirlik güvencesi yoktur (aşağıda) |

**Canlı koşular arası eşitlik.** Aynı senaryo ve tohumla yapılan iki canlı SIL koşusunun A1 baytlarının eşitliği yalnız kamera kapalıyken güvence altındadır: kamera karesinin üretimi ve algılama sonucunun dönüşü bilgisayarın zamanlamasına bağlıdır ve sim SIL'de bunu beklemez. Kamera açık koşularda güvence, kayıtlı çekirdek çağrı girdilerinin tekrarıdır (3, A6 satırı).

**Uygulama gecikmesi sabittir.** `k` numaralı partiye verilen aktüatör yanıtı, zamanında geldiyse, sim zamanında partinin kapanışından tam `L_act` sonra uygulanır; erken gelen yanıt bekletilir. `L_act` iki kontrol periyodudur (10 ms; sekiz fizik adımı). Dayanak: gidiş-dönüş taşıma süresinin ortancası (≈ 2,2 ms; MP3) ile yer tutucu adımın en uzun süresi (≈ 3,3 ms; MP4a) birlikte tek periyodu (5 ms) aşıyor. Bu toplam bir en kötü durum kanıtı değildir (ortanca ile bir yer tutucunun en büyüğü toplanmıştır); `L_act`, gerçek çekirdeğin ve A1'in birlikte ölçümünden (W3, W4) sonra yeniden değerlendirilir. Aynı sabit gecikme HIL 1x ve SIL'de de kullanılır. Güvence yalnız **kabul edilen komutların hangi fizik adımında uygulandığı** içindir; geç kalan yanıt bu güvencenin istisnasıdır (aşağıda).

**Geç kalan yanıt.** Ölçümlerde seyrek uzun gecikmeler görüldü (MP3, 600 saniyelik koşularda: gidiş-dönüşü 10 ms'yi aşan 2–4 yanıt, en uzunu 14,5 ms; ayrıca bilgisayarın yazmayı planın 10 ms'den fazla gerisinde başlattığı 1–2 istek). Nedeni ayrıştırılmadı (işletim sistemi zamanlaması, USB, ST-LINK köprüsü); temiz koşuldaki yineleme açıktır. Sözleşme bu durumu olağan sayar ve şöyle tanımlar:

- **Zamanlama geçerliliği.** Bir yanıt kendi `L_act` anına yetişmezse çalıştırma, hangi kipte olursa olsun, "gerçek zamanı kaçırdı" diye işaretlenir ve zamanlama bakımından **geçersizdir** (Ö11); kaçırılan her parti sayılır ve A3 üzerinden bildirilir. Zamanlama kabulü yalnız hiç kaçırılan parti olmayan çalıştırmalarla yapılır.
- **Adım bariyerinde bekleme (ayrı bir davranıştır).** Bariyer kipinde sim, geciken yanıtı **bekleme sınırına** kadar (A1 sözleşmesinde sabitlenir: 10–35 ms, varsayılan 30 ms; tahmin. 0.8'deki 50 ms başlangıç değeri, olay sonucunun iki partide teslimi kartın sonuç bekleme süresine sığsın diye sözleşmede indirildi) sim zamanını ilerletmeden bekler ve yanıtı öngörülen fizik adımında uygular. Böylece işlevsel inceleme sürdürülebilir; çalıştırma yine de zamanlama bakımından geçersiz işaretlidir ve bekleme süresi "gerçek zaman kayması" olarak kaydedilir. Bekleme, kart yazılımının duvar saatine bağlı davranışlarını (gözetim, emniyet süreleri, bağlantı zaman aşımı) durdurmaz; bu yüzden beklemeli bir çalıştırma için tekrarlanabilirlik ya da kipler arası eşitlik güvencesi verilmez (Ç1).
- **Yanıtın kabul kuralı (bütün kipler).** Her yanıt, yanıtladığı partinin sıra numarasını taşır ve yalnız o partinin **uygulama anında** (partinin kapanışı + `L_act`) uygulanabilir. Sim her sıra numarası için tek bir yuva tutar. Uygulama anından önce gelen yanıt kendi yuvasına konur ve o ana kadar bekletilir; birden çok parti aynı anda yolda olabilir, sonraki partilere ait yanıtlar kendi yuvalarında bekler. Aynı sıra numarasıyla ikinci kez gelen yanıt ve uygulama anı geçmiş bir sıra numarasına ait yanıt reddedilir ve sayılır; hiçbir yanıt geriye dönük ya da başka bir partinin yerine uygulanmaz.
- **Uygulama anında yuva boşsa.** Bariyer kipinde sim, yalnız o sıra numarasının yanıtını bekleme sınırına kadar bekler; bu sırada gelen eski numaralı yanıtlar reddedilir, sonraki numaralı yanıtlar kendi yuvalarına konur, ikisi de beklemeyi bitirmez. Yanıt gelirse öngörülen fizik adımında uygulanır. Bekleme sınırı aşılırsa ya da kip bariyersizse (HIL 1x'te bekleme yoktur) parti "kaçırıldı" olarak kapanır: o adımda komut değişmez, sim zamanı ilerler, o numaranın sonradan gelen yanıtı reddedilir.
- **Komutun ömrü ve nötr durum.** Yürürlükteki sürekli komut, kendi kullanım süresi dolana kadar uygulanmaya devam eder; süresi dolduğunda **uygulanmaz** ve aktüatör modeli nötr duruma (iticilerde sıfır itki isteği) geçer. Nötr durumdan çıkış için ayrı bir kural yoktur: kendi uygulama anında yuvasında bulunan ilk yanıt yukarıdaki kabul kuralıyla uygulanır. Art arda kaçırılan partiler ayrı ayrı sayılır.
- Sürekli komutların kullanım süresi ve kartın bağlantı zaman aşımı A1 sözleşmesinde, bekleme sınırıyla birlikte ve birbirine göre tanımlanır. Kullanım süresi yanıtın sabit uygulama anından sayılır.
- **Geçerli komut taşımayan yanıt.** Kartın "sürekli komut geçersiz" diye işaretlediği yanıt yuvayı doldurur (parti kaçırılmış sayılmaz) ama yürürlükteki komutu değiştirmez.
- **Üretim sırası.** Kartın ürettiği her sürekli çıktı tek bir üretim sayacı taşır; sim yalnız son uygulanandan büyük sayaçlı çıktıyı uygular, aynı fizik sınırına düşen çıktılar üretim sırasıyla uygulanır.
- **Eşzamansız güvenli çıktı** sabit uygulama anı kuralının tek istisnasıdır: alımdan sonraki ilk fizik adımında uygulanır ve kendinden önce üretilmiş, henüz uygulanmamış bütün sürekli çıktıları iptal eder.
- **Tek seferlik olayların zamanlaması.** Zamanında gelen olay, ait olduğu yanıtın yuvasıyla aynı adımda uygulanır; geç gelen olay son geçerliliği içindeyse alımdan sonraki ilk adımda uygulanır ve çalıştırmayı zamanlama bakımından geçersiz kılar.
- **Oturum sonunun önceliği.** Bağlantı oturumunun sonu, yeni oturumun başlangıcından ve girdilerinden önce, ayrı bir çekirdek çağrısında işlenir.
- **Çağrı çakışması.** Bir parti geldiğinde kartta başka bir çekirdek çağrısı sürüyorsa o parti ayrıca sayılır ve zamanlama bakımından geçersizdir.

**`L_act` payı (sözleşme hesabı; ölçüm değil).** A1 sözleşmesinin kalem kalem hesabıyla, çakışmasız bir partinin uçtan uca en kötü süresi ≈ 9,73 ms, `L_act`'e pay ≈ 0,27 ms'dir; kalemlerin çoğu tahmindir (gerçek çekirdeğin 3,5 ms'lik bütçesi dahil). Bu pay, `L_act`'in iki periyotta kalabileceğini göstermez, yalnız çelişmediğini gösterir. Karar W3/W4'teki birlikte ölçümle verilir: ölçülen en kötü süre 10 ms'yi aşarsa `L_act` üç periyoda (15 ms) çıkar; bu bir yedektir, kalite kademesi değildir.

## 5. Araç ve donanım seti

Araç parametrelerini mimar oturum belirler (K10). Başlangıç aracı **İDA-0**:

- BlueROV2 Heavy sınıfı açık çerçeveli araç; sekiz itici (dördü yatay düzlemde vektörlü, dördü dikey).
- Hafif pozitif yüzerlik: yüzeyde kısmen batık seyreder, dikey iticilerle dalar; balast sistemi yoktur.
- Hidrodinamik parametreler yayımlanmış ve test tankında ölçülmüş sönüm değerlerine dayanır (von Benzon 2022; 00 belgesi, 04 düzeltmeleri). Bütün parametreler A5 dosyasında durur ve değiştirilebilir.
- **Yük muhasebesi:** araç üç kütle durumunda tanımlanır: iki torpido yüklü, bir torpido yüklü, boş. Torpido tüpten ayrılana kadar aracın bağlı kütlesidir; ayrılma anında ayrı bir gövdeye dönüşür ve aracın kütlesi, kütle merkezi, ataleti ve deplasmanı aynı fizik adımında güncellenir. Hiçbir anda torpidonun kütlesi iki gövdede birden sayılmaz.
- **Fırlatma modeli:** ilk kademe, ayrılmayı tek bir anda çözen darbe modelidir (araç ve torpido etkin kütleleriyle birlikte; 00 belgesi, 04 düzeltmeleri). Tüp boyunca sonlu süreli hareketi çözen model sonraki kademedir ve ilk sürümün kapsamında değildir.
- **Torpido itkisizdir:** ayrıldıktan sonra yalnızca ayrılma hızıyla ilerleyen, kısa menzilli basit bir simülasyon nesnesidir. Etkili menzili yaklaşık 1 m mertebesindedir (03 numaralı bölümün hesabı; ölçüm değil). İtkili torpido, daha uzun menzil ve hareketli hedefe önleme hesabı bu projenin kapsamında değildir.
- Torpido ve fırlatıcının sayısal varsayılanları (kütle, ayrılma hızı, tüp konumu) mimari varsayımdır, ölçüm değildir. Yük durumları arasında aracın eklenmiş kütlesi değişmez kabul edilir.
- **Bilinen sınır:** bu menzil, taramalı sonarın göremediği yakın bölgeyle çakışır. Bu bölgede yalnız kamera algılama kayıtları bulunur; öne bakan ikinci bir menzil ölçer eklenmez. Bu çakışma modelin kabul edilen bir sınırıdır; giderilmesi için torpidonun yetenekleri artırılmaz.

| Simüle edilen | Öğeler |
|---|---|
| Seyrüsefer sensörleri | IMU, manyetometre, basınç/derinlik, GNSS (yalnızca yüzeyde), DVL |
| Sonar | Ping1D (dibe mesafe), Ping360 (mekanik taramalı) |
| Görüntü | Öne bakan kamera |
| Sistem sensörleri | Batarya gerilimi ve akımı, sızıntı, fırlatıcı durumu (tüp dolu ya da boş) |
| Sanal emniyet girdileri | Acil durdurma, silahlandırma fişi, emniyet pimi. Kartın dışında, sim sunucusunda durur; kart yazılımı bunları değiştiremez, yalnızca durumlarını okur. Atış olayı sim sunucusunda bu girdiler izin veriyorsa uygulanır |
| Aktüatörler | Sekiz itici (motor sürücü modeliyle), iki torpido fırlatıcı |
| Çevre | Dalga, akıntı, rüzgar, havuz ve açık su ortamı, hedefler |
| Bağlantı | Yer istasyonu bağlantısının gecikmesi ve kaybı (telsiz, kablo) |

## 6. Kalite kademeleri

Windows hedef bilgisayarı düşük donanımlıdır (Ö11). Ağır özelliklerin her biri A5 dosyasından ayrı ayrı kısılır ya da kapatılır.

| Özellik | Kapalı | Sade | Tam |
|---|---|---|---|
| 3B görünüm | Görselleştirici çalışmaz | Düz su, basit ışık | Dalga yüzeyi, su altı optiği |
| Kamera sensörü | Kare üretilmez | Düşük çözünürlük ve hız | Tam çözünürlük, optik etkiler |
| Sonar | — | Yalnız doğrudan yol, eşevresiz toplam, kutu başına benek; ping başına 5 000 ışın | Eşevreli saçıcı hücreleri, çok yollu yansıma; ışın sayısı ayarlanır |
| Kayıt | Yalnızca olaylar | Telemetri | Telemetri, sonar, kamera |

Fizik, kamera dışı sensörler, otopilot ve HIL her kademede tam çalışır. Sim sunucusu gerçek zamanı kaçırdığı adımları her kipte sayar ve A3 üzerinden bildirir.

Emniyet kabulü kalite kademesinden bağımsızdır: emniyet zincirinin testleri her kademede aynı sonucu vermelidir. Kameraya bağlı davranış yalnız kamera açıkken sınanır.

Kamera karelerini görselleştirici üretir; bu yüzden kamera sensörü açıkken 3B görünüm "kapalı" seçilemez (böyle bir senaryo doğrulamada reddedilir). Kamera kademelerinin çözünürlük ve hız değerleri A2 sözleşmesinde tanımlıdır; öteki sözleşmeler oraya atıf verir.

Sonarın kartlı çalışmadaki varsayılanı sade kademedir (8). Tam kademe kartlı çalışmada ancak azaltılmış ışın sayısıyla ve raporlanmış hatasıyla seçilebilir; tam ayrıntı gerçek zamanlı olmayan SIL koşuları içindir.

Kalite kademesi kullanıcının seçtiği bir ayardır. 8. bölümdeki ölçüme bağlı yedekler kademe değildir: bir ölçüm sonucunda tasarımın kendisi yedeğe geçer ve bu bütün kademeler için geçerli olur.

## 7. Depo düzeni (planlanan)

```
core/              ortak çekirdek (ISO C17): kestirim, güdüm, kontrol, görev, atış kontrol, port arayüzü
firmware/          kart yazılımı (C): bsp/ (kart destek katmanı), bağlantı, uygulama,
                   third_party/ (ST HAL, CMSIS; sürümü sabit), cubemx/ (yalnız pin ve saat referansı)
protocol/          MAVLink dialect tanımı, üretilmiş başlıklar ve ida_link (çerçeveleme + A1↔A4 dönüştürücü; tek modül)
sim/               sim sunucusu (C++20): fizik, çevre, sensörler, sonar, hil, kayıt, api
viz/               Godot projesi ve GDExtension
web/               web panelleri
scenarios/         senaryo ve araç tanım dosyaları
assets/            sahne ve model kaynakları
tests/             bütünleme ve senaryo testleri; kabul/ (dondurulmuş kabul testleri)
tools/             yardımcı betikler; denetim/ (derleme denetimleri), olcum/ (W0 ölçüm prototipleri; ürün kodu değil)
cmake/             araç zinciri ve ortak CMake dosyaları; kökte CMakePresets.json ve vcpkg.json
.github/workflows/ otomatik derleme ve test
docs/mimari/       bu belgeler ve açık bulgular kaydı
docs/sozlesmeler/  dondurulmuş arayüz sözleşmeleri (A1–A8)
docs/gorevler/     mimarın yazdığı görev belgeleri
docs/olcumler/     ölçüm raporları ve incelemeleri
```

Otomatik derleme macOS ve Windows'ta koşar; ürün Linux'ta derlenmez ve sınanmaz, karta bağlı kendi kendine barındırılan koşucu kurulmaz (kartlı testler geliştirme Mac'inde betikle koşar). Belge ve depo denetimi yalnız Python betiğidir, derleme içermez ve Linux koşucusunda koşar. Hesabın otomatik derleme kotası dar olduğu için (W2-depo ölçümü) Windows işi her pull request gönderiminde, macOS işi birleştirmeden önce bir kez koşar; her iş ayrıca geliştirme Mac'inde yerelde derlenip sınanır. Dal koruma bu hesap planında yoktur; birleştirmeden önceki kapı mimar denetimidir. Derleme zamanında Python kullanılabilir (MAVLink kod üreteci ve yardımcı betikler için); Node yalnız web panellerinin derlemesinde kullanılır. Hiçbiri kartta ya da çalışan simülatörde koşmaz.

## 8. Ölçüme bağlı kararlar

Sürüm 0.7'de bu bölüm, ölçüm yapılmadan kesinleşmeyecek kararları birincil seçenek ve yedeğiyle listeliyordu. W0 ölçümleri yapıldı; aşağıdaki tablo her kararın bugünkü durumunu verir. Rakamlar ölçüm raporlarındandır (`docs/olcumler/`); ölçümlerin hepsi tek bir Mac'te (Apple M3 Pro) ve tek kartta alınmış ön turlardır.

| Karar | Durum | Dayanak |
|---|---|---|
| A1 taşıması | **Kesin: ST-LINK sanal seri portu (USART1), 3 000 000 baud, 8N1.** Başka taşıma yok; USB yüksek hız yolu ve kart tarafı USB yığını kaldırıldı. 3 000 000 baud'da 600 saniyelik çift yönlü akış %80 yükte kayıpsız geçti. 6 000 000 baud'da veri temizdi ama bilgisayarın yazma takvimi ölçütü geçmedi; daha yüksek hız, temiz koşuldaki yinelemeden sonra ayrı bir kararla ele alınır, sözleşme 3 000 000 baud'a göre yazılır | K17; MP3 (`MP3-vcp.md`) |
| A1 gidiş-dönüş süresi | Ölçüldü: 3 000 000 baud'da 64 baytta ≈ 0,8 ms, 256 baytta ≈ 2,2 ms (ortanca); p99,9 yaklaşık 1,1 ve 2,5 ms; 600 saniyelik koşularda 10 ms'yi aşan 2–4 yanıt, en uzunu 14,5 ms. Uzun gecikmelerin nedeni ayrıştırılmadı; ölçüm sırasında bilgisayarda ilgisiz yük vardı, temiz koşuldaki yineleme açık | MP3 |
| Kontrol döngüsü | **Tasarım tabanı: RTOS'suz, 200 Hz.** Yer tutucu çekirdek adımı `-O2` ağır yükte en uzun 3,32 ms sürdü ve 400 Hz'te her adımı aştı; 200 Hz'te 121 000 adımda aşım olmadı. Yer tutucu gerçek çekirdek değildir; gerçek çekirdekle ölçüm W3 sonunda yinelenir. RTOS kararı değişmedi | MP4a (`MP4-dongu-yuku.md`); M5 |
| Kart saati | Kesin: HSE 16 MHz + PLL1 ile 160 MHz, gerilim ölçeği Range 1; PLL1'e geçişte geçici AHB ÷2 ve en az 5 µs bekleme. SMPS ve LDO ile 10 dakikalık turlar hatasız; 1 saatlik tur yapılmadı | MP1a (`MP1-arac-zinciri.md`); M4 |
| A1 biçimi | Öneri sürüyor: MAVLink 2 + özel dialect. Karttaki ayrıştırma ve kodlama maliyeti ölçülmedi; MP5 ya da protokol kütüphanesinin ilk sürümü (W2) ölçer. Sonuç, 200 Hz'lik adım bütçesini aşarsa yedek açık serileştirmeli özel biçimdir | Ö2; M9 |
| Kamera sensörü | **Seçilen yol: Godot içinden, pencere görünürlüğünden bağımsız çizim.** Kamera karesi ana iş parçacığından açıkça çizdirilir (`force_draw` ile, kamera görünümü "bir kez güncelle" kipinde) ve eşzamanlı geri okunur. Ön kanıt (Mac, betik düzeyinde ön tur): bu yol pencere örtülü, gizli ve simge durumundayken 300 saniyede 9000/9000 kare üretti, içerik görünür pencereyle bit düzeyinde aynıydı; 640 × 480 geri okuma p99 5,7 ms. Olağan çizim döngüsü bu durumlarda kare üretmedi ve kullanılmaz. Asenkron geri okuma kullanılmaz (gizli pencerede ≈ 70 ms). Ölçülmeyenler: A2'nin soket ve paylaşımlı bellek hattı, C++ uzantısı, başka uygulama tam ekrandayken durum, zamansal görsel etkiler, 5 dakikadan uzun koşular, Windows. Windows'ta geçmezse yedek, pencerenin görünür tutulmasıdır | MP6a (`MP6-godot-kamera.md`); M13, M28, M30; Windows için MP8 |
| Kamerada iki zaman | Gerek: operatör görünümü ve kamera aynı sahnede iki ayrı zamanı çizer (ayrık çizim; kamera karesi kendi geçerlilik anındaki, yani pozlama aralığının ortasındaki durumu gösterir; taşınan ölçüm zamanı aralığın sonudur, 3.1). Ölçülmedi; Godot sahnesinin ilk işinde sınanır | 07; M28'in açık kalan kısmı |
| Sonar ayrıntısı | **Kesin: kartlı çalışmada varsayılan sade kademe.** Sade kademe ping başına 5 000 ışınla dondurulmuş yakınsama ölçütünü 7 m menzilde sağladı (50 m'de yakınsama ölçülmedi, yalnız süre); tek iş parçacığında ping başına p99 0,93 ms (7 m) ve 0,79 ms (50 m). Tam kademe ölçütü ≈ 2,56 milyon ışına kadar sağlamadı; 1 280 000 ışında ping başına p99 504 ms. Tam kademe kartlı çalışmada yalnız azaltılmış ışınla seçilebilir. Ölçülen sınırlar (orta sahne, 7 m, çift yansıma, tek iş parçacığı): 20 000 ışın 9,33 ms'lik dinleme süresi alt sınırına sığıyor; 40 014 ışın yalnız **tahmini** 25,17 ms'lik ping aralığına sığıyor; on bir iş parçacığıyla 80 000 ışın 9,33 ms'ye sığıyor. Bu ışın sayılarının referansa göre farkı raporda. İşçi iş parçacıklarının dünya adım süresine etkisi ölçülmedi; bu rakamlar bütünleşik kartlı çalışmanın yeterlilik kanıtı değildir. Tam ayrıntı yalnız gerçek zamanlı olmayan SIL koşuları içindir | MP7a (`MP7-sonar-suresi.md`); M11 |
| Sonar saçıcı kuralı | Kesin: bölüm 05 §2.4.3'teki kural hatalıdır (enerji ışın sayısıyla büyür). Geçerli kural: yüzey, gövdeye sabit hücrelere bölünür; hücreye düşen ışınların enerji ağırlıkları toplanır, genlik toplamın kareköküdür; faz ve benek, (gövde, hücre, yol türü, gerçekleşim) dörtlüsünden türer ve ışın sayısından bağımsızdır; aynı yol türündeki hücreler eşevreli, farklı yol türleri enerji olarak toplanır. Yakınsama büyüklükleri aynı belgededir | MP7a; `docs/gorevler/W0-MP7a.md` |
| Kalite kademesi tabanı | Açık: 6. bölümdeki sade kademe varsayılır. Windows hedef bilgisayarında (8 GB bellek, dahili ekran kartı; K16) ölçüm yapılmadı; ölçüm paketi hazırlanıyor | MP8; M12, M20, M21, M30 |
| Araç zinciri | Kesin (Mac): GNU Tools for STM32 14.3.rel1, STM32CubeProgrammer, ST-LINK gdbserver, CMake + Ninja ile derleme, yükleme ve hata ayıklama çalışıyor. Bu derleyici paketinde Picolibc yok; kart libc'si newlib-nano'dur, Picolibc kararı W2'de yeniden değerlendirilir | MP1a; M21 |

Bilgisayar tarafı gerçek zamanlı değildir: ölçümlerde 10–20 ms'lik seyrek gecikmeler görüldü (nedeni ayrıştırılmadı). Zamanlamayla ilgili her sözleşme (A1'de geç kalan yanıt, A2'de kare teslimi) bunu olağan durum sayar ve davranışını tanımlar (4).

## 9. Bölümler

| # | Dosya | Kapsam | Sınır (yazmaz) |
|---|---|---|---|
| 01 | `01-kart-yazilimi.md` | Başlatma, saat, bağlantı sürücüsü, döngü zamanlaması, portun kart tarafı, bellek ve kesme düzeni, gözetim zamanlayıcısı, karttaki ekranın kullanımı | Kestirim ve kontrol algoritmaları (02), mesaj içerikleri (06) |
| 02 | `02-ortak-cekirdek.md` | Port arayüzü (A4), kestirim, güdüm, kontrol, itki tahsisi, kipler, görev yönetimi, arıza güvenliği, sayı tipleri, SIL koşucusu | Atış kontrol (03), donanım (01) |
| 03 | `03-atis-kontrol.md` | Algılama kaydının arayüzü, sonar verisinden algılama kaydı üretimi, durum makinesi, emniyet zinciri, atış olayının sözleşmesi, sanal emniyet girdileri. Sözleşmeler (W1) yalnız emniyet zincirini ve arayüzleri bağlar: hedef seçimi, hedefleme çözümü ve atış ölçütü sözleşmelerde tanımlanmaz ve hiçbir arayüzde (opak alan, günlük kodu ya da ret nedeni dahil) taşınmaz | Torpido fiziğinin kendisi (04), kamera görüntüsünün üretimi (07). Atış çözümünün ayrıntılandırılması (hareketli hedefe önleme, yörünge eniyileme) ve torpidonun yeteneklerinin artırılması kapsam dışıdır |
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

## 10. Bölümlerin iskelete önerileri: kararlar (sürüm 0.8)

Açık bulgular kaydının 6. başlığındaki yirmi altı öneri burada karara bağlanmıştır; numaralar o listedeki numaralardır. "Kabul" edilenler yukarıdaki bölümlere işlendi. Bölüm belgeleri (01–09) bu kararlara göre yeniden yazılmaz; sözleşmeler ve uygulama işleri iskeleti esas alır.

| # | Öneri (özü) | Karar |
|---|---|---|
| 1 | Kontrol tetikleyicisini taşıma ayrıntısından ayırmak | Kabul: tetik, adım partisinin kapanışıdır (4) |
| 2 | Bariyer tablosunun ifadesi; sabit uygulama gecikmesi | Kabul: parti başına yanıt, sabit `L_act`; geç kalan yanıtın kuralı ayrı (4) |
| 3 | Menzil profilinde zaman damgası | Kabul (3.1): çok yollu profilde kutu zamanı doğrudan yolun nominal zamanıdır |
| 4 | Sıcaklık birimi | Kabul: °C (3.1) |
| 5 | İtici komutunun ESC darbe genişliği olarak taşınması | Kabul (A1 satırı) |
| 6 | Algılama kaydı sensör çerçevesinde | Kabul (3) |
| 7 | İzin veren olayın süreli olması | Kabul (3.2); kalıcılık ve taşma davranışı A1 sözleşmesinde |
| 8 | Fırlatıcı durumu ve sanal emniyet girdileri; "bakım" girdisi | Satır 5. bölümde var. "Bakım" girdisi eklenmez: üç girdi (acil durdurma, silahlandırma fişi, emniyet pimi) yeterlidir |
| 9 | Torpido varsayılanlarının varsayım olduğu notu | Kabul (5) |
| 10 | İç basınç ve iç sıcaklık sensörü | Eklenmez: ilk sürümün sensör seti 5. bölümdeki gibidir; sızıntı algılayıcısı arıza girdisi olarak yeterlidir |
| 11 | Depo düzeni eklemeleri | Kabul (7); protokol tek modül (`protocol/` altında `ida_link`) |
| 12 | Çekirdek ISO C17, platform `gnu17` | Kabul (2, 7) |
| 13 | USB yığını yedeğinin adı | Geçersiz: USB yolu kaldırıldı (K17) |
| 14 | A1'in iki aşamada dondurulması | Geçersiz: ölçümler sözleşmelerden önce yapıldı |
| 15 | Emniyet kabulünün kademeden bağımsızlığı | Kabul (6) |
| 16 | Sanal seri port ile kontrol hızı arasındaki bağ | Yerini ölçüme dayalı karara bıraktı: 200 Hz ve `L_act` = iki periyot (4, 8) |
| 17 | Harici yer istasyonu arayüzü | İlk sürümde yok: yer istasyonu işlevi web panellerindedir (3). Harici yazılım hedef değildir |
| 18 | A2 içeriğinin genişletilmesi | Kabul (A2 satırı); A8 için yeni numara gerekmez |
| 19 | Tek sahnede iki zaman | Kabul, gerek olarak (8); ölçümü Godot sahnesinin ilk işinde |
| 20 | Sade kademede dalga seçeneği | Karar yok: maliyeti ölçülmeden eklenmez (Windows ölçümüyle birlikte değerlendirilir) |
| 21 | Kamera yedeğe düşerse kör bölge notu | Koşullu not: kamera birincil yolu Mac'te geçti (8). Windows'ta yedeğe düşülürse, sonarın kör bölgesindeki algılama pencerenin görünür olmasına bağlı kalır; bu durumda sınır olarak belgelenir |
| 22 | A6 içeriğinin ifadesi | Kabul (A6 satırı) |
| 23 | Tekrar oynatmanın üç düzeyi | Kabul (A6 satırı) |
| 24 | Ek Linux derleme işi; karta bağlı koşucu | Eklenmez (7) |
| 25 | İskeleti etkileyen açık sorular | Mimar varsayılanları: harici yer istasyonu hedef değil (17); derleme zamanında Python ve Node kullanımı 7. bölümdeki gibi; kamera görünürlükten bağımsız, tek kamera, üst sınır 640 × 480 ve 30 Hz (kademe değerleri A2 sözleşmesinde, Windows ölçümüne göre; 0.9'da düzeltildi); en düşük macOS hedefi geliştirme makinesinin ana sürümüdür (26); görselleştirici ayrı süreçtir, gömülü Godot kitaplığı kullanılmaz |
| 26 | Web paneli tercihi ile yerleşim önerisinin ayrımı | Not: paneller web teknolojisiyledir (K9, kesin); panelin ayrı pencerede mi gömülü mü duracağı 08'in önerisidir, sözleşmeyi etkilemez |

Açık bulgular kaydında K17 nedeniyle anlamı değişen ve hem USB'ye hem sanal seri porta değinen bulgular (AB-003, AB-211, AB-215, AB-224) A1 sözleşmesi işinde, bu sürümdeki taşıma kararına göre ele alınır: USB'ye özgü kısımları düşer, sanal seri port için geçerli kısımları kalır.
