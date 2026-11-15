# sol incelemesi: mp7

## Hüküm

**Kabul yok: dört kritik, iki önemli bulgu var.** Özet süre tabloları CSV’lerle uyuşuyor; ancak yakınsama ölçütünün değiştirilmesi ve sonar hesabındaki hatalar, seçilen `N` değerinin ve buna dayanan mimari kararın geçerliliğini engelliyor.

- **S1:** 576 tam + 96 sade hücre eksiksiz; her hücre 200 ping içeriyor. Ek A ve `sonuc/ozet.md`, CSV’lerden birebir yeniden üretildi.
- **S2:** Kodun seçtiği özet istatistikler belirtilen adımlarda geçiyor. Bu, görevdeki kutu varyansının yakınsadığını göstermiyor. “Kararlı yakınsama yok” değerlendirmesi doğru.
- **S3:** İncelenen prototip ve seçilen `N = 80 000` için geçmediği doğru: uzun koşu p99’u **36,6075 ms**. Bunun geçerli bir yakınsama noktasındaki tam sonar modelini temsil ettiği doğrulanmış değil.
- **S4:** Saklanan özetler ve sabit toplama sırası, sınanan aynı ikili/makine kapsamındaki tekrar üretilebilirliği destekliyor.

Dosya değiştirilmedi; kurulum, derleme veya ölçüm koşusu yapılmadı. `derleme/` ve `veri/` kapsam dışında tutuldu. Dolayısıyla ham ping süreleri, özgün profil dizileri ve ölçüm anındaki sistem günlükleri bağımsız doğrulanmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `tools/olcum/mp7/kaynak/main.cpp:307`, `:343`, `:445`; `docs/olcumler/MP7-sonar-suresi.md:123` | S2, **kutu varyansı yerine `V/E²`** karşılaştırıyor. Bu, enerji ölçeğindeki değişimi kaldırır: yoğunluklar 0,8 ile çarpıldığında varyans %36 değişirken `V/E²` değişmez. Ayrıca geometrik enerji oranı, kutular arasındaki artış ve azalışları götürebilir; oranları 2 ve 0,5 olan iki kutu birlikte sıfır değişim verir. Bu ölçütlerle görevdeki yakınsama testi “biçimsel geçti” sayılamaz. | Ham kutu ortalaması, ham varyans ve ilintinin hangi kutu kümesinde, nasıl birleştirileceğini ölçümden önce dondur. Benek kontrastını ayrı tanılayıcı olarak tut. Mevcut sonucu “uygulanan alternatif istatistik geçti; görevdeki S2 doğrulanmadı” olarak düzelt; `N` seçimini buna göre yeniden değerlendir. | kritik |
| 2 | `tools/olcum/mp7/kaynak/sonar.cpp:266`, `:299` | `Br > 0` / `Br <= 0` koşulları, koninin içindeki **negatif sinc yan loblarını** eliyor. Örneğin `θ = 1,5 θ₀`, `φ = 0` için `B = −0,212207`; bu geçerli bir genliktir. Kod bu yolları ve ilgili görünürlük sorgularını atıyor. Böylece tam kademenin hem profili hem ölçülen multipath yükü değişiyor. | Koni dışını eleme ile genliğin işaretini ayır; geçerli negatif genlikleri faz işaretiyle koru. Etkilenen yakınsama ve süre sonuçlarını yenile. | kritik |
| 3 | `tools/olcum/mp7/kaynak/sonar.cpp:205`, `:240`; `tools/olcum/mp7/kaynak/main.cpp:191` | Birleşik hücre kuralı sade kademeye de uygulanıyor; fakat sade kademe yoğunlukları eşevresiz topluyor. `dA < Δ_h²` iken katkı enerjisi `σ dA²/Δ_h²` oluyor. Sabit yüzey daha çok ışına bölündüğünde toplam enerji yaklaşık `1/N` ile azalır. 05’in sade modeli için gereken alanla doğrusal enerji ağırlığı kayboluyor; raporda bu sapma açıklanmıyor. | Sade kademede eşevresiz katkının enerjisini `σ dA` ile ağırlıklandır. Birleşik kuralı eşevreli hücre hesabıyla sınırla. Sade kademe sonuçlarını doğru modelle yeniden değerlendir. | kritik |
| 4 | `tools/olcum/mp7/kaynak/sonar.cpp:99`, `:103`, `:208`; `docs/olcumler/MP7-sonar-suresi.md:78` | Hücre koordinatları iki üçgen kenarı boyunca kuruluyor; bu eksenler genellikle dik değil. İç hücrenin gerçek alanı `A_c = Δ_h² sin α`, normalizasyonda kullanılan alan ise `Δ_h²`. Su yüzeyi üçgenlerinde `α = 45°`: hücre alanı **0,707107 cm²**, 1 cm² değil. Üçgen sınırındaki hücreler ayrıca kırpılmıyor. “Tam kaplanmış hücrede `√(σ Δ_h²)`” iddiası kod için doğru değil. Bu hata yakınsamanın bağlandığı hücre ölçeğini değiştiriyor. | Ortonormal yüzey koordinatları kullan veya her hücrenin gerçek, üçgene kırpılmış alanını hesapla; merkez ve normalizasyonu aynı hücre tanımından üret. Yakınsama sonuçlarını yeniden değerlendir. | kritik |
| 5 | `tools/olcum/mp7/kaynak/main.cpp:428`, `:457`; `docs/olcumler/MP7-sonar-suresi.md:127`, `:150` | Kutu kümesi `S`, aynı 32 gerçekleşimin `2N` ortalamasından seçiliyor; bootstrap sırasında yeniden seçilmiyor. Aralıklar, veriyle seçilmiş bu sabit `S` için koşullu aralıklardır; bütün seçim işleminin belirsizliğini taşımaz. Nokta tahmininin aralığın içinde bulunması, %95 kapsamayı doğrulamaz. | `S`’yi bağımsız referansla önceden sabitle veya yeniden örneklemede seçim işlemini de tekrarla. Güven aralıklarının kapsamını açıkça belirt; özellikle seyrek rejimde görülen yanlılığın karar verilen adımlara etkisini denetle. | önemli |
| 6 | `docs/olcumler/MP7-sonar-suresi.md:173`, `:216` | İki sonuç cümlesi verilerle uyuşmuyor. Sahne büyümesinin p99’a etkisi 50 m seçkisinde “≤ %10” değil **%17,36**. “Tam kademede `N ≥ 2000` olan tek ping 1,25 ms’yi aşıyor” genellemesi de yanlış: orta sahne, 7 m, `N = 2002`, tek iş parçacığında `D = 2` için **en büyük süre 1,0999 ms**. | Bu cümleleri menzil, `D`, iş parçacığı ve kullanılan istatistikle sınırla. Dünya iş parçacığından ayırma gerekçesini gerçekten ağır hücrelere ve mimarideki işçi düzenine dayandır. | önemli |

