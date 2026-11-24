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

## Olay kümesi (W4-K15, EMNİYET)

Parti `n`'in kümesi: `IDA_ACTUATOR(n)` ve `batch_seq = n` taşıyan, `TICK_GRID` olmayan `n_events` farklı `event_id` (A1-K73). Sayıma giren olaylar (W4-K15 ek (6)): `ACTUATOR(n)`'den önce alınan her olay ve `ACTUATOR(n)`'den sonra alınan genişletici olaylar (sayılmış ama kaybolmuş olayın yinelemesi, H12). `ACTUATOR(n)`'den sonra alınan yeni kimlikli `SAFETY_CLOSE(n)` uygulanır ama sayıma girmez (sonraki hata dönüşünün olayı olabilir); sayılmış bir `SAFETY_CLOSE` kaybolup yinelemeyle gelirse küme tamamlanmaz, genişletici olaylar süreleri dolunca `REJ_EXPIRED` alır. Küme durumu partinin ilk alınan yanıtı ya da olayıyla açılır (`kopru_ic.hpp` `PartiKumesi`, `kopru_sinir.cpp` `kume_tamamla`).

| Durum | Geçiş | Koşul | Etki |
|---|---|---|---|
| BEKLİYOR | → DOĞRULANDI | Yanıt var ve küme tam (hata dönüşü `BAD_INPUT`/`NOT_INIT`: olay beklemeden; `OK`/`IMU_REJECTED`: alınan olay sayısı ≥ `n_events`) ve topluca tutarlı (A1-K118, Tablo 4.6b: genişletici olay varsa `core_rc = OK`, `SRC_TICK = 0`; `FIRE` varsa `ARMED = 1`, `IS_SAFE = 0`) | Yanıt kabul edilir: üretim sırası, `yanit(n)`, yanıt süresi, birleşim, yuva; canlılık (`T_bağ`) yenilenir. Uygulama anı geçmişse yuvaya ve yanıt geçmişine girmez, yalnız olayların dayanağı olur |
| BEKLİYOR | → REDDEDİLDİ | Küme tam, tutarsız | Yanıt hiçbir geçmişe girmez, canlılığı yenilemez; kümeyi tamamlayan son çerçeve (yanıt ya da olay) da canlılığı yenilemez (W4-K15 ek (7)); yuva kaçırılır; sayılan genişletici olaylar `REJ_SAFE` (`kume_reddedilen`, `alan_ret`) |
| BEKLİYOR | → REDDEDİLDİ + oturum sonu | Yanıt varken genişletici olay sayısı `n_events`'i aşıyor (yanıttan önce alınmış fazla olay) | Sayım ihlali: genişletici olaylar `REJ_SAFE`, `IDA_SESSION_CLOSE(PROTOCOL)`, oturum sonu iki tüpün iznini kapatır (`olay_sayim_ihlali`) |
| BEKLİYOR | (kalır) | Yanıt yok ya da küme eksik | Genişletici olay bekler, her sınırda yeniden denenir (geç olay, A1-K83); son geçerlilikte `REJ_EXPIRED`. Yanıt artık gelemiyorsa (yuvanın uygulama anı geldi) `REJ_SAFE` |
| DOĞRULANDI | → UYGULANDI | Yanıt `a_n`'de uygulandı | — |
| DOĞRULANDI / UYGULANDI / REDDEDİLDİ | → oturum sonu | Aynı `batch_seq`'li **yeni** `event_id`'li genişletici olay | Sayım ihlali (yukarıdaki gibi); "sonradan tutarsızlaşma" ve geri alma yolu yoktur |
| her durum | — | `SAFETY_CLOSE` (sayılan ya da sayıyı aşan; `ACTUATOR`'dan sonra gelen yeni kimlikli olan sayıma girmez) | Beklemeden uygulanır (A1-K87); ihlal değildir |
| her durum | — | Aynı `event_id`'li yineleme | A1-K81 (saklı sonuç); ihlal değildir |

Canlılık (`T_bağ`; W4-K15 ek (7)): çerçevenin katkısına tek yerde karar verilir (`kopru.cpp` `kart_cercevesi`, `kopru_sinir.cpp` `kume_canliligi`). `T_bağ` denetimi yalnız işlenmiş canlılığa bakar; bekleyen küme zaman aşımını ertelemez.

| Çerçeve | Katkı |
|---|---|
| Olağan yanıt (`OK`, `IMU_REJECTED`), genişletici olay | Kümeye bağlı: alım zamanı kümede bekler (en son alım); küme DOĞRULANDI olunca işlenir; küme reddedilir ya da tahliye edilirse atılır |
| Genişletici olayın kararı bekleyen yinelemesi | Kümesi bekliyorsa kümeye bağlı; doğrulandıysa anında; reddedildiyse yok |
| Saklı sonucu olan olayın yinelemesi (A1-K81) | Sonuç `APPLIED` ise anında; değilse yok |
| `SAFETY_CLOSE` (ve yinelemesi), hata dönüşlü yanıt (`BAD_INPUT`, `NOT_INIT`), `ASYNC` çıktı | Anında |
| Komut sonucu, `KEEPALIVE`, durum, seyrüsefer, günlük, sayaçlar, çağrı izi ve öteki geçerli çerçeveler | Anında |
| Alan, bağ, oturum ya da sıra denetiminde reddedilen çerçeve; sayım ihlali | Yok |

Sınır durumu (kısıtlayan yönde, bilerek): `ACTUATOR(n)`'den önce gelen bir `SAFETY_CLOSE(n)` sayıma girer; genişletici sayıyı o tamamlarsa sonradan gelen gerçek genişletici olay sayım ihlali sayılır ve oturum biter. A1-K90 sırasıyla `ACTUATOR(n)`'den önce gelen olaylar çağrı `n`'in kendi olaylarıdır; bu durum ancak sıra dışı teslimde (A1-K90 ihlali) doğar.

Tablo en çok 256 partiyi tutar (ekleme sırasıyla); tahliye edilen bekleyen kümenin genişletici olayları `REJ_SAFE` alır (`kume_bekleyen_tahliye`, `kume_en_cok`).

Emniyet örneği (W4-K12, W4-K15 (5)): revizyon köprünün bütün yetkili geçmişinden seçilir (tam eşleşen en yeni; yoksa en az o kadar kısıtlayıcı olunan en yeni). Hiçbiri yoksa (kaydın revizyonu tablodan atılmış olabilir) durum güncel yetkili durumla alan alan en kısıtlayıcı birleşime çevrilir ve güncel revizyonla gönderilir.

## Senaryo yürütücüsü için arayüz (W4-sim-senaryo)

- `emniyet_baslangic(...)`: senaryonun başlangıç emniyet durumu (A5-K86, K87); yalnız ilk fizik sınırından önce, revizyon 1 kalır ve yetkili geçmişin ilk kaydı bu durumla kurulur.
- `SinirSonucu::gevsetme_reddedildi`, `rev_adim_basi`, `rev_adim3_sonu`: o sınırın gevşeten atamaları reddedildi mi ve revizyonlar (A5 §3.15 `gevsetme_reddedildi`); sayaç `gevsetme_reddedildi`.
- `komut_gorev_ogesi(...)`: `IDA_CMD_MISSION_ITEM` (A1-K69; toplu sınıf, bayt boyu 59 ile sayılır).

## Bu dilimin sınırları

- Bariyer kipi (HIL 1x + adım bariyeri) yoktur; SIL kilitli adım ve HIL 1x vardır.
- Parti bütçeleri (A1-K34, K113): sensör üyeleri pencerede tutulur ve kapanışta gerçek kodlanmış boylarıyla yazılır; partide 1 IMU, emniyet dahil en çok 16 örnek; kritik toplam (TF, IMU, emniyet, daraltıcı komutlar, olay sonuçları, diğer sensörler) `B_krit` = 520 B'ı aşarsa diğer sensörler sıralarının sonundan sonraki partiye kayar (`kritik_erteleme`). Toplu komutların baytları pencere toplamına girer; `B_pencere` aşımı sayılır ve çalıştırmayı geçersiz kılar. İDA-0 sensör kümesinde en büyük kritik bayt 287 B'tır (SIL Z1, ölçüm).
- Daraltıcı komutlar tür başına duvar saatli kovayla gider (patlama 1, 5 ms'de 1; A1-K61, K119); pencere açılışı kovayı yenilemez.
- `batch_seq` 2³²'de sarar, 0 geçerlidir; TF, yanıt ve yuva geçmişleri gönderim sırasıyla tutulur.
- SIL'de (W4-K14) `KOSUYOR`'da `IDA_ACTUATOR`, `IDA_SESSION_CLOSE` ve `IDA_BOARD_HELLO` dışındaki kart çerçeveleri tamponlanır ve sonraki `IDA_ACTUATOR`'dan (ya da bu iki oturum çerçevesinden) hemen önce, oturum sonunda kapanışta işlenir; komut sonuçları ve telemetri bir periyot geç görünür. Tampon sınırı 128 çerçeve (bir çağrının `ACTUATOR` dışı çıktısı en çok `Q_ii` = 64 + `R_iii` = 500 B); aşımı protokol hatasıyla oturum sonudur, çerçeve atılmaz.
- `Kopru::al(p, n, duvar, dur)`: SIL kilitli adım için; her teslim edilen çerçeveden sonra `dur()` doğruysa o çerçevede durur ve tüketilen bayt sayısını döndürür (yarım çerçeve ayrıştırıcıda kalır, tüketilmiş sayılır). Sim sunucusu kalan baytları saklar ve yalnız bekleme noktalarında verir; böylece çerçevelerin işlenme anı geliş zamanına bağlı olmaz (A1-K102, K121).
- Seq tanısı (`rx_sira_disi`, `rx_kayip`): son görülen en büyük başlık `seq`'inin gerisinde 32 numaralık pencere; atlanan numara sonradan gelirse sıra dışı, gelmeden pencereden çıkarsa kayıp. Yalnız tanıdır (A1-K08). Pencerede bekleyen son numaralar oturum sonunda sayılmaz.
- Sonar (Ping360 parçalama, A1-K49–K53), algılama (A1-K38, `NO_FRAME` A1-K58), bağlantı emülatörü ve hata enjeksiyonu yoktur.
- A1-K115 (seri sürücünün çıkış kuyruğu boşken yazma) uygulanmadı.
- Atış kontrol: kabul edilen `FIRE` yalnız tüpün "dolu" bitini siler; torpido fiziği yoktur.
