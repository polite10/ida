# sim/sensor — sensör kümesi (`ida_sensor`)

Fizik durumundan zaman damgalı ölçüm üreten başsız ve belirlenimci kitaplık (W4-sensor; görev belgesi `docs/gorevler/W4-sensor.md`). İş parçacığı, saat, dosya ve ağ kullanmaz. Dayanak: iskelet §3.1, §4, §5; `docs/mimari/05-sensorler-ve-sonar.md` §2.1–§2.3, §2.6, §2.7; A5 §3.6, §3.9, §3.10, Ek D, Ek E; A1 §3.4; A4 §3.2–§3.3.

Kapsam: IMU, manyetometre, basınç, GNSS, DVL (düz taban, sade model), batarya ölçer, sızıntı, fırlatıcı ve emniyet geri okuması. Sonar (Ping1D, Ping360) ve kamera bu kümede değildir: ayarda bulunabilirler, kayıt üretilmez, kurulum uyarı yazar.

| Dosya | İçerik |
|---|---|
| `include/ida/sensor/sensor_kumesi.hpp` | `SensorKumesi`: `kur`, `dis_durum_ver`, `adim`, `bosalt`/`siradaki`, `arm`/`disarm`/`active` |
| `include/ida/sensor/olcum.hpp` | `Olcum` = `OlcumBasligi` + yük (A1/A4 alan adları ve tipleri) |
| `include/ida/sensor/ayarlar.hpp` | A5 alan adlarıyla ayar yapıları, profil varsayılanları (Ek E), `ida0_sensor_kumesi()` |
| `include/ida/sensor/ariza.hpp` | Arıza tanımı (A5 `olaylar[i]`, Tablo 3.10-A) |
| `include/ida/sensor/rastgele.hpp` | A5 Ek D: SHA-256 anahtar, Philox4x32-10, dönüşümler |
| `src/` | Modeller, kuyruk, arıza zinciri, WGS-84 dönüşümü |
| `test/` | CTest `sim.sensor.*`; `test_butunleme.cpp` gerçek çekirdeği bağlar |

Fizik anlık görüntüsü (`ida::fizik::DurumAnlik`) yalnız başlıktan kullanılır; kitaplık `ida_fizik`'e ve Eigen'e bağlanmaz.

## Zaman

- IMU kontrol sınırlarında (`t = k · 5 ms`), pencere `[t − 5 ms, t]`, değer adım sınırlarındaki yamuk ortalaması; `t_teslim = t_meas`.
- Periyodik sensörler: ideal an `round(k · 10⁹/f · (1 + ppm·10⁻⁶))`, en yakın adım sınırına bağlanır (yarıda yukarı).
- Basınç: örnekleme olayı D1'i başlatır; `t_meas` = D1 sonu, `window_ns` = dönüşüm penceresi; kayıt fiziksel bitişin (`t_meas + T_D2 + T_okuma`) üst adım sınırında üretilir; `t_teslim` = bu üretim sınırı (fiziksel bitiş 19,75 ms, teslim 20 ms; W4-K9).
- Batarya: pencere = örnek aralığı, `t_meas` pencere sonu.
- DVL: gönderim adım sınırında; kilitliyken `t_meas = t_tx + r̄/c`, rapor `t_tx + 2r̄/c + T_işlem`, kayıt raporun üst sınırında; hız `t_meas` anındaki durumun ara değeridir. `T_iletim` seri profilde 120 bayt × 10 / baud [T]. `altitude` dönüştürücüden yatay tabana dik mesafedir (A4 §3.3, W4-K8; 05 §2.3.5'in `r·cos α` ortalaması kullanılmaz).
- `t_hazir_ns` her zaman bir adım sınırıdır; her kayıtta `t_meas ≤ t_hazir ≤ t_teslim` (W4-K9: model teslimi daha erken hesaplarsa teslim üretim sınırına çekilir).
- Model içi örnek sayacı 64 bittir (RNG sayacı `n`, A5-K110); teldeki `seq` onun 2³² kalanıdır. Arıza çekilişleri A5 Ek D.4 gereği teldeki `seq`'i kullanır.

## GNSS yüksekliği

`alt_mm` ortalama deniz seviyesine göredir (A4 §3.3): `alt = h_elipsoit − N`. Jeoit yüksekliği `N` sensör kümesinin ortam ayarıdır (`SensorOrtami::jeoit_yuksekligi`, varsayılan 0 m; kabul, sim dünyasında jeoit modeli yok; W4-K7). Kurulum |N| ≤ 200 m ister.

## Arıza zinciri ve temsil

- Arıza pencereleri, zaman desenleri ve sürüklenmeler kaydın değişmez temel ölçüm zamanıyla değerlendirilir; `donma_zaman_damgasi`'nın donmuş damgası yalnız çıktı başlığına yazılır.
- Zincirden sonra, binary32'ye daraltmadan önce bütün yük alanları (kanallar, DVL kovaryansı ve `fom`, `eph`, `epv`, GNSS enlem/boylam/yükseklik) sonlu ve tel tipinin aralığında olmalıdır. Değilse kayıt teslim edilir ama `VALID` = 0 olur ve değerler son geçerli temel kayıttan (yoksa 0) alınır; `Sayaclar::aralik_reddi` artar (05 §4'ün sonlu olmayan çıktı kuralıyla aynı).

