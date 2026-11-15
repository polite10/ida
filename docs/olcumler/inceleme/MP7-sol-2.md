# sol ikinci incelemesi: mp7

## Hüküm

**Koşulsuz kabul yok: bir kritik, iki önemli yeni bulgu var.** İlk incelemenin altı maddesindeki düzeltmeler uygulanmış. Ancak uygulanan hücre/faz modeli, dondurulmuş (A) tanımıyla tamamen aynı değil.

Mevcut kodun CSV’lerinde **tam kademe S2 geçmedi**, **S3 geçmedi** hükümleri destekleniyor. Sade kademenin **7 m’de `N = 5 000`** sonucu ve benekli yolun süreleri doğrulanıyor. Dosya değiştirilmedi; kurulum, derleme veya ölçüm yapılmadı.

## İlk incelemedeki maddelerin durumu

| # | Durum | Not |
|---|---|---|
| 1 | giderildi | `main.cpp:310` ham ortalama ve varyansın mutlak farklarını topluyor; `δ_C` ortalama ilintilerin mutlak farkı. `V/E²` yalnız tanılayıcı. Ardışık iki geçiş aranıyor. |
| 2 | giderildi | `sonar.cpp:194` koni denetimini genlik işaretinden ayırıyor. Negatif dönüş deseni görünürlük sorgusundan ve katkı hesabından elenmiyor. |
| 3 | giderildi | Sade dalında `w = a²` doğrusal toplanıyor; kutu başına, `N`’den bağımsız üstel benek ana yolda etkin. |
| 4 | giderildi | `u = normalize(e1)`, `w = n × u` ortonormal. Eski eğik hücre alanı normalizasyonu kaldırılmış. Yeni tanımdan sapmalar aşağıda. |
| 5 | giderildi | S2’nin `S` kümesi ayrı 5000–5031 tohumlarından kuruluyor; sınanan gerçekleşimler 1000–1031. Hata tablosunun taban satırı için aynı bağımsızlık sağlanmıyor. |
| 6 | giderildi | Sahne etkisi menzile göre sınırlandırılmış; bütün küçük `N` değerlerinin 1,25 ms’yi aştığı genellemesi kaldırılmış. Yeni sayılar CSV’lerle uyumlu. |

