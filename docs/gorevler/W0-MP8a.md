# W0-MP8a — Windows hedef bilgisayarı: donanım kaydı ve Godot kamera ön turu (betik hazırlığı)

| | |
|---|---|
| Kimlik, dalga | `W0-MP8a`, W0 (ölçüm prototipi) |
| Uygulayıcı | `ida-uygulayici` (Opus) |
| İnceleyen | sol (`tools/sol-incele.sh`; çağrıyı mimar başlatır) |
| Dal | `olcum/mp8-windows` |
| Tarih | 2026-11-19 |

## Amaç

Kullanıcının düşük donanımlı Windows bilgisayarında (8 GB bellek, dahili ekran kartı; K16) **kullanıcının kendisinin çalıştıracağı** bir ölçüm paketi hazırlamak. Paket iki şeyi ölçer: (1) makinenin donanımı (işlemci modeli dahil; bilinmiyor); (2) MP6a'da Mac'te geçen kamera yolunun (`force_draw(false, dt)` + `SubViewport` `UPDATE_ONCE` + eşzamanlı geri okuma) bu makinede çalışıp çalışmadığı ve ne kadar sürdüğü. Sonuç, kalite kademesi tabanının (iskelet §6, §8; Ö11; M12, M28, M30) ilk Windows girdisidir.

Bu iş **betik hazırlığıdır**: agent'ın Windows makinesi yoktur, paketi Windows'ta çalıştıramaz. Bu yüzden paket olabildiğince yalın, hataya dayanıklı ve kendi kendini açıklayan biçimde yazılır; çalıştırma ve sonuçların değerlendirilmesi ayrı bir adımdır (kullanıcı çalıştırır, çıktı klasörünü verir).

Kapsam dışı (sonraki işler): sonar hesap süresinin Windows'ta ölçümü (derleme gerektirir; otomatik derleme kurulunca), kart ve sanal seri port sürücüsü denemesi, web panelleri, 5 dakikadan uzun koşular.

## Okunacaklar

- `AGENTS.md`
- `docs/gorevler/W0-MP6a.md` (özellikle ölçütler ve "İlk turdan sonra" / "Tam kampanyadan sonra" başlıkları) ve `docs/olcumler/MP6-godot-kamera.md` (yöntem, pencere durumlarının tanımı, bilinen sınırlar)
- `tools/olcum/mp6/` (Godot projesi, `kos.sh`, `kampanya.sh`, `pencere.js`, `analiz.py`): Windows paketi bu projeyi **yeniden kullanır**; proje dosyaları kopyalanmaz, yerinden çağrılır
- `docs/mimari/07-gorsellestirme.md` §7'de P5'in Windows satırı (simge durumunda ve örtülü)

## Yapılacak

`tools/olcum/windows/` altında:

1. **`calistir.ps1`** (Windows PowerShell 5.1 ile çalışır; yönetici yetkisi istemez; hiçbir şey kurmaz, sistem ayarı değiştirmez):
   - **Donanım kaydı:** işlemci modeli, çekirdek/iş parçacığı sayısı, bellek, ekran kartı adı ve sürücü sürümü, ekran çözünürlüğü ve yenileme hızı, Windows sürümü, güç durumu (priz/pil) ve güç planı. Hepsi `donanim.txt` ve makinece okunur `donanim.json` dosyasına.
   - **Godot:** 4.7.2 resmî Windows ikilisini (standart, .NET değil; `Godot_v4.7.2-stable_win64.exe.zip`) resmî dağıtımdan kullanıcı profili altındaki bir önbellek dizinine indirir, özetini (SHA-512 ya da yayımlanan SHA-256) betikte sabit yazılmış değerle karşılaştırır; uyuşmazsa çalıştırmaz. Beklenen özet değerini birincil kaynaktan al ve kaynağını `BENIOKU.md`'ye yaz; doğrulayamazsan bunu açıkça belirt ve betikte özet denetimini "bilinmiyor, kullanıcıya göster ve onay iste" dalına bağla. İkili zaten indirilmişse yeniden indirmez.
   - **Kampanya (kısa):** yalnız yol (b) (`force_draw`). Durumlar: görünür (referans), simge durumunda, başka bir pencereyle tamamen örtülü. Her biri 120 saniye, 640 × 480, 30 Hz istek; ardından görünür pencerede 60 saniyelik geri okuma koşusu (eşzamanlı okuma). Toplam süre 10 dakikayı geçmez. Pencere durumunu olabildiğince Godot projesinin kendi olanaklarıyla kur (projede var olan kip ve örtü süreci); Windows'a özgü dış gözlem gerekiyorsa PowerShell içinden `user32` çağrılarıyla (simge durumu, görünürlük, ön plandaki pencere) yap ve kaydet; yapılamıyorsa o durumun dış doğrulaması "yok" diye kaydedilir, koşu yine yapılır.
   - **Ekran kartı etkisi:** Godot varsayılan (Forward+) ile başlar; açılmazsa (Vulkan yok/sürücü hatası) hata çıktısını kaydeder ve bir kez `--rendering-method mobile`, o da olmazsa `--rendering-driver opengl3` ile dener; hangi yolla çalıştığı sonuçta açıkça yazılır. Her deneme ayrı alt klasöre.
   - **Çıktı:** tek bir zaman damgalı klasör (masaüstünde ya da betiğin yanında): donanım kaydı, her koşunun kayıtları, Godot'nun standart çıktı ve hata günlükleri, betiğin kendi adım adım günlüğü. Kare dökümleri (PNG) yalnız seyrek örnekler (toplam boyut 50 MB'ı geçmez). Sonunda klasörü `.zip` yapar ve yolunu ekrana yazar.
   - **Dayanıklılık:** her adım `try/catch` içinde; bir adım başarısız olsa da sonrakiler denenir; hata tam iletisiyle günlüğe girer; betik hiçbir durumda sessizce "başarılı" demez. `-KuruCalistir` anahtarı hiçbir şey indirmeden ve Godot'yu başlatmadan adımları ve yolları yazar.
