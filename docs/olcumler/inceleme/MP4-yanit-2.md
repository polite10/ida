# MP4a — sol ikinci incelemesine yanıt (ikinci ve son düzeltme turu)

Tarih: 2026-11-18. Yazan: `ida-uygulayici` (Opus, `high`). İnceleme: `docs/olcumler/inceleme/MP4-sol-2.md` (değiştirilmedi). Görev: `docs/gorevler/W0-MP4a-sol-duzeltme-2.md`. Önceki yanıt `MP4-yanit.md` yeniden yazılmadı.

Bu turda yalnız `tools/olcum/mp4/pc/mp4_seri.cpp`, `tools/olcum/mp4/test/analiz_test.py`, `tools/olcum/mp4/kos.sh` ve `docs/olcumler/MP4-dongu-yuku.md` değişti. Firmware, adım kodu, derleme dosyaları ve imajlar değişmedi; karta erişilmedi, yeniden ölçüm ve yükleme yapılmadı. Kapanış sol'un son incelemesindedir; bu dosya kabul kararı değildir.

**Önceki yanıtta yanlış bir iddia:** `MP4-yanit.md` #2'deki "herhangi bir hata varsa … histogram CSV'leri hiç basılmaz/yazılmaz" cümlesi, o turdaki kod için **doğru değildi**: CSV'ler faz döngüsünde, T denetiminden ve son karar vermeden önce yazılıyordu (sol Y4). Bu tur düzeltildi (aşağıda); o dosya kurala uygun olarak değiştirilmedi.

## Y1 (kritik) — bütün örnekler taşmışken toplam ve en küçük denetlenmiyordu

**Değişiklik** (`mp4_seri.cpp`, adım süresi histogramı denetimi):
- Toplam süre sınırı histogram boş olsa da uygulanır: her kutudaki örnek `[k·16, k·16+15]`, her taşan örnek `[aralık_üstü, max]` içindedir; `top` bu toplam alt/üst sınırların dışındaysa hata (aralık_üstü = 200.000 × 16 = 3.200.000 çevrim).
- `tasan = n` ise en küçük değer aralığın dışında olmalıdır; histogramda örnek varsa en küçük değer aralık içinde ve ilk kutuda olmalıdır. Histogram boşken `tasan ≠ n` hatadır.
- Geçerli tümü-taşmış kayıt desteklenir: bütün yüzdelikler "belirlenemedi" yazılır, sayı uydurulmaz.

**Doğrulama:**
- sol'un karşı örneği (test üretecinde A'nın H satırı silindi, `h=0`, `tasan=1000 max=4000000`, `min=96003 top=96035119` korunarak CRC/sıra yenilendi) — komut `MP4-yanit-2.md` hazırlanırken çalıştırılan Python girdisi (test modülünün `kampanya/degistir/analiz` yardımcılarıyla):
  ```
  Y1 sol karşı örneği rc= 1
  HATA: faz A: bütün örnekler taşmış ama en küçük değer aralık içinde
  HATA: faz A: süre toplamı histogram ve taşma sınırlarıyla tutarsız
  sonuç: GEÇERSİZ (2 hata; sayısal özet basılmadı)
  ```
- Yeni testler (`sinama_ikinci`): eksik H tablosu (taşma yok) → geçersiz; tutarlı tümü-taşmış kayıt → geçerli, p50 ve p99,99 "belirlenemedi"; tümü-taşmış ama `min` aralık içinde → geçersiz; tümü-taşmış ama toplam `n·3.200.000`'den küçük → geçersiz; sınırdaki tek örnek (n−1 örnek son kutuda, 1 örnek tam aralık üstünde, `max = 3.200.000`) → geçerli, p50 ≤ 19999,99375 µs, p99,99 belirlenemedi; aynı kayıtta `max = 3.199.999` (taşan var ama en büyük aralık içinde) → geçersiz; taşmasız olağan kampanya → geçerli.

## Y2 (kritik) — yığın üst sınırı ve kapsama ilişkisi

**Değişiklik:**
- Çözümleyiciye zorunlu `--yigin-azami B` eklendi (verilmezse çıkış 2). Değer **kayıttan değil**, ölçülen imajın ELF'inden gelir: firmware yığını `_ebss + 256 B`'den `_estack`'e kadar boyar (`fw/platform.c`, `YIGIN_TABAN`), dolayısıyla ölçülebilecek en büyük yığın `_estack − (_ebss + 256)`'dır. `kos.sh` bunu `arm-none-eabi-nm` ile ELF sembollerinden hesaplayıp geçirir; ELF'te semboller yoksa koşmaz.
- Denetimler: `0 < yigin_tepe ≤ yigin_azami`; `0 < yigin_kayit ≤ 33.280` (firmware sabitleri: 32.768 B boyama bölgesi + 256 B üst pay + 256 B main/boyama işlevi çerçeve payı); kapsama ilişkisi `yigin_tepe > yigin_kayit` (`yigin_tepe` aynı yeniden boyanan bölgeyi ve ek olarak main çerçevesini kapsar, daha sonra okunur); açılış K satırındaki `yigin_tepe_acilis` de `≤ yigin_azami`. Firmware'e alan eklenmedi.
- İmajların sınırları (ELF'ten): -O2 1.586.416 B (sol'un harita dosyasından bulduğu değerle aynı), -Og 1.586.024 B, -O2-200 1.486.416 B, -Og-200 1.486.024 B, kisa 1.586.416 B.

