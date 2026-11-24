# sol incelemesi: sunucu

## Hüküm

**Emniyet kapsamı KAPANMADI.** İncelenen uç `502cd28`, fark `fa12abe..502cd28`.

Önceki üç bulgu: **#1 kapandı** (`sim/sunucu/src/sunucu.cpp:353,781`); **#2’nin hazırlık karşı örneği kapandı** (özyineleme kaldırılmış, ortak son tarih ve beş deneme sınırı var); **#3 kapanmadı** (aşağıdaki #1 ve #3). Koşu ortasındaki yeniden kurmada ayrıca sınırsız bekleme kalıyor.

İnceleme statiktir; dosya değiştirilmedi, test çalıştırılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `sim/sunucu/src/sunucu.cpp:1583`, `:93`, `:1810` | **EMNİYET:** Oturum kurulduktan sonra `satirlar.reserve()` bellek ayıramazsa `std::bad_alloc` dışarı çıkar; kapanış koruyucusu/yakalayıcı yoktur. Yıkıcı yalnız karşı taraf iş parçacığını durdurur; `SESSION_CLOSE` ve düzgün TCP kapanışı atlanır. Taşıma çağrılarından yükselen istisnalar da korunmuyor. | Oturumun bütün yaşamını kapsayan, istisna sırasında da çalışan ve tahsis başarısına dayanmayan kapanış koruyucusu kullan; hata enjeksiyonuyla doğrula. | kritik |
| 2 | `sim/sunucu/src/sunucu.cpp:667`, `:674`, `:1659` | **EMNİYET / sınırsız bekleme:** Koşu ortasında yeniden kurmaya girildikten sonra karşı uç her bağlantıda `KOSUYOR` olmadan FIN gönderip yeniden bağlansın. Hazırlık başlamadığından mutlak toplam sınır yoktur; her kabul `son` değerini yeniler, hazırlık sayacı artmaz. SIL zamanı donukken sonsuz yeniden kurma mümkündür. | Yeniden kurma işleminin tamamına, hazırlık gerekip gerekmediğinden bağımsız, korunan mutlak son tarih ve toplam deneme sınırı uygula. | kritik |
| 3 | `sim/sunucu/src/sunucu.cpp:232`, `:1707`; `sim/kopru/src/kopru.cpp:144`; `sim/kopru/src/kopru_sinir.cpp:644` | **EMNİYET:** İzin açık ve yürürlükteki itki nötr dışıyken koşu bitsin. `oturum_kapat()` yerel daraltmayı yalnız `oturum_sonu_bekliyor`/`yururluk_sil` bayraklarına bırakır. Kapanıştan sonra bunları uygulayan sınır çağrılmadığından izin ve fizik girdisi çıkıştan önce daraltılmaz. | Ortak kapanışta izinleri gerçekten kapatıp fizik girdisini nötrle; işlemi yeni kart çıktısı tüketmeden ve fizik ilerletmeden tamamla. | kritik |

## Yeniden hesaplananlar

- **4 MiB = 4.194.304 B**, **64 MiB = 67.108.864 B**; **8000 × 5 ms = 40 s**.
- **Bütçe + EOF:** tam 4.194.304’üncü baytta önce PROTOCOL; bir bayt eksikte yalnız bağlantı kaybı. Birlikte/ayrı teslimde sıra aynı.
- **Tampon taşması + EOF:** 129’uncu tamponlanan çerçeve taşmayı üretir; EOF sonra değerlendirilir. Tek/bölünmüş teslim aynı kararı verir.
- **Bütçe + tampon taşması:** kalan bütçeyle sınırlı besleme önceki konumdaki olayı belirler; aynı son baytta çerçeve kararı önce gelir. Parçalanma sonucu değiştirmiyor.
- **EOF + yazma hatası:** SIL’de `−1` yazım alımı kesmez; kuyruk tüketildikten sonra EOF işlenir. İki teslim biçiminde karar aynı; fiziksel gönderim başarısı bundan ayrı.

## İskelete ya da diğer bölümlere notlar

`Bekleme::oturum_bitti` çağıranları yarım yuvayı ve bekleyen olayları temizleyen oturum sonuyla uyumlu; yeniden kurma dönerse `sil_kisitlayan_uygula()` fizik ilerlemeden nötrlemeyi uygular (`sunucu.cpp:1666`). Kapanıştaki SIL `pompa()` yeni baytları köprüde işlemez. `erken_kapanis` mevcut akışta tek kez çağrılır; çift `SESSION_CLOSE` yolu bulmadım.

Tahsis istisnası [C++ taslağı](https://eel.is/c++draft/new.delete.single), EOF/kısa okuma davranışı [Apple](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/recv.2.html) ve [Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-recv) kaynaklarından canlı denetlendi. Kabul edilen HIL seri gönderim kuyruğu sınırı bulgu sayılmadı.