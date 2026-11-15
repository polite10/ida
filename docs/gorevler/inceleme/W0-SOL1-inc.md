# Claude incelemesi: W0-SOL1

## Hüküm

**Düzeltme gerekli.**

Görev belgesine uyum ve raporlama açısından sol'un yazma denemesi olumlu:
- Yalnız `tools/denetim/**` dosyaları ve `.gitignore`'a tek satır değişmiş.
- Commit atılmamış; HEAD hâlâ d473b81.
- Kişisel makine yolu yok, dosya adları ASCII.
- Betik bash 3.2 ile çalışıyor.
- Yerel (Mach-O) testlerin hepsi gerçekten `GEÇTİ`.
- ELF kısmı, ARM derleyicisi PATH'te olmadığı için dürüstçe "ATLANDI" diye raporlanmış. sol'un raporundaki `test.sh` çıktısı benim çalıştırdığımla birebir aynı.

Buna karşın betiğin asıl hedef biçimi olan **ELF arşivinde (kart yazılımının `.a` kütüphaneleri)** iki sorun var:
- Denetim yanlış sonuç veriyor: bulgu 1 yerine 70 dönüyor ve yasak başvurular yazılmıyor.
- `nm` seçimi görev belgesine uymuyor.

Bunun üstüne test, bu hatayı kısmen gizleyecek biçimde yazılmış. Hata çıktısını bulgu sanıp iki içerik denetimine `GEÇTİ` diyor.

