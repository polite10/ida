# A6 — Kayıt

| | |
|---|---|
| Sözleşme | A6 |
| Sürüm | 1.0-aday (dosya sürümü `a6_surumu = 1.0`) |
| Tarih | 2026-11-19 |
| Görev | `docs/gorevler/W1-A3A6.md` |
| Makinece okunur parça | `docs/sozlesmeler/sema/a6-kanallar.json` |
| Dayanak | `docs/mimari/00-iskelet.md` sürüm 0.8 (§3 A6 satırı, §4, §6, §10 md.22–23); `docs/mimari/08-laboratuvar-uygulamasi.md` §2.6, §3.5; 04 §2.13; 06 §2.13 |
| İlgili sözleşme | A3 (`docs/sozlesmeler/A3-web.md`): ortak kanallar ve yük düzenleri; A1, A2, A4, A5, A8 (ayrı işler) |

Bu belge kural yazar; gerekçe 08 §2.6 ve §3.5'tedir. Makinece okunur dosya bu belgenin parçasıdır. Ortak kanalların yük düzenleri A3 kataloğundakiyle aynıdır ve iki dosya aynı tanımdan üretilmiştir; bu eşitlik 7. başlıktaki denetimle gösterilir.

---

## 1. Kapsam ve taraflar

**Yazan:** sim sunucusunun kayıt yazıcısı (`sim/` altında, C++20; tek iş parçacığı).
**Okuyanlar:** sim sunucusunun izleme ve tekrar koşusu sürücüleri; A3 kayıt listesi ve üst veri uçları; dışa aktarıcı (`tools/` altında, 08 §3.5; uygulama işi); test araçları.

**Kapsam:** dosya biçimi (MCAP sürümü, sıkıştırma, parça boyutu); kanal ve şema kataloğu ve her kanalın hangi kayıt kademesinde yazıldığı; üst veri ve ekler; tekrar oynatılabilirlik gerekleri (her komutun uygulandığı fizik adımı, bariyer kararları, geç kalan ve reddedilen yanıtlar, düşürülen veriler); tekrar oynatmanın üç düzeyi, her birinin gerektirdiği kayıt ve verdiği güvence; eksik, bozuk ve yarım kayıtta davranış; sürüm uyumluluğu.

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
| Tekrar düzeyleri | `izleme` (a), `fizik` (b: kayıtlı komutlarla fizik yeniden koşturma), `girdi` (c: kayıtlı sensör akışının çekirdeğe verilmesi); iskelet A6 satırı |
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
| Chunk | zstd sıkıştırmalı; hedef 1 MiB sıkıştırmasız (A6-K03, K04) |
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
| `a3/komut_sonucu` | `yuk_a3_komut_sonucu` | olaylar | her_uretimde | sim | izleme, fizik | hayır | dünya iş parçacığı |
| `senaryo/oge_uygulama` | `yuk_senaryo_oge_uygulama` | olaylar | her_uretimde | sim | izleme, fizik | hayır | senaryo motoru (08 §2.2) |
| `hil/oturum` | `yuk_hil_oturum` | olaylar | her_uretimde | sim | izleme | hayır | 06 oturum |
| `hil/bariyer` | `yuk_hil_bariyer` | olaylar | olaylar kademesinde yalnız `karar != zamaninda`; telemetri ve tamda her parti | sim | izleme, fizik | hayır | iskelet §4, 06 bariyer |
| `hil/dusen` | `yuk_hil_dusen` | olaylar | her_uretimde | sim | izleme | hayır | 05, 06 |
| `kayit/eksik` | `yuk_kayit_eksik` | olaylar | her_uretimde | sim | izleme, fizik, girdi | hayır | kayıt yazıcısı |
| `sayac/gercek_zaman` | `yuk_sayac_gercek_zaman` | olaylar | 1 Hz (duvar saati) ve kapanışta | sim | izleme | evet | 04, 05, 06 |
| `calistirma/durum` | `yuk_calistirma_durum` | olaylar | değişimde | sim | izleme | evet | çalıştırma denetçisi |
| `emniyet/girdiler` | `yuk_emniyet_girdiler` | olaylar | değişimde | sim | izleme, fizik | evet | sanal emniyet girdileri |
| `atis/durum` | `yuk_atis_durum` | olaylar | değişimde | sim | izleme | evet | fırlatıcı modeli + kart FC durumu |
| `ariza/etkin` | `yuk_ariza_etkin` | olaylar | değişimde | sim | izleme | evet | arıza dağıtıcısı |
| `fizik/durum` | `yuk_fizik_durum` | telemetri | her kontrol periyodu sınırı (200 Hz) | olcum | izleme, fizik | evet | 04 WorldSnapshot |
| `fizik/adim_sonucu` | `yuk_fizik_adim_sonucu` | telemetri | boş olmayan her fizik adımı | sim | fizik | hayır | 04 StepResult |
| `a1/kart_sim` | `yuk_a1_blok` | telemetri | her_uretimde | alim | izleme, girdi | hayır | 06 G/Ç iş parçacığı |
| `hil/zaman_esleme` | `yuk_hil_zaman_esleme` | telemetri | değişimde ve 1 Hz | sim | izleme | hayır | 06 §2.5 |
| `hil/enjeksiyon` | `yuk_hil_enjeksiyon` | telemetri | her_uretimde | sim | izleme, girdi | hayır | 06 §2.12 |
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
| `a1/sim_kart` | `yuk_a1_blok` | tam | her_uretimde | sim | izleme, girdi | hayır | 06 G/Ç iş parçacığı |
| `sonar/ping360` | `yuk_ping360` | tam | her_uretimde | gonderim | izleme | evet | 05 |
| `sonar/ping1d` | `yuk_ping1d` | tam | her_uretimde | gonderim | izleme | evet | 05 |
| `sonar_gercek/ping360` | `yuk_sonar_gercek_ping360` | tam | her_uretimde | gonderim | izleme | hayır | 05 yer gerçeği |
| `kamera/kare` | `yuk_kamera_kare` | tam (kamera_kareleri != kapali) | her_uretimde | olcum | izleme | hayır | A2 |

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
| `govde` | metin(u32, en çok 4194304) | bayt | tipin aralığı | istek gövdesi (UTF-8 JSON) olduğu gibi |
| `alim_duvar_ns` | i64 | ns | tipin aralığı | isteğin alındığı kayıt duvar saati |
| `senkron_sonuc` | u8 | - | 0=kuyruga_alindi, 1=tamamlandi, 2=reddedildi | HTTP yanıtındaki sonuç |
| `http_durum` | u16 | - | tipin aralığı | döndürülen HTTP durum kodu |
| `hata_kodu` | metin(u16, en çok 64) | bayt | tipin aralığı | reddedildiyse A3 hata kodu; değilse boş |

