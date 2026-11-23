# sim/sonar — sonar kümesi (`ida_sonar`, `ida_sonar_akustik`)

Fizik durumundan ve sahneden zaman damgalı Ping1D ve Ping360 kayıtları üreten başsız ve belirlenimci kitaplık (W4-sonar; görev belgesi `docs/gorevler/W4-sonar.md`). Bu dilim **sade kademedir** (iskelet §6, §8). İş parçacığı, saat, dosya ve ağ kullanmaz. Dayanak: iskelet §3.1, §4, §6, §8 ("Sonar ayrıntısı", "Sonar saçıcı kuralı"); `docs/mimari/05-sensorler-ve-sonar.md` §2.2, §2.4, §2.6, §2.7; A5 §3.6, §3.9, §3.12, Ek D; A1 §3.4, A1-K49–K54; A4 `ida_ping1d_t`, `ida_ping360_t`; A7 §3.4; MP7 (`docs/olcumler/MP7-sonar-suresi.md`, prototip `olcum/mp7-sonar-suresi` dalında; kod kopyalanmadı, yeniden yazıldı).

| Dosya | İçerik |
|---|---|
| `include/ida/sonar/akustik.hpp` | Ses hızı (UNESCO/Chen–Millero, Mackenzie, sabit) ve soğurma; sensör kümesi de kullanır (W4-K1) |
| `include/ida/sonar/sahne.hpp` | `Sahne` arayüzü (ışın–sahne kesişimi), `CozumselSahne` (havuz kutusu, düzlem, küre, kutu, silindir; hareketli gövde), A7 akustik malzeme alanları |
| `include/ida/sonar/ayarlar.hpp` | A5 alan adlarıyla ayarlar; cihaz yapılandırması (A5'te yok, aşağıda) |
| `include/ida/sonar/olcum.hpp` | `SonarOlcumu` = sensör kümesinin `OlcumBasligi` + A4 yükü (+ A3/A6 alanları) |
| `include/ida/sonar/sonar_kumesi.hpp` | `SonarKumesi`: `kur`, `adim`, `bosalt`/`siradaki`, `*_yapilandir`, `arm`/`disarm`/`active` |
| `src/demet.*` | Işın demeti, saçıcı hücreleri, biriktirme, darbe/alıcı çekirdeği |
| `test/` | CTest `sim.sonar.*` (19 test) |

## Arayüz kararı: ayrı `SonarKumesi`

Sonar modelleri `SensorKumesi`'ne eklenmedi; ayrı bir `SonarKumesi` sınıfı yazıldı. Gerekçe: (1) dosya sahipliği: `sim/sensor`'a yalnız W4-K1 değişikliği yapılabilir, model eklemek `SensorAyarGovdesi`, `OlcumYuku` ve arıza zincirinin değişmesini gerektirirdi; (2) çıktı türü farklıdır (Ping360 satırı 1 200 bayt, A1'de parçalanır; A3'te ayrı kanallar); (3) maliyet profili farklıdır ve işçi iş parçacığına taşınması sonraki dilimdir (05 §2.4.1). Ortak olan her şey `sim/sensor`'dan kullanılır: A5 Ek D akışları (`sensor::Akis`), arıza türleri ve arm/disarm sözleşmesi (`ariza.hpp`), ölçüm başlığı (`OlcumBasligi`), `KurulumSonucu`, `AdimSonucu`. Kullanım düzeni sensör kümesininkiyle aynıdır (`adim` her fizik adımında, `bosalt(t_n)`).

## Model (sade kademe)

- **Işın demeti:** ayrılabilir sinc deseni `B = sinc(θ/θ0) sinc(φ/φ0)`, tek yönlü −3 dB genişlikleri Ping360 2° × 25°, Ping1D 25° × 25° (05 §2.4.2 [T]); koni ana lob. Açı uzayında katmanlı titreşimli `nh × nv` ızgarası, `dΩ = Δθ Δφ cos φ`; titreşim `sensor/<ad>/sonar_isin` akışından (`n` = ping sayacı, `c2` = ışın numarası, 24 bit; 05 §2.6). Ping başına 5 000 ışın (A5-K78).
- **Yol:** yalnız doğrudan yol. Işın ağırlığı `w = B⁴ σ_bs dΩ 10^(−2αr/10) / (r² cos θ)`, `σ_bs = μ cos²θ` (`lambert_cos2`) ya da `μ cos θ` (`cos1`), `cos θ ≥ 0,05` (05 §2.4.2).
- **Saçıcı kuralı (iskelet §8):** yüzey gövdeye sabit, kenarı 1 cm hücrelere bölünür (`Sahne::kesis` hücre indisini verir); hücreye düşen ışınların enerjileri toplanır, gecikme enerji ağırlıklı ortalamadır; sade kademede hücreler eşevresiz (enerji) toplanır. Işın sayısı iki katına çıkınca enerji büyümez (`sim.sonar.enerji_isin_sayisi`).
- **Biriktirme ve çekirdek:** ince ızgara `T_s/4`, doğrusal kesirli gecikme; darbe ⊗ alıcı süzgeci = üçgen (yarı genişlik darbe süresi, sıfır fazlı [T]); sade kademede çekirdeğin karesi; kutuya indirgeme (MP7 §3 md. 4).
- **Benek ve gürültü:** kutu başına ortalaması 1 olan üstel çarpan (05 §2.4.3; anahtar `(gerçekleşim = tohum, ping, kutu)`, akış `sensor/<ad>/sonar_hucre`); gürültü tabanı `gurultu_tabani · Exp(1)` ve ilk 1 m'de `yakin_alan_genligi · Exp(1)` (akış `sensor/<ad>/olcum`).
- **Alıcı zinciri (Ping360):** `L = 10 log Ĩ + tvg_a log r + 2αr + G(gain)`; `u8 = clamp(round((L − L_min) · 255 / (L_maks − L_min)))`; 255'i aşan kutu varsa `DOYMUS` bayrağı.
- **Ping1D algılama [S]:** TVG'li profil 5 kutuluk ortalamayla yumuşatılır; tepe SNR'si ≥ 10 dB ise yumuşatılmış tepenin −3 dB'sini aşan ilk kutunun merkezi mesafedir; güven SNR 10 → 40 dB arası 0 → 100. Eko yoksa `VALID` = 0, güven 0, `t_meas = t_tx`. Otomatik menzil: pencere son mesafenin `oto_menzil_carpani` katı (en az 0,5 m), eko yoksa iki katı.
- **Ses hızı:** sahne boyunca tek değer, yüzeyde değerlendirilir (05 §2.4.1: kırılma ihmal [T]). Ping1D mesafeyi bu ses hızıyla verir (cihazın ses hızı ayarı A5'te yok; DVL `c_ayar` gibi ideal değer).

### Model sabitleri (A5 §8 md. 4: model sabitler, sürümüyle A6'ya yazılır; `model_surumu` = 1)

Hepsi tahmindir [T]/[K]; ölçüm değildir.

| Sabit | Değer | Not |
|---|---|---|
| Ping360 `L_min`, `L_maks` | −95, −15 dB | Havuz duvarı ≈ 200, taban 10–90 (u8) |
| Ping360 kazanç ofseti | gain 0/1/2 → −6/0/+6 dB | Cihaz eğrisi bilinmiyor |
| Ping360 iletim baytı | 24 + n | ping-protocol `device_data` (başlık 8 + alan 14 + sağlama 2); doğrulanmadı |
| Ping360 varsayılan darbe | 4 · T_s (µs'ye yuvarlanır) | MP7 §3 md. 4 |
| Yakın alan bandı | 1,0 m | 05 §2.4.4 md. 2 (0,75–1 m) |
| Ping1D gürültü tabanı | Ĩ_n = 10⁻⁹ | |
| Ping1D u8 | −95, −25 dB (TVG 20 log r) | Havuz tabanı tepesi ≈ 200 (benek tepeleri 255) |
| Ping1D darbe | 1 kutu | |
| Su yüzeyi malzemesi | μ = 10⁻⁴, `lambert_cos2`, R_s = −1 | A7'de su yüzeyi malzemesi yok |
| Hücre kenarı | 0,01 m | A5 `hucre_kenari` tam kademe varsayılanı; sade kademede A5'te anahtar yok |

## Zaman (05 §2.2; iskelet §3.1; A1-K53, K54)

- Gönderim `t_tx` bir adım sınırıdır; geometri o adımın durumu ve sahne pozlarıyla hesaplanır (dur-ve-atla).
- Kayıt fiziksel bitişin (`t_tx + T_dinleme + T_işlem + T_iletim`) üst adım sınırında üretilir; `t_hazir` = o sınır, `t_teslim = max(fiziksel bitiş, t_hazir)` (W4-K9).
- Ping360: `t_meas = t_tx`; `T_dinleme = n · sp · 25 ns`; sonraki gönderim `üst_sınır(bitiş + T_delay + T_motor)`, `T_motor = t_m0 + |Δaçı| · t_grad` (Δ = 0 ise 0). Tam turda açı sarar; sektörde uca varınca orada durur ve yön değiştirir [T].
- Ping1D: `t_meas = t_tx + tx_offset_ns` (yansıma anı), `T_dinleme = 2 (scan_start + scan_length) / c`; ping aralığı `max(ping_araligi_ns, T_dinleme + T_işlem)`, üst adım sınırına.

## Cihaz yapılandırması (A5'te ve A1'de yok)

A5-K48 Ping360 menzilini ve örnek sayısını "kartın yapılandırması" sayar; A1 (AB-024 satırı) yapılandırmanın A5'te olduğunu yazar ve yapılandırma komutu taşımaz. İki sözleşmede de yolu olmadığı için bu dilimde yapılandırma kurulumun alanıdır (`SonarAyari::yapilandirma`) ve `SonarKumesi::ping360_yapilandir` / `ping1d_yapilandir` ile değişir (05 §2.4.5 komut tablosu: süren ping tamamlanır, en yeni bekleyen geçerli olur, aralık dışı reddedilir). Varsayılan Ping360 yapılandırması 05 §2.4.5'in 50 m'sidir (1 200 örnek, `sp` = 2 222, 1 gradyan adım).

## Arızalar

Sensör kümesinin zinciriyle aynı sıra, anlam ve RNG yerleri (A5 Tablo 3.10-A, Ek D.4–D.5): `donma`, `donma_zaman_damgasi`, `yanlis_gecerlilik`, `kendini_bildiren`, `kayip`, `gec_teslim`; zaman desenleri `surekli`, `bernoulli`, `gilbert_elliott`. Reddedilenler: `gurultu_artisi` (A5 Tablo 3.10-A ve K65 sonarda tanımlamaz); `ofset`, `olcek`, `suruklenme_*`, `sicrama` (sonar kayıtlarının kanal numaralandırması tanımlı değil); durumlu `motor_takilmasi`, `guc_cevrimi` (yer tutucu).

## Ses hızı ve soğurma kaynakları

- UNESCO 1983 katsayıları (Fofonoff ve Millard 1983, UNESCO Tech. Pap. Mar. Sci. 44) MBARI `SW_svel.txt` ile karşılaştırıldı; denetim değeri 1731,995 m/s (S = 40, T68 = 40 °C, P = 1000 bar; R `marelac::sw_svel` belgesindeki örnek). Wong–Zhu (1995) ITS-90 katsayıları bu işte doğrulanamadı (NPL sayfası açılamadı); bunun yerine T68 = 1,00024 · T90 dönüşümüyle UNESCO 1983 katsayıları kullanıldı. Wong ve Zhu'nun özetine göre iki yolun farkı ≤ 0,024 m/s'dir.
- Mackenzie (1981) katsayıları MBARI `Sndspd.txt` ile karşılaştırıldı; denetim değerleri 1718,774 m/s (MBARI; S = 40, T = 40 °C, D = 10 000 m) ve 1550,744 m/s (T = 25 °C, S = 35, D = 1000 m; makalenin denetim değeri olarak bilinir, bu işte yalnız aritmetikle doğrulandı).
- Soğurma 05 §2.4.2'nin Ainslie–McColm yazımıdır; yayımlanmış bağımsız bir denetim değeri bulunamadı (R `sonar::AbsorptionAlphaAinslieMcColm` örneği f² yerine √f kullandığı için denetim olamaz). Test 05'in kendi aritmetiğini (750 kHz 0,1314 dB/m; 115 kHz 0,00309 dB/m) ve yapısal özellikleri sınar.

## Yakınsama (MP7 ölçütü)

`sim.sonar.yakinsama_mp7` MP7'nin dondurulmuş ölçütünü (M = 32, 8 açı, δ_E, δ_V, δ_C, bootstrap %95 üst ucu ≤ 0,05, iki adım) MP7 orta sahnesinin çözümsel karşılığında uygular. MP7 prototipi sade kademede satırı 600 örneğe indiriyordu (05 §2.4.7); ölçüt bu yapılandırmada (7 m, n = 600, sp = 622) 5 000 ışında sağlanır. A5-K78'in istediği 1 200 örnekte (sp = 311) aynı sahnede 5 000 ve 10 000 ışında sağlanmaz (`ida_sonar_test yakinsama_mp7 tani`). Bu, A5-K78'in "MP7'de ölçülen yapılandırma" gerekçesiyle çelişir; mimara bildirildi.
