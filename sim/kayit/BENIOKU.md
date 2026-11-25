# sim/kayit — A6 kayıt kitaplığı (`ida_kayit`)

Bir çalıştırmanın kayıt dosyasını üreten ve okuyan kitaplık (`docs/sozlesmeler/A6-kayit.md`, sürüm 1.0): MCAP dosyası, kanal ve şema kataloğu, kayıt kademeleri, üreticileri bekletmeyen yazım yolu, üst veri ve ekler, kapanış, bütünlük denetimi, kurtarma okuması ve `izleme` düzeyi için sıralı okuyucu. Veri kaynağı soyuttur: bu dilimde (`docs/gorevler/W5-kayit.md`) kitaplık sahte üreticilerle sınanır; `ida_sim`'e bağlanması, tekrar sürücüleri ve A3 kayıt uçları sonraki dilimlerdir.

## Derleme

```
cmake --preset host-debug          # VCPKG_ROOT gerekir (mcap, zstd)
cmake --build --preset host-debug --target ida_kayit ida_kayit_dok ida_kayit_test ida_kayit_test_yazan
ctest --preset host-debug -R "sim\.kayit\."
```

Bağımlılıklar kökteki `vcpkg.json`'da sürümleriyle sabittir (W5-K35): `mcap` 2.1.3 (`foxglove/mcap` C++ kitaplığı; başlık-yalnız; MIT) yalnız `zstd` özelliğiyle ve `zstd` 1.5.7 (BSD-3-Clause; port "BSD-3-Clause OR GPL-2.0-only" bildirir, BSD seçilir). lz4 yoktur: port `lz4` özelliği kapalıyken `MCAP_COMPRESSION_NO_LZ4` tanımını verir, lz4 derlenmez ve bağlanmaz. Kitaplığın başlıkları yalnız `src/mcap_yazim.cpp`'de içerilir (ayrı hedef `ida_kayit_mcap`; üçüncü taraf kodu İDA uyarı politikasıyla derlenmez). Yük kodlayıcıları ve katalog tabloları derleme zamanında `docs/sozlesmeler/sema/a6-kanallar.json`'dan `arac/uret_kayit.py` ile üretilir (Python 3.9+, yalnız standart kitaplık).

## Yapı

```
include/ida/kayit/
  yazici.hpp     Yazici::ac, Uretici::yaz<Kanal>, guncelle, kapanis_taslagi, isle, kapat; Dosya (yazım hedefi)
  okuyucu.hpp    Okuyucu::ac / ac_bellek, DosyaOzeti, sonraki (log_time, sequence sırası), suz, ek_oku
  ust_veri.hpp   KapanisAlanlari, Ek, EksikKaydi, guvence_belirle (A6-K33), ust_veri_denetle
  katalog.hpp    üretilen tabloların tipleri: KanalTanimi, SemaTanimi, Kademe, üst veri anahtarları, ekler
  coz.hpp        tablo güdümlü yük çözücü (json_dok, alanlari_gez; A6-K48)
  (üretilen) yukler.hpp   A6'ya özgü yük yapıları, Kanal kimlikleri, KanalYuku<K>, AcilisUstVerisi
src/
  yazici.cpp     üretici yolu (kilitsiz) ve dosyaya yazan tek iş parçacığı; kapanış ve hata kapanışı
  halka.hpp      tek üretici–tek tüketici bayt halkası (paylı)
  dusme.hpp      üreticinin düşme sayaçları (sıra numaralı, kilitsiz)
  mcap_yazim.*   `foxglove/mcap` yazıcısının sarmalayıcısı (kitaplığın tek kullanıldığı yer)
  dosya.cpp      yerel dosya: dışlayıcı oluşturma, işletim sistemine teslim, kesme
  okuyucu.cpp    kendi MCAP ayrıştırıcısı: CRC doğrulaması, kurtarma okuması, sıralı okuma
  crc32.*, coz.cpp, katalog.cpp, ust_veri.cpp
arac/uret_kayit.py   katalog üreteci (A3 üretecini modül olarak yükler)
dok/main.cpp         ida_kayit_dok
test/                CTest (sim.kayit.*); çökme testinin yazan süreci (ida_kayit_test_yazan)
```

## Kodlayıcı: A3 ile aynı (A6-K08, W5-K36)

