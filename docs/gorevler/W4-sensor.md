# W4-sensor — simülatörün sensör modelleri (kamera ve sonar dışı)

Tarih: 2026-11-21. Yazan: `ida-uygulayici` (Opus). İnceleyen: sol (tek tur). Dal: `is/w4-sensor`. Ortak kurallar ve mimar kararları: `docs/gorevler/W3-ortak.md` (W3-K12 ve W3-K14 dahil; W4 işlerinde de geçerlidir).

## Amaç

`sim/sensor/` altında, fizik durumundan zaman damgalı ölçüm üreten sensör kitaplığını (`ida_sensor`) kurmak. Sim tarafındaki A1 köprüsü (sonraki iş) bu ölçümleri mesaja çevirip karta ya da kartsız koşucuya gönderecek; Z2'nin (kartta çalışma) ön koşuludur.

Okunacaklar: iskelet §3.1 (çerçeveler, birimler, zaman), §4, §5 (simüle edilen sensör listesi), §6; `docs/mimari/05-sensorler-ve-sonar.md` §2.1–§2.3, §2.6, §2.7, §3 (sonar §2.4 ve kamera §2.5 hariç); A5 sözleşmesinin sensör ve montaj alanları ile Ek D (rastgelelik sözleşmesi) ve Ek E; A1 §3'ün sensör mesajları ve A4 §3.2 (örnek tipleri, bayraklar, `window_ns`, zaman kuralları): üretilen ölçümlerin alanları ve birimleri bunlarla bire bir eşlenebilmelidir; `sim/fizik/include/ida/fizik/dunya.hpp` (durum anlık görüntüsü; fizik koduna dokunulmaz).

## Kapsam

1. **Sensörler:** IMU (özgül kuvvet ve açısal hız; sapma, sapma kararsızlığı, beyaz gürültü, ölçek ve eksen hatası, doyma, örnekleme penceresi), manyetometre (yerel alan + sabit ve yumuşak demir bozulması, gürültü), basınç/derinlik (yoğunluk ve atmosfer basıncıyla; gürültü, sapma, pencere), GNSS (yalnız anten su üstündeyken geçerli; konum ve hız gürültüsü, fix türü), DVL (dibe kilit koşulu, ışın geometrisi ya da 05'teki sade model, hız gürültüsü, irtifa sınırları, geçerlilik), batarya gerilimi ve akımı (fizik güç modeli yokken: sabit gerilim ve itki isteğinden türetilen basit akım; "tahmin" diye işaretlenir), sızıntı, sanal emniyet girdileri ve fırlatıcı durumu (yetkili durum sim'de tutulur; geri okuma ölçümü üretilir).
2. **Zaman:** her sensörün kendi hızı ve gecikmesi; ölçüm zamanı (`t_meas`) fizik adımına bağlıdır; sensör örneklemesi fizik adımlarının tam katlarında yapılır ve adımlar arasında ara değerleme gerekiyorsa 05 §2.2'deki kuralla. Çıktı, fizik adımı sırasına göre belirlenimcidir.
3. **Rastgelelik:** A5 Ek D: akışlar `(tohum, alt sistem, sensör)` ile anahtarlanır; bir sensörün açılıp kapanması ötekilerin dizisini değiştirmez.
4. **Arıza kancaları:** 05 §2.7'deki sensör arızalarının arayüzü (`arm`/`disarm`/`active`) ve şu arızaların gövdesi: ofset, ölçek, gürültü artışı, donma, kayıp, geç teslim, geçersiz bayrak. Öteki arızalar arayüzde yer tutar.
5. **Arayüz:** `SensorKumesi` sınıfı: kurulum (A5 sensör parametreleri; alan adları A5 ile bire bir; TOML okuma yok), `adim(const DurumAnlik&, adim_no)`, üretilen ölçümlerin sırayla okunması. Ölçüm yapıları A1/A4 alanlarına doğrudan eşlenecek biçimde (birim, çerçeve, bayrak, pencere, örnek numarası). İş parçacığı, saat, dosya ve ağ yok.
6. `ida0` için varsayılan sensör kümesi (A5 Ek E; yoksa 05'teki değerler, "tahmin" işaretiyle).

## Kapsam dışı

Sonar (Ping1D, Ping360) ve kamera; A1 mesaj kodlaması ve köprü; fizik güç modeli; senaryo dosyası okuma; kestirim.

## Dosya sahipliği

Yazılır: `sim/sensor/**`, `sim/CMakeLists.txt`'e tek satır. `sim/fizik/**` değiştirilmez; fizik anlık görüntüsünde eksik bir büyüklük gerekiyorsa en küçük eklemeyi öner ve raporla.

## Kabul (testleri uygulayıcı yazar; CTest)

1. **İdeal eşitlik:** gürültü ve hata kapalıyken her sensörün çıktısı fizik durumundan analitik hesaplanan değere eşittir (durgun, sabit dönüş, sabit ivme, dalış senaryoları); IMU durgun ve düz araçta (0, 0, −g) ölçer (iskelet §3.1 işaret kuralı).
2. **İstatistik:** gürültü açıkken uzun örneklemde ortalama, standart sapma ve (IMU için) Allan sapmasının beyaz gürültü eğimi parametrelerle tolerans içinde.
3. **Zaman:** her sensörün hızı, gecikmesi ve `t_meas` değerleri beklendiği gibi; örnek numaraları kesintisiz.
4. **Geçerlilik:** GNSS dalınca geçersiz, yüzeye çıkınca geçerli; DVL irtifa sınırları dışında geçersiz; doyma bayrakları.
5. **Rastgelelik:** aynı tohumla bit düzeyinde aynı dizi; bir sensörü kapatmak ötekilerin dizisini değiştirmez; farklı tohum farklı dizi.
6. **Arızalar:** her arızanın etkisi ve kalkınca temel davranışa dönüş; arıza akışının tohumdan bağımsız anahtarlanması.
7. **Çekirdekle uyum (bütünleme testi):** üretilen IMU, manyetometre ve basınç ölçümleri A4 örneklerine çevrilip gerçek çekirdeğin kestirimine verildiğinde (durgun ve yavaş dalış), kestirim yönelim ve derinliği sınır içinde bulur. Çevirme testin içinde yazılır (köprü değildir).
8. `host-debug`, `host-asan` (yalnız derleme; yerelde ASan koşmaz), yerel statik analiz gerekmez (C++; `core/` ve `protocol/` dışı) ama W3-K14 taraması zorunlu. Bir fizik adımı başına sensör maliyeti (ölçüm, bu Mac).
