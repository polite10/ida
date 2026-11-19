# A6 — Kayıt

| | |
|---|---|
| Sözleşme | A6 |
| Sürüm | 1.0-aday, düzeltme turu 2 (dosya sürümü `a6_surumu = 1.0`) |
| Tarih | 2026-11-19; düzeltme turu 1: 2026-11-20 (`docs/sozlesmeler/inceleme/A3A6-yanit.md`); düzeltme turu 2: 2026-11-20 (`docs/sozlesmeler/inceleme/A3A6-yanit-2.md`) |
| Görev | `docs/gorevler/W1-A3A6.md` |
| Makinece okunur parça | `docs/sozlesmeler/sema/a6-kanallar.json` |
| Dayanak | `docs/mimari/00-iskelet.md` sürüm 0.8 (§3 A6 satırı, §4, §6, §10 md.22–23); `docs/mimari/08-laboratuvar-uygulamasi.md` §2.6, §3.5; 04 §2.13; 06 §2.13 |
| İlgili sözleşme | A3 (`docs/sozlesmeler/A3-web.md`): ortak kanallar ve yük düzenleri; A1, A2, A4, A5, A8 (ayrı işler) |

Bu belge kural yazar; gerekçe 08 §2.6 ve §3.5'tedir. Makinece okunur dosya bu belgenin parçasıdır. Ortak kanalların yük düzenleri A3 kataloğundakiyle aynıdır ve iki dosya aynı tanımdan üretilmiştir; bu eşitlik 7. başlıktaki denetimle gösterilir.

---

## 1. Kapsam ve taraflar

**Yazan:** sim sunucusunun kayıt yazıcısı (`sim/` altında, C++20; tek iş parçacığı).
**Okuyanlar:** sim sunucusunun izleme ve tekrar koşusu sürücüleri; A3 kayıt listesi ve üst veri uçları; dışa aktarıcı (`tools/` altında, 08 §3.5; uygulama işi); test araçları.

**Kapsam:** dosya biçimi (MCAP sürümü, sıkıştırma, parça boyutu); kanal ve şema kataloğu ve her kanalın hangi kayıt kademesinde yazıldığı; üst veri ve ekler; tekrar oynatılabilirlik gerekleri (her komutun uygulandığı fizik adımı, bariyer kararları, geç kalan ve reddedilen yanıtlar, düşürülen veriler); tekrar oynatmanın üç düzeyi (girdi düzeyinin HIL kaydı için güvencesiz yeniden kurma biçimiyle birlikte), her birinin gerektirdiği kayıt ve verdiği güvence; bütünlük denetimi (CRC) ve kurtarma okuması; eksik, bozuk ve yarım kayıtta davranış; sürüm uyumluluğu.

**Kapsam dışı:**
- A1 mesajlarının içeriği (A1 sözleşmesi). A6 yalnız "A1 baytları ham ve zaman damgalı kaydedilir" kuralını ve gereken ek alanları yazar.
- A3 telemetri çerçevelerinin kendisi kaydedilmez; aynı tanımdan türeyen veri doğrudan yazılır (iskelet A6 satırı, §10 md.22; U-28).
- Kayıt verisinin dış araçlarda (Foxglove, PlotJuggler vb.) doğrudan çizilebilmesi iddia edilmez (00 düzeltme 03.5); dışa aktarıcının biçimi uygulama işidir.
- Atış çözümü, takip ve hedefleme verisi kaydedilmez; atışla ilgili yalnız emniyet zincirinin durumu ve olay kararları kaydedilir (atış kontrol sınırı).

## 2. Terimler ve kabuller

İskelet §3.1 ve §4 kabulleri aynen geçerlidir. A3 §2'deki terimler (kanal, kanal-anahtar, komut, komut numarası, adım sınırı, canlı çalıştırma) aynı anlamdadır.

| Terim | Anlam |
|---|---|
| Kayıt kademesi | İskelet §6 kayıt satırı: `olaylar` (kapalı = yalnızca olaylar), `telemetri` (sade), `tam` |
| Kayıt duvar saati | Kayıt açılışında 0 olan monoton saat, ns; dosyadaki bütün duvar saati alanlarının kaynağı (A6-K13) |
| Tekrar düzeyleri | `izleme` (a), `fizik` (b: kayıtlı komutlarla fizik yeniden koşturma), `girdi` (c: kayıtlı çekirdek çağrılarının PC'deki çekirdeğe verilmesi; yalnız SIL kaydından), `yeniden_kurma` (d: HIL kaydındaki A1 bayt akışından çağrı girdilerinin PC'de yeniden kurulması; güvencesiz); iskelet A6 satırı |
| Çekirdek çağrı kaydı | SIL platform katmanının çekirdeğe yaptığı her çağrının işlev türü, sırası, girdisi, dönüş kodu ve çıktısı (`cekirdek/*` kanalları; A6-K54) |
| Kurtarma okuması | Kapanmamış dosyanın A6'nın tanımladığı sıralı okuması (A6-K60) |
| Bozuk dosya | CRC'si tutmayan ya da çözülemeyen en az bir parça ya da ek içeren dosya (A6-K59) |
| Tekrar güvencesi | Bir kaydın hangi düzeye kadar, hangi koşulda ne güvence verdiği (§4.6) |
| Kanonik işlevsel iz | Karşılaştırmaya giren, duvar saatinden bağımsız alanların sıralı listesi (§4.7; 06 §2.13) |
| Önek güvencesi | Eksik kayıtta güvencenin yalnız ilk eksik adımdan önceki adımlar için geçerli olması (A6-K33) |

---

## 3. Veri tanımları

### 3.1 Dosya ve adlandırma

- Bir çalıştırma bir dosyadır: `<kayit_dizini>/<run_id>.mcap`. `run_id` A3-K63'teki biçimdedir (`YYYYMMDD-hhmmss-xxxxxx`).
- Varsayılan kayıt dizini sim sunucusunun çalışma dizinindeki `kayitlar/`'dır; kayıtlar depoya girmez (`.gitignore`; AGENTS.md "Git").

### 3.2 MCAP kayıtlarının kullanımı

