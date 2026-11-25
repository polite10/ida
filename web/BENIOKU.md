# web/ — laboratuvar web panelleri

Laboratuvarın tarayıcı arayüzü (iskelet §7; bölüm 08; görev `docs/gorevler/W3-web.md`). Paneller sim sunucusuyla yalnız A3 sözleşmesi (`docs/sozlesmeler/A3-web.md`) üzerinden konuşur. Node yalnız derleme, test ve geliştirme aracında kullanılır; çalışan simülatörde ya da kartta koşmaz.

## Komutlar

`web/` dizininde:

| Komut | Ne yapar |
|---|---|
| `npm ci` | Kilit dosyasındaki sabit sürümleri kurar |
| `npm run gelistir` | Panelleri izleme kipinde derler ve sahte A3 sunucusunu başlatır; açılış adresini yazar (`http://127.0.0.1:47100/#erisim=<jeton>`). Kaynak değişince yeniden derler; sahte sunucu aynı jetonla yeniden başlar, açık paneller kendiliğinden toparlanır |
| `npm run gelistir -- --kip hil_1x --atla 60` | Aynısı; HIL 1× kipini taklit eder ve sim zamanını 60 s'ye atlatır |
| `npm run gelistir -- --elle` | Sahte sunucu elle yönetilir: çalıştırma kendiliğinden başlamaz ve bitince yeniden başlamaz; başlat, duraklat, adımla, sürdür, hız, durdur, sıfırla yalnız panelden (üstteki çalıştırma denetimi) gelir |
| `npm run gelistir -- --emniyet bilinmiyor` (ya da `gecersiz`) | Emniyet gösterimi sınaması: kart geri okuması bilinmiyor (255) ya da emniyet kanallarında değer listesi dışı değerler |
| `npm run derle` | Katalog güncelliği + katı tip denetimi + üretim derlemesi: `dist/` (statik dosyalar) ve `.derleme/sahte-a3.mjs` |
| `npm test` | Tip denetimi, ardından testler (Node'un yerleşik test koşucusu) |
| `npm run katalog` | `src/a3/katalog.ts`'yi `docs/sozlesmeler/sema/a3-kanallar.json`'dan yeniden üretir |
| `npm run olc -- --ekran ekran` | Başsız Chrome'da her panelin ekran görüntüsünü alır (`ekran/` depoya girmez); `--etkilesim` durum panelinde Duraklat'a, senaryo panelinde hatali.toml + Doğrula'ya basar; `--sim <açılış adresi>` sahte sunucu yerine çalışan `ida_sim`'e bağlanır |
| `npm run olc -- --olc 30` | Beş panel aynı anda açıkken sekme başına işlemci ve bellek ölçer (macOS/Linux; `ps` kullanır) |
| `npm run olc -- --kopma ekran --paneller durum` | Bağlantı kopması ve toparlanma görüntüleri |

Paneller: `/` (liste), `/panel/durum.html`, `/panel/grafik.html`, `/panel/harita.html`, `/panel/sonar.html`, `/panel/atis.html`, `/panel/emniyet.html`, `/panel/senaryo.html`. Erişim jetonu adresin `#erisim=` parçasındadır (A3-K05); panel bağlantıları onu taşır.

## Yapı

```
src/a3/          A3 istemci kitaplığı
  katalog.ts       ÜRETİLİR: kanal kataloğu, yük düzenleri, yük tipleri (elle değiştirilmez)
  cerceve.ts       ikili çerçeve çözücüsü (A3 §3.1–3.4, K18–K28)
  kodlayici.ts     kodlayıcı (yalnız sahte sunucu ve testler)
  baglanti.ts      sürüm el sıkışması, WebSocket, abonelik, yeniden bağlanma, olay boşluğu doldurma
  http.ts          HTTP uçları (okuma uçları; çalıştırma kontrolü ve senaryo yükleme taşıması)
  komut.ts         KomutYoneticisi: durum değiştiren komutların tek yolu (202 → komut_sonucu eşlemesi, süre sınırı, kopma)
  emniyet-istek.ts EmniyetIstekleri: emniyet girdisi ve yer istasyonu isteklerinin tek modülü (iki adımlı gevşetme, onay jetonu; DOM yok)
  etkinlik.ts      düğme etkinliği (A3 Tablo 4.7a ve 4.7b; saf işlev)
  hata.ts          A3 §3.9 hata kodlarının Türkçe iletileri
  olay.ts, iletiler.ts
src/ortak/       kabuk (üst çubuk + durum şeridi), çalıştırma denetimi (denetim.ts), acil durdurma şeridi (acil.ts), yer istasyonu parametre formu (sema-form.ts), tema, biçimler, olay günlüğü
src/paneller/    yedi panel (durum, grafik, harita, sonar, atış, emniyet girdileri, senaryo) ve kök sayfa
sayfalar/        HTML iskeletleri
arac/            derleme, geliştirme, test, ölçüm betikleri; sahte-a3/ (yalnız geliştirme)
test/            birim, sözleşme ve bağlantı testleri
```

## Sahte A3 sunucusu

`arac/sahte-a3/` yalnız geliştirme aracıdır; ürünle dağıtılmaz. A3 tel biçimini (HTTP, WebSocket, ikili çerçeveler, abonelik, seyreltme, erişim koruması) konuşur ve yazılı bir senaryoyu **basit kinematikle** oynatır: araç yüzeyde silahsız başlar, silahlanır, 3 m'ye dalar, dört yol noktasını dolaşır, ~72 s'de 3 numaralı itici zayıflar ve denetleyici toparlar, ~100 s ve ~118 s'de senaryodan emniyet girdisi değişir, ~125–140 s fırlatıcı geri okuma arızası kartın okumasını yetkili durumdan ayırır; sonra yüzer, biter ve yeniden başlar. Değerler fizik değildir; panellerde "SAHTE VERİ" rozeti görünür (`sim_surumu` "sahte" ile başlar). `--elle` verilmezse çalıştırma kendiliğinden başlar. Çalıştırma kontrolü uçlarını (`/calistirma/*`), `/senaryo/yukle` ve `/senaryo/dogrula`, `/senaryolar`, `/komutlar/{no}` uçlarını sözleşme şemalarına uygun taklit eder (`arac/sahte-a3/sema.ts` şemaları `a3-kanallar.json`'dan doğrular); senaryo dosyaları ve A5 raporu sahtedir (üç küçük dosya, iki satırlık kural). Emniyet girdisi (`/emniyet`, `/emniyet/onay_jetonu`, `/emniyet/degistir`) ve yer istasyonu (`/yer_istasyonu/komutlar`, `/onay_jetonu`, `/komut`) uçlarını sözleşmeye uygun taklit eder (iki adımlı gevşetme, tek kullanımlık jeton, revizyon bağı; `protocol/komutlar.json` listesi); arıza, kayıt ve izleme uçlarını uygulamaz (404).

