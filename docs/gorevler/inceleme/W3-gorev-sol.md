# sol incelemesi: gorev

## Hüküm

**Bu hâliyle kabul edilmemeli.** Statik incelemede 5 kritik, 1 önemli bulgu saptandı. Özellikle kip değişiminden sonra eski arıza görüntüsüyle ARM kabul edilebiliyor ve aynı çağrıdaki sızıntı eşik aşımı kaybolabiliyor.

Hedefin tamamı, güdüm bağlantısı ve belirtilen birim testleri okundu. Karşı örnekler kaynak akışından çıkarıldı; derleme ve test çalıştırılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `core/src/gorev/gorev.c:223`, `core/src/gorev/gorev.c:280`, `core/src/gorev/gorev.c:320` | **Kip değişince arıza düzeyi ve ARM engeli eski kipten kalıyor.** Örnek: silahsız `DEPTH_HOLD`, süregelen GCS kaybı nedeniyle `WARN`, geçerli yatay kestirim. Aynı çağrıda `SET_MODE(GUIDED)` ardından `ARM` kabul edilir. Oysa aynı GCS koşulu `GUIDED` sütununda `HOLD` gerektirir ve ARM ön koşulunu bozar. Çağrı sonuna kadar `fs_level = WARN` kalır. Bu, W3-K2/K3 ve A4 §3.17 ARM koşulunu aşar. | Hedef kipin arıza etkilerini genişletme kabulünden önce değerlendirin. Kip değişiminin ağırlaştırdığı arıza aynı çağrının daraltma ve ARM kararına katılmalı. İki komut sırasını da sınayın. | kritik |
| 2 | `core/src/gorev/gorev.c:93`, `core/src/gorev/gorev.c:99`, `core/src/gorev/ariza.c:212` | **Sızıntı eşiğinin çağrı içindeki aşımı kayboluyor.** `SIZINTI_N = 2` iken aynı sensörden kabul edilen `ıslak, ıslak, kuru` örnekleri aynı çağrıya gelirse sayaç `0→1→2→0` olur. Arıza değerlendirmesi bütün örneklerden sonra yapıldığı için sızıntı mandalı hiç kurulmaz. İki ardışık ıslak örnek gerçekleşmiş olmasına rağmen SURFACE ve ARM engeli oluşmaz. | Eşik aşımını örnek işlenirken kaydedin; sonraki kuru örnek bu tetik kaydını silememeli. Aynı çağrıda eşik aşımı ve kuruma vakasını ekleyin. | kritik |
| 3 | `core/src/gorev/ariza.c:172`, `core/src/gorev/gorev.c:120` | **Yatay geçerlilik iki farklı tanımla değerlendiriliyor.** Kip ön koşulu konum **ve yatay hız** isterken arıza hesabı yalnız `KONUM` bitine bakıyor. `GUIDED` sırasında yalnız `YATAY_HIZ` geçerliliği kaybolursa `nav_konum_var` yanlış olur fakat konum kaybı zamanlayıcısı başlamaz. Kip kalır; beklenen HOLD ve uzun kayıpta SURFACE oluşmaz. W3-K5’in bileşen temelli arıza kararı sağlanmıyor. | Kip kabulü, kip düşmesi ve arıza hesabında aynı doğrulanmış yatay geçerlilik koşulunu kullanın. Konum ve yatay hız kaybını ayrı ayrı sınayın. | kritik |
| 4 | `core/src/gorev/gorev.c:101`, `core/src/gorev/ariza.c:217` | **Bir sızıntı sensörünün kaybını başka sensör gizleyebiliyor.** Sayaçlar `inst` başına tutulurken tazelik tek `sizinti_rx_ns` üzerinden hesaplanıyor. Daha önce örnek alınan `inst=1` kesilse bile `inst=0` akışı ortak damgayı yeniler; kaybolan sensör nedeniyle `SIZINTI_BAYAT`, WARN ve dalış kısıtı oluşmaz. | Kullanılan sensör örneklerinin tazeliğini `inst` başına izleyin. Bir sensör akarken diğerinin bayatladığı vaka eklenmeli. | kritik |
| 5 | `core/src/gorev/gorev.c:180`, `core/src/gorev/ariza.c:283`, `core/src/gorev/istek.c:60` | **Tanımsız arıza düzeyi güvenli eylem uygulanmadan siliniyor.** Sağlıklı girdilerle, silahlı bir itkili kipte `fs_level=6` olduğunda değerlendirme bunu `NONE` ile değiştirir. `armed` korunur, `adim_daralt` yanlış kalabilir ve kontrol isteği açık çıkar. Çerçevenin `core_ok=0` bildirmesi isteği kapatmıyor. Tanımsız düzeyin güvenli yöne çözülmesi şartı sağlanmıyor. | Tanımsız düzeyi normal hesapla örtmeden önce güvenli daraltmaya dönüştürün; aynı çağrıda kontrol isteğini kapatın ve genişletmeyi engelleyin. Yalnız `ida_gor_tutarli()` dönüşünü değil, çağrı sonu davranışını sınayın. | kritik |
| 6 | `core/test/gorev/test_gorev_degismez.c:315`, `core/test/gorev/test_gorev_degismez.c:319`, `core/test/gorev/test_gorev_degismez.c:349` | **Rastgele test gerçek yineleme ve daraltma dağıtımını sınamıyor.** Düzenek kendi yineleme penceresiyle komutu atlıyor ve daraltmalı genişleticiyi gerçek dağıtıcıya vermiyor. D4 ise aynı iç işlevi yeniden çağırıp `ACCEPTED` bekliyor; A4-K48’in `DUPLICATE` davranışını ölçmüyor. Dolayısıyla bu testin geçmesi gerçek çağrı zincirindeki öncelik ve yineleme hatalarını dışlamaz. | Genişletmenin mümkün olduğu durumlarda gerçek dağıtıcıyı/port yolunu kullanın. Aynı kimliğin ikinci gelişinde `DUPLICATE`, değişmeyen durum ve günlük üretmeme birlikte doğrulanmalı. | önemli |

