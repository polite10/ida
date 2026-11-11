# Codex doğrulaması: 07-hedef-algilama-ve-atis-kontrol

## Hüküm tablosu

İlk 10 satır, raporun Bölüm 5’indeki maddelerle aynı sıradadır. **DOĞRU**, takım raporlarında anlatılan sistemler için kaynak atfının doğruluğunu belirtir; bağımsız deneysel doğrulama anlamına gelmez. Kontrol tarihi: 11 Kasım 2026.

| # | İddia (kısa) | Hüküm | Kanıt (URL) | Not |
|---|---|---|---|---|
| 1 | RoboSub 2025: 51×51×152 mm, ≤0,91 kg, en fazla iki torpido, güvenli hız ve kill switch | DOĞRU | [Resmî el kitabı, §§5.3.1–5.4.2](https://robonation.org/app/uploads/sites/4/2025/07/2025-RoboSub_Team-Handbook-07_21.pdf) | Sınırlar doğru. Bilgisayarın kapatılması gerekmiyor; belge bunu açıkça istisna tutuyor. Boyut/kütle aşımı için ayrıca ceza hükümleri var. |
| 2 | Teknofest 2026: temel kategoride manuel görev, üç renk, beş atış, 20 puan; pnömatik/hidrolik/piroteknik yasak | DOĞRU | [Resmî şartname, §6.2](https://cdn.teknofest.org/media/upload/userFormUpload/2026_%C4%B0NSANSIZ_SU_ALTI_S%C4%B0STEMLER%C4%B0_YARI%C5%9EMASI_-_T%C3%9CRK%C3%87E_-_13.07.2026_jOTfB.pdf) | Başlık “Yarı Otonom” olsa da görev metni açıkça manuel diyor. |
| 3 | Kartta kamera konnektörü/DCMI listesi yok; USB sink-only; VDD 1,8 V | KISMEN DOĞRU | [ST UM2967, erişilen ayna](https://evo.net.ua/content/files/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics-12836994.pdf), [ST kart sayfası](https://www.st.com/en/evaluation-tools/stm32u5a9j-dk.html) | Donanım özellikleri doğru. Buradan “hiçbir gerçek kamera bağlanamaz” sonucu çıkmıyor. Sink, USB güç rolüdür. |
| 4 | `st_yoloxn`: H747’de 357,79 ms; N6570’de 6,63 ms; U5 destek/öneri işareti yok | DOĞRU | [ST’nin erişilen model kartı](https://huggingface.co/STMicroelectronics/st_yolo_x/raw/main/README.md) | Belirtilen model ve çözünürlük için rakamlar mevcut. Bunlar U5 ölçümü veya uçtan uca kamera FPS’i değildir. |
| 5 | Ping360 ışın, menzil ve tarama süreleri | DOĞRU | [Üretici teknik özellikleri](https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/) | 2°×25°, 0,75–50 m; 1 m’de Ethernet 3,42 s, USB 4,17 s, RS485 4,26 s; 50 m’de 33 s. |
| 6 | NASA: ayrı arm/fire, iki adımlı komut inhibit değildir, bağımsızlık ve watchdog ilkeleri | DOĞRU | [NASA-GB-8719.13](https://s3vi.ndc.nasa.gov/ssri-kb/static/resources/nasa-gb-8719.13.pdf) | İlgili ifadeler mevcut. Farklı işlemci/yazılım için belge “genellikle” diyor; asıl ölçüt ortak nedenli tek arızadır. |
| 7 | Stanford 2025: iki torpido, torpido başına azami 75 N yay, tek servo ve mekanik zincir | DOĞRU | [Stanford 2025, §2.1.7](https://robonation.org/app/uploads/sites/4/2025/07/RS25_TDR_Stanford-University-AUV-compressed.pdf) | 75 N, yayın azami kuvvetidir; enerji veya çıkış hızı değildir. |
| 8 | U575I-EV/OV5640: 320×240 RGB565, 12→24 MHz ile 7,5→15 FPS | DOĞRU | [ST teknik moderatörünün makalesi](https://community.st.com/stm32-mcus-60/how-to-improve-dcmi-frame-rate-an-example-using-a-ov5640-camera-module-127871) | Kamera yakalama örneğidir; görüntü işleme süresi ölçümü değildir. |
| 9 | HoloOcean 2.0: Linux/Windows desteği, macOS desteği yok | DOĞRU | [Proje belgeleri](https://byu-holoocean.github.io/holoocean-docs/v2.0.0/index.html) | Belgelenen platform desteği raporla uyumlu. |
| 10 | Stanford 2026: kuru görüntülerde eğitim su altında kötü genelliyor; JTA-GAN kullanılıyor | DOĞRU | [Stanford 2026, §2.1.1](https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_StanfordAUV.pdf) | Takım bunları açıkça bildiriyor. Bu, yöntemin bu projede aynı başarıyı vereceğini kanıtlamıyor. |
| 11 | İTÜ 2024/2025 fırlatıcıları | DOĞRU | [2024 raporu](https://robonation.org/app/uploads/sites/4/2024/07/RS24_TDR_IstanbulTechU.pdf), [2025 raporu](https://robonation.org/app/uploads/sites/4/2025/07/RS25_TDR_Istanbul-Technical-University-AUV-compressed.pdf) | 2024 yay+solenoid; 2025 dikey çift tüp, yay ve manyetik kilit anlatılıyor. |
| 12 | Victoria 2026: yay, çekmeli solenoid, yaklaşık 8 N açma kuvveti | DOĞRU | [Victoria raporu, Torpedo System](https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_Univ-of-Victoria.pdf) | Delrin tüpler, su giriş delikleri ve açılma güvenilirliği iyileştirmeleri de kaynakta var. |
| 13 | Berkeley 2026: yay ve iki su geçirmez servo; nötr yüzerlik | DOĞRU | [Berkeley raporu](https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_UCBerkeley.pdf) | İki servo açıkça belirtiliyor. Bir foot uzaklıktan atış, tasarım hedefi olarak yazılmış. |
| 14 | METU 2026: step motor yerine solenoid, yay; 50+ kuru çevrimde sıkışma | DOĞRU | [METU raporu, §II ve TS-HW-04](https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_MiddleEastTechnicalUniversity.pdf) | Mekanizma ve test atıfları doğru. Ek I’deki sayısal bilgiler raporda atlanmış. |
| 15 | Sherbrooke: dört kam, yaylı piston, Myring gövde, Simulink | DOĞRU | [Sherbrooke raporu](https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_UniversitedeSherbrooke.pdf) | İki bırakıcı ve iki torpido kamı; CFD ve yörünge simülasyonu anlatılıyor. |
| 16 | Brasília: solenoid pimi CO₂ kartuşuna sürüyor | KISMEN DOĞRU | [Brasília raporu, Torpedo](https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_UniversidadeBrasilia_AllBlue.pdf) | Solenoid mandalı bırakıyor; pimi kartuşa süren yaylar. Gazla itiş bilgisi doğru. |
| 17 | NTU: motor, 6 V/5 F süperkapasitör, yaklaşık 15 s itki, reed/tristör, Qi | DOĞRU | [NTU Mecatron raporu](https://robonation.org/app/uploads/sites/4/2025/07/RS25_TDR_Nanyang-Technological-University-Mecatron.pdf) | Reed anahtarı aktivasyon içindir; tüpte nesne varlığını ölçtüğü kaynakta söylenmiyor. |
| 18 | Boulder: anahtar nokta eşleme, PnP ve kamera–tüp ofsetiyle hizalama | DOĞRU | [Boulder 2020, Software Sub-Team](https://robonation.org/app/uploads/sites/4/2020/08/RS20_TDR_UC-Boulder.pdf) | Yöntem açıkça anlatılıyor. |
| 19 | RoboSub 2026 bonus mesafeleri 0,30 ve 0,46 m | DOĞRU | [Resmî görev açıklaması](https://robonation.gitbook.io/robosub-resources/section-3-autonomy-challenge/3.2-task-descriptions) | Metin mesafeleri panodan uzaklık olarak tanımlıyor. Açıklık ölçüleri metinde yok. |
| 20 | Takım raporlarında sayısal atış mesafesi bulunamadı | KISMEN DOĞRU | [Berkeley](https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_UCBerkeley.pdf), [METU Ek I](https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_MiddleEastTechnicalUniversity.pdf) | Ölçülmüş gerçek menzil doğrulanmadı; fakat sayısal tasarım hedefleri ve analitik menziller var. |
| 21 | Bu MCU sınıfında CNN algılama için NPU/GPU gerekir | YANLIŞ | [Edge Impulse FOMO belgeleri](https://docs.edgeimpulse.com/studio/projects/learning-blocks/blocks/object-detection/fomo) | Üretici, 80 MHz Cortex-M4F üzerinde yaklaşık 10 FPS ve <100 KB RAM örneği veriyor. Bu, U5’te aynı hızı garanti etmez. |
| 22 | U5’te YOLO 1–2 s/kare; klasik hat birkaç–20 ms | DOĞRULANAMADI | [ST model kartı](https://huggingface.co/STMicroelectronics/st_yolo_x/raw/main/README.md), [ST kamera örneği](https://community.st.com/stm32-mcus-60/how-to-improve-dcmi-frame-rate-an-example-using-a-ov5640-camera-module-127871) | Kaynaklar U5A9 üzerinde bu algoritmaları ölçmüyor. Saat oranıyla kesin süre çıkarılamaz. |
| 23 | MLPerf Tiny: L4R5, 120 MHz, VWW yaklaşık 230 ms | DOĞRU | [Plumerai’nin sonuç tablosu](https://blog.plumerai.com/2022/11/mlperf-tiny-1.0/) | 230,5 ms STMicroelectronics satırıdır; Plumerai satırı 208,6 ms. U5 için süre vermez. |
| 24 | CA-CFAR eşik formülü Ping360 verisine uygulanabilir | KISMEN DOĞRU | [MathWorks CFAR](https://www.mathworks.com/help/phased/ug/constant-false-alarm-rate-cfar-detection.html), [Ping360 protokolü](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/) | Formül doğru; istatistiksel varsayımları eksik. Ping360’ın u8 dönüş şiddeti örneklerinde istenen Pfa’yı otomatik sağlamaz. |
| 25 | Mahalanobis kapısı, ki-kare eşikleri ve M-of-N onaylama | DOĞRU | [MathWorks trackerGNN](https://www.mathworks.com/help/fusion/ref/trackergnn-system-object.html), [NIST tablosu](https://www.itl.nist.gov/div898/handbook/eda/section3/eda3674.htm) | %99 için 2B: 9,210; 3B: 11,345. Varsayımlar ve kovaryans tutarlılığı ayrıca sağlanmalı. |
| 26 | GNN, Hungarian/Munkres veya açgözlü eşlemeyle aynı şekilde elde edilir | KISMEN DOĞRU | [MathWorks atama yöntemleri](https://www.mathworks.com/help/fusion/ug/introduction-to-assignment-methods-in-tracking-systems.html) | Açgözlü eşleme genel durumda küresel en küçük maliyeti garanti etmez. |
| 27 | Alfa-beta filtresi kararlı durum Kalman’a denktir | KISMEN DOĞRU | [Birincil araştırma, §3](https://www.mdpi.com/2079-9292/14/10/1938) | Uygun model, örnekleme ve kazanç koşullarında. Her alfa-beta seçimi için geçerli değil. Kalata’nın asıl kaynağına erişilemedi. |
| 28 | Quidu/Karoui kaynakları önerilen NED modelini ve tüm teknik ayrıntıları doğruluyor | DOĞRULANAMADI | [Quidu için verilen adres](https://hal-ensta-bretagne.archives-ouvertes.fr/hal-00773673), [Karoui için verilen adres](https://hal.archives-ouvertes.fr/hal-01203532) | Tam metinlere erişilemedi. [Yazarın listesi](https://webperso.ensta.fr/jaulin/publications.html) Quidu makalesinin varlığını doğruluyor; uygulama ayrıntılarını doğrulamıyor. |
| 29 | Dış kalibrasyon yalnız atış çözümünde gerekir; dünya çerçevesi seyrüsefer hatasını ortadan kaldırır | YANLIŞ | [Boulder’ın dönüşüm yaklaşımı](https://robonation.org/app/uploads/sites/4/2020/08/RS20_TDR_UC-Boulder.pdf); aşağıdaki bağımsız dönüşüm hesabı | Sensör–gövde dönüşümü ölçüm modelinde de gerekir. Çerçeve değişimi belirsizliği yok etmez. |
| 30 | Örnekte 2 m uçuş yaklaşık 2 s; son hız 0,44 m/s | DOĞRU | [Sürükleme denklemi](https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/drag-coefficient/); bağımsız hesap | Verilen varsayımlarla 2,031 s ve 0,436 m/s. |
| 31 | %2 yüzerlik hatası: 1 m’de 1,5 cm, 2 m’de 14 cm düşüş | KISMEN DOĞRU | [Sürükleme bağıntısı](https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/drag-coefficient/); bağımsız hesap | Aynı basitleştirmede ve 0,11 kg yanal etkin kütlede yaklaşık 2,43 ve 15,77 cm çıkıyor. Yanal eklenen kütle belirtilmemiş. |
| 32 | 5 cm/s yan akıntı 2 m’de yaklaşık 10 cm sapma oluşturur | KISMEN DOĞRU | [Sürükleme bağıntısı](https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/drag-coefficient/); bağımsız hesap | `akıntı×süre` aritmetiği doğru; torpidonun akıntı hızına anında ulaştığı varsayılıyor. |
| 33 | U5 IWDG zaman aşımı aralığı 16 ms–4096 s | YANLIŞ | [ST U5 IWDG kaynak kodu](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx-hal-driver/main/Src/stm32u5xx_hal_iwdg.c), [başlık dosyası](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx-hal-driver/main/Inc/stm32u5xx_hal_iwdg.h) | Nominal 32 kHz’te yaklaşık 125 µs–131 s. 12 bit sayaç ve azami 1024 bölücü var. |
| 34 | Zamanlayıcıdan dinamik enable üretmek MCU kilitlenince enerjiyi kendiliğinden keser | YANLIŞ | [ST AN4776](https://www.st.com/resource/en/application_note/an4776-generalpurpose-timer-cookbook-for-stm32-microcontrollers-stmicroelectronics.pdf) | Serbest çalışan zamanlayıcı/DMA, CPU ilerlemese de dalga üretmeye devam edebilir. |
| 35 | Oculus M750d: azami 40 Hz, 512 ışın, Ethernet | DOĞRU | [Üretici tablosu](https://www.blueprintsubsea.com/oculus/oculus-m-series) | Azami değerlerdir; her menzil/ayar birleşiminde eşzamanlı performans garantisi değildir. |
| 36 | X-CUBE-STL, STM32U5’i destekler | DOĞRU | [ST açıklaması](https://blog.st.com/x-cube-stl/), [paket veri özeti](https://www.st.com.cn/resource/en/data_brief/x-cube-stl.pdf) | Bu destek, kurulan uygulamanın veya Rust entegrasyonunun SIL sertifikalı olduğu anlamına gelmez. |

## Yanlış ya da yanıltıcı bulunanlar

1. **“Bu kartta gerçek kamera ile algılama yapılamaz.”**  
   Rapor, hazır kamera konnektörü bulunmamasından bütün kamera seçeneklerini eliyor. Doğrusu: kartta belgelenmiş standart kamera bağlantısı yok; buna karşılık SPI/I²C üzerinden tamponlu kamera bağlantısı teknik bir seçenektir. Üretici bu kameraların SPI ve I²C bulunan platformlarda kullanılabildiğini belirtiyor. Seviye uyarlaması, sürücü ve aktarım hızı ayrıca doğrulanmalıdır. DCMI’nin kullanılabilirliği de yalnız konnektör tablosundaki işlev adlarından kesinleştirilemez; alternatif pin işlevleri ve kart bağlantıları gerekir. [ST UM2967](https://evo.net.ua/content/files/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics-12836994.pdf), [Arducam belgeleri](https://docs.arducam.com/Arduino-SPI-camera/Legacy-SPI-camera/Introduction/)

2. **“Sink-only = USB yalnız cihaz kipinde.”**  
   Sink, güç tüketen tarafı tanımlar; veri rolüyle aynı kavram değildir. UM2967 kartın varsayılan olarak USB cihazı olduğunu ve güç alacak şekilde tasarlandığını doğruluyor. Bu kart üzerinde doğrudan UVC host çalışması doğrulanmış değildir; fakat rapordaki terminolojik eşitlik de doğru değildir. [ST USB-C açıklaması](https://www.st.com/content/st_com/en/ecosystems/stm32-usb-c.html), [UM2967 §12.2](https://evo.net.ua/content/files/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics-12836994.pdf)

3. **“TinyML nesne algılama pratik değil; NPU/GPU gerekir.”**  
   Tek bir YOLO modelinden bütün TinyML algılayıcılarına genelleme yapılmış. FOMO belgelerinde 96×96 gri girişli küçük model için 80 MHz Cortex-M4F üzerinde yaklaşık 10 FPS, 100 KB’tan az RAM bildiriliyor. Bunun çıktısı nesne merkezleridir; açıklık konturu veya hassas poz değildir. U5A9 için ölçüm yapılmadan uygunluğu da uygunsuzluğu da kesinleştirilemez. [FOMO teknik açıklaması ve ölçümleri](https://docs.edgeimpulse.com/studio/projects/learning-blocks/blocks/object-detection/fomo)

4. **U5 için süre ve FPS tahminleri yeterince temellendirilmemiş.**  
   ST’nin YOLO süreleri doğru aktarılmış; U5 için 1–2 saniye sonucu ölçülmüş değil. M7/M33 saat oranı; çekirdek, çekirdek kitaplığı, bellek yerleşimi ve derleyici farklarını çözmez. N6’daki 150,74 çıkarım/s de ön işleme, aktarım ve son işlem dahil kamera hızı değildir. [ST model kartı](https://huggingface.co/STMicroelectronics/st_yolo_x/raw/main/README.md)

   Klasik hat için raporun işlem bütçesi de iyimserdir: piksel başına geçiş başına 20–40 çevrim ve 2–3 geçiş kabul edilirse QVGA’da **19,2–57,6 ms** çıkar. Bu hesap bile HSV, morfoloji, etiketleme ve konturun gerçek ölçümü değildir. ST’nin 15 FPS örneği yalnız yakalamayı gösterir. [ST örneği](https://community.st.com/stm32-mcus-60/how-to-improve-dcmi-frame-rate-an-example-using-a-ov5640-camera-module-127871)

5. **“Takım raporlarında sayısal mesafe yok.”**  
   Ölçülmüş gerçek atış menzili ile tasarım hesabı ayrılmalı. Berkeley **1 foot** hedefini yazıyor. METU Ek I; **16,5 mm çap, 38 mm uzunluk, nötr yüzerlikten türetilmiş 8,13 g kütle**, **0,30 m** değerlendirme mesafesi ve **0,60–0,80 m** beklenen menzil içeriyor. Bunlar analitik varsayımlar; tartılmış kütle veya doğrulanmış havuz menzili değildir. RoboSub’ın **0,91 kg** değeri de tipik torpido kütlesi değil, kural üst sınırıdır. [Berkeley](https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_UCBerkeley.pdf), [METU Ek I](https://robonation.org/app/uploads/sites/4/2026/07/RS26_TDR_MiddleEastTechnicalUniversity.pdf), [RoboSub kuralları](https://robonation.org/app/uploads/sites/4/2025/07/2025-RoboSub_Team-Handbook-07_21.pdf)

6. **CFAR formülü doğru, kullanım koşulları eksik.**  
   `α=N(Pfa^(−1/N)−1)` bağımsız, homojen eğitim örnekleri ve tek bakışlı kare-kanun güç istatistiği koşullarına dayanır. Ping360 protokolü yalnız u8 dönüş şiddeti örnekleri tanımlar; bunların gerekli dağılımı taşıdığını garanti etmez. Korelasyon, kazanç, doygunluk ve duvar yankıları varken ayarlanan Pfa ile gerçekleşen yanlış alarm oranı farklı olabilir. Gerçek kayıtlarla kalibrasyon gerekir. [MathWorks](https://www.mathworks.com/help/phased/ug/constant-false-alarm-rate-cfar-detection.html), [Ping360 protokolü](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/)

7. **GNN ve alfa-beta eşdeğerlikleri fazla geniş yazılmış.**  
   Açgözlü atama küresel optimum değildir. Örneğin maliyet matrisi `[[1,2],[2,100]]` için en küçük çifti önce alan yöntem toplam **101**, çapraz atama **4** verir. Alfa-beta da uygun sabit hızlı model ve kararlı durum kazançlarında Kalman’la ilişkilidir; keyfî kazançlarda değil. [Atama yöntemleri](https://www.mathworks.com/help/fusion/ug/introduction-to-assignment-methods-in-tracking-systems.html), [Filtre araştırması](https://www.mdpi.com/2079-9292/14/10/1938)

8. **Dış kalibrasyon ve seyrüsefer belirsizliği yanlış konumlandırılmış.**  
   Rapor dış kalibrasyonu yalnız atış çözümüne bırakıyor. Oysa sensör ölçümünden dünya konumu üretirken de
   \[
   p_W=p_B+R_{WB}(R_{BS}p_S+t_{BS})
   \]
   gerekir. Sensör–gövde dönüşümü burada doğrudan kullanılır. Dünya çerçevesine geçmek aracın bilinen hareketini telafi eder; poz/tutum hatalarını kaldırmaz. Kovaryanslar uygun Jacobian’larla taşınmalı; ortak seyrüsefer hatalarının ölçümler arasında oluşturduğu korelasyon göz ardı edilmemelidir. Bu sonuç bağımsız dönüşüm hesabıdır; Boulder da kamera pozunu gövdeye dönüştürüyor. [Boulder raporu](https://robonation.org/app/uploads/sites/4/2020/08/RS20_TDR_UC-Boulder.pdf)

9. **Uçuş süresi doğru; düşüş ve akıntı sapması tekil sonuçlar olarak desteklenmiyor.**  
   Tatlı su için `ρ=1000 kg/m³` alarak raporun eksenel modelini yeniden hesapladım:
   \[
   A=0{,}000706858\ \mathrm{m^2},\qquad L=1{,}037454\ \mathrm m.
   \]

   | Mesafe | Yeniden hesaplanan süre | Yeniden hesaplanan hız |
   |---|---:|---:|
   | 1 m | 0,560881 s | 1,144210 m/s |
   | 2 m | 2,031453 s | 0,436406 m/s |

   Dolayısıyla rapordaki yaklaşık uçuş süreleri ve hızlar doğrudur. Kullanılan sürükleme bağıntısı standarttır; seçilen `Cd=0,3` ise bu torpido için doğrulanmış katsayı değildir. [NASA sürükleme açıklaması](https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/drag-coefficient/)

   Rapordaki ayrı yanal sürükleme yaklaşımını kullanıp `F=0,02 N`, `m_y=0,11 kg`, `Cdy=1`, `Ay=0,03×0,15 m²` varsayarsak:
   \[
   b_y=2{,}25,\qquad
   z(t)=\frac{m_y}{b_y}\ln\cosh\left(\frac{\sqrt{Fb_y}}{m_y}t\right).
   \]
   Sonuç **1 m’de 2,43 cm**, **2 m’de 15,77 cm** düşüştür. Rapordaki 1,5/14 cm çifti yeniden üretilemiyor. Gerçek yanal eklenen kütle verilmediğinden bunlar da doğrulanmış yörünge değildir.

   Akıntı için `0,05×2,031453=10,16 cm` aritmetiği doğru. Fakat başlangıçtaki yer referanslı yanal hız sıfırsa, torpidonun akıntıya ivmelenmesi gerekir. Aynı bağımsız bileşen modelinde:
   \[
   y(t)=Ut-\frac{m_y}{b_y}\ln\left(1+\frac{b_yUt}{m_y}\right)
   \]
   yaklaşık **4,66 cm** verir. Böylece 10 cm’nin genel bir fiziksel sonuç olmadığı görülür; başlangıç hızı ve hidrodinamik model belirtilmelidir.

10. **IWDG üst süresi yanlış.**  
    Rapordaki **4096 saniye**, U5 donanımıyla uyuşmuyor. ST başlığında 12 bit reload ve azami 1024 bölücü var. Nominal 32 kHz için üst sınır yaklaşık **131 saniye**; gerçek süre LSI toleransına bağlı. [ST sürücüsü](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx-hal-driver/main/Src/stm32u5xx_hal_iwdg.c), [tanımlar](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx-hal-driver/main/Inc/stm32u5xx_hal_iwdg.h)

11. **Dinamik enable önerisi kilitlenmeye karşı yeterli değil.**  
    Rapor, zamanlayıcı sinyalinin MCU kilitlenince duracağını varsayıyor. Donanım PWM’i ve DMA dalga üretimi CPU’dan bağımsız sürebilir. İlerleyen yazılımın yeniden kurmasını gerektiren süreli izin veya bağımsız gözetim olmadan bu özellik garanti edilemez. [ST AN4776](https://www.st.com/resource/en/application_note/an4776-generalpurpose-timer-cookbook-for-stm32-microcontrollers-stmicroelectronics.pdf)

    NASA atıfları genel olarak doğrudur; ancak bakımda çıkarılan fiş/pimlerin, çıkarıldıktan sonra çalışma sırasında bağımsız engelleme sağladığı söylenemez. Bağımsızlık her işletim durumunda incelenmelidir. [NASA §7.4.2 ve sözlük](https://s3vi.ndc.nasa.gov/ssri-kb/static/resources/nasa-gb-8719.13.pdf)

12. **Piksel ölçeği hesabı yaklaşık %11 iyimser.**  
    60° yatay görüş, 320 piksel ve 2 m uzaklıkta standart pinhole modeli:
    \[
    \frac{2R\tan(30^\circ)}{320}=7{,}217\ \mathrm{mm/piksel}
    \]
    verir. 0,6 m pano yaklaşık **83 piksel** genişliğindedir; rapordaki 6,5 mm/piksel ve 90 piksel değil. Bu bağımsız geometrik hesaptır; su altında geçerli kalibrasyon ayrıca gerekir.

## Rapor dışı ek bulgular

- **Stonefish de platform şartını karşılamıyor.** Projenin kendi README’si OpenGL 4.3 istediğini, macOS’u desteklemediğini ve Windows sürümü sunmadığını söylüyor. HoloOcean’ın elenip Stonefish’in doğrudan alternatif kabul edilmesi uygun olmaz. [Stonefish deposu](https://raw.githubusercontent.com/patrykcieslak/stonefish/master/README.md)

- **Kartın tedarik durumu değişmiş.** ST, STM32U5A9J-DK’yi üretimden kalkmış olarak listeliyor. Mevcut kartla HIL çalışmasını engellemez; yeni kart temini ve laboratuvarın çoğaltılmasını etkiler. [ST ürün sayfası](https://www.st.com/en/evaluation-tools/stm32u5a9j-dk.html)

- **Teknofest geometrisi serbest tasarım varsayımıyla sabitlenmemeli.** Şartname organizatörün torpido modellerini öngörüyor. Tam parkur ayrıca MiniROV içeriyor; tek hibrit araç bağlamıyla uyuşmazlık var. Resmî sayfada teknik çizim arşivi yayımlanmış; arşivin içeriğine erişemedim, kesin ölçü/kütleyi doğrulamadım. Operatör onayından sonra gecikmeli otomatik tetiklemenin manuel kurala uygunluğu da doğrulanmış değil. [Şartname](https://cdn.teknofest.org/media/upload/userFormUpload/2026_%C4%B0NSANSIZ_SU_ALTI_S%C4%B0STEMLER%C4%B0_YARI%C5%9EMASI_-_T%C3%9CRK%C3%87E_-_13.07.2026_jOTfB.pdf), [resmî belge bağlantıları](https://www.teknofest.org/tr/yarismalar/insansiz-su-alti-sistemleri-yarismasi/)

- **Ping360 son yaklaşmanın tamamını kapsamaz.** 0,75 m minimum menzil, RoboSub’ın 0,30/0,46 m bonus mesafelerinden büyüktür. Sensör ile tüp referansları da farklıdır; son yaklaşmada alternatif menzil kaynağı gerekir. Protokol mesajında yakalama zaman damgası bulunmaması da rapordaki ortak zaman tabanı için ayrıca çözülmelidir. [Ping360 özellikleri](https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/), [protokol](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/)

- **Sürekli itkili görev nesnesi ayrı model ister.** NTU örneğinde itki yaklaşık 15 saniye sürüyor. Rapordaki yalnız tüpte itki üreten model bu örneği temsil etmez; simülatör fırlatıcı ve nesne türünü ayırmalıdır. [NTU raporu](https://robonation.org/app/uploads/sites/4/2025/07/RS25_TDR_Nanyang-Technological-University-Mecatron.pdf)

- **Yan taramalı sonar çalışmasının kimliği doğrulanabiliyor.** K35’in v1 kaydı 16 Ekim 2024 tarihli ve Agrawal vd. imzalı. Yazarlar yaklaşık %60 göreli AP artışı bildiriyor; bu sonuç Ping360 pano/açıklık algılamasına doğrudan taşınamaz. [Birincil arXiv kaydı](https://arxiv.org/abs/2410.12953v1)

**Erişim sınırları:** Quidu, Karoui ve Kalata’nın raporda verilen asıl tam metinlerine erişemedim; özgül atıflarını doğrulanmış saymadım. RoboSub açıklık CAD klasörü de açılamadı. UM2967’nin ST sunucusundaki PDF’i zaman aşımı/boyut hatası verdi; ST’nin yazdığı Rev.2 ayna kopyasını okuyabildim. İlk Hugging Face adresi açılamadı; ST’nin `st_yolo_x` model kartındaki tablolar okundu. Teknofest şartnamesi resmî yarışma sayfasından açılabildi; RAR teknik çizim arşivi açılamadı.

## Mimari için net sonuçlar

1. **MCU’da durum makinesi, eyleyici denetimi ve takip tutmak makul;** bunun her durumda zorunlu tek mimari olduğu kaynaklarla kanıtlanmış değil.
2. **Algılayıcı sınırını zaman damgalı ölçümde koruyun.** Çerçeve, kalibrasyon sürümü, kovaryans ve zaman belirsizliği mesaj sözleşmesine dahil olsun.
3. **U5 görüntü işleme seçeneklerini ölçümle eleyin.** Klasik hat ve küçük TinyML algılayıcısı için otopilot yükü altındaki gecikme/jitter belirleyici olsun.
4. **Hazır kamera bağlantısı yokluğunu kamera imkânsızlığı saymayın.** Yardımcı bilgisayar seçimi; doğrulanmış aktarım ve işlem gereksinimine dayansın.
5. **Balistik tabloyu yarışma nesnesi ve deney verisiyle üretin.** 2 m uçuş hesabı doğru; düşüş/sürüklenme değerleri tasarım toleransı olarak kullanılamaz.
6. **CFAR ve takip belirsizliklerini gerçek kayıtlarda sınayın.** Ham sensör istatistiği, korelasyon, dış kalibrasyon ve seyrüsefer sıfırlamaları modele girsin.
7. **Emniyet zincirini durum başına inceleyin.** Serbest çalışan PWM’i yazılım sağlığı kanıtı saymayın; watchdog sürelerini düzeltin.
8. **macOS/Windows native şartı simülatör seçiminde eleme ölçütü olsun.** İncelenen HoloOcean ve Stonefish sürümleri bu şartı birlikte karşılamıyor.