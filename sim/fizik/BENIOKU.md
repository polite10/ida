# sim/fizik — fizik çekirdeği (`ida_fizik`)

Başsız ve belirlenimci 6 serbestlik dereceli fizik kitaplığı (W3-fizik, dilim 1; görev belgesi `docs/gorevler/W3-fizik.md`). İş parçacığı, dosya, saat ve ağ kullanmaz. Dayanak: iskelet §3.1, §4, §5; `docs/mimari/04-simulator-cekirdegi.md` §2.1–§2.4, §2.7, §2.10; A5 §3.2–§3.5, §3.9, §3.13, Ek A.

| Dosya | İçerik |
|---|---|
| `include/ida/fizik/parametreler.hpp` | `AracParametreleri`, `OrtamParametreleri`, `BaslangicDurumu` (alan adları A5 anahtarlarıyla aynı), `SayisalAyarlar`; `ida0_arac()`, `havuz_ortami()` |
| `include/ida/fizik/kutle_durumu.hpp` | Üç kütle durumunun türetilmesi (A5-K23, K24) |
| `include/ida/fizik/dunya.hpp` | `Dunya`: kurulum, `adim()`, `itici_komutu_ver(adim_no, pwm_us)`, `durum()` anlık görüntüsü |
| `src/` | Hidrostatik (kapalı ağ kırpma), itici ve ESC modeli, hareket denklemi ve RK4 |
| `deneme/ida_fizik_deneme.cpp` | Açık çevrim PWM profilini koşturup CSV yazan araç |
| `test/` | CTest testleri (`sim.fizik.*`), kabul maddeleri 1–9 dahil |

Dilim 1'de yok: dalga ve rüzgâr gövdesi (kurulum bunları reddeder), çarpışma ve temas (havuz tabanı yok), torpido ayrılması, batarya ve güç modeli (bara gerilimi `arac.batarya.v_sabit`), sensörler, zaman yöneticisinin gerçek zaman kısmı. Yüzerlik geometrisi A7 gelene kadar 04 §2.6'daki yer tutucu kutu ve tüplerdeki torpidolar için sferoid ağlarıdır.

Deneme: `build/host-release/sim/fizik/ida_fizik_deneme --sure 120 --cikti cikti.csv` (yerleşik profil) ya da `--profil profil.csv` (her satır `t_s, pwm0, …, pwm7`, kanal sırası, µs).