#### `yuk_a3_komut_sonucu`

| Alan | Tip | Birim | Geçerli aralık / değerler | Anlam |
|---|---|---|---|---|
| `komut_no` | u32 | - | tipin aralığı | A3 komut numarası |
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
| `genislik` | u16 | piksel | [1, 640] | genişlik |
| `yukseklik` | u16 | piksel | [1, 480] | yükseklik |
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

İki MCAP Metadata kaydı yazılır; değerler metindir (MCAP Metadata `Map<string, string>`). Anahtar listesi `a6-kanallar.json` `x_ida_ust_veri` ile aynıdır; JSON Schema karşılığı `$defs/ust_veri_ida_kayit` ve `$defs/ust_veri_ida_kayit_kapanis`.

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
| `kip` | evet | `sil`, `hil_1x`, `hil_bariyer`, `tekrar_fizik`, `tekrar_girdi_sil`, `tekrar_girdi_kart` |
| `kaynak_run_id` | koşullu | tekrar koşusunda kaynak kaydın run_id'si |
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

**`ida_kayit_kapanis`** — olağan kapanışta, son iletiden sonra.

| Anahtar | Zorunlu | Anlam |
|---|---|---|
| `bitis_durumu` | evet | `bitti`, `durduruldu`, `on_denetim_kaldi`, `hata`, `disk_dolu`, `sinira_ulasti`, `sim_kapanisi` |
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
| `ileti_sayisi` | evet | dosyaya yazılan ileti sayısı |

### 3.6 Ekler

| Ad | Ortam türü | Zorunlu | Anlam |
|---|---|---|---|
| `cozumlenmis_senaryo.toml` | `application/toml` | evet | A5 çözümlenmiş senaryo, kanonik biçim (media type IANA kaydı doğrulanmadı) |
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
- **A6-K03** Yazıcı `foxglove/mcap` C++ kitaplığıdır (Ö7, Ç11); sürümü W2'de paket yöneticisiyle sabitlenir ve `bagimliliklar` üst verisine yazılır. Yazım parçalıdır; sıkıştırma zstd'dir; parça hedef boyutu 1 MiB sıkıştırmasız veridir (kitaplığın `DefaultChunkSize` değeri; hedeftir, bir kayıt kadar aşılabilir); parça CRC'si, ileti indeksleri ve özet bölümü kitaplık varsayılanlarıyla açıktır. Sıkıştırmadan kazanç olmayan parçayı kitaplık sıkıştırmasız yazabilir (`forceCompression = false`, kitaplık varsayılanı).
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

