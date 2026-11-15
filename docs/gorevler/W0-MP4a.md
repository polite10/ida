# W0-MP4a — Kartta 400 Hz döngü yükü, yer tutucu çekirdek adımıyla (ön tur)

| | |
|---|---|
| Kimlik, dalga | `W0-MP4a`, W0 (ölçüm prototipi) |
| Uygulayıcı | `ida-uygulayici` (Opus) |
| İnceleyen | sol (`tools/sol-incele.sh`; çağrıyı mimar başlatır) |
| Dal | `olcum/mp4-dongu-yuku` |
| Tarih | 2026-11-15 |

## Amaç

Kontrol hızı kararının (400 Hz mi, yedek 200 Hz mi; iskelet §4 ve §8, M5) ilk sayısal girdisini almak: bölüm 02'nin öngördüğü çekirdek adımının hesap hacmini taklit eden bir **yer tutucu adım**, kartta 160 MHz'te ne kadar sürüyor? Gerçek çekirdek henüz yazılmadı (W3); bu ölçüm ön bilgidir, gerçek çekirdekle W3 sonunda yinelenir. Bağlantı (A1) bu işte yoktur: "varıştan çıktıya" süresi MP3'ten sonra ölçülür. Kullanıcı kararıyla (2026-11-16) sistem tek kabloyla, yalnız ST-LINK portu üzerinden çalışır; A1 taşıması ST-LINK sanal seri portudur (USART1), USB HS yolu kullanılmaz. Bu ölçümde seri port yalnız sonuç bildirimi içindir.

## Okunacaklar

- `AGENTS.md`
- `docs/mimari/09-test-ve-is-akisi.md` (0.2): 2.6 (kayan nokta bayrakları), 3.3, 6.1 (MP4 satırı)
- `docs/mimari/02-ortak-cekirdek.md`: adımın yapısı ve sırası, kestirim (15 durumlu hata durumlu EKF, `f32`, sabit gecikmeli füzyon ufku), güdüm, kontrol, itki tahsisi başlıkları; sensör hızları
- `docs/mimari/01-kart-yazilimi.md`: 400 Hz döngü, zamanlama ve dilimler, §6, §7'de K7
- `docs/olcumler/MP1-arac-zinciri.md` ve `tools/olcum/mp1/` (saat yapılandırması, yükleme ve kayıt betikleri; düzeltilmiş 160 MHz geçiş dizisi buradan alınır)
- `docs/research/00-duzeltmeler-ve-kararlar.md`: M5, Ç17 (FPU ve sayı tipi)

## Ortam

- Mac: Apple M3 Pro, macOS 26.5.2. Kart ST-LINK portundan bağlı (hub üzerinden); ikinci USB bağlantısı gerekmez. Araç zinciri MP1'deki gibi (GNU Tools for STM32 14.3.rel1, STM32CubeProgrammer CLI, CMake + Ninja); ST kaynakları `${IDA_CACHE:-$HOME/.cache/ida}/src` altında hazır.
- Kartta şu an MP1 ölçüm imajı koşuyor. Dahili flash'ın fabrika yedeği `${IDA_CACHE}/yedek/` altında doğrulanmış duruyor; yükleme için `tools/olcum/mp1/yukle.sh` ve `yedek-dogrula.sh` kullanılabilir (çağırmak serbest, değiştirmek yasak).
- Uzun koşular `caffeinate -dims` altında; güç durumu kaydedilir.

## Kapsam ve sıra

