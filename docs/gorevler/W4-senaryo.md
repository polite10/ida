# W4-senaryo — A5 çözümleyici ve doğrulayıcı (senaryo ve araç dosyaları)

Tarih: 2026-11-21. Yazan: `ida-uygulayici` (Opus). İnceleyen: sol (tek tur). Dal: `is/w4-senaryo`. Ortak kurallar ve mimar kararları: `docs/gorevler/W3-ortak.md` (W3-K14 taraması W4 işlerinde de zorunlu).

## Amaç

`sim/senaryo/` altında, A5 sözleşmesindeki senaryo ve araç dosyalarını okuyan, doğrulayan ve tek bir **çözümlenmiş senaryo** üreten kitaplığı (`ida_senaryo`) ve küçük bir komut satırı aracını (`ida_senaryo_dogrula`) yazmak. Bugün `ida_sim`, fizik ve sensör kümesi koddaki varsayılanlarla kuruluyor; bu iş onların dosyadan kurulmasının ön koşuludur.

Okunacaklar: `docs/sozlesmeler/A5-senaryo.md`'nin tamamı (özellikle §3, §4.1, §4.2, §4.12, §5, §7, Ek B, Ek D.7, Ek E); `scenarios/` altındaki dört dosya (`sema/a5.schema.json`, `ida0.toml`, `ornek-havuz.toml`, `ornek-havuz.cozumlenmis.json`) ve `scenarios/BENIOKU.md`; A7 sözleşmesinin manifest bölümü ve `assets/manifest.schema.json`, `assets/ornek/manifest.json` (A5-K18, K19, K115 için); iskelet §3 (A5 satırı), §6. Sözleşme ile makinece okunur dosyalar çelişirse sözleşme metni geçerlidir; çelişkiyi raporla (küçük sürüm notu olarak toplanacak), dosyayı kendin değiştirme.

## Kapsam

### Dilim 1 — dosyadan çözümlenmiş senaryoya

1. **Okuma (aşama 1):** TOML 1.0.0; UTF-8 denetimi; yinelenen anahtar reddi; satır numarası; sayıların A5-K96'ya göre binary64'e çevrilmesi (taşma, alt taşma, `inf`/`nan` reddi; −0 → +0; tablodaki bütün sınır vektörleri) ve A5-K09 tip ayrımı (tamsayı alanında `3.0` reddi); tarih/saat türlerinin reddi (A5-K97). TOML kitaplığı alt taşmayı sessizce sıfıra çeviriyorsa değer kaynak metinden yeniden çevrilir; vektörler geçmeden bu madde bitmiş sayılmaz.
2. **Şema ve kural doğrulaması (aşama 1–2):** tanımsız anahtar reddi ve `x_` istisnası (A5-K06, K07), zorunlu ve koşullu anahtarlar, aralık/küme/desen/eleman sayısı, sürüm kuralları (K03, K04), "desteklenmiyor" işaretliler (K102), dosya başvurusu (K12), §4.3–§4.13'ün **yükleme anında** denetlenebilen bütün kuralları (çelişen zamanlı atamalar, profil zorunlulukları, HIL'de SIL alanlarının reddi, gövde bütçesi, kamera kuralları, görev öğesi alanları vb.). Şema tabloları veri güdümlü tutulur (kural başına elle yazılmış `if` zinciri değil); `scenarios/sema/a5.schema.json` doğrudan yorumlanabilir ya da ondan derleme zamanında tablo üretilebilir; hangisini seçtiğini raporla.
3. **Çözümleme (A5-K13–K16, K98–K100):** birleştirme, kaynak alanlarının çıkarılması, eksik tablolar, koşulsuz ve koşullu varsayılanlar, ürün profilleri, kalibre edilmeyen değerler, `otopilot` bloğu; sıra sözleşmedeki sıradır. Ek E, sözleşmedeki bloktan `sim/senaryo/veri/` altına çıkarılır ve bir test dosyanın sözleşmedeki blokla bayt bayt aynı olduğunu denetler (sürüklenme algılansın).
4. **Kanonik biçim ve özet (A5-K17):** RFC 8785 dizilimi (anahtar sırası UTF-16 kod birimlerine göre; sayılar ECMAScript Number dizilimi; A5-K96 tablosunun "JCS dizilimi" sütunu) ve SHA-256 davranış özeti; kaynak dosya özetleri ayrı.
5. **A7 atıfları (A5-K18, K19, K36, K115):** manifest `assets/<kume>/manifest.json`'dan okunur; var olan / olmayan / yanlış türde kimlik, malzeme alan adları ve aralıkları, `kume_surumu` ve `manifest_ozeti`.
6. **Çözümlenmiş senaryonun yeniden yüklenmesi (A5-K02):** kanonik JSON'dan; özet eşit değilse ret.
7. **Doğrulama raporu (A5-K90–K95, §3.14):** aşama sırası, bulguların toplanması (ilk hatada durmaz; aşama kuralları sözleşmedeki gibi), sıralama, `yol` sözdizimi.
8. **`ida_senaryo_dogrula <senaryo.toml> [--cozumlenmis <çıktı.json>] [--rapor <çıktı.json>]`:** çıkış kodları belgeli (geçerli / geçersiz / kullanım / G/Ç).

