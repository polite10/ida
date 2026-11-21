# W2-depo — Depo iskeleti, derleme sistemi ve otomatik derleme

Tarih: 2026-11-20. **Başlatıldı (2026-11-20):** dondurma sol doğrulama incelemesini beklerken iş bütünleştirme dalı (`sozlesme/w1-butunlesme`) üzerinde başlatıldı; sözleşme başlıklarında çıkan derleme sorunu dondurmadan önce düzeltilebilsin diye. Dondurmadan sonra dal `main`'e göre yeniden tabanlanır. Yazan: `ida-uygulayici` (Opus; K18). İnceleyen: sol. Dal: `is/w2-depo`. Kayıt: `ida-kayit`.

Bu iş W2'nin ilk işidir ve ürün kodu yazmaz: sonraki bütün işlerin içine yerleşeceği dizinleri, derleme ön ayarlarını, bağımlılık düzenini ve her pull request'te koşan denetimleri kurar. Başvuru: `docs/mimari/00-iskelet.md` §7 (depo düzeni), `docs/mimari/09-test-ve-is-akisi.md` §2.6–§2.8 (araç zinciri, dal, otomatik derleme). 09 ile bu belge çelişirse bu belge geçerlidir; aşağıdaki "09'dan farklar" bölümü bilinen farkları sayar.

## 09'dan farklar (sonradan verilen kararlar)

