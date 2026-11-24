# sol incelemesi: kontrol

## Hüküm

1. **sol-3 #1: kapandı.** Kontrol çalışmayan çağrılarda `IDA_EMN_IPTAL_KON` korunuyor. Bit yalnız kontrol hesabından sonra temizleniyor; etkisiz, erken dönüş ve sayısal çökme yolları belleği sıfırlıyor (`core/src/kontrol/kontrol.c:729–781`). Kip değişimi, hedef iptali, yasak başlangıcı ve arıza düzeyi değişiminde bit kuruluyor. Referans geçerliliğine karar veren sayaç eşitliği kalmamış.
2. **sol-3 #2: kapanmadı.** Önceki eğik duruş karşı örnekleri giderilmiş; ancak `core/src/kontrol/kontrol.c:641` kesin kuvvet sınırının küçük miktarda aşılmasına izin veriyor. Karşı örnek aşağıda.
3. **W4-K13: kapandı.** Veri yokluğu ve bayatlık ARM’ı engelliyor; denetim kip sütunundan bağımsız. İki koşul mandalsız ve eylemleri WARN olarak kalmış; değişiklik silahlı aracı silahsızlandırmıyor (`core/src/gorev/ariza.c:35,41,59,65`; `core/src/gorev/gorev.c:171–189`).

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `core/src/kontrol/kontrol.c:641` | **Kuvvet değişmezinde pozitif tolerans var.** Küçültme yalnız `fa > tutma_z + 0.0001` iken çalışıyor. Düz duruşta, varsayılan geometriyle önceki dört dikey itki `1.7050125 N`, diğer itkiler sıfır olsun. Aynı kip ve seçicilerle, 5 ms sonraki adımda yasak başlasın ve Z isteği tutma sınırına indirilsin. Varsayılan 250 N/s değişim sınırı, tahsisi yaklaşık `1.8200498 N` aşağı kuvvette bırakır. Bu değer `BUOY_FF ≈ 1.8200001 N` üstündedir; küçültme çalışmaz. Aşım yaklaşık **0.00005 N** olsa da istenen kesin değişmez sağlanmıyor. | Pozitif aşım aralığını kaldırın; yuvarlama payını güvenli tarafta bırakıp küçültme sonrasında `Bf` üzerinden sınırı yeniden doğrulayın. Bu geçişi sınır testiyle kapsayın. | kritik |

## Yeniden hesaplananlar

- **Karşı örnek:** Önceki simetrik tahsis, yasak yokken toplam `6.82005 N` Z isteğiyle elde edilebilir. Yeni adımda itici başına izin verilen azalma `250 × 0.005 = 1.25 N` olur. Her dikey iticinin alt sınırı yaklaşık `0.4550125 N`; toplamı `1.82005 N`’dir. Binary32 ara hesaplarıyla toplam **1.8200497627 N**, küçültme eşiği **1.8201000690 N** çıkar. Böylece 641. satırdaki koşul yanlıştır. Kontrolün iptal mandalı önceki `f[]` değerlerini etkin yolda silmediğinden bu geçiş engellenmez.
- **Önceki eğik duruş örneği:** Baş-kıç 30°, X=−40 N için eski aşağı bileşen `20 + 1.82 cos30° = 21.576166 N` idi. Yeni girdi koruması Z’yi yaklaşık **−20.992456 N** yapar; aşağı bileşen **1.82 N** olur.
- **Küçültme sınırları:** Girdi aşamasında oran, koşulu sağlandığında `0 < (yatay−aşım)/yatay < 1`; diğer durumda sıfırdır. `r22 ≤ 0` yolunda aşağı katkı veren Z kaldırılır. Tahsis sonrası bölme yalnız pozitif `fa` için yapılır; `tutma=0` oranı sıfırlar, `BUOY_FF<0` tutmayı sıfır yapar. Küçültmeden sonra yeniden kuvvet artıran değişim sınırlayıcısı yoktur.
- **Tazelik:** Varsayılan 2000 ms eşik `2 000 000 000 ns`’dir: eşikte taze, bir nanosaniye sonrasında bayattır. Dönüşüm çarpmadan önce `int64_t`’ye yapılır (`core/src/zaman/zaman.h:69`). Damga 0 geçerlidir; veri yokluğu ayrı `var` bitleriyle ayrılır. Kabul kapısı gelecek damgayı reddeder.

## İskelete ya da diğer bölümlere notlar

Denetim yalnız üç commit ve belirtilen üç konuyla sınırlıdır. Yasak kapalıyken yeni kuvvet korumaları çalışmıyor. Açısal hız integrallerinin korunması `core/TASARIM.md` §9’da açıkça belgelenmiş.

Dosya değiştirilmedi, kurulum ve test çalıştırması yapılmadı; hükümler statik takip ve bağımsız hesaplamaya dayanır. WG14 N1570 erişimi zaman aşımına uğradı; doğrulanmış sayılmadı. İşaretsiz aritmetik ve tür dönüşümleri canlı olarak birincil [WG14 N2310 §6.2.5 ve §6.3.1.8](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2310.pdf) üzerinden denetlendi.