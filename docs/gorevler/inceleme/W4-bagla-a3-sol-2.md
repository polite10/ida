# sol incelemesi: sunucu

## Hüküm

**Emniyet kapsamı KAPANMADI; kabul edilmez.** İncelenen uç `419fadf`, düzeltme farkı `8ccd678..419fadf`.

Önceki bulgular: **#1 kapandı** (senaryo yeniden yürütülmüyor); **#2 kapandı** (kesim sırası düzeldi); **#3 kapanmadı** (aşağıda 4); **#4 kapandı** (duraklatmada alınan yer istasyonu komutları tele verilmiyor); **#6 kapanmadı** (aşağıda 5–6). **#5**, emniyetle ilgili aktarım bakımından kapandı. **#7**, belirtilen eski test boşlukları bakımından kapandı; aşağıdaki birleşik durumlar kapsam dışı kalmış.

İnceleme salt okunurdur; testler, kart ve Windows üzerinde çalıştırma yapılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `sim/sunucu/src/sunucu.cpp:1967`, `:1968`, `:3357` | Aynı duraklatma yoklamasında **acil bas + sürdür/adımla** alınırsa çalıştırma denetimi önce duraklatmayı kaldırır; `a3_artimli` atlanır. Nötr dışı girdisi önceden hazırlanmış **b adımı**, basma işlenmeden koşar. SIL ve HIL etkilenir. | Çıkış yoklamasının emniyet kesimini fizik ilerlemeden tamamla; gerekiyorsa b’nin girdisini nötrle. Senaryoyu yeniden yürütme. | kritik |
| 2 | `sim/sunucu/src/sunucu.cpp:1966`; `sim/sunucu/src/a3_baglanti.cpp:186`; `sim/a3/src/cekirdek.cpp:1851`, `:1862`, `:2134` | Duraklatmada **DISARM → ikinci DISARM → acil bas**: ilk istek atlanan yuvada kalır; ikincisi kitaplık mandalına gider. Mandalın başındaki dolu yuva ilerlemeyi durdurur; farklı etkideki acil bas da arkasında bekler. Sürdür/durdur ise `mesgul` alır. Emniyet isteği süresiz bekleyebilir. | Daraltıcıları tele çıkarmadan, etki başına sınırlı depoya aktararak alıcı yuvalarını boşalt; çalıştırma denetiminin ilerlemesini koru. Gerçek kitaplık yoluyla bu diziyi sına. | kritik |
| 3 | `sim/kopru/src/kopru_sinir.cpp:678`; `sim/sunucu/src/sunucu.cpp:2041`, `:1979` | Artımlı işlem A1-K85’in oturum sonu, kart olayları ve sürekli çıktı adımlarını yürütmüyor. HIL duraklatmasında alınan `SESSION_CLOSE`, izinleri/revizyonu hemen kapatmıyor; eski revizyonlu panel gevşetmesi kabul edilebiliyor. `SAFETY_CLOSE` ve güvenli `ASYNC` de bekliyor. Sürdürmede hazırlanmış b girdisi bu kapanışlar uygulanmadan koşabiliyor. | Yeni gelen emniyet etkilerini panel isteğinden bağımsız tüket; kapanışları ve güvenli çıktıyı gevşetmeden/fizikten önce uygula. Sınırın olağan işlemini yineleme. | kritik |
| 4 | `sim/kopru/src/kopru_sinir.cpp:132`, `:145`; `sim/kopru/src/kopru_ic.hpp:240` | Son `ASYNC` vektörde tekilleştiriliyor, fakat ilk olağan yanıttan önceki her `batch_seq=0` çıktı **`esz_muaf` kümesine ekleniyor**. İlk TF’den önce, örneğin b=0’da uzun HIL duraklatması, belleği süreyle büyütür. Yeni test önce üç periyot gönderdiğinden bunu kaçırıyor. | Muafiyeti tutulan tek çıktının yanında sakla; birleşen çıktıların kimliklerini biriktirme. İlk TF öncesini de sına. | kritik |
| 5 | `sim/sunucu/src/sunucu.cpp:643`, `:666`, `:3247`, `:1851` | Hazırlığın son yoklamasından sonra, son `pompa` sırasında `hazirlaniyor` revizyonuyla kabul edilen `durdur` çekilmiyor. El sıkışma tamamlanınca `kosuyor` revizyonu artırıyor; ilk sınırda durdurma bayat sayılıyor ve koşu başlıyor. | Hazırlık tamamlanması ile kabul edilmiş durdurmanın sahipliğini birlikte çöz; durdurmayı iç geçişle geçersizleştirme. Son el sıkışma sırasında enjeksiyonla sına. | kritik |
| 6 | `sim/sunucu/src/sunucu.cpp:3225`, `:3226`, `:3232`, `:169` | Hazırlık iptalinde `SESSION_CLOSE` için yalnız bir `yaz()` çağrılıyor. Taşıma o anda 0/kısmi yazım döndürürse kalan kapanış tamponda kalıyor; erken dönüş yolu tamponu boşaltmıyor ve normal düzgün kapanışı çağırmıyor. Kartın kapanışı aldığı güvence altında değil. | Mutlak süre sınırıyla kalan çıkışı tamamla ve taşımanın düzgün kapanışını yürüt; ilerlemeyen yazımı açıkça sonuçlandır. | kritik |

## Yeniden hesaplananlar

- `h=1,25 ms`, `P=5 ms`: **4 fizik adımı/periyot**; `b=4800` → **6 s**; `L_act=10 ms` → **8 adım**. Bulgu 1’de eski girdi bir adım, **1,25 ms** daha uygulanabilir.
- 10 ms ızgarada bir saat **360.000 çıktı** demektir. Vektör 1 kayıtta kalsa da bulgu 4’te küme bu sayıda farklı kimlik tutabilir.
- Dünya beklemesi 512, genel halka 256: **768 genel istek** birikebilir. Dünya deposu dolunca halka tüketilmez; halka da dolduğunda sürdür/durdur kabul edilemez. Kısıtlayanlar genel halkada düşmez, ancak bulgu 2’de mandalda kilitlenir.

## İskelete ya da diğer bölümlere notlar

`sinir_isle` ve artımlı işlem gerçekten `kisitla`, `operator_kisitla`, `operator_gevset`, `estop_isle` yardımcılarını paylaşıyor. Doyma engeli korunuyor; sim zamanı donduğundan yeni izin süresi dolumu oluşmuyor. Ayrı yoklamalarda işlenen bas→bırak→sürdür yolunda W4-K3 korunuyor; çıkış yoklaması istisnası bulgu 1’dir.

Kesim ve sıra denetiminde ayrıca kayıp/çift uygulama yolu saptamadım. Son güvenli çıktının birleştirilmesi, üretim sırası penceresinde A1-K78’in iptal kapsamını daraltmıyor; uygulanmasının gecikmesi bulgu 3’tür.

Release/acquire kuralı [C++ taslağından](https://eel.is/c%2B%2Bdraft/atomics.order), kısmi yazım olasılığı [Apple belgesinden](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/write.2.html) canlı doğrulandı. OpenGroup sayfaları 403 döndürdü; doğrulanmış kaynak sayılmadı.