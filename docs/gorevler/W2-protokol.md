# W2-protokol — `ida_link` tel katmanı (çerçeveleme, kodlama, altın baytlar)

Tarih: 2026-11-20. Yazan: `ida-uygulayici` (Opus; K18). Kabul testleri: `ida-test-yazari` (kodu yazandan bağımsız; uygulamadan sonra). İnceleyen: sol. Dal: `is/w2-protokol` (taban: `is/w2-depo`'nun kayda geçmiş son hâli). Kayıt: `ida-kayit`.

Bu iş A1 sözleşmesinin **tel düzeyini** ürün koduna çevirir: bayt akışından doğrulanmış çerçeve çıkaran alıcı, tipli mesajdan çerçeve üreten gönderici, sayaçlar ve sözleşmenin makinece denetlenebilir değerleri (altın baytlar, `dialect_hash`, ayna numaralandırma eşitlikleri). Başvuru: `docs/sozlesmeler/A1-kart-baglantisi.md` §3.1–§3.3, §3.11, §4.1, A1-K101, §7.1 (K02–K08, K10 satırları), §7.3, Ek B; `protocol/ida.xml`; `core/include/ida/port.h`; iskelet §7. Sözleşme ile bu belge çelişirse sözleşme geçerlidir ve çelişki raporlanır.

İskelet §7'ye göre `ida_link` tek modüldür (çerçeveleme + A1↔A4 dönüştürücü). Bu iş modülün alt katmanını yazar; dönüştürücü (oturum, parti, alan eşlemesi) aynı modüle W3/W4 işlerinde eklenir. Tel katmanının açık arayüzü, dönüştürücünün ve sim tarafındaki HIL köprüsünün üzerine kurulacağı yerdir; bu yüzden küçük, durumsuz çağrılara dayalı ve kararlı olmalıdır.

## Kapsam

### 1. Kod üretimi

- Üretilen MAVLink C başlıkları `protocol/ida.xml`'den, W2-depo'nun sabitlediği pymavlink sürümüyle (`--lang=C --wire-protocol=2.0`) derleme sırasında üretilir; depoya girmez; uyarı politikası dışında (sistem başlığı olarak) içerilir. W2-depo'nun `cmake/IdaMavlink.cmake` düzeni kullanılır; değişiklik gerekiyorsa gerekçesiyle yapılır ve raporlanır.
- `MAVLINK_CHECK_MESSAGE_LENGTH` tanımlanmaz (A1 §7.3: tanımlıyken kırpılmış yük reddediliyor); uzunluk denetimi bu katmanda yapılır (A1-K05).
- Üretilen kitaplığın yalnız çağıranın durumuyla çalışan işlevleri kullanılır; kanal dizilerini kullanan işlevler kullanılmaz (A1-K101). Bunun derleme düzeyinde bir güvencesi kurulur (ör. kanal işlevlerinin bağlanan nesnelerde bulunmadığının denetimi) ve nasıl kurulduğu raporlanır.

### 2. Tel katmanı (`protocol/ida_link/`)

- Dil ve sınırlar: ISO C17, `-pedantic-errors`, uzantı yok; bellek ayırma yok; işletim sistemi, saat, dosya ve standart giriş/çıkış bağımlılığı yok; değişebilir genel (global ya da `static`) durum yok: bütün durum çağıranın verdiği örnek yapısındadır. Aynı kaynak host kökünde ve firmware kökünde derlenir; açık başlık C++20'den de içerilir.
- **Alıcı:** baytlar parça parça verilir; her doğrulanmış çerçeve için mesaj kimliği ve mesajın en büyük yük boyuna sıfırla doldurulmuş yük teslim edilir. Uygulanacak kurallar: A1-K02 (yalnız `0xFD`; `incompat_flags ≠ 0` ve MAVLink 1 atılır), A1-K04 (karşı tarafın `sysid`/`compid` değeri; örnek kurulurken rol verilir: kart ya da sim), A1-K05 (kırpılmış ve kırpılmamış kabul; `len` = 0 ya da azamiyi aşan ret), A1-K06 (dialect dışı ya da yönü yanlış kimlik; yön bilgisi A1 Tablo 3.3'ten), A1-K07 (sağlama hatasında yeniden eşleme; çerçeve dışı baytların sayımı), A1-K08 (`seq` boşluğu yalnız sayılır). Her ret sözleşmedeki sayacı artırır (§3.11'deki adlar ve genişlikler).
- **Gönderici:** tipli mesaj yapısından çağıranın tamponuna tam bir çerçeve yazar: yük kırpması (A1-K05; ilk bayt kırpılmaz), örnek başına `seq` (A1-K08), role göre `sysid`/`compid` (A1-K04). Tampon yetmezse hiçbir bayt yazılmaz ve hata döner. Taşımaya yazım, kuyruk ve öncelik bu işin konusu değildir (A1-K10, K90–K96, K110 kart yazılımı ve HIL köprüsü işlerindedir).
- **Sayaçlar:** çağıranın okuyabildiği yapı; taşma davranışı sözleşmedeki gibidir (sözleşme söylemiyorsa doyma seçilir ve raporlanır).
- Hata yolları dahil hiçbir girdi tanımsız davranış üretmez: geçersiz bayt dizisi, sıfır uzunluklu besleme, yetersiz çıkış tamponu, boş gösterici dışında (boş göstericinin sözleşmesi başlıkta yazılır).

### 3. Sözleşme değerlerinin makinece denetimi

- **`dialect_hash`:** A1 §3.2'deki kanonik metin `protocol/ida.xml`'den derleme sırasında kurulur (Python, yalnız standart kitaplık); değer koda sabit olarak girer. Beklenen değer ve satır sayıları (mesaj, alan, numaralandırma girdisi) depoda tek bir veri dosyasında durur ve bir test hesaplananla karşılaştırır; XML değişirse test kırılır ve beklenen değer bilerek güncellenir. Değer elle koda yazılmaz.
- **Ayna numaralandırmalar (A1-K03):** K03'te sayılan numaralandırmaların her girdisi için `IDAW_…` = `IDA_…` eşitliği derleme zamanında denetlenir (C17 ve C++20; üretilen başlık ile `port.h` aynı çeviri biriminde). Ad eşlemesi mekanik değildir (önek değişimi her girdide tutmaz; W2-depo raporu); eşleme, sözleşmeyle birlikte dondurulan makinece okunur eşleme dosyasından alınır (A1 yazarı dondurma öncesi son turda ekler; yolu A1 §7.3'te yazılıdır). Eşitlik kaynak dosyası bu eşleme dosyasından üretilir, elle yazılmaz; bir denetim, K03'te sayılan numaralandırmaların XML'deki her girdisinin eşleme dosyasında tam bir kez bulunduğunu (`IDAW_REASON` yalnız 0–15) ve eşitlik sayısının sözleşmedeki sayıya (A1 §7.3) eşit olduğunu doğrular. Eşleme dosyası bu iş başladığında yoksa iş bu maddede durur ve raporlar; eşlemeyi kendi tahminiyle kurmaz.
- **Altın baytlar (A1 Ek B):** 38 çerçeve sözleşme metninden betikle çıkarılır (elle kopyalanmaz) ve makinece okunur bir veri dosyasına yazılır; bir denetim, veri dosyasının sözleşmedeki Ek B ile aynı olduğunu her derlemede doğrular. Her çerçeve için: alıcı kabul eder; kimlik, yük boyu ve `CRC_EXTRA` Ek B'deki başlık satırıyla aynıdır; çözülen mesaj yeniden kodlanınca bayt bayt aynı dizi çıkar; aynı çerçevenin kırpılmamış biçimi de kabul edilir ve aynı mesajı verir.

