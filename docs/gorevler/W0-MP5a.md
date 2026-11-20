# W0-MP5a — Kartta tel katmanının maliyeti (MAVLink 2 ayrıştırma ve kodlama)

Tarih: 2026-11-20. Yazan: `ida-uygulayici` (Opus; K18). İnceleyen: sol. Dal: `olcum/mp5-tel-maliyeti` (taban: `is/w2-protokol`'ün gönderilmiş son hâli). Kayıt: `ida-kayit`. Kart işidir: kart tektir, bu iş sürerken başka kart işi koşmaz.

Bu ölçüm iskelet §8'in "A1 biçimi" kararını ve A1 §8.1 O2'yi kapatır: MAVLink 2 + özel dialect'in kartta ayrıştırma ve kodlama maliyeti hiç ölçülmedi. `docs/mimari/09-test-ve-is-akisi.md` MP5 satırı iki adayı (MAVLink ile açık serileştirmeli özel biçim) temsili mesajlarla karşılaştırmayı öngörüyordu; o zamandan beri dialect yazıldı ve `ida_link` tel katmanı var. **Kapsam değişikliği (mimar):** yalnız seçilen biçim, nihai dialect ve ürün kitaplığıyla ölçülür; yedek biçim ancak sonuç aşağıdaki eşikleri aşarsa ve iyileştirme denemesi de yetmezse ayrı karardır, bu işte yazılmaz.

Ölçüm düzeneği atılabilir prototiptir (`tools/olcum/mp5/`; ürün kodu değildir). Ürün kitaplığı (`protocol/ida_link/`) bu işte **değiştirilmez**; kartta olduğu gibi, `fw-release` bayraklarıyla derlenip bağlanır.

## Eşikler (ölçümden önce yazıldı; mimar seçimi, dayanakları tahmin)

Koşul: `fw-release` derlemesi (W2-depo ön ayarının bayrakları), 160 MHz, kesmeler ölçüm bölgesinde kapalı ya da ölçüme girmediği gösterilmiş, süre DWT çevrim sayacıyla.

| # | Büyüklük | Eşik | Dayanak |
|---|---|---|---|
| E1 | Bir çekirdek adımı sürerken birikebilecek en büyük akışın (3,5 ms × 300 B/ms = 1 050 B; A1'in sim → kart yönündeki en ağır gerçekçi çerçeve karışımı) ayrıştırılması, en kötü | ≤ 0,20 ms | A4-K19: adım dönüşünden dağıtıma 0,5 ms; bunun 0,3 ms'si en uzun arka plan dilimi, kalanı ayrıştırma (A4 §4.3; tahmin) |
| E2 | Bir kontrol periyodunun tel işi toplamı: 1 200 B'lık pencerenin ayrıştırılması + kart → sim yönünün en kötü periyodunun kodlanması (A1 E19'un çerçeve kümesi), en kötü | ≤ 0,50 ms | Periyodun %10'u (mimar seçimi); adım bütçesi 3,5 ms ile birlikte periyodun içinde kalır |
| E3 | Dialect'in en büyük çerçevesinin (258 B) tek başına ayrıştırılması ve kodlanması, en kötü | Raporlanır (eşik yok) | Bayt başına ve çerçeve başına maliyetin ayrıştırılması için |

Eşik aşılırsa iş başarısız sayılmaz: sonuç budur ve karar mimarındır. O durumda aynı düzenekte, ürün kitaplığına dokunmadan (prototip içinde kopya üzerinde) şu iki deneme de ölçülür ve raporlanır: tablolu CRC (256 girdili tablo; flash maliyetiyle) ve derleyici eniyileme düzeyinin etkisi (`-O2` ile `-Os`). Başka iyileştirme önerisi yalnız yazılır, uygulanmaz.

## Kapsam

1. **Düzenek.** `tools/olcum/mp5/`: kart açılışı, 160 MHz saat ve DWT ölçümü için MP4a düzeneğinden (`tools/olcum/mp4/`; `main`'de) yeniden kullanım; çıktı ST-LINK sanal seri portundan (USART1) metin olarak. Ürün kitaplığı ve üretilmiş MAVLink başlıkları W2-depo/W2-protokol derleme düzeninden alınır; `protocol/ida_link/olcum/olcum_cekirdek.c` kullanılabilir (host'a özgü kod içermez), yetmiyorsa prototip kendi ölçüm çekirdeğini yazar. Yükleme ve koşturma MP4a'daki betik düzeniyle; kişisel yol dosyalara girmez.
2. **Girdi akışları** (derleme zamanında üretilir, kartta sabit veri): (a) E1 akışı: 1 050 B; (b) tam pencere: 1 200 B (A1 §3.9/§3.10'daki pencere planına uyan gerçekçi karışım: TF, IMU, öteki sensörler, emniyet, iki Ping360 parçası, algılama partisi ve kayıtları, komutlar); (c) kart → sim en kötü periyot (A1 E19: aktüatör, olaylar, komut sonuçları, seyir, durum, çağrı izi ve kaybedilebilir sınıfın payı); (d) en büyük tek çerçeve. Her akışın çerçeve dökümü (kimlik, boy, adet) raporda; akışlar A1'in altın çerçevelerindeki alan desenleriyle ya da belirlenimci desenle doldurulur ve kırpma gerçekçidir (sıfırlarla dolu yük ölçümü yanıltır: kırpılmış ve kırpılmamış iki uç ayrıca ölçülür).
3. **Ölçümler.** Her akış için en az 10 000 yineleme: en küçük, ortanca, p99,9, en büyük (çevrim ve µs); bayt başına çevrim; çerçeve başına sabit maliyet (doğrusal uyumla); besleme biçiminin etkisi (tek seferde, 64 B'lık parçalarla, bayt bayt). Önbellek ve ön getirme etkisi için ICACHE açık/kapalı durumu yazılır (hangisi ürün yapılandırmasıysa o esas alınır; MP1/MP4 raporlarına bak). Ölçüm bölgesinin kendisinin maliyeti (boş döngü) çıkarılır ve raporlanır.
4. **Doğruluk.** Kartta ayrıştırılan çerçeve sayısı ve sayaçlar beklenenle aynı; kodlanan baytların sağlaması host'ta üretilen referansla aynı (ölçülen işin gerçekten yapıldığının kanıtı; derleyicinin işi silmediği gösterilir).
5. **Bellek.** Kitaplığın ve örnek yapılarının kartta kapladığı yer (W2-protokol raporundaki değerlerin bu derlemede doğrulanması), yığın kullanımı.
6. **Rapor.** `docs/olcumler/MP5-tel-maliyeti.md`: düzenek, koşullar, tablolar, eşiklerle karşılaştırma, ham çıktının yeri, ölçülmeyenler. Ölçülen ile tahmin edilen ayrı yazılır. Ham kayıtlar depoya girmez (yerel önbellek; özetleri ve betikleri girer).

## Kapsam dışı

Ürün kitaplığında değişiklik; UART alım yolu (DMA, kesme, tampon) ve gerçek bağlantı üzerinden uçtan uca ölçüm (W3 kart yazılımı ve W4); A1↔A4 dönüştürücüsünün maliyeti (henüz yok); yedek biçimin yazımı; Windows.

## Kabul

1. E1, E2, E3 ölçülmüş ve eşiklerle karşılaştırılmış; her sayı ham çıktıdan yeniden üretilebilir (betik ve komut raporda).
2. Doğruluk denetimi (kapsam md.4) geçmiş; iki bağımsız koşunun sonuçları arasındaki fark raporda.
3. Eşik aşıldıysa iki deneme (tablolu CRC, eniyileme düzeyi) ölçülmüş.
4. Kart ölçümden sonra bilinen bir durumda bırakılmış (hangi yazılımın yüklü olduğu raporda).

## Rapor (agent'ın mimara raporu)

Yazılan dosyalar; kabul maddelerinin sonucu; eşik karşılaştırması tek tabloda; düzenekte karşılaşılan sorunlar; doğrulanamayanlar; worktree ve dal. Commit atılmaz.
