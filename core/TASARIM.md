# Ortak çekirdek iskeleti — tasarım notu

| | |
|---|---|
| İş | W2-cekirdek (`docs/gorevler/W2-cekirdek.md`); birinci aşama notu mimarca kabul edildi, ikinci aşama uygulandı |
| Yazan | `ida-uygulayici` (Opus) |
| Başvuru | `docs/sozlesmeler/A4-port.md` 1.0-aday, dondurma öncesi son tur (130 kural), `core/include/ida/port.h`, `core/include/ida/algilama_kaydi.h`; iskelet §3.1, §3.2, §4, §5 |
| Kapsam | Çağrı çerçevesi, emniyet daraltması, örnek kapısı ve IMU zamanlaması, olay altyapısı, parametreler ve komutları, zaman/seri aritmetik, matematik, günlük. Kestirim, güdüm, görev/kip/arıza, kontrol, atış kontrol ve sonar yer tutucudur |

Bu belgede ölçülmüş olmayan süre rakamı yoktur. Boyutlar §3'te ölçüm kaynağıyla verilir.

---

## 1. Modüller ve dosya sahipliği

Her modül `core/src/<modül>/` dizinindedir ve her dosyanın tek sahibi vardır. Üç paylaşım noktası bir kez çözülmüştür; sonraki işler yalnız kendi dizinine dokunur:

- **Kaynak listesi:** `core/CMakeLists.txt` her modülün `kaynaklar.cmake` parçasını içerir; modüle dosya eklemek yalnız o parçada bir satırdır.
- **Parametreler:** her modülün `<modül>_param.def` X-makro dosyası vardır; `param/param_listesi.inc` hepsini kimlik bloğu sırasıyla içerir (§5).
- **`ida_core_t`:** `cekirdek.h` yalnız modül durum tiplerini dizer; tiplerin içi modülün kendi başlığındadır. Çağrı sırası `cerceve/cerceve.c`'dedir ve bütün modül kancalarını baştan çağırır (§2); sonraki işler yalnız kancaların gövdesini doldurur.

| Modül | Dosyalar | Sahibi | Durum |
|---|---|---|---|
| Bileşim | `cekirdek.h` | W2 | tam |
| Çağrı çerçevesi | `cerceve/`: `cerceve.{c,h}` (port işlevleri, (a1)–(e)), `komut.{c,h}` (dağıtıcı), `cikti.{c,h}`, `ornek.{c,h}` (kabul kapısı), `imu_zaman.{c,h}`, `cerceve_param.def` | W2 | tam |
| Emniyet | `emniyet/emniyet.{c,h}`: yalnız kısıtlayan yön | W2 | tam |
| Emniyet gevşetmesi | `emniyet/emniyet_gevset.c` (W4 açar) | W4 atış kontrol | yok |
| Olaylar | `olay/olay.{c,h}`: kimlik, SAFETY_CLOSE, bekleyenler, sonuçlar, yalnız silen çıkış kapısı | W2 | tam; genişletici aday üretimi W4'ün kendi dosyasında |
| Parametreler | `param/`: `param.{c,h}`, `param_komut.c`, `ozet.{c,h}` (SHA-256), `param_listesi.inc` | W2 | tam |
| Zaman | `zaman/zaman.h` (yalnız `static inline`) | W2 | tam |
| Matematik | `mat/`: `mat.{c,h}`, `ida_math.{c,h}` | W2 | tam (çekirdek küme) |
| Günlük | `telemetri/`: `telemetri.h`, `gunluk.c` | W2 | tam |
| Kestirim | `kestirim/` | W3 kestirim | yer tutucu |
| Güdüm | `gudum/` | W3 güdüm ve görev | yer tutucu |
| Görev, kip, arıza | `gorev/` | W3 güdüm ve görev | yer tutucu |
| Kontrol, tahsis | `kontrol/` | W3 kontrol | yer tutucu |
| Atış kontrol | `atis/` | W4 atış kontrol | yer tutucu (yalnız sınır) |
| Sonar, arka plan işi | `sonar/` | W4 sonar | yer tutucu |

Durum, sağlık ve fc alanlarının yazımı `cerceve/cikti.c`'dedir (çekirdek tarafı W2'de tam; M10c). `src/iskelet.{c,h}` W2-depo'nun firmware yer tutucusu (`firmware/yer_tutucu/main.c`) için korunmuştur.

Yetki alanları (`armed`, `mode`) `ida_emn_t`'dedir. Emniyet modülü bunlara yalnız kısıtlayan yönde yazar; genişletme yönünde yalnız görev/kip modülü (W3) yazar (`ida_gor_komut` ve `ida_gor_adim` kancaları `ida_emn_t *` alır). Gevşetme W4'ün `ida_fc_emniyet_ornegi` kancasından, `cagri_kisit` alanlarını gevşetmeden yapılır (M9).

## 2. Step ve tick'in sırası (`ida_cer_cagri`)