## Yeniden hesaplananlar

- Sızıntı karşı örneği: `N=2`, sayaç `0→1→2→0`; son karşılaştırma `0 ≥ 2` yanlış olduğundan tetik kaybolur.
- ARM karşı örneği: GCS ilk kayıp koşulunda `DEPTH_HOLD→WARN(1)`, `GUIDED→HOLD(2)`. Kod kip değişiminden sonra `1` değerini koruyarak ARM’ı kabul eder.
- Kip tablosu `8×10=80`, arıza tablosu `14×8=112` hücredir. Arıza senaryo döngüsü iki silahlandırma değeriyle 224 deneme kurar; ancak bazı denemelerde kip önceden düşürüldüğünden başlangıç sütunundaki hücre doğrudan uygulanmaz.
- Sızıntı sayacı 255’te doyar; izin verilen en büyük eşik 20 olduğundan sayaç taşması saptanmadı.
- Geçerli zamanların farkı `[-2⁶², 2⁶²]` içindedir ve `int64_t`’ye sığar. En büyük tanımlı süre `600000 ms = 600000000000 ns`; süre dönüşümünde taşma saptanmadı.
- Birim kuaterniyonda yalnız yön dönüşü için yön çıkarma formülü `atan2(sin ψ, cos ψ)` biçimine indirgenir; işaret ve katsayı hatası saptanmadı.

## İskelete ya da diğer bölümlere notlar

- `origin/main` üzerindeki W3 görev ve ortak kurallarına erişildi. W3-K1 gereğince süregelen acil durdurmanın araç ARM’ını engellememesi bulgu sayılmadı.
- `gecerlilik_al()` hâlen genel kestirim düzeyinden bileşen geçerliliği üreten geçici bağdır. Gerçek `ida_kes_gecerlilik()` bağlantısı yapılmadan W3-K5’in bütünleşik doğrulaması tamamlanmış sayılamaz.
- Arıza daraltması yalnız görev modülünde `NARROWED` üretirken gerçek dağıtıcı önce `IMU_REJECTED`, `IMU_LAG` ve ARM’ın ortak retlerini seçebiliyor. W3-K2’nin A4 §3.17 önceliğine nasıl katılacağı mimarca tekleştirilmeli.
- Mevcut `kapsayan()` çağrıları gerçek `&c->gor` üyesini kullanıyor; ayrı bir hizalama hatası saptanmadı. [N2176 C17 taslağına](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2176.pdf) erişilemedi; bu kaynak doğrulanmış sayılmadı.
- MSVC’nin `/std:c17` desteği [Microsoft belgesinden](https://learn.microsoft.com/en-us/cpp/build/reference/std-specify-language-standard-version?view=msvc-170) denetlendi. Bu kodun `/W4 /WX /analyze`, Windows, sanitizer ve kart sonuçları doğrulanmadı. Çalışma ağacında `tests/kabul/gorev/` bulunmadığından bağımsız görev kabul testleri incelenemedi.