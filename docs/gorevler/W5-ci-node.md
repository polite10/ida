# W5-ci-node — CI'da Node testlerinin gerçekten koşması

Tarih: 2026-11-25. Yazan: `ida-hafif-uygulayici` (Sonnet). İnceleyen: sol (emniyet dışı dilim; K23; sol sınırı doluyken K25/K27). Dal: `is/w5-ci-node` (taban `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `W4-ortak.md`.

## Amaç

Dört Node testi her tam turda atlanıyor (`butunleme.a3_uctan_uca_web`, `butunleme.a3_sonar_web`, `sim.a3.uyum_web_cozucu`, `sim.a3.uctan_uca_web`): CI koşucularında ve yerelde `web/node_modules` yok, testler 77 dönüp atlanıyor. `web/`'in kendi testleri (`npm test`) de CI'da hiç koşmuyor. Sonuç: C++ A3 sunucusu ile panelin A3 istemcisi arasındaki uyum, yalnız bir geliştiricinin makinesinde elle sınanıyor.

## Kapsam

1. **Windows ve macOS iş akışları** (`.github/workflows/derleme.yml`, `derleme-mac.yml`): Node sürümü sabitlenir (`actions/setup-node`, `web/package-lock.json` üzerinden npm önbelleği), `web/` altında `npm ci` derlemeden/testten önce koşar. Bağımlılık kurulumu başarısızsa iş düşer (sessizce atlanmaz).
2. **Atlamanın CI'da hata sayılması:** CI'da bu dört testten biri atlanırsa (77) iş düşer. Yerelde (Node ya da `node_modules` yokken) atlama davranışı değişmez. Yol: var olan `IDA_BAGIMLILIK_ZORUNLU` seçeneği bu testleri de kapsıyorsa onu kullan; kapsamıyorsa en dar eklemeyi yap.
3. **`web/`'in kendi testleri:** `npm test` (tür denetimi + `arac/test.mjs`) CI'da tek bir koşucuda koşar; en ucuz koşucu seçilir (Linux'taki belge işi gibi) ve gerekçesi yazılır. Kota dar (GitHub Free): her gönderimde eklenen süre raporlanır.
4. Dört test CI'da ilk kez koşacak: düşerse neden ayrılır. Kusur testteyse (zamanlama, sabit port, yol ayırıcı, Windows'ta süreç başlatma) düzeltilir; kusur üründeyse (`sim/a3`, `sim/sunucu`, `web/src`) **düzeltilmez**, kanıtıyla raporlanır ve test o platformda açık bir gerekçe satırıyla işaretlenir (mimar karar verir).

## Kapsam dışı

`web/src` ve paneller; `sim/sunucu/src`, `sim/a3/src` (ürün kodu); yeni npm bağımlılığı; Godot'lu testler; `tests/butunleme/CMakeLists.txt`'te bu dört testin kaydı dışındaki satırlar (aynı dosyaya W5-bagla-a2 dokunuyor: değişikliği en küçük tut).

## Kabul

1. Bir PR koşusunda Windows ve macOS işlerinde dört test "Passed" (atlanmamış) görünür; kanıt: koşu bağlantısı ve ctest satırları.
2. `npm test` CI'da koşar ve geçer.
3. `node_modules` kasıtlı olarak yokken CI ön ayarıyla yerel bir koşuda dört testin atlanması iş hatası üretir (yerelde gösterilir); CI ön ayarı olmadan atlama sürer.
4. İş akışına eklenen süre (soğuk ve sıcak önbellek; ölçüm, CI koşusu) raporlanır.
5. `host-debug` tam tur yerelde bir kez (bu Mac'te `npm ci` yapılmış hâlde dört test koşar).

## Rapor

PR ve dal ucu; kabul maddeleri; eklenen CI süresi; dört testten düşen olduysa neden ve yapılan; ürün kusuru bulunduysa kanıtı.
