# sol incelemesi: a3

## Hüküm

**b3c600c kabul edilmemeli.** W4-K23’ün tehdit modeli altında: **#1a kapandı, #1b kapandı; #2a, #2b ve #3 kapanmadı.** Dört kural eksiksiz uygulanmış değil.

Önceki büyük gövde ve chunked+EOF karşı örnekleri giderilmiş; ancak kabul sırasında tahliye, sınırsız çıkış birikmesi ve jeton geçersizleştirme açıkları kalıyor. Aşağıdaki karşı örnekler statik kod izlemesine dayanır; derleme veya test çalıştırılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 2a | `sim/a3/src/cekirdek.cpp:556`, `:562`; `sim/a3/src/sunucu.cpp:180`, `:202` | **Kapanmadı: yeni bağlantı ilk okumasından önce tahliye edilebilir.** 64 jetonsuz bağlantıda başlıklar yarım olsun. Aynı kabul turunda önce geçerli Bearer ve acil durdurma isteğini göndermiş istemci, ardından saldırgan kabul edilsin. İlk kabul eski yarım bağlantıyı kapatır; ikinci kabul, henüz okunmamış meşru bağlantıyı “boş” sınıfında seçip kapatır. Eski yarım bağlantılar korunur. İstek doğrulanma ve mandala ulaşma fırsatı bulamaz. Bu ayrıca W4-K23’ün “en eskisi önce” kuralına aykırıdır. | Yeni kabul edilen bağlantıya sonraki tahliyeden önce sınırlı okuma/doğrulama fırsatı ver; doğrulanmamışlar arasında sınıf önceliği yerine karardaki yaş sırasını uygula. Kabul–okuma arasına ikinci kabul giren soket testi ekle. | kritik |
| 2b | `sim/a3/src/cekirdek.cpp:751`, `:843`, `:854`, `:877` | **Kapanmadı: bekleyen çıkış sınırsız büyüyebilir.** Jetonsuz istemci yanıtları okumadan 403’lerle çıkışı 4 MiB üstüne taşısın; ardından ayrı okumalarda `{"girdi":"acil_durdurma","hedef_durum":"basili"}` gövdeli POST’lar göndersin. Biçim kısıtlayan sayıldığından her istek öne alınır ve 403 üretir. Sonraki çağrıda `on.islendi` dalı önceki yanıtı **eşik denetiminden önce** çıkışa ekler. Sıra yaklaşık bir kayıtta kalırken çıkış sürekli büyür; jetonsuz süreç A3 hizmetini bellek tüketimiyle durdurabilir. | Önceden işlenen yanıtları da toplam bayt/sayı tavanına dahil et; `islendi` dalında çıkış eşiğini uygula. Taşmada kısıtlayan etkileri koruyarak bağlantıyı kapat. | kritik |
| 3a | `sim/a3/src/cekirdek.cpp:633`, `:674`, `:730`, `:915` | **#3 kapanmadı: çerçeveleme hatası jetonu koruyor.** İstemci T onay jetonunu aldıktan sonra geçerli Bearer ve `Ida-Istemci` ile gevşetme ucuna chunked istek gönderip gövdeye `Z\r\n` yazsın. 400+kapanış yolu isteği kaybedip aşamayı `baslik` yapıyor; toplu geçersizleştirme çağrılmıyor. T, aynı run/revizyonla ve süresi içinde düzgün istekte kullanılabilir. | Kimliği doğrulanmış gevşetme isteğinin çerçeveleme hatasında, bağlamı silmeden önce ilgili istemcinin bütün bekleyen jetonlarını geçersizleştir. | kritik |
| 3b | `sim/a3/src/cekirdek.cpp:1007`, `:1012` | **#3 kapanmadı: istemci kapsamı aşılabiliyor.** Geçerli Bearer taşıyan bozuk JSON isteğinde `Ida-Istemci` eksik/geçersizse bütün istemcilerin jetonları siliniyor. Bir istemcinin hatalı isteği diğer istemcilerin bekleyen onaylarını bozuyor; W4-K23 yalnız **o istemciyi** kapsıyor. | İstemci belirlenemediğinde küresel silme yapma; geçersizleştirmeyi doğrulanabilen istemci kapsamıyla sınırla. Çok istemcili karşı örnek ekle. | kritik |

## Yeniden hesaplananlar

- Çıkış eşiği **4.194.304 B**. Yukarıdaki standart 403 yanıtı, **95 B UTF-8 gövde + 136 B başlık = 231 B**. Hiç çıkış tüketilmezse 18.158 yanıt **4.194.498 B** eder.
- #2b’de sonraki her tur yaklaşık bir bekleyen kayıt bırakırken çıkışa **231 B** daha ekler: `R(n) = R(0) + 231n`. **32 istek / 8 MiB sıra sınırı bu büyümeyi sınırlamaz.** Bu hesap, ölçüm değildir.

## İskelete ya da diğer bölümlere notlar

- **#1a kapandı:** kök başlangıçta tutuluyor; Windows dalında silme paylaşımı yok ve yeniden açılan kökün kimliği denetleniyor (`statik.cpp:131`, `:172`); POSIX dalı tutulan köke göreli açıyor (`:238`, `:262`). API davranışları [Microsoft CreateFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew) ve [Apple open/openat](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/man/man2/open.2) kaynaklarıyla karşılaştırıldı.
- **#1b kapandı:** iki dal da bağlantı sayısı 1 olmayan dosyayı reddediyor (`statik.cpp:202`, `:279`). Alanların anlamı [Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/ns-fileapi-by_handle_file_information) ve [Apple fstat](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/man/man2/stat.2) kaynaklarından doğrulandı. Model içinde ayrıca kök dışına kaçış saptanmadı.
- Geçerli Bearer olmadan doğrulanmış işareti edinme yolu saptanmadı (`cekirdek.cpp:671`, `:927`). #3b jetonsuz saldırı değildir; doğrulanmış istemcilerin birbirini etkilemesidir.
- **Model dışı not:** kökü veya üst dizinlerini değiştirmeyi gerektiren yarışlar hükme dahil edilmedi.
- `docs/gorevler/W4-a3.md` bulunamadı; görev belgesine tam uygunluk doğrulanamadı. Open Group `fstat` sayfası 403 verdi; doğrulamada Apple kaynağı kullanıldı.