## Araçlar ve bağımlılıklar

Rust tabanlı araç yoktur (W3-web "Araçlar"). Node'un yerleşik TypeScript tip silme özelliği (Amaro/SWC) kullanılmaz: testler ve sahte sunucu esbuild ile JavaScript'e çevrilip çalıştırılır.

| Paket | Sürüm | Görev | Dil |
|---|---|---|---|
| `esbuild` (+ `@esbuild/<platform>`) | 0.28.2 | derleme | Go |
| `typescript` | 5.9.3 | tip denetimi (`tsc`) | JavaScript |
| `uplot` | 1.6.32 | grafik | JavaScript |
| `ws` | 8.22.1 | sahte sunucunun WebSocket sunucusu | JavaScript |
| `@types/node`, `@types/ws` (+ `undici-types`) | 22.20.5, 8.18.1 | tip tanımları | — |

## Çalıştırma denetimi ve komut katmanı

Her panelin durum şeridinin altındaki satır çalıştırmayı denetler: başlat, duraklat, sürdür, adımla (adım sayısıyla; bir adım = 5 ms kontrol periyodu), hız, durdur, sıfırla. Kurallar:

- **Etkinlik yalnız sunucudan gelen durumdan türer** (`calistirma/durum` kanalı; A3 Tablo 4.7a/4.7b). Etkin olmayan düğmenin üzerine gelince ve tıklanınca nedeni yazılır.
- **İyimser gösterim yoktur (W5-K44).** Komut "gönderiliyor" → "kuyrukta · uygulanmadı" → "uygulandı" / "reddedildi" / "sonuç bilinmiyor" diye ayrı gösterilir; çalıştırma durumu şeritte yalnız sunucu yeni durumu bildirince değişir. Komut ekranın gösterdiği `run_id` ve `durum_rev` ile gider; eski bir ekran sunucuda `durum_rev_uyusmuyor` ile reddedilir ve bu görünür.
- **Süre sınırı ve kopma:** sonuç `komut_sonucu` olayıyla beklenir, olay kaçarsa `GET /komutlar/{no}` sorulur; 15 s (başlat için 60 s) içinde sonuç gelmezse ya da bağlantı koparsa kayıt "sonuç bilinmiyor" olur; sonra sonuç öğrenilirse kayıt düzelir.
- HIL kiplerinde duraklatma çalıştırmayı geçersiz kıldığı için onay ister (tarayıcı onay kutusu); adımla ve hız HIL'de kapalıdır.
- Çalıştırma denetimi satırında emniyet, yer istasyonu ya da atışa giden denetim yoktur (W5-K46, A3-K99); emniyet istekleri aşağıdaki bölümdedir.

