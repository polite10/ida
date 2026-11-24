# W4-bagla-a3 — A3 sunucusunun `ida_sim`'e bağlanması

Tarih: 2026-11-24. Yazan: `ida-uygulayici` (Opus; `sim/sunucu`'nun sahibi). İnceleyen: sol (tek tur; emniyet girdilerinin köprüye ulaşmasına dokunan bulgularda ikinci tur). Dal: `is/w4-bagla-a3` (taban: `is/w4-mac-ci`; o dal `main`'e girdiyse `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `docs/gorevler/W4-ortak.md` (özellikle W4-K3, K4, K5, K12, K15, K19, K22, K23, K24, K25).

## Amaç

A3 sunucusu (`sim/a3`, `ida_a3`) bugün yalnız sahte kaynakla (`ida_a3_sahte`) çalışıyor; `ida_sim` ise web panellerine hiçbir şey yayınlamıyor. Bu iş ikisini bağlar: `ida_sim --a3-port <port> --a3-kok <dizin>` verilince sim, dünya durumunu A3 kanallarından yayınlar ve panelden gelen komutları (çalıştırma denetimi, sanal emniyet girdileri, yer istasyonu komutları) kendi karar yollarına iletir. Sonuç: paneller gerçek simülasyonu (kartsız ya da kartlı) gösterir ve yönetir.

Okunacaklar: `docs/sozlesmeler/A3-web.md` §3.4 (yayınlanan kanalların yük düzenleri), §4.4–§4.8 (abonelik, seyreltme, komut yolu, çalıştırma durumu), §4.11, §4.12, §4.15 (sanal emniyet girdileri, yer istasyonu komutları), A3-K33 (yayın tiki), A3-K95 (karar sahibi sim); A1-K85 (emniyet işlemlerinin sırası); iskelet §3.2 (fizik sınırındaki sıra) ve `docs/mimari/04-simulator-cekirdegi.md` §2.12 (iş parçacıkları); `sim/a3/include/ida/a3/**` (`Yayinci`, komut alıcısı arayüzü, `sunucu.hpp`) ve `sim/a3/BENIOKU.md`; `sim/a3/test/` altındaki sahte kaynak (davranış örneği); `sim/sunucu/**`, `sim/kopru/include/**`.

## Kapsam

1. **Yaşam döngüsü:** `--a3-port` verilmezse sunucu hiç başlamaz ve `ida_sim`'in bugünkü davranışı bayt bayt aynı kalır (Z1 CSV eşitliği testleri değişmeden geçer). Verilirse A3 sunucusu dünya iş parçacığından AYRI kendi iş parçacıklarında çalışır (kitaplıkta böyle); açılış adresi ve erişim jetonu standart çıktıya bir kez yazılır (A3-K01–K12 kitaplıkta uygulanmış; bu iş yalnız bağlar). Kapanış: koşu bitince sunucu düzgün durur; asılma olmaz.
2. **Yayın (dünya → A3):** dünya iş parçacığı her yayın noktasında `Yayinci` üzerinden engellemeyen, bellek ayırmayan çağrılarla yayınlar (kitaplığın güvencesi; bu iş çağrı yerlerini seçer). Yayınlanan kanallar: kitaplığın bu dilimde desteklediği, beş panelin kullandığı kanallar ve `olay` (liste `sim/a3/BENIOKU.md`'de). Her kanalın alanı sim'deki gerçek kaynağından doldurulur (fizik durumu, sensör çıktıları, köprü durumu ve sayaçları, kartın bildirdiği durum/seyrüsefer, emniyet durumu ve revizyonu, çalıştırma durumu); sim'de karşılığı henüz olmayan alan sözleşmedeki "yok/geçersiz" gösterimiyle gider ve raporda listelenir (uydurma değer yok). Zaman damgaları sim zamanıdır. **Yayın, fizik ve köprü sırasını değiştirmez:** yayın çağrıları fizik sınırındaki sıranın sonunda yapılır ve SIL'de belirlenimciliği bozmaz (A3 açıkken ve kapalıyken aynı tohumla CSV ve A1 bayt akışı aynı; test).
3. **Komut alıcısı (A3 → dünya):** A3 iş parçacığından gelen istekler dünya iş parçacığına kilitsiz/sınırlı bir kuyrukla geçer ve yalnız fizik sınırında uygulanır; sonuçlar `komut_sonucu` ile döner.
   - **Sanal emniyet girdileri (EMNİYET):** acil durdurma bas/bırak, fiş, pim. Karar sim'indir (A1-K85, A3-K95): istek, senaryo zaman çizelgesindeki emniyet atamalarının kullandığı AYNI köprü yolundan girer (ikinci bir uygulama yolu yazılmaz). Kısıtlayan istek hiçbir kuyruk doluluğunda düşürülmez ve gevşeten isteğin arkasında beklemez (kitaplıkta mandal var: W4-K22; dünya tarafındaki aktarım da aynı güvenceyi verir: kısıtlayanlar için ayrı, taşmayan sabit boylu yol). Kısıtlayan istek `beklenen_run_id` bayat olsa da uygulanır (W4-K19). Gevşeten istek revizyon kuralıyla (W4-K4, K12) değerlendirilir; ret `gevsetme_reddedildi` olarak döner. W4-K3 ve W4-K5 aynen geçerlidir.
   - **Yer istasyonu komutları:** `ARM`, `DISARM`, `SET_MODE` ve sözleşmenin bu dilimde tanımladığı ötekiler; senaryo zaman çizelgesindeki operatör komutlarının kullandığı aynı köprü yolundan karta gider; kartın yanıtı sonuç olarak döner. Atış isteği (`FC_FIRE`) yalnız katalogdaki biçimiyle taşınan komuttur: köprüde bugün karşılığı varsa aynen iletilir, yoksa sözleşmedeki "desteklenmiyor" yanıtı döner; atış kontrol mantığına ilişkin hiçbir ekleme yapılmaz.
   - **Çalıştırma denetimi (§4.7–§4.8):** durum makinesinin sahibi sim'dir. Bu dilimde: durumun yayınlanması ve sözleşmenin tanımladığı duraklat/sürdür/durdur isteklerinden sim'de karşılığı olanlar; karşılığı olmayan istek sözleşmedeki hata yanıtını alır (liste raporda). SIL'de duraklatma sim zamanını durdurur; HIL'de duraklatmanın anlamı sözleşmeye göre uygulanır (belirsizse kısıtlayan okuma ve rapor).
4. **Senaryoyla birlikte çalışma:** aynı sınırda hem senaryo zaman çizelgesinden hem A3'ten emniyet ataması gelirse sıra A1-K85'e göre (kısıtlayan önce); A3'ten gelen komutlar CSV/olay kaydına senaryodan gelenlerle aynı biçimde, kaynağı belirtilerek yazılır.
5. **`ida_a3_sahte`** kalır (panel geliştirme için); değişmez.

## Kapsam dışı

Senaryo yükleme ve ön denetim uçları (§4.9), hata enjeksiyonu (§4.10), kayıt yönetimi (§4.13), yardımcı süreçler (§4.14), tekrar kipleri; sonarın sim'e bağlanması ve sonar kanalları (ayrı iş: W4-bagla-sonar); `web/` altındaki kod (bir uyumsuzluk paneldeyse raporlanır); `sim/a3` kitaplığının iç davranışı (eksik görürsen en küçük arayüz eklemesini yap ve raporla; erişim/emniyet kurallarına dokunma: W4-K23); `core/`, `protocol/`, `firmware/`.

## Dosya sahipliği

Yazılır: `sim/sunucu/**`, `sim/kopru/**` (yalnız gereken arayüz eklemeleri), `sim/a3/include/**` ve `sim/a3/src/**` (yalnız bağlama için zorunlu en küçük eklemeler), `tests/butunleme/**`. `tests/kabul/**` değiştirilmez.

## Kabul

1. **A3 kapalıyken eşitlik:** `--a3-port` verilmeden bütün var olan testler değişmeden geçer (Z1 bayt eşitliği dahil).
2. **A3 açıkken belirlenimcilik:** SIL'de aynı tohumla A3 açık ve bağlı bir istemci (hızlı ve hiç okumayan yavaş istemci dahil) varken CSV ve A1 bayt akışı, A3 kapalı koşuyla aynı (komut gönderilmediği sürece).
3. **Uçtan uca:** `ida_sim --kip sil --senaryo scenarios/z1.toml --a3-port ...` + Node'daki panel A3 istemcisi (`sim/a3/test`'teki uyum düzeneği gibi; `web/`'e dosya eklemeden): `surum` → WebSocket → `merhaba` → abonelik → beş panelin kanallarında gerçek değerler (konum/derinlik senaryoyla tutarlı; değerlerin sim'in CSV'siyle aynı adımlarda eşleştiği) → `ARM` gönder → sonuç ve kartın durumu.
4. **Emniyet (EMNİYET):** A3'ten acil durdurma bas: sonraki fizik sınırından itibaren itki nötr (W4-K3: bırakınca eski komut dönmez); yavaş istemci ve dolu kuyrukla aynı sonuç; revizyonsuz/bayat revizyonlu gevşetme reddi; kısıtlayan isteğin bayat `beklenen_run_id` ile uygulanması; senaryo ataması ile A3 atamasının aynı sınırda karşılaşması. Bağımsız kabul testleri (`kabul.kopru.*`) değişmeden geçer.
5. **Zamanlama (ölçüm; W4-K24'e göre işaretlenir):** A3 açık ve 50 Hz yayın sürerken 60 s kartsız HIL koşusunda kaçırma sayısı ve yayın çağrısının dünya iş parçacığındaki en kötü süresi (bu Mac; ölçüm olarak raporlanır, tahmin gerçek gibi yazılmaz).
6. `host-debug` + `host-release`; `host-asan` yalnız derleme; W3-K14 taraması; MSVC kalıpları (C4267, C4389, C6385/C6386, C6011, C6001).
