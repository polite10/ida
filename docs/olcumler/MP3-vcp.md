# MP3a — ST-LINK VCP bağlantı ölçümü

| | |
|---|---|
| Görev | `docs/gorevler/W0-MP3a.md`; düzeltmeler `W0-MP3a-duzeltme-1.md`, `W0-MP3a-duzeltme-2.md` (W0, ölçüm prototipi; atılabilir, ürün protokolü değil) |
| Ölçüm | M3 (ST-LINK VCP hız, kayıp, gidiş-dönüş gecikmesi); K17 |
| Uygulayan | `ida-uygulayici` (Opus) |
| Dal | `olcum/mp3-vcp` (commit yok) |
| Durum | **600 s kampanya tamamlandı (2026-11-19); sol son veri incelemesi yapıldı (`docs/olcumler/inceleme/MP3-sol-veri.md`): hükümler, sayaçlar ve RTT tablosu ham kayıtlardan bağımsız doğrulandı; üç rapor düzeltmesi uygulandı (`inceleme/MP3-yanit-3.md`).** Kapsam: `W0-MP3a-kampanya.md` ve `W0-MP3a-kapsam-karari.md`. Akış: 921 600, 2 M ve 3 M GECTI; 6 M GECMEDI (PC takvimi). Seçilen hız 3 M; echo profillerinden yalnız 256 B 400/s GECTI, diğer üçü PC takvim sınırını tek tük aştığı için GECMEDI. Kod, aygıt yazılımı ve betikler kampanyada değiştirilmedi. Önceki turların metni (düzeltme turu 2 ön taraması dahil) aşağıda tarihli geçmiş olarak korunur. |

## Uzun kampanya (2026-11-19)

Ölçütler `W0-MP3a.md` "Dondurulmuş kampanya tanımı" ve V2/V3 ile aynıdır; kapsam kararı uyarınca echo yalnız azami bekleyen 64 ve yanıt zaman aşımı 1000 ms ile değerlendirilir. Baud/BRR/kapasite nominal 160 MHz'ten hesaptır; fiziksel hat hızı ölçülmedi. Bu bölüm kontrol hızı ya da ürün başarımı kararı içermez.

### Kimlik ve ortam

- İmaj ve PC aracı düzeltme turu 2 derlemesidir: kimlik `20261119-005548-mp3-43f9c836`, `mp3.elf` SHA-256 `43f48ae2b89f9983b1ad59676ae21cd638f67b08aaca31d642e6305a1c345b13`, `mp3_vcp` SHA-256 `477a92ee8c8ae107e8fd2fef6825d002866399f0c0f7e36efdaf9a8f0bd40598`.
- Kaynak özeti (derle.sh yöntemi, `tools/olcum/mp3/` `derleme/` hariç) öğleden sonra yeniden hesaplandı: `769a005f…d1d8e2`, derleme özetindekiyle aynı. Kampanyada derleme ve yükleme yapılmadı; her koşudan önce ve sonra `kos.sh` kartın `KIMLIK` yanıtını yerel derleme kimliğiyle karşılaştırdı.
- Makine Apple M3 Pro, macOS 26.5.2 (25F84); ST-LINK `STLINK_V3`, seri no `001D000E4D4B500620373831`, seri uç `/dev/cu.usbmodem11403`. Koşular `kos.sh` içindeki `caffeinate -dims` altında, sırayla, tek süreçle yürütüldü.
- Her koşunun ham dizini `${IDA_CACHE}/mp3/kos/<damga>-<ad>/` (kayıt, `ortam.txt`, `cikti.txt`); yürütücü çıktısı, ikinci çözümleme ve B denetimi `${IDA_CACHE}/mp3/kampanya-20261119/<n>-*.txt` içindedir.

### Akış adayları (256 B, yön başına %80, 600 s)

Komut biçimi (her aday için yalnız baud değişir):

```sh
tools/olcum/mp3/kos.sh kos kmp-akis256-<baud> --mod akis --baud <baud> --yuk 256 --sure 600 --en-az-sn 600 --kampanya
tools/olcum/mp3/derleme/pc/mp3_vcp analiz <kayıt> --en-az-sn 600 --kampanya      # ikinci çözümleme
python3 -I ${IDA_CACHE}/mp3/kampanya-20261119/b-denetim.py <kayıt> akis <baud>     # B alanları, sabit beklenti
```

İkinci çözümleme ve B denetimi her koşudan hemen sonra bir sarmalayıcıyla (oturum karalama dizininde; depoya girmedi) yürütüldü; çıktıları 3b–8 için `kampanya-20261119/<n>-*.txt` dosyalarının sonundadır. Sabah koşularının (1–3) ikinci çözümlemesi ve B denetimi öğleden sonra elle yinelendi (çıktısı dosyaya yazılmadı; çözümleyici çıkış kodları 0, 0, 2 ve hükümler GECTI, GECTI, GECERSIZ, ilk hükümle aynı). B denetimi, önceki yürütücünün yazdığı `b-denetim.py` ile yapıldı (çözümleyiciden bağımsız: satır CRC-32 ve numaraları, B/S yeri, profil/mod/baud/yük/hız/oran/aralık/adet, echo'da `azami_bekleyen=64` ve `zaman_asimi_ms=1000`, 600 s'yi kapsayan en küçük adet); dokuz kaydın dokuzunda TAMAM.

| # | Aday | BRR'den baud (hesap) | Ham dizin (`kos/`) | Hüküm | Çerçeve (her yön) | PC takvim azami / yazma azami (µs) | Kart takvim azami | Kart alım halkası azami | Kayıt SHA-256 |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 921 600 | 919 540 | `20261119-091812-kmp-akis256-921600` | **GECTI** | 159 920 | 4 408,667 / 154,083 | 13 µs | 8/16 384 B | `042626559a15ab5a603619017d6b7b346fa5496757d301e29bbb229ac03fe089` |
| 2 | 2 000 000 | 2 000 000 | `20261119-092841-kmp-akis256-2000000` | **GECTI** | 347 827 | 1 143,291 / 194,625 | 54 µs | 16/16 384 B | `b3a3dd1b724f81b568e59f636d12e6a4e3c3bc1ea2f9e634aa2f585b5d29bed0` |
| 3 | 3 000 000 | 3 018 868 | `20261119-093853-kmp-akis256-3000000` | **GECERSIZ (kopma)** | — (kesildi) | — | — | — | `a7c1aac3f3d234671ca8b7fc5bfd748b5df86240b50797d989dac248a3b6087f` |
| 3b | 3 000 000 | 3 018 868 | `20261119-191457-kmp-akis256-3000000-y` | **GECTI** | 521 740 | 7 437,834 / 3 295,875 | 54 µs | 30/16 384 B | `fa5d1c1a464b8f5ae04b51536682c47868e1504fa6421869ad34bcc3b5a22b0b` |
| 4 | 6 000 000 | 5 925 926 | `20261119-192506-kmp-akis256-6000000` | **GECMEDI** | 1 030 595 | **18 116,452** / 11 308,250 | 54 µs | 63/16 384 B | `d61995d4c5a19a2b21af67d771e798e001f94748ce83003359b9fcbc2abbbbb5` |

- **Geçen koşular (1, 2, 3b):** iki uçta veri/CRC/sıra/UART/DMA/taşma sayaçları 0; çerçeve ve tel baytı sayaçları iki uçta eşleşti; plan, son yazma (t0 + 599,996–599,9999 s) ve kart süresi (600,001–600,006 s) 600 s ölçütünü karşıladı; denetim hızına dönüş doğrulandı. Yön başına ortalama trafik 73 564 / 160 000 / 240 000 B/s (%80 profil).
- **6 M (4):** veri tarafı temiz (1 030 595 çerçeve her yönde, bütün hata sayaçları 0, sayaçlar eşleşti), ama azami PC takvim gecikmesi 18 116,452 µs oldu; dondurulmuş sınır 10 ms. Gönderim aralığı 582 188 ns olduğundan bu gecikmenin ardından ardışık en az 14 istek 10 ms sınırının üzerinde kaldı (18 116 452 − 13 × 582 188 = 10 548 008 ns); son yazma yeniden plana yetişti. Kesin sayı akış kaydında tutulmadığı için yeniden sayılamaz. Hüküm GECMEDI; yinelenmedi (ölçüt ihlali geçerli sonuçtur). Bu koşunun 3. dakikasından itibaren makinede başka bir oturumun işlemci yoğun süreçleri koşuyordu (aşağıda "Host yükü"); gecikmenin nedeni ayrılmadı.
- **3 M (3b):** PC takvim azami 7 437,834 µs ile sınırın altında kaldı ama pay dar; yazma çağrısı azami 3 295,875 µs.
- **Kısa/uzun ayrımı:** ön taramada (10 s) dört aday da geçmişti; uzun koşuda 6 M takvimden geçmedi. 12 M ön taramada geçmediği için uzun aday değildi; kısa sonucu aşağıdaki geçmişte korunur.

