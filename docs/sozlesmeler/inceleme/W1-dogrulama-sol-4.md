# sol incelemesi: sozlesmeler

## Hüküm

**Dondurulamaz.** Önceki eksiklerin çoğu giderilmiş; ancak port kaydının kodlanması, hatalı arka plan çağrılarının tekrarı ve A5 olaylarının kayıpsız taşınması bakımından kritik açıklar kalmış.

Öncelik sırası kullanıcı kararları > iskelet 0.10 > `docs/gorevler/W1-uyum.md` olarak uygulandı. İnceleme kayıt, tekrar ve panel kapsamındadır. Dosya değiştirilmedi, kurulum yapılmadı.

## Düzeltilmesi gerekenler

Aşağıdaki tablo, uyum turundan kalan veya turda oluşan çelişkileri gösterir. Önceki bulguların tek tek kapanışları sonraki bölümdedir.

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| Y1 | `docs/sozlesmeler/A6-kayit.md` K52 ↔ `core/include/ida/port.h`, `ida_param_save_t`; `docs/sozlesmeler/A4-port.md` §3.8/§3.14 | **`PARAM_SAVE` kodlaması hâlâ iki okumaya açık.** Başlıktaki sıra `gen, blob, len`; K52 ise blobu `len + baytlar` olarak kodlatıyor. Sondaki yapı alanı `len` ayrıca yazılıyor mu? Aşağıdaki örnek bir okumada 40, bildirim sırasını harfiyen uygulayan okumada 44 bayt oluyor. | A6-K52, bütün `ida_param_save_t` için kesin tel sırasını yazmalı; uzunluğun bir kez mi yazıldığını ve işaretçinin yerini hangi alanların aldığını açıkça belirtmeli. | kritik |
| Y2 | `docs/sozlesmeler/A6-kayit.md` K52/K54/K55 ↔ `docs/sozlesmeler/A4-port.md` §3.14, K30/K121 | **Reddedilen girdilerin tamamı tekrar edilemiyor.** A4, bilinmeyen `hdr.type` değerli örneği reddeder; çağrının kendisi ve tanılama sonucu vardır. A6-K54 bütün girdiyi kaydetmeyi isterken K52, tanımsız seçicili kaydı `bozuk` sayıyor. Örneğin `hdr.type = 99` içeren çağrı, kaydedilen ret davranışını yeniden üretmeden durur. | A6, A4’ün reddettiği seçicileri nasıl kodlayıp yeniden vereceğini tanımlamalı. Geçersiz girdinin kaydı ile bozulmuş dosya ayrılmalı; yeni opak yük açılmamalı. | kritik |
| Y3 | `docs/sozlesmeler/A6-kayit.md` K55 ↔ `docs/sozlesmeler/A4-port.md` K109/K117; `core/include/ida/port.h`, `ida_bg_rc_t` | **Arka plan sayımı farklı.** A4 yalnız `IDLE`/`MORE` dönen çağrıları `bg_slices`’a sayar. A6-K55 kayıtlı bütün background çağrılarını saydırıyor. `BAD_INPUT, MORE, step(bg_slices=1)` geçerli bir izken A6 iki çağrı görüp tekrarı reddeder. | A6-K55 tutarlılık sayımında `BAD_INPUT` dönüşlerini dışlamalı; çağrıların kendisini kayıttan yürütmeye devam etmeli. | kritik |
| Y4 | `scenarios/sema/a5.schema.json`, `$defs/olay*/properties/kimlik`; `docs/sozlesmeler/A5-senaryo.md` §3.10 ↔ `docs/sozlesmeler/sema/a3-kanallar.json`, `$defs/olay_senaryo_ogesi_uygulandi`, `olay_gevsetme_reddedildi`, `olay_donma_onceki_cikti_yok`, `olay_atama_celiskisi` | **Geçerli A5 kimlikleri yeni olay şemalarına sığmıyor.** A5 `^[A-Za-z][A-Za-z0-9_]{0,31}$` kabul ediyor; A3 yalnız küçük harf kabul ediyor. `Freeze_1` geçerli A5 kimliğidir fakat ilgili olayların kimlik alanlarından geçmez. | A3 olay şemaları A5’in kimlik kümesini değiştirmeden taşımalı; kimlik küçük harfe çevrilmemeli. | kritik |
| Y5 | `docs/sozlesmeler/A5-senaryo.md` §3.10, §3.15, K63 ↔ `docs/sozlesmeler/sema/a3-kanallar.json`, `$defs/olay_atama_celiskisi` | **Çelişen atamaların kaydı sekiz öğeyle sınırlanmış.** A5 en çok 4096 olay kabul ediyor; aynı sınırdaki koşullu atamaları sekizle sınırlamıyor. Aynı koşulla başlayan dokuz atamanın `degerler[]` ve `olay_kimlikleri[]` listeleri A3’ün `maxItems=8` sınırını aşar. Bölme veya devam kaydı kuralı yok. | A3/A6, A5’in izin verdiği bütün çelişkiyi sırası korunarak kayıpsız taşımalı; taşıyıcı sınırı ve gerekiyorsa parçalama bağı açıkça tanımlanmalı. | kritik |
| Y6 | `docs/sozlesmeler/A3-web.md` K71, Tablo 4.7b/4.8 ↔ `docs/sozlesmeler/sema/a3-kanallar.json`, `$defs/olay_gercek_zaman_kacirildi`, geçerlilik enumları | **Zamanlama tablosu düzelmiş, olay şeması geride kalmış.** K71 artık çağrı çakışması, eşzamansız güvenli çıktı ve diğer A1 nedenlerinde de olay ister; olayın zorunlu `neden` alanı yalnız `yanit_gec`/`sim_adimi_gec` kabul ediyor. Ayrıca Tablo 4.7b `duraklatma_hil`, kapalı katalog `hil_duraklatma` diyor. | A3 olay nedeni eşlemesini kapalı ve eksiksiz tanımlamalı; duraklatma adı bütün yerlerde aynı olmalı. | önemli |
| Y7 | `docs/sozlesmeler/A6-kayit.md`, `yuk_kamera_kare`; `docs/sozlesmeler/sema/a6-kanallar.json` aynı yük ↔ `docs/sozlesmeler/A2-gorsellestirici.md` §3.8 | **Kare sürümleri eksik ve bir alanın dönüşümü belirsiz.** A2 yuva başlığındaki `kalibrasyon_surumu`, `dalga_kumesi_surumu`, `ortam_surumu` kayıtta yok. A2 sürümü `duzen_ana/duzen_alt` ikilisiyken A6 tek `u16 a2_surumu` tanımlıyor; eşleme yok. | A6, U18’in istediği kare sürüm alanlarını kaynaklarına bağlamalı; iki parçalı A2 sürümünün gösterimini açıkça tanımlamalı. | önemli |
| Y8 | `docs/sozlesmeler/A1-kart-baglantisi.md` K43; `docs/sozlesmeler/A6-kayit.md` K28 ↔ A6 Tablo 3.3 ve `a6-kanallar.json`, `hil/tf_yazim.kademe` | **“Her TF kaydedilir” kuralının kademe istisnası tanımsız.** K43/K28 her zaman çerçevesinin gerçek başlangıç ve bitişini ister; yeni kanal yalnız `telemetri` ve `tam` kademelerinde yazılıyor. `olaylar` kademesinde bu kayıt hiç yok. | A6 kanal kademesi koşulsuz kayıt yükümlülüğüne eşitlenmeli; istisna amaçlanıyorsa karşı sözleşmeyle birlikte açıkça karara bağlanmalı. | önemli |
| Y9 | `docs/sozlesmeler/A1-kart-baglantisi.md` K125 ↔ `protocol/komutlar.json`; `docs/sozlesmeler/A4-port.md` K50 | **Yeni komut kataloğunda iki yerel uyuşmazlık var.** K125 “orta öğe yinelenir” diyor; `param_save`’da yinelenecek `PARAM_SET` ilk öğedir, iki öğeli `param_apply`’ın orta öğesi yoktur. Ayrıca `guided_sp.depth/altitude` şemaları negatif değer kabul ederken A4 bunları ≥0 ister. | A1-K125 tekrar edilen öğeyi komut başına açıkça göstermeli; katalogdaki ilgili sayısal alt sınırlar A4’e eşitlenmeli. | önemli |
| Y10 | `docs/sozlesmeler/A3-web.md` K97, `yuk_emniyet_girdiler.kart_*` ↔ `docs/sozlesmeler/A1-kart-baglantisi.md` Tablo 3.3, `IDA_SAFETY` | **Kart geri okumasına gösterilen kaynak ters yönde.** A3, kartın bildirdiği okumayı `IDA_SAFETY` üzerinden alacağını söylüyor; A1’de bu mesaj yalnız sim → kart yönünde. Gönderilen değer kartın kabul ettiği etkin değer değildir. | A3 mevcut A1’in sağlamadığı okumayı bilinmiyor göstermeli ve bunu açıkça yazmalı; gönderilmiş sensör değeri kart geri okuması olarak sunulmamalı. | önemli |
| Y11 | `docs/sozlesmeler/A6-kayit.md` §7, “K27, K57 (çok oturum)” sınaması ↔ `docs/sozlesmeler/A3-web.md` K67/K108 | **Çok oturum kabul sınaması kendi önkoşuluyla çelişiyor.** Sınama ortasında `sifirla` olan tek HIL kaydında iki oturum bekliyor. A3 ise `sifirla` uygulandığında mevcut dosyayı kapatıyor; sonraki çalıştırma yeni dosyadır. | Tek dosyada çok oturum sınaması, çalıştırmayı bitirmeyen oturum kopması/yeniden bağlanmasıyla kurulmalı; `sifirla` için iki dosyalı yaşam döngüsü ayrıca sınanmalı. | kritik |
| Y12 | `docs/sozlesmeler/A6-kayit.md` §3.5/K54 ↔ `docs/sozlesmeler/sema/a6-kanallar.json`, `$defs/ust_veri_ida_kayit` | **`sil_n_bg` metinde zorunlu, şemada isteğe bağlı ve sınırsız metin.** SIL koşulunda zorunluluk kuran `if/allOf` yok; alanın onluk u32 olduğu da denetlenmiyor. | A6 açılış şeması SIL koşulunu ve onluk u32 aralığını doğrulamalı. | önemli |
| Y13 | `docs/sozlesmeler/A3-web.md`, `yuk_algilama_kayit.batch_seq`, `yuk_algilama_parti.batch_seq`; iki kanal JSON’u ↔ `docs/sozlesmeler/A8-kamera-algilama.md` K16/K62 | **Atıfta kullanılan `kare_seq` adı karşı belgede yok.** A8’in adı `kare_sirasi`; `batch_seq` buna eşittir. | A3/A6 açıklamalarındaki ad `kare_sirasi` yapılmalı. | küçük |

