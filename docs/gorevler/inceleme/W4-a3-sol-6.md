# sol incelemesi: a3

## Hüküm

**7a62f7a için erişim ve emniyet kapsamı KAPANDI.**

- **Önceki #1 kapandı:** `sim/a3/src/sunucu.cpp:188` kabul kararını `accept` öncesinde denetliyor; `bekle` durumunda kabul turunu bırakıyor. `sim/a3/src/cekirdek.cpp:585` ilk okuma fırsatını bekleyen doğrulanmamış bağlantılarla dolu sınırı, doğrulanmış isteklerle dolu sınırdan ayırıyor.
- **Önceki #2 kapandı:** `sim/a3/src/cekirdek.cpp:1139` geçerli Bearer ve istemci kimliği için toplu geçersizleştirmeyi uyguluyor; kapanış, Host ve Origin retleri bu yardımcıdan geçiyor (`:1157`, `:1165`, `:1171`). Önceki bozuk JSON karşı örneğinde jetonlar artık korunmuyor.
- **(3'') hesabında taşma, yarış veya doğrulanmış bağlantının toplam bütçe nedeniyle kapatıldığı yol saptanmadı.** `hic_dogrulandi` başlık doğrulanınca kalıcı olarak kuruluyor (`:760`); toplam ve tahliye bu bağlantıları dışlıyor (`:395–402`). Hesap ve değişiklikler aynı G/Ç iş parçacığında yürütülüyor.

Hüküm statik kod ve test incelemesine dayanır; talimat gereği derleme ve test çalıştırılmadı. Önceden kapanan bulgular yeniden değerlendirilmedi.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| — | — | Bu dar kapsamda yeni bulgu saptanmadı. | — | — |

## Yeniden hesaplananlar

- Toplam tavan **64 × 2²⁰ = 67.108.864 B**; bağlantı başına tavan **4 MiB + 16 MiB + 64 KiB = 21.037.056 B**. Toplama öncesindeki toplam en çok 64 MiB olduğundan, denetimdeki `toplam + s.size()` en çok **88.145.920 B** olabilir; 32 bit işaretsiz aralıkta bile taşmaz. Tahliye çıkışı sıfırlayarak toplamı azaltır.
- Yeni testte **12 × 5 MiB = 62.914.560 B**, **13 × 5 MiB = 68.157.440 B**; yanıt başlıkları eklenmeden bile on üçüncü yanıt tahliye gerektirir. Yirmi isteğin sonunda en az sekiz tahliye beklentisi tutarlıdır.
- Kabul testindeki 12 saldırganın arkasındaki meşru bağlantı, dört kullanılabilir yuva koşulunda **⌊12/4⌋ + 1 = 4. kabul turunda** alınır; hazır isteği sonraki okuma turunda işlenir. Bu hesap, ölçüm değildir.

## İskelete ya da diğer bölümlere notlar

**(b) İlerleme sınırı:** meşru bağlantı dinleme kuyruğuna girdiyse, önündeki bağlantı sayısı sonluysa ve isteği okuma turunda hazırsa, sonraki saldırgan bağlantıları önüne geçmez: `accept` kuyruğun ilk bağlantısını çıkarır ([Apple](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/man/man2/accept.2), [Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-accept)). Okuma kabulden önce yapılır; doğrulanmamış bağlantılar sonraki turda yaş sırasıyla tahliye edilebilir. Bu koşullarda kabulü bırakmanın oluşturduğu süresiz bir kod kilitlenmesi saptanmadı.

**Model sınırı notu:** kuyruk saldırgan tarafından doldurulursa meşru bağlantı işletim sistemi düzeyinde reddedilebilir; macOS’ta istek yeniden iletim için yanıtsız da bırakılabilir ([Apple `listen`](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/man/man2/listen.2), [Microsoft `listen`](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-listen)). Dolayısıyla bağlantı kurma girişiminden mandala kadar koşulsuz, sınırlı tur güvencesi yoktur. **2 s başlık zaman aşımı dinleme kuyruğuna uygulanmaz**; başlamış başlığın uygulama tarafındaki süresini sınırlar (`cekirdek.cpp:3147`). Bu işletim sistemi kaynak sınırı bulgu sayılmadı.

**Model dışı not:** Bearer sahibi kötü niyetli istemci ve statik köke yazabilme senaryoları hükme dahil edilmedi.