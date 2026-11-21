# sol incelemesi: src

## Hüküm

**İncelenen dört emniyet bulgusu kapandı.** Kabul edilen davranıştan daha kötü bir emniyet karşı örneği bulunmadı. İnceleme statiktir; test kaynakları okundu, testler çalıştırılmadı.

- **1 — kapandı.** Bayt içeren pencere ihlali işlendikten sonra oturum kapanıyor (`protocol/ida_link/src/ust_oturum.c:471`). O çağrının çıktısı kuşak denetiminden geçmiyor; sonraki `take`, `SESSION_END` veriyor (`ust_cikti.c:218`, `ust_parti.c:554`). `n=0 rx` ve `rx_tasma` ihlal kapatması üretmiyor; `poll_tx` izinli, `akis_kapandi` oturumu bitiriyor. RUNNING dışında kapatma etkisiz olsa da bu yollar üzerinden eski genişleticinin sonraki çağrılara sızdığı bir durum bulunmadı.
- **2 — kapandı.** Sonuç gönderimi oturumu bitirirse sıkıştırma hemen bırakılıyor; temizlenen kuyruk sayısı geri yazılmıyor (`protocol/ida_link/src/ust_komut.c:108`). Açık/bekleyen parti ve erteleme siliniyor; korunan mandal oturum sonu çağrısında tüketiliyor. Olay tablosunda kapasite nedeniyle kalan **nihai sonuçlar** erişilebilir kalabiliyor; bu A4-K32/K68 gereği, eski olayın yeniden gönderilmesi değil.
- **4 — kapandı.** `bek_sonuc` açık pencerede çekirdeğe verilmiyor; olağan kapanışta etkinleşiyor, DUP/OLD’da siliniyor (`protocol/ida_link/src/ust_cikti.c:85`). Silinince, son geçerlilik dolmamışsa yineleme yeniden başlayabiliyor. Oturum sonunda açık sonuç `SESSION_END` oluyor; sonuç payı dolunca `EXPIRED_NO_ACK` üretilmesi kısıtlayan yönde.
- **6 — kapandı.** Altı komut mesajı ortak nihailik denetimine giriyor. Bozuk ve daraltıcı olmayan biçimler `NARROWED` alıyor; biçimce geçerli daraltıcılar W3-K11 uyarınca mandala ulaşıyor (`protocol/ida_link/src/ust_komut.c:150`). Kuyruk doluluğu bu yolu engellemiyor.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| — | — | Bu dar kapsamda bulgu yok. | — | — |

## Yeniden hesaplananlar

- Komut kapasitesi: **24 − 8 = 16** ertelenmiş giriş. Taşmalı sıkıştırmada temizlenmiş sayı **0** kalıyor.
- Olay sonucu sınırı: **20 + 95 = 115 ms**; çekirdeğin **120 ms** sınırına **5 ms** pay.
- DUP farkı **0**, OLD sınırı **2³¹**. İşaretsiz çıkarımın modüler davranışı birincil kaynaktan doğrulandı: [WG14 N1570 §6.2.5/9](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf).

## İskelete ya da diğer bölümlere notlar

W3-K10 ve W3-K11 esas alındı. W3-K11’in A1-K63/A4-K105 metnine işlenmesi belge eşitlemesi olarak bekliyor. Diğer dört bulgu ve genel inceleme bu hükmün dışındadır; Windows, sanitizer ve kart sonuçları doğrulanmış sayılmamıştır.