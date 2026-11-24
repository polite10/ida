# sol incelemesi: kopru

## Hüküm

**#11 kapandı.** `8576249`, yalnız W4-K15 ek (7) canlılık kararı bakımından incelendi. İncelenen akışlarda reddedilen veya tahliye edilen kümenin canlılığı yenilediği, sağlıklı trafiğin yanlış T_bağ aşımına uğradığı ya da SIL gözetiminin bozulduğu bir yol saptanmadı.

Dosya değiştirilmedi; derleme ve test çalıştırılmadı. Hüküm kaynak incelemesine dayanır. #1, #2, #3 ve (6) yeniden değerlendirilmedi.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| — | — | Bu dar kapsamda saptanmadı. | — | — |

## Yeniden hesaplananlar

- Önceki karşı örnekte FIRE 490 ms, tutarsız yanıt 491 ms’de alınır. İkisi de bekleyen katkıdır; toplu ret katkıyı siler (`sim/kopru/src/kopru_sinir.cpp:240`). Son bağımsız geçerli alım 0 ms ise 505 ms’de **505−0=505 ms ≥ 500 ms**: T_bağ kapanışı gerçekleşir.
- Tutarlı kümede işlenen zaman **max(490,491)=491 ms** olur; 505 ms’de geçen süre **14 ms**’dir. 490 ms’de alınan bağımsız geçerli çerçeve için süre **15 ms**’dir.
- İşlenmiş canlılık 499 ms iken 491 ms’lik bekleyen katkının işlenmesi sonucu **max(499,491)=499 ms** kalır; zaman geri gitmez (`sim/kopru/src/kopru.cpp:204`). `std::max` davranışı canlı [C++ standart taslağından](https://eel.is/c++draft/alg.min.max#10) doğrulandı.

## İskelete ya da diğer bölümlere notlar

- **Ret, tahliye ve yinelemeler:** bekleyen zaman doğrulamada işlenir, rette silinir; tahliyede kayıtla birlikte atılır. Bekleyen veya reddedilmiş kümenin yinelemesi doğrudan yenilemez; saklı sonucu `APPLIED` olan yineleme yeniler (`sim/kopru/src/kopru_sinir.cpp:189`, `:251`, `:262`, `:349`).
- **Geçerli trafik:** SAFETY_CLOSE, hata dönüşü, ASYNC ve diğer geçerli çerçevelerin canlılık yolları korunmuş. Oturum sonunda küme tablosu temizlenir; yeni kabulde canlılık tabanı yeniden kurulur (`sim/kopru/src/kopru.cpp:156`, `:316`, `:460`).
- **SIL:** T_bağ koşulu SIL’i dışlar; yuva bekleme ve gerçek duvar saatli A1-K76 gözetimi korunur (`sim/kopru/src/kopru.cpp:783`; `sim/sunucu/src/sunucu.cpp:506`).
- `test_emniyet5.cpp` önceki olay→yanıt karşı örneğini, eksik kümeyi, yinelemeleri ve olumlu canlılık yollarını kapsıyor. Testlerin çalışma sonuçları bu incelemede doğrulanmadı.