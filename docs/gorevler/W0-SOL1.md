# W0-SOL1 — sol yazma denemesi: bellek ayırma denetim betiği

| | |
|---|---|
| Kimlik, dalga | `W0-SOL1`, W0 |
| Uygulayıcı | Codex `gpt-6.1-sol` (yazma kipi; bu depoda ilk deneme) |
| İnceleyen | `ida-inceleyici` (Opus) |
| Dal | `is/w0-sol-denemesi` |
| Tarih | 2026-11-15 |

## Amaç

İki amaç birlikte: (1) sol'un bu depoda görev belgesine uyarak kod yazabildiğini sınamak (09 §2.9); (2) ileride otomatik derlemede kullanılacak küçük bir araç üretmek: derlenmiş nesne dosyalarında dinamik bellek ayırma çağrısı arayan denetim betiği. Ortak çekirdek ve kart yazılımı dinamik bellek kullanmaz (bölüm 02 ve 01); bu betik o kuralı derleme çıktısı üzerinden denetler.

## Okunacaklar

- `AGENTS.md`
- Bu belge. Başka belge okumak gerekmez.

## Yapılacak

1. **`tools/denetim/bellek-ayirma.sh`** (bash; macOS'un bash 3.2'siyle çalışır):
   - Kullanım: `tools/denetim/bellek-ayirma.sh <nesne-ya-da-arşiv>...` (`.o` ve `.a`; ELF ve Mach-O).
   - Sembol listesini `nm` ile alır. Kullanılacak `nm`, `NM` ortam değişkeniyle verilir; verilmezse dosya ELF ise `PATH`'teki `arm-none-eabi-nm`, o yoksa ya da dosya Mach-O ise `nm` kullanılır.
   - Bir dosya aşağıdaki sembollerden birine **tanımsız başvuru** içeriyorsa (yani çağırıyorsa) denetim başarısızdır: `malloc`, `calloc`, `realloc`, `free`, `aligned_alloc`, `posix_memalign`, `memalign`, `valloc`, `strdup`, `strndup`, `sbrk`, `_sbrk`, newlib'in yeniden girilebilir karşılıkları (`_malloc_r`, `_calloc_r`, `_realloc_r`, `_free_r`, `_sbrk_r`) ve C++ `operator new` / `operator delete` ailesinin bozulmuş (mangled) adları (`_Znw*`, `_Zna*`, `_Zdl*`, `_Zda*`). Mach-O'daki baştaki ek alt çizgi hesaba katılır. Yalnız tam ad eşleşir: `my_malloc_count` ya da `free_list` gibi adlar yanlış alarm vermez.
   - Başarısızlıkta her bulgu için dosya (arşivde üye adıyla) ve sembol yazılır; bütün dosyalar taranır, ilk bulguda durulmaz.
   - Çıkış kodları: 0 temiz; 1 yasak başvuru bulundu; 64 kullanım hatası (argüman yok); 66 dosya yok ya da okunamıyor; 70 `nm` bulunamadı ya da çalıştırılamadı. Bir `nm` hatası "temiz" sayılmaz.
   - Betiğin başında, `tools/sol-incele.sh` üslubunda kısa bir Türkçe kullanım yorumu bulunur.