### 4.6 Tekrar oynatmanın üç düzeyi

**Tablo 4.6 — Düzeyler, gerekenler ve güvenceler.**

| Düzey | En düşük kademe | Gereken kayıt | Ne yapılır | Güvence | Güvencenin koşulu |
|---|---|---|---|---|---|
| (a) `izleme` | `olaylar` | Okunabilen herhangi bir dosya | Kayıttaki ortak kanallar A3 üzerinden `kaynak_kayit` bayrağıyla yayınlanır (A3-K112); fizik koşmaz | Gösterilen her değer kayıttaki değerin aynısıdır | Yok. `olaylar` kademesinde yalnız olay zaman çizelgesi izlenir; tekrar oynatma güvencesi yoktur (iskelet A6) |
| (b) `fizik` | `telemetri` | `fizik/adim_sonucu`, `a3/komut`, `a3/komut_sonucu`, `senaryo/oge_uygulama`, `emniyet/girdiler`, `fizik/durum`; ekler `cozumlenmis_senaryo.toml`, `arac.toml`; üst veri `tohum`, sürümler | Senaryo ve tohumla dünya yeniden kurulur; kart, A1, görselleştirici ve algılama bileşeni yerine kayıtlı komut ve olay kararları, elle arızalar, emniyet değişiklikleri ve senaryo öğeleri kayıtlı adımlarda uygulanır | Kanonik fizik izi (§4.7) kayıttakiyle bit düzeyinde aynıdır | Aynı `sim_surumu`, `platform`, `derleyici`, `derleme_bayraklari`, `bagimliliklar`, `rng_surumu`; `senaryo_ozeti` ve `arac_ozeti` eşit; gereken kanallarda eksik yok (ya da önek). Farklı platformda yalnız toleranslı eşdeğerlik (Ç7; tolerans 04 §6 ve 09'da). Senaryo, tohum ya da ikili farklıysa koşu yapılır ama "tekrar" sayılmaz (08 §2.6.2) |
| (c) `girdi` | `tam` | `a1/sim_kart` (bütün bloklar), `hil/zaman_esleme`, `hil/oturum`, `a1/kart_sim` (karşılaştırma için) | Kayıtlı sim → kart bayt blokları, parti sırasıyla SIL çekirdeğine (`tekrar_girdi_sil`) ya da karta (`tekrar_girdi_kart`) verilir; fizik koşmaz | SIL'de, kaynak çalıştırma da SIL ise: kart çıktısının kanonik izi bit düzeyinde aynıdır (A4'ten beklenen koşulla). Kaynak HIL ise ya da hedef kartsa: yalnız toleranslı karşılaştırma | Aynı çekirdek ikilisi ve platform; gereken kanallarda eksik yok (ya da önek). Kartın duvar saatine bağlı davranışı (gözetim, emniyet süreleri, bağlantı zaman aşımı) için güvence yoktur (Ç1; iskelet §4) |