## Yeniden hesaplananlar

**Süre verisi ve yöntem.** `sure.csv` içinde 672 benzersiz hücre var; yinelenen yapılandırma yok. Her hücrede 200 ping bulunuyor. Kod 10 ısınma pinginden sonra `Sonar::ping` çağrısını `std::chrono::steady_clock` ile çevreliyor. En yakın sıra yöntemiyle p99, 200 örnekte **198.**, 2000 örnekte **1980.** sıralı değerdir.

Zamanlama; üst sahnenin kurulması/commit edilmesi ve bırakılması, ışın örnekleme, kesişim ve görünürlük sorguları, katkı üretimi, sıralı biriktirme, evrişim, indirgeme, gürültü, log, TVG ve kuantalamayı kapsıyor. İşçi havuzunun oluşturulması, statik sahnenin ilk kurulumu ve `Sonar` nesnesinin başlangıç tampon ayırması dışarıda. İş dağıtımı ve tamamlanmasını bekleme içeride.

Saat seçimi süre ölçümü için uygun. LLVM libc++’ın Apple uygulaması monoton `CLOCK_MONOTONIC_RAW` kullanıyor; ölçümde kullanılan Apple libc++ ikilisinin tam kaynak eşleşmesi ayrıca doğrulanmadı. [LLVM kaynağı](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-17.0.6/libcxx/src/chrono.cpp)

**S3 hücresi:**

| Koşu | Ping | p50, ms | p99, ms | En büyük, ms | Ortalama, ms |
|---|---:|---:|---:|---:|---:|
| Tarama | 200 | 19,9207 | 36,7845 | 37,1066 | 20,9655 |
| Uzun | 2000 | 20,1145 | 36,6075 | 38,3426 | 21,1756 |

Bunlar CSV özetlerinden doğrulanan değerlerdir; ham süreler incelenmediğinden özgün yüzdelik hesabı yeniden yapılmış sayılmaz. Süreler seçilen açı/hedef dizisinin ampirik dağılımıdır; işletim sistemi gecikmelerinin genel p99 garantisi değildir.

