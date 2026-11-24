# sol incelemesi: app

## Hüküm

**Kapanmadı — 1 kritik bulgu.** `8d526d6` kritik reddi görünür ve kalıcı hâle getiriyor; ancak yeni kapanış yolu, zorunlu oturum sonu emniyet daraltmasını çekirdeğe teslim etmiyor.

İnceleme yalnız önceki #1 bulguyla sınırlıdır. Dosya değiştirilmedi; testler ve kart ölçümleri çalıştırılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `firmware/app/main.c:718`, `firmware/app/main.c:585`; `firmware/test/test_gonderim_oturum.c:181` | **Kritik ret sonrasında `SESSION_END` çekirdek çağrısı atlanıyor.** Karşı örnek: çekirdek silahlıyken taşıma durur, sekiz kritik yuva dolar ve sonraki çıktı reddedilir. `kapanis.c:29` oturum sonu çağrısını beklemeye alır; hemen ardından `main.c:718` çağırıcıyı güvenli beklemeye geçirir. Sonraki turda `take` oturum sonunu teslim etse de `main.c:586` çekirdeği çağırmadan `put_out(NOT_INIT, NULL)` yapar. Böylece A1-K22/K107 ve A4-K72/K73’ün zorunlu silahsızlandırma ve izin iptali uygulanmaz; çekirdeğin önceki durumu bellekte kalır. Test, gerçek çağırıcı kapısını uygulamadığından bunu yakalayamaz. | Yeni girdileri hemen engelle ve hata mandalını koru. Çalışabilir çekirdeğe tam bir `SESSION_END` tick’i teslim ettikten sonra kalıcı çağrı yasağına geç; işlemi gönderim başarısına bağlama. Gerçek çağırıcı davranışıyla, başlangıçta silahlı durumda bu sıralamayı doğrula. | kritik |

## Yeniden hesaplananlar

- Kritik kapasite **8 çerçeve**. Mevcut bütünleşik senaryoda dolu kuyruğa yeni `SAFETY_CLOSE` ve `ACTUATOR` verilmesi **iki kritik ret** üretir; `kritik_hata` kurulur. Temizleme sekiz bekleyen kritik çerçeveyi `temizlenen` toplamına ekler.
- İmzasız MAVLink 2 üst sınırı **10 + 255 + 2 = 267 B**; sıra alanı sıfır tabanlı **4. bayttadır**. Kodun sınırı ve okuması [birincil MAVLink belgesiyle](https://mavlink.io/en/guide/serialization.html) uyumludur.

## İskelete ya da diğer bölümlere notlar

- **Kuyruk ve yarışlar:** dolu/geçersiz kritik giriş sayılıyor ve mandallanıyor. Sürücü meşgulse çerçeve kuyrukta kalıyor; yazma reddinde `yazma_ret` artıyor. Kuyruk işlemleri ana bağlamda; kesmeler gönderim yöneticisini değiştirmiyor. Temizleme sırasında eşzamanlı ekleme yolu bulunmadı.
- **Kapanış bağımsızlığı:** kapanış gönderimi beklemiyor; yeniden giriş veya sonsuz gönderim döngüsü bulunmadı. IWDG beslemesi güvenli beklemede ilerleme koşulları sağlandıkça sürebiliyor (`saglik.c:65`); eksik çekirdek daraltmasını sıfırlama tamamlamıyor.
- **Öncelik ve kalıcılık:** seçim çerçeve baytlarını/CRC’yi değiştirmiyor; başarılı sürücü kopyasından sonra yuva çıkarılıyor. Çift gönderim yolu bulunmadı. Sürücü DMA tamamlanmasına ek olarak USART `TC` arıyor; bu ayrım [ST’nin U5 sürücüsüyle](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx-hal-driver/main/Src/stm32u5xx_hal_uart.c) uyumlu. Hata mandalları temizlemeyle kalkmıyor; yeni el sıkışma engelleniyor.
- **Fiziksel çıkış:** mevcut kapsamda emniyetle ilgili fiziksel aktüatör çıkışı yok; çıkışlar A1 mesajlarıdır (`docs/mimari/01-kart-yazilimi.md:221`). Fiziksel çıkışların gönderimden bağımsız güvenliği bu kodla doğrulanmış değildir.