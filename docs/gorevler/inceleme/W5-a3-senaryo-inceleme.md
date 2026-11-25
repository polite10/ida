# W5-a3-senaryo incelemesi (PR #57, uç `c389be3`)

Tarih: 2026-11-25. İnceleyen: `ida-inceleyici` (Opus; K21 tam inceleme). Yalnız okuyarak yapıldı; derleme ve test koşturulmadı. Aşağıdaki "PR #57 inceleme raporu" inceleyicinin metnidir (agent çıktısı; mimar kararı değildir).

## Mimar kararları (düzeltme turu)

Dilim emniyet sınıfındadır (K21; görev belgesindeki sınıf düzeltmesi). Bu turda düzeltilenler: 1, 2, 3, 4, 6 ve bulgu 10'daki eksik emniyet testleri ile sabit bekleme. Açık bulgulara yazılanlar (bu turda değil): 5, 7, 8, 9 ve 10'un kalanı.

- **W5-K55 — `emniyet_rev` süreç boyunca yaşar (bulgu 1).** Son revizyon ve doyma bayrağı süreç boyunca yaşayan durumda tutulur; eski dünyanın oturum sonu artışından sonraki değer yeni dünyaya geçer (A3-K117, A3-K67). Bunun için `sim/kopru` kapsamı açılır, ama yalnız şu kadar: köprüye başlangıç revizyonu ve doyma bayrağı verilebilir. Köprünün başka davranışı değişmez. `yuklendi` durumunda (köprü yokken) yayınlanan revizyon da bu değerdir.
- **W5-K56 — Çekilmiş emniyet isteği sonuçsuz kalmaz (bulgu 2).** `sifirla`/`yukle` ile aynı sınırda çekilen isteklerden, `komut_no`'su sıfırlamadan küçük olan kısıtlayanlar eski dünyada uygulanır; geri kalanların hepsi `canli_calistirma_yok` ile reddedilir. Her komut nihai sonucunu alır ve kayda geçer (A3-K57). Hazırlıkta biriken istekler için de aynısı.
- **W5-K57 — Canlı çalıştırmada senaryo ayrıştırılmaz (bulgu 3).** `senaryo/yukle` durum ve revizyon denetimini ayrıştırmadan önce yapar. `senaryo/dogrula` canlı çalıştırmada (`hazirlaniyor`, `kosuyor`, `duraklatildi`) 409 `gecersiz_durum` ile reddedilir. Bu, A3-K76'dan bilinçli sapmadır (sözleşmeye küçük sürüm notu yazılacak); gerekçe: tek G/Ç iş parçacığında uzun ayrıştırma, acil durdurma isteğinin okunmasını geciktirir. Ayrıştırmanın işçi iş parçacığına taşınması bu turda yapılmaz.
- **W5-K58 — Yeni dünya emniyet durumunu hemen yayınlar (bulgu 4).** Dünya `yuklendi`'ye döndüğünde senaryonun başlangıç değerleriyle bir `emniyet/girdiler` karesi yayınlanır. Geçiş penceresinde anlık görüntüdeki senaryo özeti boştur ve kuruluş bitene kadar `baslat` reddedilir.
- **W5-K59 — Sıfırlamada A1 oturumu doğru nedenle kapanır (bulgu 6).** `RUN_END` ve `planli_sifirlama`; `sim_kapanisi` yalnız süreç gerçekten kapanırken.

---

## PR #57 inceleme raporu (dal `is/w5-a3-senaryo`, uç `c389be3`)

**Hüküm:** Düzeltme turundan sonra birleştirilebilir. K21 sınıfında üç bulgu var (1, 2, 3); düzeltmeden sonra dar denetim gerekir.

Satır numaraları dal ucundaki çalışma ağacına göredir (`.claude/worktrees/w5-a3-senaryo`). Derleme ve test koşturmadım, yalnız okudum.

---

### Mimarın şüphesi: `emniyet_rev` yeniden kurulumda 1'e dönüyor mu?

