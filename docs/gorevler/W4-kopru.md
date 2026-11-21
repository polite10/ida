# W4-kopru — sim sunucusunun A1 köprüsü, zaman yöneticisi ve başsız `ida_sim`

Tarih: 2026-11-21. Yazan: `ida-uygulayici` (Opus). Bağımsız kabul testleri: `ida-test-yazari` (emniyete dokunan kısım: olayların uygulanması ve güvenli çıktı; iş bittikten sonra). İnceleyen: sol. Dal: `is/w4-kopru`. Ortak kurallar ve mimar kararları: `docs/gorevler/W3-ortak.md` (W3-K1…K14).

## Amaç

Sim sunucusunun kartla (ve kartsız koşucuyla) konuşan yarısını yazmak ve fizik, sensörler ve köprüyü tek bir başsız programda (`ida_sim`) birleştirmek. Sonuç: Z1 senaryosu artık aynı süreçte değil, **A1 mesajları üzerinden** koşar; karşı taraf önce kartsız koşucu (`ida_sil`), sonra kart (Z2) olur.

Okunacaklar: iskelet §2, §3 (A1 satırı; kapasite), §3.2 (sürekli komutlar ve tek seferlik olaylar; fizik sınırındaki işlem sırası), §4 (zaman, `L_act`, bariyer); A1 sözleşmesinin sim tarafı yükümlülükleri (el sıkışma ve sürüm/`dialect_hash` denetimi A1-K14 ve çevresi; zaman çerçevesi ve parti içeriği; sensör mesajları ve Ping360 parçalama hariç; aktüatör komutunun uygulanması A1-K74–K78; olayların uygulanması ve sonuçları; oturum sonu; SIL kuralları A1-K121); A5'in zaman ve kip alanları; `docs/mimari/06-protokol-ve-hil.md` §2.5–§2.9 ve `04-simulator-cekirdegi.md` §2.11–§2.12 (fikir kaynağı); `sim/sensor/include/ida/sensor/sensor_kumesi.hpp` ve `olcum.hpp` (ölçümler; `bosalt(t_n)` ile parti kurulur; `t_hazir`/`t_teslim` karta gitmez); `sim/fizik/include/ida/fizik/dunya.hpp`; tel katmanı (`protocol/ida_link`, sim rolü); `tests/zincir/` (Z1 senaryosu ve platform taklidi; `main`'e henüz girmediyse `origin/is/w3-zincir` dalında, yalnız oku).

## Kapsam