1. (a1) `c`/`in` NULL → `BAD_INPUT`. (a2) kurulu işareti yok → `NOT_INIT` (A4-K126'nın sabit çıktısı; örnek belleği okunmaz).
2. Kullanılan `now_w` = max(önceki, verilen) (A4-K10; izin tabanı init'in `now_w`'si; geri adım `wall_backstep` ve günlüğü, (b2) çağrısında da; aralık dışı `now_w` geri adım sayılmaz).
3. (b) ve (b3)'ün koşulsuz üzerine yazabildiği alanların (kip, etkin emniyet değerleri, `armed` ve `inst0_var`'ın ham baytı, `inst0_tube` bitleri, FC durumu) geçerliliği yazımdan önce denetlenir; bozulma kodu `bozuk_izi`'ne kaydedilir ve sonraki yazılabilir çağrının iç tutarlılık sonucuna taşınır (A4-K89; sol bulgu 1). Daraltma bu sonuçtan bağımsız olarak her koşulda uygulanır.
4. (b) `ida_emn_daralt` → `ida_fc_daralt` → `ida_olay_kapat_iste`; SESSION_END günlüğü. Arada SESSION_START olmadan gelen sonraki SESSION_END yeni kapanma olayı üretmez, yine daraltıcı girdidir (A4-K73).
5. (b3) SESSION_START, `out` NULL olsa da (A4-K72, K113; sıra (b) → (b3) → (b2)); kapalı liste: her (`type`, `inst`) için örnek izi (IMU, Ping360 dahil), IMU izi (bayat sayılır; taze idiyse `IMU_STALE` = 1; `IMU_LAG` çözülme sayacı sıfırlanır, mandal çözülmez), zaman çerçevesi izi; A4-K73'ün oturum sonu izi; modüllerin yeniden başlaması bekler. A4-K118'in süre başlangıcı atış kontrol işinindir.
6. (b2) `out` NULL → (b) ve (b3) uygulanmış kalır; kapanma ve günlük bekler; çekirdeğin dilim sayımı karşılaştırılmadan sıfırlanır; `bg_bad_input` bayrağı silinmez (A4-K117); init'ten sonraki ilk çağrıysa `step_miss` tabanı kurulur; emniyet örneğinden doğan kapanma `narrow_from_rejected`'a sayılır; iç tutarlılık denetimi yapılmaz; `cnt.bad_input` artmaz; `BAD_INPUT`.
7. `out` sıfırlanır (A4-K27, K121); iç tutarlılık (A4-K89): önce kaydedilmiş `bozuk_izi`, yoksa tam denetim; saptanan bozulmada `core_ok` init'e kadar 0 kalır (mandallı), `CORE_INTEGRITY` bir kez; çağrı başına izler: `bg_slices` karşılaştırması, `bg_bad_input`, `step_miss` (taban ilk çağrı; tabanın `prev_overrun`'u sayılmaz, 3.8).
8. (c) yapısal doğrulama, (d) sıra; hata → sayaç, `ida_kom_reddet`, `ida_cik_yaz`.
9. (e): modüllerin yeniden başlaması; kayıt deposu izleme; IMU ve örnek kapısı; kabul edilenler sahiplerine (SAFETY → emniyet ve `ida_fc_emniyet_ornegi`; BATT/LEAK → görev; diğerleri → kestirim); Ping360; algılama (`ida_fc_algilama`); tf kabulü; IMU gecikmesi ve bayatlık; olay sonuçları ve zaman aşımı; kestirim; görev; komutlar (3.17); güdüm; step OK'ta atış kontrol, kontrol, sonar; çağrı sonunda PARAM_COMMIT ve PARAM_SAVE kararı ve commit uygulaması (A4-K81, K82); `narrow_from_rejected`; `ida_cik_yaz`.

## 3. `ida_core_t` ve boyutlar

Sıra: koruma sözcüğü, kurulu işareti ve tümleyeni, çerçeve, emniyet, olay, örnek kapısı, IMU zamanlaması, parametreler (blob tamponu dahil), günlük, kestirim, güdüm, görev, kontrol, atış kontrol, sonar, koruma sözcüğü.

| Ölçüm | Değer | Kaynak |
|---|---|---|
| `sizeof(ida_core_t)` | 2160 B (Cortex-M33 ve macOS arm64 aynı); hizalama 8 | Derleyicinin çıktısındaki sabit (GCC 14.3.1 `-O2`, Apple Clang) |
| Çekirdek arşivi, fw-release | text 19 820 B, data 0, bss 0 | `arm-none-eabi-size -t libida_core.a` |
| Çekirdek arşivi, fw-debug | text 21 166 B, data 0, bss 0 | aynı |
| En büyük yığın çerçevesi (fw-release) | 344 B (`blok_isle`, SHA-256) | `-fstack-usage` |

`ida_core_info`: port 1.0, `core_size` = `sizeof`, `core_align` = `_Alignof`, `param_schema_hash` her çağrıda hesaplanır (önbellek yok), `core_version` = `0x000100` (0.1.0).

A4-K22: dosya ya da işlev ölçeğinde `const` olmayan nesne yoktur; tablolar işaretçisiz `static const`'tır. `cekirdek.yazilabilir_veri` testi çekirdek arşivinde yazılabilir veri simgesi olmadığını simge tablosundan gösterir (host Mach-O ve ARM ELF); karşı örneği yakalar.

## 4. Emniyet ve olay sınırı