**Şüphe doğru (doğrulandı).** Sayaç her dünyada yeniden başlıyor:

- Sayaç dünyanın köprüsünde (`kopru::Kopru`) tutuluyor. Başlangıç değeri `sim/kopru/include/ida/kopru/kopru.hpp:173`'te 1.
- Her dünya kendi köprüsünü kuruyor: `sim/sunucu/src/sunucu.cpp:4901` (`k_ = std::make_unique<kopru::Kopru>(ka)`).
- Süreç boyunca yaşayan `A3Kalici` (`sunucu.cpp:105-130`) yalnız `durum_rev`'i taşıyor (`:110`, `:2763`, `:4282`). `emniyet_rev` ve `emniyet_rev_doydu` için alan yok.
- Geçiş anında anlık görüntü sayacı 1 diye açıkça yazıyor: `sunucu.cpp:2617` (`p.emniyet_rev = 1;`, `temiz` dalı `:2589`).
- `sunucu.cpp:2915`'teki yorum bunu tasarım diye yazıyor ("yeni dünya revizyon 1'den başlar").
- Test ihlali sabitliyor: `tests/butunleme/test_a3_senaryo.cpp:717` `emniyet_rev == 1` bekliyor.
- Düzeltme köprüye dokunmayı gerektiriyor: `sim/kopru/src/kopru_sinir.cpp:691`'deki `emniyet_baslangic` yalnız revizyon 1 iken çalışıyor. Görev belgesi `sim/kopru`'yu kapsam dışı sayıyor, bu yüzden kapsamı mimar kararı açmalı.

Somut sonuçlar:

**(a) Çelişen sözleşme hükümleri.** A3-K117 ("süreç başında 1; çalıştırmalar ve oturumlar arasında sıfırlanmaz") ve A1 `safety_rev` tanımı (`protocol/ida.xml:481`, "süreç boyunca tekduze artar") çiğneniyor. Dışarıdan görünen her yerde sayaç her sıfırlamada geri gidiyor ve eski değerleri yeniden kullanıyor: `GET /emniyet`, `GET /durum`, `emniyet/girdiler`, A6 dosyaları, karta giden `IDA_SAFETY.safety_rev`. A3-K67'deki oturum sonu artışı eski köprüde yapılıyor, sonra köprüyle birlikte atılıyor.

**(b) Doyma (K21 sınıfı).** A3-K117'ye göre sayaç doyduktan sonra süreç yeniden başlayana kadar bütün gevşetmeler reddedilmeli. Bu kodda bir `sifirla` doymayı temizliyor ve gevşetmeler yeniden kabul ediliyor. Kısıtlamayı kaldıran istek yanlış kabul edilmiş oluyor. Uygulamada erişilmesi güç: 2³²−1 artış gerekiyor.

**(c) Eski çalıştırmadan kalan isteğin yeniden geçerli görünmesi.** Bugünkü kodda böyle bir yol bulmadım. Yalnız `run_id` bağı koruyor, ikinci savunma hattı kayboldu:
- Onay jetonu `run_id`'ye bağlı (`sim/a3/src/cekirdek.cpp:1936-1937`).
- Giriş denetimi `run_id`'yi karşılaştırıyor (`sim/sunucu/src/a3_baglanti.cpp`, `al`, "Emniyet ve yer istasyonu" dalı).
- Yeni dünya eski kuyruğu `a3_baslat_bekle`'de `canli_calistirma_yok` ile reddediyor.
- `run_id` saniye çözünürlüklü zaman + 24 bit rastgele (`a3_baglanti.cpp:713-727`).

Sayaç tekduze artsaydı, eski revizyona bağlı bir jeton ya da istek oturum sonu artışından sonra hiçbir zaman eşleşemezdi. Şimdi revizyon değerleri çalıştırmalar arasında tekrarlandığı için bu ayrım yok. Kart tarafında da yol görmedim: kart `ozel_emn_rev`'i oturum başına sıfırlıyor (`protocol/ida_link/src/ust_oturum.c:292`) ve oturum numarası `sess_sec(prev_sess)` ile değişiyor (çıkarım).

