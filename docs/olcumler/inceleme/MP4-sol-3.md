# sol ikinci incelemesi: mp4

## Hüküm

**Kabul yok.** Y2, Y3 ve Y4 giderilmiş; Y1 kısmen giderilmiş. Bütün örneklerin taştığı durumda minimum, maksimum ve toplamın birlikte olanaksız olduğu kayıtlar hâlâ `GEÇERLİ` sayılıyor. **1 kritik ve 2 önemli bulgu** kaldı.

Tamamlanmış gerçek kayıtlarda bu kusura rastlamadım. Beş geçerli kaydın yeniden analizi v3 sayısal çıktılarıyla eşleşti; bağımsız O2 ve Og/200 doğrulamalarının sonuçları korundu. D1 ve D2 geçmedi; D3 ve D4 sağlandı.

Dosya değiştirmedim, kurulum yapmadım, karta veya seri porta erişmedim. Kritik bulgu nedeniyle iş, `docs/mimari/09-test-ve-is-akisi.md` §2.4 uyarınca mimara dönmelidir; aynı kapsamda üçüncü düzeltme turu önerilmiyor.

## İlk incelemedeki maddelerin durumu

| # | Durum | Not |
|---|---|---|
| Y1 | kısmen | Özgün karşı örnek artık reddediliyor. Eksik H, taşma sınırı ve aralık içindeki minimum denetimleri eklenmiş. Ancak gözlenen minimum ve maksimumun toplamda gerçekten bulunması gerektiği denetlenmiyor; aşağıdaki Y5 sürüyor. |
| Y2 | giderildi | Üst sınır kayıt yerine ölçülen ELF’ten geliyor. Beş imajın sınırlarını yeniden hesapladım. Gerçek O2 üzerinde `4294967295 B` karşı örneği `rc=1` verdi; kapsama aykırı ve sınırı aşan yapay örnekler de reddedildi. |
| Y3 | giderildi | `lat_temiz_n=n` için maksimum eşitliği uygulanmış. Gerçek O2 A’da `86→59` değişikliği, uzun kampanyanın bütün beklenen parametreleriyle `rc=1` verdi; sayısal özet basılmadı. |
| Y4 | giderildi | Dışa aktarım T denetimi ve son hata kararından sonraya taşınmış. Eksik T, yanlış T ve yalnız C’de hata durumları `rc=1`; mevcut hedef dosya durumu `rc=3` verdi. Mevcut histogramların içerikleri değişmedi. Yeni dosya oluşturma ve yazma hatasında geri alma yollarını bu incelemede çalıştırmadım. |

Yazarın önceki CSV yazım iddiasının yanlış olduğunu kabul etmesi kaynak akışıyla doğrulanıyor. Yeni akış, kampanya geçersizken dışa aktarımı engelliyor.

## Yeni bulgular

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem | Kapatılacağı yer |
|---|---|---|---|---|---|
| Y5 | `tools/olcum/mp4/pc/mp4_seri.cpp:553`, `tools/olcum/mp4/pc/mp4_seri.cpp:647` | **Y1’in toplam/minimum/maksimum tutarlılığı tamamlanmamış.** `n=tasan=241000`, H boş, `min=3300000`, `max=4000000` iken `top=795300000000` ve `top=964000000000` ayrı ayrı `rc=0 / GEÇERLİ` veriyor. İki toplam da olanaksızdır: gözlenen minimum ve maksimum en az birer örnekte bulunmalıdır. | `n≥2` için gerekli sınır `(n−1)·min+max ≤ top ≤ min+(n−1)·max`; bunu histogram sınırlarıyla birlikte uygula. `n=1` için `min=max=top` gerekir. İki olanaksız uç ve iki geçerli sınır toplamı kabul kanıtına eklenmeli. | kritik | Y1 kapanışı; §2.4 gereği mimarın kapsam/görev kararı |
| Y6 | `docs/olcumler/MP4-dongu-yuku.md:243` | FNV sağlama toplamlarının eşitliğinden “bit düzeyinde aynı çıktı” sonucu çıkarılıyor. `MP4-Og-200-dogrulama.md` açıkça tekil çıktı baytlarının kayıtta bulunmadığını ve bu eşitliğin bağımsız bit eşitliği kanıtı olmadığını belirtiyor. | Sonucu “fazların 32 bit FNV sağlama toplamları eşleşti; tekil çıktıların bit eşitliği doğrulanmadı” olarak sınırla. | önemli | Nihai rapor §4.5; kalan belge bulguları kaydı |
| Y7 | `docs/olcumler/MP4-dongu-yuku.md:143` | “Bütün geçerli koşularda” TIM2 periyodu 399.998–400.000 çevrim deniyor. 200 Hz ham kayıtlarında değerler **799.998** ve **800.001**, beklenen **800.000** çevrimdir. | 400 Hz ve 200 Hz periyot doğrulamalarını ayrı yaz. Çözümleyici gerçek değerleri doğru kabul ediyor; hata rapordadır. | önemli | Nihai rapor §4.1; kalan belge bulguları kaydı |

## Yeniden hesaplananlar

