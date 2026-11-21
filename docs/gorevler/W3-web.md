# W3-web — web panelleri (dilim 1: bağlantı kitaplığı, sahte sunucu, beş panel)

Tarih: 2026-11-21. Yazan: `ida-uygulayici` (Opus). İnceleyen: sol (tek tur). Dal: `is/w3-web`. Ortak kurallar: `docs/gorevler/W3-ortak.md`.

## Amaç

`web/` altında laboratuvarın tarayıcı arayüzünü başlatmak. Gerçek A3 sunucusu (sim sunucusunun içinde, C++) henüz yok; bu iş panelleri A3 sözleşmesinin tel biçimini konuşan **sahte bir sunucuya** karşı geliştirir, böylece gerçek sunucu geldiğinde paneller değişmeden bağlanır. Kullanıcı ilk kez çalışan ekranlar görecek; görünüş önemlidir.

Okunacaklar: A3 sözleşmesi (§3 veri tanımları: WebSocket ikili telemetri çerçeveleri, kanallar, HTTP uçları; §4 davranış; §5 hatalar); `docs/mimari/08-laboratuvar-uygulamasi.md` §2.1, §2.3, §2.4; iskelet §3.1 (çerçeveler, birimler), §5, §6.

## Araçlar (Rust tabanlı araç yasaktır)

TypeScript; derleme `esbuild` + tür denetimi `tsc`; paket yöneticisi `npm`, kilit dosyası depoda; test Node'un yerleşik test koşucusu; grafik `uPlot`. Kullanılmayacaklar: Vite (güncel sürümleri Rust tabanlı parçalar içerir), Rolldown, SWC, Biome, Lightning CSS, Tailwind 4, Turbopack, Parcel, Deno. Çatı (React vb.) kullanılmaz: paneller küçük, düz TypeScript modülleridir. CSS düz CSS'tir. Her paket açık sürümle sabitlenir; eklediğin her paketin Rust içermediğini doğrula ve raporla. Hedef: 8 GB bellekli, dahili ekran kartlı Windows bilgisayarında güncel Edge/Chrome ve macOS'ta Safari/Chrome.

## Kapsam (dilim 1)

1. **A3 istemci kitaplığı (`web/src/a3/`):** WebSocket ikili çerçevelerinin çözümü, kanal aboneliği, yeniden bağlanma, HTTP kontrol çağrıları; A3'teki her çerçeve türü için tip.
2. **Sahte A3 sunucusu (`web/arac/sahte-a3/`, Node):** A3 tel biçimiyle, yazılı bir senaryoyu yayınlar: araç silahsız başlar, 3 m'ye dalar, dört yol noktasını dolaşır, bir itici arızası olur ve toparlanır; Ping360 taraması, Ping1D, durum, sayaç ve olay kanalları. Yalnız geliştirme aracıdır; ürünle dağıtılmaz. Değerleri basit kinematikle üretir (fizik değildir; ekranda "sahte veri" etiketi görünür).
3. **Ortak durum şeridi:** çalıştırma durumu, zaman kipi, sim zamanı, geçerlilik, kaçırılan adım (08 §2.1).
4. **Paneller** (her biri ayrı sayfa olarak açılabilir):
   - *Araç ve sistem durumu.*
   - *Canlı grafikler:* kanal seçimi, pencere uzunluğu; yer gerçeği ile kart kestirimi ayrı renk.
   - *Harita:* NED yerel düzlemde ızgara (Canvas 2B), gerçek iz, kestirim izi, yol noktaları, hedef, ölçek ve kuzey; yakınlaştırma ve kaydırma. Coğrafi altlık (MapLibre) sonraki dilimdir.
   - *Sonar:* Ping360 kutupsal tarama ve Ping1D zaman–menzil şeridi (Canvas; satır zamanı darbe gönderim anı).
   - *Atış kontrol ve emniyet durumu:* yalnız gösterim (durum, sanal acil durdurma, fiş, pim, tüp durumu; yetkili durum ile kartın geri okuması yan yana, fark işaretli). **Atış tetikleyen ya da emniyet girdisi değiştiren hiçbir denetim bu dilimde yoktur.**
5. **Görsel dil:** bütün panellerde tek renk ve yazı düzeni (CSS değişkenleri); koyu, alet paneli görünümü; durum renkleri (iyi, uyarı, kötü) vurgu renginden ayrı; sayılar sabit genişlikli rakamla; 1280 × 720'de ve dar pencerede kullanılabilir. Arayüz metinleri Türkçe.
6. **Çalıştırma:** `npm run gelistir` sahte sunucuyu ve panelleri başlatır; `npm run derle` statik dosyaları üretir; `npm test`.

## Kapsam dışı

Senaryo editörü, hata enjeksiyonu, kayıt ve tekrar oynatma panelleri; görev düzenleme; gerçek A3 sunucusu; erişim jetonu; CMake ve CI bağlantısı (öneri olarak raporla).

## Dosya sahipliği

Yazılır: `web/**`.

## Kabul

1. `npm ci`, `npm run derle`, `npm test` temiz; `tsc` katı kipte hatasız.
2. Çözücü testleri: A3'teki her çerçeve türü için sözleşmedeki alan düzeninden elle kurulmuş bayt dizisi doğru çözülür; bozuk ve kısa çerçeve reddedilir.
3. Sahte sunucunun ürettiği her çerçeve istemci çözücüsünden geçer (gidiş-dönüş testi).
4. Beş panel sahte sunucuya bağlanıp canlı çalışır; her panelin ekran görüntüsü alınır (Chrome başsız kipte ya da eldeki tarayıcı aracıyla) ve yolları raporda verilir; ekran görüntüleri depoya girmez.
5. Bağlantı kopunca şerit bunu gösterir ve sunucu dönünce paneller kendiliğinden toparlar.
6. Yük (ölçüm, bu Mac): beş panel açıkken sekme başına bellek ve işlemci kullanımı raporlanır.
7. Bağımlılık listesi ve her birinin Rust içermediğinin doğrulaması raporda.

A3'ün ikili biçiminde ya da kanal tanımlarında uygulanamayan bir yer bulursan sözleşmeye en yakın makul okumayı uygula ve "Okumalar"a yaz; gerçek sunucu aynı okumayla yazılacak.