**Doğrulama:**
- Gerçek O2 kaydının geçici kopyası, A fazının `yigin_tepe=3392` alanı `4294967295` yapılıp CRC yenilenerek, gerçek beklentilerle (400 Hz, maske 7, 241.000, en az 240.000 / 600 s, `--yigin-azami 1586416`) çözümlendi → **rc=1**, "imajın boyanabilen bölgesinden büyük", sayısal özet yok (test: `sinama_gercek_kayit`; ham dosya değiştirilmedi).
- Yapay testler: `yigin_tepe=4294967295` → geçersiz; `yigin_tepe=1, yigin_kayit=3000` → geçersiz ("kapsama aykırı"); `yigin_tepe = yigin_kayit` → geçersiz; `yigin_tepe = sınır + 1` → geçersiz; `yigin_kayit = 33.281` → geçersiz; `yigin_tepe` tam sınırda → geçerli; `--yigin-azami` yok → çıkış 2; aynı kayıt daha dar bir imaj sınırıyla (3.400 B) → geçersiz (sınır kayıttan gevşetilemez).
- Gerçek değerler geçer: O2 3.452 / 2.500 B, Og 2.752 / 2.264 B, O2-200 3.392 / 2.440 B, Og-200 2.752 / 2.264 B (`yigin_tepe` / `yigin_kayit`, fazların en büyüğü).

## Y3 (kritik) — bütün küme temizken temiz maksimum

**Değişiklik:** `lat_temiz_n = n` ise `lat_temiz_max = lat_max` zorunlu. Önceki kurallar sürer: `lat_temiz_n = 0` ise `lat_temiz_max = 0`; alt kümede `lat_min ≤ lat_temiz_max ≤ lat_max`.

**Doğrulama:**
- Gerçek O2 kaydının geçici kopyasında A'nın `lat_temiz_max=86` → `59` yapılıp CRC yenilenerek, gerçek beklentilerle → **rc=1**, "bütün adımlar temizken temiz gecikme en büyüğü genel en büyüğe eşit değil", sayısal özet yok. Değiştirilmemiş kopya → geçerli.
- Yapay testler: bütün küme (`n`) ve `59 < lat_max` → geçersiz; alt küme (`n−1`) ve temiz maks < genel maks → geçerli; sıfır küme (`0, 0`) → geçerli; sıfır küme ve temiz maks ≠ 0 → geçersiz.

## Y4 (önemli) — histogram dosyaları kampanya geçerliliğinden önce yazılıyordu

**Değişiklik:** histogram CSV'leri faz döngüsünde yalnız belleğe hazırlanır; **bütün** denetimler (T satırı dahil) geçtikten sonra yazılır. Yazmadan önce hedeflerin hiçbirinin var olmadığı denetlenir; biri varsa hiçbir dosya yazılmaz, var olan dosya ezilmez ve çıkış 3 olur ("veri geçerli ama dışa aktarım yapılmadı"; sayısal özet basılmaz). Dosyalar geçici adla yazılıp `link` ile yerine konur (hedef varsa başarısız olur, ezmez); bir yazma hatasında bu çağrının yazdıkları geri alınır. Geçersiz analizde dosya yazılmaz, çıktı yalnız hata listesi ve `sonuç: GEÇERSİZ` olur.

**Doğrulama** (`sinama_disa_aktarim`, geçici dizinde dosya sistemi gözlemiyle): geçerli kampanya → 9 dosya; eksik T → geçersiz, **0 dosya**; yanlış T (toplam adım) → geçersiz, **0 dosya**; yalnız C fazında hata (A ve B geçerli) → **0 dosya**; hedeflerden biri önceden var → çıkış 3, var olan dosyanın içeriği değişmedi, başka dosya oluşmadı. Gerçek kayıtlarda: geçersiz `og-20261117-193512` v3 analizi için hiç histogram dosyası oluşmadı.

## Testler

Komut (depo kökünden): `python3 -I tools/olcum/mp4/test/analiz_test.py`