2. **`BENIOKU.md`**: kullanıcıya dönük, kısa, Türkçe: nasıl çalıştırılır (tek komut; PowerShell yürütme ilkesi engeline takılırsa ne yazılacağı), ne kadar sürer, o sırada bilgisayara dokunulmaması gerektiği (pencereler açılıp kapanır), prizde olması, çıktının nerede olduğu ve hangi dosyanın mimara verileceği, bir şey ters giderse ne yapılacağı.
3. **Çözümleme:** `tools/olcum/mp6/analiz.py` Windows çıktısını da okuyabilmeli; okuyamıyorsa `tools/olcum/windows/` altına küçük bir uyarlama (aynı dil) eklenir. `tools/olcum/mp6/` dosyaları değiştirilmez; Godot projesinde Windows için değişiklik zorunluysa uygulamadan önce dur ve bildir.
4. **Mac'te yapılabilen doğrulama:** betiğin sözdizimi (PowerShell çekirdeği Homebrew ile kurulabilir: `brew install powershell`; kurulum serbest) ve `-KuruCalistir` yolu Mac'te `pwsh` ile koşturulur; Windows'a özgü çağrılar bu koşuda atlanır ve "Windows'ta sınanmadı" diye listelenir. Godot projesinin Windows paketi tarafından kullanılan komut satırı (aynı argümanlarla) Mac'teki Godot ile 20 saniyelik bir koşuda denenir.
5. **Rapor taslağı:** `docs/olcumler/MP8-windows.md`: yöntem, paketin içeriği, Mac'te doğrulananlar, **Windows'ta sınanmayanların tam listesi**, ölçütler ve sonuçlar için boş bölümler (kullanıcı çalıştırınca doldurulur).

## Geçme ölçütleri

Bu işin (betik hazırlığının) bitiş ölçütleri:

| # | Ölçüt |
|---|---|
| W1 | `calistir.ps1` `pwsh` ile sözdizimi hatasız ayrışır; `-KuruCalistir` Mac'te uçtan uca koşar ve bütün adımları listeler |
| W2 | Paketin kullandığı Godot komut satırı Mac'te 20 saniyelik koşuda kare üretir (yol b) |
| W3 | `BENIOKU.md` tek komutla çalıştırmayı ve çıktının yerini eksiksiz anlatır |
| W4 | Windows'ta sınanmayan her şey raporda madde madde yazılıdır |

Windows'taki ölçümün kendi ölçütleri (kullanıcı çalıştırınca değerlendirilir; bu görev belgesi dondurur, hedeftir): **H1** donanım kaydı eksiksiz (işlemci modeli dahil); **H2** görünür, simge durumunda ve örtülü durumların her birinde yol (b) ile 120 saniyede düşen kare < %1 ve kare kimliği kesintisiz; **H3** simge durumunda ve örtülü kareler görünür referansla ortalama mutlak fark ≤ 2/255; **H4** 640 × 480 eşzamanlı geri okuma süresi p50/p99 ve gerçekleşen kare hızı raporlanır (eşik yok; kademe tabanı kararının girdisi); **H5** hangi çizim yöntemiyle (Forward+ / Mobile / uyumluluk) çalıştığı kayıtlı. H2 ya da H3 geçmezse "geçmedi" geçerli sonuçtur.

### Düzeltme turu (mimar, 2026-11-19; 1. tur)