A3 ile ortak 24 yük (ör. `yuk_fizik_durum`, `yuk_olay`, sensör yükleri) yeniden üretilmez: `KanalYuku<K>::tip` doğrudan `ida::a3` yapısıdır ve baytları `ida::a3::kodla` yazar. Bunun için `sim/a3`'te davranış değiştirmeyen iki düzenleme yapıldı: üretilen `yukler.cpp` ayrı bir hedefe (`ida_a3_yukler`) alındı ve tel tipi yazıcısı `ida/a3/kodlayici.hpp` olarak genel başlığa açıldı (`sim/a3/src/kodlayici.hpp` oraya yönlendirir). A6'ya özgü 19 yükün yapı ve kodlayıcı metni A3 üretecinin aynı işlevleriyle (`Uretec.yapi_yaz`, `kodlayici_yaz`, …) ve aynı `ida::a3::Yazici` ile üretilir. Aynı adlı bir yük iki katalogda farklıysa üretim durur. `sim.kayit.kodlayici_ortak` her ortak yük için A3 kodlayıcısı ile A6 kataloğundan üretilmiş kodlayıcının aynı baytları verdiğini gösterir.

`kamera/kare` ve `yuk_kamera_kare` bu dilimde tablolarda yoktur (W5-K37); `kamera_kareleri` yalnız `kapali` olabilir.

## Yazım yolu ve iş parçacıkları (A6-K05, K06)

- **Üretici → yazıcı:** her üreticinin kendi sabit boylu halkası vardır. `Uretici::yaz<Kanal>(sim_t_ns, adim, yuk)` yükü doğrudan halkaya kodlar; **kilit almaz, bellek ayırmaz, dosyaya dokunmaz** (`sim.kayit.uretici_ayirmaz` ayırma sayısının 0 olduğunu, `uretici_beklemez` çağrı süresini ölçer). Halka doluysa ileti atılır ve üreticinin düşme sayaçlarına (kanal ve neden başına sayı, ilk/son sim zamanı ve adım) işlenir. Her üretici için `uretilen = kuyruklanan + dusen_halka + dusen_gecersiz`.
- **Yazıcı:** dosyaya tek iş parçacığı yazar (`Ayar::is_parcacigi`; kapalıyken çağıran `isle()` ile sürer). Her turda halkaları boşaltır, düşme sayaçlarındaki değişikliği `kayit/eksik` kaydına çevirir (A6-K30), süresi dolan parçayı kapatır.
- **Pay:** `olaylar` kademesinin kanalları (her kayıtta bulunması gerekenler; A6-K18) halkanın tamamını, öteki kanallar sekizde yedisini kullanır; telemetri taşarken olay iletileri girmeye devam eder (uygulama seçimi).
- **`kayit/eksik`** yalnız yazıcı tarafından üretilir; üretici arayüzünde bu kanala yazmak derleme hatasıdır.
- **Parçalar (A6-K03, K04):** zstd; hedef 1 MiB sıkıştırmasız veri (bir kayıt kadar aşılabilir); açık parça en geç 1 s duvar saatinde kapatılır ve her kapanan parça işletim sistemine teslim edilir (`write`; diske eşitleme `fsync` yapılmaz). Parça sınırını yazıcı yönetir; kitaplığın kendi eşiği hiçbir iletide dolmayacak kadar büyük verilir, böylece hangi iletinin hangi parçada olduğu kesin bilinir.
- **Saat:** `Ayar::saat` enjekte edilebilir (testler süreye bağlı kuralları gerçek zaman beklemeden sınar). `publish_time` çağrı anındaki kayıt duvar saatidir (açılışta 0); üretici isterse kendisi verir.

## Kapanış (A6-K12, K31, K44)

