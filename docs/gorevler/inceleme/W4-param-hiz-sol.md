# sol incelemesi: param

## Hüküm

**Bu haliyle kabul edilemez.** `bc83bca..0fa75ef` değişikliğinde, bozulmuş şema özeti önbelleğinin kalıcı kayda taşınmasına izin veren **1 kritik**, test ve raporlama kapsamına ilişkin **3 önemli** bulgu var.

İnceleme kaynak okuması, Git karşılaştırması, bellekte bağımsız hesaplama ve canlı birincil kaynak denetimiyle yapıldı. Dosya değiştirilmedi; C testleri, derlemeler, MSVC analizi ve kart ölçümleri çalıştırılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `core/src/param/param_komut.c:203`; `core/src/param/param.h:52`; `core/src/cerceve/cerceve.c:41` | **Özet önbelleği bütünlük denetiminin dışında.** Geçerli init sonrasında yalnız `c->prm.sema_ozeti ^= 1` yapılırsa koruma sözcükleri ve mevcut tutarlılık kontrolleri geçmeye devam eder. Silahsız `PARAM_SAVE`, bozuk özeti başlığa yazar; CRC yalnız yükü kapsadığından doğru kalır. Platform bu bloğu saklayabilir; sonraki init doğru özeti yeniden hesaplayıp bloğu denetim (2) ile reddeder. Böylece geçerli parametre kaydı, çekirdek bozulmayı bildirmeden kullanılamaz hale gelir. A4 §3.10, K79 ve K89 ile uyumsuzdur. | Önbelleği doğrulanabilir korumayla saklayın veya derleme sırasında üretilen salt okunur özet kullanın. Bozulmayı bütünlük sonucuna taşıyın ve yanlış özetle blok üretilmesini engelleyin. Tek bit bozma → SAVE → yeniden init karşı örneğini teste ekleyin. | kritik |
| 2 | `core/test/test_param_hiz.c:54`; `core/src/param/param.c:194` | **Eski blok referansı, şema özeti açısından bağımsız değil.** `ref_blok_kodla`, güncel `ida_prm_sema_ozeti()` üzerinden yeni `ida_prm_sema_ozeti_tablo()` yardımcısını çağırıyor. Bu yardımcıdaki bir değişiklik hem referansın hem yeni kodlayıcının başlığını birlikte değiştirebilir; W4 bayt eşitliği testi bunu yakalayamaz. Bit bit CRC referansı gerçekten eskidir. | Önceki sürümün özet hesabını bağımsız referans olarak koruyun veya mevcut şema için bağımsız üretilmiş özet/altın blok sabitleyin. | önemli |
| 3 | `core/test/test_param_hiz.c:145`; `core/test/test_param_hiz.c:168` | **“Her tip” kapsamı sağlanmıyor.** Testler yalnız derlenmiş tabloyu dolaşıyor; bu tabloda 28 U32 ve 131 F32, **0 I32** var. `INT32_MIN/MAX` bit desenlerini mevcut satırlara yazmak, I32 tipli kayıt kodlamasını sınamıyor. Görev belgesinin kabul maddesi 1 eksik kalıyor. | Ürün şemasını değiştirmeden I32 tipli kayıt içeren uygun bir test fixture’ıyla uç ve rastgele değerlerin bayt eşitliğini sınayın; kapsam iddiasını gerçek testlerle eşleştirin. | önemli |
| 4 | `core/TASARIM.md:95` | **Süre kabulünün en kötü değerleri eksik.** Görev belgesi kabul maddesi 4, önce/sonra ortanca **ve en kötü** kodlama süresini istiyor. Kayıtta yalnız ortancalar bulunuyor; bunlar çağrının en kötü gecikmesinin iyileştiğini göstermiyor. | Aynı ölçüm koşullarında tekrar sayısını ve önce/sonra en büyük süreleri raporlayın. Mevcut ortanca sonuçlarını kart hedefinin sağlandığına kanıt saymayın. | önemli |

