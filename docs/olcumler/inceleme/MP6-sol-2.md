# sol ikinci incelemesi: mp6

## Hüküm

**Koşulsuz kabul yok: 2 önemli bulgu var; kritik hata saptamadım.** Yeni görünür referansın G1/G3 için kullanımı rapor içinde tutarlı. G4’ün eski görünür koşudaki ilk okuma süresine dayanması gerekçeli.

Hedefin tamamı ve rapor okundu; `.godot/` dışlandı. Ham verilere erişilmediğinden ölçülmüş yüzdelikler, görünürlük kayıtları ve piksel eşitlikleri bağımsız yeniden üretilemedi. Dosya değiştirilmedi, kurulum yapılmadı.

## İlk incelemedeki maddelerin durumu

| # | Durum | Not |
|---|---|---|
| 1 | giderildi | §4.1, §4.2 ve G1/G3 aynı `b_gorunur_2` koşusunu kullanıyor: 9000 kare, 299/299 aralık, 300,0 s, 19/19 dış örnek. Önceki çizimsiz aralıklar doğrulanmamış bırakılmış. Güç koşulu sapması aşağıda. |
| 2 | giderildi | Geri çağrı p99’u 58,86 ms; hazır olma p99’u 68,00 ms olarak ayrılmış. Kod iki farklı başlangıç noktası kullanıyor. İlk incelemedeki “maksimum p99 sanılmış” teşhisi kesinleşmiyor; yazarın farklı büyüklük açıklaması tutarlı. |
| 3 | giderildi | `olcum.gd:422,445` ilk verinin hazır zamanını ikinci okumadan önce kaydediyor. Yinelemenin görünür kalmadığı §4.4 ve §8’de açık. G4’te kullanılan eski ilk okuma süresi bu hatadan etkilenmez. Tekrarlanan okuma da GPU beklemesi içerir; düzeltilmiş açıklama [Godot kaynağıyla](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/servers/rendering/rendering_device.cpp) uyumlu. |
| 4 | giderildi | `--ref` varsayılan seçimi gerçekten geçersiz kılıyor. Karşılaştırılan sayı referansta bulunan üretilmiş karelerden, karşılaştırılmayan sayı kalan üretilmiş karelerden hesaplanıyor (`analiz.py:195–226,269–270`). |
| 5 | kısmen | Geçersiz `a_*` koşuları yinelenmemiş ve genelleme kaldırılmış. Ancak eklenen `b_tamekran` doğrulaması yanlış olumlu sonuç verebiliyor; yeni bulgu 1. |
| 6 | giderildi | Bulunamayan küre sayısı hesaplanıp tabloda gösteriliyor; G3 bütün karelerde konum bulunmasını şart koşuyor. Bildirilen sıfır sayısı ham veri olmadan doğrulanamadı. |

## Yeni bulgular

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem | Kapatılacağı yer |
|---|---|---|---|---|---|
| 1 | `tools/olcum/mp6/analiz.py:59–66`; `docs/olcumler/MP6-godot-kamera.md:124,226` | **Ekran genişliği ölçülmüyor.** `sag`, listedeki pencerelerin sağ kenarından türetiliyor. Ekranda yalnız örtünün X=0’daki 100×100 penceresi bulunduğunda gerçek işlev 19/19 “tam ekran” döndürdü. Yükseklik de sınanmıyor. Dolayısıyla bu sayaç tam ekranın sürdüğünü kanıtlamaz. | Pencereyi gerçek ekran sınırlarıyla karşılaştır; [Apple `CGDisplayBounds`](https://developer.apple.com/documentation/coregraphics/cgdisplaybounds(_:)) bu sınırları verir. Mevcut koşu için bağımsız boyut kanıtı yoksa tam ekran hükmünü “doğrulanmadı”ya indir. G2’nin üç zorunlu durumunu etkilemez. | önemli | MP6 çözümleme ve rapor |
| 2 | `docs/olcumler/MP6-godot-kamera.md:18,213–215`; ana daldaki `docs/gorevler/W0-MP6a.md:59` | Yeni görünür referans pilde alınmış; görev belgesindeki **“Mac prize takılı” şartına** istisna kaydı yok. Rapor güç durumunu açıklıyor fakat kabul hükmünde bu görev sapmasını değerlendirmiyor. | G1/G3 hükmüne koşul sapmasını ekle; pildeki kanıtın mimarca kabul edildiğini belgele veya şartın sağlandığı ölçüme dayan. Bu sapma tek başına kare sonuçlarının yanlış olduğunu göstermez. | önemli | MP6 raporu ve mimar değerlendirmesi |

## Yeniden hesaplananlar

| Hesap | Sonuç |
|---|---:|
| 30 Hz × 300 / 120 / 20 s | 9000 / 3600 / 600 istek |
| 5875 / 9000; 195 / 9000 | %65,2778; %2,1667 |
| Referans 0…8999; üretilen 9000 / 3125 / 8805 kare | Tamamı referansta bulunduğunda karşılaştırılmayan 0 |
| Her 30. kare, 0…8999 | 300 örnek |
| 640×480 RGB8 / RGBA8 | 921.600 / 1.228.800 B |
| 1280×720 RGB8 / RGBA8 | 2.764.800 / 3.686.400 B |
| İki kare / 30 veya 120 çizim/s | 66,667 / 16,667 ms |
| 0,309 ≤ 1; 0,49 ≤ 0,75 | Konum ve gölge eşikleriyle uyumlu |

Aralık sürelerinin zaman damgası farklarından toplanması doğru; **299 aralık doğrudan 299 saniye değildir**. Gerçek süre toplamları ve p99 değerleri ham veri olmadan yeniden hesaplanamadı.

## İskelete ya da diğer bölümlere notlar

- Yol (a)’nın elenmesi, gizlinin uygulama gizleme olarak tanımlanması ve gölge eşiği mimar kararlarıyla uyumlu. Windows, iki zamanlı çizim, zamansal etkiler ve uzun koşular açık bırakılmış; Ö5-k/M28/M30’un kapatılmaması doğru.
- `force_draw` bağlantısı ve `UPDATE_ONCE` davranışı canlı [RenderingServer](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/servers/rendering/rendering_server.cpp) ve [viewport kaynaklarıyla](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/servers/rendering/renderer_viewport.cpp) destekleniyor.
- Sürüm ve kısa commit [resmî duyuruyla](https://godotengine.org/article/maintenance-release-godot-4-7-2/), ZIP SHA-256 değeri [resmî varlık listesiyle](https://github.com/godotengine/godot-builds/releases/expanded_assets/4.7.2-stable) uyuşuyor. `SHA512-SUMS.txt` içeriğine erişilemedi; SHA-512 eşleşmesi doğrulanmış sayılmadı.