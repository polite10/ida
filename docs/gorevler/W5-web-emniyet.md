# W5-web-emniyet — panelden sanal emniyet girdileri ve yer istasyonu komutu

Tarih: 2026-11-25. Yazan: `ida-hafif-uygulayici` (Sonnet). İnceleyen: `ida-inceleyici` (Opus). **Sınıf: emniyet zincirine dokunur (K21):** tam inceleme + dar denetim; incelemesiz birleşmez (K27). Dal: `is/w5-web-emniyet` (taban `origin/main`; W5-web-komut `main`'e girdikten sonra başlar: aynı dosyalar). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `W4-ortak.md`, `W5-ortak.md` (W5-K43).

Yazar ataması üzerine not (mimar; kullanıcı kararı değil): K28 emniyet zinciri işlerini Fable'a verir. Bu dilim zincirin mantığını yazmaz; emniyet kararını sunucu ve çekirdek verir (A3 §4.11, §4.15), panel yalnız istek gönderir ve sunucunun bildirdiğini gösterir. Fable sayacı %94 olduğu için yazım Sonnet'e verildi; kullanıcı aksini söylerse `ida-kritik-uygulayici` yazar.

## Amaç

Operatör sanal acil durdurmaya tarayıcıdan basabilsin, emniyet girdilerini sözleşmenin iki adımlı kuralıyla değiştirebilsin ve yer istasyonu komutu gönderebilsin. Atış kontrol paneli yalnız göstermeye devam eder.

## Okunacaklar

`docs/sozlesmeler/A3-web.md` §3.6, §3.9, §4.6, §4.11, §4.12, §4.15; `docs/mimari/08-laboratuvar-uygulamasi.md` §3.4; `docs/mimari/03-atis-kontrol.md` §2.7; `web/src/ortak/emniyet.ts`, `web/src/paneller/atis.ts`; W5-web-komut'un komut katmanı.

## Kapsam

1. **Acil durdurmaya bas** (emniyeti artıran yön): ortak durum şeridinde, her panelde, tek adımda.
2. **Emniyet girdileri sayfası** (yeni): acil durdurma, silahlandırma fişi, emniyet pimi. Artıran yön tek adım; azaltan yön iki adım (onay jetonu iste → onay kutusu + jetonla gönder). Yetkili durum ile kartın geri okuması yan yana.
3. **Yer istasyonu komutu** (yeni bölüm): `GET /yer_istasyonu/komutlar` listesinden seçim, şemadan üretilen parametre formu, sözleşmenin istediği komutlarda onay jetonu.
4. Sahte A3 sunucusunda bu uçların sözleşmeye uygun taklidi; testler.

## Mimar kararları

- **W5-K51 — Gösterilen emniyet durumu yalnız sunucudan gelir.** Düğmeye basmak göstergeyi değiştirmez. İstek gitmediyse ya da yanıt gelmediyse ekranda açıkça "UYGULANMADI" ya da "SONUÇ BİLİNMİYOR" yazar; sessiz başarısızlık yoktur.
- **W5-K52 — Azaltan yön asla kendiliğinden yinelenmez.** Jeton süresi dolarsa, revizyon çakışırsa ya da bağlantı koparsa istek düşer ve kullanıcı iki adımı baştan yapar. Artıran yön (acil durdurmaya bas) sözleşme izin veriyorsa revizyon çakışmasında güncel revizyonla en çok üç kez yeniden gönderilir; izin vermiyorsa yinelenmez ve bu raporlanır.
- **W5-K53 — Kısayol ve toplu eylem yok.** Azaltan yön için klavye kısayolu, "hepsini kaldır" düğmesi ya da onay kutusunun önceden işaretli gelmesi yazılmaz. Jeton bellekte tutulur; depolamaya ve adres çubuğuna yazılmaz.
- **W5-K54 — Atış tetikleyen denetim yok** (A3-K99, K101). Yer istasyonu komut formu yalnız sunucunun listelediği komutları gösterir; listeyi istemci genişletmez.

## Kapsam dışı

`sim/`, `core/`, `firmware/` (zincirin mantığı ve bağımsız kabul testleri `tests/kabul/` altında, değişmez); haritada görev düzenleme; hata enjeksiyonu (geri okumayı bozmak ayrı bir eylemdir); kayıt ve izleme.

## Kabul

1. `npm test` geçer. Şu testler düşürülerek gösterilir: azaltan yön jetonsuz gönderilemez; süresi dolmuş jetonla istek yinelenmez; bağlantı kopukken acil durdurma "UYGULANMADI" gösterir; gösterge sunucu verisi gelmeden değişmez.
2. Gerçek sim'e karşı Node testi: panelin komut katmanıyla acil durdurmaya bas → sunucunun emniyet kanalında etkisi görülür; kaldırma jetonsuz reddedilir, jetonla kabul edilir.
3. `host-debug` tam tur yerelde bir kez; Windows ve macOS CI yeşil.
4. Kartta deneme bu dilimin kabulü değildir (kartta silahlandırma ve atış izni denemesi yapılmadı; ayrı iş).

## Rapor

PR ve dal ucu; kabul maddeleri; düşürülerek gösterilen testler; sözleşmede belirsiz bulunan yerler ve seçilen okuma; emniyet girdisinin uygulanmadığı ya da geç uygulandığı görülen her yol.
