# viz/ — görselleştirici (Godot projesi, GDExtension ve A2 istemcisi)

Operatörün 3B görünümü (iskelet §7; bölüm 07; A2 ve A7 sözleşmeleri). W3-viz dilim 1: Godot
sahnesi, A2 istemcisi ve gerçek sim sunucusu gelene kadar kullanılan sahte A2 kaynağı. Kamera
sensörü hattı (kare halkası) bu dilimde yoktur.

## Çalıştırma (macOS)

```
viz/calistir.sh                    # tam kademe
viz/calistir.sh --kademe sade      # sade kademe
```

Betik gerekiyorsa eklentiyi ve sahte kaynağı derler (`build/viz`, `IDA_VIZ=ON`, Release),
Godot projesini ilk kez içe aktarır, sahte kaynağı başlatır ve pencereyi açar. Godot 4.7.2
ikilisinin yolu `GODOT` ortam değişkeninden alınır; verilmezse
`${IDA_CACHE:-$HOME/.cache/ida}/godot/4.7.2/` altındaki resmî ikili denenir (MP6 ölçümünün
kurulum yeri). Pencerede "SAHTE VERİ" etiketi görünür: sahte kaynak kinematiktir, fizik değildir.

Tuşlar: `C` kamera kipi (araç takip ↔ serbest), `G` gövde eksenleri, `I` iz çizgisi, `H`
göstergeleri gizle, `F12` ekran görüntüsü. Takip kipinde sol fare ile aracın çevresinde dönülür,
tekerlekle yaklaşılır; serbest kipte sağ fare ile bakılır, W/A/S/D/Q/E ile gezilir.

## Dizinler

| Yol | İçerik |
|---|---|
| `a2/include`, `a2/src` | A2 kitaplığı (C++20, Godot'suz): tel kodlaması ve geçerlilik (`kodek`), SHA-256, NED → Godot dönüşümü (`cerceve`), dalga yüksekliği, sahne durumu + operatör saati + ara değerleme (`sahne`), soketsiz istemci oturumu (`oturum`), duvar saati |
| `a2/soket` | bloklamayan TCP ve yeniden bağlanan istemci (`IDA_VIZ=ON`) |
| `eklenti/` | GDExtension: `IdaA2Istemci` düğümü (godot-cpp 10.0.0-stable, API 4.7) |
| `proje/` | Godot 4.7.2 projesi: sahne ağacı (07 §2.2), su, ışık, operatör kamerası, göstergeler |
| `arac/sahte-a2/` | sahte A2 kaynağı: kinematik senaryo (yüzeyde bekler, dalar, tur atar, hedefe yaklaşır, 121 s'de sıfırlanır) |
| `arac/model-uret/` | `uret.py`: `assets/acik_su/` varlıklarını ve manifestini basit geometriden üretir |
| `test/` | birim testleri (`host.viz.*`), Godot ile yeniden bağlanma denemesi |

## Derleme ve test

Godot'suz kısım her host derlemesine girer (`host.viz.sha256`, `kodek`, `cerceve`, `sahne`,
`oturum`, `dalga`). `IDA_VIZ=ON` ile ayrıca `host.viz.okuma_butcesi`, `yeniden_baglanma` ve
`varlik_godot` (Godot başsız; varlık yükleyicisi, olumsuz A7 kümeleri `viz/test/varlik/`). Eklenti, soket katmanı ve sahte kaynak yalnız `-DIDA_VIZ=ON` ile derlenir (godot-cpp
derlemesi uzun sürer; CI'da kapalıdır):

```
cmake -S . -B build/viz -G Ninja -DIDA_HEDEF=host -DCMAKE_BUILD_TYPE=Release -DIDA_VIZ=ON
cmake --build build/viz --target ida_viz_eklenti ida_sahte_a2
ctest --test-dir build/viz -R 'host\.viz'
viz/test/yeniden_baglanma_godot.sh          # sahte kaynak öldürülüp yeniden başlatılır
```

Varlıklar yeniden üretilirse (`python3 -I viz/arac/model-uret/uret.py`) manifest özeti değişir;
`--denetle` depodakiyle bayt bayt karşılaştırır. Aynı betik `viz/test/varlik/` altındaki sınama
kümelerini de üretir.

## Otomasyon seçenekleri (Godot'da `--` sonrası)

`--a2-port <n>`, `--a2-operator-gecikme-us <n>` (A2-K93), `--sahte` (etiket),
`--ekran-goruntusu=<dizin>` (senaryonun 6, 17,5, 45 ve 104. saniyelerinde ekran görüntüsü),
`--olcum=<dosya.json>` (kare süresi; senaryo sıfırlanmadan önce yazar ve çıkar).