MCAP biçimi birincil kaynaktan doğrulandı (https://mcap.dev/spec ve https://mcap.dev/spec/registry, 2026-11-19 erişimi; yazıcı ve okuyucu davranışı `foxglove/mcap` deposunun `cpp/mcap/include/mcap/writer.hpp`, `reader.hpp`, `types.hpp` dosyaları, `main` dalı, aynı tarih).

| MCAP kaydı | A6'daki kullanım |
|---|---|
| Sihirli baytlar | Başta ve sonda `0x89 'M' 'C' 'A' 'P' 0x30 '\r' '\n'`; `0x30` büyük sürüm 0'dır (spec "Magic") |
| Header | `profile = "ida"` (spec, kayıtlı olmayan çerçeve adına izin verir); `library` = yazıcı kitaplığının değeri |
| Schema | Her (yük, `kanal_surumu`) için bir kayıt: `name = "ida.<yük adı>.v<kanal_surumu>"`, `encoding = "ida-yuk-v1"`, `data` = o yükün `a6-kanallar.json` içindeki alan listesinin kanonik JSON metni (A6-K07). `id` sıfır olamaz (spec) |
| Channel | Her kanal için bir kayıt: `topic` = kanal adı (ör. `fizik/durum`), `message_encoding = "ida-ikili-v1"`, `metadata` = `{kanal_surumu, zaman_anlami, kademe}` |
| Message | `channel_id`; `sequence` = dosya genelinde yazım sırası; `log_time` = sim zamanı; `publish_time` = kayıt duvar saati; `data` = yük (A6-K08, K09) |
| Chunk | zstd sıkıştırmalı; hedef 1 MiB sıkıştırmasız (A6-K03, K04); `uncompressed_crc` her zaman hesaplanıp yazılır (A6-K51) ve A6 okuyucusu tarafından doğrulanır (A6-K59) |
| Message Index, Chunk Index, Statistics, Summary, Summary Offset, Data End, Footer | Kitaplığın varsayılanlarıyla yazılır (A6-K03) |
| Metadata | `ida_kayit` (açılış) ve `ida_kayit_kapanis` (kapanış) kayıtları (§3.5) |
| Attachment | §3.6'daki ekler; ekler parça içine yazılmaz (spec) |

`ida-yuk-v1` ve `ida-ikili-v1` MCAP kaydındaki iyi bilinen kodlamalar listesinde yoktur (registry: ileti kodlamaları `ros1`, `cdr`, `protobuf`, `flatbuffer`, `capnproto`, `cbor`, `msgpack`, `json`; şema kodlamaları `protobuf`, `flatbuffer`, `capnproto`, `ros1msg`, `ros2msg`, `ros2idl`, `omgidl`, `jsonschema`). Registry kayıtlı olmayan kodlama hakkında bir şey söylemez; genel MCAP okuyucularının bu kanalları açıp çözemeyeceği beklenir, çizebileceği iddia edilmez.

### 3.3 Kanal kataloğu

"Kademe" kanalın yazıldığı en düşük kayıt kademesidir; daha yüksek kademelerde de yazılır. "Tekrar" sütunu kanalın hangi tekrar düzeyinin gereği olduğunu gösterir. "A3 ortak" kanallar A3'teki kanalla aynı ad, yük ve `kanal_surumu`'nu taşır.

| Ad | Yük | Kademe | Ne zaman yazılır | `log_time` anlamı | Tekrar | A3 ortak | Kaynak |
|---|---|---|---|---|---|---|---|
| `olay` | `yuk_olay` | olaylar | her_uretimde | sim | izleme | evet | bütün parçalar |
| `a3/komut` | `yuk_a3_komut` | olaylar | her_uretimde | sim | izleme, fizik | hayır | A3 sunucusu |
| `a3/gecersiz_istek` | `yuk_a3_gecersiz_istek` | olaylar | her_uretimde | sim | izleme | hayır | A3 sunucusu |
| `a3/komut_sonucu` | `yuk_a3_komut_sonucu` | olaylar | her_uretimde | sim | izleme, fizik | hayır | dünya iş parçacığı |
| `senaryo/oge_uygulama` | `yuk_senaryo_oge_uygulama` | olaylar | her_uretimde | sim | izleme, fizik | hayır | senaryo motoru (08 §2.2) |
| `hil/oturum` | `yuk_hil_oturum` | olaylar | her_uretimde | sim | izleme, yeniden_kurma | hayır | 06 oturum |
| `hil/bariyer` | `yuk_hil_bariyer` | olaylar | olaylar kademesinde yalnız `karar != zamaninda`; telemetri ve tamda her parti | sim | izleme, fizik | hayır | iskelet §4, 06 bariyer |
| `hil/dusen` | `yuk_hil_dusen` | olaylar | her_uretimde | sim | izleme | hayır | 05, 06 |
| `kayit/eksik` | `yuk_kayit_eksik` | olaylar | her_uretimde | sim | izleme, fizik, girdi, yeniden_kurma | hayır | kayıt yazıcısı |
| `sayac/gercek_zaman` | `yuk_sayac_gercek_zaman` | olaylar | 1 Hz (duvar saati) ve kapanışta | sim | izleme | evet | 04, 05, 06 |
| `calistirma/durum` | `yuk_calistirma_durum` | olaylar | değişimde | sim | izleme | evet | çalıştırma denetçisi |
| `emniyet/girdiler` | `yuk_emniyet_girdiler` | olaylar | değişimde | sim | izleme, fizik | evet | sanal emniyet girdileri |
| `atis/durum` | `yuk_atis_durum` | olaylar | değişimde | sim | izleme | evet | fırlatıcı modeli + kart FC durumu |
| `ariza/etkin` | `yuk_ariza_etkin` | olaylar | değişimde | sim | izleme | evet | arıza dağıtıcısı |
| `fizik/durum` | `yuk_fizik_durum` | telemetri | her kontrol periyodu sınırı (200 Hz) | olcum | izleme, fizik | evet | 04 WorldSnapshot |
| `fizik/adim_sonucu` | `yuk_fizik_adim_sonucu` | telemetri | boş olmayan her fizik adımı | sim | fizik | hayır | 04 StepResult |
| `a1/kart_sim` | `yuk_a1_blok` | telemetri | her_uretimde | alim | izleme, yeniden_kurma | hayır | 06 G/Ç iş parçacığı |
| `hil/zaman_esleme` | `yuk_hil_zaman_esleme` | telemetri | değişimde ve 1 Hz | sim | izleme, yeniden_kurma | hayır | 06 §2.5 |
| `hil/enjeksiyon` | `yuk_hil_enjeksiyon` | telemetri | her_uretimde | sim | izleme, yeniden_kurma | hayır | 06 §2.12 |
| `yer_istasyonu/emulator` | `yuk_yer_istasyonu_emulator` | telemetri | her_uretimde | sim | izleme | hayır | 06 §2.11 |
| `kart/durum` | `yuk_kart_durum` | telemetri | her_uretimde | alim | izleme | evet | A1 HIL düzlemi |
| `yer_istasyonu/durum` | `yuk_kart_durum` | telemetri | her_uretimde | alim | izleme | evet | A1 yer istasyonu düzlemi |
| `algilama/kayit` | `yuk_algilama_kayit` | telemetri | her_uretimde | olcum | izleme | evet | A8, A1 |
| `sensor/imu` | `yuk_imu` | telemetri | her_uretimde | olcum | izleme | evet | 05 |
| `sensor_temel/imu` | `yuk_imu` | telemetri | her_uretimde | olcum | izleme | hayır | 05 arızasız temel kayıt |
| `sensor_gercek/imu` | `yuk_imu` | telemetri | her_uretimde | olcum | izleme | hayır | 05 yer gerçeği |
| `sensor/manyetometre` | `yuk_manyetometre` | telemetri | her_uretimde | olcum | izleme | evet | 05 |
| `sensor_temel/manyetometre` | `yuk_manyetometre` | telemetri | her_uretimde | olcum | izleme | hayır | 05 arızasız temel kayıt |
| `sensor_gercek/manyetometre` | `yuk_manyetometre` | telemetri | her_uretimde | olcum | izleme | hayır | 05 yer gerçeği |
| `sensor/basinc` | `yuk_basinc` | telemetri | her_uretimde | olcum | izleme | evet | 05 |
| `sensor_temel/basinc` | `yuk_basinc` | telemetri | her_uretimde | olcum | izleme | hayır | 05 arızasız temel kayıt |
| `sensor_gercek/basinc` | `yuk_basinc` | telemetri | her_uretimde | olcum | izleme | hayır | 05 yer gerçeği |
| `sensor/gnss` | `yuk_gnss` | telemetri | her_uretimde | olcum | izleme | evet | 05 |
| `sensor_temel/gnss` | `yuk_gnss` | telemetri | her_uretimde | olcum | izleme | hayır | 05 arızasız temel kayıt |
| `sensor_gercek/gnss` | `yuk_gnss` | telemetri | her_uretimde | olcum | izleme | hayır | 05 yer gerçeği |
| `sensor/dvl` | `yuk_dvl` | telemetri | her_uretimde | olcum | izleme | evet | 05 |
| `sensor_temel/dvl` | `yuk_dvl` | telemetri | her_uretimde | olcum | izleme | hayır | 05 arızasız temel kayıt |
| `sensor_gercek/dvl` | `yuk_dvl` | telemetri | her_uretimde | olcum | izleme | hayır | 05 yer gerçeği |
| `sensor/batarya` | `yuk_batarya` | telemetri | her_uretimde | olcum | izleme | evet | 05 |
| `sensor_temel/batarya` | `yuk_batarya` | telemetri | her_uretimde | olcum | izleme | hayır | 05 arızasız temel kayıt |
| `sensor_gercek/batarya` | `yuk_batarya` | telemetri | her_uretimde | olcum | izleme | hayır | 05 yer gerçeği |
| `sensor/sizinti` | `yuk_sizinti` | telemetri | her_uretimde | olcum | izleme | evet | 05 |
| `sensor_temel/sizinti` | `yuk_sizinti` | telemetri | her_uretimde | olcum | izleme | hayır | 05 arızasız temel kayıt |
| `sensor_gercek/sizinti` | `yuk_sizinti` | telemetri | her_uretimde | olcum | izleme | hayır | 05 yer gerçeği |
| `sensor/firlatici` | `yuk_firlatici` | telemetri | her_uretimde | olcum | izleme | evet | 05 |
| `sensor_temel/firlatici` | `yuk_firlatici` | telemetri | her_uretimde | olcum | izleme | hayır | 05 arızasız temel kayıt |
| `sensor_gercek/firlatici` | `yuk_firlatici` | telemetri | her_uretimde | olcum | izleme | hayır | 05 yer gerçeği |
| `a1/sim_kart` | `yuk_a1_blok` | tam | her_uretimde | sim | izleme, yeniden_kurma | hayır | 06 G/Ç iş parçacığı |
| `cekirdek/baslatma` | `yuk_cekirdek_baslatma` | tam (yalnız çekirdeği PC'de çağıran kiplerde: `sil`, `tekrar_girdi_sil`, `tekrar_yeniden_kurma`) | her `ida_core_init` çağrısında | sim | girdi | hayır | SIL platform katmanı (A4) |
| `cekirdek/cagri` | `yuk_cekirdek_cagri` | tam (`cekirdek/baslatma` ile aynı kiplerde) | her çekirdek çağrısı | sim | girdi | hayır | SIL platform katmanı (A4) |
| `sonar/ping360` | `yuk_ping360` | tam | her_uretimde | gonderim | izleme | evet | 05 |
| `sonar/ping1d` | `yuk_ping1d` | tam | her_uretimde | gonderim | izleme | evet | 05 |
| `sonar_gercek/ping360` | `yuk_sonar_gercek_ping360` | tam | her_uretimde | gonderim | izleme | hayır | 05 yer gerçeği |
| `kamera/kare` | `yuk_kamera_kare` | tam (`kamera_kareleri != kapali`) | her_uretimde | olcum | izleme | hayır | A2 |

`sensor_temel/<tür>` arızasız temel kaydı, `sensor_gercek/<tür>` aynı ölçümün yer gerçeğini taşır (05 §3 `base` ve `truth`); yükleri `sensor/<tür>` ile aynıdır.

### 3.4 Yük düzenleri

A3 ile ortak yüklerin tabloları A3 §3.4'tedir ve burada yinelenmez: `yuk_calistirma_durum`, `yuk_sayac_gercek_zaman`, `yuk_olay`, `yuk_fizik_durum`, `yuk_kart_durum`, `yuk_atis_durum`, `yuk_emniyet_girdiler`, `yuk_ariza_etkin`, sensör yükleri (`yuk_imu` … `yuk_firlatici`), `yuk_ping360`, `yuk_ping1d`, `yuk_algilama_kayit`. Tel tipleri A3 §3.1'deki gibidir. A6'ya özgü yükler:

#### `yuk_a1_blok`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `duvar_ns` | i64 | ns | tipin aralığı | bloğun taşımaya verildiği (sim_kart) ya da taşımadan alındığı (kart_sim) an; kayıt duvar saati (A6-K13) |
| `plan_duvar_ns` | i64 | ns | yok: INT64_MIN | sim_kart: penceresinin planlanan başlangıcı; kart_sim: INT64_MIN |
| `blok_turu` | u8 | - | 0=kritik, 1=toplu, 2=oturum, 3=bilinmiyor | sim'in bildiği blok türü |
| `enjeksiyon` | u8 | - | 0=hayir, 1=evet | bloğa hata enjektörü işlem uyguladı mı |
| `bayt` | bayt_dizisi(u32, en çok 65536) | bayt | tipin aralığı | taşımaya verilen ya da taşımadan alınan baytlar, olduğu gibi |

#### `yuk_hil_bariyer`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `step_seq` | u32 | - | tipin aralığı | partinin sıra numarası (A1) |
| `kapanis_adimi` | u64 | - | tipin aralığı | partinin kapandığı fizik adımı |
| `uygulama_adimi` | u64 | - | tipin aralığı | kapanış + L_act'e denk gelen fizik adımı |
| `karar` | u8 | - | 0=zamaninda, 1=gec_bekletildi, 2=kacirildi, 3=ret_yinelenen, 4=ret_uygulama_ani_gecmis | iskelet §4 kararı |
| `varis_duvar_ns` | i64 | ns | yok: INT64_MIN | yanıtın vardığı duvar saati; gelmediyse INT64_MIN |
| `bekleme_ns` | i64 | ns | tipin aralığı | bariyer beklemesi (gerçek zaman kayması); beklenmediyse 0 |
| `kip` | u8 | - | 1=sil, 2=hil_1x, 3=hil_bariyer | zaman kipi |

#### `yuk_fizik_adim_sonucu`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `adim_no` | u64 | - | tipin aralığı | fizik adımı |
| `komut_durumu` | u8 | - | 0=degismedi, 1=yeni_uygulandi, 2=omur_doldu_notr, 3=kacirildi_degismedi | bu adımdaki sürekli komut kararı (iskelet §4) |
| `uygulanan_step_seq` | u32 | - | yok: 0xFFFFFFFF | yürürlükteki komutun parti numarası; nötrde 0xFFFFFFFF |
| `pwm_us` | u16[8] | us | tipin aralığı | yürürlükteki itici komutları (ESC darbe genişliği; iskelet A1) |
| `gecerlilik_maskesi` | u8 | - | tipin aralığı | itici başına komut geçerli biti |
| `omur_bitis_t_ns` | i64 | ns | yok: INT64_MIN | yürürlükteki komutun kullanım süresinin bittiği sim zamanı; nötrde INT64_MIN |
| `kutle_durumu` | u8 | - | 0=iki_torpido, 1=bir_torpido, 2=bos | adım sonundaki kütle durumu |
| `model_olaylari` | u32 | - | tipin aralığı | 04 §4 model olayları bit maskesi (NaN, güç çökmesi, temas derinliği vb.; bit tablosu 04 uygulamasından beklenen) |
| `olaylar` | dizi(u8, en çok 8) | - | tipin aralığı | bu adımda karar verilen tek seferlik olaylar |
| `olaylar[].olay_kimligi` | u64 | - | tipin aralığı | A1'deki ham olay kimliği (açılış sayacı + olay sayacı; bit düzeni A1'den beklenen) |
| `olaylar[].normallestirilmis_sira` | u32 | - | tipin aralığı | bu kimliğin çalıştırmadaki ilk görülme sırası, 1'den (A6-K41) |
| `olaylar[].olay_turu` | u8 | - | tipin aralığı | A1 olay türü kodu (A1'den beklenen) |
| `olaylar[].tup` | u8 | - | yok: 255 | hedef tüp; yoksa 255 |
| `olaylar[].sonuc` | u8 | - | 1=uygulandi, 2=yineleme, 3=ret_emniyet, 4=ret_suresi_gecmis, 5=ret_diger | sim'in kararı |

#### `yuk_hil_oturum`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `onceki` | u8 | - | 0=kapali, 1=el_sikisma, 2=hazir, 3=kosuyor, 4=kopuk | önceki oturum durumu |
| `sonraki` | u8 | - | 0=kapali, 1=el_sikisma, 2=hazir, 3=kosuyor, 4=kopuk | yeni oturum durumu |
| `sess` | u16 | - | tipin aralığı | oturum numarası; yoksa 0 |
| `acilis_sayaci` | u32 | - | tipin aralığı | kartın açılış sayacı; yoksa 0 |
| `neden` | u8 | - | 0=el_sikisma_tamam, 1=uyumsuzluk, 2=reboot_istendi, 3=canlilik_kaybi, 4=aygit_kalkti, 5=beklenmedik_sifirlama, 6=planli_sifirlama, 7=sim_kapanisi | geçiş nedeni |
| `planli` | u8 | - | 0=hayir, 1=evet | geçiş planlı mı |
| `ayrinti` | metin(u16, en çok 256) | bayt | tipin aralığı | ek bilgi (HELLO_ACK sonucu vb.) |

#### `yuk_hil_dusen`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `tur` | u8 | - | 0=bulk_drop, 1=ping360_eksik, 2=sonar_gec, 3=sonar_kuyruk_dolu, 4=parti_eksik, 5=sensor_kuyruk_tasma, 6=gcs_kaymasi, 7=crc_hatasi, 8=eski_oturum, 9=gec_olay, 10=es_zamansiz_guvenli_cikti, 11=a1_alan_hatasi, 12=kamera_kare_dusen | düşen verinin türü |
| `step_seq` | u32 | - | yok: 0xFFFFFFFF | ilgili parti; yoksa 0xFFFFFFFF |
| `sensor_id` | u16 | - | yok: 0xFFFF | ilgili sensör; yoksa 0xFFFF |
| `adet` | u32 | - | tipin aralığı | bu kayıtla bildirilen düşen öğe sayısı |
| `ayrinti` | metin(u16, en çok 256) | bayt | tipin aralığı | neden ve ek bilgi |

#### `yuk_hil_enjeksiyon`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `yon` | u8 | - | 0=sim_kart, 1=kart_sim | yön |
| `islem` | u8 | - | 0=dusurme, 1=cogaltma, 2=yer_degistirme, 3=bayt_bozma, 4=kesme, 5=gecikme_sicramasi, 6=cop_bayt | 06 §2.12 işlemi |
| `step_seq` | u32 | - | yok: 0xFFFFFFFF | ilgili parti; yoksa 0xFFFFFFFF |
| `bayt_konumu` | u32 | - | yok: 0xFFFFFFFF | blok içindeki bayt konumu; yoksa 0xFFFFFFFF |
| `ayrinti` | metin(u16, en çok 256) | bayt | tipin aralığı | işlem parametreleri |

#### `yuk_hil_zaman_esleme`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `t0_duvar_ns` | i64 | ns | tipin aralığı | W(t) = T0 + (t - t0) eşleminin T0'i (06 §2.5), kayıt duvar saatinde |
| `t0_sim_ns` | i64 | ns | tipin aralığı | eşlemenin t0'i |
| `timesync_ofset_ns` | i64 | ns | yok: INT64_MIN | kart saati ile sim duvar saati arasındaki ofset kestirimi; yoksa INT64_MIN |
| `d_asagi_ns` | i64 | ns | yok: INT64_MIN | aşağı yön teslim gecikmesi kestirimi; yoksa INT64_MIN |

#### `yuk_yer_istasyonu_emulator`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `yon` | u8 | - | 0=yer_to_arac, 1=arac_to_yer | yön |
| `karar` | u8 | - | 0=gecti, 1=dus_kayip, 2=dus_kuyruk, 3=kopuk, 4=model_tutulamadi | emülatör kararı |
| `giris_t_ns` | i64 | ns | tipin aralığı | çerçevenin emülatöre girdiği sim zamanı |
| `cikis_t_ns` | i64 | ns | yok: INT64_MIN | çerçevenin emülatörden çıktığı sim zamanı; düştüyse INT64_MIN |
| `cerceve` | bayt_dizisi(u16, en çok 280) | bayt | tipin aralığı | emülatöre giren MAVLink çerçevesi |

#### `yuk_a3_komut`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `komut_no` | u32 | - | tipin aralığı | A3 komut numarası |
| `istemci_id` | u32 | - | tipin aralığı | istemci kimliği; yoksa 0 |
| `istek_kimligi` | metin(u16, en çok 64) | bayt | tipin aralığı | istemcinin verdiği istek kimliği |
| `yontem` | metin(u16, en çok 8) | bayt | tipin aralığı | HTTP yöntemi |
| `yol` | metin(u16, en çok 256) | bayt | tipin aralığı | istek yolu |
| `govde` | metin(u32, en çok 4194304) | bayt | tipin aralığı | istek gövdesi; yalnız uç şemasından geçen istekler bu kanala yazılır, diğerleri `a3/gecersiz_istek`e (A3-K52) |
| `alim_duvar_ns` | i64 | ns | tipin aralığı | isteğin alındığı kayıt duvar saati |
| `senkron_sonuc` | u8 | - | 0=kuyruga_alindi, 1=tamamlandi, 2=reddedildi | HTTP yanıtındaki sonuç |
| `http_durum` | u16 | - | tipin aralığı | döndürülen HTTP durum kodu |
| `hata_kodu` | metin(u16, en çok 64) | bayt | tipin aralığı | reddedildiyse A3 hata kodu; değilse boş |

#### `yuk_a3_gecersiz_istek`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `komut_no` | u32 | - | tipin aralığı | A3 komut numarası (A3-K52) |
| `istemci_id` | u32 | - | tipin aralığı | istemci kimliği; başlık yoksa ya da kayıtlı değilse 0 |
| `yontem` | metin(u16, en çok 8) | bayt | tipin aralığı | HTTP yöntemi |
| `yol` | metin(u16, en çok 256) | bayt | tipin aralığı | istek yolu (256 bayttan uzunsa kesilmiş) |
| `alim_duvar_ns` | i64 | ns | tipin aralığı | isteğin alındığı kayıt duvar saati (A6-K13) |
| `bildirilen_uzunluk` | u64 | bayt | yok: 2^64-1 | `Content-Length` değeri; başlık yoksa 2^64-1 |
| `okunan_uzunluk` | u64 | bayt | tipin aralığı | sunucunun fiilen okuduğu gövde baytı; gövde sınırı aşıldıysa sınıra kadar |
| `sinir_asildi` | u8 | - | 0=hayir, 1=evet | gövde sınırı aşıldı ve sunucu sınırın ötesini okumadı (A3-K51) |
| `kesildi` | u8 | - | 0=hayir, 1=evet | okunan gövde 4096 bayttan uzun olduğu için `on_ek` kesildi |
| `http_durum` | u16 | - | tipin aralığı | döndürülen HTTP durum kodu |
| `hata_kodu` | metin(u16, en çok 64) | bayt | tipin aralığı | ret nedeni: A3 hata kodu |
| `on_ek` | bayt_dizisi(u16, en çok 4096) | bayt | tipin aralığı | okunan gövdenin ilk en çok 4096 baytı, olduğu gibi (geçerli UTF-8 olmayabilir) |

#### `yuk_cekirdek_baslatma`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `cagri_sirasi` | u64 | - | tipin aralığı | çalıştırma genelinde 1'den başlayan, init dahil her çekirdek çağrısında bir artan sıra; init'te sıfırlanmaz (A6-K54) |
| `init_kusagi` | u32 | - | tipin aralığı | bu init çağrısıyla başlayan kuşak; çalıştırmanın ilk init'i 1, her yeni init bir artar |
| `port_major` | u16 | - | tipin aralığı | platformun derlendiği A4 port sürümü (A4 `IDA_PORT_VERSION_MAJOR`) |
| `port_minor` | u16 | - | tipin aralığı | A4 `IDA_PORT_VERSION_MINOR` |
| `cekirdek_bilgisi` | bayt_dizisi(u16, en çok 256) | bayt | tipin aralığı | `ida_core_info_t` (çekirdek yapı kimliği: port sürümü, `core_size`, `core_align`, `param_schema_hash`, `core_version`), A4 alan sırasıyla alan alan kodlanmış (A6-K52) |
| `baslatma_girdisi` | bayt_dizisi(u16, en çok 1024) | bayt | tipin aralığı | `ida_init_in_t`'nin `param_blob` dışındaki alanları, A4 alan sırasıyla alan alan kodlanmış (A6-K52) |
| `param_blob` | bayt_dizisi(u16, en çok 8000) | bayt | tipin aralığı | kodlanmış parametre bloğu, A4 §3.10 bayt düzeninde olduğu gibi |
| `donus_kodu` | u8 | - | tipin aralığı | `ida_core_init` dönüş kodu (A4 `ida_rc_t`) |

#### `yuk_cekirdek_cagri`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `cagri_sirasi` | u64 | - | tipin aralığı | çalıştırma genelinde tekdüze çekirdek çağrısı sırası; init, step, tick ve background ortak sıradadır, init'te sıfırlanmaz (A6-K54) |
| `init_kusagi` | u32 | - | tipin aralığı | çağrının ait olduğu init kuşağı (`cekirdek/baslatma.init_kusagi`) |
| `islev` | u8 | - | 1=step, 2=tick, 3=background | çağrılan işlev |
| `girdi` | bayt_dizisi(u32, en çok 262144) | bayt | tipin aralığı | step ve tick: `ida_in_t` (`bg_slices` ve `ping360` varlık bayrağı ile satırı dahil); background: `now_w`; A4 alan sırasıyla alan alan kodlanmış (A6-K52) |
| `donus_kodu` | u8 | - | tipin aralığı | çağrının dönüş kodu (`ida_rc_t` ya da `ida_bg_rc_t`) |
| `cikti` | bayt_dizisi(u32, en çok 262144) | bayt | tipin aralığı | step ve tick: `ida_out_t`, alan alan kodlanmış; background: boş |

#### `yuk_a3_komut_sonucu`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `komut_no` | u32 | - | tipin aralığı | bu sim sürecinin A3 komut numarası; tekrarda uygulanan kayıtlı komut da güncel süreçten yeni numara alır (A6-K37) |
| `kaynak_run_id` | metin(u16, en çok 32) | bayt | tipin aralığı | tekrar koşusunda komutun alındığı kaydın `run_id`'si; canlı komutta boş |
| `kaynak_komut_no` | u32 | - | tipin aralığı | tekrar koşusunda komutun kaynak kayıttaki `komut_no`'su; canlı komutta 0 |
| `sonuc` | u8 | - | 0=uygulandi, 1=reddedildi | komutun nihai sonucu |
| `uygulama_adimi` | u64 | - | yok: 2^64-1 | uygulandığı fizik adımı; reddedildiyse 2^64-1 |
| `hata_kodu` | metin(u16, en çok 64) | bayt | tipin aralığı | reddedildiyse A3 hata kodu |
| `ayrinti` | metin(u16, en çok 8192) | bayt | tipin aralığı | UTF-8 JSON nesnesi: komuta özgü sonuç |

#### `yuk_senaryo_oge_uygulama`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `oge_kimligi` | metin(u16, en çok 64) | bayt | tipin aralığı | A5 zaman çizelgesi öğesinin kimliği |
| `tur` | u8 | - | 0=ariza, 1=emniyet_girdisi, 2=ortam, 3=gorev, 4=diger | öğe türü |
| `kosul_saglandi_adimi` | u64 | - | tipin aralığı | koşulun sağlandığı adım; zamanlı öğede uygulama adımıyla aynı |
| `uygulama_adimi` | u64 | - | yok: 2^64-1 | öğenin uygulandığı fizik adımı; reddedildiyse 2^64-1 |
| `sonuc` | u8 | - | 0=uygulandi, 1=reddedildi | sonuç |
| `ayrinti` | metin(u16, en çok 1024) | bayt | tipin aralığı | UTF-8 JSON nesnesi |

#### `yuk_sonar_gercek_ping360`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `aci_rad` | f32 | rad | [0, 6.283185307179586] | kafa açısı |
| `katkilar` | dizi(u16, en çok 64) | - | tipin aralığı | satırın en büyük enerjili katkıları, büyükten küçüğe |
| `katkilar[].kutu` | u16 | - | tipin aralığı | katkının düştüğü kutu |
| `katkilar[].yol_turu` | u8 | - | 0=dogrudan, 1=tek_yuzey, 2=tek_dip, 3=coklu | yol türü |
| `katkilar[].gercek_yansima_t_ns` | i64 | ns | tipin aralığı | katkının fiziksel yansıma anı (nominal kutu zamanıyla aynı olmayabilir; iskelet §3.1) |
| `katkilar[].govde_id` | u16 | - | yok: 0xFFFF | yansıtan gövde; dip ve yüzey için 0xFFFF |
| `katkilar[].enerji_payi` | f32 | - | [0, 1] | kutunun toplam enerjisindeki pay |

#### `yuk_kamera_kare`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `kare_id` | u64 | - | tipin aralığı | kare kimliği (A2) |
| `genislik` | u16 | piksel | [1, 640] | genişlik; kademe değerleri A2'de (tam 640, sade 320; tahmin) |
| `yukseklik` | u16 | piksel | [1, 480] | yükseklik; kademe değerleri A2'de (tam 480, sade 240; tahmin) |
| `piksel_bicimi` | u8 | - | tipin aralığı | A2'nin piksel biçimi kodu (A2'den beklenen) |
| `a2_surumu` | u16 | - | tipin aralığı | karenin A2 sürümü |
| `piksel` | bayt_dizisi(u32, en çok 921600) | bayt | tipin aralığı | piksel verisi, A2 düzeninde |

#### `yuk_kayit_eksik`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `kanal` | metin(u16, en çok 64) | bayt | tipin aralığı | iletisi düşen kanal |
| `ilk_sim_t_ns` | i64 | ns | tipin aralığı | ilk düşen iletinin zamanı |
| `son_sim_t_ns` | i64 | ns | tipin aralığı | son düşen iletinin zamanı |
| `ilk_adim` | u64 | - | tipin aralığı | ilk düşen iletinin fizik adımı |
| `son_adim` | u64 | - | tipin aralığı | son düşen iletinin fizik adımı |
| `kayip_sayisi` | u64 | - | tipin aralığı | düşen ileti sayısı |
| `neden` | u8 | - | 0=halka_tasmasi, 1=yazma_hatasi, 2=disk_dolu | neden |

### 3.5 Üst veri

Üç tür MCAP Metadata kaydı vardır; değerler metindir (MCAP Metadata `Map<string, string>`): açılış kaydı `ida_kayit` (bir tane), kısmi güncelleme kaydı `ida_kayit_guncelleme` (sıfır ya da daha çok) ve kapanış kaydı `ida_kayit_kapanis` (olağan kapanışta bir tane). Anahtar listesi `a6-kanallar.json` `x_ida_ust_veri` ile aynıdır. Şemalar fiziksel kayıtları doğrular: `$defs/ust_veri_ida_kayit` (açılış; bütün zorunlu anahtarlar), `$defs/ust_veri_ida_kayit_guncelleme` (yalnız güncellenebilir anahtarlar; kapalı liste) ve `$defs/ust_veri_ida_kayit_kapanis`. Birleştirme kuralı A6-K46'dadır.

**`ida_kayit`** — dosya açılışında, ilk iletiden önce.

| Anahtar | Zorunlu | Anlam |
|---|---|---|
| `a6_surumu` | evet | A6 sürümü, 'major.minor' |
| `a3_surumu` | evet | A3 sürümü |
| `a1_proto_surumu` | evet | A1 proto_major.proto_minor (A1'den beklenen); SIL tekrar koşusunda da |
| `a1_sema_ozeti` | evet | A1 hash_base ve hash_full (A1'den beklenen) |
| `a2_surumu` | koşullu | görselleştirici bağlandıysa |
| `a5_sema_surumu` | evet | senaryonun schema_version'i (A5) |
| `a7_manifest_ozeti` | evet | A7 manifest özeti (A7'den beklenen biçimde) |
| `a8_surumu` | koşullu | algılama bileşeni bağlandıysa |
| `run_id` | evet | çalıştırma kimliği; dosya adıyla aynı |
| `kip` | evet | `sil`, `hil_1x`, `hil_bariyer`, `tekrar_fizik`, `tekrar_girdi_sil`, `tekrar_yeniden_kurma` |
| `kaynak_run_id` | koşullu | tekrar koşusunda kaynak kaydın run_id'si |
| `guvencesiz_nedenler` | koşullu | `guvencesiz: true` ile başlatılan tekrar koşusunda sağlanmayan koşulların virgülle ayrılmış listesi (A3-K68, A6-K38) |
| `kayit_kademesi` | evet | `olaylar`, `telemetri`, `tam` |
| `kamera_kareleri` | evet | `kapali`, `pencereli`, `surekli` |
| `kademeler` | evet | `gorunum=..,kamera=..,sonar=..,kayit=..` (iskelet §6) |
| `tohum` | evet | ana tohum, onluk u64 |
| `senaryo_ozeti` | evet | `sha256:<hex>` çözümlenmiş senaryonun özeti (A5) |
| `arac_ozeti` | evet | `sha256:<hex>` araç tanım dosyasının özeti (A5) |
| `sim_surumu` | evet | sim sunucusunun sürüm kimliği (git commit ve kirli bayrağı) |
| `platform` | evet | `macos-arm64`, `windows-x64` |
| `derleyici` | evet | derleyici adı ve sürümü |
| `derleme_bayraklari` | evet | fizik yolunu etkileyen bayraklar (04 §2.13) |
| `bagimliliklar` | evet | `ad=surum` listesi: eigen, jolt, embree, mcap ve diğerleri |
| `rng_surumu` | evet | RNG ve dağılım kodu sürümü (04 §2.13, 05 §2.6) |
| `dalga_listesi_ozeti` | evet | 04 dalga listesi özeti |
| `kart_yazilimi_ozeti` | koşullu | HIL'de kartın BOOT_RECORD'undaki yazılım özeti (A1'den beklenen) |
| `kontrol_hizi_hz` | evet | 200 (iskelet §4) |
| `fizik_adimi_ns` | evet | 1250000 (iskelet §4) |
| `l_act_ns` | evet | 10000000 (iskelet §4) |
| `bariyer_bekleme_siniri_ns` | koşullu | hil_bariyer kipinde A1'de sabitlenen değer |
| `komut_kullanim_suresi_ns` | koşullu | A1'de sabitlenen değer |
| `baslangic_utc` | evet | ISO 8601 UTC başlangıç anı |
| `kayit_duvar_saati` | evet | duvar_ns alanlarının kaynağı: monoton saat, kayıt açılışından itibaren ns |

**`ida_kayit_guncelleme`** — acilista boş bırakılan bir kosullu anahtar sonradan öğrenildiğinde (A6-K46).

| Anahtar | Zorunlu | Anlam |
|---|---|---|
| `a2_surumu` | koşullu | görselleştirici bağlandıysa |
| `a8_surumu` | koşullu | algılama bileşeni bağlandıysa |
| `kart_yazilimi_ozeti` | koşullu | HIL'de kartın BOOT_RECORD'undaki yazılım özeti (A1'den beklenen) |

**`ida_kayit_kapanis`** — olağan kapanışta, son iletiden sonra.

| Anahtar | Zorunlu | Anlam |
|---|---|---|
| `bitis_durumu` | evet | `bitti`, `durduruldu`, `on_denetim_kaldi`, `hata`, `disk_dolu`, `yazma_hatasi`, `sinira_ulasti`, `sim_kapanisi` |
| `son_adim_no` | evet | son tamamlanan fizik adımı |
| `son_sim_t_ns` | evet | son sim zamanı |
| `gecerli` | evet | `true`, `false` (A3 Tablo 4.8) |
| `gercek_zamani_kacirdi` | evet | `true`, `false` |
| `gecerlilik_nedenleri` | evet | virgülle ayrılmış neden adları (A3 Tablo 4.8); yoksa boş |
| `sayaclar` | evet | `ad=deger` listesi: yuk_sayac_gercek_zaman alanlarının son değerleri |
| `kayit_eksik` | evet | `true`, `false` |
| `ilk_eksik_adim` | koşullu | `kayit_eksik` `true` ise tekrar düzeyi kanallarındaki ilk eksik adım |
| `tekrar_guvencesi` | evet | `yok`, `izleme`, `fizik`, `girdi` (A6 §4.6) |
| `guvence_on_eki_son_adim` | koşullu | güvencenin geçerli olduğu son fizik adımı (A6-K33) |
| `guvence_nedenleri` | koşullu | `tekrar_guvencesi` beklenen düzeyden düşükse nedenler, virgülle ayrılmış (örnek: `canli_mudahale`, `guvencesiz_baslatma`, `kayit_eksik`) |
| `canli_mudahale_adimi` | koşullu | tekrar koşusunda ilk canlı müdahalenin uygulandığı fizik adımı (A3-K120) |
| `ileti_sayisi` | evet | dosyaya yazılan ileti sayısı |

### 3.6 Ekler

| Ad | Ortam türü | Zorunlu | Anlam |
|---|---|---|---|
| `cozumlenmis_senaryo.toml` | `application/toml` | evet | A5 çözümlenmiş senaryo, kanonik biçim (`application/toml` IANA'da kayıtlı, 2024-10-21) |
| `arac.toml` | `application/toml` | evet | A5 araç tanım dosyası olduğu gibi |
| `a6_kanallar.json` | `application/json` | evet | yazan sürümün a6-kanallar.json dosyası, değiştirilmeden |
| `a1_dialect.xml` | `application/xml` | koşullu | A1 dialect XML'i (A1 MAVLink kullanıyorsa; A1'den beklenen) |
| `a7_manifest.json` | `application/json` | evet | A7 manifesti |
| `on_denetim_raporu.json` | `application/json` | evet | A3 ön denetim raporu (A3 $defs/on_denetim_raporu) |

---

## 4. Davranış kuralları

### 4.1 Yazma

- **A6-K01** Her canlı ve tekrar çalıştırması için bir dosya açılır (A3-K108); dosya adı `<run_id>.mcap`'tir ve aynı adlı dosya varsa yazıcı onu ezmez, çalıştırma başlamaz (ön denetim `kayit_disk_alani` maddesi `kaldi`, ileti "dosya var").
- **A6-K02** Dosya MCAP büyük sürüm 0 biçimindedir; Header `profile` alanı `ida`'dır.
- **A6-K03** Yazıcı `foxglove/mcap` C++ kitaplığıdır (Ö7, Ç11); sürümü W2'de paket yöneticisiyle sabitlenir ve `bagimliliklar` üst verisine yazılır. Yazım parçalıdır; sıkıştırma zstd'dir; parça hedef boyutu 1 MiB sıkıştırmasız veridir (kitaplığın `DefaultChunkSize` değeri; hedeftir, bir kayıt kadar aşılabilir); parça CRC'si (A6-K51), ileti indeksleri ve özet bölümü kitaplık varsayılanlarıyla açıktır. Sıkıştırmadan kazanç olmayan parçayı kitaplık sıkıştırmasız yazabilir (`forceCompression = false`, kitaplık varsayılanı).
- **A6-K04** Yazıcı açık parçayı en geç 1 s duvar saatinde bir kapatır (`McapWriter::closeLastChunk`; mimari seçimi, tahmin). Böylece çökmede kaybolan veri en çok son parçanın verisidir.
- **A6-K05** Dosyaya tek bir iş parçacığı (kayıt yazıcısı) yazar. Kitaplığın yazıcısının iş parçacığı güvenliği belgelenmemiştir (`writer.hpp`'de buna dair açıklama yok; 08 §3.5).
- **A6-K06** Üreticiler (dünya, HIL G/Ç, A3 sunucusu, A2 kare alıcısı, A8 kayıt alıcısı) veriyi yazıcıya üretici başına ayrı tek üretici–tek tüketici halka tamponlarıyla verir. Tampon doluysa üretici iletiyi atar, düşen iletinin kanalını, sim zamanını ve adımını kendi düşme sayaçlarında biriktirir ve beklemez; hiçbir üretici kayıt nedeniyle beklemez (04 §2.12; 08 §2.1).
- **A6-K07** Şema kaydının `data` alanı, yükün `a6-kanallar.json` içindeki alan listesinin anahtarları sıralanmış, boşluksuz, UTF-8 JSON metnidir; yazıcı bu metni kendi gömülü kataloğundan üretir.
- **A6-K08** İleti `data` alanı yük düzeninin A3 §3.1 tel tipleriyle kodlanmış halidir ve A3 ile aynı kodlayıcıyla üretilir; A3 çerçeve başlığı yazılmaz.
- **A6-K09** `log_time` kanalın `zaman_anlami`'na (A3-K26) göre sim zamanıdır (ns); `publish_time`, iletinin üretildiği andaki kayıt duvar saatidir (MCAP spec: ikisi de ns, çağ kullanıcı tanımlı); `sequence` dosyadaki yazım sırasıdır ve 1'den artar.
- **A6-K10** Sim zamanı çalıştırma başında 0'dır (04'ten beklenen). Yazıcı `log_time`'ı negatif olacak iletiyi yazmaz, `kayit/eksik` kaydında `yazma_hatasi` nedeniyle bildirir.
- **A6-K11** Dosyadaki ileti sırası `log_time` sırası olmak zorunda değildir (MCAP buna izin verir). Okuyucu iletileri `log_time`'a, eşitlikte `sequence`'a göre sıralar. Tekrar oynatma kararları dosya sırasına değil kayıtlardaki `adim_no`, `step_seq` ve `komut_no` alanlarına dayanır.
- **A6-K12** Bir dosya en çok 2^32 − 1 ileti taşır (`sequence` `u32`'dir). Sınıra ulaşılırsa yazıcı dosyayı `bitis_durumu = sinira_ulasti` ile kapatır, `kayit_durumu` olayı üretir ve çalıştırma kaydedilmeden sürer; tekrar güvencesi A6-K33'teki önek kuralıyla verilir.

### 4.2 Zaman alanları

- **A6-K13** `publish_time` ve yüklerdeki bütün duvar saati alanları (`duvar_ns`, `plan_duvar_ns`, `varis_duvar_ns`, `alim_duvar_ns`, `bekleme_ns`, `t0_duvar_ns`) kayıt duvar saatindedir: kayıt açılışında 0 olan, sim sunucusu sürecinin monoton saati, ns. Sistem saatinin (UTC) değeri yalnız `baslangic_utc` üst verisindedir.
- **A6-K14** `hil/zaman_esleme`, `W(t) = T0 + (t − t0)` eşlemesi (06 §2.5) kurulduğunda ve değiştiğinde, ayrıca 1 Hz'te yazılır; TIMESYNC ofseti ve aşağı yön teslim gecikmesi kestirimi aynı kayıttadır.
- **A6-K15** Bir sensör ölçümünün ölçüm zamanı (`log_time`), modellenmiş hazır ve teslim anları (`t_hazir_ns`, `t_teslim_ns`) ve fiilen girdiği A1 partisi (`parti_step_seq`) ayrı alanlardır (AB-099). Sonar satırında `log_time` darbe gönderim anıdır ve kutu zamanları nominaldir (iskelet §3.1); kutuya düşen katkıların fiziksel yansıma anları ayrı kanaldadır (`sonar_gercek/ping360`).

### 4.3 Kayıt kademesine göre kanallar

- **A6-K16** Bir kanal, Tablo 3.3'teki "Kademe" değerinde ve üstündeki kademelerde yazılır: `olaylar` kanalları her kademede, `telemetri` kanalları `telemetri` ve `tam`'da, `tam` kanalları yalnız `tam`'da. `kamera/kare` ayrıca `kamera_kareleri` `kapali` değilse yazılır.
- **A6-K17** `hil/bariyer` `olaylar` kademesinde yalnız `karar` değeri `zamaninda` olmayan partiler için, diğer kademelerde her parti için yazılır.
- **A6-K18** Kademeden bağımsız olarak her kayıtta şunlar bulunur: olaylar; A3 komutları ve nihai sonuçları; senaryo zaman çizelgesi öğelerinin uygulanması; sanal emniyet girdisi değişimleri; atış kontrol ve arıza durumu değişimleri; kaçırılan, geç bekletilen ve reddedilen her yanıt; düşürülen her veri; A1 oturum geçişleri; kayıt eksikliği bildirimleri; 1 Hz'te ve kapanışta gerçek zaman sayaçları (iskelet §4: geç kalan yanıt, kaçırılan parti ve "gerçek zamanı kaçırdı" işareti A6'ya kaydedilir).
- **A6-K19** `kamera_kareleri = surekli` her kareyi; `pencereli` yalnız bir `algilama/kayit` kaydının ya da `atis_olayi_sonucu` olayının sim zamanının ±2 s çevresindeki kareleri yazar (pencere mimari seçimi, tahmin); yazıcı bunun için son 2 s'lik kareleri bellekte tutar.
- **A6-K20** Kayıt kademesi ve `kamera_kareleri` çalıştırma boyunca değişmez.

### 4.4 Tekrar oynatılabilirlik gerekleri

İskelet A6 satırı: tekrar oynatılabilir bir kayıt, mesaj içeriğine ek olarak her komutun uygulandığı fizik adımını, bariyer kararlarını ve düşürülen verileri içerir.

- **A6-K21** Sim, sürekli komut kararı (yeni komut uygulandı, ömrü doldu ve nötre geçildi, parti kaçırıldı ve komut değişmedi), tek seferlik olay kararı, kütle durumu değişimi ya da model olayı olan her fizik adımı için bir `fizik/adim_sonucu` kaydı yazar; bunların hiçbiri olmayan adım yazılmaz. Bu kayıt 04'ün `StepResult`'ının sürümlü karşılığıdır (AB-101).
- **A6-K22** `komut_durumu = yeni_uygulandi` olan kayıttaki `pwm_us`, `gecerlilik_maskesi`, `uygulanan_step_seq` ve `omur_bitis_t_ns`, o adımda yürürlüğe giren komutun değerleridir; fizik yeniden koşturması A1 baytlarını çözmeden bu kayıtlardan yapılır.
- **A6-K23** Her bariyer kararı (iskelet §4: zamanında, geç bekletildi, kaçırıldı, yinelenen yanıt reddi, uygulama anı geçmiş yanıt reddi) `hil/bariyer` kaydıyla, partinin kapanış ve uygulama adımı, yanıtın varış zamanı ve bekleme süresiyle (gerçek zaman kayması) yazılır.
- **A6-K24** Her A3 komutu alındığı anda `a3/komut`'a, nihai sonucu uygulama adımıyla `a3/komut_sonucu`'na yazılır (A3-K52, K57).
- **A6-K25** Senaryo zaman çizelgesindeki her öğe (zamanlı ya da koşullu) uygulandığında ya da reddedildiğinde `senaryo/oge_uygulama`'ya, koşulun sağlandığı ve öğenin uygulandığı adımla yazılır (08 §2.2).
- **A6-K26** Düşürülen her veri `hil/dusen` kaydıyla, türü, ilgili parti ve sensör, adet ve nedeniyle yazılır.
- **A6-K27** A1 oturumunun her geçişi (el sıkışma sonucu, `HAZIR`, `KOŞUYOR`, `KOPUK`, yeniden başlatma isteği, planlı ve planlanmamış kart sıfırlaması, sim kapanışı) `hil/oturum` kaydıyla, oturum numarası ve kartın açılış sayacıyla yazılır (AB-096).
- **A6-K28** A1 baytları ham ve zaman damgalı kaydedilir: sim'in taşımaya verdiği her bayt bloğu (hata enjektörünün değişikliklerinden **sonraki** hali) `a1/sim_kart`'a, taşımadan aldığı her blok (hata enjektörünün değişikliklerinden **önceki** hali) `a1/kart_sim`'e, kayıt duvar saati ve sim zamanıyla yazılır. Enjektörün her işlemi ayrıca `hil/enjeksiyon`'a yazılır. A6 A1 çerçevelerini çözmez; çerçeve içeriği A1 sözleşmesindedir.
- **A6-K29** Bağlantı emülatörünün her çerçeve kararı `yer_istasyonu/emulator`'a yazılır; modellenen gecikme (emülatöre giriş ve çıkış sim zamanları) ile köprünün gerçek iletimi (A1 bloklarının duvar saati) ayrı alanlardadır (06 §2.11).

### 4.5 Eksik kayıt

- **A6-K30** Bir üreticinin halka tamponunda ileti düştüğünde yazıcı, üreticinin düşme sayaçlarındaki değişikliği gördüğü ilk fırsatta ve en geç 1 s içinde (duvar saati) her etkilenen kanal için bir `kayit/eksik` kaydı yazar (kanal, ilk ve son düşen iletinin sim zamanı ve adımı, düşen ileti sayısı, neden); `sayac/gercek_zaman.kayit_dusen` artar ve `kayit_eksik` olayı üretilir (AB-098).
- **A6-K31** Disk yazma hatasında ya da disk dolduğunda yazıcı dosyayı özetiyle kapatmayı dener, `kayit_durumu` olayını (`disk_dolu` ya da `yazma_hatasi`) üretir ve `bitis_durumu`'nu buna göre yazar; çalıştırma kayıtsız sürer.
- **A6-K32** Kayıt eksikliği çalıştırmanın `gecerli` değerini değiştirmez (kayıt koşunun fiziği değil izidir; 08 §3.5); yalnız tekrar güvencesini daraltır (A6-K33).

### 4.6 Tekrar oynatmanın üç düzeyi ve yeniden kurma

**Tablo 4.6 — Düzeyler, gerekenler ve güvenceler.**

| Düzey | En düşük kademe | Gereken kayıt | Ne yapılır | Güvence | Güvencenin koşulu |
|---|---|---|---|---|---|
| (a) `izleme` | `olaylar` | Okunabilen herhangi bir dosya | Kayıttaki ortak kanallar A3 üzerinden `kaynak_kayit` bayrağıyla yayınlanır (A3-K112); fizik koşmaz | Gösterilen her değer kayıttaki değerin aynısıdır | Yok. `olaylar` kademesinde yalnız olay zaman çizelgesi izlenir; tekrar oynatma güvencesi yoktur (iskelet A6) |
| (b) `fizik` | `telemetri` | `fizik/adim_sonucu`, `a3/komut`, `a3/komut_sonucu`, `senaryo/oge_uygulama`, `emniyet/girdiler`, `fizik/durum`; ekler `cozumlenmis_senaryo.toml`, `arac.toml`; üst veri `tohum`, sürümler | Senaryo ve tohumla dünya yeniden kurulur; kart, A1, görselleştirici ve algılama bileşeni yerine kayıtlı komut ve olay kararları, elle arızalar, emniyet değişiklikleri ve senaryo öğeleri kayıtlı adımlarda uygulanır | Kanonik fizik izi (§4.7) kayıttakiyle bit düzeyinde aynıdır | Aynı `sim_surumu`, `platform`, `derleyici`, `derleme_bayraklari`, `bagimliliklar`, `rng_surumu`; `senaryo_ozeti` ve `arac_ozeti` eşit; gereken kanallarda eksik yok (ya da önek). Farklı platformda yalnız toleranslı eşdeğerlik (Ç7; tolerans 04 §6 ve 09'da). Senaryo, tohum ya da ikili farklıysa koşu yapılır ama "tekrar" sayılmaz (08 §2.6.2) |
| (c) `girdi` | `tam`, yalnız `sil` kipindeki kayıt | `cekirdek/baslatma`, `cekirdek/cagri` (bütün init ve çağrılar, `cagri_sirasi` boşluksuz) | Tekrar hedefi PC'deki çekirdektir: iki kanalın kayıtları `cagri_sirasi` sırasıyla yürütülür; her init kaydı çekirdeği kayıttaki başlatma girdisi ve parametre bloğuyla yeniden başlatır ve init dönüş kodu da karşılaştırılır; çağrılar ait oldukları `init_kusagi` altında, duvar saatinden bağımsız koşturulur (A6-K55); fizik koşmaz | Her init ve çağrının dönüş kodu ve çıktısı kayıttakiyle bit düzeyinde aynıdır (A4-K24) | Kaynak kayıt `sil` kipinde; `cekirdek_bilgisi` (çekirdek yapı kimliği), `platform`, `derleyici`, `derleme_bayraklari` ve `senaryo_ozeti` eşit; gereken kanallarda eksik yok (ya da önek) |
| (d) `yeniden_kurma` | `tam`, `hil_1x` ya da `hil_bariyer` kaydı | `a1/sim_kart`, `a1/kart_sim` (alım damgalarıyla), `hil/oturum` ve `hil/zaman_esleme` (yalnız üst veri) | Kayıtlı sim → kart bayt blokları PC'deki ortak platform katmanı koduna verilir, bu katman çağrı girdilerini yeniden kurar ve PC'deki çekirdeği çağırır; oturum el sıkışması tekrar edilmez (A6-K56, K57) | **Güvence yoktur.** Kartın o koşudaki çıktılarıyla karşılaştırma yalnız raporlanır | — (güvence olmadığı için sürüm koşulu yoktur) |

- **A6-K33** Kapanışta yazıcı `tekrar_guvencesi`'ni şöyle belirler: düzeyin kademe, kip ve kanal gereği sağlanan en yüksek düzey (`fizik` için `telemetri`; `girdi` için `tam` ve `sil` kipi; HIL kaydı en çok `fizik` verir, `yeniden_kurma` bir güvence düzeyi değildir); `olaylar` kademesinde `izleme`; A6-K38 ve A6-K53'teki durumlarda `yok`. O düzeyin gerektirdiği bir kanalda `kayit/eksik` kaydı varsa güvence o düzeyde **önek güvencesidir**: `guvence_on_eki_son_adim` = gereken kanallardaki ilk eksik adım − 1 yazılır ve güvence yalnız 0 … bu adım için geçerlidir; ilk eksik adım 0 ise o düzey verilmez ve bir alt düzeye inilir.
- **A6-K34** Bariyer beklemesi olan ya da gerçek zamanı kaçıran bir çalıştırmanın kaydı `fizik` düzeyini korur (komutlar kayıtlı adımlarda uygulanır). HIL kaydından `girdi` güvencesi hiçbir koşulda verilmez; kartın duvar saatine bağlı davranışı (gözetim, emniyet süreleri, bağlantı zaman aşımı) için güvence yoktur (iskelet §4; Ç1).
- **A6-K35** Fizik yeniden koşturmada sim kayıtlı sürekli komutları ve tek seferlik olay kararlarını kaydedildikleri adımlarda uygular ve kararı yeniden hesaplar; yeniden hesaplanan karar kayıttakinden farklıysa kayıttaki uygulanır ve `uyari` olayı (`kod = tekrar_karar_farki`) üretilir.
- **A6-K36** Fizik yeniden koşturmada sensör modelleri ve sonar tohumdan yeniden üretilir ve hiçbir yere iletilmez; sonar çıktısının kayıttakiyle eşitliği güvence değildir (Embree'nin bit eşitliği garantisi bulunamadı, aynı ikili ve platformda eşitlik M11/M15 ile doğrulanmadı; 08 §2.6.2). Fizik sonar çıktısını tüketmediği için kanonik fizik izi bundan etkilenmez.
- **A6-K37** Tekrar koşusunun kendisi yeni bir dosyaya kaydedilir; üst veride `kip` `tekrar_fizik`, `tekrar_girdi_sil` ya da `tekrar_yeniden_kurma`, `kaynak_run_id` kaynak kaydın `run_id`'sidir. Kayıttan uygulanan her A3 komutu güncel sim sürecinden **yeni bir `komut_no`** alır ve yeni dosyada `a3/komut_sonucu`'na bu numarayla yazılır; kaynak kimliği ayrı alanlardadır: `kaynak_run_id` ve `kaynak_komut_no` (kaynak kayıttaki numara). Canlı komutlarda bu iki alan boş ve 0'dır. `komut_no` ile yapılan sorgular (A3-K60) yalnız güncel sürecin numaralarını kapsar; kaynak kayıttaki bir komut ancak (`kaynak_run_id`, `kaynak_komut_no`) çiftiyle aranır.
- **A6-K38** **Sürüm, platform ya da özet koşulu sağlanmayan tekrar isteği reddedilir** (A3-K68: 409 `tekrar_kosulu_saglanmiyor`, `ayrinti.nedenler`). Yalnız istekte açık `guvencesiz: true` varsa koşu başlar; o zaman yeni dosyanın açılış üst verisine `guvencesiz_nedenler`, kapanışına `tekrar_guvencesi = yok` ve `guvence_nedenleri = guvencesiz_baslatma` yazılır ve `uyari` olayı (`kod = tekrar_guvencesi_yok`) üretilir. `yeniden_kurma` düzeyi güvence vermediği için bu koşula bağlı değildir.

### 4.7 Kanonik işlevsel iz ve normalleştirme

- **A6-K39** Kanonik fizik izi `fizik/durum` iletilerinin bütün alanları ile `fizik/adim_sonucu` iletilerinin `olay_kimligi` dışındaki bütün alanlarından oluşur ve `adim_no` sırasıyla dizilir.
- **A6-K40** `girdi` tekrarında karşılaştırma, her çekirdek çağrısının `donus_kodu` ve `cikti` alanlarının `cagri_sirasi` sırasıyla bayt eşitliğidir. `yeniden_kurma` tekrarında kart çıktısı izi `a1/kart_sim` bloklarından A1 ayrıştırıcısıyla çözülen işlevsel alanlardan oluşur, yanıtın parti sıra numarasıyla (`step_seq`) eşlenir ve yalnız raporlanır; hangi alanların işlevsel, hangilerinin tanısal olduğu A1'den beklenir (06 §2.13'teki liste başlangıç önerisidir).
- **A6-K41** Olay kimliği normalleştirilir: her ham `olay_kimligi`, çalıştırma içinde ilk görüldüğü sırayla 1'den numaralanır (`normallestirilmis_sira`); yinelenen kimlik aynı sırayı alır. Karşılaştırma ham kimliği değil bu sırayı kullanır; böylece kartın açılış sayacı farklı iki HIL çalıştırması karşılaştırılabilir (AB-100).
- **A6-K42** `normallestirilmis_sira`'yı dünya atar ve hem `fizik/adim_sonucu`'na hem A3 `atis_olayi_sonucu` olayına aynı değerle yazar.
- **A6-K43** Kayıt duvar saati alanları, `publish_time`, `sequence`, oturum numarası (`sess`), açılış sayacı ve kartın tanısal alanları (ör. `adim_suresi_us`) kanonik ize girmez.

### 4.8 Kapanış

- **A6-K44** Olağan kapanışta yazıcı bekleyen iletileri yazar, son `sayac/gercek_zaman` iletisini yazar, `ida_kayit_kapanis` üst verisini yazar ve MCAP dosyasını kitaplığın `close` çağrısıyla (Data End, özet, Footer, sondaki sihirli baytlar) kapatır.
- **A6-K45** `bitis_durumu` çalıştırmanın son durumudur ve şu değerlerden biridir: `bitti`, `durduruldu` (`durdur` ya da `sifirla` ile; A3-K67), `on_denetim_kaldi` (çalıştırma ön denetimde kaldı; A3-K62), `hata`, `disk_dolu` ya da `yazma_hatasi` (A6-K31), `sinira_ulasti` (A6-K12) ya da `sim_kapanisi` (canlı çalıştırma sürerken sim sunucusu olağan kapandı). Liste `x_ida_ust_veri` kataloğundakiyle aynıdır.
- **A6-K46** `ida_kayit` açılış kaydı dosya açılışında, ilk kanal iletisinden önce bir kez yazılır, bütün zorunlu anahtarları taşır ve `$defs/ust_veri_ida_kayit`'e uyar; açılışta değeri bilinmeyen koşullu anahtar yazılmaz ya da boş metinle yazılır. Sonradan öğrenilen değerler `ida_kayit_guncelleme` kısmi güncelleme kaydıyla yazılır; bu kayıt yalnız kapalı listedeki anahtarları (`kart_yazilimi_ozeti`, `a2_surumu`, `a8_surumu`) taşıyabilir ve `$defs/ust_veri_ida_kayit_guncelleme`'ye uyar. **Birleştirme:** okuyucu açılış kaydından başlar ve güncelleme kayıtlarını dosya sırasıyla uygular; bir güncelleme yalnız açılışta yok ya da boş olan anahtarı doldurur. Dolu bir anahtarı (açılışta ya da önceki bir güncellemede doldurulmuş) farklı bir değerle yeniden yazan güncelleme dosyayı `bozuk` yapar (A6-K59'daki sonuçlarla); aynı değerle yeniden yazan güncelleme etkisizdir. İkinci bir `ida_kayit` açılış kaydı da dosyayı `bozuk` yapar.

### 4.9 Sürüm uyumluluğu

- **A6-K47** `a6_surumu` `büyük.küçük` biçimindedir; bu sözleşme `1.0`'dır. Küçük artış yalnız yeni kanal, var olan yükün sonuna alan (o kanalın `kanal_surumu` bir artar), yeni üst veri anahtarı ya da yeni ek ekleyebilir; başka her değişiklik büyük artıştır. A3 ile ortak kanallarda `kanal_surumu` A3 ile birlikte değişir.
- **A6-K48** Okuyucu aynı büyük sürümdeki dosyayı okur; bilinmeyen kanalı atlar; bildiğinden büyük `kanal_surumu`'lu iletide bildiği alanları okur ve sondaki fazla baytları yok sayar, küçük `kanal_surumu`'lu iletide eksik sondaki alanları "yok" sayar (A3-K19 ile aynı kural). Farklı büyük sürümdeki dosyayı okumaz (`kayit_okunamadi`).
- **A6-K49** Okuyucu, dosyadaki şema `data`'sının kendi bildiği aynı `kanal_surumu`'lu yük tanımıyla aynı olmadığını görürse o kanalı okumaz ve uyarı verir (sürüm kuralı ihlali).
- **A6-K50** Her dosya, kendisini yazan sürümün `a6-kanallar.json` dosyasını ek olarak taşır; dışa aktarıcı yeni bir okuyucu olmadan eski dosyayı bu ekle çözebilir.

### 4.10 Bütünlük, kurtarma ve girdi tekrarının sınırı (düzeltme turu 1)

- **A6-K51** Yazıcı her parçanın `uncompressed_crc` alanını (MCAP spesifikasyonundaki CRC-32, parçanın sıkıştırmasız kayıt verisi üzerinden) ve her ekin `crc` alanını hesaplar ve yazar; CRC hesaplamasını kapatan kitaplık seçenekleri (`noChunkCRC`, `noAttachmentCRC`) kullanılmaz.
- **A6-K52** `cekirdek/baslatma` ve `cekirdek/cagri` yüklerindeki A4 yapıları bellekteki bayt düzeniyle yazılmaz (A4-K03); A4 §3'teki alan sırasıyla alan alan kodlanır: A3 §3.1 tel tipleri, `bool` için `u8` (0/1), sabit boyutlu diziler kapasiteleri kadar tam (kullanılmayan öğeler sıfır), işaretçi alanı (`ping360`) önce `u8` varlık bayrağı ve varsa ardından gösterdiği yapı, iç içe yapılar sırayla, doldurma yok. Yerleşimin sürümü `cekirdek/baslatma`'daki `port_major`/`port_minor`'dur; kodlayıcı ve çözücü `core/include/ida/port.h`'den üretilir (uygulama işi).
- **A6-K53** Tekrar koşusunda A3-K120'ye göre kabul edilen ilk canlı müdahalenin uygulandığı adım kapanış üst verisine `canli_mudahale_adimi` olarak yazılır; dosyanın `tekrar_guvencesi` `yok`, `guvence_nedenleri` `canli_mudahale` olur. Kayıtlı girdiyle beslenen kiplerde müdahale reddedildiği için bu kural yalnız `tekrar_fizik` koşularına uygulanır.
- **A6-K54** Çekirdeği PC'de çağıran kiplerde (`sil`, `tekrar_girdi_sil`, `tekrar_yeniden_kurma`) ve `tam` kademede, çekirdeği çağıran platform katmanı her `ida_core_init` çağrısını `cekirdek/baslatma`'ya (çekirdek yapı kimliği olarak `ida_core_info_t`, başlatma girdisi, parametre bloğu, dönüş kodu) ve her `ida_core_step`, `ida_core_tick` ve `ida_core_background` çağrısını `cekirdek/cagri`'ya (işlev türü, çekirdeğe verdiği girdinin tamamı, dönüş kodu, çıktı) yazar. **`cagri_sirasi`** çalıştırma genelinde tekdüzedir: çalıştırmanın ilk çekirdek çağrısında 1'dir, init dahil her çağrıda bir artar ve init'te sıfırlanmaz; iki kanal birlikte boşluksuz bir sıra oluşturur. **`init_kusagi`** çalıştırmanın ilk init'inde 1'dir, her init'te bir artar; her çağrı kaydı ait olduğu kuşağı taşır. Çekirdeğin gördüğü bütün zaman değerleri (`now_w`, zaman çerçevesi, alım zamanları) ve `bg_slices` girdinin parçasıdır. Kaydı platform katmanı çağrının kendisinden üretir; A1 baytlarından türetmez.
- **A6-K55** `girdi` tekrarında tekrar sürücüsü önce ilk `cekirdek/baslatma` kaydındaki `cekirdek_bilgisi`'ni kendi çekirdeğinin `ida_core_info` çıktısıyla karşılaştırır; farklıysa A6-K38 uygulanır. Sonra `cekirdek/baslatma` ve `cekirdek/cagri` kayıtlarını birlikte, `cagri_sirasi` sırasıyla yürütür: bir init kaydında çekirdeği kayıttaki başlatma girdisi ve parametre bloğuyla yeniden başlatır (`ida_core_init`) ve init'in dönüş kodunu kayıttakiyle karşılaştırır; bir çağrı kaydında, kaydın `init_kusagi`'nin son yürütülen init'inkine eşit olduğunu denetler ve çağrıyı kayıttaki girdiyle, duvar saatinden ve gerçek zamandan bağımsız olarak yapar; dönüş kodunu ve çıktıyı kayıttakiyle bayt bayt karşılaştırır. `cagri_sirasi`'nda boşluk, kuşak uyuşmazlığı ya da ilk karşılaştırma farkı tekrarı durdurur ve ilgili sıra raporlanır. Bit eşitliği güvencesi yalnız kaynak kayıt `sil` kipindeyse ve Tablo 4.6'daki koşul sağlanıyorsa verilir.
- **A6-K56** HIL kaydından (`hil_1x`, `hil_bariyer`) yapılan tekrar **yeniden kurma tekrarıdır** (`tekrar_yeniden_kurma`). Kartın içindeki çağrı girdileri PC'de görünmediği için tekrar sürücüsü kayıttaki `a1/sim_kart` bloklarını kayıt duvar saati damgalarıyla PC'deki ortak platform katmanı koduna (A1 ↔ A4 dönüştürücüsü, `protocol/ida_link`) verir; bu katman çağrı girdilerini yeniden kurar ve PC'deki çekirdeği çağırır (kurulan çağrılar `cekirdek/*` kanallarına yeni dosyada yazılır). Kartın gerçek alım zamanları ve arka plan dilimleri bilinmediğinden kurulan girdiler kartınkilerden farklı olabilir; kartın o koşudaki çıktılarıyla bit eşitliği güvencesi verilmez, karşılaştırma (A6-K40) yalnız raporlanır. HIL'de `bg_slices`'ın PC'ye taşınması gerekmez.
- **A6-K57** Yeniden kurma tekrarında A1 oturum el sıkışması tekrar edilmez: hedef yeni bir kart oturumu değil PC'deki çekirdektir. Kayıtlı `HELLO`, `HELLO_ACK` ve `hil/oturum` kayıtları yalnız üst veri olarak okunur; platform katmanı kayıttaki ilk `KOŞUYOR` geçişinden itibaren oturumu kurulmuş sayar (A1'den beklenen: el sıkışmasız başlatma kipi).
- **A6-K58** Kayıtlı bayt akışının gerçek karta yeniden verilmesi (06 §2.9 ve 08 §2.6.2'deki "karta besleme" tanımı) 1.0'da desteklenmez; A3'te bunun için bir tekrar düzeyi yoktur (8. başlık, açık).
- **A6-K59** A6 okuyucusu (izleme, tekrar sürücüsü, kayıt listesi ve dışa aktarıcı) her parçayı açarken sıkıştırmasız verinin CRC-32'sini kendisi hesaplar ve `uncompressed_crc` ile karşılaştırır; MCAP kitaplığının bunu yaptığını varsaymaz (`foxglove/mcap` C++ okuyucusu `uncompressedCrc`'yi ayrıştırır ama doğrulamaz: `cpp/mcap/include/mcap/reader.inl`, `main` dalı, 2026-11-20). A6-K51 gereği `uncompressed_crc = 0` değeri de "doğrulama yok" sayılmaz, hesaplanan değerle karşılaştırılır. CRC'si tutmayan, açılamayan ya da çözülemeyen parçadan hiçbir ileti yayınlanmaz ve tekrar sürücüsüne verilmez; dosya `bozuk` işaretlenir; o dosyadan hiçbir tekrar güvencesi (`fizik`, `girdi`) verilmez ve tekrar isteği 422 `kayit_okunamadi` ile reddedilir; izleme yalnız ilk bozuk parçadan önceki parçalarla yapılır. Eklerin `crc`'si de aynı biçimde doğrulanır; tutmayan ek kullanılmaz ve dosya `bozuk` işaretlenir.
- **A6-K60** Footer'ı, özeti ya da Data End kaydı okunamayan (kapanmamış) dosya A6'nın **kurtarma okumasıyla** okunur; kitaplığın özetsiz tarama yoluna (`AllowFallbackScan`) bırakılmaz (o yol Footer bulunmayan dosyada veri sonunu dosya boyu eksi Footer boyu varsayar ve son tam parçanın bir kısmını dışarıda bırakabilir; `reader.inl`, aynı kaynak). Kurtarma okuması: baştaki sihirli baytları ve Header'ı doğrular; dosyanın başından fiziksel dosya sonuna kadar kayıtları sırayla (`opcode` `u8` + uzunluk `u64` + gövde) okur; gövdesi dosya sonunu aşan kayıtta durur; yalnız tam kayıtları kabul eder; her Chunk'ı A6-K59'a göre doğrular ve ilk CRC hatasında durur. **Kurtarma sonu** son tam ve CRC'si tutan parçanın sonudur: o noktaya kadarki parçaların iletileri ile o noktaya kadarki tam Schema, Channel, Metadata ve (CRC'si tutan) Attachment kayıtları kullanılır; sonrasındaki hiçbir ileti kullanılmaz. Kurtarılan dosya `kapanmis: false` olarak listelenir ve yalnız `izleme` verir (kapanış üst verisi ve eksiklik bildirimi olmadan önek belirlenemez).

---

## 5. Hata durumları

| Durum | Algılama | Yazıcının davranışı | Okuyucunun davranışı ve güvence |
|---|---|---|---|
| Halka tamponu taştı | Üretici düşme sayacı | `kayit/eksik`, `kayit_eksik` olayı, `kayit_dusen` (A6-K30) | Önek güvencesi (A6-K33) |
| Disk dolu ya da yazma hatası | Yazma çağrısı başarısız | Kapatmayı dener, `bitis_durumu` (A6-K31) | Kapanmışsa önek güvencesi; kapanmamışsa aşağıdaki satır |
| Başta sihirli baytlar yok ya da Header okunamıyor | Açılış | — | Okunmaz; `kayit_okunamadi`; listede `kapanmis: false`, güvence yok (A3-K110) |
| Dosya kapanmamış (sim çöktü, Footer, özet ya da Data End yok) | Footer/özet okunamıyor | — | Kurtarma okuması (A6-K60): son tam ve CRC'si tutan parçaya kadar `izleme`; `fizik` ve `girdi` güvencesi verilmez |
| Parçada ya da ekte CRC hatası; açılamayan ya da çözülemeyen parça | A6 okuyucusunun kendi CRC hesabı (A6-K59); kitaplığa bırakılmaz | — | O parçadan veri yayınlanmaz; dosya `bozuk`; tekrar güvencesi yok, tekrar isteği 422 `kayit_okunamadi`; izleme yalnız ilk bozuk parçadan öncesiyle |
| `ida_kayit` gerekli anahtarı eksik | Üst veri denetimi | — | Yalnız `izleme` |
| Farklı büyük `a6_surumu` | Üst veri | — | Okunmaz (A6-K48) |
| Şema `data`'sı bilinen tanımla aynı değil | A6-K49 | — | Kanal okunmaz, uyarı |
| `log_time` negatif olacak ileti | Yazma öncesi | Yazmaz, `kayit/eksik` (A6-K10) | Önek güvencesi |
| İleti sayısı sınırı | `sequence` taşacak | `sinira_ulasti` ile kapatır (A6-K12) | Önek güvencesi |
| Aynı adlı dosya var | Açılış | Ezmez; çalıştırma başlamaz (A6-K01) | — |
| Tekrar isteğinde sürüm, platform ya da özet farkı | Üst veri ve `cekirdek_bilgisi` karşılaştırması | `guvencesiz: true` yoksa istek reddedilir; varsa yeni dosyada güvence yok ve nedenler (A6-K38) | Koşu "tekrar" sayılmaz |
| Tekrar koşusunda canlı müdahale | A3-K120 | `canli_mudahale_adimi`, güvence yok (A6-K53) | Koşu yeni deney sayılır |
| HIL kaydından girdi tekrarı istendi | Kip `hil_*` | — | `girdi` güvencesi verilmez; yalnız `yeniden_kurma` (A6-K34, K56) |

## 6. Kalite kademelerinin etkisi

Kayıt kademesi iskelet §6'daki kayıt satırıdır ve kullanıcı seçimidir (A5; A3-K109 ile çalıştırma başında geçersiz kılınabilir). Hangi kanalın hangi kademede yazıldığı Tablo 3.3'tedir; tekrar düzeyleri Tablo 4.6'dadır.

Veri hızları (hesap, sıkıştırmasız, MCAP kayıt ek yükü hariç; **ölçülmedi**; sıkıştırma etkisi bilinmiyor):

| Kaynak | Kademe | Hesap |
|---|---|---|
| `a1/kart_sim` ya da `a1/sim_kart` | `telemetri` / `tam` | Yön başına en çok 300 000 B/s (3 000 000 baud 8N1'in üst sınırı; iskelet §3); sözleşme yükü periyot başına 900 B'nin altında tuttuğu için olağan yük ≤ 180 000 B/s (900 B × 200 Hz) |
| `fizik/durum` | `telemetri` | 4 gövde ve 8 iticiyle ileti 647 B; 200 Hz'te ≈ 129 kB/s (≈ 466 MB/saat) |
| `kamera/kare` | `tam` + `surekli` | Tam kamera kademesinde 640 × 480 × 3 B (RGB8; piksel biçimi ve kademe değerleri A2'de: tam 640 × 480 / 30 Hz, sade 320 × 240 / 15 Hz, tahmin) × 30 Hz ≈ 27,6 MB/s (≈ 99,5 GB/saat); sade kademede ≈ 3,46 MB/s |
| `cekirdek/cagri` | `tam` (SIL) | Ölçülmedi; çağrı başına kodlanmış girdi ve çıktı boyu A4 yapıları kodlanınca belli olur (8. başlık md.9) |

Bu yüzden `kamera_kareleri` varsayılanı `kapali`'dır (A5'ten beklenen) ve `surekli` yalnız kısa deneyler içindir. Düşük donanımlı Windows bilgisayarında hangi kademenin taşınabildiği ölçülmedi (MP8; 8. başlık md.1).

Emniyet kabulü kademeden bağımsızdır (iskelet §6): emniyet girdisi değişimleri, atış olayı kararları, A3 komutları ve bariyer anomalileri her kademede kaydedilir (A6-K18).

## 7. Uygunluk

| Kurallar | Test türü | Sınama yöntemi ve gözlenecek büyüklük |
|---|---|---|
| K01–K05, K51 | birim | Üretilen dosyayı kitaplıktan bağımsız bir MCAP okuyucusuyla açma (sihirli baytlar, Header `profile`, parça sıkıştırması, özet); 1 s'den kısa aralıkla parça kapanması; aynı adlı dosyanın ezilmemesi; her parçada ve ekte CRC'nin yazılmış ve doğru olması |
| K06, K30–K32 | SIL | Yapay yavaş disk ya da küçültülmüş halka tamponuyla: dünyanın adım süresinin kayıtsız koşudan ayırt edilemezliği (eşik ölçümle), `kayit/eksik` kayıtlarının düşen iletilerle sayıca ve aralıkça eşleşmesi, `gecerli`'nin değişmemesi |
| K07–K12 | birim | Şema `data`'sının katalogdan üretilenle bayt eşitliği; C++ kodlayıcı ile dışa aktarıcının çözücüsünün altın iletilerde gidiş-dönüşü; `log_time`/`publish_time`/`sequence` kuralları; karışık dosya sırasında okuyucunun sıralaması |
| K13–K15 | SIL | Duvar saati alanlarının tek monoton kaynaktan geldiği (kayıt açılışında ≈ 0); sensör kaydında `t_teslim_ns` ile `parti_step_seq`'in ayrı ve tutarlı olması; Ping360 satırında `log_time` = `t_tx_ns` |
| K16–K20 | SIL | Üç kademede ve üç `kamera_kareleri` değerinde dosyadaki kanal kümesinin Tablo 3.3 ile birebir eşleşmesi; `olaylar` kademesinde `hil/bariyer`'de yalnız anomalilerin bulunması |
| K21–K29 | SIL + kartlı | Hata enjektörüyle geciktirilen ve yinelenen yanıtlar, kaçırılan partiler, düşürülen profiller, kart sıfırlaması: her olayın ilgili kanalda doğru adım ve nedenle bulunması; A1 bloklarının taşımaya verilen ve alınan baytlarla eşitliği |
| K33–K38 | SIL | Aynı ikili ve platformda iki SIL koşusu ile kayıttan `fizik` tekrarı: kanonik fizik izinin bit düzeyinde eşitliği; `telemetri` kaydından `girdi` tekrarı isteğinin reddi; önek güvencesi olan kayıtta tekrarın önek adımında durması; farklı `sim_surumu` ile güvencenin "yok" yazılması |
| K33–K38, K56, K57 | kartlı | Bariyerli HIL kaydından `fizik` tekrarı: kanonik fizik izinin eşitliği; `tam` HIL kaydından `yeniden_kurma`: koşunun el sıkışmasız başlaması, kart çıktısıyla karşılaştırma raporunun üretilmesi ve kayda güvence yazılmaması; aynı kayıttan `girdi` isteğinin reddi |
| K39–K43 | birim + kartlı | Açılış sayacı farklı iki HIL koşusunda normalleştirilmiş olay sırasının eşleşmesi; duvar saati alanlarının ize girmemesi |
| K44–K46 | birim | Olağan kapanışta kapanış üst verisinin ve özetin bulunması |
| K54, K55 | SIL | Aynı yapı ve platformda SIL koşusunun `tam` kaydından `girdi` tekrarı: her init ve çağrının dönüş kodu ve çıktısının bayt eşitliği; `bg_slices` ve background çağrılarının kayıtlı sayı ve `now_w` ile yinelenmesi; çalıştırma ortasında yeniden init içeren kayıtta (`init A → çağrı → init B → çağrı`) `cagri_sirasi`'nın init'te sıfırlanmaması, `init_kusagi`'nin 1 → 2 olması ve tekrarın B'yi doğru yerde yeniden başlatması; `cagri_sirasi` boşluğunda ve kuşak uyuşmazlığında tekrarın durması; farklı `cekirdek_bilgisi` ile isteğin reddi |
| K37 | SIL | Kayıttan tekrar koşusunda `a3/komut_sonucu`'ndaki `komut_no`'ların güncel süreçten yeni numaralar olması, `kaynak_run_id`/`kaynak_komut_no`'nun kaynak kayıttakilerle eşleşmesi ve aynı süreçteki canlı bir komutun numarasıyla çakışmaması |
| K46 | birim | Açılışta boş bırakılan `kart_yazilimi_ozeti`'nin güncelleme kaydıyla doldurulması; kapalı liste dışında anahtar taşıyan güncellemenin şemadan geçmemesi; dolu anahtarı farklı değerle değiştiren güncellemenin dosyayı `bozuk` yapması; ikinci açılış kaydının dosyayı `bozuk` yapması |
| K59 | birim | Sıkıştırmasız yazılmış bir dosyada bir parçanın içindeki tek bir ileti baytı değiştirilir (yapı geçerli kalır): okuyucunun dosyayı `bozuk` işaretlemesi, o parçadan hiçbir iletinin yayınlanmaması, tekrar isteğinin reddi; ek CRC'si bozulmuş dosyada aynısı; zstd parçası bozulduğunda aynısı |
| K60 | birim | Üç ayrı durum: (1) **kesik indeks** — dosya Data End'den sonra, özet bölümünün ortasında kesilir: kurtarma bütün parçaları verir; (2) **kesik parça** — dosya bir parçanın ortasında kesilir: kurtarma o parçadan önceki son tam parçada biter, kesik parçadan ileti gelmez; (3) **tam parça sonunda çökme** — dosya bir parçanın son baytında biter (Data End ve Footer yok): kurtarma o parça dahil bütün parçaları verir. Her durumda kurtarılan ileti sayısının beklenenle eşitliği, `kapanmis: false` ve güvencenin `izleme` olması |
| K47–K50 | birim | Sonuna alan eklenmiş (`kanal_surumu` + 1) iletinin eski okuyucuda okunması; farklı büyük sürümün reddi; şema `data` uyuşmazlığında kanalın okunmaması; ekteki katalogla dışa aktarım |

**A3 ile ortak kanalların eşitliği** (görev belgesinin istediği karşılaştırma): `docs/sozlesmeler/sema/a6-kanallar.json` içinde `a3_ortak: true` olan her kanal için `a3-kanallar.json`'da aynı adlı kanal bulunur; ikisinde `yuk`, `kanal_surumu` ve `zaman_anlami` aynıdır ve yük tanımları (alan adı, tip, adet, birim, aralık, değer listesi, yok değeri, sıra) iki dosyada derin eşittir. Bu işte yapılan denetimin sonucu 7.1'dedir.

### 7.1 Bu işte yapılan şema denetimi

Yöntem: kurulu bir JSON Schema doğrulayıcısı yoktu; Python 3.12 standart kitaplığıyla yazılmış bir yapısal denetim betiği kullanıldı (betik depoya girmez). Betik, bilerek bozulmuş kopyalarda yapılan bozulmaların hepsini yakaladı: A6 tarafında bir ortak yük alanının tipi değiştirildi, bir `$ref` kırıldı, bir `kanal_id` yinelendi (2026-11-19); ek nesnenin tamsayı değerinin üst sınırı silindi ve betik sınırsız sayısal alanı yakaladı (2026-11-20, düzeltme turu 2). Şemaların JSON Schema 2020-12 meta-şemasına karşı resmî bir doğrulayıcıyla denetimi yapılmadı (doğrulanmadı). Sonuç (2026-11-20):

- İki dosya geçerli UTF-8 ve geçerli JSON (Python 3.12 `json`).
- `$defs` altındaki şemalar: a3'te 88, a6'da 3 tanım; kullanılan anahtar sözcükler JSON Schema 2020-12 sözlüğünde, bütün `$ref`'ler dosya içinde çözüldü, `pattern`'ler derlendi.
- A3: 24 kanal (kimlik ve ad tekil; yükler, anahtar alanları, sınıf, hız ve kuyruk kuralları tutarlı), 23 yük, 23 olay türü (her birinin ayrıntı şeması var), 46 uç (yinelenen yok; durum değiştiren her uçta `istek_kimligi` zorunlu; yolunda `ates`/`fire`/`tetik` geçen uç yok), 29 hata kodu; çerçeve başlığı 24 bayt.
- A6: 54 kanal (ad tekil; yük, kademe ve tekrar düzeyi değerleri geçerli; `fizik` düzeyinin gerektirdiği hiçbir kanal `tam` kademesinde değil), 36 yük.
- Ortak kanal karşılaştırması: A6'da `a3_ortak: true` olan 20 kanalın 20'i A3'te aynı adla, aynı `yuk`, `kanal_surumu` ve `zaman_anlami` ile ve derin eşit yük tanımıyla bulundu. İki dosyada aynı adı taşıyan 19 yükün hepsi derin eşit. A3'e özgü (kaydedilmeyen) kanallar: `baglanti/durum`, `kayit/durum`, `surec/durum`, `yer_istasyonu/mavlink`.
- Hesap denetimi: `yuk_fizik_durum` 4 gövde ve 8 iticiyle 647 bayt (A6 §6'daki hesap).
- Olay ayrıntısı boyutu (A3-K121 politikası: boşluksuz JSON, ASCII dışı ham UTF-8, `\u` kaçışı yok, denetim karakteri yasak): her sayısal alan sınırlı, her dize ve dizi sınırlı; 23 olay türünün her biri için şemanın izin verdiği en kötü örnek üretildi, serileştirilmiş boyu hesapla aynı ve 8192 baytın içinde; en büyüğü `komut_sonucu` 4966 bayt, `on_denetim_sonucu` 2104 bayt.
- Belge ↔ şema eşleşmesi: iki belgede ters tırnakla geçen her kanal adı, yük adı, `$defs` adı ve HTTP koduyla verilen hata kodu şemalarda var; şemalardaki 58 kanalın ve 40 yükün her biri belgelerde geçiyor.

## 8. Açık kalanlar

1. **Kayıt yazıcı verimi ve taşma eşiği** (ölçülecek): zstd sıkıştırmanın işlemci maliyeti, `telemetri` ve `tam` kademelerinin sürdürülebilir veri hızı, halka tamponu boyutları. 00 belgesinde M numarası yok (08 §8 11.1 md.4 önerisi); MP8 (Windows) ve W5 kayıt işinde ölçülür.
2. **Kamera kare kaydı:** sıkıştırma yöntemi ve maliyeti ölçülmedi (M30 ile); `pencereli` kipin ±2 s penceresi tahmin.
3. **Parça kapatma aralığı 1 s** (A6-K04): tahmin; küçük parçaların sıkıştırma oranına etkisi ölçülmedi.
4. **mcap kitaplığının sürümü:** `main` dalındaki `types.hpp` `MCAP_LIBRARY_VERSION` değeri `2.2.0`; yayımlanmış sürüm etiketiyle eşleştiği ve paket yöneticisinde bulunduğu doğrulanmadı; W2'de sabitlenir.
5. ~~`application/toml` ortam türü~~ — **kapandı:** IANA'da kayıtlı (https://www.iana.org/assignments/media-types/application/toml, kayıt 2024-10-21).
6. **`sonar_gercek/ping360` katkı sayısı (64):** tahmin; 05'in sonar uygulaması (W4) katkıları bu biçimde verebiliyor mu doğrulanmadı.
7. **`fizik/durum` kayıt hızı 200 Hz** (kontrol periyodu sınırı): mimari seçimi; fizik adımı hızında (800 Hz) kayıt gerekip gerekmediği tolerans karşılaştırmasıyla (Ç7) değerlendirilir.
8. **Kapanmamış dosyada önek güvencesi:** 1.0'da verilmez (§5, A6-K60); periyodik "güvence işareti" kaydıyla önek tanımlanabilir (küçük sürüm adayı).
9. **`cekirdek/cagri` boyutları ve veri hızı:** `girdi` ve `cikti` için 256 KiB üst sınırı ve `baslatma_girdisi` için 1 KiB tahmindir; A4 yapılarının alan alan kodlanmış boyu kodlayıcı üretilince (W2/W3) kesinleşir; `tam` kademede SIL kaydının veri hızı ölçülmedi.
10. **Kayıtlı bayt akışının gerçek karta yeniden verilmesi** (A6-K58): **açık**; 1.0'da desteklenmez. Gerekirse kartın oturum ve zaman davranışını (el sıkışma, `HELLO_ACK` nonce'u, alım zamanları, sessiz aralıklar) A1 ve A4 ile birlikte tanımlayan ayrı bir iş olarak ele alınır; o zamana kadar karttan bit eşitliği güvencesi yoktur.

## 9. Açık bulgu kapanış tablosu

`docs/mimari/acik-bulgular.md` "A6 — Kayıt" başlığındaki bulguların tamamı ve uyumsuzluk tablosundaki A6 satırı:

| Bulgu | Durum | Sözleşmede nerede | Gerekçe |
|---|---|---|---|
| AB-096 | kapandı | A6-K21, K23, K27; `hil/oturum`, `fizik/adim_sonucu` | Oturum geçişleri, kart sıfırlamaları ve komut ömrü kararları (`omur_doldu_notr`, `omur_bitis_t_ns`) kayda bağlandı; değerlerin kendisi (kullanım süresi, zaman aşımı) A1'den gelir ve üst veriye yazılır. |
| AB-097 | kapandı | A6-K54, K55, K56; Tablo 4.6 (c), (d) | SIL'de platform katmanı her çekirdek çağrısını (`bg_slices`, background çağrıları ve `now_w` dahil) girdisi, dönüş kodu ve çıktısıyla kaydeder; arka plan işinin tüketildi/düştü kararı çıktının parçası olarak karşılaştırılır ve tekrarda aynı çağrı dizisiyle yeniden üretilir. HIL'de bu girdiler PC'de görünmez; HIL kaydı yalnız güvencesiz yeniden kurma verir ve bu açıkça yazılıdır (mimar kararı, 2026-11-20). |
| AB-098 | kapandı | A6-K30, K33; `kayit/eksik` | Taşmada hangi kanalın hangi adım aralığında eksik olduğu kaydediliyor ve güvence önek olarak daraltılıyor. |
| AB-099 | kapandı | A6-K15; `yuk_*` ölçüm başlığı; `sonar_gercek/ping360` | Nominal kutu zamanı (`log_time` + kutu genişliği) ile gerçek yansıma anları, modellenmiş teslim (`t_teslim_ns`) ile fiilî parti (`parti_step_seq`) ayrı alanlarda. |
| AB-100 | kapandı | A6-K39…K43 | Olay kimliği normalleştirmesi (ilk görülme sırası) ve `step_seq` ile girdi bağlama tanımlandı; işlevsel/tanısal A1 alan listesi A1'den beklenir. |
| AB-101 | kapandı | A6-K21, K22; `yuk_fizik_adim_sonucu` | `StepResult`'ın kayıt karşılığı sürümlü (`kanal_surumu`) ve dondurulabilir yük olarak tanımlandı. |
| U-28 | kapandı | 1. başlık; Tablo 3.3; Tablo 4.6 | A6 kapsamı "A1 trafiği, A3 komutları ve olayları, fizik durumu" oldu (iskelet 0.8); A3 telemetrisi kaydedilmez. `a1/sim_kart` yalnız `tam`'da yazılır; A1 bayt kanalları (`a1/sim_kart`, `a1/kart_sim`) ve `hil/enjeksiyon` güvencesiz `yeniden_kurma` düzeyinin girdisidir; güvenceli `girdi` düzeyi yalnız `cekirdek/*` kayıtlarına dayanır. Böylece 06 §2.13'ün "bütün A1 çerçeveleri" gereği yalnız HIL yeniden kurmaya bağlandı. Tablo 3.3 ve şemadaki `tekrar_duzeyi` bu ayrımla eşittir. |

Açık bırakılan A6 bulgusu yoktur.

Bilgi için: AB-022 (A1 başlığında, "A6 yan verisi" notlu) A6 tarafında A6-K15 ve `sonar_gercek/ping360` ile karşılanır; bulgunun kendisi A1'dedir.

## 10. Başka sözleşmelerden beklenenler

**A1'den beklenen:**
1. A1 sürüm alanları (`proto_major`, `proto_minor`, `hash_base`, `hash_full`) ve dialect XML'i (`a1_dialect.xml` eki; MAVLink kullanılıyorsa).
2. Tek seferlik olay kimliğinin bit düzeni (açılış sayacı + olay sayacı) ve `u64`'e sığması; olay türü kodları.
3. Kanonik iz için A1 alanlarının işlevsel/tanısal sınıflaması (A6-K40).
4. Bekleme sınırı ve sürekli komut kullanım süresi değerleri (üst veri anahtarları).
5. Kart yazılımı özetinin (`BOOT_RECORD`) biçimi.
6. HIL'de `bg_slices`'ın ve kart içi çağrı girdilerinin PC'ye taşınması **gerekmez** (HIL kaydından girdi güvencesi verilmez; A6-K56).
7. Yeniden kurma tekrarı için `ida_link`'in el sıkışmasız başlatma kipi: kayıtlı ilk `KOŞUYOR` geçişinden itibaren oturumu kurulmuş sayan, nonce denetimi yapmayan PC'ye özgü bir giriş (A6-K57).

**A4'ten beklenen:**
1. SIL platform katmanının (SIL koşucusu) her çekirdek çağrısının girdisini, dönüş kodunu ve çıktısını, `ida_core_init` girdisini ve `ida_core_info` çıktısını A6 kayıt yazıcısına vermesi (A6-K54).
2. `ida_init_in_t`, `ida_in_t`, `ida_out_t` ve `ida_core_info_t` için alan sırasının A4 §3 tablolarındaki gibi sabit olması; A6 bu sırayla alan alan kodlar (A6-K52; A4-K03).
3. A4-K24'ün (aynı ikili, aynı init girdisi ve `bg_slices` dahil aynı çağrı dizisi → bit düzeyinde aynı çıktı) background çağrılarının `now_w` değerlerini de kapsaması.
4. HIL'de `bg_slices`'ın PC'ye taşınması gerekmez (A6-K56).

**A5'ten beklenen:** kayıt kademesi anahtarı (`olaylar`/`telemetri`/`tam`); `kamera_kareleri` anahtarı ve varsayılanı `kapali`; çözümlenmiş senaryonun kanonik biçimi ve `sha256:<hex>` özeti; araç dosyasının özeti; `schema_version`.

**A2'den beklenen:** kamera karesinin piksel biçimi kodu, kare kimliği ve A2 sürüm numarası (`yuk_kamera_kare`).

**A8'den beklenen:** algılama kaydının tel düzeni, sürümü ve en büyük boyutu (`yuk_algilama_kayit`).

**A7'den beklenen:** manifest özetinin metin biçimi (`a7_manifest_ozeti`).

**A3 (bu işte):** ortak kanallar ve yükler A3 ile aynı tanımdan üretildi; A3-K52, K57, K63, K67, K108–K115 A6'yla birlikte okunur.

**Uygulama işlerinden beklenen:** 04: sim zamanının çalıştırma başında 0 olması; `model_olaylari` bit tablosu; `StepResult` alanlarının `yuk_fizik_adim_sonucu`'na eşlenmesi. 05: sonar katkılarının (`sonar_gercek/ping360`) üretilmesi; `fault_mask` bit numaralarının 05 §2.7 bileşim sırasıyla eşlenmesi. 06: `a1/*` bloklarının hata enjektörüne göre yeri (A6-K28).

## 11. İskelete ya da diğer bölümlere notlar

- **06 §2.9 ve §2.13, 08 §2.6.2 (c) ile fark:** bölümler girdi tekrarını "kayıtlı sim → kart çerçevelerini SIL çekirdeğine ya da karta beslemek" olarak anlatır ve 06 §2.9, çalıştırma başında kartı `REBOOT` ile yeni oturuma alan akışı tanımlar. Bu sözleşmede (mimar kararı, 2026-11-20) güvenceli girdi tekrarının sınırı A1 bayt akışı değil A4 port sınırıdır: SIL'de çekirdek çağrıları doğrudan kaydedilir ve PC'deki çekirdeğe verilir (A6-K54, K55); HIL kaydından yalnız güvencesiz yeniden kurma yapılır ve oturum el sıkışması tekrar edilmez (A6-K56, K57); kayıtlı akışın gerçek karta verilmesi 1.0'da yoktur (A6-K58). Bölümler yeniden yazılmaz; bu not mimarın kaydı içindir.
- **İskelet A6 satırı** "girdi tekrarı (kayıtlı sensör akışını çekirdeğe verme)" ifadesiyle uyumludur; "çekirdeğe" burada PC'deki çekirdek olarak okunmuştur.