## Yeni bulgular

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem | Kapatılacağı yer |
|---|---|---|---|---|---|
| 1 | `tools/olcum/mp7/kaynak/sonar.cpp:93`, `:210`, `:369`; rapor §3 | (A), dünya uzayında sabit hücre ve hücre+tohumdan faz tanımlıyor. Kod hücreleri hareketli gövdeye bağlıyor; ayrıca yol türünü toplama ve faz anahtarına katıyor. Aynı yüzey hücresi farklı yol türlerinde farklı faz alıyor. Bu yalnız adlandırma değildir: aynı gecikmedeki eşit ve zıt iki katkı ortak fazla sönerken ayrı rastgele fazlarla beklenen enerji sıfır olmaz. Dolayısıyla CSV’ler dondurulmuş modelin bütünüyle uygulanmış olduğunu kanıtlamıyor. | Dünya/gövde sabitliği ve yol başına faz ayrımını mimarın tanımıyla uzlaştır. Mevcut değişkeyi açıkça ayır; tanım değiştirilmeyecekse etkilenen istatistikleri uygun modelle yenile. | kritik | MP7 model tanımı, kod ve rapor; ardından 05 §2.4.3 |
| 2 | `tools/olcum/mp7/kaynak/main.cpp:403`, `:454`, `:474`; rapor §4.6, §9 | Eşli referans, ortak benek alanında sayısal farkı ölçmek için anlamlıdır; fakat görevde istenen bağımsız referans karşılaştırmasıyla aynı değildir. “Gürültü tabanı”nda karşılaştırılan `Rf`, aynı zamanda `S`’yi seçmiştir. Üstelik bağımsız iki küme aynı bootstrap indisleriyle yeniden örnekleniyor. Bu satır temiz ve genel bir “M=32 belirsizlik tabanı” sayılamaz. | Eşli tabloyu bu kapsamla koru. Bağımsız karşılaştırmada `S`’yi üçüncü bir kümeden sabitle; iki örneklemi bağımsız yeniden örnekle. Mevcut tabanı yalnız gözlenen iki kümenin farkı olarak yorumla. [Bootstrap ayrımı](https://docs.scipy.org/doc/scipy/reference/generated/scipy.stats.bootstrap.html). | önemli | MP7 hata çözümlemesi ve rapor §4.6, §6, §9 |
| 3 | `docs/olcumler/MP7-sonar-suresi.md:18`, `:153`, `:159`, §9 | Taranan ızgarada S2 bulunamaması, bütün olası `N` değerleri için `N > 1 280 000` alt sınırı vermez. Ölçülen süre artışı da ölçülmemiş bütün büyük `N`’lerde `p99 ≥ 504 ms` garantisi değildir. Özellikle varyans yakınsaması tekdüze değil. | S3’ü “geçmedi” bırak; kanıtı taranan yapılandırmalar ve `N = 1 280 000` destek koşusuyla sınırla. Evrensel alt sınır ve süre garantisi cümlelerini çıkar. | önemli | MP7 raporu Özet, §4.4, §6 ve §9 |

## Yeniden hesaplananlar

**Enerji normalizasyonu:** sabit alan `A`, sabit `σG` ve `m` eşit parçayla:

\[
w_j=\sigma G\,A/m,\qquad
\mathbb E|p_c|^2=\sum_jw_j=\sigma GA.
\]

Yeni toplama, eski `m` ile büyüme veya sade kademedeki `1/m` ile küçülme hatasını kaldırıyor. Bu, sonlu ışın örneklemesinin her gerçekleşimde tam aynı sonucu vereceği anlamına gelmez. Hücre kimliğinde, `g` ve faz üretiminde `N` bulunmuyor. Negatif sinc örneği yeniden **−0,21220659** çıktı; işaret artık korunuyor.

**S2:** CSV’lerde altı eşik yeniden uygulanınca kayıtlı bütün `adim_gecti` değerleri doğrulandı.

| Koşul | Yeniden elde edilen sonuç |
|---|---|
| 7 m tam, son adım | `δ_E = 0,01466`, üst `0,02719`; `δ_V = 0,04615`, üst **0,09283**; `δ_C = 0,00104`, üst `0,00224`: geçmedi |
| 50 m tam, son adım | `δ_E = 0,01832`, üst `0,03932`; `δ_V = 0,09812`, üst **0,17462**; `δ_C = 0,00294`, üst `0,00779`: geçmedi |
| 7 m sade, benekli | İlk ardışık iki geçiş **5 000 → 9 996 → 20 000**; S2’nin seçtiği `N = 5 000` doğru |

Kod, (B)’nin formüllerini ve bağımsız `S` seçimini uyguluyor. CSV’deki `|S|` değerleri tam kademede **4157 / 1395**, benekli sadede **1782**. Ancak kutu×gerçekleşim profilleri özetlerde bulunmadığından bu kümeleri, ham `δ` değerlerini ve bootstrap yüzdeliklerini bağımsız yeniden üretmek mümkün değil; yeniden üretilen, CSV’lerden verilen geçme hükümleridir.

**Süre:** `sure2.csv` içinde **248 benzersiz yapılandırma: 216 tam, 32 sade**, hepsinde 200 ping var. `ozet.py` çıktısı raporun **Ek A’sıyla birebir aynı**. Bütçeye giren taranmış en büyük `N` tabloları da doğru.

| Koşu | p50, ms | p99, ms | En büyük, ms |
|---|---:|---:|---:|
| Tam S3, 1 280 000 ışın, 2000 ping | 291,6193 | **504,4458** | 544,6825 |
| Benekli sade, 5000 ışın, 7 m | 0,5365 | **0,9277** | 0,9343 |
| Benekli sade, 5000 ışın, 50 m | 0,6881 | **0,7926** | 0,8228 |

S3 p99’u, 9,33 ms’nin **54,07**, 25,17 ms’nin **20,04** katı. Ek A’daki eski, beneksiz sade sürelerinin ayrımı raporda açıklanmış.

Ping hesabı yeniden **6,9975 / 49,995 m**, dinleme süresi **9,33 / 66,66 ms** verdi. Üreticinin 50 m’de 33 saniye değeri ve protokolün 25 ns birimi canlı kaynaklardan doğrulandı; **15,84 ms kalanın kısa menzile taşınması varsayımdır**. [Üretici](https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/), [protokol](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/).

## İskelete ya da diğer bölümlere notlar

05’in eski hücre kuralındaki `E = σAm` hatası hakkındaki yazar gerekçesi doğru. Yeni kural 05’e aktarılmadan önce yeni bulgu 1’deki model ayrımları netleşmeli; dondurulmuş S2 ölçütü gevşetilmemeli.

Hata tablosu açıkça **ölçüt olmayan** tablo olarak sunuluyor. İskelet §8 için nihai karar cümlesi veya Windows süre tahmini bulunmuyor.

“Doğrulanmayanlar” temel platform ve bütünleme sınırlarını kapsıyor; bunlara hücre/faz sapmaları, henüz yakınsamamış sonlu referansın sınırı ve gürültü tabanının seçim/yeniden örnekleme sınırları eklenmeli. Sade kademede 50 m için süre ölçülmüş, S2 ayrıca ölçülmemiş.

Embree API’leri, `threads=1` seçeneğinin kurulum iş parçacıklarını sınırlaması ve Homebrew’ün NEON2X ayarı doğrulandı. Bunlar kullanılan ikilinin tam derleme kaydını doğrulamıyor. [Embree](https://raw.githubusercontent.com/RenderKit/embree/v4.4.1/doc/src/api/rtcNewDevice.md), [Homebrew](https://raw.githubusercontent.com/Homebrew/homebrew-core/master/Formula/e/embree.rb). Apple teknik özelliklerinin tam sayfasına erişim **429** nedeniyle başarısız oldu; donanım düzeni bu incelemede tam kaynaktan doğrulanmış sayılmadı. `derleme/` ve `veri/` kapsam dışında tutuldu; özgün profiller ve ham süre yüzdelikleri bağımsız doğrulanmadı.