**Önerilen düzeltme:**
- Son revizyonu ve doyma bayrağını `A3Kalici`'de tutun. Eski dünyanın oturum sonu artışından sonraki değeri alın.
- Köprüye başlangıç revizyonu veren bir yol açın (mimar kararı: `sim/kopru`).
- `:2617`'deki sabit 1 yerine bu değeri yayınlayın.
- `:717` testini "yalnız artar, doyma korunur" olarak değiştirin.

---

### Bulgular

**1. kritik — K21 sınıfı — doğrulandı.** Yukarıdaki `emniyet_rev` sıfırlanması. Ayrıntı ve düzeltme önceki bölümde.

**2. önemli — K21 sınıfı — doğrulandı. Sıfırla/yükle ile aynı sınırda çekilen emniyet istekleri ne uygulanıyor ne reddediliyor.**
- Yer: `sim/sunucu/src/sunucu.cpp:3059-3064` (`a3_sinir_basi`) ve `:4096-4101` (`a3_kalanlari_reddet` erken dönüşü).
- Akış: `a3_sinir_basi` önce `a3_cek(0)` ile kısıtlayan yuvaları `a3_kisit_`'e, gevşetenleri `a3_genel_`'e alıyor. Sonra `a3_calistirma_isle` `sifirla`'yı kabul edip `a3_durdur_`'u kuruyor ve tur `a3_emniyet_ver`'e varmadan bitiyor. `a3_bitir` → `a3_kalanlari_reddet` `a3_yeniden_` dolu olduğu için hemen dönüyor. Bu iki vektör dünya nesnesiyle birlikte yok oluyor.
- `main`'de aynı vektörler `a3_bitir`'in başında koşulsuz reddediliyordu. Bu PR'da gerileme.
- Hata senaryosu: koşu sürerken bir istemci acil durdurmaya basıyor (`komut_no` n), başka biri `sifirla` gönderiyor (n+1). İkisi aynı sınırda çekiliyor. Acil durdurma eski dünyada uygulanmıyor ve `komut_sonucu` hiç gelmiyor. Çekirdekteki kayıt sonsuza dek `kuyrukta` kalıyor; A6'ya da yazılmıyor. A3-K57 (nihai sonuç zorunlu; `komut_no` sırası, A3-K95 emniyet sırası) çiğneniyor.
- Hazırlıkta sıfırlamada `a3_hazirlik_durdu` (`:4057`) içinde biriken gevşeten emniyet ve yer istasyonu istekleri de aynı biçimde kayboluyor.
- Fiziksel etki sınırlı: eski dünya `kapanis_daraltmasi` ile güvenli kapanıyor. Ama girdi sonuçsuz kalıyor.
- Düzeltme: `a3_yeniden_` doluyken de `a3_kisit_` ve `a3_genel_`'de çekilmiş olanları işleyin. `komut_no` sırası `sifirla`'dan küçük kısıtlayanlar eski dünyada uygulanmalı, kalanlar `canli_calistirma_yok` almalı. Alıcıda bekleyenler yeni dünyaya kalabilir. Aynı sınırda kısıtlayan + `sifirla` için test ekleyin; `a3_sinir_kancasi` ile enjekte edilebilir.

