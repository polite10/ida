# sol ikinci incelemesi: 00-iskelet

## Hüküm

İlk yedi bulgunun altısı giderildi; üçüncüsü kısmen giderildi. Bariyerli kipte geç yanıtların sıra eşlemesi tamamlanmadan A1’in zamanlama kuralları dondurulmamalı.

## İlk incelemedeki maddelerin durumu

| # | Durum | Not |
|---|---|---|
| 1 | giderildi | §4, `L_act` kaçırıldığında bekleyerek devam edilse bile çalıştırmayı zamanlama bakımından geçersiz sayıyor. |
| 2 | giderildi | Güvence uygulama adımıyla sınırlandırılmış; beklemenin firmware saatlerini durdurmadığı ve kipler arası eşitlik sağlamadığı açık. Ç1 ile uyumlu. |
| 3 | kısmen | Komut ömrü dolunca nötr davranış ve bariyersiz kipte geç yanıtın reddi tanımlı. Bariyerli kipin sıra ilerletme kuralı eksik; Y1. |
| 4 | giderildi | Gecikme nedeni ayrıştırılmamış olarak aktarılıyor; RTT uçları, yazma takvimi aşımları ve temiz host yinelemesi ayrılmış. |
| 5 | giderildi | A2 bütünlemesi ile betik düzeyindeki kamera ön kanıtı ayrılmış; kalan kabul kapıları açıkça belirtilmiş. |
| 6 | giderildi | 9,33 ms ve tahmini 25,17 ms bütçeleri ayrılmış; 50 m yakınsaması ve işçilerin dünya adımına etkisi doğrulanmış sayılmıyor. |
| 7 | giderildi | §3.1 artık çok yollu profilde kutu zamanını nominal doğrudan yol zamanı olarak tanımlıyor. |

## Yeni bulgular

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem | Kapatılacağı yer |
|---|---|---|---|---|---|
| Y1 | `docs/mimari/00-iskelet.md` §4, bekleme ve ret paragrafları | “Beklenen parti”nin nasıl ilerlediği tanımsız. `L_act=2P` ile birden fazla parti uçuşta olabilir; `k` beklenirken gelen eski/gelecek yanıtların ve art arda zaman aşımlarından sonraki ilk geçerli yanıtın uygulanma kuralı tamamlanmamış. | Beklenen sırayı uygulama adımına bağlayın; eski yanıt beklemeyi bitirmesin, gelecek yanıt kendi adımına saklansın, bekleme sınırı aşılmış parti kapansın. Nötrden çıkış da bu kabul kuralını kullansın. | kritik | İskelet §4; A1 sözleşmesi |

## Yeniden hesaplananlar

- **Hat kapasitesi:** `3.000.000 / 10 = 300.000 B/s/yön`; 200 Hz’te `1.500 B/periyot`; %60’ı `900 B`. Doğru.
- **Zaman:** `200 Hz → 5 ms → 4 fizik adımı`; `L_act=10 ms → 8 fizik adımı`. Doğru.
- **Gecikme gerekçesi:** `2,2 + 3,32238125 = 5,52238125 ms > 5 ms`. Aritmetik doğru; metnin belirttiği gibi en kötü durum kanıtı değil.
- **MP4:** 400 Hz ağır yükte `241.000/241.000`, 200 Hz’te `0/121.000` aşım raporla eşleşiyor. 200 Hz koşusunda hesap içeriği ve kontrol `dt` varsayımı değişmemiş; üretim çekirdeğinin yeterliliği kanıtlanmış değil.
- **MP3:** plandan RTT’si 10 ms’yi aşan yanıtlar `4/4/4/2`; yazma takvimi aşımları `2/1/1/0`. En büyük plandan RTT `14,508041 ms`; aktarım doğru.
- **Sonar:** `1200×311×25 ns=9,33 ms`; `1200×2222×25 ns=66,66 ms`; tahmini kısa ping aralığı `9,33+(82,5−66,66)=25,17 ms`. MP7’deki ışın sınırları ve sade kademe süreleriyle eşleşiyor.
- **Kamera ve saat:** `300×30=9.000 kare`; `16/4×80/2=160 MHz`. Doğru.

## İskelete ya da diğer bölümlere notlar

Ö11’in HIL geçersizlik şartı ve Ç1’in duvar saati sınırlaması hedefte karşılanmış. `01`–`09` belgelerindeki eski USB, kontrol hızı ve gecikme hükümleri sözleşmelere doğrudan taşınmamalı; §10 gereği güncel iskelet esas alınmalı.

Canlı birincil kaynaklar [Godot’un `force_draw` ana iş parçacığı şartını](https://docs.godotengine.org/en/4.7/classes/class_renderingserver.html#class-renderingserver-method-force-draw), [ST’nin geçici AHB ÷2 ve 5 µs dizisini](https://community.st.com/stm32-mcus-products-25/rm0456-10-5-4-unclear-restrictions-for-stm32u59x-5ax-5fx-5gx-devices-129223), [14.3.rel1’in Newlib bileşenini](https://github.com/STMicroelectronics/gnu-tools-for-stm32/blob/14.3.rel1/README.md) ve [MAVLink’in 255 bayt yük sınırını](https://mavlink.io/en/guide/serialization.html) destekliyor. Picolibc yokluğunun dayanağı MP1’deki yerel paket denetimidir.

UM2967’nin tam PDF’i zaman aşımı ve boyut sınırı nedeniyle açılamadı; kart bağlantıları bu tam metinden bağımsız doğrulanmış sayılmadı.