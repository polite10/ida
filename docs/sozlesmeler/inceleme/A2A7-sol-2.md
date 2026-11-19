# sol ikinci incelemesi: sozlesmeler

## Hüküm

**Dondurulmaya uygun değil.** A2’de halka emekliliği ve atlama sonrası sahnenin kurulması, A7’de örtü denetimi için üç kritik bulgu kaldı. İlk dokuz bulgunun sekizi giderildi; örtü denetimi kısmen giderildi.

## İlk incelemedeki maddelerin durumu

| # | Durum | Not |
|---|---|---|
| 1 | giderildi | A2-K42, K56–K59 okunuyor olabilecek yuvanın yeniden yazılmasını ve zaman aşımıyla geri alınmasını yasaklıyor. Yeni emeklilik protokolünün açığı Y1’de. |
| 2 | giderildi | A2-K58 gönderen tüketicinin başvurusunu yalnız bir kez kaldırıyor. Bağlantı kapanışını bırakma saymayan daha sıkı kuralın gerekçesi doğru: bağlantısı kopan süreç okumayı sürdürebilir. |
| 3 | giderildi | A2-K80–K85 yanıtsız, hazır ve tüketiciye verilmiş eski çalıştırma nesli karelerini ayrı ayrı ele alıyor. Sayaçların sıfırlanmaması eski yanıtların yeni isteklerle karışmasını önlüyor. |
| 4 | kısmen | Köşe denetimi kaldırılmış; delikli prizma reddediliyor. Ancak A7-K44 gerçek birleşim sınırını doğru oluşturmuyor ve ileri sürülen sınırı kanıtlamıyor; Y2. |
| 5 | giderildi | A2-K21, K32 ve K41’de `T` olay sonrası durum; `[T−h,T)` eski son durum; sınırda ara değerleme yok. |
| 6 | giderildi | A7-K19 erişilebilirliği, K22 ise Godot eşlemesi yoksa reddi zorunlu kılıyor. Gerekçe [glTF hiyerarşi kuralı](https://raw.githubusercontent.com/KhronosGroup/glTF/main/specification/2.0/Specification.adoc) ve [`get_scene_node`](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/modules/gltf/doc_classes/GLTFState.xml) belgesiyle uyumlu. |
| 7 | giderildi | A2-K75–K79 porttan bağımsız sahiplik kilidi, atomik kayıt güncellemesi ve oluşturma öncesi kayıt sırasını tanımlıyor. |
| 8 | giderildi | A7-U11 farklı tohumda farklı geometri şartını kaldırmış; aynı tohumla tekrar ve parametre aktarımı ayrı sınanıyor. [Sürüm notları](https://github.com/SarahWeiii/CoACD/releases/tag/1.0.14) bu gerekçeyi destekliyor. |
| 9 | giderildi | Şemada 19 ayar var. [API’nin](https://raw.githubusercontent.com/SarahWeiii/CoACD/1.0.14/public/coacd.h) açtığı 17 ayar açık veriliyor; diğer ikisinin sabit değerleri [kurucuyla](https://raw.githubusercontent.com/SarahWeiii/CoACD/1.0.14/src/config.h) uyuşuyor. Kitaplık çağrısına geçişin gerekçesi doğru. |

## Yeni bulgular

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem | Kapatılacağı yer |
|---|---|---|---|---|---|
| Y1 | **A2:** `docs/sozlesmeler/A2-gorsellestirici.md:429`, K46, K56, K61, K85 | Emeklilik yalnız yanıtsız istekleri düşürüyor. Eski halkadaki **hazır** kare, çalıştırma nesli değişmediğinden teslim koşulunu koruyor; yeni halkayla el sıkışmış tüketiciye eski halka karesi verilebilir. Emeklilik sırasında gelen yanıtların terminal davranışı da tanımsız. | Emekli halkadan yeni teslimi kapat; hazır kareleri düşür; sonraki yanıtları yalnız eski isteği kapatmak için işle. Eski eşlemeyi devam eden okuma/yazma bitmeden bırakmama koşulunu yeniden el sıkışmaya bağla. | kritik | A2-K56–K65; A8 yenileme sözleşmesi |
| Y2 | **A7:** `docs/sozlesmeler/A7-sahne.md:185`, K44 | Bitişik iki parçanın ortak iç yüzeyi, diğer parçanın **kesin içinde** olmadığı için tutuluyor. Birim küpün iki yarıya kusursuz ayrıştırılması bile ortak yüz merkezinde **0,5 m** hata bulunarak reddedilir. Ayrıca örnek silme sonrası gerçek sınırın δ ile kapsandığı gösterilmiyor. | Gerçek birleşim sınırını, ortak iç yüzeyleri çıkararak oluştur; bu sınırı yeniden örnekle ve δ kapsama koşulunu güvenceye al. | kritik | A7-K44, A7-U12 |
| Y3 | **A2:** `docs/sozlesmeler/A2-gorsellestirici.md:390`, §3.4 | `atlama` yalnız geçmişi temizliyor. Doğum/ölüm üzerinden atlanınca yeni zamanda yaşayan gövdelerin varlık eşlemesi gönderilmiyor; `Durum` içinde `varlik_indeksi` de yok. Alıcı bilinmeyen/ölmüş gövdeyi reddedebilir veya yanlış sahne kurabilir. | Atlamadan sonra yaşayan gövdelerin tam tablosunu ve oturum içindeki kimlik eşlemesini, ilk durumdan önce yeniden bildir. | kritik | A2-K29; atlama uygunluk testi |

## Yeniden hesaplananlar

- **26 yapının boyutu ve 52 `offsetof` değeri uyuşuyor.** Özellikle `KareHazir=40`, `HalkaEslendi=16`, `KameraIstegi=96`, `SahneOlayi=120`, `Hosgeldin=600`, `HalkaBasligi=128`, `YuvaBasligi=256 B`. [Windows x64 yerleşim kurallarıyla](https://learn.microsoft.com/en-us/cpp/build/x64-software-conventions?view=msvc-170) da hesap aynı; MSVC çalıştırılmadı.

| Kamera | Piksel/kare | Yuva | 8 yuvalı halka | Piksel akışı |
|---|---:|---:|---:|---:|
| 640×480, 30 Hz | 921.600 B | 925.696 B | 7.409.664 B | 27,648 MB/s |
| 320×240, 15 Hz | 230.400 B | 233.472 B | 1.871.872 B | 3,456 MB/s |

- **Örtü sınırı:** `B_S⊂S` ve `B_D⊂∂D`, kendi yüzeylerinin δ örtüleri ise üçgen eşitsizliğiyle  
  `d_H(S,∂D) ≤ δ + d_H(B_S,B_D) ≤ δ + (τ−δ) = τ`.  
  Dolayısıyla **tek δ payı yeterlidir**; sorun formülde değil, filtrelenmiş `B_D` için gereken koşulların sağlanmamasındadır.
- Y2 karşı örneğinde gerçek Hausdorff uzaklığı **0**; tutulan ortak yüz merkezi kaynak yüzeyden **0,5 m** uzaktadır. `τ=0,01 m` için ölçüt **0,008 m** olduğundan doğru ayrıştırma reddedilir.
- Delikli prizmanın `(1,0,0)` noktasındaki fark **1 m**; izin verilen `τ≤1 m` için `0,8τ<1 m`, ret doğru. U12’de büyütülen küplerin yüz merkezindeki fark **2,5 mm**, gerçek Hausdorff uzaklığı köşede **4,330127 mm**.
- `det(C)=det(M)=+1`, `CM=diag(−1,1,−1)`. 60 Hz yayın aralığı **13⅓ adım**. Örnek hacmi **0,0136992 m³**, yüz köşegeni **0,6248199741 m**.
- Örnek manifest yapısal şema denetiminden geçti. Tam JSON Schema doğrulayıcısı kurulu değil; örneğin GLB dosyası bulunmadığından geometrisi doğrulanmadı.

## İskelete ya da diğer bölümlere notlar

- Kamera kademeleri, tek kamera ve pozlama sonu+süre gösterimi verilen kararlarla uyumlu. İskelet §10/25’teki “kademe değerleri A8’de” kaydı güncellenmeli.
- **Bellek görünürlüğü doğrulanmış değil.** K53’ün platform varsayımı açıkça yazılmış; ancak atomik alanları kaldırmak tek başına yeterlilik kanıtı değildir. [C++20 çit eşzamanlaması](https://raw.githubusercontent.com/cplusplus/draft/n4861/source/atomics.tex) atomik işlemler üzerinden tanımlanır; [Arm bariyer kuralları](https://support.arm.com/documentation/102336/0100/Limiting-the-scope-of-memory-barriers?lang=en) tek başına uçtan uca soket güvencesini kanıtlamaz. A2-A10 açık kalmalı.
- macOS’ta adın kaldırılması ile mevcut eşlemenin ömrü ayrıdır; Windows’ta yalnız sim tutamağını kapatmak mevcut görünümleri geçersiz kılmaz. Bu ayrım Y1’deki teslim ve eşleme kurallarının yerini tutmaz. [Apple kaynak kodu](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/kern/posix_shm.c), [Microsoft eşleme yaşam döngüsü](https://learn.microsoft.com/en-us/windows/win32/memory/closing-a-file-mapping-object).
- Open Group’un bellek sıralaması ve `shm_unlink` sayfaları 403 verdi; doğrulanmış sayılmadı. Dosya değiştirilmedi, kurulum yapılmadı.