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
