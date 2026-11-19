# Açık bulgular kaydı

Durum: 2026-11-14 tarihli inceleme durumu. Kaynak: `docs/mimari/inceleme/` altındaki sol incelemeleri ve `docs/mimari/01`–`09` belgelerinin "8. Açık noktalar ve iskelete öneriler" başlıkları. Bu belge toplama ve etiketlemedir; çözüm içermez, önemi değiştirmez.

Dalga etiketleri 09 sürüm 0.2'ye göre güncellendi (ölçümler W0, sözleşmeler W1).

2026-11-19: W0 ölçüm bulguları (AB-234…) eklendi; K17 ile kapsam dışına çıkan bulgular işaretlendi.

## 1. Nasıl kullanılır

- Kayıt, mimari bölümlerin (01–09) incelemelerinde kapanmamış bulguları ve bölümlerin kendi açık noktalarını tek yerde toplar. İnceleme ve düzeltme döngüsü yakınsamadığı için mimar döngüyü kesti: bulgular belgelerde değil, A1–A8 sözleşmeleri yazılırken ve uygulama işleri yapılırken kapatılır.
- Her bulgu, kapatılacağı işe göre bir grupta durur (3. bölüm). Grup adları `docs/mimari/09-test-ve-is-akisi.md` §2.9'daki iş adlarıyla uyumludur. Bir iş, kendisine atanan bulguları kapatmadan bitmiş sayılmaz (09 §2.2, §7.2).
- Bir bulgu kapandığında satırı silinmez; Not sütununun sonuna "KAPANDI: iş adı, commit" yazılır. Mimarın "kabul edilen sınır" kararı da aynı biçimde gerekçesiyle yazılır.
- Kaynak sütunu: bölüm numarası, dosya ve madde. `NN-sol-2.md` ikinci inceleme, `NN-sol.md` ilk inceleme (`Y` yeni bulgu, `#` madde numarası); `NN-xxx.md §8` bölümün kendi açık noktaları; "notlar" inceleme dosyasındaki "İskelete ya da diğer bölümlere notlar" başlığıdır. İki incelemenin ya da inceleme ile bölüm notunun aynı sorunu bildirdiği satırlar tek satırda birleştirilmiştir.
- İkinci incelemesi olan bölümlerde (01, 02, 04, 05, 06) açık sayılanlar: ilk incelemedeki "kısmen" ve "giderilmedi" maddeleri ile "Yeni bulgular" tablosunun tamamı. "giderildi" ve "yazar haklı" olanlar alınmadı. 03 ve 07 için "Düzeltilmesi gerekenler" tablosunun tamamı açık sayıldı. 08 ve 09 incelenmediği için tek kaynakları kendi §8 başlıklarıdır.
- Önem sütunu, inceleme dosyasındaki önemdir; ilk ve ikinci inceleme birleştirilen satırlarda ikinci incelemenin önemi yazılır, ilk incelemedeki önem Not'ta belirtilir. İncelemede önem verilmeyen satırlar (notlar tablosu, bölümlerin kendi açık noktaları) "belirtilmemiş" yazılır.
- Kimlikler (`AB-nnn`) bu belgede kalıcıdır; yeni bulgu sona eklenir, mevcut kimlik yeniden numaralanmaz.
- Önem tanımları ve inceleme sınırı için bkz. `docs/mimari/09-test-ve-is-akisi.md` §2.4.

## 2. Özet

**Bölüm başına**

| Bölüm | Kritik | Önemli | Küçük | Belirtilmemiş | Toplam |
|---|---:|---:|---:|---:|---:|
| 01 Kart yazılımı | 8 | 16 | 2 | 13 | 39 |
| 02 Ortak çekirdek | 2 | 11 | 0 | 24 | 37 |
| 03 Atış kontrol | 10 | 18 | 2 | 3 | 33 |
| 04 Simülatör çekirdeği | 0 | 5 | 3 | 10 | 18 |
| 05 Sensörler ve sonar | 3 | 16 | 0 | 6 | 25 |
| 06 Protokol ve HIL | 6 | 17 | 1 | 6 | 30 |
| 07 Görselleştirme | 9 | 17 | 2 | 9 | 37 |
| 08 Laboratuvar uygulaması | 0 | 0 | 0 | 13 | 13 |
| 09 Test ve iş akışı | 0 | 1 | 0 | 5 | 6 |
| **Toplam (açık)** | **38** | **101** | **10** | **89** | **238** |
| Kapsam dışı (ayrı, 5. bölüm) | 2 | 2 | 0 | 5 | 9 |
| **Genel toplam** | **40** | **103** | **10** | **94** | **247** |

**Kapatılacağı yere göre (açık bulgular)**

| Kapatılacağı yer | Kritik | Önemli | Küçük | Belirtilmemiş | Toplam |
|---|---:|---:|---:|---:|---:|
| A1 — Sim sunucusu ↔ kart | 13 | 21 | 0 | 15 | 49 |
| A2 — Sim sunucusu ↔ görselleştirici | 4 | 2 | 0 | 2 | 8 |
| A3 — Sim sunucusu ↔ web panelleri | 0 | 0 | 0 | 3 | 3 |
| A4 — Ortak çekirdek ↔ çevresi | 5 | 8 | 0 | 13 | 26 |
| A5 — Senaryo ve araç tanımı | 0 | 2 | 0 | 9 | 11 |
| A6 — Kayıt | 0 | 2 | 0 | 4 | 6 |
| A7 — Sahne ve varlıklar | 0 | 1 | 0 | 1 | 2 |
| A8 — Sim sunucusu ↔ kamera algılama bileşeni | 1 | 5 | 0 | 0 | 6 |
| Uygulama: kart yazılımı | 1 | 10 | 2 | 4 | 17 |
| Uygulama: ortak çekirdek — kestirim, güdüm, kontrol | 2 | 8 | 0 | 4 | 14 |
| Uygulama: atış kontrol, emniyet zinciri ve arayüzler | 4 | 14 | 2 | 3 | 23 |
| Uygulama: fizik çekirdeği | 1 | 5 | 3 | 7 | 16 |
| Uygulama: sensör modelleri | 3 | 10 | 1 | 5 | 19 |
| Uygulama: HIL köprüsü, bariyer, bağlantı emülatörü | 0 | 1 | 0 | 0 | 1 |
| Uygulama: görselleştirme — Godot sahnesi ve GDExtension | 4 | 10 | 2 | 1 | 17 |
| Uygulama: laboratuvar uygulaması | 0 | 0 | 0 | 1 | 1 |
| Uygulama: test ve derleme altyapısı | 0 | 1 | 0 | 4 | 5 |
| Ölçüm işleri | 0 | 1 | 0 | 13 | 14 |

Yeri belirsiz olarak işaretlenen bulgu sayısı: 3 (AB-103, AB-206, AB-213); Not sütununda "yer belirsiz" yazar.

W0 ölçüm bulguları AB-234…AB-247 (14 bulgu) bu sayılara dahildir. K17 ile kapsam dışına çıkan AB-109, AB-115 ve AB-116 satırları 3. bölümde yerinde durur, Not sütununda "KAPSAM DIŞI (K17)" yazar ve yukarıdaki açık sayılara hâlâ dahildir (önem dağılımı: AB-109 önemli, AB-115 önemli, AB-116 belirtilmemiş). Bu üçünü çıkarınca açık sayı kritik 38, önemli 99, küçük 10, belirtilmemiş 88, toplam 235 olur.

## 3. Bulgular (kapatılacağı yere göre)

Gruplar: arayüz sözleşmeleri A1–A8, uygulama işleri, ölçüm işleri. Bir bulgu birden çok yerde kapanacaksa en çok ilgili gruba konmuş, diğer yer Not sütununda belirtilmiştir. Kapsam dışı bulgular 5. bölümdedir.

### A1 — Sim sunucusu ↔ kart (MAVLink 2 + özel dialect; W1, ölçüm sonuçlarıyla tek aşamada)