Mimarın ön gözlemi doğru; kök nedeni aşağıda.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `tools/denetim/bellek-ayirma.sh:21`, `:105-108` | **ELF arşivi denetlenemiyor.** Kök neden: üyenin sihirli baytlarını okumak için sabit kodlu sistem `ar`'ı (`/usr/bin/ar`, BSD/cctools) kullanılıyor. Bu `ar`, GNU/SysV arşiv biçimini anlamıyor. `arm-none-eabi-ar` ile üretilen arşivde `ar t` şunları listeliyor: `/`, `gecen.o/`, `gecmeyen.o/`. Sembol indeksi üye sanılıyor, üye adlarının sonuna `/` ekleniyor. Bu yüzden `ar p ornek.a gecmeyen.o` "not found in archive" verip düşüyor. Çıkış 70 oluyor (beklenen 1) ve o satırdaki `malloc`/`free` bulguları hiç yazılmıyor. Sorun `NM=arm-none-eabi-nm` verildiğinde de, ARM araç zinciri PATH'te yokken de aynı. | Üye başına `ar p` çağrısını kaldırın. Biçimi arşiv başlığından belirleyin: 8. bayttaki ilk üye adı `/` ya da `//` ise ya da adlar `/` ile bitiyorsa GNU/SysV arşivi → ELF (alt çizgi soyulmaz). `__.SYMDEF` ya da `#1/` ise BSD arşivi → Mach-O. `ar` kullanılacaksa `nm` ile aynı aileden seçilmeli (`arm-none-eabi-ar` ya da bir `AR` değişkeni). Ancak ARM araç zinciri yokken bu da yetmez; başlık ayrıştırma daha sağlam. | kritik |
| 2 | `tools/denetim/bellek-ayirma.sh:23`, `:66-81` | **ELF arşivinde yanlış `nm` seçiliyor.** Arşivin "türü", `ar p arşiv` çıktısının ilk 8 baytından okunuyor. GNU arşivinde BSD `ar` önce sembol indeksini döküyor; okunan bayt `00 00 00 04 00 00 00 88` oluyor, ELF sihri `7f454c46` değil. Sonuç: ARM araç zinciri PATH'teyken bile `nm_araci=nm` (sistem llvm-nm) seçiliyor (`bash -x` ile doğrulandı). Görev belgesi "dosya ELF ise `arm-none-eabi-nm`" diyor. Satır 16-17'deki "ar ... sembol indeksini atarak" yorumu bu durumda yanlış. | #1'deki başlık tabanlı biçim tespitini `nm` seçiminde de kullanın: GNU arşivi ELF sayılsın ve `arm-none-eabi-nm` (varsa) seçilsin. Yorumu düzeltin. | önemli |
| 3 | `tools/denetim/test/test.sh:18`, `:57`, `:59-60` | **Test, hata iletisini bulgu sayıyor.** `dogrula` stdout ile stderr'i birleştiriyor (`2>&1`). `icerik_dogrula` ise yalnız alt dize arıyor (`ornek.a[gecmeyen.o]`, `$cikti/gecmeyen.o:`). ELF koşusunda hiçbir bulgu yazılmadığı halde `GEÇTİ: ELF çıktıda arşiv üyesi` ve `GEÇTİ: ELF sonraki girdide bulgu` çıktı; bu denetimleri hata satırı `...ornek.a[gecmeyen.o]: arşiv üyesi okunamadı (ar)` geçirdi. Test iddia ettiğini sınamıyor. | stdout ile stderr'i ayrı yakalayın. Bulgu satırını tam arayın: `<arşiv>[gecmeyen.o]: yasak tanımsız başvuru: malloc` (Mach-O'da `_malloc`). Stderr'in boş olduğunu ve `[gecen.o]` için bulgu çıkmadığını da doğrulayın. | önemli |
| 4 | `tools/denetim/test/test.sh:54-55` | "çıktıda malloc/free" denetimi yalnız `malloc` / `free` alt dizesini arıyor. Hata iletisi ya da yol bu alt dizeleri içerse de geçer. | #3'teki gibi tam bulgu satırı aransın. | küçük |
| 5 | `tools/denetim/test/test.sh` (kapsam) | Yasak listesinin yalnız 2 sembolü sınanıyor (`malloc`, `free`). Şunlar hiç sınanmıyor: C++ `_Znw*/_Zna*/_Zdl*/_Zda*` (Mach-O'da `__Z...`), newlib `_r` aileleri ile `_sbrk`, zayıf (`w`) tanımsız başvuru, `70 > 66 > 1` önceliği ve okunamayan (izinsiz) dosya. sol bunu raporunda "Doğrulanmayan" altında kendisi yazmış. Ben elle sınadım, betik bunlarda doğru çalışıyor (aşağıda). Ama otomatik derlemede kalıcı güvence yok. | En az bir C++ örneği (`new`/`delete`/`new[]`/`delete[]`) eklensin. ELF tarafında bir `_malloc_r`/`_sbrk` örneği ile bir zayıf `malloc` örneği eklensin. Bir de "eksik dosya + bulgu → 66" denetimi eklensin. | önemli |
| 6 | `tools/denetim/bellek-ayirma.sh:105-108` | Üye okunamadığında `onceki_konum` güncellenmiyor. Aynı üyenin her sembol satırı için `ar` yeniden çağrılıyor ve hata iletisi tekrarlanıyor (ELF koşusunda 2 kez). O satırın sembolü de değerlendirilmeden atlanıyor. | #1 düzeltilince büyük ölçüde ortadan kalkar. Hata yolu kalacaksa `onceki_konum` hata durumunda da güncellensin; sembol yine de değerlendirilip bulgu yazılsın. | küçük |
| 7 | `tools/denetim/bellek-ayirma.sh:75-76` | `NM=` (boş) verilince varsayılan seçime düşülmüyor; çıkış 70, ileti "nm bulunamadı: " oluyor. Argümanlı değer de çalışmıyor (`NM="xcrun nm"` → 70). | Boş `NM` "verilmemiş" sayılsın (`[ -n "${NM-}" ]`). Argümanlı `NM` desteklenmiyorsa bu kullanım yorumunda belirtilsin. | küçük |
| 8 | `tools/denetim/bellek-ayirma.sh:1-5` | Kullanım yorumu, `tools/sol-incele.sh` üslubuna göre çok kısa. `NM` verilmediğinde nasıl seçim yapıldığı (ELF → `arm-none-eabi-nm`), yasak sembol listesinin özeti ve "nm hatası temiz sayılmaz" kuralı yazılmamış. | Kullanım satırı, argüman açıklaması, `NM` seçim kuralı ve çıkış kodları ayrı satırlarla, `sol-incele.sh` biçiminde yazılsın. | küçük |

## Doğrulananlar

Hepsi çalışma dizininde (`.claude/worktrees/sol-w0-denemesi`) çalıştırıldı. Elle yapılan uç durum denemeleri yalnız oturumun scratchpad dizinine yazıldı.

- `/bin/bash -n tools/denetim/bellek-ayirma.sh tools/denetim/test/test.sh` → çıkış 0. `/bin/bash --version` → 3.2.57.
- `/bin/bash tools/denetim/test/test.sh` (ARM araç zinciri PATH'te değil) → 14 `GEÇTİ`, `ATLANDI: ELF denetimleri ...`, çıkış 0. sol'un raporundaki çıktıyla birebir aynı.
- ARM araç zinciri PATH'e eklenerek (`PATH="$ARM:$PATH" /bin/bash tools/denetim/test/test.sh`) → çıkış 1. Yerel denetimlerin hepsi `GEÇTİ`.
  - ELF'te geçenler: `.o` denetimleri, 66, 64, 70.
  - ELF'te kalanlar: `KALDI: ELF arşiv -> 1 (beklenen çıkış 1, gerçek 70)` ve `KALDI: ELF birden çok girdi -> 1 (... gerçek 70)`. İletiler `ar: gecmeyen.o: not found in archive` ve `...ornek.a[gecmeyen.o]: arşiv üyesi okunamadı (ar)`; her biri iki kez basılıyor.
  - Buna karşın `GEÇTİ: ELF çıktıda arşiv üyesi` ve `GEÇTİ: ELF sonraki girdide bulgu` çıktı (bkz. #3).
- Kök neden incelemesi (`tools/denetim/test/derleme/elf/ornek.a`):
  - `ar t` (sistem): `/`, `gecen.o/`, `gecmeyen.o/`. `arm-none-eabi-ar t`: `gecen.o`, `gecmeyen.o`.
  - `ar p ornek.a | od -An -tx1 -N8` → `00 00 00 04 00 00 00 88`, yani GNU sembol indeksi.
  - `nm -A -P -u` (llvm-nm) ve `arm-none-eabi-nm -A -P -u` ikisi de `ornek.a[gecmeyen.o]: free U` / `malloc U` veriyor. `nm` tarafında sorun yok; sorun yalnız `ar`'da.
  - `PATH="$ARM:$PATH" bash -x bellek-ayirma.sh .../elf/ornek.a` → `nm_araci=nm`. Aynı denemede `.../elf/gecen.o` → `nm_araci=arm-none-eabi-nm`.
  - `NM=<arm-none-eabi-nm> bellek-ayirma.sh .../elf/ornek.a` → yine 70.
- Elle uç durumlar (scratchpad'de):
  - C++ Mach-O nesnesi: `__ZdaPv`, `__ZdlPv`, `__Znam`, `__Znwm` → 4 bulgu, çıkış 1. ELF (`arm-none-eabi-g++`): `_ZdaPv`, `_ZdlPvj`, `_Znaj`, `_Znwj` → çıkış 1.
  - `calloc/realloc/strdup/strndup/aligned_alloc/posix_memalign` → Mach-O'da 6 bulgu (`_` önekli), ELF'te 6 bulgu, çıkış 1.
  - ELF'te `_malloc_r U`, `_sbrk U` ve zayıf `malloc w` → 3 bulgu, çıkış 1. Mach-O'da `__malloc_r`, `__sbrk`, `_malloc` → 3 bulgu. Mach-O'da `weak_import` → `_malloc U` olarak yakalanıyor.
  - Boşluk ve iki nokta içeren, `-` ile başlayan yol (`-gec: meyen.o`) → `./-gec: meyen.o: ...` bulguları, çıkış 1.
  - Aynı adlı iki üye (`ar q`, biri temiz biri değil) → `dup.a[a.o]` bulguları, çıkış 1. Bulgu kaçmıyor, ancak hangi `a.o` olduğu ayırt edilemiyor.
  - 15 karakterden uzun üye adı (Mach-O) ve `libtool -static` arşivi → doğru bulgu, çıkış 1.
  - Dizin argümanı → 66. Metin dosyası ve boş dosya → `nm` hatası, çıkış 70; "temiz" sayılmıyor.
  - Eksik dosya + bulgulu dosya → bulgular yazılıyor, çıkış 66 (sol'un `70 > 66 > 1` önceliğine uygun).
  - `NM=` → 70; `NM="xcrun nm"` → 70.
  - Yanlış alarm tuzakları (`free_list`, `my_malloc_count`) Mach-O'da ve ELF'te `gecen.o` → 0.
- Dosya sahipliği ve yasaklar:
  - `git status --short` → ` M .gitignore`, `?? docs/gorevler/W0-SOL1.md` (mimarın kopyası), `?? tools/denetim/`. `--ignored` → yalnız `tools/denetim/test/derleme/`.
  - `git diff .gitignore` → tek satır eklenmiş: `tools/denetim/test/derleme/`.
  - `git log --oneline -1` → `d473b81`. Dal `is/w0-sol-denemesi`.
  - `grep` ile `/Users/`, kullanıcı adı ve `/Applications` aranınca bulunmadı. `tools/denetim` altında ASCII dışı dosya adı yok.
  - İki betiğin de yürütme izni var.

## Notlar

- **Doğrulanmayan:** Betik Windows'ta (Git Bash) denenmedi. `ar`/`od`'un orada bulunup bulunmayacağı bilinmiyor. Görev belgesi bunu istemiyor.
- **Kapsam dışı kör nokta:** `malloc`'u kendisi tanımlayan bir çeviri birimi (`T malloc`) yakalanmıyor; denememde çıkış 0. Görev belgesi yalnız "tanımsız başvuru" dediği için bu sol'un hatası değil. Kendi ayırıcısını yazan kod bu denetimden kaçar; bağlama (link) sonrası ELF üzerinde ayrı bir denetimle kapatılabilir. Mimarın kararına bırakıyorum.
- **Görev belgesine bağlı bir sınırlama:** `nm` hata verdiğinde (geçersiz ya da boş nesne) çıkış 70 oluyor; 66 yalnız dosya yokken ya da okunamıyorken veriliyor. Bu görev belgesiyle tutarlı, ama belgede açık değil.
- **Fazla dar tür süzgeci:** `-u` zaten yalnız tanımsız sembolleri verdiği için `satir_deseni`'ndeki `[Uwv]` süzgeci gereksiz derecede dar. Bugünkü araçlarda bir sembolün kaçtığını görmedim; ama `nm` başka bir tanımsız tür harfi basarsa o başvuru sessizce atlanır. İleride düzeltilecekse süzgeç kaldırılabilir ya da bilinmeyen harf görülünce 70 dönülebilir.
- **sol'un raporu:** Gerçekle uyuşuyor. "Doğrulanmayan" bölümü dürüst: ELF atlandı, diğer yasak API'ler ve C++ ayrı örnekle sınanmadı. Rapor, ELF arşiv yolunun sistem `ar`'ına bağımlı olduğunu söylemiyor; ancak ELF kısmı koşulmadığı için bunu görmesi beklenemezdi. "Belirsiz nokta" olarak `70 > 66 > 1` önceliğini bildirmesi yerinde. Bunu görev belgesine eklemek mimarın kararı.
- **Deneme değerlendirmesi (09 §2.9):** Uyum ve dürüstlük ölçütleri karşılandı. Kod doğruluğunda, yerelde koşturulamayan asıl hedef biçimde (ELF arşivi) kritik bir hata ve onu gizleyen bir test zayıflığı var. Deneme "geçti" sayılacaksa #1-#3 düzeltilip ARM araç zinciri PATH'teyken `test.sh` yeniden koşturulmalı.
