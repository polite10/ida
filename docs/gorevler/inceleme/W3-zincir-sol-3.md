# sol incelemesi: emniyet

## Hüküm

1. **Kapanmadı:** saklı hedeflerin sayaç turuyla canlanması engellenmiş; ancak eski tutma hedefinin kontrol referansı, `ref_kusak` turunda korunabiliyor (#1).
2. **Kapanmadı:** düz duruştaki 3 m → 2 m karşı örneği kapanmış; eğik duruşta hız ve doğrudan kuvvet kanalları NED aşağı kuvvet sınırını aşabiliyor (#2).

Yalnız belirtilen iki konunun kapanışı incelendi. Bulgular statik kod takibi ve hesaplamaya dayanır; testler çalıştırılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `core/src/kontrol/kontrol.c:245`; `core/src/gorev/gorev.c:150`; `core/src/cerceve/cerceve.c:474` | **Kontrol referansında sayaç turu kalıyor.** Silahlı DEPTH_HOLD’da ölçülen derinlik 1 m, eski tutma hedefi ve `d_ref` 2 m olsun. Zamanı ilerletmeden, her birinde aynı DEPTH_HOLD daraltması bulunan 2³² geçerli `tick` çağrısı yapın. Görev her seferinde hedefi 1 m’den yakalar, bitini temizler ve `ref_kusak` artırır; kontrol tick’te çalışmaz. Sayaç başlangıca döndüğünde sonraki step eski `d_ref`’i sıfırlamaz ve yeni komut olmadan aşağı kuvvet ister. | Kontrolün eski referans ve integrallerini bırakmasını da kontrol tarafından tüketilen kalıcı iptal mandalına bağlayın. Hedef kaydının silinmesi, türetilmiş kontrol belleğini tek başına temizlemiyor. | kritik |
| 2 | `core/src/kontrol/kontrol.c:549`; `:555`; `:569` | **Sınır son NED kuvvetine uygulanmıyor.** Doğrudan gövde X/Y kuvvetleri dikey sınırlamadan sonra ekleniyor; ayrıca dönüşümden sonraki gövde ekseni kırpmaları NED aşağı bileşenini büyütebiliyor. Aşağıdaki iki hesapta da `dalis_yasak=1` ve sınır 1,82 N olduğu hâlde aşım var. | Bütün kanalların birleşmesi ve eksen sınırları sonrasında NED aşağı kuvvet değişmezini koruyun; eğik duruş karşı örneklerini kapsayın. | kritik |

## Yeniden hesaplananlar

- **Sayaç:** `(r + 2³²) mod 2³² = r`. Hedef mandalı bundan etkilenmez; fakat `son_ref_kusak != ref_kusak` denetimi etkilenir. #1’in sonraki 5 ms adımında `d_ref=1,99875 m`; `w_sp=min(0,8×0,99875−0,25; 0,5)=0,5 m/s`. İntegral sıfırken bile ham kuvvet `120×0,5+1,82+80×0,5=101,82 N`, kırpılmış istek **80 N aşağıdır**. İşaretsiz aritmetik canlı olarak [WG14 N1570 §6.2.5/9](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) üzerinden denetlendi.
- **Hız kanalı:** yalpa=baş-kıç=30°, yön=0; dikey hız ve integraller sıfır, derinlik referansı ölçümle eşit olsun. Yatay hız isteğinin ürettiği `F_NED=(-40,-40,1,82) N`, gövdede yaklaşık `(-35,551; -43,853; 4,044) N` olur. Y ekseninin −40 N’ye kırpılması sonrasında NED aşağı istek **3,488 N** olur.
- **Doğrudan kuvvet:** baş-kıç=30°, yalpa=0; gövde isteği `X=-40 N`, `Z=50 N`. Kod Z’yi 1,82 N’ye indirir; sonuç NED aşağıda `40×sin30°+1,82×cos30°=21,576 N` olur.

## İskelete ya da diğer bölümlere notlar

- Güdüm biti hedef silindikten, görev biti hedefler yeniden yakalandıktan sonra temizleniyor. Bayrak görev isteğine bütün kiplerden önce taşınıyor ve güdüm tarafından korunuyor (`gudum.c:146`, `gorev.c:258`, `istek.c:61`).
- GUIDED_SP’den sonra gerçekten işlenen kip değişimi mandalı yeniden kurar. Ancak `cmds[]` içinde sonra duran daraltıcı, A4-K46 gereği önce (b)’de uygulanır; ardından kabul edilen GUIDED_SP yeni hedef olarak saklanır.
- N2176 web kaynağı ayrıştırılamadı; doğrulanmış sayılmadı. Dosya değiştirilmedi, kurulum yapılmadı.