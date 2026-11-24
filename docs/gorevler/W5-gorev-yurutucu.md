# W5-gorev-yurutucu — çekirdekte görev yükleme ve yürütme (AUTO kipi)

Tarih: 2026-11-24. Yazan: `ida-uygulayici` (Opus). İnceleyen: emniyet dışı dilim (K23/K26): tek inceleme (`ida-inceleyici`; sol kullanım sınırı doluyken), tek düzeltme turu. Dal: `is/w5-gorev-yurutucu` (taban: `origin/is/w5-kestirim-yatay`; o dal `main`'e girdiyse `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `docs/gorevler/W5-ortak.md`. Önceki dilim: `docs/gorevler/W3-gorev.md` (kip tablosu, ARM, arıza matrisi, `GUIDED_SP`; `main`'de).

## Amaç

Bugün çekirdekte görev yok: `MISSION_*` komutları `UNSUPPORTED` alır, `mission_state` hep `IDLE` kalır, `AUTO` kipine hiç girilemez. Taşıma tarafı hazırdır (`ida_link`, köprü ve `ida_sim` senaryodaki görevi yükleme dizisi olarak gönderiyor). Bu iş çekirdekte görevin yüklenmesini ve yürütülmesini yazar; kestirimin yatay hız ve konumu (W5-kestirim-yatay) artık vardır.

Okunacaklar (dar tut): `docs/mimari/02-ortak-cekirdek.md` §2.4 (güdüm) ve §2.6 (kipler ve görev yönetimi), §4.2–4.3; A4 (`docs/sozlesmeler/A4-port.md`): §3.17'de `MISSION_*` ve `SET_MODE` satırları, A4-K50, K51, `status`'taki görev alanları; A1 (`docs/sozlesmeler/A1-kart-baglantisi.md`): Tablo 3.6a (görev öğeleri) ve görev yükleme mesajları; A5'in §3.10 `gorev` bölümü (yalnız öğe türleri ve alanları); `core/src/gorev/**`, `core/src/gudum/**`, `core/src/kontrol/kontrol.c`'nin istek arayüzü; `core/TASARIM.md` §1, §6, §9; `docs/gorevler/W5-ortak.md` W5-K24.

## Kapsam

1. **Görev deposu ve atomik yükleme (A4-K51):** `MISSION_BEGIN` / `MISSION_ITEM` / `MISSION_END`; öğe denetimi A1 Tablo 3.6a'ya göre (tür, alan aralıkları, sayı sınırı); yükleme ya tümüyle kabul edilir ya hiç (yarım yükleme yürürlüğe girmez; yürüyen görevin üstüne yükleme kuralı sözleşmeden); sabit boylu depo, dinamik bellek yok.
2. **Görev durum makinesi (02 §2.6):** `IDLE` → `LOADED` → `RUNNING` ⇄ `PAUSED` → `COMPLETE` | `ABORTED`; `MISSION_CTL` (başlat, duraklat, sürdür, iptal); `status`'taki görev alanları (durum, etkin öğe, ilerleme) sözleşmedeki alanlarla.
3. **`AUTO` kipi:** giriş koşulları (konum geçerli, görev `LOADED`/`PAUSED`), kipten çıkışta görevin duraklaması, var olan kip × arıza matrisiyle (W3) uyum: arıza eylemi her zaman görevin önündedir.
4. **Öğe yürütücüleri:** `WAYPOINT`, `DIVE_TO`, `SURFACE`, `HOLD`, `WAIT_GNSS`; her biri bitiş ölçütü (kabul yarıçapı, derinlik bandı, süre) ve zaman aşımıyla; aşımda görev `ABORTED` olur ve araç `SURFACE` davranışına geçer (02 §2.6). `SCAN_LINES` ve `SCAN_POINT` bu dilimde yürütülmez: yüklemede kabul edilip edilmeyeceği sözleşmeden okunur; sözleşme reddi tanımlıyorsa o kodla reddedilir, tanımlamıyorsa yüklenir ama yürütmede görev `ABORTED` olur ve bu açıkça raporlanır (tarama deseni sonar işleme dilimiyle gelir).
5. **Güdüm (02 §2.4):** yol noktasına LOS (ya da belgedeki ILOS) yol takibi; derinlik için hız ve ivme sınırlı referans modeli; güdüm çıktısı var olan kontrol isteği arayüzüne (yön, derinlik, ileri hız/kuvvet kanalları) verilir; kontrolcünün iç yapısı değişmez.
6. **`GUIDED`'da "3 s yeni hedef yoksa son hedefte kalır"** kuralı (02 §2.6).
7. **Konuma dayalı arıza koşulları (02 §4.3):** coğrafi sınır ve en az irtifa; irtifa `nav.altitude`'dan. Konum geçerliliği düşünce `AUTO`'nun davranışı var olan matristen. W5-K24 gereği ölü hesapta konuma güven sınırlıdır: `AUTO`'da GNSS'siz kat edilen yol için parametreli bir üst sınır (varsayılanı tahmin) aşılırsa görev duraklar ve araç yüzeye çıkar (`WAIT_GNSS` davranışı); bu, W5-K24'ün açık konusunun görev tarafındaki karşılığıdır.
8. Parametreler `gorev_param.def` / `gudum_param.def`'e; altın şema özeti güncellenir.

