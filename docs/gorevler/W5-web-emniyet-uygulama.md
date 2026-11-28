# W5-web-emniyet — uygulayıcı raporu

Tarih: 2026-11-28. Dal: `is/w5-web-emniyet`. Dal ucu: `a21c2be` (bu çalışmanın değişiklikleri commit'lenemedi). Bağımsız inceleme raporu değildir.

## Tamamlanan değişiklikler

- Önceki uygulayıcının gösterim testi ekleri ve gerçek sim testindeki revizyon eşitlemesi korundu. `Kurulum.durum` ile gösterge adı çakışması yok; yeni şerit testinde alan `gosterge`.
- Bekleyen jeton yanıtının kopmadan sonra yeniden kullanılabilir onay oluşturduğu yol testle yeniden üretildi: düzeltmeden önce 6 bellek testinden biri düştü. Kopma, vazgeçme, yeni istek, daha yeni revizyon ve ömrü aşan yanıt artık eski onayı etkinleştirmiyor. Jeton ömrü monotonik saatle, istek başlangıcından sayılıyor; ağda geçen süre ömre eklenmiyor (`web/src/a3/emniyet-istek.ts`).
- Bağlantı kopukken yeni emniyet/yer istasyonu isteği başlatılmıyor; sonuç açıkça `UYGULANMADI`. Bağlantı kurulunca operatör yeniden basar. Başlamış isteğin taşıma hatası `SONUÇ BİLİNMİYOR`; kopuk WebSocket'ten HTTP teslim edilmediği sonucu çıkarılmıyor (`web/src/a3/komut.ts`).
- Sahte sunucuda nötr komutun daraltıcı sayılması düzeltildi; nötr komut revizyon artırmıyor ve bayat çalıştırma kimliğiyle kabul edilmiyor. Aynı sınırda kısıtlayan istekler önce işleniyor, gevşetme `kisitlama_hakim` alıyor. Uygulama anında çalıştırma kimliği yeniden denetleniyor (`web/arac/sahte-a3/sunucu.ts`). Bu ek soket testlerinin çalışması ortam engeline takıldı.
- Gerçek sim testinde başlangıç revizyonu yetkili HTTP yanıtından alınıyor, olay ve kanal ayrı doğrulanıyor. Açılış hatası, erken süreç bitişi ve zaman aşımı artık temizlenerek raporlanıyor (`tests/butunleme/a3_emniyet_web.mjs`).

## Kabul maddeleri

| Madde | Sonuç |
|---|---|
| 1 — `npm test` ve dört düşürme denemesi | **Tamamlandı (2026-11-28, bu Mac).** `npm run denetle` geçti; `node arac/test.mjs`: 153 test, 153 geçti, 0 kaldı, 0 atlandı (18,8 s). Önceki oturumdaki `listen EPERM` engeli bu koşuda görülmedi. Dört hedef senaryo port açmayan gerçek istemci/komut/çözücü + bellek içi taşıma ve küçük DOM üzerinde doğrulandı; aşağıdaki mutasyonlar düştü. |
| 2 — gerçek sim Node testi | **Engelli.** Var olan `build/host-debug/sim/sunucu/ida_sim` kullanıldı; derleme yapılmadı. Test, sim'in çıkış 2 ile açılış adresinden önce bittiğini gösterdi: `A3 sunucusu başlatılamadı: dinleme başlatılamadı`. #57 sonrası uçların işlevsel akışına ulaşılamadı; geçti denemez. |
| 3 — host-debug tam tur ve Windows/macOS CI | **Bekliyor.** Kullanıcının talimatıyla ağır tur çalıştırılmadı; mimar koşturacak. Push/PR yok, bu değişiklikler için yeni CI sonucu yok. |
| 4 — kartta deneme | **Kapsam dışı.** Kartta deneme yapılmadı. |

## Düşürülerek gösterilen testler

Her bozma ürün kaynak dosyasında geçici uygulandı, hedef test çalıştırıldı ve kaynak `finally` ile eski hâline getirildi. Derleme hatası düşme kanıtı sayılmadı.

| Hedef test (`web/test/acil-serit.test.ts`) | Geçici bozma | Sonuç |
|---|---|---|
| Azaltan yön jetonsuz gönderilemez | Jetonsuz yerel ret yerine HTTP gönderimi | Test düştü. |
| Süresi dolmuş jetonla istek yinelenmez | Yerel süre denetimi kaldırıldı; ayrıca sunucu reddinde aynı gövdeyle otomatik ikinci POST eklendi | İki denemede de test düştü; yineleme bozmasında gerçek POST sayısı 2, beklenen 1. |
| Kopukken şerit `UYGULANMADI` yazar | Ret etiketi `SONUÇ BİLİNMİYOR` yapıldı | Test düştü. |
| Gösterge sunucu verisi gelmeden değişmez | Tıklama işleyicisi göstergeyi hemen `BASILI` yaptı | Test düştü. Sonuç olayı tek başına geldiğinde de gösterge değişmiyor; kanal bekleniyor. |

Bozmalar geri alındıktan sonraki son odaklı koşu: **10 geçti, 0 kaldı**. Testler olay/yanıt kapılarıyla sürülüyor; sabit uyku ve hız eşiği yok. Mevcut soketli testler kaldırılmadı veya atlatılmadı (`web/test/yardimci-emniyet.ts` yalnız ek taşıma fikstürüdür).

## W5-K51…K54 ve sözleşme okumaları

- **K51:** `src/ortak/acil.ts` ve `src/paneller/emniyet.ts` göstergeyi yalnız `emniyet/girdiler` kanalından çiziyor. Komut sonucu ayrı gösteriliyor; 202 uygulandı sayılmıyor. Kopuk bağlantıda `UYGULANMADI` sözü, HTTP başlamadan yerel engel olarak okundu; bu tercih `web/BENIOKU.md`'de de yazılı.
- **K52:** `EmniyetIstekleri.gevset` ve `yerGonder` jetonu gönderimden önce tüketiyor; hiçbir otomatik POST yinelemesi yok. Artıran yön A3-K92/K119 gereği revizyon bağı taşımıyor; revizyon çakışmasında yeniden gönderme yazılmadı. A3-K54/K57'nin 1.0.1 notu, kısıtlayan istekte bayat `run_id` reddinin önüne geçiyor.
- **K53:** Jeton yalnız bellekte; onay kutuları işaretsiz başlıyor; kısayol/toplu gevşetme yok. Kaynak denetimi testi geçti.
- **K54:** Sunucunun listesi istemcide genişletilmiyor; atış isteği adı veya A1 eşlemesi `atisKomutuMu` ile süzülüyor. Atış paneli gösterim olarak kaldı. A3 genel yer istasyonu yoluyla `FC_FIRE`'a izin verse de bu dilimde daha dar W5-K54 uygulandı.
- Mimari 08 §3.4'teki kontrol sahipliği ifadesine karşı A3-K55'in eşit yetki kuralı esas alındı; istemciye yeni rol/sahiplik mantığı eklenmedi. Jeton için 1 s erken düşürme payı tahmindir; ölçüm sonucu değildir. Desteklenmeyen parametre şemaları formda açıkça engelleniyor; doğrulamanın sahibi sunucu.

## Uygulanmama/gecikme yolları ve açık kalanlar

- Kopuk bağlantıda operatörün yeni girdisi gönderilmiyor ve uygulanmıyor; şerit `UYGULANMADI`, son gösterge `BAYAT` yazıyor. Yeniden bağlantı otomatik bir emniyet isteği üretmiyor; yeniden basmak gerekiyor.
- Jetonsuz, onaysız, yanlış seçimli, tüketilmiş veya süresi dolmuş gevşetme yerelde engelleniyor. Revizyon/jeton reddinde yeni onay kendiliğinden alınmıyor.
- İstek başladıktan sonraki yanıt kaybında uygulama bilinmiyor; panel bunu başarılı veya uygulanmamış saymıyor. Gerçek sim çalışamadığı için bu yolda fiziksel sınır gecikmesi ölçülmedi.
- Eski uygulamada geç jeton yanıtının onayı geri getirmesi testle gösterildi ve düzeltildi. Sahte sunucudaki kısıtlama sırası ve nötr komut kusurları kod incelemesinde bulundu ve düzeltildi; yeni soket testleri **henüz çalışarak doğrulanmadı**. Gerçek sim'de bir emniyet girdisinin geç uygulandığına dair bu koşudan kanıt yok.
- Tam `npm test`, gerçek sim akışı, host-debug turu, Windows/macOS CI ve emniyet sınıfının bağımsız tam inceleme + dar denetimi açık. Soket testlerini geçerli ortamda çalıştırmadan kabul kapanmaz.
- Git staging/commit denemesi ortak Git dizinindeki worktree `index.lock` dosyasını oluştururken `Operation not permitted` ile durdu. Commit oluşmadı; istenen `Co-Authored-By: Codex gpt-6.1-sol <noreply@openai.com>` satırıyla commit atma işi de açık. Push/PR yapılmadı. `sim/`, `core/`, `firmware/`, `tests/kabul/`, `docs/DURUM.md` ve karar günlüğü değiştirilmedi.
