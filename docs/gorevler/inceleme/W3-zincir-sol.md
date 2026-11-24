# sol incelemesi: gudum

## Hüküm

**Kabul edilmemeli: 3 kritik bulgu.** Eski hedefin DISARM sonrasında yeniden etkinleşebildiği bir yol, emniyet geçişinde korunan kontrol referansı ve fizik tarafındaki nötrlüğü doğrulamayan kabul denetimi bulundu.

İnceleme statiktir; test veya derleme çalıştırılmadı, dosya değiştirilmedi. Çalışma ağacı inceleme sırasında başka bir süreç tarafından güncellendi; bulgular son okunan içerikte yeniden denetlendi.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `core/src/gudum/gudum.c:142`; `core/src/cerceve/cerceve.c:527`, `:598`, `:472` | **Reddedilen çağrıdaki DISARM eski hedefi silmiyor.** Silahlı GUIDED ve saklı hedefle başlayın. Yinelenen zaman çerçevesi taşıyan çağrıdaki DISARM, (b) adımında uygulanır; çağrı `DUP_SEQ` ile döndüğünden `ida_gud_adim` çalışmaz. Sonraki geçerli çağrıda kabul edilen ARM, `armed` değerini yeniden 1 yapar. Güdüm yalnız son değeri karşılaştırdığı için `onceki_armed=1 → armed=1` görür; yeni GUIDED_SP gelmeden eski hedef uygulanır. Çağrıları aynı kontrol aralığına yerleştirerek IMU boşluğu oluşturmadan bu yol izlenebilir. | Hedef geçersizleştirmesini daraltmanın gerçekleştiği anda, reddedilen ve `out=NULL` çağrıları da kapsayacak biçimde yapın veya kalıcı bir daraltma kuşağıyla izleyin. Gerçek çekirdek üzerinden `GUIDED_SP → reddedilen DISARM → ARM` karşı örneğini sınayın. | kritik |
| 2 | `core/src/gudum/gudum.c:270`; `core/src/gorev/gorev.c:242`; `core/src/kontrol/kontrol.c:204`, `:393` | **Hedef silinse de kontrolün eski referansı yaşamaya devam ediyor.** HOLD’a girişte görev mevcut derinliği yakalar ve güdüm hedefi siler; fakat kontrol isteği yine `DALIS/DERINLIK` olduğundan `kanal_degisimi` eski `d_ref`, `psi_ref` ve integralleri korur. Böylece HOLD, önceki dalış referansını izleyebilir. Örneğin mevcut derinlik 1 m, eski `d_ref=2 m`, yeni tutma hedefi 1 m iken ilk adım hâlâ **+0,5 m/s aşağı hız** ister. `dalis_yasak` nedeniyle hedefin mevcut derinliğe kırpılması da aynı sorunu taşır. | Emniyet eylemine geçişi ve hedef iptalini kontrol belleğine aktarın. İlgili referansları geçerli mevcut durumdan yeniden başlatın; integralleri emniyet davranışına uygun yönetin. Yalnız istek alanlarını denetleyen test yerine kontrol çıkışına kadar ilerleyen geçiş testi ekleyin. | kritik |
| 3 | `tests/zincir/zincir.cpp:644`, `:662`, `:681`, `:811` | **Nötrlük denetimi uygulanan PWM’yi ölçmüyor.** Denetlenen ve CSV’ye yazılan `pwm`, henüz `t+10 ms` için kuyruğa konan komuttur; fiziğin yürürlükteki `d.pwm_us` değeri değildir. Önceki komutlar nötr dışındaysa DISARM çıktısı nötr görünürken fizik iki periyot daha eski komutlarla ilerleyebilir. Buna rağmen `disarm_sonrasi_sifir` ve emniyet değişmezleri geçer. Dört fizik alt adımındaki uygulama da denetlenmiyor; görev belgesinin kabul md.4’ü doğrulanmış olmuyor. | Üretilen, planlanan ve gerçekten uygulanan PWM’yi ayrı izleyin. Her fizik sınırında yürürlükteki PWM’yi, daraltmanın fizik tarafındaki etkin anıyla karşılaştırın. `L_act` ve DISARM nötrlüğünü, önceden kuyruğa alınmış nötr dışı komutlar bulunan bir vakayla birlikte sınayın. | kritik |