## Kapsam dışı (kesin)

Tarama deseni üretimi ve `scan_leg_active`; atış kontrolle her türlü bağ (görev hiçbir koşulda silahlandırma, atış izni ya da `FC_*` komutu üretmez; hedefe yönelme, hedefleme, atış ölçütü yazılmaz); `MANUAL` komutu; A3 `mission_upload` ve web görev paneli (ayrı iş); `core/src/atis/**`, `core/src/kestirim/**`, `core/src/cerceve/**` (kanca imzası değişmesi gerekiyorsa yazma, raporla); `port.h`; `tests/kabul/**` (var olan `kabul.gorev` testleri değişmeden geçer; yer tutucu davranışına bağlı bir kabul testi artık geçmiyorsa testi değiştirme, raporla).

## Dosya sahipliği

Yazılır: `core/src/gorev/**`, `core/src/gudum/**`, `core/test/gorev/**`, `core/test/gudum/**`, `core/test/test_param_hiz.c` (yalnız altın özet), `core/TASARIM.md`'nin görev ve güdüm bölümleri; `core/src/kontrol/**` yalnız istek arayüzünde zorunlu en küçük ekleme (gerekiyorsa; raporlanır); uçtan uca deneme için `scenarios/` altında yeni bir görev senaryosu ve `tests/butunleme/**` içinde yeni test.

## Kabul

1. **Yükleme:** geçerli görev `LOADED`; her geçersizlik türü için (tür, aralık, sayı, sıra, yarım yükleme, `END` sayısı tutmuyor) ret ve deponun değişmediği; yürüyen görev sırasında yükleme kuralı.
2. **Durum makinesi:** her geçiş ve her yasak geçiş için test; `status` alanları.
3. **Öğeler:** her öğe türü için bitiş ölçütü ve zaman aşımı; aşımda `ABORTED` + yüzeye çıkış.
4. **Güdüm:** sentetik araç modeliyle (testin içinde; basit kinematik) düz hat, köşe dönüşü ve akıntı altında yol takibi hatası belirtilen sınırlar içinde; derinlik referansı hız/ivme sınırını aşmaz.
5. **Arıza önceliği:** `AUTO`'da arıza eylemi görevi keser; konum geçerliliği düşünce davranış; ölü hesap yol sınırı.
6. **Uçtan uca (SIL):** yeni senaryo (yüzeyde iki yol noktası → `DIVE_TO` → su altında bir yol noktası → `SURFACE`) `ida_sim --kip sil` ile koşar; görev `COMPLETE` olur; yer gerçeğine göre yol noktası kaçırma uzaklıkları ve derinlik hatası raporlanır (ölçüm); iki koşu bayt bayt aynı.
7. Belirlenimcilik (A4-K24); var olan `core/test`, `tests/kabul/**` (181 kabul testi), `zincir.z1` değişmeden geçer; Z1 çıktısındaki değişim varsa hangi alanların ve neden değiştiği yazılır.
8. `fw-release` kod boyu farkı ve `ida_core_t` payı; `host-release`'te step süresinin bağıl farkı (makine sakinken).
9. `host-debug` + `host-release` tam tur (sonda bir kez); `fw-release` derleme; `host-asan` yalnız derleme; W3-K12 statik analiz; W3-K14; MSVC kalıpları (`(uint8_t)~SABIT` yok; `/analyze` için iki dizi üzerinde ortak indeksli üçlü işleç yok).
