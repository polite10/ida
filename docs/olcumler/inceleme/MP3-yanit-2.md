# MP3a — sol ikinci incelemesine yanıt (düzeltme turu 2)

| | |
|---|---|
| İnceleme | `docs/olcumler/inceleme/MP3-sol-2.md` (Y1–Y4, dördü kritik) |
| Görev | `docs/gorevler/W0-MP3a-duzeltme-2.md` |
| Uygulayan | `ida-uygulayici` (Opus) |
| Tarih | 2026-11-19 |

Dört bulguyu da haklı buldum ve giderdim. Bu ikinci ve son olağan düzeltme turudur; üçüncü tura girilmedi.

**Kanıt özeti:**
- Son derleme `$IDA_CACHE/mp3/derle-20261119-005548/` (kimlik `20261119-005548-mp3-43f9c836`, `mp3.elf` SHA-256 `43f48ae2b89f9983b1ad59676ae21cd638f67b08aaca31d642e6305a1c345b13`, `mp3_vcp` SHA-256 `477a92ee8c8ae107e8fd2fef6825d002866399f0c0f7e36efdaf9a8f0bd40598`).
- `derle.sh` içinde masaüstü testleri **219 denetim, 0 başarısız**. UBSan derlemesi 219/0, çalışma zamanı hatası yok.
- İlk turun on bir bulgusunun testleri aynen koşuyor; gerileme yok.
- Kartta kısa ön tarama yenilendi (§5).

## Y1 — DMA TC/yeniden yükleme dönemi (kritik) — giderildi; muhasebe sadeleştirildi

**Sorun:** Eski işlev hizmet edilmiş HT/TC sayısını (yarım-tur) BNDT ile birleştiriyordu. TC hizmet görüp düğüm yeniden yüklenmeden okununca (`yarim=2, BNDT=0`) konum 2N çıkıyor, tekdüzelik zorlaması da hatayı saklıyordu.

**Değişiklik (`ortak/mp3_halka.{h,c}`, `fw/platform.c`):** Konum artık kesme sayımı kullanmıyor; HT/TC kesmeleri kapatıldı, yalnız DMA hata kesmeleri açık.

- **Kararlı görüntü:** PRIMASK altında BNDT, FIFOL, BNDT ve zaman okunur. İki BNDT farklıysa okuma sırasında kaynaktan bayt alınmış ya da düğüm yüklenmiştir: görüntü KARARSIZ, hiçbir şey değişmez, veri tüketilmez. En çok 4 deneme yapılır.
- **Konum:** RAM'e yazılmış konum `p = (N − BNDT − FIFOL) mod N`. Bu, yeniden yüklemenin öncesinde (BNDT = 0) ve sonrasında (BNDT = N) aynı değeri verir. TC bayrağı, kesme hizmeti ve LLI yazmaç güncellemesi hangi sırayla olursa olsun konum iki kez sayılmaz.
- **İlerleme ve tur belirsizliği:** ilerleme `(p − p_önceki) mod N`. Son kabul edilen görüntüden bu yana hatta gelebilecek bayt sayısı (geçen süre × BRR'den hesaplanan hızın %110'u + 1 + FIFOL üst sınırı 255) yarım halkaya ulaşırsa, ilerleme yarım halkayı geçerse ya da yazmaç aralık dışıysa sonuç BELIRSIZ olur. Bekleyen veri atılır ve `dma_rx_hata` sayılır.
- **Tekdüzelik zorlaması kaldırıldı:** hiçbir değer sıkıştırılmıyor; geri giden konum belirsiz sayılıyor.
- **Üzerine yazma denetimi:** kopyadan önce ve sonra, kopya sonrası görüntü kararlı değilse kopya atılır ve sonra yeniden denenir.
- **Varsayımlar:** kaynakla doğrulanmamış donanım varsayımları başlıkta açıkça yazıldı (V1 BNDT davranışı, V2 BNDT/FIFOL birlikte görünürlüğü, V3 sıralı hedef yazımı). V2 bozulursa en çok bir bayt erken tüketilebilir; bayat bayt CRC/içerik denetiminde yakalanır, sessiz başarı üretmez. FIFO derinliğine dayanılmadı; FIFOL alanının 8 bitlik sınırı kullanıldı.

**Test:**
- `halka_sinir()`:
  - sol'un `(BNDT=0, FIFOL=0)` durumu toplamı tam N veriyor (eski işlev 2N veriyordu);
  - ardından yeniden yükleme (BNDT = N) konumu değiştirmiyor;
  - FIFO'da bayt varken `(0,3)` ve `(N,3)` aynı konumu veriyor;
  - kararsız görüntü hiçbir şeyi değiştirmiyor;
  - yarım halka ya da fazlası ilerleme, uzun yoklama boşluğu ve aralık dışı BNDT/FIFOL BELIRSIZ.
