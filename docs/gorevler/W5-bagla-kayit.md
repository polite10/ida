# W5-bagla-kayit — kayıt kitaplığının `ida_sim`'e bağlanması (dilim 2)

Tarih: 2026-11-25. Yazan: `ida-hafif-uygulayici` (Sonnet; K28). İnceleyen: `ida-inceleyici` (Opus; K23 tek tam inceleme, tek düzeltme turu; inceleme bu dilimde ZORUNLU: yapılamazsa PR birleşmez). Dal: `is/w5-bagla-kayit` (taban `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `W4-ortak.md`, `W5-ortak.md`.

## Amaç

`ida_kayit` (`sim/kayit`, PR #52) sahte üreticilerle sınandı. Bu iş onu `ida_sim`'e bağlar: bir çalıştırmadan CSV'nin yanında A6 kayıt dosyası (`<run_id>.mcap`) çıkar. Tekrar sürücüleri, A3 kayıt uçları ve kamera kareleri bu dilimde yoktur.

Okunacaklar: `sim/kayit/BENIOKU.md` (özellikle "Bağlama dilimi için kancalar" ve "Bilinen sınırlar"), `sim/kayit/include/ida/kayit/*.hpp`; `docs/gorevler/inceleme/W5-kayit-inceleme.md`; A6 §3.3 (Tablo 3.3), §3.5, §3.6, §4.3, §4.6 (Tablo 4.6), §4.8, A6-K01, K06, K18, K33, K53; `sim/sunucu/src/a2_baglanti.*` ve `a3_baglanti.*` (aynı türden bağlamalar); `sim/sunucu/BENIOKU.md`.

## Mimar kararları (kayıtta `W5-ortak.md`'ye işlenir)

- **W5-K40 — Kayıt bu dilimde isteğe bağlı açılır:** yalnız `--kayit-dizini <dizin>` verildiğinde. Seçenek yokken dosya açılmaz, iş parçacığı başlamaz, çıktılar bu iş öncesiyle bayt bayt aynıdır (A2'deki W5-K31 ile aynı düzen; A6 §3.1'in varsayılan `kayitlar/` dizininden bilinçli sapma, küçük sürüm notu). Kademe senaryonun `kalite.kayit` alanından gelir.
- **W5-K41 — Dosya, üretilmeyen kanallar için güvence iddia edemez.** Bu dilimde bir düzeyin (Tablo 4.6) gerektirdiği kanalların tamamı üretilmiyorsa dosyanın güvencesi `izleme`'dir ve nedeni dosyada yazılıdır. Kitaplıkta bunu söylemenin yolu yoksa `sim/kayit`'e en dar ekleme yapılır (testiyle) ve raporda işaretlenir. Hangi kanalların üretildiği ve hangilerinin eksik kaldığı tablo olarak raporlanır.
- **W5-K42 — Tek yapı, iki hedef.** Dünyanın A3'e zaten yayınladığı her yük aynı yapıdan kayda da verilir (kitaplık A3 tiplerini kullanır); kayıt için ayrı alan doldurma kodu yazılmaz. A3 kapalıyken de kayıt çalışır.

## Kapsam

1. **Ömür:** dünya kurulduktan sonra `Yazici::ac` (dosya varsa çalıştırma başlamaz; A6-K01); açılış üst verisi ve §3.6 ekleri (çözümlenmiş senaryo, katalog, manifest özeti, sürüm ve bağımlılık bilgisi: elde olanlar; olmayanlar raporlanır); her çıkış yolunda (olağan, hata, sinyal: tek kapanış yolu) son sayaçlar + `kapat(alanlar)`. Dünya iş parçacığı üreticidir ve kapanışı kendisi çağırır (kapanış anı yarışı bu üretici için oluşmaz); başka iş parçacığından üretici eklenirse kapanıştan önce durdurulur.
2. **Dünya üreticisi:** `fizik/durum`, `fizik/adim_sonucu`, `olay`, `emniyet/girdiler`, `calistirma/durum`, `sayac/gercek_zaman`, A3 komutları ve sonuçları, senaryo zaman çizelgesi uygulamaları, sensör ve sonar kanallarından dünyada bugün verisi olanlar (W5-K42). A6-K18'in zorunlu kanallarından verisi olanların hiçbiri atlanmaz. `fizik/*` iletilerinde adım etiketi yükteki `adim_no` ile aynıdır.
3. **HIL/SIL bağlantı üreticisi:** `a1/*` ham baytları ve `hil/*` kanalları, köprüden veri kopyalamadan alınabiliyorsa; alınamıyorsa bu dilimde yazılmaz ve W5-K41 uygulanır. `sim/kopru` DEĞİŞMEZ.
4. **Canlı müdahale (A6-K53):** çalıştırma sırasında A3'ten gelen ve sonucu değiştiren komut olduğunda kapanış taslağı HEMEN güncellenir (1 Hz'i beklemez).
5. **Sayaçlar:** kayıt durumu ve düşme sayaçları kapanış özet satırına eklenir; 1 Hz denetimde `kapanis_taslagi()` güncellenir.

## Kapsam dışı

Tekrar sürücüleri; A3 kayıt/izleme uçları ve `web/`; `cekirdek/*` kanalları (A6-K52); kamera; `sim/kopru`, `sim/a2`, `sim/a3`, `core/`, `protocol/`, `firmware/`, `tests/kabul/**`, `docs/sozlesmeler/`. `sim/kayit`'te yalnız W5-K41'in gerektirdiği dar ekleme.

## Kabul (testleri uygulayıcı yazar)

1. **Varsayılan değişmedi:** `--kayit-dizini` yokken yerleşik `z1`, `scenarios/z1.toml` ve `scenarios/gorev-havuz.toml` CSV/stdout/stderr çıktıları bu iş öncesi ikiliyle aynı; hiçbir dosya ya da iş parçacığı oluşmuyor.
2. **Kayıt belirlenimciliği etkilemez:** SIL'de aynı senaryo kayıt açık (üç kademe) ve kapalıyken bayt bayt aynı CSV ve A1 akışını verir; kartsız HIL'de kaçırılan parti ve bütçe aşımı sayaçları kayıt açıkken farklı değil (eşikler ortak zamanlama yardımcısıyla).
3. **İçerik:** kaydedilen dosya `Okuyucu` ile `saglam` açılır; `fizik/durum` iletilerindeki poz aynı adımın CSV satırıyla bit düzeyinde aynı; ileti sayıları adım sayısıyla tutarlı; kademe süzgeci Tablo 3.3'e uyar; üretilen/eksik kanal tablosu testte sabitlenir; güvence W5-K41'e göre.
4. **Kapanış:** olağan bitiş, hata ve sinyalde dosya kapanmış ve kapanış kaydı doğru (`bitis_durumu`, `son_adim_no`); süreç `kill -9` ile öldürülünce kurtarma okuması kapanmış parçaları verir; var olan dosya ezilmez ve çalıştırma başlamaz.
5. **Maliyet:** dünya döngüsüne eklenen süre (ölçüm, bu Mac) ve 60 s'lik `telemetri` kaydının dosya boyutu raporlanır.
6. `host-debug` + `host-release` tam tur (sonda bir kez); `host-asan` yalnız derleme; W3-K14; MSVC kalıpları.

## Test yazım kuralları (bugünkü CI derslerinden; bağlayıcı)

Süreye göre değil olaya göre bekle (sabit `sleep` yok; hazır olma işareti); "N saniyede en az M kez oldu" türünden hıza bağlı eşik yazma, gerekiyorsa ortak zamanlama yardımcısından geçir; geçici dosya ve dizin adları benzersiz; POSIX'e özgü test `if(NOT WIN32)` içinde ve tanımı da kaydı da aynı korumada; sabit port ve gerçek kilit dosyası yok; testi ön planda koştur, süreç adıyla bekleme döngüsü kurma.

## Rapor

PR ve dal ucu; kabul maddeleri ve kanıtı; üretilen/eksik kanal tablosu ve dosyanın güvence düzeyi; kancaların dünya döngüsündeki yeri (dosya:satır); ölçümler; `sim/kayit`'te dokunulan yer; belirsizlikler ve seçilen okuma (K20).
