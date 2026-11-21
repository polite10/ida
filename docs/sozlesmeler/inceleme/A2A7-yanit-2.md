# A2 ve A7 sözleşmeleri: sol ikinci incelemesine yanıt (2. ve son düzeltme turu)

| | |
|---|---|
| İnceleme | `docs/sozlesmeler/inceleme/A2A7-sol-2.md` |
| Uygulanan kararlar | Mimarın 2. düzeltme turu kararları (Y1, Y2, Y3 ve notlar) |
| Değişen dosyalar | `docs/sozlesmeler/A2-gorsellestirici.md`, `docs/sozlesmeler/A7-sahne.md`, `sim/include/ida/a2_wire.h` (`assets/manifest.schema.json` ve `assets/ornek/manifest.json` bu turda değişmedi) |
| Kural numaraları | Mevcut numaralar korundu; yeni kurallar A2-K86–K92 |

## Madde madde

| # | Önem | Durum | Yer | Ne yapıldı |
|---|---|---|---|---|
| Y1 | kritik | giderildi | A2-K46, K56, K59, K60, K61, K64, K85; yeni §4.10 (K86–K89 ve E1–E24 durum tablosu); §5 H18, H27, H28; A2-U14, U24; `a2_wire.h` `SimKareDusmeNedeni`, `kTuketiciBasinaAzamiEmekliHalka` | (a) Emeklilik anında halkadan yeni teslim kapanır; teslim için halkanın etkin olması koşul (K46, K86 (1)). (b) Hazır kareler düşer: neden kodu `halka_emekli_hazir` (`SimKareDusmeNedeni` = 8), sayılır ve A6'ya yazılır (K86 (2)); A8 kare durumlarına eşlemesi "A8'den beklenen"de. (c) Emekli halkaya atıf yapan yanıtlar yalnız isteği kapatır, teslim üretmez, `emekli_halka_yaniti` sayılır (K87, E15). (d) Sim eski eşlemesini ve tutamağını, emeklilik anında bağlı her taraf daha yeni halkayla el sıkışana ya da A2 oturumu bitene kadar tutar; bağlantı kopması el sıkışma sayılmaz (K88, K60). Tüketici başına emekli halka sınırı 2 (tahmin); aşılacaksa o tüketicinin kamera yolu kapanır, olay A3 ve A6'ya gider, yeni bağlantı ve tam el sıkışmayla döner (K89, E24). Durum makinesi tek tabloda (§4.10) |
| Y2 | kritik | giderildi | A7-K44, A7-U12, §5, AB-102 satırı | Hausdorff iddiası bırakıldı. Denetim, katı cisme göre iki tek yönlü ölçüttür: eksik yok `d(s, D) ≤ τ − δ`, taşma yok `d(g, K) ≤ τ − δ`; hiçbir örnek atılmaz. İçeride olma sınaması için tek yöntem: sarım sayısı (Van Oosterom–Strackee katı açısı); sınır üstü noktalar 1e-9 m uzaklık eşiğiyle içeride sayılır; ışın kullanılmadığı için dejenere ışın durumu yoktur. `S`'nin kapalı, manifold ve dışa yönlü olması A7-K26 ve K25 ile zaten zorunluydu; K44 bunu açıkça bağladı ve parçalara da dışbükeylik denetimi uygulandı. Ölçütün `D`'nin iç boşluklarını sınamadığı açıkça yazıldı |
| Y3 | kritik | giderildi | A2-K29; yeni §4.11 (K90–K92); §3.2 ileti tablosu; §3.3 `GovdeTablosu`; §5 H29; A2-U23; `a2_wire.h` `GovdeTablosu` (24 B), ileti türü `govde_tablosu` = 0x020A | Her `atlama`dan sonra, ilk `durum` ve `kamera_istegi`'nden önce, yeni zamanda yaşayan bütün gövdelerin tam tablosu gönderilir. Alıcı tabloda olmayanları ölmüş sayar, sahneyi tablodan kurar; tablodan önce gelen `durum` ya da `kamera_istegi` protokol hatasıdır (K90, K91). Kimlik eşlemesi: `govde_kimligi` oturum boyunca tek gövdeyi gösterir ve hep aynı varlık indeksi ve türle gelir; tekrar izlemede atlamadan sonra yeniden görünen kimlik aynı gövdedir; `varlik_indeksi` bir kimlik değildir, birden çok gövde aynı varlığı kullanabilir (K92) |
| Not | — | uygulandı | A2-A10 | Bellek görünürlüğü açık kaldı; sol'un notu gerekçeye eklendi: C++20 çit eşzamanlaması atomik işlemler üzerinden tanımlı, soket gönderimi ve alımı bu ilişkiye girmez; Arm bariyer kuralları tek başına uçtan uca soket güvencesini kanıtlamaz |
| Not | — | dokunulmadı | iskelet §10 md.25 | Mimarın 0.9'da düzelteceği kayıt |

