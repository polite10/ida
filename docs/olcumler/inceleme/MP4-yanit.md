# MP4a — sol ilk incelemesine yanıt

Tarih: 2026-11-18. Yazan: `ida-uygulayici` (Opus, `high`). İnceleme: `docs/olcumler/inceleme/MP4-sol.md` (değiştirilmedi). Görev: `docs/gorevler/W0-MP4a-sol-duzeltme.md`.

Bu tur yalnız PC çözümleyicisini (`tools/olcum/mp4/pc/mp4_seri.cpp`), testleri (`tools/olcum/mp4/test/analiz_test.py`), koşu betiğini (`tools/olcum/mp4/kos.sh`) ve raporu değiştirdi. Firmware (`fw/**`), adım kodu (`adim/**`), derleme dosyaları ve derleme betiği değişmedi; O2 kampanyası yeniden koşulmadı. Kapanış hükmü sol'un ikinci incelemesindedir; bu dosya kabul kararı değildir.

## Bulgu bulgu

### #1 (kritik) — başlık ve beklenen kampanya doğrulanmıyordu

**Değişiklik.**
- `analiz` artık `--kimlik`, `--beklenen-hz`, `--beklenen-fazlar`, `--beklenen-faz-adim` olmadan çalışmaz (çıkış 2). Açılış satırındaki `hz`, `fazlar`, `faz_adim` bunlarla **aynı** olmalı; böylece imajın bildirdiği maske zorunlu kampanyayı daraltamaz (A+B+C beklenirken yalnız-B maskeli kayıt rc=1).
- Açılış başlığı: kesin şema (tanımsız alan yok), `hz` 160 MHz'i tam böler, `periyot_cyc = 160 MHz / hz`, maske 1..7, `faz_adim > 0`, `gecis = hz`, histogram parametreleri > 0 ve `lat_kaba_n` bir periyodu kapsar; `--en-az-adim` verilirse `faz_adim` bunun altında olamaz.
- B2: tam bir kez ve B'nin hemen ardından; zorunlu alanların hepsi; `saat_adim = 0`, `hesap_hz = hal_hz = 160000000`, `uart_hata = 0`, `DEV_ID 0x481`, flash gecikmesi 4, ICACHE açık, FPU açık, FPSCR'de FZ kapalı ve en yakına yuvarlama, USART1'de UE/TE/FIFOEN açık.
- K satırları: `damga_100_cyc` ve `tim2_periyot_cyc` zorunlu; TIM2 periyodu DWT ile ±16 çevrim içinde ve `beklenen = periyot_cyc`.
- T satırı: maske açılışla aynı; toplam adım = `2·hz + faz_sayısı·(hz + faz_adim)`; `d_atlanan` son fazın S satırıyla aynı.
- `kos.sh` her derleme için beklenen ayarları açıkça verir: `o2|og` → 400 Hz, maske 7, 241.000 adım, en az 240.000 adım ve 600 s; `*-200` → 200 Hz, maske 2, 121.000; `kisa` → 400 Hz, 7, 4.000; bilinmeyen derleme adı reddedilir.

**Kanıt.** Yeni testler (aşağıda): boş maske (`--en-az` ile), yalnız-B maskesinin A+B+C beklentisini daraltamaması, beklenen `faz_adim` uyuşmazlığı, B2 yok, iki B2, saat değerleri hatalı, yalnız `uart_hata=1`, `hz=0`, TIM2 periyodu uyuşmazlığı, T toplam adımı yanlış, her bir `--beklenen-*` eksikse çıkış 2. İncelemedeki yeniden üretme betiği (son satırdaki `next(...)` çağrısına yalnız varsayılan değer eklenerek; özgün haliyle gecikme satırı artık basılmadığı için StopIteration verirdi) bütün vakalarda rc=1 verdi:

```
faz_yok rc= 1 sonuç: GEÇERSİZ (3 hata; sayısal özet basılmadı)
B2_yok rc= 1 sonuç: GEÇERSİZ (1 hata; sayısal özet basılmadı)
hatali_saat rc= 1 sonuç: GEÇERSİZ (3 hata; sayısal özet basılmadı)
hz_sifir rc= 1 sonuç: GEÇERSİZ (3 hata; sayısal özet basılmadı)
S_alanlari_yok rc= 1 sonuç: GEÇERSİZ (48 hata; sayısal özet basılmadı)
Q_nan rc= 1 sonuç: GEÇERSİZ (3 hata; sayısal özet basılmadı)
lat_max_sifir rc= 1 sonuç: GEÇERSİZ (12 hata; sayısal özet basılmadı)
```

