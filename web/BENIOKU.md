# web/ — laboratuvar web panelleri

Laboratuvarın tarayıcı arayüzü (iskelet §7; bölüm 08; görev `docs/gorevler/W3-web.md`). Paneller sim sunucusuyla yalnız A3 sözleşmesi (`docs/sozlesmeler/A3-web.md`) üzerinden konuşur. Node yalnız derleme, test ve geliştirme aracında kullanılır; çalışan simülatörde ya da kartta koşmaz.

## Komutlar

`web/` dizininde:

| Komut | Ne yapar |
|---|---|
| `npm ci` | Kilit dosyasındaki sabit sürümleri kurar |
| `npm run gelistir` | Panelleri izleme kipinde derler ve sahte A3 sunucusunu başlatır; açılış adresini yazar (`http://127.0.0.1:47100/#erisim=<jeton>`). Kaynak değişince yeniden derler; sahte sunucu aynı jetonla yeniden başlar, açık paneller kendiliğinden toparlanır |
| `npm run gelistir -- --kip hil_1x --atla 60` | Aynısı; HIL 1× kipini taklit eder ve sim zamanını 60 s'ye atlatır |
| `npm run gelistir -- --emniyet bilinmiyor` (ya da `gecersiz`) | Emniyet gösterimi sınaması: kart geri okuması bilinmiyor (255) ya da emniyet kanallarında değer listesi dışı değerler |
| `npm run derle` | Katalog güncelliği + katı tip denetimi + üretim derlemesi: `dist/` (statik dosyalar) ve `.derleme/sahte-a3.mjs` |
| `npm test` | Tip denetimi, ardından testler (Node'un yerleşik test koşucusu) |
| `npm run katalog` | `src/a3/katalog.ts`'yi `docs/sozlesmeler/sema/a3-kanallar.json`'dan yeniden üretir |
| `npm run olc -- --ekran ekran` | Başsız Chrome'da her panelin ekran görüntüsünü alır (`ekran/` depoya girmez) |
| `npm run olc -- --olc 30` | Beş panel aynı anda açıkken sekme başına işlemci ve bellek ölçer (macOS/Linux; `ps` kullanır) |
| `npm run olc -- --kopma ekran --paneller durum` | Bağlantı kopması ve toparlanma görüntüleri |

Paneller: `/` (liste), `/panel/durum.html`, `/panel/grafik.html`, `/panel/harita.html`, `/panel/sonar.html`, `/panel/atis.html`. Erişim jetonu adresin `#erisim=` parçasındadır (A3-K05); panel bağlantıları onu taşır.

## Yapı

```
src/a3/          A3 istemci kitaplığı
  katalog.ts       ÜRETİLİR: kanal kataloğu, yük düzenleri, yük tipleri (elle değiştirilmez)
  cerceve.ts       ikili çerçeve çözücüsü (A3 §3.1–3.4, K18–K28)
  kodlayici.ts     kodlayıcı (yalnız sahte sunucu ve testler)
  baglanti.ts      sürüm el sıkışması, WebSocket, abonelik, yeniden bağlanma, olay boşluğu doldurma
  http.ts          HTTP uçları (okuma uçları; genel komut gönderimi)
  olay.ts, iletiler.ts
src/ortak/       kabuk (üst çubuk + durum şeridi), tema, biçimler, olay günlüğü
src/paneller/    beş panel ve kök sayfa
sayfalar/        HTML iskeletleri
arac/            derleme, geliştirme, test, ölçüm betikleri; sahte-a3/ (yalnız geliştirme)
test/            birim, sözleşme ve bağlantı testleri
```

## Sahte A3 sunucusu

`arac/sahte-a3/` yalnız geliştirme aracıdır; ürünle dağıtılmaz. A3 tel biçimini (HTTP, WebSocket, ikili çerçeveler, abonelik, seyreltme, erişim koruması) konuşur ve yazılı bir senaryoyu **basit kinematikle** oynatır: araç yüzeyde silahsız başlar, silahlanır, 3 m'ye dalar, dört yol noktasını dolaşır, ~72 s'de 3 numaralı itici zayıflar ve denetleyici toparlar, ~100 s ve ~118 s'de senaryodan emniyet girdisi değişir, ~125–140 s fırlatıcı geri okuma arızası kartın okumasını yetkili durumdan ayırır; sonra yüzer, biter ve yeniden başlar. Değerler fizik değildir; panellerde "SAHTE VERİ" rozeti görünür (`sim_surumu` "sahte" ile başlar). Durum değiştiren uçları uygulamaz (404).

## Araçlar ve bağımlılıklar

Rust tabanlı araç yoktur (W3-web "Araçlar"). Node'un yerleşik TypeScript tip silme özelliği (Amaro/SWC) kullanılmaz: testler ve sahte sunucu esbuild ile JavaScript'e çevrilip çalıştırılır.

| Paket | Sürüm | Görev | Dil |
|---|---|---|---|
| `esbuild` (+ `@esbuild/<platform>`) | 0.28.2 | derleme | Go |
| `typescript` | 5.9.3 | tip denetimi (`tsc`) | JavaScript |
| `uplot` | 1.6.32 | grafik | JavaScript |
| `ws` | 8.22.1 | sahte sunucunun WebSocket sunucusu | JavaScript |
| `@types/node`, `@types/ws` (+ `undici-types`) | 22.20.5, 8.18.1 | tip tanımları | — |