## Fırlatıcı ve emniyet geri okuması (W4-K4, K5; EMNİYET)

Kısıtlayan anahtar değişimi gecikmesiz, gevşeten değişim `anahtar_gecikmesi_ns` sonra algılanır. Örneğin `safety_rev`'i gösterdiği durumun revizyonudur: bekleyen gevşetme yoksa güncel revizyon; varsa en erken bekleyen gevşetmeden önceki adımdaki yetkili revizyon. Gevşetme beklerken gecikmesiz algılanmış bir kısıtlama varsa (karışık durum) bu eski revizyon gönderilir: kart o revizyonla izin açamaz (kısıtlayan okuma). `PERMIT_OPEN` örnek anındaki değerdir.

## Süre denetimi

Kurulumda bütün `_ns` alanları A5-K48'e göre denetlenir (süreler ≥ 0, zaman sabitleri ve periyotlar > 0) ve üstten `2⁵³` ns ile sınırlanır (sim zamanıyla toplamlar int64'te taşmasın); `window_ns`'e giden pencereler `u32` sınırını aşamaz. `arm` aynı sınırları arıza sürelerine uygular (`zaman_ns ≤ 2⁶²`, `sure_ns ≤ 2⁶¹`).

## Kanal numaraları (A5 Ek D.4 `kanal`; arızalar bu kanallara uygulanır)

| Tür | Kanallar |
|---|---|
| IMU | 0–2 gyro, 3–5 accel, 6 temp_c |
| Manyetometre | 0–2 field |
| Basınç | 0 p_abs, 1 temp_c |
| GNSS | 0–2 anten konumu N, E, D (orijine göre, m; WGS-84'e arızadan sonra çevrilir), 3–5 vel_ned |
| DVL | 0–2 vel, 3–6 beam_dist, 7 altitude |
| Batarya | 0 voltage, 1 current, 2 temp_c |
| Sızıntı, emniyet | kanalsız: sayısal arızalar `arm`'da reddedilir |

## Rastgele çekilişlerin yerleri (A5-K84, K110)

Alt sistem adı `sensor/<ad>/<akış>`; sayaç `(n, c2 = kanal<<24 | bilesen<<16 | cekilis)`; normal değer `z0` (w0, w1). Tablodaki `seq`, model içi 64 bitlik örnek numarasıdır (2³²'de sarmaz; teldeki `seq` bunun kalanı).

| Sensör | Akış | `n` | kanal / bileşen | Çekiliş |
|---|---|---|---|---|
| IMU | `acilis` | 0 | kanal 0–5; bileşen 0 açılış sapması, 1 sıcaklık katsayısı, 2 ölçek, 3–4 eksenler arası (satır içinde sırayla), 5 doğrusalsızlık, 6 GM başlangıcı | N(0, (sınır/3)²) |
| IMU | `olcum` | örnek `seq` | kanal 0–5; bileşen 0 beyaz gürültü, 1 GM yeniliği | |
| IMU | `olcum` | adım no | kanal 0–5; bileşen 2 | süzgeç açıkken adım gürültüsü |
| Manyetometre | `olcum` | `seq` | kanal 0–2, bileşen 0 | gürültü |
| Basınç | `acilis` | 0 | kanal 0 ofset, kanal 1 sıcaklık ofseti | |
| Basınç | `olcum` | `seq` | kanal 0 | gürültü |
| GNSS | `acilis` | 0 | kanal 0–2 bileşen 1: GM başlangıcı; kanal 16+j: uydu j yükselmesi (sin e düzgün) | |
| GNSS | `olcum` | `seq` | kanal 0–2 bileşen 0 beyaz, bileşen 1 GM; kanal 3–5 hız; kanal 16+j geçiş bandında kayıp | |
| GNSS | `zamanlama` | edinim no | bileşen 0 ilk fix süresi (log-normal), bileşen 1 yakınsama yönü | |
| DVL | `acilis` | 0 | kanal 0 | uzun dönem ölçek |
| DVL | `olcum` | ping `seq` | kanal 0–3 (ışın) | ışın hızı gürültüsü |
| DVL | `zamanlama` | ping `seq` | bileşen 0 sonraki ping hızı, bileşen 1 TCP gecikmesi | |
| Batarya | `olcum` | `seq` | kanal 0 gerilim, 1 akım | analog gürültü |
| Sızıntı | `olcum` | `seq` | kanal = prob | sahte pozitif |

Arızalar `ariza/<kimlik>` ve `ariza/<kimlik>/gecis` akışlarını A5 Ek D.4–D.5'teki yerlerle kullanır.

## Bu dilimin varsayımları (tahmin; ölçüm değil)

Gövde içi sıcaklık modeli (05 §2.3: 2000 J/K, 2 W/K, aviyonik güç); manyetometre itici girişiminin yönü (itki eksenli dipol, itici başına tam gazda oran·‖B‖/8, iki yönde aynı); GNSS uydu yükselme maskesi 10°, edinim için en az dört kuru uydu, ilk fix log-normal yayılımı 0,3; DVL ilk arama modu 2, mod 4 üst ucu 50 m, rapor satırı 120 bayt, yakın mod denemesi yalnız irtifa o modun aralığındayken; basınç ölçüm aralığı 0–30 bar. Batarya akımı fizik güç modeli yokken itki isteğinden (04 §2.7.4 yasası) ve sabit bara geriliminden türetilir.
