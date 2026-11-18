# sol incelemesi: MP4a

Tarih: 2026-11-17. İnceleyen: `gpt-6.1-sol`, `high`.

Süreç notu: Mimarın başlattığı ilk `tools/sol-incele.sh` çağrısı CLI kullanım kotası nedeniyle rapor üretmeden sonlandı. Bağımsız inceleme aynı modelle Codex alt agent üzerinden tamamlandı. Bu dosya ilk kod/kısa koşu incelemesidir; uzun kampanya raporunun nihai incelemesi değildir.

## Hüküm

**Koşulsuz kabul yok: 2 kritik çözümleyici bulgusu, 3 önemli ölçüm/sunum tutarsızlığı var.** Mevcut kısa koşunun ham verisinin bozuk olduğunu gösteren bulgu yok. Uzun kart koşusu sürdürülebilir; kritik düzeltmeler PC tarafındadır ve **yeniden firmware yüklemesi gerektirmez**.

Kodun tamamını okudum; inceleme sırasında dosya değiştirmedim, seri port açmadım, karta erişmedim. Uzun kampanyanın tamamını ve tamamlanacak raporu bu incelemede onaylamıyorum. Bu inceleme çıktı dosyası, inceleme tamamlandıktan sonra mimarın açık izniyle kaydedildi.

## Düzeltilmesi gerekenler

| # | Yer | Sorun ve kanıt | Düzeltme | Önem |
|---|---|---|---|---|
| 1 | `pc/mp4_seri.cpp:313–320,341–353`; `kos.sh:23–27,65` | Başlık değerleri ve beklenen kampanya doğrulanmıyor. `fazlar=0` olan, hiç faz özeti/örnek içermeyen kampanya **`--en-az-adim 240000 --en-az-sn 600` ile rc=0** veriyor. `hz=0`, eksik B2 veya B2 içinde `saat_adim=7 hesap_hz=80000000 uart_hata=1` de kabul ediliyor. Böylece “bütün fazlar eksiksiz” sonucu ölçüm olmadan üretilebiliyor. | Başlık alanlarının değer/aralık/tutarlılık denetimi; tek ve eksiksiz B2; başarılı saat/UART bilgisi; koşu betiğinden beklenen hız/faz maskesi/adım hedefinin ayrıca verilmesi ve doğrulanması. `o2/og` zorunlu A/B kampanyasının bildirilen maske tarafından daraltılamaması gerekir. | **Kritik** |
| 2 | `pc/mp4_seri.cpp:383–414,424–430,453–464` | Özet şeması ve gecikme histogramının sayısal tutarlılığı eksik. Yapay geçerli kampanyada yalnız `G.lat_max=4802→0` değiştirilip CRC yenilendiğinde histogram hâlâ 58 ve 4800+ çevrim içerirken araç **p99,9 ≤0.000 µs ve GEÇERLİ** yazıyor. S satırındaki bütün ölçüm alanlarının silinmesi ve Q içinde `p0=nan/nan` da geçiyor. D4 verisi bulunmadan kabul mümkün. | G için min/max/top ve iki histogramın kutu sınırlarını karşılaştır; temiz örnek ve sayaç sınırlarını doğrula. S’nin zorunlu alanlarını ve taşma bayrağını denetle. Q değerlerini tam tüketilen, sonlu, işaretsiz sayılar olarak çöz; toplam/maksimum ilişkisini denetle. Bozuk şemada raporlamadan önce rc=1 dön. | **Kritik** |
| 3 | `fw/platform.c:616–618`; `pc/mp4_seri.cpp:391` | `duvar_cyc`, ilk adımın başlangıcından son adımın **başlangıcına** kadardır; kod açıklamasındaki son adımın sonunu içermez. Son `sure` eklenmiyor. Kısa A/C’de yaklaşık 9.9975 s bu nedenle beklenir. | Mevcut koşularda alanı açıkça **muhafazakâr süre alt sınırı** olarak raporla. İstenirse gerçek bitiş süresini `[duvar+min, duvar+max]` aralığıyla ver. Son adımın süresi ayrıca saklanmadığından kesin bitiş süresi uydurulamaz. Gelecek firmware sürümünde son süre eklenebilir. | Önemli |
| 4 | `fw/platform.c:355–371,657,680` | `yigin_tepe` “açılıştan bu yana” olarak tanımlı, fakat her faz öncesindeki yeniden boyama önceki derin kullanımın izini silebiliyor. Gerçek kısa kayıtta **3392→3452→3396 B**; ömür boyu tepe azalmaz. Ayrıca `yigin_kayit` boyaması kayıt başlangıcında değil, 400 geçiş adımından önce yapılıyor. | Mevcut raporda kapsamı doğru adlandır; koşu özeti için raporlanan tepelerin maksimumunu kullan. `yigin_kayit` değerinin geçiş adımlarını ve ölçüm çevresindeki işleri içerdiğini belirt. Mevcut kayıt için yeniden koşu gerekmez. | Önemli |
| 5 | `pc/mp4_seri.cpp:191–196`; `test/analiz_test.py` yüzdelik beklentisi | `≤` ile sunulan üst sınırlar en yakına yuvarlanıyor ve aşağı düşebiliyor. Mevcut testte 58 çevrim = **0.3625 µs** için “**≤0.362**” bekleniyor; bu matematiksel üst sınır değildir. | Üst sınırları gösterilen basamakta yukarı yuvarla veya yeterli kesinlikle ver; test beklentisini düzelt. | Önemli |

