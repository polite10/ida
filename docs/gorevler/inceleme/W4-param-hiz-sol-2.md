# sol incelemesi: param

## Hüküm

**Kapanmadı.** Yalnız `3cc6bed` ve `57521db` incelendi. Önceki #1 kısmen düzeldi; aşağıdaki üç açık sürüyor. #2–#4 kapandı. Protokol CRC değişikliği kaynak ve bağımsız hesap düzeyinde temiz.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `core/src/param/param_komut.c:197`, `:210` | **Kapanmadı: denetlenen özet yeniden okunuyor.** İstenen çağrı içi hata modelinde, 197’deki denetim geçtikten sonra, 210’daki okuma öncesinde yalnız `sema_ozeti ^= 1` olursa bozuk özetli blok üretilir ve `gen` tüketilir. Tümleyenin bozulması gerekmez. | Özeti ve tümleyenini yerel değerlere alın; bu değerleri doğrulayıp kodlayıcıya aynı doğrulanmış özeti geçirin. Araya tek-bit bozma karşı örneğini sınayın. | kritik |
| 2 | `core/src/param/param_komut.c:197`; `core/src/cerceve/cerceve.c:585`; `core/src/cerceve/cikti.c:198` | **Kapanmadı: SAVE’in saptadığı bozulma bildirilmez.** Çağrı başındaki bütünlük denetiminden sonra, SAVE denetiminden önce özet veya tümleyen bozulursa SAVE reddedilir; fakat önceden hesaplanmış `cg.core_ok` nedeniyle aynı çıktı `core_ok = 1` taşır. SAVE hata mandalını veya `CORE_INTEGRITY` kaydını güncellemez; A4-K89 ihlalidir. | SAVE’de saptanan bozulmayı ortak bütünlük mandalına ve aynı çağrının sağlık/günlük çıktısına taşıyın. | kritik |
| 3 | `core/src/param/param_komut.c:198`; `docs/sozlesmeler/A4-port.md:629`, `:784` | **Kapanmadı: kapalı tabloya yeni ret koşulu eklenmiş.** Geçerli blokla init sonrası `param_ok = 1` iken tümleyeni bozmak `PARAM_INVALID` üretir. §3.17 bu nedeni yalnız `param_ok = 0` ve kabul edilmiş commit yokluğuna bağlar; K124 tabloda olmayan koşulu yasaklar. | Bozuk blok üretimini engellemeyi koruyarak bütünlük hatasında SAVE sonucunu sözleşmede açıkça tanımlayın; “en yakın neden” mevcut sözleşmeye uygunluk sağlamaz. | kritik |

## Yeniden hesaplananlar

- **CRC temiz:** `0xEDB88320` ile 256 girdiyi yeniden ürettim; protokol ve çekirdek tablolarının **256/256 girdisi eşleşti**. Güncelleme [W3C örneğiyle](https://www.w3.org/TR/PNG-CRCAppendix.html) uyumlu; 10.000 bağımsız girdide `zlib.crc32` ile fark yok.
- Boş girdi `00000000`, `"123456789"` `CBF43926`; varsayılan 1296 baytlık bloğun A1-K68 **tam blok** `save_crc` değeri eski/yeni hesapta `335363E5`. Yük CRC’si ayrı olarak `B011EAB1`.
- Şema metni 2426 bayt; özet `34065414A77F8846`, tümleyen `CBF9ABEB588077B9`. Özetin düşük biti bozulunca yük CRC’si değişmez; bulgu 1’deki blok sonraki doğru init’te reddedilir.

## İskelete ya da diğer bölümlere notlar

- **Sıra/init temiz:** normal kurulum özeti blok çözümünden önce kuruyor; ürün SAVE yolu çağrı başındaki bütünlük denetiminden sonra çalışıyor. Önceden mevcut tek-alan bozulması yakalanıyor; reddedilen SAVE kuşak tüketmiyor. Yukarıdaki çağrı içi boşluklar sürüyor.
- **#2 kapandı:** referans kanonikleştirme/özet gövdesi ayrı; SHA-256 ilkeli ortak olsa da bağımsız altın değer denetimi var (`core/test/test_param_hiz.c:69`, `:366`).
- **#3 ve kodlayıcı temiz:** düzenekte gerçekten dört I32 kayıt var; uçlar ve 1000 rastgele durum aynı ürün kodlayıcısından geçiyor. Ürün sarmalayıcısının tablo, sayı ve bayt düzeni korunmuş (`param.c:318`; `test_param_hiz.c:387`).
- **#4 kapandı, kayıt düzeyinde:** tekrar sayıları ve önce/sonra en kötü süreler eklendi (`core/TASARIM.md:103`); ölçümleri yeniden yapmadım.
- **57521db bağımlılık/indis temiz:** `uint8_t` indis 0–255; [C6385](https://learn.microsoft.com/en-us/cpp/code-quality/c6385?view=msvc-170) açısından kaynakta sınır aşımı yok. Tablo `static const`; yeni çekirdek simge başvurusu yok. `ida_core` bağlantısı CRC test hedefine `PRIVATE`; ürün hedefi yalnız `ida_link`’e bağlı (`protocol/ida_link/test/oturum/CMakeLists.txt:35`; ürün `CMakeLists.txt:218`).
- Dosya değiştirilmedi. C testleri, arşiv simge denetimi, MSVC `/analyze` ve kart ölçümleri çalıştırılmadı; bunların başarısı doğrulanmış değildir.