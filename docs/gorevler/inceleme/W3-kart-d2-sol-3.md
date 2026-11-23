# sol incelemesi: app

## Hüküm

**Kapanmadı — 1 kritik bulgu.** `5a556d4`, özgün kritik-ret senaryosundaki SESSION_END atlamasını gideriyor; ancak istenen bütün-yollar denetiminde **(b) sağlanmıyor:** RNG hatası mandallandıktan sonra olağan çağrı çekirdeğe ulaşabiliyor.

İnceleme belirtilen kapanış zinciriyle sınırlıdır. Dosya değiştirilmedi; test veya kart ölçümü çalıştırılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `firmware/app/main.c:142`, `firmware/app/main.c:809`, `firmware/app/main.c:811`; `firmware/test/test_gonderim_oturum.c:469` | **RNG hatasından sonra SESSION_END dışındaki çağrılara açık pencere var.** Karşı örnek: tek alım bloğunda geçerli `SIM_HELLO` → RNG başarısızlığı → `SESSION_ACCEPT(nonce_board=1)` → komut → TF gelir. `nonce_cek()` beş başarısız okumadan sonra 1 döndürür (`protocol/ida_link/src/ust_oturum.c:103`); kabul çerçevesi oturumu kurar. Ana döngü kapanış denetiminden **önce** `cagri()` çalıştırdığı için komutlu `TICK_TF` çekirdeğe ulaşır. SESSION_END ancak ardından hazırlanır. Mevcut RNG testi, HELLO ile ACCEPT arasına `tur()` koyarak bu sırayı kaçırıyor. | Alım/poll sonrasında, `take` öncesinde hata mandallarını denetleyip kapanış kapısını kur; `put_out` kaynaklı hatalar için çağrı sonrası denetimi koru. Aynı bloktaki karşı örnekte hata sonrasında yalnız SESSION_END’in çekirdeğe ulaşmasını sına. | kritik |

## Yeniden hesaplananlar

- Karşı örneğin dört imzasız çerçevesi, kesilmemiş yüklerle en çok **22 + 16 + 10 + 19 + 4×12 = 115 B**: `B_AYR = 1024 B` içine sığar (`protocol/ida.xml:313`, `:353`, `:373`, `:542`). Çerçeve başına 12 B ek yük [MAVLink birincil belgesiyle](https://mavlink.io/en/guide/serialization.html) doğrulandı.
- Teslim beklerken çekirdek çağrısı hiç gerçekleşmezse **20.000 µs = 20 ms** sonrasında sağlık denetimi beslemeyi keser (`firmware/app/saglik.c:69`). Çalışma IWDG ayarı, nominal 32 kHz için **4×(799+1)/32.000 = 100 ms**’dir (`firmware/bsp/bsp.c:97`); gerçek süre ölçülmedi. Besleme kesilince sıfırlama davranışı [ST’nin U5 sürücüsünden](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx-hal-driver/main/Src/stm32u5xx_hal_iwdg.c) denetlendi.

## İskelete ya da diğer bölümlere notlar

- **(a):** Kritik ret `put_out` sırasında oluştuğunda kapanış çekirdeği hemen yasaklamıyor. Bekleyen SESSION_END öncelikli alınıyor, teslimden sonra yasak başlıyor (`kapanis.c:48`, `:69`). İkinci hata, zaten sonlanan oturum ve aynı turdaki `T_bağ` kapanışında yinelenen teslim veya silahlı çekirdeği kalıcı olarak çağrısız bırakan sıra bulunmadı.
- **(b):** Yukarıdaki RNG karşı örneği nedeniyle sağlanmıyor.
- **(c):** SESSION_END teslimi kuyruk boşluğuna veya taşımanın ilerlemesine bağlı değil. Bekleme evresinde diğer çağrılar kapalıdır; `take` teslimi hiç üretmezse çağırıcı NORMAL kalır ve yukarıdaki ilerleme bekçisi beslemeyi keser (`kapanis.c:57`, `main.c:492`).
- **(d):** Mevcut çalıştırılamazlık nedenleri başlangıçta belirleniyor; bu yollarda silahlandırıcı çağrı yapılmadığından teslimin atlanması güvenlidir (`cagirici.c:50`). Özgün düzeltme A1-K22/K107 ve A4-K72/K73 sırasını koruyor; W3-K10 çağrı penceresinde RX yolu bulunmadı.