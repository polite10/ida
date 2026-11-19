# İskelet 0.8 — sol incelemesine yanıt

İnceleme: `00-iskelet-08-sol.md` (2026-11-19; 3 kritik, 4 önemli). Yazan: mimar oturum. Yedi maddenin hepsi kabul edildi ve iskelete işlendi; ölçüm rakamı değişmedi.

| # | Durum | Yer | Yapılan |
|---|---|---|---|
| 1 | giderildi | §4 "Geç kalan yanıt", ilk madde | Kendi `L_act` anını kaçıran her yanıt çalıştırmayı zamanlama bakımından geçersiz kılar (Ö11); "kaymalı ama geçerli" ayrımı kaldırıldı. Bariyerde bekleyerek devam etmek ayrı bir davranış olarak tanımlandı |
| 2 | giderildi | §4 "Uygulama gecikmesi sabittir" son cümleleri; "Geç kalan yanıt", ikinci madde | Güvence, kabul edilen komutların uygulandığı fizik adımıyla sınırlandı. Beklemeli çalıştırma için tekrarlanabilirlik ve kipler arası eşitlik iddiası kaldırıldı; beklemenin kartın duvar saatine bağlı davranışlarını durdurmadığı yazıldı (Ç1) |
| 3 | giderildi | §4 "Geç kalan yanıt", üçüncü ve dördüncü madde | Kullanım süresi dolan sürekli komut uygulanmaz, aktüatör modeli nötr duruma geçer; beklenen partiden eski sıra numaralı yanıt reddedilir ve sayılır, geriye dönük uygulama yoktur; bariyersiz kipte aynı kural. Sabit `L_act` güvencesinin geç yanıt istisnası açıkça yazıldı |
| 4 | giderildi | §4 "Geç kalan yanıt" girişi; §8 "A1 gidiş-dönüş süresi" satırı ve tablo altı | Uzun gecikmelerin nedeni "ayrıştırılmadı" olarak aktarıldı; gidiş-dönüşü 10 ms'yi aşan yanıt sayısı (2–4) ile yazma takvimi aşımı (1–2) ayrıldı; temiz koşuldaki yinelemenin açık olduğu yazıldı |
| 5 | giderildi | Sürüm 0.8 özeti; §3 A2 satırı; §8 "Kamera sensörü" satırı | "Kesin (Mac)" yerine "seçilen yol" ve "ön kanıt"; A2 hattının, C++ uzantısının, tam ekran durumunun, zamansal etkilerin ve uzun koşuların ölçülmediği yazıldı |
| 6 | giderildi | §8 "Sonar ayrıntısı" satırı | 9,33 ms alt sınırı ile tahmini 25,17 ms ping aralığı ayrı verildi; sade kademenin yakınsamasının yalnız 7 m'de ölçüldüğü ve işçi iş parçacıklarının etkisinin ölçülmediği eklendi |
| 7 | giderildi | §3.1 akustik ölçüm cümlesi; §10 satır 3 | Çok yollu profilde kutu zamanının nominal olduğu istisnası gövdeye işlendi |

## İkinci inceleme (`00-iskelet-08-sol-2.md`)

İlk yedi maddenin altısı "giderildi", üçüncüsü "kısmen" bulundu; bir yeni kritik bulgu (Y1: bariyerli kipte beklenen partinin nasıl ilerlediği ve geç/erken yanıtların sıra eşlemesi tanımsız). İki inceleme turu dolduğu için düzeltme mimarca doğrudan yapıldı ve üçüncü inceleme istenmedi; kural A1 sözleşmesinin sol incelemesinde yeniden sınanacaktır.

| # | Durum | Yer | Yapılan |
|---|---|---|---|
| Y1 | giderildi | §4 "Geç kalan yanıt": "Yanıtın kabul kuralı", "Uygulama anında yuva boşsa", "Komutun ömrü ve nötr durum" | Beklenen sıra uygulama anına bağlandı: her sıra numarası için tek yuva; erken ve sonraki partilere ait yanıtlar kendi yuvasında bekler; eski ve yinelenen yanıt reddedilir ve beklemeyi bitirmez; bekleme sınırı aşılan parti "kaçırıldı" olarak kapanır; nötrden çıkış aynı kabul kuralını kullanır |

Ek: `L_act` gerekçesindeki toplamın (ortanca gidiş-dönüş + yer tutucunun en büyük adım süresi) en kötü durum kanıtı olmadığı ve değerin W3/W4 ölçümlerinden sonra yeniden değerlendirileceği §4'e yazıldı (inceleme, "Yeniden hesaplananlar").
