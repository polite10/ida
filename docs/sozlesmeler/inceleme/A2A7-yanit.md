# A2 ve A7 sözleşmeleri: sol incelemesine yanıt (1. düzeltme turu)

| | |
|---|---|
| İnceleme | `docs/sozlesmeler/inceleme/A2A7-sol.md` |
| Uygulanan kararlar | Mimarın 1. düzeltme turu kararları (dokuz madde, ek kararlar) |
| Değişen dosyalar | `docs/sozlesmeler/A2-gorsellestirici.md`, `docs/sozlesmeler/A7-sahne.md`, `sim/include/ida/a2_wire.h`, `assets/manifest.schema.json` (`assets/ornek/manifest.json` değişmedi) |
| Sürüm | İki sözleşme 1.0-aday kaldı |

## Madde madde

| # | Önem | Durum | Yer | Ne yapıldı |
|---|---|---|---|---|
| 1 | kritik | giderildi | A2-K42, K52–K61, §3.8 yuva başlığı; `a2_wire.h` `YuvaBasligi`, `KareHazir`, `HalkaEslendi`, `halka_yenile` / `halka_eslendi` | Okunuyor olabilecek bir yuva hiçbir koşulda yeniden yazılmaz. Sim **boş** olmayan yuvayı ayırmaz ve hiçbir zaman aşımıyla geri almaz (K56, K57); boş yuva yoksa kare üretilmez ve `halka_dolu` olarak düşer (K42). Tüketici 1 s içinde bırakmazsa ya da bekleyen başvurusu varken bağlantısı koparsa **halka emekliye ayrılır**: yeni nesille yeni halka, eski adın kaldırılması, görselleştiriciye `halka_yenile`, tüketicilerin yeniden el sıkışması (K59). Eski halkaya artık kimse yazmadığı için yavaş okuyucu yarışa girmez. Seqlock kaldırıldı: paylaşımlı bellekte **atomik alan kalmadı**; `yazim_sayaci` yalnız tanılama içindir (K53, §3.8) |
| 2 | kritik | giderildi | A2-K58, K60, K61 | Sim her kare için bekleyen tüketici kümesini tutar; tüketici kimliği bağlantıdan türer; bırakma yalnız gönderenin o karedeki başvurusunu bir kez kaldırır; yinelenen ve yabancı bırakma yok sayılır. Bağlantı kapanışı hiçbir başvuruyu kaldırmaz; bekleyen başvuru varsa halka emekliye ayrılır (K60). Bu, mimar kararındaki "bağlantı kapanışı aynı başvuruyu ikinci kez azaltamaz" koşulundan sıkıdır: süreci hâlâ yaşayan ama bağlantısı kopan bir okuyucunun okumayı sürdürme olasılığı ayırt edilemediği için kapanışı bırakma saymak madde 1'i ihlal ederdi |
| 3 | kritik | giderildi | A2-K80–K85 (yeni §4.9), K29, K46, K66; `calisma_nesli` alanları (`Hosgeldin`, `KameraIstegi`, `KareHazir`, `KareDustu`, `SahneOlayi`, yuva başlığı) | Çalıştırma nesli (zaman çizgisi kimliği) sıfırlama ve atlamada artar. Yanıtlanmamış istekler iptal edilir ve geç yanıt yalnız yuvayı kurtarır (K81); hazır bekleyen kareler teslim edilmez (K82); tüketiciye verilmiş karelerin sonuçları reddedilir, tüketici sonucu göndermez ama yuvayı yine bırakır, kayıt yazıcısı eski kareleri eski nesil etiketiyle bırakır (K83, yaşam döngüsü) |
| 4 | kritik | giderildi | A7-K44, A7-U12, şemada `ortu_toleransi_m` | Köşe denetimi yerine iki yönlü, yüzey örneklemeli ölçüt: alanla orantılı ızgara örneklemesi, en büyük örnek aralığı `δ = τ/5`, birleşimin sınırı dışındaki parça örnekleri atılır, her örneğin öteki yüzeyin örneklerine uzaklığı ≤ `τ − δ`; bu, gerçek iki yönlü Hausdorff uzaklığının ≤ `τ` olduğunu garanti eder. `τ` düğüm başına manifestte zorunlu ve açıktır (varsayılan yok); bilinçli içbükeylik doldurması bu değerle kapsanır. sol'un delikli prizma karşı örneği A7-U12'de, şemanın izin verdiği her `τ` ile reddedilir |
| 5 | kritik | giderildi | A2-K21, K22, K23, K32, K33, K41; A2-U05, U06 | Tek tanım: `T` anındaki olay `T` ile başlayan adımın başında uygulanır; `T` zamanlı durum olay **sonrası** durumu taşır. Sim `T − h` (eski parçanın son durumu) ve `T` (olay sonrası ilk durum) iletilerini gönderir; görselleştirici `[T − h, T)` aralığında eski son durumu, `T`'den itibaren olay sonrası durumu çizer, iki yan arasında ara değerlemez. Doğum, ölüm ve kamera isteğindeki gövde durumları aynı tanıma bağlandı |
| 6 | kritik | giderildi | A7-K19 (yeni), K22, §3.6 `ERISILEMEZ`, §5, A7-U05 | Bütün rol ve bağlantı düğümleri `scenes[0]`'ın köklerinden erişilebilir olmalı; düğüm hiyerarşisinin ayrık katı ağaç olduğu da denetlenir (glTF 2.0 "Nodes and Hierarchy"). Godot'da bir rol düğümü için `get_scene_node` `null` dönerse (4.7.2 sınıf belgesi bu durumu açıkça yazıyor) görselleştirici varlığı ve sahneyi kurmaz, `hazir.durum = varlik_kurali` |
| 7 | kritik | giderildi | A2-K75–K79, A2-U16, A2-U21 | Porttan bağımsız sahiplik kilidi (`flock` / `LockFileEx`, `ida/a2.kilit`); yalnız kilit sahibi halka oluşturur, kaldırır ve dinleyiciyi açar. Kayıt dosyası güncellemeleri geçici dosya + `fsync` + `rename` ile atomiktir. Oluşturma ve kaldırmanın sırası, çökme penceresi ve açılıştaki kurtarma tanımlandı; kilidi alabilmek önceki sahibin sonlandığını gösterdiği için yaşayan başka bir sürecin nesnesi kaldırılmaz. İkinci sim süreci izinlidir ama A2'siz çalışır (K79) |
| 8 | kritik | giderildi | A7-U11 | "Tohum değişince özet değişir" kaldırıldı. Yerine: aynı girdi ve tohumla tekrarın (1 ve N iş parçacığı) aynı özeti verdiği; tohumun araca iletildiği ayrıca, API çağrısının parametreleri yakalanarak sınanır |
| 9 | önemli | giderildi | A7-K41, şemada `rv_k` ve `dmc_esigi` | CoACD 1.0.14'ün `Params` yapısındaki 19 algoritma ayarı kaynak koddan (`src/config.h`) sayıldı ve hepsi manifestte. Türetme kitaplık API'siyle yapılır ve API'nin aldığı 17 ayarın hepsi açıkça verilir; API'nin açmadığı `rv_k` (0,3) ve `dmc_thres` (0,55) 1.0.14'te `Params` kurucusunda sabit olduğu için şemada bu değerlere sabitlendi. Komut satırı aracı kullanılmaz: 1.0.14 `main.cpp`'de `-dt` hem `max_ch_vertex`'e hem `dmc_thres`'e bağlı |

