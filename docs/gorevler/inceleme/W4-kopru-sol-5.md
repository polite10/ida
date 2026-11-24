# sol incelemesi: kopru

## Hüküm

**Kabul edilmemeli.** `f1125f8`, yalnız istenen iki emniyet noktası bakımından incelendi.

- **(7), #11 kapanmadı:** son çerçevenin ret yolu düzeldi; ancak reddedilen kümenin önceki olayı ve yinelemeleri canlılığı yenileyebiliyor.
- **(6) kapandı:** ACTUATOR sonrası yeni kimlikli SAFETY_CLOSE sayıma girmiyor; geçerli kapanışın uygulanması kümenin doğrulanmasını beklemiyor (`sim/kopru/src/kopru_sinir.cpp:357`, `:441`).

Dosya değiştirilmedi; derleme ve test çalıştırılmadı. Karşı örnek kaynak akışından çıkarıldı. #1, #2, #3 yeniden değerlendirilmedi.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 11 | `sim/kopru/src/kopru_sinir.cpp:317`, `:321`, `:374`; `sim/kopru/src/kopru.cpp:448`, `:511` | **Kapanmadı: topluca reddedilen kümenin olayları canlılıkta iz bırakıyor.** HIL’de son bağımsız geçerli alım 0 ms olsun. 485 ms’de gönderilmiş partinin FIRE olayı 490 ms’de, `ACTUATOR(n_events=1, IS_SAFE=1, ARMED=1, core_rc=OK)` yanıtı 491 ms’de gelsin. FIRE, henüz yanıt bulunmadığından true döner ve canlılığı 490 ms’ye taşır. Yanıt kümeyi topluca reddeder; önceki yenileme korunur. Ayrıca aynı FIRE’ın karar öncesi yinelemesi `karari_bekleyen`, karar sonrası yinelemesi `sakli` yolundan true döner; bilinen toplu ret bile canlılık yenilemesini engellemez. | Sayılan olayların canlılık etkisini toplu doğrulamaya bağla; reddedilen kümeye ait yinelemelerde de ret durumunu koru. Olay→yanıt sırasını, eksik kümenin önceki üyelerini ve ret sonrası yinelemeyi sınayan testler ekle. Geçerli küme, bağımsız geçerli trafik ve SAFETY_CLOSE yollarının canlılığı yenilediğini koru. | kritik |

## Yeniden hesaplananlar

- Karşı örnekte `a_n = 485 + 10 = 495 ms`; iki çerçeve de uygulama sınırından önce gelir. `apply_by=20 ms` için olayın uygulama süresi de geçmemiştir.
- 505 ms’de doğru geçen süre **505 ms ≥ T_bağ=500 ms**; kodun kullandığı süre **505−490=15 ms** olur. Oturum kapanışı kaçırılır.
- (6): `n_events=2`, alınmış bir genişletici olay ve ACTUATOR sonrası yeni SAFETY_CLOSE için sayım **1+0=1<2** kalır; kapanış eksik kümeyi tamamlamaz.

## İskelete ya da diğer bölümlere notlar

- SIL tamponu sonraki ACTUATOR işlenmeden önce boşaltılır (`sim/kopru/src/kopru.cpp:348`). Böylece tamponlanan SAFETY_CLOSE(n), önceden alınmış ACTUATOR(n)’in `act_var` durumunu görür; HIL doğrudan aynı denetime gider. Genişletici uygulama doğrulanmış kümeye bağlı kalır (`kopru_sinir.cpp:453`).
- Yeni ret koşulu yalnız reddedilen geçişte false döndürüyor; geçerli trafik için yeni bir yanlış T_bağ aşımı yolu saptanmadı.
- Yeni `test_emniyet4.cpp:388` son olayın ret yolunu kapsıyor; tablodaki önceki olay/yineleme yollarını kapsamıyor.
- MAVLink 2’nin sondaki sıfır baytlarını kırpma kuralı canlı [resmî kaynaktan](https://mavlink.io/en/guide/serialization.html) doğrulandı.