### 4. Birim testleri (uygulayıcı yazar; Unity, CTest)

A1 §7.1'in bu katmana düşen satırları: K02 ve K04–K07 (her başlık hatası türü; atılan çerçeve, artan sayaç, ardından gelen geçerli çerçevenin kabulü), K05 (`len` sınırları), K07 (belirlenimci sözde rastgele bayt akışına gömülü geçerli çerçeveler; bozulan adaydan sonraki ilk geçerli çerçevenin yakalanması), K08, K101 (aynı süreçte birbirine karışarak beslenen iki örneğin, ayrı ayrı beslenen örneklerle aynı sonucu vermesi; A1 §7.3'te "doğrulanmadı" denen madde budur). Testler `host-asan` ön ayarında da koşar. Rastgelelik sabit tohumludur; saatten tohum alınmaz. Aynı testlerin firmware araç zinciriyle derlenebildiği gösterilir (kartta koşturma yok).

### 5. Boyut ve host maliyeti

- Firmware kökünde kitaplığın kapladığı yer (text, rodata, bss; örnek yapısının boyu) raporlanır.
- Host'ta küçük bir ölçüm programı: en büyük çerçevenin ve 1 200 B'lık bir pencere dolusu tipik çerçevenin ayrıştırma ve kodlama süresi. Sonuç **yalnız host ölçümüdür**; kart maliyeti (A1 §8.1 O2, MP5) ayrı kart işidir ve bu rakamlardan çıkarılmaz. Ölçüm programı kartta koşturulabilecek biçimde ayrılır (çekirdek döngü host'a özgü kod içermez).

## Kapsam dışı

A1↔A4 dönüştürücü (oturum, el sıkışma, parti, sensör ve komut eşlemesi, olaylar; W3/W4); kayıttan yeniden kurma girişi (A1-K127; W5); taşıma (UART, soket), gönderim kuyruğu ve önceliği; sim'in pencere planlayıcısı; `protocol/komutlar.json` denetimi (A1-K125; A3 sunucusu işi); C++ sarmalayıcı sınıflar (yalnız C++20'den içerilebilirlik gösterilir); kartta çalıştırma ve MP5 ölçümü; `protocol/ida.xml`, `port.h` ve sözleşme metninde değişiklik.