## Yeniden hesaplananlar

**Önceki çapraz incelemenin 20 bulgusu**

`P4-n`, `docs/sozlesmeler/inceleme/W1-capraz-sol-4.md` içindeki n numaralı bulgudur. Aşağıdaki A1–A8 belge adları `docs/sozlesmeler/` altındaki sözleşmelerdir.

| Bulgu | Hüküm | Baktığım yer ve kalan eksik |
|---|---|---|
| P4-1 — alan alan kodlama | KISMEN | A6-K52; A3 §3.1; A4 §3.14; `port.h`: `i32` ve etkin birlikler eklenmiş. Blob sırası ve reddedilen seçiciler Y1/Y2’de açık. |
| P4-2 — background takvimi | KISMEN | A1-K121, A4-K95/K109, A6-K54/K55: çift çağrı üretimi kaldırılmış, `sil_n_bg` eklenmiş. `BAD_INPUT` sayımı ve şema zorunluluğu Y3/Y12. |
| P4-3 — güvence sınırları | KAPANDI | A1-K102/K121 kamera kapalı koşulunu taşıyor; A4 §7 ilgili uygunluk satırı yalnız SIL port kaydına bağlı; A6-K34/K55/K56 HIL’e bit eşitliği vermiyor. |
| P4-4 — emniyet revizyonu | KAPANDI | A3-K117/K118; ortak `yuk_emniyet_girdiler.emniyet_rev`; HTTP alanları: u32 sınırı, doyma ve altı artış işlemi iskelet §3.2 ile aynı. |
| P4-5 — fizik sınırı sırası | KAPANDI | A1-K85, A3-K95 aynı beş adımı yazıyor; A6-K35 bunlara bağlı. |
| P4-6 — iki tüpün temsili | KAPANDI | A3-K89/K93/K94; ortak yükte iki elemanlı fiş/pim dizileri; HTTP `$defs/girdi_secimi`: `f1/f2` ayrımı mevcut. Geri okuma kaynağı ayrı Y10. |
| P4-7 — zamanlama geçerliliği | KISMEN | A3 Tablo 4.8 ve katalog bit 0–11 A1-K79’u kapsıyor; 99,8 ms örneği düzelmiş. Olay nedeni ve duraklatma adı Y6. |
| P4-8 — sayaç ve karar taşıyıcıları | KAPANDI | A3 §3.3/§3.4; `sayac/gercek_zaman`, `kart/baglanti_sayaclari`, `kart/cekirdek_sayaclari`; `$defs/olay_yapilandirma_reddedildi`. Üç ek sayaç ve E1–E19 ayrıntısı mevcut. |
| P4-9 — kamera/algılama tanılaması | KAPANDI | Ortak `kamera/halka_sayaclari`, `algilama/parti`, `algilama/sayaclar`; A6 `kamera/kare_dusme`; A8-K30/K63/K65/K76/K79 karşılaştırıldı. Boş partiler ve geç yanıt işareti taşınabiliyor. Ad kusuru Y13. |
| P4-10 — A5 olayları ve raporu | KISMEN | A3 olayları 21/25/26/27; A6 `senaryo/oge_uygulama.sonuc=uygulanmadi`; `dogrulama_raporu.json` eki mevcut. Geçerli kimlik ve olay sayıları Y4/Y5’te taşınamıyor. |
| P4-11 — çözümlenmiş dosya/özet | KAPANDI | A3-K80; A6 §3.5/§3.6; A5-K17: kanonik JSON baytları, `sha256:` gösterimi ve ayrı `senaryo_kaynak_ozetleri` tanımlı. |
| P4-12 — kayıt ayarları | KAPANDI | A5 §3.12/K78, A3-K109, A6 §3.5: `olaylar/telemetri/tam` ve `kapali/pencereli/surekli` aynı. |
| P4-13 — komut kataloğunun yokluğu | KAPANDI | A1-K125, `protocol/komutlar.json`, A3-K100/K103 ve `$defs/a1_komut_listesi`: katalog mevcut, bileşik diziler kabul ediliyor. Yeni yerel kusurlar Y9. |
| P4-14 — sonuç verisinin kaybı | KAPANDI | A3-K102/K105; `$defs/olay_yer_istasyonu_yaniti.veri`; katalog `sonuc` şemaları: parametre kimliği/türü/değeri ve ret nedeni taşınabiliyor. |
| P4-15 — kamera kimliği/zamanı | KISMEN | A6 `yuk_kamera_kare`: istek, kare sırası, kamera, çalışma/halka nesli ve pozlama süresi eklenmiş; `log_time` pozlama sonu. Sürüm kapsamı Y7. |
| P4-16 — Ping360 dönüşümü | KAPANDI | Ortak `yuk_ping360` özgün cihaz alanlarını koruyor; A3 §3.4 formülleri A1-K52 ile aynı. |
| P4-17 — TF yazım zamanları | KISMEN | A6 `hil/tf_yazim` başlangıç/bitişi taşıyor; `hil/bariyer.varis_duvar_ns` son tamamlayıcı çerçeve olarak açıklanmış. Kademe çelişkisi Y8. |
| P4-18 — yeniden kurma girişi | KAPANDI | A1-K127 ve A6-K56/K57 aynı iki yönlü çerçeve girdisini, el sıkışmadan oturum alanlarını alma ve oturum başına init işlemini tarif ediyor. Uygunluk sınaması Y11. |
| P4-19 — kapsam dışı port alanları | KAPANDI | `port.h`, A4 §3.14–§3.16/K119/K120, A6-K52: kaldırılan tür/alanlar yapıda yok; FC argümanları ayrılmış ve sıfır zorunlu; günlük/engel/neden tabloları kapalı. |
| P4-20 — çalıştırma kimliği | KAPANDI | A1-K126, A3-K63, A6 §3.5; A5-K118/Ek D.7: metin kimliği ile `a1_run_id` ayrılmış, türetim ve çakışmanın anlamı yazılmış. |

