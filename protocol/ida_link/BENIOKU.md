# protocol/ida_link — A1 tel katmanı

`ida_link` modülünün alt katmanı (görev: `docs/gorevler/W2-protokol.md`; sözleşme: `docs/sozlesmeler/A1-kart-baglantisi.md`). Bayt akışından doğrulanmış MAVLink 2 çerçevesi çıkaran alıcı, tipli mesaj yapısından çerçeve yazan gönderici ve alıcı sayaçları. A1 ↔ A4 dönüştürücüsü (oturum, parti, alan eşlemesi) sonraki işlerde bu katmanın üzerine eklenir.

- Açık arayüz: `include/ida_link/tel.h` (her işlevin önkoşulu, dönüşü ve hata halinde ne yazıldığı başlıktadır). Uygulama: `src/tel.c`. ISO C17, bellek ayırma, işletim sistemi, saat, dosya ve stdio yok; bütün durum çağıranın örnek yapısındadır (A1-K101). Başlık C++20'den de içerilir.
- Üretim (derleme sırasında, `araclar/ida_link_arac.py`, yalnız Python standart kitaplığı): `protocol/ida.xml` ve A1 Tablo 3.3'ten mesaj tablosu (yön dahil) ve `ida_link/dialect.h` (`IDA_LINK_DIALECT_HASH`, A1 §3.2); `protocol/ayna.json`'dan A1-K03 eşitlik başlığı; `veri/altin.json`'dan altın test verisi. Üretilenler derleme dizinindedir, depoya girmez.
- Veri: `veri/dialect_beklenen.json` (sözleşmedeki `dialect_hash` ve satır sayıları; XML bilerek değişince sözleşmeyle birlikte güncellenir), `veri/altin.json` (A1 Ek B'den `altin-cikar` ile çıkarılır, elle değiştirilmez; her derlemede Ek B ile karşılaştırılır).
- Denetimler (CTest, `protokol.*`): `dialect.beklenen`, `altin.ek_b_esit`, `bellek.ida_link`, `simge.ida_link` (yazılabilir veri, kanal simgesi ve izinsiz dış başvuru yok) ve her birinin bilerek bozulmuş girdiyle denemesi (`dialect.deneme_alan_adi`, `altin.deneme_ek_b_bayt`, `ayna.deneme_port_degeri`, `simge.deneme_durumlu`). Ayna eşitlikleri ayrıca `ida_link_ayna_denetimi` hedefinin derlenmesiyle (ALL) denetlenir.
- Birim testleri: `test/test_tel.c` (Unity). Bağımsız kabul testleri `tests/kabul/protokol/` altında ayrı iştir.
- Ölçüm: `olcum/` — çekirdek döngü (`olcum_cekirdek.c`, kartta da derlenir) ve PC sürücüsü `ida_link_olcum [yineleme]`. PC sonuçları kart maliyeti yerine kullanılmaz (A1 §8.1 O2).

## Hız ve besleme biçimi (W3-tel-hiz)

Tel katmanı W0-MP5a ölçümünün (`docs/olcumler/MP5-tel-maliyeti.md` §9; `olcum/mp5-tel-maliyeti` dalı) iyileştirmeleriyle yeniden yazıldı; açık arayüz ve davranış değişmedi. `src/tel.c` şunları içerir (MP5 §9.6'daki madde numaralarıyla): (2) sözcük genişliğinde yerel kopya ve sıfırlama (kartın newlib-nano `memcpy`/`memset`'i bayt döngüsüdür); (3) başlık geçtikten sonra yük ve sağlama baytlarının blok hâlinde işlenmesi, CRC'nin tek geçişte hesaplanması; (4) 256 girdili CRC tablosu (`src/tel_crc_tablo.h`); (5) kodlayıcıda sözcük sözcük kırpma taraması, yük kopyası ve CRC'nin aynı geçişte yapılması; (6) kimlikten tablo satırına doğrudan dizin, `rx_bytes`'ın çağrı başına bir kez toplanması, `ida_link_coz`'un yapıyı bir kez yazması. Madde 1 (teslim edilen yükün `[yuk_azami, 255)` kısmını sıfırlamamak) alınmadı: `tel.h`'deki "`yuk[len, IDA_LINK_YUK_AZAMI)` sıfırdır" sözünü değiştirirdi.

**Besleme biçimi.** Birincil yol, alıcıya bayt **bloğu** vermektir: `ida_link_al(a, veri, n, ...)` çağrısında `n` eldeki baytların tamamı (ör. DMA halkasının o ana kadar dolan kısmı) ya da en az birkaç on bayttır. Bayt bayt besleme (`n = 1`; ör. her UART kesmesinde bir bayt) desteklenir ve aynı sonucu verir, ama **kartta kullanılmamalıdır**: her `ida_link_al` çağrısının sabit maliyeti bayt başına ödenir. Kartta ölçülen (MP5 §9.2–§9.3; `fw-release`, 160 MHz, ICACHE açık, en kötü değer; ölçülen kod bu dosyanın değil prototip kopyasının `wbt256eh` çeşididir, ürüne taşınmış hâlinin kartta ölçümü W3-kart işinin ikinci dilimindedir):

| 1 050 B'lık akışın ayrıştırılması | Süre | Eşik (E1) |
|---|---:|---:|
| tek seferde | 0,1841 ms | ≤ 0,20 ms |
| 64 B'lık parçalarla | 0,2020 ms | ≤ 0,20 ms |
| bayt bayt | 0,8478 ms | ≤ 0,20 ms |

Bayt bayt beslemede süre tek seferdekinin ≈ 4,6 katıdır; E2 (ayrıştırma + kodlama ≤ 0,50 ms) de bayt bayt sağlanmaz. 64 B'lık parçalar E1 sınırındadır (%1 üstü); bu yüzden alım yolu baytları elde biriktiği kadar, mümkünse 64 B'tan büyük bloklarla vermelidir. Parça boyu davranışı değiştirmez (fark testi her bölünmede aynı sonucu gösterir).

**CRC tablosu seçimi (flash ve hız).** 256 girdili tablo 512 B salt okunur veri karşılığında ayrıştırmada bayt başına ≈ 6 çevrim kazandırır (MP5 §9.2: 18,0 → 12,0 çevrim/B; E1 0,2038 → 0,1661 ms). 16 girdili yarım bayt tablosu (32 B) bu çekirdekte bit işlemli CRC'den de yavaştır (0,2149 ms > 0,2038 ms). Kartın 4 MB flash'ında 512 B önemsizdir; hız seçildi.

**Boy (`fw-release`, arm-none-eabi-gcc 14.3.1, `libida_link.a`; `arm-none-eabi-size -A`):** kod 1 624 B → 2 240 B (+616 B), salt okunur veri 444 B → 1 056 B (+612 B: CRC tablosu 512 B, kimlik dizini 100 B); yazılabilir veri 0 B; örnek yapılarının boyu değişmedi.

**Eşitlik ve ölçüm.** `olcum/tel_eski.c` değişiklik öncesi `tel.c`'nin değiştirilmemiş kopyasıdır (açık işlevleri `ida_link_eski_` önekli; ürüne bağlanmaz). `olcum/tel_fark.c` (CTest `protokol.fark.tel`) iki uygulamayı altın çerçevelerde, sabit tohumlu rastgele ve bozulmuş akışlarda, çok sayıda blok boyu bölünmesiyle karşılaştırır (dönüş, tüketilen bayt, teslim edilen çerçevenin bütün baytları, sayaçlar, iç durum; kodlayıcı, çözme ve bilgi işlevleri; CRC tablosunun 65 536 × 256 çiftte `crc_accumulate`'le eşitliği). `ida_link_olcum` her ölçümü iki uygulamayla, aynı programda sırayla koşar ve özetlerinin aynı olduğunu denetler; PC'deki oranlar kartınkini göstermez (PC'nin `memcpy`'si ve bit işlemli CRC'si kartınkinden farklı davranır).
