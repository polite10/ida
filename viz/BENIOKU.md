# viz/ — görselleştirici (Godot projesi, GDExtension ve A2 istemcisi)

Operatörün 3B görünümü (iskelet §7; bölüm 07; A2 ve A7 sözleşmeleri). W3-viz dilim 1: Godot
sahnesi, A2 istemcisi ve sahte A2 kaynağı. Gerçek simülasyona `viz/calistir.sh --sim` ile
bağlanılır (W5-bagla-a2; `ida_sim --a2-port`). Kamera sensörü hattı (kare halkası) bu dilimde yoktur.

## Çalıştırma (macOS)

```
viz/calistir.sh                    # sahte kaynak, tam kademe
viz/calistir.sh --kademe sade      # sahte kaynak, sade kademe
IDA_SIM_SECENEKLER="--sonar" viz/calistir.sh --sim scenarios/sonar-havuz.toml   # gerçek sim (SIL, gerçek zamana yakın hız)
```

Betik gerekiyorsa eklentiyi ve sahte kaynağı derler (`build/viz`, `IDA_VIZ=ON`, Release),
Godot projesini ilk kez içe aktarır, sahte kaynağı başlatır ve pencereyi açar. Godot 4.7.2
ikilisinin yolu `GODOT` ortam değişkeninden alınır; verilmezse
`${IDA_CACHE:-$HOME/.cache/ida}/godot/4.7.2/` altındaki resmî ikili denenir (MP6 ölçümünün
kurulum yeri). Pencerede "SAHTE VERİ" etiketi görünür: sahte kaynak kinematiktir, fizik değildir.

**Gerçek sim (`--sim <senaryo.toml>`).** Betik `ida_sim`'i (`build/host-release` ya da
`build/host-debug`; `IDA_SIM` ile başka yol) `--senaryo <senaryo> --a2-port <port> --sil-hiz 1`
ile başlatır, sim'in günlüğünde A2 dinleyicisinin açıldığını görünce Godot'yu aynı porta bağlar
(`--sahte` verilmez: "SAHTE VERİ" etiketi görünmez). Seçenekler: `--hiz <çarpan>` (SIL hız
çarpanı; varsayılan 1), `--kok <dizin>` (senaryonun depo kökü; varsayılan bu depo), `--a2-port`,
`--cozunurluk`, `--derle`; `IDA_SIM_SECENEKLER` ile sim'e ek seçenek verilir (ör. `"--a3-port 47100"`).
Görünüm kademesi senaryonun `kalite.gorunum` alanındandır. Pencere kapanınca sim tek `SIGINT` ile
düzgün kapatılır (terminalde Ctrl-C hem Godot'ya hem sim'e doğrudan ulaşır; betik sinyali yinelemez, sim'in
kapanışını en çok 10 s bekler, süre dolarsa tek `SIGTERM` yollar) ve günlüğü (kapanış özeti, A2 sayaçları) yazdırılır; senaryonun süresi dolarsa
sim biter ve pencere "BAĞLANTI YOK" uyarısıyla son sahnede kalır. Senaryonun varlık kümesinin
ikili dosyaları bulunmalıdır: `assets/ornek` kümesinin `.glb` dosyaları yoktur, onu gösteren
senaryolar (`scenarios/z1.toml`, `gorev-havuz.toml`) sim'de koşar ama görselleştirici sahneyi
kuramaz (`EKSIK_VARLIK`; sim `ret(manifest)` verir); gösterilebilir görev senaryosu
`scenarios/gorev-havuz-3b.toml`'dur (`gorev-havuz.toml`'un `havuz` kümeli eşi:
`viz/calistir.sh --sim scenarios/gorev-havuz-3b.toml`; sade kademe) ve açık deniz eşi
`scenarios/gorev-acik-su-3b.toml`'dur (`acik_su` kümesi, 6 m derinde kumlu taban, tam kademe:
`viz/calistir.sh --sim scenarios/gorev-acik-su-3b.toml`); `assets/havuz` kümesini gösteren
`scenarios/sonar-havuz.toml` yüklenir (zaman çizelgesindeki sonar arızaları yüzünden sim'e
`--sonar` verilmelidir: `IDA_SIM_SECENEKLER="--sonar"`). Bugün fizik dünyasında yalnız araç
vardır: hedef ve torpido gövdesi yayınlanmaz (`sim/sunucu/BENIOKU.md` "A2 sunucusu").