1. `tools/olcum/mp4/` altında aygıt yazılımı (platform kodu `gnu17`; adım kodu ISO C17, `-std=c17`, tek duyarlıklı; bayraklar 09 §2.6: `-ffp-contract=off -fno-fast-math -Wdouble-promotion -Wfloat-conversion`). Saat: MP1'in düzeltilmiş dizisiyle 160 MHz (SMPS).
2. **Tetik:** donanım zamanlayıcısı 400 Hz kesme üretir; ana döngü her tetikte bir adım koşar. Adım bir sonraki tetikten önce bitmezse "kaçırılan adım" sayılır.
3. **Yer tutucu adım** (02'nin yapısını ve boyutlarını izler; sayısal doğruluk aranmaz, hesap hacmi ve bellek erişim düzeni gerçekçi olmalı):
   - Kestirim: 15 durumlu hata durumlu EKF, `f32`. Her adımda durum yayılımı (kuaterniyon dahil) ve yoğun 15 × 15 kovaryans yayılımı. Ölçüm güncellemeleri sıralı skaler güncelleme olarak: manyetometre (3), derinlik (1), DVL (3), GNSS (02'deki boyutla). 02'nin sabit gecikmeli füzyon ufku (D = 150 ms) ek hesap gerektiriyorsa (tampon, yeniden yayılım) o da taklit edilir; 02'de belirsizse en pahalı makul yorum seçilir ve raporda yazılır.
   - Güdüm ve kontrol: LOS güdüm, kademeli PID (6 eksen), 8 iticiye itki tahsisi (doygunluk işleme dahil).
   - Mesaj işi: gelen ≈ 300 bayt ve giden ≈ 100 bayt üzerinde CRC-16 (MAVLink'in kullandığı polinom) ve alan kopyalama.
   - Girdiler belirlenimci bir sözde ölçüm dizisinden; derleyicinin hesabı silmemesi sağlanır (çıktılar bir sağlama toplamına katılır ve seri porttan bildirilir).
4. **İki yük durumu:** (a) *olağan*: ölçümler 02'deki hızlarında gelir (çoğu adımda yalnız IMU); (b) *en kötü*: her adımda bütün ölçüm güncellemeleri birlikte yapılır (üst sınır).
5. **Ölçüm:** adım süresi DWT çevrim sayacıyla (kesmeler açık, gerçekçi koşul). Her yük durumu için en az 10 dakika (≥ 240 000 adım): en küçük, p50, p99, p99,9, en büyük; 10 µs kutulu histogram; kaçırılan adım sayısı; tetik gecikmesi (zamanlayıcı kesmesinden adımın başına; en büyük ve p99,9). Seri port çıktısı ölçülen bölgenin dışında üretilir (ölçümü bozmaz). İki derleme: `-O2` (birincil) ve `-Og`.
6. **Ek gözlemler:** adım yolunda çift duyarlıklı yazılım kayan nokta çağrısı (`__aeabi_d*`) olmadığı (bağlanmış ELF'ten); yığın tepe kullanımı (desen doldurma); adım kodunun flash ve RAM boyutu.
7. **Rapor:** `docs/olcumler/MP4-dongu-yuku.md` (09 §3.3 biçimi).

## Geçme ölçütleri (bu görev belgesi dondurur; ölçülmüş değer değil, hedef)

| # | Ölçüt |
|---|---|
| D1 | İki yük durumunda, `-O2` ile 10 dakikada kaçırılan adım 0 |
| D2 | *En kötü* yük durumunda adım süresinin en büyük değeri ≤ 1,0 ms (`-O2`) |
| D3 | Adım yolunda çift duyarlıklı yazılım kayan nokta çağrısı yok |
| D4 | Histogram, yığın tepe kullanımı ve kod boyutu raporlanmış (eşik yok) |

D2 geçmezse zorlamadan raporla: hangi parça ne kadar sürüyor (parça başına süre dökümü), 200 Hz'te (2,5 ms periyot) durum ne olurdu. "Geçmedi" geçerli bir sonuçtur. Yer tutucunun gerçek çekirdeği temsil etme sınırları raporda açıkça yazılır; sonuç "ön bilgi"dir.

## Dosya sahipliği

- Yazılır: `tools/olcum/mp4/**`, `docs/olcumler/MP4-dongu-yuku.md`.
- Dokunulmaz: `tools/olcum/mp1/**` (çağrılabilir), `docs/mimari/**`, `docs/research/**`, `docs/DURUM.md`, `AGENTS.md`, `README.md`, `docs/gorevler/**`, `.gitignore` (gereken satır zaten var: `tools/olcum/*/derleme/`).
- Bu kod atılabilir prototiptir; ürün dizinlerini (`core/`, `firmware/`) oluşturma.

## Yasaklar

- Seçenek baytlarına yazma yok; harici flash'a dokunma; ST-LINK aygıt yazılımını güncelleme. Yükleme yalnız doğrulanmış yedek varken.
- Sözleşme (A1, A4) yazma ya da varsayma; USB, RTOS, ekran, gerçek sensör sürücüsü bu işte yok.
- Atış kontrol mantığı bu işte yok (yük taklidi gerekmez).
- Ölçülmemiş rakamı sonuç gibi yazma. Kapsam dışı: başka kart, Rust.
- Commit atma, `git add` yapma, `git push` yapma, başka agent başlatma. Dalı (`git switch -c olcum/mp4-dongu-yuku`) oluşturabilirsin; commit'i kayıt agent'ı atar.

## Rapor ve bitiş

Son iletinde ayrı başlıklarla, kısa: **yapılan**, **doğrulanan** (komut ve gerçek çıktı özetiyle), **doğrulanmayan**, **02'de yaptığın yorum seçimleri**, **çalışma dizininin yolu ve dal adı**, görev belgesinde hatalı bulduğun nokta. İş, rapor dosyası yazılıp dört ölçütün her biri "geçti" ya da gerekçeli "geçmedi" olarak işaretlendiğinde biter. Karta erişim kesilirse dur ve raporla. Düzeltme turu sınırı iki.
