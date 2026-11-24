# sol incelemesi: app

## Hüküm

**Kapandı.** `93a6e59`, `W3-kart-d2-sol-3.md` içindeki tek kritik bulguyu gideriyor. Belirtilen sıralarda hata mandallandıktan sonra SESSION_END dışında `STEP`, `TICK_TF`, `TICK_GRID` veya komutlu çağrının çekirdeğe ulaştığı bir yol bulunmadı.

Bu, dördüncü dar emniyet denetimidir. `6f9260e` incelenmedi; dosya değiştirilmedi, test veya kart ölçümü çalıştırılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|

Bu kapsamda kalan bulgu yok.

## Yeniden hesaplananlar

- Dört çerçevenin kesilmemiş yükleriyle üst sınırı **22 + 16 + 10 + 17 + 4×12 = 113 B < 1024 B**. Önceki rapordaki TF için 19 B ve toplam 115 B hesabı yanlıştır (`protocol/ida.xml:313`, `:353`, `:373`, `:542`). İmzasız MAVLink 2 ek yükü 12 B’dir. [Birincil kaynak](https://mavlink.io/en/guide/serialization.html)
- `nonce_cek()` beş başarısız platform çağrısından sonra **1** döndürür; ilk başarısızlıkta `rng_hata` mandallanır ve sonraki başarılı okuma bunu kaldırmaz (`protocol/ida_link/src/ust_oturum.c:103`, `firmware/app/main.c:139`). Tohum hatasında okunan RNG değerinin kullanılmaması [ST sürücüsünde](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx-hal-driver/main/Src/stm32u5xx_hal_rng.c) doğrulandı.

## İskelete ya da diğer bölümlere notlar

- **(a) Aynı blok ve ızgara:** Alım/poll sonrasındaki denetim `take` öncesinde kapıyı kuruyor (`firmware/app/main.c:862–869`). HELLO → RNG hatası → ACCEPT(1) → komut → TF oturumu geçici olarak RUNNING yapabilse de denetim partiyi temizleyip SESSION_END hazırlıyor. SESSION_END, TF ve ızgaradan öncelikli; teslimden sonra kapı kalıcı kapanıyor (`firmware/app/kapanis.c:36–39`, `:55–73`; `protocol/ida_link/src/ust_parti.c:554`).
- **(a) Yeniden el sıkışma ve çağrı içindeki hata:** Kurulu oturumun yeniden el sıkışmasında RNG, önceki SESSION_END’in `put_out` işlemi içinde çağrılır. Çağrı sonrası denetim, sonraki çekirdek çağrısından ve gönderim pompasından önce kapanışı tamamlar. `put_out` sırasında mandallanan kritik ret de aynı denetimle yakalanır (`protocol/ida_link/src/ust_oturum.c:204–213`; `firmware/app/main.c:867–906`).
- **(b) Sıra korunmuş:** Yeni denetim çağrı penceresinin dışında; `take → çekirdek → put_out` arasında RX eklenmemiştir. Kritik ret → girdileri engelleme → tek SESSION_END teslimi → yasak sırası korunuyor. Bekleyen sonlandırmayı ikinci denetim yeniden kurmuyor; çalışabilir çekirdekte teslimin yinelendiği veya atlandığı yol bulunmadı (`firmware/app/kapanis.c:16–17`, `:69–73`).
- **(c) Tel çıktısı:** Başarılı el sıkışma izlenimi veren nonce=1 yanıtı ve kabulün RUNNING KEEPALIVE’ı pompadan önce siliniyor. Sonraki IDLE duyurusu **nonce=1, peer_nonce=0, init_rc=NOT_INIT** ile çıkabilir; el sıkışma yanıtı değildir. Kart güvenli IDLE ve kalıcı çağrı yasağında kalır; sonraki ACCEPT işlenmez (`firmware/app/kapanis.c:33–37`; `protocol/ida_link/src/ust_oturum.c:126–143`, `:276`, `:556–567`). Bu davranış A1-K13 ve H33 ile uyumludur.