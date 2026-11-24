# sol incelemesi: sunucu

## Hüküm

**Emniyet kapsamı KAPANMADI.** `b32a34c` üzerinde, `3903a32..b32a34c` farkının W4-K25/K26’ya göre dar denetiminde bir kritik belirlenimcilik açığı kaldı.

Eski bulguların durumu:

- **#1 kapandı:** oturum sonu, sayım ihlalindeki `REJ_SAFE` ve eşzamansız güvenli çıktı, bekleme sınırındaki fizik ilerlemeden uygulanıyor (`sim/sunucu/src/sunucu.cpp:1516`).
- **#2 kapandı:** FIN ve yazma hatası, bekleyen alım kuyruğunu erken silmiyor; eski `ACTUATOR → 150 LOG → FIN` karşı örneğinde PROTOCOL kararı korunuyor.
- **#3 kapanmadı:** sınırsız okuma/büyüme giderilmiş; ancak eklenen 4 MiB denetimi teslim parçalanmasına bağlı karar üretiyor. Somut karşı örnek aşağıda.
- **#4 kapandı:** düzgün kapanışın mutlak son tarihi başarılı okumalar dahil her turda denetleniyor (`sim/kopru/src/tasima_tcp.cpp:223`).

İnceleme statiktir; testler çalıştırılmadı, dosya değiştirilmedi.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 3 | `sim/sunucu/src/sunucu.cpp:281,638–648` | **EMNİYET / belirlenimcilik:** Beklenen yuva boşken aynı akış `4.194.305 × 0x00 → geçerli ACTUATOR(seq)` olsun. İlk 4 MiB işlendikten sonra kalan sıfır ile ACTUATOR aynı beslemede gelirse `sil_besle` yuvayı tamamlar; `!yuva_tamam` koşulu nedeniyle taşma denetimi atlanır ve fizik ilerler. Son sıfır ayrı teslim edilip ACTUATOR sonraya kalırsa PROTOCOL üretilir. Aynı baytlar farklı oturum ve fizik sonucu verir; W4-K26(2)–(3) sağlanmaz. | Bayt bütçesini **köprüye besleme sırasında**, kalan bütçeyle sınırla. Yuva tamamlanmadan gerçekleşmiş aşımı, aynı beslemenin sonundaki ACTUATOR affetmemeli. Bu iki teslim biçimini karşılaştıran sınama ekle. | kritik |

## Yeniden hesaplananlar

- `h = 1,25 ms`, `P = 4h = 5 ms`. Kısıtlayan erken uygulama artık bekleme sınırı `b*` üzerinde, `dn_.adim()` çağrısından önce gerçekleşiyor.
- **1 MiB = 1.048.576 B = 64 × 16.384 B:** pompa başına iş sınırı; aşılması protokol hatası değildir. **64 MiB = 67.108.864 B:** kuyruk eklemesinden önce denetleniyor. **4 MiB = 4.194.304 B:** tüketim sınırı; yukarıdaki hata bu denetimin sırasındadır.
- A4’ün çağrı başına sınırları ve A1 §3.3 çerçeve boylarıyla, bütün seyrek telemetriyi de birlikte sayan üst toplam **1.854 B**’tır. Zorunlu kuyruğun 64 çerçevesini en büyük zorunlu çerçeveyle sayan ihtiyatlı patlama hesabı `64×94 + 4×49 + 50 + 500 = 6.762 B` olur. Meşru parti çıktısı yeni MiB sınırlarına yaklaşmaz.
- En çok 997 parametrenin sonuçları `997×39 = 38.883 B` eder; A4-K43 gereği tek çağrıda en çok 12 sonuç çıkar. Günlük sınırı da çağrı başına dörttür. Meşru döküm veya günlük üretimi için yeni sınırlar nedeniyle yanlış PROTOCOL yolu bulmadım.

## İskelete ya da diğer bölümlere notlar

`sil_kisitlayan_uygula` yalnız oturum sonunu ve doğrulanmış nötr ASYNC çıktıları tüketiyor; `SAFETY_OPEN`, `FIRE`, izin açma veya nötr dışı itki üretmiyor. HIL’de durum değiştirmeden dönüyor. W4-K3/K4/K12/K15’i bozan ek bir yol bulmadım. `swap` ve tüketilen bayraklar aynı çıktının sonraki `sinir_isle` tarafından yeniden uygulanmasını önlüyor; sayaç ve kullanım süresi ikinci kez başlatılmıyor. `cikti_uygula` ayrıştırması eski olağan sınır davranışını koruyor. Aynı adımın fizik girdisi yeniden verildiğinde mevcut kayıt değiştiriliyor (`sim/fizik/src/dunya.cpp:901`).

EOF sonrası `pompa`, `sil_akis_bitti_` nedeniyle okumayı bırakıyor; sonsuz okunabilirlik döngüsü veya sahipsiz soket bulmadım. Yanıt gelmezse gerçek duvar saatindeki T_SIL denetimi sürüyor (`sunucu.cpp:657`). Seri yol değişmemiştir. **A3 test istemcisi bu ağaçta aynı taşımayı kullanmıyor:** `sim/a3/src/soket.cpp` üzerinden çalışıyor. EOF ve kapanış API yorumları [Apple recv belgesi](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/recv.2.html) ve [Microsoft shutdown belgesiyle](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-shutdown) uyumlu. Open Group sayfası 403 verdi; doğrulanmış kaynak sayılmadı.

Toml++ dört yamasının `include/` hunkları canlı üst akım commit’leriyle birebir eşleşti: [242](https://github.com/marzer/tomlplusplus/commit/c4369ae1d8955cae20c4ab40b9813ef4b60e48be), [301](https://github.com/marzer/tomlplusplus/commit/e4bcc150894a3d0fc9e115263bc573aa1394b1ae), [305](https://github.com/marzer/tomlplusplus/commit/1e8829b793b66ad17011732a146b8077d379b011), [321](https://github.com/marzer/tomlplusplus/commit/1bf29cf3415908ffab1f59321ac03f9511dabb8d). Eski sezgisel Unicode engeli kaldırılmış; kalan ön tarama bu UB yollarının güvenlik sınırı değildir.

Koşu sonunda son ACTUATOR sonrası çıktıları işlememe davranışı, W4-K26(4)–(5)’teki ayrımla uyumludur.