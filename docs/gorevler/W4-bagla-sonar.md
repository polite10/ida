# W4-bagla-sonar — sonar modelinin `ida_sim`'e bağlanması

Tarih: 2026-11-24. Yazan: `ida-uygulayici` (Opus). İnceleyen: sol (tek tur; emniyet zincirine dokunan bulgu çıkarsa ikinci tur). Dal: `is/w4-bagla-sonar` (taban: `is/w4-bagla-a3`; o dal `main`'e girdiyse `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `docs/gorevler/W4-ortak.md` (özellikle W4-K1, K14, K18, K24, K25, K26).

**Bu iş, W4-bagla-a3'ün sol incelemesi kapanmadan başlatılmaz** (ikisi de `sim/sunucu` ve `sim/kopru`'ya dokunur).

## Amaç

Sonar modeli (`sim/sonar`, W4-sonar) bugün yalnız kendi testleriyle çalışıyor; `ida_sim` karta sonar verisi göndermiyor ve panellere sonar yayınlamıyor. Bu iş üç ucu bağlar: (1) senaryodan sonar kümesinin kurulması, (2) sonar ölçümlerinin A1 üzerinden karta gönderilmesi (Ping1D ve Ping360; Ping360'ın parçalanması dahil), (3) sonar kanallarının A3'ten yayınlanması.

Okunacaklar: `sim/sonar/include/ida/sonar/**` ve `sim/sonar/BENIOKU.md`; `docs/gorevler/W4-sonar.md` ve sol incelemesi; A1'in sonar mesajları ve parçalama kuralları (`docs/sozlesmeler/A1-kart-baglantisi.md`: Ping1D ve Ping360 mesaj tanımları, A1-K49, A1-K53, parti üyeliği ve boy sınırları, kaybedilebilir sınıf); A5'in sonar ve su bölümleri (`docs/sozlesmeler/A5-senaryo.md` §3.9 `su.ses_hizi`, A5-K41, A5-K78, Tablo 3.10-A sonar arızaları, Ek C `max_ping360_samples`); A3'ün sonar kanallarının yük düzenleri (`docs/sozlesmeler/A3-web.md` §3.4; `sema/a3-kanallar.json`); W4-K1 (ses hızı işlevi sim tarafında), W4-K18 (sade kademe: model 1 200 örnek üretir; doğru değer 600; Ping360 `t_meas` gönderim anı; `gurultu_artisi` yok; çalışma yapılandırması alanı 1.1 konusu); `sim/senaryo`'nun `sensor_kur` arayüzü; `sim/sunucu/**`, `sim/kopru/**`; `web/src/paneller/sonar.ts` (panelin beklediği: yalnız okunur).

## Kapsam

1. **Kurulum:** senaryodaki sonar tanımlarından sonar kümesi kurulur (yalnız doğrulanmış senaryo ağacından; `sensor_kur` ile aynı kural). Ses hızı: senaryo açıkça veriyorsa o, vermiyorsa W4-K1 işleviyle (bugün `sensor_kur`'da bu durum için duran geçici kısıt kalkar). Sahne (dip, duvarlar, hedef cisimleri) senaryonun ortam tanımından gelir; senaryoda karşılığı olmayan sahne öğesi uydurulmaz.
2. **Ölçüm üretimi ve zaman:** sonar ölçümleri fizik sınırındaki sıranın sensör adımında, öteki sensörlerle aynı teslim kurallarıyla (`t_meas`, `t_hazir ≤ t_teslim`: W4-K9; Ping360 `t_meas` gönderim anı: W4-K18) üretilir. Rastgelelik senaryonun tohumundan türeyen ayrı akışlardan gelir (A5 Ek D); sonarın eklenmesi öteki sensörlerin rastgele akışlarını DEĞİŞTİRMEZ (sonarsız senaryoda çıktı bu iş öncesiyle bayt bayt aynı).
3. **A1'e gönderim (köprü):** Ping1D ölçümü tek üye; Ping360 taraması A1'in parçalama kuralıyla parçalara bölünür (parça boyu, sıra, son parça işareti, bir partiye sığmayan taramanın sonraki partilere yayılması; kaybedilebilir sınıfın öncelik ve düşürme kuralı). Parçalama parti bütçesini (bayt ve üye sayısı) aşmaz ve kontrol için gerekli üyelerin (TF, emniyet örneği, öteki sensörler) önüne geçmez. HIL'de tel bütçesi: W4-K16'nın çağrı turu ve A1 §3.9 hizmet eğrisi sınırları; aşım olasılığı varsa ölçümle (kartsız HIL; kart ölçümü ayrı iş) raporlanır.
4. **Kart tarafı:** `core/` ve `firmware/` değişmez. Kartın sonar mesajlarını bugün nasıl karşıladığı (yok sayma, sayma, kullanma) kod okunarak raporlanır; kart bu mesajları reddediyor ya da oturumu bozuyorsa iş DURUR ve mimara raporlanır (karta sonar işleme eklemek ayrı iştir).
5. **A3 yayını:** sonar kanalları (Ping1D menzili ve güveni; Ping360 tarama dilimleri) W4-bagla-a3'ün yayın noktasından, aynı kurallarla (dünya iş parçacığında bellek ayırma ve kilit yok: W4-K27 (5); Ping360 dilimleri için sabit boylu depo) yayınlanır. Panelin (`web/src/paneller/sonar.ts`) beklediği düzenle uyumsuzluk varsa raporlanır; `web/` değişmez.
6. **Arızalar:** A5 Tablo 3.10-A'daki sonar arızalarından sonar modelinin desteklediği kadarı zaman çizelgesinden uygulanır; desteklenmeyen sınıf senaryo yüklenirken açık hatayla reddedilir (sessiz atlama yok).
7. **Kalite kademesi (K16, Ö11):** sade kademede Ping360 örnek sayısı ve tarama hızı kısılabilir; W4-K18'in ara durumu (model 1 200 üretir) BENIOKU'da yazılı kalır; kademe seçimi senaryodan/komut satırından.

## Kapsam dışı

Kartta sonar verisinin işlenmesi (engelden kaçınma, haritalama, hedef algılama); atış kontrol ve hedefleme (bu işte hiçbir biçimde yer almaz); kamera; A3'ten senaryo yükleme; `core/`, `protocol/`, `firmware/`, `web/`; sonar modelinin akustik içeriği (W4-sonar'da yazıldı; eksik görürsen en küçük değişikliği dosya:satır ile öner).

## Dosya sahipliği

Yazılır: `sim/sunucu/**`, `sim/kopru/**` (sonar üyeleri ve Ping360 parçalaması), `sim/senaryo/**` (yalnız sonar kurulum arayüzü ve geçici kısıtın kaldırılması), `sim/sonar/**` (yalnız bağlama için zorunlu en küçük arayüz eklemeleri), `tests/butunleme/**`, `scenarios/` altında yeni örnek senaryo (var olan dosyalar değişmez). `tests/kabul/**` değiştirilmez.

## Kabul

1. **Sonarsız eşitlik:** sonar tanımı olmayan senaryolarda (`scenarios/z1.toml` dahil) CSV ve A1 bayt akışı bu iş öncesiyle bayt bayt aynı; bütün var olan testler değişmeden geçer.
2. **Sonarlı belirlenimcilik:** aynı senaryo ve tohumla iki SIL koşusu bayt bayt aynı; A3 açık/kapalı fark etmez.
3. **Ping1D:** bilinen düz dipli sahnede kartın aldığı menzil, aracın irtifasıyla (sonar eksenine göre) modelin belgelenmiş hata sınırı içinde tutarlı; `t_meas` ve teslim sırası; arıza (kayıp, donma) uygulanınca üyenin davranışı.
4. **Ping360 parçalaması:** her tarama için parçaların birleşimi modelin ürettiği örnek dizisine bayt bayt eşit (alıcı tarafı sahte kartla yeniden birleştirir); parça sırası ve son parça işareti; parti bütçesinin hiç aşılmadığı (her partide denetim); bir parçanın düşürüldüğü durumda (kaybedilebilir sınıf) alıcının taramayı eksik işaretlemesi için A1'in öngördüğü bilgi; kontrol üyelerinin gecikmediği (TF ve emniyet örneği her partide).
5. **A3:** sonar kanallarında yayınlanan değerler karta gönderilenle aynı ölçümden; Node'daki panel A3 istemcisiyle (bütünleme düzeneği) çözülen değerler CSV ile eşleşir; dünya iş parçacığında ayırma 0.
6. **Zamanlama (ölçüm; W4-K24):** 60 s kartsız HIL, Ping1D + Ping360 açık: kaçırma sayısı, parti boyu dağılımı (en büyük parti baytı ve üye sayısı), sonar adımının dünya iş parçacığındaki en uzun süresi (bu Mac; ölçüm olarak raporlanır). Sade kademe ile tam kademe ayrı.
7. `host-debug` + `host-release`; `host-asan` yalnız derleme; W3-K14; MSVC kalıpları; `kabul.kopru.*` değişmeden.
