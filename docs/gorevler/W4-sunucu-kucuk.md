# W4-sunucu-kucuk — `ida_sim` için küçük sunucu işleri

Tarih: 2026-11-24. Yazan: `ida-uygulayici` (Opus). İnceleyen: sol (emniyet dışı dilim; K23: tek tam inceleme, tek düzeltme turu, dar denetim yok; aşağıdaki 1. madde kapanışta kartın güvenli duruma geçişine dokunduğu için incelemede o maddede K21 sınıfı bir bulgu çıkarsa dar denetim açılır). Dal: `is/w4-sunucu-kucuk` (taban: W4-duraklat `main`'e girdiyse `origin/main`, girmediyse `origin/is/w4-duraklat`; ikisi de `sim/sunucu`'ya dokunur, aynı anda aynı tabandan koşmaz). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `docs/gorevler/W4-ortak.md`.

## Amaç

W4'ün kart koşularında ve incelemelerinde birikmiş küçük eksikleri tek dilimde kapatmak (`docs/gorevler/W4-ortak.md` açık bulgular; `docs/DURUM.md`).

## Kapsam

1. **Ctrl-C'de düzgün kapanış.** `ida_sim` bugün sinyal işleyicisi taşımıyor; kesilince oturum kapanış çerçevesi gitmiyor ve kart kendi zaman aşımıyla güvenli duruma geçiyor. İstenen: `SIGINT`/`SIGTERM` (Windows'ta konsol denetim olayı: Ctrl-C, Ctrl-Break, pencere kapatma) yalnız bir bayrak kurar (işleyicide başka iş yok); dünya döngüsü bayrağı bir sonraki sınırda görür ve var olan düzgün kapanış yolunu (kapanış çerçevesi, A3 kapanışı, kayıtların boşaltılması) işletir. İkinci sinyal beklemeyi keser ve süreci hemen sonlandırır. Kapanış yolu, var olan hata kapanışıyla aynı işlevdir (ikinci bir kapanış yolu yazılmaz). Kartın güvenliği bu yola DAYANMAZ: kart zaman aşımıyla güvenli duruma geçmeyi sürdürür; bu madde yalnız kapanışı hızlandırır ve temizler.
2. **Kapanış sayaçları.** Her kapanışta (düzgün, hata, sinyal) özet satırı yazdırılır: kaçırılan parti, düşen kayıt, yeniden gönderim, sonar taşma sayaçları, oturum sayısı, son hata. Var olan sayaçlar kullanılır; yeni ölçüm eklenmez.
3. **Hazırlık süre sınırı.** `hazirlik` beklemesi `kurma_son`'u aşabiliyor (W4-ortak açık bulgular (a)). Bekleme iki sınırın küçüğüyle sınırlanır; aşımda var olan açık hata.
4. **`scenarios/z1.toml` ve kip.** Dosya `kip = "sil"` taşıdığı için `--kip hil` ile reddediliyor. A5'in `calistirma.kip` kuralları okunur (A5-K76 ve çevresi). Karar: komut satırındaki `--kip`, senaryodaki `calistirma.kip`'i ancak A5 buna izin veriyorsa geçersiz kılar; izin vermiyorsa senaryo dosyası değişmez ve yanına HIL kipli bir eş (`scenarios/z1-hil.toml`; yalnız kip ve kipe bağlı alanlar farklı) eklenir. Hangi yolun seçildiği sözleşme maddesiyle raporlanır; sözleşme değişmez.
5. **Sonarın açılması.** Bugün `--sonar` bayrağıyla açılıyor. Karar (mimar): varsayılan değişmez (bayrak yoksa sonar kapalı; `z1` zincirinin bayt eşitliği korunur). Bayrağa iki açık değer eklenir: `--sonar=acik|kapali`; değersiz `--sonar` = `acik`. Senaryoda etkin sonar varken bayrak verilmemişse başlangıçta tek satır bilgi yazılır ("senaryoda N etkin sonar var; `--sonar` ile açılır"). Varsayılanın senaryodan türetilmesi W4-sonar-sahne `main`'e girdikten sonra yeniden ele alınır.
6. **Açık su sonar örneği.** `scenarios/sonar-acik-su.toml`: sonar açıkken bugün çalışan (tek tabanlı, hedefsiz) örnek senaryo; W4-bagla-sonar testlerinin bellekte kurduğu sahnenin dosya karşılığı. Var olan senaryo dosyaları değişmez.
7. **Zamanlama yardımcısının tek kopyası (W4-K29).** `sim/a3/test/test_destek.hpp`'deki kopya kaldırılır; A3 testleri ortak yardımcıyı (`sim/kopru/test/zamanlama_olcutu.hpp`; gerekiyorsa ortak bir test dizinine taşınır) kullanır.

## Kapsam dışı

Duraklatma (W4-duraklat); sonar sahne geometrisi (W4-sonar-sahne); A3'ten senaryo yükleme; kayıt/tekrar; `core/`, `protocol/`, `firmware/`, `web/`, `tests/kabul/**`.

## Kabul

1. Sinyal: SIL ve kartsız HIL'de koşu sırasında `SIGINT` → süreç sınırlı sürede (ölçülür, raporlanır) 0 olmayan belgelenmiş çıkış koduyla biter; telde kapanış çerçevesi görülür (sahte kartla); A3 istemcisi kapanış bildirimini alır; kayıt dosyaları eksiksiz kapanır. İkinci sinyal hemen sonlandırır. Windows yolu derlenir ve CI'da en az kapanış işlevinin birim testi koşar (konsol olayı CI'da üretilemiyorsa neden raporlanır).
2. Özet satırı üç kapanış türünde de var; alanları sayaçlarla tutarlı.
3. Hazırlık: `kurma_son`'dan uzun hazırlık beklemesi isteyen sahte kartla süre sınırı testi.
4. `z1` HIL kipinde (seçilen yolla) kartsız HIL'de başlar; `zincir.z1` ve `kabul.*` değişmeden; sonar kapalıyken çıktılar bu iş öncesiyle bayt bayt aynı (iki ikilinin bir kez karşılaştırılması).
5. `scenarios/sonar-acik-su.toml --sonar` SIL'de koşar; iki koşu bayt bayt aynı.
6. `host-debug` + `host-release` tam tur (sonda bir kez); `host-asan` yalnız derleme; W3-K14; MSVC kalıpları; zamanlama eşikleri yalnız ortak yardımcıyla (W4-K24, K29).