## Ek kararlar

| Konu | Durum | Yer |
|---|---|---|
| Kamera kademe değerleri A2'de: tam 640 × 480, 30 Hz; sade 320 × 240, 15 Hz (tahmin; Windows ölçümüyle sabitlenecek); tek kamera | giderildi | A2 §6, §3.6, A2-A01; `a2_wire.h` `kTam*`, `kSade*` |
| `is_always_lock_free` yeterli kanıt değil; atomik alanları en aza indir | giderildi | Paylaşımlı bellekte atomik alan sıfır; `static_assert` ve `<atomic>` başlıktan çıkarıldı. Görünürlük varsayımı (çitler + soket sistem çağrısı; clang ve MSVC; arm64 ve x86-64) A2-K53'te açıkça bağlandı, "doğrulanmadı" işaretli (A2-A10) ve A2-U13 ile sınanır |
| SIL'de beklememe ve tekrarlanabilirlik sınırı | değişiklik yok | A2-K47, K51 |

## A8'in beklentileriyle uyum

| A8'in beklentisi | A2'deki karşılığı |
|---|---|
| Ölçüm zamanı pozlama sonu + ayrı pozlama süresi; çizim zamanı bunlardan türer | Uyumlu hale getirildi: yuva başlığında ve A2-K41'de birincil gösterim `t_olcum_ns` + `pozlama_ns`; `t_cizim_ns` türetilmiş kolaylık alanı olarak kalır (çizim için gerekli) |
| Piksel biçimi tek ve bir kodla bildirilir | Uyumlu: `piksel_bicimi = 1` (`rgb8_srgb`) |
| Halkada yer gerçeği ya da fizik gerçeği montajı yok | Uyumlu: A2-K55 |
| `kare_seq` 1'den başlar ve kesin artar | Uyumlu hale getirildi: `kare_sirasi` sim süreci başlarken 1, her kamera periyodunda 1 artar (istek gönderilmese de), sıfırlamada sıfırlanmaz (A2-K49, §3.5) |
| Her yuvada yazım sayacı | Uyumlu: `yazim_sayaci` (u32, tanılama; A2-K53) |

## Başlık yeniden derlemesi ve şema yeniden denetimi

- `c++ -std=c++20 -fsyntax-only -Wall -Wextra -Wpedantic -x c++ sim/include/ida/a2_wire.h`: arm64 (yerel) ve `--target=x86_64-apple-macos13` ile uyarısız geçti. MSVC çalıştırılmadı.
- Yeni boyutlar: `KareHazir` 40, `KameraIstegi` 96, `SahneOlayi` 120, `HalkaEslendi` 16; `Hosgeldin` 600, `KareDustu` 24, `YuvaBasligi` 256 bayt olarak kaldı (dolgu alanları kullanıldı).
- `assets/ornek/manifest.json` şemadan 0 hatayla geçti; 14 bozuk türev reddedildi. Ek olarak CoACD ayrıştırmalı geçerli bir türev geçti ve 5 yeni bozuk türev (örtü toleransı yok ya da 0, `rv_k` eksik, `dmc_esigi` farklı, `yok` ayrıştırmada örtü toleransı) reddedildi. Denetleyici standart kitaplıkla yazılmış yapısal denetimdir; depoya girmedi.

## Bilinen çelişki (mimara)

İskelet §10 md.25 kamera kademe değerlerinin yerini A8 olarak yazıyor; mimar kararı bunları A2'ye aldı. A2 §6 bu kararı uygular ve çelişkiye atıf verir; iskeletin güncellenmesi mimarındır.