- **Emniyet (W2):** `kisitli` alan bitleri yalnız `|=` ile ya da SESSION_END'de hepsi birden kısıtlayana yazılır; init'te hepsi kısıtlayandır (A4-K116, K128). Mandal bitleri, `cmds[]`'teki daraltıcılar (ilk `min(n_cmds, 8)`), SESSION_END ve yapısal geçerli her SAFETY örneğinin (her `inst`) kısıtlayıcı içeriği (b)'de uygulanır. Kapanma SAFETY_CLOSE ister (A4-K125 birleştirmesi olay modülünde); SAFETY_INPUT günlüğü; kapanmaya yalnız reddedilen örnek yol açtıysa `narrow_from_rejected` (oturum sonu sayılmaz).
- **Olaylar (W2):** kimlik `(boot_count << 32) | ctr`; SAFETY_CLOSE'lar önce, en çok iki; bekleyen kümesi 8 (7 + 1); sonuç kabulü (yalnız 3.5'in tanımlı kodları; tanımsız kod `evres_ignored`, olay beklemede kalır, `last_event_result` değişmez; A4-K69), 100 ms paylı zaman aşımı. Çıkış kapısı W4'ün `aday[]` dizisine koyduğu SAFETY_OPEN/FIRE'ı yalnız siler: step OK değil, daraltma, IMU_LAG (bu ikisi `widen_blocked`), kısıtlı tüp, bekleyen sonuç, FIRE'da `armed ∧ ¬is_safe` yok, `hold_ns` aralık dışı, olay kimliği ya da yuva yok, `events[]`'te yer yok. Yer tutucu çekirdekte aday üreten kod yoktur.
- **Atış kontrol sınırı:** `atis/atis.h`'taki `ida_fc_daralt`, `ida_fc_emniyet_ornegi`, `ida_fc_algilama`, `ida_fc_olay_sonucu`, `ida_fc_olay_bilinmiyor`, `ida_fc_engel`, `ida_fc_durum`, `ida_fc_komut`, `ida_fc_adim`. İç durum makinesi, hedefle ilgili hesap ve fırlatma mantığı bu işte yoktur.

## 5. Parametreler

Satır biçimi: `IDA_PARAM(kimlik, AD, TIP, SINIF, en_az, en_cok, varsayilan, alan)`; kimlik blokları `0x01xx` çerçeve, `0x02xx` kestirim, `0x03xx` güdüm, `0x04xx` görev, `0x05xx` kontrol, `0x06xx` atış, `0x07xx` sonar. Kanonik şema metni: kimlik artan sırada `"<id>\t<ad>\t<type>\n"` (ondalık, ASCII); özet SHA-256'nın ilk 8 baytı, küçük uçlu.

| Kimlik | Ad | Tip, sınıf | Aralık | Varsayılan | Kural |
|---|---|---|---|---|---|
| 0x0101 | `T_imu_stale` | U32 (ms), B | 5–30 | 20 | K39, 3.9 |
| 0x0102 | `VALID_FOR_NS` | U32 (ns), B | 60–450 ms | 100 ms | K58 |
| 0x0103 | `T_lag_set` | U32 (ms), B | 10–50 | 20 | K37, 3.9 |
| 0x0104 | `T_lag_clear` | U32 (ms), B | 1–50; grup: ≤ `T_lag_set` | 10 | K37, 3.9 |
| 0x0105 | `N_lag_clear` | U32 (zaman çerçeveli çağrı), B | 1–200 | 20 | K37, 3.9 |
| 0x0106 | `T_imu_gap_tol` | F32 (oran; sınır = oran × `window_ns`), B | 0,01–1 | 0,1 | K38, 3.9 |
| 0x0107 | `T_det_stale` | U32 (ms), B | 100–10 000 | 2000 | K118, 3.9 |
| 0x0108 | `FC_APPLY_BY_NS` | U32 (ns), B | 16–1000 ms | 20 ms (A1 §3.9) | K66 |
| 0x0109 | `FC_HOLD_NS` | U32 (ns), B | 60–450 ms | 200 ms (A1 §3.9) | K66 |

`T_*` ve `N_lag_clear` satırlarının adı, birimi, tipi, varsayılanı ve aralığı A4 §3.9 tablosunun birebir karşılığıdır (`cekirdek.birim.param` `test_39_parametre_tablosu`). ms parametresi kullanılırken ns'ye `int64_t`'de çevrilir (`zaman/zaman.h` `ida_ms_ns`: ns = ms × 1 000 000); `port.h`'nin ns sabitleri (`IDA_IMU_STALE_*_NS`, `IDA_DET_STALE_DEFAULT_NS`) değişmez ve testte ms değerleriyle karşılaştırılır. `VALID_FOR_NS`, `FC_APPLY_BY_NS`, `FC_HOLD_NS` 3.9 tablosunda yoktur; adları ve birimleri bu işin seçimidir (ns, `uint32_t`'ye sığar). B sınıfı (yalnız silahsızken) seçimi uygulamanındır: zamanlama emniyetine giren değerler silahlıyken değişmez.

Kayıt durumu (A4-K83; sol bulgu 2): `plat.param_gen`, `param_store`'un bildirdiği son tamamlanan kayıt denemesinin kuşağıdır (OK'te kalıcı bloğun, FAIL'de başarısız denemenin). Bekleyen PARAM_SAVE'in tamamlanması `param_store` ∈ {OK, FAIL} ve `param_gen` = beklenen `gen` eşleşmesiyle, durum kodu değişmeden de (aradaki BUSY görülmeden) saptanır; art arda iki FAIL ayrı ayrı saptanır. PARAM_SAVE günlüğü durum geçişinde ya da bu tamamlanmada bir kez üretilir; başka bir kuşağın OK'i ya da FAIL'i beklemeyi bitirmez; FAIL'de `param_ok` değişmez ve `gen` tabanı yükselmiş kalır (A4-K82). `cekirdek.birim.param` `test_k83_ayni_durum_koduyla_tamamlanma`.

`core/test/dis_basvurular.cmake`'in izinli listesindeki `bzero`: Apple Clang `-O2` sıfır ilklendirmeyi `bzero` çağrısına çevirir (host-release); firmware arşivinde geçmez.

