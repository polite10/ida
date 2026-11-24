# W5-kontrol-acil — acil durdurmada çekirdeğin aktüatör çıktısı (EMNİYET)

Tarih: 2026-11-24. Yazan: `ida-kritik-uygulayici` (Fable; kontrol). Bağımsız kabul testleri: `ida-test-yazari` (uygulama kaynağını okumadan; aynı anda başlar). İnceleyen: K21 (bir tam inceleme + bir dar denetim; sol kullanım sınırı doluyken `ida-inceleyici`, sol inceleme borcuna yazılır). Dal: `is/w5-kontrol-acil` (taban `origin/is/w5-kestirim-yatay`; o dal `main`'e girdiyse `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md` (W3-K1), `docs/gorevler/W5-ortak.md` (W5-K4, K20, K26).

## Neden

Kart doğrulamasında ölçüldü (W5-K26): kart acil durdurmayı görüyor (`fc.inhibit_mask`'te ESTOP biti kurulu) ama kendi aktüatör isteğini nötrlemiyor ve kontrolcüyü dondurmuyor. Acil durdurma boyunca istek büyüyor (6 s'de 1 526 → 1 638 µs); kalkıştan 15 ms sonra fiziğe giden ilk komut bu birikmiş değer oluyor. Acil durdurma sırasında sim tarafı (A1-K85) nötrlediği için araca nötr dışı darbe gitmiyor; sorun (1) kalkış anındaki ani itki, (2) kartın tek başına güvenli olmaması (gerçek donanımda sim'in nötrlemesi yoktur).

## Kural (mimar kararı)

Etkin acil durdurma değeri kısıtlayan olduğu sürece (basılı ya da bayat: W5-K4; init'ten sonra hiç gevşememiş durum dahil) çekirdek:

1. **Aktüatör çıktısını nötr verir:** `ida_core_step` çıktısındaki sürekli komut her itici ve her kanal için nötrdür (A4'ün "güvenli çıktı" tanımıyla aynı değerler); kip, silah durumu ve görev ne olursa olsun; acil durdurmanın kısıtlayana döndüğü ÇAĞRININ çıktısı dahil (bir çağrı gecikme yok).
2. **Kontrolcünün birikimli durumlarını sıfırlar** (tümlevler, hız/ivme sınırlı referans modellerinin iç durumu, güdümün ILOS tümlevi, tahsisin doyma belleği) ve acil durdurma sürerken biriktirmez.
3. **Kalkıştan sonra çıktıyı sınırlı hızla açar:** acil durdurma gevşediği çağrıdan başlayarak kontrol çıktısının büyüklüğü parametreli bir rampayla (varsayılan süre tahmin; `IDA_PCLS_B`) nötrden olağan değere yükselir; referanslar o anki ölçülen durumdan başlatılır (sıçramasız geçiş).
4. Bu davranış sim'in nötrlemesinden bağımsızdır ve onun yerine geçmez; ikisi birlikte çalışır.

`health`/`status`'ta bu durumu gösteren var olan bir alan varsa kullanılır; yeni alan eklenmez (`port.h` DEĞİŞMEZ). Silah durumu ve kip bu kuralla değişmez (acil durdurmanın silahsızlandırıp silahsızlandırmayacağı var olan W3/A4 kurallarındadır; onlara dokunulmaz).

Okunacaklar (dar tut): A4 (`docs/sozlesmeler/A4-port.md`): aktüatör çıktısı ve güvenli çıktı tanımı (§3'te `act`/sürekli komut alanları, A4-K28, K55), A4-K116 (etkin emniyet değerleri); `core/src/kontrol/**`, `core/src/gudum/**`'de tümlev durumları, `core/src/emniyet/**`'in etkin acil durdurma erişimi, `core/src/cerceve/cerceve.c`'de kontrol kancasının çağrıldığı yer; `core/TASARIM.md` kontrol bölümü; `docs/gorevler/W5-ortak.md` W5-K4 (bayatlık), W5-K26.

## Dosya sahipliği

Yazılır: `core/src/kontrol/**`, `core/test/kontrol/**`, `core/TASARIM.md` kontrol bölümü, `core/test/test_param_hiz.c` (altın özet; rampa parametresi); gerekiyorsa `core/src/gudum/**`'de yalnız tümlev sıfırlama çağrısı ve `core/src/cerceve/**`'de yalnız kontrol kancasına etkin acil durdurma bilgisinin verilmesi (çerçevenin öteki davranışı değişmez; değişmesi gerekiyorsa DUR ve raporla). `core/src/atis/**`, `core/src/kestirim/**`, `core/src/gorev/**`, `port.h`, `tests/kabul/**` değişmez.

## Kabul

1. **Nötr çıktı:** her kipte (silahlı, `GUIDED`'da nötr dışı komut yürürlükteyken) acil durdurma örneği geldiği çağrının çıktısında ve sürdüğü her çağrıda bütün kanallar nötr; bayatlıkla (W5-K4'ün iki ölçütü) kısıtlayana dönüşte de aynı; reddedilen çağrıda gelen kısıtlayan örnekte de (A4-K116).
2. **Birikim yok:** uzun acil durdurma (≥ 60 s) boyunca büyük hata altında kontrolcü durumları sıfır kalır; kalkıştan sonraki ilk çıktı, hiç acil durdurma olmamış ve aynı anda sıfırdan başlatılmış bir kontrolcününkiyle aynıdır.
3. **Rampa:** kalkıştan sonra çıktı büyüklüğü rampa zarfını hiçbir çağrıda aşmaz; rampa bitince olağan çıktıya eşittir; rampa sırasında yeniden acil durdurma gelirse aynı çağrıda nötr.
4. **Özellik testi (rastgele komut/örnek dizileri, sabit tohum, ≥ 200 000 adım):** etkin acil durdurma kısıtlayanken nötr dışı çıktı veren çağrı sayısı 0.
5. **Gerileme yok:** acil durdurma hiç kullanılmayan koşularda çıktı bu iş öncesiyle bayt bayt aynı (Z1 zinciri: `ida_zincir_deneme` CSV'si); var olan `core/test`, `tests/kabul/**` (181 kabul testi), `zincir.z1` değişmeden geçer. Z1 senaryosunda başlangıçta acil durdurma kısıtlayansa ve bu çıktıyı değiştiriyorsa hangi adımların neden değiştiği yazılır.
6. `fw-release` kod boyu farkı; `host-release`'te step süresi bağıl farkı; `host-debug` + `host-release` tam tur (sonda bir kez); `host-asan` yalnız derleme; W3-K12; W3-K14; MSVC kalıpları.
7. **Kart doğrulaması (ayrı; mimar başlatır):** W5-K26'daki acil durdurma koşusu yinelenir; kartın ürettiği darbe genişliklerinde acil durdurma boyunca nötr dışı değer 0 ve kalkıştaki ilk komutun rampa içinde olduğu ölçülür.

## Bağımsız kabul testleri (ayrı agent; `tests/kabul/kontrol/**`)

Test yazarı yalnız şunları okur: bu belgenin "Kural" ve "Kabul" bölümleri, A4 portu (`port.h`, A4'ün aktüatör çıktısı, güvenli çıktı ve emniyet örneği maddeleri), `docs/gorevler/W5-ortak.md` W5-K4 ve W5-K20, var olan `tests/kabul/cekirdek/**` ve `tests/kabul/atis/**` düzeneği. `core/src/**` okunmaz. Testler: kabul 1–4'ün kara kutu karşılıkları (ad: `kabul.kontrol.acil.<kısa ad>`); rampa zarfı için parametre adı uygulamayla ortak olmak zorunda olduğundan ad bu belgede sabitlenir: `KON_ACIL_RAMPA_MS` (varsayılan 500 ms, tahmin; aralık 0–5 000; 0 = rampa yok). Yer tutucu (bugünkü) davranışa karşı düşen testler "bekleniyor" diye işaretlenir; uygulama dalıyla birleşince varsayılan turda koşar.