Sonuç: **94 denetim geçti, 0 başarısız** — önceki 67 denetim ve 27 yeni (Y1–Y3: 19, Y4 dosya sistemi: 5, gerçek O2 kaydının geçici kopyası üzerinde: 3). Gerçek kayıt testleri `${IDA_CACHE}/mp4/kos/o2-20261117-190043.csv` yoksa "ATLANDI" yazar ve sayılmaz; bu çalıştırmada kayıt vardı ve üçü de koştu. PC araçları `-Wall -Wextra -Wconversion` ile uyarısız derlendi (`cmake --build tools/olcum/mp4/derleme/pc`).

## Gerçek kayıtların v3 analizi

Önceki analiz çıktıları ezilmedi. Yeni çıktılar `${IDA_CACHE}/mp4/kos/<kayıt>-analiz-v3-20261118.txt`, histogramlar `<kayıt>-hist-v3-20261118-*`. Her çıktının başında kullanılan imajın SHA-256'sı, yığın sınırı ve tam komut yazılıdır. Yığın sınırı her kayıt için kendi ölçülen ELF'inden hesaplandı.

| Kayıt | Gerçek ayarlar | v3 sonucu |
|---|---|---|
| `o2-20261117-190043` | -O2, 400 Hz, maske 7, 241.000, en az 240.000 / 600 s, yığın ≤ 1.586.416 | **GEÇERLİ**; sayısal satırlar v2 ile birebir aynı |
| `og-20261118-005058` | -Og, 400 Hz, 7, 241.000, en az 240.000 / 600 s, ≤ 1.586.024 | **GEÇERLİ**; koşu içindeki analizle birebir aynı |
| `o2-200-20261118-013802` | -O2, 200 Hz, maske 2, 121.000, en az 120.000 / 600 s, ≤ 1.486.416 | **GEÇERLİ**; aynı |
| `og-200-20261118-014813` | -Og, 200 Hz, 2, 121.000, en az 120.000 / 600 s, ≤ 1.486.024 | **GEÇERLİ**; aynı |
| `kisa-20261117-185942` | -O2, 400 Hz, 7, 4.000, ≤ 1.586.416 | **GEÇERLİ** (düzenek denemesi, kabul kampanyası değil) |
| `og-20261117-193512` | -Og, 400 Hz, 7, 241.000 | **GEÇERSİZ** (fazlar ve T yok); histogram dosyası oluşmadı |
| 2026-11-16 kayıtları | eski biçim | v2'de zaten geçersiz; v3'te yeniden koşulmadı (eski imajların ELF'i korunmadığından yığın sınırı imajdan türetilemez; açılış satırında kimlik/başlık olmadığı için sonuç değişmez) |

Toplam 33 histogram dosyası yazıldı (beş geçerli kaydın 11 fazı × 3). Karşılaştırma komutu: v2/koşu-içi ve v3 çıktılarının `=== faz`, `adım süresi`, `kaçırılan`, `tetik gecikmesi`, `önceki adım`, `parça`, `S`, `sağlama`, `yığın` satırları `diff` ile karşılaştırıldı; beş geçerli kayıtta fark yok.

Gerçek veride yeni bir geçersizlik çıkmadı. Bu turda ilk v3 denemesi yanlış kabukta (zsh, araç yolu bulunamadı) koşup altı kayıt için kullanım hatası (çıkış 2) çıktısı yazdı; bu az önce oluşmuş hatalı dosyalar silinip aynı komutlar `sh` altında yeniden koşuldu. Önceden var olan hiçbir kayıt ya da analiz dosyası silinmedi.

## Hükümler (değişmedi)

O2, 400 Hz: **D1 geçmedi** (A'da kaçırılan 0; B'de 241.000/241.000), **D2 geçmedi** (B en büyük 3322,38125 µs > 1000 µs), **D3 geçti**, **D4 geçti** (histogram, yığın ve kod boyutu O2 ve Og için raporda). Og ve 200 Hz sonuçları bilgi içindir (rapor Bölüm 4–5).

## Çalıştırılmayan ya da doğrulanmayanlar

- Kart koşusu ve yükleme yapılmadı (görev gereği); `kos.sh`'nin yeni yığın sınırı bölümü yalnız kuru olarak (`sh -n` ve aynı komutların Og ELF'iyle ayrı çalıştırılması → 1.586.024) denendi, gerçek bir koşu içinde koşmadı.
- `yigin_kayit` üst sınırındaki 256 B'lik "main/boyama işlevi çerçeve payı" firmware kodundan çıkarılmış bir paydır; boyama işlevinin gerçek çerçeve boyutu ölçülmedi. Gerçek değerler (≤ 2.500 B) sınırın çok altındadır.
- Yığın ölçümleri tek koşunun gözlemidir; adımın kendi payı ve erişilebilir en kötü yol analiz edilmedi (önceki yanıttaki gibi).
- Önceki yanıtta listelenen diğer doğrulanmayanlar (kesme → adım başı gecikme yüzdelikleri, aynı imajın iki uzun koşusu, mutlak saat) geçerliliğini korur.