## Kabul

1. `host-debug`, `host-asan`, `fw-debug`, `fw-release` ön ayarlarında uyarısız derlenir; birim testleri host'ta geçer; bellek ayırma denetimi kitaplık nesnelerinde ve firmware bağlamasında ayırma simgesi bulmaz.
2. 38 altın çerçevenin 38'i çözülür ve yeniden kodlanınca bayt bayt aynıdır; veri dosyası ile Ek B'nin eşitliği denetimi, Ek B'nin bir baytı bilerek değiştirilince başarısız olur.
3. `dialect_hash` derlemede hesaplanır ve sözleşmedeki değere eşittir; XML'de bir alan adı bilerek değiştirilince test başarısız olur.
4. Ayna eşitlikleri C17 ve C++20'de derleme zamanında geçer; `port.h`'nin bir kopyasında tek bir değer bilerek değiştirilince derleme başarısız olur (deneme hedefi).
5. §4'teki her kural satırı için en az bir test vardır ve rapordaki tabloda kural → test adı eşlemesi verilir.
6. Kitaplıkta değişebilir genel durum yoktur (simge tablosundan gösterilir) ve iki örnek testi geçer.
7. Boyut raporu ve host ölçümü raporda; ölçüm koşulları (derleyici, ön ayar, makine, yineleme sayısı) yazılıdır.

Bağımsız kabul testleri (`tests/kabul/protokol/`; `ida-test-yazari`): yalnız bu belge, sözleşme ve açık başlık okunarak yazılır; uygulama kaynağı okunmaz. Kapsamı: A1-K02–K08'in her biri için sözleşme metninden türetilmiş vakalar ve Ek B çerçeveleri. Test ile uygulama uyuşmazsa hangisinin sözleşmeye aykırı olduğuna mimar karar verir.

## Rapor

Yazılan dosyalar; açık arayüzün kısa tanımı (işlevler, örnek yapısı, hata kodları); her kabul maddesinin sonucu (komut ve çıktı özetiyle); kural → test tablosu; boyut ve host ölçümü; sözleşmede bulunan belirsizlik ya da çelişkiler (ayrı başlık; sözleşmeye dokunulmaz); doğrulanamayanlar (MSVC yalnız CI'da); çalışma dizini ve dal. Commit atılmaz.
