# sol incelemesi: .

## Hüküm

birleştirilemez

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `tests/kabul/protokol/kabul_belirsiz.c:47`, `:195`, `:311`; `kabul_k06.c:51`; `kabul_ortak.c:455` | **Güncel sözleşmenin kesin kuralları bağımsız kabul kapısında doğrulanmıyor.** B1–B8 vakaları beklenen sonuç yerine bilgi yazıyor; “hiçbiri” sonucu bile testi başarısız yapmıyor. Bilinmeyen kimlik testi `rx_unknown_msgid + rx_crc_err >= 1` kabul ederek K06’nın yasakladığı sayımı geçiriyor. Referans model ayrıca aday dışı `0xFE` için `rx_resync_bytes` artırmıyor. Denetim sırası, başlık reddinden sonra yeniden tarama, çoklu/tekrarlanan `seq` ve reddedilen çerçevenin geçmişe alınmaması bağımsız olarak eksik sınanıyor. | Güncel A1-K02/K06/K07/K08’e göre tek beklenen davranışı iddia eden testlere dönüştürün. Her vakada teslimleri ve ilgili **bütün sayaçları** kesin karşılaştırın; referans modeli düzeltin. Uygulayıcının bazı birim testleri bu davranışları kapsıyor; bağımsız kabul testlerindeki boşluğu kapatmıyor. | kritik |
| 2 | `tests/kabul/protokol/ek_b_cikar.py:379`; `tests/kabul/protokol/kabul_ek_b.c:147`; `tests/kabul/protokol/kabul_ortak.h:79` | Üretilen `altin_yapi_*`, `uint8_t b[512]` tamponunu `mavlink_*_t *` yapıp alanları okuyor. Tamponun hizalı olması, bildirilen bayt dizisini bu yapının nesnesine dönüştürmez. Bu erişim **strict aliasing / effective type ihlalidir**; Ek B’nin tipli alan kabul testleri tanımsız davranış içeriyor. [Clang’ın birincil açıklaması](https://clang.llvm.org/docs/UsersManual.html#strict-aliasing) bu ayrımı ve güvenli kopyalama yolunu doğruluyor. | Üretilen okuyucuda gerçek bir `mavlink_*_t` yerel nesnesi oluşturup `memcpy` ile doldurun; alanları o nesneden okuyun. | kritik |
| 3 | `protocol/ida_link/src/tel.c:143`, `:333` | Rol **doğrulanmadan önce `uint8_t`’ye daraltılıyor**. Örneğin `(ida_link_rol_t)257` kart, `258` sim rolü olarak başarıyla kuruluyor. Açık başlığın geçersiz rolde `IDA_LINK_HATA_ROL` dönme sözü karşılanmıyor; mevcut test yalnız 0 ve 7’yi deniyor. | Özgün `ida_link_rol_t` değerini iki geçerli sabitle karşılaştırın; yalnız doğrulamadan sonra saklamak için daraltın. Her iki kurulum işlevinde 257/258 gibi karşı örnekleri sınayın. | önemli |

## Yeniden hesaplananlar

İncelenen HEAD: `0fa569e`. Başvuru sözleşmeleri `origin/sozlesme/w1-butunlesme` (`5b419a8`) üzerinden okundu.

- **Kanonik metin:** mesajları kimliğe, alanları temel tip boyuna göre kararlı biçimde, numaralandırmaları ASCII adına ve girdileri sayısal değerine göre yeniden sıraladım. Her satırın sonuna LF ekleyerek **37 M + 334 F + 194 E = 565 satır** elde ettim. SHA-256’nın ilk sekiz baytı `80 ff a5 84 89 78 39 39`; küçük uçlu değer **`0x3939788984A5FF80`**. Üreteç ve beklenen veri dosyasıyla aynı.
- **Mesaj tablosu:** 37 satırın adları ve boyları XML ile aynı. Yönler **22 yalnız sim → kart, 12 yalnız kart → sim, 3 iki yönlü**. Güncel sözleşmeden bellekte üretilen tablo, mevcut üretilmiş başlıkla aynı.
- **Ayna:** 21 grubun kapsamındaki **136 XML girdisi tam bir kez** eşlenmiş; 136 karşılığın sayısal değeri de `port.h` ile aynı. `IDAW_REASON` kapsamı 0–15.
- **Altın baytlar:** 38 çerçevenin tamamının uzunluğunu, başlığını, `CRC_EXTRA` değerini ve CRC’sini bağımsız bit düzeyi hesapla doğruladım. 37 farklı mesaj kapsanıyor; toplam **2.058 bayt**. CRC denetim girdisi `"123456789"` → **`0x6F91`**. Tel sırası, kırpma ve CRC kapsamı [MAVLink’in birincil belgesiyle](https://mavlink.io/en/guide/serialization.html) uyumlu.
- **Bozma denemeleri:** Dosya yazmadan, bellekte bir XML alan adını değiştirmek `[DIALECT_FARKI]`; Ek B’nin bir yük baytını değiştirmek `[ALTIN_FARKI]` üretti.
- **Sınırlar:** Güncel en büyük yük **246**, çerçeve **258 bayt**. `len = 255` başlıkta reddediliyor. Genel 255 bayt sınırında bile son CRC dizini 266; 267 baytlık tampon içinde. Yeniden tarama en çok dokuz bayt; parça halinde besleme bunu aşmıyor. Çıkış kapasitesi bütün çerçeve yazımlarından önce denetleniyor. Sıfır besleme ve taşma bildiriminin akışı kaynakta izlendi.
- **Mevcut firmware nesneleri:** `fw-debug` için text **1.540 B**, rodata **444 B**; `fw-release` için text **1.624 B**, rodata **444 B**; ikisinde data/bss **0**. Simgelerde kanal durumu veya bellek ayırma başvurusu yok; dış başvurular `memcpy`/`memset`. Alan yerleşimi hesabıyla alıcı **324 B**, teslim yapısı **264 B**, gönderici **2 B**.
- **Ölçüm girdisi:** 17 çerçevenin toplamı **1.190 B**; başlıktaki açıklamayla aynı. Buradan kart performansı sonucu çıkarmadım.

Bulgu 1 için somut ayırıcı girdiler:

- `seq = 1, 5, 6` için `rx_seq_gap` **1** olmalı; **3** olmamalı.
- Tek `fe` baytı için `rx_mav1 = 1` ve `rx_resync_bytes = 1` olmalı.
- Kart alıcısına `fd 01 00 00 02 07 19 ec b3 00 00 89 93` verildiğinde CRC doğrudur, kimlik çifti ve yön yanlıştır. Beklenen yalnız **`rx_wrong_sys = 1`**; yön denetimini öne alan uygulama mevcut “belirsiz” vakada başarısız sayılmıyor.

## İskelete ya da diğer bölümlere notlar

Tel katmanının kapsamı iskelet §7 ile uyumlu. Ürün alıcısında K02–K08’in belirtilen denetim sırasına, başlık reddindeki yeniden taramaya, CRC hatasında adayın tüketilmesine ve yalnız kabul edilen çerçevenin `seq` geçmişini güncellemesine aykırılık bulmadım. Kitaplık kaynaklarında değişebilir genel durum görmedim.

Kabul yardımcılarının kendi CRC’si ve çerçeve kurucusu bulunuyor; uygulama üretecini veya altın JSON’unu kullanmıyorlar. Kaynak düzeyindeki bağımsızlık korunmuş. Hedef seçimi, hedefleme çözümü veya atış ölçütü uygulayan kod görmedim.

**Görev belgesinin kabul maddeleri:**

| Madde | Durum | Kanıt |
|---|---|---|
| 1 — Dört ön ayar, host testleri, ayırma yasağı | kısmen | Derleme ayarları uygun; mevcut host-debug ve firmware kayıtları var. Nesne simgelerini okudum. Güncel `host-asan` protokol koşusu bağımsız doğrulanamadı. |
| 2 — 38 altın çerçeve ve Ek B bozma denemesi | kısmen | Bayt/CRC hesabı ve bellekte bozma denemesi doğrulandı; mevcut birim testi kaydı başarılı. Tipli bağımsız doğrulamada bulgu 2 var. |
| 3 — `dialect_hash` ve alan adı bozma denemesi | karşılandı | Bağımsız hash/satır hesabı ve bellekte alan adı değiştirme denemesi aynı sonucu verdi. |
| 4 — C17/C++20 aynalar ve bozuk değer denemesi | karşılandı | 136 değer bağımsız karşılaştırıldı; iki derleme birimi ve mevcut bozuk değer denemesinin başarısız derleme kaydı var. |
| 5 — Her kural için test ve rapor eşlemesi | kısmen | Birim testleri temel kuralları kapsıyor; bağımsız kabul kapsamı bulgu 1 nedeniyle eksik. İstenen sonuç raporu sunulmadı. |
| 6 — Genel durum yokluğu ve iki örnek testi | karşılandı | Kaynak ve mevcut arşiv simgeleri uygun; iki örnek testlerinin başarılı mevcut kayıtları var. |
| 7 — Boyut ve koşulları belirtilmiş host ölçüm raporu | kısmen | Ölçüm programı ve mevcut çıktılar var; bütün ölçüm koşullarını birleştiren görev raporuna erişemedim. |

Altın karşılaştırıcısı tek başına boş Ek B ile boş JSON listesini kabul edebiliyor; ayrıca tablo ayrıştırıcısı yinelenen kimliği üzerine yazıyor. Bugünkü verilerde bu durum yok. Boş altın kümesini bütün kabul akışında ayrıca bulunan 38 çerçeve denetimi yakalıyor.

**Doğrulanamayanlar:** Dosya değiştirmedim, kurulum veya yeniden derleme/CTest çalıştırmadım. Mevcut derleme kayıtlarını yeni koşu saymadım. Yerel `host-asan` kaydı protokolden önceki çekirdek testinde başarısız bitiyor; protokolün sanitizer sonucu için kanıt oluşturmuyor. Windows CI’nin yeşil olduğu kullanıcı beyanıdır; bu turda CI kaydı bağımsız incelenmedi. Test yazarının geçmişte hangi dosyaları okuduğu kaynaklardan doğrulanamaz. WG14 N1570/N2176 PDF’lerine erişilemedi; tür erişimi değerlendirmesi erişilebilen Clang birincil belgesine dayanıyor.