# sim/a2 — A2 sunucusu, sim tarafı (`ida_a2`)

Sim ile görselleştirici (`viz/`) arasındaki A2 bağlantısının sim tarafı (`docs/sozlesmeler/A2-gorsellestirici.md`): yalnız `127.0.0.1`'de tek TCP dinleyicisi, sahiplik kilidi, el sıkışma, durum akışı ve sahne olayları, nabız, kopma ve yeniden bağlanma, çalıştırma nesli. C++20 kitaplığıdır; veri kaynağı soyuttur. Bu dilimde (`docs/gorevler/W5-a2-sunucu.md`) sunucu sahte bir kaynakla sınanır; `ida_sim`'e bağlanması (`--a2-port`, dünya döngüsünden yayın) W5-bagla-a2'de yapıldı: bağlama `sim/sunucu/src/a2_baglanti.*`'dedir (`sim/sunucu/BENIOKU.md` "A2 sunucusu").

Bu dilimde **yok**: kamera hattı (kare isteği, kare halkası, paylaşımlı bellek, halka kayıt dosyası), atlama ve gövde tablosu (tekrar izleme), A3'e bildirim. Her oturumda `kamera_sayisi = 0` bildirilir; `kamera_istegi`, `kamera_iptal`, `halka_yenile`, `govde_tablosu` ve `atlama` üretilmez; görselleştiriciden gelen `kare_hazir`, `kare_dustu`, `halka_eslendi` protokol hatasıdır (W5-K30).

## Derleme ve test

```
cmake --preset host-debug
cmake --build --preset host-debug --target ida_a2 ida_a2_sahte ida_a2_test
ctest --preset host-debug -R "sim\.a2\."
```

Dış bağımlılık yoktur (vcpkg gerekmez); Windows'ta `ws2_32`. Kitaplık `viz/`, `sim/a3`, `sim/kopru` ve `sim/sunucu` hedeflerine bağlanmaz; tel kodlaması `sim/include/ida/a2_wire.h` yapılarından yazılır (W5-K29). Testler karşı uç olarak gerçek istemciyi (`viz/a2`'nin kodeği ve oturumu: `ida_viz_a2`) kullanır.

## Sahte kaynakla gerçek görselleştiriciyi denemek

```
build/host-debug/sim/a2/ida_a2_sahte --manifest assets/acik_su/manifest.json
```

Program A2'yi 47210 portunda açar (`--port N`), durumunu ilk satırda yazar (`A2_DURUM dinliyor port=47210`) ve fizik olmayan bir sahneyi (dairede dönen araç, sabit hedef, 20. saniyede torpido) gerçek zamanlı yayınlar. Seçenekler: `--sil` (SIL kipi: seyreltme), `--hiz X`, `--hedef N`, `--firlat S`, `--sure S`, `--kilit DOSYA`, `--bos-port`.

## Yapı

```
include/ida/a2/
  yayinci.hpp    Yayinci: dünya iş parçacığının yayın işlevleri (birim ve çerçeve kabulleri burada)
  sunucu.hpp     Sunucu::baslat(ayar, hata), yayinci(), durum(), sayaclar(), durdur()
src/
  yayinci.*      yayın halkası ve tam durum kesiti (dünya tarafı)
  son_deger.hpp  tek yazar–tek okur üç yuvalı "son değer" deposu (yazar beklemez)
  halka.hpp      tek üretici–tek tüketici bayt halkası (atılabilir kayıtlara ayrılmış pay ile)
  takvim.hpp     60 Hz durum takvimi (adım numarasından)
  cekirdek.*     soketten bağımsız oturum çekirdeği (el sıkışma, akış, seyreltme, nabız, süreler)
  kuyruk.hpp     bağlantı başına gönderme kuyruğu (8 MiB; A2-K26)
  kodlayici.*    tel kodlaması (sim → görselleştirici) ve gelen ileti okuyucusu
  sunucu.cpp     soket döngüsü ve G/Ç iş parçacığı
  soket.*        engellemesiz TCP ve poll/WSAPoll
  kilit.*        sahiplik kilidi (flock / LockFileEx)
  saat.*         duvar saati (A2 §2) ve kısa bekleme
  sha256.*       SHA-256 (dalga kümesi özeti)
sahte/           sahte veri kaynağı (fizik değil) ve ida_a2_sahte programı
test/            CTest (sim.a2.*)
```

