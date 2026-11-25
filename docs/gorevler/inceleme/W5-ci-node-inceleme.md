# Bağımsız inceleme: W5-ci-node (PR #50, uç `2a83de2`)

İnceleyen: `ida-inceleyici` (Opus; sol sınırı dolu, K25/K27; sol inceleme borcundadır). İncelenen uç: `2a83de2`.

## Hüküm

Düzeltme turundan sonra birleştirilebilir. K23 gereği tek düzeltme turu yapıldı (aşağıdaki sonuçlar).

## Ürün kusuru (CI'daki Node testleri ilk kez koşunca bulundu)

- Ne: `hiz` komutu dünya `durum_rev` sayacını artırıp `komut_sonucu`'nu yayınlıyordu, ama yalnız `a3_durum_kirli_`'yi kuruyordu; anlık görüntü ancak `b % kDurumSinir == 0`'da (80 adım) yenileniyordu. O sürede `GET /api/v1/durum` ve HTTP girişindeki eşzamanlılık denetimi bayat revizyonu, `calistirma/durum` kanalı R+1'i gösteriyordu (A3-K122, K123: iki yayın noktası birbirini tutmuyor; 409 yanıtı "güncel" diye bayat değeri söylüyordu).
- Nerede: `sim/sunucu/src/sunucu.cpp`, `a3_calistirma_isle` içindeki `calistirma_hiz` kolu.
- Nasıl bulundu: dört Node testi (`butunleme.a3_uctan_uca_web`, `butunleme.a3_sonar_web` ve iki `sim.a3.*_web`) CI'da ilk kez koşunca `hiz` sonrası `duraklat` bayat `durum_rev` ile 409 aldı. İlk uygulayıcı turu bunu yeniden deneme yardımcısıyla maskelemişti; mimar kararı: ürün düzeltilir, test kesinleşir.
- Düzeltme: `hiz` kolu `a3_anlik_guncelle()` çağırır (komut_sonucu'ndan önce; `a3_gecis` ile aynı yardımcı). Aynı işlevdeki öteki `durum_rev` artıran komutlar (`durdur`, `duraklat`, `surdur`, `adimla`) `a3_gecis` üzerinden gittiği için anlık görüntüyü zaten yeniliyor; başka etkilenen komut yok.

## Bulgular

| # | Bulgu | Sonuç |
|---|---|---|
| ÖNEMLİ 1 | `a3_uctan_uca.mjs`: yeniden deneme yardımcısı ürün kusurunu maskeliyor | Yardımcı kaldırıldı; `hiz` sonrası `GET /durum` `durum_rev` = önceki + 1 kesin doğrulanıyor, `duraklat` ilk denemede uygulanıyor |
| ÖNEMLİ 2 | `hiz` anlık görüntüyü yenilemiyor (ürün kusuru, yukarıda) | Düzeltildi; C++ bütünleme testi (`butunleme.a3_calistirma`) ve Node testi aynı kusuru yakalar |
| ÖNEMLİ 3 | macOS doğrulanmadı | macOS koşusu 45571803225 (`workflow_dispatch`, `2a83de2`) yeşildi; PR denetim listesinde atlanmış görünen, gönderimin kendi koşusudur (`derleme-mac` yeni gönderimde yalnız eşzamanlılık grubu için tetiklenir, işi atlanır). Yeni uç için koşu yeniden başlatıldı |
| KÜÇÜK 4 | Yeniden yazımda yanlış madde atfı (A3-K123'e "olay görüntüden önce görünebilir") | Yorum kaldırıldı; yerine A3-K122 doğru atfıyla kesin doğrulama yazıldı |
| KÜÇÜK 5 | `IDA_BAGIMLILIK_ZORUNLU` açıklaması Node/`web/node_modules` atlamasını söylemiyor | Kök `CMakeLists.txt` açıklaması güncellendi |
| KÜÇÜK 6 | `npm ci` kurulum zincirinin başında; düşerse ikincil hata üretir | Adım zincirin sonuna taşındı (`id: npm`); Node testi isteyen `host-debug` ve `host-asan` adımları `steps.npm.outcome == 'success'` ister (`derleme.yml`, `derleme-mac.yml`) |
| KÜÇÜK 7 | `paths-ignore: docs/**` şema değişikliğini yok sayıyor, oysa derleme `docs/sozlesmeler/sema/*.json`'u tüketiyor | `paths` + `!` düzenine çevrildi (`**`, `!docs/**`, `docs/sozlesmeler/sema/**`, `!**/*.md`); öteki yollar için davranış aynı; `web.yml` yorumu güncellendi |
| KÜÇÜK 8 | `a3_sonar_web.mjs` abonelik sayısını `size >= 2` ile denetliyor | İki kanal adıyla (`sonar/ping360`, `sonar/ping1d`) denetleniyor |