49 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-001 | 01 · 01-sol-2.md Y7 | kritik | Reset ya da kilitlenmede "darbe yok" nötr komut sayılıyor; PWM kaybında `son_değer` seçeneği varken iticilerin nötrleşmesi garanti değil. | 04 ve 06 bu kurala göre değişecek; A5 ESC sinyal kaybı seçimiyle eşleşmeli |
| AB-002 | 01 · 01-sol-2.md Y8; 01-sol.md #12 | kritik | Taşıma temizliği (TX FIFO/DMA iptali, eski tamamlanmaların ayıklanması, dönem bilgisi) uygulanabilir biçimde tanımlı değil. | iki ayrı kayıttan biri; diğeri A4 (oturum sonu izin iptali) |
| AB-003 | 01 · 01-sol-2.md Y10; 01-sol.md #3 | önemli | Alım damgası kaybında geri çağrı anının kullanılması gecikmiş veriye yeni alım zamanı verebilir; ZLP, iptal edilmiş aktarım ve damga kalitesi ele alınmamış. | USB/VCP damga uygulaması; ilk incelemede #3 kritik |
| AB-004 | 01 · 01-sol-2.md Y12; 01-sol.md #13 | önemli | Komut yuvalarının hepsi daraltıcı komutlarla doluyken yeni daraltıcı komutun korunması tanımsız. | daraltıcı etkiler A4'e de aktarılacak; ilk incelemede #13 önemli |
| AB-005 | 01 · 01-sol-2.md notlar (00-iskelet §3.2, §4) | belirtilmemiş | Algoritmik kontrol tetikleyicisi ("IMU gelişi") ile taşıma sonlandırıcısı (parti kapanışı) ayrı sorumluluk olarak yazılmalı. | A1 ve A4'te ayrı kayıtlar |
| AB-006 | 01 · 01-sol-2.md notlar (05 §2.2) | belirtilmemiş | Güncel sensör zamanlamasına göre kritik pencere boyu, olay anında değişen emniyet geri okumaları ve 200 Hz yedek yeniden türetilmeli; 06'daki eski tablo otomatik doğru sayılamaz. | A1 zamanlama/boyut sözleşmesi |
| AB-007 | 01 · 01-kart-yazilimi.md §8.3 md.2 | belirtilmemiş | `HELLO`'daki `boot_id` ve `boot_count` kartta aynı değeri taşıyor; iki alanın ayrı anlamı varsa tanımlanmalı. | — |
| AB-008 | 02 · 02-sol-2.md notlar (06 ile kritik uyumsuzluklar); 04 · 04-sol-2.md notlar (06 ve IMU kaybı) | belirtilmemiş | 06'da parti sonlandırıcısı yalnız IMU ve zaman yalnız örnek damgalarıyla taşınıyor; bağımsız zaman çerçevesi yok, IMU'suz SIL bariyeri ilerleyemiyor. | 06 değişmeli |
| AB-009 | 02 · 02-sol-2.md notlar (06 ile kritik uyumsuzluklar); 04 · 04-sol-2.md notlar (06 ve tek seferlik olaylar); 03 · 03-sol.md notlar (komşu belgeler: 06) | belirtilmemiş | 06'da `IDA_ACTUATOR.launcher` seviye, atış yükselen kenar olarak tanımlı ve `event_id` yok; iskelet §3.2 ve 04'ün `OneShotEvent` sözleşmesiyle çelişiyor. | 06 değişmeli; 02'nin önbellekten yeniden gönderilen çıktıları da aynı kimlikli olay düzenine bağlanmalı |
| AB-010 | 02 · 02-sol-2.md notlar (06 ile kritik uyumsuzluklar) | belirtilmemiş | 06'da DUP yanıtında önceki aktüatör çerçevesi yeniden gönderiliyor; 02'nin güncel güvenlikle yeniden üretme kuralına çevrilmeli. | 06 değişmeli |
| AB-011 | 02 · 02-sol-2.md notlar (06 ile kritik uyumsuzluklar) | belirtilmemiş | 06'nın ortak sensör başlığında üretici örnek sıra numarası yok; `step_seq` 02'nin sensör başına `seq` denetiminin yerini tutmuyor. | 06 değişmeli |
| AB-012 | 02 · 02-sol-2.md notlar (06 ile kritik uyumsuzluklar) | belirtilmemiş | 06'da basınç damgası "dönüşüm sonu" ve IMU kesilmesi 10 ms; ölçüm zamanı iskeletle, bayatlık eşiği 02'deki 20 ms ile eşitlenmeli. | 06 değişmeli |
| AB-013 | 02 · 02-ortak-cekirdek.md §8.3 (06) | belirtilmemiş | Zaman çerçevesinin alanları, `src=TICK` eş zamansız güvenli çıktının ve `event_id` tekrar elemesinin A1 karşılığı ve sürekli komut kullanım süresinin alıcıda uygulanması 06'dan bekleniyor. | — |
| AB-014 | 03 · 03-sol.md #3 | kritik | İptal edilen yetkiye bağlı bekleyen atış olaylarının akıbeti tanımsız. | kart, 06 ve sim birlikte |
| AB-015 | 03 · 03-sol.md #4 | kritik | Aynı adımdaki daraltma ile eski genişletici olayların etkileşimi için alıcı kuralı eksik. | olay kuralları |
| AB-016 | 03 · 03-sol.md #5 | kritik | Olay kimliğindeki açılış sayacının sahibi, üretimi ve yeniden bağlantıdaki tekilleştirme davranışı tanımsız. | 01 Y9 (A4) ile birlikte; 06'daki rastgele `boot_id` tanımıyla uyumsuz |
| AB-017 | 03 · 03-sol.md #6 | kritik | Olay geçerlilik süresi yalnız atış olayında var; izin ömrünün başlangıcı ve sim/duvar zamanı ayrımı belirsiz. | — |
| AB-018 | 03 · 03-sol.md #18; 05 · 05-sol-2.md notlar (Ping360) | önemli | Sonar kutu merkezi 03'te (i+½)Δr, 05'te iΔr; örnek anı sözleşmesi eşitlenmemiş. | 05 ve 06 ile |
| AB-019 | 03 · 03-sol.md notlar (komşu belgeler: 05, 06) | belirtilmemiş | Sonar son 8 bit çıktısının anlamı, gönderim/yansıma pozunun kullanımı ve sade/tam kademedeki darbe yanıtı 03 ve 05 arasında birleştirilmeli; 06'da atış olay önceliği, geçerlilik, oturum, ACK ve geri okuma mesajları eklenmeli. | 05 ve 06 değişmeli |
| AB-020 | 04 · 04-sol-2.md notlar (06'nın bariyeri) | belirtilmemiş | 06'nın atıf yaptığı `ActuatorFrame.t_sim_ns` 04'te yok (`t_ölçüm_ns` var); ölçüm zamanı, planlanan ve gerçekten uygulanan adım ayrı anlamlarla eşlenmeli. | 04 ve 06 birlikte |
| AB-021 | 04 · 04-simulator-cekirdegi.md §8 md.4 | belirtilmemiş | Bariyerin iki fizik adımda bir kurulması, bariyer bütçesi ile fizik adım süresinin ve giriş/çıkış fazının eşlenmesi, `komut_ömrü` ile kart tarafındaki çıkış ömrünün uyumu beklenen. | 04 ve 06 birlikte |
| AB-022 | 05 · 05-sol-2.md Y3; 05-sol.md #4 | önemli | Çok yollu ekoda kutu zamanından türetilen an gerçek saçılma anı değil; nominal doğrudan-yol zamanı olarak tanımlanmalı. | ilk incelemede #4 kritik; iskelet §3.1 açıklaması; A6 yan verisi |
| AB-023 | 05 · 05-sol-2.md Y4; 05-sol.md #2, #3, #16; 06 · 06-sol-2.md notlar (05 §2.2, §4) | önemli | Fiziksel bitiş, ızgaradaki üretim ve modellenmiş teslim ayrılmadığı için `t_hazır` ve `t_teslim` ilişkisi çelişiyor (ör. basınçta 19,75 ms teslim farkı). | ilk incelemede #2 ve #3 kritik; A5 doğrulayıcısındaki 25,97 Hz sınırı bundan türetilecek |
| AB-024 | 05 · 05-sol-2.md Y14; 05-sol.md #23 | önemli | Ping360 tek ping isteği yinelenebilir CONFIG içinde tanımlı (çoğaltılan komut ek ping üretebilir) ve parametre sınırları A1'e bağlanmamış. | A1 komut sözleşmesi; HEAD alanları değişirse boyut ve bant bütçeleri yeniden hesaplanmalı |
| AB-025 | 05 · 05-sol-2.md Y15; 05-sol.md #29 | önemli | Ping360 menzil hata sınırı yalnız yuvarlamaya ait; [80, 40000] kırpması durumunda geçerli değil. | A1 Ping360 yapılandırma sözleşmesi |
| AB-026 | 05 · 05-sol-2.md notlar (06 §2.2, §3, §8) | belirtilmemiş | 06'da 05'in hâlâ orta damgalı olduğunu söyleyen eski dönüşüm notları kalmış (çift düzeltme riski); Ping1D hâlâ gönderim anında. | 06 değişmeli |
| AB-027 | 06 · 06-sol-2.md Y1; 06-sol.md #2, #12 | kritik | ACTUATOR yanıtı kaç olay beklendiğini bildirmediği için olayların bariyer içinde tamamlanması belirlenemiyor; son tarihten önce gelecek olay uygulanacağı fizik adımını kaçırabilir. | yeni alanlar bütçeye eklenecek; ilk incelemede #2 kritik |
| AB-028 | 06 · 06-sol-2.md Y2; 06-sol.md #2 | kritik | Yalnız uygulanan olay kimlikleri tutuluyor; emniyet nedeniyle reddedilen olayın ACK'si kaybolup yeniden gelirse sonradan uygulanabilir, bekleyen olayların yinelenmesi de tanımsız. | ilk incelemede #2 kritik |
| AB-029 | 06 · 06-sol-2.md Y3; 06-sol.md #2 | kritik | İzin olaylarının sıralanması, nesli ve iptali tanımsız: eski `ARM_CHANGE(1)` daha yeni kapatmadan sonra yeniden açabilir ve açılmış iznin düşeceği süre yok. | 03'teki süreli izin ayrı etkinlik süresi olarak taşınacak; ilk incelemede #2 kritik |
| AB-030 | 06 · 06-sol-2.md Y4; 06-sol.md #7, #8 | kritik | Sonar profili ilk uygun partiye atanırken parçaların dört toplu pencereye yayılması kapanmış partiye düşüyor; profil hedef partisi ile çok pencereli teslim birlikte uygulanamıyor. | ilk incelemede #7 ve #8 önemli |
| AB-031 | 06 · 06-sol-2.md Y5; 06-sol.md #5, #7 | önemli | Aynı anahtar ve indisli tekrar koşulsuz yineleme sayılıyor; farklı içerikli tekrar bayt karşılaştırmasına ulaşmadan atılıyor ve `n_members≤64` / `idx<64` ret kuralları yok. | ilk incelemede #7 önemli |
| AB-032 | 06 · 06-sol-2.md Y6; 06-sol.md #1 | önemli | TF kaybında sonraki TF'ye güveniliyor; SIL bariyerinde sonraki TF hiç gelmeyebilir ve HIL'de büyük TF boşluğunda bütün sonraki TF'ler reddedilebilir. | SIL koşucusu ile birlikte |
| AB-033 | 06 · 06-sol-2.md Y7; 06-sol.md #3 | önemli | Sürekli komut ömrü uygulama anında başlıyor; bariyersiz kipte çok gecikmiş yanıt yeni 50 ms ömür kazanıyor, `CMD_VALID=0` davranışı belirsiz. | 04 ile ortak kabul/uygulama sözleşmesi; ilk incelemede #3 kritik |
| AB-034 | 06 · 06-sol-2.md Y8; 06-sol.md #12 | önemli | Gecikme koşulu `W(t_n)` başlangıcından, ölçülen RTT ise `e₁` sonrasından hesaplanıyor; `e₁−W(t_n)` dışarıda kalıyor. | M1 ölçüm tanımı da buna göre; ilk incelemede #12 önemli |
| AB-035 | 06 · 06-sol-2.md Y9; 06-sol.md #10 | önemli | 3 Mbaud / 200 Hz tam kritik yükte seri yol 2,963 ms tutuyor ve 2,90 ms taşıma payına, 1 ms köprü payıyla sığmıyor. | "200 Hz yedek yeterli" sonucu koşullu yazılacak; M1/M5 ölçümü; 01 Y16 ile birlikte |
| AB-036 | 06 · 06-sol-2.md Y10; 06-sol.md #9, #10 | önemli | Kritik bütçe yalnız 59 B yanıtı alıyor; aynı adımın olayları, tekrarlar, güvenli eş zamansız çıktılar ve SAFETY_IO değişim patlamaları sınırlanmamış. | ilk incelemede #9 ve #10 önemli |
| AB-037 | 06 · 06-sol-2.md Y11; 06-sol.md #9, #13 | önemli | Bayt ortalaması tam çerçevenin pencereye sığacağını göstermiyor (ör. 199 B pencerede 267 B DATA) ve gönderim kuyruğunun zamanında boşalması garanti edilemiyor. | ilk incelemede #9 ve #13 önemli |
| AB-038 | 06 · 06-sol-2.md Y12; 06-sol.md #14 | önemli | El sıkışmada aynı HELLO'nun yinelenmesi oturumu bitiriyor; ilk TF kaybı ve kartın reset sonrası HELLO üretimi geçiş tablosunda yok. | ilk incelemede #14 kritik |
| AB-039 | 06 · 06-sol-2.md Y13; 06-sol.md #14 | önemli | Oturum kimliği seçimi eski oturum kimliğiyle çakışmayı önlemiyor; veri mesajlarında `run_id` ya da açılış kimliği yok. | ilk incelemede #14 kritik |
| AB-040 | 06 · 06-sol-2.md Y15; 06-sol.md #21 | kritik | SAFETY_IO mesajında 03'ün istediği `emniyet_açık` ve bakım girdisi yok; olay ACK'sinin A4 üzerinden FC'ye dönüşü eşlenmemiş, atış zinciri tamamlanamıyor. | A4 ile; ilk incelemede #21 önemli |
| AB-041 | 06 · 06-sol-2.md Y16; 06-sol.md #21 | önemli | DETECTION'da `t_sigma_ns` ve `calib_ver` alanları yok; Ping1D `t_meas=t_tx` iskeletin yansıma anı gereğiyle çelişiyor. | 06 "bekleniyor" ifadesi güncel değil; 03'ün RB2/B2/RB3 ve SENSOR çerçevesi kuralları eşlenecek; ilk incelemede #21 önemli |
| AB-042 | 06 · 06-sol-2.md Y17 | önemli | `rx_wall_us` u32 damgası yaklaşık 71,58 dakikada sarıyor; TIMESYNC ofset kestirimi sonrasında yanlış sonuç verir. | — |
| AB-043 | 06 · 06-sol-2.md Y18; 06-sol.md #16 | önemli | Farklı minor sürümlerde `hash_full` uyuşmazlığı otomatik kabul ediliyor; uzantı uyumluluğu append-only kuralına bağlanmamış ve testler 8 bit `CRC_EXTRA` değişimi bekliyor. | T2 şema özeti testi; ilk incelemede #16 önemli |
| AB-044 | 06 · 06-sol.md #6; 06-sol-2.md ilk-inceleme #6 | kritik | `(step_seq, idx)` kaynaklı komut kimliği, aynı GCS komutunun sonraki partide yeniden teslimini tekilleştirmiyor; 32 bit kimlik üretim kuralı yok. | 02 §3.5 ve 06 §3 komut kimliği kapsamını birlikte tanımlamalı |
| AB-045 | 06 · 06-sol-2.md ilk-inceleme #24 ve DIAG/FC_STATUS satırları; 06-sol.md #24; 01 · 01-kart-yazilimi.md §8.3 md.1 | önemli | DIAG ve FC_STATUS mesajlarının kesin alan listesi, tipleri ve `IDA_DIAG` grup/bayt düzeni yok, yalnız üst sınır var; özel FC komut ve yanıtlarının tipleri de verilmemiş ve `BAD_INPUT` için ayrı bayrak gerekliliği belirsiz. | — |
| AB-046 | 06 · 06-protokol-ve-hil.md §8 (açık noktalar) | belirtilmemiş | `sysid` değerleri, mesaj kimliği aralığı, şema özetinin kanonik metin kuralı, altı eksenli elle kumandada `MANUAL_CONTROL`'ün yeterliliği ve c_library_v2'nin örnek başına durumla kullanımı sözleşme işinde sabitlenip doğrulanacak. | — |
| AB-047 | 07 · 07-sol.md notlar (06 ile komşu) | belirtilmemiş | 06 en çok 4 algılama/kare, 07 ve 03 en çok 8 kabul ediyor; 06'daki 120 kayıt/s ve 10.200 B/s hesabı sekiz kayıtla (85 B çerçevede 20.400 B/s) aynı kalamaz. | 06 değişmeli; `t_sigma_ns` ve `calib_ver` eklenince yeniden hesaplanmalı |
| AB-234 | MP1 · docs/olcumler/MP1-arac-zinciri.md §4 md.6; §3.1 | önemli | ST-LINK sanal seri portu, PC portu kapalıyken gelen veriyi kısmen saklıyor, kısmen kaybediyor (ilk turda yeni imajın açılış satırları ve ilk 3–5 T satırı gelmedi); oturum açılışı buna göre tanımlanmalı. | A1 oturum açılışı; MP3 portu kart sıfırlanmadan önce açık tutmalı |
| AB-235 | MP4 · docs/olcumler/MP4-dongu-yuku.md §4.2, §4.3, §5, §7 | kritik | Yer tutucu çekirdek adımı `-O2` ağır yükte (B) 400 Hz'e sığmıyor: en büyük 3322,38125 µs (2500 µs periyodun %132,9), 241.000 adımın tamamı periyodu aştı; olağan yükte (A) en büyük 1338,91875 µs, aşım 0. 200 Hz'te B `-O2`: en büyük 3321,96875 µs (5000 µs'nin %66,4), aşım 0/121.000. Sonuç yer tutucuya ve ön bilgiye aittir; gerçek çekirdekle W3 sonunda yinelenmeden kesin girdi değildir. | iskelet §8 kontrol hızı kararı (M5); A1 ve A4 (zaman ve bütçe ilişkileri) |

### A2 — Sim sunucusu ↔ görselleştirici (W1)

8 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-048 | 01 · 01-sol-2.md notlar (07); 05 · 05-sol-2.md notlar (03, 07) | belirtilmemiş | 07'deki `VizState` içinde torpido itki durumu gibi iskelet 0.7 öncesinden kalan alanlar temizlenmeli. | 07 de değişmeli |
| AB-049 | 07 · 07-sol.md #6 | kritik | Kamera halkasında çoklu okuyucu (algılama ve kayıt) protokolü yok; ilk okuyucu yuvayı boşaltınca ikincinin verisi ezilebilir ve kontrol alanı salt okunur eşlemeden değiştirilemez. | A8 okuyucusu da etkilenir |
| AB-050 | 07 · 07-sol.md #7 | kritik | `SlotRelease` aynı nesildeki yuva yeniden kullanımını ayıramıyor; yinelenen ya da gecikmiş bırakma yeni kareyi serbest bırakabilir ve sahiplenme atomik değil. | A8 okuyucusu da etkilenir |
| AB-051 | 07 · 07-sol.md #8 | kritik | Algılama ya da kayıt süreci çökerse tuttuğu yuvaların kurtarılması ve görselleştirici yeniden bağlanırken halkanın sıfırlanması güvenli tanımlı değil. | A8 ile; süreç öldürme testleri |
| AB-052 | 07 · 07-sol.md #11; 04 · 04-sol-2.md Y7 (ilgili) | önemli | "Son iki durum" tamponu ve olay adımında tek snapshot, olay sınırının iki tarafını ve yayın gecikmesini karşılamıyor. | 04 Y7 ile aynı konu |
| AB-053 | 07 · 07-sol.md #12; 05 · 05-sol-2.md notlar (07 §2, §3) | kritik | Kamera gecikmesi pozlama sonrası gecikme olduğu halde pozlama ortasına ekleniyor ve 05'te pozlama sonu, 07'de pozlama ortası olan ölçüm zamanı algılama kaydına değiştirilmeden kopyalanıyor; teslim ve kart alım anı ayrıştırılmamış. | A8 algılama eşlemesi; 05, 06 ve 07 `t_capture` tanımı eşitlenecek |
| AB-054 | 07 · 07-sol.md #18 | önemli | Sade kademede görüntü–fizik tutarsızlığı için üst veri şemasında `dalga_görseli` alanı yok; yarı batık kamerada tek ortam uygulanması deney geçerliliğini belirsizleştiriyor. | — |
| AB-055 | 07 · 07-sol.md notlar (04 ile komşu) | belirtilmemiş | `VizState` hızları, süreklilik bilgisi ve olay öncesi/sonrası durumlar eksik. | — |

### A3 — Sim sunucusu ↔ web panelleri (W1)

3 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-056 | 08 · 08-laboratuvar-uygulamasi.md §8 (11.2, 06'dan beklenen iii) | belirtilmemiş | Ön denetim raporunun A3 şeması tanımsız. | — |
| AB-057 | 08 · 08-laboratuvar-uygulamasi.md §8 (11.3) | belirtilmemiş | Çoklu istemci için kontrol sahipliğinin devri ve "izleyici" rolünün kapsamı için kullanıcı kararı yok. | — |
| AB-058 | 08 · 08-laboratuvar-uygulamasi.md §8 (11.3) | belirtilmemiş | Foxglove SDK'nın canlı yayın için kullanılıp kullanılmayacağı çözülmemiş. | 00 A2, K3 kapsamı |

### A4 — Ortak çekirdek ↔ çevresi (`port`; W1)

26 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-059 | 01 · 01-sol-2.md Y1; 01-sol.md #6; 01 · 01-kart-yazilimi.md §8.2 md.1 | kritik | İç sağlık ara kuralı çekirdeğin iç tutarlılık hatasını yakalamıyor; sağlık nedenleri (iç tutarlılık / veri kullanılabilirliği) ayrılmadan gözetim emniyetli uygulanmış sayılamıyor. | 02 §4.4 ve 03 sağlık katkısı eşitlenecek; ilk incelemede #6 kritik |
| AB-060 | 01 · 01-sol-2.md Y8; 01-sol.md #12; 01 · 01-kart-yazilimi.md §8.2 md.5 | kritik | Oturum sonunda çekirdeğin arm, FC jetonu ve izin durumunu hangi çağrıyla iptal ettiği bağlanmamış, yalnız öneri düzeyinde. | iki ayrı kayıttan biri; diğeri A1 (taşıma temizliği) |
| AB-061 | 01 · 01-sol-2.md Y9; 01-sol.md #1; 01 · 01-kart-yazilimi.md §8.2 md.3 | önemli | Olay sayacı tükenmesi, `ARM_CHANGE(0)` ifadesi ve açılış sayacı kayıt politikası A4'te açık durum olarak bağlanmamış. | ilk incelemede #1 kritik |
| AB-062 | 01 · 01-sol-2.md ilk-inceleme #3; 01-sol.md #3; 02 · 02-sol-2.md Y7; 02-sol.md #9 | kritik | Düzenli teslim edilen ama gecikmiş (bayat) IMU akışı sıra ve ilerleme denetimlerini geçiyor; bağımsız zaman çerçevesiyle azami fark denetimi yok. | 06'da saat eşleme/gecikme varsayımı A1'de; 01 Y10 ile birlikte okunmalı |
| AB-063 | 01 · 01-sol-2.md ilk-inceleme #16; 01-sol.md #16 | önemli | Parametre kaydı (BUSY) sürerken arm isteğinin reddi 02'nin bağlayıcı sözleşmesinde yok. | Y14 ile aynı ilk inceleme maddesinin kalanı |
| AB-064 | 01 · 01-sol-2.md Y15, ilk-inceleme #2, #22; 01-sol.md #22; 02 · 02-sol-2.md Y6; 02-sol.md #11 | kritik | Reddedilen girdi dönüşlerinde (DUP_SEQ/OLD_SEQ/BAD_INPUT) aynı çağrıdaki DISARM/FC_ABORT gibi daraltıcı komutların uygulanması garanti değil; OLD_SEQ için yanıt kuralı da çelişik. | 02 erken dönüş yolları değişecek; iki inceleme aynı sorunu bildiriyor; ilk incelemede 01 #22 önemli, 02 #11 kritik |
| AB-065 | 01 · 01-sol-2.md ilk-inceleme #22; 01-sol.md #22; 01 · 01-kart-yazilimi.md §8.2 md.2; 02 · 02-ortak-cekirdek.md §8.1 md.6 | önemli | Yeni `plat` alanları, `ida_telem_t` ve olay kayıtlarının alanları ile her hata dönüşünün çıktı anlamı A4'te henüz bağlayıcı değil. | — |
| AB-066 | 01 · 01-sol-2.md notlar (03 §2.7) | belirtilmemiş | FC olay kutusu ile `ida_link` tekrar tablosunun sahibi tekleştirilmeli; kapının düşürdüğü ya da taşımanın alamadığı olayın sonucu FC'ye dönmeli. | telde tekrar kısmı A1 |
| AB-067 | 01 · 01-kart-yazilimi.md §8.2 md.4 | belirtilmemiş | 06'nın güvenli/olağan sınıflaması ve platform kapısı için `ida_out_t`'de arıza eylemi düzeyi bulunmalı. | — |
| AB-068 | 01 · 01-kart-yazilimi.md §8.2 md.6 | belirtilmemiş | 02 §7'nin IMU bayatlığı sınırına kartın dağıtım gecikmesi (ızgara durumunda ≤0,35 ms) eklenmeli. | — |
| AB-069 | 02 · 02-sol-2.md Y9; 02-sol.md #14; 05 · 05-sol-2.md notlar (02 §3.2–3.3) | önemli | Basınç ve batarya ölçümleri aralık sonu ve süreyi, Ping1D gönderim alanını taşımıyor; ortak başlığın "geçerlilik anı" tanımı ile akustik yansıma anı dönüşümü ayrıştırılmamış. | iskelet §3.1 ile eşitlenecek; 05 ve 06 birlikte; ilk incelemede #14 önemli |
| AB-070 | 02 · 02-sol-2.md Y14; 02-sol.md #2 | önemli | Yeni IMU aralığı önceki örneğin sonundan başlamıyorsa (kayıp örnek) filtrenin yayılım, süreç gürültüsü ve sağlık davranışı tanımsız. | ilk incelemede #2 kritik; filtre davranışı çekirdekte, boşluk denetimi A4 girdisinde |
| AB-071 | 02 · 02-sol-2.md ilk-inceleme #13; 02-sol.md #13; 02 · 02-ortak-cekirdek.md §8.1 md.5 | önemli | Manuel girdinin anlamı, setpoint çerçeveleri ve hata dönüşlerinin emniyet semantiği tanımlı değil. | kesin telemetri yerleşimi ertelenebilir |
| AB-072 | 02 · 02-ortak-cekirdek.md §8.1 md.1 | belirtilmemiş | `Q_c` blokları, `L` matrisi, `P₀` değerleri ve başlangıç hizalamasının kabul ölçütü A4 sözleşmesinde tamamlanmalı. | — |
| AB-073 | 02 · 02-ortak-cekirdek.md §8.1 md.2 | belirtilmemiş | Manyetometre yön ölçümünün Jacobian'ı, eğim telafisi, sapma açısı ve geçerlilik koşulları tamamlanmalı. | — |
| AB-074 | 02 · 02-ortak-cekirdek.md §8.1 md.3 | belirtilmemiş | GNSS anten konumu ve hızı Jacobian'ları ile `eph`'in σ'ya dönüşüm ölçeği tamamlanmalı. | 05 ile |
| AB-075 | 02 · 02-ortak-cekirdek.md §8.1 md.7 | belirtilmemiş | `ida_fc_in_t`, `ida_fc_out_t`, `ida_fc_bg_in_t` ve `ida_fc_bg_out_t` alanları tamamlanmalı. | 03 ile birlikte |
| AB-076 | 02 · 02-ortak-cekirdek.md §8.1 md.8 | belirtilmemiş | Kapasitelerin son değerleri ve yapı boyut/hizalama `_Static_assert` değerleri tamamlanmalı. | — |
| AB-077 | 02 · 02-ortak-cekirdek.md §8.1 md.9 | belirtilmemiş | Parametre tablosunun tam listesi ve sınıfları ile kodlanmış kalıcı biçimin bayt düzeni tamamlanmalı. | — |
| AB-078 | 03 · 03-sol.md #28 | kritik | FC komut kapasitesi (4) ile 02'nin kapasitesi (8) uyuşmuyor; daraltıcı komutların taşmada kaybolma olasılığı ve diğer tamponların taşma politikaları eksik. | 01 Y12 ile birlikte |
| AB-079 | 03 · 03-sol.md #32; notlar (02 sürüm 0.2) | önemli | FC arayüzü ile 02 §3.7 olay alanları, ACK dönüşü, arka plan imzası ve olay üretimi sahipliği açısından tekleşmemiş. | — |
| AB-080 | 03 · 03-sol.md notlar (komşu belgeler: 02) | belirtilmemiş | 02'nin duvar çağrısı yolunda FC çağrısı görünmüyor; IMU'suz çağrının FC'ye nasıl ulaştığı yazılmalı. | — |
| AB-081 | 05 · 05-sol-2.md notlar (03, 06 geri okuma) | belirtilmemiş | 06 mesajı tüp emniyetini taşımıyor ve 02 port örneği eksik; aynı adımda periyodik ve değişim örneği eşit ölçüm zamanıyla gelirse güncel durumun reddedilmemesi için birleştirme/sıralama gerekir. | A1 ile tek ortak geri okuma sözleşmesi |
| AB-082 | 05 · 05-sol-2.md notlar (05 §2.3.8, 03, 08); 08 · 08-laboratuvar-uygulamasi.md §8 (11.2, 03 ↔ iskelet) | belirtilmemiş | 05 ve 03 iskelette olmayan `bakım` girdisini ekliyor; "kart bu durumları değiştiremez" ifadesi yalnız sanal girdilere özgülenmeli, kimlikli olayla değişen fırlatıcı emniyetiyle karıştırılmamalı. | A1 ile; 05 ve 03 kapsam eşlemesi; 08 üç girdiye uyuyor |
| AB-083 | 06 · 06-sol-2.md Y14; 06-sol.md #24 | önemli | Kalıcı açılış sayacının atomik artırımı, bozuk kayıt ve tükenme davranışı yok; olay sayacı sararak aynı açılışta kimlik tekrar üretebiliyor. | 01 Y9 ve 03 #5 ile birlikte; 01, 02 ve 06 birlikte; ilk incelemede #24 önemli |
| AB-084 | 06 · 06-sol-2.md notlar (02 §3.1–3.4) | belirtilmemiş | 02'de emniyet/tüp örneği, olay ACK yolu ve olayın son geçerlilik alanları eksik; kimlik hâlâ rastgele `boot_id` temelli. | 02 değişmeli (iskelet 0.6 ve tamamlanmış 03/06 sözleşmesine) |

### A5 — Senaryo ve araç tanımı (W1)

11 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-085 | 01 · 01-sol-2.md notlar (08) | belirtilmemiş | A5'te ESC sinyal kaybı davranış seçimi A1'de belirlenecek emniyet davranışıyla eşleşmeli. | A6'daki karşılığı ayrı satır |
| AB-086 | 02 · 02-ortak-cekirdek.md §8.2 md.1 | belirtilmemiş | Fizik gerçeği ile otopilot parametreleri A5'te iki ayrı küme olarak tutulmalı; aksi halde model hatasına dayanıklılık sınanamaz. | — |
| AB-087 | 02 · 02-ortak-cekirdek.md §8.2 md.3 | belirtilmemiş | Batarya eşikleri İDA-0 paketine (04) ve A19'a bağlı olarak belirlenmemiş. | 00 A19 |
| AB-088 | 04 · 04-simulator-cekirdegi.md §8 (açık noktalar) | belirtilmemiş | 30Q haritaları ve hücre kapasitesi X-57'den A5'e aktarılmadı; aktarılana kadar yalnız ideal kaynak var. | — |
| AB-089 | 05 · 05-sol-2.md Y8; 05-sol.md #21; 05 · 05-sensorler-ve-sonar.md §8 (Ainslie–McColm doğrulaması) | önemli | Tatlı su için S=0 konarak deniz suyu soğurma denkleminin geçerlilik aralığı belirtilmeden doğrulanmış tatlı su modeli gibi kullanılıyor. | A5 akustik ortam/profil sözleşmesi |
| AB-090 | 05 · 05-sol-2.md Y13; 05-sol.md #27 | önemli | Arıza modelinde teslim titreşimi nedensel alt sınırı aşabiliyor ve sıfır zaman sabiti sıfıra bölmeyi dışlamıyor. | A5 arıza parametre sözleşmesi |
| AB-091 | 05 · 05-sol-2.md notlar (08) | belirtilmemiş | A5 doğrulayıcısında sensör geçersiz kılmaları, pozitif süreç süreleri ve etkin sonar menzili şemada doğrulanmalı. | — |
| AB-092 | 08 · 08-laboratuvar-uygulamasi.md §8 (11.1 md.3) | belirtilmemiş | 04'ün A5 grup adları (`akıntı`, `rüzgar`, `fırlatıcı`, `araç`) dosyada ASCII karşılıklarıyla yazılmalı; ad eşlemesi sözleşmede dondurulmalı. | — |
| AB-093 | 08 · 08-laboratuvar-uygulamasi.md §8 (11.2, 04'ten beklenen ii) | belirtilmemiş | 04'ün `apply_fault` için `FaultSpec` alan listesi yazılı değil; 08'in `ArızaTanımı` eşlemesi buna bağlı. | 04 ile |
| AB-094 | 08 · 08-laboratuvar-uygulamasi.md §8 (11.2, 06'dan beklenen i) | belirtilmemiş | `FaultInjector` ve bağlantı emülatörü profil alanlarının A5 biçimi 08'de yalnız tablo satırı; alan listesi 06 ile tamamlanmalı. | — |
| AB-095 | 08 · 08-laboratuvar-uygulamasi.md §8 (11.2, 02'den beklenen) | belirtilmemiş | Otopilot parametre bloğunun A5'teki yeri ve `PARAM_*` dönüştürücüsü tanımsız. | — |

### A6 — Kayıt (W1)

6 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-096 | 01 · 01-sol-2.md notlar (08) | belirtilmemiş | A6 kayıt sözleşmesinde oturum, reset ve komut ömrü kararları A1'deki emniyet davranışıyla eşleşmeli. | A5'teki karşılığı ayrı satır |
| AB-097 | 03 · 03-sol.md #27 | önemli | Tekrar oynatmada arka plan işinin tüketildi/düştü kararı ve gerekli FC girdileri kayıt sözleşmesine bağlanmamış. | — |
| AB-098 | 04 · 04-sol-2.md notlar (A6) | belirtilmemiş | Kayıt tamponu taşarsa tekrar oynatma güvencesinin hangi kayıtlar için kaybolduğu açıkça işaretlenmeli. | — |
| AB-099 | 05 · 05-sol-2.md notlar (A6) | belirtilmemiş | Nominal kutu zamanı ile gerçek yol zamanı, modellenmiş teslim ile fiilî parti teslimi A6'da ayrı kaydedilmeli. | — |
| AB-100 | 06 · 06-sol-2.md Y20; 06-sol.md #20 | önemli | HIL tekrar testinde her koşuda kart yeniden açıldığı için kanonik izdeki ham olay kimlikleri farklı çıkıyor; karşılaştırma için normalizasyon ve girdi bağlama tanımsız. | ilk incelemede #20 önemli |
| AB-101 | 08 · 08-laboratuvar-uygulamasi.md §8 (11.2, 04'ten beklenen iii) | belirtilmemiş | A6 için `StepResult` sürümlü ve donmuş olmalı. | — |

### A7 — Sahne ve varlıklar (W1)

2 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-102 | 07 · 07-sol.md #21 | önemli | Türetilmiş dışbükey ağları depoya koyma önerisi iskelet A7 ile çelişiyor ve manifest türetilmiş çıktıların özetini listelemiyor; yeniden üretim özeti tutmazsa başarısızlık politikası yok. | CoACD çıktısı platformlar arası aynı olmayabilir |
| AB-103 | 07 · 07-gorsellestirme.md §8 (açık noktalar); 02 · 02-ortak-cekirdek.md §8.2 md.4; 09 · 09-test-ve-is-akisi.md §8.2 md.2 | belirtilmemiş | A16 (yapı zamanında Python) yanıtlanmazsa Blender dışa aktarma adımı ve A7 hattı yeniden düşünülmeli. | 00 A16; yer belirsiz |

### A8 — Sim sunucusu ↔ kamera algılama bileşeni (W1)

6 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-104 | 03 · 03-sol.md #15 | önemli | Algılama kaydında kare/parti kimliği yok; aynı karedeki birden çok algılama zaman ilerlemesi denetimine takılabiliyor. | A1 algılama mesajıyla birlikte |
| AB-105 | 07 · 07-sol.md #13; 03 · 03-atis-kontrol.md §8.4; 05 · 05-sensorler-ve-sonar.md §8; 07 · 07-gorsellestirme.md §8 (yer gerçeği maskesi) | önemli | Yer gerçeği sınırı çelişiyor: bileşene poz verilmediği söylenirken yapılandırma ve halka fizik gerçeği montajını taşıyor, anlamsal algılama üreteci aynı bileşene takılabiliyor. | — |
| AB-106 | 07 · 07-sol.md #20 | önemli | §3.5 ve süreç şeması kamera algılayıcı bağlantısını adsız/yeni arayüz sayıyor; iskelet 0.6'da bu A8'dir. | belge atfı; 07 §8 madde 3 de güncel bağlayıcı kurala çevrilmeli |
| AB-236 | MP6 · docs/olcumler/MP6-godot-kamera.md §4.1, §6, §5 (G2) | kritik | Olağan çizim döngüsü pencere örtülü, gizli ve simge durumlarında kare üretmiyor (yol a: 0 kare). Ana iş parçacığından `force_draw(false, dt)` + kamera `SubViewport`'u `UPDATE_ONCE` ile üç durumda da 9000/9000 kare, %0 düşme, kimlik boşluğu 0 ve görünür referansla bit düzeyinde aynı çıktı (Mac, 5 dk ön tur); 07'nin kamera yolu buna göre yazılmalı. | A8, A2; Godot sahnesi (07) |
| AB-237 | MP6 · docs/olcumler/MP6-godot-kamera.md §4.4, §7, §9 | önemli | Saydam olmayan `SubViewport`'ta `get_image()` RGB8 döndürüyor (640 × 480 için 921.600 B) ve RGBA8 → RGB8 dönüşümünü CPU'da yapıyor; `texture_get_data` RGBA8 verir (1.228.800 B). Biçim varsayımı buna göre belirtilmeli. | A8; rapor 07 §2.5'in RGBA8 varsayımını anmıyor (yalnız 07 §2.5'teki `get_image()` yedek yolunu ölçtü); varsayımın yeri doğrulanmadı |
| AB-238 | MP6 · docs/olcumler/MP6-godot-kamera.md §4.3, §6 | önemli | Asenkron geri okuma gizli pencerede ilerliyor ama gecikmesi gerçek çizim hızına bağlı: 30 Hz `force_draw` ile istekten geri çağrıya p50/p99/maks 58,56 / 70,52 / 72,72 ms (görünür pencerede 7,29 / 9,15 / 10,43 ms). 07 §2.4'teki "döngü kamera hızından hızlı koşar" varsayımı kendiliğinden sağlanmıyor. | A8; Godot sahnesi (07) |

### Uygulama: kart yazılımı (W3 Kart yazılımı; 01)

17 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-107 | 01 · 01-sol-2.md Y2; 01-sol.md #8, #9, #14 | önemli | A geçişi sürekli RX ya da birikmiş partilerle içeride kalabildiği için sağlık denetimi, B dilimi ve "sağlık aralığı ≤3 ms" / yaklaşık 128 ms güvenceleri mevcut düzenden çıkmıyor. | M5/M19 kapanış koşulu; ilk incelemede #8, #9, #14 önemli |
| AB-108 | 01 · 01-sol-2.md Y3; 01-sol.md #5 | önemli | Kritik yol yanıt süresi (1,695 ms; 24,2 ms ve 30,4 ms eşikleri) bütün giriş durumlarını, kodlama ve kesme giderlerini ve 200 Hz'i kapsamıyor. | M5 ölçümü; ilk incelemede #5 önemli |
| AB-109 | 01 · 01-sol-2.md Y4; 01-sol.md #10 | önemli | TinyUSB/DWC2 servis çağrısında işlenecek olay ve paket sayısı sınırlanmamış; "en çok 10 paket" çıkarımı yanlış. | USB sürücüsü uygulaması; M5; ilk incelemede #10 önemli KAPSAM DIŞI (K17): USB HS ve TinyUSB/DWC2 yığını plandan çıktı. |
| AB-110 | 01 · 01-sol-2.md Y5; 01-sol.md #11 | önemli | Olağan veri kabulü FIFO'daki eski baytları geçmeyi ve kritik çıktı son tarihini garantilemiyor; ACTUATOR gecikme ölçüm noktası (e₃) tutarsız. | gönderim yöneticisi; A1 ölçüm noktalarına uygunluk testi; ilk incelemede #11 kritik |
| AB-111 | 01 · 01-sol-2.md Y6; 01-sol.md #11 | önemli | Host okumazken iki ACTUATOR yuvası dolunca yeni güvenli çıktının konacağı yer yok; "atılmaz" ve sonlu kuyruk kuralları birlikte uygulanamıyor. | gönderim yöneticisi; K16; ilk incelemede #11 kritik |
| AB-112 | 01 · 01-sol-2.md Y11; 01-sol.md #7 | önemli | Erken başlatma hatasında güvenli bekleme, henüz kurulmamış zamanlayıcı ve bağlantıya dayanıyor; 2 s'lik başlangıç IWDG'siyle tekrar reset olabilir. | BSP başlatma işi; erken hata enjeksiyon testleri; ilk incelemede #7 önemli |
| AB-113 | 01 · 01-sol-2.md Y13; 01-sol.md #20 | önemli | Hata işleyicisinde acil yığına geçmeden özgün MSP/PSP ve EXC_RETURN saklanmıyor; exception çerçevesinin okunabilirliği doğrulanmamış. | K9; ilk incelemede #20 önemli |
| AB-114 | 01 · 01-sol-2.md Y14; 01-sol.md #16 | küçük | Kalıcı parametre başlığı 32 değil 30 B; iki ayrılmış baytın CRC kapsamı ve `gen` sarma açıklaması yanlış/eksik. | kalıcı parametre deposu; altın bayt testleri; ilk incelemede #16 önemli |
| AB-115 | 01 · 01-sol-2.md Y17 | önemli | TinyUSB 0.21.0 terminal bağlı değilken TX FIFO üzerine yazmaya varsayılan olarak izin veriyor; çerçeve bütünlüğü iddiası bu ayar belirtilmeden zayıf. | USB sürücüsü yapılandırması; DTR geçişleri K16'ya eklenecek KAPSAM DIŞI (K17): TinyUSB CDC yapılandırması ve DTR geçişleri USB HS'e özgü; USB HS plandan çıktı. |
| AB-116 | 01 · 01-kart-yazilimi.md §8.1 md.2 | belirtilmemiş | TinyUSB OSAL'ın statik oluşturulduğu (FreeRTOS yedeği) ve servis çağrısının en kötü süresi doğrulanmadı. | M5; 01 Y4 ile birlikte KAPSAM DIŞI (K17): TinyUSB OSAL ve USB servis çağrısı USB HS'e özgü; USB HS plandan çıktı. |
| AB-117 | 02 · 02-sol-2.md notlar (01 ile uyum) | belirtilmemiş | 01 eski `ida_core_tick_wall` imzasını ve yalnız IMU kesilince çağırma düzenini kullanıyor; yeni girdili `ida_core_tick`, alım damgaları ve çağrı sıklığına uyarlanmalı. | A4 imzasıyla birlikte |
| AB-118 | 02 · 02-ortak-cekirdek.md §8.3 (01) | belirtilmemiş | 01'den ≥100 Hz `tick`, `t_rx` damgası, `alive_ok` ile IWDG beslemesinin bağlanması ve FPSCR kaydı bekleniyor. | — |
| AB-119 | 06 · 06-sol-2.md notlar (01 §2.4, §2.5, §3.3, bütçeler) | belirtilmemiş | 01'de IMU tamamlanınca çağrı ve 10 ms boşluktan sonra tick anlatımı 06'nın TF kapanışıyla uyuşmuyor; `ida_conv_*` ile `ida_link` sahipliği birleşmemiş ve eski 2,5 ms kabulü yeni son tarihle uyuşmuyor. | 01 ve 06 ortak uçtan uca kabul kullanmalı |
| AB-239 | MP1 · docs/olcumler/MP1-arac-zinciri.md §4 md.2; §3.4 | kritik | STM32U59x/5Ax'te 160 MHz'e geçişte geçici AHB ÷2, 5 µs bekleme ve sonra ÷1 gerekiyor (RM0456 §10.5.4 açıklaması; RM bu ortamda doğrulanmadı); 01 §2.2'deki dizide yok. Prototipte açılış satırı `gecis_bekleme_cyc=420` (80 MHz'te 5,25 µs) gösterdi. | kart yazılımı uygulaması (01 §2.2 md.8) |
| AB-240 | MP1 · docs/olcumler/MP1-arac-zinciri.md §4 md.1 | küçük | 01 §2.2'deki "ST'nin DK örneği" HSE dizisi yalnız USBX Host örneklerine (`Applications/USBX/Ux_Host_CDC_ACM`) uyuyor; DK'nın diğer örnekleri PLL1'i MSIS 4 MHz'ten besliyor. | 01 belge düzeltmesi |
| AB-241 | MP1 · docs/olcumler/MP1-arac-zinciri.md §4 md.8; §3.1 | önemli | Kart revizyonu `REV_ID` 0x3000, ES0553 Rev 6 cihaz kimliği listesinde yok (liste 0x3001 ve 0x3003); errata kapsamının bu karta uygulanabilirliği belirsiz. | kart yazılımı uygulaması (01); M6 |
| AB-242 | MP1 · docs/olcumler/MP1-arac-zinciri.md §4 md.4 | önemli | HAL, `FRACN` = 0 iken de `PLL1FRACEN`'i 1 yapıyor; `SYSCLK` geri okuması `RCC_PLL1FRACR`'ı hesaba katmalı (ilk sürümde yanlışlıkla `E SAAT_HATASI` bildirildi). | kart yazılımı uygulaması (01) |

### Uygulama: ortak çekirdek — kestirim, güdüm, kontrol (W3; 02)

14 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-120 | 02 · 02-sol-2.md Y1 | kritik | Skaler ölçüm güncellemesinde yenilik iki kez düzeltiliyor; kovaryans küçülürken durum yanlış düzeltilebiliyor. | karşı örnek teste eklenecek |
| AB-121 | 02 · 02-sol-2.md Y2; 02-sol.md #3 | önemli | Geçmiş gövde çerçevesindeki yerel tutum hatası, araç dönmüşken güncel yönelime ve geçmiş kayıtlara doğrudan ekleniyor; çıkış öngörücüsünde tutum hatası taşıması ve takip hatası sınırları tanımsız. | ilk incelemede #3 kritik |
| AB-122 | 02 · 02-sol-2.md Y3; 02-sol.md #6 | kritik | Kovaryans geçerlilik denetimi yalnız 100 Hz'e bağlı; füzyon ve reset sonrası bozulan ama sonlu P aynı çağrıda kullanılabiliyor. | ilk incelemede #6 kritik |
| AB-123 | 02 · 02-sol-2.md Y4; 02-sol.md #2, #22 | önemli | İşlem bütçesi çağrı başına tek kovaryans yayılımı ve tek reset varsayıyor; birden çok ölçüm yığılması ve öngörücü geçmişi düzeltme maliyeti hesapta yok. | M5 bütün izin verilen yığılmaları kapsamalı; ilk incelemede #22 önemli |
| AB-124 | 02 · 02-sol-2.md Y5; 02-sol.md #3, #15, #22 | önemli | "Gecikme 200 ms'yi aşınca en eski ölçümü at" kuralı IMU tamponunun taşmayacağını kanıtlamıyor; genel Joseph yedeğinde vektör ölçümün çağrılar arasında tamamlanması tanımsız. | ilk incelemede #3 kritik, #15 ve #22 önemli |
| AB-125 | 02 · 02-sol-2.md Y8; 02-sol.md #1 | önemli | Konum ve hız O_b'ye ait olduğu halde ivmeölçerin dönme kolu etkileri nominal modelden çıkarılmıyor; 05 bu etkileri ölçüme ekliyor. | ilk incelemede #1 önemli |
| AB-126 | 02 · 02-sol-2.md Y10; 02-sol.md #15 | önemli | Arka plan girdisi değişmez ilan edilirken yeni dilim gelince etkin işin okuduğu sonar yuvası yeniden kullanılabiliyor. | ilk incelemede #15 önemli |
| AB-127 | 02 · 02-sol-2.md Y11; 02-sol.md #4; 02 · 02-ortak-cekirdek.md §8.1 md.4 | önemli | DVL modeli sensör çerçevesinde çıktı üretirken kovaryans gövdeye döndürülüyor; model, yenilik ve kovaryans aynı çerçevede olmalı. | 05 `eph` tanımıyla tutarlı olmalı; ilk incelemede #4 önemli |
| AB-128 | 02 · 02-sol-2.md Y12; 02-sol.md #20 | önemli | Tahsis kalıntısı ölü bant ve PWM nicemlemesinden önceki itki isteğinden hesaplanıyor; küçük komutun sıfırlanması halinde kalıntı gerçekte sıfır değil. | — |
| AB-129 | 02 · 02-sol-2.md Y13; 02-sol.md #10 | önemli | Derinlik ya da tutum geçersizken HOLD/SURFACE ve yön tutma için ön koşul ve yedek eylem yok; EST_DEGRADED derinlik kullanılabilirliğini kapsamıyor. | ilk incelemede #10 önemli |
| AB-130 | 02 · 02-sol-2.md notlar (iskelet 0.4) | belirtilmemiş | Bölüm başındaki iskelet 0.3 sürüm atfı güncellenmeli. | yalnız belge atfı |
| AB-131 | 02 · 02-sol-2.md notlar (04 ile uyum) | belirtilmemiş | Bir yüklü durum hangi tüpün kaldığına göre aynalıdır; çekirdeğin yük durumu yalnız sayacı değil bu ayrımı da korumalı. | — |
| AB-132 | 02 · 02-sol-2.md notlar (05 ile uyum) | belirtilmemiş | DVL su takibi, dip takibiyle aynı mutlak hız modeliyle füzyonlanmamalı. | — |
| AB-133 | 04 · 04-sol-2.md notlar (02'nin itki tahsisi); 04 · 04-simulator-cekirdegi.md §8 md.1; 02 · 02-ortak-cekirdek.md §8.3 (04) | belirtilmemiş | 02 tahsis matrisini CG referansıyla kuruyor, 04 sabit gövde referans noktasını kullanıyor; kontrol wrench'i ile tahsis noktası ve karekök ters eğri ile 04'ün polinom eğrisi eşitlenmemiş. | 04 ile birlikte |

### Uygulama: atış kontrol, emniyet zinciri ve arayüzler (W4; 03, F1–F10)

23 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-134 | 02 · 02-ortak-cekirdek.md §8.3 (03) | belirtilmemiş | Açılış içinde benzersiz `shot_id`, arka plan işinin `ida_core_t`'ye dokunmaması ve geçmiş sorgu dönüş kodlarına uyum 03'ten bekleniyor. | — |
| AB-135 | 03 · 03-sol.md #2 | kritik | Kilitli arıza durumundan çıkış koşulu tutarsız. | — |
| AB-136 | 03 · 03-sol.md #7 | önemli | Atış geçerlilik süresi, ACK zaman aşımı ve ret yanıtlarının durum geçişleri birbiriyle tutarsız. | — |
| AB-137 | 03 · 03-sol.md #8 | kritik | Çözümün dayandığı sürümler ve atış adımındaki geçerlilik denetimleri eksik. | — |
| AB-138 | 03 · 03-sol.md #9 | önemli | Durum geçiş tablosu eksik; genel ölçüt kaybı, eşzamanlı olay öncelikleri ve TEST durumu tanımsız. | — |
| AB-139 | 03 · 03-sol.md #10 | kritik | Fırlatıcı geri okumasında alım zamanı, geçerlilik ve bayatlık davranışı tanımsız. | A1/A4 ile; 05 ve 04 geri okuma yolu |
| AB-140 | 03 · 03-sol.md #12 | önemli | Ölçüm belirsizliği dönüşümü ve ortak hata bileşenlerinin tutulduğu yer tanımsız. | — |
| AB-141 | 03 · 03-sol.md #13 | önemli | Seyrüsefer reset bildirimi iz ve kovaryans dönüşümü için yeterli bilgiyi taşımıyor. | — |
| AB-142 | 03 · 03-sol.md #14 | önemli | Kullanılmayan kovaryans satırı sıfır olan ölçüm türleri Cholesky denetiminde reddediliyor. | — |
| AB-143 | 03 · 03-sol.md #16 | önemli | Yükseliş ölçülmeyen sonar kümesinin iki bileşene indirgenmesi tersinir değil; eksik yükselişin temsili tanımsız. | — |
| AB-144 | 03 · 03-sol.md #17 | önemli | Hedefin tarama süresindeki hareketi ve kümenin zaman belirsizliği modellenmemiş. | — |
| AB-145 | 03 · 03-sol.md #19 | önemli | 60 ms sıralama tamponunun anlamı ve geç kayıt politikası belirsiz. | — |
| AB-146 | 03 · 03-sol.md #20 | önemli | CV6 başlatma, negatif gözlem ve RECONFIRM silme kuralları eksik. | — |
| AB-147 | 03 · 03-sol.md #21 | önemli | Veri ilişkilendirmede yeni iz maliyeti ve farklı ölçüm türleri için olasılık sözleşmesi tanımsız. | — |
| AB-148 | 03 · 03-sol.md #24 | kritik | Emniyet iddiası, üç sanal engelin de açık olduğu işletim durumunda ortak kart arızasına karşı yeterli değil. | — |
| AB-149 | 03 · 03-sol.md #26 | önemli | FreeRTOS yedeğinde arka plan iş yuvasının sahiplik devri ve iptal protokolü tanımsız. | — |
| AB-150 | 03 · 03-sol.md #29 | önemli | ARMED altında parametre değişikliğine izin verilen durumlar belirsiz; etkilenen iş, iz ve yetkilerin geçersizleştirilmesi tanımsız. | 02 değiştirilebilirlik sınıflarıyla eşitlenecek |
| AB-151 | 03 · 03-sol.md #30 | önemli | Hızlı yolun adım başına en kötü iş yığılması sınırlandırılmamış. | M5 en kötü dolulukta ölçüm |
| AB-152 | 03 · 03-sol.md #31 | önemli | Test tasarımında kaba kuvvet atama karşılaştırması pahalı; F3, F8 ve GCS kaybı kuralları tutarsız. | — |
| AB-153 | 03 · 03-sol.md #33; notlar (0.5 değişikliklerinin durumu) | küçük | Belge atıfları ve sürüm kaydı eski; karşılanmış iskelet önerileri ve ikinci menzil ölçer seçeneği açık gösteriliyor. | belge temizliği |
| AB-154 | 03 · 03-sol.md notlar (iskelet 0.5 uyum) | küçük | Kuaterniyon ve sabit gövde referansı ortak kabullerine açık atıf eklenmeli. | — |
| AB-155 | 03 · 03-atis-kontrol.md §8.4 | belirtilmemiş | Hedef sınıfları ve isabet bölgeleri senaryo verisi olarak A5'te tanımlanmadı. | A5 biçimi 08'in işi |
| AB-156 | 05 · 05-sol-2.md notlar (03, 07) | belirtilmemiş | 03'te iskelet 0.7 öncesinden kalan torpido yetenekleri ve itki alanları kapsamdan çıkarılmalı; 05'te kaldırılan kabarcık izi geri getirilmemeli. | W4 dar kapsamlı emniyet/arayüz işi; 07 tarafı A2 satırında |

### Uygulama: fizik çekirdeği (W3; 04; SIL koşucusu W4 ile ilgili satırlar Not sütununda)

16 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-157 | 02 · 02-sol-2.md notlar (04 ile uyum) | belirtilmemiş | Çekirdekte olay kabulü ile 04'te fiziksel olay uygulaması farklı anlardır; kütle geçişinin bu gecikmeyle ilişkisi açıklaştırılmalı. | A1 olay kuralları ile birlikte |
| AB-158 | 03 · 03-sol.md #25; 06 · 06-sol-2.md notlar (04 adım sırası) | kritik | Sim sunucusunda yetkili emniyet durumu ile karta giden geri okuma ayrılmamış; olay uygulama anında yetkili durum denetimi ve aynı adımdaki senaryo değişikliği sırası belirsiz. | A1 ve 05/06 ile |
| AB-159 | 04 · 04-sol-2.md Y1; 04-sol.md #6, #8 | önemli | Torpido CG'si ile CB'si ve birleşik CG/CB/atalet hesabı `r_t` için farklı referanslar kullanıyor; düzgün yoğunluk kabulü ile kaymış CG de tutarsız. | ilk incelemede #6 kritik, #8 önemli |
| AB-160 | 04 · 04-sol-2.md Y2; 04-sol.md #7 | önemli | Torpido tarafında yalnız skaler hız artışı uygulanıyor; CG ofsetiyle öteleme–dönme bağlaşımı olan tam 6-SD darbe çözümüyle ve açısal momentum testiyle uyuşmuyor. | ilk incelemede #7 kritik |
| AB-161 | 04 · 04-sol-2.md Y3; 04-sol.md #9 | önemli | Elektrik çözücüsü uç noktalardaki aynı işareti "kök yok" sayıyor; sabit güç yükünde iki kök aralıkta olabilir ve yanlış güç çökmesi üretilir. | ilk incelemede #9 önemli |
| AB-162 | 04 · 04-sol-2.md Y4; 04-sol.md #12 | önemli | İdeal kaynak kipinde kablo direnci sıfır olmadığı için 16 kat yakınsama testi koşulu yeterli değil. | ilk incelemede #12 önemli |
| AB-163 | 04 · 04-sol-2.md Y5 | küçük | "Küçük sinyal kazancı 0,29'a iner" açıklaması yanlış kapsamda kullanılmış; oran yalnız sonlu genlikli basamağın ilk geçişine ait. | — |
| AB-164 | 04 · 04-sol-2.md Y6; 04-sol.md #18 | küçük | 30 Hz için "27/26 adım dönüşümlü" ifadesi ortalama 26,5 adım verir, 30 Hz üretmez. | ilk incelemede #18 önemli |
| AB-165 | 04 · 04-sol-2.md Y7; 04-sol.md #21 | önemli | Snapshot interpolasyonunun atış, kütle değişimi ve gövde doğumu gibi süreksizlik sınırlarını aşıp aşmayacağı tanımsız. | A2 ile birlikte (poz akışı); ilk incelemede #21 önemli |
| AB-166 | 04 · 04-sol-2.md Y8; 04-sol.md #22 | küçük | §2.1 adım 3 elektrik bağlaşımı için hâlâ yanlış başlığa (§2.7.6) atıf yapıyor. | doğrusu §2.7.7; belge düzeltmesi |
| AB-167 | 04 · 04-sol-2.md notlar (kaynaktan çıkarılanlar); 04 · 04-simulator-cekirdegi.md §8 (von Benzon Tablo A1) | belirtilmemiş | İtici tablosu kaynak makale tablosuyla (üçüncü ve dördüncü itici yönleri, dikey yönler) aynı değil; mimari varsayım olarak etiketlenmeli. | — |
| AB-168 | 04 · 04-simulator-cekirdegi.md §8 (açık noktalar) | belirtilmemiş | Ortalama yüzey üstü dalga kinematiği (Wheeler germe) doğrulanmadı. | M15 |
| AB-169 | 04 · 04-simulator-cekirdegi.md §8 (açık noktalar) | belirtilmemiş | İkinci mertebe sürüklenme, frekansa bağlı eklenmiş kütle, sıvı belleği, frenlemede rejeneratif akım, ESC/itici/batarya ısıl modelleri ve derinliğe bağlı akıntı profili yok. | bölümde yazılı model sınırları; kabul edilen sınır kararı mimarda |
| AB-170 | 06 · 06-sol-2.md notlar (04 §2.7.1, §3) | belirtilmemiş | 04'te 50 ms komut ömrünün başlangıç anı açık değil; `ActuatorFrame` ve `OneShotEvent` 06'nın nesil, süre, oturum ve onay alanlarını karşılamıyor. | 04 ve 06 ortak kabul/uygulama sözleşmesi |
| AB-171 | 06 · 06-sol-2.md notlar (04 adım sırası) | belirtilmemiş | 04'te aktüatör ve atış olayları senaryo olaylarından önce işleniyor; aynı sınırdaki acil durdurma ya da pim değişiminin atıştan önce yetkili duruma yansıması garanti değil. | 03 #25 ile aynı konu; 04 aynı adımın emniyet daraltmalarını atıştan önce uygulamalı |
| AB-172 | 08 · 08-laboratuvar-uygulamasi.md §8 (11.2, 04'ten beklenen i) | belirtilmemiş | `World::step()` bir fizik adımıdır, SIL "adımla" birimi kontrol periyodudur; koşucunun iki fizik adımını bir arada çağırması ya da 04'ün kontrol sınırı görevinin bölünebilir olması gerekir. | SIL koşucusu işiyle birlikte |

### Uygulama: sensör modelleri (W3) ve sonar (W4) (05)

19 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-173 | 02 · 02-sol-2.md notlar (05 ile uyum); 04 · 04-sol-2.md notlar (05'in referansı) | belirtilmemiş | 05 §3 torpido ayrılınca kayan kütle merkezi referansı bekliyor; iskelet 0.4'teki sabit gövde referans noktasına uyarlanmalı. | 05 değişmeli |
| AB-174 | 04 · 04-sol-2.md notlar (05'in referansı ve akımları) | belirtilmemiş | 05'in manyetometre için istediği işaretli motor akımı, 04'ün negatif olmayan besleme akımıyla aynı alan olarak kullanılmamalı. | 04 ile birlikte |
| AB-175 | 04 · 04-sol-2.md notlar (05'in diğer gereksinimleri); 05 · 05-sol-2.md notlar (04 §3) | belirtilmemiş | `water_pressure`, `water_salinity`, tek seferlik zamanlayıcı olayı, batarya sıcaklığı ve yerçekimi erişimi 04–05 sözleşmesinde tamamlanmamış; snapshot halkasının gerçek geçmiş uzunluğu, DVL ihtiyacı ve paylaşımlı ortam verisinin yaşam süresi birleştirilmemiş. | W3 fizik/sensör bütünleştirmesi; 04 ile birlikte |
| AB-176 | 05 · 05-sol-2.md Y1; 05-sol.md #17, #18 | kritik | Tam sonar kademesinde aynı hücreye düşen ışınların saçıcı katkıları ışın sayısıyla büyüyor (alanı m parçaya bölmek enerjiyi m kat artırır); "aynı alana yakınsar" iddiası yanlış. | W4 sonar uygulaması, M11'den önce; Y1 kapanmadan M11 sonucu kabul kanıtı sayılamaz |
| AB-177 | 05 · 05-sol-2.md Y2; 05-sol.md #19; 07 · 07-gorsellestirme.md §8.1 md.3 | önemli | Karşılıklı yollarda genlikte ×2 çarpanı koşulsuz yazılmış; yalnız geliş açısına bağlı geri saçılım katsayısı sekmeli yollar için genel karşılıklı saçılım yasası değil. | malzeme parametreleri A7'de |
| AB-178 | 05 · 05-sol-2.md Y5; 05-sol.md #9, #13, #15 | önemli | DVL raporu son ışın dönmeden üretilebiliyor, azami menzil ve halka gereksinimi tanımsız, gürültü σ'sı gürültülü hıza öz bağımlı ve ses hızı oranı örneği ters. | W3 kamera dışı sensör modelleri; 04 ile iç zaman sözleşmesi |
| AB-179 | 05 · 05-sol-2.md Y6; 05-sol.md #11, #30 | önemli | GNSS'te uydu sayısı ölçeği varsayılan CEP kabulünü bozuyor (20 uyduda 2 m yerine 1,549 m) ve σ büyümesinin hata bileşenlerine uygulanışı yazılmamış. | W3 kamera dışı sensör modelleri |
| AB-180 | 05 · 05-sol-2.md Y7; 05-sol.md #9, #12 | önemli | GNSS'te uydu başına edinim ile ayrı çekilen ilk-fix süresi iki edinim mekanizması oluşturuyor; kayıp olasılığı çıktı hızına bağlı. | W3 kamera dışı sensör modelleri |
| AB-181 | 05 · 05-sol-2.md Y9; 05-sol.md #20 | önemli | Darbe/alıcı çekirdeğinin normalizasyonu, destek aralığı, grup gecikmesi ve zaman orijini tanımsız; tepe testi bunu hesaba katmıyor. | W4 sonar uygulaması |
| AB-182 | 05 · 05-sol-2.md Y10; 05-sol.md #19 | önemli | Çok yollu modelde ayna adaylarının seçimi, budama ve azami yol sayısı tanımsız; iş yükü yalnız ışın sayısıyla sınırlı değil. | W4 sonar uygulaması; M11 |
| AB-183 | 05 · 05-sol-2.md Y11; 05-sol.md #25 | önemli | Embree üst sahne paylaşımında yedek kopyaların sahipliği, dünya iş parçacığı sorguları, bellek ve kurulum maliyeti sınırlandırılmamış. | W4 sonar uygulaması; M11; ilk incelemede #25 kritik |
| AB-184 | 05 · 05-sol-2.md Y12; 05-sol.md #26 | önemli | Durumlu arızalarda (takılma, güç çevrimi) temel dizi bit eşitliğinin nasıl korunacağı belirsiz. | W3 sensör/arıza altyapısı |
| AB-185 | 05 · 05-sol-2.md Y16; 05-sol.md #30 | önemli | 32 gerçekleşimle %5 yakınsamanın ve tohum başına KS p>0,05 koşulunun istatistiksel karar yöntemi tanımsız. | W4 sonar kabul testleri; 09 §7.3 sınır dosyası; kabulden önce dondurulmalı |
| AB-186 | 06 · 06-sol-2.md notlar (05 zamanlama) | küçük | 05'in 04'e atfettiği eski 33/34 adım ifadesi güncelliğini yitirmiş (04 artık 26/27 kullanıyor). | 05'teki uyumsuzluk notu düzeltilecek |
| AB-187 | 07 · 07-gorsellestirme.md §8.1 md.4 | belirtilmemiş | 05'in zamanlama tablosu kamera için 30 Hz'i sabit yazıyor; sade kademede 15 Hz için 53/54 adım dizisi tablo notu olarak gerekiyor. | — |
| AB-188 | 08 · 08-laboratuvar-uygulamasi.md §8 (11.2, 05'ten beklenen) | belirtilmemiş | "Su girişi" yer gerçeği olayı 08'in sistem olayları sınıfından geliyor ama 04 su girişini modellemiyor; 05 ile ortak sahiplik gerekiyor. | 04 ve 05 ile |
| AB-244 | MP7 · docs/olcumler/MP7-sonar-suresi.md §7 | kritik | Bölüm 05 §2.4.3'ün saçıcı hücresi kuralı hatalı: aynı hücredeki `m` ışının genlikleri eşevreli toplandığından enerji `σ A m` olup ışın sayısıyla sınırsız büyüyor (7 m'de her ikiye katlamada +%8'den +%64'e). Yerine geçecek kural: hücre içinde enerji ağırlıkları `w_j = σ_j dA_j G_j` toplanır; hücreler gövdeye sabittir; faz anahtarı (gövde, hücre, yol türü, gerçekleşim) dörtlüsüdür (`docs/gorevler/W0-MP7a.md`). Bu kuralla 7 m'de `δ_E` 0,204 → 0,015. | sonar uygulaması (05 §2.4.3) |
| AB-245 | MP7 · docs/olcumler/MP7-sonar-suresi.md §7, §4.3 | önemli | 05 §6'nın yakınsama büyüklükleri tanımsızdı; tanım `docs/gorevler/W0-MP7a.md`'de: `δ_E`, ham varyansla `δ_V`, `δ_C`; bağımsız referanstan kutu kümesi `S`; ölçüt nokta değeri ve bootstrap %95 GA üst ucu ≤ 0,05 ve sonraki ikiye katlamada da sağlanma. Sade kademede 05'in kutu başına beneği korunmalı (beneksiz tanımda `δ_V` yapısal olarak sağlanamaz). | sonar uygulaması (05 §6) |
| AB-246 | MP7 · docs/olcumler/MP7-sonar-suresi.md Özet (S2, S3), §4.3, §4.4, §9 | kritik | Tam kademe dondurulmuş yakınsama ölçütünü taranan aralıkta (`N` ≤ 2 559 903) sağlamıyor (7 m'de `δ_V` GA üst ucu 0,093, 50 m'de 0,175; eşik 0,05); `N` = 1 280 000 destek koşusunda tek iş parçacığı p99 504,4 ms (7 m eşikleri 9,33 ve 25,17 ms). Sade kademe (7 m) `N` = 5 000 ile ölçütü sağlıyor; tek iş parçacığında p99 7 m'de 0,93 ms, 50 m'de 0,79 ms. M3 Pro sonucu Windows'u temsil etmez. | iskelet §8 sonar kademesi kararı (M11) |

### Uygulama: HIL köprüsü, bariyer, bağlantı emülatörü (W4; 06)

1 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-189 | 06 · 06-sol-2.md Y19; 06-sol.md #25 | önemli | Bozuk çerçeveden sonra "280 B içinde toparlanma" sınırının başlangıç noktası ve başarı ölçütü tanımsız; ayrıştırıcının yeniden baş araması ile geçerli çerçeve teslimi ayrılmamış. | ayrıştırıcı sürümü sabitlendikten sonra |

### Uygulama: görselleştirme — Godot sahnesi ve GDExtension (W5; 07)

17 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-190 | 07 · 07-sol.md #1 | kritik | Dalga fazı 04'te `cos(ωt−kx+φ)`, 07'de `cos(kx−ωt+φ)`; aynı φ için yüzey yüksekliği farklı çıkıyor. | 04 ile aynı dönüşüm kullanılacak; sıfırdan farklı fazlı test |
| AB-191 | 07 · 07-sol.md #2 | kritik | Sahne düğümlerine yazılan pozun elle çizimden önce RenderingServer'a ulaştığı garanti değil; dönüşüm bildirimlerinin işleneceği açık aşama tanımsız. | — |
| AB-192 | 07 · 07-sol.md #3 | kritik | İki zamanlı çizimde geri okumanın doğru kareyi okuması için dönüşüm, çizim ve doku okuma komut sırası ve istek kimliği tanımlı değil. | — |
| AB-193 | 07 · 07-sol.md #4 | önemli | TAA/SDFGI kapatmak iki zamanlı sahnenin zamana bağlı malzeme, animasyon, pozlama ve ışık girdilerini çözmüyor; `CameraRequest` yalnız gövde listesi ve kamera yapılandırma sürümünü sabitliyor. | — |
| AB-194 | 07 · 07-sol.md #5 | kritik | `max_fps=120` üst sınırdır; gizli pencerede geri okuma kuyruğunun ilerlemesi ve uçtan uca kamera teslim bütçesi güvence altında değil. | MP6 (M13, M28, M30); 05 ve 06 ile |
| AB-195 | 07 · 07-sol.md #9 | önemli | Geç, çizilmekte olan, geri okuması beklenen istekler ve hazır kare için iptal kuralları ayrılmıyor; iptalden sonra gelen callback kareyi yeniden yayımlayabilir. | — |
| AB-196 | 07 · 07-sol.md #10 | önemli | Yedek yöntemde operatör zamanının geriye gitmeyeceği kanıtlanmıyor. | — |
| AB-197 | 07 · 07-sol.md #14 | önemli | RD dokusunun doğrudan `RGBA8_SRGB` olduğu varsayılmış; renk uzayı, ton eşleme ve kanal biçimi dönüşüm sırası tanımsız. | — |
| AB-198 | 07 · 07-sol.md #15 | önemli | Genel kamera iç parametrelerinin yalnız FOV ve ana nokta kaydırmasıyla temsil edildiği kanıtlanmamış ve derinlik tamponundan fiziksel menzile dönüşüm yok. | — |
| AB-199 | 07 · 07-sol.md #16 | önemli | `Δx≤λ_min/16` kuralı toplam yüzey hata bütçesini sağlamıyor; ağ sıklığı ve 25 m yarıçap sınırı spektrumla ilişkilendirilmemiş. | M32 |
| AB-200 | 07 · 07-sol.md #17 | önemli | 64 bileşenli dalga toplamı için bileşen üst sınırı, uniform düzeni ve ek çizim geçişleri bütçelenmemiş; FFT yedeğinin yeniden örnekleme hatası hesaba katılmamış. | — |
| AB-201 | 07 · 07-sol.md #19 | önemli | Düşük Windows hedefi için operatör çözünürlüğü, geri okuma ve ortak kaynak yükü sınırlandırılmamış; yerleşik sis yedeği sensör modeliyle aynı değil. | MP8 (M12, M20, M21, M30) birleşik HIL yükü altında |
| AB-202 | 07 · 07-sol.md #22 | önemli | Geçersizlik kuralları uyuşmuyor: genel %1 kayıp eşiği ile çökme/algılayıcı kaybında doğrudan geçersizlik ayrılmamış ve `ViewerStatus` kesilme zaman aşımı yok. | — |
| AB-203 | 07 · 07-sol.md #23 | önemli | Testler iki okuyuculu halka yarışı, okuyucu çökmesi, iptal sonrası callback, sim yeniden başlatma ve reset sırasında bekleyen işleri kapsamıyor; P4 kayıp ölçütü algılama ve karta teslim zincirini kapsamıyor. | — |
| AB-204 | 07 · 07-sol.md notlar (04 ile komşu) | küçük | 07 §3.7 dalga fazı için hâlâ "beklenen iş" diyor; 04 bunu zaten tanımlıyor. | belge güncelliği |
| AB-205 | 07 · 07-sol.md notlar (M29) | belirtilmemiş | Shader derlemesi ile uzun koşu hatası §4'te tek hata satırında ele alınıyor; ayrıştırılmalı. | M29 |
| AB-243 | MP6 · docs/olcumler/MP6-godot-kamera.md §7, §4.1 | küçük | Godot 4.7.2 ana pencerenin görünürlüğünü içeriden değiştirmeye izin vermiyor (`scene/main/window.cpp:1017`); "gizli" durumu uygulamanın gizlenmesi (`NSRunningApplication.hide`, Cmd+H eşdeğeri) olarak tanımlanmalı. | 07 belge düzeltmesi; P5 atfı raporda doğrulanmadı |

### Uygulama: laboratuvar uygulaması (W5 web panelleri, A3 sunucusu, kayıt, hata enjeksiyonu; 08)

1 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-206 | 08 · 08-laboratuvar-uygulamasi.md §8 (11.2, 06'dan beklenen ii; 11.3); 06 · 06-protokol-ve-hil.md §8; 09 · 09-test-ve-is-akisi.md §8.2 md.5 | belirtilmemiş | QGC'nin hedef mi prova mı olduğu (00 A6) yanıtsız; UDP uç noktasının kimlik denetimsiz doğası ve kapsamı buna bağlı. | yer belirsiz; kullanıcı yanıtı |

### Uygulama: test ve derleme altyapısı (W2 depo iskeleti, CI; 09)

5 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-207 | 01 · 01-sol-2.md notlar (09) | belirtilmemiş | 09 derleme planı güncel 01'e uymuyor: tek modül `protocol/ida_link`, `protocol/conv/` yolu, LVGL istisnası ve K0–K20 kapsamı. | Y16 ölçüt kısmı ayrı satırda |
| AB-208 | 02 · 02-sol-2.md notlar (04 ile uyum) | belirtilmemiş | Yüzey kontrolü için hidrostatik denge, yatay itkinin doğurduğu roll/pitch ve dikeylerin açılıp kapanması birlikte sınanmalı. | kabul testi |
| AB-209 | 09 · 09-test-ve-is-akisi.md §8.2 md.4 | belirtilmemiş | CI olanakları (özel depoda dal koruma, dakika kotası, macOS ve Windows iş süresi maliyeti, ST ve Arm araç zincirinin CI'da kurulabilirliği) doğrulanmadı. | W2'nin ilk işi kayda geçirecek |
| AB-210 | 09 · 09-test-ve-is-akisi.md §8.1 md.5 | belirtilmemiş | 01–08'deki "09'da kesinleşir" eşikleri "sözleşme işinde dondurulur" olarak yansıtılmalı ve her sözleşme işine sınır dosyası sahibi (test yazarı) atanmalı. | — |
| AB-247 | MP1 · docs/olcumler/MP1-arac-zinciri.md §4 md.3 | önemli | Picolibc, CubeIDE'nin GNU Tools for STM32 14.3.rel1 macOS arm64 paketinde yok (`picolibc.specs` ve kitaplığı bulunmadı; 01 §2.10, 00 08.7, 09 §2.6 "libc" satırı etkileniyor); prototip newlib-nano ile bağlandı. | W2 araç zinciri (09 §2.6) |

### Ölçüm işleri (W0: MP1–MP8; MP9 W3 sonu ve W6; ölçümle kapanacak olanlar)

14 bulgu.

| Kimlik | Kaynak | Önem | Bulgu | Not |
|---|---|---|---|---|
| AB-211 | 01 · 01-sol-2.md Y16; 01-sol.md #23 | önemli | 09'daki MP2/MP4 ölçütleri güncel tam yol ve en büyük kart payı (≤1,70 ms) ile eşit değil; VCP yedeği 1 ms köprü varsayımıyla tam kritik yükte sığmıyor. | MP2/MP4/MP8 (M1, M2, M5, M17); ilk incelemede #23 önemli |
| AB-212 | 01 · 01-sol-2.md notlar (03 §2.5) | belirtilmemiş | 01'in arka plan bütçesi daraltılmış 03 iş yüküyle yeniden ölçülmeli. | M5; atış kontrol işinden sonra |
| AB-213 | 01 · 01-kart-yazilimi.md §8.1 md.1 | belirtilmemiş | Doğrulanmamış donanım ayrıntıları (32 bit zamanlayıcı seçimi, PLLRGE ve booster dizisi, DOEPINTx.XFRC yan etkisizliği, USART1 karakter eşleşmesi, RWW, yarım yazım ECC/NMI, flash süreleri ve ömrü, lockup, LSI sapması, SRAM silme seçenek baytı, BOR bayrağı) kart üzerinde kapanmadı. | yer belirsiz: MP1/MP4 kapsamı açık değil |
| AB-214 | 02 · 02-ortak-cekirdek.md §8.2 md.5 | belirtilmemiş | Sabit teslim gecikmesi kart tarafından ölçülemiyor; 06'nın gidiş-dönüş ölçümüyle tamamlanmalı. | MP2 |
| AB-215 | 06 · 06-protokol-ve-hil.md §8 (açık noktalar) | belirtilmemiş | Yeni ölçüm önerileri tanımlı değil: SIL TCP loopback gidiş-dönüşü ve koşu hızına etkisi, kartta 400 Hz ayrıştırma ve USB alım kesmelerinin işlemci payı, işletim sistemi uyku/USB güç yönetiminin etkisi, ST-LINK köprüsünün kendi gecikmesi. | M5, M1, M3'e ek |
| AB-216 | 07 · 07-sol.md notlar (M12) | belirtilmemiş | M12'nin 720p deney noktası tabloda kalmış, kabul testine açıkça bağlanmamış. | MP6/MP8 |
| AB-217 | 07 · 07-gorsellestirme.md §8.2 md.4 | belirtilmemiş | A2 poz akışı gecikmesi ve kaybı için ölçüm tanımı yok; M12 ve M30 yalnız kamera hattını kapsıyor. | 00 §7; P2 |
| AB-218 | 07 · 07-gorsellestirme.md §8 (açık noktalar) | belirtilmemiş | `force_draw(false, dt)` ile olağan çizimin aynı karede art arda kullanımının iş parçacıklı render kipinde güvenliği ve gizli pencerede asenkron geri okuma kuyruğunun ilerleyip ilerlemediği doğrulanmadı. | MP6 (P5, P6; M13, M28) |
| AB-219 | 07 · 07-gorsellestirme.md §8 (açık noktalar) | belirtilmemiş | A23, A24, A26, A27 (pencere görünürlüğü, kamera çözünürlüğü/hızı/sayısı, macOS sürümü, iki pencere ya da LibGodot ile gömme) yanıtı bekleniyor; LibGodot ile mevcut yüzeye gömme ayrı prototip istiyor. | MP6/MP8; kullanıcı yanıtı; 03 §8.4 ve 06 §8 ile |
| AB-220 | 08 · 08-laboratuvar-uygulamasi.md §8 (11.1 md.4) | belirtilmemiş | Kayıt yazıcı verimi ve kamera kare sıkıştırma maliyeti için ölçüm tanımı yok. | 00 §7; M20 yanına eklenmesi öneriliyor |
| AB-221 | 08 · 08-laboratuvar-uygulamasi.md §8 (11.3) | belirtilmemiş | Windows hedef bilgisayarının özellikleri bilinmediği için panel yoğunluğu ve kademe sınırları ölçülmedi; uygulama kabuğu gerekliliği M20 sonrasına bırakıldı. | MP8 (M20, Ö11) |
| AB-222 | 09 · 09-test-ve-is-akisi.md §8.2 md.6 | belirtilmemiş | Windows hedef bilgisayarının özellikleri bilinmiyor; MP8 öncesi kalite kademesi tabanı varsayım. | MP8; Ö11 |
| AB-223 | 09 · 09-test-ve-is-akisi.md §8.2 md.7 | belirtilmemiş | Kod gerektiren ölçümler (M10, M15, MP4'ün gerçek çekirdekli turu) W3 sonrasına kalıyor; W0 yalnız yer tutucu yükle ön bilgi verir. | MP4, MP9 |
| AB-224 | 09 · 09-test-ve-is-akisi.md §8.1 md.8 | belirtilmemiş | 06 §7 kablo çıkarma için yazılımla kesilebilen USB hub'ı anıyor, aracı seçilmemiş; MP2 ve T7'nin HIL kısmı kullanıcıdan donanım gerektirebilir. | MP2 |

## 4. Bölümler arası uyumsuzluklar

Aynı konuda iki bölümün farklı şey söylediği durumlar. Kimlikler `U-nn`'dir; ayrıntı için Bulgu satırlarına ve parantezdeki kaynağa bakılır. Bu tablo yalnız eşitlenecek yeri gösterir.

| Kimlik | Konu | Bölümler | Ne farklı | Eşitlenecek yer |
|---|---|---|---|---|
| U-01 | Atış ve olay taşıması | 06, 01, 04 ↔ iskelet §3.2 | 06 `launcher` alanını seviye/yükselen kenar taşıyor ve `event_id` yok; 01 eski gösterimde yeniden gönderimde tetiği sıfırlıyor; 04 `OneShotEvent` alanları 06'nın nesil, süre, oturum ve onay alanlarını karşılamıyor (03-sol.md notlar; 02-sol-2.md notlar; 04-sol-2.md notlar) | A1 olay kuralları; A4 |
| U-02 | Zaman çerçevesi ve parti kapanışı | 06, 01 ↔ iskelet §4, 02 | 06 parti sonlandırıcısı yalnız IMU ve zamanı yalnız örnek damgalarıyla taşıyor; 01 çağrıyı IMU gelişine bağlıyor ve eski `ida_core_tick_wall` imzasını kullanıyor; 02 IMU'suz `tick` tanımlıyor (02-sol-2.md notlar; 06-sol-2.md notlar) | A1; A4 |
| U-03 | Olay kimliği ve açılış sayacı | 02, 06, 03, 01 ↔ iskelet §3.2 | 02 ve 06 rastgele `boot_id` kullanıyor; iskelet kalıcı açılış sayacı ister; 03 sayacın sahibini belirtmiyor; 01 sayacı üretiyor ama tükenme davranışı 02'den bekleniyor; HELLO'da `boot_id` ve `boot_count` aynı değeri taşıyor (03-sol.md #5; 06-sol-2.md Y14; 01-sol-2.md Y9) | A1 ve A4; 01, 02, 06 birlikte |
| U-04 | Aralık ölçümlerinde ölçüm zamanı ve akustik zaman | 02, 05, 06 ↔ iskelet §3.1 | 02 BARO için dönüşüm ortası ve süre alanı yok; Ping1D gönderim alanı yok; 06'da eski "orta damga" dönüşüm notları ve Ping1D `t_meas=t_tx` kalmış; 05 aralık sonu kullanıyor (05-sol-2.md notlar; 06-sol-2.md notlar) | A4; A1; iskelet §3.1 açıklaması |
| U-05 | Kamera ölçüm zamanı | 05, 07 (ve algılama kaydı) | 05'te `t_meas` pozlama sonu, 07'de pozlama ortası; 07 pozlama sonrası gecikmeyi ortaya ekliyor ve algılamaya değiştirmeden kopyalıyor (05-sol-2.md notlar; 07-sol.md #12) | A2 kare sözleşmesi; A8 algılama eşlemesi |
| U-06 | Sonar kutu merkezi ve örnek anı | 03, 05, 06 | 03 ve 05 §8 `(i+½)` kullanıyor, 03-sol.md #18'e göre 05 §2.4.3 kutuyu `iΔr` ile, 05 §8'e göre 06 `t_tx + i·T_s/2` ile yazıyor | A1 sonar profili sözleşmesi |
| U-07 | Sağlık göstergesi | 01, 02, 03 | 01 iç sağlık ile veri kullanılabilirliğini ayırmak istiyor; 02 §4.4 `alive_ok` taze IMU şartı içeriyor; 03 iç tutarlılık hatasında IWDG'nin durmasını bekliyor (01-sol-2.md Y1; 02-sol-2.md notlar) | A4 |
| U-08 | FC ve çekirdek komut kapasitesi, olay sahipliği | 03, 02, 01 | 03 FC komut kapasitesi 4, 02 kapasitesi 8; arka plan imzası, olay kimliği üretimi ve ACK dönüşü iki belgede farklı sahiplerde; 01'de yuvalar daraltıcı komutlarla dolabiliyor (03-sol.md #28, #32; 01-sol-2.md Y12) | A4; A1 |
| U-09 | Algılama kaydı alanları ve hızı | 06, 03, 07 | 06 DETECTION'da `t_sigma_ns` ve `calib_ver` yok; 06 en çok 4 algılama/kare, 03 ve 07 en çok 8 kabul ediyor ve 10.200 B/s hesabı değişir (06-sol-2.md Y16; 07-sol.md notlar) | A1; A8 |
| U-10 | Emniyet geri okuması ve bakım girdisi | 06, 03, 05, 08 ↔ iskelet §5 | 06 SAFETY_IO'da tüp başına `emniyet_açık` ve `bakım` yok; 03 ve 05 iskelette bulunmayan `bakım` girdisini ekliyor, 08 üç girdiye uyuyor (06-sol-2.md Y15; 05-sol-2.md notlar; 08 §8) | A1; A4; iskelet kararı |
| U-11 | Aktüatör çerçevesi, komut ömrü ve PWM kaybı | 04, 06, 01 | 04 `t_ölçüm_ns`, 06 `t_sim_ns` kullanıyor; 50 ms ömrün başlangıç anı belirsiz; 04 PWM kaybında `son_değer` seçeneği bırakırken 01 reset/kilitlenmede nötrlük bekliyor (04-sol-2.md notlar; 06-sol-2.md Y7; 01-sol-2.md Y7) | A1; A5 ESC sinyal kaybı seçimi |
| U-12 | Aynı adımda emniyet daraltması ile atış sırası | 04, 06, 03 | 04'te aktüatör/atış olayları senaryo olaylarından önce işleniyor; aynı sınırdaki acil durdurma ya da pim değişiminin atıştan önce yetkili duruma yansıması garanti değil (06-sol-2.md notlar; 03-sol.md #25) | A1; 04 uygulaması |
| U-13 | Gövde referans noktası | 05, 02 ↔ iskelet §3.1, 04 | 05 §3 torpido ayrılınca kayan kütle merkezi referansı bekliyor; 02 tahsis matrisini CG referansıyla kuruyor; iskelet ve 04 sabit geometrik nokta kullanıyor (05-sol-2.md; 02-sol-2.md notlar; 04-sol-2.md notlar) | A4; A5 |
| U-14 | IMU dönme kolu etkisi | 02, 05 | 05 ivmeölçerin dönme kolu etkilerini ölçüme ekliyor, 02 modelden çıkarmıyor (02-sol-2.md Y8) | A4 |
| U-15 | DVL ve GNSS belirsizlik tanımı | 02, 05 | 02 DVL modelini sensör çerçevesinde, kovaryansı gövdede tutuyor; `eph` 05'te raporlanan σ olarak tanımlı (02-sol-2.md Y11 ve notlar) | A4 |
| U-16 | İtki eğrisi ve tahsis | 02, 04 | 02 yaklaşık karekök ters eğri, 04 polinom/tablo eğrisi kullanıyor; kaynağı aynı A5 sürümünden okunmalı (04-sol-2.md notlar; 04 §8) | A4; A5 |
| U-17 | Dalga fazı | 04, 07 | 04 `cos(ωt−kx+φ)`, 07 `cos(kx−ωt+φ)` (07-sol.md #1) | A2; 07 uygulaması |
| U-18 | Snapshot interpolasyonu ve olay sınırı | 04, 07 | 04 süreksizlik sınırlarını aşan interpolasyonu tanımlamıyor; 07 son iki durumu ve tek snapshot'ı yetersiz bırakıyor (04-sol-2.md Y7; 07-sol.md #11) | A2 |
| U-19 | Torpido itki kalıntıları | 03, 04, 07 ↔ iskelet 0.7 | 03, 04 ve 07'de iskelet 0.5–0.6'dan kalan itkili torpido gösterimleri (VizState itki durumu vb.) var (05-sol-2.md notlar; 01-sol-2.md notlar; 07-sol.md notlar) | A2; atış kontrol dar kapsamlı iş |
| U-20 | Akustik malzeme değerlerinin yeri | 05, 07 ↔ iskelet A7 | 05 §3 değerleri A5 tablosunda gösteriyor, 05 §2.4.1 ve 07 A7'de; kural "A7 varsayılan, A5 geçersiz kılar" (07 §8.1 md.3) | A7; A5 |
| U-21 | Kamera algılama bağlantısının adı | 07 ↔ iskelet 0.6 | 07 §3.5 bağlantıyı adsız sayıyor, iskelet A8 olarak adlandırdı (07-sol.md #20) | A8; 07 atıfları |
| U-22 | Yer gerçeği sınırı | 07, 03, 05 ↔ A8 | 07 bileşene yer gerçeği verilmediğini söylerken halka fizik gerçeği montajını taşıyor; anlamsal algılama üreteci aynı bileşene takılabilir deniyor (07-sol.md #13; 03 §8.4; 05 §8) | A8 |
| U-23 | Sensör girdileri | 04, 05, 08 | 04 `water_pressure`, `water_salinity`, tek seferlik zamanlayıcı olayı ve su girişini sağlamıyor; manyetometre için işaretli motor akımı 04'ün negatif olmayan besleme akımıyla aynı alan değil (04-sol-2.md notlar; 05-sol-2.md notlar; 08 §8) | W3 fizik/sensör bütünleştirmesi |
| U-24 | Ölçüm planı ve derleme planı | 09 ↔ 01 | 09 MP2/MP4 eşikleri güncel tam yol ve en büyük kart payıyla eşit değil; `protocol/conv/` yolu ve LVGL istisnası güncel 01'le uyuşmuyor (01-sol-2.md Y16 ve notlar) | 09 ölçüm/derleme planı işi (MP2/MP4/MP8, W2) |
| U-25 | Bariyerli koşu süresi | 06, 01 | 06 §7 bariyerli 400 Hz için 30 dk, 01 K7 bir saat ister; iki ayrı ölçümdür (M17 ve M5) (09 §8.1 md.7) | MP4 ve MP2 tanımı |
| U-26 | Kart testlerinin Windows'ta tekrarı | 01 ↔ 09 | 01 §7 her kart testinin macOS ve Windows'ta tekrarlanmasını ister; agent'lar yalnız Mac'te çalışır, Windows yarısı MP8 kapsamında kullanıcıda (09 §8.1 md.6) | MP8; 01 notu |
| U-27 | Adım dönüşümü ifadesi | 05 ↔ 04 | 05'in 04'e atfettiği eski 33/34 adım ifadesi güncel değil; 04 artık 26/27 kullanıyor (06-sol-2.md notlar) | 05 notu |
| U-28 | A6 kapsamı | iskelet A6, 06, 08 | A6 "bütün A1 ve A3 trafiği" diyor; A3 telemetrisi sim durumunun türevi; 06 §2.13 "bütün A1 çerçeveleri" ile kademe tablosundaki "sonar yalnız Tam" arasında gerilim var (08 §8 11.1) | A6 |
| U-29 | Kamera algılayıcı kayıt tanımları | 03, 06, 07 | 03'ün RB2/B2/RB3 ve SENSOR çerçevesi kuralları ile 06'nın kayıt biçimi ve 07'nin kare/parti kimliği eşlenmemiş (06-sol-2.md notlar; 03-sol.md #15) | A8; A1 |

## 5. Kapsam dışı kalan bulgular

İskelet 0.7 ve kesin kararlar (ardıl kart ve gerçek sensör yok; yarışma kuralları yok; itkili torpido, uzun menzil ve atış çözümünün ayrıntılandırılması yok) nedeniyle artık geçerli olmayan ya da kapatılmayacak bulgular. Kimlikleri 3. bölümdeki dizinin devamıdır; sayıma dahil edilmemiştir. Bu satırlarda kapsam yorumu mimara aittir: örneğin 03 #11 atış ölçütünün seyrüsefer belirsizliği ayrıntısıdır ve 03 #22 ile #23 atış çözümünün ayrıntılandırılmasına girer; mimar bunlardan birini kapsam içi sayarsa ilgili satır ATIS grubuna taşınır.

| Kimlik | Kaynak | Önem | Bulgu | Gerekçe |
|---|---|---|---|---|
| AB-225 | 03 · 03-sol.md #1 | kritik | Bölüm iskelet 0.5'in itkili torpido ve yaklaşık 3 m/s, 10 m menzil gereğini uygulamıyor; tablo üretimi ve menzil değerlendirmesi itkili modeli kapsamıyor. | İskelet 0.7 itkili torpidoyu kapsam dışı bıraktı; 0.5 kararı geri alındı |
| AB-226 | 03 · 03-sol.md #11 | kritik | Seyrüsefer belirsizliği yalnız σ artışından üretiliyor; hedef–seyrüsefer çapraz kovaryansı ve zamanlar arası hata modeli atış ölçütünde yok. | atış ölçütü ayrıntılandırması; kapsam yorumu mimara bırakıldı |
| AB-227 | 03 · 03-sol.md #22 | önemli | Hedef yönelimi izde taşınmadığı için "hedef düzlemindeki" hata ve isabet bölgesi geometrisi genel durumda hesaplanamıyor. | atış çözümünün ayrıntılandırılması; iskelet 0.7 atış ölçütünü basit hizalanma ve menzil koşuluyla sınırladı |
| AB-228 | 03 · 03-sol.md #23 | önemli | Gecikme kovaryansında bilinen açısal hareket ile hareket kestirim hatası karıştırılmış. | atış çözümünün ayrıntılandırılması |
| AB-229 | 03 · 03-atis-kontrol.md §8.3 | belirtilmemiş | Atış menzili (≈1 m) ile sonar kör bölgesi çakışıyor; seçenekler arasında torpido varsayılanlarını uzun menzil için gözden geçirmek ve öne bakan ikinci menzil ölçer eklemek var. | iskelet 0.7 çakışmayı kabul edilen sınır sayıyor, ikinci menzil ölçer eklenmiyor ve torpido yetenekleri artırılmıyor |
| AB-230 | 03 · 03-atis-kontrol.md §8.4; 04 · 04-simulator-cekirdegi.md §8 | belirtilmemiş | Sürekli itkili torpido ve sonlu süreli tüp modeli ile torpido sürekli çarpışma gereksinimi (A25) sonraki kademeye bırakılmış. | iskelet 0.7: itkili torpido kapsam dışı |
| AB-231 | 03 · 03-atis-kontrol.md §8.4 | belirtilmemiş | Gerçek tetik devresi gecikme dağılımı bu aşamada yok; `L_çıkış` yalnız protokol ve sim uygulama gecikmesini temsil ediyor. | gerçek donanım kapsam dışı |
| AB-232 | 07 · 07-sol.md notlar (04 ve 03 ile komşu) | belirtilmemiş | İnceleme, iskelet 0.5–0.6 uyarınca 07'nin torpido itki durumu istemesini doğru bulmuştu; iskelet 0.7 itkili torpidoyu kapsamdan çıkardı. | A2'deki VizState temizliği ayrı satırda |
| AB-233 | 07 · 07-gorsellestirme.md §8 (açık noktalar) | belirtilmemiş | Hareket bulanıklığı, kayan perde, araç lambası, kostik, parçacıklar ve yarı batık kamerada piksel başına su altı/su üstü maskesi ilk sürümde yok. | bölümün kendi ilk sürüm sınırı; sonraki kademe |

## 6. İskelete öneriler (mimar kararı bekliyor)

Bölümlerin ve incelemelerin iskeletin (sürüm 0.7) değişmesini önerdiği noktalar. İskeleti yalnızca mimar oturum değiştirir; bu liste karar vermez. Kaynaklar parantez içindedir.

1. §3.2 ve §4: algoritmik kontrol tetikleyicisini ("yeni IMU örneğinin gelişi") taşıma sonlandırıcısından ayır; "IMU örneğini içeren adım partisinin kapanışı tetikler" (01 §8.4 md.1; 01-sol-2.md notlar).
2. §4 bariyer tablosu: "her sensör çerçevesinden sonra" ifadesini "her kontrol periyodunun zaman çerçevesinden/partisinden sonra; yanıt sim zamanında sabit `L_act` gecikmesiyle uygulanır" biçiminde netleştir (06 §8 md.2; 06-sol-2.md notlar).
3. §3.1: profil boyunca toplanan akustik ölçümde başlık gönderim anını taşır; çok yollu profilde kutu zamanı nominal doğrudan-yol zamanıdır ve tamamlanma/örnekleme olayları ayrılır (03 §8.2 md.1, 0.5'te karşılanmış görünüyor; 05 §8 ve 05-sol-2.md notlar).
4. §3.1: sıcaklık birimi (°C) kabulü iskelete yazılsın (06 §8 md.5).
5. §3 A1 satırı: itici komutları ESC darbe genişliği (`pwm_us`) olarak taşınır, çeviri kartın platform katmanındadır (06 §8 md.3).
6. §3: algılama kaydı çerçeve kuralı "ölçüm sensör çerçevesinde; dünya dönüşümü yalnız kartta" (03 §8.2 md.4).
7. §3.2: izin veren olay süreli olur ve yenilenmezse düşer; olayın oturum kapsamı, son geçerlilik ve alıcı yanıtı yazılsın (03 §8.2 md.3, kısmen 0.5'te karşılanmış; 06-sol-2.md notlar: iskelet kararı değişmeden 01, 02 ve 06 kalıcılık ve taşma davranışını tamamlamalı).
8. §5 tablosu: fırlatıcı durumu (tüp dolu, emniyet, fiş, pim) ve sanal emniyet girdileri satırı (03 §8.2 md.2, 0.5'te karşılanmış görünüyor); "bakım" girdisinin eklenip eklenmeyeceği mimar kararı bekliyor (03 §8.2 md.2 ekler; 05-sol-2.md notlar zorunlu saymamayı, 08 §8 üç girdiyi öneriyor).
9. §5: torpido ve fırlatıcı varsayılanlarının mimari varsayım olduğu ve yük durumlarında aracın eklenmiş kütlesinin değişmediği kabulü iskelete not düşülsün (04 §8 md.5).
10. §5 sensör seti: iç basınç ve iç sıcaklık sensörünün (ArduSub arıza girdisi) eklenip eklenmeyeceği mimar kararı bekliyor (02 §8.2 md.2; 05 §8).
11. §7 depo düzeni: `firmware/third_party/`, `firmware/cubemx/`, `firmware/bsp/`, `docs/sozlesmeler/`, `docs/olcumler/`, `docs/gorevler/`, `docs/mimari/acik-bulgular.md`, `cmake/`, `CMakePresets.json`, `vcpkg.json`, `.github/workflows/`, `tests/kabul/`; 01 §8.4 md.3 ve 09 §8.1 md.2 `protocol/conv/` yolunu da öneriyor, 01-sol-2.md notlar ise tek modül `protocol/ida_link` ister (01 §8.4 md.3; 09 §8.1 md.2; 01-sol-2.md notlar).
12. §2 tablosu "Kart yazılımı C17": çekirdek ISO C17, platform kodu `gnu17` ve uzantılar yalnız `firmware/` içinde (01 §8.4 md.2).
13. §8 A1 taşıması yedeği: "ST'nin USB yığını" yerine "HAL PCD + upstream Eclipse USBX (MIT)" (01 §8.4 md.4).
14. §3 ve §8: A1 iki aşamalı dondurulur (anlam W0'da, taşıma ve tel biçimi eki W1 sonucundan sonra) (09 §8.1 md.1). **09 sürüm 0.2: sıra değişikliğiyle gereksiz kaldı (09 §2.1).**
15. §6: "emniyet kabulü kalite kademesinden bağımsızdır; kameraya bağlı davranış yalnız kamera açıkken sınanır" (09 §8.1 md.3).
16. §8 A1 taşıması ile kontrol hızı: VCP yedeğinin 200 Hz ve `L_act = 3h` gerektirdiği notu; 06-sol-2.md notlar hesabın bu zorunluluğu kanıtlamadığını söylüyor, koşulsuz bağ eklenmemeli (06 §8 md.1; 06-sol-2.md notlar).
17. §3 yeni dış arayüz: emülatör arkasında "sim sunucusu ↔ harici yer istasyonu" (MAVLink/UDP) arayüzü ve numarası; QGC'nin hedef mi prova mı olduğu yanıtsız (06 §8 md.4; 08 §8; 09 §8.2 md.5).
18. A2 satırı: içerik pozlar ve hızlar, dalga bileşenleri, sim zamanı, ışık ve su parametreleri, sahne olayları (gövde doğumu ve ölümü, kütle durumu, isabet), kamera istekleri, el sıkışmada A2 sürümü ve A7 manifest özeti olarak genişletilsin; A8 için yeni numara gerekmiyor (07 §8.2 md.1; 07-sol.md notlar).
19. §8 kamera satırı: "tek sahnede iki zaman: ayrık çizim / ortak yakalama zamanı" kararı eklensin ya da M28 kapsamına alınsın (07 §8.2 md.3).
20. §6 sade kademede dalga seçeneği (kaba ağ, kesmesiz doğrudan toplam): maliyeti ölçülmeden önerilmez, M12 ile birlikte değerlendirilmeli (07 §8.2 md.5).
21. §5 bilinen sınır: Ö5-k yedeğe düşerse kör bölgedeki atış yeteneği pencere görünürlüğüne bağlı kalır, iskelete not edilsin (07 §8.2 md.6).
22. A6 satırı: "Bütün A1 ve A3 trafiği" yerine "A1 trafiği, A3 komutları ve olayları" (08 §8, 11.1 md.1).
23. A6 satırı: tekrar oynatma için üç düzey (izleme, fizik yeniden koşturma, girdi tekrarı) ayrımı (08 §8, 11.1 md.2).
24. Mimar kararı: ek Linux CI işi (09 §8.2 md.8) ve self-hosted kartlı runner (09 §8.2 md.9) şu an kurulu değil.
25. Kullanıcı yanıtı bekleyen 00 açık soruları iskelet kararlarını etkiliyor: A6 (QGC), A16 (yapı zamanında Python/Node), A23 ve A24 (kamera görünürlüğü, çözünürlük, hız, sayı), A26 (macOS sürümü), A27 (iki pencere / LibGodot) (09 §8.2; 07 §8; 06 §8; 03 §8.4).
26. 00 numaralı belge: Ö6 açık görünüyor, K9 ile birlikte okunmalı (web panel tercihi kesin, ayrı pencere/gömme yerleşimi öneri) (07-sol.md notlar).