**3. önemli — K21 sınıfı (geç uygulama) — kod yolu doğrulandı, süre ölçülmedi. Ağır senaryo işi tek A3 G/Ç iş parçacığında koşuyor.**
- Yer: `sim/a3/src/cekirdek.cpp:1585` (`dogrula`), `:1650` (`hazirla`), `:1674` (`yukleme_bekle`).
- A3 sunucusunda tek G/Ç iş parçacığı var (`sim/a3/src/sunucu.cpp:283`). `POST /senaryo/dogrula` her durumda kabul ediliyor. `POST /senaryo/yukle` A5 çözümlemesini ve `plan_kur`'u giriş denetiminden **önce** yapıyor (`hazirla`, sonra `alici_gonder`). Gövde sınırı 4 MiB (`cekirdek.hpp:36`).
- Hata senaryosu: koşu sürerken bir panel 4 MiB'lik bir `icerik` ile doğrula ya da yükle gönderiyor. A5 çözümlemesi süresince başka hiçbir istemcinin isteği okunmuyor, acil durdurma da. Yükleme sonunda `gecersiz_durum` ile reddedilecek olsa bile ayrıştırma yapılıyor. Gecikmenin büyüklüğü ölçülmedi.
- Düzeltme: yüklemede durum ve revizyon denetimini ayrıştırmadan önce yapın. Ayrıştırmayı G/Ç dışı bir işçiye taşıyın, ya da canlı çalıştırmada bu uçları `gecersiz_durum` ile reddedin (mimar seçimi).

**4. önemli — doğrulandı (panel tarafı çıkarım). Sıfırlamadan sonra istemciler bayat ya da eksik emniyet görüntüsü alıyor; geçişte karışık görüntü var.**
- Yer: `sunucu.cpp:3671` ve `:3742-3765`. `emniyet/girdiler` yalnız `k_` varken yayınlanıyor. Yeni dünyanın `yuklendi` durumunda köprü yok (köprü `baslat`'tan sonra `:4901`'de kuruluyor).
- Çekirdek `yuklendi`'ye dönüşte son-değer önbelleğini boşaltıyor (`cekirdek.cpp:3110-3118`, `:3157`). Sonuçlar:
  - Yeni abone `emniyet/girdiler` alamıyor.
  - Zaten abone olan panel eski dünyanın son karesini (ör. acil durdurma basılı) yeni koşu başlayana kadar gösteriyor. `GET /emniyet` ise başlangıç değerini veriyor.
