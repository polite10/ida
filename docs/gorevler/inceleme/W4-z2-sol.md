# sol incelemesi: app

## Hüküm

**Kabul edilmemeli:** acil durdurma denemesi hiç gerçekleşmeden program “GECTI” dönebiliyor. CRC dönüşümünde hesap veya eski blok uyumluluğunu bozan bir hata bulunmadı.

İnceleme yalnız `638cf70..3fa4e9b` arasındaki beş commitin farkına yöneltildi; `firmware/app` bütünü bağlam için okundu. Dosya değiştirilmedi; derleme ve kart denemesi yapılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `tests/z2/ida_z2.cpp:259`, `:275` | Kaynak akışından karşı örnek: `sil --sure 10 --acil 12 --acil-kalk 18`. Acil durdurma gerçekleşmez; etkin adım ve satır sayıları sıfırken ihlaller de sıfırdır. Z1 denetimleri atlanır ve sonuç geçebilir. | Olayın koşu içinde gerçekleşmesini, pozitif etkin adım/satır sayısını ve basılmadan önce etkin itki bulunmasını zorunlu tut. | kritik |
| 2 | `tests/z2/ida_z2.cpp:254`, `:320`, `:470` | `calistirma_gecersiz` yalnız yazdırılıyor; genel “GECTI” ve çıkış kodunu etkilemiyor. Rapordaki `0x67` koşuları da sıfır kalan üretebilir. Bu sonuç zamanlama kabulüyle karışabilir. | İşlevsel sonucu ve A1-K79 zamanlama sonucunu açıkça ayır; zamanlama kabulünü geçersizlik sıfırken ver. | önemli |
| 3 | `firmware/app/main.c:103`, `:334`, `:748`, `:784`, `:1008`; `firmware/app/olcum.h:44` | Yeni alanlar `IDA_OLCUM` ile korunmuyor. Normal sürümde ek sayaçlar, DWT okumaları, 64 bit toplama ve yayın işlemleri çalışıyor; ölçüm dizisi 40 B büyüyor. İstenen sıfır etki sağlanmıyor. | Yeni ölçüm durumunu, işlemlerini ve alanlarını derleme koşuluyla sınırla; okuyucunun düzenini buna uygun tut. | önemli |
| 4 | `firmware/test/test_kalici.c:243`, `:437`; `docs/olcumler/W4-z2.md:581` | Test eski imajın bloğunu okumuyor; yazma ve doğrulama aynı güncel CRC’yi kullanıyor. İki denetim vektörü bütün tabloyu sınamıyor. Rapordaki `9258f73` de `:541–543` uyarınca zaten tablolu CRC içeriyor. | Önceki bit hesabıyla üretilmiş sabit sayaç/parametre bloğu ekle; tabloyu bağımsız referansla karşılaştır. Rapordaki uyumluluk kanıtının imajını düzelt. | önemli |
| 5 | `tests/z2/ida_z2.cpp:231` | Sayılar tüketilen metin, aralık ve seçenek bayrakları denetlenmeden okunuyor. Örneğin `--acil -1`, `acil_verildi` kurulmadığından sunucuda “seçenek verilmedi” sayılıp denemeyi kapatıyor; `--sure xyz` sıfır oluyor. | `ida_sim` önyüzündeki tam ayrıştırma ve seçenek doğrulamasını uygula; açıkça verilmiş geçersiz değeri reddet. | önemli |
| 6 | `docs/olcumler/W4-z2.md:84` | “HIL 2 ve 3, SIL ile dördüncü ondalığa kadar aynı” ifadesi tabloyla uyuşmuyor: `:80` derinlik kestirim hatası SIL’de `0,0142`, HIL 2’de `0,0144`. | Eşitlik ifadesini geçerli ölçülerle sınırla veya bu farkı belirt. | küçük |

## Yeniden hesaplananlar

- **CRC:** 256 girdinin tamamı yeniden üretildi; uyuşmazlık yok. İlk sekiz ve son dört girdi:

  ```text
  00000000 77073096 EE0E612C 990951BA 076DC419 706AF48F E963A535 9E6495A3
  B40BBE37 C30C8EA1 5A05DF1B 2D02EF8D
  ```

  Çokterim `0x04C11DB7`, yansıtılmış karşılığı `0xEDB88320`; başlangıç ve son XOR `FFFFFFFF`, yansıtma korunuyor. `"123456789"` → `CBF43926`, boş girdi → `00000000`. Bayt güncellemesinin eski sekiz bit adımıyla eşitliği bütün girdiler için korunuyor; blok biçimi değişmedi. Tanım [zlib’in birincil kaynağıyla](https://raw.githubusercontent.com/madler/zlib/develop/crc32.c) uyumlu.

- Tablo `static const uint32_t[256]`; bağlayıcı betiği `.rodata*` alanını FLASH’a yerleştiriyor. Dizin `0..255`; işaret genişlemesi yok.
- Test açığına somut karşı örnek: yalnız `tablo[0] ^= 1` yapılırsa iki yeni denetim vektörü hâlâ geçer; tek `FF` baytının CRC’si `FF000000` yerine `FF000001` olur.
- Histogram toplamları: step **18 000**, S_kart **18 000**, tel **18 200**; üç TF histogramı ayrı ayrı **17 999** — 18 000 zaman damgasının fark sayısıyla uyumlu.
- Yığın **%19,751**, step bütçe kullanımı **%32,029**, S_kart periyot kullanımı **%27,74**. `765−176−129 = 460 µs`; gecikme payları ve **4 800 fizik adımı / 1 200 kontrol satırı** hesapları tutarlı.
- W4-K20 denetimi boş arıza verisiyle geçmiyor: her arızalı kanal için en az 200 etkin örnek aranıyor. Kişisel makine yolu veya seri numarası bulunmadı.

## İskelete ya da diğer bölümlere notlar

Yeni sayaçlar ana bağlamda işleniyor; farkta kesme içinde uzun işlem, paylaşılan sayaç yarışı veya tampon taşması bulunmadı. Seri bağlantı ve beklemeler mevcut sunucuya devrediliyor; farkta yeni sınırsız bekleme bulunmadı.

Rapor ölçüm/tahmin ayrımını yapıyor; ancak W4-K20 öncesindeki koşular güncel fizik için yeniden ölçüm kanıtı değildir. Ham kayıtlar bulunmadığından yüzdelikler yeniden hesaplanamadı. UM2967 ve CubeProgrammer CLI tam metinlerine erişim başarısız oldu; ilgili donanım/API ayrıntıları doğrulanmış sayılmadı.