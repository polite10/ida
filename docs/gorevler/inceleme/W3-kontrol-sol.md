# sol incelemesi: kontrol

## Hüküm

**Bu hâliyle kabul edilemez: 4 kritik, 2 önemli bulgu.**

Silahsızlık, kısıtlama, yetersiz kestirim ve geçersiz istek için doğrudan sıfırlama yolları mevcut. Ancak sonlu girdilerden doğan sayısal taşma sıfırdan farklı itkiye dönüşebiliyor; tahsis önceliği de bazı geçerli durumlarda ters işaretli moment üretiyor.

İnceleme kaynak okuması ve bağımsız sayısal hesaplarla yapıldı. Dosya değiştirilmedi; derleme ve testler çalıştırılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `core/src/kontrol/kontrol.c:321`, `core/src/kontrol/kontrol.c:480` | **Sonlu olmayan ara sonuç kırpılarak gizleniyor.** Örneğin geçerli hız isteği ve sonlu `v_ned[2] = −FLT_MAX`, `ham_w = +∞` üretiyor. Satır 322 bunu 80 N’a kırpıyor; integral donduğu için son denetimde incelenen değerler sonlu kalıyor. Sonuç: `cokme = 0` ve sıfırdan farklı itki. | Kırpma ve doymayı işleyen işlemlerden **önce** hata, ham PI çıktısı, dönüşüm ve tahsis ara sonuçlarını sonluluk açısından denetle. Başarısızlıkta bütün çıktıları sıfırla ve çökme bildir. Sonlu girdiden taşma üreten sınır testleri ekle. | kritik |
| 2 | `core/src/kontrol/kontrol.c:436`; `core/test/kontrol/test_tahsis.c:161` | **Tahsis sonrasındaki bağımsız hız sınırlaması eksen önceliğini bozuyor.** Önceki itkiler sıfırken, 5 ms adımda `X = 40 N, M = +1 N·m` isteği sonrasında hesaplanan moment yaklaşık **−0,29948 N·m** oluyor. Yalnız `M` istendiğinde ise aynı sınırlayıcı yaklaşık **+0,60000 N·m** veriyor. Alt öncelikli öteleme, üst öncelikli momentin işaretini tersine çeviriyor. Test yalnız tahsis işlevini çağırdığı için bunu görmüyor. | Değişim hızı pencerelerini tahsisin uygulanabilir itici sınırlarına kat; öncelikleri bu sınırlar altında koru. Kabulü `ida_kon_hesapla` çıktısına kadar sınayan birleşik test ekle. | kritik |
| 3 | `core/src/kontrol/tahsis.c:193`, `core/src/kontrol/tahsis.c:430`; `core/test/kontrol/test_tahsis.c:266` | **Rank kaybında grup sırası öncelik sağlamıyor.** T5 ve T7 kapalıyken sönümlü tersin farklı eksen sütunları birbirini etkiliyor. `K = +4 N·m` isteğine alt öncelikli `Z = +10 N` eklemek, elde edilen yalpa momentini pozitiften negatife çeviriyor. Mevcut iki arıza testi yalnız itici sınırlarını denetliyor. | Rank kaybını açıkça ele al. Üst öncelikli eksenlerde elde edileni koruyan tahsis uygula veya korunamayan durumda tanımlı güvenli kısıtlama kullan. Arıza maskeleri altında moment işareti ve öncelik testi ekle. | kritik |
| 4 | `core/src/kontrol/kontrol.c:346`, `core/src/kontrol/kontrol.c:349`, `core/src/kontrol/kontrol.c:353` | **Karma açı/hız kanallarında kinematik dönüşüm bozuluyor.** Euler hızları gövdeye çevrildikten sonra bazı gövde bileşenleri değiştiriliyor; kalan bileşenler yeniden çözülmüyor. Örneğin `TUTUM_HIZ`, `YON_ACI`, `φ = 0,5`, `θ = 0`, istenen `p = q = 0` ve dış döngü `ψ̇ = 0,35` için üretilen `r = 0,307154`, gerçekte `ψ̇ = 0,269553` verir. | Seçili kanalların Euler ve gövde kısıtlarını birlikte çöz. Karma kanal seçimlerini sıfırdan farklı yalpa/baş-kıç altında sınayarak istenen hızların dönüşüm sonrası korunduğunu göster. | kritik |
| 5 | `core/src/kontrol/tahsis.c:221`, `core/src/kontrol/tahsis.c:228` | **Cholesky köşegen oranı koşul sayısı sınırı değildir.** Kodun `κ ≤ 10⁶` diye kabul ettiği denetim, aşağıdaki geçerli geometride yaklaşık `κ₂(BBᵀ) = 6,243·10⁶` olmasına rağmen geçiyor. Böylece kötü koşullu parametreleri reddettiği iddiası sağlanmıyor. | Gerçek koşul sayısını güvenilir biçimde tahmin eden bir denetim kullan; eşik çevresindeki kötü koşullu fakat tam ranklı geometrileri test et. | önemli |
| 6 | `core/src/kontrol/kontrol.c:442`, `core/src/kontrol/kontrol.c:488` | **Sarma önleminin kalıntısı ölü banttan önce hesaplanıyor.** Dört dikey iticiye 0,1 N ayrıldığında hesaplanan toplam 0,4 N ve kalıntı yaklaşık sıfır; eğri tersinden sonra bütün komutlar sıfır olduğundan nominal gerçekleşen toplam sıfırdır. İntegral, gerçekte elde edilmeyen kuvveti elde edilmiş sayıyor. Bu, açık bulgu AB-128’in sürmesidir. | Kalıntıyı son ESC komutundan geri hesaplanan nominal itkiyle oluştur; ölü bant ve nominal nicemleme etkisini hesaba kat. Eşik çevresinde kalıntı ve integral davranışını sınayan test ekle. | önemli |

