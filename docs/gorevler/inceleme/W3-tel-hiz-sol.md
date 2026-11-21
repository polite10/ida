# sol incelemesi: ida_link

## Hüküm

**Bir önemli bulgu var:** fark testi, iddia ettiği sabit blok sınırlarını korumuyor; W3 kabul maddesi 2’nin kapsaması eksik.

Ürün kodunda statik incelemeyle kritik bellek güvenliği hatası, sayaç davranışı farkı veya strict aliasing ihlali saptamadım. Derleme ve test çalıştırılmadı; MSVC `/W4 /WX /analyze` sonucu bu incelemede doğrulanmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `protocol/ida_link/olcum/tel_fark.c:298`, `protocol/ida_link/olcum/tel_fark.c:351` | `besle`, erken çerçeve tesliminden sonra mevcut bloğun kalanını korumuyor: `i += te` sonrasında yeniden tam `parca` boyu veriyor. Örneğin 64 B’lık ilk çağrı, ilk altın çerçeveyi 33 B tüketerek teslim eder; sonraki çağrı kalan 31 B yerine 64 B alır. Dolayısıyla `test_altin_akis_her_sabit_blok_boyuyla` gerçek sabit blok bölünmesini sınamıyor. Rastgele bölünmelerde de seçilen bloğun kalanı korunmuyor. | Dış döngüde blok sonunu sabitle; iç döngüde çerçeve teslimlerinden sonra yalnız o bloğun kalanını besle. Yeni blok boyunu mevcut blok tamamen tüketildiğinde seç. 33 B’lık çerçeveyi izleyen 94 B’lık çerçeve ve 64 B bloklarla çağrı uzunluklarının `64, 31, …` olduğunu denetleyen bir karşı örnek ekle. | önemli |

## Yeniden hesaplananlar

- **CRC tablosu:** 256 girdiyi yansıtılmış `0x8408` polinomuyla sekiz bit adımından yeniden hesapladım; fark **0**. `"123456789"` sonucu **`0x6F91`**. 38 altın çerçevenin CRC’sini başlık, yük ve `CRC_EXTRA` üzerinden bağımsız hesapladım; **38’i de eşleşti**. Kapsanan baytlar [MAVLink serileştirme tanımıyla](https://mavlink.io/en/guide/serialization.html) uyumlu.
- **Tampon sınırları:** `hedef = 12 + len ≤ 267`. `govde_bekliyor` koşulunu sağlayan bütün bayt değerleri için **33.152 `len/dolu` çiftinde** son CRC ve yük kaynak indekslerini yeniden hesapladım; sınır aşımı çıkmadı. Eksik blokta `n < hedef − dolu`, kopyanın tampon sonuna ulaşmasını engelliyor.
- **Sayaç sarma:** tüketilen `k` baytı bir defada `(uint32_t)k` olarak eklemek, bayt başına artırmayla mod `2³²` altında aynı sonucu verir.
- **Sabit veri:** CRC tablosu `256 × 2 = 512 B`; dizin `100 B`; toplam ek sabit veri **612 B**.
- **Başvuru kopyası:** `tel_eski.c` içindeki işaretli özgün bölüm, `origin/main:protocol/ida_link/src/tel.c` ile bayt bayt aynı.

## İskelete ya da diğer bölümlere notlar

- A1-K07’nin ret sırası ve `rx_resync_bytes` sınıflaması korunuyor. Başlıkta reddedilen adayın dokuz baytı yeniden taranıyor; CRC, kimlik çifti veya yön nedeniyle bütünüyle tüketilen adayın baytları yeniden eşleme sayacına eklenmiyor. Taşma bildirimi yarım adayı bırakıp sıra geçmişini koruyor.
- `DIZIN` ile `TABLO` aynı makro listesinden kuruluyor. Aralık dışı kimlikler `_Static_assert` ile, yinelenen kimlikler üreteçte reddediliyor; dialect değişiminde sessiz yanlış eşleme yolu saptamadım.
- Sözcük işlemleri hizasız tamsayı göstericisi erişimi yerine `memcpy` kullanıyor; sıfır sözcük karşılaştırması bayt sırasından bağımsız. İlgili nesne erişim kuralları [WG14 N1570 §6.5 ve §7.24.2.1](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) üzerinden denetlendi. **N2176 adresi zaman aşımı nedeniyle okunamadı.**
- Cortex-M33’te hizasız erişim desteği Normal memory, uygun komut ve `UNALIGN_TRP` koşullarına bağlıdır; koşulsuz bir donanım güvencesi değildir. [Arm teknik başvuru kılavuzu §2.4.2](https://documentation-service.arm.com/static/66be1ce8882fec713ef49954?token=)
- MSVC’nin C17 ve `_Static_assert` desteği [Microsoft’un birincil kaynağından](https://devblogs.microsoft.com/cppblog/c11-and-c17-standard-support-arriving-in-msvc/) denetlendi; bu, mevcut değişikliğin Windows derleme ve analizinden geçtiği anlamına gelmez.