# sol incelemesi: A5-senaryo

## Hüküm

**Dondurulmaya uygun değil.** Belge, doğrulayıcı ile tüketicilerin bağımsız ve uyumlu yazılmasını henüz sağlamıyor. Örnekler yapısal denetimden geçiyor; ancak olay yaşam döngüsü, çözümleme, sayısal dönüşüm ve rastgelelik kuralları farklı simülasyonlara izin veriyor.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `docs/sozlesmeler/A5-senaryo.md` K62–K64 | Süreli emniyet değişiminin bitmesi tanımsız: örneğin süreli fiş değişiminde eski değer mi geri gelir, yeni değer mi kalır? Çakışan olayların kaldırılması da belirlenmemiş. Uygulamalar farklı yetkili emniyet durumu oluşturabilir. | Durum değişimi ile geçici arızayı ayır; bitiş etkisini, çakışma önceliğini ve aynı sınırdaki başlangıç/bitiş sırasını tanımla. | kritik |
| 2 | Aynı belge K30; `scenarios/ida0.toml:98` | “`itici_modeli` tablolarının uzunlukları eşittir” kuralı örneği reddeder: polinom 5, gerilim dizileri 3, transfer payı 2, payda 4 elemanlıdır. | Eşitliği yalnız dört gerilim tablosu arasında ve ayrı olarak iki havalanma tablosu arasında iste. | kritik |
| 3 | Aynı belge §3.6, K45; `scenarios/ornek-havuz.toml:13` | Orijin yalnız etkin GNSS için zorunlu. Örnek GNSS’i kapatıp orijini atlıyor; fakat etkin IMU’nun varsayılan `dunya_donusu_etkin=true` modeli enlem kullanıyor (`docs/mimari/05-sensorler-ve-sonar.md:133`). | Orijini bu modeli kullanan IMU için de zorunlu kıl ve örneğe mimarca belirlenmiş değer ekle. | kritik |
| 4 | Aynı belge K13–K16, K40, K76 | Çözümleme tek sonuç vermiyor. “Bütün varsayılanlar” HIL’de yasak SIL alanlarını da ekliyor; profilindeki “—” değerler uygulayıcıya bırakılıyor; eksik `baglanti` için somut L0 nesnesi tanımlanmıyor. | Koşullu varsayılanları, eksik tablo üretimini ve profil değerlerini sabitle; `arac.dosya` gibi kaynak alanlarının çıkarılmasını açıkça tanımla. Çözümlenmiş altın örnek ver. | kritik |
| 5 | `scenarios/sema/a5.schema.json`, `$defs/cozumlenmis_senaryo`; belge §3.1, K07 | Şema, zorunlu denilen iki kaynak özetini ve `otopilot` bloğunu istemiyor. Ayrıca `arac.sensorler.x_note=1`, K07’nin her tabloda kabulüne rağmen sensör olarak doğrulanıp reddediliyor. | Çözümlenmiş biçimin zorunluluklarını şemaya işle; koleksiyon girdileriyle deneysel alanların ayrımını belge ve şemada aynı yap. | kritik |
| 6 | Belge K16–K17 ve §7, K16–K17 testi | Ham dosya özetleri kanonik nesnenin içinde. Dolayısıyla anahtar sırası veya yorum değişince çalıştırma özeti değişiyor; §7 ise eşdeğer TOML’nin aynı özeti vermesini istiyor. Bunu bellekte anahtar sırası değiştirerek doğruladım. | Ham kaynak özetlerini davranış özetinin dışında tut veya kaynakları da kanonikleştir; uygunluk testini seçilen kuralla eşleştir. | kritik |
| 7 | Aynı belge K07, K09–K10, K17 | JCS’ye giden dönüşüm eksik: ondalıktan binary64’e yuvarlama, alt taşma ve negatif sıfır politikası belirtilmemiş. Ayrıca `x_date=2026-11-19` kabul edilebilir TOML’dir; “aynen” aktarılacağı JSON/JCS karşılığı yoktur. | Sayısal dönüşümü sınır vektörleriyle sabitle; `x_` değerlerini JSON ile temsil edilebilir türlerle sınırla. [TOML 1.0](https://toml.io/en/v1.0.0), [RFC 8785](https://www.rfc-editor.org/rfc/rfc8785.html). | kritik |
| 8 | Aynı belge K83–K84 | SHA-256 anahtar türetimi açık; fakat Philox4x32/4x64 seçimi, tur sayısı, sayaç sözcüklerine yerleşim ve çıktıdan dağılıma dönüşüm sabit değil. 04/05 atıfları da bunları tamamlamıyor. A6’ya sonradan yazmak seçimi belirlemiyor. | Tek sürümlü RNG sözleşmesi, sayaç eşlemesi ve bilinen sonuç vektörleri tanımla. | kritik |
| 9 | Aynı belge §3.10, K65–K69; AB-093/094 kapanışları | Alan listesi bazı arızaları uygulanabilir kılmıyor: `yanlis_gecerlilik` hangi değeri zorlar; `kismi` seçilen kanala ne yapar; protokol `kesme` kaç bayt keser; `ani_cokme` hangi büyüklüğü değiştirir? Bunlar tanımsız. | Desteklenen her türün parametrelerini ve etkisini kesinleştir; tanımlanamayan türleri reddet veya açık bırak. İlgili kapanışları düzelt. | kritik |
| 10 | Aynı belge §3.2, K23–K24 | Pozitif `trim_kutlesi` kabul ediliyor; kütle, merkez ve atalete nasıl katılacağı yazılmıyor. K23 yalnız torpidoları ekliyor. | Trimin boş kütleye dahil olup olmadığını ve türetim sırasını tanımla; sıfırdan farklı trim test vektörü ekle. | kritik |
| 11 | Aynı belge §3.3, K30 | Şema sıfır polinomunu ve tamamen sıfır transfer paydasını kabul ediyor. İlki azami değere bölmede `0/0`, ikincisi tanımsız transfer fonksiyonu üretir; bunları reddeden anlamsal kural yok. | Normalleştirme böleninin pozitif/sonlu olmasını ve transfer fonksiyonunun geçerli katsayı/mertebe koşullarını doğrula. | kritik |
| 12 | Aynı belge K79; şema `kalite.sonar_tam.isin_sayisi` | HIL’de 1…10⁷ ışının tamamı yalnız genel uyarıyla kabul ediliyor. İskelet §6/§8’in “azaltılmış ışın ve raporlanmış hata” koşulu böyle uygulanmıyor; seçilen sayının MP7’de hata karşılığı bulunmayabilir. | HIL seçimini belirli yapılandırma ve ona ait hata kanıtıyla eşleştir; kanıtsız seçimi ayrıca ele al. Ölçüm sınırlarını evrensel donanım sınırı yapma. | kritik |
| 13 | Aynı belge §9, AB-089 | “Kapandı” gerekçesi geçerlilik sorununu çözmüyor. Havuz örneği `S=0` ve soğurma geçersiz kılması olmadan hâlâ doğrulanmamış varsayılanı kullanıyor; zorunlu uyarı da yok. | Bulguyu açık veya uygulamaya devredilmiş göster; doğrulanmamış kullanımın çözümlenmiş senaryoda ve doğrulama raporunda görünmesini zorunlu kıl. | önemli |

## Yeniden hesaplananlar

`scenarios/ida0.toml` değerlerinden:

| Durum | Kütle, kg | Hacim, m³ | `r_g`, m | `r_b`, m | `Ixx / Iyy / Izz`, kg·m² | `Iyz`, kg·m² |
|---|---:|---:|---|---|---|---:|
| Boş | 13,5 | 0,0137 | (0; 0; 0) | (0; 0; −0,01) | 0,26 / 0,23 / 0,37 | 0 |
| Bir yüklü, f1 | 13,675 | 0,01386755161 | (0; 0,0012797075; 0,0012797075) | (0; 0,0012082278; −0,0086709495) | 0,263528 / 0,232114 / 0,372114 | −0,00175 |
| İki yüklü | 13,85 | 0,01403510322 | (0; 0; 0,0025270758) | (0; 0; −0,0073736314) | 0,267056 / 0,234228 / 0,374228 | 0 |

Ek A, belirtilen `1e-4` bağıl toleransı sağlıyor. NED işaretleri doğru; f2 tek yüklü durumda y bileşenleri ve `Iyz` işaret değiştirir. Üç toplam kütle matrisi Cholesky denetiminden geçti; itici tahsis matrisinin rankı 6.

- Tatlı suda `B−W`: boş **1,964 N**, bir yüklü **1,890857 N**, iki yüklü **1,817714 N**.
- kgf→N: ileri **36,3826715 / 51,4849125 / 65,704555**; geri **28,635418 / 40,207265 / 49,5235825**. Örneğin yuvarlamaları doğru; uç değerler [üreticiyle uyumlu](https://bluerobotics.com/store/thrusters/t100-t200-thrusters/t200-thruster-r2-rp/).
- Basınç hız sınırı: `1/(2×0,01875+0,001) = 25,974026 Hz`. `18 Ah = 64 800 C`; iki saat `7,2×10¹² ns`.
- `2⁵³−1 ns ≈ 104,25 gün`; tamsayı sınırı JCS açısından tutarlı.
- Anahtar türetiminde tohum 1 ve `fizik/dalga` için SHA-256: `570da964f86c050a90b5ea1c153ea64195bd3a09e29e8f7b38c8213608a24ad3`.

## İskelete ya da diğer bölümlere notlar

İki örnek Python 3.12 `tomllib` ile ayrıştırıldı. Kurulu `jsonschema` paketi yoktu; kurulum yapmadan, şemada kullanılan doğrulama anahtarlarını kapsayan bellek içi yapısal denetim uyguladım. İki örnek geçti; bilinmeyen anahtar, negatif kütle ve kanal 8 bozunumları reddedildi. Bu sonuç, anlamsal kuralların veya A4/A7 ile bütünleşmenin geçtiği anlamına gelmez.

İDA-0 aktarımı 04 §2.6 ve karar belgesi §4.4 ile uyumlu. Yayımlanmış katsayıların niteliği ve değiştirilmiş hacmin varsayım olarak işaretlenmesi [özgün makaleyle](https://www.mdpi.com/2077-1312/10/12/1898) uyumlu. ESC değerleri [üreticiden](https://bluerobotics.com/store/thrusters/speed-controllers/besc30-r3/), paket değerleri [batarya sayfasından](https://bluerobotics.com/store/comm-control-power/powersupplies-batteries/battery-li-4s-18ah-r3/) denetlendi.

A5 başlığındaki **11 bulgunun tamamı** kapanış tablosunda var; ancak AB-089, AB-093 ve AB-094 için kapanış gerekçeleri yukarıdaki eksikleri karşılamıyor.

K80’in görünüm kapalıyken kamerayı yasaklaması için mimar açıklaması gerekiyor: iskelet §6 özelliklerin ayrı seçilmesini söylüyor, aynı tabloda görünüm kapalıyken görselleştiriciyi durduruyor.

Torpido/fırlatıcı alanlarında kapsamı genişleten veya atış çözümünü ayrıntılandıran yeni alan görmedim. NPL soğurma sayfalarına ve TDK DS-000347 rev 1.6 bağlantısına erişilemedi; bunların tam içerikleri bu incelemede doğrulanmış sayılmadı.