## Yeniden hesaplananlar

- **HOLD karşı örneği:** `dt=0,005 s`, referans sınırı `0,25 m/s` olduğundan adım `−0,00125 m`, yeni `d_ref≈1,99875 m` olur. Hız isteği `0,8×(1,99875−1)−0,25=0,549 m/s`; `LIM_W` bunu **+0,5 m/s** yapar. Ölçülen hız ve integral sıfırken kuvvet isteği `120×0,5+1,82+80×0,5=101,82 N`, sınırlandırılmış istek **80 N aşağıdır**.
- **Yaw sınırı:** `0x40490FDA=3,141592502593994 < π`; sonraki float `0x40490FDB=3,141592741012573 > π`. `PI_ALT` ile simetrik karşılaştırma, A4’ün float girdiler için istediği aralıkla eşdeğerdir. `OUT_OF_RANGE → BAD_MODE` sırası ve maskesiz alanların doğrulanmaması doğru uygulanmış.
- **Çerçeve dönüşümü:** 10 m kuzey hatası ve `GUD_POS_KP=0,3` ile NED hız isteği `(3,0)` olur. Yön `π/2` iken yön çerçevesinde `(ileri,yan)=(0,−3)` çıkar; işaret doğru.
- **Zamanlama:** `5 ms / 1,25 ms=4` fizik adımı; iki kontrol periyodu **10 ms=8 fizik adımıdır**. Kuyruğa verilen adım hesabı bu gecikmeyle uyumlu; üçüncü bulgu uygulamanın denetlenmemesidir.
- **IMU:** Durgun, düz araçta fizik çıktısından üretilen sapmasız özgül kuvvet `(0,0,−g)` olur; montaj kolu terimleri sıfırlanır. Üreticinin `2,8 mdps/√Hz` ve `70 µg/√Hz` değerleri, kodun kullandığı `yoğunluk/√0,005` kabulüyle yaklaşık `6,91×10⁻⁴ rad/s` ve `9,71×10⁻³ m/s²` verir. Bunlar donanım ölçümü değildir. [TDK ürün belgesi](https://www.invensense.tdk.com/en-us/products/6-axis/icm-42688-p)
- **PWM:** `1500+400u`, `u=−1,0,+1` için `1100,1500,1900 µs` verir. Belgelenen ±25 µs ölü bant dışında ilk tam sayı sapması 26 µs, dolayısıyla `26/400=0,065` doğrudur. [Blue Robotics teknik başvurusu](https://bluerobotics.com/learn/technical-reference/)
- **Kazançlar:** Dikey sönüm kirişi `(33×0,25+190×0,25²)/0,25=80,5 N·s/m`; 80 varsayılanı yaklaşık değerdir. `test_sol_bulgulari.c` içinde eski kazançların sabitlenmesi kabul edilebilir: kinematik karşı örnek korunuyor; ölü bant testinde başlangıç isteği `1,2+60×0,01=1,8 N`, itici başına `0,45 N` kalıyor.

## İskelete ya da diğer bölümlere notlar

- Görev belgeleri istendiği gibi `origin/main` üzerinden okundu; kullanıcının W3-K13 kararı esas alındı. Son kod, kabul edilen genişletici kip değişiminde arıza düzeyini komut işlenirken yeniden hesaplıyor.
- Birinci bulgunun çözümü, görev belgesinde değişiklik dışında bırakılan çağrı çerçevesine küçük bir bağlantı gerektirebilir; mimara kapsam gereksinimi olarak bildirilmelidir.
- Gerçek derinlik ve yön başarım ölçütleri fizik durumundan alınıyor. Son eklenen manevra ve arızasız karşılaştırmalar, üçüncü bulgudaki PWM gözlem boşluğunu kapatmıyor.
- MSVC `/W4 /WX`, firmware derlemeleri, kart ölçümleri ve bit düzeyinde tekrar sonucu bu incelemede **doğrulanmadı**. İstenen depo kaynaklarına erişildi; kestirim ve fizik uygulamaları yalnız bağlantıları anlamak için okundu.