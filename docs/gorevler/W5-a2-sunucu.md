# W5-a2-sunucu — sim tarafında A2 sunucusu (sim ↔ görselleştirici), dilim 1

Tarih: 2026-11-25. Yazan: `ida-uygulayici`. İnceleyen: sol (emniyet dışı dilim; K23: tek tam inceleme, tek düzeltme turu; sol sınırı dolu olduğu sürece K25 ile `ida-inceleyici`, iş "sol inceleme borcu" tablosuna yazılır). Dal: `is/w5-a2-sunucu` (taban `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `docs/gorevler/W4-ortak.md`, `docs/gorevler/W5-ortak.md`.

## Amaç

Görselleştirici (`viz/`) bugün yalnız kinematik sahte kaynakla (`viz/arac/sahte-a2`) çalışıyor; sim tarafında A2'den yalnız `sim/include/ida/a2_wire.h` var. Bu iş, sim tarafındaki gerçek A2 sunucusunu C++ kitaplığı olarak (`sim/a2/`, `ida_a2`) yazar: döngü arabiriminde TCP dinleyici, sahiplik kilidi, el sıkışma, durum akışı ve sahne olayları, nabız, kopma ve yeniden bağlanma, çalıştırma nesli. Veri kaynağı soyuttur: bu dilimde sunucu sahte bir kaynakla sınanır. `ida_sim`'e bağlanması (`--a2-port`, dünya döngüsünden yayın) **sonraki dilimdir** (W5-bagla-a2); `sim/sunucu` şu an W4-sunucu-kucuk'ta olduğu için bu dilim o dizine dokunmaz (W4-a3 → W4-bagla-a3 ayrımının aynısı).

Kamera hattı (kare isteği, kare halkası, paylaşımlı bellek) bu dilimde **yoktur**: sunucu her oturumda `kamera_sayisi = 0` bildirir.

Okunacaklar: `docs/sozlesmeler/A2-gorsellestirici.md` §1–§3.4, §4.1–§4.3, §4.7, §4.8'den yalnız A2-K77 ve K79, §4.9'dan A2-K80 ve K96, §4.12'den A2-K93, §5, §6, §7; `sim/include/ida/a2_wire.h` (bayt düzeni; belgeyle çelişemez); `docs/mimari/04-simulator-cekirdegi.md` §2.12 ve `07`'nin §3 (fikir kaynağı; bağlayıcı değil); `viz/BENIOKU.md`, `viz/a2/**` (gerçek istemci: kodek ve oturum) ve `viz/arac/sahte-a2/**` (davranış örneği: hangi ileti hangi sırayla; kopyalanmaz, sözleşmeden yazılır; sahte kaynak ile sözleşme çelişirse sözleşme geçerlidir ve çelişki raporlanır); `sim/a3/**` (iş parçacığı ve yayın arayüzü düzeni için örnek; değiştirilmez).

## Mimar kararları (bu iş; kayıtta `W5-ortak.md`'ye W5-K28…K30 olarak işlenir)

- **W5-K28 — Nabız el sıkışma sırasında da gider (A2-K15 ile A2-K62'nin bağdaştırılması; W3'ten kalan not).** A2-K62'nin 1 s'lik nabzı ve 3 s'lik ölü sayma ölçütü bağlantı kurulduğu andan başlar: görselleştirici `merhaba`dan, sim `hosgeldin`den itibaren nabız gönderir; `hazir` beklenen 120 s'lik pencerede (A2-K15) de 3 s ölçütü işler. İstemcinin var olan okuması budur (`viz/a2/src/oturum.cpp`); sunucu aynısını uygular. A2'nin küçük sürüm notu olarak yazılacak.
- **W5-K29 — Sim tarafı kendi kodlayıcısını yazar.** `ida_a2` ürün kitaplığı `viz/` altındaki hiçbir hedefe bağlanmaz (bağımlılık yönü: sim → viz olmaz); kodlama `a2_wire.h` yapılarından yazılır. Testler ise karşı uç olarak `viz/a2`'nin kodek ve oturumunu (gerçek istemci) kullanır: iki bağımsız gerçekleştirimin birbirini sınaması uygunluk kanıtıdır. Kodeğin tek yerde toplanması ayrı iştir.
- **W5-K30 — Bu dilimde üretilmeyen iletiler.** `kamera_istegi`, `kamera_iptal`, `halka_yenile` ve `govde_tablosu`/`atlama` (tekrar izleme; A6 işiyle gelir) üretilmez. Görselleştiriciden gelen `kare_hazir`, `kare_dustu`, `halka_eslendi` bu dilimde beklenmeyen iletidir: sözleşme bunlar için başka bir yanıt tanımlamıyorsa kısıtlayan okuma seçilir (protokol hatası, A2-K06) ve raporlanır.

## Kapsam

1. **Taşıma (A2-K01, K03–K08, K93):** yalnız `127.0.0.1`; port yapılandırmadan (geçerli aralık dışı değer kitaplıkta reddedilir; komut satırı seçeneği sonraki dilim); tek oturum, ikinciye `ret(mesgul)`; `TCP_NODELAY`; dinleyici açılamazsa sunucu "A2'siz" durumda kalır ve durumu sorgulanabilir kılar (A3'e bildirim sonraki dilim); başlık, tür, uzunluk, sıra ve metin denetimleri; duvar saati §2'deki kaynaktan.
2. **Sahiplik kilidi (A2-K77, K79):** dinleyici açılmadan önce kullanıcının geçici dizinindeki `ida/a2.kilit` bloklamadan alınır (macOS `flock`, Windows `LockFileEx`); alınamazsa sunucu A2'siz çalışır ve nedeni sorgulanabilir. Halka kayıt dosyası ve açılış kurtarması (A2-K75, K78) kamera dilimindedir; bu dilimde kayıt dosyasına dokunulmaz.
3. **El sıkışma (A2-K10–K17):** `merhaba` için 5 s; ana sürüm denetimi ve oturum alt sürümü; `hosgeldin` (yaşayan gövdeler, `kamera_sayisi = 0`, çalıştırma nesli, kademe) → `dalga_kumesi` → `ortam` → `malzeme_optik` → akış; `hazir`'da manifest özeti karşılaştırması ve `hazir.durum`; `ret(surum | manifest | zaman_asimi)`; 120 s penceresi; W5-K28.
4. **Durum akışı ve sahne olayları (A2-K20–K26, K29):** 60 Hz takvim adım numarasından belirlenimci hesaplanır (13/14 adım; kayma birikmez); süreklilik sınırında takvim dışı iki ileti (A2-K21); olay aynı zamanlı `durum`dan önce (A2-K22); gövde listesi (A2-K23); duraklatmada `duraklat` olayı, `durum` yok, nabız sürer (A2-K24; W4-duraklat `main`'de, bağlama sonraki dilim: kitaplık arayüzü bunu taşır); SIL'de duvar saatinin saniyesinde en çok 120 takvimli `durum` (A2-K25); gönderme kuyruğu 8 MiB, dolunca yalnız takvimli `durum` atılır ve sayılır, öteki iletilere yer kalmazsa `veda(tampon_tasti)` (A2-K26); `sifirlama` olayı ve nesil artışı (A2-K29).
5. **Nabız, kopma, yeniden bağlanma (A2-K62, K64'ün kamera dışı kısmı, K65, K67, K68):** 1 s `sim_nabzi`; 3 s sessizlikte `veda(nabiz_zaman_asimi)`; oturum bitince sunucu yeni bağlantıya hazır; her yeni oturumda yeni `oturum_kimligi` ve **tam durum** (o adım sınırındaki yaşayan gövdeler, güncel dalga kümesi, ortam, malzeme); olağan kapanışta `veda(sim_kapaniyor)`.
6. **Çalıştırma nesli (A2-K80, K96'nın kamera dışı kısmı):** süreç başında 1; yeni çalıştırmada artış, `veda(yeni_calistirma)`, oturumun kapanması, yeni oturumda yeni nesil; sıfırlamada artış oturumu kapatmaz. Nesli başka hiçbir olay değiştirmez.
7. **Veri kaynağı arayüzü:** dünya iş parçacığından çağrılan yayın işlevleri (adım sınırındaki gövde durumları, sahne olayı, dalga kümesi/ortam/malzeme değişimi, duraklat/sürdür, sıfırlama, yeni çalıştırma, kapanış) **engellemez ve bellek ayırmaz** (sabit boylu yapılar; taşma sayılır); sunucunun kendi G/Ç iş parçacığı vardır; görselleştiricinin yokluğu, yavaşlığı, donması ya da çökmesi dünya iş parçacığını hiçbir biçimde bekletmez (A2-K17, K47). Oturum koşunun ortasında açıldığında `hosgeldin`deki gövde tablosu ile ilk `durum` iletisi birbiriyle tutarlıdır (aynı adım sınırının durumu ya da sonrası; arada kaybolan doğum/ölüm olmaz). Arayüz başlıkları `sim/a2/include/ida/a2/` altında; `sim/kopru`, `sim/sunucu`, `sim/a3` ve `viz/`'e bağımlılık yok.
8. **Sayaçlar (§5 son paragrafın kamera dışı kısmı):** seyreltilen `durum`, protokol hatası, bağlantı sayısı ve son kopma nedeni, kuyruk taşması, yayın taşması; okunabilir bir yapı olarak (A3/A6 biçimi sonraki dilim).

## Kapsam dışı

Kamera isteği, kare halkası, paylaşımlı bellek, kayıt dosyası (A2 §3.5–§3.8, §4.5, §4.6, §4.8'in kalanı, §4.10); atlama ve gövde tablosu (§4.11); `ida_sim`'e bağlama ve komut satırı; A3'e bildirim; Godot projesi ve `viz/` altındaki kod (değiştirilmez; bir uyumsuzluk istemcideyse raporlanır); `sim/sunucu`, `sim/kopru`, `sim/a3`, `core/`, `protocol/`, `firmware/`, `web/`, `docs/sozlesmeler/`.

## Dosya sahipliği

Yazılır: `sim/a2/**`, `sim/CMakeLists.txt`'e tek satır. Soket katmanı gerekiyorsa `sim/a2` içinde yazılır (`sim/a3` ve `viz/a2/soket` ile ortaklaştırma ayrı iş). Yeni dış bağımlılık eklenmez.

## Kabul (testleri uygulayıcı yazar; CTest, var olan adlandırma düzeniyle)

1. **Tel (A2-U01):** sunucunun ürettiği her ileti türü için bayt düzeni `a2_wire.h` ile ve `viz/a2` kodeğinin çözdüğü değerlerle aynı; `yuk_bayt`, `sira` kuralları.
2. **Uçtan uca (gerçek istemciyle):** `viz/a2` oturumu sunucuya bağlanır: `merhaba` → `hosgeldin` → `dalga_kumesi` → `ortam` → `malzeme_optik` → `hazir` → `durum` akışı; istemci sahne durumu sahte kaynağın yayınladığıyla aynı değerleri verir; gövde doğumu, ölümü ve süreklilik değişimi (A2-U05'in kamera gerektirmeyen kısmı: `T − h` ve `T` iletileri, olay sırası).
3. **Protokol hataları ve el sıkışma (A2-U02, U03, U04):** bozuk sihirli sayı, bilinmeyen tür, yanlış uzunluk, sıfırdan farklı bayrak, azalan/yinelenen/boşluklu sıra → `veda(protokol_hatasi)`; ana sürüm farkı → `ret(surum)`; yanlış manifest özeti ve `hazir.durum ≠ 0` → `ret(manifest)`; `merhaba` ve `hazir` zaman aşımları (süreler testte enjekte edilen saatle; gerçek 120 s beklenmez); ikinci bağlantı → `ret(mesgul)`, ilk oturum etkilenmez; port doluyken sunucu A2'siz durumda, yayın işlevleri çalışmayı sürdürür.
4. **Takvim:** en az bir saatlik adım dizisinde takvimli iletiler arası 13 ya da 14 adım, ideal 60 Hz ızgarasından sapma ≤ 0,625 ms, kayma yok; SIL seyreltmesi (enjekte edilen duvar saatiyle saniyede ≤ 120; takvim dışı iletiler seyreltilmez).
5. **Yavaş, donuk ve ölü görselleştirici (A2-U17, A2-U10'un kitaplık düzeyi):** okumayan istemci, hiç bağlanmayan istemci ve ortada kapanan istemciyle yayın çağrısının süresi (en kötü ve p99; ölçüm, bu Mac; eşik ortak zamanlama yardımcısıyla, W4-K24/K29) bağlı ve sağlıklı istemcili koşudan ayırt edilemez; `durum` seyreltilir ve sayılır; olay iletileri kaybolmaz; kuyruk taşınca `veda(tampon_tasti)`; bellek üst sınırı.
6. **Nabız ve yeniden bağlanma (A2-U15, U14'ün kamera dışı kısmı):** 3 s sessizlikte kapanış; istemci kapatılıp yeniden bağlanınca yeni oturum kimliği ve tam durum; koşunun ortasında bağlanan istemcide `hosgeldin` gövde tablosu ile ilk `durum` tutarlı; W5-K28 (el sıkışma penceresinde nabız).
7. **Çalıştırma nesli (A2-U27'nin kamera dışı kısmı):** yeni çalıştırmada `veda(yeni_calistirma)`, yeni oturumda nesil +1; sıfırlamada nesil +1 ve oturum sürer; duraklatma nesli değiştirmez.
8. **Sahiplik kilidi (A2-U21'in halkasız kısmı):** aynı kilit dosyasına ikinci sunucu örneği (ayrı süreç) kilidi alamaz ve A2'siz çalışır; sahibi sonlanınca yenisi alır.
9. **Dayanıklılık:** sabit tohumlu rastgele ve bozuk girdi (en az 100 000 vaka): çökme, sınırsız bellek ya da kilitlenme yok.
10. `host-debug` + `host-release` tam tur (sonda bir kez); `host-asan` yalnız derleme (bu Mac'te ASan'lı ikili koşturulmaz); W3-K14 taraması; MSVC kalıpları (Winsock ve `LockFileEx` dalları ilk kez CI'da derlenecek).

## Rapor

PR numarası ve dal ucu; kabul maddelerinin her biri için sonuç ve kanıt; ölçülen yayın çağrısı süreleri (ölçüm, bu Mac); sözleşmede bulunan belirsizlikler ve seçilen makul okuma (K20); istemcide (`viz/a2`) görülen sözleşme uyumsuzlukları; sonraki dilimin (W5-bagla-a2) dünya döngüsünden hangi veriyi hangi noktada çekmesi gerektiğine dair kısa not.