## Veri kaynağı arayüzü (dünya iş parçacığı)

`Yayinci` yalnız dünya iş parçacığından çağrılır. Hiçbir işlevi engellemez, kilit almaz, bellek ayırmaz ve sistem çağrısı yapmaz; görselleştiricinin yokluğu, yavaşlığı, donması ya da çökmesi bu işlevlerin yaptığı işi değiştirmez (A2-K17, K26, K47). Birim ve çerçeve kabulleri (`v_n` NED'dedir, `w_b` gövde çerçevesindedir, kuaterniyon w x y z gövdeden dünyaya) `yayinci.hpp` başındadır.

Bir fizik adımı sınırında sıra: önce o sınırın olayları (`govde_dogdu`, `govde_oldu`, `kutle_durumu`, `sureklilik_kir`, `isabet`), sonra `adim_siniri(adim_no, durumlar)` (olaydan sonraki durum; A2-K21). 60 Hz takvimini (adım numarasından; 13/14 adım), takvim dışı iletileri ve süreklilik numarasını kitaplık yönetir. Öteki işlevler: `yeni_calistirma` (ilk çağrı nesil 1; sonrakiler nesli artırır ve oturumu `veda(yeni_calistirma)` ile kapatır), `sifirla` (nesil artar, oturum sürer), `duraklat`/`surdur`, `calistirma_durdu`, `dalga_kumesi`/`ortam`/`malzeme_optik`, `kapaniyor`.

Çağıranın yükümlülükleri: gövde kimliği oturum içinde yeniden kullanılmaz (sıfırlamadan sonra doğan gövdeler yeni kimlik alır; A2-K23); `adim_siniri` her adım sınırında bir kez çağrılır; duraklatılmışken çağrılmaz.

## İş parçacıkları ve tutarlılık

- **Dünya → A2, iki yol.** (1) *Kesit*: her yayında güncellenen tam durum (çalıştırma tanımı, ortam, dalga kümesi, malzeme, yaşayan gövdeler ve o adım sınırındaki durumları); üç yuvalı depoda durur, yazar beklemez. (2) *Halka*: sıralı kayıt akışı (olay, durum, küme değişimi, yeni çalıştırma, kapanış); 4 MiB (ayar).
- **Değişmez.** Dünya önce kayıtları kapsayan kesiti yayına alır, sonra kayıtları halkaya yazar; her kayıt bir sıra numarası taşır. G/Ç iş parçacığı oturumu kesitten kurar ve sırası kesitinkinden büyük olmayan kayıtları atar. Böylece koşunun ortasında (ya da dünya ilerlemezken, duraklatılmışken) bağlanan oturumda `hosgeldin` gövde tablosu ile ilk `durum` aynı adım sınırındandır ve arada kaybolan ya da iki kez uygulanan doğum/ölüm olmaz (A2-K65). `sim.a2.kesit_halka_degismezi` bunu bir yayının her ara noktasında belirlenimci olarak, `sim.a2.ortada_baglanma_eszamanli` gerçek iki iş parçacığıyla sınar.
- **Taşma.** Takvimli `durum` halkaya sığmazsa atılır ve sayılır (`yayin.dusen_durum`); halkanın dörtte biri (en çok 1 MiB) atılamayan kayıtlara ayrılmıştır. Atılamayan bir kayıt (olay, takvim dışı durum, küme) yine de yazılamazsa ya da bir adım sınırında 192'den çok olay verilirse bağlı oturum `veda(tampon_tasti)` ile kapanır (`kopma: yayin_tasmasi`); istemci yeniden bağlanınca tam durumu kesitten alır.
- **G/Ç iş parçacığı.** Soketleri engellemesiz yönetir; dünya uyandırma göndermez (sistem çağrısı gerektirir): döngü oturum varken 2 ms, yokken 20 ms aralıkla halkayı yoklar (ölçülmemiş seçim; `durum` gecikmesine en çok bir tik ekler). Gönderme kuyruğu 8 MiB'dir (A2-K26): dolunca gönderilmemiş takvimli `durum` iletileri atılır ve sayılır; öteki iletilere yer kalmazsa `veda(tampon_tasti)`.
- **Bellek üst sınırı (hesap; ölçülmedi):** yayın halkası 4 MiB + kesit yuvaları ≈ 3 × 34 KiB + gönderme kuyruğu ≤ 8 MiB (+ 64 KiB sığa payı) + kapanışı süren en çok 8 soket × ≤ 270 KiB. Gelen yön için tampon sabittir (160 bayt: en büyük gelen ileti `merhaba`).

## Seçilen okumalar (K20; ayrıntı iş raporunda)

- **Süreler.** `merhaba` için 5 s (A2-K10) kabulden itibaren işler; 3 s'lik ölü sayma ölçütü (A2-K62) `merhaba`'nın alındığı andan itibaren, `hazir` beklenen 120 s'lik pencerede de işler; `sim_nabzi` `hosgeldin`'den itibaren 1 s aralıkla gider (W5-K28).
- **İlk durum.** Oturum kurulunca `malzeme_optik`'ten hemen sonra kesitin adım sınırındaki `durum` gönderilir (takvimde değilse takvim dışı bayrağıyla; duraklatılmışken `calisma_durumu = 2` ile). Duraklatmada o sınırın durumu `duraklat` olayından *önce* gider; olaydan sonra sürdürene kadar durum gitmez (A2-K24).
- **Sınır iletileri.** `T − h` ve `T` iletileri yalnız süreklilik numarası değişince değil, gövde doğumu ve ölümünde de gider. `T − h` az önce takvimli olarak gittiyse yinelenmez. Sınır iletileri takvim adımına denk gelse de seyreltilmez ve atılmaz.
- **SIL seyreltmesi (A2-K25).** Jeton kovası: sürekli 100 ileti/s, kova 20 ileti; herhangi bir 1 s'lik pencerede en çok 120 takvimli durum. 1x hızda seyreltme olmaz.
- **`merhaba` alan hataları.** Geçersiz platform, çizici ve NUL'suz metin `ret(protokol)` alır (§3.3); başlık ve sıra hataları `veda(protokol_hatasi)` (A2-K06, K07). Ana sürüm öteki alanlardan önce değerlendirilir.
- **Çalıştırma tanımı yokken** (`yeni_calistirma` henüz çağrılmadı) `hosgeldin` gönderilemez; sunucu sessiz kalır, tanım gelince el sıkışma sürer (istemci 3 s sonra kendi kuralıyla kopup yeniden dener).
- **Kapanışta sıra numarası.** Kapanışta hiç gönderilmemiş iletiler atılırsa son `ret`/`veda` onların sırasını yeniden kullanır; istemcinin gördüğü sıra 0'dan başlar ve boşluk yalnız atılan takvimli durumlardan doğar (A2-K07).

## A2'siz durum

Sahiplik kilidi (`<kullanıcının geçici dizini>/ida/a2.kilit`; A2-K77) ya da port alınamazsa `Sunucu::baslat` yine nesne döndürür: `durum()` nedeni söyler (`kilit_baskasinda`, `kilit_acilamadi`, `port_acilamadi`), yayın işlevleri hemen `false` döner, dünya etkilenmez (A2-K05, K79). Aralık dışı port (A2-K93: 1024 … 65 535) ile `baslat` `nullptr` döner ve nedeni bildirir. Kilit, `Sunucu` nesnesi yaşadıkça (durdurulsa da) tutulur.