Not: sınama üretecinin B2 satırı artık bütün zorunlu alanları içerir. İnceleme betiğindeki `hatali_saat` değiştirmesi (`rcc_csr=0x0 idcode=…` → `saat_adim=7 …`) yeni üreteçte yinelenen alan oluşturduğu için biçim hatasıyla reddedilir; aynı değerlerin **anlamsal** reddi (`saat kurulumu başarısız`, `160 MHz değil`) test kümesindeki ayrı vakayla sınanır. Bunu sağlamak için onaltılık alan ayrıştırması "0x + 1..8 büyük harfli hane" olarak kesinleştirildi (kart 8 hane basar).

### #2 (kritik) — G/Q/S şeması ve sayısal tutarlılık

**Değişiklik.**
- Bütün sayılar `kesin_sayi` ile ayrıştırılır: yalnız rakam (eksi, artı, boşluk, ondalık, çöp yok), uint64 taşması yok; 32 bitlik kart alanları ayrıca ≤ 2³²−1. Satır içinde yinelenen alan biçim hatasıdır. Her satır türü (B, B2, K, F, G, Q, S, E, H/L/M, T) için zorunlu alan listesi ve tanımsız alan reddi.
- F: `min ≤ max`, `max_adim < n`, `n·min ≤ top ≤ n·max`, `asim, asim_dwt, tasan ≤ n`, `tik_aralik ≥ n`, `atlanan ≥ tik_aralik − n`, `|duvar_cyc − (tik_aralik−1)·periyot| < periyot`, `duvar_cyc ≥ (n−1)·min`; her adım periyottan uzunsa `asim = asim_dwt = n`; hiçbir adım periyoda yaklaşmıyorsa (maks + gecikme maks + 64 < periyot) aşım 0.
- G: `lat_min ≤ lat_max < periyot`, `lat_tasan = 0`, birleşik gecikme histogramının ilk/son kutusu `lat_min`/`lat_max`'ı içerir, `lat_top` histogram kutularının alt/üst sınır toplamları arasında ve `n·lat_min..n·lat_max` içinde, kaba kutular ince aralıkla çakışmaz; `lat_temiz_n ≤ n`, `lat_temiz_max ≤ lat_max` (temiz örnek yoksa 0, varsa ≥ `lat_min`); `isr_min ≤ isr_max < periyot`.
- Q: tam olarak `p0..p9`, her biri `toplam/en_büyük` kesin sayı; en büyük ≤ adım en büyüğü, en büyük ≤ toplam ≤ n·en büyük, parça toplamlarının toplamı ≤ adım süresi toplamı.
- S: bütün alanlar zorunlu; `crc_hata = 0`; `yigin_kayit_tasti = 0` (taşmışsa değer yalnız alt sınırdır, geçersiz); yığın değerleri aralıkta; birikimli sayaçlar (`kov`, `skaler`, `d_atlanan`) fazdan faza azalmaz.
- Histogram: sözcük biçimi kesin, sayı > 0, kutu aralıkta, yinelenmez, satır boş değil; adım histogramı toplamı + taşan = n, en küçük/en büyük kutuları, toplam süre sınırları; taşma varsa en büyük aralık dışında olmalı, yoksa içinde.
- **Herhangi bir hata varsa sayısal özet ve histogram CSV'leri hiç basılmaz/yazılmaz**; çıktı yalnız hata listesi ve `sonuç: GEÇERSİZ`.

**Kanıt.** Yeni test vakaları: S alanları silinmiş, `p0=nan/nan`, `lat_max=0`, `lat_min > lat_max`, histogramla çelişen `lat_top`, temiz en büyük > `lat_max`, eksik `isr_max`, eksi sayı (`n=-1000`), taşan sayı (23 hane), Q'da sayı sonrası çöp (`1000000x`), parça toplamı > adım toplamı, `yigin_kayit_tasti=1`, `crc_hata=3`, `asim > n`, tutarsız `duvar_cyc`, tanımsız alan, yinelenen alan, `max_adim ≥ n`, yinelenen ve sıfır sayılı histogram kutusu, ince aralıkta kaba gecikme kutusu. Hepsi rc=1 ve çıktıda sayısal özet yok (test bunu ayrıca denetler).

### #3 (önemli) — `duvar_cyc` son adımın sonunu içermiyor

**Değişiklik.** Çözümleyici ve rapor alanı "**süre alt sınırı (ilk adım başı → son adım başı)**" olarak adlandırır, mikrosaniyede aşağı yuvarlar; son adım dahil süreyi yalnız `[duvar + min, duvar + max]` aralığı olarak verir (alt uç aşağı, üst uç yukarı yuvarlanır). `--en-az-sn` denetimi bu alt sınırla yapılır. Firmware değişmedi; son adımın kesin süresi uydurulmadı.