`analiz_test.py` içindeki analiz testlerini, dosya oluşturmadan `/dev/stdin` üzerinden çalıştırdım: **77 denetim geçti, 0 başarısız**. Ayrıca gerçek O2’nin değişmemiş girdisi ve iki bozma örneği ile dört dışa aktarım denetimi geçti. **94 testlik komutun tamamını yeniden çalıştırmadım**; dosya oluşturan ve PTY kullanan yollar bu incelemede çalıştırılmadı.

Y5 karşı örneği test yardımcılarıyla üretildi; sıra ve CRC yenilendi:

```python
kampanya(
    n=241000,
    faz_ozel={"A": faz_tasmis("A", n=241000, top=795300000000)}
)
```

Analizde şu beklentiler korundu:

```text
--kimlik K1 --beklenen-hz 400 --beklenen-fazlar 7
--beklenen-faz-adim 241000 --en-az-adim 240000
--en-az-sn 600 --yigin-azami 1586416
```

Bağımsız toplam hesabı:

```text
alt = 240999 × 3300000 + 4000000 = 795300700000
üst = 3300000 + 240999 × 4000000 = 963999300000
```

Olanaksız `795300000000` ve `964000000000` toplamları kabul edildi. Doğru iki sınır toplamı da kabul edildi.

ELF sembollerinden `_estack − (_ebss + 256)` hesabı:

| İmaj | `_ebss` | `_estack` | Üst sınır, B |
|---|---|---|---:|
| O2 | `0x200ECA10` | `0x20270000` | 1.586.416 |
| Og | `0x200ECB98` | `0x20270000` | 1.586.024 |
| O2-200 | `0x201050B0` | `0x20270000` | 1.486.416 |
| Og-200 | `0x20105238` | `0x20270000` | 1.486.024 |
| Kısa | `0x200ECA10` | `0x20270000` | 1.586.416 |

`nm` çıktısının varsayılan onaltılık sembol değeri biçimi canlı birincil belgeden doğrulandı. [GNU Binutils `nm`](https://sourceware.org/binutils/docs/binutils/nm.html)

Gerçek kayıtların CRC/sıra hesabını ayrı bit tabanlı CRC uygulamasıyla tekrarladım:

| Kayıt | B’den itibaren satır | CRC/sıra hatası | Yeniden analiz |
|---|---:|---:|---|
| O2 400 Hz | 3.539 | 0 | geçerli; v3 ile aynı |
| Og 400 Hz | 3.604 | 0 | geçerli; v3 ile aynı |
| O2 200 Hz | 623 | 0 | geçerli; v3 ile aynı |
| Og 200 Hz | 3.490 | 0 | geçerli; v3 ile aynı |
| Kısa | 302 | 0 | geçerli; kabul kampanyası değil |
| Kesilen Og | 243 | 0 | geçersiz; fazlar ve T eksik |

Dört uzun kaydın ham SHA-256 değerleri bağımsız doğrulama belgeleriyle; beş ELF’in hash’leri önceki kimliklerle eşleşti. **33 v3 histogram dosyası** mevcut; kesilen Og için histogram yok.

| Koşu / faz | Süre alt sınırı, s | p99,9 üst sınırı, µs | Maksimum, µs | Aşım |
|---|---:|---:|---:|---:|
| O2 400 A | 602,49750004375 | 1338,09375 | 1338,91875 | 0 |
| O2 400 B | 821,4287758375 | 3321,59375 | 3322,38125 | 241.000 |
| O2 400 C | 602,4974999625 | 1300,69375 | 1301,73125 | 0 |
| Og 400 A | 602,49750005625 | 2571,79375 | 2572,78125 | 3.013 |
| Og 400 B | 1573,7124755 | 6420,89375 | 6421,69375 | 241.000 |
| Og 400 C | 602,49749984375 | 2306,79375 | 2307,84375 | 0 |
| O2 200 B | 604,99499995625 | 3321,59375 | 3321,96875 | 0 |
| Og 200 B | 788,22253429375 | 6420,69375 | 6421,45000 | 121.000 |

Arşivlenmiş objdump verisinde D3 yeniden `rc=0` verdi: O2/Og için **46/61 işlev**, çift duyarlıklı yordam/komut ve dolaylı çağrı **0**. `kos.sh` sözdizimi denetimi geçti; betik kart koşusuyla çalıştırılmadı.

## İskelete ya da diğer bölümlere notlar

- Gerçek ölçümlerin geçerliliği ile çözümleyicinin bozuk girdiyi reddetmesi ayrı sonuçlardır. Y5 gerçek kayıtları geçersiz kılmıyor; çözümleyicinin kapanışını engelliyor.
- Yer tutucu sonuçları kontrol hızını kesinleştirmez. Gerçek çekirdekle yineleme ve taşıma dahil ölçüm gereği korunmuş; GNC veya atış kontrol geliştirmesi incelenmedi.
- Ana görev bu worktree’de bulunmadığından ana depodaki aynı göreli yol okundu. İstenen yanıt, nihai rapor ve iki bağımsız doğrulama belgesine erişildi.
- STM32U5A9’un 160 MHz, tek duyarlıklı FPU ve 2,5 MB SRAM özellikleri canlı ST kaynağından doğrulandı. [ST ürün belgesi](https://www.st.com/en/microcontrollers-microprocessors/stm32u5a9bj.html)
- Open Group `link()` belgesine erişim 403 verdi; bu API’nin dış kaynak doğrulaması tamamlanmış sayılmadı.