#### Kopma olayı (koşu 3)

- 2026-11-19 09:38:53 (yerel) başlayan 3 M koşusunda, plan zamanına göre t0'dan yaklaşık 80 s sonra `/dev/cu.usbmodem11403` kayboldu; araç `asama=kopma`, `kopma=1`, `takvim_kesildi=1` yazdı ve kaydı GECERSIZ kapattı (`kos.sh` çıkış kodu 2). O ana kadar PC 70 116 çerçeve gönderdi, 70 144 çerçeveyi hatasız aldı (CRC/sıra/atılan bayt 0). Kopmadan önce PC yazmaları plandan 1 898 430,667 µs'ye kadar geride kalmıştı (`tx_gec_azami_ns`).
- `kos.sh` sondaki ortam kaydında güç "AC Power"dan "Battery Power"a geçmişti; kart `KIMLIK` sorgusu "port açılamadı" verdi.
- Öğleden sonra kart yeniden bağlıyken `KIMLIK`: aynı imaj, `kosu_sayisi=0`, `rcc_csr=0x0C004400` (sabah `0x14004400`). Kart yeniden başlamış görünüyor; güç/USB kesintisiyle tutarlı. Bayrak anlamı RM0456'dan doğrulanmadı; nedenin kablo, bağlantı noktası ya da güç kaynağı olduğu ayrılmadı. Ölçüt ihlali değildir; yineleme (3b) yeni dizinde bir kez yapıldı ve geçti.

### Echo profilleri (seçilen hız 3 000 000)

Seçim kuralı: uzun akış koşularını geçen en yüksek aday (921 600, 2 M, 3 M geçti; 6 M geçmedi) → **3 000 000** (BRR'den 3 018 868, hesap). Etkin hız = min(istenen, BRR'den) = 3 000 000; dört profilin yön başına yükü %5,60 / %18,40 / %11,20 / %36,80, hiçbiri %100 fiziksel sınırı aşmadığından hesapla geçmeyen profil yok.

```sh
tools/olcum/mp3/kos.sh kos kmp-echo<yük>-<hız>-3000000 --mod echo --baud 3000000 --yuk <yük> --hiz <hız> --sure 600 \
    --azami-bekleyen 64 --zaman-asimi-ms 1000 --en-az-sn 600 --kampanya
tools/olcum/mp3/derleme/pc/mp3_vcp analiz <kayıt> --en-az-sn 600 --kampanya
python3 -I ${IDA_CACHE}/mp3/kampanya-20261119/b-denetim.py <kayıt> echo 3000000 <yük> <hız>
```

`--azami-bekleyen` ve `--zaman-asimi-ms` CLI kullanım satırında listelenmiyor; seçenek adları `pc/mp3_vcp.cpp` çözümleyicisinden doğrulandı ve B satırında `azami_bekleyen=64 zaman_asimi_ms=1000` olarak görüldü.

| # | Profil | Ham dizin (`kos/`) | Hüküm | İstek / yanıt | Zaman aşımı, atlanan | Gözlenen azami bekleyen | PC takvim azami (µs) | Kayıt SHA-256 |
|---|---|---|---|---|---|---|---|---|
| 5 | 64 B, 200/s | `20261119-193517-kmp-echo64-200-3000000` | **GECMEDI** | 120 000 / 120 000 | 0, 0 | 3 | **11 484,917** | `26aeb17dac153437eb4d1f753e5121a114682895b61ff86c153f9fb87626c3bf` |
| 6 | 256 B, 200/s | `20261119-194528-kmp-echo256-200-3000000` | **GECMEDI** | 120 000 / 120 000 | 0, 0 | 3 | **11 839,583** | `969f87013f01ea498e73a9f91907af31f7cec5c0680f815c79ed6f390e3354b6` |
| 7 | 64 B, 400/s | `20261119-195539-kmp-echo64-400-3000000` | **GECMEDI** | 240 000 / 240 000 | 0, 0 | 5 | **10 096,708** | `aa404e05f0c0d1edb3e1302aeca0fda6f9dfd77428f8b69f4963d2a69284c454` |
| 8 | 256 B, 400/s | `20261119-200550-kmp-echo256-400-3000000` | **GECTI** | 240 000 / 240 000 | 0, 0 | 6 | 4 625,458 | `d26d377ca78538f2e637054cecda19e5451d0d5a72df239397aabe5c82b7c243` |

Dört koşuda da: iki uçta veri/CRC/sıra/UART/DMA/taşma sayaçları 0, sayaçlar ve tel baytları eşleşti, kart süresi 600,002–600,006 s, son yazma t0 + 599,995 s'den sonra, denetim hızına dönüş doğrulandı. 5–7'nin tek geçmeme nedeni PC takvim sınırıdır (her istek yazma başlangıcı ≤ plan + 10 ms). R satırlarından bağımsız yeniden hesapla (yazma başlangıcı − plan): sınırı aşan istek sayısı 5'te 2, 6'da 1, 7'de 1 (120 000 / 120 000 / 240 000 istek içinde); azami değerler çözümleyicininkiyle aynı. Koşular yinelenmedi.

**Gidiş-dönüş gecikmesi** (host `steady_clock`, işletim sistemi kuyruğu dahil; µs; n = bütün yanıtlar; gecikme eşiği yok, karşılaştırma yapılmaz):

| Profil | Plandan: p50 / p99 / p99,9 / maks | Yazma başından: p50 / p99 / p99,9 / maks | Yazma çağrısı p99 / maks |
|---|---|---|---|
| 64 B, 200/s (n = 120 000) | 833,000 / 982,750 / 1 100,875 / 12 463,792 | 829,917 / 977,375 / 1 062,459 / 11 837,959 | 37,292 / 3 802,875 |
| 256 B, 200/s (n = 120 000) | 2 188,250 / 2 296,417 / 2 488,000 / 14 073,875 | 2 184,708 / 2 287,834 / 2 404,875 / 12 536,167 | 30,041 / 724,292 |
| 64 B, 400/s (n = 240 000) | 814,541 / 895,791 / 1 114,416 / 12 474,625 | 813,833 / 894,667 / 1 071,458 / 11 198,167 | 11,167 / 5 630,625 |
| 256 B, 400/s (n = 240 000) | 2 165,000 / 2 266,166 / 2 340,833 / 14 508,041 | 2 163,500 / 2 264,292 / 2 334,625 / 10 527,541 | 8,625 / 571,292 |

En büyük değerler (10–14,5 ms) tek tük olaylardır: plandan RTT'si 10 ms'yi aşan yanıt sayısı sırasıyla 4, 4, 4, 2 (R satırlarından sayıldı). Geçen profil 8'de de bu tür bir uç değer var; o koşuda takvim gecikmesi 10 ms'nin altında kaldığı için ölçüt ihlali doğmadı. Uç değerlerin kaynağı (macOS zamanlayıcısı, USB, ST-LINK) ayrılmadı.

### Host yükü (ölçüm ortamı uyarısı)

- Yük günlüğü 19:45:45'te başlar; günlükte aynı makinede bu işle ilgisiz başka bir oturumun iki işlemci yoğun süreci (`sig`, `misc`; her biri bir çekirdeği yaklaşık %95 meşgul) ve kısa süreli bir dosya sistemi taraması (`bfs`, günlükteki en büyük değer %523,0) görülür. Yükün 19:28'de başladığı ve önceki koşuların (1, 2, 3b) etkilenmediği kaydedilmemiş gözlemdir, **doğrulanmadı**. Bu süreçlere dokunulmadı.
- Günlük dönemi içinde kalan koşular: 5, 6, 7, 8 (4 için yük 3. dakikadan itibaren gözlendi, günlük öncesidir). 1, 2 (sabah; yük kaydı yok) ve 3b (19:15–19:25) günlük öncesidir; bu koşular için "etkilenmedi" ancak kaydedilmemiş gözlemdir, doğrulanmadı.
- 19:45:45–20:16 arası 20 s'lik örnekleme `kampanya-20261119/host-yuk-gunlugu.txt` içinde (1 dk yük ortalaması 3,4–5,8; iki süreç her örnekte var). 19:45:45 öncesi örneklenmedi.
- Geçmeyen dört koşunun (4–7) hepsi yalnız PC takvim ölçütünden geçmedi ve hepsi bu yük altında koştu; ancak koşu 8 aynı yük altında geçti, 3b yük yokken 7,4 ms'ye çıktı. Yük ile takvim aşımı arasında nedensellik kurulamaz. Temiz host koşulunda yineleme mimarın kararıdır; bu turda yapılmadı.
- Güç: bütün öğleden sonra koşularında başta ve sonda "AC Power, AC attached; not charging", pil %29 (sabah %45). Uyku engeli `kos.sh` içindeki `caffeinate -dims` ile; termal/performans uyarısı kaydı yok (`pmset -g therm`).

