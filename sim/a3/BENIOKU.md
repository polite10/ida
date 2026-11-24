# sim/a3 — A3 sunucusu (`ida_a3`)

Sim tarafında web panellerine hizmet veren A3 sunucusu (`docs/sozlesmeler/A3-web.md`): tek TCP portunda HTTP/1.1 ve WebSocket, yalnız döngü arabiriminde. C++20 kitaplığı olarak yazılır; veri kaynağı ve komut alıcısı soyuttur. Bu dilimde (`docs/gorevler/W4-a3.md`) sunucu sahte bir kaynakla (`ida_a3_sahte`) sınanır; `ida_sim`'e bağlanması sonraki dilimdir.

## Derleme

```
cmake --preset host-debug
cmake --build --preset host-debug --target ida_a3 ida_a3_sahte ida_a3_test
ctest --preset host-debug -R "sim\.a3\."
```

Dış bağımlılık yoktur (vcpkg gerekmez); Windows'ta `ws2_32` ve `bcrypt`. Yük kodlayıcıları ve katalog tabloları derleme zamanında `docs/sozlesmeler/sema/a3-kanallar.json`'dan `arac/uret_katalog.py` ile üretilir (Python 3.9+, yalnız standart kitaplık); alan sırası elle yazılmaz. Katalog (`a3-kanallar.json`) ve A1 komut kataloğu (`protocol/komutlar.json`) ikiliye bayt bayt gömülür (A3-K16, K100).

## Sahte sunucuyla panelleri denemek

```
cd web && npm ci && npm run derle          # paneller: web/dist
build/host-debug/sim/a3/ida_a3_sahte --port 0 --statik web/dist
```

Program açılış adresini (`http://127.0.0.1:<port>/#erisim=<jeton>`) standart çıktıya bir kez yazar (A3-K04, K05); adres tarayıcıda açılır. Seçenekler: `--port` (varsayılan 47100; 0 = işletim sistemi atar), `--statik`, `--adres` (yalnız `127.0.0.1` ya da `::1`; başka adreste dinlemez ve 1 ile çıkar: A3-K02), `--sure <s>`. Sahte dünya değerleri fizik değildir (`sahte/sahte_dunya.hpp`).

## Yapı

```
include/ida/a3/
  sunucu.hpp     Sunucu::baslat(ayar, alici, hata), yayinci(), durdur(), sayaclar()
  kaynak.hpp     Yayinci: dünya iş parçacığının yayın işlevleri (yayinla<Kanal>, olay, komut_sonucu)
  komut.hpp      KomutAlicisi (anlik, al), Komut, AlimSonucu, AnlikDurum
  katalog.hpp    üretilen katalog tablolarının tipleri
  (üretilen) yukler.hpp   yük yapıları, Kanal kimlikleri, kodlayıcı/çözücü bildirimleri
src/
  cekirdek.*     soketten bağımsız sunucu çekirdeği (erişim, yönlendirme, komutlar, abonelik, yayın)
  sunucu.cpp     soket döngüsü ve G/Ç iş parçacığı
  http.*         HTTP/1.1 istek ayrıştırıcısı, chunked çözücü, yanıt yazıcısı; WebSocket çerçeveleri
  json.*         sınırlı JSON çözücü/yazıcı (A3-K121 politikası, kanonik yazım)
  sema.*         JSON Schema alt kümesi doğrulayıcısı ve ECMAScript desen eşleyicisi
  halka.hpp      tek üretici–tek tüketici bayt halkası (A3-K38)
  ozet.*         SHA-1 (yalnız WebSocket el sıkışması), SHA-256, Base64
  rastgele.*     işletim sisteminin kriptografik kaynağı (getentropy / BCryptGenRandom)
  soket.*        engellemesiz TCP ve poll/WSAPoll
sahte/           sahte dünya ve komut alıcısı; ida_a3_sahte programı
test/            CTest (sim.a3.*) ve Node uyum testleri (uyum.mjs, uctan_uca.mjs)
```

## İş parçacıkları ve sınırlar

