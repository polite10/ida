# sol incelemesi: sunucu

## Hüküm

**Emniyet kapsamı KAPANMADI.** `fa12abe` ucunda, `54f6ed1` dahil dar denetimde üç kritik bulgu var. Önceki #3’ün ACTUATOR karşı örneği düzeltilmiş; ancak EOF ile birleşen sınır durumu nedeniyle #3 kapanmadı.

İnceleme statiktir; dosya değiştirilmedi, test çalıştırılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `sim/sunucu/src/sunucu.cpp:299`, `:664` | **EMNİYET / belirlenimcilik:** Boş yuva beklenirken akış `4.194.304 × 0x00 → FIN` olsun. İlk 4.194.303 bayt tüketildikten sonra son sıfır ve EOF aynı pompa turunda görülürse `sil_besle`, bütçe denetiminden önce `baglanti_koptu` çağırır. Durum artık `kosuyor` olmadığından PROTOCOL atlanır. FIN sonraki tura gecikirse aynı son baytta PROTOCOL üretilir. Aynı akış farklı kapanış çıktısı verir; W4-K26(2)–(3) ihlalidir. | Son tüketilen baytın bütçe kararını EOF işlemeden önce kesinleştir. Son sıfırla EOF’un birlikte ve ayrı görülmesini karşılaştıran sınama ekle; `sil_butce_konumu` yalnız ACTUATOR sonlu akışları kapsıyor. | kritik |
| 2 | `sim/sunucu/src/sunucu.cpp:550`, `:593`, `:612`, `:676` | **EMNİYET / sınırsız bekleme:** Hazırlıkta her ilk TF’den sonra kart oturumu kapatıp yeniden PREP gerektiren el sıkışmayı tamamlasın. Çağrı zinciri `hazirlik → yuva_bekle → oturum_kur → hazirlik` olarak derinleşir. Her yeni çağrı hazırlık başlangıcını ve el sıkışma süresini yeniler; dış çağrılar kendi süre denetimlerine dönemez. Toplam 600 s sınırı aşılabilir ve yığın sınırsız büyür. | Yeniden kurmayı özyinelemesiz yürüt; aynı hazırlık işleminin yeniden bağlantılar boyunca korunan mutlak son tarihini bütün beklemelere taşı. Tekrarlayan PREP/kapanış sınaması ekle. | kritik |
| 3 | `sim/sunucu/src/sunucu.cpp:595`, `:1445` | **EMNİYET / güvenli kapanış:** Hazırlıkta ACTUATOR’lar 100 ms aralıkla gelirken komut sonuçları gelmesin. 600 s dolunca doğrudan `false`, ardından `kos`tan dönüş gerçekleşir. Olağan kapanış yolu (`:1579`) çalışmaz; `SESSION_CLOSE` gönderilmez. HIL’de kartın oturum sonu daraltması `T_bağ` zaman aşımına bırakılır; A1-K21 ihlalidir. Mevcut hazırlık testi yalnız hata metnini ve dönüş süresini denetliyor. | Kurulmuş oturumun bütün hata çıkışlarını ortak güvenli kapanıştan geçir: yerel daraltma, kapanış çerçevesinin süreyle sınırlı boşaltılması ve TCP’de düzgün kapatma. Kartın SESSION_END çağrısını da sınamada doğrula. | kritik |

## Yeniden hesaplananlar

- **4 MiB = 4.194.304 B**, **64 MiB = 67.108.864 B**; **1 MiB = 64 × 16.384 B**.
- Bütçede ACTUATOR’ın son baytı tam sınırdaysa kabul, bir bayt dışarıdaysa PROTOCOL doğru uygulanıyor; EOF önceliği yukarıdaki istisnadır.
- Sayaç tabanı her `yuva_bekle` girişinde yenileniyor. Kuyrukta kalan baytlar sonraki beklemede tüketildikleri miktarla sayılıyor; burada parçalanmaya bağlı fark bulmadım.
- `8000 × 5 ms = 40 s` hazırlık TF zamanıdır; duvar saatindeki 600 s sınırını sağlamaz.

## İskelete ya da diğer bölümlere notlar

64 MiB geri basınç yalnız SIL dalındadır; HIL okumalarına sızmıyor. Yuva beklemesi okumadan önce kuyruğu tüketiyor; dolu kuyruk nedeniyle karşılıklı bekleme veya `T_SIL` denetimini atlayan yeni yol bulmadım (`sunucu.cpp:663,678–679`).

El sıkışmada tüketim bayt sınırı yoktur; sonlu kuyruk beslemesinden sonra duvar süresi denetlenir. Artan sıfırlar tek başına sonsuz PROTOCOL döngüsü üretmez. Olağan el sıkışma/yuva beklemelerinde fizik ve senaryo zamanı ilerlemiyor; hazırlık kendi TF saatini yürütüyor.

EOF karşı örneğinin dayandığı kısa okuma ve veri bittikten sonra `recv = 0` davranışı canlı birincil kaynaklarla doğrulandı: [Apple recv](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/recv.2.html), [Microsoft recv](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-recv).