- **A6-K33** Kapanışta yazıcı `tekrar_guvencesi`'ni şöyle belirler: düzeyin kademe ve kanal gereği sağlanan en yüksek düzey (`fizik` için `telemetri`, `girdi` için `tam`); `olaylar` kademesinde `izleme`. O düzeyin gerektirdiği bir kanalda `kayit/eksik` kaydı varsa güvence o düzeyde **önek güvencesidir**: `guvence_on_eki_son_adim` = gereken kanallardaki ilk eksik adım − 1 yazılır ve güvence yalnız 0 … bu adım için geçerlidir; ilk eksik adım 0 ise o düzey verilmez ve bir alt düzeye inilir.
- **A6-K34** Bariyer beklemesi olan ya da gerçek zamanı kaçıran bir çalıştırmanın kaydı `fizik` düzeyini korur (komutlar kayıtlı adımlarda uygulanır); `girdi` düzeyinde kartın duvar saatine bağlı davranışı için güvence verilmez (iskelet §4).
- **A6-K35** Fizik yeniden koşturmada sim kayıtlı sürekli komutları ve tek seferlik olay kararlarını kaydedildikleri adımlarda uygular ve kararı yeniden hesaplar; yeniden hesaplanan karar kayıttakinden farklıysa kayıttaki uygulanır ve `uyari` olayı (`kod = tekrar_karar_farki`) üretilir.
- **A6-K36** Fizik yeniden koşturmada sensör modelleri ve sonar tohumdan yeniden üretilir ve hiçbir yere iletilmez; sonar çıktısının kayıttakiyle eşitliği güvence değildir (Embree'nin bit eşitliği garantisi bulunamadı, aynı ikili ve platformda eşitlik M11/M15 ile doğrulanmadı; 08 §2.6.2). Fizik sonar çıktısını tüketmediği için kanonik fizik izi bundan etkilenmez.
- **A6-K37** Tekrar koşusunun kendisi yeni bir dosyaya kaydedilir; üst veride `kip` `tekrar_fizik`, `tekrar_girdi_sil` ya da `tekrar_girdi_kart`, `kaynak_run_id` kaynak kaydın `run_id`'sidir. Kayıttan uygulanan A3 komutları yeni dosyada aynı `komut_no` ile ve `ayrinti.kaynak = "kayit"` ile `a3/komut_sonucu`'na yazılır.
- **A6-K38** Tekrar koşusu başlarken A6 §4.6 koşullarından biri sağlanmıyorsa (sürüm, platform ya da özet farkı) koşu yapılır, `uyari` olayı (`kod = tekrar_guvencesi_yok`) üretilir ve yeni dosyanın `tekrar_guvencesi` yok yazılır; A3 bunu `yeniden_kostur` öncesinde `ayrinti.gereken` ile bildirir (A3-K68).

### 4.7 Kanonik işlevsel iz ve normalleştirme

- **A6-K39** Kanonik fizik izi `fizik/durum` iletilerinin bütün alanları ile `fizik/adim_sonucu` iletilerinin `olay_kimligi` dışındaki bütün alanlarından oluşur ve `adim_no` sırasıyla dizilir.
- **A6-K40** Kanonik kart çıktısı izi `a1/kart_sim` bloklarından A1 ayrıştırıcısıyla çözülen işlevsel alanlardan oluşur ve yanıtın parti sıra numarasıyla (`step_seq`) dizilir; hangi alanların işlevsel, hangilerinin tanısal olduğu A1'den beklenir (06 §2.13'teki liste başlangıç önerisidir).
- **A6-K41** Olay kimliği normalleştirilir: her ham `olay_kimligi`, çalıştırma içinde ilk görüldüğü sırayla 1'den numaralanır (`normallestirilmis_sira`); yinelenen kimlik aynı sırayı alır. Karşılaştırma ham kimliği değil bu sırayı kullanır; böylece kartın açılış sayacı farklı iki HIL çalıştırması karşılaştırılabilir (AB-100).
- **A6-K42** `normallestirilmis_sira`'yı dünya atar ve hem `fizik/adim_sonucu`'na hem A3 `atis_olayi_sonucu` olayına aynı değerle yazar.
- **A6-K43** Kayıt duvar saati alanları, `publish_time`, `sequence`, oturum numarası (`sess`), açılış sayacı ve kartın tanısal alanları (ör. `adim_suresi_us`) kanonik ize girmez.

### 4.8 Kapanış