**Mimar notu (2026-11-19).** 256 B 200/s echo koşusunun (`kmp-echo256-200-3000000`) ilk yaklaşık 70 saniyesi, aynı Mac'te başka bir agent'ın 19:45:27'de başlattığı Homebrew kurulumuyla (ağ ve disk yükü) çakışmıştır; bu koşunun takvim aşımı bu yükten etkilenmiş olabilir, nedensellik kurulamaz. Geçmeyen üç echo koşusu ve 6 Mbaud akışı için temiz host koşulunda (başka agent ve ağır süreç yokken) bir kez yineleme mimarca planlanmıştır; yapıldığında sonuçlar bu rapora ek olarak, mevcut hükümler silinmeden yazılır.

### V1–V4 (daraltılmış kapsamla)

- **V1:** derleme kimliği, imaj ve PC aracı SHA-256'ları, kaynak özeti (yeniden hesaplandı, aynı), seri uç, ST-LINK seri no, makine ve güç her koşunun `ortam.txt`'sinde; kart `KIMLIK` tamamlanan sekiz koşuda önce ve sonra eşleşti; kopan koşunun (3 000 000, sabah) son sorgusunda port açılamadı. Karşılandı. Sınırlama: kaynak işlenmemiş (`?? tools/olcum/mp3/`); kimlik commit'e değil dosya özetine bağlı.
- **V2:** 921 600, 2 M ve 3 M (3b) için 600 s, iki uç sayaçları eşleşti, sıfır kayıp. 6 M geçmedi (takvim). 3 M'nin ilk koşusu kopma nedeniyle geçersiz; kayıt silinmedi. Bu "kayıpsız" hükmü yalnız %80 sabit hızlı 256 B akış için geçerlidir; doygunluk kapasitesi iddiası değildir.
- **V3:** dört profilde süre, örnek ve sayaç muhasebesi çözümleyici ve B denetimiyle doğrulandı; ayrıca takvim azamisi R satırlarından bağımsız yeniden hesaplandı. Bir profil geçti, üçü PC takviminden geçmedi; hepsi raporlandı.
- **V4:** bu turda yeni negatif test yapılmadı; düzeltme turu 2'nin 219/219 masaüstü testi (UBSan dahil) geçerli. Sol'un A1 bulgusu kapsam kararı uyarınca açık araç sınırlamasıdır: 64 dışı kuyruk ayarı ve deneysel profiller kabul kapsamı dışında; bu kampanyadaki bütün echo kayıtlarında `azami_bekleyen=64` B denetimiyle doğrulandı. DMA donanım varsayımları V1–V3 (`ortak/mp3_halka.h`) RM0456'dan okunmadı; ölçümün sınırlamasıdır (kartın `belirsiz` sayacı kampanya boyunca 0; `kararsiz` öğleden sonra 1 067 593'e çıktı, yeniden denenen kararsız yazmaç görüntüsü sayısıdır). CRC'nin her donanım hatasını yakalayacağı garantisi yoktur.

### Ham veri ve manifest

- SHA-256 manifesti: `${IDA_CACHE}/mp3/kampanya-20261119/SHA256SUMS.txt` (dokuz koşu dizininin kayıt/`ortam.txt`/`cikti.txt` dosyaları, `kampanya-20261119/` altındaki yürütücü çıktıları, `b-denetim.py`, host yük günlüğü ve `derle-20261119-005548/mp3.{elf,bin}`). Yazıldıktan sonra `shasum -c` ile doğrulandı.
- Yürütücü çıktıları: `1-akis-921600.txt`, `2-akis-2000000.txt`, `3-akis-3000000.txt` (sabah), `3b-akis-3000000-yineleme.txt`, `4-akis-6000000.txt`, `5-echo-3000000-64-200.txt`, `6-echo-3000000-256-200.txt`, `7-echo-3000000-64-400.txt`, `8-echo-3000000-256-400.txt`.

### Doğrulanmayanlar (kampanya)

