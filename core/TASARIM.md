# Ortak çekirdek iskeleti — tasarım notu

| | |
|---|---|
| İş | W2-cekirdek (`docs/gorevler/W2-cekirdek.md`); birinci aşama notu mimarca kabul edildi, ikinci aşama uygulandı |
| Yazan | `ida-uygulayici` (Opus) |
| Başvuru | `docs/sozlesmeler/A4-port.md` 1.0-aday, dondurma öncesi son tur (130 kural), `core/include/ida/port.h`, `core/include/ida/algilama_kaydi.h`; iskelet §3.1, §3.2, §4, §5 |
| Kapsam | Çağrı çerçevesi, emniyet daraltması, örnek kapısı ve IMU zamanlaması, olay altyapısı, parametreler ve komutları, zaman/seri aritmetik, matematik, günlük (W2). W3'te kestirim, güdüm (GUIDED_SP), görev/kip/arıza ve kontrol yazıldı ve birbirine bağlandı (W3-zincir, §9); atış kontrol ve sonar yer tutucudur |

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
| Emniyet gevşetmesi | `atis/atis_emniyet.c` (W5; dosya sahipliği gereği `emniyet/` yerine) | W5 atış kontrol | tam (§10) |
| Olaylar | `olay/olay.{c,h}`: kimlik, SAFETY_CLOSE, bekleyenler, sonuçlar, yalnız silen çıkış kapısı | W2 | tam; genişletici aday üretimi W4'ün kendi dosyasında |
| Parametreler | `param/`: `param.{c,h}`, `param_komut.c`, `ozet.{c,h}` (SHA-256), `crc32.c` (CRC-32, tablolu; W4-param-hiz), `param_listesi.inc` | W2 | tam |
| Zaman | `zaman/zaman.h` (yalnız `static inline`) | W2 | tam |
| Matematik | `mat/`: `mat.{c,h}`, `ida_math.{c,h}` | W2 | tam (çekirdek küme) |
| Günlük | `telemetri/`: `telemetri.h`, `gunluk.c` | W2 | tam |
| Kestirim | `kestirim/`: `kestirim.{c,h}`, `kes_eskf.c`, `kes_ufuk.c`, `kes_yatay.c`, `kes_ic.h` | W3 kestirim; W5 kestirim-yatay | dilim 1 (yönelim, derinlik, dikey hız) ve dilim 2 (yatay hız, konum, irtifa, düzey histerezisi; §11) |
| Güdüm | `gudum/`: `gudum.{c,h}` (GUIDED_SP, kontrol isteğinin tek erişimcisi), `gudum_oto.c` (AUTO: ILOS, derinlik referansı, en az irtifa) | W3-zincir; W5-gorev-yurutucu | GUIDED_SP (3 s kuralı dahil) ve AUTO güdümü (§12); tarama deseni yok |
| Görev, kip, arıza | `gorev/`: `gorev.{c,h}`, `kip.c`, `ariza.c`, `istek.c`, `gorev_yukle.c`, `gorev_yurut.c`, `gorev_ic.h` | W3 güdüm ve görev; W5-gorev-yurutucu | kipler, ARM, arıza güvenliği, kontrol isteği; görev yükleme, durum makinesi ve öğe yürütücüleri (§12) |
| Kontrol, tahsis | `kontrol/`: `kontrol.{c,h}`, `tahsis.{c,h}` | W3 kontrol | tam (dilim 1); `MANUAL` komutu yok |
| Atış kontrol | `atis/` | W5-atis-emniyet | emniyet zinciri (§10); hedefleme, algılama, takip kapsam dışı |
| Sonar, arka plan işi | `sonar/` | W4 sonar | yer tutucu |

Durum, sağlık ve fc alanlarının yazımı `cerceve/cikti.c`'dedir (çekirdek tarafı W2'de tam; M10c). `src/iskelet.{c,h}` W2-depo'nun firmware yer tutucusu (`firmware/yer_tutucu/main.c`) için korunmuştur.

Yetki alanları (`armed`, `mode`) `ida_emn_t`'dedir. Emniyet modülü bunlara yalnız kısıtlayan yönde yazar; genişletme yönünde yalnız görev/kip modülü (W3) yazar. Görev kancaları çekirdek işaretçisi alır: `ida_gor_adim(ida_gor_t *g, ida_emn_t *yetki, ida_core_t *c)` (c'den kestirim, IMU zamanlaması ve parametreler okunur, yalnız günlük yazılır) ve `ida_gor_komut(ida_core_t *c, const ida_komut_baglam_t *b, const ida_cmd_t *cmd, ida_cmd_result_t *r)` (yazılanlar: görev durumu, `armed`/`mode` genişletme yönü, günlük; FAILSAFE_RESET mandalları temizlediğinde kestirimin sayısal çökme mandalı da kaldırılır, `ida_kes_ariza_temizle`). Gevşetme W4'ün `ida_fc_emniyet_ornegi` kancasından, `cagri_kisit` alanlarını gevşetmeden yapılır (M9).

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
| `sizeof(ida_core_t)` | W2: 2160 B. W3-zincir sonrası (2026-11-21): **12 568 B** (Cortex-M33 ve macOS arm64 aynı); hizalama 8. Büyük parçalar: kestirim 5960 B, kontrol 1592 B, görev 272 B, güdüm 40 B. W4-param-hiz: 12 584 → **12 600 B** (macOS arm64; `ida_param_t.sema_ozeti` ve tümleyeni, 16 B) | Derleyicinin çıktısındaki sabit (GCC 14.3.1 `-O2` `.rodata`, Apple Clang `__const`) |
| Çekirdek arşivi, fw-release | W2: text 19 820 B. W3-zincir sonrası: **text 59 985 B**, data 0, bss 0. W4-param-hiz: 61 241 → **62 525 B** (+1 284 B; CRC tablosu 1 024 B `.rodata`), data 0, bss 0 | `arm-none-eabi-size -t libida_core.a` |
| Çekirdek arşivi, fw-debug | W2: text 21 166 B. W3-zincir sonrası: **text 61 775 B**, data 0, bss 0 | aynı |
| En büyük yığın çerçevesi (fw-release) | W2: 344 B. W3-zincir sonrası: **720 B** (`ida_kon_hesapla` ve `ida_kes_saglam`); sonra `ida_prm_blok_coz` 680 B, `commit_karari` ve `kayit_isle_d` 672 B. Tek çerçevedir, çağrı zincirinin toplamı değildir | `-fstack-usage` (.su) |

`ida_core_info`: port 1.0, `core_size` = `sizeof`, `core_align` = `_Alignof`, `param_schema_hash` her çağrıda tablodan hesaplanır (örneksiz işlevdir; önbelleği yok), `core_version` = `0x000100` (0.1.0). Çekirdek örneği özeti `ida_core_init`'te bir kez hesaplayıp `ida_param_t.sema_ozeti`'de tutar (`ida_prm_kur`; §5).

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

**Şema özeti ve CRC'nin maliyeti (W4-param-hiz, 2026-11-24).** Tablo derlemede sabit olduğundan şema özeti `ida_core_init`'te, örnek sıfırlandıktan sonra ve blok çözülmeden önce bir kez hesaplanır (`ida_prm_kur` → `ida_param_t.sema_ozeti` ve tümleyeni `sema_ozeti_tumleyen` = `UINT64_MAX ^ sema_ozeti`); init'in blok denetimi (2) ve PARAM_SAVE'in kodlaması (`ida_prm_blok_kodla(k, sema_ozeti, blok)`) bu değeri kullanır. Özet `ida_prm_sema_ozeti_tablo(tablo, sayi)` ile verilen tanım dizisinden türer; `ida_prm_sema_ozeti()` onu derlenmiş tabloyla çağırır. Kodlama da tablo parametrelidir (`ida_prm_blok_kodla_tablo`); derlenmiş tabloda I32 kayıt olmadığından I32 kodlaması bir test düzeneğiyle sınanır. CRC-32/ISO-HDLC bayt başına 256 girdilik `static const` tabloyla hesaplanır (`param/crc32.c`; ROM'da 1 024 B); çokterim, başlangıç ve son XOR değişmedi.

Önbelleğin bütünlüğü (A4-K89; sol W4-param-hiz bulgu 1 ve sol-2 bulguları 1–3): önbellek tümleyeniyle tutarsızsa (`ida_prm_tutarli`) çerçevenin çağrı başındaki bütünlük denetimi 2'yi (tamamlayıcı kopya) verir. PARAM_SAVE'in kararı (`save_karari`) özeti ve tümleyenini `ida_prm_ozet_al` ile birer kez (volatile) okur, doğrular ve bloğu yalnız bu doğrulanmış yerel değerle kodlar; önbellek yeniden okunmaz. Tutarsızlıkta blok üretilmez, `gen` tüketilmez ve `ida_prm_cagri_sonu` 2 döner; çerçeve bunu çağrı başındaki denetimle aynı yoldan (`butunluk_bildir`) aynı çağrının bütünlük sonucuna taşır: `health.core_ok` = 0 (mandallı) ve `CORE_INTEGRITY` (arg0 2) o bozulma için bir kez. Komut sonucu: A4 bütünlük hatalı çağrıdaki komut sonucunu tanımlamaz; `ACCEPTED` blob ister (K82), `REJECTED` 3.17'den bir neden ister (K120, K124) ve sonuç yazmamak K43'e aykırıdır; `REJECTED` / `PARAM_INVALID` seçildi (sözleşme boşluğu; mimar küçük sürüm notu olarak kaydeder). Böylece bozuk özet kalıcı kayda yazılmaz; yeniden init özeti tablodan yeniden kurar ve önceki geçerli blok okunur. Denetimin çağrı başına maliyeti iki 64 bitlik okuma ve bir karşılaştırmadır; host ölçümünde komutsuz adımda ayırt edilemedi (tahmin: birkaç çevrim).

Doğrulama (`cekirdek.birim.param_hiz`, 14 test): değişiklikten önceki bit bit CRC, özet gövdesi ve kodlayıcı testte çekirdek işlevleri çağrılmadan referans olarak tutulur; bayt eşitliği varsayılanlarda, tablodaki her tipin (U32, F32) en az/en çok değerinde, yedi bit deseninde, 1 000 rastgele durumda (sabit tohum), PARAM_SET + COMMIT + SAVE'in çekirdek yolunda ve I32 içeren sekiz kayıtlı düzenekte (uç değerler ve 1 000 rastgele durum) sınanır. Güncel şemanın özeti altın değere sabitlenir: `0x34065414A77F8846`, kanonik metin 2 426 B, varsayılan blok 1 296 B (W5-atis-emniyet'te üç parametreyle `0x18BE798234EDD38D`, 2 477 B, 1 320 B; §10.4) (eski çekirdeğin ikilisiyle ve sol incelemesinin bağımsız hesabıyla aynı; şema bilerek değişirse güncellenir, testin iletisi söyler; parametre sayısı ayrıca sabitlenmez, W3-K6). CRC bilinen vektörlerde, 256 tek baytta, 8 KB'ta ve 10 000 rastgele girdide referansla eşlenir; bozulma testi özette ve tümleyende tek bit bozar, SAVE'in blok üretmediğini, `core_ok` = 0 ve `CORE_INTEGRITY` (2) bildirildiğini, yeniden init'te eski geçerli bloğun okunduğunu gösterir; beyaz kutu testi tutarlı ama tablonunkinden farklı bir çiftin başlığa aynen yazıldığını (doğrulanan yerel değer) ve birim testi çağrı sonundaki SAVE kararının bozulmayı 2 ile döndürdüğünü, blok üretmediğini ve `gen` tüketmediğini gösterir. Denetimle kullanım arasına bozma enjekte eden kanca çekirdekte yoktur (M9); tek okuma yapısal olarak sağlanır.

Host ölçümü (ölçüm, bağıl; Apple M3 Pro, Apple Clang 17, çekirdek `-O3`; önce ve sonra aynı oturumda arka arkaya üçer koşu; 159 parametre, 1 296 B blok):

| Ölçülen | Tekrar (koşu başına) | Önce: ortanca / en kötü | Sonra: ortanca / en kötü |
|---|---|---|---|
| `ida_prm_blok_kodla` | 20 000 | 20,8 µs / 26,7–29,2 µs | 3,0 µs / 8,3–10,2 µs |
| `ida_core_step`, yalnız PARAM_SAVE | 10 000 | 22,2–22,7 µs / 28,8–44,8 µs | 4,7–4,8 µs / 10,7–12,8 µs |
| `ida_core_step`, SET + COMMIT + SAVE | 10 000 | 24,5 µs / 34,3–41,4 µs | 6,5 µs / 13,9–15,5 µs |
| `ida_core_step`, komutsuz | 10 000 | 1,75 µs / 7,1–20,1 µs | 1,71 µs / 4,0–12,4 µs |

En kötü değerler işletim sisteminin kesintilerini de içerir; host ortancaları kart hedefinin kanıtı değildir. Kart ölçümü ayrı yapılır (W4-K16).

Kayıt durumu (A4-K83; sol bulgu 2): `plat.param_gen`, `param_store`'un bildirdiği son tamamlanan kayıt denemesinin kuşağıdır (OK'te kalıcı bloğun, FAIL'de başarısız denemenin). Bekleyen PARAM_SAVE'in tamamlanması `param_store` ∈ {OK, FAIL} ve `param_gen` = beklenen `gen` eşleşmesiyle, durum kodu değişmeden de (aradaki BUSY görülmeden) saptanır; art arda iki FAIL ayrı ayrı saptanır. PARAM_SAVE günlüğü durum geçişinde ya da bu tamamlanmada bir kez üretilir; başka bir kuşağın OK'i ya da FAIL'i beklemeyi bitirmez; FAIL'de `param_ok` değişmez ve `gen` tabanı yükselmiş kalır (A4-K82). `cekirdek.birim.param` `test_k83_ayni_durum_koduyla_tamamlanma`.

`core/test/dis_basvurular.cmake`'in izinli listesindeki `bzero`: Apple Clang `-O2` sıfır ilklendirmeyi `bzero` çağrısına çevirir (host-release); firmware arşivinde geçmez.