- **USB yığını yok (K17).** TinyUSB, USBX ve kart tarafı USB kodu depoya girmez; A1 yalnız ST-LINK sanal seri portudur (USART1).
- **libc newlib-nano'dur.** Picolibc bu makinedeki GNU Tools for STM32 14.3.rel1 paketinde yok (MP1 raporu); derleyici, libc ve bağlayıcı ayarı birlikte sabitlenir.
- **Kontrol hızı 200 Hz**, 400 Hz değil (iskelet 0.8+); bu iş hız sabiti tanımlamaz, yalnız bilgi.
- **Eşzamanlı agent sınırı yok**; 09 §2.5'teki "en fazla iki" artık geçerli değil.
- **Kod yazımı Opus'ta (K18).** 09 §2.9 tablosundaki yazar atamaları geçersiz; inceleme sol'da.
- **Kota düzeni (mimar kararı, 2026-11-20; işin kota ölçümünden sonra).** Hesap GitHub Free planındadır (aylık dahil kullanım 12 USD; macOS dakikası Windows'un altı katı; dal koruma bu planda yok, mimar denetimi tek kapıdır). Bu yüzden: Windows işi her PR gönderiminde koşar; macOS işi yalnız PR'a `ci-mac` etiketi eklendiğinde ya da elle tetiklenince koşar (her iş zaten Mac'te yerelde derlenip test edilir; kayıt agent'ı birleştirmeden önce etiketi bir kez ekler); belge ve depo denetimi Linux koşucusunda koşar. Linux koşucusu yalnız Python betiği koşturur: ürün derlemesi değildir, iskelet §7'deki "ek bir Linux işi kurulmaz" cümlesi ürün derlemesi içindir (iskelette açıkça yazılacak). Aşağıdaki §5 ve kabul md.5 bu düzenle okunur.
- **İnceleme kayıtları değiştirilemez kayıttır (mimar kararı, 2026-11-20).** `docs/**/inceleme/` altındaki dosyalardaki çözülmeyen bağlantılar belge denetiminde bulgu değil uyarıdır (`docs/research/dogrulama/` ile aynı); kişisel yol denetimi bu dosyalarda da geçerlidir.

## Kapsam

### 1. Dizin iskeleti

İskelet §7'deki dizinler, her birinde amacını tek paragrafta söyleyen kısa bir `BENIOKU.md` ile (kök `README.md` güncellenir). Var olan içerik taşınmaz: `core/include/ida/`, `sim/include/ida/`, `protocol/ida.xml`, `scenarios/`, `assets/` sözleşmelerden gelir ve bu iş onları **değiştirmez**. `tools/olcum/` W0 prototipleridir; derleme sistemine bağlanmaz.

### 2. Derleme sistemi

- Kökte `CMakeLists.txt` ve `CMakePresets.json`; ön ayarlar: `host-debug`, `host-asan`, `host-release`, `fw-debug`, `fw-release`, `fw-measure`. CMake alt sınırı ve üreteç (Ninja) sabit.
- **Tek depo, iki kök** (09 §2.6): `core/` kendi hedefidir (`ida_core`; statik, ISO C17, `-pedantic-errors`, uzantı yok, işletim sistemi bağımlılığı yok) ve aynı `core/CMakeLists.txt` hem PC kökünden hem firmware kökünden alınır. Çekirdek hedefinin include yoluna `stm32*`, CMSIS ya da RTOS başlığı giremez; bunu CMake zorlar ve zorladığı bir deneme hedefiyle gösterilir.
- Firmware kökü: `cmake/toolchains/arm-none-eabi-cm33.cmake` (Cortex-M33, tek duyarlıklı donanım FPU, newlib-nano); araç zincirinin yolu ortam değişkeniyle verilir, kişisel yol dosyalara yazılmaz. Bu işte firmware hedefi yalnız **bağlanabilir en küçük yer tutucudur** (başlangıç dosyası + boş `main`); kart destek katmanı, saat kurulumu ve sürücüler W3 kart yazılımı işinindir. ST kütüphaneleri bu işte depoya alınmaz; yalnız alınacakları yer (`firmware/third_party/`) ve sabitleme yöntemi (alt modül, commit) belgelenir.
- Uyarı politikası (09 §2.6, §2.8): GCC/Clang `-Wall -Wextra -Wconversion -Wdouble-promotion -Wfloat-conversion`, uyarı hatadır; MSVC `/W4 /permissive-`, uyarı hatadır. Kayan nokta politikası: `-ffp-contract=off -fno-fast-math`; MSVC `/fp:precise`. PC tarafı C++20, `CXX_EXTENSIONS OFF`.
- **Sözleşme başlıkları derleme denetimi** (bu işin en somut ürünü): `core/include/ida/port.h` ve `core/include/ida/algilama_kaydi.h` C17 olarak (host Clang, MSVC, arm-none-eabi-gcc) ve C++20 olarak; `sim/include/ida/a2_wire.h` C++20 olarak (Clang, MSVC) uyarısız derlenir; başlıklardaki `static_assert` yerleşim denetimleri üç derleyicide de geçer. Bugüne kadar bu başlıklar MSVC ile hiç derlenmedi ve `port.h` gerçek `algilama_kaydi.h` ile değil yer tutucuyla derlendi; bu denetim o açığı kapatır. Başlıkta derleme hatası çıkarsa **başlığı düzeltme**: hatayı raporla (dondurulmuş sözleşmede değişiklik ayrı iştir). `protocol/ida.xml`'den üretilen MAVLink başlığı ile `port.h`'nin aynı çeviri biriminde derlenmesi de bu denetimin parçasıdır (W1-uyum U15: tel enumları `IDAW_`, port sabitleri `IDA_` önekinde); üreteç bu işte kurulamıyorsa denetim protokol kütüphanesi işine devredilir ve bu açıkça raporlanır.

### 3. Bağımlılık düzeni

- `vcpkg.json` (`builtin-baseline` sabit; `overrides` ile sürüm) ve küçük başlık-yalnız bağımlılıklar için CPM (commit ya da `URL_HASH` sabit). Hareketli etiket yok.
- Bu işte düzeneğin iki işletim sisteminde çalıştığını göstermek için **tek** gerçek bağımlılık çözülür ve küçük bir duman hedefinde kullanılır (öneri: Eigen; başka bir seçim gerekçesiyle yazılır). Embree, Jolt, MCAP, zstd ve ötekiler kendi işlerinde eklenir.
- C birim test düzeneği: Unity (commit sabit); `ida_core` için tek bir örnek test (boş çekirdekte derleme ve koşma zincirini gösteren) CTest'e bağlanır. Aynı testin firmware araç zinciriyle **derlenebildiği** gösterilir (kartta koşturma bu işte yok).

### 4. Denetim betikleri (`tools/denetim/`)

- **Belge ve depo denetimi:** ham raporlarda (`docs/research/NN-*.md`) ve doğrulamalarda diff yok (taban: `main`); depoya kişisel makine yolu girmemiş; belgelerdeki göreli bağlantılar çözülüyor; Rust ya da ardıl kart bağımlılığı yok. Her denetimin bilerek bozulmuş girdiyi reddettiğini gösteren kendi testi olur.
- **Bellek ayırma yasağı:** var olan `tools/denetim/bellek-ayirma.sh` (W0-SOL1; sol yazdı) `ida_core` nesnelerine ve firmware bağlamasına bağlanır; betiğin kendisi değiştirilmez, değişiklik gerekiyorsa raporlanır. Firmware bağlamasında `--wrap` ile ayırma simgeleri tanımsız bırakılır (boş tanım değil).
- **Statik analiz:** clang-tidy (`bugprone-*`, `cert-*`, `clang-analyzer-*`) ve cppcheck `core/` üzerinde; GCC `-fanalyzer` firmware derlemesinde; MSVC `/analyze`. Boş çekirdekte gürültü ve süre kaydedilir (09 §8: yığın kararı bu kayda bağlı).

### 5. Otomatik derleme (`.github/workflows/`)

- Her pull request'te macOS arm64 ve Windows x64: host derleme, birim testi, sözleşme başlıkları denetimi, kartsız firmware derlemesi (harita ve boyut raporu artefakt), bellek ayırma denetimi, sanitizer (`host-asan`), statik analiz; yalnız macOS'ta belge ve depo denetimi.
- Koşucu etiketleri açık sürümlüdür (`*-latest` yok); eylemler commit SHA ile sabitlenir; derleyici sürümleri günlüğe yazılır.
- Yalnız belge değiştiren PR derleme işlerini atlar; aynı dalda yeni gönderim eski koşuyu iptal eder.
- **Önce ölç, sonra kur:** depo özeldir ve dakika kotası doğrulanmadı (09 §8.2 md.4). İşin ilk adımı şunları birincil kaynaktan ve `gh` ile kayda geçirmektir: hesabın planı ve aylık dakika kotası, macOS ve Windows koşucularının dakika çarpanı, GNU Tools for STM32 / Arm GNU 14.3.rel1'in koşucuda kurulabilirliği (indirme adresi, boyut, lisans, önbellekleme), dal koruma kuralının bu planda kullanılabilirliği. Bir PR koşusunun ölçülen süresi ve aylık kaç PR'a yettiği rapora yazılır. Kota bir PR için bile yetmiyorsa iş durur ve mimara bildirir; kotayı aşacak düzen kurulmaz, ücretli seçenek önerilmez.

## Kapsam dışı

Ürün kodu (fizik, kestirim, kart sürücüleri, protokol kütüphanesi, sim sunucusu, Godot projesi, web panelleri); `docs/mimari/`, `docs/sozlesmeler/`, `docs/research/` ve sözleşme başlıklarının içeriği; kartta çalıştırma ve yükleme; Linux işi; karta bağlı kendi kendine barındırılan koşucu; Rust tabanlı araç; ardıl kart ya da başka mikrodenetleyici için ön ayar.

## Kabul

1. Temiz bir kopyada `cmake --preset host-debug` ve derleme, macOS'ta yerelde ve CI'da iki işletim sisteminde uyarısız geçer; örnek birim testi koşar.
2. `fw-debug` ve `fw-release` kartsız bağlanır; harita ve boyut raporu üretilir; çekirdek nesnelerinde ve firmware bağlamasında ayırma simgesi yoktur ve `malloc` çağıran deneme nesnesi denetimce yakalanır.
3. Sözleşme başlıkları denetimi beş derleyici/dil birleşiminde (C17: Apple Clang, MSVC, Arm GCC; C++20: Apple Clang, MSVC) geçer; geçmeyen varsa başlığa dokunulmadan hata ayrıntısıyla raporlanmıştır.
4. Belge denetimi bilerek değiştirilmiş bir ham raporu ve kişisel yol içeren bir dosyayı reddeder; çekirdeğe `stm32` başlığı eklemeye çalışan deneme hedefi derlenmez.
5. CI bir PR'da yukarıdaki işleri iki işletim sisteminde yeşil koşturur; koşu süresi ve kota hesabı raporda.
6. Depoda kişisel makine yolu, hareketli sürüm etiketi, Rust bağımlılığı yoktur.

## Rapor

Yazılan dosyalar; her kabul maddesinin sonucu (komut ve çıktı özetiyle); sabitlenen sürümlerin tablosu (araç, sürüm ya da commit, kaynağı); CI ölçümleri (süre, dakika, kota); doğrulanamayanlar; 09 ya da iskeletle görülen çelişki; çalışma dizini ve dal. Commit atılmaz; kayıt agent'ı atar.
