# sol incelemesi: atis

## Hüküm

**Düzeltme gerekli: dört kritik bulgu; ilk ikisi K21 sınıfında.** İnceleme yalnız `6517c9c..81af064` farkını kapsar. Çağrı dizileri kaynak kod üzerinden izlenmiştir; test veya derleme çalıştırılmamış, dosya değiştirilmemiştir.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `core/src/atis/atis_emniyet.c:241–262` | **K21 — Bayatlık bir çağrı geç uygulanabiliyor.** Önce `now_w=2000 ms`, `t_rx=2000 ms`, `t_meas=100 ms` olan gevşek örnek kabul edilsin. `now_w=2015 ms` çağrısında daha büyük `seq`, `t_meas=101 ms`, fakat `t_rx=1700 ms` taşıyan örnek gelsin. A4-K33 alım zamanının monotonluğunu şart koşmadığından örnek kabul edilir. Çağrı başındaki denetim eski, taze izi görür; yeni izlerin yaşı **315 ms** olunca yalnız gevşetme maskelenir, zaten gevşek olan `ESTOP` kurulmaz. Gerçek çıktı `ESTOP=0`, kapanma yok; kapanma sonraki çağrıdadır. Silahlı düzenekte aynı çağrıya bekleyen OPEN’ın `APPLIED` sonucu ve uygun `PERMIT_OPEN` geri okuması eklenirse `FIRE` yolu da açıktır. | Kabul edilen örnek izleri güncellediğinde oluşan bayatlığı aynı çağrıda etkin acil durdurmaya, kapanma isteğine ve genişletici yasağına dönüştür. G-Ç29 ve W5-K4 eki bunu gerektirir. Geri giden `t_rx` dizisini bağımsız beklentiyle sına. | kritik |
| 2 | `core/src/atis/atis_emniyet.c:62–65`; `core/src/atis/atis.c:423–429` | **K21 — Süren tekleme nedeni resetle temizleniyor.** Kabul edilmiş `FC_FIRE` → OPEN `APPLIED` → `FIRE` → FIRE `APPLIED` ve `t_meas > t_apply`, `LOADED=1` → `FAULT_LATCHED`. Tekleme saptanırken `atis_izleme=false` yapılıyor; kalıcı neden kaydedilmiyor. Sonraki çağrıda kapanma `APPLIED`, izin geri okuması kapalı, yeni ölçümde yine `LOADED=1` ve yeni kimlikli `FC_RESET` gelsin: gerçek sonuç `ACCEPTED` ve `SAFE`. Beklenen, geri okuma çelişkisi sürdüğünden aynı çağrıda yeniden kilitlenmedir. | G-Ç53’ün ikinci çelişkisini de süren neden olarak izle; `FC_RESET` sonunda W5-K1/G-Ç20’ye göre değerlendir. `test_atis_ozellik.c:443` yalnız uygulamanın mevcut neden bayraklarını denetlediğinden bu eksikliği yakalayamaz. | kritik |
| 3 | `core/src/atis/atis_emniyet.c:153–155` | **Bütünlük denetiminden önce bozulmanın kanıtı siliniyor.** Kurulu, `SAFE` örnekte `reset_kabul` veya `bu_cagri_kilit` ham baytı `0x03` yapılsın; ardından olağan port çağrısı gelsin. Çerçevenin ön denetimi bozulmayı görüp `core_ok=0` verir; fakat bu iki alan FC’nin kendi denetiminden **önce** sıfırlanır. `butunluk_onar` artık tutarlı durum görür: FC `SAFE` kalır, bütünlük kilidi ve `SAFETY_CLOSE(ALL)` oluşmaz. İç bayrak bozulmasının `FAULT_LATCHED`’e götürülmesi sağlanmıyor. | Ham bütünlük denetimini çağrı bayraklarını sıfırlamadan yap; saptanan iç bozulmayı onarım ve kilide taşı. İki alanı da bozarak gerçek port üzerinden sına. Düzenek, çerçevenin ön bütünlük denetimini çalıştırmıyor. | kritik |
| 4 | `core/src/atis/atis.c:106–113`; `core/src/atis/atis_emniyet.c:350–358` | **Dış durum A4 §3.16’yı doğru yansıtmıyor.** OPEN izni henüz dolmamışken FIRE `APPLIED` gelsin, fakat uygulama sonrasına ait örnek henüz bulunmasın: `fire_bekliyor=false` olmasına rağmen çıktı `FIRE_PENDING` kalır. Ardından tüpün boşaldığını gösteren örnek gelince `ASSESS` üzerinden `ARMED` yazılır; oysa `izin_var=true` ve izin süresi hâlâ geçerli olabilir. A4’te `FIRE_PENDING` sonuç beklenmesini, `ARMED` açık izin bulunmamasını ifade eder. | Dış durumu bekleyen FIRE ve gerçekten açık sayılan izinlerle birlikte türet. `test_atis_durum.c:241` ve `:616` mevcut yanlış eşlemeyi beklediği için sözleşme ihlalini sabitliyor; beklentileri A4’e göre düzelt. | kritik |

## Yeniden hesaplananlar

- Bulgu 1’de bayatlık: `2015−1700=315 ms > 250 ms`; çağrı başında görülen eski iz ise yalnız `15 ms` yaşındadır.
- Varsayılan `hold_ns=200 ms`: yenileme aralığı kodda **50–100 ms**, sonuç bilinmezliği sınırı `20+100=120 ms` sonrasıdır. İzin bitişinde kullanılan `çıkış now_w + hold_ns` saat alanı doğrudur.
- Son kapatma kimlikleri: `0xFFFFFFFF−0xFFFFFFF0+1=16`. Kimlik ayırıcı son değerden sonra sarmaz.
- Şema bağımsız yeniden hesaplandı: **159 → 162** parametre; kanonik metin **2426 → 2477 bayt**; blok **1296 → 1320 bayt**; özet **`0x18BE798234EDD38D`**. `test_param_hiz.c` değişikliği bu hesapla uyumludur.
- Geçerli zamanlarda `2^62 + 450000000 < INT64_MAX`; izin bitişi toplamında taşma bulunmadı. C tamsayı kuralları [WG14 N1570 §6.2.5 ve §6.5](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) üzerinden denetlendi. N2176’ya erişilemedi; doğrulanmış sayılmadı.

## İskelete ya da diğer bölümlere notlar

Bulgu 4’ün kaynağı `03-atis-kontrol.md` §2.6.1/1’deki eşlemede de bulunuyor; A4 §3.16 önceliklidir. Bulgu 2 açısından `core/TASARIM.md` §10.5/7’nin teklemeyi yalnız olay sayan okuması, W5-K1’in süren neden şartını karşılamıyor.

Özellik testleri bütünüyle boş değildir; ancak (a) bayatlığı bağımsız hesaplamak yerine çıktıdaki `ESTOP` bitine, (c) uygulamanın neden bayraklarına, (e) ağırlıkla `ACCEPTED` sonucuna dayanıyor. Bunlar emniyet etkisinin bağımsız doğrulaması değildir. `test_emniyet.c` değişikliği ise yer tutucu beklentisini A4-K116’daki gerçek gevşetme davranışına uyarlıyor; bu değişiklikte gerileme örten bir durum bulunmadı.