- **Dünya → A3:** `Yayinci` yalnız dünya iş parçacığından çağrılır; iki sabit boylu halkaya (telemetri 8 MiB, olay 2 MiB; ayarlanabilir) yazar, engellemez, kilit almaz, bellek ayırmaz. Yer yoksa kayıt düşer ve sayılır (`halka_tasma()` → `sayac/gercek_zaman.a3_halka_tasma`); düşen dizi kaydından sonraki ilk kaydın çerçevesinde `oncesinde_dusme` kurulur, düşen olay `olay_kaybi` ile bildirilir (A3-K44).
- **A3 G/Ç iş parçacığı:** soketleri engellemesiz yönetir; yayın tiki duvar saatiyle 50 Hz (A3-K33). Bir bağlantının bekleyen çıkışı 256 KiB'ı aşarsa o bağlantıya yeni veri eklenmez (durum yuvası ezilir, dizi kuyruğu en eskiyi atar, olay kuyruğu 4096'da 4001 ile kapanır); başka bağlantılar ve dünya beklemez (A3-K39…K42).
- **A3 → sim:** `KomutAlicisi::anlik` ve `al` G/Ç iş parçacığından çağrılır; dünyayı bekletmemeli ve kısa sürmelidir. `al` içinden `Yayinci` çağrılmaz (tek üretici dünyadır).
- **Sınırlar (mimari seçimi ya da bu uygulamanın seçimi; tahmin):** HTTP başlık bloğu 16 KiB, en çok 100 başlık; gövde 1 MiB (senaryo uçlarında 4 MiB; A3-K51); WebSocket istemci iletisi 64 KiB (A3-K47); ikili ileti 1 MiB (A3-K20); 64 HTTP ve 64 WebSocket bağlantısı (HTTP sınırında yer açmak için kimliği doğrulanmamış bağlantılar arasından yalnız yaş sırasıyla en eskisi kapatılır; kabulden sonra henüz okuma fırsatı bulmamış bağlantı kapatılmaz; kapatılacak bağlantı yokken okuma fırsatı bekleyen kimliği doğrulanmamış bağlantı varsa kabul o turda bırakılır (bekleyenler dinleme kuyruğunda kalır); 503 yalnız sınırı kimliği doğrulanmış bağlantılar doldurduğunda yazılmaya çalışılır: W4-K23 2, 2', 2''); bağlantı başına sırası bekleyen en çok 32 HTTP isteği / 8 MiB (aşılırsa bağlantı kapanır); olay geçmişi (W4-K22 b) tek gösterimle (çerçeve; `GET /olaylar` JSON'u istek anında üretir) saklanır ve pencere "son 65 536 olay ya da 64 MiB, hangisi önce dolarsa"dır: pencere dışı `sonra` 410 `gecmis_yok` alır, `merhaba`'daki `son_olay_sirasi` ile istemci aradakileri ister (A3-K45'in küçük sürüm notu); chunked istekte trailer ve chunk uzantısı 400 (en kısıtlayan okuma); HEAD yanıtı gövdesizdir; soket okuma bütçesi ayrıştırma aşamasına bağlıdır (başlık aşamasında 4096 bayt; A3-K51); başlık 2 s, gövde 30 s, boşta 30 s zaman aşımı.

## Bu dilimin kapsamı

Uygulanan uçlar: `surum`, `katalog`, `istemciler` (POST, DELETE), `durum`, `komutlar/{n}`, `olaylar`, `emniyet`, `emniyet/onay_jetonu`, `emniyet/degistir`, `yer_istasyonu/komutlar`, `yer_istasyonu/onay_jetonu`, `yer_istasyonu/komut`, `calistirma/{baslat,duraklat,surdur,adimla,hiz,durdur,sifirla}`, `yayin` ve `/ws`. Kataloğun öteki uçları (senaryo, ön denetim, hata enjeksiyonu, kayıt ve izleme, yardımcı süreçler, `calistirma/yeniden_kostur`) 404 `bulunamadi` döner; durum değiştirenler yine komut numarası alır (A3-K52).

EMNİYET (W4-K19): kısıtlayan emniyet isteği (acil durdurma bas, fiş çıkar, pim tak) ve `daraltici` sınıfındaki yer istasyonu komutu `beklenen_run_id` uyuşmasa da ya da hiç verilmese de alıcıya iletilir (`Komut::run_id_uyusmazligi`); kabul yanıtı şemadaki `yanit_kuyrukta`dır (dondurulmuş 1.0 şemasında olmayan `guncel_run_id` taşınmaz; güncel run_id olayda ve `GET /durum`da), uyuşmazlık `uyari` olayıyla (`kod` = `kisitlayan_run_id_uyusmazligi`) kaydedilir. Gevşeten isteklerde A3-K54 aynen geçerlidir.