- `halka_modeli()`, iki tohum, ~60 000 kabul edilen görüntü:
  - kaynak okuması, hedef yazımı, TC bayrağı, kesme hizmeti ve LLI yeniden yüklemesi ayrı olaylar; sıraları rastgele, yeniden yükleme FIFO boşalmadan da olabiliyor;
  - okuyucunun üç okuması arasına DMA olayları giriyor;
  - kabul edilen her görüntüde toplam o anki RAM yazımına tam eşit (tekdüzelik zorlaması yok);
  - yanlış bayt kabul edilmedi;
  - kararsız görüntüler (~14 300), uzun boşluklarla belirsizlikler (~780) ve geride kalan tüketicide taşmalar yakalandı.

**Kart:** son imajla 10 koşuluk taramada kart 31 519 kararsız görüntüyü yeniden denedi, **belirsiz görüntü 0** (`KIMLIK` sonda: `kararsiz=31519 belirsiz=0`). Bütün koşularda kart tarafında `dma_rx_hata`, `rx_tasma`, UART hataları ve CRC 0. Alım halkasının azami doluluğu 141/16 384 B (12 M).

**Doğrulanmayan:** V1–V3 RM0456'dan okunmadı; ST Linux sürücüsünün yaptığı gibi kanal askıya alınmıyor. Kararlı görüntü bunun yerine iki BNDT okumasının eşitliğiyle seçiliyor.

## Y2 — Temsil aralığı ve taşma (kritik) — giderildi

**Değişiklik (`pc/kayit.cpp`):** Bütün alanlar türlerinin temsil aralığıyla sınanıyor; aşan kayıt GECERSIZ.
- **B ayarları:** KUR ve araç sınırları; örneğin `baud ≤ 20 000 000`, `pc_adet` ve `aralik_ns ≤ UINT32_MAX`, `kart_brr ≤ 0xFFFF`, bayraklar ≤ 1, `kart_hata_ppm` int32 içinde.
- **P sayaç ve zamanları:** ≤ INT64_MAX; bayraklar ≤ 1.
- **K alanları:** teldeki u32 sınırı.

Bütün toplama ve çarpmalar `__builtin_*_overflow` ile ve INT64_MAX sınırıyla yapılıyor; taşma GECERSIZ. Böylece işaretli zamana dönüşüm her zaman güvenli.