## Yeniden hesaplananlar

- **CRC tablosu:** `0xEDB88320` ile sekiz sağa kaydırma/XOR adımından 256 girdiyi yeniden ürettim; **256/256 eşleşti**. Başlangıç ve son XOR `0xFFFFFFFF`; bayt güncellemesi ve yansıtılmış bit sırası [W3C’nin birincil CRC örneğiyle](https://www.w3.org/TR/PNG-CRCAppendix.html) uyumlu.
- **CRC vektörleri:** boş girdi `00000000`; `"123456789"` `CBF43926`; `00` `D202EF8D`; `FF` `FF000000`; `"a"` `E8B7BE43`; 8192 sıfır bayt `D8F49994`; `(7i+3) mod 256` desenli 8192 bayt `B65EF7BF`. Ayrıca bellekte 10.000 rastgele girdide tablo hesabını `zlib.crc32` ile karşılaştırdım; fark yok.
- **Şema:** taban ve yeni sürümün 159 parametre tanımı aynı. Kanonik metin **2426 bayt**; SHA-256’nın ilk sekiz baytı `46 88 7f a7 14 54 06 34`, küçük uçlu özet **`0x34065414A77F8846`**. Eski özet işlevinin gövdesi, tablo/sayı değişkenleştirmesi dışında yeni yardımcıyla aynı. SHA-256 tanımı [NIST FIPS 180-4](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf) üzerinden denetlendi.
- **Varsayılan blok:** yük `159 × 8 = 1272`, toplam `24 + 1272 = 1296` bayt; yük CRC’si **`0xB011EAB1`**. Özetin düşük biti değiştirildiğinde başlık özeti `0x34065414A77F8847` olurken bu CRC değişmiyor; bulgu 1’in kalıcı blok sonucu budur.
- **Sınırlar:** `(8000−24)/8 = 997` kayıt. Son mevcut kayıt 1288, değer alanı 1292 konumunda; son erişilen bayt 1295. Çözücü, yükü dolaşmadan uzunluğu doğruluyor; `TABLO[r]` erişiminden önce kayıt sayısını şema sayısına eşitliyor.
- **Boyut ve süre aritmetiği:** tablo `256 × 4 = 1024` bayt; bildirilen arşiv farkı `62229−61241 = 988` bayt, örnek farkı `12592−12584 = 8` bayt. Bildirilen ortancalar yaklaşık **6,91 kat** kodlama ve **4,69 kat** SAVE adımı hızlanmasına karşılık geliyor; ölçümlerin kendisi yeniden yapılmadı.

## İskelete ya da diğer bölümlere notlar

- Normal init sırasında `ida_prm_kur`, sıfırlamadan sonra ve blok çözümünden önce çağrılıyor. Başarısız init örneği kurulu bırakmıyor; başarılı yeniden init özeti tekrar hesaplıyor. İncelenen üretim yollarında kurulumdan önce sıfır önbellekle blok kabulü/yazımı yolu bulmadım. `ida_core_info` örneksizdir ve özeti tablodan hesaplamaya devam ediyor.
- Silahlıyken ve aynı çağrıda ARM varken SAVE reddi, kuşak doyması ve A4-K83’ün OK/FAIL kuşak eşleştirmesi korunmuş. Yeni CRC tablosu `static const`; kaynak düzeyinde yeni platform bağımlılığı veya yazılabilir statik veri eklenmemiş.
- MSVC `/W4 /WX /analyze` başarısı bu incelemeyle doğrulanmış sayılmaz. Görevde belirtilen `sim/kopru/src/param_blogu.*` ve `sim/sil/src/depo.cpp` bu commit’in ağacında bulunmadığından okunamadı; bunların blok testlerinin değişmeden geçtiği doğrulanmadı.
- İskelet değişikliği gerektiren yeni bir karar ihtiyacı görmedim; bulgu 1 mevcut A4 gereklerinin uygulamada tamamlanmasını gerektiriyor.