**Uyum görevindeki maddeler**

| Madde | Hüküm | Baktığım yer ve kalan eksik |
|---|---|---|
| U2 — A3/A6 tarafı | KAPANDI | A3-K117; iki katalogdaki `yuk_emniyet_girdiler`; HTTP revizyon alanları. Başlangıç 1, u32, doyma, sıfırlanmama ve altı işlem aynı. |
| U3 — A3/A6 tarafı | KAPANDI | A3-K95, A6-K35; A1-K85 ve iskelet §3.2 ile aynı sıra. |
| U4a | KAPANDI | A3-K101/K102; `bayat_yetki` olay/sayaç; A6 `yer_istasyonu/emulator.karar=5`. |
| U6 — A3/A6 tarafı | KAPANDI | Tüp başına durum/istek/jeton/olay alanları ve ortak yükler mevcut; A3-K89–K97. |
| U10 | KISMEN | A1-K121, A4-K95/K109, A6-K54/K55, `sil_n_bg`: olağan takvim aynı; Y3/Y12 kalıyor. |
| U16 | KISMEN | 19 komut, sınıflar, onaylar, bileşik komut ve sonuç taşıyıcısı mevcut; Y9’daki katalog uyuşmazlıkları kalıyor. |
| U17 | KAPANDI | A3-K63; A6 `run_id/a1_run_id`; A1-K126; A5-K118/Ek D.7. |
| U18 | KISMEN | Sayaç taşıyıcıları tamamlanmış; olay kimliği/kapasitesi, zamanlama olayları, kare sürümleri ve TF kademesi Y4–Y8’de açık. |
| U19 | KISMEN | A1-K128, A4-K122, A6-K27/K57 normatif oturum sınırında tutarlı; çok oturum kabul sınaması Y11’de çelişiyor. |
| A3/A6 “+ P4-1” | KISMEN | `i32`, birlik seçimi, kullanılmayan alanların çıkarılması ve blob kuralı eklenmiş; Y1/Y2 nedeniyle kodlama bütünüyle tek anlamlı değil. |
| A3/A6 “+ üreteç ve ortak yük eşitliği” | KAPANDI | İki JSON dosyasının ortak 25 kanalı ve aynı adlı 24 yükü yeniden karşılaştırıldı; fark yok. Üretecin çalıştırılması bu incelemede yapılmadı. |

