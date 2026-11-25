# W5-web-komut — panelden çalıştırma denetimi ve senaryo paneli

Tarih: 2026-11-25. Yazan: `ida-hafif-uygulayici` (Sonnet; K28). İnceleyen: `ida-inceleyici` (Opus; emniyet dışı dilim; K23, K27). Dal: `is/w5-web-komut` (taban `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `W4-ortak.md`, `W5-ortak.md` (W5-K43 test kuralları).

## Amaç

Beş panel bugün yalnız gösteriyor: tarayıcıdan bir çalıştırmayı başlatmak, duraklatmak ya da senaryo seçmek mümkün değil (`web/src` içinde istemci kaydı dışında durum değiştiren istek yok). Bu dilim paneli laboratuvarı yöneten arayüze çevirir: kullanıcı tarayıcıdan senaryoyu görür, yükler, çalıştırmayı başlatır ve denetler.

## Okunacaklar

`web/BENIOKU.md`; `docs/sozlesmeler/A3-web.md` §3.6 (uçlar), §3.8 (ön denetim raporu), §3.9 (hata kodları), §4.6 (komut işleme), §4.7 (durum makinesi; Tablo 4.7 ve 4.7a), §4.9 (senaryo); `docs/mimari/08-laboratuvar-uygulamasi.md` §2.3, §2.4; `web/src/a3/http.ts`, `web/src/ortak/kabuk.ts`, `web/arac/sahte-a3/`.

## Kapsam

1. **Komut katmanı** (`web/src/a3/`): durum değiştiren isteklerin tek yolu. `Ida-Istemci` başlığı ve sözleşmenin istediği revizyon alanları; 202 yanıtının `komut_sonucu` olayıyla (olay kaçarsa `GET /komutlar/{komut_no}` ile) eşlenmesi; §3.9 hata kodlarının okunur Türkçe iletiye çevrilmesi; süre sınırı; bağlantı koptuğunda bekleyen komutun sonucunun "bilinmiyor" diye gösterilmesi.
2. **Çalıştırma denetimi** (ortak durum şeridi; her panelde): başlat, duraklat, sürdür, adımla (adım sayısıyla), hız, durdur, sıfırla. Hangi durumda hangi düğmenin etkin olduğu Tablo 4.7'den gelir; etkin olmayan düğme nedenini söyler.
3. **Senaryo paneli** (yeni sayfa `web/sayfalar/panel/senaryo.html`, `web/src/paneller/senaryo.ts`): dosya listesi, seçilen dosyanın metni (salt okunur), doğrula (A5 raporu: hata listesi, yol ve satır), yükle, yüklü çözümlenmiş senaryonun görüntüsü ve davranış özeti, ön denetim raporu tablosu (madde, durum, ölçülen, sınır, birim).
4. **Sahte A3 sunucusu** (`web/arac/sahte-a3/`): senaryo ve ön denetim uçlarını ve çalıştırma komutlarını sözleşme şemalarına uygun taklit eder.
5. **Testler** (`web/test/`): komut katmanı (kabul, ret, süre aşımı, kopma sırasında bekleyen komut); düğme etkinliği Tablo 4.7'nin her satırı için; senaryo akışı (listele → doğrula → yükle → başlat). İstek ve yanıt gövdeleri `docs/sozlesmeler/sema/a3-kanallar.json` `$defs` şemalarına karşı doğrulanır.

## Mimar kararları

- **W5-K44 — İyimser gösterim yok.** Düğmelerin ve durum şeridinin gösterdiği durum yalnız sunucudan gelen veriyle değişir. "Komut gönderildi" ile "uygulandı" ayrı gösterilir; reddedilen komut görünür biçimde reddedildi yazar.
- **W5-K45 — Gerçek sim senaryo uçlarını henüz sunmuyor** (W5-a3-senaryo'da gelecek). Bir uç yoksa panel "sunucu bu ucu sunmuyor" der, çökmez ve yeniden denemez. Çalıştırma denetimi senaryo paneline bağımlı değildir: senaryo komut satırından yüklenmişse (`--senaryo … --a3-bekle`) düğmeler çalışır.
- **W5-K46 — Emniyet ve yer istasyonu yok.** `/emniyet/degistir`, `/emniyet/onay_jetonu`, `/yer_istasyonu/komut`, `/yer_istasyonu/onay_jetonu` uçlarına istek atan kod bu dilimde yazılmaz (ayrı dilim: W5-web-emniyet; emniyet sınıfı).

## Kapsam dışı

Form tabanlı senaryo editörü ve `PUT /senaryolar/{ad}`; hata enjeksiyonu; kayıt ve izleme uçları; `yeniden_kostur`; `sim/`, `core/`, `firmware/`, `viz/`; `tests/butunleme/CMakeLists.txt` (PR #55 aynı dosyada; gerçek sim adımı var olan Node uçtan uca testine eklenir, yeni CTest kaydı açılmaz); yeni npm bağımlılığı. Atış tetikleyen hiçbir denetim yazılmaz (A3-K99).

## Kabul

1. `npm test` geçer (tür denetimi dahil); yeni testlerin her biri, sınadığı davranış bozulduğunda düşer (en az komut katmanı ve düğme etkinliği için düşürülerek gösterilir).
2. Gerçek sim'e karşı: var olan Node uçtan uca testi, panelin komut katmanıyla başlat → duraklat → adımla → sürdür → hız → durdur zincirini koşar ve her adımda sunucunun bildirdiği durumu doğrular. `host-debug` tam tur yerelde bir kez.
3. `npm run olc -- --ekran` ile durum şeridinin ve senaryo panelinin ekran görüntüsü alınır (depoya girmez; yolu raporda).
4. Windows ve macOS CI yeşil.

## Rapor

PR ve dal ucu; kabul maddeleri (geçti / kısmen / koşturulmadı); düşürülerek gösterilen testler; sözleşmede belirsiz bulunan yerler ve seçilen okuma; bulunan ürün kusuru (`sim/a3`, `sim/sunucu`) varsa düzeltilmeden, kanıtıyla.
