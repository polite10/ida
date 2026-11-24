# sol incelemesi: sunucu

## Hüküm

**Emniyet kapsamı KAPANMADI; kabul edilmez.** Uç: `6f1988d`; incelenen fark: `419fadf..6f1988d`. Birleştirmenin yalnız belirtilen çakışma çözümleri değerlendirildi.

Sol-2 #4 kapandı. #5 ve #6 aşağıdaki karşı örneklerle açık kalıyor. Duraklatmaya özgü #1–#3 yeniden açılmadı. İnceleme salt okunurdur; testler çalıştırılmadı, kart ve Windows davranışı deneysel olarak doğrulanmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `sim/sunucu/src/sunucu.cpp:1842`, `:2870`, `:3164`, `:1948` | **EMNİYET — dolu depoda “durdurma kazanır” sağlanmıyor.** Hazırlıkta iki çekişle 512 nötr `param_get` isteği dünya deposunda biriksin. Ardından `hazirlaniyor` revizyonuyla kabul edilen `durdur` genel halkada kalır. Kilit altındaki son çekişte `yer=0` olduğundan alınmaz; `kosuyor` geçişi revizyonu artırır. İlk sınır mevcut 512 isteği işler; sonraki sınır durdurmayı bayat revizyonla reddeder. Koşu başlar. | Hazırlık bitişinde kabul edilmiş çalıştırma denetimlerini, bekleyen diğer isteklerin depo doluluğundan bağımsız tüket. 512 bekleyen istek ve son anda kabul edilmiş durdurma birleşimini sına. | kritik |
| 2 | `sim/sunucu/src/sunucu.cpp:210`, `:221`, `:291`, `:317` | **EMNİYET — kapanışın 500 ms sınırı mutlak değil.** Kapanış yazımı kısmi/0 ilerlerken HIL taşımasının her `oku` çağrısı veri döndürürse `pompa(0)` içindeki döngü bitmez. Okuma turu sınırı yalnız SIL’e uygulanıyor. Dıştaki son tarih denetimine dönülmez; kalan `SESSION_CLOSE` yazımı ve düzgün kapanış süresiz bekleyebilir. Aynı yol yeniden açılmış seri portta da kullanılır. | Kapanış son tarihini iç okuma/yazma hizmetine geçir; HIL’de de her turda denetle ve okuma işini sınırla. Sürekli okunabilir, yazımı ilerlemeyen taşıma karşı örneğini sına. | kritik |
| 3 | `sim/sunucu/src/sunucu.cpp:242`, `:244`; `sim/kopru/include/ida/kopru/tasima.hpp:53`; `sim/kopru/src/tasima_seri.cpp:141`, `:197` | **EMNİYET — HIL’de uygulama tamponunun boşalması telin boşalması sayılıyor.** Son kapanış baytları işletim sisteminin gönderim kuyruğuna kabul edilince yardımcı başarılı döner; seri `kapat_duzgun` doğrudan `close` çağırır. `O_NONBLOCK` yolunda henüz aktarılmamış çıktı kapanışta atılabilir; kart tam kapanış çerçevesini alamaz. Apple’ın yayımladığı [seri kapanış yolu](https://github.com/apple-oss-distributions/IOSerialFamily/blob/main/IOSerialFamily.kmodproj/IOSerialBSDClient.cpp) ve [XNU `ttylclose`/`ttyflush`](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/tty.c) bu olasılığı gösteriyor. `port_ac` da aynı taşıma sınıfını açar. | Seri taşıma için son tarihle sınırlı gönderim boşaltma güvencesi kur; süre dolumunu eksik kapanış olarak bildir. Sınama, yalnız `yaz_biraz` dönüşlerini değil karşı ucun tam çerçeveyi aldığını denetlemeli. | kritik |

## Yeniden hesaplananlar

- `P/h = 5/1,25 = 4` fizik adımı; `L_act=10 ms` → **8 adım**.
- Bulgu 1: **256 + 256 = 512** bekleyen istek; kabul edilmiş **513. istek** olan durdurma son çekişin dışında kalabilir.
- Ağ girişinde hız **0** azami hızdır; en küçük pozitif değer **0,01**: bir kontrol periyodu **500 ms duvar süresi** alır. Daha küçük pozitif değer reddedilir.
- Bir saatlik 10 ms ızgara **360.000 çıktı**, tek bekleyen çıktı halinde **359.999 birleşme** demektir; muafiyet deposu artık süreyle büyümez.

## İskelete ya da diğer bölümlere notlar

**A:** Geçerli HTTP komutları ve doğrudan alıcı girişinde üç duraklatma komutu reddediliyor. WebSocket metin yolu yalnız abonelik yönetiyor; kitaplık kendi başına `duraklatildi` üretmiyor. `--a3-bekle` koşu öncesindedir. Üretimde `emniyet_artimli` çağrısı kalmamış.

**B/E/F:** Muafiyet tutulan çıktıyla taşınıp tüketimde sıfırlanıyor; çift uygulama yolu saptamadım. Mandallar mevcut katalogla **9 etkiyle sınırlı**; dolu etki atlanırken diğerleri ilerliyor. Mandal varken yeni gevşetme reddi, tutarlı kesim ve sıra denetimi korunuyor. Birleştirme sonrası belirtilen sınır sırası W4-K26(1) ve W4-K3’ü koruyor.

**C:** Olağan sınır döngüsü üretici kilidini almıyor; kilit çevrimi veya hazırlıktan ilk sınıra taşınan acil basın kaybı saptamadım. Durdurma için bulgu 1 geçerlidir.

Release/acquire kuralı [C++ taslağından](https://eel.is/c%2B%2Bdraft/atomics.order) canlı denetlendi. Apple modem belgesinin Markdown ucu 404 döndürdü; doğrulanmış kaynak sayılmadı.