**U18 taşıyıcı eşlemeleri**

| Kaynak kalemi | A3/A6 taşıyıcısı | Denetim sonucu |
|---|---|---|
| A1 Tablo 3.11b, 36 bağlantı sayacı | `kart/baglanti_sayaclari.<aynı_ad>` | `cnt[0..35]` sırasıyla 36/36 ad eşit; aşağıdaki listeyle karşılaştırıldı. |
| A4’ün 22 çekirdek sayacı | `kart/cekirdek_sayaclari.<aynı_ad>` | `port.h` ve A1 `IDA_CORE_COUNTERS` ile 22/22 eşit. |
| A4’ün sekiz sağlık biti | `kart/cekirdek_sayaclari.saglik`, ayrıca `kart/durum.saglik` | Bit 0–7: `core_ok`, `data_ok`, `imu_rejected`, `imu_lag`, `input_dropped`, `narrowed`, `sample_conflict`, `bg_bad_input`; eşit. |
| A1’in üç ek sayacı | `sayac/gercek_zaman.kritik_erteleme`, `.cagri_izi_eksik`, `.sonuc_talebi_atildi` | Üçü de mevcut. |
| A1 yapılandırma reddi | `sayac/gercek_zaman.yapilandirma_ret`; `olay/tur=24`, `parametre`, `esitsizlik` | Parametre ve E1–E19 bilgisi taşınabiliyor. |
| A2 halka sayaçları | `kamera/halka_sayaclari` | `dusen_sim[8]` içinde neden 8=`halka_emekli_hazir`; `dusen_gorsellestirici[7]` neden 33–39; `gec_hazir`, `tuketici_gec`, `tuketici_koptu`, `halka_emekliye_ayirma`, `emekli_halka_yaniti`, `tuketici_kamera_kapali`, `tutulan_emekli_halka`, `eski_nesil`, `yok_sayilan_birakma`, `seyreltilen_durum`, `protokol_hatasi`, `baglanti`, `kopma` ve süre dağılımı alanları mevcut. |
| A2 halka olayları | `olay/tur=28` ve `29`; A6 `kamera/kare_dusme` | Emeklilik/kamera yolu kapanışı ile özgün kare düşme nedenleri taşınıyor. |
| A8 parti başlığı ve nihai durum | `algilama/parti` | Kaynak, örnek, parti/kare/nesil, pencere, kalibrasyon/üretici, bayraklar, bileşen durumu/sayısı, nihai durum, ret nedeni ve A1 parti bağı mevcut. |
| A8 ret/durum/geç yanıt sayaçları | `algilama/sayaclar.akislar[]` | `ret[10]`, `durum[9]`, `gec`, `eski_nesil`, `bilinmeyen_dusme_nedeni` mevcut; A8-K30/K65 ile eşleşiyor. |
| A5 §3.15 olayları | `olay/tur=25/26/27`; sonuç için `tur=21` ve A6 `senaryo/oge_uygulama` | Adlandırılmış taşıyıcılar var; Y4/Y5 nedeniyle bütün geçerli girdileri taşıyamıyorlar. |
| A5 doğrulama raporu | A6 `dogrulama_raporu.json` eki | Ön denetim raporundan ayrı ve zorunlu. |