**Ping bütçesi.** `c = 1500 m/s`, `n = 1200` ve `T_s = sp × 25 ns` varsayımlarıyla:

| Menzil ayarı | `sp` | `T_s` | Gerçekleşen menzil | `2R/c` | Eklenen sabit | Model ping aralığı |
|---|---:|---:|---:|---:|---:|---:|
| 7 m | 311 | 7,775 µs | 6,9975 m | 9,330 ms | 15,840 ms | 25,170 ms |
| 50 m | 2222 | 55,550 µs | 49,995 m | 66,660 ms | 15,840 ms | 82,500 ms |

İdeal, nicemlenmemiş menziller için `2 × 7 / 1500 = 9,333333 ms`, `2 × 50 / 1500 = 66,666667 ms`.

Üretici 50 m’de 33 saniyelik tam taramayı yayımlıyor; `sample_period` birimi ve örnek sayısı sınırları da resmî protokolde doğrulanıyor. Ancak `33 s / 400 = 82,5 ms` değerini seçilen yapılandırmaya bağlamak ve buradan çıkan **15,84 ms** kalanı 7 m’ye taşımak model varsayımıdır. Motor/işlem/iletim bölüşümü ölçülmüş değildir. Rapor bu ayrımı doğru işaretliyor. [Üretici özellikleri](https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/), [Ping360 protokolü](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/)

Uzun koşu için:

- `36,6075 / 9,33 = 3,92363`: akustik alt sınır aşılmış.
- `36,6075 / 25,17 = 1,45441`: model ping aralığı aşılmış.
- `21,1756 / 25,17 = 0,841303`: rapordaki yaklaşık 0,84 oranı doğru.
- `21,1756 / 9,33 = 2,26962`: alt sınırdaki üretim hızını tek iş parçacığı sürdüremiyor.

**İş parçacığı ölçeklenmesi.** Orta sahne, 7 m, tam kademe, `D = 2`, `N = 80 000`:

| İş parçacığı | p50, ms | p99, ms | p50 hızlanması | p99 hızlanması |
|---:|---:|---:|---:|---:|
| 1 | 19,9207 | 36,7845 | 1,000× | 1,000× |
| 2 | 10,3100 | 19,0244 | 1,932× | 1,934× |
| 4 | 5,4364 | 9,9916 | 3,664× | 3,682× |
| 11 | 3,0172 | 5,3658 | 6,602× | 6,855× |

“11 iş parçacığında yaklaşık 6,6×” p50 için doğru. Taranan seçeneklerde 25,17 ms sınırına iki, 9,33 ms sınırına 11 iş parçacığıyla giriliyor. Dört iş parçacığı 9,9916 ms ile kısa sınırın dışında.

Tek iş parçacığında, orta sahne ve 7 m için bütçeye giren en büyük **taranmış gerçek `N`** değerleri:

| Eşik | `D = 0` | `D = 1` | `D = 2` |
|---|---:|---:|---:|
| 9,33 ms | 40 014 | 20 000 | 9 996 |
| 25,17 ms | 80 000 | 80 000 | 40 014 |

50 m’nin raporda seçilen `N = 160 008` hücresinde p99, tek iş parçacığında **107,8858 ms**, iki iş parçacığında **54,5341 ms**: rapordaki bütçe karşılaştırması doğru.

**Yakınsama verisi.** CSV’deki koşullar yeniden uygulanınca ilk ortak nokta+GA geçişleri 7 m’de `80 000 → 160 008`, 50 m’de `160 008 → 320 000` çıkıyor. Özetlerden `δ_V`, `δ_C` ve `δ_W` oranları yeniden hesaplandığında kayıtlarla uyuşuyor:

| Adım | `δ_E` — kaydedilmiş | `δ_V` | `δ_C` | `δ_W` |
|---|---:|---:|---:|---:|
| 7 m, 80 000 → 160 008 | −%1,985 | −%1,329 | +%0,522 | −%20,508 |
| 50 m, 160 008 → 320 000 | +%2,394 | +%0,789 | +%0,401 | −%14,058 |

Kutu başına gerçekleşimler özet CSV’lerde bulunmadığından geometrik `δ_E` ve bootstrap aralıkları bağımsız yeniden hesaplanamıyor. Sonraki 7 m adımlarındaki kaydedilmiş enerji değişimleri **−%6,596** ve **−%10,330**; kararlı yakınsama olmadığı sonucu destekleniyor.

