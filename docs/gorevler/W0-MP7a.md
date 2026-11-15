# W0-MP7a — Sonar hesap süresi ve ışın yakınsaması (Mac ön turu)

| | |
|---|---|
| Kimlik, dalga | `W0-MP7a`, W0 (ölçüm prototipi) |
| Uygulayıcı | `ida-uygulayici` (Opus) |
| İnceleyen | sol (`tools/sol-incele.sh`; çağrıyı mimar başlatır) |
| Dal | `olcum/mp7-sonar-suresi` |
| Tarih | 2026-11-14 |

## Amaç

Ping360 taramalı sonar modelinin (bölüm 05 §2.4) CPU'da Embree ile bir ping'i ne kadar sürede hesapladığını ve ping başına kaç ışının yeterli olduğunu ölçmek. Sonuç şu kararların girdisidir: sonarın tam ayrıntısı HIL'de gerçek zamanlı koşabilir mi yoksa yalnız SIL'de mi (iskelet §8, 05 §6), ping başına ışın sayısı, işçi iş parçacığı sayısı. Windows hedef bilgisayarı düşük donanımlıdır (8 GB bellek, dahili ekran kartı, işlemci bilinmiyor; K16); bu yüzden **tek iş parçacıklı süre** ve bellek kullanımı ayrıca raporlanır.

MP7'nin tamamı değil, **Mac ön turudur**: platformlar arası eşdeğerlik, Windows süresi ve açık veri kümesiyle karşılaştırma sonraki işlerdedir.

## Okunacaklar

