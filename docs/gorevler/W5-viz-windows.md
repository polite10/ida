# W5-viz-windows — görselleştiricinin Windows derlemesi ve başlatıcısı

Tarih: 2026-11-25. Yazan: `ida-hafif-uygulayici` (Sonnet; K28). İnceleyen: `ida-inceleyici` (Opus; K23). Dal: `is/w5-viz-windows` (taban `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `W4-ortak.md`.

## Amaç

3B görünüm (`viz/`: Godot 4.7.2 projesi + C++ GDExtension) bugüne dek yalnız macOS'ta derlendi ve çalıştı. Ödevin hedef platformlarından biri Windows'tur (K16: 8 GB bellek, dahili ekran kartı). Bu iş eklentiyi Windows'ta (MSVC) derlenir, CI'da derlenip sınanır ve hedef makinede tek komutla başlatılır hâle getirir. Hedef makinede elle deneme bu işin parçası değildir (kullanıcı yapar); bu iş onu mümkün kılar.

Okunacaklar: `viz/BENIOKU.md`, `viz/CMakeLists.txt`, `viz/eklenti/**`, `viz/a2/soket/**`, `viz/calistir.sh`, `viz/test/**`; `.github/workflows/derleme.yml` (önbellek ve MSVC kurulum düzeni); A2-K38, K93; iskelet §6 (kalite kademeleri).

## Kapsam

1. **Windows derlemesi:** `-DIDA_VIZ=ON` ile `ida_viz_eklenti` (DLL), soket katmanı ve `ida_sahte_a2` MSVC ile derlenir; `.gdextension` dosyasında Windows girdisi; gereken taşınabilirlik düzeltmeleri (Winsock, yol ayırıcı, dışa aktarım). Davranış değişikliği yok.
2. **CI işi:** yeni iş akışı `.github/workflows/viz-windows.yml`: yalnız elle (`workflow_dispatch`) ve `viz/**` ya da kendi dosyası değişen PR'larda koşar; godot-cpp derlemesi önbelleklenir; `host.viz.*` testleri (soketli olanlar dahil) koşar; resmî Godot 4.7.2 Windows ikilisi indirilebiliyorsa (sürüm ve SHA-256 sabit; birincil kaynaktan) başsız `varlik_godot` testi de koşar, indirilemiyorsa nedeni raporlanır. Eklenti DLL'i ve çalıştırmak için gerekenler iş çıktısı (artifact) olarak yüklenir. Var olan `derleme.yml` ve `derleme-mac.yml` DEĞİŞMEZ.
3. **Windows başlatıcısı:** `viz/calistir.ps1`: `viz/calistir.sh`'in eşi (sahte kaynaklı kip ve `-Sim <senaryo>` kipi: `ida_sim.exe`'yi `--a2-port` ile başlatır, Godot'yu bağlar, kapanışta sim'i düzgün kapatır). Godot ikilisinin yolu `GODOT` ortam değişkeninden; varsayılan kademe `sade` (düşük donanım).
4. **Belge:** `viz/BENIOKU.md`'ye Windows bölümü: derleme, başlatma, hedef makinede ilk deneme için adım adım liste ve neyin ölçüleceği (kare süresi, bellek; `--olcum`).

## Kapsam dışı

Görselleştiricinin sahne, gölgelendirici ya da A2 istemci mantığında değişiklik; kamera hattı; `sim/**`, `core/`, `protocol/`, `firmware/`, `web/`, `docs/sozlesmeler/`; paketleme/kurulum programı.

## Kabul

1. `viz-windows` iş akışı bir koşuda yeşil: eklenti derlenir, `host.viz.*` testleri geçer; koşu kimliği ve süre (soğuk/sıcak önbellek; ölçüm, CI) raporlanır.
2. İş çıktısında eklenti DLL'i ve `.gdextension` bulunur; içeriği listelenir.
3. `viz/calistir.ps1` sözdizimi denetiminden geçer (CI'da `pwsh` ile ayrıştırma); Godot ile gerçek koşu yapılamıyorsa "koşturulmadı" yazılır.
4. macOS davranışı değişmedi: `viz/calistir.sh` ve `host.viz.*` testleri yerelde geçer.

## Test yazım kuralları (bağlayıcı)

Süreye göre değil olaya göre bekle; hıza bağlı eşik yazma; geçici ad ve portlar benzersiz; testi ön planda koştur, süreç adıyla bekleme döngüsü kurma.

## Rapor

PR ve dal ucu; kabul maddeleri; Windows'ta yapılan taşınabilirlik düzeltmeleri; CI süresi; hedef makinede denemeyi engelleyebilecek bilinen eksikler.