2. **`tools/denetim/test/`**:
   - `gecen.c`: dinamik bellek kullanmayan küçük bir C17 kaynağı; içinde yanlış alarm tuzağı olarak `free_list` ve `my_malloc_count` adlı kendi tanımladığı semboller bulunur.
   - `gecmeyen.c`: `malloc` ve `free` çağıran küçük bir C17 kaynağı (derleyicinin çağrıyı silmemesi sağlanır).
   - `test.sh`: iki kaynağı sistem derleyicisiyle (`cc`) nesne dosyasına derler (derleme çıktısı `tools/denetim/test/derleme/` altına; bu dizin `.gitignore`'a eklenir), bir de ikisini içeren arşiv üretir; betiği çalıştırıp şunları doğrular: `gecen.o` → 0; `gecmeyen.o` → 1 ve çıktıda `malloc` ile `free`; arşiv → 1 ve çıktıda üye adı; olmayan dosya → 66; argümansız → 64. `PATH`'te `arm-none-eabi-gcc` varsa aynı denetimler ELF nesneleriyle de yapılır; yoksa bu kısım "atlandı" diye yazılır (başarısız sayılmaz). Her denetim için `GEÇTİ`/`KALDI` satırı yazar; biri kalırsa 1 ile çıkar.
3. `test.sh`'yi çalıştır; çıktısını raporuna aynen koy.

## Dosya sahipliği

- Yazılır: `tools/denetim/**`, `.gitignore` (yalnız `tools/denetim/test/derleme/` satırı).
- Dokunulmaz: bunların dışındaki her şey.

## Yasaklar

- Commit atma, `git add` yapma, `git push` yapma, dal değiştirme. Değişiklikler çalışma dizininde kalır; commit'i kayıt agent'ı atar.
- Hiçbir şey kurma, indirme; ağ kullanma.
- Kişisel makine yolu yazma (betikler depo köküne ya da kendi konumuna göre çalışır). Dosya ve dizin adları ASCII.
- Python, Rust ya da başka dil yok: betik bash, örnekler C.
- Çalıştırmadığın bir testi "geçti" diye yazma.

## Rapor ve bitiş

Son iletin şu başlıklarla, Türkçe: **Yapılan** (dosya listesi), **Doğrulanan** (çalıştırdığın komutlar ve gerçek çıktıları; `test.sh` çıktısı aynen), **Doğrulanmayan**, **Görev belgesinde hatalı ya da belirsiz bulduğun nokta**. İş, `test.sh` gerçekten çalıştırılıp bütün zorunlu denetimler `GEÇTİ` olduğunda biter; biri kalıyorsa nedenini yaz, sonucu gizleme.

## Düzeltme turu (mimar, 2026-11-15; 1. tur)

İnceleme: `docs/gorevler/inceleme/W0-SOL1-inc.md` (`ida-inceleyici`; 1 kritik, 3 önemli, 4 küçük bulgu). Sekiz bulgunun hepsi uygulanır. Mimarın kararları:

- **ELF arşivi (md.1, md.2, md.6):** sistem `ar`'ına bağımlılık kaldırılır; arşivin biçimi arşiv başlığından belirlenir (GNU/SysV arşivi ELF sayılır ve varsa `arm-none-eabi-nm` seçilir; BSD arşivi Mach-O sayılır). Üye adları ve semboller `nm` çıktısından alınır.
- **Test (md.3, md.4, md.5):** stdout ve stderr ayrı yakalanır; bulgu satırının tamamı aranır; temiz üye için bulgu çıkmadığı ve başarılı koşuda stderr'in boş olduğu doğrulanır. Eklenecek örnekler: C++ (`new`, `delete`, `new[]`, `delete[]`; derleyici `c++`), zayıf `malloc` başvurusu, ELF tarafında `_malloc_r` ve `_sbrk` başvurusu, "eksik dosya + bulgulu dosya → 66". Bu örnekler için `tools/denetim/test/` altına yeni kaynak dosyaları eklenebilir (C++ örneği bu görevde serbesttir).
- **Çıkış kodu önceliği:** birden çok hata türü birlikte oluşursa `70 > 66 > 1` (sol'un seçimi kabul edildi); kullanım yorumuna yazılır.
- **`NM` (md.7):** boş `NM` verilmemiş sayılır; `NM` tek bir komut adı ya da yoludur (argümanlı değer desteklenmez); kullanım yorumuna yazılır.
- **Kullanım yorumu (md.8):** `tools/sol-incele.sh` biçiminde genişletilir.
- **Tanımsız tür süzgeci:** `nm -u` çıktısında bilinmeyen bir tür harfi görülürse sessizce atlanmaz; 70 ile bitirilir.
- Kapsam dışı (bu görevde yapılmaz): kendi ayırıcısını tanımlayan kodun yakalanması (bağlama sonrası ayrı bir denetimin işi).

Bu turda ARM araç zinciri `PATH`'e eklenmiş olarak çalışırsın; `test.sh`'nin ELF kısmı da koşmalı ve bütün denetimler `GEÇTİ` olmalı. Son raporunda `test.sh` çıktısını aynen ver ve her inceleme maddesi için "giderildi / kısmen / uygulanmadı" yaz.

## Denemede mimarın baktıkları (09 §2.9)

Görev belgesine ve dosya sahipliğine uyum; commit atmaması; ASCII ad ve göreli yol; testi gerçekten koşturup sonucu doğru raporlaması; `ida-inceleyici`'nin çıktıyı inceleyebilmesi. Deneme geçmezse sol'a ayrılan yazım işleri Opus'a verilir, sol yalnız inceler.
