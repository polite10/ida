# sol incelemesi: a3

## Hüküm

**df4dabf için, belirtilen tehdit modeli ve W4-K23’ün güncel tanımları altında erişim ve emniyet kapsamı KAPANDI.** Windows’ta görülen, işlenmiş doğrulanmış isteğin yanıt yazılmadan tahliye edilmesi yolu kapatılmıştır. Yeni kritik bulgu saptanmadı.

Hüküm dar kapsamlı statik kod ve test incelemesine dayanır; test çalıştırılmadı, Windows sonucu yeniden doğrulanmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| — | — | Bu kapsamda yeni bulgu saptanmadı. | — | — |

## Yeniden hesaplananlar

- **Sessiz saldırganlar:** HTTP sınırı `N`, dinleme kuyruğunda meşru bağlantının önündeki saldırgan sayısı `M` olsun. Tamamı sessiz saldırganlarla dolu başlangıçta gereken yenileme dalgası `r = ⌊M/N⌋ + 1` olur. Her dalga en geç kabul yaşının **2 s** olmasıyla açılır (`sim/a3/src/cekirdek.cpp:590`). İki adet 20 ms yoklama payıyla tutucu hesap **2,04r saniye + döngü işi ve işletim sistemi gecikmesi**dir. `M=0` için 2,04 s; `N=4, M=12` için dört dalga, 8,16 s. Bunlar ölçüm veya kesin duvar saati garantisi değildir.
- **Tek bayt gönderenler:** veri okunduğunda tahliyeye uygunluk hemen oluşur (`cekirdek.cpp:681`); 2 s beklemek gerekmez. `N=4, M=12` örneğinde dört kabul turu ve meşru isteğin sonraki okuma turu yeterlidir. Sonradan gelen saldırganlar kuyruğa girmiş meşru bağlantıyı geçmez; iki platformun `accept` belgesi de kuyruktaki ilk bağlantının çıkarıldığını belirtir ([Apple](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/man/man2/accept.2), [Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-accept)).
- **Çıkış tavanları:** toplam **67.108.864 B**; bağlantı başına **21.037.056 B**. İkisinin toplamı **88.145.920 B**; bu toplamada 32 bit taşması yoktur.

## İskelete ya da diğer bölümlere notlar

**(a)** Tur sırası okuma/işleme → kabul → bakım → yazmadır (`sim/a3/src/sunucu.cpp:151`, `:183`, `:219`, `:224`). Doğrulanmış gövde, sıradaki doğrulanmış istek, öne alınmış yanıt ve ana çıkışta bekleyen yanıt korunur (`cekirdek.cpp:569`). Kısmi yazım kalan çıkışı korur (`:3233`); çıkış baskısındaki işlenmiş istek de sırada korumaya dahildir. Pipelining’de ikinci isteğin başlığı henüz tamamlanmamışsa aşağıdaki kapanış yarışı mümkündür. Yükselmiş WebSocket’ler HTTP sınırından ve tahliyeden dışlanır (`:590`, `:607`); ayrı sınırları vardır (`:2380`).

**(b)** Dinleyicinin okunabilirliği kapatılsa da yoklama zaman aşımı en fazla **20 ms istenir** (`sunucu.cpp:132`, `:143`); dolayısıyla süre dolumunu fark etmek yeni soket olayına bağlı değildir ([WSAPoll](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsapoll)). Yaş sırası kabul kimliğiyle belirlenir (`cekirdek.cpp:645`). Normal kabul yolunda 503 yalnız sınırı isteği süren doğrulanmış bağlantılar doldurduğunda seçilir (`:611`, `:619`).

**(c)** Boşta keep-alive bağlantı, yeni istek baytları yoldayken veya yeni başlık henüz tamamlanmamışken kapatılabilir. Bu, verilen tanımın izin verdiği sınırdır; [RFC 9112 §9.5](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.5) aynı yarışı açıkça tanımlar. Eksik istek uygulanmaz; tam doğrulanmış istek korunur ve kısmi gövdeden emniyet etkisi üretilmez (`cekirdek.cpp:818`, `:856`). Yeniden gönderim aynı istek kimliğiyle yapılmalıdır; otomatik POST tekrarına koşulsuz güvence verilmez.

**(d)** Toplam çıkış tahliyesi `hic_dogrulandi` bağlantılarını dışlar (`cekirdek.cpp:395`, `:401`); bayrak başlık doğrulanırken kurulur (`:778`). Yeni yanıt korumasıyla çelişmez.

**Model sınırı notu:** işletim sistemi dinleme kuyruğunun dolması meşru bağlantıyı reddettirebilir ([Apple](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/man/man2/listen.2)). Ayrıca başarılı `send`, istemcinin yanıtı aldığını kanıtlamaz ([Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-send)). Bağlantı girişiminden yanıt teslimine koşulsuz süre güvencesi verilmez.