### Dilim 2 — çözümlenmiş senaryodan kurulum yapılarına (aynı dalda, ayrı commit; dilim 1 testleri geçince ara rapor beklemeden devam et)

9. **Fizik yükleyici:** çözümlenmiş senaryodan `ida::fizik` kurulum yapıları (kütle, hidrodinamik, iticiler, itici modeli, ortam, başlangıç); A5-K20–K34, K89, K104, K105 denetimleri ve Ek A test vektörleri (1e-4 bağıl).
10. **Sensör yükleyici:** çözümlenmiş senaryodan `ida::sensor` kurulum yapıları (alan adları zaten A5 ile bire bir). `sim/sensor` `main`'e girmediyse `origin/is/w4-sensor` dalından yalnız okunur ve bu madde o dal `main`'e girince (ya da kendi dalına birleştirilerek) derlenir; hangi yolu izlediğini raporla.
11. **Karşılaştırma:** `scenarios/ida0.toml` + `ornek-havuz.toml`'dan kurulan fizik ve sensör yapıları ile koddaki İDA-0 varsayılanları (`sim/fizik`, `sim/sensor`) alan alan karşılaştırılır; farklar **raporlanır**, hiçbiri sessizce eşitlenmez (hangisinin doğru olduğuna mimar karar verir).

## Kapsam dışı

Senaryonun **yürütülmesi** (zaman çizelgesinin koşturulması, olayların uygulanması, bağlantı emülatörü, görev öğelerinin karta gönderilmesi): `ida_sim` işidir. Kalite kademelerinin uygulanması. A6'ya yazma. `scenarios/`, `assets/` ve sözleşme dosyalarının değiştirilmesi. Torpido alanları yalnız şema ve aralık olarak doğrulanır; torpido modeli kurulmaz.

## Kitaplıklar

TOML için toml++ ve JSON okuma için nlohmann-json önerilir (ikisi de C++, MIT, vcpkg'de var; sürüm `vcpkg.json`'da `overrides` ile sabitlenir; sürümü ve lisansı birincil kaynaktan doğrula ve raporla). Başka seçim yaparsan gerekçesini yaz; Rust tabanlı araç ya da kitaplık kullanılmaz. Kanonik JSON yazımı ve SHA-256 bu kitaplıkta yazılır (`sim/sensor` ve `sim/sil` de birer SHA-256 taşıyor; ortak yere toplama ayrı iştir, şimdi başka dizine dokunma). Sayı dizilimi için `std::to_chars` (en kısa gidiş-dönüş) kullanılacaksa Apple Clang ve MSVC'de davranışı A5-K96 vektörleriyle sınanır.

## Dosya sahipliği

Yazılır: `sim/senaryo/**`, `sim/CMakeLists.txt`'e tek satır, `vcpkg.json` (yalnız bağımlılık ekleme). `sim/fizik`, `sim/sensor`, `core/`, `protocol/`, `scenarios/`, `assets/`, `docs/sozlesmeler/` değiştirilmez; gereken en küçük değişikliği öner ve raporla.

## Kabul (testleri uygulayıcı yazar; CTest)

A5 §7 uygunluk tablosunun **birim**, **altın dosya**, **belge tutarlılığı** ve **sözleşme (A7 ile)** satırları; "SIL" yazan kısımlar bu işin kabulü değildir. En az:

1. **Altın dosya:** `ornek-havuz.toml`'un çözümlenmiş kanonik baytları `scenarios/ornek-havuz.cozumlenmis.json` ile bayt bayt eşit; davranış özeti dosyadaki/sözleşmedeki değerle eşit. Eşit çıkmıyorsa farkı kural numarasıyla raporla; altın dosyayı değiştirme.
2. **Bozuk dosya takımı:** tablo güdümlü; A5-K01–K11, K96, K97, K102 ve §5'teki her satır için en az bir dosya; beklenen `kural`, `asama`, `yol`, `satir`.
3. **A5-K17:** anahtar sırası, yorum, satır içi tablo, sayı yazımı (`3` / `3.0`, `0.30` / `3e-1`, `-0.0`) ve açıkça yazılmış varsayılanlar değişince özet aynı, kaynak özeti farklı; olay sırası ya da bir yaprak değişince özet farklı.
4. **Koşullu varsayılanlar:** A5-K98 tablosunun her satırı için koşulun iki tarafı.
5. **A7 atıfları:** §7'deki K18, K19, K36, K115 satırı.
6. **Yeniden yükleme:** çözümlenmiş JSON → aynı özet; bir bayt değişince ret.
7. **Dayanıklılık:** rastgele bozulmuş girdi (en az 10 000 dosya; sabit tohum) çökme, sonsuz döngü ya da sınırsız bellek üretmez; iç içe tablo derinliği ve dosya boyu için açık üst sınır.
8. **Dilim 2:** Ek A vektörleri; §7'deki K20–K34, K89, K104, K105 satırı; karşılaştırma tablosu (madde 11) raporda.
9. `host-debug`; `host-asan` yalnız derleme; W3-K14 taraması; `ornek-havuz` için çözümleme süresi (ölçüm, bu Mac). Yeni vcpkg bağımlılığının Windows CI süresine etkisi PR'da görülecek; yerelde tahmin yürütme.
