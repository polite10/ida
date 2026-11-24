# W4-sonar-sahne — A7 ağlarının sonar sahnesine yüklenmesi

Tarih: 2026-11-24. Yazan: `ida-uygulayici` (Opus). İnceleyen: sol (emniyet dışı dilim; K23: tek tam inceleme, tek düzeltme turu, dar denetim yok). Dal: `is/w4-sonar-sahne` (taban `origin/main`; W4-bagla-sonar `main`'e girdikten sonra). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `docs/gorevler/W4-ortak.md` (özellikle W4-K1, K18, K24, K27 (5), K29).

## Neden

W4-bagla-sonar'ın sol incelemesi (#4) havuz duvarlarının uyarıyla atlandığını buldu; düzeltme turunda sessiz atlama kaldırıldı ve sonar açıkken şu sahneler açık hatayla reddedilir oldu: `ortam.tur = "havuz"`, statik varlıkta tabandan başka çarpışma ağı, dolu `hedefler`. Sonuç: depodaki havuz senaryoları (`scenarios/z1.toml`, `scenarios/sonar-havuz.toml`, `scenarios/ornek-havuz.toml`) `--sonar` ile çalışmıyor; sonarlı testler bellekte kurulan açık su sahnesiyle koşuyor. Sonar bugün yalnız sonsuz yüzey ve taban düzlemlerini görüyor.

Sözleşme bu konuda açıktır ve değişmez: sonar, varlığın `akustik[]` düğümlerinin ağlarını, liste boşsa `carpisma[]` düğümlerinin **kaynak** ağlarını (dışbükey parçaları değil) birebir kullanır (A7-K27); sim kümeyi çalıştırma başında bir kez okur ve fizik, sonar, hedef şekilleri bu tek yüklemeden kurulur (A7-K50).

## Amaç

Sonar sahnesinin, senaryonun gösterdiği A7 kümesindeki statik varlığın (havuz: taban ve duvarlar) ve hedef varlıklarının üçgen ağlarından kurulması; reddedilen üç durumun kalkması.

Okunacaklar: `docs/sozlesmeler/A7-sahne.md` (özellikle §3 dosya düzeni, A7-K19–K29, K50, K56; hata kodları), `docs/sozlesmeler/A5-senaryo.md`'nin `ortam.sahne` ve `hedefler` bölümleri (A5-K36, K115), `sim/sonar/**` ve `sim/sonar/BENIOKU.md` (sahne arayüzü, ışın izleme, malzeme), `sim/senaryo/src/sonar_yukleyici.cpp`, `docs/gorevler/W4-bagla-sonar.md` ve sol incelemesi (`docs/gorevler/inceleme/W4-bagla-sonar-sol.md`), `assets/ornek/manifest.json`.

## Önce saptanacaklar (rapora yazılır; işin biçimini belirler)

1. `assets/ornek/` kümesinin `.glb` dosyaları depoda yok. Nasıl üretilecekleri (A7'nin öngördüğü hat, bir üretim betiği, derleme sırasında üretim) kod ve belgeden saptanır. Depoda üretim yolu yoksa: `havuz_ornek` ve `hedef_ornek` için ağları belirlenimci üreten küçük bir üretici (derleme sırasında koşan, ikili dosyayı depoya koymayan; manifestteki özet ve boyutla tutarlı) bu işin kapsamındadır. Manifestteki özetlerle tutarlı dosya üretilemiyorsa DUR ve raporla (manifesti değiştirmek A7 uygunluk örneğini değiştirir; mimar kararı ister).
2. Sim'de A7 kümesini okuyan bir yükleyici (GLB ayrıştırma, düğüm/rol denetimi) var mı; fizik bugün çarpışma ağlarını nereden alıyor. Varsa sonar aynı yüklemeyi kullanır (A7-K50); yoksa en küçük GLB okuyucu (yalnız A7'nin izin verdiği alt küme: `mode` = 4, `POSITION` float32, indeksli) `sim/` içinde ortak bir yerde yazılır ve sonar onu kullanır.
3. Sonar modelinin sahne arayüzü üçgen ağ destekliyor mu (W4-sonar'da ne yazıldı).

## Kapsam

1. **Ağ yükleme:** statik varlık ve hedef varlıkları için A7-K27'nin seçtiği ağlar (akustik; yoksa çarpışma kaynak ağları), düğüm dönüşümleri ve senaryodaki pozla dünya çerçevesine taşınır; üçgen başına malzeme A7-K28'den.
2. **Işın izleme:** sonar ışınları üçgen ağlara karşı izlenir. Hızlandırma yapısı (ör. sınır kutusu ağacı) kurulumda kurulur; dünya iş parçacığında koşu sırasında bellek ayrılmaz (W4-K27 (5)). Hareketli hedefler için yapı yeniden kurulmaz; hedef gövde çerçevesinde izlenir (ışın hedef çerçevesine çevrilir).
3. **Reddin kalkması:** havuz sahnesi, tabandan başka çarpışma ağı ve dolu `hedefler` sonar açıkken kabul edilir. Yüklenemeyen ağ (dosya yok, özet tutmuyor, A7 denetimi düşüyor) A7'nin hata koduyla açık hatadır; sessiz atlama yok.
4. **Kalite kademesi (K16, Ö11):** sade kademede ışın sayısı kısılabilir; ağ çözünürlüğü kısılmaz (geometri aynı kalır).
5. **Kapsam sınırı:** hedef varlığı burada yalnız akustik yansıtıcı bir cisimdir. Hedef algılama, sınıflandırma, takip, hedefleme ya da atış kontrolle ilgili hiçbir şey yazılmaz.

## Kapsam dışı

Sonarın bayrak yerine senaryodan açılması (W4-sunucu-kucuk'ta karara bağlanır); kartta sonar işleme; görselleştirme; `core/`, `protocol/`, `firmware/`, `web/`, `tests/kabul/**`; `sim/sunucu/**` ve `sim/kopru/**` (yalnız sonar kurulumundaki üç ret koşulunun kaldırılması için gereken en küçük değişiklik serbest; W4-duraklat aynı anda bu dizinlerde çalışıyor, başka satıra dokunulmaz).

## Kabul

1. **Havuz duvarı:** bilinen ölçülü havuzda duvara dik bakan Ping1D ve Ping360 ışınının menzili geometrik uzaklıkla modelin belgelenmiş hata sınırı içinde; duvara eğik açıda beklenen davranış (modelin yansıma kuralına göre) belgelenir ve sınanır.
2. **Hedef cismi:** bilinen pozdaki hedefin Ping360 taramasında beklenen kerterizde ve menzilde dönüş verdiği; hedef hareket edince dönüşün izlediği.
3. **Sözleşme uyumu (A7-K27):** akustik liste boşken sonarın kullandığı üçgen sayısı çarpışma kaynak ağınınkine eşit (A7-U09'un sonar yarısı).
4. **Senaryolar:** `scenarios/z1.toml --sonar`, `scenarios/sonar-havuz.toml --sonar` çalışır; W4-bagla-sonar'ın bellekte açık suya çeviren test düzeneği gerçek havuz senaryosuna döndürülür (açık su testi ayrıca kalır).
5. **Eşitlik:** sonar kapalıyken bütün çıktılar bu iş öncesiyle bayt bayt aynı (iki ikilinin bir kez karşılaştırılması; W4-bagla-sonar düzeltme turundaki yöntem); `kabul.*` ve `zincir.z1` değişmeden.
6. **Belirlenimcilik:** aynı senaryo ve tohumla iki sonarlı SIL koşusu bayt bayt aynı.
7. **Zamanlama (ölçüm; W4-K24, K29):** havuz sahnesinde Ping1D + Ping360 açık 60 s kartsız HIL: kaçırma sayısı, sonar adımının en uzun ve p50/p99 süresi (bu Mac, release, makine sakinken); sade ve tam kademe ayrı. Ayırma 0 (ayırma testi ağlı sahneyle).
8. `host-debug` + `host-release` tam tur (sonda bir kez); `host-asan` yalnız derleme; W3-K14; MSVC kalıpları.
