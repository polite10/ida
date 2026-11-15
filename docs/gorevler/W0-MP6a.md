# W0-MP6a — Godot'da görünmeyen pencerede kamera görüntüsü (Mac ön turu)

| | |
|---|---|
| Kimlik, dalga | `W0-MP6a`, W0 (ölçüm prototipi) |
| Uygulayıcı | `ida-uygulayici` (Opus) |
| İnceleyen | sol (`tools/sol-incele.sh`; çağrıyı mimar başlatır) |
| Dal | `olcum/mp6-godot-kamera` |
| Tarih | 2026-11-14 |

## Amaç

Mimarinin en riskli görsel varsayımını en ucuz yoldan sınamak: Godot 4.7.2'de operatör penceresi görünmezken (örtülü, gizli, simge durumunda) öne bakan kamera için kare üretimi sürüyor mu, içerik doğru mu, kare CPU'ya ne kadar sürede geri okunuyor? Sonuç, kameranın birincil yolunun (Godot içinden) mı yedeğinin mi kullanılacağını ve açık soru A23'ü belirler. MP6'nın tamamı değil, **Mac ön turudur**: C++ uzantı derlemesi (P1, M33), 30 dakikalık ve 2 saatlik koşular, ekran koruyucu ve kapalı ekran durumları ile Windows yarısı sonraki işlerdedir.

## Okunacaklar

- `AGENTS.md`
- `docs/mimari/09-test-ve-is-akisi.md` (sürüm 0.2): 2.1, 2.6, 3.3, 6.1 (MP6 satırı)
- `docs/mimari/07-gorsellestirme.md`: kamera hattı ve görünmeyen pencere başlıkları, §6, §7'de P4, P5, P6; §8'deki `force_draw` notu
- `docs/research/00-duzeltmeler-ve-kararlar.md`: Ö5, Ö5-k, Ç14, M13, M28, M30, 4.12 düzeltme tablosu, A23
- Gerektiğinde `docs/research/12-godot-entegrasyonu.md` ve doğrulaması (ham rapor bilinen hatalar içerir; önce 00)

Godot API davranışına ilişkin her iddia (ör. `RenderingServer.force_draw`, `SubViewport` güncelleme kipleri, doku geri okuma) 4.7.2 belgelerinden ya da kaynak kodundan denetlenir; kaynağı raporda yazılır.

## Ortam

- Mac: Apple M3 Pro, 18 GB, macOS 26.5.2. Godot kurulu değil.
- Godot 4.7.2 resmî ikilisini (standart sürüm, .NET değil) resmî dağıtımdan indir; SHA-256'yı yayımlanan değerle karşılaştır ve rapora yaz. İndirme `${IDA_CACHE:-$HOME/.cache/ida}` altına; sistem geneline kurulum yapma. 4.7.2 bulunamazsa başka sürüme geçme: dur ve raporla.
- Deney kullanıcının masaüstünde pencere açar; pencereleri küçük tut, her koşudan sonra kapat, arkada süreç bırakma.

## Kapsam ve sıra

1. `tools/olcum/mp6/` altında en küçük Godot projesi (GDScript yeterli; C++ uzantı bu işte **yok**):
   - Operatör penceresi (ana görünüm) ve ondan bağımsız bir kamera `SubViewport`'u (640 × 480, RGBA8).
   - İçerik sınaması için belirlenimci sahne: konumu kare sayacından hesaplanan hareketli bir cisim, bir yönlü ışık ve gölge, kare sayacını piksellere kodlayan küçük bir bölge (kare kimliği geri okunabilsin).
   - Her kamera karesi CPU'ya geri okunur; kare başına kayıt: istek zamanı, hazır olma zamanı (monoton saat), kare kimliği, içerik özeti (sağlama toplamı), hareketli cismin ölçülen piksel konumu.