**Test (`y2_y3_negatif`, sol'un koşulu: `--kampanya`, en az 0,9 s):**
- `P.tx_gec_azami_ns` ve `P.t_son_yaz_ns`: UINT64_MAX, UINT64_MAX−1 ve INT64_MAX+1 GECERSIZ; INT64_MAX GECTI değil.
- `K.tx_gec_azami_us` ve `K.tx_son_us`: UINT64_MAX ve UINT32_MAX+1 GECERSIZ; UINT32_MAX GECTI değil.
- `P.rx_alinan = INT64_MAX` (çarpım taşması), `P.rx_okunan_bayt = UINT64_MAX`, `B.pc_adet = UINT32_MAX+1`, `B.aralik_ns = UINT64_MAX`, `K.sure_us = UINT32_MAX+1`: hepsi GECERSIZ.
- Temiz kayıt aynı koşulda GECTI.

## Y3 — Akış kronolojisi (kritik) — giderildi

**Değişiklik:**
- Kart akışta `tx_son_us`'u gecikme hesabıyla aynı zaman damgasından yazar (`ortak/mp3_kart.c`).
- Çözümleyici imkânsız kronolojiyi GECERSIZ sayar:
  - **PC:** son plan ≤ son yazma sonu ≤ son plan + azami gecikme + azami yazma süresi; son plan pencere içinde; hiç gönderim yoksa son zamanlar 0.
  - **Kart:** `plan_us = ⌊(gönderilen − 1) × aralık / 1000⌋` (firmware ile aynı tam sayı bölmesi); `plan_us ≤ tx_son_us ≤ plan_us + tx_gec_azami_us`; `tx_son_us ≤ sure_us`; hiç gönderim yoksa `tx_son_us = 0`.
- **ns/µs yuvarlaması:** kart planı firmware ile birebir aynı tam sayı bölmesiyle hesaplandığı ve tek damga kullanıldığı için tolerans **sıfırdır**.

**Test:**
- `K.tx_son_us` için 0, plan − 1 µs ve plan + gecikme + 1 µs GECERSIZ; sınır değerleri (plan ve plan + gecikme) GECTI.
- `P.t_son_yaz_ns = 0` GECERSIZ; P ve K son zamanlarının birlikte sıfırlanması (süre beklentisi olmadan) GECERSIZ.
- Kartın son gönderiminin koşu süresinden sonra olması GECERSIZ.
- Gecikme alanıyla tutarlı geç bitiş GECMEDI; yalnız son yazmayı büyütmek GECERSIZ.

## Y4 — Bekleyen kuyruğu (kritik) — giderildi

**Tanım (`pc/oturum.hpp`):**
- k isteğinin karar anı t_k'da j < k bekler ⇔ j gönderilmiş, yanıtı t_k'da görünür değil (yanıt > t_k; eşitlikte yanıt önce sayılır) ve t_k − yazma_sonu_j ≤ zaman aşımı (eşitlikte hâlâ bekler).
- Bekleyen ≥ 64 ise k gönderilmez.
- **Ölçüm:** k eklendikten SONRA kuyruk boyu (k dahil). Önceden ekleme öncesi ölçülüyordu; bu yüzden gerçek koşular 0 bildiriyordu. Sınır 64 olduğu için en çok 64.
- Yanıt yazma çağrısı dönmeden gelebilir; bu durumda k bir sonraki kararda çıkar.

**Çözümleyici:** R'den aynı tanımı karar anı = yazma başı alarak bağımsız yeniden üretir. Her j için son bekleme anı `E_j = min(yanıt_j − 1, yazma_sonu_j + zaman_aşımı)` alınır ve bir öncelik kuyruğuyla taranır (O(n log n)).

Okuyucunun yanıtı görünür kılması gecikebildiği için çevrim içi ölçüm yeniden üretilenden büyük olabilir ama küçük olamaz. Kurallar (tutmazsa GECERSIZ):
- yeniden üretilen ≤ P değeri ≤ 64;
- gönderim varsa P ≥ 1.

Bu kurallar yarış nedeniyle doğru koşuyu reddetmez.

**Test:**
- sol'un karşı örneği: yanıt = max(eski, 0,5 s + sıra × 1 ms), zaman aşımı 1 s; yeniden üretilen bekleyen 64'ü aşıyor, GECERSIZ.
- P = 0 ve P = 65 GECERSIZ.
- Uçtan uca, kart çıkışı 50 ms gecikmeli (200 istek/s): GECTI, gözlenen azami 11.
- Uçtan uca, 400 ms gecikmeli: kuyruk 64'te doluyor, istekler atlanıyor, GECMEDI ("kuyruk sınırı"); gözlenen 64, kayıt tutarlı.

**Kartta:** beş echo koşusunda gözlenen azami bekleyen 1. RTT (0,3–2,5 ms) 5 ms'lik istek aralığından kısa olduğu için yalnız eklenen istek bekliyor; yeniden üretilenle tutarlı.

## İlk turun bulgularında gerileme yok

İlk turun bütün birim, uçtan uca ve negatif testleri aynen koşuyor. Değişen tek şey, artık Y3/Y4 kurallarına takılan birkaç "tutarlı sahte" kaydın kronoloji ve kuyruk alanlarının da tutarlı biçimde uyarlanması; uyarlanmamış halleri artık GECERSIZ olarak ayrıca denetleniyor.

## 5. Kart ve kısa ön tarama

Önce yedek doğrulandı (SHA-256 `1f13870b…2457`) ve önceki imaj okundu. Kartta bir önceki MP3 imajı vardı: kimlik `20261118-192703-mp3-ca321d0d`, ilk 256 KB SHA-256 `c4d2d33d…4ba5`, `$IDA_CACHE/mp3/onceki-imaj-20261119-005404/`. Okumada eski MP4 kimlik dizgesi de çıktı; yükleme yalnız yazılan sektörleri sildiği için flash'ta kalmış artık bayttır.

Bu tur yeni imajla iki tarama yapıldı:

1. Ara imaj `20261119-005255-mp3-a5261e1f` (`$IDA_CACHE/mp3/kos/20261119-005413-tara/`). `KIMLIK` yanıtı 256 B sınırında kesildi ve konum tanıları görünmedi. Korunuyor; aynı hükümler: 12 M akış yalnız takvimden GECMEDI, diğerleri GECTI.
2. Son imaj `20261119-005548-mp3-43f9c836` (yalnız `KIMLIK` metni kısaltıldı; `$IDA_CACHE/mp3/kos/20261119-005626-tara/`; `kos.sh` çıkış kodu 1):

| Aday | Akış 256 B %80, 10 s | Echo 64 B 200/s, 5 s |
|---|---|---|
| 921 600 | GECTI | GECTI |
| 2 000 000 | GECTI | GECTI |
| 3 000 000 | GECTI | GECTI |
| 6 000 000 | GECTI | GECTI |
| 12 000 000 | **GECMEDI** | GECTI |

12 M akış:
- Kart PC'nin 34 783 çerçevesini hatasız aldı.
- PC, kartın gönderdiği 34 783 çerçeveden 34 782'sini aldı: bir CRC hatası, 275 atılan bayt.
- PC'nin karta yazması plandan 745 947 µs'ye kadar geride kaldı; son yazma t0 + 10,717 s'de bitti, sınır 10,100 s.

**Eski sonuçlar:** 1. tur taraması (`20261118-192402-tara`) yeni çözümleyiciyle bütün kayıtlarda GECERSIZ. Nedenler: eski firmware'in son gönderim damgası Y3 kronolojisine, eski "ekleme öncesi" kuyruk ölçümü (0) Y4'e uymuyor. Bu sonuçlar geçmiş olarak korunur; yeni kodla elde edilmiş gibi sunulmaz.

## Kalan belirsizlikler

- DMA varsayımları V1–V3 RM0456'dan okunmadı.
- 12 Mbaud'da kart→PC kaybının ve PC→kart gecikmesinin nedeni ayrılmadı.
- ASan/TSan bu macOS'ta başlamıyor.
- Windows yazılmadı.
- 600 s kampanya yapılmadı.
