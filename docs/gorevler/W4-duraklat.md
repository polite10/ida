# W4-duraklat — çalıştırmanın duraklatılması, sürdürülmesi ve adımlanması

Tarih: 2026-11-24. Yazan: `ida-uygulayici` (Opus; `sim/sunucu`'nun sahibi). İnceleyen: sol (EMNİYET sınıfı; ikinci tur açık). Dal: `is/w4-duraklat` (taban: `is/w4-bagla-a3`; o dal `main`'e girdiyse `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `docs/gorevler/W4-ortak.md` (özellikle W4-K3, K4, K5, K12, K15, K22, K25, K26, K27, K28).

**Bu iş, W4-bagla-a3 `main`'e girmeden ve emniyet kapsamı sol incelemesinde kapanmadan başlatılmaz.**

## Neden ayrı dilim

W4-bagla-a3'te duraklatma, olağan sınır döngüsünün yanında ikinci bir işlem yolu ("duraklatma yoklaması") olarak yazılmıştı. İki sol turunda çıkan emniyet bulgularının çoğu bu yapıdan geldi: olağan döngünün yaptığı her emniyet işinin o yolda ayrıca yinelenmesi gerekiyordu ve her turda biri eksik kaldı (`docs/gorevler/inceleme/W4-bagla-a3-sol.md` #1, #3, #4; `-sol-2.md` #1, #2, #3). W4-K28 ile duraklatma o dilimden çıkarıldı; `duraklat`, `surdur`, `adimla` bugün reddediliyor.

## Amaç

A3'ün çalıştırma denetimindeki `duraklat`, `surdur` ve `adimla` isteklerini (A3 §4.7–§4.8, Tablo 4.7a) SIL ve HIL'de, **tek kod yoluyla** uygulamak.

Okunacaklar: `docs/sozlesmeler/A3-web.md` §4.7–§4.8 (Tablo 4.7a, Tablo 4.8 bit 11), A3-K91, A3-K95 (duraklatılmışken gelen emniyet işlemleri beklemez); `docs/sozlesmeler/A1-kart-baglantisi.md` A1-K41 (HIL'de duraklatma: TF ve üye gönderimi durur, `IDA_KEEPALIVE` sürer, çalıştırma geçersiz), A1-K29 (sürdürme), A1-K78, A1-K79, A1-K85; iskelet §3.2 ve §4; W4-K27 ve W4-K28'in tamamı; iki sol incelemesi (yukarıda); `sim/sunucu/src/sunucu.cpp`'nin sınır döngüsü ve `sim/kopru/src/kopru_sinir.cpp` (`sinir_isle`, `emniyet_artimli`).

## Yapı kuralı (W4-K28 (2); bu işin özü)

Duraklatma ayrı bir döngü değildir. Olağan sınır işlemi iki kipte çalışır:

- **ilerleyen kip** (bugünkü davranış): A1-K85 adımlarının hepsi, senaryonun zaman çizelgesi, parti gönderimi, fizik adımı, sim zamanının ilerlemesi;
- **duran kip** (duraklatılmış): **aynı işlev, aynı sıra**, A1-K85 adımlarının hepsi (oturum sonu ve süresi dolan izinler; kısıtlayan komutlar ve kısıtlayan yuvaların boşaltılması; kart olayları; eşzamansız güvenli çıktı; gevşetenler revizyon kuralıyla). Atlanan yalnız üç şeydir: (1) senaryonun zaman çizelgesi (aynı sınırın olayları yeniden girmez); (2) fizik adımı ve sim zamanının ilerlemesi; (3) HIL'de parti ve üye gönderimi (A1-K41; `IDA_KEEPALIVE` sürer).

Kipler arasında kod kopyası olmaz: duran kip, ilerleyen kipin atladığı adımları bir bayrakla atlayan aynı işlevdir. Köprüdeki `emniyet_artimli` bu yapıya uyuyorsa kullanılır, uymuyorsa kaldırılır (iki ayrı karar yolu kalmaz).

Geçişler:

- **Duraklatmaya giriş:** `duraklat` isteği bir sınırın ilerleyen kipteki işlemi tamamlandıktan sonra, fizik adımından ÖNCE yürürlüğe girer.
- **Duraklatmadan çıkış (`surdur`, `adimla`):** çıkış isteğinin alındığı duran kip turu, emniyet kesimini (W4-K27 (2)) ve A1-K85 adımlarını TAMAMLADIKTAN sonra çıkar; çıkmadan önce sınırın itici girdisi güncel emniyet durumuna göre yeniden değerlendirilir (kısıtlayan bir etki uygulandıysa nötr). Aynı turda "acil bas" ile "sürdür" gelirse fizik eski girdiyle tek adım bile koşmaz.
- **Adımlama:** tam bir kontrol periyodu (ya da sözleşmedeki adım tanımı) ilerleyen kipte koşar, sonra duran kipe dönülür; yayın bir sınır geriden kalmaz.

## Kapsam

1. Yukarıdaki yapı; `duraklat`, `surdur`, `adimla` isteklerinin kabulü, durum geçişleri (`kosuyor` ↔ `duraklatildi`), `durum_rev`, sonuç ve olaylar.
2. **Emniyet (EMNİYET):** duraklatılmışken panel emniyet istekleri beklemeden işlenir (A3-K95): kısıtlayan koşulsuz ve revizyonu artırır; gevşeten yalnız `beklenen_rev` güncel revizyona eşitse. W4-K3, K5 geçerli. Kısıtlayan yuvalar duran kipte de her turda boşalır; art arda gelen kısıtlayan istekler birbirini ya da çalıştırma denetimini kilitlemez.
3. **Kart çıktıları duran kipte de tüketilir:** oturum kapanışı, `SAFETY_CLOSE`, eşzamansız güvenli çıktı; kapanışlar ve güvenli çıktı, gevşetmelerden ve fizikten önce uygulanır; bekleyen bellek sınırlıdır.
4. **HIL (A1-K41):** duraklatma yalnız `gecersizligi_onayla: true` ile; çalıştırma `hil_duraklatma` nedeniyle geçersiz olur; hiçbir parti üyesi (daraltıcı komut dahil) tele çıkmaz; komutlar sınırlı bir depoda birikir ve sürdürmedeki ilk sınırda işlenir (depo dolarsa kısıtlayan/daraltıcı düşmez; öteki istek `mesgul` alır); sürdürmede A1-K29.
5. **SIL:** duraklatmada sim zamanı durur (W4-K25 (1) ile aynı ilke); belirlenimcilik: aynı tohum ve aynı sınırlarda aynı istek dizisiyle iki koşu aynı çıktıyı verir.
6. Uçtan uca Node testi yeniden duraklat + adımla ile hızdan bağımsız kurulabilir.

## Kapsam dışı

Senaryo yükleme uçları, tekrar kipleri, kayıt; A3 kitaplığının erişim kuralları (W4-K23); `core/`, `protocol/`, `firmware/`.

## Kabul (testleri uygulayıcı yazar; emniyet testleri "önce nötr dışı komut yürürlükteydi" ön koşulunu doğrular)

1. **Tek yol kanıtı:** duran kip ile ilerleyen kip aynı işlevi çağırır (kodda ikinci bir emniyet/tüketim döngüsü yoktur); rapor bunu dosya:satır ile gösterir.
2. **İki sol incelemesinin karşı örnekleri (hepsi test):** senaryo bırakması + duraklatmada operatör basması (acil basılı kalır); aynı turda acil bas + sürdür/adımla (fizik eski girdiyle koşmaz; SIL ve HIL); duraklatmada `DISARM` → `DISARM` → acil bas (kilitlenme ve gecikme yok; gerçek kitaplık yoluyla); HIL duraklatmasında gelen `SESSION_CLOSE` (izinler ve revizyon hemen kapanır; eski revizyonlu gevşetme reddedilir); duraklatmada `SAFETY_CLOSE` ve eşzamansız güvenli çıktı; ilk TF'den önce uzun HIL duraklatması (bellek sabit); duraklatmada bas → bırak → sürdür (W4-K3: eski komut geri gelmez).
3. **HIL teli:** duraklatma boyunca telde yalnız `IDA_KEEPALIVE`; biriken komutlar sürdürmedeki ilk sınırda; depo dolu durumu.
4. **Belirlenimcilik (SIL):** duraklat/adımla/sürdür içeren iki koşu bayt bayt aynı; duraklatma hiç kullanılmayan koşu, bu iş öncesiyle bayt bayt aynı.
5. `kabul.kopru.*` değişmeden; A3 kapalı bayt eşitliği; dünya iş parçacığında ayırma 0 (W4-K27 (5)); `host-debug` + `host-release`; `host-asan` yalnız derleme; W3-K14; MSVC kalıpları; duvar saati eşikleri W4-K24 yardımcısıyla.