- 6 M ve echo profillerindeki PC takvim aşımlarının nedeni; host yükünün etkisi ayrılmadı.
- RTT uç değerlerinin (10–14,5 ms) kaynağı.
- Kopmanın nedeni (kablo, bağlantı noktası, güç); `rcc_csr` bayraklarının anlamı.
- Sabah koşuları sırasındaki host yükü (örneklenmedi); 19:45 öncesindeki host yükü (19:28 başlangıcı ve önceki koşuların etkilenmediği kayıtsız gözlem).
- 6 M akışındaki kesin takvim aşımı sayısı (akış kaydında tutulmadı; en az 14).
- ASan/TSan altında koşu ve uzun koşuda ısıl etki: yapılmadı, bkz. aşağıdaki "düzeltme turu 2" ve "düzeltme turu 1" bölümlerinin doğrulanmayanlar listeleri (ASan/TSan bu macOS'ta başlamıyor; ısıl etki ölçülmedi).
- Fiziksel hat hızı; DMA varsayımları V1–V3; Windows (yazılmadı, çalıştırılmadı).
- Bu sonuçlar kontrol hızı, A1 sözleşmesi ya da ürün başarımı hakkında karar değildir.

---

# Geçmiş: düzeltme turu 2 ön taraması (2026-11-19; değiştirilmeden korunur)

## Güncel sonuç (düzeltme turu 2, 2026-11-19)

Kart sonuçları tek bir kısa taramadan gelir: akış 10 s, echo 5 s, her aday bir kez. 600 s kampanya değildir; hiçbir hız için "kayıpsız" ya da "sürdürülebilir" hükmü vermez. Baud/BRR/kapasite nominal 160 MHz'ten hesaptır; fiziksel hat hızı ölçülmedi. Bu turda yöntemde değişenler (ayrıntı `inceleme/MP3-yanit-2.md`):

- **DMA alım konumu (Y1):** kesme sayımı kullanılmaz; HT/TC kesmeleri kapalı. Kararlı yazmaç görüntüsünden (BNDT, FIFOL, BNDT eşit) RAM konumu `(N − BNDT − FIFOL) mod N` alınır; yeniden yükleme öncesi ve sonrası aynı konumu verir. Tur belirsizliği (yoklama boşluğu × hız ya da ilerleme ≥ N/2) hata sayılır ve veri atılır. Tekdüzelik zorlaması yok. Doğrulanmamış donanım varsayımları `ortak/mp3_halka.h` içinde V1–V3 olarak yazılı.
- **Kayıt alanları (Y2):** B, P ve K alanları temsil aralıklarıyla (KUR/araç sınırları, INT64_MAX, u32) sınanır; bütün aritmetik taşma denetimlidir.
- **Akış kronolojisi (Y3):**
  - PC: son plan ≤ son yazma sonu ≤ son plan + azami gecikme + azami yazma süresi.
  - Kart: `plan_us = ⌊(n−1)·aralık/1000⌋ ≤ tx_son_us ≤ plan_us + tx_gec_azami_us` ve `tx_son_us ≤ sure_us`. Kart son gönderimi gecikmeyle aynı damgadan yazar; yuvarlama payı sıfır.
  - İmkânsız kronoloji GECERSIZ.
- **Echo kuyruğu (Y4):** doluluk ekleme SONRASI ölçülür (sınır 64). Çözümleyici R'den aynı tanımla yeniden üretir; P değeri yeniden üretilenden küçük ya da sınırdan büyükse GECERSIZ.

**Derleme ve testler:**
- `$IDA_CACHE/mp3/derle-20261119-005548/`, kimlik `20261119-005548-mp3-43f9c836`.
- `mp3.elf` SHA-256 `43f48ae2b89f9983b1ad59676ae21cd638f67b08aaca31d642e6305a1c345b13`; `mp3_vcp` SHA-256 `477a92ee8c8ae107e8fd2fef6825d002866399f0c0f7e36efdaf9a8f0bd40598`.
- text 23 248 B, bss 37 616 B.
- Masaüstü testleri 219/0, UBSan 219/0.

**Kart:**
- Yedek geçerli.
- Önceki imaj: MP3 `20261118-192703-mp3-ca321d0d` (`$IDA_CACHE/mp3/onceki-imaj-20261119-005404/`; okumada görünen MP4 kimliği silinmemiş sektörlerde kalan artık bayttır).
- Yükleme yalnız dahili flash'a, doğrulamalı.
- Tarama `$IDA_CACHE/mp3/kos/20261119-005626-tara/` (`kos.sh` çıkış kodu 1). Makine Apple M3 Pro, macOS 26.5.2; AC güç; uyku engeli altında.

| Aday | BRR'den baud (hesap) | Akış 256 B %80 | Echo 64 B 200/s: RTT plandan p50 / p99 / p99,9 / maks (µs), n = 1000 |
|---|---|---|---|
| 921 600 | 919 540 | GECTI | GECTI: 2 135,916 / 2 293,041 / 2 419,958 / 2 547,333 |
| 2 000 000 | 2 000 000 | GECTI | GECTI: 1 131,666 / 1 303,250 / 1 416,916 / 1 484,083 |
| 3 000 000 | 3 018 868 | GECTI | GECTI: 886,375 / 958,083 / 1 004,000 / 1 048,417 |
| 6 000 000 | 5 925 926 | GECTI | GECTI: 569,042 / 750,292 / 817,084 / 818,792 |
| 12 000 000 | 12 307 692 | **GECMEDI** | GECTI: 469,958 / 598,333 / 646,125 / 678,416 |

- **Geçen akış koşuları:** iki uçta veri/CRC/sıra/UART/DMA/taşma sayaçları 0, sayaçlar ve baytlar eşleşti. PC takvim gecikmesi azami 14,542 µs; yazma çağrısı azami 152,250 µs; kart gecikmesi azami 53 µs; kart alım halkası azami 49/16 384 B.
- **Echo koşuları:** zaman aşımı ve atlanan 0; gözlenen azami bekleyen 1 (RTT istek aralığından kısa). PC takvim gecikmesi azami 327,458 µs (921 600).
- **12 Mbaud akış:**
  - Kart PC'nin 34 783 çerçevesini hatasız aldı.
  - PC, kartın gönderdiği 34 783 çerçeveden 34 782'sini aldı: bir CRC hatası, 275 atılan bayt.
  - PC yazmaları plandan 745 946,791 µs'ye kadar geride kaldı; son yazma t0 + 10,717 s'de bitti, sınır 10,100 s.
  - Tek ve kısa koşudur; nedeni (ST-LINK, USB, macOS sürücüsü, +2,56 % BRR hatası) ayrılmadı.
- **Konum muhasebesi:** kart 10 koşu boyunca 31 519 kararsız görüntüyü yeniden denedi; tur belirsizliği 0 (`KIMLIK`: `kararsiz=31519 belirsiz=0`).

**Ara tarama ve eski sonuçlar:**
- Aynı turda ara imajla (`…005255…`; `KIMLIK` 256 B'de kesildi) yapılan tarama `$IDA_CACHE/mp3/kos/20261119-005413-tara/` içinde korunur. 12 M akış orada yalnız takvimden (736 924 µs) GECMEDI; diğer dokuz koşu GECTI.
- 1. turun taraması (`20261118-192402-tara`) yeni çözümleyiciyle bütün kayıtlarda GECERSIZ, çünkü eski firmware ve eski kuyruk ölçümü yeni tanımlara uymuyor. Aşağıdaki geçmişte yer alır; yeni sonuç sayılmaz.

**Doğrulanmayanlar:**
- DMA varsayımları V1–V3 RM0456'dan okunmadı.
- 12 M hatasının nedeni ayrılmadı.
- ASan/TSan bu macOS'ta başlamıyor.
- Windows yazılmadı.
- 600 s kampanya yapılmadı.
- Takvim toleransları ve profil sınırları mimar seçimidir.

**Sıradaki adım (öneri):** sol'un bu tur incelemesi; ardından mimar kararıyla kampanya. Ön taramada iki profilde de geçen adaylar 921 600, 2 M, 3 M ve 6 M.

---

# Geçmiş: düzeltme turu 1 (2026-11-18; değiştirilmeden korunur)

> Not (turu 2): Bu bölümdeki kart taraması (`20261118-192402-tara`) turu 2 çözümleyicisiyle GECERSIZ'dir (Y3/Y4 tanımları). DMA konum yöntemi de turu 2'de değişti.

Bu raporun kart sonuçları tek bir kısa taramadan gelir: akış 10 s, echo 5 s, her aday için bir kez. V2/V3'ün istediği 600 s koşular değildir; hiçbir hız için "kayıpsız" ya da "sürdürülebilir" hükmü vermez. Baud, BRR ve kapasite değerleri nominal 160 MHz'ten hesaptır; fiziksel hat hızı ölçülmedi.

### 1. Yöntem (güncel)

Kod `tools/olcum/mp3/`. İlk teslimdeki yapı korunur (aşağıda "Geçmiş", 1. Yapılanlar). Bu turda değişen ya da eklenenler:

- **Baud:** BRR önce kodlanır, etkin hız yazılan BRR'den ST `LL_USART_GetBaudRate` çözümüyle geri hesaplanır (`mp3_brr_baud`). 160 MHz'te OVER8 yalnız 12 Mbaud'da seçilir.

  | İstenen | Kip | BRR | BRR'den baud | Hata (hesap) |
  |---|---|---|---|---|
  | 921 600 | OVER16 | 174 | 919 540 | −2 235 ppm |
  | 2 000 000 | OVER16 | 80 | 2 000 000 | 0 |
  | 3 000 000 | OVER16 | 53 | 3 018 868 | +6 289 ppm |
  | 6 000 000 | OVER16 | 27 | 5 925 926 | −12 345 ppm |
  | 12 000 000 | OVER8 | 0x15 (D = 26) | 12 307 692 | +25 641 ppm |

- **DMA alımı:** konum (yarım-tur sayacı, BNDT, FIFOL) üçlüsünden hesaplanır (`ortak/mp3_halka.c`). Yalnız kanal FIFO'sundan RAM'e yazılmış olduğu kesin baytlar tüketilir (alt sınır). Üzerine yazma denetimi kaynak ilerlemesiyle yapılır (üst sınır), kopyadan önce ve sonra.
- **Profiller:** `mp3-akis-v1` (256 B, yön başına 8N1 kapasitesinin %80'i, etkin hız = min(istenen, BRR'den)); `mp3-echo-v1` (64/256 B, 200/400 istek/s, fiziksel sınır %100); diğerleri `deneysel`. %80 sabittir; CLI daha yükseğini reddeder. Profil kayda yazılır ve çözümlemede ayarlardan yeniden üretilir. `--kampanya` deneysel profili GECMEDI sayar.
- **Takvim toleransı (mimar seçimi; ölçüm geçerliliği için, kontrol gereği değil):**
  - PC'de her yazma başlangıcı ≤ plan + 10 ms; son yazma tamamlanması ≤ pencere (adet × aralık) + max(10 ms, pencerenin %1'i).
  - Kartta akış gönderimi için aynı iki sınır; ölçülen an, çerçevenin gönderim halkasına konduğu andır.
  - Aşımda GECMEDI. Takvim kayıttan yeniden üretilemezse GECERSIZ. Planlanan ve gerçekleşen trafik ayrı raporlanır.
- **Sayaç kapsamı:**
  - Kart penceresi: başlatan ESLE'den sonra, ilk BITIR dahil.
  - PC penceresi: ilk ESLE_YANIT'tan sonra, ilk OZET dahil; OZET anında dondurulur.
  - Denetim çerçeveleri ayrı sayılır (`rx_kontrol`, `rx_kontrol_bayt`).
  - PC'de `read()` toplamı = ESLE öncesi + pencere + OZET sonrası.
  - Çözümleyici çerçeve ve bayt muhasebesini iki uçta ve uçlar arasında denetler.
- **Echo kuyruğu:** yalnız gönderilmiş, yanıtsız ve zaman aşımına uğramamış istekler (sınır 64). Zaman aşımı yazma sonundan 1 s; geç yanıt bir kez sayılır.
- **Kayıt çapraz denetimleri:** R plan = sıra × aralık; R'den çıkan bütün azami/son değerler P'yle aynı olmalı. Hız kabulü, OS sonucu ve geri okuması, BRR/ppm zorunlu.
- **PC IO:** yazma süre sınırı her denemeden önce denetlenir. CLI sayıları katı çözülür.
- **Betikler:** `derle.sh` derleme hatası, uyarı ya da başarısız testte sıfır dışı çıkar. `kos.sh` araç çıkış kodunu boru hattısız alır. `tara` çıkış kodu: 0 hepsi geçti, 1 tamamlandı ama aday geçmedi, 2 geçersiz koşu, 4 kullanım.

### 2. Gerçek komutlar (bu tur)

```sh
tools/olcum/mp3/derle.sh                         # temiz derleme + masaüstü testleri
tools/olcum/mp1/yedek-dogrula.sh
tools/olcum/mp3/onceki-imaj.sh
tools/olcum/mp3/yukle.sh
tools/olcum/mp3/kos.sh kimlik
tools/olcum/mp3/kos.sh kos deneme-akis-921600 --mod akis --baud 921600 --yuk 256 --sure 3
tools/olcum/mp3/kos.sh tara                      # varsayılan: 5 aday, akış 10 s, echo 5 s, profil zorunlu
tools/olcum/mp3/derleme/pc/mp3_vcp analiz <kayıt> --kampanya
cmake --preset mp3-pc-ubsan && cmake --build --preset mp3-pc-ubsan && tools/olcum/mp3/derleme/pc-ubsan/mp3_test
```