1. **Köprü (`sim/kopru/`):** sim rolünde oturum (duyuruyu dinleme, `SIM_HELLO`, sürüm ve `dialect_hash` denetimi, kabul/ret, oturum sonu, yeniden kurulum); her kontrol periyodunda zaman çerçevesi + o periyodun ölçümleri (IMU, manyetometre, basınç, GNSS, DVL, batarya, sızıntı, emniyet örneği) tek parti olarak; kart çıktılarının alınması (aktüatör, durum, seyrüsefer, günlük, komut sonuçları, olaylar, sayaçlar); komut gönderme (operatör komutları için arayüz: `ARM`, `DISARM`, `SET_MODE`, `GUIDED_SP`, parametre komutları).
2. **Aktüatör uygulaması:** alınan komut A1'deki uygulama anı (`L_act`), geçerlilik süresi ve geç yanıt kuralıyla fiziğe verilir; süre dolunca nötr; eşzamansız güvenli çıktı anında uygulanır (A1-K44, K74–K78).
3. **Sanal emniyet girdileri ve fırlatıcının yetkili durumu:** sim'de tutulur (acil durdurma, fiş, pim, tüp dolu/boş, süreli izin); senaryodan değişir; sensör kümesine `dis_durum_ver` ile verilir. Kart olayları (`SAFETY_CLOSE`, `SAFETY_OPEN`, `FIRE`): A1'deki sıra, tekillik (`boot_id`, olay kimliği), son geçerlilik ve yetkili durum koşullarıyla uygulanır ya da reddedilir; her olaya sözleşmedeki sonuç gönderilir. Kısıtlayan olay aynı adımda genişletenden önce uygulanır. **Torpido fiziği, ayrılma ve isabet bu işte yoktur:** kabul edilen `FIRE` yalnız "tüp boşaldı" yetkili durumunu ve geri okumasını üretir; acil durdurma iticilerin ve fırlatıcının enerjisini keser (iskelet §5).
4. **Zaman yöneticisi:** iki kip. *Kartsız (SIL):* kilitli adım; sim bir kontrol periyodunun fiziğini koşturur, partiyi gönderir, karşı tarafın o partiye yanıtını bekler (bariyer; sim zamanında zaman aşımı ve sözleşmedeki "eksik yanıt" davranışı), belirlenimcidir ve olabildiğince hızlı koşar. *Kartlı (HIL 1x):* duvar saatine kilitli 200 Hz; yanıt beklenmez, geç yanıt kuralı uygulanır; gerçek zamanı kaçıran adımlar sayılır.
5. **Taşıma:** yerel TCP sunucusu (SIL; yalnız 127.0.0.1) ve seri port (HIL; 3 000 000 baud; macOS ve Windows dalları; `tools/olcum/mp3`'teki PC tarafı kanıtlanmış ayarları temel al).
6. **`ida_sim` (`sim/sunucu/`):** fizik + sensör kümesi + köprü + zaman yöneticisi; yapılandırma şimdilik koddan (İDA-0 ve örnek havuz varsayılanları) ve komut satırından (kip, port ya da seri aygıt, tohum, süre, senaryo adı); yerleşik senaryo `z1` (W3-zincir'deki Z1: nötr bekleme, `ARM`, `GUIDED`, 1,5 m ve +90°, manevra ve itici arızası, `DISARM`); çıktı: her kontrol adımında gerçek durum, kartın bildirdiği durum ve seyrüsefer, uygulanan darbe genişlikleri, oturum ve bariyer sayaçları CSV olarak.

## Kapsam dışı

A2 ve A3 sunucuları, kayıt (A6), senaryo dosyası okuma (A5 çözümleyici), bağlantı emülatörü (gecikme/kayıp), protokol düzeyinde hata enjeksiyonu, sonar ve kamera, torpido fiziği, kart yazılımı ve `ida_sil`'in kendisi.

## Dosya sahipliği

Yazılır: `sim/kopru/**`, `sim/sunucu/**`, `tests/butunleme/**` (iki süreçli testler), `sim/CMakeLists.txt` ve `tests/CMakeLists.txt`'e birer satır. `sim/fizik`, `sim/sensor`, `sim/sil`, `core/`, `protocol/`, `firmware/` değiştirilmez; gereken en küçük değişikliği öner ve raporla.

## Kabul

1. **Oturum:** süreç içi bir karşı tarafla (testte: `ida_link` üst katmanı + gerçek çekirdek, bellek içi taşıma) el sıkışma, 10 000 parti, kopma ve yeniden kurulum, yanlış `dialect_hash`'te ret.
2. **Parti içeriği:** her partide tam bir zaman çerçevesi ve o periyoda ait ölçümler; `t_meas` ve örnek sıraları A4 kabul kapısından geçer (çekirdeğin reddettiği örnek sayısı 0).
3. **Aktüatör zamanlaması:** komutun fiziğe uygulandığı fizik adımı sözleşmedeki ana eşit; geç yanıt, süre dolumu ve güvenli çıktı vakaları.
4. **Olaylar ve emniyet (EMNİYET):** yetkili durum izin vermiyorken hiçbir olay fırlatıcı durumunu değiştirmez; yinelenen olay bir kez uygulanır; süresi geçmiş olay uygulanmaz; acil durdurma etkinken iticilere giden komut ne olursa olsun itki sıfır. Belirsizlikte kısıtlayan okuma.
5. **Z1, A1 üzerinden, kartsız:** `ida_sim --kip sil` ile karşı taraf arasında (önce süreç içi karşı taraf; `ida_sil` `main`'e ya da `origin/is/w4-sil`'e girdiyse iki süreç olarak da) Z1 senaryosu W3-zincir'deki sınırlarla geçer; aynı süreçteki Z1 sonuçlarıyla farkı raporlanır (fark beklenir: gerçek sensör modelleri ve mesaj nicemlemesi).
6. **Belirlenimcilik (SIL):** aynı tohumla iki koşu aynı CSV.
7. **HIL kipi (kartsız doğrulama):** seri port yerine sözde uçbirim (pty) çiftiyle ya da TCP ile 1x kipte 60 s: gerçek zaman kaçırma sayacı ve periyot dağılımı (ölçüm, bu Mac). Kartla koşu (Z2) bu işin kabulü değildir; kart yazılımının ikinci dilimi bitince ayrı bütünleme adımıdır, ama `ida_sim --kip hil --seri <aygıt>` yolu hazır olmalıdır.
8. `host-debug`; W3-K14 taraması; görev modülü `main`'e henüz girmediyse (W3-zincir) `ARM` reddedilir ve Z1 koşamaz: o durumda kendi dalına `origin/is/w3-zincir`'i birleştir ve raporla.
