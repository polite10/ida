# sol incelemesi: kopru

## Hüküm

**Kabul edilmemeli.** `b70fd07` üzerindeki üçüncü dar emniyet denetiminde **#1, #2, #3 ve #11 kapanmadı.** Önceki karşı örneklerin bazıları giderilmiş; aşağıdaki sınır durumları kalıyor.

İnceleme salt okunur yapıldı. Testler çalıştırılmadı; karşı örnekler kaynak akışından çıkarıldı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `sim/kopru/src/kopru_parti.cpp:427`; `sim/kopru/src/kopru_sinir.cpp:340` | **Kapanmadı: geçmişten atılan revizyonda değişmez korunmuyor.** Arama artık örneğin revizyonuyla sınırlı değil; bas–bırak–bas düzeldi. Ancak hiçbir uygun revizyon bulunmadığında, örneğin kendi revizyonu da tablodan silinmişse düzeltme yapılmadan gönderiliyor. Karşı örnek: mevcut `test_emniyet_izin_acik` girdisindeki `(rev=R, PERMIT_OPEN=1)` için R’de izin hiç açılmamış olsun; R, 256 artışla atılsın ve kalan revizyonlarda da izin kapalı olsun. `find(R)` başarısız olur; karta R’nin gerçeğinden gevşek izin biti gider. | Yetkili kaydı bulunamayan çift olduğu gibi gönderilmemeli. Bilinen yetkili revizyonla kısıtlayıcı birleşim kurulmalı; geçmiş tahliyesi karşı örneği eklenmeli. | kritik |
| 2 | `sim/kopru/src/kopru_sinir.cpp:190`; `sim/kopru/src/kopru_sinir.cpp:310` | **Kapanmadı: sonradan tutarsızlaşan yanıt kabul edilmiş geçmişte kalıyor.** Önce `SAFETY_OPEN` ve `n_events=1, IS_SAFE=1` yanıtı gelsin. Yanıt kabul edilip üretim sırası, süreler ve birleşim geçmişine yazılır. Aynı partinin ek `FIRE`’ı gelince yalnız `p.tutarli=false` yapılıyor; `gecen_yanit`, süreler, birleşim ve yuva kabulü kaldırılmıyor. `yanit(n)` tutarsız yanıtı döndürmeye devam eder. | Sonradan tutarsızlık yolu da A1-K109’un etkisizlik şartını sağlamalı. Küme kesinleşmesi ve kabul edilmiş kayıtların yaşam döngüsü birlikte düzeltilmeli. | kritik |
| 3 | `sim/kopru/src/kopru_sinir.cpp:190`; `sim/kopru/src/kopru_sinir.cpp:310`; `sim/kopru/src/kopru_sinir.cpp:437` | **Kapanmadı: küçük bildirilen olay sayısıyla izin erken açılabiliyor.** Aynı partide `SAFETY_OPEN` ve kaybolan `FIRE` varken yanıt `n_events=1, IS_SAFE=1` bildirsin. Tek olay yeterli sayılır; izin `a_n`’de açılır. `FIRE` yinelemesi +15 ms’de gelince küme tutarsızlaşır fakat açılmış izin kapanmaz. Birlikte reddedilmesi gereken kümenin genişletici etkisi kalır. | Yanlış olay sayısı ve sonradan eklenen olay için güvenli küme sonlandırma kuralı uygulanmalı; bu sırada genişletici etkiler kesinleştirilmemeli. Küçük `n_events` ile parçalı teslim testi eklenmeli. | kritik |
| 11 | `sim/kopru/src/kopru.cpp:499`; `sim/kopru/src/kopru_sinir.cpp:190` | **Kapanmadı.** #2’de sonradan tutarsızlaşan yanıt, ilk kabulde canlılığı yeniliyor. Ters yönde de hata var: geçerli `BAD_INPUT, n_events=1` yanıtı kendi partisine bağlı olay bekletiliyor. Oysa A1-K73’e göre hata yanıtı olay beklemeden tamamlanır; `SAFETY_CLOSE` başka partiyi taşıyabilir. Böylece geçerli yanıt canlılığı yenilemeyip yanlış `T_bağ` aşımına yol açabilir. | Hata dönüşlerinin A1-K73 istisnasını koru; canlılık güncellemesini doğru kabul sonucuna bağla. Her iki yön için karşı örnek ekle. | kritik |

## Yeniden hesaplananlar

- `P/h = 5/1,25 = 4`; `L_act/h = 10/1,25 = 8` fizik adımı.
- #3’te izin +10 ms’de açılır. +15 ms’deki yineleme, 20 ms son geçerlilik içindedir; izin kendiliğinden ancak `10+200=210 ms`’de kapanabilir.
- 256 kayıtlık geçmişte, R’den sonra 256 yeni revizyon eklenince R silinir.
- #11’de son geçerli alım 0, hata yanıtı 490 ms olsun: 505 ms’de kod zaman aşımı verir; yanıt sayılmış olsaydı geçen süre yalnız 15 ms olurdu.

## İskelete ya da diğer bölümlere notlar

- Doğru bildirilen `n_events=2` kayıp olay yolu düzelmiş; `SAFETY_CLOSE` küme bekleyişinden önce işleniyor. Küme tablosu 256 kayıtla sınırlı, fakat eksik yanıt kaydı karar verilmeden tahliyeye kadar kalabiliyor.
- `tests/kabul/kopru/kabul_olay.cpp:508` sayılmamış geç olayı bilerek kabul ettiriyor; bu beklenti, yanlış olay sayısında toplu doğrulama şartıyla uzlaştırılmalı.
- `origin/main:docs/gorevler/W4-ortak.md` erişildi; K14 metni bu kopyada yoktu. Kullanıcının verdiği K14 kararı esas alındı; SIL tamponlaması bulgu sayılmadı.
- MAVLink 2’nin sondaki sıfır baytlarını kırpma davranışı canlı [resmî belgeden](https://mavlink.io/en/guide/serialization.html) doğrulandı. Windows ve kart davranışı bu denetimde ölçülmedi.