- **Olağan:** `kapat(alanlar)` bekleyen iletileri ve düşme bildirimlerini yazar, `ida_kayit_kapanis`'ı yazar ve dosyayı kapatır. `ileti_sayisi`, `kayit_eksik`, `ilk_eksik_adim`, `tekrar_guvencesi`, `guvence_on_eki_son_adim` ve `guvence_nedenleri`'ni yazıcı hesaplar (A6-K33). Kip × kademe için beklenen en yüksek düzey `beklenen_duzey()`'dedir (Tablo 4.6'nın kısıtlayan okuması: fiziğin koşmadığı `tekrar_girdi_sil` ve `tekrar_yeniden_kurma` kayıtları `fizik` iddia edemez).
- **Sıra sınırı:** `sequence` 2^32 − 1'e yaklaşınca (kapanıştaki `kayit/eksik` kayıtları için ayrılan pay kadar önce: 2·üretici·kanal·2 + üretici·kanal + 16) dosya `sinira_ulasti` ile kapatılır; üreticiler `kapali` alır, çalıştırma kayıtsız sürer.
- **Yazma hatası / disk dolu:** dosya son sağlam parçanın sonuna kesilir; kaybolan iletiler (`kayit/eksik`, neden `yazma_hatasi`/`disk_dolu`) ve kapanış kaydı elle, özet bölümü olmadan yazılır (MCAP'te özet isteğe bağlıdır). Bu da yazılamazsa dosya kapanmamış ama tutarlı kalır.
- **Erken kapanışta hiçbir ileti sessizce kaybolmaz:** sınırda da hatada da kapanış kaydından ÖNCE bütün üreticilerin halkaları boşaltılır; dosyaya yazılamayan her ileti kanal başına `kayit/eksik`'e çevrilir (aralık: en küçük ve en büyük adım; neden kapanışın nedeni, sınırda `yazma_hatasi`: yük şemasında sınır için ayrı değer yok) ve önek o iletilerin en küçük adımını kapsamaz (kanal ayrımı yapılmadan). Dosya kapandıktan SONRA halkaya girebilen ileti (kabul kesilirken yazmakta olan üretici) dosyaya yazılamaz; `YaziciSayaclari::kapali_iken_atilan`'da sayılır. Hesap: kuyruklanan = `yazilan_uretici_iletisi` + `yazimda_kaybolan` (`kayit/eksik`'te) + `kapali_iken_atilan`.
- Erken kapanışlarda kapanış kaydının çalıştırmaya ait alanları son `kapanis_taslagi()` çağrısından alınır. `kapat(alanlar)` kendi alanlarını girişte taslağa yazar: kapanış sırasında çıkan hata ya da sınır da onları kullanır.
- **Veri bölümü CRC'si (W5-K39):** yazıcı Data End `data_section_crc`'sini her kapanış yolunda yazar (kitaplığın `enableDataCRC` seçeneği; hata kapanışının elle yazılan kuyruğu son sağlam noktadaki CRC ara durumundan sürdürülür). Kapsam: dosyanın ilk baytından (baştaki sihirli baytlar dahil) Data End kaydının başına kadar. A6 §3.2'deki "Data End kitaplık varsayılanı" cümlesinden bilinçli sapmadır.

## Okuyucu (A6-K11, K46, K48, K49, K59, K60)

Okuyucu MCAP kitaplığının okuyucusunu kullanmaz. Açılışta dosyayı baştan sona tarar: her parçayı açar, CRC-32'sini kendisi hesaplar (0 değeri de karşılaştırılır), eklerin CRC'sini doğrular. Sonuç `DosyaOzeti`'dir:

| Durum | Ne okunur | Güvence |
|---|---|---|
| kapanmış, bozuk değil, üst verisi geçerli, veri bölümü CRC'si tutuyor, zorunlu ekleri tam | hepsi | kapanış kaydındaki; dosyanın içeriğinden yeniden hesaplananla yalnız KISITLANIR (aşağıda) |
| veri bölümü CRC'si yazılmamış (`data_section_crc` = 0) | hepsi | `izleme` (`guvence_kisitlari`) |
| zorunlu bir ek (A6 §3.6) yok; `kayit/eksik` kanalı okunamıyor (A6-K49) | hepsi | `izleme` (`guvence_kisitlari`) |
| kapanmamış (Footer, özet ya da Data End yok) | son tam ve CRC'si tutan parçaya kadar (kurtarma okuması) | `izleme` |
| bozuk parça (CRC, açılamayan, çözülemeyen) | ilk bozuk parçadan öncesi | `izleme`; tekrar reddedilir |
| veri bölümü CRC'si tutmuyor; bozuk ek; A6-K46'yı çiğneyen üst veri; kapanıştaki `ileti_sayisi` uyuşmuyor | parçaların hepsi | `izleme`; dosya bozuk |
| sonu kapalı (Footer ve sihirli baytlar yerinde) ama tarama oraya ulaşamıyor (bozuk kayıt uzunluğu) | bozulmadan önceki parçalar | `izleme`; dosya bozuk (kapanmamış sayılmaz) |
| açılış üst verisi eksik ya da geçersiz (zorunlu anahtar yok) | hepsi | `izleme` |
| sihirli bayt/Header yok, profil `ida` değil, farklı büyük `a6_surumu` | okunmaz | — |

Güvencenin kısıtlanması (W5-K39; dosya kendisi hakkında doğrulanamayan bir güvence veremez): düzey, kademe ve kipin verebileceğini (`beklenen_duzey`) aşamaz; önek, düzeyin kanallarında dosyada görülen ilk `kayit/eksik` adımını ve dosyada yazılmış son fizik adımını (`fizik/durum` ve `fizik/adim_sonucu` iletilerindeki en büyük `adim_no`; `DosyaOzeti::son_fizik_adimi`) aşamaz; erken kapanmış (`sinira_ulasti`, `yazma_hatasi`, `disk_dolu`) kayıt öneksiz güvence veremez. Okuyucu güvenceyi hiçbir zaman kapanış kaydındakinin üstüne çıkarmaz.

İletiler `log_time`, eşitlikte `sequence` sırasıyla verilir; bellekte yalnız zaman aralıkları örtüşen parçalar tutulur. Girdi güvenilmez sayılır: her uzunluk dosya boyuyla ve `OkumaAyari`'ndaki üst sınırlarla denetlenir (`sim.kayit.rastgele_bozulma`: 12 000 bozulmuş dosya; bozulmuş dosya özgününden yüksek güvence ya da geniş önek vermez).

## `ida_kayit_dok`

```
build/host-debug/sim/kayit/ida_kayit_dok <dosya.mcap> [--iletiler <n>]
```

Dosyanın özetini (bütünlük, üst veri, ekler, kanallar, ileti sayıları, eksikler) yazar. Çıkış kodu: 0 sağlam, 1 okunamadı, 2 bozuk, 3 kapanmamış.

## Bağlama dilimi için kancalar

(W5-bagla-kayit: kitaplık `ida_sim`'e bağlandı; ayrıntı, kanal tablosu ve seçilen okumalar `sim/sunucu/BENIOKU.md` "A6 kayıt". Bu bölüm kitaplığın bağlayana verdiği kancaları anlatır.)

- **Güvence tavanı (W5-K41, `sim/kayit`'e dar ekleme).** Kaydı yazan taraf, düzeyin gerektirdiği kanalların hepsini üretemiyorsa `KapanisAlanlari::guvence_tavani` (genellikle `izleme`) ve `guvence_tavan_nedeni` verir; `guvence_belirle` düzeyi yalnız AŞAĞI çeker ve nedeni `guvence_nedenleri`ne yazar (erken kapanışta taslaktaki tavan kullanılır). `sim.kayit.guvence` sınar; okuyucu zaten kapanış kaydındakinin üstüne çıkmaz.

Üreticiler (A6-K06; her biri ayrı `Uretici`, yalnız kendi iş parçacığından):

| Üretici | Kanalları |
|---|---|
| dünya iş parçacığı | `fizik/durum`, `fizik/adim_sonucu`, `sensor/*`, `sensor_temel/*`, `sensor_gercek/*`, `sonar/*`, `sonar_gercek/ping360`, `olay`, `a3/komut_sonucu`, `senaryo/oge_uygulama`, `emniyet/girdiler`, `atis/durum`, `ariza/etkin`, `calistirma/durum`, `sayac/gercek_zaman`, `hil/bariyer`; SIL'de `cekirdek/baslatma`, `cekirdek/cagri` |
| HIL G/Ç iş parçacığı | `a1/sim_kart`, `a1/kart_sim`, `hil/tf_yazim`, `hil/enjeksiyon`, `hil/oturum`, `hil/kart_acilis`, `hil/zaman_esleme`, `hil/dusen`, `kart/durum`, `yer_istasyonu/durum`, `yer_istasyonu/emulator`, `kart/baglanti_sayaclari`, `kart/cekirdek_sayaclari` |
| A3 sunucusu | `a3/komut`, `a3/gecersiz_istek` |
| A2 kare alıcısı | `kamera/halka_sayaclari`, `kamera/kare_dusme` (ileride `kamera/kare`) |
| A8 kayıt alıcısı | `algilama/kayit`, `algilama/parti`, `algilama/sayaclar` |

Kanca noktaları:

- **Çalıştırma başlarken:** `Yazici::ac(ayar, ust_veri, ekler, hata)`; hata "dosya var" ise ön denetimin `kayit_disk_alani` maddesi `kaldi` olur (A6-K01).
- **1 Hz denetçi:** `kapanis_taslagi()`; `sayaclar()` ve `Uretici::sayaclar()` ile `sayac/gercek_zaman.kayit_dusen`; `sayaclar().eksik_kaydi` arttıysa `kayit_eksik` olayı; `durum()` değiştiyse `kayit_durumu` olayı (A6-K12, K30, K31).
- **Canlı müdahale (A6-K53):** tekrar koşusunda ilk canlı müdahale kabul edildiği ANDA `kapanis_taslagi()` `canli_mudahale_adimi` ile yeniden çağrılır; 1 Hz'lik tur beklenmez. Dosya o aralıkta erken kapanırsa (sınır, yazma hatası) kapanış kaydı bayat taslağı kullanır ve gerçekte olmayan bir güvence (`fizik`) yazar.
- **`adim` etiketi:** `Uretici::yaz`'a verilen adım, erken kapanışta önekin ve `kayit/eksik` aralığının kaynağıdır. Bir üretici bir kanala adımları artan sırayla vermelidir; `fizik/durum` ve `fizik/adim_sonucu` için etiket yükteki `adim_no` ile aynı olmalıdır (okuyucu öneki yükteki değerle sınırlar).
- **Sonradan öğrenilen değerler:** `guncelle("kart_fw_version", …)` vb. (ilk `IDA_BOARD_HELLO`, görselleştirici ya da algılama bileşeni bağlandığında; A6-K46).
- **Çalıştırma biterken:** son `sayac/gercek_zaman` iletisi üretilir, üreticiler durur, `kapat(alanlar)` çağrılır.
- **İzleme (A3-K112):** `Okuyucu::ac`, `suz(ortak kanallar)`, `sonraki()`; ileti baytları A3 yüküyle aynıdır (çerçeve başlığı eklenip `kaynak_kayit` bayrağıyla yayınlanır).
- **Kayıt listesi:** `Okuyucu::ac(...)->ozet()`. Açılış dosyanın tamamını tarar (her parça açılır ve doğrulanır); çok sayıda büyük dosyanın listelenmesi için özetin önbelleğe alınması gerekebilir (ölçülmedi).

## Bilinen sınırlar

- MCAP'te üst veri (Metadata) kayıtlarının kendi CRC'si yoktur; onları veri bölümü CRC'si korur (W5-K39). Özet bölümü (Data End'den sonrası) bu CRC'nin dışındadır ve okuyucu özet CRC'sini doğrulamaz: okuyucu özete hiç dayanmaz, ama dosyayı özetinden okuyan genel MCAP araçları için özetin bozulmadığı güvencesi verilmez.
- Kapanış anındaki yarış: `kabul` kesilirken yazmakta olan bir üreticinin iletisi halkalar boşaltıldıktan sonra halkaya girerse çağrı `yazildi` döndürmüş olur ama ileti ne dosyada ne `kayit/eksik`'tedir; yalnız `kapali_iken_atilan` sayacında görünür. Olağan kapanışta üreticiler `kapat`'tan önce durdurulur; erken kapanışta pencere bir iletilik bir yazım süresidir.
- Açılış üst verisi `$defs/ust_veri_ida_kayit`'in karşılığıyla denetlenir (anahtar kümesi, zorunluluk, onluk u32, `kip = sil` → `sil_n_bg`); değerlerin anlamı (özet biçimleri vb.) denetlenmez.
- `cekirdek/*` yüklerindeki A4 yapılarının alan alan kodlanması (A6-K52) ve parçalı `atama_celiskisi` olaylarının birleştirilmesi (A6-K61) bu dilimde yoktur: ilki çekirdeği çağıran platform katmanının, ikincisi olay ayrıntısını yorumlayan tüketicinin işidir.
- Halka boyları, yoklama aralığı (2 ms) ve pay oranı tahmindir; hedef Windows bilgisayarında ölçülmedi (A6 §8 md.1).