36 bağlantı sayacının denetlenen adları:

`rx_bytes`, `rx_frames`, `rx_crc_err`, `rx_unknown_msgid`, `rx_iflag`, `rx_bad_len`, `rx_wrong_sys`, `rx_mav1`, `rx_overflow`, `rx_resync_bytes`, `rx_seq_gap`, `rx_stale_session`, `rx_outside_session`, `stamp_coarse`, `batch_closed`, `batch_incomplete`, `batch_skipped`, `batch_old`, `batch_dup`, `batch_merged`, `member_overflow`, `ping360_incomplete`, `ping360_bad`, `ping360_orphan`, `cmd_deferred`, `cmd_dropped`, `narrow_coalesced`, `cmd_bad_field`, `grid_missed`, `out_fixed`, `tx_frames`, `tx_dropped`, `tx_crit_stall`, `events_sent`, `events_resent`, `sessions`.

22 çekirdek sayacının denetlenen adları:

`sample_rejected`, `sample_future`, `sample_conflict`, `sample_dup_dropped`, `narrow_from_rejected`, `bg_bad_input`, `imu_rejected`, `imu_gap`, `bad_input`, `dup_seq`, `old_seq`, `wall_backstep`, `cmd_rejected`, `widen_blocked`, `evres_ignored`, `event_unknown`, `det_rejected`, `ping360_rejected`, `bg_dropped`, `bg_slice_mismatch`, `log_dropped`, `step_miss`.

