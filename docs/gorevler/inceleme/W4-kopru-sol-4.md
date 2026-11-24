# sol incelemesi: kopru

## Hüküm

**Kabul edilmemeli.** `c146c5f` ve asıl değişiklik `e664324`, W4-K12/K14/K15 esas alınarak dar kapsamda incelendi.

- **#1 kapandı:** `sim/kopru/src/kopru_parti.cpp:427` uygun geçmiş bulunamadığında güncel revizyonu seçiyor; estop’u VEYA, izin veren bitleri VE yönünde birleştiriyor. Eski revizyonun tahliyesi değişmezi bozmuyor.
- **#2 kapandı:** `sim/kopru/src/kopru_sinir.cpp:153` yanıtı bekletiyor; kabul geçmişine geçiş yalnız toplu doğrulamadan sonra `:246` üzerinden gerçekleşiyor. Sonradan gelen sayılmamış genişletici olay artık geçmişi tutarsızlaştırmak yerine oturumu bitiriyor.
- **#3 kapandı:** `sim/kopru/src/kopru_sinir.cpp:441` genişletici uygulamayı doğrulanmış kümeye bağlıyor. Küçük bildirilen sayıdan sonra gelen yeni genişletici olayın oturumu kapatması, W4-K15’in seçtiği davranışla uyumlu.
- **#11 kapanmadı:** aşağıdaki karşı örnekte topluca reddedilen olay canlılığı yeniliyor.

Dosya değiştirilmedi; derleme ve test çalıştırılmadı. Karşı örnek kaynak akışından çıkarıldı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 11 | `sim/kopru/src/kopru_sinir.cpp:359`; `sim/kopru/src/kopru.cpp:511` | **Kapanmadı: kümeyi son olay tamamladığında toplu ret canlılığa taşınmıyor.** Önce `ACTUATOR(n, n_events=1, IS_SAFE=1)` gelsin; yanıt bekler. Ardından aynı partinin, bağımsız alan denetimleri geçerli `FIRE`’ı gelsin. `kume_tamamla()` tutarsız kümeyi `:236`’da reddeder; fakat dönüşü yok sayılır. Oturum hâlâ çalıştığından `olay_al()` true döner ve `gecerli(true)` canlılığı yeniler. W4-K15(2) ve Tablo 4.6b’nin birlikte ret şartı sağlanmaz. | Olay alımının sonucuna toplu ret kararını taşı; reddedilen kümenin son olayı canlılığı yenilemesin. Mevcut `test_emniyet3.cpp:316` olay→yanıt sırasını sınar; yanıt→olay sırasını da ekle. | kritik |

## Yeniden hesaplananlar

- `P/h = 5/1,25 = 4`; `L_act/h = 10/1,25 = 8` fizik adımı.
- #11’de son geçerli alım 0 ms, reddedilen kümeyi tamamlayan olay 490 ms olsun. 505 ms’de doğru geçen süre **505 ms**, kodun kullandığı süre **15 ms** olur; `T_bağ=500 ms` kapanışı kaçırılır.
- 256 kayıtlık revizyon geçmişinde R’den sonra 256 yeni revizyon eklenince R tahliye edilir; yeni yedek yol R’nin bulunmasına bağımlı değildir.

## İskelete ya da diğer bölümlere notlar

- Sayım ihlali `oturum_bitti()` üzerinden bekleyen olayları temizliyor (`sim/kopru/src/kopru.cpp:151`); izinler sonraki fizik sınırının ilk işleminde kapanıyor (`kopru_sinir.cpp:523`). Aynı sınırda eski oturumun başka genişletici olayı uygulanamıyor.
- Küme kayıtları 256 ile sınırlı; tahliye edilen bekleyen küme genişletici uygulamaya dayanak olmuyor. Hata dönüşü olay beklemiyor; genişletici olayla eşleşen hata yanıtı tutarlılık denetimini geçemiyor.
- SIL tamponundan çıkan olay kendi `batch_seq`’ine sayılıyor (`kopru_sinir.cpp:329`); tamponu boşaltan sonraki ACTUATOR’ın yuvasına aktarılmıyor.
- **Karar notu — bulgu değildir:** A1-K73, hata dönüşünün olaylarını yuvaya bağlamaz. W4-K15’in aynı `batch_seq`’li bütün ızgara dışı kimlikleri sayan tanımı bu istisnayı ayıramıyor: hata yanıtı `n+1`’in taşıdığı `SAFETY_CLOSE(n)`, eksik `n` kümesini tamamlayabilir. Bu karar–sözleşme ayrımı netleştirilmeli.
- MAVLink 2’nin sondaki sıfır baytlarını kırpma kuralı canlı [resmî serileştirme belgesinden](https://mavlink.io/en/guide/serialization.html) doğrulandı. Windows ve kart davranışı bu denetimde ölçülmedi.