## Kararların uygulanmasında yorum ve bulgu

1. **Y3, "oturum başındaki tablo bildirimiyle aynı ileti türleri":** oturum başındaki gövde tablosu `hosgeldin` iletisinin içinde, yeniden gönderilemeyen bir ileti. Bu yüzden aynı girdi yapısını (`GovdeBilgisi`) taşıyan yeni bir ileti türü (`govde_tablosu`) tanımlandı. Blok tek iletidir: başlangıcı `atlama` olayı, sonu `govde_tablosu` iletisidir; ileti bütün olarak alındığı için "yarım blok" durumu yoktur.
2. **Y1, tüketici başına sınır:** kuralların yürütülebilir bir modeliyle sınanınca görüldü ki A2-K61 (tüketiciye yalnız el sıkıştığı etkin halkadan kare verilir) ve A2-K88 (daha yeni halkayla el sıkışma eski halkayı o tüketici için bırakır) birlikte, kurallara uyan bir akışta bir tüketici adına **en çok bir** emekli halkanın birikmesine izin veriyor. 2'lik sınır uygulandı, ama bu iki kuraldan birinin uygulamada çiğnenmesine karşı savunma işlevi görüyor; K89'a not olarak yazıldı.
3. **Y1 (d), "sim oturumu":** A2 oturumu (görselleştirici bağlantısının ömrü) olarak yorumlandı, çünkü halkalar A2 oturumuna aittir (A2-K65) ve oturum bitince bütün halkaları tutan tek taraf, yani sim, eşlemelerini bırakır. Bu, okuyucuların kendi eşlemelerini etkilemez; emekli halkaya kimse yazmadığı için güvenlik koşulu da değişmez.

## Kendi sınamalarım (bu turda)

**Emeklilik durum makinesi.** §4.10 tablosunu izleyen yürütülebilir bir model kuruldu (betik depoya girmedi). 7 senaryonun hepsi geçti. Senaryolar şu durumları kapsadı:
- emeklilik anındaki hazır karenin yeni halkayla el sıkışmış tüketiciye verilmemesi;
- emeklilikten sonra gelen yanıtın yalnız isteği kapatması;
- bağlantısı kopan tüketici varken halkanın oturum sonuna kadar tutulması;
- bütün tarafların el sıkışmasıyla halkanın serbest bırakılması;
- tüketici başına sınır (kurallara uyan akış ve zorlanmış durum);
- emekli halkanın yuvasının bir daha ayrılmaması;
- yinelenen bırakmanın yok sayılması.

Değişmezler her adımda denetlendi:
- okunuyor olabilecek yuva ayrılmaz;
- emekli halkadan teslim yapılmaz;
- emekli halkada hazır kare kalmaz;
- tüketici başına sınır aşılmaz.

**A7-K44.** Yeni tanım `numpy` ile yürütülebilir olarak yazıldı: ızgara örneklemesi, nokta–üçgen uzaklığı, sarım sayısı. Dört sınama vektörü koşuldu ve hepsi beklenen sonucu verdi:

| Vaka | τ | Ölçüt 1 en büyük | Ölçüt 2 en büyük | Sonuç |
|---|---|---|---|---|
| Delikli prizma + deliği dolduran kutu (hacim 24 m³ doğrulandı) | 0,1 m (eşik 0,08) | 0 | 0,9929 m (örnekli; gerçek üst değer 1 m) | ret |
| Birim küp, iki kusursuz yarı | 0,01 | 0 | 0 | kabul |
| Birim küp, eksik parça | 0,01 | 0,5 m | 0 | ret |
| Büyütülmüş küp (+2,5 mm) | 0,01 / 0,0055 / 0,005 / 0,004 | 0 | 4,330127 mm (hesapla aynı) | kabul / kabul / ret / ret |

Delikli prizmada τ = 0,1 yalnız örnek sayısını sınırlamak için seçildi; ret sonucu ölçeğe bağlı değil (her `τ ≤ 1 m` için eşik < 1 m).

## Başlık ve şema

- `c++ -std=c++20 -fsyntax-only -Wall -Wextra -Wpedantic -x c++ sim/include/ida/a2_wire.h` hem arm64'te (yerel) hem `--target=x86_64-apple-macos13` ile uyarısız geçti. MSVC çalıştırılmadı.
- Başlıkta 27 yapı boyutu ve 54 `offsetof` denetimi var. Yeniler:
  - `GovdeTablosu` = 24 B; `calisma_nesli` ofset 16, `govde_sayisi` ofset 20;
  - `kAzamiGovdeTablosuYuku` ≤ 1 MiB.
- `assets/ornek/manifest.json` şemadan yeniden geçti. 14 bozuk türev reddedildi; CoACD'li geçerli türev geçti ve 5 bozuk CoACD türevi reddedildi.