Çalıştırılmayan: `kampanya.sh` (600 s kampanya).

### 3. Sonuçlar

#### 3.1 Derleme ve masaüstü testleri

| Kalem | Değer |
|---|---|
| Son derleme | `$IDA_CACHE/mp3/derle-20261118-192703/`, kimlik `20261118-192703-mp3-ca321d0d`, kaynak `b1a56e4` + işlenmemiş `tools/olcum/mp3/` |
| `mp3.elf` SHA-256 | `c42503f6acb21d6de468cee8ba0d99114c00d5236f0abb25547df859531de82e` |
| `mp3_vcp` SHA-256 | `8c0fa4b691f2e1a4c487d233449aa8b04bd1e00662611e960dfdb27d014af94a` (taramadaki derlemeyle aynı) |
| Boyut | text 22 980, data 12, bss 37 616 |
| Uyarı | yok (newlib bağlayıcı uyarıları hariç) |
| Masaüstü testleri | **163 denetim, 0 başarısız** (`derle.sh` içinde); UBSan derlemesi 163/0, çalışma zamanı hatası yok |

Test kapsamı (ayrıntı `inceleme/MP3-yanit-1.md`):
- **Birim:** CRC, ayrıştırıcı ve yeniden eşleme, sıra izleyici; el hesabı beklentilerle baud/BRR; DMA alım modeli (iki HT çeşitlemesi, yaklaşık 80 000 okuma, taşma yakalama); echo kuyruğu (sol dizisi); katı seçenek çözümü; yazma süre sınırı.
- **Uçtan uca** (socketpair + kart mantığı öykünücüsü):
  - temiz ve kısmi IO'lu koşular GECTI;
  - bayt bozma/düşürme, ORE, taşma, kapasite ve yavaş gönderici (takvim) GECMEDI;
  - kopma, susma ve tıkanma GECERSIZ;
  - hız reddi ve eşleme yok BASLAMADI;
  - profil/kampanya ayrımı; tarama çıkış kodları 0 ve 1.
- **Kayıt negatif (V4):** sol'un karşı örnekleri dahil; hiçbiri yanlış GECTI üretmedi.

Öykünücüdeki süreler kart ölçümü değildir.

#### 3.2 Kart

- **Yükleme öncesi:** yedek geçerli. Kartta MP4 imajı vardı (kimlik `20261117-185727-og-200-5fe214ad`, ilk 256 KB SHA-256 `e80fc091561b1905aa0736e335df15037037e68f0027359df20f4ba4abe03513`; `$IDA_CACHE/mp3/onceki-imaj-20261118-192258/`).
- **Yükleme:** yalnız dahili flash, doğrulamalı.
- **KIMLIK:** saat 160 MHz, USART saati 160 MHz (`CCIPR1 = 0x00000000`, PCLK2), `RCC_CSR 0x14004400`, `IDCODE 0x30006481`.
- **Deneme koşusu** (imaj `…192227…`, OS geri okuma denetimi eklenmeden önce): 921 600 akış 3 s GECTI (`$IDA_CACHE/mp3/kos/20261118-192316-deneme-akis-921600/`).

**Ön tarama** (imaj `20261118-192327-mp3-782a472b`, aygıt yazılımı kaynağı son derlemeyle aynı; `$IDA_CACHE/mp3/kos/20261118-192402-tara/`; `kos.sh` çıkış kodu 1). Makine Apple M3 Pro, macOS 26.5.2, AC güç; uyku engeli altında; ST-LINK `STLINK_V3`.

| Aday | İşletim sistemi | Akış 256 B %80, 10 s | Echo 64 B 200/s, 5 s |
|---|---|---|---|
| 921 600 | istenen = geri okunan | GECTI: 2 666 + 2 666 çerçeve | GECTI |
| 2 000 000 | aynı | GECTI: 5 798 + 5 798 | GECTI |
| 3 000 000 | aynı | GECTI: 8 696 + 8 696 | GECTI |
| 6 000 000 | aynı | GECTI: 17 177 + 17 177 | GECTI |
| 12 000 000 | aynı | **GECMEDI** | GECTI |

Geçen akış koşularında iki uçta da veri/CRC/sıra/UART/DMA/taşma sayaçları 0 ve sayaçlar eşleşti. Bu turda kaydedilen en büyük değerler:
- PC takvim gecikmesi azami 2 685,583 µs (2 Mbaud), yazma çağrısı azami 3 492,833 µs (2 Mbaud);
- kart takvim gecikmesi azami 51 µs;
- kart alım halkası azami doluluk 50/16 384 B (6 Mbaud).

**12 Mbaud akış:** PC 34 783 çerçeve gönderdi, kart hepsini hatasız aldı (kart tarafında UART/DMA/taşma/CRC 0). Kart 34 783 çerçeve gönderdi, PC 34 782'sini aldı: bir CRC hatası, 275 atılan bayt, bir eksik sıra. Ayrıca PC yazmaları plandan 776 967,709 µs'ye kadar geride kaldı ve son yazma t0 + 10,731 s'de bitti; sınır pencere + pay = 10,100 s. Hüküm GECMEDI.

Tek, kısa koşudur. Kayıp ve gecikmenin nedeni ayrılmadı: ST-LINK, USB, macOS sürücüsü ya da +2,56 %'lik BRR hatası. Her biri doğrulanmadı.

**Echo RTT** (host monotonik saati, işletim sistemi kuyruğu dahil; n = 1000; µs; zaman aşımı ve atlanan 0):

| Aday | Plandan p50 / p99 / p99,9 / maks | Yazma başından p50 / p99 / p99,9 / maks |
|---|---|---|
| 921 600 | 2 124,459 / 2 175,000 / 2 189,125 / 2 205,625 | 2 123,708 / 2 174,166 / 2 185,458 / 2 204,833 |
| 2 000 000 | 1 164,459 / 1 213,209 / 1 313,167 / 1 555,625 | 1 160,917 / 1 208,375 / 1 306,167 / 1 550,583 |
| 3 000 000 | 881,333 / 970,625 / 1 056,000 / 1 643,500 | 880,292 / 969,500 / 1 055,042 / 1 598,875 |
| 6 000 000 | 560,417 / 703,250 / 707,375 / 732,792 | 558,583 / 699,542 / 706,125 / 729,833 |
| 12 000 000 | 398,166 / 550,041 / 809,666 / 1 840,291 | 396,667 / 543,084 / 699,417 / 802,250 |

n = 1000'de p99,9 = 999'uncu, maksimum 1000'inci örnektir; kuyruk istatistiği zayıftır. Bu değerler gecikme eşiğiyle karşılaştırılmaz (eşik yok); 600 s profilin sonucu değildir.

### 4. Doğrulanmayanlar ve sınırlamalar

- **600 s kampanya (V2, V3):** yapılmadı; "kayıpsız en yüksek baud" henüz yok. Ön tarama tek ve kısa.
- **12 Mbaud akış hatası:** nedeni ayrılmadı. Hesaplanan BRR hatası +2,56 %; ST-LINK tarafının gerçek hızı bilinmiyor.
- **GPDMA:** kanal 0 FIFO derinliği ve HT olayının kaynağa mı hedefe mi bağlı olduğu RM0456'dan okunmadı (model iki çeşitlemeyi sınıyor). İlk bloğun yazmaçtan yüklenmesi kartta çalıştı (taramada DMA hatası 0), ama ayrı bir donanım testi yok.
- **ASan/TSan:** bu macOS 26.5.2 + Apple clang 17'de başlamıyor; yalnız UBSan koştu. İş parçacığı yarışsızlığı araçla doğrulanmadı.
- **Windows:** seri port yazılmadı, çalıştırılmadı.
- **Takvim:** PC takviminin son 3 ms'si meşgul beklemedir; uzun koşuda işlemci ve ısıl etki ölçülmedi.
- **Tolerans ve profil sınırları** mimar seçimidir; ölçüm sonucu ya da kullanıcı kararı değildir.

### 5. Ham veri

| Ne | Nerede |
|---|---|
| Derlemeler | `${IDA_CACHE}/mp3/derle-20261118-192227/`, `…-192327/`, `…-192703/` (ilk teslimdeki `…-185701/` korunur) |
| Önceki imaj | `${IDA_CACHE}/mp3/onceki-imaj-20261118-192258/` (ilk teslimdeki başarısız okuma `…-185645/`) |
| Deneme koşusu | `${IDA_CACHE}/mp3/kos/20261118-192316-deneme-akis-921600/` |
| Ön tarama | `${IDA_CACHE}/mp3/kos/20261118-192402-tara/` (10 kayıt, `ortam.txt`, `cikti.txt`) |