**Su altı görünümü ve senaryo değerleri (W5-viz-gorsel).** Görünüm, senaryonun `ortam.su_optigi` ve
`ortam.isik` değerlerinden kurulur (A2 `Ortam`: `beta_d`, `beta_b`, `b_sonsuz`, ortam ışığı). A5-K101
gereği senaryo bunları vermezse sim 0 yazar ("berrak su", ortam ışığı 0) ve çıkan görüntü siyah arka plan ve
ışıksız sahnedir; görüntü gerçekçiliği için değerleri senaryo yazarı verir (`gorev-havuz*.toml` ve
`gorev-acik-su-3b.toml` verir; değerler ölçüm değil, yer tutucudur). Senaryo vermemişse görselleştirici
**yalnız çizim için** kendi görsel varsayılanını (kıyı suyu, `ana.gd` `GORSEL_*`) kullanır ve günlüğe
"görsel varsayılan kullanıldı" yazar; fiziğe, sonara ve sensörlere dokunmaz. Bugün `viz/proje`'de kamera
sensörü hattı yok. Hat eklendiğinde kamera karesi `GORSEL_*` varsayılanlarını, araç ışık çarpanını ve karo
dokusunu değil, A2 değerlerini aynen kullanmalı. A2 `Ortam` "verildi" ile "kalibre edilmedi" ayrımını
taşımaz: açıkça verilen 0 (berrak su, karanlık sahne) da "verilmedi" sayılır ve görsel varsayılan
kullanılır (günlük: "sıfır geldi; verilmemiş sayıldı"). Sahne kurulunca günlüğe
A2'den gelen `ortam` değerleri de yazılır (`ida-viz: ortam (A2): ...`). Kademe ayrımı 07 §5'tedir:
sade = Godot sisi (geri saçılım rengi + zayıflama yoğunluğu), ekran/derinlik dokusu okumaz; tam = son
işlem gölgelendirici (`su_alti_son`) ve dalga geometrili yüzey. `havuz_ornek` varlığının `havuz_beton`
malzemesine (havuz tabanı ve duvarları) görselleştirici dünya uzayında 0,5 m karolu üç düzlemli bir doku
giydirir (`varlik_yukleyici.gd` `KAPLAMALAR`; doku çalışma zamanında bir kez üretilir ve paylaşılır, depoda
ikili dosya yok): düz renkli yüzey su altında ölçek ve derinlik hissi vermiyordu. Aynı malzemeyi kullanan
`acik_su_sonar/deniz_ornek` (açık deniz tabanı) düz kalır. Aracın ön ışıkları (A7 `KHR_lights_punctual`)
`ARAC_ISIK_CARPANI` ile kısılır. Açık deniz senaryosunda sim'in fiziğinde dalga modeli olmadığı için A2
dalga kümesi boştur (`a2_baglanti.cpp`); yüzey sim'de düzdür, yüzeydeki dalgacıklar yalnız
gölgelendirici ayrıntı normalleridir (W5-K61). Dalgalı yüzey geometrisi sahte kaynakla görülür
(`viz/calistir.sh`).

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
`--olcum=<dosya.json>` (kare süresi; senaryo sıfırlanmadan önce yazar ve çıkar),
`--ekran-plani=<t:ad:sapma:egim:uzak;...>` (ekran görüntüsü anları: sim zamanı s, dosya adı, yörünge sapma°, eğim°,
uzaklık m; t'ye göre sıralanır, bozuk öğe atlanır ve uyarı yazılır; en büyük t'den 1 s sonra çıkar;
`--ekran-goruntusu` olmadan görüntü alınmaz; verilmezse yukarıdaki sahte senaryo çizelgesi),
`--max-fps=<n>` (kare sınırı; 0 = sınırsız. Proje 120 fps ile sınırlıdır ve ölçümde bu sınır kare süresini
maskeler; Godot'ya doğrudan `--max-fps 0` de verilebilir). Örnek:
`viz/calistir.sh --sim scenarios/gorev-havuz-3b.toml -- --ekran-goruntusu=/tmp/ekran --ekran-plani="6:yuzey:-38:13:1.55;36:gecis:30:-10:1.6;52:alt:-125:24:2.6"`.

## Windows (x64, MSVC)

Eklenti (`ida_viz.dll`), soket katmanı ve sahte kaynak MSVC ile derlenir; CI işi
`.github/workflows/viz-windows.yml`'dir (elle ya da `viz/**` değişince koşar; çıktı: `viz-windows-x64`
iş çıktısı = `ida_viz.dll`, `ida_viz.gdextension`, `ida_sahte_a2.exe`, `calistir.ps1`).