- **A6-K44** Olağan kapanışta yazıcı bekleyen iletileri yazar, son `sayac/gercek_zaman` iletisini yazar, `ida_kayit_kapanis` üst verisini yazar ve MCAP dosyasını kitaplığın `close` çağrısıyla (Data End, özet, Footer, sondaki sihirli baytlar) kapatır.
- **A6-K45** `bitis_durumu` çalıştırmanın son durumudur: `bitti`, `durduruldu` (`durdur` ya da `sifirla` ile; A3-K67), `on_denetim_kaldi` (çalıştırma ön denetimde kaldı; A3-K62), `hata`, `disk_dolu`, `sinira_ulasti` ya da `sim_kapanisi` (canlı çalıştırma sürerken sim sunucusu olağan kapandı).
- **A6-K46** `ida_kayit` üst verisi dosya açılışında, ilk kanal iletisinden önce yazılır ve gerekli anahtarların tamamını taşır; açılışta bilinmeyen bir değer (ör. HIL'de el sıkışmadan önce kart yazılım özeti) bilinince `ida_kayit` adlı ikinci bir Metadata kaydıyla yalnız o anahtar yazılır; okuyucu aynı anahtarın son değerini kullanır.

### 4.9 Sürüm uyumluluğu

- **A6-K47** `a6_surumu` `büyük.küçük` biçimindedir; bu sözleşme `1.0`'dır. Küçük artış yalnız yeni kanal, var olan yükün sonuna alan (o kanalın `kanal_surumu` bir artar), yeni üst veri anahtarı ya da yeni ek ekleyebilir; başka her değişiklik büyük artıştır. A3 ile ortak kanallarda `kanal_surumu` A3 ile birlikte değişir.
- **A6-K48** Okuyucu aynı büyük sürümdeki dosyayı okur; bilinmeyen kanalı atlar; bildiğinden büyük `kanal_surumu`'lu iletide bildiği alanları okur ve sondaki fazla baytları yok sayar, küçük `kanal_surumu`'lu iletide eksik sondaki alanları "yok" sayar (A3-K19 ile aynı kural). Farklı büyük sürümdeki dosyayı okumaz (`kayit_okunamadi`).
- **A6-K49** Okuyucu, dosyadaki şema `data`'sının kendi bildiği aynı `kanal_surumu`'lu yük tanımıyla aynı olmadığını görürse o kanalı okumaz ve uyarı verir (sürüm kuralı ihlali).
- **A6-K50** Her dosya, kendisini yazan sürümün `a6-kanallar.json` dosyasını ek olarak taşır; dışa aktarıcı yeni bir okuyucu olmadan eski dosyayı bu ekle çözebilir.

---

## 5. Hata durumları

| Durum | Algılama | Yazıcının davranışı | Okuyucunun davranışı ve güvence |
|---|---|---|---|
| Halka tamponu taştı | Üretici düşme sayacı | `kayit/eksik`, `kayit_eksik` olayı, `kayit_dusen` (A6-K30) | Önek güvencesi (A6-K33) |
| Disk dolu ya da yazma hatası | Yazma çağrısı başarısız | Kapatmayı dener, `bitis_durumu` (A6-K31) | Kapanmışsa önek güvencesi; kapanmamışsa aşağıdaki satır |
| Başta sihirli baytlar yok ya da Header okunamıyor | Açılış | — | Okunmaz; `kayit_okunamadi`; listede `kapanmis: false`, güvence yok (A3-K110) |
| Dosya kapanmamış (sim çöktü, Footer ya da özet yok) | Footer/özet okunamıyor | — | Kitaplığın `AllowFallbackScan` yoluyla baştan sıralı okunur (reader.hpp: özet yoksa ya da eksikse sıralı okumaya düşer). Okunabilen son tam parçaya kadar `izleme`; `fizik` ve `girdi` güvencesi verilmez (kapanış üst verisi ve eksik bildirimi olmadan önek belirlenemez) |
| Parçada CRC hatası ya da okunamayan kayıt | Kitaplık sorun bildirir | — | Kitaplık okunamayan kayıtta yinelemeyi durdurur (reader.hpp); okuyucu o noktaya kadar `izleme` verir ve dosyayı "bozuk" işaretler; `fizik` ve `girdi` güvencesi verilmez |
| `ida_kayit` gerekli anahtarı eksik | Üst veri denetimi | — | Yalnız `izleme` |
| Farklı büyük `a6_surumu` | Üst veri | — | Okunmaz (A6-K48) |
| Şema `data`'sı bilinen tanımla aynı değil | A6-K49 | — | Kanal okunmaz, uyarı |
| `log_time` negatif olacak ileti | Yazma öncesi | Yazmaz, `kayit/eksik` (A6-K10) | Önek güvencesi |
| İleti sayısı sınırı | `sequence` taşacak | `sinira_ulasti` ile kapatır (A6-K12) | Önek güvencesi |
| Aynı adlı dosya var | Açılış | Ezmez; çalıştırma başlamaz (A6-K01) | — |
| Tekrar koşusunda sürüm, platform ya da özet farkı | Üst veri karşılaştırması | Yeni dosyada güvence yok (A6-K38) | Koşu "tekrar" sayılmaz |

## 6. Kalite kademelerinin etkisi

Kayıt kademesi iskelet §6'daki kayıt satırıdır ve kullanıcı seçimidir (A5; A3-K109 ile çalıştırma başında geçersiz kılınabilir). Hangi kanalın hangi kademede yazıldığı Tablo 3.3'tedir; tekrar düzeyleri Tablo 4.6'dadır.

Veri hızları (hesap, sıkıştırmasız, MCAP kayıt ek yükü hariç; **ölçülmedi**; sıkıştırma etkisi bilinmiyor):

| Kaynak | Kademe | Hesap |
|---|---|---|
| `a1/kart_sim` ya da `a1/sim_kart` | `telemetri` / `tam` | Yön başına en çok 300 000 B/s (3 000 000 baud 8N1'in üst sınırı; iskelet §3); sözleşme yükü periyot başına 900 B'nin altında tuttuğu için olağan yük ≤ 180 000 B/s (900 B × 200 Hz) |
| `fizik/durum` | `telemetri` | 4 gövde ve 8 iticiyle ileti 647 B; 200 Hz'te ≈ 129 kB/s (≈ 466 MB/saat) |
| `kamera/kare` | `tam` + `surekli` | 640 × 480 × 3 B (RGB8; piksel biçimi A2'nin) × 30 Hz ≈ 27,6 MB/s (≈ 99 GB/saat) |

Bu yüzden `kamera_kareleri` varsayılanı `kapali`'dır (A5'ten beklenen) ve `surekli` yalnız kısa deneyler içindir. Düşük donanımlı Windows bilgisayarında hangi kademenin taşınabildiği ölçülmedi (MP8; 8. başlık md.1).

Emniyet kabulü kademeden bağımsızdır (iskelet §6): emniyet girdisi değişimleri, atış olayı kararları, A3 komutları ve bariyer anomalileri her kademede kaydedilir (A6-K18).

## 7. Uygunluk

| Kurallar | Test türü | Sınama yöntemi ve gözlenecek büyüklük |
|---|---|---|
| K01–K05 | birim | Üretilen dosyayı kitaplıktan bağımsız bir MCAP okuyucusuyla açma (sihirli baytlar, Header `profile`, parça sıkıştırması, özet); 1 s'den kısa aralıkla parça kapanması; aynı adlı dosyanın ezilmemesi |
| K06, K30–K32 | SIL | Yapay yavaş disk ya da küçültülmüş halka tamponuyla: dünyanın adım süresinin kayıtsız koşudan ayırt edilemezliği (eşik ölçümle), `kayit/eksik` kayıtlarının düşen iletilerle sayıca ve aralıkça eşleşmesi, `gecerli`'nin değişmemesi |
| K07–K12 | birim | Şema `data`'sının katalogdan üretilenle bayt eşitliği; C++ kodlayıcı ile dışa aktarıcının çözücüsünün altın iletilerde gidiş-dönüşü; `log_time`/`publish_time`/`sequence` kuralları; karışık dosya sırasında okuyucunun sıralaması |
| K13–K15 | SIL | Duvar saati alanlarının tek monoton kaynaktan geldiği (kayıt açılışında ≈ 0); sensör kaydında `t_teslim_ns` ile `parti_step_seq`'in ayrı ve tutarlı olması; Ping360 satırında `log_time` = `t_tx_ns` |
| K16–K20 | SIL | Üç kademede ve üç `kamera_kareleri` değerinde dosyadaki kanal kümesinin Tablo 3.3 ile birebir eşleşmesi; `olaylar` kademesinde `hil/bariyer`'de yalnız anomalilerin bulunması |
| K21–K29 | SIL + kartlı | Hata enjektörüyle geciktirilen ve yinelenen yanıtlar, kaçırılan partiler, düşürülen profiller, kart sıfırlaması: her olayın ilgili kanalda doğru adım ve nedenle bulunması; A1 bloklarının taşımaya verilen ve alınan baytlarla eşitliği |
| K33–K38 | SIL | Aynı ikili ve platformda iki SIL koşusu ile kayıttan `fizik` tekrarı: kanonik fizik izinin bit düzeyinde eşitliği; `telemetri` kaydından `girdi` tekrarı isteğinin reddi; önek güvencesi olan kayıtta tekrarın önek adımında durması; farklı `sim_surumu` ile güvencenin "yok" yazılması |
| K33–K38 | kartlı | Bariyerli HIL kaydından `fizik` tekrarı: kanonik fizik izinin eşitliği; `tam` kaydından `tekrar_girdi_sil`: kart çıktısının toleranslı karşılaştırması |
| K39–K43 | birim + kartlı | Açılış sayacı farklı iki HIL koşusunda normalleştirilmiş olay sırasının eşleşmesi; duvar saati alanlarının ize girmemesi |
| K44–K46 | birim | Olağan kapanışta kapanış üst verisinin ve özetin bulunması; süreç öldürülünce dosyanın sıralı okunabilmesi ve güvencenin `izleme` olması |
| K47–K50 | birim | Sonuna alan eklenmiş (`kanal_surumu` + 1) iletinin eski okuyucuda okunması; farklı büyük sürümün reddi; şema `data` uyuşmazlığında kanalın okunmaması; ekteki katalogla dışa aktarım |

**A3 ile ortak kanalların eşitliği** (görev belgesinin istediği karşılaştırma): `docs/sozlesmeler/sema/a6-kanallar.json` içinde `a3_ortak: true` olan her kanal için `a3-kanallar.json`'da aynı adlı kanal bulunur; ikisinde `yuk`, `kanal_surumu` ve `zaman_anlami` aynıdır ve yük tanımları (alan adı, tip, adet, birim, aralık, değer listesi, yok değeri, sıra) iki dosyada derin eşittir. Bu işte yapılan denetimin sonucu 7.1'dedir.

### 7.1 Bu işte yapılan şema denetimi

Yöntem: kurulu bir JSON Schema doğrulayıcısı yoktu; Python 3.12 standart kitaplığıyla yazılmış bir yapısal denetim betiği kullanıldı (betik depoya girmez). Betik, bilerek bozulmuş kopyalarda (yük alanının tipi değiştirildi, bir `$ref` kırıldı, bir `kanal_id` yinelendi) dört hatanın dördünü yakaladı. Sonuç (2026-11-19):

- İki dosya geçerli UTF-8 ve geçerli JSON (Python 3.12 `json`).
- `$defs` altındaki şemalar: a3'te 82, a6'da 2 tanım; kullanılan anahtar sözcükler JSON Schema 2020-12 sözlüğünde, bütün `$ref`'ler dosya içinde çözüldü, `pattern`'ler derlendi.
- A3: 24 kanal (kimlik ve ad tekil; yükler, anahtar alanları, sınıf, hız ve kuyruk kuralları tutarlı), 23 yük, 22 olay türü (her birinin ayrıntı şeması var), 45 uç (yinelenen yok; durum değiştiren her uçta `istek_kimligi` zorunlu; yolunda `ates`/`fire`/`tetik` geçen uç yok), 24 hata kodu; çerçeve başlığı 24 bayt.
- A6: 51 kanal (ad tekil; yük, kademe ve tekrar düzeyi değerleri geçerli; `fizik` düzeyinin gerektirdiği hiçbir kanal `tam` kademesinde değil), 33 yük.
- Ortak kanal karşılaştırması: A6'da `a3_ortak: true` olan 20 kanalın 20'i A3'te aynı adla, aynı `yuk`, `kanal_surumu` ve `zaman_anlami` ile ve derin eşit yük tanımıyla bulundu. İki dosyada aynı adı taşıyan 19 yükün hepsi derin eşit. A3'e özgü (kaydedilmeyen) kanallar: `baglanti/durum`, `kayit/durum`, `surec/durum`, `yer_istasyonu/mavlink`.
- Hesap denetimi: `yuk_fizik_durum` 4 gövde ve 8 iticiyle 647 bayt (A6 §6'daki hesap).

## 8. Açık kalanlar

1. **Kayıt yazıcı verimi ve taşma eşiği** (ölçülecek): zstd sıkıştırmanın işlemci maliyeti, `telemetri` ve `tam` kademelerinin sürdürülebilir veri hızı, halka tamponu boyutları. 00 belgesinde M numarası yok (08 §8 11.1 md.4 önerisi); MP8 (Windows) ve W5 kayıt işinde ölçülür.
2. **Kamera kare kaydı:** sıkıştırma yöntemi ve maliyeti ölçülmedi (M30 ile); `pencereli` kipin ±2 s penceresi tahmin.
3. **Parça kapatma aralığı 1 s** (A6-K04): tahmin; küçük parçaların sıkıştırma oranına etkisi ölçülmedi.
4. **mcap kitaplığının sürümü:** `main` dalındaki `types.hpp` `MCAP_LIBRARY_VERSION` değeri `2.2.0`; yayımlanmış sürüm etiketiyle eşleştiği ve paket yöneticisinde bulunduğu doğrulanmadı; W2'de sabitlenir.
5. **`application/toml` ortam türü:** IANA kaydı doğrulanmadı; doğrulanamazsa ek `text/plain; charset=utf-8` ile yazılır (küçük sürüm).
6. **`sonar_gercek/ping360` katkı sayısı (64):** tahmin; 05'in sonar uygulaması (W4) katkıları bu biçimde verebiliyor mu doğrulanmadı.
7. **`fizik/durum` kayıt hızı 200 Hz** (kontrol periyodu sınırı): mimari seçimi; fizik adımı hızında (800 Hz) kayıt gerekip gerekmediği tolerans karşılaştırmasıyla (Ç7) değerlendirilir.
8. **Kapanmamış dosyada önek güvencesi:** 1.0'da verilmez (§5); periyodik "güvence işareti" kaydıyla önek tanımlanabilir (küçük sürüm adayı).

## 9. Açık bulgu kapanış tablosu

`docs/mimari/acik-bulgular.md` "A6 — Kayıt" başlığındaki bulguların tamamı ve uyumsuzluk tablosundaki A6 satırı:

| Bulgu | Durum | Sözleşmede nerede | Gerekçe |
|---|---|---|---|
| AB-096 | kapandı | A6-K21, K23, K27; `hil/oturum`, `fizik/adim_sonucu` | Oturum geçişleri, kart sıfırlamaları ve komut ömrü kararları (`omur_doldu_notr`, `omur_bitis_t_ns`) kayda bağlandı; değerlerin kendisi (kullanım süresi, zaman aşımı) A1'den gelir ve üst veriye yazılır. |
| AB-097 | açık | A6-K40; 10. başlık (A1, A4) | Arka plan işinin tüketildi/düştü kararını A6 ancak A1 telemetrisinde taşınırsa kaydedebilir (`a1/kart_sim` ham baytları); kararın A4/A1'de görünür kılınması ve SIL çekirdeğinin bu kararla deterministik olması bekleniyor. |
| AB-098 | kapandı | A6-K30, K33; `kayit/eksik` | Taşmada hangi kanalın hangi adım aralığında eksik olduğu kaydediliyor ve güvence önek olarak daraltılıyor. |
| AB-099 | kapandı | A6-K15; `yuk_*` ölçüm başlığı; `sonar_gercek/ping360` | Nominal kutu zamanı (`log_time` + kutu genişliği) ile gerçek yansıma anları, modellenmiş teslim (`t_teslim_ns`) ile fiilî parti (`parti_step_seq`) ayrı alanlarda. |
| AB-100 | kapandı | A6-K39…K43 | Olay kimliği normalleştirmesi (ilk görülme sırası) ve `step_seq` ile girdi bağlama tanımlandı; işlevsel/tanısal A1 alan listesi A1'den beklenir. |
| AB-101 | kapandı | A6-K21, K22; `yuk_fizik_adim_sonucu` | `StepResult`'ın kayıt karşılığı sürümlü (`kanal_surumu`) ve dondurulabilir yük olarak tanımlandı. |
| U-28 | kapandı | 1. başlık; Tablo 3.3; Tablo 4.6 | A6 kapsamı "A1 trafiği, A3 komutları ve olayları, fizik durumu" oldu (iskelet 0.8); A3 telemetrisi kaydedilmez; `a1/sim_kart` yalnız `tam`'da, böylece "bütün A1 çerçeveleri" gereği yalnız `girdi` düzeyine bağlandı. |

Kritik önemde açık bırakılan A6 bulgusu yoktur (AB-097 "önemli"dir).

Bilgi için: AB-022 (A1 başlığında, "A6 yan verisi" notlu) A6 tarafında A6-K15 ve `sonar_gercek/ping360` ile karşılanır; bulgunun kendisi A1'dedir.

## 10. Başka sözleşmelerden beklenenler

**A1'den beklenen:**
1. A1 sürüm alanları (`proto_major`, `proto_minor`, `hash_base`, `hash_full`) ve dialect XML'i (`a1_dialect.xml` eki; MAVLink kullanılıyorsa).
2. Tek seferlik olay kimliğinin bit düzeni (açılış sayacı + olay sayacı) ve `u64`'e sığması; olay türü kodları.
3. Kanonik iz için A1 alanlarının işlevsel/tanısal sınıflaması (A6-K40).
4. Bekleme sınırı ve sürekli komut kullanım süresi değerleri (üst veri anahtarları).
5. Kart yazılımı özetinin (`BOOT_RECORD`) biçimi.
6. Arka plan işinin karar adımının telemetride taşınması (AB-097; A4 ile birlikte).

**A4'ten beklenen:** SIL çekirdeğinin aynı girdi bayt akışı ve sim zamanından türeyen duvar saatiyle bit düzeyinde aynı çıktıyı üretmesinin koşulları (arka plan işi dahil) (Tablo 4.6, `girdi`).

**A5'ten beklenen:** kayıt kademesi anahtarı (`olaylar`/`telemetri`/`tam`); `kamera_kareleri` anahtarı ve varsayılanı `kapali`; çözümlenmiş senaryonun kanonik biçimi ve `sha256:<hex>` özeti; araç dosyasının özeti; `schema_version`.

**A2'den beklenen:** kamera karesinin piksel biçimi kodu, kare kimliği ve A2 sürüm numarası (`yuk_kamera_kare`).

**A8'den beklenen:** algılama kaydının tel düzeni, sürümü ve en büyük boyutu (`yuk_algilama_kayit`).

**A7'den beklenen:** manifest özetinin metin biçimi (`a7_manifest_ozeti`).

**A3 (bu işte):** ortak kanallar ve yükler A3 ile aynı tanımdan üretildi; A3-K52, K57, K63, K67, K108–K115 A6'yla birlikte okunur.

**Uygulama işlerinden beklenen:** 04: sim zamanının çalıştırma başında 0 olması; `model_olaylari` bit tablosu; `StepResult` alanlarının `yuk_fizik_adim_sonucu`'na eşlenmesi. 05: sonar katkılarının (`sonar_gercek/ping360`) üretilmesi; `fault_mask` bit numaralarının 05 §2.7 bileşim sırasıyla eşlenmesi. 06: `a1/*` bloklarının hata enjektörüne göre yeri (A6-K28).
