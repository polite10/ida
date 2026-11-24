# Bağımsız inceleme: W5-kontrol-acil (`c882648..744256f`, `core/src`)

İnceleyen: `ida-inceleyici` (Opus; taze bağlam; sol yerine, kullanıcı kararı K25; sol inceleme borcu listesindedir). Kod okunarak ve rampa çarpanı için ayrı bir prob programıyla (1,6·10⁸ rastgele örnek) incelendi; test turları koşturulmadı.

## Hüküm

Acil durdurmanın uygulanmadığı ya da geç uygulandığı bir yol ve yanlış gevşetme bulunmadı (K21 sınıfı bulgu yok). Sıfır olmayan `thr_cmd` yalnız tek yerde (`cikti.c:131`), yalnız `step_ok` iken ve `ida_kon_cikti` kapısından geçtikten sonra yazılıyor; öteki bütün çıktı yolları (NOT_INIT, hata dönüşleri, `IMU_REJECTED`, tick, çökme, arıza eylemi) sıfır yazıyor. Kısıtlamanın bütün kaynakları ((b)'deki örnek içeriği, bayatlık, SESSION_END, init değeri, tanımsız bit, (e)'deki bayatlık) kanca ya da aynı çağrının adımıyla yakalanıyor. Rampa çarpanı [0, 1) aralığında; prob programında zarf aşımı 0.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Önem | K21 |
|---|---|---|---|---|
| 1 | `core/src/gorev/gorev_yurut.c:419-446`; `gorev_yukle.c:305-308` | Acil durdurmayla (ya da çekirdeğin başka bir koşuluyla) duran görevde `MISSION_CTL PAUSE` `BAD_MODE` alıyor; operatör duraklaması kaydedilemiyor, engel kalkınca görev kendiliğinden sürüyor. | orta-düşük | hayır |
| 2 | `core/src/cerceve/cerceve.c:603` | Acil durdurma kancası `out` NULL ve reddedilen çağrıda da kontrol durumuna yazıyor; kısıtlayan yönde, ama A4-K113'ün (b2) satırıyla sözleşme notu gerektirir. | düşük | hayır |
| 3 | `core/src/kontrol/kontrol.c:205-207, 937-946` | `rampa_t0_ns` tutarlılık denetiminde yok; bellek bozulursa çarpan kalıcı 0 kalabilir ya da rampa kısalır. | düşük | hayır |
| 4 | `core/src/gorev/gorev_yurut.c:412-433` | Görev modülü etkin acil durdurmayı kontrol modülündeki işlevden okuyor (bağımlılık); karar kaydı gerekli. | düşük | hayır |

## Denetlenip sorun bulunmayanlar

Çıktı yollarının tam listesi ve kapıdan geçişleri; tick → step ve kısa acil durdurmalarda mandal; rampa (T = 0, ilk adım, 50 ms kırpma, rampa sırasında yeniden acil durdurma, SESSION_START, taşma ve sıfıra bölme yok); kart tarafında double yok; kalkışta sıfırlanan birikimli durumlar; erken dönüşün yan etkileri; görev engelinin operatör duraklamasını ve arıza iptalini silmemesi. Doğrulanmadı: Z1 CSV bayt eşitliği ve test turları (uygulayıcı ve bütünleme raporlarında ölçüldü).
