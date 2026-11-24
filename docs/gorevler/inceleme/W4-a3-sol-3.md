# sol incelemesi: a3

## Hüküm

**939d270 kabul edilmemeli: #1 kapanmadı, #2 kapanmadı, #3 kapanmadı.** İnceleme yalnız önceki üç bulgunun erişim ve emniyet kapsamındadır; #4–#13 yeniden değerlendirilmedi.

Karşı örnekler statik kod izlemesine dayanıyor. Dosya değiştirilmedi, derleme veya test çalıştırılmadı; Windows davranışları canlı birincil kaynaklarla karşılaştırıldı, Windows’ta çalıştırılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1a | `sim/a3/src/statik.cpp:113`, `:130`, `:137`, `:147` | **Kapanmadı: kökün yeniden açılmasında yarış var.** Gerçek yol alındıktan sonra, 130’dan önce `web` yeniden adlandırılıp yerine dış dizini gösteren junction konabilir. İlk tanıtıcı silme paylaşımı veriyor; yeniden açılan kökte `i > 0` koşulu reparse denetimini atlıyor. Ardından mutlak `web/panel/x.txt` açılışı dışarı yöneliyor. Bu, [CreateFile paylaşım kurallarından](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew) çıkarılan karşı örnektir. | Kök kimliğini tanıtıcı üzerinden sabitle; yeniden açılan nesnenin aynı kök olduğunu doğrula. Alt açılışları değişebilir mutlak yol çözümlemesine dayandırma. | kritik |
| 1b | `sim/a3/src/statik.cpp:154`, `:195` | **Sabit bağ kaçışı kalıyor.** Aynı birimdeki dış `gizli.txt` dosyasının `web/panel/x.txt` sabit bağı jetonsuz sunuluyor. İki dal da yalnız dosya türü/boyunu denetliyor; bağlantı sayısını denetlemiyor. [Sabit bağlar aynı dosyayı paylaşır](https://learn.microsoft.com/en-us/windows/win32/fileio/hard-links-and-junctions). | Dış sim verisinin sabit bağla sunulmasını engelleyen politika uygula; tutucu seçenek çok bağlantılı dosyaları reddetmek. İki platforma karşı örnek ekle. | kritik |
| 2a | `sim/a3/src/cekirdek.cpp:528`, `:543`; `sim/a3/src/sunucu.cpp:145` | **Kapanmadı: emniyet isteği taşıyan bağlantı tahliye ediliyor.** 64 bağlantıda boş/kapanan bağlantı bulunmasın; en eski bağlantıda geçerli kimlik başlıkları alınmış, acil durdurma gövdesinin son baytı bekleniyor olsun. 65. bağlantı bu bağlantıyı siler; tamamlanan gönderim mandala ulaşamaz. Sunucu yeni bağlantıları mevcut okunabilir soketlerden önce kabul ediyor. | Gövdesi gelmekte olan emniyet isteğini yeni bağlantı uğruna düşürmeyen kabul düzeni kur; bu sıralamayı bağımsız testle doğrula. | kritik |
| 2b | `sim/a3/src/cekirdek.cpp:749`, `:2833`, `:2922`, `:2961` | **Çıkış baskısı hâlâ etkiyi kaybettirebiliyor.** 5 MiB GET yanıtı beklerken, boşlukla 65.536 B’a tamamlanmış geçerli acil durdurma JSON’u gönder: başlıklarla birlikte 64 KiB tamponuna sığmaz; tarama tamamını beklerken okuma kapanır. Ayrıca tam bir chunked istek arkasındaki tam emniyet POST’u taranmaz; istemci gönderim yönünü kapatınca `kapanacak` kurulur ve tampon çıkış boşalsa da işlenmez. | Baskı altında gövdeyi artımlı tüket; desteklenen chunked çerçevelemeyi işle. EOF sonrasında tamponlanmış tam emniyet isteklerini kaybetme; yanıt sırasını ayrı koru. | kritik |
| 3 | `sim/a3/src/cekirdek.cpp:640`, `:854`, `:877`; `sim/a3/test/test_komut.cpp:937` | **Kapanmadı: büyük gövde reddinde tüketim önekin içeriğine bağlı.** Geçerli gevşetme JSON’unun önüne 1 MiB boşluk koy: 413 döner, ilk 4096 baytta bulunmayan jeton tüketilmez; aynı jeton normal gövdeyle, süre/run/revizyon değişmeden kullanılabilir. Mevcut test boşluğu jetondan **sonra** koyduğu için bunu yakalamıyor. Bozuk JSON’da da ham jeton tüketilirken `\u` kaçışlı gösterimi ham aramadan kaçıyor. | A3-K51’in okuma sınırını koruyarak, çözülemeyen/eksik önizlemede bekleyen yetkileri kısıtlayan yönde geçersizleştiren açık politika belirle. Önek, parçalanmış aktarım ve kaçışlı JSON karşı örneklerini ekle. | kritik |

## Yeniden hesaplananlar

- **5 MiB = 5.242.880 B > 4.194.304 B** çıkış eşiği; statik dosyanın 16 MiB sınırı içinde.
- #2b’de 77 B JSON + 65.459 boşluk = **65.536 B gövde**. Örnek 224 B başlıkla toplam **65.760 B**; tampon dolduğunda gövdenin 224 B’ı eksik kalır.
- Mandal kümesi **1 + 2 + 2 + 4 = 9**; mandala ulaşan etkilerin sonuç kaydı sınırı nedeniyle silindiği yol saptanmadı.

## İskelete ya da diğer bölümlere notlar

- W4-K19 ve W4-K22 esas alındı. Baskıda öne alma ortak `istek_isle` yolundan geçiyor: Host, Origin, Bearer ve şema denetimlerini atlamıyor; gevşeten biçim öne alınmıyor.
- Tam gövdesi işlenen gevşetmelerde yöntem, içerik türü, şema, kimlik çakışması ve alıcı retlerinden önce tüketim düzeltilmiş. Kimliği doğrulanmamış istek tüketim noktasına ulaşmıyor (`cekirdek.cpp:892–908`).
- Eski harf karşılaştırması kaldırılmış. Özel adlar/8.3/ADS üzerinden ayrıca kaçış saptanmadı; karakter süzgeci ve [Windows ad alanı davranışı](https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file) incelendi. POSIX dalı bu commit’te değişmemiş; [Apple’ın `openat`/`O_NOFOLLOW` tanımıyla](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/man/man2/open.2) uyumlu, sabit bağ eksiği sürüyor.
- `docs/gorevler/W4-a3.md` bu ağaçta bulunamadı; görev belgesine uygunluk doğrulanmış sayılmadı.