**Port kodlamasının üç örneği**

İlk okuma C bildirim sırası ve A6-K52’den, ikinci okuma A4 alan tablolarından ayrı küçük uçlu alan birleştirmesiyle yapıldı. Aşağıdakiler alt yapı baytlarıdır; A6 dış yükünün uzunluk önekleri dahil değildir.

1. **GNSS — `ida_sample_t`:** başlık `(t_meas=10, t_rx=20, seq=3, window=0, flags=1, type=4, inst=0)`; değerler `(lat=1, lon=-2, alt=3000, vel=[1,0,-1], eph=1, epv=2, fix=2, n_sv=6)`. İki hesap aynı **62 baytı** verdi:

   ```text
   0a00000000000000 1400000000000000 03000000 00000000 0100 04 00
   01000000 feffffff b80b0000
   0000803f 00000000 000080bf 0000803f 00000040 02 06
   ```

   Yalnız `gnss` üyesi yazılıyor; öteki birlik üyeleri yazılmıyor.

2. **FC komutu — `ida_cmd_t`:** `cmd_id=9, type=16, source=250`, kapalı listedeki işlem ve tüp alanları, ayrılmış argümanlar sıfır. İki hesap aynı **16 baytı** verdi:

   ```text
   09000000 10 fa 05 01 00000000 00000000
   ```

   Kaldırılmış `SELECT`, `fc_ref` veya `ext[]` alanı yok.