Yolların kökü: `tools/olcum/mp4/`. Önem sınıfları `docs/mimari/09-test-ve-is-akisi.md` §2.4'e göredir.

### Bağımsız olumsuz testleri tekrar üretme

Aşağıdaki komut çalışma kopyasının kökünde çalışır; kaynak dosyalarını değiştirmez. Mevcut test üreticisini kullanarak satır CRC’lerini ve sıralarını yeniler:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -I - <<'PY'
import runpy

m = runpy.run_path("tools/olcum/mp4/test/analiz_test.py")
base = m["kampanya"]()

def degistir(lines, fn):
    out = []
    for s in lines:
        b = fn(s[:s.rfind(" *")])
        if b is not None:
            out.append(m["imzala"](b))
    return m["yeniden_imzala"](out)

cases = {
    "faz_yok": m["kampanya"](fazlar=""),
    "B2_yok": degistir(base, lambda b:
        None if b.startswith("B2 ") else b),
    "hatali_saat": degistir(base, lambda b:
        b.replace("rcc_csr=0x0 idcode=0x30006481",
                  "saat_adim=7 hesap_hz=80000000 uart_hata=1")
        if b.startswith("B2 ") else b),
    "hz_sifir": degistir(base, lambda b:
        b.replace("hz=400 ", "hz=0 ")
        if b.startswith("B ") else b),
    "S_alanlari_yok": degistir(base, lambda b:
        " ".join(b.split()[:3]) if b.startswith("S ") else b),
    "Q_nan": degistir(base, lambda b:
        b.replace("p0=1000000/2000", "p0=nan/nan")
        if b.startswith("Q ") else b),
    "lat_max_sifir": degistir(base, lambda b:
        b.replace("lat_max=4802", "lat_max=0")
        if b.startswith("G ") else b),
}

for name, lines in cases.items():
    args = ("--en-az-adim", "240000", "--en-az-sn", "600") \
        if name == "faz_yok" else ()
    rc, out = m["analiz"](lines, *args)
    print(name, "rc=", rc, out.splitlines()[-1])
    if name == "lat_max_sifir":
        print(next(x for x in out.splitlines()
                   if x.startswith("tetik gecikmesi")))