sol incelemesi: `docs/olcumler/inceleme/MP8-sol.md` (dal `olcum/mp8-windows`; 8 kritik, 1 önemli). Dokuz bulgunun hepsi uygulanır. Mimarın kararları:

- **Süreç sahipliği (md.1):** betik yalnız kendi başlattığı süreçleri (kaydettiği süreç kimlikleriyle) kapatır; yürütülebilir dosya yolu sahiplik sayılmaz. `-KuruCalistir` hiçbir sürece dokunmaz.
- **Çözümleme hükümleri (md.2, md.3, md.6, md.7):** hükümden önce deney koşulları doğrulanır (H2: yol b, 120 s, 640 × 480, 30 Hz, 3600 istek; H4: eşzamanlı okuma yolu, dolu p50/p99); deneme parametreleriyle alınmış kayıt kabul ölçümü sayılmaz. Dış gözlemdeki hata satırları ve zaman boşlukları hükme girer; kapsam yetersizse hüküm "doğrulanmadı"dır. Geçersiz görünür referans çözümlemeyi durdurmaz: H3 "değerlendirilemedi" yazılır, diğer koşular çözümlenir. H1 alanları anlamsal doğrulanır (iki çözünürlük boyutu, bilinen güç durumu, gerçek yenileme hızı, başarılı güç planı sorgusu).
- **Örtülme (md.4):** tam örtülme, ölçüm penceresinin dikdörtgeninin örtü penceresinin dikdörtgeni içinde kaldığı ve örtünün pencere sırasında üstte olduğu, aynı koordinat düzeninde (DPI farkındalığı açıkça ayarlanarak) doğrulanırsa kabul edilir; beş nokta denetimi ek gözlem olarak kalır. Geometri doğrulanamıyorsa durum "doğrulanmadı" yazılır, H2'nin örtülü satırı "geçti" sayılmaz.
- **Süreç tamamlanması (md.5):** özetin varlığı ayrı, sürecin temiz bitişi ayrı denetlenir (hata iletisi yok, zaman aşımı yok, çıkış kodu 0); "ölçüt geçmedi" ile "düzenek çalışmadı" ayrı kaydedilir.
- **Gizlilik (md.9):** başka uygulamaların pencere başlıkları kaydedilmez; ön plandaki pencerenin yalnız bu çalıştırmanın süreçlerine ait olup olmadığı ve pencere sınıf adı kaydedilir. `BENIOKU.md` çıktının içeriğini doğru anlatır.
- **Toplam süre:** betiğe küresel süre sınırı eklenir (15 dakika); aşılırsa kalan adımlar "atlandı" yazılır.
- **Hazırlık ölçütleri (md.8):** W2, gerçek argümanlarla 20 saniyelik ölçüm koşusuyla yinelenir. **W1 bu ortamda tamamlanamaz:** `pwsh` çağrısı ortam tarafından reddedilmiştir; hiçbir dolaylı yolla çalıştırılmaz (ilk turdaki Python başlatıcısı yolu kullanılmamalıydı). W1 raporda "tamamlanmadı: betiğin ayrıştırılması ve kuru çalıştırması kullanıcı ya da mimar tarafından yapılacak" diye yazılır ve gereken komutlar tam olarak verilir. Betik bu yüzden özellikle dikkatli, yalın ve satır satır gözden geçirilerek düzenlenir.
- `docs/olcumler/inceleme/MP8-yanit.md` yazılır (madde → giderildi / kısmen / uygulanmadı, yer, gerekçe).

## Dosya sahipliği

- Yazılır: `tools/olcum/windows/**`, `docs/olcumler/MP8-windows.md`.
- Dokunulmaz: `tools/olcum/mp6/**` (çağrılır), `docs/mimari/**`, `docs/research/**`, `docs/DURUM.md`, `AGENTS.md`, `README.md`, `docs/gorevler/**`, `.gitignore`.

## Yasaklar

- Betik yönetici yetkisi istemez; kayıt defterine, güç planına, ekran koruyucuya, sürücülere dokunmaz; Godot dışında hiçbir şey indirmez; indirdiğini yalnız kullanıcı profili altındaki kendi önbellek dizinine koyar.
- Ölçülmemiş rakamı sonuç gibi yazma; Windows için süre ya da kare hızı tahmini yazma.
- Commit atma, `git add` yapma, `git push` yapma, başka agent başlatma. Dalı (`git switch -c olcum/mp8-windows`) oluşturabilirsin.

## Rapor ve bitiş

Son iletinde kısa: **yapılan**, **Mac'te doğrulanan** (komut ve gerçek çıktı özetiyle), **Windows'ta sınanmayan**, **çalışma dizininin yolu ve dal adı**, görev belgesinde hatalı bulduğun nokta. Düzeltme turu sınırı iki.