### 6. Sıradaki adım (öneri)

1. Bu tur için sol ikinci incelemesi.
2. Mimar onayıyla `kampanya.sh <en_yüksek_baud> <baudlar>`. Ön tarama ile adaylar 921 600, 2 M, 3 M, 6 M; 12 M akışta geçmedi.

---

# Geçmiş: ilk teslim (2026-11-18, düzeltme turu 1'den önce; değiştirilmeden korunur)

> Not (düzeltme turu 1): Aşağıdaki §1.2 baud tablosu yanlıştır. OVER8'de yazılamayan bölücü biti hesaba katılmamıştı (sol bulgu 1). Doğru değerler yukarıdaki §1'dedir. Bu metindeki "kart bağlı değildi" durumu ilk teslim anına aittir.


| | |
|---|---|
| Görev | `docs/gorevler/W0-MP3a.md` (W0, ölçüm prototipi; atılabilir, ürün protokolü değil) |
| Ölçüm | M3 (ST-LINK VCP gerçek hız, kayıp, gidiş-dönüş gecikmesi); K17 |
| Uygulayan | `ida-uygulayici` (Opus) |
| Dal | `olcum/mp3-vcp` (commit yok) |
| Tarih | 2026-11-18 |
| Durum | **İlk teslim; kartta hiçbir ölçüm yapılmadı.** Kart denemeleri sırasında kart Mac'e bağlı değildi (§5). Uzun kampanya başlatılmadı. Mimar sol ön incelemesini başlattı; inceleme sonucu gelmeden kod değiştirilmedi. |

**Bu raporda kart üzerinde ölçülmüş hiçbir hız, kayıp ya da gecikme değeri yoktur.** Aşağıdaki baud, BRR ve kapasite rakamları hesaptır; masaüstü testlerindeki süreler kart değil, PC içi öykünücü süreleridir.

### 1. Yapılanlar

Kod `tools/olcum/mp3/` altında: kart C17/gnu17, PC C++20, ortak çerçeve ve kart test mantığı C (PC'de C olarak derlenir).

| Dosya | İçerik |
|---|---|
| `ortak/mp3_cerceve.{h,c}` | Genel test çerçevesi (sürüm, tür, koşu kimliği, sıra, uzunluk, başlık CRC-16, CRC-32), belirlenimci veri üreteci, akış ayrıştırıcı, sıra izleyici (eksik/tekrar/sırasız/eski), OZET alan listesi, BRR hesabı |
| `ortak/mp3_kart.{h,c}` | Kartın test mantığı, donanımdan bağımsız: denetim hızında KIMLIK/KUR, hız geçişi, ESLE, akış/echo, BITIR→OZET, DONUS; zaman aşımları |
| `fw/platform.c`, `fw/kesmeler.c` | STM32U5A9J-DK platformu: 160 MHz saat (MP1/MP4 dizisi), USART1 + GPDMA (alım dairesel, gönderim DMA), TIM2 1 MHz zaman, hata sayaçları |
| `pc/seri*.{hpp,cpp}` | POSIX seri port (macOS `IOSSIOSPEED`), kısmi okuma/yazma, zaman aşımı, kopma |
| `pc/oturum.{hpp,cpp}` | PC koşu akışı (okuyucu iş parçacığı, sabit takvim, sınırlı boşaltma, yeniden eşleme) |
| `pc/kayit.{hpp,cpp}` | Satır başına CRC'li koşu kaydı ve tek hüküm kaynağı olan çözümleyici |
| `pc/mp3_vcp.cpp` | `kimlik`, `kos`, `tara`, `analiz` komutları |
| `test/mp3_test.cpp`, `test/sahte_hat.{hpp,cpp}` | Masaüstü testleri: socketpair üzerinde gerçek PosixPort okuma/yazma kodu ve karşı uçta kartın test mantığını koşan öykünücü (hata enjeksiyonlu) |
| `derle.sh`, `yukle.sh`, `onceki-imaj.sh`, `kos.sh`, `kampanya.sh`, `ortam.sh` | Temiz derleme ve kayıt; yedek doğrulamalı yükleme; yükleme öncesi imaj kimliği; uyku engelli koşu sarmalayıcısı; dondurulmuş kampanya (çalıştırılmadı) |

#### 1.1 Çerçeve

`A5 5A | sürüm u8 | tür u8 | koşu u32 | sıra u32 | uzunluk u16 | başlık CRC-16/CCITT-FALSE u16 | yük | CRC-32/ISO-HDLC u32`, küçük uçlu; ek yük 20 B, 256 B yükte 276 B. Başlık CRC'si bozuk uzunluk alanının ayrıştırıcıyı bekletmesini önler. CRC-32 tutmazsa yalnız bir bayt atılıp yeniden aranır; böylece kesik bir çerçevenin iddia ettiği uzunluğa düşen sağlam çerçeveler kaybolmaz (birim testi). Veri yükü koşu, tür ve sıradan xorshift32 ile üretilir; alıcı aynı üreteçle karşılaştırır (CRC'yi geçen ama yanlış veri `icerik_hata` sayılır). Yük üst sınırı 256 B.

#### 1.2 Kart

- **USART1:** PA9 TX / PA10 RX, AF7 (yerel BSP `stm32u5x9j_discovery.h`, COM1). Çekirdek saati `HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_USART1)` ile yazmaçtan; `KIMLIK` yanıtı `usart_hz` ve `CCIPR1`'i bildirir. 8N1, FIFO açık, akış denetimi yok, `DDRE = 0`, `OVRDIS = 0`, `EIE` ile ORE/FE/NE (ve PE) sayılıp temizlenir; alım durmaz. GPIO hızı `HIGH` (12 Mbaud'da bit 83 ns; hesap).
- **BRR:** HAL `UART_DIV_SAMPLING16/8` yuvarlaması ve `UART_BRR_MIN/MAX` (16/0xFFFF) sınırlarıyla (yerel `stm32u5xx_hal_uart.h`). OVER16 ve OVER8'den istenen hıza en yakın olan seçilir (eşitlikte OVER16). Seçim, BRR ve hesaplanan baud `KUR_YANIT` ile bildirilir ve kayda girer; PC aynı işlevle önceden hesaplar, tutmazsa koşu başlamaz. Hız hiçbir yerde sessizce değiştirilmez.

  | İstenen | Kip | BRR | Hesaplanan baud | Hata |
  |---|---|---|---|---|
  | 115 200 (denetim) | OVER16 | 1389 | 115 191 | −78 ppm |
  | 921 600 | OVER8 | 341 (USARTDIV 347) | 922 190 | +640 ppm |
  | 2 000 000 | OVER16 | 80 | 2 000 000 | 0 |
  | 3 000 000 | OVER8 | USARTDIV 107 | 2 990 654 | −3 115 ppm |
  | 6 000 000 | OVER8 | USARTDIV 53 | 6 037 736 | +6 289 ppm |
  | 12 000 000 | OVER8 (OVER16 sınır dışı) | USARTDIV 27 | 11 851 852 | −12 346 ppm |

  Hepsi 160 MHz USART saati varsayımıyla **hesaptır** (birim testi 115 200, 921 600, 2 M ve 12 M satırlarını denetler; 3 M ve 6 M satırları aynı formülle elde hesaplandı). ST-LINK tarafının gerçek baud'u bilinmiyor; fiziksel hat hızı ölçülmedi.
