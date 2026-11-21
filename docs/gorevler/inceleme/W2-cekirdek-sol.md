# sol incelemesi: .

## Hüküm

birleştirilemez

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `core/src/atis/atis.c:15`; `core/src/cerceve/cerceve.c:478`, `:518` | **Bütünlük denetimi bozulmanın kanıtını kaybediyor.** Kurulu örnekte `fc.durum = 255` yapılıp daraltıcısız olağan tick çağrıldığında, `ida_fc_daralt` değeri koşulsuz `SAFE` yapıyor. Sonraki `butunluk()` artık bozukluğu göremiyor: `core_ok = 1`, `CORE_INTEGRITY(3)` yok. Böylece A4-K89 için yazılmış FC durum denetimi etkisizleşiyor; platform bozulmayı görmeden gözetimi besleyebilir. | Zorunlu daraltmayı ve §3.11 sırasını koruyarak, üzerine yazılacak bozuk durumun tanılama bilgisini kaybetme. FC durum bozulmasını da kapsayan hata enjeksiyonu testi ekle. | kritik |
| 2 | `core/src/param/param_komut.c:238` | **Başarılı kayıt, durum kodu değişmediyse görülmüyor.** Karşı örnek: bloksuz init; `param_store = OK`, `param_gen = 41` taşıyan kabul edilmiş çağrıda boş `COMMIT` ve `SAVE` → kuşak 42 kabul edilir. Kayıt iki çağrı arasında tamamlanır; sonraki girdide `OK/42` vardır. Önceki durum da `OK` olduğundan sonuç işlenmez, `param_ok` sürekli 0 kalır. A4-K83, çekirdeğin aradaki `BUSY` durumunu mutlaka görmesini şart koşmuyor. | Bekleyen kaydın sonucunu durum kodu değişiminden bağımsız olarak, beklenen kuşakla eşleştir. Aynı durum koduyla gelen yeni kayıt tamamlanmasını test et; günlükte yinelemeyi ayrıca önle. | kritik |
| 3 | `core/src/mat/mat.c:50`, `:85` | **Birimleme yanlış sonucu başarı olarak bildiriyor.** `v = (1e20f, 0, 0)` ve `q = (1e20f, 0, 0, 0)` sonlu girdilerdir. Kare alma binary32’de taşar; norm `∞`, ters norm 0 olur. İşlevler sıfır vektör/kuaterniyon yazıp `true` döner. Kuaterniyon artık birim değildir. Mevcut testler yalnız küçük değerleri sınar. | Taşmaya dayanıklı ölçeklenmiş norm kullan; sonlu olmayan ara sonuçlarda başarılı dönüş verme. Büyük sonlu değerler ve sonsuzluk için sınır testleri ekle. Binary32 aralığı [Oracle’ın sayısal hesap belgesinden](https://docs.oracle.com/cd/E19059-01/stud.10/819-0499/ncg_math.html) denetlendi. | kritik |
| 4 | `tests/kabul/cekirdek/kabul_cekirdek.c:1150` | **Ortak kabul denetiminde tanımsız davranış var.** `ida_core_counters_t` bir yapıdır; `uint32_t[]` değildir. Yapıyı `uint32_t *` yapıp `yeni[i]`/`eski[i]` ile üyeler arasında dolaşmak geçerli dizi erişimi oluşturmaz. Bu erişim her olağan kabul denetiminde çalışıyor; ASan’ın geçmesi ISO C uygunluğunu kanıtlamaz. Aynı örüntü [SEI CERT ARR37-C](https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/arrays-arr/arr37-c/) içinde açıkça gösteriliyor. | Üyelere adlarıyla eriş veya mevcut `kc_fark` yardımcısındaki gibi `offsetof` ve bayt işaretçisi üzerinden `memcpy` kullan. Port yapısını değiştirme. | kritik |
| 5 | `tests/kabul/cekirdek/kabul_cekirdek.c:1154` | **Sayaç denetimi A4-K88’in izin verdiği sarmayı reddediyor.** Örneğin önceki `sample_rejected = 0xFFFFFFFE` iken dört yeni ret sonrası doğru değer 2’dir; `yeni >= eski` denetimi doğru uygulamayı başarısız sayar. | Beklenen artışları mod 2³² farklarıyla denetle; genel monotonluk koşulunu kaldır. Sarma sınırı için ayrı vaka ekle. | önemli |

Karşı örnekler kaynak üzerinde izlenmiştir; bu oturumda derlenip çalıştırılmamıştır.

## Yeniden hesaplananlar

| Hesap | Sonuç |
|---|---|
| NOT_INIT engel bitlerinin birleşimi | `0x00026FFF`; uygulamadaki sabitle aynı |
| Parametre bloğu | 9 kayıt: `24 + 9 × 8 = 96` bayt; 8000 baytta en çok 997 kayıt |
| Kanonik şemanın bağımsız SHA-256 hesabı | `0a5e614c86321f52602105d6cdbef7968fab0ae7969fc4ad1e23d17ad22dc9cd`; ilk sekiz baytın küçük uçlu değeri `0x521F32864C615E0A` |
| CRC-32 denetim girdisi `"123456789"` | `0xCBF43926`; test sabitiyle aynı |
| En büyük `uint32_t` ms → ns | `4 294 967 295 000 000`; `int64_t` içinde |
| IMU boşluğu ifadesinin mutlak üst sınırı | `2⁶² + (2³²−1) = 4 611 686 022 722 355 199`; `INT64_MAX` altında |
| Seri aritmetik | Fark 0: yineleme; `0x7FFFFFFF`: yeni; `0x80000000`: eski; `0xFFFFFFFF → 0`: fark 1 |
| Yapı yerleşimi | 8 baytlık hizalama kabulüyle modüllerden yeniden toplandığında `ida_core_t = 2160` bayt; tasarım notuyla aynı |
| Bayatlık bütçesi | `20 + 11 + 3,5 = 34,5 ms`; üst eşikle `44,5 ms`. Bunlar ölçüm değil bütçe hesaplarıdır |
| Kaynak sayımı | 79 birim testi, 137 kabul testi kaydı; dört gevşetme vakasında atlama yolu; 130 benzersiz kural satırı, eksik numara yok |

SHA-256 işlemleri [NIST FIPS 180-4](https://csrc.nist.gov/files/pubs/fips/180-4/final/docs/fips180-4.pdf) ile karşılaştırıldı.

## İskelete ya da diğer bölümlere notlar

Başvuru olarak `origin/sozlesme/w1-butunlesme` içindeki güncel A4 kullanıldı. İncelenen HEAD `8e6468b`, çalışma dalı `is/w2-cekirdek-kabul`; kapsam, istenen üç noktalı diff’tir. Sözleşme dosyaları bulgu kapsamına alınmadı.

**Emniyet yollarının sonucu:**

| Alan | Yazım ve erişim koşulu |
|---|---|
| `armed` | `emniyet.c:14` başlangıçta 0; `:185` daraltmada yalnız 0 yazar. Ürün kodunda 1 yazan yol yok |
| Kip | `emniyet.c:15` INIT; `:197` yalnız geçerli daraltıcı hedefe ve mevcut kip INIT değilken yazar. Port girdileriyle INIT’ten çıkaran yol yok |
| Etkin emniyet değerleri | `emniyet.c:154` yalnız kısıt bitlerini ekler; `:162` oturum sonunda tümünü kısıtlar. Gevşeten kanca boştur |
| FC durumu | `atis.c:9`, `:15`, `:42` yalnız SAFE yazar; ilk bulgu bu yazımın bozulmayı gizlemesidir |
| Olaylar | Ürün kodunda genişletici `aday[]` girdisi veya `n_aday` artışı yok. `kapat_cikar` yalnız `SAFETY_CLOSE` üretir |
| İtki | Kontrol yer tutucusu sekiz sıfır verir; `cikti.c:131` çıkış kapısını uygular |

Dolayısıyla **başarıyla kurulmuş örneğe verilen girdi dizileriyle** itki, silahlandırma veya genişletici olay üreten yol bulmadım. Hedef seçimi, hedefleme çözümü veya atış ölçütü uygulaması da bulunmadı.

§3.11 sırası `(a1) → (a2) → (b) → (b3) → (b2) → (c) → (d) → (e)` ile uyumlu. Daraltma ilk `min(n_samples,16)` ve `min(n_cmds,8)` girdisinde, ret ve `out == NULL` öncesinde uygulanıyor. `VALID = 0`, diğer geçerli `inst` değerleri ve `estop ≥ 2` kısıtlamayı kaybettirmiyor. Yazılabilir hata dönüşleri ortak çıktı kapısından geçiyor; A4-K28’in iki alanını atlayan dönüş bulmadım. NOT_INIT çıktısı tam sıfırlama ve sabit alan yazımlarıyla kuruluyor.

**Görev kabul maddeleri:**

| Madde | Durum | Kanıt / sınır |
|---|---|---|
| 1 — Onaylı tasarım ve bütün kurallar | karşılandı | Onay görev belgesinde kayıtlı; dağılım tablosunda 130 kural eksiksiz |
| 2 — Dört derleme, test ve simge denetimleri | kısmen | Denetim hedefleri mevcut; Windows başarısı verilen bilgi. Dört ön ayar ve simge sonuçları bu oturumda yeniden doğrulanmadı |
| 3 — Her tam kural için test ve eşleme | kısmen | Geniş test kapsamı var; yukarıdaki sınırlar eksik. İncelenen değişiklikte bütün tam kuralları test adlarına bağlayan rapor tablosu yok |
| 4 — Yer tutucu emniyet değişmezleri | karşılandı | Kaynakta genişleten yol yok; sabit tohumlu birim ve kabul dizileri var. Bağımsız ortak denetim hem üst düzey hem `status.armed` alanını ve genişletici sonuçlarını kontrol ediyor |
| 5 — Firmware ve örnek boyutları raporu | kısmen | `core/TASARIM.md:61` değerleri bildiriyor; 2160 bayt yerleşimi yeniden hesaplandı. Arşiv boyutları bağımsız doğrulanmadı |

**Doğrulanamayanlar:** Derleme, test ve ASan çalıştırılmadı; firmware metin boyutları ile yığın raporu yeniden ölçülmedi. Kart süre ölçümü kapsam dışındaki W3 işidir. WG14 N1570/N2176 PDF erişimleri zaman aşımıyla başarısız oldu; bunlar erişilmiş kaynak sayılmadı.

Tanımsız olay sonucu kodunun politikası A4’te açık değil: mevcut kod, bekleyen kimlikle gelen `result = 198` değerini `fc.last_event_result`’a taşıyor. Kabul testindeki B20 de yalnız gözlem yapıyor. Bunu kesin kod bulgusu saymadım; kapalı çıktı değer kümesiyle birlikte sözleşme incelemesinde netleştirilmeli.

Dosya değiştirilmedi ve kurulum yapılmadı; çalışma ağacı temiz kaldı.