PY
```

**Mevcut sonuç: bütün vakalar rc=0 / GEÇERLİ.** Düzeltmeden sonra her biri rc=1 üretmeli. Bunların yanında `lat_min>lat_max`, histogramla çelişen `lat_top`, negatif/taşan sayılar, eksik S alanları ve Q alanındaki sayı sonrasında çöp karakter örnekleri bağımsız test kümesine eklenmeli.

## Yeniden hesaplananlar

`${IDA_CACHE:-$HOME/.cache/ida}/mp4/kos/kisa-20261117-185942.csv` üzerinde bağımsız CRC, sıra, histogram toplamı ve yüzdelik hesabı:

- Açılıştan itibaren **302 satır**, sıra **0…301**, CRC hatası/sıra boşluğu yok.
- Her fazda adım histogramı ve birleşik gecikme histogramı **4000 örnek** içeriyor.
- Histogram taşması yok.

| Faz | Min / maks süre, µs | p50 üst sınır, µs | p99 üst sınır, µs | p99,9 üst sınır, µs | Gecikme p99,9 üst sınır, µs | Aşım / atlanan tetik |
|---|---:|---:|---:|---:|---:|---:|
| A | 598.44375 / 1337.31875 | 600.99375 | 1336.69375 | 1337.29375 | 0.46875 | 0 / 0 |
| B | 3315.23125 / 3318.36875 | 3317.09375 | 3318.36875 | 3318.36875 | 2497.49375 | 4000 / 1423 |
| C | 1295.96875 / 1298.23750 | 1297.29375 | 1298.23750 | 1298.23750 | 0.50000 | 0 / 0 |

Kısa koşu kabul kampanyası değildir. B fazında `asim=asim_dwt=4000`; ağır yükün başarısızlığı gizlenmemiştir.

Ek doğrulamalar:

- Mimarın yerel başlangıç SHA-256 listesine karşı `shasum -a 256 -c` denetimi: **7/7 aynı** (`adim/**`).
- `python3 -I tools/olcum/mp4/test/analiz_test.py`: **31 denetim geçti**; yukarıdaki yanlış kabul vakaları bu kümede yok.
- Güncel O2/Og ELF SHA-256 değerleri `${IDA_CACHE:-$HOME/.cache/ida}/mp4/derle-20261117-185727/derle-ozet.txt` içindeki değerlerle aynı. O2: `9ad49a48b1c14049b9b0c34083c084d139b8dc371037dc900a599a621cb9948f`; Og: `5f0fd81f63a38bdcc014e77e2157ec841f639e007dc833704e6e1372b6223b5c`.
- Kısa koşu ELF SHA-256 değeri koşu başı/sonu kaydıyla aynı: `f331a952c8ca95dc26e273c562a8c343d1c071f265d1815faf2b823e31925c04`; kimlik: `20261117-185727-kisa-bc64cc5d`.
- Güncel ELF’ten yeniden alınan objdump komut gövdeleri aynı derleme kayıt dizinindeki arşivdekilerle aynı.
- D3: O2’de **46**, Og’de **61** erişilen işlev; çift duyarlıklı yordam/komut ve çözülmemiş dolaylı çağrı **0**. O2’nin tüm sembol listesinde de çift duyarlıklı yordam görmedim.
- `ninja -C tools/olcum/mp4/derleme/o2 -n` ve `ninja -C tools/olcum/mp4/derleme/og -n`: **no work to do**. Yeniden derleme yapmadım.

## Mimara notlar

- **Uzun koşuyu kesme veya firmware’i sırf bu bulgular için yeniden yükleme gereği görmüyorum.** Çözümleyici düzeltildikten sonra korunmuş ham kayıt yeniden analiz edilmeli.
- Tamamlanan O2 A fazının bildirilen `duvar_cyc=96399600007` değeri **602.49750004375 s** muhafazakâr alt sınırdır; 600 s şartını zaten sağlar. Verilen min/max ile gerçek son-adım-bitiş süresi **[602.4980953875, 602.4988389625] s** aralığındadır.
- Zamanlama yarışlarının büyük sapma ürettiğine dair kısa koşuda kanıt yok. Başlangıç TIM2 okumasıyla DWT başlangıcı arasında ve DWT bitişiyle aşım bayrağı okuması arasında küçük ölçüm uç farkları vardır; bunlar rapordaki gecikme tanımında açıklanmalı.
- Bildirilen gecikme **güncelleme olayı→ana döngü örnekleme noktası** ölçümüdür. Görevdeki kesme→adım başlangıcı tanımıyla aynı değildir. ISR minimum/maksimumunu yüzdeliklerden çıkararak istenen gecikmenin p99,9’u türetilemez.
- Rapor inceleme anında `SONUÇLAR_BEKLENIYOR`; eksik uzun sonuçları planlı kabul ettim, bulgu saymadım. D1/D2’nin nihai hükümleri ve D4’ün tamamlığı uzun O2/Og verisiyle ayrıca incelenmeli. Kısa B’de D2 aşılması geçerli bir ölçüm sonucudur.
- Kartlı yeniden koşu, mutlak saat doğrulaması ve tamamlanmamış uzun kampanyanın sonuçları bu incelemede çalıştırılmadı/doğrulanmadı.