- **Alım:** GPDMA1 kanal 0, istek 24 (`GPDMA1_REQUEST_USART1_RX`, yerel `stm32u5xx_hal_dma.h`), 16 KB halkaya dairesel. Dairesellik, HAL'in `DMA_LINKEDLIST_CIRCULAR` düzenindeki gibi kendine bağlanan tek düğümlü bağlı listeyle (düğüm sırası CTR1, CTR2, CBR1, CSAR, CDAR, CLLR; maske UT1|UT2|UB1|USA|UDA|ULL; yerel `stm32u5xx_hal_dma_ex.c` `DMA_List_GetCLLRNodeInfo`). HAL'den farkı: ilk blok düğümden değil kanal yazmaçlarından yüklenir. Yazılan toplam bayt, PRIMASK altında okunan (yarım-tur sayacı, BNDT) çiftinden hesaplanır; okuyucu halkadan 16 KB'den fazla geride kalırsa `rx_tasma` sayılır ve bekleyen baytlar atılır; kopyadan sonra yeniden denetlenir (kopya sırasında üzerine yazılma da taşma sayılır). Bir kesme girişinde HT ve TC birlikte bekliyorsa `dma_rx_hata` sayılır. Sessiz tampon kaybı başarı sayılmaz.
- **Gönderim:** GPDMA1 kanal 1, istek 25, `DREQ = 1`; 16 KB halkadan bitişik parça tek blok; TC kesmesi sonrakini başlatır; DMA hatası sayılır.
- **Durumlar:** KONTROL (115 200) → KUR kabulünde gönderim boşalınca yeni hız → ESLEME (`esle_ms`, varsayılan 5 s; dolarsa denetim hızına dönüş) → ESLE gelince koşu sayaçları sıfırlanır (hız geçişindeki çöp sayılmaz) → KOSU (akış: sabit aralıkla `VERI_KP`; echo: her geçerli `ECHO_ISTEK`'e aynı sıra ve yükle yanıt; `bos_ms` sessizlikte kesilir) → BITIR'de sayaçlar dondurulur, OZET → DONUS ya da 3 s sessizlikte denetim hızı.

#### 1.3 PC

- Seri port: `O_NONBLOCK` + `poll`; `yaz` kısmi yazımları birleştirir, süre sınırlı (varsayılan 1 s); `oku` en az 1 bayt ya da zaman aşımı; `POLLHUP/POLLERR/POLLNVAL`, hata dönüşü ya da "`poll` okunur dedi ama `read` 0 döndü" kopma sayılır. `TIOCEXCL` ile özel açılır. macOS'ta hız `tcsetattr` ardından `IOSSIOSPEED` ile verilir; istenen değer, çağrıların sonucu, `errno` ve `tcgetattr` sonrası `cfgetispeed/cfgetospeed` geri okuması kaydedilir. Bu geri okuma işletim sisteminin bildirdiğidir; fiziksel hız değildir.
- Koşu: denetim hızında `KIMLIK` → `KUR` (3 deneme × 500 ms) → işletim sisteminde yeni hız → ESLE (200 ms arayla, en çok 3 s) → t0 → sabit takvimli trafik → sınırlı boşaltma → `BITIR` (3 × 1 s) → `OZET` → `DONUS` → denetim hızında `KIMLIK` ile yeniden eşleme (en çok 6 s). Hiçbir bekleme sınırsız değildir.
- Takvim: hedefin 3 ms öncesine kadar uyunur, kalanı meşgul beklenir; iş parçacıkları `QOS_CLASS_USER_INTERACTIVE`. Neden: masaüstü testinde `sleep_until` yaklaşık 2,5 ms'ye varan geç uyanma gösterdi (öykünücüyle gözlem; kart ölçümü değil). Kalan takvim gecikmesi her koşuda ölçülür ve kaydedilir.
- Akış hızı: yön başına aralık = ⌈(yük + 20) × 10 × 10⁹ / (0,80 × min(istenen baud, kartın hesapladığı baud))⌉ ns; çerçeve sayısı süreyi kapsayacak biçimde yukarı yuvarlanır. İki yön ayrı sayılır; iki yön toplamı tek yön kapasitesiyle karşılaştırılmaz.
- Echo: planlanan zaman, yazma başlangıcı, yazma sonu ve tam yanıtın alınış zamanı (okuyucunun `read` dönüşü) host monotonik saatinden (`steady_clock`) kaydedilir. RTT planlanan zamana ve yazma başlangıcına göre ayrı raporlanır; işletim sistemi kuyruğu dahildir. Tek yön gecikme hesaplanmaz. **Kuyruk sınırı:** yanıtı gelmemiş istek sayısı 64'e ulaşırsa o istek gönderilmez ve sayılır (profil geçmez). **Zaman aşımı:** yazma sonundan 1 s. **Boşaltma:** son yazmadan sonra en çok zaman aşımı kadar.
- Kapasiteyi aşan profil (akışta %80, echoda %100) koşturulmaz; hesapla `GECMEDI` kaydedilir.

#### 1.4 Kayıt ve hüküm

Her satır `<tür> <no> <gövde> *<CRC-32>`; sıra B, [R…], P, [K], S. Hüküm yalnız çözümleyiciden çıkar; canlı araç da kaydı yazıp onunla okur.

- **GECERSIZ:** satır sonu yok, CRC tutmuyor, satır numarası kesintili, S yok ya da S'den sonra veri, alan eksik/yinelenen/sayı değil, B/P/K koşu kimliği tutmuyor, özet yok, kopma, yazma zaman aşımı, R satırları eksik/yinelenen/zaman sırası bozuk.
- **GECMEDI:** iki ucun sayaçları eşleşmiyor (PC gönderilen = kart alınan = kart beklenen sıra; kart gönderilen = PC alınan = PC beklenen sıra; tel baytı = çerçeve × boy), herhangi bir veri/CRC/sıra/UART/DMA/taşma sayacı sıfır değil, kart planını bitirmedi, echo zaman aşımı ya da atlanan istek, kapasite aşımı, süre ölçütü (`--en-az-sn`: plan, son yazma ve kart süresi ayrı ayrı), denetim hızına dönüş doğrulanmadı.
- **BASLAMADI:** kart hızı reddetti (`kur_ret`), işletim sistemi reddetti (`os_hiz_ret`), yeni hızda eşleme olmadı (`esle_yok`), kart ve PC'nin BRR hesabı tutmadı (`kur_uyusmaz`). Bu adaylar "doğrulanmadı" sayılır.

### 2. Gerçek komutlar

```sh
tools/olcum/mp3/derle.sh                      # temiz derleme; özet $IDA_CACHE/mp3/derle-<damga>/
tools/olcum/mp3/derleme/pc/mp3_test           # masaüstü testleri (şu an derlenmiyor; §4.1)
tools/olcum/mp3/onceki-imaj.sh                # yüklemeden önce kartta duran imajın kimliği
tools/olcum/mp3/yukle.sh                      # yedek doğrulamalı yükleme
tools/olcum/mp3/kos.sh kimlik
tools/olcum/mp3/kos.sh tara --akis-sn 10 --echo-sn 5          # ön tarama (kısa)
tools/olcum/mp3/kampanya.sh <en_yüksek_baud> <baud>...        # dondurulmuş kampanya; ÇALIŞTIRILMADI
tools/olcum/mp3/derleme/pc/mp3_vcp analiz <kayıt> --en-az-sn 600
```

Bu teslimde gerçekten çalıştırılanlar: `derle.sh` (bir kez, aşağıda), `onceki-imaj.sh` (kart yok, okuma başarısız), `cmake --build` ile PC ve aygıt yazılımı derlemeleri, test dosyasının depo dışındaki geçici kopyasıyla masaüstü testleri (normal ve UBSan; §4.1).

### 3. Sonuçlar

#### 3.1 Derleme (V1'in kartsız kısmı)

`derle.sh`, 2026-11-18, `$IDA_CACHE/mp3/derle-20261118-185701/`:

| Kalem | Değer |
|---|---|
| Derleme kimliği | `20261118-185701-mp3-253bd110` (ELF'te bulunduğu denetlendi) |
| Kaynak | `b1a56e4` (dal `olcum/mp3-vcp`), `tools/olcum/mp3/` işlenmemiş (izlenmeyen); dosya özeti SHA-256 `30a269ef…fb01` |
| Derleyici | GNU Tools for STM32 14.3.rel1 (`arm-none-eabi-gcc 14.3.1 20250623`); host Apple clang 17.0.0, cmake 4.5.0 |
| `mp3.elf` SHA-256 | `578639c7cfb7b5d71b1dd97d28f5a89c239d9640b8d3df0e793c6b028e50dadb` |
| `mp3.bin` SHA-256 | `6297994e79cb36fc474a0c2db9e7da286d1f7ad5daa39968044d33d4d132e5a8` |
| Boyut | text 22 616, data 12, bss 37 560; FLASH 22 628 B, RAM 37 592 B |
| Uyarı | Aygıt yazılımı ve `mp3_vcp`: kendi kodumuzda uyarı yok (yalnız newlib'in `_close/_lseek/_read/_write` bağlayıcı uyarıları; MP4 ile aynı). **`mp3_test`: derleme hatası** (§4.1) |

#### 3.2 Masaüstü testleri (V4 ve kısmi IO; kart yok)

Depodaki `test/mp3_test.cpp` şu an derlenmiyor (§4.1). Güncel kütüphane kodunu (`ortak/`, `pc/`) sınamak için test dosyasının **depo dışında geçici bir kopyası** yalnız tanımların sırası değiştirilerek (`Satir` ve yardımcıları `uctan_uca`'dan önceye) derlendi; depodaki dosya değiştirilmedi.

| Derleme | Denetim | Başarısız |
|---|---|---|
| Geçici kopya, `-O2` | 94 | 1 |
| Geçici kopya, `-O1 -fsanitize=undefined` | 94 | 1; UBSan çalışma zamanı hatası yok |

Başarısız denetim tasarım tutarsızlığını gösteriyor (§4.2). Geçen gruplar:

- **Birim:** CRC kontrol değerleri; 300 çerçevelik rastgele parçalı gidiş-dönüş; çöp, bozuk uzunluk, bozuk yük ve kesik çerçeve sonrası yeniden eşleme (bozuk/kesik çerçeve asla iyi sayılmıyor; arkadaki sağlam çerçeveler kurtarılıyor); sürüm ve uzunluk reddi; sıra izleyici; baud hesabı; KUR/OZET kodlaması; yüzdelik.
- **Uçtan uca (öykünücü):** temiz akış ve echo → GECTI; 1..7 baytlık kısmi okuma/yazma ve 2 KB soket tamponuyla akış ve echo → GECTI; PC→kart bayt çevirme, kart→PC bayt düşürme ve çevirme, enjekte ORE, enjekte halka taşması → GECMEDI; hat kopması → GECERSIZ (≈ 0,09 s); kartın susması → GECERSIZ (`ozet_yok`, ≈ 8 s); PC yazmasının tıkanması → GECERSIZ (`yazma_hata`, ≈ 0,4 s); kart reddi, işletim sistemi reddi, eşleme yok → BASLAMADI ve denetim hızına dönüş doğrulandı; kapasiteyi aşan echo profili koşturulmadan GECMEDI; süre ölçütü; var olan kayıt ezilmiyor.
- **Kayıt negatif (V4):** satır sonu yok, S yok, ortadan kesik, boş, tek bayt bozuk, S'den sonra veri, K yok, yanlış koşu kimliği (K ve P), sıfır olmayan hata sayaçları (kart ve PC), sayaç eşleşmemesi, dönüş yok, sayı olmayan alan, yinelenen alan, eksik alan, yinelenen/eksik R, yanıtsız R, geç yanıt, yanıt yazmadan önce, atlanan istek, P'den sonra R. Hiçbiri yanlış GECTI üretmedi.

Öykünücüde gözlenen RTT ve takvim değerleri hattı ölçmez; kartla ilgili sonuç olarak kullanılmamalıdır.

#### 3.3 Kart denemeleri

**Yapılamadı.** `onceki-imaj.sh` çalıştırıldığında STM32CubeProgrammer 2.23.0 `Error: No debug probe detected.` verdi (`$IDA_CACHE/mp3/onceki-imaj-20261118-185645/okuma.log`). `ioreg -p IOUSB` hiçbir USB aygıtı göstermedi; `/dev/cu.usbmodem*` yoktu. Görev belgesinin "kart erişimi kaybolursa ölçümü kes" kuralı gereği yükleme ve kısa denemeler yapılmadı. Bu nedenle:

- Kartta duran imajın (MP4 olabileceği varsayılıyor) kimliği **kaydedilmedi**.
- MP3 imajı **yüklenmedi**; kartta hiç çalışmadı.
- Ön tarama (921 600, 2 M, 3 M, 6 M, 12 M) **yapılmadı**; hiçbir aday için geçti/geçmedi/doğrulanmadı hükmü yok.

### 4. Doğrulanmayanlar ve tasarımda gördüğüm sorunlar

#### 4.1 Depodaki test dosyası derlenmiyor (benim hatam)

Kesintiden hemen önceki son düzenlemem (kapasite testi için `Satir` türünün ileri bildirimi ve `imzala2`) doğrulanmadan kaldı: `std::vector<Satir>` eksik türle kullanılıyor ve `imzala2` bildirilmeden önce çağrılıyor. Önerilen düzeltme: `Satir` ve kayıt yardımcılarını `uctan_uca()`'dan önceye taşımak (geçici kopyada denendi). Mimarın talimatı gereği inceleme öncesinde değiştirilmedi. `derleme/pc/mp3_test` ikilisi bu değişiklikten önceki koddan kalmıştır; kanıt olarak kullanılmamalıdır.

#### 4.2 Kapasite sınırının kaynağı tutarsız

`kos()` kapasiteyi koşu öncesinde `AnalizBeklenti::akis_oran` ile denetliyor; CLI `kos --oran` bu değeri hem trafik hem çözümleyici sınırı yapıyor. Sonuçlar:

1. CLI ile `--oran 0,9` verilirse koşu %90'da yapılır ve kendi sınırına göre GECTI olabilir. Dondurulmuş kampanyanın %80 sınırı araçta sabit değil (`kampanya.sh` `--oran` vermiyor, yalnız bununla korunuyor).
2. %80 beklentisiyle koşturulmadan reddedilen bir kayıt `--oran 0,9` ile çözümlenirse "kapasite dendi ama aşmıyor" diye GECERSIZ çıkıyor. Bu, başarısız denetimin nedeni; testin beklentisi de eskimiş.

Önerim: akış sınırını çözümleyicide %80'e sabitlemek ya da kullanılan sınırı B satırına yazıp çözümleyicinin kayıttaki sınırla denetlemesini sağlamak. Mimar kararı gerekiyor.

#### 4.3 Donanımda doğrulanmamış ayrıntılar

- GPDMA dairesel alımın ilk bloğu kanal yazmaçlarından yükleniyor. HAL ise ilk düğümü bellekten yükletiyor. Bu fark ve tek düğümlü döngünün yarım/tam kesmeleri doğrulanmadı.
- Gönderim DMA'sında hata sonrası kanalın kendiliğinden kapandığı varsayılıyor; doğrulanmadı.
- Alım sayımı, DMA kesme gecikmesinin yarım halkadan (8 KB; 12 Mbaud'da ≈ 6,8 ms, hesap) kısa olduğunu varsayıyor. HT ve TC birlikte görülürse hata sayılıyor, ama bir turdan fazla gecikme kendi başına ayırt edilemez (kuramsal).
- OVER8'in en küçük hata kuralıyla 921 600, 3 M, 6 M ve 12 M'de seçilmesi: OVER8 alıcı toleransı OVER16'dan düşük; ST-LINK tarafının baud hatası bilinmiyor.
- USART1 çekirdek saatinin PCLK2 = 160 MHz olduğu yazmaçtan okunacak; kartta okunmadı.
- 12 Mbaud'da `GPIO_SPEED_FREQ_HIGH`'ın yeterliliği doğrulanmadı.
- Kart tarafı işlemci yükü (CRC-32 tablo, veri üretimi/doğrulaması) 12 Mbaud'da iki yönde ölçülmedi. Tahmin olarak çekirdeğin küçük bir payı; ölçüm yok.

#### 4.4 PC ve ortam

- **Windows:** seri port gerçeklemesi yazılmadı; çalıştırılmadı. macOS dışında hız ayarı reddedilir.
- **Sanitizer:** bu macOS 26.5.2 + Apple clang 17'de ASan başlatmada (`AsanInitInternal` → `InitializeShadowMemory`) takıldı, TSan açılışta SIGSEGV verdi. Bu yüzden yalnız UBSan koştu. İki iş parçacıklı okuyucu/yazıcının yarışsızlığı araçla doğrulanmadı; tasarım ayrık bellek ve atomikler kullanıyor.
- **Takvim:** 3 ms meşgul bekleme yüksek hızlarda bir çekirdeği neredeyse sürekli meşgul eder; 600 s'lik koşularda ısıl ve güç etkisi ölçülmedi.
- **Echo kuyruk sınırı:** kaybolan yanıtlar bekleyen sayısından düşmez. 64 kayıptan sonra bütün istekler atlanır; profil zaten geçmez, ama sonraki RTT örnekleri kaybolur.
- **Gerçek fiziksel baud:** ne kartta ne PC'de ölçülebiliyor; yalnız istenen, BRR'den hesaplanan ve işletim sisteminin bildirdiği değerler kaydediliyor.

### 5. Ham veri ve kimlikler

| Ne | Nerede |
|---|---|
| Derleme özeti, imaj kopyaları | `${IDA_CACHE}/mp3/derle-20261118-185701/` |
| Başarısız imaj okuma günlüğü (kart yok) | `${IDA_CACHE}/mp3/onceki-imaj-20261118-185645/okuma.log` |
| Masaüstü test kayıtları (geçici) | `/tmp/mp3-test-*` (kalıcı değil) |
| Kart koşuları | yok |

### 6. Sıradaki adım (öneri)

1. sol ön incelemesinin bulgularıyla birlikte §4.1 ve §4.2'yi düzelt.
2. Kart bağlanınca sırasıyla `onceki-imaj.sh`, `yukle.sh`, `kos.sh kimlik`, `kos.sh tara` çalıştır; ön tarama sonuçlarıyla rapora dön.
3. Uzun kampanya (`kampanya.sh`) yalnız mimar onayından sonra yapılır.
