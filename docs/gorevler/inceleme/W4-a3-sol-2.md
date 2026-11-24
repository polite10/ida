# sol incelemesi: a3

## Hüküm

**Bu hâliyle kabul edilmemeli.** `b66c1a2` üzerindeki dar ikinci denetimde **#1, #2 ve #3 kapanmadı**; karşı örnekleri aşağıdadır.

**#14 kapandı:** `sim/a3/test/test_yavas.cpp:234` sonuçta `uygulandi`, satır 235 yetkili durumda `basili` arıyor; satır 254 reddeden alıcıyla bu doğrulamaların başarısız olmasını istiyor.

Dosya değiştirilmedi; derleme ve test çalıştırılmadı. Windows karşı örneği, canlı birincil kaynaklarla doğrulanan API davranışı ve statik kod izlemesine dayanıyor; Windows’ta çalıştırılmadı. #4–#13 değerlendirilmedi.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `sim/a3/src/statik.cpp:119`; `sim/a3/src/statik.cpp:130` | **Kapanmadı: Windows’ta harfe duyarlı dizin üzerinden kök dışına kaçış kalıyor.** Harfe duyarlı bir üst dizinde ayrı `web` ve `WEB` dizinleri, statik kök `web` olsun. `web/panel/x.txt` bağı dışarıdaki `WEB/panel/x.txt` dosyasını göstersin. Jetonsuz `GET /panel/x.txt` hedef dosyayı açar; gerçek yol ile beklenen yol yalnız harf büyüklüğünde ayrıldığından `CompareStringOrdinal(..., TRUE)` eşitlik verir. Açılan hedef düzenli dosya olduğundan reparse özniteliği denetimi de engellemez. Windows [bu ayrı dizinleri destekliyor](https://devblogs.microsoft.com/commandline/per-directory-case-sensitivity-and-wsl/); karşılaştırma API’si [`TRUE` ile harf ayrımını kaldırıyor](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-comparestringordinal). | Bileşenleri reparse noktalarını izlemeden, tanıtıcılar üzerinden doğrula. Harf duyarsız metin eşitliğini kök üyeliğinin kanıtı sayma; karşı örneği Windows kabulüne ekle. | kritik |
| 2 | `sim/a3/src/cekirdek.cpp:507`; `sim/a3/src/cekirdek.cpp:541`; `sim/a3/src/cekirdek.cpp:2726` | **Kapanmadı: emniyet isteği mandala ulaşmadan sınırda kaybolabiliyor.** Varsayılan 64 HTTP bağlantısını eksik başlıkla meşgul tutunca boş bağlantı bulunamıyor; acil durdurmayı taşıyacak 65. bağlantı kapatılıyor. Ayrıca aynı okumada 5 MiB statik dosya GET’i ardından geçerli acil durdurma POST’u gelirse ilk yanıt 4 MiB çıkış eşiğini aşınca POST girdide kalıyor. Çıkış boşalırken `yazildi()` ayrıştırmayı yeniden başlatmıyor; yeni ağ verisi gelmezse etki teslim edilmiyor. | Bağlantı ve çıkış sınırlarını da kapsayan emniyet kabul yolu kur. Çıkış baskısı kalkınca tamponlanmış istekleri yeniden işle. Her iki karşı örnekte yetkili emniyet durumunu doğrula. | kritik |
| 3 | `sim/a3/src/cekirdek.cpp:1416`; `sim/a3/src/cekirdek.cpp:1437` | **Kapanmadı: içerik türü reddi jetonu tüketmiyor.** Geçerli Bearer ve `Ida-Istemci` ile, geçerli jeton taşıyan eksiksiz gevşetme JSON’unu `Content-Type: text/plain` gönder: 415 tüketimden önce dönüyor. Revizyon değişmeden ve 10 saniye dolmadan başlığı `application/json` yapıp yeniden gönderince aynı jeton kabul edilebiliyor. A3-K94’ün “ilk kullanım, kabul ya da ret” kuralı sağlanmıyor. | Kimlik doğrulamasından sonra çözülebilen gevşetme gövdesindeki jetonu, içerik türü dahil erken retlerden önce tüket; 415 sonrası yeniden kullanım karşı örneğini ekle. | kritik |

## Yeniden hesaplananlar

- Mandal sayısı **1 + 2 + 2 + 4 = 9**: acil durdurma, tüp başına fiş/pim ve katalogdaki dört daraltıcı tür. Sonuç kaydı sınırları bu etki kümesini büyütmüyor.
- Mandal başına **1 güncel istek + 64 birleşen sonuç kimliği** saklanabiliyor. Eski sonuçların atılması, etkinin saklanmasından ayrı tutulmuş.
- **5 MiB = 5.242.880 B > 4.194.304 B** çıkış eşiği; ikinci karşı örnek statik dosyanın 16 MiB üst sınırı içinde.

## İskelete ya da diğer bölümlere notlar

- W4-K19 ve W4-K22(a) esas alındı. Mandaldaki etkiyi sonradan gelen gevşetmenin sildiği yol saptamadım; dolu mandal gevşetmeyi engelliyor. Eski sonuç kaydının düşmesini tek başına hata saymadım.
- Parametreli tek daraltıcı tür `set_mode_daraltici`; son hedefi saklama A4-K46’nın açık kuralıyla uyumlu. Bu noktada ek emniyet ihlali saptamadım.
- Şema, kimlik çakışması, run/revizyon ve alıcı retlerine ulaşan isteklerde tüketim öne alınmış. Bearer doğrulamasından geçmeyen istemci tüketim noktasına ulaşamıyor (`cekirdek.cpp:723`).
- Kodlanmış yollar, ters bölü, NUL ve `:` karakter süzgecini geçmiyor. POSIX bileşen açılışları [Apple’ın `openat`/`O_NOFOLLOW` tanımıyla](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/man/man2/open.2) uyumlu. Windows’ta diğer özel ad ve yarış yollarının çalıştırmalı doğrulaması yapılmadı.
- Atıf verilen `docs/gorevler/W4-a3.md` bu ağaçta bulunmuyor; görev belgesine uygunluk doğrulanmış sayılmadı.