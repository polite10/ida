# sim/sunucu — başsız sim sunucusu `ida_sim`

Fizik (`sim/fizik`) + sensör kümesi (`sim/sensor`) + A1 köprüsü (`sim/kopru`) + zaman yöneticisi (görev: `docs/gorevler/W4-kopru.md`). Yapılandırma şimdilik koddan (İDA-0, örnek havuz) ve komut satırından gelir; senaryo dosyası (A5) okunmaz. Fizik Eigen'e bağlı olduğundan vcpkg gerekir (`VCPKG_ROOT`); yoksa dizin atlanır.

Program: `build/<ön ayar>/sim/sunucu/ida_sim`.

## Çalıştırma

```
# SIL, süreç içi karşı taraf (ida_link + gerçek çekirdek), Z1, 90 s, CSV:
ida_sim --kip sil --karsi ic --senaryo z1 --tohum 1 --csv z1.csv

# SIL, iki süreç: ida_sim TCP dinler ve koşucuyu başlatır (A1-K97, K114):
ida_sim --kip sil --karsi tcp --a1-port 0 --sil build/host-debug/sim/sil/ida_sil --csv z1.csv

# SIL, koşucu dışarıdan: ida_sim portu ve --a1-seed'i stderr'e yazar
ida_sim --kip sil --karsi tcp --a1-port 47001
ida_sil --a1-port 47001 --a1-seed <yazılan değer>

# HIL 1x, kart (ST-LINK sanal seri portu, 3 000 000 baud):
ida_sim --kip hil --seri /dev/cu.usbmodemXXXX --csv z2.csv

# HIL 1x, kartsız sınama: sözde uçbirim + kart rolünde süreç içi karşı taraf (POSIX):
ida_sim --kip hil --karsi pty --sure 60
```

Seçenekler: `--sure <s>` (90), `--tohum <n>` (1; sensör gürültüsü ve A1-K114 türetimleri), `--senaryo z1|bos`, `--ariza <k1,k2|yok>` (Z1 itici arızası; varsayılan 4,0), `--acil <s>` ve `--acil-kalk <s>` (sanal acil durdurma), `--gunluk 0..2`. Çıkış kodu: 0 tamam, 1 komut satırı, 2 çalıştırma hatası.

## Zaman yöneticisi

- **SIL (kilitli adım; A1-K121):** bir kontrol periyodunun fiziği koşar, parti gönderilir, karşı tarafın o partiye yanıtı (yanıt + sayılan olaylar, A1-K73) gelene kadar sim zamanı ilerlemez; `T_SIL` = 5 s duvar gözetimi (A1-K76). Protokol saatleri sim zamanıyla sayılır. Belirlenimcidir: aynı tohumla iki koşu aynı CSV.
- **HIL 1x:** `W(t) = W0 + t`; sınır b `W(b·h) − C_sim`'de hesaplanır (C_sim = 0,4 ms, tahmin), sensör üyeleri hazır oldukça, TF(n) `W(t_n)`'den önce değil ve ardışık iki TF arası ≥ P/2 yazılır (A1-K39); yanıt beklenmez, a_n'de yuva boşsa parti kaçırılır (A1-K75); sim gecikmesi, son tarih kaçırması (A1-K108) ve TF periyotları ölçülür.
- **Hazırlık (A1-K18, K19):** kartın parametre bloğu A5'tekinden farklıysa (ya da `init_rc` ≠ OK) fizik yürütülmeden hazırlık oturumu açılır: bütün parametreler için `PARAM_SET`, `PARAM_COMMIT`, `PARAM_SAVE`; ardından `RUN_END` ve `REBOOT`'lu yeni el sıkışma.
- **Acil durdurma (iskelet §5; W3-K1, W4-K3):** etkinken fiziğe giden itici girdisi, köprüden gelen komut ne olursa olsun nötrdür; izinler kapanır. Etkinleştiği sınırda köprü yürürlükteki komutu ve bekleyen yuvaları iptal eder; kalktıktan sonra itki yalnız kalkıştan sonra gönderilen bir partinin yanıtıyla gelir.

## CSV

Her kontrol adımında bir satır (`csv_basligi()`): gerçek durum (derinlik, yön, yalpa, baş-kıç), kartın bildirdiği seyrüsefer (son `IDA_NAV`), kartın yanıtındaki ve yürürlükteki ve fiziğin uyguladığı darbe genişlikleri, kip/silahlı/arıza düzeyi, oturum ve kaçırma sayaçları, uçtan uca yanıt süresi (`yanit_sure_us`: TF yazımından o partinin yanıtının alınmasına, duvar; HIL'de anlamlı), kartın son `IDA_FC_STATUS`'undaki durum ve engel maskesi (`fc_state`, `fc_inhibit`; yalnız gözlem) ve sim'in yetkili izin bitleri (`sim_izin`). HIL özetinde yanıt süresinin p50/p99/en çoğu ve karttan gelen akışın çalıştırma oturumundaki çerçeve hataları ayrıca yazılır. Kart tarafı alanları bir parti sonra doldurulur (yanıt ve telemetri o ana kadar gelmiştir).
