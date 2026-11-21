# sol incelemesi: fizik

## Hüküm

**Birleştirmeye uygun değil: dört kritik bulgu var.** Varsayılan parametrelerde hareket denkleminin işaretlerinde veya hidrostatik kırpmanın denetlenen örneklerinde hata saptamadım; kabul edilen diğer girdilerde sayısal doğruluk ve başlatma davranışı bozuluyor.

İnceleme kaynak okuması ve bağımsız hesaplarla yapıldı. Dosya değiştirilmedi, kurulum yapılmadı. Derleme, CTest, ASan ve MSVC `/W4 /WX` sonuçları bu incelemede doğrulanmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `sim/fizik/src/itici.cpp:184`; `sim/fizik/src/dunya.cpp:703` | Sürekli zamanda kararlı filtre kabul edilince RK4 için de uygun sayılıyor. A5-K105’i sağlayan `pay=[1]`, `payda=[0.0001,1]` kabul edilir; fakat 1,25 ms adımda `hλ=−12,5`, RK4 büyütme çarpanı **758,35677** olur. Sıfır başlangıçtan pozitif basamakta ilk çıktı **−757,35677 × F_ss** çıkar; doğru değer **0,99999627 × F_ss**’dir. Sonluluk denetimi, bu yanlış fakat sonlu çıktıyı yakalamaz. | Kabul edilen filtreleri fizik adımıyla birlikte değerlendirin. Filtreyi ve gövdeyle bağlaşımını kararlı biçimde ilerleten bir yöntem uygulayın; hızlı, kararlı birinci dereceden filtreyi bağımsız analitik basamak yanıtıyla sınayın. | kritik |
| 2 | `sim/fizik/src/dunya.cpp:630`; `sim/fizik/test/test_dunya.cpp:57` | ESC yalnız geçen süreye bakılarak hazır oluyor. Adım 0’dan itibaren 1700 µs verilse ve hiç nötr beklenmese bile iki saniye sonunda itki başlıyor. Test de bu yanlış davranışı bekliyor. A5 §3.3’teki alan **nötr bekleme** süresidir; üretici de başlatma için dur komutu verilmesini şart koşar. [Blue Robotics başlatma açıklaması](https://bluerobotics.com/store/thrusters/speed-controllers/besc30-r3/) | Başlatmayı kanal başına nötr komutun tutulmasına bağlayın. Sürekli nötr dışı komutla hazır olunmadığını ve yeterli nötr beklemeden sonra hazır olunduğunu ayrı testlerle doğrulayın. | kritik |
| 3 | `sim/fizik/src/dunya.cpp:221`; `sim/fizik/src/dunya.cpp:431` | Kurulum sonlu olmayan başlangıç çıktısını başarıyla yayımlayabiliyor. Örneğin `dogrusal_sonum[0]=+∞`, yalnız `≥0` denetiminden geçer; durgun başlangıçta `∞×0` NaN üretir. İlk türev ve anlık görüntü denetlenmeden `kur()` başarılı döner. `r_g`, `r_b` ve kullanılan bazı diğer parametrelerde de sonluluk denetimi eksiktir. Sonraki `adim()` denetimi başlangıçta yayımlanan bozuk çıktıyı önlemez. | Kullanılan bütün sayısal parametreleri sonluluk açısından doğrulayın. Kurulumun ilk türevini, türetilmiş büyüklüklerini ve anlık görüntüsünü de denetleyip bozuk kurulumda başarı döndürmeyin. NaN/sonsuz girdiler ve sonlu girdilerden oluşan taşmalar için karşı örnek ekleyin. | kritik |
| 4 | `sim/fizik/src/itici.cpp:67` | Routh hesabı katsayı ölçeğine bağlı yanlış ret üretiyor. Varsayılan pay ve paydayı birlikte `10¹⁵⁵` ile çarpmak aynı transfer fonksiyonunu korur ve bütün katsayılar sonlu kalır; ancak tablodaki çarpımlar `∞−∞` üzerinden NaN olur. `10⁻²⁰⁰` ölçeğinde ise çarpımlar sıfıra alt taşar. İki eşdeğer, kararlı gösterim de A5-K105 denetiminden yanlış biçimde reddedilir. | Routh hesabını güvenli katsayı ve satır ölçeklemesiyle yapın; ara işlemlerin sonluluğunu denetleyin. Aynı transfer fonksiyonunun büyük ve küçük ortak ölçeklerle yazılmış biçimlerinin aynı sonucu verdiğini test edin. | kritik |

## Yeniden hesaplananlar

- **Kütle muhasebesi:** bağlı yük başına hacim `0,0001675516082 m³`; boş/bir/iki yüklü toplam kütleler `13,5 / 13,675 / 13,85 kg`. Bir yüklü durumda `r_g,y=r_g,z=0,0012797075 m`, `I_yz=−0,00175 kg·m²`; iki yüklü durumda `I_xx/I_yy/I_zz=0,267056/0,234228/0,374228 kg·m²`. A5 Ek A ile uyuşuyor.
- **Hareket denklemi:** `M_RB` çapraz blokları, ofsetli kütle merkezinin Coriolis terimleri ve Kirchhoff momenti yeniden türetildi. Sabit akıntıda `ν̇_w=[−ω×Rᵀv_c;0]` işareti doğru. [Fossen’in birincil model açıklaması](https://www.fossen.biz/html/marineCraftModel.html) ile karşılaştırıldı.
- **IMU çıktısı:** `f_b=ν̇₁:₃+ω×v−Rᵀg` doğru özgül kuvvettir. Durgun, düz araçta `(0,0,−9,82) m/s²` verir; yerçekimi çıkarılır. Açısal ivme çıktısı `ω̇` olarak tutarlıdır.
- **Varsayılan filtre:** kutuplar `−13,1598811` ve `−37,9200595±82,5955334i`; 1,25 ms’de RK4 büyütme büyüklükleri `0,98368471 / 0,95370591`. Varsayılan filtrenin kararlılığı, bulgu 1’deki genel kabul sorununu ortadan kaldırmaz.
- **Hidrostatik kırpma:** birim küpte yüzeyle çakışan yüz için hacimler `0/1`, merkezden kesmede `0,5` bulundu. Düşey doğrultusu `(1,1,1)/√3` olan küpün merkezden kesilmesinde hacim `0,5`, batık merkez derinliği `13/(32√3)=0,2345485469`; köşelerden geçen diğer kesitte hacim `5/6`. Kodun kırpma formülleri bu analitik sonuçlarla yuvarlama düzeyinde uyuştu.
- **Denge ve son hız:** boş araç için düz yönelim denge derinliği `0,0446354167 m`, batık oran `0,9854014599`, heave doğal frekansı `7,6544385 rad/s`. İleri `40 N` için son hız `0,48625310 m/s`; `85 N` için `0,72936256 m/s`.

## İskelete ya da diğer bölümlere notlar

- Fizik adımı **1,25 ms** olarak doğru uygulanmış. Görev belgesindeki 400 Hz ifadesi bulgu sayılmadı.
- `M_A`’nın yüzey geçişinde sabit tutulması ve sönümün batıklıkla ölçeklenmesi, 04 §2.4’te kabul edilen model yaklaşımıdır.
- Salınım ve son hız testlerinin analitik beklentileri bağımsızdır. Filtre referansı farklı kanonik biçim kullanıyor; buna karşılık hızlı filtre ve katsayı ölçekleme sınırlarını kapsamıyor. ESC başlatma testi ise bulgu 2’deki hatalı davranışı kabul ölçütüne dönüştürüyor.
- İskelet veya sözleşme değişikliği gerektiren yeni bir çelişki saptamadım. Bulgular uygulama ve test düzeyindedir.