3. **`PARAM_SAVE` sonucu — `ida_cmd_result_t`:** `cmd_id=9, kind=3, ack=1, reason=0, gen=7`, örnek blob uzunluğu 24:

   ```text
   09000000 03 01 0000 07000000 18000000
   49444150 0100 0000 0100000000000000 00000000 00000000
   ```

   `gen + len + blob` okuması **40 bayt** verir. Başlığın `gen, blob, len` sırasıyla blob için eklenen uzunluk öneki birlikte uygulanırsa sonda bir `18000000` daha vardır: **44 bayt**. Bu örnekte bağımsız okumaların tek biçime zorlanması mümkün değil; Y1.

**Arka plan takvimi**

- Canlı SIL: A1-K121 ve A4-K95 aynı; her step/tick öncesi tam `N_bg`, oturum sonu öncesi sıfır.
- Tekrar: iki kayıtlı background çağrısı **iki çağrıdır**; dört olmaz.
- `IDLE, MORE` sonrası `bg_slices=2` tutarlıdır.
- `BAD_INPUT, MORE` sonrası A4’e göre `bg_slices=1` olmalıdır; A6-K55’in mevcut sayımı bunu reddeder.
- `sil_n_bg`, A6 açılış üst verisindedir; metinde SIL için zorunludur, JSON şeması bunu zorlamaz.

**Zamanlama ve diğer hesaplar**

| Denetim | Yeniden hesaplanan sonuç |
|---|---|
| 99,8 ms karşı örneği | `D_n=100−0,4=99,6 ms`; yanıt **0,2 ms geç**, uygulama anından **0,2 ms erken**. Güncel A3 bit 1/K71 ile A1 gibi geçersiz sayıyor; bariyer beklemesi gerekmemesi bunu değiştirmiyor. |
| Kontrol/fizik | 200 Hz → 5 ms → dört fizik adımı; `L_act=10 ms` → sekiz fizik adımı. |
| A3 başlığı | `8+4+4+2+2+2+2=24 B`. |
| Parametre bloğu | `(8000−24)/8=997` kayıt. |
| Fizik yükü | `8+2+4×132+1+8×12+12=647 B`; 200 Hz’te 129400 B/s, 465,84 MB/saat. |
| Tam kamera pikselleri | 921600 B/kare; 30 Hz’te 27,648 MB/s, 99,5328 GB/saat. |
| Sade kamera pikselleri | 320×240×3×15 = 3,456 MB/s. |
| İki saniyelik tam piksel tamponu | 55,296 MB; yük başlıkları hariç. |
| Ping360 | A3 formülleri A1-K52 ile aynı: `r_i=c(2i+1)sample_period·25 ns/4`; nominal yansıma zamanı aynı ifadenin `c`’siz kısmı kadar ileride. |
| Çözümlenmiş senaryo özeti | `scenarios/ornek-havuz.cozumlenmis.json` SHA-256: `d1735f8b7eafabede9350c591bb4db7aa3a563b8ac4f45500200d33d4427cd92`; A5-K17 ile eşit. |
| HIL kimlik örneği | `20261120-120000-abcdef` SHA-256’sının ilk dört baytı `69 7a 66 aa`; LE u32 = **2858842729**. A1-K126/A3-K63/A6 eşlemesi aynı. |

Bunlar hesap ve belge denetimidir; performans ölçümü değildir.

**Komut ve JSON denetimi**