## Yeniden hesaplananlar

- **Geometri ve işaretler:** `Bᵢ = [εᵢ; rᵢ × εᵢ]` hesabı varsayılan geometriyle uyumlu. Yatay grubun çapraz momentleri `K = −0,085Y`, `M = +0,085X`; yön momenti kolunun büyüklüğü yaklaşık `0,18879751 m`. Kuaterniyonun `(w,x,y,z)` sırası ve gövde → NED dönüşümü doğru okunuyor.
- **Sönümlü ters:** varsayılan geometride `λ = 10⁻⁶·iz(BBᵀ)/6 ≈ 1,40320·10⁻⁶`. Tam ranklı, doymayan tek eksen örneklerinde desenler beklenen yönde. Bu sonuç rank kaybındaki öncelik güvencesine taşınamıyor.
- **Taşma örneği:** `60·FLT_MAX ≈ 2,04169·10⁴⁰`, binary32 aralığını aşıyor. Sonraki kırpma 80 N, 5 ms hız sınırı dikey itici başına 1,25 N; ters eğri yaklaşık **0,07637** komut veriyor. Microsoft belgeleri `FLT_MAX` değerini ve `/fp:precise` altında özel değer davranışını doğruluyor. [Kayan nokta sınırları](https://learn.microsoft.com/en-us/cpp/c-language/limits-on-floating-point-constants?view=msvc-170), [/fp davranışı](https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior?view=msvc-170).
- **Öncelik karşı örneği:** `X=40, M=1` için tahsis yaklaşık `[14,142124 × 4; +4,999872 × 2; −4,999872 × 2] N`. Her bileşenin ±1,25 N’a ayrı kırpılması `M ≈ −0,29948 N·m` üretiyor.
- **Rank kaybı:** T5/T7 kapalıyken formülün bağımsız binary64 hesabında yalnız `K=4` için elde edilen `K≈+0,20763`; `Z=10` eklendiğinde `K≈−1,85921`. Binary32 işlem tekrarı da işaretin tersine döndüğünü gösteriyor.
- **Karma kanallar:** `ψ̇=(sinφ·q+cosφ·r)/cosθ`. Yukarıdaki örnekte doğru `r≈0,398823` olmalı. Kullanılan kinematik bağıntı birincil kaynakla denetlendi. [Fossen, Kinematics](https://www.fossen.biz/html/marineCraftModel.html).
- **Koşul denetimi:** yalnız T5/T6 için `rx=2`, T7/T8 için `rx=1,996 m` yapıldığında mevcut Cholesky oranı yaklaşık `0,0018911` ile geçiyor; bağımsız özdeğer hesabında `κ₂(BBᵀ)≈6.243.225`.
- **Eğri ve ölü bant:** varsayılan polinomun binary32 normalleştirme böleni yaklaşık `30,621708`; tabloya göre `F_şekil(0,065)≈0,02054253`. Sıfırlama eşikleri ileri yaklaşık `0,512947 N`, geri `0,400785 N`. Testteki yüzde birlik örneklerde ters eğrinin en büyük normalleştirilmiş kuvvet hatası yaklaşık `0,000596`. `26/400=0,065` hesabı doğru; 1500 µs merkez ve ±25 µs dur bandı üretici kaynağıyla doğrulandı. [Blue Robotics teknik başvurusu](https://bluerobotics.com/learn/technical-reference/).

## İskelete ya da diğer bölümlere notlar

- `docs/gorevler/W3-kontrol.md` ve `W3-ortak.md` çalışma ağacında bulunmadığından `origin/main` üzerinden okundu.
- A4-K54 açısından eğri tersi ve ölü bant doğru katmanda bulunuyor. Bulgular 1–4 kazanç ayarı veya üslup sorunu değildir.
- `core/src/kontrol/kontrol.c:536` hâlen sürekli `KAPALI` istek üretiyor. Bu nedenle mevcut bütünleşik sıfır çıktı testi, etkin denetleyicinin gerçek çıktı kapısıyla emniyet davranışını kanıtlamıyor.
- MSVC `/W4 /WX /analyze`, sanitizer, Windows derlemesi, kart ölçümü ve `fw-release` boyutu bu incelemede **doğrulanmadı**. Bağımsız W3 emniyet kabul testlerinin tamamlandığına ilişkin kanıt da görülmedi.