# W4-sonar — simülatörün sonar modelleri (Ping1D ve Ping360; sade kademe)

Tarih: 2026-11-21. Yazan: `ida-uygulayici` (Opus). İnceleyen: sol (tek tur). Dal: `is/w4-sonar` (`sim/sensor` `main`'e girdikten sonra `origin/main`'den açılır). Ortak kurallar: `docs/gorevler/W3-ortak.md` ve `docs/gorevler/W4-ortak.md` (W4-K1).

## Amaç

`sim/sonar/` altında, fizik durumundan ve sahneden zaman damgalı sonar profili üreten kitaplığı (`ida_sonar`) kurmak: aşağı bakan Ping1D ve taramalı Ping360, **sade kademe** (kartlı çalışmanın kesinleşmiş varsayılanı; iskelet §8 "Sonar ayrıntısı"). Araç su altında taramayı bu veriyle yapar; karttaki hedef algılama bu profilleri girdi alır (o iş ayrıdır).

Okunacaklar: iskelet §3.1, §4, §6, §8 ("Sonar ayrıntısı" ve **"Sonar saçıcı kuralı"** satırları: 05 §2.4.3'teki kural hatalıdır, iskeletteki kural geçerlidir); `docs/mimari/05-sensorler-ve-sonar.md` §2.2 (zaman), §2.4 (tamamı), §2.6, §2.7, §5; `docs/olcumler/MP7-sonar-suresi.md` ve `olcum/mp7-sonar-suresi` dalındaki prototip (`tools/olcum/` altında; fikir ve ölçüm yöntemi kaynağıdır, ürün koduna kopyalanmaz: yeniden yazılır); A5'in `ping1d` ve `ping360` sensör alanları (§3.6), `ortam` akustik alanları (§3.9), `kalite` sonar alanları (§3.12), Ek D (rastgelelik) ve A5-K78–K81 (ışın sayısı sınırları); A1 §3'ün sonar mesajları (Ping1D profili, Ping360 tarama satırı ve parçalanması) ve A4'ün karşılık gelen örnek tipleri: üretilen yapılar bunlara bire bir eşlenebilmelidir; A7'nin akustik malzeme alanları (A7 §3.4); `sim/sensor/include/ida/sensor/` (rastgelelik akışları, zamanlayıcı, arıza zinciri, `Olcum`): yeniden yazma, kullan.

## Kapsam

1. **Ortam akustiği:** ses hızı (W4-K1: tatlı suda UNESCO/Chen–Millero, deniz suyunda Mackenzie; A5'ten sabit `c` ile geçersiz kılınır) ve soğurma (05 §2.4.2'deki model). Tek bir başlıkta, sensör kümesinin de kullanacağı biçimde. `SensorKumesi::kur`'un `ses_hizi` yokken hata vermek yerine bu işlevi kullanması için `sim/sensor`'a gereken **en küçük** değişiklik (W4-K1) bu işin içindedir; başka `sim/sensor` değişikliği yapılmaz.
2. **Sahne arayüzü:** ışın–sahne kesişimi soyut bir arayüzün arkasındadır. Bu dilimde çözümsel sahne: havuz kutusu (taban, dört duvar, su yüzeyi), düz deniz tabanı ve basit hedef gövdeleri (küre, kutu, silindir; hareketli olabilir), her yüzeyde A7 akustik malzeme parametreleri. A7 ağlarından (glTF) sahne kurma sonraki dilimdir; arayüz onu engellemeyecek biçimde yazılır.
3. **Sade kademe modeli (05 §2.4, iskelet §8):** ışın demeti, yollar, yüzeyin gövdeye sabit hücrelere bölünmesi ve iskeletteki saçıcı kuralı (hücreye düşen ışınların enerji ağırlıkları toplanır, genlik toplamın kareköküdür; faz ve benek `(gövde, hücre, yol türü, gerçekleşim)` dörtlüsünden türer ve ışın sayısından bağımsızdır), biriktirme, alıcı zinciri, gürültü tabanı. Ping başına ışın sayısı A5'ten; kartlı kipte A5-K78–K81 sınırları yükleme tarafının işidir.
4. **Ping360:** tarama geometrisi ve adımı, ping zamanlaması (iletim, uçuş süresi, sonraki açı), örnek sayısı ve menzil ayarları, profil yoğunluk nicemlemesi; ölçüm zamanı yansımanın gerçekleştiği andır (iskelet §3.1). **Ping1D:** mesafe, güven ve (ayara göre) profil; `yuk_ping1d` menzil penceresi.
5. **Zaman, rastgelelik, arıza:** `sim/sensor` altyapısıyla (A5 Ek D akışları; bir sonarın açılıp kapanması ötekilerin dizisini değiştirmez; arıza kancaları: kayıp, donma, gürültü artışı, geç teslim; ötekiler yer tutar).
6. **Arayüz:** `SonarKumesi` (ya da `SensorKumesi`'ne eklenebilir model sınıfları; hangisini seçtiğini gerekçesiyle raporla): kurulum (A5 alan adlarıyla), `adim(durum, adim_no)`, üretilen profillerin sırayla okunması. İş parçacığı, saat, dosya ve ağ yok; işçi iş parçacıklarıyla paralelleştirme sonraki dilimdir (MP7'ye göre tek iş parçacığı sade kademede yeterli: ping başına p99 ≈ 0,9 ms, ölçüm).

## Kapsam dışı

Tam kademe (çok yollu, eşevreli toplam); A7 glTF ağlarından sahne; A1 mesaj kodlaması ve Ping360 parçalaması (köprünün sonraki dilimi); web panelindeki sonar görüntüsü; karttaki hedef algılama ve atış kontrol; kamera. Model torpidonun ya da algılamanın yeteneğini artıracak biçimde "iyileştirilmez": amaç gerçekçi sensör verisidir.

## Dosya sahipliği

Yazılır: `sim/sonar/**`, `sim/CMakeLists.txt`'e tek satır, `sim/sensor`'da yalnız W4-K1'in gerektirdiği değişiklik (ayrı commit). `sim/fizik`, `sim/kopru`, `sim/senaryo`, `core/`, `protocol/` değiştirilmez.

## Kabul (testleri uygulayıcı yazar; CTest)

1. **Geometri:** düz tabana dik bakan Ping1D mesafeyi `c·t/2` ile verir (birkaç derinlik ve eğim); Ping360'ta bilinen konumdaki küre ve duvar doğru açı kutusunda ve menzil kutusunda tepe verir; aracın yönelimi ve montajı değişince tepe beklenen yere kayar.
2. **Enerji ve yakınsama:** ışın sayısı iki katına çıkınca profil enerjisi büyümez (iskelet §8 kuralı; 05 §2.4.3'ün hatası yinelenmez); MP7'nin yakınsama ölçütü 7 m menzilde 5 000 ışınla sağlanır.
3. **Akustik büyüklükler:** ses hızı ve soğurma işlevleri yayımlanmış test değerleriyle (kaynağı raporla); menzille azalma yasası; malzeme parametresinin etkisi (geri saçılım, geliş yasası).
4. **Zaman:** ölçüm zamanı yansıma anı; ping aralığı uçuş süresiyle tutarlı; örnek numaraları kesintisiz; aracın ping sırasında hareketi doğru ele alınır (05 §2.2'deki kural).
5. **Rastgelelik ve belirlenimcilik:** aynı tohumla bit düzeyinde aynı profil; öteki sensörlerin dizisi değişmez; farklı tohum farklı benek.
6. **Arızalar:** her gövdeli arızanın etkisi ve kalkınca temel davranışa dönüş.
7. **W4-K1:** `ornek-havuz` + etkin DVL artık `SensorKumesi::kur`'dan geçer; A5'ten sabit `c` verilince o kullanılır.
8. **Süre (ölçüm, bu Mac):** sade kademede ping başına ortanca ve p99 (7 m ve 50 m; 5 000 ışın), MP7 sonuçlarıyla yan yana; fizik adımı başına maliyet.
9. `host-debug`; `host-asan` yalnız derleme; W3-K14 taraması.