Parametre komutları (A4-K81, K82, 3.17): PARAM_SET ve PARAM_GET komut anında yanıtlanır (GET çağrı başındaki uygulanmış değeri okur). PARAM_COMMIT ve PARAM_SAVE'in kararı kabul edilen çağrının sonundadır ve girdideki sıradan bağımsızdır: commit o anda bekleyen bütün değerleri (aynı çağrının sonraki PARAM_SET'leri dahil) doğrular; aynı çağrıdaki bütün commit'ler aynı sonucu ve her biri bir PARAM_COMMIT günlüğü alır; bekleyen değer yokken ACCEPTED olan commit uygulanmış sayılır. PARAM_SAVE'in sırayla belirlenen koşulları (ONE_PER_CALL, ARMED, PARAM_BUSY) komut anında, PARAM_INVALID (aynı çağrının kabul edilen commit'i sayılır) ve `gen` doyması çağrı sonunda değerlendirilir; kabulde commit sonrası küme kodlanır. `armed` koşulları komutun `cmds[]` sırasında işlendiği andaki `armed`'la (commit'lerden biri silahlıyken işlendiyse commit `ARMED`) ve girdideki her `ARM` (yinelenen ya da NARROWED alan dahil) sayılarak denetlenir. `gen` = max(çekirdeğin son `gen`'i (yoksa 0), `plat.param_gen`) + 1; max(…) = 0xFFFFFFFF ise SAVE `REJECTED` / `OUT_OF_RANGE` alır ve blok üretilmez (ret nedeni A4 3.17 PARAM_SAVE satırından ve A4-K82'den). Çağrı sonunda karara bağlanan sonuçların reddi `cnt.cmd_rejected`'e orada sayılır.

## 6. Yer tutucu çekirdeğin görünen davranışı

> W2 kaydıdır. W3'te görev, güdüm, kestirim ve kontrol yazıldığı için ARM, SET_MODE(GUIDED/AUTO), HEARTBEAT, FAILSAFE_RESET ve GUIDED_SP satırları artık modüllerinindir (§9); `MANUAL` satırı bu tablodaki gibidir. FC satırları W5'te atış kontrol modülünün (§10), `MISSION_*` satırları görev modülünündür (§12).

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

> W2 kaydıdır; W3'te sınıfı değişen satırlar §9'dadır (ör. K50 artık güdümde uygulanır).

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

## 9. W3 bütünleştirmesi (W3-zincir, 2026-11-21)

**Hedef iptal mandalı** (sol W3-zincir bulguları 1, 2 ve 2.–3. tur bulguları; EMNİYET). `ida_emn_t.hedef_iptal` tüketici başına bir mandal bitidir: `IDA_EMN_IPTAL_GUD` (güdümün GUIDED_SP hedefi), `IDA_EMN_IPTAL_GOR` (görevin tutma hedefleri), `IDA_EMN_IPTAL_KON` (kontrolün türetilmiş belleği: derinlik ve yön referansı, öteleme integralleri). `ida_emn_kusak_artir` her silahsızlandırma girdisinde (DISARM, SESSION_END; (b) adımında, reddedilen ve `out` NULL çağrılar dahil), her kip daralmasında ve değişiminde ((b)'de NARROW_MODE, görevin kip düşmesi ve kip değişimi), görevin kendi silahsızlandırmasında ve SESSION_START'ta ((b3)) üç biti de kurar; init'te üçü kuruludur. Kontrol biti ayrıca görevin tutma hedefini her yeniden yakalayışında (kip, arıza düzeyi değişimi, ARM, FAILSAFE_RESET), derinleşme yasağı başladığında ve güdümün saklı hedefi silindiğinde kurulur. Bit kuruluyken o tüketicinin saklı durumu kullanılmaz; tüketici bitini yalnız saklı durumunu sildikten (güdüm), yeniden yakaladıktan (görev) ya da belleğini sıfırladıktan (kontrol: `ida_kon_adim`, `ida_kon_hesapla`'dan sonra; etkin değilse `dongu_sifirla`, etkinse `referans_yenile`) sonra temizler. Aynı çağrıda daraltmadan sonra işlenen GUIDED_SP geçerli yeni hedeftir. Kuşak sayacı (`kusak`, tümleyeniyle) yalnız tanı içindir, sarabilir ve hiçbir karar ona dayanmaz; iç tutarlılık denetimi mandalda tanımsız bit ve tümleyeni denetler. Kontrol, bit kuruluyken derinlik ve yön referanslarını ölçülen durumdan yeniden kurar, öteleme integrallerini sıfırlar, açısal hız integrallerini korur (HOLD geçişinde moment değişimi itki değişim hızı sınırının içinde; `zincir.z1` HOLD testi). `dalis_yasak` boyunca derinlik referansı ölçülen derinlikten derin olamaz, aşağı yönlü dikey hız isteği üretilmez ve dikey integral aşağı yönde birikmez. Değişmez (sol 3. tur bulgu 2): bütün kanallar (derinlik, dikey hız, doğrudan kuvvet X/Y/Z, tutum, yatay hız) birleşip gövde ekseni kırpmalarından geçtikten sonra NED aşağı kuvvet ≤ max(`BUOY_FF`, 0) (aracı yerinde tutan, net aşağı ivme üretmeyen yüzerlik dengelemesi). Tahsis girdisinde önce gövde Z yukarı çekilir (LIM_FZ içinde), yetmezse X ve Y aynı oranda küçültülür; tahsisin gerçekleştirdiği kuvvet (τ = B f; doyma, öncelik ve değişim hızı penceresi yüzünden istenenden sapabilir) yine aşıyorsa bütün itici kuvvetleri aynı oranda, yuvarlama payıyla sınırın biraz altına (× 0,999) küçültülür ve gerçekleşen kuvvet yeniden doğrulanır; dört denemede sağlanmazsa itki sıfırlanır. Tolerans yoktur: değişmez binary32 hesabında tam sağlanır (sol 4. tur). Değişim hızı sınırlayıcısının belleği (önceki f[]) yasak başlarken bilinçli olarak korunur (yasaksız eksenlerde yumuşak geçiş); aşağı yönü bu küçültme sınırlar ve o adımda pencere aşılabilir (emniyet önceliklidir). Rastgele taramada ikinci adım gereklidir: yalnız girdi korumasıyla değişmez değişim hızı penceresinde bozulur. Testler: `cekirdek.birim.gudum_gecis` (gerçek çekirdek, itki isteği düzeyinde: reddedilen çağrıda DISARM, SESSION_END ve daraltıcı kip; kuşağın tam turu; daraltmadan sonra gelen GUIDED_SP; HOLD'a giriş, hedef iptali, derinleşme yasağı), `cekirdek.birim.kontrol_dalis_yasak` (sol'un hareketli derinlik karşı örneği, eğik duruşta hız ve doğrudan kuvvet karşı örnekleri, değişim sınırlayıcısı etkinken yasak başlangıcı, sabit tohumlu rastgele duruş, istek ve geçiş taramaları; tahsisin gerçekleştirdiği kuvvete kadar, toleranssız), `zincir.z1` HOLD geçişi (fizikte yön momenti ve aşağı kuvvet).

**Veri yokken ARM reddi** (mimar kararı W4-K13; EMNİYET). Arıza tablosunda `SIZINTI_BAYAT` (sızıntı algılayıcısından hiç örnek yok ya da bayat) ve `BATT_BAYAT` (batarya ölçerinden hiç örnek yok ya da bayat) satırları arm reddi gerektirir (`gorev/ariza.c` `OZELLIK`): ARM `REJECTED` / `NOT_ARMABLE` alır. Neden bitleri (`IDA_FSR_LEAK`, `IDA_FSR_BATT_LOW`) ve eylem düzeyleri (WARN; sızıntı bayatsa derinleşme yasağı) değişmedi; koşullar mandallı değildir: silahlıyken bayatlama silahsızlandırmaz, ilk taze örnekle ret kendiliğinden kalkar. "Veri yok" ile gerçek sızıntı ya da düşük bataryanın aynı neden bitini paylaşması A4'ün küçük sürüm notudur. Testler: `cekirdek.birim.gorev_arm` (`test_w4k13_*`).

**Sayısal çökme ve FAILSAFE_RESET.** Kestirim, yayımlanan düzeyden (`ida_kes_duzey`; çökme mandalı kuruluyken hep FAULT) ayrı olarak süzgecin mandaldan bağımsız düzeyini tutar (`ida_kes_suzgec_duzey`: hizalanmamışsa INIT, hizalanmışsa OK, DEGRADED ya da BAD). Görev modülünün "kestirim sağlıklı" sayacı (EST_KOTU ve SAYISAL mandallarının temizleme koşulu, EST_OK_MS) yayımlanan düzey FAULT iken bu süzgeç düzeyini sayar: yeniden hizalanmış süzgeç EST_OK_MS boyunca kesintisiz sağlıklıysa FAILSAFE_RESET kabul edilir ve kestirimin mandalı kaldırılır; aksi hâlde NOT_ARMABLE (EMNİYET). Önceden süzgeç mandal yüzünden hep FAULT göründüğü için araç init'e kadar arıza eyleminde kalıyordu. Test: `cekirdek.birim.gorev_cokme`.

**Bağlama.** Görev modülünün geçici tanımları kaldırıldı: kontrol isteği `ida_kon_istek_t` (`kontrol/kontrol.h`), bileşen geçerliliği `IDA_KES_G_*` ve `ida_kes_gecerlilik` (W3-K5). Kontrolün `istek_al`'ı güdümün erişimcisini (`ida_gud_kon_istegi`), itici maskesi `ida_gor_itici_disi`'ni okur. Kabul edilen ve mandalları gerçekten temizleyen FAILSAFE_RESET kestirimin sayısal çökme mandalını da kaldırır (`ida_kes_ariza_temizle`; etkisiz kabulde çağrılmaz, EMNİYET).

**Çağrı içi akış** (değişmedi, §2 (e)): kestirim → `ida_gor_adim` (kısıtlayan her şey) → komutlar (`ida_gor_komut`, `ida_gud_komut`, …) → `ida_gud_adim` (hedefin silinme kuralları) → step OK'ta `ida_kon_adim` → `ida_kon_girdi_topla` → `ida_gud_kon_istegi` → `ida_gor_istek` + GUIDED_SP kanalları.

**GUIDED_SP** (`gudum/`; A4-K50, 3.17, 3.4; W3-K7):

| Konu | Uygulama |
|---|---|
| Ret sırası | `mask` = 0, bit 0x80, maskeli alanın 3.4 aralığı → `OUT_OF_RANGE`; sonra kip `GUIDED` değil → `BAD_MODE`. Kip komutun işlendiği andaki kiptir (aynı çağrıda önce gelen SET_MODE(GUIDED) görülür): kanca görev kancaları gibi çekirdek işaretçisi alır (dağıtıcıda `komut.c` tek satır, mimar izniyle), `ida_gud_komut(ida_core_t *c, …)`; dağıtıcının bağlamındaki `mode` yalnız komutlardan önceki kiptir |
| `yaw` aralığı | −π < yaw ≤ π (double'da) ⇔ float için −3,14159250f ≤ yaw ≤ 3,14159250f (0x40490FDA); float π (0x40490FDB) aralık dışı |
| Saklama | Maskesiz alan o kanalın saklı hedefini değiştirmez; kanal içi öncelik DEPTH > ALTITUDE > VEL_D, YAW > YAW_RATE, POS_NE > VEL_NE |
| Uygulama | Yalnız `GUIDED`, silahlı, arıza eylemi < HOLD; maskeli kanal görev modülünün tutma isteğinin üzerine yazılır, maskesiz kanal görev modülünün tutma hedefinde kalır |
| Geçerlilik yok (W3-K7) | Kanal sönümlü, sıfır hedefli: önce hız 0 (dikey hız, açısal hız, yatay hız geçerliyse), değilse kuvvet/moment 0; bileşen geçerli olunca hedefe döner. İrtifa kestirimi bu sürümde yok: ALTITUDE hep sönümlü |
| Silinme | Kip GUIDED değil, arıza eylemi HOLD ve üstü, `armed` 1 → 0, SESSION_START. EMNİYET: silinen hedefe kendiliğinden dönülmez |
| Parametre | `GUD_POS_KP` (0x0301, F32, A, 0–5, 0,3 1/s; TAHMİN): konum hatasından yatay hız isteği |

**Kazanç ayarı** (`kontrol/kontrol_param.def`): W_KP 60 → 120, W_KI 30 → 15, W_DAMP_FF 40 → 80, ATT_KP_Y 1,5 → 3, RATE_KP_R 4 → 8; **bu fizik modeline göre** (sim/fizik dilim 1, `scenarios/ida0.toml`) Z1 koşusunda ayarlandı, gerçek araçta ölçülmedi. `test_sol_bulgulari.c`'nin iki testi ilk varsayılanlarla yazılmış beklenen değerleri koruyarak o kazançları kendi kurulumunda sabitler.

**Kapalı çevrim düzeneği** `tests/zincir/` (C++20): fizik + sade sensörler + gerçek çekirdek + platform taklidi; CTest `zincir.z1`, araç `ida_zincir_deneme` (CSV). Senaryo: dalış 1,5 m / +90°, 40 s tutma; manevra 2,5 m / −90° ve aynı anda itici arızası (itki × 0,4; şekil polinomunun tersiyle darbe genişliği ölçeklenir), 40 s tutma; DISARM. Test dört koşuyu karşılaştırır: arızasız, dikey t5, yatay t1, ikisi. Parametre bloğu SIL yoluyla verilir (A4-K78): geçici bir örnek PARAM_SET/COMMIT/SAVE ile bloğu üretir; senaryonun yapılandırması `YER_G` = 9,82 (havuz, A5 Ek E) ve `BARO_KAL_D` = yüzer denge derinliğidir.

## 10. Atış kontrol emniyet zinciri (W5-atis-emniyet, 2026-11-24)

Görev belgesi `docs/gorevler/W5-atis-emniyet.md`; gereksinimler mimari Bölüm 03 sürüm 0.3'ün "Ek: uygulama gereksinimleri" çekirdek maddeleri G-Ç01–G-Ç53, mimar kararları W5-K1…K11 (`docs/gorevler/W5-ortak.md`). Her maddenin kodda uygulandığı yer `atis/atis.h` başındaki izlenebilirlik tablosunda, testi `core/test/atis/`'tedir (adlar `test_gcNN_*`).

### 10.1 Kapsam sınırı

Yalnız emniyet zinciri: silahlandırma, süreli atış izni, acil durdurma ve sanal emniyet girdilerinin geri okuması, durum makinesi, öncelik ve sonuç kodları, olayların üretimi ve sonuçları, güvenli duruma dönüş, kilitli arıza. Hedefleme çözümü, önleme hesabı, atış ölçütü, hedef algılama, takip ve torpido başarımı yoktur. Çekirdek kendiliğinden atış kararı üretmez: `FIRE` yalnız kabul edilmiş bir operatör `FC_FIRE`'ının taşınmasıdır; araya ölçüt ya da zamanlama iyileştirmesi konmaz. Algılama girdisi okunmaz (`ida_fc_algilama` boştur); kamera akışından başlık kabul edilmediği için A4-K118'e göre akış init'ten, oturum sonu çağrısından ya da `SESSION_START`'tan `T_det_stale` (2 s) sonra bayattır (W5-K21; taban `det_taban_ns`, bit her çağrının (b) adımında hesaplanır). A4-K118'in `IDA_LOG_DET_STREAM` geçiş günlüğü yazılmaz (W5-K21 yalnız bitin koşulunu ister; açık nokta). `TRACKING` ve `SOLUTION_VALID` iç durumları yoktur.

### 10.2 İç durumlar ve dış eşleme

İç durum `ida_fc_t.ic` özel bit desenleriyle ve tümleyeniyle (`ic_tumleyen`) tutulur; silahlı tüp (`tup`) de tümleyenlidir (03 §2.6; A4-K89). `durum` alanı A4 §3.16 değerinin yansımasıdır: her daraltmada ve her çıktı yazımında kurulur. **Dış durum (A4 §3.16; düzeltme turu, sol bulgu 4):** iç durum `FAULT` → `FAULT_LATCHED`; değilse sonucu beklenen bir `FIRE` varsa → `FIRE_PENDING`; değilse en az bir tüpün izni çekirdeğin saatinde açık sayılıyorsa → `PERMIT_OPEN`; değilse `SAFE` → `SAFE`, `TEST` → `TEST`, öteki iç durumlar → `ARMED`. Bu, 03 §2.6.1 md. 1'in iç duruma dayalı eşlemesinden farklıdır (sözleşme önceliklidir): `FIRING`'de `FIRE`'ın sonucu alınmışsa `FIRE_PENDING` değil; `ASSESS`'te izin hâlâ açıksa `PERMIT_OPEN`.

| İç durum | Dış değer | Giriş | Çıkış |
|---|---|---|---|
| `SAFE` | `SAFE` | init, güvenli duruma dönüş, kabul edilen `FC_RESET`, `FC_TEST_EXIT` | `FC_ARM` → `ARMED`; `FC_TEST_ENTER` → `TEST` |
| `ARMED` | `ARMED` | `SAFE`'te kabul edilen `FC_ARM(tüp)` (ARMING ayrı durum değildir: §3.17'nin `FC_ARM` satırı ön denetimdir) | `FC_FIRE(aynı tüp)` → `FIRE_AUTH` |
| `FIRE_AUTH` | yukarıdaki kural (izin açık sayılıyorsa `PERMIT_OPEN`) | `ARMED`'de kabul edilen `FC_FIRE` | `FIRE` çıktı → `FIRING`; `T_pencere` → `SAFE`; `FC_ABORT`, kip koşulu → `ARMED` |
| `FIRING` | sonuç beklenirken `FIRE_PENDING`; sonra yukarıdaki kural | `FIRE` çıktı | `APPLIED`/`DUPLICATE` ve tüp boşaldı → `ASSESS`; öteki sonuçlar W5-K2 |
| `ASSESS` | yukarıdaki kural (izin açıksa `PERMIT_OPEN`, değilse `ARMED`) | tüp boşaldı | `T_assess` → `SAFE` (+ `SAFETY_CLOSE(tüp)`) |
| `TEST` | `TEST` | `SAFE`'te kabul edilen `FC_TEST_ENTER` | `FC_TEST_EXIT` ya da evrensel geçiş → `SAFE` |
| `FAULT` | `FAULT_LATCHED` | W5-K1, K2, K4, K11 nedenleri; iç durum bozulması | yalnız kabul edilen `FC_RESET` |

**Güvenli duruma dönüş** (`ida_fc_guvenli`; W5-K8): silahlı tüpe (ve çekirdeğin açık saydığı izne) `SAFETY_CLOSE`, silahlı tüp ve atış isteği silinir, durum `SAFE`; `FAULT` korunur. Nedenler: `FC_DISARM`, araç silahsızlandırması ya da `SESSION_END` ((b)), acil durdurmanın ya da silahlı tüpün fiş/piminin kısıtlayana dönmesi ((b)), emniyet örneğinin bayatlığı ((b); W5-K4), `armed` = 0, arıza düzeyi `HOLD` ve üstü, kestirim `OK`/`DEGRADED` dışında, IMU bayat, oturum yok, olay kimliği kullanılamaz (W5-K11 b), yer istasyonu kalp atışı `T_hb_fc` boyunca yok (`ida_fc_evrensel`; komutlardan önce ve sonra iki kez: "aynı adımda").

**Kip izin kümesi (okuma).** A4 §3.16'nın `MODE` biti "kip atış izni kümesinde değil" der; küme tanımlı değildir. 02 §3'ün `mode_ok`'u "AUTO'da TARGET_ALIGN ya da GUIDED"dir; görev öğeleri kapalı listesinde TARGET_ALIGN yoktur (A1 Tablo 3.6a). Bu yüzden küme yalnız `GUIDED`'dır (`IDA_FC_IZIN_KIPI`). `NARROW_MODE` (geçerli ya da geçersiz hedef) kip koşulunu düşürür (A4-K46); kip izin kümesine yeniden girildiğinde kalkar.

**FC_RESET (W5-K1).** Kabul edilen `FC_RESET` durumu `SAFE` yapar; `ida_fc_adim` çağrı sonunda bu çağrıda ortaya çıkan bir kilit nedeni ya da süren bir neden (iç durum bozulması: init'e kadar; G-Ç53'ün iki çelişkisi sürdükçe: kapatmadan sonra `PERMIT_OPEN` = 1 ve tekleme; `T_tup_bos` zaman aşımı, `t_meas > t_apply` bir örnek `LOADED` = 0 gösterene dek; W5-K11 a'nın doğrulanamamış kapatması) varsa aynı çağrıda yeniden kilitler; çağrı sonundaki durum değişmediği için `FC_STATE` günlüğü üretilmez (A4-K90).

### 10.3 Çerçevedeki kanca sırası

Çerçevenin yalnız çağrılma noktaları değişti (`cerceve/cerceve.c`, `cerceve/cikti.c`): init'te `ida_fc_kur(fc, now_w)` (A4-K118 tabanı); (b) `ida_emn_daralt` → `ida_fc_daralt` (önce ham bütünlük denetimi ve onarım, sonra çağrı bayrakları, `DET_STALE`, bayatlık, daraltmaya tepki; FC'nin kapatma istekleri `emn.d.kapat`'a) → `ida_olay_kapat_iste`; (b3) `ida_fc_oturum_basi(fc, ortam)`; (e) kabul edilen her `SAFETY` örneği `ida_fc_emniyet_ornegi` (ortam bağlamıyla), olay sonuçları `ida_fc_olay_sonucu` (sonucun tamamıyla: `t_apply` gerekir) ve `ida_fc_olay_bilinmiyor`, görev adımından sonra komutlardan önce `ida_fc_cagri`, komutlardan sonra kabul edilen **her** çağrıda `ida_fc_adim` (genişletici aday yalnız `IDA_RC_OK` step'te); çıktı yazımında `ida_olay_yaz`'dan hemen sonra `ida_fc_cikti` (çıkan `SAFETY_OPEN`/`FIRE`'ın kaydı, dış durum yansıması). Kalp atışının izi görev modülününkidir (`gcs_son_ns`; A4-K52: yalnız kabul edilen `HEARTBEAT`).

### 10.4 Parametreler

| Kimlik | Ad | Tip, sınıf | Aralık | Varsayılan | Dayanak |
|---|---|---|---|---|---|
| 0x0601 | `T_emn_bayat` | U32 (ms), B | 150–2000 | 250 (**tahmin**; W5-K4: `T_emniyet` = 100 ms'nin 2,5 katı; kartta ölçülecek) | G-Ç29 |
| 0x0602 | `T_pencere` | U32 (ms), B | 100–2000 | 300 (**tahmin**; 03 §2.6) | G-Ç48 |
| 0x0603 | `T_hb_fc` | U32 (ms), B | 200–10000 | 2500 (**tahmin**; W5-K16: A1-K70'in 1 Hz kalp atışının 2,5 katı; kartta ölçülecek) | G-Ç25 |
| 0x0604 | `T_assess` | U32 (ms), B | 200–5000 | 1000 (**tahmin**; 03 §2.6) | G-Ç48 |
| 0x0605 | `T_tup_bos` | U32 (ms), B | 100–2000 | 500 (**tahmin**; 03 §2.6) | G-Ç53 |

`T_assess` ve `T_tup_bos` düzeltme turunda parametre oldu (W5-K13; G-Ç28). Önce `butunleme.sil_oturum_dusme_belirlenim` düzeltildi: ihlal noktaları sabit ACTUATOR sırası yerine sim'in ilk çalıştırma kipli `SESSION_ACCEPT`'inden (hazırlık oturumunun bitişi) sayılır; şema 164 parametredir. `FC_APPLY_BY_NS` ve `FC_HOLD_NS` §5'tedir; hepsi B sınıfıdır (W5-K6). Aralık alt ve üst sınırları uygulayıcının seçimidir.

### 10.5 Okuma kararları (mimarın toplu onayına)

1. Kip izin kümesi yalnız `GUIDED` (10.2).
2. `ARMING` iç durumu yoktur; `FC_ARM` aynı çağrıda `ARMED`'e geçer. 03 §2.6'nın ön denetimindeki rejim, derinlik ve yörünge tablosu özeti kapsam dışıdır (atış çözümüne ait) ve A4 §3.17'de ret nedeni yoktur.
3. `FC_DISARM`/`FC_ABORT`: `SAFE` dışındaki her iç durumda (FAULT dahil; G-Ç19) ya da çekirdek bir izni açık sayarken `SAFETY_CLOSE(IDA_TUBE_ALL)`; `SAFE`'te olay üretilmez (var olan `cekirdek.birim.olay` testi `SAFE`'te `FC_ABORT`'un olay üretmemesini bekler). `FC_ABORT` `FIRE_AUTH`'ı `ARMED`'e alır, öteki durumları değiştirmez.
4. İzin açma ve yenileme: `FIRE_AUTH`'ta izin açık değilse `SAFETY_OPEN`; açıkken yenileme son `SAFETY_OPEN`'ın çıkışından `hold_ns / 4` ile `hold_ns / 2` arasında ve yalnız sonucu alınmışsa (G-Ç12). `FIRE` izin açık sayılırken ve son kabul edilen `inst = 0` örneği, izni veren sonucun `t_apply`'ından geç olmayan ölçüm zamanıyla `PERMIT_OPEN` = 1 gösterirken, tüpün engelleri temizse üretilir.
5. Genişletici adayın yasak maskesi `FC_FIRE` satırının bütün koşullarıdır (`SESSION`, `EVENTS_OFF`, `MODE`, `NOT_ARMED`, `EST`, `FAILSAFE`, `ESTOP`, `IMU_STALE`, `FAULT_LATCHED`, tüpün `PLUG`/`PIN`/`EMPTY` bitleri) ve `IMU_LAG`; kestirim `DEGRADED`'da `FIRE_AUTH` sürer ama olay yoktur, pencere dolunca `SAFE`.
6. Bayatlık iki yerde değerlendirilir (G-Ç29, W5-K4 eki, W5-K20; sol bulgu 1): (b)'de çağrı başındaki izlerle (reddedilen ve out NULL çağrıda da) ve kabul edilen her `inst = 0` örneği izleri güncelledikten sonra. İkisinde de bayatlık acil durdurmayı gevşekten kısıtlayana döndürürse dönüş o çağrının daraltıcı girdisidir (ESTOP, `SAFETY_CLOSE(ALL)`, `health.narrowed`, genişletici yasağı, FC güvenli) ve aynı çağrıda acil durdurma gevşemez (W5-K20 (1)). Bayatlık önceki bir çağrıda saptanmışsa iki ölçütü de tazeleyen gevşek örnek kabul edildiği çağrıda gevşetir (W5-K20 (3)). (ii) ölçütünün zamanı, ölçüm zamanının son ilerlediği örneğin alım zamanıdır; `SESSION_START` ölçüm zamanı tabanını siler. Geri giden `t_rx` (A4-K33 monotonluk istemez) izleri bayatlatabilir.
7. G-Ç53 izleri: kapatma izi yalnız o kapatmadan sonra çıkmış bir `SAFETY_OPEN` yoksa kurulur (kimlik karşılaştırması); sonuç örnekten sonra gelirse son kabul edilen örnekle de değerlendirilir. İki çelişki de geri okuma çelişkisi sürdükçe süren nedendir (W5-K1, G-Ç20; sol bulgu 2): tekleme (`tekleme`) sonraki her `t_meas > t_apply` örnekle yenilenir ve `LOADED` = 0 görülünce kalkar; `T_tup_bos` zaman aşımı da tekleme izini kurar (fırlatıcının durumu doğrulanamadı). Oturum sonu tekleme izini silmez; ölçüm zamanı oturumlar arasında sürekli sayılmadığı için sonraki her örnek değerlendirilir.
11. Bağlantı olayı taşıyan çağrıda gevşetme yoktur (W5-K20 (2); G-Ç18, G-Ç45): `SESSION_END` çağrısında kabul edilen `inst = 0` örneği hiçbir alanı gevşetmez, tüp/izin değerlerini ve izleri kurmaz (A4-K128: oturum sonundan sonra kabul edilmiş sayılmaz); `SESSION_START` çağrısında örnek kabul edilir ve izleri kurar ama hiçbir alanı gevşetmez.
12. Bütünlük (sol bulgu 3): FC'nin ham bütünlük denetimi (b)'de çağrı bayrakları (`reset_kabul`, `bu_cagri_kilit` vb.) sıfırlanmadan önce yapılır; bozulma onarım, `FAULT_LATCHED` ve `SAFETY_CLOSE(ALL)`'a taşınır.
8. W5-K11 (a): son `SESSION_END`'den önce çıkmış bir `SAFETY_CLOSE`'un sonucu bilinmezse yine `FAULT_LATCHED` olunur ama kapatma yinelenmez ve süren neden sayılmaz (alıcı oturum sonunda bütün izinleri düşürür: A1-K23, A4-K71). Var olan `kabul.cekirdek.A4-K113.daraltma` testi (A4-K129) oturum sonu kapatmalarını yanıtsız bırakıp `SESSION_START` çağrısında olay beklemez.
9. 03 §4'ün "beklenmeyen tüp boşalması → FAULT_LATCHED" satırı uygulanmadı: Ek'te karşılığı yoktur ve G-Ç53 çelişkiyi iki durumla sınırlar; boşalan tüpe `FC_FIRE` `EMPTY` engeliyle reddedilir, `FIRE_AUTH`'ta olay üretilmez.
10. İç durum bozulması (desen, tümleyen, bayrak baytı) `FAULT_LATCHED`'e götürür ve init'e kadar süren nedendir; yalnız dış durum yansıması (`durum`) bozuksa iç durumdan yeniden kurulur (`cekirdek.birim.cerceve` `test_k89_ustune_yazilan_bozulma` bu yolu bekler).

### 10.6 Testler

`cekirdek.birim.atis_port` (port yolu, 22 test), `cekirdek.birim.atis_durum` (düzenek yolu, 22 test), `cekirdek.birim.atis_ozellik` (yedi emniyet değişmezi; iki dizi, her biri 200 000 adım, tohum `0x5EED2026`). Düzeltme turunda özellik testi bağımsız bir modelle güçlendirildi: (a) bayatlık testin kendi örnek izlerinden hesaplanır (geri giden `t_rx` dahil) ve bayatken `ESTOP` biti beklenir; (c) süren neden testin izlediği geri okuma, bekleyen olay kümesi (8 yuva) ve olay sonucu durumundan hesaplanır (uygulamanın bayrakları okunmaz); (e) daraltıcının etkisi (genişletici yok, `narrowed`, `ARMED`/`PERMIT_OPEN`/`TEST` kalmaz, `SAFETY_CLOSE(ALL)`) denetlenir. Düzenek çerçevenin ön ve tam bütünlük denetimini, `core_ok`'u, `narrow_from_rejected`'ı, parametre commit'ini, `FC_STATE`/`FC_INHIBIT` günlüklerini ve `now_w` izini de çalıştırır. Bu sürümde kestirim düzeyi port üzerinden `OK` olamaz (`kestirim/kestirim.c`: yatay hız ve konum geçerliliği sonraki dilimdedir); A4 §3.17'nin `FC_ARM` satırı gereği atış kontrol port yolunda silahlanamaz. Silahlı yollar bu yüzden düzenekle (`core/test/atis/atis_duzenek.c`) sınanır: gerçek `ida_core_t` ve gerçek modül işlevleri çerçevenin sırasıyla çağrılır, yalnız araç silahlandırması, kip, kestirim düzeyi, arıza düzeyi, IMU durumu ve kalp atışı testin koşuludur. Var olan testlerde değişiklikler: `test_param_hiz.c`'nin altın şema özeti (bağımsız SHA-256 hesabıyla; düzeltme turunda 164 parametre: `0xE0ED504FB6AEA22D`, metin 2510 B, blok 1336 B), `test_emniyet.c`'nin W2 yer tutucusunu sınayan "gevşetmez" satırı (artık gevşetir) ve düzeltme turunda aynı testin ilk çağrıda `DET_STALE` beklentisi (W5-K21: 2 s dolmadan kurulmaz), `tests/butunleme/test_sil_belirlenim.cpp`'nin ihlal noktası tabanı (W5-K13).

### 10.7 Boyut

`sizeof(ida_core_t)`: 12 600 → 12 936 B (W5 ilk sürüm) → **12 984 B** (düzeltme turu; macOS arm64 ve Cortex-M33 aynı; `ida_fc_t` 280 → 296 B). Kod boyu §3 tablosunun yöntemiyle W5 raporundadır.

## 11. Kestirim dilim 2: yatay hız, konum, irtifa ve düzey (W5-kestirim-yatay, 2026-11-24)

Görev belgesi `docs/gorevler/W5-kestirim-yatay.md`. Bu bölümdeki bütün eşik, süre, sayı ve gürültü değerleri **tahmindir** (parametre varsayılanı ya da derleme sabiti); kartta ya da gerçek duyargayla ölçülmedi. Süre rakamları yalnız host ölçümüdür ve bağıldır.

**Yöntem.** Durum vektörü büyümedi: 15 hata durumlu süzgecin (`kes_eskf.c`) yatay hız ve konum durumlarına iki ölçüm eklendi; sabit gecikmeli ufuk ve çıkış öngörücüsü aynen kullanılır (02 §2.3.2). Ölçüm modelleri ve Jacobian'ları `kes_yatay.c`'dedir ve üç işlev olarak ayrıdır (`ida_kes_dvl_model`, `ida_kes_gnss_hiz_model`, `ida_kes_gnss_konum_model`); birim testi Jacobian'ı sayısal türevle karşılaştırır.

| Ölçüm | Model (R: gövde → NED; kollar O_b'den, gövde çerçevesinde) | Gürültü | Kapı |
|---|---|---|---|
| DVL hızı, gövdede üç eksen | `h = Rᵀ v + ω × l_dvl`; `H_v = Rᵀ`, `H_θ = [Rᵀ v]×`, `H_bg = [l_dvl]×`; ölçüm DVL çerçevesinden gövdeye `DVL_ROLL/PITCH/YAW` ile döner | `R_dvl C R_dvlᵀ + R_DVL² I` (sensör kovaryansı varsa ve pozitif tanımlıysa; yoksa yalnız `R_DVL² I`); beyazlatılır | 3 boyut, `KAPI_G3` |
| GNSS yatay hızı | `h = (v + R (ω × l_a))_NE`; `H_θ = −(R [ω × l_a]×)_NE`, `H_bg = (R [l_a]×)_NE` | `R_GNSS_V² I` | 2 boyut, `KAPI_G2` |
| GNSS yatay konumu | `h = (p + R l_a)_NE`; `H_θ = −(R [l_a]×)_NE` | kapıda `σ_g² I`, güncellemede `(GNSS_SIG_K σ_g)² I`; `σ_g = max(eph, R_GNSS_P)` | 2 boyut, `KAPI_G2` |

- **Kullanım koşulları.** DVL: başlık bayrağı temiz, `VEL_VALID`, `TRACK_BOTTOM`, `fom ≤ DVL_FOM_MAX`, irtifa biliniyor ve `≥ DVL_ALT_MIN`, sensör kovaryansı bildirilmişse en büyük köşegeni `≤ DVL_FOM_MAX²` (çok büyük kovaryanslı örnek bilgi taşımaz; beyazlatma NIS'ini sıfıra indirdiği için kapıdan geçip desteği yenilerdi). GNSS: başlık bayrağı temiz, `fix_type` 3B ve üstü, `eph ≤ GNSS_EPH_MAX`, basınç destekli derinlik `< KES_YUZEY_D` (dalışta GNSS örneği kullanılmaz), konum orijinden en çok 0,2° uzakta.
- **Yerel düzlem.** Enlem ve boylam farkı `int64`'te alınır, boylam ±180°'ye sarılır; kuzey ve doğu ikinci mertebeye kadar hesaplanır (`ida_kes_gnss_ne`). Orijin `ORIJIN_LAT`, `ORIJIN_LON` parametreleridir (I32, 1e-7 derece; varsayılan `scenarios/ornek-havuz.toml` orijini) ve `nav.origin_*` alanlarına yazılır. Tam ECEF dönüşümüne göre hata testte ölçüldü: 5 km içinde 7 mm, 15 km içinde 41 mm.
- **Nitelendirme.** Bir kaynak ancak `KES_N_IYI` ardışık iyi örnekten sonra füzyonlanır. Nitelikli kaynakta iyi olmayan tek örnek yalnız atlanır; iyi örnekler arasındaki boşluk bayatlık sınırını aşarsa nitelik düşer. Sayım ölçüm zamanında, ufukta, zaman sırasıyla yapılır.
- **Ret, kuşku ve geri dönüş yolu** (düzeltme turu; inceleme `docs/gorevler/inceleme/W5-kestirim-yatay-inceleme.md`, bulgu 1, 2, 4). İlke: `IDA_EST_OK` silahlanmanın ön koşuludur; **hiçbir sıfırlama `OK`'u korumaz ve destek üretmez.**
  - Kapıdan geçemeyen ölçüm füzyonlanmaz ve desteği yenilemez; destek yalnız kapıdan geçen örnekten gelir.
  - Ret izi (duvar saati; `KES_RED_MS` kuralı) ilk rette başlar ve yalnız kabul edilen örnekte ya da sıfırlamada temizlenir; kaynağın yeniden nitelenmesi onu silmez.
  - `KES_N_RED` (3) ardışık ret kaynağı **kuşkulu** yapar: ilgili geçerlilik süre dolmadan, örnek sayısıyla düşer. Kuşkuyu yalnız kapıdan geçen örnek kaldırır; sıfırlama kaldırmaz.
  - `KES_N_RESET` (10; grup koşulu `KES_N_RED < KES_N_RESET`) ardışık rette durum ölçüme sıfırlanır: hız DVL'e ya da GNSS hızına, konum GNSS'e. Hız sıfırlamasında yatay konum σ'sı `|Δv| × ret süresi` kadar büyütülür. Sıfırlama OK adayını siler (`KES_OK_MS` baştan), desteği yenilemez ve kaynağı kuşkulu bırakır: geçerlilik ancak sonraki, kapıdan geçen örnekle döner.
  - Mutlak başvuru yokken (taze ve kuşkusuz GNSS konum desteği yok; ör. dalışta) yapılan hız sıfırlaması yatay konumu da geçersiz kılar: ret dizisi boyunca konum yanlış hızla tümlenmiştir. Konum yeniden ancak GNSS'ten kurulunca geçerli olur.
  - Destek zamanı ölçümün değil füzyon anının duvar saatidir (dilim 1 ile aynı kalıp): ölçüm zamanına göre bayatlık sınırı `T_*_STALE`'den en çok kuyruk gecikmesi (ufuk gecikmesi D ≤ 200 ms; bütçe ertelerse birkaç çağrı daha) ve taşıma süresi kadar uzar. Davranış bilerek değiştirilmedi (bulgu 6).
- **GNSS'in ilişkili hatası.** GNSS konum hatası zamanda ilişkilidir (sim modelinde varyansın %90'ı τ = 60 s). Kazanç `GNSS_SIG_K` katı büyütülmüş gürültüyle kurulur ve güncellemeden sonra yatay konum varyansı `σ_g²`'nin altına indirilmez: bildirilen σ alıcının bildirdiği doğruluktan iyi olmaz.
- **Ölü hesap.** Yatay hız ölçümle destekliyken ufuktaki yatay konum σ'sı her yayılımda `DR_ORAN × |v_yatay| × Δt` kadar büyütülür (yön hatası ve DVL ölçek hatası yolla orantılı hata verir; beyaz süreç gürültüsü bunu vermez). Hız desteksizken uygulanmaz; o durumda P ivme gürültüsü ve sapma belirsizliğiyle büyür.
- **Konumun kurulması ve yüzeye çıkış.** Hizalamadaki yatay konum (0, 0) bir kabuldür: nitelikli ilk fix konumu kapısız kurar (`reset_counter` artar). Kurulum bir sıfırlamadır: destek üretmez; konum, kurulumdan sonraki fix kapıdan geçince geçerli olur. Aynı yol konum "bilinmiyor"a düştüğünde de işler: yeniden hizalama, oturum yenilenmesi, başvurusuz hız sıfırlaması ve orijin değişimi. **Orijin değişimi:** `ORIJIN_LAT` ya da `ORIJIN_LON` değişirse aynı çağrıda konum geçersiz olur, OK adayı silinir ve `nav.origin_*` yeni değeri bildirir; durum ötelenmez, konum yeni orijine göre GNSS'ten yeniden kurulur. Sonraki fix'ler kapıdan geçerse olağan güncellemedir; tek güncellemenin yatay düzeltmesi `KONUM_SICRA`'yı aşarsa çıkışa ve geçmişe aynı çağrıda sıçrama olarak uygulanır ve `reset_counter` artar, aşmazsa τ_o ile yumuşar (çağrı başına en çok `KONUM_SICRA × T/τ_o`). Kapıdan geçemeyen fix'ler `KES_N_RESET` örnekten sonra konumu sıfırlar (tek sıçrama, sayaçla).
- **Seyreltme ve kuyruklar.** Her türün kuyruğa alınmasında en kısa ölçüm zamanı aralığı vardır (`kes_ic.h`: manyetometre 8 ms, basınç 18 ms, DVL 45 ms, GNSS 90 ms); olağan cihaz hızları geçer, daha sık gelen örnek akışı canlı sayar ama füzyonlanmaz. DVL ve GNSS ayrı kuyruktadır (8 kayıt); manyetometre ve basınç kuyruğu 40 kayıttır (dilim 1'de 24; düzeltme turunda 32'den 40'a: D = 200 ms ve en kısa aralıklarda 25 + 12 = 37 bekleyen ölçüm; testte çağrı sonunda 36 ölçüldü, 32'de 105 ölçüm taşıyordu). Gerekçe: her çağrıda örnek veren bir akışta kuyruk sürekli dolu kalıyor, boşalan yeri hep sıralamada önce gelen tür alıyor ve öteki türler füzyonlanamıyordu (testte görüldü: derinlik geçerliliği ≈ 4,9 s'de düşüyordu).
- **İrtifa.** `nav.altitude` O_b'nin dibe mesafesidir: DVL irtifası (dibe dik mesafe; A4 §3.3) ve Ping1D menzili (ışın gövde +z ekseninde sayılır, düşeye izdüşürülür; güven ≥ %50 ve eğim kosinüsü ≥ 0,8) montaj kolunun düşey bileşeniyle O_b'ye taşınır; ikisi de varsa küçüğü verilir; kaynak yok ya da `T_DVL_STALE`'den eskiyse −1. Filtreye girmez. Son örnekten beri derinlik değişimi eklenmez.

**Geçerlilik ve düzey** (`kestirim.c` `suzgec_duzeyi`; `IDA_EST_OK` atış kontrolün silahlanma ön koşuludur, A4 §3.16).

| Büyüklük | Koşul |
|---|---|
| Kaynak "ret altında" | kuşkulu (`KES_N_RED` ardışık ret ya da kaynağa sıfırlama; yalnız kapıdan geçen örnek kaldırır) ya da kapı reddi `KES_RED_MS`'tir sürüyor (yalnız kabul edilen örnek ya da sıfırlama temizler) |
| Kaynak "etkin" | akışı canlı ve son kapı kararı (kabul ya da ret) bayatlık sınırı içinde: canlı ve nitelikli |
| Yatay hız (`IDA_KES_G_YATAY_HIZ`) | tutum ve yön geçerli; **bildirilen** (`nav.v_sigma`; çıkış zamanı için muhafazakâr) yatay hız σ'sı ≤ `SIG_HVEL`; bir kaynak destekliyor (DVL ya da GNSS hızı: akış canlı, son kabul edilen füzyon `T_DVL_STALE` / `T_GNSS_STALE` içinde, ret altında değil); **ve etkin hiçbir hız kaynağı ret altında değil** (iki nitelikli kaynak çelişirken birinin kabul edilmesi yetmez). Akışı kesilen kaynak bayatlık süresinden sonra etkin sayılmaz |
| Konum (`IDA_KES_G_KONUM`) | konum GNSS'ten kuruldu ve o günden beri bilinir sayılıyor (hizalama, oturum yenilenmesi, orijin değişimi ve başvurusuz hız sıfırlaması düşürür); yatay konum σ'sı ≤ `SIG_HPOS`; konumu taşıyan destek var (geçerli yatay hız ya da canlı ve taze GNSS konumu); GNSS konum kapısı ret altında değil (akış kesilse de sürer: dalarken ret altındaysa dalış boyunca geçersizdir) |
| `OK`'a yükselme | tutum, yön, derinlik, yatay hız ve konum geçerli, çıkış gecikmesi yok ve bu durum `KES_OK_MS` boyunca (duvar saati) kesintisiz sürdü. Hız ya da konum sıfırlaması ve orijin değişimi süreyi baştan başlatır; sıfırlamalar `KES_OK_MS`'ten sık yineleniyorsa (çelişen kaynaklar) düzey `DEGRADED` kalır |
| Hız sıfırlamasından sonra (üçüncü tur; dar denetim `docs/gorevler/inceleme/W5-kestirim-yatay-inceleme-2.md`, bulgu 1) | Hız sıfırlaması, hangi kaynağa yapılırsa yapılsın, iki hız kaynağını (DVL, GNSS hızı) birlikte kuşkulu yapar; her kaynağın kuşkusunu yalnız kendi kapıdan geçen örneği kaldırır. Sıfırlamadan sonra yatay hız geçerliliği için etkin her hız kaynağının kapıdan geçen en az bir örnek vermesi gerekir; sıfırlanan kaynağın kendi örneğinin geçmesi (durum ona eşitlendi) bağımsız destek değildir. Çelişen iki kaynakta öteki kaynak yeni duruma karşı reddetmeyi sürdürdüğünden geçerlilik hiç kurulmaz ve düzey `DEGRADED` kalır: `KES_OK_MS`, `KES_N_RED` ve `KES_RED_MS` değerinden bağımsız (testte `KES_OK_MS` = 100/200/300/1000 ms ve `KES_N_RED` = 15, `KES_N_RESET` = 20, `KES_RED_MS` = 5000; önce sapma sürerken 1239/819/399/0 ve 1590 `OK` çağrısı ile her durumda ≥ 1687 yatay hız geçerli çağrı, sonra hepsi 0). Tek etkin kaynakta davranış değişmez; etkin olmayan kaynağın kuşkusu geçerliliği düşürmez ve kaynak etkin olunca ilk kabul edilen örneğiyle kalkar. Konumun tek kaynağı GNSS'tir: ilke konum sıfırlamasına uygulanmaz. Bilinen sınır (9) bu satırla kapanır |
| `OK`'tan düşme | koşullardan biri bozulduğu çağrıda `DEGRADED`; süre baştan sayılır. Kaynak kesilirse geçerlilik son kabul edilen füzyondan `T_DVL_STALE` / `T_GNSS_STALE` sonra düşer; kaynak reddedilirse `KES_N_RED`'inci rette (5 Hz'de ≈ 0,4 s + D) düşer; ölü hesapta σ `SIG_HPOS`'u aşınca düşer |
| `BAD` | değişmedi (bayraklı IMU, tutum σ'sı, yerçekimi ya da basınç kapısının sürekli reddi). DVL ya da GNSS kapısının sürekli reddi `BAD` değil, ilgili geçerliliğin düşmesidir |

Oturum yenilenmesinde ve yeniden hizalamada yatay konum korunmaz; yeniden GNSS'ten kurulana kadar geçersizdir.

**Parametreler** (`kestirim_param.def`, 0x0234–0x0252; hepsi B sınıfı; varsayılanlar tahmin): montajlar `GNSS_L_X/Y/Z`, `DVL_L_X/Y/Z`, `DVL_ROLL/PITCH/YAW`, `PING_L_X/Y/Z`; orijin `ORIJIN_LAT`, `ORIJIN_LON` (I32); DVL `R_DVL` (0,02 m/s), `DVL_FOM_MAX` (0,1 m/s), `DVL_ALT_MIN` (0,1 m); GNSS `R_GNSS_P` (0,5 m), `GNSS_SIG_K` (4), `R_GNSS_V` (0,1 m/s), `GNSS_EPH_MAX` (5 m), `KAPI_G2` (11,83), `KES_YUZEY_D` (0,2 m); ölü hesap ve düzey `DR_ORAN` (0,02), `SIG_HVEL` (0,2 m/s), `KONUM_SICRA` (2 m), `KES_N_IYI` (5), `KES_N_RESET` (10; en az 3), `KES_OK_MS` (2000), `T_DVL_STALE` (1000), `T_GNSS_STALE` (1000); düzeltme turunda `KES_N_RED` (0x0253; 3, en az 2; grup koşulu `KES_N_RED < KES_N_RESET`, `ida_kes_param_grup`). Şema 159 → 190 → 191 parametre (kestirim payı; başka işlerin eklediği parametreler ayrıca sayılır); bu dalın altın özeti `0xA4B1E4F1ECAADCBB`, kanonik metin 2 933 B, varsayılan blok 1 552 B (`cekirdek.birim.param_hiz`; birleştirmede yeniden hesaplanır). Başka orijinli senaryo `ORIJIN_LAT` ve `ORIJIN_LON`'u kendi (kimlik, değer) çiftleriyle verir.

**Kaynak** (ölçüm; fw-release GCC 14.3.1 `-O2`, host Apple Clang `-O3`):

| Ölçüm | Önce | Sonra |
|---|---|---|
| Çekirdek arşivi, fw-release (`arm-none-eabi-size -t libida_core.a`) | text 62 525 B | text 73 701 B (+11 176 B: kestirim nesneleri +9 760 B, `param.c` +1 480 B, öteki nesneler −64 B); data 0, bss 0 |
| `ida_fw.elf`, fw-release | text 124 248 B, data 92 B, bss 168 608 B | text 135 460 B (+11 212 B), data 92 B, bss 168 608 B |
| `sizeof(ida_core_t)` (Cortex-M33 ve macOS arm64 aynı) | 12 600 B | 14 304 B (+1 704 B: kestirim 5 960 → 7 136 B, parametreler 2 768 → 3 296 B) |
| En büyük yığın çerçeveleri, fw-release (`-fstack-usage`; tek çerçeve) | `ida_kon_hesapla` 728 B, `ida_kes_saglam` 720 B, `commit_karari` ve `kayit_isle_d` 672 B, `ida_prm_blok_coz` 664 B | `commit_karari` 792 B ve `ida_prm_blok_coz` 784 B (parametre sayısıyla büyür), `ida_kes_dvl_fuzyon` 752 B (yeni), `ida_kon_hesapla` 728 B, `ida_kes_saglam` 720 B, `kayit_isle_d` 640 B, `ida_kes_gnss_fuzyon` 384 B |

Düzeltme turu (aynı ölçüm yöntemi; bu dalın birleştirmeden önceki hali): çekirdek arşivi text 73 701 → 74 765 B (+1 064 B: `kestirim.c` +832 B, `kes_yatay.c` +152 B, `param.c` +56 B, öteki +24 B); `sizeof(ida_core_t)` 14 304 → 14 544 B (+240 B: kestirim 7 136 → 7 360 B, 192 B'ı ölçüm kuyruğu; parametreler 3 296 → 3 312 B); `ida_kes_dvl_fuzyon` ve `ida_kes_gnss_fuzyon` yığın çerçeveleri değişmedi (752 B, 384 B).

Kart üzerindeki adım süresi bu işte ölçülmedi (ayrı iş). Host'taki bağıl step süresi raporda verilir.

**Doğrulama** (`cekirdek.birim.kestirim`; dilim 2'nin testleri `core/test/kestirim/test_kestirim_yatay.inc`, dilim 1'in düzeneğini kullanır; ilk teslimde 22 test, düzeltme turundan sonra 35). Yer gerçeği çözümsel yörüngelerdir; GNSS enlem ve boylamı tam ECEF dönüşümüyle, DVL hızı `Rᵀ v + ω × l`'den, çekirdeğin modellerinden bağımsız üretilir. Kabul 1–8'in testleri ve ölçülen değerler iş raporundadır. Ek sınamalar: yerel düzlem dönüşümü, Jacobian'ların sayısal türevi, büyük kollar ve dönük DVL montajı, irtifa, seyreltme, her çağrıda bütün duyargaların geldiği sade akış, tek kaynak (yalnız GNSS, yalnız DVL), GNSS kalitesi ve oturum yenilenmesi. Depoya girmeyen bir çapraz denetimde `sim/sensor` modellerinin (GNSS, DVL dahil) çıktısı çekirdeğe verildi (çember, dalış, yüzeye çıkış; altı tohum): yüzeyde yatay konum hatası ≤ 2,7 m, 140 m ölü hesapta ≤ 2,1 m, yatay hız hatası ≤ 0,043 m/s, düzey 4,15 s'de `OK` ve koşu boyunca `OK` (bu çapraz denetim düzeltme turundan önce yapıldı; sonra yinelenmedi).

**Düzeltme turunun doğrulaması** (inceleme `docs/gorevler/inceleme/W5-kestirim-yatay-inceleme.md`; `cekirdek.birim.kestirim` 41 → 54 test). İnceleyicinin üç dizisi birim testidir ve düzeltmeden önceki kodda düşer: (a) dalış, yalnız DVL 10 Hz ve 5 Hz, kalıcı +1 m/s sapma (önce: 10 Hz'de düzey hiç düşmüyor, `OK` iken hız hatası 1,0 m/s ve konum hatası 118 m; sonra: düzey 80,36 s / 80,56 s'de düşer ve `OK`'a dönmez); (b) yüzey, GNSS 5 Hz + DVL 10 Hz, DVL'de kalıcı sapma (önce: `OK` dışı adım 0, 43 hız sıfırlaması; sonra: düzey 40,36 s'de düşer ve dönmez; `KES_OK_MS` = 1000 iken de); (c) `OK` iken orijin değişimi (önce: 230 adım `OK`; sonra: aynı çağrıda konum geçersiz ve `DEGRADED`, 0,155 s'de yeniden kurulum, 2,355 s'de `OK`). Ayrıca: başvurusuz hız sıfırlamasından sonra konumun ancak yüzeyde GNSS'ten kurulması; ret altındaki kaynağın susması; yeniden nitelendirmede ret izinin korunması; büyük kovaryanslı DVL örneği; ölçüm kuyruğunun en kısa aralıklarda taşmaması; grup koşulu; sıfırlama çağrısının beyaz kutu sınaması (OK adayı, destek zamanları, kuşku, ret izi). Bütün yörünge testlerinde her çağrıda iki denetim koşar: düzey `OK` iken yatay hız hatası 3·`SIG_HVEL`'i ya da yatay konum hatası 3·`SIG_HPOS`'u aşamaz; yatay hız geçerliyken bildirilen σ `SIG_HVEL`'i aşamaz. Konum bileşeni yalnız yer gerçeği ile mutlak başvurunun bilerek ayrıldığı üç testte kapalıdır (kalıcı GNSS sıçraması, orijin değişimi, %10 DVL ölçek hatası; aşağıda (7)). Değişen ölçümler: `OK`'a ilk yükselme 4,155 → 4,355 s (kurulumdan sonraki fix beklenir); ardışık rette geçerliliğin düşmesi GNSS'te 41,155 → 40,555 s, DVL'de 80,965 → 80,560 s.

**Bilinen sınırlar.** (1) DVL fiziksel rejime bakılmadan kullanılır (02 §2.3.3 "yalnız DALIŞ" diyordu; görev belgesi yüzeyde GNSS + DVL ister). (2) Su izi (dip kilidi olmayan DVL hızı) kullanılmaz; akıntı durumu yoktur. (3) GNSS yüksekliği kullanılmaz. (4) Hızlanırken yerçekimi ölçümü ivmeyi eğim sanar; DVL yokken (yalnız GNSS hızı) ≈ 0,4 m/s² hızlanmada yatay hız hatası 0,17 m/s'ye çıktı (testte ölçüldü). (5) Araç sabit hızla giderken hizalanırsa hız 0 kabul edilir; DVL ya da GNSS hızı `KES_N_RESET` örnek reddedildikten sonra sıfırlamayla düzeltir (testte oturum yenilenmesinden `OK`'a 5,96 s; düzeltme turundan sonra 6,16 s). (6) Ping1D ışın ekseni gövde +z sayılır; montaj dönüşü parametresi yoktur. (7) **Ölü hesapta model dışı hız hatası ayırt edilemez (açık bulgu).** Dalışta tek hız kaynağı DVL'dir; hatası `DR_ORAN`'ı (yolun %2'si) aşarsa bildirilen konum σ'sı gerçeğin gerisinde kalır ve düzey `OK` iken konum hatası 3·`SIG_HPOS`'u aşabilir. Testte ölçüldü (%10 DVL ölçek hatası, 310 m ölü hesap): `OK` iken konum hatası 34,4 m, bildirilen σ 8,4 m, 8 697 çağrı (≈ 43 s) 30 m'nin üstünde. Hata yüzeyde GNSS'le görülür (ret, sıfırlama). (8) Tek mutlak başvurunun (GNSS) kalıcı hatası ayırt edilemez: `KES_N_RESET` retten sonra konum ona sıfırlanır ve sonraki fix'le yeniden geçerli olur (düzey bu arada `DEGRADED`'dır, `OK` sıfırlamadan sonraki kabulden `KES_OK_MS` sonra döner). Tek hız kaynağının kalıcı hatasında da hız ona sıfırlanır; mutlak başvuru yoksa konum geçersiz kalır. (9) [Üçüncü turda kapandı: düzey tablosunun "Hız sıfırlamasından sonra" satırı; aşağıdaki tümce önceki turun durumudur.] Çelişen iki hız kaynağında düzeyi tutan, kuşku kuralı ile `KES_OK_MS`'in sıfırlama çevriminden (varsayılanlarda 1–2 s) uzun olmasıdır; `KES_OK_MS` çok kısa seçilirse (ör. 100 ms) sıfırlama ile öteki kaynağın `KES_N_RED`'inci reddi arasında `OK` görülebilir (parametre aralığı bunu engellemiyor; sınanmadı). (10) Hızlanırken ufuk hız σ'sı iyimserdir (yalnız GNSS hızıyla 0,4 m/s² hızlanmada hata / ufuk σ'sı eksen başına 2,9, büyüklükte 3,4); geçerlilik bu yüzden bildirilen σ'ya bakar (aynı durumda hata / bildirilen σ = 1,2). Konum geçerliliği ufuk σ'sına bakmayı sürdürür.

## 12. Görev yürütücüsü ve AUTO güdümü (W5-gorev-yurutucu, 2026-11-24)

Görev belgesi `docs/gorevler/W5-gorev-yurutucu.md`. Bütün eşik, süre ve kazanç varsayılanları **tahmindir** (parametre); kartta ya da gerçek araçta ölçülmedi. Kapsam sınırı: görev hiçbir koşulda silahlandırma, kip genişletmesi, atış izni ya da `FC_*` komutu üretmez; hedefe yönelme, hedefleme ve tarama deseni yoktur.

**Yükleme (A4-K51, A1 Tablo 3.6a; `gorev_yukle.c`).** Depo iki sabit tampondur (yürürlükteki görev ve açık yükleme; her biri 128 × 32 B sıkıştırılmış öğe). `MISSION_BEGIN(count)` (1–128; değilse `OUT_OF_RANGE`; görev değiştirilemez durumdaysa `BAD_MODE`) yüklemeyi açar, yeni `BEGIN` öncekini bırakır. `MISSION_ITEM` sırayla (`index` = alınan sayı) ve Tablo 3.6a'nın bütün koşullarıyla (kapalı tür listesi, türün çerçevesi, kullanılmayan konum alanları 0, `depth` ve `p[i]` aralıkları, "0" yazan `p[i]` tam 0, tamsayı değerli alanlar, sonluluk; `ida_gor_oge_gecerli`) denetlenir; herhangi bir ret yüklemeyi kapatır. `MISSION_END` açık yükleme yoksa ya da sayı tutmuyorsa `OUT_OF_RANGE`; tutuyorsa tampon yürürlüğe girer, durum `LOADED`, etkin öğe 0. Görev yalnız `IDLE`, `LOADED`, `PAUSED`, `COMPLETE`, `ABORTED` durumunda ya da `HOLD` arıza eylemi sürerken değişir; görev `RUNNING`'e geçince açık yükleme kapanır, bu yüzden yürüyen görevin üstüne yükleme hiçbir sırayla yürürlüğe giremez (END "açık yükleme yok" alır).

**Durum makinesi (02 §2.6).**

| Geçiş | Koşul |
|---|---|
| `IDLE`/`COMPLETE`/`ABORTED`/`LOADED`/`PAUSED` → `LOADED` | kabul edilen `MISSION_END` |
| `LOADED` → `RUNNING` (öğe 0) | `SET_MODE(AUTO)` kabulü (ön koşul: yatay konum geçerli, görev `LOADED`/`PAUSED`/`RUNNING`) |
| `PAUSED` → `RUNNING` | `SET_MODE(AUTO)` ya da AUTO'da `MISSION_CTL RESUME`; kendiliğinden yalnız duraklamanın nedeni çekirdeğin kendi koşuluysa (yatay konum geçersizliği, `HOLD` arıza eylemi, ölü hesap beklemesi) ve o koşul kalktıysa. Silahsızlanma ve operatör duraklaması kendiliğinden çözülmez (`ARM` tek başına sürdürmez). Her geçişte açık yükleme kapanır |
| `RUNNING` → `PAUSED` | `MISSION_CTL PAUSE` (operatör); silahsızlanma (`DISARM`, `SESSION_END`, arıza eylemi; operatör duraklaması gibi); AUTO'dan çıkış; yatay konum geçersiz; arıza eylemi `HOLD`; ölü hesap yol sınırı |
| `RUNNING`/`PAUSED` → `RUNNING` (öğe 0) | AUTO'da `MISSION_CTL START` |
| `RUNNING` → `COMPLETE` | son öğenin bitiş ölçütü |
| `LOADED`/`RUNNING`/`PAUSED` → `ABORTED` | `MISSION_CTL ABORT` (operatör; yüzeye çıkış istenmez); öğe zaman aşımı, yürütülemez öğe, ölü hesap beklemesinin aşımı ya da `SURFACE` ve üstü arıza eylemi (iptal mandalı, aşağıda) |
| Yasak | `IDLE`'da her `MISSION_CTL`; `PAUSE` `RUNNING` dışında; `RESUME` `PAUSED` dışında ya da AUTO dışında; `START` AUTO dışında; `ABORT` `IDLE`/`COMPLETE`/`ABORTED`'da: hepsi `BAD_MODE`. `action` 1–4 dışı `OUT_OF_RANGE` |

AUTO kipi görev `RUNNING` ya da `PAUSED` iken sürer; görev başka durumdaysa (bitti, iptal, yeniden yüklendi) kip tablosunun `GOREV_YOK` olayıyla `DEPTH_HOLD`'a iner (W3 kip tablosu değişmedi; olayın koşulu "yürüyen ya da duraklamış görev yok" oldu). `status.mission_state` ve `mission_item` (etkin öğe; `COMPLETE`'te son öğe) durumdan yazılır; AUTO'da yürüyen ya da duraklamış görev varken ve arıza eylemi `HOLD`'dan hafifken `status.phase` güdümün aşamasıdır (yüzey hedefi ve yüzeyde `SURFACE_TRANSIT`, `DIVE_TO` `DIVE`, yüzey hedefi ve su altında `ASCENT`, öteki su altı `SUBMERGED`).

**Öğe yürütücüleri (`gorev_yurut.c`).** Öğe süresi (zaman aşımı) duvar saatinde ve yalnız `RUNNING`'de sayılır; davranış süreleri (DIVE_TO bandı, HOLD süresi) ölçüm zamanında (son zaman çerçevesi) ve yalnız `RUNNING` adımlarında (duraklamada birikmez; DIVE_TO bandı duraklamadan sonra yeniden başlar). `w` = `REF_DEP_RATE` (kontrol), `a` = `GUD_DER_IVME`, `KAT` = `ASIM_KAT`, `PAY` = `ASIM_PAY_S`, `D(x, v)` = `x/v + v/a` (ivme sınırlı referansın hızlanma ve yavaşlaması dahil dikey süre; üçgen profil `2√(x/a)` ≤ `D`); uzaklık ve `|Δd|` öğe başındaki konumdan.

| Öğe | Güdüm hedefi | Bitiş ölçütü | Zaman aşımı |
|---|---|---|---|
| `WAYPOINT` | bacak: önceki öğe yol noktasıysa onun hedefinden, değilse öğe başındaki konumdan hedefe ILOS, hız `p1`; derinlik `depth` (≤ `YUZEY_D` ise yüzey seyri) | yatay uzaklık ≤ `p0` ya da bacağın sonunun dikmesi geçildi | `KAT·(uzaklık/p1 + D(|Δd|, w)) + PAY` |
| `DIVE_TO` | öğe başındaki konum ve yönde tutma; derinlik `depth`, hız `min(p0, w)` | `|d − depth| ≤ p1`, `BANT_MS` kesintisiz | `KAT·D(|Δd|, min(p0, w)) + BANT_MS + PAY` |
| `SURFACE` | tutma; yüzeye, hız `min(p0, w)` | `d ≤ YUZEY_D` | `KAT·D(d, min(p0, w)) + PAY` |
| `HOLD` | hedef (NONE: öğe başındaki konum) ve `depth`'te tutma | `RUNNING` adımlarında yatay ve dikey `p1` içinde biriken süre ≥ `p0` ve araç o an `p1` içinde (dışarıda ve duraklamada birikim durur, sıfırlanmaz) | `p0 + KAT·(uzaklık/LIM_V + D(|Δd|, w)) + PAY` |
| `WAIT_GNSS` | tutma; yüzeye, `YUKSELME_HIZI` | yüzeyde, GNSS konumu taze (kestirimin mutlak başvuru koşulu) ve `p1` ≤ 3D | yükselme `KAT·D(d, v_asc) + PAY`; bekleme yüzeye çıkıştan `p0` |
| `SCAN_LINES`, `SCAN_POINT` | — | — | başlangıcında iptal (bu dilimde yürütülmez) |

`WAIT_GNSS`'te `p1` = DGPS (3) ya da RTK (4) doğrulanamaz: kestirim yalnız 3D ve üstünü füzyonlar ve fix türünü görev modülüne taşımaz; öğe `p0`'da zaman aşımına uğrar (okuma kararı; çerçeveye ya da kestirime GNSS fix türü aktarımı sonraki iştir). `GLOBAL` hedefler kestirimin yerel düzlem dönüşümüyle (`ida_kes_gnss_ne`, nav orijini) çözülür; çözülemezse iptal.

**Arıza önceliği.** Arıza eylemi her zaman görevin önündedir: `HOLD` görevi duraklatır (AUTO sürer, güdüm hedefi yok; arıza kalkınca kaldığı öğeden sürer), `SURFACE` ve üstü görevi iptal eder. AUTO sütununun GCS hücreleri (`GCS_1` eylem yok, `GCS_2` su altında eylem yok) yalnız görev `RUNNING` iken uygulanır; AUTO'da görev yürümezken (`PAUSED`: operatör, silahsızlanma, `HOLD`, ölü hesap) GCS satırları `DEPTH_HOLD` sütunuyla değerlendirilir (`GCS_1` `WARN`, `GCS_2` `SURFACE` ve görev iptali); `HOLD`'un görevi duraklattığı adımda düzey aynı adımda yeniden hesaplanır. İptal (zaman aşımı, yürütülemez öğe, ölü hesap beklemesinin aşımı, `SURFACE`+ arıza) yeni arıza koşulunu (`IDA_GOR_K_GOREV_IPTAL`, mandallı) aynı adımda kurar: bütün silahlı kip sütunlarında `SURFACE`, `DISARMED`'da `WARN`; nedeni iptalin nedenidir (zaman ve öğe: `GEOFENCE`; ölü hesap: `EST_DEGRADED`; arızanın yol açtığı: o arızanın nedenleri). Böylece görev bitip kip `DEPTH_HOLD`'a indiğinde de (ör. düşük bataryada `DEPTH_HOLD` sütunu yalnız `WARN`) yüzeye çıkış sürer; `FAILSAFE_RESET` kaldırır. Mandalın nedeni (`iptal_nedeni`) yalnız `FAILSAFE_RESET`'te temizlenir; yeni görevin yüklenmesi onu değiştirmez. 02 §4.3'ün AUTO sütunundaki "düşük batarya: görev iptali + SURFACE" bu yolla uygulanır.

**Konuma dayalı arıza koşulları (02 §4.3).** *Coğrafi sınır* (`IDA_GOR_K_COGRAFI`, mandallı, `GEOFENCE`): orijinden yatay uzaklık `GEO_YARICAP`'ı aşarsa ya da görevin başlangıcından (duvar; duraklamalar dahil) `GOREV_AZAMI_S` geçerse; sütunlar M/STB/DH/SK `WARN`, G/A `SURFACE`, D/INIT yok; sınır hâlâ aşılıyorsa `FAILSAFE_RESET` `NOT_ARMABLE`. *En az irtifa* (`IDA_GOR_K_IRTIFA`, otomatik, `ALT_MIN`): irtifa biliniyorsa (`nav.altitude` ≥ 0; INIT ve FAULT'ta nav sıfır olduğu için yok sayılır) ve `ALT_MIN`'in altındaysa silahlı kiplerde `WARN`; eylem güdümde (`ida_gud_irtifa_kisit`, isteğin en son adımı): her kipte derinleşme yasağı (`dalis_yasak`), DEPTH_HOLD/STATION_KEEP/GUIDED/AUTO'da derinlik hedefi irtifa açığı kadar yukarı çekilir ("yükselme"). `ALT_MIN` = 0 koşulu kapatır. Derinlik sınırı satırı bu işte yoktur.

**Ölü hesap yol sınırı (W5-K24'ün görev tarafı).** Görev modülü, GNSS konumu taze değilken (kestirimin `gnss_konum_taze` koşuluyla aynı: konum kurulu, kuşkusuz, füzyon desteği `T_GNSS_STALE` içinde) kat edilen yatay yolu toplar; taze GNSS sıfırlar. AUTO'da koşan görevde yol `DR_YOL_MAX`'ı aşarsa görev duraklar, araç yüzeye çıkar (`YUKSELME_HIZI`) ve duraklama noktasında konum tutup GNSS bekler (`WAIT_GNSS` davranışı); yüzeyde taze GNSS'le kaldığı öğeden sürer, `DR_BEKLE_S` aşılırsa iptal. Bekleme yalnız AUTO'da ve silahlıyken sürer. Bu, kestirimin "ölü hesapta `OK` için yol/süre sınırı" açık konusunu kapatmaz (kestirim değişmedi); görev tarafında konuma güveni sınırlar.

**AUTO güdümü (`gudum_oto.c`; 02 §2.4).** Yatay: ILOS `χ_d = α − atan((y + σ·y_int)/Δ)`, `ẏ_int = Δ·y/((y + σ·y_int)² + Δ²)` (Caharija vd. 2016; Fossen ve Pettersen 2014); istek NED hızıdır, `hız × max(0, cos(χ_d − ψ))` (araç rotaya dönmeden ilerlemez), yön çerçevesine çevrilip `IDA_KON_YATAY_HIZ` ile, yön `χ_d`'ye `IDA_KON_YON_ACI` ile verilir. Konum tutma `GUD_POS_KP·(hedef − konum)` (GUIDED_SP'nin yasası). ILOS integrali hedef kuşağı değişince (öğe, durum) sıfırlanır. Dikey: hız ve ivme sınırlı ikinci mertebe referans (`ida_gud_ref_adim`; `|w| ≤ dikey_hiz`, `|ẇ| ≤ GUD_DER_IVME`, hedefe `w² = 2a|e|` profiliyle); hız sınırı `REF_DEP_RATE`'i aşmadığı için kontrolün kendi referans sınırlayıcısı bunu gecikmesiz izler ve ileri beslemesi referansın hızıdır. `dikey_hiz` düşünce (daha yavaş öğe ya da duraklama hedefi) `|w|` yeni sınıra sıçramaz, ivme sınırıyla iner (geçişte en çok `(|w| − w_yeni)/a` süre sınırın üstünde). Derinleşme kırpması (`dalis_yasak`, en az irtifa) sürerken referans eski hedefe ilerlemez: `ref.d` isteğin kırpılmış değerine, `ref.w` sıfıra çekilir; kırpma kalkınca oradan sürer. Yüzey hedefinde araç yüzeydeyken ve referans `YUZEY_D`'ye varınca istek yüzey kipine geçer (dikey kapalı; 02 §2.4 `SURFACE_TRANSIT`). Güdüm yalnız AUTO'da, silahlıyken, arıza eylemi `HOLD`'dan hafifken, görev hedefi varken ve güdümün iptal biti kurulu değilken yazar; aksi hâlde görev modülünün AUTO tutması (W3) kalır. Referans ve integral ölçüm zamanıyla (son zaman çerçevesi, adım ≤ 0,1 s) ilerler; iptal bitinde (kip değişimi, silahsızlanma, oturum) bulunulan derinlikten yeniden kurulur. Kontrolcünün iç yapısı ve istek arayüzü değişmedi.

**GUIDED'da 3 s kuralı (02 §2.6).** Son kabul edilen GUIDED_SP'den (komutun `now_w`'si) `GUD_SP_ASIM_MS` geçince hız türü kanallar (`VEL_D`, `YAW_RATE`, `VEL_NE`) bulunulan derinliği, yönü ve konumu tutmaya çevrilir (bileşen geçersizse kanal görev modülünün tutmasına bırakılır); konum türü kanallar zaten son hedeftir. Kontrol referansı bulunulan durumdan yenilenir.

**Parametreler** (12; hepsi TAHMİN). Görev (B sınıfı): `GEO_YARICAP` 0x0411 (1000 m), `GOREV_AZAMI_S` 0x0412 (7200 s), `ALT_MIN` 0x0413 (0,3 m), `DR_YOL_MAX` 0x0414 (200 m), `DR_BEKLE_S` 0x0415 (300 s), `ASIM_KAT` 0x0416 (2), `ASIM_PAY_S` 0x0417 (60 s), `BANT_MS` 0x0418 (1000 ms). Güdüm (A sınıfı): `GUD_LOS_DELTA` 0x0302 (3 m), `GUD_ILOS_SIGMA` 0x0303 (0,2), `GUD_DER_IVME` 0x0304 (0,05 m/s²), `GUD_SP_ASIM_MS` 0x0305 (3000 ms). Şema özeti `0xCCBD046F048653C3`, kanonik metin 3 238 B, blok 1 688 B, 208 parametre (`.def` dosyalarından bağımsız hesap; aynı hesap önceki şemada `0x8F918D01B26C0791` / 3 017 B verir).

**Okuma kararları (mimarın toplu onayına).** (1) Sözleşme `SCAN_*` için ret tanımlamaz (A1 Tablo 3.6a'da geçerli türler): yüklenir, yürütmede görev iptal edilir ve yüzeye çıkış istenir. (2) A5 1.0'ın `gcs_komutu`'nda `MISSION_CTL` olmadığından AUTO'ya giriş yüklü görevi başlatır; `START` yalnız AUTO'da baştan başlatmadır. (3) AUTO, görev `PAUSED` iken de sürer (02 §2.6'nın "AUTO: görev yürütücüsü" satırı; duraklamada araç duraklama noktasında tutar); bu sırada AUTO'nun GCS istisnası uygulanmaz, GCS satırları `DEPTH_HOLD` sütunuyla değerlendirilir (düzeltme turu, mimar kararı). (4) `SURFACE` ve üstü arıza görevi iptal eder (02 §4.3 yalnız düşük batarya için "görev iptali" yazar; öteki `SURFACE` hücreleri yüzeye çıkışın kesinliği için aynı yoldan). (5) Silahsızlanan AUTO'da görev operatör duraklaması gibi durur; yeniden silahlanma tek başına sürdürmez, `RESUME` ya da `SET_MODE(AUTO)` gerekir (düzeltme turu, mimar kararı). Silahsızken verilen `RESUME` ya da `SET_MODE(AUTO)` kabul edilir ama sonraki adım görevi yeniden duraklatır (silahsızken AUTO'ya girip sonra silahlanan araç da `RESUME` bekler). (6) Görevin iptali `fs_reasons`'a yeni bit eklemez (`port.h` değişmez): zaman ve öğe iptali `GEOFENCE`'ı kullanır (02 §4.3 coğrafi sınır satırı "toplam görev süresi"ni içerir). (7) `WAIT_GNSS`'in DGPS/RTK isteği doğrulanamaz (yukarıda). (8) `core/test/test_komut.c`'nin yer tutucu satırları (MISSION_* `UNSUPPORTED`) yeni sözleşme satırlarına çevrildi; yeni birim testleri `core/test/CMakeLists.txt`'e üç satırla kaydedildi (görev belgesinin dosya listesi dışında, zorunlu en küçük ekleme).

**Düzeltme turu (bağımsız incelemenin sekiz bulgusu, mimar kararları).** (1) AUTO'nun GCS istisnası yalnız görev `RUNNING` iken (yukarıda, Arıza önceliği). (2) Silahsızlanma operatör duraklaması gibidir (okuma kararı 5); genel ilke: duraklamadan kendiliğinden çıkış yalnız çekirdeğin kendi koşulu kalkınca. `SESSION_START`'ta görevin yakaladığı yerel düzlem noktaları (öğe başındaki konum, önceki yol noktasının hedefi, duraklama noktası) ve öğenin başlangıç durumu sıfırlanır: etkin öğe aynı indisten yeniden başlar (öğe süresi ve HOLD birikimi sıfırdan), duraklama noktası ilk geçerli konumda yeniden yakalanır; görev durumu, indis ve toplam görev süresi korunur. (3) HOLD süresi yalnız `RUNNING`'de ve tolerans içinde birikir (öğe tablosu). (4) İptal mandalının nedeni yalnız `FAILSAFE_RESET`'te temizlenir. (5) Zaman aşımları `D(x, v)` ve DIVE_TO'da `BANT_MS` ile (öğe tablosu). (6) Kırpma sürerken derinlik referansı tutulur (AUTO güdümü). (7) Kendiliğinden `PAUSED` → `RUNNING` geçişi de açık yüklemeyi kapatır. (8) Kip × arıza matrisi testi eylemi koşulun oluştuğu adımda da denetler; yükleme testinin depo karşılaştırması modülün bütün durumunu kapsar. `sizeof(ida_core_t)` 23 568 → 23 576 B (macOS arm64; `hold_birikim_ns`, +8 B); parametre ve şema değişmedi.

**Testler.** `cekirdek.birim.gorev_yukle` (Tablo 3.6a sınır değerleri, atomik yükleme ve her geçersizlik türünde deponun değişmemesi, yürüyen görevde yükleme, durum makinesinin geçişleri ve yasak geçişleri), `cekirdek.birim.gorev_yurut` (öğe başına bitiş ölçütü ve zaman aşımı, arıza önceliği, konum kaybı, ölü hesap sınırı, coğrafi sınır, en az irtifa, basit kinematik modelle baştan sona görev), `cekirdek.birim.gudum_oto` (referans modelinin hız/ivme sınırı 24 rastgele durumda, sentetik araçla düz hat, köşe ve akıntı, konum tutma, AUTO isteğinin kanalları, irtifa kırpması, GUIDED 3 s kuralı); `butunleme.gorev_sil` ve `butunleme.ida_sim_gorev_senaryosu` (`scenarios/gorev-havuz.toml`). W3'ün kip × arıza matrisi testi AUTO sütununda iptalin kip düşmesini hesaba katacak biçimde ve üç yeni satırla genişletildi.

**Ölçümler (2026-11-24; host ölçümleri bağıldır, kart hedefinin kanıtı değildir).**

| Ölçülen | Önce (taban `7901f3f`) | Sonra | Kaynak |
|---|---|---|---|
| `sizeof(ida_core_t)` (Cortex-M33 ve macOS arm64 aynı) | 14 928 B | 23 568 B (+8 640; görev 272 → 8 672 B, güdüm 44 → 88 B) | derleyicinin sabiti (`arm-none-eabi-objdump` `.rodata`, Apple Clang) |
| Çekirdek arşivi, fw-release, text | 80 173 B | 90 801 B (+10 628) | `arm-none-eabi-size -t libida_core.a` |
| `ida_fw.elf`, fw-release, text / data / bss | 141 732 / 92 / 168 608 B | 152 216 / 92 / 168 608 B | `ida_fw.boyut.txt` (çekirdek 64 KB'lık sabit alanda, bss değişmez) |
| Yeni işlevlerin en büyük yığın çerçevesi (fw-release) | — | 88 B (`ida_gud_oto_adim`) | `-fstack-usage` |
| `ida_core_step`, yer tutucu girdiyle (200 000 adım × 7, ortanca) | 1 751–1 839 ns | 1 821–1 869 ns | Apple M3 Pro, `-O2`; önce/sonra sırayla üçer koşu, yük ≈ 1,0–1,3: fark koşular arası gürültü düzeyinde (yaklaşık %0–3) |
| Z1 kapalı çevrim (18 000 adım, fizik dahil), duvar | 0,160–0,165 s | 0,161–0,192 s | `ida_zincir_deneme`; gürültü düzeyinde |

Z1: `ida_zincir_deneme` CSV'si bayt bayt aynı. `ida_sim --senaryo scenarios/z1.toml` CSV'sinde fizik, kestirim, kip, arıza ve darbe genişliği sütunlarının hepsi aynı; yalnız `parti` (parti sıra numarası) her satırda 20 kayık: parametre bloğu 12 parametre büyüdüğü için hazırlık oturumunda (A1-K19) daha çok parti gönderiliyor.

`scenarios/gorev-havuz.toml` (SIL, süreç içi; `butunleme.gorev_sil`): görev 80 s'de `COMPLETE`, `PAUSED`/`ABORTED` yok, arıza eylemi yok. Yer gerçeğine göre yol noktası kaçırma (en yakın geçiş) 1,92 / 0,23 / 1,63 m, bitişteki uzaklık 2,01 / 0,99 / 1,63 m; kestirilen konuma göre üçü de 0,50 m (kabul yarıçapı). Yer gerçeğiyle kestirim arasındaki yatay fark aynı aralıkta 1,4–2,7 m: kaçırmanın asıl kaynağı havuzdaki GNSS'le kurulan konumun hatasıdır (kestirim; bu işin kapsamı dışında, W5-K24). Su altı bacağında derinlik hatası en çok 0,071 m, RMS 0,009 m; DIVE_TO bitişte 1,43 m (hedef 1,5, tolerans 0,2); SURFACE `YUZEY_D`'de (0,30 m) biter ve kip `DEPTH_HOLD`'a iner (W3: bulunulan derinliği tutar). Kestirime göre çapraz hata en çok 1,15 m (ilk bacağın başı; kestirilen konumun sıçramaları), su altı bacağında 0,11 m. Aynı tohumla iki koşu bayt bayt aynı.

## 13. Kontrol: acil durdurmada çekirdeğin aktüatör çıktısı (W5-kontrol-acil, 2026-11-24; EMNİYET)

Görev belgesi `docs/gorevler/W5-kontrol-acil.md`; karar W5-K26 (kart doğrulamasında ölçülen açık: kart acil durdurmayı gördüğü halde kendi isteğini nötrlemiyor, kontrolcü acil durdurma boyunca biriktiriyordu). Bu bölüm W3-K1 ve W3-K4'ün "acil durdurma itki kapısında yer almaz" maddesinin yerine geçer; sim sunucusundaki bağımsız nötrleme (A1-K85, iskelet §5) değişmez ve bununla birlikte çalışır.

**Etkin değerin okunması.** Tek kaynak `ida_emn_t.kisitli`'nin `IDA_EMN_ESTOP` bitidir (A4-K116): basılı örnek, bayatlık (W5-K4'ün iki ölçütü; `atis/atis_emniyet.c` aynı biti kurar), `SESSION_END` ve init (hiç gevşememiş değer) aynı bite düşer. `ida_kon_acil_kisitli` tanımsız bit taşıyan (bozuk) değeri de kısıtlayan sayar. Değer bir çağrı içinde üç yerde değişebilir: (b) `ida_emn_daralt` ve `ida_fc_daralt`, (e) `ida_fc_emniyet_ornegi`; üçü de `ida_kon_adim`'den öncedir, bu yüzden kontrol adımının okuduğu değer o çağrının son değeridir.

**Üç katman** (`kontrol/kontrol.{c,h}`; çerçevede iki satır):

| Katman | Yer | Ne yapar |
|---|---|---|
| Çıktı kapısı | `ida_kon_cikti` (`cerceve/cikti.c` sürekli komutu buradan alır; önce `ida_kon_itki`'den alıyordu) | Etkin değer kısıtlayansa ya da mandal kuruluysa sekiz komut tam `0.0f`; kontrolcünün öteki durumuna bakmaz |
| Kontrol adımı | `ida_kon_hesapla`, `ida_kon_girdi_t.acil` (`ida_kon_girdi_topla` doldurur) | Kısıtlayanken birikimli durumun tamamını sıfırlar (`acil_sifirla`: integraller, derinlik ve yön referansları, itki değişim hızı belleği `f[]`, doyma belleği, son çıktı), mandalı kurar, hesap yapmaz. Adım zamanı izi korunur |
| Her çağrının kancası | `ida_kon_acil_izle` (`cerceve/cerceve.c`, (b) ve (b3)'ten hemen sonra; reddedilen ve `out` NULL çağrıda da) | Kısıtlayanken aynı sıfırlamayı yapar ve mandalı kurar. Step OK olmayan çağrıda (tick, `IMU_REJECTED`, reddedilen çağrı, `out` NULL) başlayıp sonraki step'in (e) adımında gevşeyen dönemi kontrol adımı hiç görmez; kanca görür. (e)'de başlayan dönem aynı çağrıda gevşeyemez (W5-K20 (1)) ve sonraki çağrının kancasına düşer |

`is_safe` ve `thr_valid` değişmez (A4-K55'in koşulları kapalı listedir; acil durdurma o listede yoktur): step OK'ta `thr_valid = 1`, `is_safe` olağan değerinde, sekiz `thr_cmd` `0.0f`. Silah durumu, kip, saklı GUIDED_SP hedefi ve görevin tutma hedefleri bu kuralla değişmez. `port.h` değişmedi; yeni `status`/`health` alanı yok (durum `fc.inhibit_mask`'in `ESTOP` bitinden okunur).

**Kalkış rampası** (mimar kararı: rampa son çıktı düzeyinde, itici komutu üzerinde). Mandal kuruluyken gelen ilk kısıtsız step OK adımı kalkış adımıdır (k = 0): durum sıfırdan, referanslar ölçülen durumdan başlar; kontrolcü ve tahsis olağan biçimde koşar; yalnız son komut vektörü çarpan_k = min(1, (k + 1) · P / T) ile ölçeklenir (P adım aralığı, T = `KON_ACIL_RAMPA`). Uygulama zamanla sayar: çarpan = (tf.t − t0) / T, t0 kalkıştan önceki son adımın zamanı (aralık 50 ms'den uzunsa kalkış − 50 ms; önceki adım yoksa kalkış adımı, çarpan 0). Rampa içinde çarpan 1 − 2⁻²² ile küçültülür: bölme ve iki çarpmanın yuvarlamalarıyla birlikte de |çıktı| ≤ (Δt / T) · |rampasız| gerçek sayılarda sağlanır. Geçen süre ≥ T olduğu adımdan sonra çıktı ölçeklenmez (rampasız çıktıyla bit bit aynı). İç durum rampa boyunca ölçeksizdir: T = 0 ile koşan eş kontrolcüyle her adımda bit bit aynıdır. Rampa zamanla ilerler (silahsız geçen süre de sayılır); `SESSION_START` süren rampayı baştan başlatır (zaman tabanı yenilenir, A4-K72).

Sonuçları (bilinçli; mimar kararının doğrudan sonucu): (1) sarma önlemi olağan (ölçeksiz) tahsise göre çalışır; rampa boyunca gerçekleşen itki istenenden küçük olduğu için integraller en çok T süresince hata biriktirir. Üst sınır tahmini (varsayılanlarla, ölçülmedi): dikey `W_KI · LIM_W · T` = 15 · 0,5 · 0,5 ≈ 3,8 N (`LIM_FZ` 80 N), yön `RATE_KI_R · LIM_R · T` = 4 · 0,6 · 0,5 = 1,2 N·m (`LIM_MN` 10 N·m). (2) Ölçekli komut ESC'nin dur ölü bandına (|u| < `THR_U_MIN` = 0,065) düşebilir: o iticide itki, çarpan · |u| ölü bandı geçene kadar yoktur; komut değiştirilmez.

**Parametre.** `0x053B` `KON_ACIL_RAMPA`, U32 (ms), sınıf B, 0–5 000, varsayılan 500 (**tahmin**; ölçülmedi). 0: çarpan her adımda 1. Görev belgesindeki ad `KON_ACIL_RAMPA_MS` 17 karakterdir; `IDA_PARAM_NAME_MAX` = 16 (A4 §3.9, `port.h`) ve `ida_core_param_find` 16 karakterden uzun adı tablodan bağımsız olarak `NOT_FOUND` ile yanıtlar. Ad bu yüzden 14 karaktere indirildi, birim eki alan adında kaldı (`kon_acil_rampa_ms`); mimarın onayına. Şema: 197 parametre, özet `0x9B82228B17A10955`, kanonik metin 3 039 B, varsayılan blok 1 600 B (önce 196, `0x8F918D01B26C0791`, 3 017 B, 1 592 B).

**İç tutarlılık (A4-K89).** `ida_kon_tutarli` yeni alanları denetler (`acil_mandal`, `rampa_var` ≤ 1). Kanca, kontrol durumu tutarsızsa sıfırlamaz, yalnız mandalı kurar: (b2) çağrısında iç tutarlılık denetimi yapılmadığı için bozulmanın kanıtı sonraki yazılabilir çağrıya kalmalıdır. 0 dışındaki her mandal değeri kurulu sayılır; rampanın başlangıç izi negatif ya da gelecekteyse çarpan 0'dır.

**Güdüm ve görev (görev yürütücüsüyle bütünleme, 2026-11-25).** Kural 2'nin güdüm kısmı `ida_gud_oto_adim`'dadır (`gudum/gudum_oto.c`): etkin değer kısıtlayanken (her kabul edilen çağrıda, kipten bağımsız) AUTO güdümünün derinlik referansı (`ref.d`, `ref.w`) ve ILOS integrali (`y_int`) tam 0'dır, `oto_var` ve `t_var` düşer; adım ilerletilmez, birikme olmaz. Değer gevşemiş ama kontrolün mandalı hâlâ kuruluyken (gevşemenin olduğu çağrı; ya da step olmayan çağrıda başlayıp bu çağrıda gevşeyen dönem: o çağrıda güdüm koşmaz, dönemi yalnız mandal taşır) durum her çağrıda ölçülen durumdan, sıfır adımla (`t_var` düşük: dt = 0) yeniden kurulur; mandalı kontrolün kalkış adımı indirir ve o adım güdümden sonra koştuğu için kalkış adımının isteği, aynı ölçülen durumda ilk kez koşan güdümün isteğiyle aynıdır (acil durdurmanın süresinden bağımsız). Var olan `iptal` girdisine VEYA'lamak (yeniden kur + aynı çağrıda dt'lik adım) yetmezdi: acil durdurma boyunca durum sıfır değil bir adımlık olur ve kalkış adımının dt'si sıfır olmaz; iki biçim de birim testinde ayrıldı. Görev yürütücüsünde acil durdurma bir engeldir (`gorev/gorev_yurut.c`, `acil_engel`; mimar kararı): görev çekirdeğin kendi koşuluyla `PAUSED` olur, hedef yoktur, öğe süresi ve bitiş ölçütleri (HOLD birikimi, DIVE_TO bandı, kabul yarıçapı) ilerlemez, değer gevşeyince RESUME beklemeden sürer; operatör duraklaması bununla çözülmez. Toplam görev süresi (`GOREV_AZAMI_S`, duvar saati) ve ölü hesap beklemesi öteki duraklamalardaki gibi sayılmayı sürdürür (kısıtlayan okuma). Değer güdüm ve görev adımlarında çağrı için kesindir: (b) ve kabul edilen örnekler (`kabul_edilen_ornekler`) `ida_gor_adim`'den önce işlenir. Testler: `cekirdek.birim.gudum_oto` (2 s ve 60 s acil durdurmadan sonra kalkıştan 300 çağrı boyunca istek ve durum bit bit aynı ve kalkış anında sıfırdan başlayan güdümle aynı; reddedilen çağrıda başlayan dönem), `cekirdek.birim.gorev_yurut` (200 s acil durdurmada öğe zaman aşımı yok, kabul yarıçapında bitiş yok, gevşeyince kalan süre öncekinin üstüne; HOLD birikimi; operatör duraklaması). Düzeltmesiz kodda (güdüm ya da görev değişikliği geri alınınca) bu testler düşer.

**Var olan testlere etkisi.** `core/test/gudum/test_gudum_gecis.c` düzeneği hiç emniyet örneği vermeden itki bekliyordu (W3-K1'e dayanıyordu); düzeneğe 10 Hz gevşek emniyet örneği eklendi, sınamalar değişmedi. `core/test/kontrol/test_guvenli.c`'nin `test_acil_durdurma_itkiyi_kesmez` testi önceki kuralı sınıyordu; `test_acil_durdurma_itkiyi_keser` oldu. `core/test/gudum/test_gudum_oto.c`'nin beyaz kutu AUTO düzeneği (`auto_kur`) init'teki kısıtlayan değeri doğrudan gevşek yapar (sınamalar değişmedi); görev düzeneği (`core/test/gorev/gorev_duzenek.h`) zaten gevşek değerle kuruluyordu. `tests/kabul/**` ve `zincir.z1` değişmeden geçer. Çekirdek üzerinden itki bekleyen her düzenek acil durdurması gevşek `inst = 0` emniyet örneğini `T_emn_bayat` (250 ms) içinde yinelemelidir.

**Doğrulama** (`cekirdek.birim.kontrol_acil`, 23 test; `core/test/kontrol/test_acil.c`). Kontrol modülü doğrudan: aynı adımda tam sıfır (dört kanal seçimi), çıktı kapısının kontrolcüden bağımsızlığı, 60 s acil durdurmada durum sıfır ve kalkıştan sonraki çıktı ile iç durumun sıfırdan başlayan eşle bit bit aynılığı, kanca, rampa zarfı (T = 0, 1, 5, 500, 5 000 ms; eşle karşılaştırma; zarfın double hesabı), rampa sırasında yeniden acil durdurma, rampanın zamanla ilerlemesi, oturum başı, tutarsız iz. Çekirdek üzerinden (sim yok): örneğin geldiği çağrıda nötr, bayatlığın iki ölçütü, reddedilen çağrı, `out` NULL, tick, reddedilen örnek ve `inst` ≠ 0 örneği, init'ten beri hiç gevşememiş değer, rampa sırasında yeniden acil durdurma. İki özellik testi (her biri 200 000 adım, tohum `0x5EED2026`). Mutasyon sonuçları iş raporundadır.

**Boyut ve süre** (ölçüm; bu Mac, GNU Tools for STM32 14.3.1, `fw-release`; önce = `7901f3f`). Çekirdek arşivi text 80 173 → 80 793 B (+620 B: `kontrol.c` +532 B, `param.c` +52 B, `cerceve.c` +12 B, `cikti.c` +12 B); `ida_fw.elf` text 141 708 → 142 236 B (+528 B), data ve bss değişmedi. `sizeof(ida_core_t)` 14 928 → 14 952 B (`ida_kon_t` 1 592 → 1 600 B; parametre kümeleri ve blok tamponu +16 B). `ida_kon_hesapla`'nın yığın çerçevesi 728 → 720 B. Host'ta `ida_core_step` süresi (Z1 koşusunun 18 001 çağrısı, `host-release`, önce ve sonra dönüşümlü yedişer koşu): ortanca 2,54–2,63 µs → 2,54–2,63 µs (ilk koşu hariç), ortalama 2,59–2,67 µs → 2,56–2,65 µs; fark ölçüm gürültüsünün içindedir. Host değeri kart hedefinin kanıtı değildir; kartta ölçülmedi. Z1 zincirinin CSV'si (`ida_zincir_deneme`; varsayılan koşu ve `--arizasiz --tohum 7`; `host-debug` ve `host-release`) bu iş öncesiyle bayt bayt aynıdır: zincir ilk gevşek emniyet örneğini 25 ms'de verir (`tests/zincir/zincir.cpp`; okuma, ayrıca ölçülmedi), rampa en geç ilk yarım saniyede biter, araç 3,0 s'de silahlanır (koşu özeti).