**Gereksinimler (hedef makine).** Windows 10/11 x64; Visual Studio 2022 Build Tools ("C++ ile
masaüstü geliştirme" + "C++ CMake araçları", Ninja dahil); Python 3 (godot-cpp bağlama üreticisi);
Godot 4.7.2 Windows ikilisi (resmî: `godotengine/godot-builds`, `Godot_v4.7.2-stable_win64.exe.zip`,
SHA-512 `83decd58...2259993`, `SHA512-SUMS.txt` ile aynı). Gerçek sim kipi için `ida_sim.exe`
(`cmake --preset host-release; cmake --build --preset host-release --target ida_sim`, `sim/sunucu/BENIOKU.md`).

**Derleme (elle).** "x64 Native Tools" isteminde ya da `viz\calistir.ps1 -Derle` ile:

```
cmake -S . -B build\viz -G Ninja -DIDA_HEDEF=host -DCMAKE_BUILD_TYPE=Release -DIDA_VIZ=ON
cmake --build build\viz --target ida_viz_eklenti ida_sahte_a2
ctest --test-dir build\viz -R "host\.viz"
```

**Başlatma.**

```
$env:GODOT = "C:\araclar\Godot_v4.7.2-stable_win64.exe"
viz\calistir.ps1                                          # sahte kaynak, sade kademe (varsayılan)
viz\calistir.ps1 -Kademe tam                              # sahte kaynak, tam kademe
viz\calistir.ps1 -Sim scenarios\gorev-havuz-3b.toml        # gerçek sim (SIL); -Hiz, -Kok, -A2Port
```

Betik `calistir.sh`'in eşidir: eklenti derlenmemişse derler, Godot projesini ilk kez başsız içe
aktarır, sahte kaynağı ya da `ida_sim.exe`'yi başlatır, pencere kapanınca kaynağı kapatır. `-Sim`
kipinde sim'e pencere kapanınca tek Ctrl-C konsol olayı gider (düzgün kapanış, özet satırı); sahte
kaynak Windows'ta doğrudan sonlandırılır. Betik imzasız olduğundan ilk seferde
`powershell -ExecutionPolicy Bypass -File viz\calistir.ps1` gerekebilir.

**Hedef makinede ilk deneme (kullanıcı yapar; CI'da Godot'nun pencereli koşusu yoktur).**

1. `viz\calistir.ps1 -Derle` ile eklentiyi derleyin (ya da iş çıktısındaki `ida_viz.dll`'i `viz\proje\bin\` altına koyun).
2. `viz\calistir.ps1` açılır ve "SAHTE VERİ" etiketiyle sahne akar mı bakın (8 GB bellek, dahili ekran kartı).
3. `viz\calistir.ps1 -Sim scenarios\gorev-havuz-3b.toml` ile gerçek sim'e bağlanın; pencereyi kapatınca sim günlüğünde kapanış özeti görünmeli.
4. Ölçüm: `--olcum` ile kare süresi (`-Ek "--olcum=C:\temp\olcum.json"`); bellek için Görev Yöneticisi'nde `Godot` ve `ida_sim` çalışma kümesi. Sade ve tam kademeyi ayrı koşun, sonuçları `docs/olcumler/` altına rapor olarak işleyin. Bu rakamlar ölçülmeden bilinmez; bu belge tahmin vermez.
