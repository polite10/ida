# İDA — Hibrit İnsansız Deniz Aracı Simülasyon Laboratuvarı

Tek bir hibrit insansız deniz aracı (su üstünde seyreden ve dalabilen tek araç) için Hardware-in-the-Loop (HIL) ve Software-in-the-Loop (SIL) simülasyon laboratuvarı.

## Durum

**Araştırma aşaması tamamlandı.** Bu depoda henüz kod yok. On üç araştırma raporu yazıldı, her biri bağımsız bir model tarafından doğrulandı ve düzeltmeler tek belgede toplandı. Mimari iskelet yazıldı (`docs/mimari/00-iskelet.md`, taslak) ve bölümler yazılmakta; sıradaki adım mimarinin onayı; kod geliştirmeleri onun onayından sonra bu depoya eklenecek.

Proje bir araştırma ödevidir; bir yarışmaya hazırlık değildir. Raporlar başlangıçta yarışma varsayımıyla yazıldı; yarışma kurallarına dayalı çıkarımlar tasarımı bağlamaz.

## Proje özeti

- Gerçek otopilot yazılımı bir STM32 kartında koşar; deniz, araç dinamiği, sensörler (sonar dahil) ve aktüatörler PC'de simüle edilir.
- Aynı çekirdek kod PC'de de derlenir; kart olmadan geliştirme ve otomatik test yapılabilir.
- Kart ayrıca simüle edilen torpido için hedef algılama ve atış kontrolünü yürütür.
- Laboratuvar uygulaması 3B görünüm, harita, görev planlama, canlı telemetri, sonar görüntüsü, senaryo editörü, hata enjeksiyonu, kayıt ve tekrar oynatma sunar.
- Öncelik simülasyonun olabildiğince gerçekçi olmasıdır.

## Kesin kararlar

| Konu | Karar |
|---|---|
| Araç | Tek hibrit araç: yüzey + dalış |
| Kart | STM32U5A9J-DK; bu aşamadaki tek hedef kart |
| Odak | Simülasyon ortamını kurup bu kartta çalıştırmak |
| Kart yazılımı ve ortak çekirdek | C |
| PC tarafı (simülatör, laboratuvar uygulaması) | C++ |
| Platform | macOS (Apple Silicon) ve Windows, ikisinde de native |
| Simülatör | Sıfırdan yazılır; fizik çekirdeği görselleştirmeden bağımsız |
| Arayüz panelleri | Harita, grafik ve formlar web teknolojisiyle |

Önerilen yığın ve henüz açık olan kararlar [00 numaralı belgede](docs/research/00-duzeltmeler-ve-kararlar.md) duruyor.

## Belgeler nasıl okunur

1. **Projenin şu anki durumu için [docs/DURUM.md](docs/DURUM.md).** Ne bitti, ne sürüyor, sıradaki adım.
2. **Kararlar ve düzeltmeler için [docs/research/00-duzeltmeler-ve-kararlar.md](docs/research/00-duzeltmeler-ve-kararlar.md).** Tek geçerli başvuru belgesi: kararlar, raporlardaki hataların düzeltmeleri, çelişkilerin çözümleri, ölçülmesi gereken konular ve açık sorular.
3. **Ham raporlar** yazıldıkları haliyle korunuyor ve bilinen hatalar içeriyor. Her raporun başında bunu belirten bir not ve doğrulamasına bağlantı var.
4. **Doğrulamalar** her raporun iddialarını birincil kaynaklardan sınayan bağımsız incelemelerdir.

Bir ham rapor ile 00 numaralı belge çelişirse 00 geçerlidir.

| # | Konu | Rapor | Doğrulama | Not |
|---|---|---|---|---|
| 01 | Kart ve araç zinciri | [rapor](docs/research/01-kart-ve-arac-zinciri.md) | [doğrulama](docs/research/dogrulama/01-codex.md) | Rust varsayımıyla yazıldı |
| 02 | Mevcut simülatörler | [rapor](docs/research/02-mevcut-simulatorler.md) | [doğrulama](docs/research/dogrulama/02-codex.md) | Rust varsayımıyla yazıldı |
| 03 | Uygulama yığını | [rapor](docs/research/03-uygulama-yigini.md) | [doğrulama](docs/research/dogrulama/03-codex.md) | Rust varsayımıyla yazıldı; yerini 09 aldı |
| 04 | Hidrodinamik | [rapor](docs/research/04-hidrodinamik.md) | [doğrulama](docs/research/dogrulama/04-codex.md) | Dilden bağımsız |
| 05 | Sensör ve sonar modelleme | [rapor](docs/research/05-sensor-modelleme.md) | [doğrulama](docs/research/dogrulama/05-codex.md) | Dilden bağımsız |
| 06 | Otopilot | [rapor](docs/research/06-otopilot.md) | [doğrulama](docs/research/dogrulama/06-codex.md) | Mimari geçerli; Rust'a özgü kısımlar geçersiz |
| 07 | Hedef algılama ve atış kontrol | [rapor](docs/research/07-hedef-algilama-ve-atis-kontrol.md) | [doğrulama](docs/research/dogrulama/07-codex.md) | Dilden bağımsız |
| 08 | Kartta C yığını | [rapor](docs/research/08-kart-c-yigini.md) | [doğrulama](docs/research/dogrulama/08-codex.md) | C/C++ kararından sonra |
| 09 | PC tarafı C++ yığını | [rapor](docs/research/09-pc-cpp-yigini.md) | [doğrulama](docs/research/dogrulama/09-codex.md) | C/C++ kararından sonra |
| 10 | Gerçekçilik ve grafik motoru | [rapor](docs/research/10-gercekcilik-ve-grafik-motoru.md) | [doğrulama](docs/research/dogrulama/10-codex.md) | C/C++ kararından sonra |
| 11 | Haberleşme ve güç sistemi | [rapor](docs/research/11-haberlesme-ve-guc.md) | [doğrulama](docs/research/dogrulama/11-codex.md) | C/C++ kararından sonra |
| 12 | Godot entegrasyonu, sahne hattı, çarpışma | [rapor](docs/research/12-godot-entegrasyonu.md) | [doğrulama](docs/research/dogrulama/12-codex.md) | C/C++ kararından sonra |
| 13 | Donanım yol haritası | [rapor](docs/research/13-donanim-yol-haritasi.md) | [doğrulama](docs/research/dogrulama/13-codex.md) | C/C++ kararından sonra |

## Depo düzeni

```
docs/DURUM.md              projenin şu anki durumu ve sıradaki adım
docs/mimari/               mimari belgeler (iskelet taslak)
docs/research/             ham araştırma raporları ve 00 numaralı düzeltme belgesi
docs/research/dogrulama/   bağımsız doğrulamalar
tools/                     yardımcı betikler
AGENTS.md                  yapay zeka agent'ları için çalışma kuralları
```

Kod dizinleri mimari belge onaylandıktan sonra eklenecek.

## Çalışma yöntemi

Araştırma, bağımsız doğrulama, düzeltme belgesi, mimari ve uygulama sırasıyla ilerler. Araştırma ve uygulama işlerini yapay zeka agent'ları yürütür; her çıktı ikinci bir model tarafından denetlenir. Web araştırmasıyla kapanmayan sorular kart üzerinde ya da prototiple ölçülür. Ayrıntılar [AGENTS.md](AGENTS.md) dosyasında.

Yeni bir raporu doğrulatmak için:

```
tools/codex-dogrula.sh <rapor-no>
```
