# Kalan iş blokları: durum taraması ve sıra (2026-11-24 akşam)

Kaynak: `main` (`b956e52`) üzerinde salt okunur tarama (agent raporu; derleme/test koşmadı). Yüzdeler kaba tahmindir, ölçüm değildir. Bu belge mimarın sıralama notudur; görev belgelerinin yerine geçmez.

## Bloklar

| Blok | Hazır (tahmin) | Bugün var olan | En büyük eksikler | Dokunulacak dizinler / çakışma |
|---|---|---|---|---|
| Görselleştirme (07, A2) | ≈ %25 | `viz/a2` (kodek, oturum, sahne; Godot'suz), `viz/eklenti` (GDExtension), `viz/proje` (Godot 4.7.2), sahte A2 kaynağı, `acik_su` modelleri | **Sim'de A2 sunucusu hiç yok** (yalnız `sim/include/ida/a2_wire.h`); kamera hattı ve kare halkası; çalıştırma nesli/atlama; Windows derlemesi; CI'da Godot'lu test | yeni `sim/a2`, `sim/sunucu` (çakışma), `sim/fizik`, `viz/`, `assets/` |
| Görev yürütücüsü (02 §2.4, §2.6) | ≈ %35–40 | Kip tablosu, ARM, arıza matrisi, GUIDED_SP (dikey ve yön kanalı) | Görev durum makinesi ve `MISSION_*` (bugün `UNSUPPORTED`); öğe yürütücüleri (WAYPOINT, DIVE_TO, SURFACE, HOLD, SCAN_*, WAIT_GNSS); LOS/ILOS; derinlik referans modeli; MANUAL komutu; A3 `mission_upload` | `core/src/{gorev,gudum,kontrol}`, `tests/kabul/gorev`; W5-kestirim-yatay'a bağlı; `test_param_hiz.c` altın özeti |
| Kartta sonar işleme (A4-K40, K92–K94, K110) | ≈ %5–10 | Kapı denetimi, `ida_link` Ping360 birleştirme, platformun arka plan çağrısı; `core/src/sonar` yer tutucu | Dilim kopyası ve 2 yuvalı depo; arka plan dilimleme ve `K_bg` tüketimi; en az irtifa arızası; **karttan engel uyarısı/harita çıktısı sözleşmede tanımlı değil** (mimar tanımlamadıkça yazılmaz) | `core/src/sonar`, `core/src/{gorev,atis}`; PR #41'e bağlı |
| Kayıt ve tekrar (A6: 61 kural, ≈ 60 kanal) | ≈ %3 | Yalnız CSV ve `run_id` | MCAP yazıcısı + zstd bağımlılıkları; kanal/kademe; kayıt kancaları (fizik adımı, A1 baytları, çekirdek çağrıları); okuyucu; dört tekrar düzeyi; A3 kayıt/izleme uçları | yeni `sim/kayit`, `sim/{sunucu,kopru,fizik,sil,a3}`, `vcpkg.json`, `web/` |
| Web ve A3 uçları (A3, 08) | ≈ %50 | Beş panel (gösterim), A3 sunucu çekirdeği, emniyet/yer istasyonu/çalıştırma uçları | Senaryo listeleme/yükleme/ön denetim uçları; panelden komut (emniyet, yer istasyonu, görev); hata enjeksiyonu; kayıt/izleme; CI'da Node testleri (bugün fiilen atlanıyor: `web/node_modules` yok) | `web/`, `sim/a3/`, `sim/sunucu` (çakışma), `sim/senaryo` |

En büyük çakışma noktası `sim/sunucu/src/sunucu.cpp` (≈ 3 700 satır): görselleştirmenin sim tarafı, kayıt, A3 uçları, W4-duraklat ve W4-sunucu-kucuk aynı dosyada. Bu işler aynı anda koşmaz; sunucu dosyası bölünmeden paralellik sınırlıdır.

## Sıra (mimar; kullanım sınırı gözetilerek)

1. Koşanlar: W5-atis-emniyet (düzeltme + dar denetim), W5-kestirim-yatay, W4-duraklat, W4-sonar-sahne; PR #41.
2. **Uçtan uca görünür zincir:** sim'de A2 sunucusu (görselleştiricinin gerçek sim'e bağlanması; kamera hariç) ve görev yürütücüsü (görev yükleme + WAYPOINT/DIVE_TO/SURFACE/HOLD; tarama öğeleri sonra).
3. W4-sunucu-kucuk, W5-atis-platform (G-K01–G-K20) ve W5-K5 sim kuralı.
4. Web: senaryo yükleme uçları ve panelden komut; CI'da Node testleri.
5. Kartta sonar işleme (A4-K40, K93, K94; en az irtifa).
6. Kayıt (önce `olaylar` ve `telemetri` kademeleri, `izleme` tekrarı), sonra öteki tekrar düzeyleri.
7. Kamera hattı; Windows hedef bilgisayarda deneme ve paketleme; kart ölçümleri.

## Güncelleme (2026-11-25)

W4-sunucu-kucuk (#48) ve W5-a2-sunucu (#49; sim tarafında A2 kitaplığı `sim/a2`, kamera hattı hariç) `main`'de. Sıradaki: W5-bagla-a2 (görev belgesi hazır, başlatıldı). Görselleştirme bloğunda kalan: bağlama, kamera hattı ve kare halkası, Windows derlemesi, CI'da Godot'lu test.