- Katalogda **19 komut** var: dört daraltıcı, yedi genişletici, sekiz nötr. Sınıflar A4 ile eşleşiyor; yedi genişleticinin tamamı `onay:true`.
- `HEARTBEAT` A1-K125’in açık istisnasıyla ağ geçidince üretiliyor. Görev yükleme ve parametre işlemleri bileşik komutlarla kapsanıyor; katalog dışı yeni komut yok.
- A3 şeması bileşik `a1.mesaj`/`a1.tur` dizilerini ve parametre sonuç verisini kabul ediyor.
- `fc_fire.parametreler` yalnız zorunlu `tube` 0–1 alanını kabul ediyor; ek özellikler yasak. Hedef, menzil, yön veya atış ölçütü taşımıyor. A3-K100, A1 ve A4’te `FIRE` olayının üreticisi çekirdek olarak kalıyor.
- İki kanal dosyası geçerli UTF-8/JSON; yinelenen JSON anahtarı bulunmadı. A3’ün 82 yerel `$ref`’i çözülüyor; desenler derleniyor.
- A3: 29 kanal/28 yük; A6: 62 kanal/44 yük. Ortak 25 kanalın ad/yük/sürüm/zaman anlamı ve aynı adlı 24 yükün bütün tanımları derin eşit.
- Revizyon değerleri ikili yüklerde `u32`; HTTP’de 1…4294967295. `emniyet_rev_doydu` ayrı bir bayraktır. Doyma olayındaki `const=4294967295` de aynı aralığın içindedir.
- Kurulu JSON Schema doğrulayıcısı bulunmadığından tam 2020-12 üst şema doğrulaması yapılmadı; bu sonuç doğrulanmış sayılmadı.

## İskelete ya da diğer bölümlere notlar

| Önceki iskelet notu | Hüküm | Güncel karşılığı |
|---|---|---|
| Bariyer kipinin tekrarlanabilirlik ifadesi | KAPANDI | İskelet 0.10 §4 artık “kartlı işlevsel testler” diyor ve beklemeli çalıştırmanın güvence sınırını koruyor. |
| Yeniden kurmanın PC girişi | KAPANDI | İskelet §3 A6 satırı, A1-K127 ve A6-K57 aynı girişe bağlı. |
| Geniş atış kontrol kapsam ifadeleri | KAPANDI | İskelet §2/§9 sözleşme kapsamını emniyet zinciri ve arayüzlerle sınırlandırıyor. İncelenen güncel port/katalogda kaldırılmış hedefleme alanlarının geri açıldığı görülmedi. |

Mimarın U16’daki onaylı `FC_FIRE` kararını bulgu saymadım. Kararı uygulamak için hedefleme içeriği eklenmesini gerektiren bir durum görmedim. U19’un oturum sınırı kararı da yeniden tartışılmadı; Y11 onun uygunluk sınamasındaki dosya yaşam döngüsü çelişkisidir.

Dış olgular canlı birincil kaynaklardan denetlendi:

- Ping360’ın 25 ns birimi ve cihaz alanları [üretici protokolü](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/) ile; kanonik JSON’un UTF-16 anahtar sırası ve sayı gösterimi [RFC 8785](https://www.rfc-editor.org/rfc/rfc8785.html) ile doğrulandı.
- MCAP kayıt düzeni [belirtimden](https://mcap.dev/spec); `closeLastChunk` ve CRC seçenekleri [yazıcı başlığından](https://raw.githubusercontent.com/foxglove/mcap/main/cpp/mcap/include/mcap/writer.hpp); 1 MiB hedef ve kaynakta yazılı `2.2.0` değeri [tipler başlığından](https://raw.githubusercontent.com/foxglove/mcap/main/cpp/mcap/include/mcap/types.hpp) doğrulandı. Okuyucunun CRC alanını ayrıştırması ve başlangıçta Footer boyunu dışlaması [okuyucu kaynağında](https://raw.githubusercontent.com/foxglove/mcap/main/cpp/mcap/include/mcap/reader.inl) görüldü. Paket yöneticisi/sürüm etiketi eşleşmesi doğrulanmadı.
- WebSocket kurucusu [WHATWG standardından](https://websockets.spec.whatwg.org/), özel kapanış kodları [RFC 6455’ten](https://www.rfc-editor.org/rfc/rfc6455.html), 401 başlığı yükümlülüğü [RFC 9110’dan](https://www.rfc-editor.org/rfc/rfc9110.html), fragment ayrımı [RFC 3986’dan](https://www.rfc-editor.org/rfc/rfc3986.html) denetlendi.

dondurulamaz