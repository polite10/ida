# W4-sil — kartsız koşucu (çekirdeği PC'de, kartın yerine çalıştıran platform)

Tarih: 2026-11-21. Yazan: `ida-uygulayici` (Opus). Bağımsız kabul testleri: yok (emniyet davranışı çekirdekte ve protokol modülünde sınandı; bu iş ince bir platformdur). İnceleyen: sol (tek tur). Dal: `is/w4-sil`. Ortak kurallar ve mimar kararları: `docs/gorevler/W3-ortak.md`.

## Amaç

Ortak çekirdeği ve `ida_link` üst katmanını PC'de, kartın yerine çalıştıran `ida_sil` programını yazmak: sim sunucusuna kartla **aynı A1 mesajlarıyla**, yerel soket üzerinden bağlanır (iskelet §2, §3 A1 satırı). Kart yazılımıyla aynı protokol modülünü ve aynı çekirdeği bağlar; farkı yalnız platformdur (soket, sim zamanından türeyen saat).

Okunacaklar: iskelet §2, §3 (A1, A4, "A1 ile A4 arasındaki dönüştürücü"), §4 (zaman; SIL'de saatler sim zamanından türer); A1 sözleşmesinin SIL'e özgü kuralları (A1-K121 ve çevresi; SIL taşıması, akış kapanması, T_bağ ve ızgara tick'inin yokluğu); A4'ün platform kuralları (`plat` alanları, `prev_call_ns`, `bg_slices`, kalıcı parametre deposu, olay kimliği için açılış sayacı); `protocol/ida_link/include/ida_link/ust.h` (çağrı akışı ve W3-K10: `take` ile `put_out` arasında `rx` çağrılmaz); `docs/mimari/02-ortak-cekirdek.md` §2.8; `firmware/app/cagirici.c` (kart tarafındaki çağırıcının yaptıkları; `main`'de yoksa `origin/is/w3-kart` dalında, yalnız oku).

## Kapsam

1. **Platform:** `ida_link_platform_t` işlev tablosu: `zaman` (sim zamanından; A1-K121), `gonder` (sokete; üç sınıflı kuyruk gerekmez, ama zorunlu sınıfta yazılamayan çerçeve protokol modülüne `false` olarak bildirilir), `kuyruk_temizle`, `nonce`, `yeniden_baslat`, `kalici_blok` (dosya tabanlı kalıcı parametre deposu ve açılış sayacı; yolu komut satırından).
2. **Ana döngü:** soketten oku → `ida_link_ust_rx` → `take` → `plat` alanlarını doldur → `ida_core_step`/`tick` → `put_out` → `poll_tx`; W3-K10 penceresine uyulur (çağrı sürerken okunan baytlar bekletilir). Arka plan dilimi (`ida_core_background`) A4 kurallarıyla.
3. **Taşıma:** yerel TCP (yalnız 127.0.0.1), istemci olarak bağlanır ve kopunca yeniden dener; akış kapanınca `ida_link_ust_akis_kapandi`. macOS ve Windows (Winsock) dalları.
4. **Parametre deposu:** A4'ün kayıt protokolü (`param_store`, `param_gen`, `FAIL` dahil; A4-K83) dosyayla; atomik yazma.
5. **Komut satırı:** bağlanılacak port, depo yolu, günlük düzeyi; `--surum`. Çıkış kodları belgeli.
6. **Belirlenimcilik:** aynı bayt akışıyla iki koşu aynı çıktı akışını üretir (duvar saatine bağlı hiçbir davranış yok).

## Kapsam dışı

Sim tarafı (köprü, bariyer, sensörler); girdi tekrarı (kayıttan çekirdek çağrısı yeniden kurma; A6); kart yazılımı.

## Dosya sahipliği

Yazılır: `sim/sil/**` (program ve testleri), `sim/CMakeLists.txt`'e tek satır. `core/`, `protocol/` ve `firmware/` değiştirilmez.

## Kabul

1. **Sahte sim ile oturum (CTest):** testteki sahte sim (tel katmanının sim rolündeki kodlayıcısıyla; `protocol/ida_link/test/oturum/destek_oturum.*` örnek alınabilir, kopyalanmaz: kendi küçük desteğini yaz) gerçek TCP üzerinden bağlanır; el sıkışma, 1000 parti (zaman çerçevesi + IMU + öteki örnekler), her partiye bir aktüatör çıktısı; kopma ve yeniden bağlanma; oturum sonu.
2. **Komut yolu:** `ARM`, `SET_MODE`, `DISARM` uçtan uca (sonuç mesajlarıyla); daraltıcının öncelik kuralı protokol modülünde sınandığı için burada yalnız duman düzeyinde.
3. **Parametre deposu:** `PARAM_SET` → `COMMIT` → `SAVE` → programı yeniden başlat → değer ve kuşak korunur; bozuk dosyada varsayılanlara düşüş ve doğru `init_rc`.
4. **W3-K10:** çağrı sürerken gelen baytların pencere dışında işlendiği (ihlal sayacı/sorgusu sıfır) bir testle gösterilir.
5. **Belirlenimcilik:** aynı kayıtlı bayt akışıyla iki koşunun çıktı baytları aynı.
6. `host-debug`; W3-K12 gerekmez (C++/platform; `core/` ve `protocol/` dışı) ama W3-K14 taraması zorunlu (Winsock dalı hiç derlenmedi: bilinen tuzaklara dikkat). Bir çağrı turunun süresi (ölçüm, bu Mac).
