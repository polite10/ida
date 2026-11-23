# sim/kopru — A1 köprüsü (`ida_kopru`)

Sim sunucusunun kartla (ve kartsız koşucuyla) konuşan yarısı (görev: `docs/gorevler/W4-kopru.md`; sözleşme: `docs/sozlesmeler/A1-kart-baglantisi.md` 1.0). Köprü G/Ç ve saat bilmez: her işlev zamanı argüman olarak alır, gidecek baytları iç tampona yazar; zaman yöneticisi (`sim/sunucu`) taşımaya verir. Aynı girdi dizisi aynı bayt dizisini üretir.

| Dosya | İçerik |
|---|---|
| `include/ida/kopru/kopru.hpp` | `Kopru`: sim rolünde oturum (A1 §4.2), parti (§4.3, §4.4), yanıtların ve olayların doğrulanması (A1-K109, Tablo 4.6) ve uygulanması (A1-K72–K78, K81–K89, K106, K111, K118), fizik sınırındaki emniyet sırası (A1-K85), operatör komutları (§4.5) |
| `include/ida/kopru/tasima.hpp` | Taşıma: bellek çifti, TCP (SIL; yalnız 127.0.0.1, `TCP_NODELAY`), seri port (HIL; 3 000 000 baud 8N1, ham kip; macOS IOSSIOSPEED, Windows DCB), sözde uçbirim çifti (POSIX; kartsız HIL sınaması) |
| `include/ida/kopru/param_blogu.hpp` | A5'ten kurulan parametre bloğu (A4 §3.10) ve CRC'si (A1-K18, K19) |
| `include/ida/kopru/karsi.hpp` | Süreç içi karşı taraf: `ida_link` üst katmanı + gerçek çekirdek (SIL ve kart rolü); sınama ve `ida_sim --karsi ic|pty` içindir, ürün değildir |
| `test/` | CTest `sim.kopru.*` |

## Fizik sınırındaki çağrı sırası

Zaman yöneticisi her fizik sınırı `b` (t = b·h) için: `sinir_isle(b)` (A1-K85'in beş adımı: oturum sonu ve dolan izinler; kısıtlayan komutlar ve senaryo atamaları; kart olayları `SAFETY_CLOSE` → `SAFETY_OPEN` → `FIRE`; gevşetmeler; sürekli çıktılar) → dönen darbe genişlikleri b'de başlayan fizik adımına → sensör kümesi `dis_durum()` ile → yeni kayıtlar `olcum_ekle` → kontrol sınırında `parti_kapat(t_n)`.

## Bu dilimin sınırları

- Bariyer kipi (HIL 1x + adım bariyeri) yoktur; SIL kilitli adım ve HIL 1x vardır.
- Parti bütçeleri (A1-K34, K113): sensör üyeleri pencerede tutulur ve kapanışta gerçek kodlanmış boylarıyla yazılır; partide 1 IMU, emniyet dahil en çok 16 örnek; kritik toplam (TF, IMU, emniyet, daraltıcı komutlar, olay sonuçları, diğer sensörler) `B_krit` = 520 B'ı aşarsa diğer sensörler sıralarının sonundan sonraki partiye kayar (`kritik_erteleme`). Toplu komutların baytları pencere toplamına girer; `B_pencere` aşımı sayılır ve çalıştırmayı geçersiz kılar. İDA-0 sensör kümesinde en büyük kritik bayt 287 B'tır (SIL Z1, ölçüm).
- Daraltıcı komutlar tür başına duvar saatli kovayla gider (patlama 1, 5 ms'de 1; A1-K61, K119); pencere açılışı kovayı yenilemez.
- `batch_seq` 2³²'de sarar, 0 geçerlidir; TF, yanıt ve yuva geçmişleri gönderim sırasıyla tutulur.
- SIL'de (W4-K14) `KOSUYOR`'da `IDA_ACTUATOR`, `IDA_SESSION_CLOSE` ve `IDA_BOARD_HELLO` dışındaki kart çerçeveleri tamponlanır ve sonraki `IDA_ACTUATOR`'dan (ya da bu iki oturum çerçevesinden) hemen önce, oturum sonunda kapanışta işlenir; komut sonuçları ve telemetri bir periyot geç görünür. Tampon sınırı 128 çerçeve (bir çağrının `ACTUATOR` dışı çıktısı en çok `Q_ii` = 64 + `R_iii` = 500 B); aşımı protokol hatasıyla oturum sonudur, çerçeve atılmaz.
- Seq tanısı (`rx_sira_disi`, `rx_kayip`): son görülen en büyük başlık `seq`'inin gerisinde 32 numaralık pencere; atlanan numara sonradan gelirse sıra dışı, gelmeden pencereden çıkarsa kayıp. Yalnız tanıdır (A1-K08). Pencerede bekleyen son numaralar oturum sonunda sayılmaz.
- Sonar (Ping360 parçalama, A1-K49–K53), algılama (A1-K38, `NO_FRAME` A1-K58), bağlantı emülatörü ve hata enjeksiyonu yoktur.
- A1-K115 (seri sürücünün çıkış kuyruğu boşken yazma) uygulanmadı.
- Atış kontrol: kabul edilen `FIRE` yalnız tüpün "dolu" bitini siler; torpido fiziği yoktur.
