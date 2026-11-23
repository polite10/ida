# sim/sil — kartsız koşucu (`ida_sil`)

Ortak çekirdeği (`ida_core`) ve `ida_link` üst katmanını (`ida_link_ust`) PC'de, kartın yerine çalıştırır. Sim sunucusuna kartla aynı A1 mesajlarıyla, 127.0.0.1 üzerindeki TCP ile istemci olarak bağlanır (A1-K97 … K101). Kart yazılımıyla aynı protokol modülünü ve aynı çekirdeği bağlar; fark yalnız platformdadır. Görev: `docs/gorevler/W4-sil.md`.

## Derleme

PC kökünde (`host-*` ön ayarları) `sim/CMakeLists.txt` üzerinden derlenir; dış bağımlılık yoktur (Windows'ta Winsock, `ws2_32`).

```
cmake --preset host-debug
cmake --build --preset host-debug --target ida_sil
```

Program: `build/<ön ayar>/sim/sil/ida_sil`.

## Komut satırı

```
ida_sil --a1-port <port> [seçenekler]
```

| Seçenek | Anlam | Varsayılan |
|---|---|---|
| `--a1-port <1..65535>` | Sim sunucusunun A1 portu (A1-K97); zorunlu | — |
| `--a1-seed <16 onaltılık hane>` | `IDA_BOARD_HELLO.nonce` tohumu (A1-K114; sim A5'in `a1/board_tohum` alt akışından verir) | 0 |
| `--depo <yol>` | Kalıcı depo dosyası: açılış sayacı ve parametre bloğu | bellek içi depo |
| `--n-bg <sayı>` | Her step ve tick'ten önceki arka plan çağrısı sayısı `N_bg` (A1-K121 (iii)) | 1 |
| `--gunluk <0..3>` | stderr günlüğü: 0 sessiz, 1 hata, 2 bilgi, 3 ayrıntı | 1 |
| `--esc <alt,notr,ust>` | ESC darbe değerleri, µs (`alt < notr < ust`) | 1100,1500,1900 (`scenarios/ida0.toml`) |
| `--onceki-acilis <sayı>` | Depo dosyası yokken önceki `boot_count` (A1-K114 kalıcı başlangıç durumu) | 0 |
| `--prev-sess <sayı>` | Başlangıç `prev_sess` (A1-K114) | 0 |
| `--oturum-sayisi <sayı>` | Başlangıç `sessions_since_boot` (A1-K114) | 0 |
| `--last-cmd-id <sayı>` | Başlangıç `last_cmd_id` (A1-K114) | 0 |
| `--bekleme-ms <sayı>` | Bağlantı yokken en uzun bekleme (duvar saati) | 10000 |
| `--olcum` | Çıkışta çağrı süresi istatistiği (stderr) | kapalı |
| `--surum` | Sürümü yazar, çıkar | — |
| `--yardim`, `-h` | Kullanım metnini yazar, çıkar | — |

Örnek: `ida_sil --a1-port 47001 --a1-seed b1536801d0cf436c --depo calisma/sil-depo.bin --n-bg 1`

### Çıkış kodları

| Kod | Anlam |
|---|---|
| 0 | Olağan bitiş: durduruldu ya da bağlantı bitti ve `--bekleme-ms` içinde yeniden kurulamadı (sim kapandı) |
| 1 | Komut satırı hatası |
| 2 | Çekirdek kurulamadı: `ida_core_info` boyu 64 KB'lık alanı aşıyor, hizalama ya da port sürümü uyuşmuyor, `ida_core_init` `VERSION`/`BAD_INPUT` döndü ya da `ida_link_ust_kur` reddetti |
| 3 | Güvenli bekleme: çekirdek `health.core_ok = 0` bildirdi (A4-K89); oturum `SAFE_STATE` ile biter, çekirdek bir daha çağrılmaz |
| 4 | Ağ başlatılamadı ya da ilk bağlantı `--bekleme-ms` içinde kurulamadı |

## Davranış

- **Döngü.** Soketten okunan baytlar çerçeve sınırlarında bölünür (ust'ün alıcısıyla aynı tel katmanı ve aynı bayt akışı üzerinde bir ön alıcı). Her çerçeve için: `ida_link_ust_rx` → `poll_tx` → `take` / arka plan / `ida_core_step|tick` / `put_out` döngüsü → üretilen çerçeveler sokete yazılır. Çağrı penceresinde (take ile put_out arası) okuma yapılmaz; bu sürede gelen baytlar işletim sisteminin soket tamponunda bekler (W3-K10).
- **Saat.** Platform saati sim zamanıdır: protokol modülünün kabul ettiği (partiyi kapatan) son zaman çerçevesinin `t_sim_ns`'i (A4-K11, A1-K121). Kabul modülün `batch_closed` sayacından okunur; modülün reddettiği TF (DUP, OLD, oturum dışı) saati ve zamanlayıcıları ilerletmez. Her çekirdek ömrü (açılış, REBOOT, sim zamanının geri başlaması) saati 0'dan başlatır (W4-K10). Çekirdeğe verilen `now_w` bir ömür içinde azalmaz (A4-K09).
- **Alım damgası (A4-K14).** Çağrı girdisindeki bütün alım damgaları (zaman çerçevesi, IMU, öteki örnekler, algılama partisi ve kaydı, Ping360) çağrının `now_w`'sidir; girdi `take` ile çekirdek çağrısı arasında tamamlanır. Izgara tick'i, `T_bağ` ve duyuru yoktur (A1-K100; ust'ün SIL rolü). Çekirdeğe giden `prev_call_ns` 0 ("ölçülmedi"), `prev_overrun` ve `tx_stalled` 0'dır: ölçülen süre çıktıya girmez.
- **Arka plan.** Her step ve tick'ten önce tam `N_bg` `ida_core_background` çağrısı; oturum sonu çağrısından önce hiç (A1-K121 (iii), A4-K95); `bg_slices` A4-K109'a göre sayılır.
- **Nonce.** `IDA_BOARD_HELLO.nonce` = A5-K118 ile `ida/a1/nonce_board/<boot_count>/<deneme>` etiketinden, tohum `--a1-seed` (`<deneme>` her açılışta 0'dan; A1-K114).
- **Bağlantı.** Kopunca (ya da sim bağlantıyı kapatınca) oturum biter (`ida_link_ust_akis_kapandi`), oturum sonu çağrısı yapılır, koşucu `--bekleme-ms` boyunca yeniden bağlanmayı dener. Soket engellemesizdir: karşı taraf okumazken gönderim bekler ve durdurma isteği bu beklemeyi en geç ~50 ms içinde keser (kısmi yazılan çerçeveyle bağlantı bırakılır).
- **REBOOT (A1-K17).** `SESSION_ACCEPT(REBOOT)` alınınca çekirdek yeni `boot_count` ve `reset_cause = SIL_REBOOT` ile yeniden kurulur, parametre bloğu depodan yüklenir, saat 0'dan başlar (W4-K10); bağlantı sürer.
- **Sim zamanının geri başlaması (A4-K11).** Bir oturumun modülce kabul edilen ilk TF'si saatten küçükse koşucu o partiyi çekirdeğe vermez (bağlantı kapanınca modül bekleyen partiyi siler), bağlantıyı kapatır (oturum iki tarafta biter, A1-K99), çekirdeği yeni `boot_count` ile kurar ve yeniden bağlanır.
- **Belirlenimcilik.** Çıktı baytları yalnız alınan bayt akışına ve başlangıç durumuna (tohum, depo, `N_bg`, komut satırı) bağlıdır; TCP parçalanmasına ve duvar saatine bağlı değildir (A1-K102). `sim.sil.belirlenim` testi bunu kayıtlı akışın farklı parçalanmasıyla sınar.

## Kalıcı depo

Kartta flash'ın yaptığını dosyayla yapar: açılış sayacı (A4-K86) ve `PARAM_SAVE` bloğu (A4-K82 … K84). Yazım kesintiye dayanıklıdır (A4-K84): içerik `<yol>.yeni`ye yazılır ve aygıta kadar kalıcılaştırılır (macOS `F_FULLFSYNC`, desteklenmezse `fsync`; Windows `FlushFileBuffers`), geri okunup karşılaştırılır, asıl dosyanın üzerine adlandırılır (POSIX `rename`; Windows `MoveFileExW` `REPLACE_EXISTING | WRITE_THROUGH`) ve POSIX'te dizin kalıcılaştırılır. Herhangi bir adım başarısızsa `OK` verilmez; adlandırmadan önceki bir hatada eski dosya aynen kalır. Başlangıçta `plat.param_gen` 0'dır (A4 §3.6, W4-K10); saklı bloğun kuşağı `kalici_blok` bilgisindedir (`BOARD_HELLO.param_gen`). Kayıt sonucu sonraki çağrının `plat.param_store` / `param_fail` / `param_gen` alanlarındadır (`OK`; yazılamazsa `FAIL`/`FLASH`, geri okuma farklıysa `FAIL`/`VERIFY`, blok `IDA_PARAM_BLOB_MAX`'ı aşıyorsa `FAIL`/`TOO_BIG`; çağrı dönerken oturum bittiyse `FAIL`/`ABORTED`).

Dosya biçimi (küçük uçlu; bu koşucunun seçimi, sözleşme değildir):

| Bayt | Alan |
|---|---|
| 0–7 | `IDA-SIL1` |
| 8–11 | sürüm = 1 |
| 12–15 | bayraklar: bit 0 = sayaç geçerli |
| 16–19 | son verilen `boot_count` |
| 20–23 | CRC-32/ISO-HDLC (0–19) |
| 24–27 | blok var (0/1) |
| 28–31 | `gen` |
| 32–35 | `len` |
| 36–39 | CRC-32/ISO-HDLC (blok) = `param_blob_crc` (A1-K68) |
| 40–43 | CRC-32/ISO-HDLC (24–39) |
| 44– | blok |

Blok kesimi bozuksa varsayılanlarla açılır (`init_rc = PARAM_DEFAULTS`), sayaç geçerli kalır. Sayaç kesimi bozuk ya da dosya tanınmıyorsa `boot_count_ok = 0` verilir (olay kimliği kapalı; atış izni olayı üretilemez) ve dosya silinene kadar öyle kalır.

## Testler

`ctest --preset host-debug -R sim.sil`: türetme vektörleri (A5 Ek D.1, D.2, D.7), CRC, depo, komut satırı, sahte sim ile oturum (1000 parti, kopma ve yeniden bağlanma), komut yolu, parametre deposu (REBOOT ve yeniden başlatma dahil), çağrı penceresi, belirlenimcilik, sim zamanının geri başlaması ve programın ayrı süreç olarak koşusu.