**Kanıt.** O2 A fazı yeniden analizi: `süre alt sınırı 602.497500 s; son adım dahil süre [602.498095, 602.498839] s` — incelemenin bağımsız hesabıyla ([602.4980953875, 602.4988389625] s) aynı. O2'de üç fazın alt sınırı 602,4975 / 821,428775 / 602,497499 s; hepsi ≥ 600 s. Og için Bölüm "Og kampanyası" aşağıda.

### #4 (önemli) — yığın ölçümlerinin kapsamı

**Değişiklik.** Raporda (§3.2 ve §4) "açılıştan bu yana" iddiası kaldırıldı. `yigin_tepe` kapsamı: son yeniden boyamadan (fazın geçişinin başı) bu yana — geçiş adımları, kayıtlı adımlar, faz dökümünün S satırına kadarki kısmı, kesmeler, main çerçevesi. `yigin_kayit` kapsamı: aynı bölgede main döngüsü çerçevesinin altı, **geçiş adımları ve ölçüm çevresindeki işler dahil** (girdi üretimi, durum satırı, UART). Koşu özeti fazlarda gözlenen değerlerin en büyüğüdür; çözümleyici bunu ayrıca basar (`yığın (fazlarda gözlenen tepelerin en büyüğü)`). Firmware değişmedi.

**Kanıt.** O2: `yigin_tepe` A/B/C = 3392 / 3452 / 3452 B → en büyük 3452 B; `yigin_kayit` 2440 / 2500 / 2500 B → 2500 B; `yigin_kayit_tasti` hepsinde 0. Kısa koşuda gözlenen 3392 → 3452 → 3396 B dizisi bu tanımla açıklanır (her faz kendi tepesini bildirir).

### #5 (önemli) — "≤" üst sınırları aşağı yuvarlanıyordu

**Değişiklik.** Yüzdelikler tam sayı aritmetiğiyle (⌈q·n⌉, `__int128`) bulunur; bütün çevrim değerleri çevrim/160 olarak beş ondalıkla **kesin** yazılır (160 = 2⁵·5 olduğundan her çevrim değeri beş ondalıkta tam gösterilir; yuvarlama hiç yok). Süre sınırlarında alt uç aşağı, üst uç yukarı yuvarlanır. Ortalamalar "≈" ile işaretli bilgi değeridir.

**Kanıt.** Test beklentisi düzeltildi: 58 çevrim için `p99,9 ≤0.36250` beklenir ve `≤0.362` görülmemelidir. Ek özellik testi: 25 rastgele histogramda (tohum 20261118) basılan her yüzdelik değerinin gerçek sıralı örnekten küçük olmadığı, o örneğin kutusunun üst kenarını (en büyükle kırpılmış) aşmadığı ve kesin çevrim değerine karşılık geldiği denetlenir.

## Testler

Komut (depo kökünden): `python3 -I tools/olcum/mp4/test/analiz_test.py`

Sonuç: **67 denetim geçti, 0 başarısız** (önceki 31 denetimin hepsi, kesin gösterime göre güncellenmiş beklentilerle, ve 36 yeni denetim). Kümeler: temel analiz (23, CRC uyumu dahil), sol olumsuz örnekleri ve sınır durumları (34), yüzdelik özelliği (1), sözde uçbirimle kayıt (2), D3 çağrı denetleyicisi (7). PC araçları `cmake --build tools/olcum/mp4/derleme/pc` ile `-Wall -Wextra -Wconversion` altında uyarısız derlendi.

## Gerçek kayıtların yeniden analizi

Eski analiz çıktıları ezilmedi; yeni çıktılar `${IDA_CACHE}/mp4/kos/<kayıt>-analiz-v2-20261118.txt`, histogramlar `<kayıt>-hist-v2-20261118-*`.

| Kayıt | Verilen gerçek ayarlar | Sonuç |
|---|---|---|
| `o2-20261117-190043` | kimlik `20261117-185727-o2-a11153bb`, 400 Hz, maske 7, 241.000, en az 240.000 / 600 s | **GEÇERLİ**; sayılar önceki analizle aynı (yalnız gösterim kesinleşti) |
| `kisa-20261117-185942` | kimlik `20261117-185727-kisa-bc64cc5d`, 400 Hz, 7, 4.000 | **GEÇERLİ** (düzenek denemesi; kabul kampanyası değil) |
| `og-20261117-193512` | kimlik `20261117-185727-og-be020939`, 400 Hz, 7, 241.000 | **GEÇERSİZ**: A/B/C özet satırı yok, T yok (USB bağlantısı 4. dakikada koptu) |
| `o2-20261116-011044`, `kisa-20261116-010707`, `kisa-20261116-010945` | eski düzenek; açılış satırında kimlik/başlık alanları yok | **GEÇERSİZ** (eski biçim; zaten kabul kanıtı sayılmıyordu) |