**05’in özgün hücre kuralı.** Sabit alanı `A` olan hücreye eşit ağırlıklı `m` ışın düşsün; `dA = A/m`, faz ve `g_c` ortak olsun. Diğer çarpanları sabit tutunca:

\[
p_c=m\sqrt{\sigma A/m}\,g_c
=\sqrt{\sigma A m}\,g_c,\qquad
E|p_c|^2=\sigma A m.
\]

Dolayısıyla enerji ışın sayısıyla sınırsız büyür. Raporun bu temel iddiası **doğru**; 05’in “aynı alana yakınsar” cümlesi yanlıştır.

Birleşik kural, doğru hücre alanı kullanıldığında yoğun örnekleme limitini sonlu yapabilir. Ancak rastgele kaplama toplamının karesi, kaplama varyansından ek enerji getirir; bu katkı `N` ile değişir. Raporun geçiş rejimindeki yanlılık açıklaması bu bakımdan tutarlı. Belirli tepenin bütün nedenlerini yalnız bu mekanizmaya bağlamak ve `10⁶–10⁷` ışın gereksinimini kesinleştirmek mümkün değil; ikincisi doğru biçimde ölçülmemiş tahmin olarak işaretlenmiş.

**Diğer hesaplar ve tekrar.**

- Üçgen sayıları bağımsız sayımla **9 886 / 100 368 / 999 558**.
- Yazılı soğurma formülünün aritmetiği **0,131407 dB/m**; fiziksel geçerliliğinin doğrulandığı anlamına gelmez.
- 6 m duvar için beklenen kutu **1028,4389**.
- RNG sayaçları işçi kimliğine bağlı değil; her ışın kendi `sonuc_[j]` alanına yazıyor. Havuz tamamlandıktan sonra katkılar ışın sırasıyla tek iş parçacığında toplanıyor. İşçi sırasından bağımsız toplama gerçekten sağlanmış.
- Saklanan tek/11 iş parçacığı özetleri iki menzilde de aynı. Philox sabitleri ve tur işlemi özgün kaynakla uyuşuyor. [Random123 kaynağı](https://raw.githubusercontent.com/DEShawResearch/random123/main/include/Random123/philox.h)

## İskelete ya da diğer bölümlere notlar

`docs/mimari/05-sensorler-ve-sonar.md` §2.4.3’teki enerji koruma iddiası düzeltilmeli. Bu, prototip raporunun doğru yakaladığı bir mimari hatadır. §6’da ortalama, varyans, ilinti, kutu seçimi ve güven aralığının anlamı birlikte tanımlanmalı; `V/E²` ham varyansın yerine geçirilmemeli.

İskelet §8’deki sonar yedeğine geçiş için bu sonuç **karar girdisidir**. Geçerli bir yakınsama `N`’si henüz bulunmadığı, tam dalga yaması ve dünya adımına etki ölçülmediği için tam kademenin genel HIL uygunluğu bu turla kesinleşmez.

Embree 4.4.1’in CPU platform desteği, kullanılan C++ sorgu API’leri ve `threads=1` yapılandırmasının **kurulum iş parçacıklarını** sınırladığı doğrulandı. Homebrew formülü 4.4.1, ARM için `NEON2X` ve ISPC ayarlarını destekliyor. Bunlar ölçümde kullanılan şişenin derleyici/TBB sürümlerini veya seçilmiş çalışma zamanı çekirdeğini bağımsız doğrulamıyor. [Embree 4.4.1](https://raw.githubusercontent.com/RenderKit/embree/v4.4.1/README.md), [cihaz yapılandırması](https://raw.githubusercontent.com/RenderKit/embree/v4.4.1/doc/src/api/rtcNewDevice.md), [Homebrew formülü](https://raw.githubusercontent.com/Homebrew/homebrew-core/master/Formula/e/embree.rb)

M3 Pro’nun belirtilen 5 P + 6 E çekirdek düzeni üreticiyle uyumlu. Rapor Windows için süre genellemesi yapmıyor. Pilde ölçüm, prizde tekrar yapılmaması ve iki bloktaki arka plan yükü açıkça belirtilmiş; bunlar gizlenmiş değil. [Apple teknik özellikleri](https://support.apple.com/en-us/117736)

“Doğrulanmayanlar” listesi temel platform ve bütünleme sınırlarını kapsıyor. Buna bu incelemedeki model sapmaları, güven aralıklarının koşullu kapsamı ve p99’un belirli tarama iş yüküne ait olduğu sınırı da eklenmeli.