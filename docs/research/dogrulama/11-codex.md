# Codex doğrulaması: 11-haberlesme-ve-guc

## Hüküm tablosu

İlk 10 satır, raporun Bölüm 5’indeki maddelerle aynı sıradadır. Raporun tamamı okundu; dosya değiştirilmedi ve yazılım kurulmadı.

| # | İddia (kısa) | Hüküm | Kanıt (URL) | Not |
|---|---|---|---|---|
| 1 | T200: 16 V’ta 1472–1528 µs; +5,25 kgf/23,83 A; −4,07 kgf/24,3 A | KISMEN DOĞRU | [Üretici](https://bluerobotics.com/store/thrusters/t100-t200-thrusters/t200-thruster-r2-rp/), [XLSX](https://cad.bluerobotics.com/T200-Public-Performance-Data-10-20V-September-2019.xlsx) | Ürün sayfası +5,25/−4,1 kgf ve yaklaşık 24 A’yı doğruluyor. XLSX okunamadığından hassas akımlar, PWM aralığı ve diğer tablo satırları doğrulanamadı. |
| 2 | ArduSub GCS: 5 s; 0–4 eylemleri; bağlantı dönünce önceki moda dönmeme | DOĞRU | [ArduSub GCS failsafe](https://ardupilot.org/sub/docs/gcs-failsafe.html) | Hiç GCS bağlanmamışsa etkinleşmiyor. ALTHOLD/SURFACE geçişi başarısız olursa disarm ayrıntısı da var. |
| 3 | ArduSub batarya: varsayılan 10,5 V; 10 s; 0/1/2; yalnız reboot ile sıfırlama | KISMEN DOĞRU | [Sub-4.5 parametre kodu](https://raw.githubusercontent.com/ArduPilot/ardupilot/Sub-4.5/libraries/AP_BattMonitor/AP_BattMonitor_Params.cpp), [Sıfırlama kodu](https://raw.githubusercontent.com/ArduPilot/ardupilot/Sub-4.5/libraries/AP_BattMonitor/AP_BattMonitor.cpp) | 10 s doğru. Genel Sub varsayılanı 0 V; eylemler 0/2/3. Uygun koşullarda reboot olmadan sıfırlama yolu bulunuyor. Wiki eski bilgiler içeriyor. |
| 4 | MAVLink: tipik 1 Hz heartbeat, 4–5 kaçırılınca kopuk sayma | DOĞRU | [MAVLink heartbeat](https://mavlink.io/en/services/heartbeat.html) | RF bağlantılarındaki yaygın uygulama; protokolün zorunlu zaman aşımı kuralı değil. |
| 5 | Fathom-X: HomePlugAV, 100/80 Mbit/s, ≥300 m | KISMEN DOĞRU | [Üretici deposu](https://github.com/bluerobotics/fathom-x), [Ürün özellikleri](https://bluerobotics.com/store/comm-control-power/tether-interface/fathom-x-tether-interface-board-set-copy/) | 100 Mbit/s Ethernet tanımı ile ölçülmüş 80 Mbit/s kullanılabilir bant genişliği çelişmiyor. Menzil kabloya bağlı. |
| 6 | Basic ESC gerilim, akım, PWM ve ısıl kısıtlama özellikleri | DOĞRU | [Basic ESC](https://bluerobotics.com/store/thrusters/speed-controllers/besc30-r3/) | 7–26 V; soğutmaya bağlı 30 A; 1100/1500/1900 µs; ±25 µs; 400 Hz ve belirtilen sıcaklık basamakları doğrulanıyor. |
| 7 | M64: 64 bit/s, yarı çift yönlü, 200 m, yaklaşık 500 ms | DOĞRU | [Water Linked datasheet](https://bluerobotics.com/wp-content/uploads/2019/02/W-MK-19009-1_Modem-M64_Datasheet.pdf) | Tarihsel ürün özellikleri doğru. 300 m çalışma derinliği, 200 m haberleşme menzilinden farklıdır. |
| 8 | Deniz suyunda 433 MHz–2,4 GHz kaybı yaklaşık 10²–10³ dB/m | KISMEN DOĞRU | [ITU-R P.527-6](https://www.itu.int/dms_pubrec/itu-r/rec/p/R-REC-P.527-6-202109-I!!PDF-E.pdf) | Mertebe makul; verilen frekanslara iyi iletken yaklaşımının uygulanması nicel olarak güvenilir değil. Yeniden hesap aşağıda. |
| 9 | Gilbert–Elliott tanımı ve kararlı durum kayıp formülü | DOĞRU | [NTIA özgün teknik çalışması](https://its.ntia.gov/publications/download/TM-23-565.pdf) | \(k,h\) başarı olasılıklarıdır. \(1/r,1/p\) sürelerinin birimi model adımıdır; doğrudan saniye değildir. |
| 10 | Chen–Rincón-Mora: çalışma süresi hatası <%0,4, azami gerilim hatası 30 mV | DOĞRU | [Yazarların makalesi](https://rincon-mora.gatech.edu/publicat/jrnls/tec05_batt_mdl.pdf) | Makalenin kendi deneyleri için doğru. Bu sonuç 1-RC modeline veya araç paketine doğrudan aktarılamaz. |
| 11 | XLSX’in altı sayfası, 201 örneği ve 4 µs adımı | DOĞRULANAMADI | [Üretici XLSX’i](https://cad.bluerobotics.com/T200-Public-Performance-Data-10-20V-September-2019.xlsx) | Dosya içeriğine erişilemedi. Rapordaki “ayrıştırıldı” beyanı bağımsız kanıt değildir. |
| 12 | Anten suya girince bağlantının anında ve tamamen kesilmesi | YANLIŞ | [ITU-R](https://www.itu.int/dms_pubrec/itu-r/rec/p/R-REC-P.527-6-202109-I!!PDF-E.pdf) | Evrensel fizik kuralı değil; özellikle tatlı su ve sığ batma için aşırı genelleme. |
| 13 | Thevenin devresi ve terminal gerilimi denklemi | DOĞRU | [PyBaMM model tanımı](https://docs.pybamm.org/en/latest/source/api/models/equivalent_circuit/thevenin.html) | RC elemanlarının her biri kendi içinde paralel R–C’dir; bu elemanlar seri gerilim düşümleri oluşturur. |
| 14 | 1-RC için yaklaşık 30 mV doğruluk beklentisi | YANLIŞ | [Chen–Rincón-Mora](https://rincon-mora.gatech.edu/publicat/jrnls/tec05_batt_mdl.pdf) | Kaynak model iki RC zaman sabiti kullanıyor; genel doğruluk garantisi vermiyor. |
| 15 | Basic ESC’de programlanabilir düşük gerilim kesmesi yok | DOĞRU | [Üretici personelinin açıklaması](https://discuss.bluerobotics.com/t/lipo-battery-safety/7789) | Raporun K22 bağlantısı bu ifadeyi desteklemiyor; doğru üretici açıklaması başka başlıkta. |
| 16 | Stall akımı ESC’nin 30 A sınırına dayanır | YANLIŞ | [ESC özellikleri](https://bluerobotics.com/store/thrusters/speed-controllers/besc30-r3/), [Firmware kılavuzu](https://bluerobotics.com/wp-content/uploads/2018/10/BLHeli_S-manual-SiLabs-Rev16.x.pdf) | 30 A bir taşıma derecesidir; belgelenmiş elektronik akım sınırlama eşiği değildir. |
| 17 | T200 motorunun içine su girmesi arızadır | YANLIŞ | [T200 tasarımı](https://bluerobotics.com/store/thrusters/t100-t200-thrusters/t200-thruster-r2-rp/) | Motor normalde tamamen suyla dolu çalışır. Yalıtım hasarı ve elektronik muhafazasına su girmesi ayrı arızalardır. |
| 18 | M64 üzerinden MAVLink taşınamaz | YANLIŞ | [M64 protokolü](https://docs.waterlinked.com/modem-m64/modem-m64-protocol/), [MAVLink çerçevesi](https://mavlink.io/en/guide/serialization.html) | Parçalama ve düşük mesaj hızlarıyla taşınabilir; olağan telemetri/heartbeat yükü sığmaz. |
| 19 | Dört T200, 16 V’ta tam gazda 100 A’yı aşar | KISMEN DOĞRU | [T200 elektrik özellikleri](https://bluerobotics.com/store/thrusters/t100-t200-thrusters/t200-thruster-r2-rp/) | Yaklaşık sürekli itici toplamı \(4×24=96\) A. Daha fazlası için yardımcı yükler veya geçiş akımları gerekir. |
| 20 | 266 Wh ve 400 W ile yaklaşık 35 dakika | YANLIŞ | [Batarya özellikleri](https://bluerobotics.com/store/comm-control-power/powersupplies-batteries/battery-li-4s-18ah-r3/) | Verilen varsayımlarla ideal sonuç yaklaşık 40 dakikadır. |
| 21 | Power Sense Module ölçekleri ve 100 A | KISMEN DOĞRU | [PSM üretici sayfası](https://bluerobotics.com/store/comm-control-power/control/psm-asm-r2-rp/) | Ölçekler doğru; 100 A açıkça sürekli olmayan değer olarak belirtiliyor. |
| 22 | Samsung 30Q için kaynaklı Thevenin parametre tablosu bulunmuyor | YANLIŞ | [X-57 çalışması, ekler](https://openmdao.org/pubs/chin_battery_performance_x57_2019.pdf) | Raporun kendi K17 kaynağında OCV, R₀, RTh ve CTh tabloları var. |
| 23 | RFD900x hız/menzil değerleri tek profil olarak kullanılabilir | KISMEN DOĞRU | [Üreticinin V2.0 datasheet’i](https://files.rfdesign.com.au/Files/documents/x%20Series%20Datasheet%20V2.0.pdf) | Hızlar varyant ve firmware’e bağlı. 0,5–1 km “engelli görüş hattı” değeridir; kapalı alan garantisi değildir. |
| 24 | MAVLink çerçevesi düşürmek bütün bağlantı türlerini gerçekçi temsil eder | KISMEN DOĞRU | [TCP standardı](https://www.rfc-editor.org/rfc/rfc9293.html) | Uygulama düzeyinde hata enjeksiyonuna uygun; TCP üzerindeki fiziksel paket kaybıyla aynı davranışı üretmez. |
| 25 | 2,4 GHz deniz suyu menzili yaklaşık 25 cm ölçülmüştür | DOĞRULANAMADI | [Raporda gösterilen tez](https://core.ac.uk/download/11805041.pdf) | Teze erişilemedi; ölçüm koşulları doğrulanamadı. |

## Yanlış ya da yanıltıcı bulunanlar

**1. Radyo sönümü: aritmetik doğru, yaklaşımın kullanımı sorunlu.**

Raporda kullanılan \(\alpha\simeq\sqrt{\pi f\mu_0\sigma}\), yalnız iyi iletken sınırındaki yaklaşımdır. İletim akımı ile dielektrik kaybını birlikte ele almak gerekir. Bu ayrım [Michael Steer’in elektromanyetik alan açıklamasında](https://eng.libretexts.org/Bookshelves/Electrical_Engineering/Electronics/Microwave_and_RF_Design_II_-_Transmission_Lines_%28Steer%29/01%3A_Introduction_to_Distributed_Microwave_Circuits/1.09%3A_Fields_in_Lossy_Mediums) açıkça veriliyor.

\(e^{j\omega t-\gamma z}\), \(\gamma=\alpha+j\beta\) kabulüyle Maxwell denklemlerinden:

\[
\gamma^2=-\omega^2\mu\varepsilon'
+j\omega\mu\sigma_{\rm eff},
\qquad
\sigma_{\rm eff}=\sigma_{\rm DC}+\omega\varepsilon''_{\rm d}
\]

\[
\alpha^2-\beta^2=-\omega^2\mu\varepsilon',
\qquad
2\alpha\beta=\omega\mu\sigma_{\rm eff}
\]

Dolayısıyla:

\[
\boxed{\alpha=
\omega\sqrt{\frac{\mu\varepsilon'}2}
\sqrt{\sqrt{1+\left(\frac{\sigma_{\rm eff}}
{\omega\varepsilon'}\right)^2}-1}},
\qquad
L_{\rm dB/m}=8{,}686\alpha .
\]

Önce yalnız raporun varsayımlarını, \(\sigma=4\) S/m, \(\varepsilon_r=80\), \(\mu=\mu_0\) ve dielektrik kaybı sıfır kabulünü kullanarak yeniden hesapladım:

| Frekans | \(\sigma/(\omega\varepsilon)\) | İyi iletken yaklaşımı, dB/m | Tam iletken-dielektrik formülü, dB/m |
|---|---:|---:|---:|
| 433 MHz | 2,08 | 718 | 569 |
| 915 MHz | 0,98 | 1044 | 668 |
| 2,4 GHz | 0,37 | 1691 | 720 |

İyi iletken koşulu oranın **1’den çok büyük** olmasıdır. 433 MHz’de bile güçlü bir asimptotik yaklaşım yoktur; diğer ikisinde kullanılamaz.

Ardından [ITU-R P.527-6, §5.1.1–5.1.2](https://www.itu.int/dms_pubrec/itu-r/rec/p/R-REC-P.527-6-202109-I!!PDF-E.pdf) kompleks permitivite denklemlerini kullanarak dielektrik kaybını ekledim:

| Ortam ve hesap varsayımı | 433 MHz, dB/m | 915 MHz, dB/m | 2,4 GHz, dB/m |
|---|---:|---:|---:|
| Deniz suyu: 20 °C, 35 ppt; hesaplanan DC iletkenliği ≈4,79 S/m | 663 | 826 | 1084 |
| Saf su: 20 °C | 8,4 | 37,5 | 255,1 |
| Tatlı su örneği: saf su modeline 0,01 S/m DC iletkenlik eklenmiş | 10,2 | 39,3 | 256,9 |
| Tatlı su örneği: saf su modeline 0,05 S/m DC iletkenlik eklenmiş | 17,6 | 46,6 | 264,3 |

Son iki satır **seçilmiş senaryo örnekleridir**, bütün tatlı suların ölçülmüş özellikleri değildir. ITU su modeli −4…40 °C ve 0…40 ppt sınırlarını belirtir.

Bu hesap homojen ortamda düzlemsel dalganın hacimsel sönümüdür; anten uyumsuzluğu, hava–su sınırı, yönlülük ve yüzeye yakın yollar ayrıca modellenmelidir. Dolayısıyla “anten battı ⇒ kalite anında sıfır” ancak seçilmiş kaba senaryo kuralı olabilir. Örneğin 0,01 S/m tatlı su örneğinde 433 MHz’de 10 cm su yolu yaklaşık **1 dB hacimsel kayıp** üretir; bağlantının mutlaka kopacağını söylemez.

**2. ArduSub batarya varsayılanları wiki’den alınmış, kodla sınanmamış.**

Raporda 10,5 V ve `BATT_FS_LOW_ACT=0/1/2` yazıyor. [Sub-4.5 kaynak kodunda](https://raw.githubusercontent.com/ArduPilot/ardupilot/Sub-4.5/libraries/AP_BattMonitor/AP_BattMonitor_Params.cpp):

- 10,5 V varsayılanı Copter/Heli için; genel Sub varsayılanı **0 V**, yani gerilim tetiği kapalı.
- `BATT_LOW_TIMER=10` s.
- `BATT_LOW_MAH`, kritik eşikler ve otomatik failsafe eylemleri başlangıçta sıfır.
- Sub eylemleri **0=otomatik hareket yok, 2=disarm, 3=SURFACE**. Güncel parametre belgesi 0’ı “Warn only” olarak adlandırıyor.

Ayrıca “reboot dışında sıfırlanamaz” mutlak ifadesi doğru değil. `MAV_CMD_BATTERY_RESET`, `reset_remaining_mask()` yolunu çağırıyor; backend koşulları uygun olduğunda failsafe durumu temizlenebiliyor. Gerilimin kendiliğinden toparlanması ile açık sıfırlama komutu aynı şey değildir. [Komut işleyicisi](https://raw.githubusercontent.com/ArduPilot/ardupilot/Sub-4.5/libraries/GCS_MAVLink/GCS_Common.cpp), [Backend sıfırlaması](https://raw.githubusercontent.com/ArduPilot/ardupilot/Sub-4.5/libraries/AP_BattMonitor/AP_BattMonitor_Backend.cpp).

**3. Fathom-X hızları farklı katmanlara ait; menzil koşulsuz değil.**

Raporda 100/80 Mbit/s bir kaynak çelişkisi olarak bırakılmış. Üretici güncel tabloda **200 Mbit/s fiziksel katman**, **80 Mbit/s azami pratik bant genişliği** veriyor; GitHub’daki 100 Mbit/s Ethernet ifadesiyle aynı ölçüt değiller. Aynı tablo 300 m’yi test edilmiş, 2000 m’yi yayımlanmış azami uzunluk olarak ayırıyor; 2000 m’yi araç kablosu için doğrulanmış menzil saymak doğru olmaz. [Fathom-X özellikleri](https://bluerobotics.com/store/comm-control-power/tether-interface/fathom-x-tether-interface-board-set-copy/).

Üreticinin uygulama rehberi standart Fathom için yaklaşık **300 m**, Slim için **200 m** belirtiyor. Model kablo tipini ve uzunluğunu içermeli. [Üretici rehberi](https://bluerobotics.com/learn/building-subsea-cameras/).

**4. T200 tablosunun hassas sayıları bağımsız olarak doğrulanmış değil.**

Üreticinin açık ürün sayfasındaki 16 V değerleri raporun yaklaşık itki/akım değerleriyle uyumlu. Ancak XLSX, web aracında desteklenmeyen dosya türü hatası verdi; doğrudan belleğe okuma girişimleri de ağ erişiminde başarısız oldu.

Bu nedenle **1472–1528 µs**, **23,83 A**, **24,3 A**, altı sayfanın bütün satırları ve 4 µs örnekleme iddiasını doğrulanmış saymıyorum. Basic ESC’nin belgelenmiş **1500±25 µs komut ölü bandı**, gerilime bağlı ölçülmüş sıfır-itki aralığının yerine geçmez. [T200 verisinin yayımlandığı sayfa](https://bluerobotics.com/store/thrusters/t100-t200-thrusters/t200-thruster-r2-rp/).

**5. ESC’nin 30 A derecesi akım sınırlayıcı değildir.**

Rapordaki stall senaryosu akımı “ESC sınırına” getiriyor. Üretici yalnız soğutmaya bağlı 30 A sürekli çalışma derecesi veriyor. Bundan 30 A’da kısma veya kesme sonucu çıkarılamaz. BLHeli_S kılavuzunda başarısız kalkış birkaç saniye sürerse denemeyi durdurup gazın sıfırlanmasını bekleyen stall koruması anlatılıyor. [ESC özellikleri](https://bluerobotics.com/store/thrusters/speed-controllers/besc30-r3/), [Firmware kılavuzu](https://bluerobotics.com/wp-content/uploads/2018/10/BLHeli_S-manual-SiLabs-Rev16.x.pdf).

Programlanabilir düşük gerilim kesmesinin bulunmadığını üretici personeli doğruluyor; fakat **7 V çalışma alt sınırı bir kesme eşiği değildir**. Gerilim çökerken gerçek kapanma, yeniden başlama ve PWM kaybı davranışları bu doğrulamada kesinleştirilemedi. [Üretici açıklaması](https://discuss.bluerobotics.com/t/lipo-battery-safety/7789).

**6. T200’de normal su teması arıza olarak tanımlanmış.**

Raporda “motor içi su alma ⇒ kısa devre” yazıyor. T200 suyla dolu, suyla soğutulan ve suyla yağlanan tasarımdır. Arıza; sargı kapsüllemesinin/yalıtımın bozulması veya kuru elektronik muhafazasına sızıntı gibi somut hasarla tanımlanmalı. [Üretici tasarım açıklaması](https://bluerobotics.com/store/thrusters/t100-t200-thrusters/t200-thruster-r2-rp/).

**7. M64 için kapasite, paket süresi ve gecikme birbirine karıştırılmış.**

64 bit/s net hız, 8 bayt yük ve yaklaşık 500 ms tipik gecikme doğrulanıyor. Ancak \(64/64=1\) s hesabı **sürdürülebilir yük aktarım zamanını** verir; fiziksel akustik paketin havada/suda kalma süresini tek başına belirlemez. Datasheet 500 ms’nin hangi ölçüm uçlarını ve mesafeyi kapsadığını açıklamadığı için bunun üzerine koşulsuz \(r/c\) eklemek çift sayım riski taşır. [Datasheet](https://bluerobotics.com/wp-content/uploads/2019/02/W-MK-19009-1_Modem-M64_Datasheet.pdf), [Protokol](https://docs.waterlinked.com/modem-m64/modem-m64-protocol/).

“MAVLink taşınamaz” da fazla kesindir. İmzasız MAVLink 2 HEARTBEAT, 9 bayt yük + 12 bayt çerçeveyle **21 bayt** tutar. Tek yönde 1 Hz bile **168 bit/s** ister; olağan kullanım sığmaz. Daha seyrek mesajlar parçalanarak taşınabilir. [HEARTBEAT tanımı](https://raw.githubusercontent.com/mavlink/c_library_v2/master/minimal/mavlink_msg_heartbeat.h), [Çerçeve biçimi](https://mavlink.io/en/guide/serialization.html).

**8. Thevenin modelinin doğruluğu ve parametre çıkarımı abartılmış.**

Devre tanımı doğru; fakat Chen’in 30 mV sonucu raporun B1 satırındaki 1-RC modele dayanak değildir. Makale **iki RC zaman sabiti** kullanır ve belirli hücre/deney koşullarını doğrular. [Özgün makale](https://rincon-mora.gatech.edu/publicat/jrnls/tec05_batt_mdl.pdf).

Başlangıçta dengelenmiş 1-RC modeline sabit akım uygulanırsa:

\[
\frac{\Delta V(t)}I=R_0+R_1(1-e^{-t/(R_1C_1)})
\]

Dolayısıyla 10 saniyelik düşüm ancak zaman sabiti yeterince küçükse \(R_0+R_1\)’e yaklaşır. Yük altındaki üretici deşarj eğrisi de doğrudan OCV eğrisi değildir. [X-57 deney ve parametre çıkarımı](https://openmdao.org/pubs/chin_battery_performance_x57_2019.pdf).

**9. Akım ve enerji bütçesinde eksik varsayımlar var.**

Dört adet yaklaşık 24 A iticinin toplamı **96 A**’dır; 100 A üzeri için yardımcı yük veya geçiş koşulu belirtilmelidir.

\[
t_{\rm ideal}=\frac{266{,}4\ {\rm Wh}}{400\ {\rm W}}
=0{,}666\ {\rm h}\approx40\ {\rm dakika}.
\]

35 dakika kullanılabilir enerji rezervi gibi ek varsayım gerektirir. Ayrıca rapordaki enerji eşitliğinin sağ tarafında integral eksiktir: enerji, güçlerin toplamının zaman integralidir. Paket için üretici değerleri 266,4 Wh, **60 A sürekli**, **132 A/10 s** ve **12 V minimum güvenli gerilim**dir. [Batarya datasheet niteliğindeki ürün tablosu](https://bluerobotics.com/store/comm-control-power/powersupplies-batteries/battery-li-4s-18ah-r3/).

**10. Çerçeve kaybı ile fiziksel bağlantı kaybı eşitlenmiş.**

TCP kaybolan ağ paketlerini yeniden göndererek sıralı bayt akışı sağlar. TCP’den çıkarılan MAVLink çerçevelerini proxy’de silmek, aynı fiziksel kaybın üreteceği gecikme ve akış durmasını temsil etmez. Uygulama mesaj kaybı ve taşıma katmanı bozulması ayrı profiller olmalı. [RFC 9293, §2.2](https://www.rfc-editor.org/rfc/rfc9293.html).

## Rapor dışı ek bulgular

**STM32U5A9 brownout eşikleri bulundu.** ST’nin **DS13543 Rev 3, Tablo 35, sayfa 190** değerleri aşağıdadır. Bunlar batarya hattına değil MCU’nun **VDD beslemesine** aittir. [STM32U5Axxx datasheet](https://www.st.com/resource/en/datasheet/stm32u5a9nj.pdf).

| BOR seviyesi | Düşen gerilim: min / tipik / maks, V | Yükselen gerilim: min / tipik / maks, V |
|---|---|---|
| BOR0 | 1,58 / 1,64 / 1,69 | 1,60 / 1,66 / 1,71 |
| BOR1 | 1,90 / 2,00 / 2,10 | 1,98 / 2,08 / 2,17 |
| BOR2 | 2,08 / 2,18 / 2,25 | 2,18 / 2,29 / 2,39 |
| BOR3 | 2,39 / 2,50 / 2,61 | 2,48 / 2,59 / 2,70 |
| BOR4 | 2,67 / 2,79 / 2,90 | 2,76 / 2,88 / 3,00 |

Seçilmiş BOR seviyesi ve regülatörün geçici davranışı bilinmeden bir batarya geriliminden kart reseti türetilemez. Yazılım/NRST reseti yeniden başlama davranışını sınar; gerçek brownout’un elektriksel etkilerini bütünüyle doğrulamaz. ST ayrıca BOR, pin ve yazılım resetleri için ayrı durum bayrakları sunar. [ST RCC belgesi](https://dev.st.com/stm32cube-docs/stm32u5-hal2/2.0.0-beta.1.1/docs/drivers/ll_drivers/rcc/rcc_rmtoll.html).

**30Q için kullanılabilir başlangıç verisi zaten var.** X-57 ekinde 0/20/30/45 °C için OCV, R₀ ve RTh haritaları; CTh için 2000 F veriliyor. Örneğin 20 °C’de haritanın geniş bölümünde R₀=0,025 Ω. Bunlar paket ölçümünün yerine geçmez, fakat “kaynaklı parametre yok” açığını kapatır. Deney matrisi 0…60 °C’dir; bu kaynak −20 °C doğrulaması sağlamaz. [NASA çalışması](https://openmdao.org/pubs/chin_battery_performance_x57_2019.pdf).

**Gilbert–Elliott doğru, zaman ölçeği eksik.** Paket başına ilerleyen modelde \(1/r\), kötü durumda geçirilen ortalama paket adımıdır. Telemetri hızı değişince saniye cinsinden kesinti süresi de değişir. Fiziksel kesintiyi temsil etmek için sabit zaman adımı veya zamana bağlı geçiş hızları gerekir. Kötü durumda kalma süresi, genel modelde ardışık kayıp dizisi uzunluğuyla aynı değildir. [NTIA](https://its.ntia.gov/publications/download/TM-23-565.pdf).

**M64 artık satışta değil.** Yeni donanım hedefi gibi sabitlenmemeli. M16’nın doğrulanan özellikleri 10 bit/s, 1000 m, 16 bit/paket ve 1,6 s/pakettir; üretici ayrıca yaklaşık **0,8 m/s azami bağıl hız** sınırı bildirir. Hibrit aracın hareketli akustik senaryoları açısından bu önemli bir kısıttır. [M64 durumu](https://docs.waterlinked.com/modem-m64/modem-m64/), [M16 özellikleri](https://docs.waterlinked.com/modem-m16/modem-m16/).

**ESC modeli frenleme ve yeniden kalkış durumlarını içermeli.** Üretici firmware belgeleri rejeneratif frenleme ve başarısız kalkıştan sonra nötr komut bekleme davranışını açıklıyor. Yalnız pozitif, durağan akım tablosu hızlı yön değiştirme olaylarını kapsamaz. [BLHeli_S kılavuzu](https://bluerobotics.com/wp-content/uploads/2018/10/BLHeli_S-manual-SiLabs-Rev16.x.pdf).

**Güç hattı ile ölçüm hattının sınırları ayrı.** PSM’nin 100 A değeri sürekli değildir. Model, kablo/regülatör kaybını ve sensörün geçerli ölçüm aralığını ayrı tutmalıdır. [PSM özellikleri](https://bluerobotics.com/store/comm-control-power/control/psm-asm-r2-rp/).

**Bir adım gecikmeli elektrik bağlaşımı koşulsuz kararlı değildir.** Rapordaki “çoğunlukla yeterli ve kararlı” ifadesi kanıtlanmamış. Basit \(V_{n+1}=E-RI(V_n)\) yaklaşımının yerel yakınsama şartı \(|R\,dI/dV|<1\)’dir. Bu, doğrudan türetimdir; yüksek iç direnç ve sert yük değişiminde çözüm kalıntısı denetlenmelidir.

**Erişim sınırları:** T200 XLSX’i ve K6 tezi okunamadı. Eski Water Linked GitHub doküman adresi açılamadı; güncel üretici dokümanı ve PDF okundu. RM0456 tam PDF’i boyut sınırına takıldı; BOR değerleri daha küçük olan datasheet’ten doğrulandı. Denenen MIT ders PDF’i 404 verdi, Utah PDF’i okunabilir metin sağlamadı; hesap bunlara dayandırılmadı. Denenen `Sub-4.5.3` ve `Sub-4.1.2` parametre dosyası adresleri 404 verdi; karşılaştırma erişilebilen `Sub-4.5` dalı ve güncel kaynakla yapıldı.

## Mimari için net sonuçlar

1. **C firmware/ortak çekirdek ve C++ PC kararı korunabilir.** Bulgular dil seçimini değil, model ve doğrulama kapsamını değiştiriyor.
2. Radyo modelinde frekans, sıcaklık, iletkenlik/tuzluluk, su içindeki yol ve anten durumu bulunsun; ikili kopma yalnız kaba profil olsun.
3. Bağlantı emülatörü iki yönü, kuyrukları, serileştirmeyi, paket kaybını ve yeniden bağlanmayı modellemeli; TCP ve seri/UDP davranışları ayrılmalı.
4. ArduSub davranışları sürüm ve gerçek parametre dökümüyle sabitlenmeli. Batarya için rapordaki 10,5 V ve 0/1/2 eşlemesi kullanılmamalı.
5. 1-RC Thevenin uygun başlangıçtır; doğruluğu kendi paketinde ölçülmeli. OCV, sıcaklık, yardımcı yükler ve elektrik bağlaşımı birlikte ele alınmalı.
6. T200 ham tablosu okunup doğrulanana kadar hassas PWM/akım değerleri kesinleştirilmemeli; ESC’nin akım derecesi koruma eşiği yapılmamalı.
7. HIL’de sanal batarya çökmesi, kart reseti ve gerçek besleme çökmesi farklı testler olarak tanımlanmalı; reset testi gerçek BOR doğrulaması sayılmamalı.
8. Gerçekçilik için M64/M16 zamanlaması, ESC düşük gerilim/PWM kaybı ve yüzeye çıkışta radyo toparlanması donanımla ölçülmeli; yayımlanmamış varsayımlar kalibrasyon parametresi olarak kalmalı.