2. **İki üretim yolu ayrı ayrı ölçülür:** (a) olağan çizim döngüsü + `SubViewport` güncelleme kipi; (b) `RenderingServer.force_draw(false, dt)` ile açık zamanlama (ana iş parçacığından). Her birinin hangi pencere durumunda kare ürettiği ayrı raporlanır.
3. **Pencere durumları (her biri en az 5 dakika, 30 Hz istek):** görünür (referans); başka bir pencereyle tamamen örtülü; gizli; simge durumunda; başka uygulama tam ekranda. Durumun gerçekten oluştuğu ayrıca doğrulanır (ör. pencere durum sorgusu); doğrulanamayan durum "doğrulanmadı" yazılır. macOS'un App Nap ve kare kısma davranışı gözlenir; kapatmak için bir ayar gerekiyorsa hangisi olduğu yazılır.
4. **İçerik karşılaştırması:** her durumda, aynı kare kimliğinin görünür pencerede üretilmiş referansıyla ortalama mutlak piksel farkı; hareketli cismin konum hatası (piksel); gölgenin varlığı.
5. **Geri okuma maliyeti:** 640 × 480 ve 1280 × 720 için kare başına geri okuma süresi (p50, p99, en büyük) ve kullanılan yol (hangi API, kaç kopya).
6. `--headless` ile aynı proje bir kez çalıştırılır; kare üretilip üretilmediği kaydedilir (00'a göre GPU sensör yolu değildir; kanıtı rapora girer).
7. **Rapor:** `docs/olcumler/MP6-godot-kamera.md` (09 §3.3 biçimi): her durum × her yol için istek sayısı, üretilen kare, düşen kare ve nedeni, gecikme p50/p99, içerik farkı; ham veri yolu ve özeti.

## Geçme ölçütleri (bu görev belgesi dondurur; ölçülmüş değer değil, hedef)

| # | Ölçüt |
|---|---|
| G1 | Görünür pencerede en az bir yolla 5 dakika boyunca düşen kare < %1 (ölçüm düzeneğinin kendisi çalışıyor) |
| G2 | Örtülü, gizli ve simge durumlarının **her birinde** en az bir yolla kare üretimi sürer: düşen kare < %1, kare kimliği kesintisiz ilerler |
| G3 | G2'yi sağlayan yolda içerik: referansla ortalama mutlak fark ≤ 2/255; hareketli cismin konum hatası ≤ 1 piksel; gölge var |
| G4 | 640 × 480 geri okuma süresi p99 raporlanmış (eşik yok; sonraki bütçe hesabının girdisi) |

G2 ya da G3 geçmezse zorlamadan raporla: hangi durumda hangi yol ne yaptı (kare durdu mu, yavaşladı mı, içerik bayatladı mı). "Geçmedi" geçerli bir sonuçtur ve yedek kamera yolu kararını tetikler.

### İlk turdan sonra (mimar, 2026-11-14)

İlk tur yarım kaldı: Mac pildeyken kapağı kapandı ve uykuya girdi; `force_draw` yolunun 5 dakikalık koşuları yapılamadı. Kalan iş ve netleştirmeler:

- **Koşu koşulları (şart):** Mac prize takılı, kapak açık, ekran uyanık; koşular boyunca kullanıcı masaüstünü (Space) değiştirmez. Koşu süresince uykuyu engellemek için `caffeinate` kullanılır (ayar değiştirmez). Kalan koşular: `tools/olcum/mp6/kampanya.sh` (yaklaşık 70 dakika; tam ekran koşusu ekranı 5 dakika kaplar). Kullanıcıya önceden haber verilir.
- **"Gizli" durumu:** uygulamanın gizlenmesi (Cmd+H eşdeğeri) olarak tanımlanır; Godot 4.7.2 ana pencereyi içeriden gizleyemiyor.
- **G3 gölge ölçütü:** gölgeli bölgenin parlaklık oranı ≤ 0,75 (ilk turda ölçümden önce donduruldu; kabul edildi).
- **İndirme özeti:** resmî dağıtım SHA-512 yayımlıyor; SHA-512 ile ya da sürüm varlığının yayımlanan özetiyle karşılaştırma yeterlidir.
- Yinelenecekler: yol (a) görünür ve tam ekran; yol (b) beş durumun hepsi; 1280 × 720 ve asenkron geri okumanın tam süreli ölçümü.

### Tam kampanyadan sonra: düzeltme turu (mimar, 2026-11-15; 1. tur)

Tam kampanya 2026-11-14 23:12 – 2026-11-15 00:21 arasında kesintisiz koştu. sol incelemesi: `docs/olcumler/inceleme/MP6-sol.md` (dal `olcum/mp6-godot-kamera`); 2 kritik, 4 önemli bulgu. Mimarın kararları:

- **Yol (a) (olağan çizim döngüsü) birincil yol olarak elendi:** örtülü, gizli ve simge durumlarında 5 dakika boyunca kare üretmedi. Geçersiz kalan `a_gorunur` ve `a_tamekran` koşuları **yinelenmez**; raporda "geçersiz, yinelenmedi; yol (a) tam ekran sonucu doğrulanmadı" diye yazılır ve §6'daki "dört durumda" genellemesi üç doğrulanmış duruma indirilir (sol md.5).
- **sol md.1:** `b_gorunur` bir kez daha, kesintisiz 300 saniye, görünürlük doğrulamasıyla koşulur (kullanıcıya haber verilir; 5 dakika masaüstü değişmez). G1 ve G3'ün görünür referansı bu koşudan alınır; diğer durumların kareleri bu referansla yeniden karşılaştırılır. Önceki koşudaki 11 saniyelik çizimsiz aralık açıklanamazsa "doğrulanmadı"da kalır.
- **sol md.2, md.4, md.6:** rapor ve çözümleme düzeltmeleri: asenkron 1280 × 720 p99 değeri ham özetten; koşu başına gerçek referans ve karşılaştırılan/karşılaştırılmayan kare sayısı; konumu bulunamayan kare sayısı tabloya girer ve G3, bütün karelerde konumun bulunmasını şart koşar.
- **sol md.3:** ikinci okuma "yeni çizim olmadan yinelenen eşzamanlı geri okuma" olarak adlandırılır; "saf aktarım" ve "büyük kısmı çizim beklemesi" ifadeleri çıkarılır. Hazır olma zamanı ilk okumadan hemen sonra kaydedilecek biçimde düzeltilir ve 640 × 480 eşzamanlı okuma koşusu (2 dakika) yinelenir.
- "Durum tutuldu 299/299" ifadesi kayıt aralığı sayısı olarak yazılır; süre zaman damgalarından hesaplanır. Doğrulanmayanlara eklenir: operatör zamanı ile kamera zamanı arasında gidip gelen çizim düzeni (07 §2.4) bu prototipte sınanmadı.
- `docs/olcumler/inceleme/MP6-yanit.md` yazılır (madde → giderildi / kısmen / uygulanmadı, yer, gerekçe).

**İkinci sol incelemesinden sonra (mimar, 2026-11-15; 2. ve son tur):** kritik bulgu yok, iki önemli bulgu (`docs/olcumler/inceleme/MP6-sol-2.md`).

- **Güç koşulu (sol-2 md.2):** yeni görünür referans (`b_gorunur_2`) ve yinelenen koşular pilde alındı. "Prize takılı" şartının amacı uykuyu önlemekti; koşular `caffeinate -dims` altında kesintisiz tamamlandığı ve kare sayımı, kimlik sürekliliği ile bit düzeyinde eşitlik güç kaynağına bağlı bir büyüklük olmadığı için **mimar pildeki kanıtı G1 ve G3 için kabul eder**. Süre ölçümleri (G4) prizdeki gece kampanyasına dayanır. Rapor bu sapmayı ve kabulü G1/G3 hükmünde belirtir.
- **Tam ekran doğrulaması (sol-2 md.1):** çözümleme ekran sınırlarını ölçmediği için "başka uygulama tam ekranda" durumunun sürdüğü kanıtlanmış sayılmaz; yol (b)'nin tam ekran sonucu da "doğrulanmadı"ya indirilir (üretilen kare sayısı ve içerik eşitliği gözlem olarak kalır). G2'nin üç zorunlu durumu (örtülü, gizli, simge) etkilenmez. Tam ekran durumu Windows turuyla birlikte yeniden ölçülür.

## Dosya sahipliği

- Yazılır: `tools/olcum/mp6/**`, `docs/olcumler/MP6-godot-kamera.md`, gerekirse `.gitignore` (Godot'nun `.godot/` önbelleği ve ham veri için ek satır).
- Dokunulmaz: `docs/mimari/**`, `docs/research/**`, `docs/DURUM.md`, `AGENTS.md`, `README.md`, `docs/gorevler/**`, başka `tools/` dosyaları.
- Bu kod atılabilir prototiptir; ürün dizinlerini (`viz/`, `sim/`) oluşturma. Büyük ham veri (kare dökümleri) depoya girmez.

## Yasaklar

- Sistem ayarlarını kalıcı değiştirme (enerji, ekran koruyucu, erişilebilirlik izinleri). Ekranı uyutma ya da kilitleme bu işte yok.
- Sözleşme (A2, A8) yazma ya da varsayma; sim sunucusu, soket, paylaşımlı bellek bu işin kapsamında değil.
- Ölçülmemiş rakamı sonuç gibi yazma. Rust tabanlı araç yok.
- Commit atma, `git add` yapma, `git push` yapma, başka agent başlatma. Dalı (`git switch -c olcum/mp6-godot-kamera`) oluşturabilirsin; commit'i kayıt agent'ı atar.

## Rapor ve bitiş

Son iletinde ayrı başlıklarla: **yapılan**, **doğrulanan** (komut ve gerçek çıktı özetiyle), **doğrulanmayan**, **kurulan ya da indirilen araçlar**, **çalışma dizininin yolu ve dal adı**, görev belgesinde hatalı bulduğun nokta. İş, rapor dosyası yazılıp dört ölçütün her biri "geçti" ya da gerekçeli "geçmedi" olarak işaretlendiğinde biter. Düzeltme turu sınırı iki.