EMNİYET (W4-K22 a): alıcı doluyken kısıtlayan etkiler sabit boylu, etki başına mandalda bekler (acil durdurma; tüp başına fiş ve pim; daraltıcı yer istasyonu komutu türü başına son istek). Aynı etkinin yeni isteği eskisinin yerini alır; eskisinin sonucu iletilenin dünya sonucuyla bildirilir (mandal başına en çok 64 birleşen sonuç kaydı; sınırda en eski sonuç kaydı atılır, etki atılmaz). Kısıtlayan hiçbir sınır yüzünden reddedilmez. Çıkış baskısı ve kapanış (W4-K23 3): HTTP girdisi çıkış baskısından bağımsız okunur ve istekler (Content-Length ve chunked) istek başına A3-K51 sınırıyla çerçevelenip sıraya girer; sıra HTTP/1.1 sırasıyla işlenir, baskı (bekleyen çıkış > 4 MiB) altında yalnız kimliği doğrulanmış kısıtlayan emniyet istekleri hemen işlenir (öne alınmış yanıtlar da eşiğe ve sıra tavanına uyar; bekleyen HTTP çıkışı kesin tavanlıdır: eşik + 16 MiB + 64 KiB, aşacaksa bağlantı kapanır: W4-K23 3'; kimliği doğrulanmamış bağlantıların bekleyen çıkış toplamı en çok 64 MiB, aşılacaksa en eski kimliği doğrulanmamış bağlantı çıkışı atılarak kapatılır: W4-K23 3''. Not: statik dosya yanıtı dosyanın tamamı belleğe okunarak üretilir; akışla gönderim ayrı iştir) ve yanıtları sırası gelince yazılır. Karşı taraf gönderim yönünü kapatınca sıradaki tam istekler işlenmeyi sürdürür (kısıtlayanlar hemen); bağlantı kapanırken sırada kalan kısıtlayanlar atılmadan işlenir. Sunucu döngüsü yeni bağlantıları mevcut okunabilir soketleri işledikten sonra kabul eder. Onay jetonu (A3-K94, W4-K23 4) Bearer doğrulamasının hemen ardından, yöntem, içerik türü, `Ida-Istemci`, gövde boyu, şema, kimlik çakışması, run/revizyon ve alıcı retleri dahil her erken retten önce tüketilir; biçimden kısıtlayan istekte tüketilmez (A3-K92, K104). Gevşetme ucuna gelen kimliği doğrulanmış isteğin jetonu okunamazsa (gövde sınırı, bozuk ya da nesne olmayan JSON, çerçeveleme hatası, eksik gövdeyle kapanış, sunucu kapanışı, Host ya da Origin reddi: W4-K23 4'') o istemcinin (`Ida-Istemci`) bekleyen bütün onay jetonları geçersizleşir; istemci kimliği eksik ya da geçersizse istek reddedilir ve hiçbir jetona dokunulmaz; jeton onu isteyen istemciye bağlıdır, yalnız aynı kimlikle sunulduğunda tüketilir (W4-K23 4'); gövde metninde jeton aranmaz. Statik kök (W4-K23 1): kök sunucu başlarken bir kez açılır ve ömür boyu tutulur (POSIX: kök fd'sine göreli `openat` + `O_NOFOLLOW` zinciri; Windows: silme/yeniden adlandırma paylaşımı verilmeden tutulan kök, her istekte birim seri numarası + dosya kimliğiyle doğrulanan yeniden açılış, bileşenler `FILE_FLAG_OPEN_REPARSE_POINT` ile); bağ, reparse noktası ve birden çok bağlantılı (sabit bağlı) dosya sunulmaz (404). Tehdit modeli: saldırgan aynı makinede çalışan, erişim jetonuna sahip olmayan ve statik köke (ve üst dizinlerine) yazamayan bir yerel süreçtir; bu denetimler derinlemesine savunmadır. **Köke yazabilen saldırgan model dışıdır** (paneli değiştirip jetonu zaten ele geçirebilir).

## Kitaplık seçimi

HTTP/WebSocket için dış kitaplık kullanılmadı (W4-a3 dilim 1 raporunda ayrıntılı). Gerekçe: sözleşmenin istediği ayrıntılar (A3-K51'in "en çok ilk 4096 bayt" okuması, A3-K07'de jetonu yansıtmadan alt protokol seçimi, A3-K39'un bağlantı başına sınırlı uygulama kuyrukları, A3-K46/K47 kapanış kodları) düşük düzey denetim gerektirir; gereken protokol alt kümesi (yalnız sunucu, TLS ve uzantı yok) küçüktür; bağımlılık eklenmediği için Windows CI süresi ve vcpkg kurulumu değişmez.
