# A8 — sol incelemesine yanıt (düzeltme turu 1)

İnceleme: `docs/sozlesmeler/inceleme/A8-sol.md`. Düzeltme, mimarın her madde için verdiği kararlarla yapıldı. Güncellenen dosyalar: `docs/sozlesmeler/A8-kamera-algilama.md` (1.0-aday, düzeltme turu 1) ve `core/include/ida/algilama_kaydi.h`.

Kurallar yeniden numaralandı (A8-K01…A8-K69); aşağıdaki numaralar yeni sürümündür.

## Maddeler

| # | Durum | Yer | Gerekçe |
|---|---|---|---|
| 1 | giderildi | K35; §1; `WELCOME.camera_count`; neden kodu 14 `CAMERA_COUNT`; 3.6 | Bu sürümde `camera_count` = 1 zorunlu. Bileşen başka bir değer alırsa `BYE(CAMERA_COUNT)` gönderir ve yeniden bağlanmaz (K43). Senaryo birden çok kamera tanımlarsa sim bileşeni başlatmaz ve A3'e hata bildirir. Bütçe tek kameraya göredir |
| 2 | giderildi | K46; K63 | Sunulan karelerin `kare_seq`'i bir oturumda kesin artar. Yeni bir kare seçilince geride kalan ve hiç sunulmamış kareler kalıcı olarak atlanır, bir daha aday olamaz ve her biri `SKIPPED_BUSY` alır |
| 3 | giderildi | K19 tablosu; 3.2 "Ret nedeni" sütunu | Kaydın `calib_ver`, `source`, `inst`, `producer_id`, `producer_ver`, `batch_seq`, `batch_count`, `batch_index` ve zaman alanlarının hangi başlık alanına eşit olacağı ve uyuşmazlığın neden kodu tabloyla verildi |
| 4 | giderildi | K27 tablosu; K28; 3.4 durum tablosu; §5 "Geçersiz çıktı" ve "Kalibrasyon uyuşmazlığı"; başlıkta `IDA_DET_RJ_*` | Doğrulama, sırası öncelik olan tek bir 10 satırlık tabloyla tanımlandı. Her hatanın sonucu bütün partinin reddidir; kayıt çıkarma kaldırıldı. Her satırın tek bir neden kodu var; birden çok hata varsa önce gelen satır bildirilir. Kalibrasyon uyuşmazlığının tek neden kodu `CALIB`, karta giden tek durumu `CALIB_MISMATCH`. Diğer bütün retler karta `COMPONENT_ERROR` olarak gider |
| 5 | giderildi | K05; K28 ("Sim'in doğruladıkları"); K29 ("Kartın doğruladıkları"); K27 tablosunun "Sim" ve "Kart" sütunları | `HELLO` yetenek denetimi ve bekleyen kareyle eşlik (`ECHO`) yalnız sim'indir. Kart, yalnız karta ulaşan alanlarla ve kendi parametreleriyle sınanabilen satırları aynı sırayla uygular |
| 6 | giderildi | 3.3; K30; başlıkta `ida_det_batch_t` | `n_rejected` kaldırıldı; `producer_id` 26. konuma alındı, ayrılmış alan 27–31 (`reserved[5]`, 0 olmak zorunda). Ret sayaçları partide taşınmaz: sim akış başına, neden ve durum başına 64 bit sayaç tutar, A3'e bildirir ve A6'ya yazar; kart neden başına sayar. Taşan sayaç kalmadı |
| 7 | giderildi | K36 (geçerlilik tablosu), K37, neden kodları 15 `CONFIG_INVALID`, 16 `READY_INVALID` | Tanımlanan koşullar: odak uzaklığı sonlu ve > 0; ana nokta görüntü alanında; çözünürlük 1…640 × 1…480; hız 1…30 000 mHz; montaj konumu sonlu ve ≤ 10 m; kuaterniyon normu toleransı 1·10⁻⁵ (binary64'te, sabit sırayla); bozulma ve biçim A2 kodlarıyla. Bileşen geçersiz yapılandırmada `BYE(CONFIG_INVALID)` gönderir ve yeniden bağlanmaz; sim geçersiz yapılandırma göndermez. `READY` için `config_ver_ack` eşitliği, `ring_mapped` = 1 ve `reserved` = 0 aranır; her başarısızlığın kapanış nedeni var. El sıkışma yalnız geçerli `READY` ile tamamlanır |
| 8 | giderildi | 3.2 (`chol[6]` ve geçerlilik tablosu); K03, K07, K09; §7 ortak örnek tablosu (E1–E6); başlıkta `chol`, `IDA_DET_L_*` | Kovaryans yerine alt üçgen Cholesky çarpanı `L` taşınır: `[L_rr, L_ar, L_aa, L_er, L_ea, L_ee]`, kovaryans `L·Lᵀ`. Birim satırın bileşenininkidir: `r` satırı m, `az` ve `el` satırları rad. Geçerlilik yalnız karşılaştırmadır: bütün elemanlar sonlu; mutlak değer ≤ 1000 m (r satırı) ve ≤ 10 rad (açı satırları); ölçülen köşegen ≥ 1·10⁻³ m ya da ≥ 1·10⁻⁶ rad; ölçülmeyen bileşenin satırı ve sütunu tam 0. Eleman sayısı aynı kaldığı için yapı boyutu değişmedi (80 B) |
| 9 | giderildi | K11, K13, K66; 3.3 | Başlığın zamanı, işlenen girdi biriminin iskelet §3.1'e göre ölçüm zamanıdır ve kayıtlardan türetilmez; boş ve dolu partide aynıdır. Kamerada karenin pozlama sonu ve pozlama süresi, sonarda tarama satırının gönderim anı (`t_tx`) ve 0'dır. Kamera kaydı başlığın zamanına bit düzeyinde eşittir. Sonar kaydı `başlık − 30 s … başlık + 1 s` aralığındadır (tahmin). Sonarda tarama satırı başına en çok bir parti üretilir; bu kural eski "10 parti/s" sınırının yerini aldı |

## Madde 8: karşı örneğin yeni gösterimde sonucu

sol'un karşı örneğindeki kovaryans binary32'de şudur: `c_aa` = 1, `c_ae` = 0,10000000149011612, `c_ee` = 0,010000000707805157 (B2). Bu kovaryansın çarpanını üretici kendi hesabıyla bulur ve gönderir. binary64 ile `L_aa` = 1, `L_ea` = 0,10000000149011612, ikinci pivot ≈ 4,0978193·10⁻¹⁰, `L_ee` = 2,0243071175·10⁻⁵ çıkar; binary32'ye yuvarlanınca `L_ee` = 2,0243071048753336·10⁻⁵ olur (hesap).

Alıcılar yalnız gelen bitleri sınırlarla karşılaştırır. Bütün değerler sonlu ve sınırların içinde, köşegenler ≥ 1·10⁻⁶: iki taraf da kabul eder. Bu, örnekler tablosundaki E1 satırıdır. Karar bir aritmetik işleme dayanmadığı için FMA'ya, işlem sırasına ve platforma bağlı değildir.

Aynı kovaryansın çarpanını binary32 ayrı çarpma ve çıkarmayla hesaplayan bir üretici ikinci pivotu 0 bulur (sol'un gözlemi) ve `L_ee` = 0 gönderir. Bu durumda da iki taraf aynı biçimde reddeder (E2). Yani gösterim, kararın taraflar arasında aynı olmasını sağlar; kötü koşullu bir kovaryansın üreticide nasıl çarpanlanacağı üreticinin sayısal işidir.

Yeniden kurulan `c_ee` (`L_ea² + L_ee²`), binary32'de ayrı işlemlerle de tek işlemli çarpma-toplamayla da 0,010000000707805157 çıktı (hesap). Kabul kararı buna dayanmaz.

**Gösterimde gördüğüm sorun:** yok. İki not:

- Ölçülmeyen bileşen için "satır ve sütun 0" kuralı, `L`'nin ölçülen bileşenlere ait alt bloğunun alt kovaryansın Cholesky çarpanı olmasını korur. Bu iki ölçülmeyen yer için de geçerlidir: ilk bileşen (B2'de `r`) ve son bileşen (RB2'de `el`).
- Pozitif tanımlılık, köşegen > 0 koşulundan tanım gereği çıkar. Kart kovaryansı kurarsa yuvarlama, sınıra yakın durumda kurulan matrisi sayısal olarak tekil gösterebilir; K09 bu durumda kaydın reddedilmesini yasaklar. Hesap yöntemi tüketicinin seçimidir.

## Mimarın kararlarıyla yapılan diğer değişiklikler

- **SIL'de bekleme:** K60'a "bu sürümde kilitli adım kipi yok" eklendi; yeni K61 ve §6.3 sınırı yazıldı. Tekrarlanabilir sınama A6'nın girdi tekrarı düzeyiyle yapılır; bu, "A6'dan beklenen 3" maddesi olarak eklendi.
- **Partide en çok 4 kayıt:** değişmedi (tahmin etiketiyle). Başlık 27 B'ye indiği için bütçe hesabı yenilendi: en kötü durumda 11 970 B/s, tepe 399 B.
- **Kamera kademe değerleri:** A8'deki sayısal tablo çıkarıldı. Değerler A2'dedir (K68; "A2'den beklenen 6"). A8'de yalnız 640 × 480 ve 30 Hz üst sınırı ile bileşenin el sıkışmada bildirilen çözünürlük, biçim ve hıza uyma kuralı (K51) kaldı.
- **`VALID` bayrağı:** kayıt çıkarma kalktığı için `VALID` = 0 kayıt artık partinin reddidir (K21).
- **`SYNTHETIC` bayrağı:** bu sürümde `SYNTHETIC` = 1 kayıt hem sim'de hem kartta reddedilir (K22).

## Derleme

`core/include/ida/algilama_kaydi.h` şu üç derlemede hatasız ve uyarısız:

- `clang -std=c17 -pedantic-errors -Wall -Wextra -fsyntax-only` (Apple clang 17)
- `clang++ -std=c++20 -pedantic-errors -Wall -Wextra -fsyntax-only`
- `arm-none-eabi-gcc 14.3.1 -std=c17 -pedantic-errors -Wall -Wextra -mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -fsyntax-only`

Yapı boyutları değişmedi: kayıt 80 B (kodlanmış 78 B), parti başlığı 32 B (kodlanmış 27 B; önce 28 B idi).

## İkinci tur

İnceleme: `docs/sozlesmeler/inceleme/A8-sol-2.md`, "Yeni bulgular". Yalnız bu dört bulgu, mimar kararlarıyla düzeltildi. Kural sayısı değişmedi (69). Başlık dosyası değişmedi.

| # | Durum | Yer | Gerekçe |
|---|---|---|---|
| 1 | giderildi | K28 (son cümle ve karar tablosu) | Bileşenin bildirdiği durum, OK dahil, karta yalnız yanıt sim için uygulanabilir bütün satırlardan geçtiyse aktarılır; bir satır başarısızsa yalnız ilk başarısız satırın sonucu geçerlidir. Kayıtsız partide kayıt denetimleri boş kümeye uygulanır ve geçer. Karar tablosu bütün durum × denetim birleşimlerini kapsar: OK; bileşenin üretebildiği dört OK dışı durum; bu durumların `count` > 0 hali; OK ile `count` > 4; yalnız sim'in üretebildiği dört durum; 8'den büyük değer. sol'un örneği (FRAME_INVALID, `count` 0, yanlış `calib_ver`) tabloda `CALIB_MISMATCH` verir. Tablo ile metin arasında başka çift sonuç yok. `M_k`'dan sonra gelen yanıt doğrulanmadan `LATE`'tir |
| 2 | giderildi | K27 `TIME` satırı; §3.3 satır 2 | Başlığa ilişkin bütün zaman koşulları `TIME` satırına açıkça yazıldı: `t_meas_ns` ≥ 0; kamerada `window_ns` ≤ 1 s; sonarda `window_ns` = 0; `NO_FRAME`'de `window_ns` = 0; `NO_FRAME` dışında K17. Kayıt için `t_meas_ns` ≥ 0 eklendi. Alıcıda sınanamayan iki eşitlik, satırda adıyla "üretici kuralı, doğrulamaya girmez" diye işaretlendi: K11'in sonar `t_tx` eşitliği ve K64'ün kapanış anı eşitliği. K11'in kamera eşitliği `ECHO` satırına bağlandı. sol'un örneği (boş sonar partisinde `window_ns` = 1) artık `TIME` ile reddedilir |
| 3 | giderildi | K46, K63 | K46 yalnız açık (nihai durumu atanmamış, `M_k`'sı gelmemiş) kareleri "sunulmayacak" yapar ve onlara durum atamaz. Her karenin nihai durumu tek yerde, K63'te, `M_k` anında ve bir kez, tek öncelik sırasıyla atanır ve sonradan değişmez. Bu yüzden hazır olup bileşen kopmuşken atlanan kare `COMPONENT_DOWN`, bileşen bağlıyken atlanan kare `SKIPPED_BUSY` alır; kapanmış kareye dokunulmaz |
| 4 | giderildi | K39, K42 (üç sınıf ve örnek tablosu), K27 `STRUCT` satırı | Hatalar üç sınıfa ayrıldı ve sırayla değerlendirilir. (a) Çerçeve hatası: tür boyut aralığının dışı, bozuk başlık ya da `length` > 4096; sonucu bağlantının kapanmasıdır. 1.0'da sabit boyutlu türlerde aralık tam o boyuttur, `DETECTION_BATCH`'te 40 ≤ `length` ≤ 360'tır. (b) Çerçeve geçerli ama `length` ≠ 40 + 80 × `count`: sonucu `STRUCT` ile bütün partinin reddidir, bağlantı açık kalır. (c) Minor uzantısı: fazla baytlar yalnız anlaşılan minor sürüm o tür için uzantı tanımlıyorsa ve fazlalık tam o boyuttaysa yok sayılır. 1.0'da hiçbir tür uzantı tanımlamaz; bu yüzden fazlalık (a) ya da (b) olur. sol'un örneği (`count` 0, yük 41 B) tabloda (b) çıkar: `STRUCT` reddi, bağlantı açık |
