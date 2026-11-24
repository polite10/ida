# W5-kestirim-yatay — çekirdekte seyrüsefer kestirimi (dilim 2: yatay hız ve konum)

Tarih: 2026-11-24. Yazan: `ida-kritik-uygulayici` (Fable). İnceleyen: sol (K21: bir tam inceleme + bir dar denetim; dar denetim yalnız kestirim düzeyi ve geçerlilik bayraklarının mantığına bakar, çünkü `est_level` silahlanmanın ön koşuludur). Dal: `is/w5-kestirim-yatay` (taban `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `docs/gorevler/W4-ortak.md`, `docs/gorevler/W5-ortak.md`. Önceki dilim: `docs/gorevler/W3-kestirim.md` (Ö1 yönelim, Ö2 derinlik; `main`'de).

## Neden şimdi

Kestirim bugün yalnız yönelim ve derinlik üretir; `core/src/kestirim/kestirim.c`'de yatay hız ve konum geçerliliği sabit `false`'tur, bu yüzden kestirim düzeyi hiçbir zaman `IDA_EST_OK` olmaz. İki sonucu var: (1) A4 §3.16/§3.17 gereği `FC_ARM` her zaman `NOT_ARMABLE` alır; emniyet zinciri (W5-atis-emniyet) port yolunda silahlı duruma hiç geçemez ve bağımsız kabul testlerinin silahlı yolu sınayanları koşamaz; (2) konum gerektiren kipler (GUIDED, STATION_KEEP) ve görev yürütücüsü gerçek konumla çalışamaz. A4'ün silahlanma koşulu GEVŞETİLMEZ (W5-K12); çözüm kestirimin tamamlanmasıdır.

## Amaç

W3-kestirim'in ertelenen iki kademesini yazmak:

- **Ö3 Gövde hızı ve ölü hesap konum:** DVL hızı (dip kilidi varken) ile yatay hız; hızın yönelimle dünya çerçevesine taşınıp tümlenmesiyle konum; DVL yokken/kilitsizken belirsizliğin büyümesi ve geçerliliğin düşmesi; DVL irtifası ve Ping1D menzilinden `nav.altitude` (bugün −1).
- **Ö4 GNSS düzeltmesi:** yalnız yüzeyde ve geçerli örnek varken konum (ve varsa hız) düzeltmesi; dalışta GNSS örneği kullanılmaz; yüzeye çıkışta konum sıçramasının sınırlı ve belgelenmiş ele alınışı.
- **Düzey ve geçerlilik:** `est_level` ve geçerlilik bayrakları A4'teki anlamıyla; `IDA_EST_OK`'a yükselme ve ondan düşme koşulları açık, sınanabilir ve histerezisli (tek örnekle yükselmez; kaynak kesilince belirlenmiş sürede düşer).

Okunacaklar: `docs/gorevler/W3-kestirim.md` ve sol incelemesi; `core/src/kestirim/**`, `core/test/kestirim/**`, `core/TASARIM.md`; A4 (`docs/sozlesmeler/A4-port.md`): sensör örnek tipleri (GNSS, DVL, Ping1D alanları ve geçerlilik bitleri), kabul kuralları (A4-K33 ve çevresi), `nav` çıktısı, `est_level` ve geçerlilik bitlerinin tanımı, zaman kuralları, `IDA_FCI_EST` bitinin koşulu (§3.16); `core/include/ida/port.h`; `docs/mimari/02-ortak-cekirdek.md` §2.3, §4.1; iskelet §3.1, §4; sensör modellerinin ürettiği değerler için `sim/sensor/**` başlıkları ve BENIOKU (yalnız okunur).

## Kurallar

- Yöntem 02 §2.3'teki tasarımdır; iki seçenek bırakılmışsa kartta daha ucuz olanı seç ve "Okumalar"a yaz. Var olan hata durumlu süzgecin (`kes_eskf.c`) yapısı korunur; durum vektörü genişletilecekse kartın çalışma süresi düşünülür (kart step en kötü ≈ 1,1 ms ölçüldü; bütçe 5 ms periyot; bu dilimin kart ölçümü ayrı iştir, burada rakam "tahmin" diye bile yazılmaz, yalnız `host-release`'te bağıl süre farkı ölçülür).
- Dinamik bellek yok; belirlenimcilik (A4-K24); `port.h` DEĞİŞMEZ. Çerçevenin kanca imzaları değişmez; değişmesi gerekiyorsa yazma, raporla.
- **Düzey mantığı güvenli yöne yaslanır:** kuşkuda düzey düşük kalır. `IDA_EST_OK` yalnız yönelim, derinlik, yatay hız ve konum geçerliliklerinin hepsi sağlanınca verilir (A4'ün tanımı farklıysa A4 geçerlidir; farkı raporla). Bayat, reddedilmiş ya da tutarsız (yenilik kapısından geçmeyen) ölçüm geçerliliği yükseltmez.
- Parametreler `kestirim_param.def`'e eklenir. Bilinen kırılganlık: parametre sayısı 162'yi aşınca `butunleme.sil_oturum_dusme_belirlenim` düşüyor (testin zamanlaması sabit; düzeltmesi W5-atis-emniyet düzeltme turunda). Bu test yalnız bu nedenle düşerse raporla ve devam et; testi değiştirme. `core/test/test_param_hiz.c`'deki altın şema özeti senin eklediğin parametrelerle güncellenir (W5-atis-emniyet de aynı satırı değiştiriyor; birleştirmede çakışma beklenir, raporuna yaz).
- Kapsam sınırı: bu iş seyrüsefer kestirimidir. Hedef algılama/takip, hedefleme, atış kontrol yazılmaz; sonar ve kamera algılama kayıtları kullanılmaz (Ping1D menzili yalnız irtifa için).

## Dosya sahipliği

Yazılır: `core/src/kestirim/**`, `core/test/kestirim/**`, `core/test/test_param_hiz.c` (yalnız altın özet), `core/TASARIM.md`'nin kestirim bölümü. `tests/kabul/**`, `core/src/atis/**`, `core/src/cerceve/**` değişmez.

## Kabul

1. **Yüzey, GNSS + DVL:** sentetik yörüngelerde (düz seyir, dönüş, hız basamağı; testin içinde üretilir, sabit tohum, sensör gürültüsü `sim/sensor` modellerinin varsayılan büyüklüklerinde) konum ve yatay hız hatası belirtilen sınırlar içinde; düzey belirlenmiş sürede `OK` olur.
2. **Dalış, yalnız DVL:** GNSS kesilince konum ölü hesapla sürer; hata büyümesi belgelenen sınırın altında; belirsizlik ölçüsü büyür; düzeyin `OK`'tan düşme koşulu (süre ya da belirsizlik eşiği) sınanır.
3. **DVL kilidi kaybı:** yatay hız geçerliliği belirlenmiş sürede düşer, düzey `OK` olmaz/olmaktan çıkar; kilit dönünce toparlar; çıktı her durumda sonlu.
4. **Yüzeye çıkış:** GNSS dönünce konum düzeltmesi sınırlı ve belirlenimci; sıçrama davranışı belgelenir.
5. **Aykırı ölçüm:** GNSS sıçraması ve DVL aykırı değeri yenilik kapısında reddedilir; kestirim belirtilen sınırdan fazla bozulmaz; arka arkaya ret geçerliliği düşürür.
6. **Düzey histerezisi:** tek geçerli örnek `OK` vermez; kaynakların sınırda gidip gelmesi düzeyi her çağrıda değiştirmez.
7. **Port yolu:** `ida_core_step` üzerinden, olağan sensör akışıyla (IMU, basınç, manyetometre, GNSS, DVL) düzey `OK` olur ve `fc.inhibit_mask`'te `IDA_FCI_EST` kalkar; kaynak kesilince yeniden kurulur (kara kutu birim testi).
8. **Belirlenimcilik** (A4-K24); var olan `core/test` ve `tests/kabul/cekirdek` testleri değişmeden geçer; Z1 çıktısındaki değişim raporlanır (Z1 konum kullanmıyorsa bayt eşitliği beklenir; değişiyorsa hangi alanların ve neden değiştiği yazılır, sessizce kabul edilmez).
9. **Kaynak:** `fw-release` kod boyu farkı, `ida_core_t` payı, en büyük yığın çerçevesi; `host-release`'te step süresinin bu iş öncesi/sonrası bağıl farkı (makine sakinken; `uptime` yük ortalamasıyla).
10. `host-debug` + `host-release` tam tur (sonda bir kez); `fw-release` derleme; `host-asan` yalnız derleme; W3-K12 statik analiz; W3-K14; MSVC kalıpları.