**Senaryo paneli** (`/panel/senaryo.html`): dosya listesi, salt okunur metin, doğrula (A5 raporu: kural, yol, satır; bulgulu satırlar metinde işaretlenir), yükle, yüklü çözümlenmiş senaryonun özeti ve ön denetim raporu. Sunucu bir ucu sunmuyorsa (404/405) ilgili bölüm bunu söyler ve yeniden denemez (W5-K45); çalıştırma denetimi buna bağlı değildir.

## Emniyet girdileri ve yer istasyonu komutu (W5-web-emniyet)

Emniyet kararını sunucu ve çekirdek verir (A3 §4.11, §4.15); panel yalnız istek gönderir ve sunucunun bildirdiğini gösterir. Tek yol `src/a3/emniyet-istek.ts`'tir (`EmniyetIstekleri`); `KomutYoneticisi` ile aynı kayıt ve sonuç izlemeyi kullanır.

- **Acil durdur düğmesi** (`src/ortak/acil.ts`) her panelin üst bloğundadır: tek adım, onay yok. Emniyeti artıran yön olduğu için gövdesi onay, jeton ve `beklenen_emniyet_rev` taşımaz; sunucu da onu revizyon ya da bayat `run_id` yüzünden reddetmez (A3-K92, K119; W4-K19). Kısayolu yoktur.
- **Emniyet girdileri paneli** (`/panel/emniyet.html`): acil durdurma, tüp başına silahlandırma fişi ve emniyet pimi. Artıran yön (basılı, fiş çıkarılmış, pim takılı) tek adım. Azaltan yön iki adım: önce "…" düğmesiyle onay jetonu istenir, sonra işaretsiz başlayan onay kutusu işaretlenip gönderilir. Tabloda yetkili durum (sim) ile kartın geri okuması yan yana, ikisi de yalnız `emniyet/girdiler` kanalından.
- **W5-K51:** gösterge yalnız sunucudan gelir; düğmeye basmak göstergeyi değiştirmez. Her isteğin sonucu ayrı yazılır: `GÖNDERİLİYOR`, `KUYRUKTA — henüz uygulandığı bildirilmedi`, `UYGULANDI`, `UYGULANMADI` (istek gitmedi ya da sunucu reddetti) ya da `SONUÇ BİLİNMİYOR` (istek gitmiş olabilir, yanıt gelmedi).
- **W5-K52:** azaltan istek hiçbir koşulda kendiliğinden yinelenmez. Jeton tek kullanımlıktır, gönderilirken tüketilir; süre dolarsa (istemci tarafında 10 s − 1 s pay; sunucu ömrü 10 s, A3-K94), emniyet revizyonu artarsa, bağlantı koparsa, istek reddedilirse ya da sonucu bilinmezse jeton düşer ve iki adım baştan yapılır. Artıran yönde revizyon çakışması yoktur (revizyon bağı taşımaz), bu yüzden "güncel revizyonla yeniden gönderme" yazılmamıştır; hiçbir istek yinelenmez.
- **W5-K53:** azaltan yön için klavye kısayolu, toplu eylem ve önceden işaretli onay yoktur; jeton yalnız bellekte durur (depolamaya ve adres çubuğuna yazılmaz). Onay sayfa içindedir (tarayıcı `confirm` kullanılmaz).
- **W5-K54:** yer istasyonu bölümü yalnız sunucunun `GET /yer_istasyonu/komutlar` listesini gösterir; atış isteği komutu (`fc_fire`) bu panelde gösterilmez ve istemci listeye komut eklemez. Parametre formu komutun JSON Schema'sından üretilir (`sema-form.ts`; sunucu doğrulaması asıldır); basit türlerin dışındaki şemalar "bu formda desteklenmiyor" diye bildirilir. Genişletici komut (`onay: true`) iki adımlıdır; daraltıcı ve nötr komut tek adımdır.