Parametre komutları (A4-K81, K82, 3.17): PARAM_SET ve PARAM_GET komut anında yanıtlanır (GET çağrı başındaki uygulanmış değeri okur). PARAM_COMMIT ve PARAM_SAVE'in kararı kabul edilen çağrının sonundadır ve girdideki sıradan bağımsızdır: commit o anda bekleyen bütün değerleri (aynı çağrının sonraki PARAM_SET'leri dahil) doğrular; aynı çağrıdaki bütün commit'ler aynı sonucu ve her biri bir PARAM_COMMIT günlüğü alır; bekleyen değer yokken ACCEPTED olan commit uygulanmış sayılır. PARAM_SAVE'in sırayla belirlenen koşulları (ONE_PER_CALL, ARMED, PARAM_BUSY) komut anında, PARAM_INVALID (aynı çağrının kabul edilen commit'i sayılır) ve `gen` doyması çağrı sonunda değerlendirilir; kabulde commit sonrası küme kodlanır. `armed` koşulları komutun `cmds[]` sırasında işlendiği andaki `armed`'la (commit'lerden biri silahlıyken işlendiyse commit `ARMED`) ve girdideki her `ARM` (yinelenen ya da NARROWED alan dahil) sayılarak denetlenir. `gen` = max(çekirdeğin son `gen`'i (yoksa 0), `plat.param_gen`) + 1; max(…) = 0xFFFFFFFF ise SAVE `REJECTED` / `OUT_OF_RANGE` alır ve blok üretilmez (ret nedeni A4 3.17 PARAM_SAVE satırından ve A4-K82'den). Çağrı sonunda karara bağlanan sonuçların reddi `cnt.cmd_rejected`'e orada sayılır.

## 6. Yer tutucu çekirdeğin görünen davranışı

İtki yok, genişletici olay yok: `armed`'ı 1 yapan bir yol yoktur, `thr_cmd` yalnız `armed ∧ ¬is_safe` iken kontrolün önerisinden kopyalanır, olay kapısı yalnız siler. Kabul md.4'ü `cekirdek.birim.belirlenimcilik` sınar (sabit tohum, 2 × 20 000 karışık çağrı).

| Alan | step `OK` | tick `OK` | step `IMU_REJECTED` |
|---|---|---|---|
| `thr_valid` / `is_safe` / `thr_cmd` | 1 / 1 / 0 | `is_safe` / 1 / 0 (A4-K56) | `is_safe` / 1 / 0 |
| `valid_for_ns` | `VALID_FOR_NS` | `thr_valid` ise aynı | aynı |
| `armed`, `fs_level` | 0, NONE | aynı | aynı |
| `seq`, `t_ref` | tf | tf; ızgarada son geçerli | tf |
| `events` | (b)'nin SAFETY_CLOSE'u (SESSION_END ya da kapanma) | aynı | aynı |
| `nav` | `nav_valid` 1; `t` = `t_ref`, `est_level` INIT, `q_nb` (1,0,0,0), diğerleri 0 (A4-K87) | 0 | 0 |
| `status` | `mode` INIT, `fs_reasons` yalnız IMU_LAG, `regime`/`phase` 0, görev IDLE | aynı | aynı |
| `fc` | SAFE; maske her bit tanımına göre, MODE ve DET_STALE hep kurulu (M7) | aynı | aynı |
| `det_tx`, `bg` | 0 | 0 | 0 |

Komut yanıtları tablo 3.17'nin yer tutucu durumuna uygulanmasıdır; modülü bu sürümde olmayan türler `UNSUPPORTED` / `NONE` alır (A4-K124):

| Komut | Yanıt |
|---|---|
| ARM | `EVENTS_OFF` → `PARAM_INVALID` → `PARAM_BUSY` → `NOT_ARMABLE` (kestirim INIT) |
| SET_MODE (GUIDED/AUTO) | `BAD_MODE` |
| FC (K119 dışı) | `OUT_OF_RANGE` |
| FC_ARM, FC_TEST_ENTER, FC_FIRE | `SESSION` → `EVENTS_OFF` → `BAD_MODE` (izin kümesi boş) |
| FC_RESET | `SESSION` → `BAD_MODE` |
| FC_TEST_EXIT | `ACCEPTED` (etkisiz) |
| PARAM_* | 3.17'nin satırları, tam |
| HEARTBEAT, MANUAL, GUIDED_SP, MISSION_*, FAILSAFE_RESET, bilinmeyen tür | `UNSUPPORTED` |
| cmds[]'te daraltıcı | `ACCEPTED`, pencereden muaf |

## 7. Kararlar ve belirsizliklerin kapanışı

Kararlar (mimar, 2026-11-20): M1 örnek kapısı ve IMU zamanlaması bu işte; M2 olay altyapısı bu işte; M3 parametre komutları bu işte; M4 yanıtlar 3.17'den, modülü olmayan tür `UNSUPPORTED`; M5 `thr_valid = is_safe`; M6 INIT nav; M7 maske tanımına göre, DET_STALE hep kurulu; M8 A1 §3.9 varsayılanları; M9 gevşetme W4'ün dosyasında, W2 kapanma yolunu test kaynağından sınar; M10 arıza W3 görev/güdüm, sonar ayrı W4 işi, durum/sağlık/günlük W2'de tam.

Birinci aşamanın belirsizliklerini kapatan A4 metni:

| Madde | Kapatan | Madde | Kapatan |
|---|---|---|---|
| E1 | K126, K88, K89 | İ10 | 3.17 ARM satırı |
| E2 | K124, 3.17 | İ11 | K116 (gevşetme yalnız `inst` 0) |
| E3 | K125 | İ12 | K122, 3.11 (c) |
| E4 | K81 | İ13 | K116, K33 |
| E5 | K128, K39 | İ14 | 3.9, init sırası (5) |
| İ1 | K46, K113, K114, K48, 3.11 (b)/(c) | İ15 | K129 |
| İ2 | 3.11 (b3), K72 | İ16 | 3.8 `step_miss` |
| İ3 | K116, K72 | İ17 | K38 |
| İ4 | 3.11 (b2), K63, K90, 4.15 | İ18 | K50 |
| İ5 | K56 | İ19 | K46 |
| İ6 | K90, 3.15, 4.14 | İ20 | K127 |
| İ7 | K45 | İ21 | 3.11 arka plan sırası |
| İ8 | K10 | İ9 | K48 |
| F1 | §4 girişi (130) | F6 | §7 |
| F2 | 3.11 (c) | F7 | `port.h` yorumları |
| F3 | 3.15 önem sütunu | F8 | dayanak 0.10 |
| F4 | K37 aralıkları, 3.8 | F9 | K39 ("kesin büyük") |
| F5 | K80 (5), 3.15 | | |

İkinci aşamada uygulamanın seçtiği okumalar A4'ün son metniyle (mimar, 2026-11-20) **kapandı**; kod ve testler son metne göredir:

| Konu | Son metin | Durum | Test |
|---|---|---|---|
| (b2) ile (b3) sırası | (b) → (b3) → (b2): `out` NULL çağrısındaki SESSION_START işlenir (3.11, A4-K72, K113) | kapandı; kod değişti | `cerceve` `test_b3_out_null_cagride` |
| (b2)'de `bg_bad_input` | bayrak silinmez, ilk yazılabilir çağrıya taşınır (A4-K117) | kapandı; kod değişti | `arka_plan` `test_k117_b2_bayragi_tasir` |
| (b2)'de dilim sayımı, geri adım | karşılaştırmadan sıfırlanır; `wall_backstep` sayılır, günlüğü bekler | kapandı; seçilen okuma | `cerceve` `test_b3_out_null_cagride` |
| `step_miss` tabanı | taban çağrısındaki `prev_overrun` sayılmaz (3.8) | kapandı; kod değişti | `cerceve` `test_step_miss_tabani` |
| Günlük taşması | önce en düşük önem (büyük `severity`), eşitte sonra üretilen atılır; bekletilenler önce üretilmiş; kalanlar üretim sırasıyla; atılan her biri `log_dropped` (A4-K90) | kapandı; kod değişti | `cerceve` `test_k90_gunluk_tasmasi` |
| FC_STATE/FC_INHIBIT tabanı | init sonrası ilk yazılabilir çağrının sonu (A4-K90) | kapandı; seçilen okuma | `cerceve` `test_k90_fc_gunluk_tabani` |
| `n_cmds` > 8 | `cmds[]` için sonuç yok; ≤ 8 reddedilen çağrıda daraltıcılar ACCEPTED, öteki CALL_REJECTED (A4-K114, 4.14) | kapandı; seçilen okuma | `cerceve` `test_k113_k114` |
| Commit zamanı ve sırası | çağrı sonunda, sıradan bağımsız; birden çok commit tek commit (A4-K81) | kapandı; kod değişti | `param` `test_k81_commit_sirasindan_bagimsiz`, `test_k81_birden_cok_commit` |
| Boş commit | uygulanmış commit; aynı çağrının SAVE'i için de (A4-K78) | kapandı; kod değişti (aynı çağrı) | `param` `test_k78_bos_commit_ve_save_ayni_cagri` |
| SAVE `gen` | max(son, `param_gen`) + 1; doyar, 0xFFFFFFFF'te OUT_OF_RANGE (A4-K82, 3.17) | kapandı; doyma eklendi | `param` `test_k82_gen_kurali_ve_doyma` |
| Reddedilen PARAM_GET/SET `value` | (id, 0, 0) (3.14, A4-K43) | kapandı; seçilen okuma | `param` `test_set_get_commit` |
| NOT_INIT `out.fs_level` | DISARM; `armed` 0; `src` çağrılan işlevinki (A4-K126) | kapandı; seçilen okuma | `cerceve` `test_k126_kurulmamis_cikti_sabit` |
| Sayaç örtüşmeleri | gelecek damgalı örnek yalnız `sample_future`; bayt bayt aynı yineleme yalnız `sample_dup_dropped`; Ping360 reddi yalnız `ping360_rejected` (3.8) | kapandı; seçilen okuma | `ornek` testleri |
| K24 eşitliği | `save.blob` işaretçisi değil içerik (A4 §7) | kapandı; seçilen okuma | `belirlenimcilik` `test_k24_ve_md4` |
| K80 kısa blok | 24 bayttan kısa blok denetim 1; `param_len` = 0 blok yok (0) | kapandı; seçilen okuma | `param` `test_blok_denetimleri` |
| Parametre birimleri | 3.9 tablosu (ms, ad ve aralıklar) | kapandı; kod değişti | `param` `test_39_parametre_tablosu` |
| HEARTBEAT, FAILSAFE_RESET | yer tutucuda (3)'te `UNSUPPORTED` / `NONE`; (1) yineleme ve (2) genişletici önceliği önce (3.17 dipnotu, A4-K124) | kapandı; seçilen okuma | `komut` `test_317_yer_tutucu_satirlari` |

Üçüncü aşamada açık bırakılan altı okuma A4'ün fark incelemesi turuyla (W1-uyum D-23; mimar, 2026-11-20) **kapandı**:

| Okuma | Karar | Kod | Test |
|---|---|---|---|
| `step_miss` tabanı | init'ten sonraki ilk çağrı, `out` NULL olsa da (3.8, 3.11 (b2)) | değişti | `cerceve` `test_step_miss_tabani` |
| (b2)'de `narrow_from_rejected` ve iç tutarlılık | sayaç artar; denetim sonraki yazılabilir çağrıda (3.11 (b2)); üzerine yazılan bozulmanın kaydı taşınır (sol bulgu 1) | değişti | `cerceve` `test_k89_ustune_yazilan_bozulma` |
| "ms" biriminin kapsamı | yalnız 3.9 tablosu; `_NS` adlı her parametre ns | aynı | `param` `test_39_parametre_tablosu` |
| `armed` anı ve ARM sayımı | komutun işlendiği andaki `armed`; girdideki her `ARM` (A4-K81, K82) | değişti (anlık `armed`) | `param` `test_b_sinifi_ve_arm` |
| FAIL'li SAVE'in `gen`'i | üretilmiş sayılır, taban yükselir (A4-K82) | aynı | `param` `test_k83_ayni_durum_koduyla_tamamlanma` |
| (b3)'ün kapsamı | kapalı liste; `IMU_LAG` mandalı çözülmez, çözülme sayacı sıfırlanır, taze IMU'da `IMU_STALE` = 1 | değişti | `ornek` `test_b3_imu_izi` |

Aynı turun öteki kararları: tanımsız olay sonucu yok sayılır (A4-K69; `olay` `test_k69_tanimsiz_sonuc_kodu`); ikinci SESSION_END olay üretmez (A4-K73; `emniyet` `test_oturum_sonu`); `mat` birimleme işlevleri ölçeklenmiş norm kullanır, sonlu olmayan girdi ve ara sonuçta başarı dönmez (sol bulgu 3; `zaman_mat` `test_birimleme_sinirlari`). Bu işlerde davranışı zaten uyan maddeler: bekletilen kapanma her yazılabilir dönüşte (F2), `cmds[]` daraltıcısının pencereden iki yönde muafiyeti ve `tube`/`arg`'ının okunmaması (F4), yer tutucuda `GUIDED_SP` = `UNSUPPORTED` (F3; 3.17 dipnotu), geri adım tabanı ve aralık dışı `now_w` (B1, B6), `t_w` kuralları (B2, B3), parametre işlevlerinin denetim sırası (B4, B5), şemada olmayan kayıt (B7), IMU `window_ns` = 0 (B8), `ACCEPTED`/`DUPLICATE`/`UNSUPPORTED`'ta `reason` = NONE (B9), `cmd_rejected` (B10), `input_dropped` (B14), `boot_count` (B16), tanımlayıcı tipleri (B17), IMU_STALE günlükleri (B18), oturum sonu çağrısının örnekleri (B19).

**W3 platform notu (D-17, A4-K105, A1-K63).** Platform bir daraltıcı komutu mandala aldığı anda, kendisinden önce alınmış ve henüz hiçbir çekirdek çağrısına girmemiş bütün genişletici komutları — açık partide, kapanmış ama çağrılmamış (bekleyen) partide ve erteleme tamponunda — atar ve her biri için `REJECTED` / `NARROWED` sonucunu kendisi üretir; nötr komutlar etkilenmez. Bu bir platform kuralıdır; çekirdek kodu değişmez: mandaldan sonra alınıp mandalla aynı çağrıya düşen genişleticiyi çekirdek A4-K45 ile reddeder.

## 8. Kural dağılımı tablosu

Sınıflar: **i** bu işte tam; **ii → iş** yer tutucu; **iii** platformun. Bölünmüş satırda ilk sınıf baskındır.

| Kural | Konu | Sınıf | Gerekçe |
|---|---|---|---|
| K01 | `port.h` ve A8 başlığı C17/C++'tan uyarısız | i | Başlık değişmez; dört ön ayarda ve W2-depo'nun sözleşme başlıkları hedefinde derlenir |
| K02 | Sürüm uyuşmazlığı → `VERSION` | i | Init sırası (3) |
| K03 | Port yapıları bayt düzeniyle taşınmaz | iii | Kodlayıcılar A1/A6'nın; blok alan alan kodlanır |
| K04 | Sayı tipleri, `double` yok | i | `port.h`; çekirdekte `double` yok |
| K05 | Birimler | i | `port.h` |
| K06 | Aynı başlık, derleyici, ikili | iii | Derleme sistemi |
| K07 | İki saat alanı karışmaz | i | Ayrı tipler; derleme hatası testi |
| K08 | Zaman aralığı | i | (c), init (4), arka plan (3), kapı |
| K09 | Duvar saati azalmaz | iii | Platform |
| K10 | Kullanılan `now_w` | i | `now_guncelle`; taban init `now_w`'si, aralık dışı geri adım değil |
| K11 | SIL'de `now_w = tf.t` | iii | SIL koşucusu |
| K12 | Saat ve rastgele kaynak okunmaz | i | Dış başvuru denetimi |
| K13 | `t_meas` anlamı | iii | Platform aktarır; kullanımı W3 |
| K14 | Alım damgası; gelecek damga reddi | iii + i | Damga platformun; ret ve sayaç kapıda |
| K15 | seq arttı t artmadı → `BAD_INPUT` | i | (c) |
| K16 | Partiler sırayla | iii | Platform |
| K17 | `imu_present` | i | (c) |
| K18 | Izgara takvimi | iii | Platform |
| K19 | Dağıtım sınırı | iii | Platform |
| K20 | Eşzamanlı çağrı yok | iii | Platform; çekirdekte paylaşılan durum yok |
| K21 | Bloklama ve geri çağırma yok | i | Dış başvuru denetimi |
| K22 | Değişebilir statik durum yok | i | Simge tablosu denetimi |
| K23 | Süre bütçeleri | ii → W3 sonu ölçümü | Kartta ölçüm |
| K24 | Belirlenimcilik | i | İki örnek iç içe ve ayrı koşu |
| K25 | Dönüş kodu ve sıra | i | (d) |
| K26 | Hata dönüşünde işleme yok | i | (e)'ye girilmez |
| K27 | Her alan yazılır | i | `out` sıfırlanır, tek yazım |
| K28 | Hata dönüşünde itki ve genişletici yok | i | `ida_cik_yaz` |
| K29 | Gönderme kararı A1'in | iii | Platform/A1 |
| K30 | Tür ve kap denetimi | i | Kapı; gövde okunmaz |
| K31 | Kapasite → `BAD_INPUT` | i | (c) |
| K32 | Platformun taşması | iii | Platform |
| K33 | Kabul koşulları | i | Kapı |
| K34 | SAFETY eşit `t_meas` | i + ii → W4 atış | Kabul kapıda; değer güncellemesi gevşetmede |
| K35 | Sıradan bağımsızlık | i | Kapı sıralar |
| K36 | Tazelik çekirdekte | i + ii → W3 kestirim | İz kapıda; tür başına karar kestirimde |
| K37 | IMU_LAG mandalı | i | `imu_zaman`; IMU yokken hesaplanmaz, SESSION_START mandalı çözmez |
| K38 | IMU boşluğu | i | Sayaç ve günlük; filtre davranışı W3 |
| K39 | Bayatlıkta nötr güvenli çıktı | i | Çıkış, parametre doğrulaması; IMU_STALE günlükleri yalnız geçişte ((b3) dahil) |
| K40 | Ping360 doğrulaması | i + ii → W4 sonar | Doğrulama kapıda; tüketim sonarda |
| K41 | Algılama partisi | ii → W4 atış | `ida_fc_algilama` |
| K42 | Montajlar, dönme kolu | ii → W3 kestirim | |
| K43 | Sonuç sayısı ve sırası | i | Dağıtıcı |
| K44 | Daraltıcı girdiler | i | Emniyet, dağıtıcı |
| K45 | Daraltma hâkim | i | Dağıtıcı ve olay kapısı |
| K46 | Mandallar, `cmds[]` daraltıcıları | i | (b), (c); pencereden iki yönde muaf, `FC_DISARM`/`FC_ABORT`'un `tube`/`arg`'ı okunmaz |
| K47 | Bütün komutlar işlenir | i + ii → W4 atış | FC iç kapasitesi W4'ün |
| K48 | Yineleme penceresi | i | Dağıtıcı; yalnız daraltıcı olmayan komutlar |
| K49 | MANUAL | ii → W3 kontrol | Yer tutucu `UNSUPPORTED` |
| K50 | GUIDED_SP | ii → W3 güdüm | Yer tutucu `UNSUPPORTED` (3.17 dipnotu); aralık ve maske denetimi W3'ün |
| K51 | Görev yüklemesi | ii → W3 görev | Yer tutucu `UNSUPPORTED` |
| K52 | HEARTBEAT canlılığı | ii → W3 görev | Yer tutucu `UNSUPPORTED` |
| K53 | Sürekli komut alanları | i | Çıkış |
| K54 | `thr_cmd` anlamı | ii → W3 kontrol | ESC dönüşümü platformun |
| K55 | `is_safe` | i | Çıkış |
| K56 | Tick ve IMU_REJECTED | i | `thr_valid = is_safe` |
| K57 | `seq`, `t_ref` | i | Çıkış; olayların seq/t_decide'ı |
| K58 | `valid_for_ns` | i | Parametre |
| K59 | Platform nötrler | iii | Platform |
| K60 | Olay alanları | i | Olay modülü |
| K61 | Olay kimliği, olaysız çalışma | i | Olay, ARM reddi |
| K62 | Sayaç sınırları | i | Olay |
| K63 | Her olay bir kez | i | Olay; (b2) bekletmesi |
| K64 | OPEN/FIRE'ın yeri | i | Kapı yalnız siler; üretim W4 |
| K65 | SAFETY_CLOSE önce | i | Olay |
| K66 | Süre aralıkları, numarasız kapanma | i + ii → W4 atış | Parametreler W2; iznin süresi W4 |
| K67 | İlk kabul kapısı | iii | Platform |
| K68 | Son sonuç döndürülür | iii | Platform |
| K69 | Bekleyen olayın sonucu | i | Olay; tanımsız kod yok sayılır, olay bekler |
| K70 | Sonuçsuz olay | i + ii → W4 atış | Sayaç W2; güvenli duruma alma `ida_fc_olay_bilinmiyor` |
| K71 | Oturum sonu sonuçları | iii | Platform |
| K72 | SESSION_START | i + ii → W3 kestirim, W4 atış | (b3) kapalı listesi W2; yeniden başlatma kancaları W3; A4-K118 süre başlangıcı W4 |
| K73 | SESSION_END | i | (b); ikinci SESSION_END olay üretmez (`oturum_kapali`) |
| K74 | Platformun silmesi | iii | Platform |
| K75 | Emniyet girdileri yalnız örnekle | iii | Platform ve sim |
| K76 | Batarya, sızıntı eşikleri | ii → W3 görev | |
| K77 | INIT ve silahsız başlangıç | i | Init |
| K78 | Geçersiz blok, ARM engeli | i | Parametre |
| K79 | Blok bayt düzeni | i | Parametre |
| K80 | Blok denetimleri | i | Grup kancaları modüllerin |
| K81 | SET, COMMIT | i | `param_komut.c` |
| K82 | SAVE | i | `param_komut.c` |
| K83 | BUSY'de ARM reddi, kayıt sonucu | i + iii | Bildirim platformun; tamamlanma `gen` eşleşmesiyle |
| K84 | Platform saklar | iii | Platform |
| K85 | Parametre işlevleri | i | `param.c` |
| K86 | Açılış sayacı | iii | Platform |
| K87 | `nav` | i + ii → W3 kestirim | INIT içeriği W2; kestirim W3 |
| K88 | `status` | i | Çıkış |
| K89 | `core_ok` | i | Koruma sözcükleri, tümleyen, durum değerleri; (b)/(b3) öncesi kayıt; mandallı |
| K90 | Günlük | i | `gunluk.c` |
| K91 | `fc` alanı | ii → W4 atış | Yer tutucu maske M7; çerçeve tablo dışını maskeler |
| K92 | `det_tx` | ii → W4 sonar | Yer tutucu 0 |
| K93 | Arka plan dilimi | ii → W4 sonar | Yer tutucu IDLE |
| K94 | Arka plan sonucu | ii → W4 sonar | |
| K95 | SIL `N_bg` | iii | SIL koşucusu |
| K96 | Kart arka plan sıklığı | iii | Platform |
| K97 | Bellek ayırma, özyineleme yok | i | Bellek ve dış başvuru denetimi; yığın raporu |
| K98 | `ida_core_info` | i | Karşılaştırma platformun |
| K99 | Girdi işaretçisi saklanmaz | i | Ping360 içeriği yer tutucuda kullanılmaz |
| K100 | Blob işaretçisinin ömrü | i | Tampon `ida_core_t`'de |
| K101 | Önkoşullar | i | Her port işlevi |
| K102 | Bekleyen parti ve birleştirme | iii | Platform; `input_dropped` W2 |
| K103 | IMU reddi | i | (e) |
| K104 | Genişleticiler | i | Dağıtıcı |
| K105 | Erteleme ve atma; mandalda bekleyen genişleticilerin atılması (D-17) | iii | Platform (§7 W3 notu) |
| K106 | Taşıma yinelemesi | iii | Platform/A1 |
| K107 | İznin açık sayılması | ii → W4 atış | |
| K108 | IMU_LAG'da genişletici yok | i | Dağıtıcı, kapı |
| K109 | `bg_slices` | i | Çerçeve |
| K110 | Arka plan iş birimi | ii → W4 sonar | |
| K111 | Emniyet girdisinin kapanması | i | (b); test kancasıyla |
| K112 | Çağrıda tek SAVE | i | `param_komut.c` |
| K113 | Değerlendirme sırası | i | Çerçeve |
| K114 | Reddedilen çağrı ve NOT_INIT sonuçları | i | Dağıtıcı |
| K115 | Eşit anahtarlı örnekler | i | Kapı |
| K116 | Etkin emniyet değerleri | i + ii → W4 atış | Kısıtlayan yön W2; gevşetme W4 |
| K117 | Arka plan geçersiz girdi | i | Çerçeve |
| K118 | Kamera akışı bayatlığı | ii → W4 atış | Yer tutucu DET_STALE hep kurulu |
| K119 | FC komut biçimi | i | Dağıtıcı |
| K120 | Neden kodu aralığı | i | Sonuç yazımı |
| K121 | Etkin olmayan üye sıfır | i | Çıkış sıfırlaması |
| K122 | Oturum sonu çağrısı | iii + ii → W4 atış | Tür ayrımı ve alan kısıtının BAD_INPUT olmaması W2; akış sıfırlaması W4 |
| K123 | Sonucu beklenen izin olayı | ii → W4 atış | Bekleyen kümesi ve maske biti W2 |
| K124 | Komut ret tablosu | i + ii → W3/W4 | Dağıtıcı ve yer tutucu yanıtları W2; modül satırları sahiplerinin |
| K125 | SAFETY_CLOSE birleştirmesi | i | Olay |
| K126 | NOT_INIT sabit çıktısı | i | `ida_cik_kurulmamis` |
| K127 | `armed` ve `mode` bağımsız | i + ii → W3 görev | Daraltma tarafı W2; kip semantiği W3 |
| K128 | İlk kabulden önceki değerler | i | IMU bayat, tüpler boş |
| K129 | Bekleyen olay kümesi | i | Olay |
| K130 | Taşınan emniyet örneği (`VALID` = 0) | i + iii | Kapı ve (b) W2; teslim platformun |

Sayım: 130 kural (K01–K130, boşluk ve yineleme yok; betikle denetlendi). Baskın sınıfa göre i 87, ii 17, iii 26 (bölünmüş satırlar: K14 iii + i, K122 iii + ii; i + ii: K34, K36, K40, K47, K66, K70, K72, K87, K116, K124, K127; i + iii: K83, K130).