Gerçek veride yeni bir geçersizlik çıkmadı: O2 uzun kayıt yeni şema, tutarlılık ve sayaç denetimlerinin hepsinden geçti.

## Og kampanyası

Yarım kalan zorunlu -Og kampanyası **mevcut imajla** (firmware kaynağı değişmeden, yeniden derlenmeden) tamamlandı.

- Ön denetim: `shasum -a 256 tools/olcum/mp4/derleme/og/mp4.elf` → `5f0fd81f63a38bdcc014e77e2157ec841f639e007dc833704e6e1372b6223b5c` (derleme kaydıyla aynı); kimlik `20261117-185727-og-be020939` ELF'te var; `/dev/cu.usbmodem11403` başka süreçte açık değil (`lsof` boş); ST-LINK/kayıt süreci yok; AC bağlı; `tools/olcum/mp1/yedek-dogrula.sh` → yedek geçerli (SHA-256 `1f13870b…2457`). Yükleme `yukle.sh` içinden yeniden yedek doğrulamasıyla yapıldı: "Download verified successfully".
- Komut: `sh tools/olcum/mp4/kos.sh og 6600` (kendini `caffeinate -dims` altında başlatır). İlk deneme yanlış çalışma dizini yüzünden betik bulunamadan sona erdi (karta erişilmedi); mutlak yolla yeniden başlatıldı.
- Kayıt: `${IDA_CACHE}/mp4/kos/og-20261118-005058*`; 2026-11-17 21:50:58–22:37:50 UTC; imaj SHA-256 başta ve sonda aynı; seri kayıt T satırıyla bitti (çıkış 0).
- Düzeltilmiş çözümleyici (koşu betiğinin içinden, `--beklenen-hz 400 --beklenen-fazlar 7 --beklenen-faz-adim 241000 --en-az-adim 240000 --en-az-sn 600`): **GEÇERLİ**; 3.604 satır, sıra 0…3603 kesintisiz, CRC/biçim hatası 0, tek açılış.
- Süre alt sınırları: A 602,497500 s, B 1573,712475 s, C 602,497499 s (hepsi ≥ 600 s); her faz 241.000 adım.
- Sonuçlar: A en büyük 2572,78125 µs, **3.013 kaçırılan adım**, atlanan 0; B en büyük 6421,69375 µs, 241.000/241.000 kaçırılan, 388.488 atlanan tetik; C en büyük 2307,84375 µs, kaçırılan 0. Sağlama toplamları üç fazda -O2 ile aynı (`0xC02B9132`, `0x8F3440B8`, `0x18529BF5`). Yığın: fazların en büyüğü `yigin_tepe` 2.752 B, `yigin_kayit` 2.264 B, taşma yok.

200 Hz ek koşuları (mevcut imajlarla, aynı ön denetimlerle; zorunlu değil): `o2-200-20261118-013802` (SHA-256 `1beaf4cb…ec6e`) **GEÇERLİ**, B en büyük 3321,96875 µs, kaçırılan 0/121.000, süre alt sınırı 604,994999 s; `og-200-20261118-014813` (SHA-256 `e7e22797…a117`) **GEÇERLİ**, B en büyük 6421,45000 µs, kaçırılan 121.000/121.000, süre alt sınırı 788,222534 s. İkisinin sağlama toplamı aynı (`0xEBC826A4`).

## Çalıştırılmayan ya da doğrulanmayanlar

- O2 uzun kampanya yeniden koşulmadı (görev gereği); yalnız yeniden analiz edildi.
- Aynı imajın iki uzun koşusu yapılmadı; belirlenimcilik yalnız sağlama toplamlarının imajlar ve derlemeler arası eşitliğiyle gösterildi.
- Görev tanımındaki "kesme → adım başı" gecikmesinin yüzdelikleri ölçülmedi; rapor "güncelleme olayı → ana döngünün tetiği gördüğü an" ölçümünü ve kesme girişinin yalnız en küçük/en büyüğünü verir (incelemenin notuyla aynı).
- `duvar_cyc` firmware'de değişmedi; son adım dahil süre yalnız aralık olarak verilebilir.
- Yığın ölçümleri tek koşunun gözlemidir; adımın kendi payı ayrı ölçülmedi; erişilebilir en kötü yol analizi yapılmadı.
- -Og A'da "temiz" adımlarda görülen 65,15625 µs'lik gecikmenin adım sonrası ölçüm dışı işlerden geldiği açıklaması koddan çıkarılmıştır; ayrıca ölçülmedi.
- Kartın mutlak saat doğruluğu (HSE) bağımsız bir referansla doğrulanmadı; TIM2 ve DWT aynı saatten beslenir.
- Gerçek çekirdekle yineleme ve taşıma (A1) dahil ölçüm bu işin kapsamı dışındadır.