- Geçiş penceresinde (eski dünya kabul etti, yeni dünya henüz `a3_baslat`'ta değil) anlık görüntü karışık: yeni `durum_rev`, eski senaryonun özeti (`:2744`), `plan_`'dan eski senaryonun başlangıç girdileri, `emniyet_rev` 1.
- Bu pencerede `beklenen_durum_rev` = X ile gelen bir `baslat` alıcıda bekliyor. Yeni dünya onu `a3_baslat_bekle`'de kabul ediyor (`:2870-2880`). Eski özeti görmüş ikinci bir istemci, görmediği yeni senaryoyu başlatabiliyor.
- Canlı çalıştırma olmadığı için yanlış kabul edilen emniyet girdisi yok; bu yüzden K21 değil.
- Düzeltme:
  - Yeni dünya `yuklendi`'de anlık görüntüden bir `emniyet/girdiler` karesi yayınlasın.
  - Geçişte görüntüdeki özet null olsun.
  - Kuruluş bitene kadar giriş denetimi `baslat`'ı reddetsin.

**5. küçük — doğrulandı. Yükleme beklemesi G/Ç'yi 10 s'ye kadar tutuyor; zaman aşımı yanlış kodla dönüyor.**
- Yer: `cekirdek.cpp:1674-1690`.
- Zaman aşımında yanıt 503 `mesgul`. Sözleşmede `mesgul` "sunucu kapanıyor ya da kuyruk dolu" anlamında, yani istek uygulanmadı demek (A3 §3.9). Burada komut kuyrukta ve sonra uygulanabiliyor; yanıt yineleme kaydına da bu biçimde giriyor (A3-K53).
- Kabulden sonra bekleme süresince canlı çalıştırma yok (ancak `baslat` kabulüyle anlık görüntü yenilenmesi arasındaki kısa pencere hariç). Asıl etki, bütün istemciler için HTTP/WS'nin durması.
- Düzeltme: sonucu bilinmeyen durum için `mesgul` dışında bir yanıt ya da 202 + `komut_sonucu`. Mimar karar versin.

**6. küçük — doğrulandı. Sıfırlamada A1 oturumu yanlış nedenle kapanıyor.**
- Koşu ortasında `sifirla` olağan bitiş yolunu kullanıyor ve `IDA_SESSION_CLOSE(SHUTDOWN)` gönderiyor (`sunucu.cpp:5173`). A1-K128 sıfırlama ve yeni çalıştırma için `RUN_END` istiyor.
- `a3_bitir` her durumda `a3_kapaniyor_ = true` kuruyor (`:4141`). Bu yüzden `a1_oturum_degisti` olayı `neden: "sim_kapanisi"` taşıyor; A6'da `planli_sifirlama` (6) var.
- Bu olaylar `a3_sessiz_` korumasına tabi değil (`a3_olay`, `:2481`). `yuklendi` karesinden sonra panele gidiyorlar; panel süreç kapanıyor diye yorumlayabilir.
- Düzeltme: `a3_yeniden_` doluyken `kCloseRunEnd` ve `planli_sifirlama` kullanın.

**7. küçük — doğrulandı (A2 istemcisiyle sınanmadı). A2 (W5-K49).**
- Yeni senaryoda `gorunum = kapali` ya da tanım kurulamıyorsa `a2_baslat` erken dönüyor (`sunucu.cpp:2633-2644`). Süreçte yaşayan A2 sunucusu eski (durmuş) sahneyle dinlemeyi sürdürüyor ve kilidi tutuyor. Dünya özeti ise `gorunum_kapali` diyor.
- A2 çalıştırma nesli yüklemede değil `baslat`'ta (`calistirma_basladi`) artıyor (`sim/a2/src/yayinci.cpp:556`). W5-K49 "yüklendiğinde" diyor.
- Düzeltme: bu durumda kalıcı A2'ye "çalıştırma yok" bildirin. Nesil artışının anını mimar seçsin.

**8. küçük — doğrulandı. Geri kurma yolu.**
- A5'ten geçen ama bu süreçte yürütülemeyen senaryo (test: SIL süreçte `z1-hil.toml`) `hazirla`'da yakalanmıyor; dünya yıkılıyor ve eski senaryo geri kuruluyor (`sunucu.cpp:5354-5370`). Durum `bitti` → `yuklendi`'ye dönüyor, `run_id` null, `durum_rev` artıyor. A3-K77'nin "durum değişmez" amacına aykırı.
- Geri kurma da başarısızsa süreç `--a3-surekli` altında bile bitiyor. Bekleyen yükleme `mesgul`/"sunucu kapanıyor" alıyor ve gerçek neden istemciye ulaşmıyor.
- Düzeltme: kip ve karşı taraf uyumunu `hazirla`'da `Ayar`'a karşı denetleyin.

**9. küçük — çıkarım. Dosya yolu.** Ad kalıbı (A3-K78) yüzde kodlamasını, çift kodlamayı, ayırıcıları, sürücü harfini ve büyük harfi kesin kapatıyor. Kalan zayıflıklar:
- (a) Windows ayrılmış aygıt adları (`con.toml`, `nul.toml`, `com1.toml` …) kalıba uyuyor. Windows 10'da `con` konsola gidebilir ve okuma G/Ç iş parçacığını kilitleyebilir. Adları açıkça reddedin.
- (b) `symlink_status` denetimiyle `ifstream` açılışı arasında simgesel bağ değiştirme yarışı var (`sim/sunucu/src/senaryo_hizmeti.cpp:161-190`). Dizine yazma yetkisi gerektiriyor. Açtıktan sonra dosya kimliği karşılaştırılmalı.
- (c) Dizin içindeki sabit bağ dışarıyı okutabiliyor.
- (d) `listele`'deki `path().filename().string()` (`senaryo_hizmeti.cpp:124`) MSVC'de kod sayfasına çevrilemeyen bir dosya adında istisna fırlatabilir. Bu da A3 G/Ç iş parçacığını ve sunucuyu durdurur. `u8string()` kullanın ya da istisnayı yakalayın.
- Gövde sınırı uygulanıyor (4 MiB). Ancak `dosya_oku_` boyut denetiminden sonra sınırsız okuyor; dosya arada büyürse tamamı okunuyor.

**10. küçük — doğrulandı. Testler.**
- `test_a3_senaryo.cpp:717` ihlali sabitliyor (bulgu 1).
- Eksik testler:
  - Sıfırla/yükleden sonra eski `run_id`'li gevşetmenin ve eski onay jetonunun reddi (A3-K57, A3-K94).
  - `emniyet_rev`'in yalnız artması.
  - Aynı sınırda kısıtlayan + `sifirla` (bulgu 2).
  - Sıfırlamadan sonra WS abonesinin gördüğü emniyet karesi (bulgu 4).
- Belirlenim testi (`:637-639`) yarıda kesilen koşunun yalnız ilk %90'ını karşılaştırıyor. Uzun bir kısmi koşuda son %10 yüzlerce satır olabilir ve gerçek bir farkı gizleyebilir. "Son tamamlanmış satıra kadar" ya da "son k satır hariç" karşılaştırması daha sıkı olur. Ardından gelen tam koşu karşılaştırması zaten katı.
- Node testinde sabit bekleme var: `a3_senaryo_uctan_uca.mjs`, `setTimeout(c, 300)`. W5-K43'ün sözüne aykırı.
- W5-K48 yalnız süreç içi gösterilmiş. A3 kapalı yolda tek davranış farkı şu: `main.cpp` artık `s.senaryo_yolu` yazıyor. Bu alan `kos_ic` senaryo bloğundan önceki erken hatalarda (`--sil-hiz` denetimi, `sunucu.cpp:4760`) boş, o yüzden stderr satırı "senaryo , davranış özeti" oluyor. CSV ve A1 akışında fark görmedim.

---

### Bakıp temiz bulduğum başlıklar

- `durum_rev` dünyalar arasında yalnız artıyor (A3-K122).
- Kullanım sonrası serbest bırakma yok: A3 G/Ç iş parçacığı dünya nesnesine işaret tutmuyor. Kapanış sırası her yolda `kapaniyor` → `durdur`, yükleme bekleyişinde kilitlenme yok.
- `A2Baglanti::tanim_` iş parçacığı yarışı yok.
- Yineleme kayıtları sıfırlamada `son_run_id` değişimiyle siliniyor.
- Yarıda sıfırlanan koşunun kaydı `durduruldu` ile kapanıyor (A6-K45 bunu açıkça sayıyor); çalıştırma başına bir dosya korunuyor.
- Yeni dünya emniyet girdilerini senaryonun başlangıç değerinden alıyor (A3-K91; köprü tarafı).
- `sim.a3.calistirma_akisi`'nde taşınan denetim eşdeğer: `yeniden_kostur` aynı `uygulanan_komut` dalından komut numarası + 404 alıyor.
- POSIX korumaları kaynak (`#ifdef _WIN32`) ile CMake (`if(NOT WIN32)`) arasında tutarlı.
- `pgrep -f` yok; MSVC'de derlenmeyecek bir kalıp görmedim.
- Kalıp denetimi uç düzeyinde dosya sistemine gitmeden çalışıyor ve erişim denetiminden sonra uygulanıyor.

---

**Doğrulanan:** yukarıda "doğrulandı" yazan her madde, kod okunarak.

**Doğrulanmayan:**
- Bulgu 3 ve 5'teki gecikme büyüklüğü (ölçülmedi).
- Windows'ta aygıt adı ve `path::string()` davranışı.
- Panelin bayat kareyi nasıl gösterdiği.
- A2 neslinin bir A2 istemcisinde gerçekten arttığı.
- Testlerin hiçbirini koşturmadım; CI sonucuna bakmadım.