- `AGENTS.md`
- `docs/mimari/09-test-ve-is-akisi.md` (sürüm 0.2): 2.1, 2.6, 3.3, 6.1 (MP7 satırı)
- `docs/mimari/05-sensorler-ve-sonar.md`: §2.2 (zaman), §2.4 (sonar; tamamı), §2.6, §5, §6, §7'nin sonar testleri
- `docs/mimari/04-simulator-cekirdegi.md`: işçi havuzu ve dünya adım bütçesi başlıkları (05 §2.4.1'in atıf yaptığı yerler)
- `docs/research/00-duzeltmeler-ve-kararlar.md`: Ö7 ve Embree satırları, Ç9, Ç13, M11, 4.5 ve 4.10 düzeltme tabloları
- Gerektiğinde `docs/research/05-sensor-modelleme.md`, `10-gercekcilik-ve-grafik-motoru.md` ve doğrulamaları (ham raporlar bilinen hatalar içerir; önce 00)

`docs/mimari/acik-bulgular.md` içinde sonar modeline ilişkin açık bulgular var; bu iş onları kapatmaz. Bölüm 05'in modelinde belirsiz ya da çelişkili bir nokta ölçümü etkiliyorsa en yalın yorumu seç, seçimi raporda açıkça yaz.

## Ortam

- Mac: Apple M3 Pro, 18 GB, macOS 26.5.2. `cmake` 4.5.0 ve `ninja` 1.13.2 Homebrew ile kurulu.
- Embree 4.4.1 hedeflenir (09 §2.6). Homebrew'deki sürüm 4.4.x ise kullanılabilir; değilse `v4.4.1` etiketinden `${IDA_CACHE:-$HOME/.cache/ida}` altında kaynaktan derle. Kullanılan sürüm, derleme bayrakları ve ISA (NEON) rapora girer. Başka bağımlılık gerekiyorsa en azıyla yetin ve sürümüyle yaz.
- Aynı Mac'te eşzamanlı başka ölçümler koşuyor olabilir (Godot, kart). Zamanlama koşularını makine boşken yinele; yük altında alınmış ölçümü işaretle. USB ve seri aygıtlara dokunma.

## Kapsam ve sıra

1. `tools/olcum/mp7/` altında C++20 ölçüm programı (CMake + Ninja):
   - **Sahne (belirlenimci, tohumlu):** engebeli deniz tabanı (yükseklik alanı), birkaç ayakta cisim (silindir, kutu, küre ağları), bir duvar. Üç boyut: yaklaşık 10⁴, 10⁵, 10⁶ üçgen. Su yüzeyi yansıtıcı düzlem.
   - **Sonar:** Ping360 geometrisi ve örnekleme (hüzme açıklıkları, menzil, örnek sayısı 05 §2.4'ten; 1200 örneğe kadar). 05 §2.4'ün ışın, saçıcı, çok yollu yayılım (multipath) ve darbe adımlarını ölçüm için yeterli sadakatte uygula: hesap yükünü belirleyen adımlar (ışın sorgusu, yansıma derecesi, kutulara dağıtma, gürültü/benek) gerçek olsun; kalibrasyon ve arıza zinciri gerekmez.
2. **Süre taraması:** ping başına ışın sayısı `N` (en az beş değer, 05 §5'teki başlangıç değerlerini içerecek aralık) × yansıma derecesi (yalnız doğrudan; +1 yansıma; 05'in öngördüğü en yüksek) × sahne boyutu × iş parçacığı sayısı (1, 2, 4, tümü). Her hücre için en az 200 ping; ping başına süre p50, p99, en büyük; sahne kurulum (BVH) süresi ve en yüksek bellek ayrı.
3. **Yakınsama (05 §6):** `N` iki katına çıkınca M = 32 bağımsız gerçekleşimde kutu ortalama enerjisi, kutu varyansı ve ardışık kutu ilintisinin değişimi; %5 ölçütü hangi `N`'de sağlanıyor.
4. **Tekrar üretilebilirlik (aynı platform):** aynı tohumla iki koşu bit düzeyinde aynı profili veriyor mu; tek iş parçacığıyla ve çok iş parçacığıyla ayrı ayrı (05 §2.6).
5. **Bütçeyle karşılaştırma:** bütçe ölçümden **önce** rapora yazılır ve kaynağı gösterilir: Ping360'ın seçilen menzildeki ping aralığı (05 §2.4; gidiş-dönüş süresi + aygıt gecikmesi) ve 04'ün işçi/dünya adım bütçesi. En az iki menzil (kısa ve uzun) için.
6. **Rapor:** `docs/olcumler/MP7-sonar-suresi.md` (09 §3.3 biçimi).

## Geçme ölçütleri (bu görev belgesi dondurur; ölçülmüş değer değil, hedef)

| # | Ölçüt |
|---|---|
| S1 | Süre taraması tablosu eksiksiz; her hücrede p50/p99/en büyük ve ping sayısı kayıtlı |
| S2 | Yakınsama ölçütünü (05 §6: üç büyüklüğün değişimi %5 içinde) sağlayan en küçük `N` bulunmuş; bulunamadıysa taranan en büyük `N`'deki değişim raporlanmış |
| S3 | S2'deki `N` ve 05'in en yüksek yansıma derecesiyle, orta sahnede (≈ 10⁵ üçgen), **tek iş parçacığında** ping başına p99 süre, kısa menzilin ping aralığından küçük |
| S4 | Aynı tohumla iki koşu tek iş parçacığında bit düzeyinde aynı |

S3 geçmezse zorlamadan raporla: hangi `N`, yansıma derecesi ve iş parçacığı sayısında bütçeye giriliyor. "Geçmedi" geçerli bir sonuçtur ve sonar kademesi kararını tetikler. M3 Pro'da ölçülen süre Windows hedefini temsil etmez; rapor bunu açıkça söyler ve Windows için tahmin **yazmaz**.

### İlk turdan sonra: düzeltme turu (mimar, 2026-11-15; 1. tur)

İlk tur ve sol incelemesi (`docs/olcumler/inceleme/MP7-sol.md`, dal `olcum/mp7-sonar-suresi`; 4 kritik, 2 önemli bulgu) şunu gösterdi: süre tablosu geçerli, ama bölüm 05 §2.4.3'ün saçıcı hücresi kuralı hatalı (hücreye düşen `m` ışının `√(σ·dA_j)` genlikleri eşevreli toplanınca enerji `σ·A·m` olur, ışın sayısıyla sınırsız büyür; sol bağımsız türeterek doğruladı) ve §6'nın yakınsama büyüklükleri tanımsız. Bu yüzden "yeterli ışın sayısı" sorusu yanıtlanamadı. Bu tur modeli mimarın aşağıdaki tanımıyla düzeltir ve yakınsamayı yeniden ölçer. (Düzeltme: bu belgede "Ö7: sonar CPU'da" atfı yanlıştı; doğrusu Ö4.)

**Mimarın tanımı (05 §2.4.3 ve §6 için düzeltme önerisi; bu ölçümde uygulanır):**

1. **Hücre içinde enerji toplanır, hücreler arasında alan toplanır.** Yüzey, dünya uzayında sabit, kenarı `Δ_h` olan saçıcı hücrelerine bölünür (ortonormal yüzey koordinatlarında; hücre kimliği ışın sayısından bağımsız). Hücre `c`'ye düşen her ışın `j` bir enerji ağırlığı taşır: `w_j = σ_j · dA_j · G_j` (`dA_j` ışının temsil ettiği yüzey alanı, `G_j` iki yönlü hüzme ve yayılım kazancının karesi alınmış hali). Hücrenin alanı (basınç) katkısı `p_c = s_c · √(Σ_{j∈c} w_j) · g_c · e^{iφ_c}` olur: `g_c` ve `φ_c` hücre kimliği ile gerçekleşim tohumundan türeyen benek çarpanı ve fazıdır (ışın sayısından bağımsız), `s_c` hücredeki işaretli genlik toplamının işaretidir (negatif yan lob korunur). Kutu (menzil örneği) alanı, o kutuya düşen hücrelerin `p_c` değerlerinin eşevreli toplamıdır. Böylece beklenen enerji her `N`'de `σ × aydınlatılan alan` ile orantılıdır; ışın izi hücreden büyükken (seyrek rejim) yalnız benek istatistiği kabalaşır.
2. **Sade kademe (eşevresiz):** kutu enerjisi `Σ_j w_j`'dir; faz ve benek çarpanı yok.
3. **Koni ve yan loblar:** koni dışı elenir; koni içindeki negatif genlikli yan loblar elenmez (sol md.2), görünürlük sorguları da yapılır.
4. **Yakınsama büyüklükleri (ölçümden önce dondurulmuştur):** `M = 32` gerçekleşim; gerçekleşimlerin faz ve benek alanı `N`'den bağımsızdır (aynı gerçekleşim tohumu farklı `N`'de aynı hücre fazlarını verir), gürültü kapalı. Kutu kümesi `S`, sınanan koşulardan **bağımsız** bir referansla (taranan en büyük `N`, ayrı gerçekleşim tohumları) önceden sabitlenir: referans ortalama enerjisi en yüksek kutunun 10⁻³ katından büyük olan kutular. `N → 2N` adımı için üç büyüklük:
   - `δ_E = Σ_{b∈S} |Ē_2N(b) − Ē_N(b)| / Σ_{b∈S} Ē_2N(b)` (kutu ortalama enerjisi; mutlak farkların toplamı, götürme yok),
   - `δ_V`: aynı ifade, kutu başına gerçekleşimler arası ham varyansla (`V/E²` değil),
   - `δ_C = |C̄_2N − C̄_N|`: `S` üzerindeki komşu kutu ilinti katsayısının ortalaması.
   Ölçüt: üçünün nokta değeri ve gerçekleşimler üzerinde bootstrap ile bulunan %95 güven aralığının üst ucu ≤ 0,05; **ve** bir sonraki ikiye katlamada da sağlanmaya devam eder (kararlılık). Benek kontrastı (`V/E²`) ayrı tanılayıcı olarak raporlanır.

**Yapılacaklar:**

- Kodu yukarıdaki tanıma göre düzelt (sol md.2, md.3, md.4 bu tanımla kapanır); S2'yi md.1 ve md.5'e göre yeni büyüklüklerle yeniden ölç: 7 m ve 50 m, tam kademe en yüksek yansıma derecesi; ayrıca 7 m sade kademe.
- Süre taramasını **daraltılmış ızgarayla** yinele (model değiştiği için): orta sahne (≈ 10⁵ üçgen), 7 m ve 50 m, `N` aynı sekiz değer, yansıma derecesi 0/1/2, iş parçacığı 1/2/4/tümü; sahne boyutu etkisi için yalnız `N` = 20 000 ve 80 000'de 10⁴ ve 10⁶ üçgen, tek iş parçacığı. S3 hücresi yeni S2'nin bulduğu `N` ile uzun koşu (2000 ping). S4 yeniden doğrulanır.
- Rapor: md.6'daki iki cümle veriyle sınırlandırılır (sahne etkisi menzile göre; 1,25 ms genellemesi kaldırılır); S2 hükmü yeni ölçümden; ilk turun sonuçları kısa "önceki tur" notuna iner; "doğrulanmayanlar"a p99'un bu tarama iş yüküne ait olduğu eklenir. Bölüm 05 için düzeltme önerisi ayrı bir başlıkta, mimarın tanımı ve ölçüm kanıtıyla özetlenir (05 dosyasına dokunulmaz).
- `docs/olcumler/inceleme/MP7-yanit.md` yazılır (madde → giderildi / kısmen / uygulanmadı, yer, gerekçe).
- Mimarın tanımında ölçümü engelleyen bir tutarsızlık görürsen uygulamadan önce dur ve bildir; küçük belirsizliklerde en yalın yorumu seçip raporla.

### Düzeltme turundan sonra (mimar, 2026-11-15; 2. ve son tur)

Birinci düzeltme turu modeli mimarın tanımıyla yeniden ölçtü: S1 ve S4 geçti; tam kademede S2, taranan en büyük ışın sayısına (`N` ≈ 2,56 milyon) kadar sağlanmadı (ortalama enerji ve ilinti sağlanıyor, ham varyansın güven aralığı sağlamıyor); S3 geçmedi. Mimarın değerlendirmesi ve bu turun kapsamı:

- **Mimarın tanımındaki hata (sade kademe):** "faz ve benek çarpanı yok" ifadesi yanlıştı; gerçekleşimler arası varyansı yapısal olarak yok ediyor. Düzeltme: sade kademede kutu enerjisi `Σ_j w_j`'dir ve bölüm 05'in **kutu başına benek çarpanı korunur** (gerçekleşim tohumundan, `N`'den bağımsız). Sade kademenin S2 hükmü bu haliyle verilir (ilk ölçüm: `N` = 5 000).
- **Dondurulmuş ölçüt değiştirilmez.** Tam kademe için S2 "geçmedi", S3 "geçmedi" olarak kalır; ölçüt sonuç görüldükten sonra gevşetilmez. Bunun yerine karar için gereken betimleyici tablo eklenir: tam kademede `N` = 20 000, 40 000, 80 000, 160 000 için **bağımsız referansa göre** `δ_E`, `δ_V`, `δ_C` (aynı tanımlar; `2N` yerine referans) ve benek kontrastı; 7 m ve 50 m. Mevcut veriden hesaplanabiliyorsa yeni koşu yapılmaz; değilse yalnız bu dört `N` için `M = 32` gerçekleşim koşulur. Bu tablo ölçüt değildir; "azaltılmış ışın sayısının bilinen hatası"dır.
- **Sade kademe süresi:** S2'nin bulduğu `N` ile sade kademenin tek iş parçacığı p99'u (orta sahne, 7 m ve 50 m) tablodan açıkça verilir ve bütçeyle karşılaştırılır.
- **Sonuç bölümü:** rapor, iskelet §8'deki sonar kararı için girdiyi tek paragrafta toplar: (i) tam kademe dondurulmuş yakınsama ölçütünü gerçek zamanlı bütçede sağlamıyor; (ii) bütçeye giren `N` değerleri ve o `N`'lerdeki bilinen hata; (iii) sade kademenin `N`'si ve süresi. Karar cümlesi yazılmaz (karar mimarındır); Windows için tahmin yazılmaz.
- `MP7-yanit.md`'ye "İkinci tur" başlığı eklenir. Kod değişikliği yalnız sade kademe beneği ve gerekiyorsa tablo için çözümleme betiğiyle sınırlıdır.

### Kapanış (mimar, 2026-11-16; iki düzeltme turundan sonra)

İkinci sol incelemesi (`docs/olcumler/inceleme/MP7-sol-2.md`): ilk altı bulgu giderildi; tam kademe S2 ve S3 "geçmedi", sade kademe S2 (`N` = 5 000) ve süreleri CSV'lerden doğrulandı; 1 kritik, 2 önemli yeni bulgu. Düzeltme turu sınırı dolduğu için (09 §2.4) iş mimara döndü; kapsam ve tanım şöyle kapatılır, yeni ölçüm yapılmaz:

- **Model tanımının düzeltilmesi (sol-2 md.1):** uygulanan değişke mimarın ilk yazımından iki noktada ayrılıyor ve **mimar uygulanan değişkeyi tanım olarak kabul eder**; ilk yazım kusurluydu:
  1. Hücreler dünya uzayına değil **yüzeyin ait olduğu gövdeye sabittir** (gövde yerel koordinatları). Durağan deniz tabanı için ikisi aynıdır; hareketli bir cisimde saçıcılar cisimle birlikte hareket eder, doğru olan budur.
  2. Faz ve benek anahtarı **(gövde, hücre, yol türü, gerçekleşim)** dörtlüsüdür. Aynı hücrenin doğrudan yoldan ve yansımalı yoldan gelen katkıları farklı geliş geometrisine ve dalga boyuna göre çok büyük yol farkına sahiptir (750 kHz'te dalga boyu ≈ 2 mm); aralarındaki faz ilişkisi rastgele kabul edilir. Sonuç olarak aynı kutuya düşen farklı yol türleri enerji olarak toplanır; aynı yol türü içinde hücreler eşevreli toplanır.
  Ölçülen bütün istatistikler bu değişkeye aittir; rapor bunu açıkça yazar. Bölüm 05 §2.4.3'e taşınacak kural bu değişkedir.
- **Hata tablosu (sol-2 md.2):** eşli tablo "ortak benek alanında, sonlu referansa göre sayısal fark" olarak adlandırılır; "gürültü tabanı" satırı "gözlenen iki bağımsız kümenin farkı (kutu kümesi karşılaştırılan kümelerden biriyle seçildi; genel bir belirsizlik tabanı değildir)" diye yeniden yazılır.
- **Genellemeler (sol-2 md.3):** "`N` > 1 280 000 gerekir" ve "daha büyük `N`'de p99 ≥ 504 ms" türü evrensel ifadeler çıkarılır; kanıt taranan yapılandırmalarla ve `N` = 1 280 000 destek koşusuyla sınırlanır. "Doğrulanmayanlar"a eklenir: sonlu referansın kendisi yakınsamamıştır; sade kademede S2 yalnız 7 m'de ölçüldü.
- Bu üç düzenleme yalnız rapor metnidir (kod ve veri değişmez); `MP7-yanit.md`'ye "Kapanış" başlığıyla işlenir.

## Dosya sahipliği

- Yazılır: `tools/olcum/mp7/**`, `docs/olcumler/MP7-sonar-suresi.md`, gerekirse `.gitignore` (derleme çıktısı ve ham veri için ek satır; `tools/olcum/*/derleme/` satırı başka bir dalda eklenmiş olabilir, çakışmayı önemseme).
- Dokunulmaz: `docs/mimari/**`, `docs/research/**`, `docs/DURUM.md`, `AGENTS.md`, `README.md`, `docs/gorevler/**`, başka `tools/` dosyaları.
- Bu kod atılabilir prototiptir; ürün dizinlerini (`sim/`, `core/`) oluşturma. Büyük ham veri depoya girmez.

## Yasaklar

- Sözleşme (A1, A4) yazma ya da varsayma; sim sunucusu, fizik, mesajlaşma bu işin kapsamında değil.
- Ölçülmemiş rakamı sonuç gibi yazma; Windows için süre tahmini yazma. Rust tabanlı araç yok. GPU ile hesap yok (Ö7: sonar CPU'da).
- Commit atma, `git add` yapma, `git push` yapma, başka agent başlatma. Dalı (`git switch -c olcum/mp7-sonar-suresi`) oluşturabilirsin; commit'i kayıt agent'ı atar.

## Rapor ve bitiş

Son iletinde ayrı başlıklarla: **yapılan**, **doğrulanan** (komut ve gerçek çıktı özetiyle), **doğrulanmayan**, **kurulan ya da indirilen araçlar**, **çalışma dizininin yolu ve dal adı**, bölüm 05'in modelinde yaptığın yorum seçimleri, görev belgesinde hatalı bulduğun nokta. İş, rapor dosyası yazılıp dört ölçütün her biri "geçti" ya da gerekçeli "geçmedi" olarak işaretlendiğinde biter. Düzeltme turu sınırı iki.
