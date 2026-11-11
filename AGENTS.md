# İDA — yapay zeka agent'ları için çalışma kuralları

Bu dosya, bu depoda çalışan her yapay zeka agent'ının (Claude Code, Codex ve diğerleri) işe başlamadan önce okuması gereken kuralları içerir.

## Proje

Tek bir hibrit insansız deniz aracı (su üstünde seyreden ve dalabilen tek araç) için simülasyon laboratuvarı.

- **Hardware-in-the-Loop (HIL):** gerçek otopilot yazılımı bir STM32 kartında koşar; deniz, araç dinamiği, sensörler (sonar dahil) ve aktüatörler PC'de simüle edilir. Kart yazılımı simülasyonda olduğunu bilmez; gerçek sensörler geldiğinde yalnızca sürücü katmanı değişir.
- **Software-in-the-Loop (SIL):** aynı çekirdek kod PC'de derlenip koşar.
- Araç su altında sonar ile tarama yapar; kart ayrıca yarışma tipi (küçük, zararsız, patlayıcısız) torpido için atış kontrolünü yürütür.
- Öncelik: simülasyonun olabildiğince gerçekçi olması.

## Kesin kararlar

Bunlar kullanıcı tarafından verilmiştir; yeniden tartışmaya açma, alternatif önerme.

| Konu | Karar |
|---|---|
| Araç | Tek hibrit araç: yüzey + dalış |
| Kart | STM32U5A9J-DK (geçici, üretimden kalkmış; ileride değişecek) |
| Kart yazılımı ve ortak çekirdek | C |
| PC tarafı (simülatör, laboratuvar uygulaması) | C++ |
| Rust | Kullanılmaz; Rust tabanlı araç da önerilmez |
| Platform | macOS (Apple Silicon) ve Windows, ikisinde de native |
| Simülatör | Sıfırdan yazılır; fizik çekirdeği görselleştirmeden bağımsız ve başsız çalışır |

Önerilen ama henüz onaylanmamış yığın ve gerekçeleri için `docs/research/00-duzeltmeler-ve-kararlar.md` belgesinin karar günlüğüne bak.

## Belgeleri okuma sırası

1. **`docs/research/00-duzeltmeler-ve-kararlar.md`** — tek geçerli başvuru belgesi. Kararlar, raporlardaki hataların düzeltmeleri, raporlar arası çelişkilerin çözümleri, ölçülmeden kesinleşemeyecek konular ve açık sorular buradadır.
2. **`docs/research/NN-*.md`** — ham araştırma raporları. Yazıldıkları haliyle korunur ve **bilinen hatalar içerir**. 01–07 Rust varsayımıyla yazılmıştır.
3. **`docs/research/dogrulama/NN-codex.md`** — her raporun bağımsız doğrulaması.

Bir ham rapor ile 00 numaralı belge çelişirse 00 geçerlidir. Bir ham rapordan bilgi alacaksan önce o raporun doğrulamasına ve 00'daki düzeltme tablosuna bak.

## Belge kuralları

- Ham raporların içeriğini yeniden yazma ya da silme. Düzeltme gerekiyorsa `00-duzeltmeler-ve-kararlar.md` belgesine eklenir.
- Yeni araştırma raporu `docs/research/NN-konu.md` adıyla, sıradaki numarayla yazılır ve sonunda "Bağımsız doğrulanması gereken kritik iddialar" bölümü bulunur.
- Her yeni rapor `tools/codex-dogrula.sh NN` ile doğrulanır; doğrulamanın düzeltmeleri 00 numaralı belgeye işlenir.
- Her önemli iddianın yanında kaynak bulunur. Doğrulanamayan şey "doğrulanmadı" diye işaretlenir.
- Ölçülmemiş bir performans, gecikme ya da işlemci yükü rakamını gerçek gibi yazma; tahmin olduğunu belirt.
- Dosya yolları depo köküne göre göreli yazılır; kişisel makine yolları depoya girmez.
- Belgeler Türkçe yazılır; teknik terimler İngilizce kalabilir.

## Çalışma yöntemi

1. Mimar oturum kullanıcıyla kararları netleştirir, işi tanımlar ve çıktıları denetler; kendisi araştırma ya da uygulama yapmaz.
2. Araştırma ve uygulama işleri alt agent'lara verilir.
3. Bağımsız ikinci bir model (Codex) çıktıyı doğrular ya da inceler; aynı işi iki modele yazdırmak yerine biri yazar, diğeri denetler.
4. Web araştırmasıyla kapanmayan sorular kart üzerinde ya da prototiple ölçülür; ölçüm sonucu çıkmadan ilgili tasarım kararı kesinleşmiş sayılmaz.

## Depo düzeni

```
docs/research/             ham araştırma raporları ve 00 numaralı düzeltme belgesi
docs/research/dogrulama/   bağımsız doğrulamalar
tools/                     yardımcı betikler
```

Kod dizinleri mimari belge onaylandıktan sonra eklenecek.

## Git

- Varsayılan dal `main`. Kod geliştirmeleri ayrı dalda yapılır ve pull request ile birleştirilir.
- Commit mesajları Türkçe, kısa ve ne değiştiğini söyleyen bir başlıkla yazılır.
- Derleme çıktıları, simülasyon kayıtları ve yerel araç durumu depoya girmez (`.gitignore`).
