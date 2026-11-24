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
- **Sınırlar (mimari seçimi ya da bu uygulamanın seçimi; tahmin):** HTTP başlık bloğu 16 KiB, en çok 100 başlık; gövde 1 MiB (senaryo uçlarında 4 MiB; A3-K51); WebSocket istemci iletisi 64 KiB (A3-K47); ikili ileti 1 MiB (A3-K20); 64 HTTP ve 64 WebSocket bağlantısı (HTTP sınırında en eski boştaki bağlantı kapatılıp yer açılır); olay geçmişi (W4-K22 b) tek gösterimle (çerçeve; `GET /olaylar` JSON'u istek anında üretir) saklanır ve pencere "son 65 536 olay ya da 64 MiB, hangisi önce dolarsa"dır: pencere dışı `sonra` 410 `gecmis_yok` alır, `merhaba`'daki `son_olay_sirasi` ile istemci aradakileri ister (A3-K45'in küçük sürüm notu); chunked istekte trailer ve chunk uzantısı 400 (en kısıtlayan okuma); HEAD yanıtı gövdesizdir; soket okuma bütçesi ayrıştırma aşamasına bağlıdır (başlık aşamasında 4096 bayt; A3-K51); başlık 10 s, gövde 30 s, boşta 30 s zaman aşımı.

## Bu dilimin kapsamı

Uygulanan uçlar: `surum`, `katalog`, `istemciler` (POST, DELETE), `durum`, `komutlar/{n}`, `olaylar`, `emniyet`, `emniyet/onay_jetonu`, `emniyet/degistir`, `yer_istasyonu/komutlar`, `yer_istasyonu/onay_jetonu`, `yer_istasyonu/komut`, `calistirma/{baslat,duraklat,surdur,adimla,hiz,durdur,sifirla}`, `yayin` ve `/ws`. Kataloğun öteki uçları (senaryo, ön denetim, hata enjeksiyonu, kayıt ve izleme, yardımcı süreçler, `calistirma/yeniden_kostur`) 404 `bulunamadi` döner; durum değiştirenler yine komut numarası alır (A3-K52).

EMNİYET (W4-K19): kısıtlayan emniyet isteği (acil durdurma bas, fiş çıkar, pim tak) ve `daraltici` sınıfındaki yer istasyonu komutu `beklenen_run_id` uyuşmasa da ya da hiç verilmese de alıcıya iletilir (`Komut::run_id_uyusmazligi`); kabul yanıtı `guncel_run_id` taşır, uyuşmazlık `uyari` olayıyla (`kod` = `kisitlayan_run_id_uyusmazligi`) kaydedilir. Gevşeten isteklerde A3-K54 aynen geçerlidir.

EMNİYET (W4-K22 a): alıcı doluyken kısıtlayan etkiler sabit boylu, etki başına mandalda bekler (acil durdurma; tüp başına fiş ve pim; daraltıcı yer istasyonu komutu türü başına son istek). Aynı etkinin yeni isteği eskisinin yerini alır; eskisinin sonucu iletilenin dünya sonucuyla bildirilir (mandal başına en çok 64 birleşen sonuç kaydı; sınırda en eski sonuç kaydı atılır, etki atılmaz). Kısıtlayan hiçbir sınır yüzünden reddedilmez. Onay jetonu, kimliği doğrulanmış ve çözülebilen gevşetme/onaylı istekte erken retler dahil ilk sunuluşta tüketilir (A3-K94). Statik kök altında simgesel bağ ya da reparse noktası içeren yol sunulmaz (`src/statik.*`).

## Kitaplık seçimi

HTTP/WebSocket için dış kitaplık kullanılmadı (W4-a3 dilim 1 raporunda ayrıntılı). Gerekçe: sözleşmenin istediği ayrıntılar (A3-K51'in "en çok ilk 4096 bayt" okuması, A3-K07'de jetonu yansıtmadan alt protokol seçimi, A3-K39'un bağlantı başına sınırlı uygulama kuyrukları, A3-K46/K47 kapanış kodları) düşük düzey denetim gerektirir; gereken protokol alt kümesi (yalnız sunucu, TLS ve uzantı yok) küçüktür; bağımlılık eklenmediği için Windows CI süresi ve vcpkg kurulumu değişmez.
