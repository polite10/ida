# Durum

Son güncelleme: 2026-11-21

Bu belge projenin şu an nerede olduğunu söyler. Anlamlı bir iş parçası bittiğinde güncellenir. Kararlar ve düzeltmeler için [research/00-duzeltmeler-ve-kararlar.md](research/00-duzeltmeler-ve-kararlar.md) belgesine bakılır.

## Şu an

- **Z1 artık A1 mesajları üzerinden çalışıyor; beş PR açık ve sol incelemesi bekliyor (2026-11-21 gece; günceldir):** açık PR'lar (hiçbiri birleştirilmedi): #24 W3-zincir (görev modülü, GUIDED_SP güdümü, emniyet mandalı, derinleşme yasağı; Windows CI yeşil; iki sol turu yapıldı, üçüncü dar denetim bekliyor), #22 W4-sil (kartsız koşucu `ida_sil`; Windows CI yeşil, Winsock dalı ilk kez derlendi), #23 W4-sensor (sensör modelleri; Windows CI yeşil), #25 W3-kart dilim 2 (kart `ida_link` üst katmanına bağlı; gönderim yöneticisi, kalıcı bellek), #26 W4-kopru (sim tarafı A1 köprüsü, zaman yöneticisi, başsız `ida_sim`; #22–#24'ü içerir). **Z1, A1 üzerinden:** `ida_sim --kip sil` hem süreç içi karşı tarafla hem `ida_sil` ile iki süreç olarak W3-zincir sınırlarını geçti (dalış oturma 5,395 s, aşma 0,056 m, tutma 0,0051 m / 0,0105 rad; ölçüm, bu Mac; uygulayıcı bildirimi, sol incelemesi bekliyor); aynı tohumla iki koşu bayt bayt aynı CSV. **Kart dilim 2 (ölçüm, kart; ön değer):** 600 s oturumda kart sayaçları 0, 120 000 partiden 2'si PC'nin (Python sahte sim) art arda göndermesiyle birleşti (katı geçiş değil); step ortalama 622 µs, en kötü 1 694 µs (silahsız, INIT kipinde; etkin kontrol altında ölçülmedi), tick en kötü 343 µs, 200 Hz ızgara kaçırma 0; `PARAM_SAVE`'i işleyen step 2,5 ms; tel katmanı oturumda 485 ns/B, periyot en kötü 0,63 ms (MP5 eşiği 0,50 ms; 394 600 periyodun 4'ünde aşıldı); parametre kaydı, kayıt sırasında IWDG, T_bağ, yeniden bağlanma geçti. **Sürenler:** Z2 (aynı senaryo kartta, `ida_sim --kip hil --seri`; görev belgesi `docs/gorevler/W4-z2.md`), W4-senaryo (A5 çözümleyici), W4-kopru bağımsız kabul testleri (emniyet). **sol kullanım sınırı** gün içinde ikinci kez doldu (17:00 dolayı; 20:31'de açılıyor): beş inceleme (zincir-3, sensör, sil, kart dilim 2, köprü) açılışta başlamak üzere kuruldu. Süreç dersi: Windows CI test yazarının kabul testlerinde MSVC `/analyze` C6385 ile düştü (PR #24); test yazarı kalıbı dört dosyada taradı. Windows CI süresi ≈ 16–20 dk.
- **W3 büyük ölçüde `main`'de; ilk kapalı çevrim çalıştı (2026-11-21 akşam; eski):** `main`'e girenler (hepsi sol incelemesinden ve Windows CI'dan geçti): PR #13 tel hızlandırması, #14 parametre testi, #15 kontrol ve itki tahsisi, #16 kestirim (yönelim ve derinlik), #17 fizik çekirdeği (dilim 1), #18 `ida_link` üst katmanı (oturum, parti, komut sırası; bağımsız kabul testleri 35 vaka; sol ikinci tur: dört emniyet bulgusu kapandı), #19 web panelleri (dilim 1: beş panel, sahte A3 sunucusu). Her sol incelemesi gerçek hata buldu (kontrol 4 kritik, kestirim 9, görev 5 + ikinci turda 2, oturum 8, fizik 4, kart 5, web 12, görselleştirici 11); hepsi karşı örnek testleriyle düzeltildi ya da düzeltiliyor. **Z1 (kartsız ilk zincir) çalıştı** (dal `is/w3-zincir`, henüz `main`'de değil; sol incelemesinde): simüle edilen İDA-0, PC'de koşan gerçek çekirdekle `ARM` → `GUIDED` → `GUIDED_SP` ile 1,5 m'ye daldı; oturma 5,39 s, aşma 0,053 m, tutmada derinlik hatası 0,0032 m, yön 0,0126 rad (ölçüm; aynı süreçte, `tests/zincir`; kazançlar bu fizik modeline göre ayarlandı; itici arızası adımı henüz anlamlı değil, yeniden kuruluyor). Görev modülü (kipler, araç silahlandırması, arıza güvenliği) `main`'e bu dalla girecek. Sürenler: W3-kart (dilim 1 kartta çalıştı: %60 yükte 600 s kayıp 0, 200 Hz ızgara kaçırması 0; sol 5 kritik + 2 önemli bulgu düzeltiliyor; ölçümler ön değer), W3-viz (Godot 3B pencere çalışıyor; sol 11 kritik + 1 önemli bulgu düzeltiliyor), W4-sensor ve W4-sil (yeni başladı). Mimar kararları W3-K1…K14 `docs/gorevler/W3-ortak.md`'de. Süreç dersleri: Windows CI ilk hatada durduğu için PR #18 dört denemede geçti → yerelde statik analiz (W3-K12) ve Clang ile MSVC kalıp taraması (W3-K14) zorunlu; bu Mac'te ASan'lı ikililer başlarken takılıyor (ortam; kök neden araştırılmadı), `host-asan` testleri yalnız CI'da koşuyor; sol kullanım sınırı gün içinde bir kez doldu, kullanıcı yeniledi. Mimar tahmini (ölçüm değil): süren işler bitince toplam işin yaklaşık yarısı.
- **W3 ara durum (2026-11-21, öğleden sonra):** birleşik `main` Windows CI'da yeşil (koşu 44896495188); macOS CI düzeltmesi PR #12 ile birleşti (kök neden: `-fsanitize=address` ile Clang'in kendi `asan_ignorelist.txt` dosyası bağımlılık sayılıyordu; düzeltmeyle macOS koşusu yeşil, ASan testleri dahil). Bu Mac'te ASan'lı programlar başlarken takılıyor (boş program bile; ortam sorunu, kök nedeni araştırılmadı): `host-asan` testleri yalnız CI'da koşuyor. W3-kontrol, W3-kestirim, W3-tel-hiz ve W3-gorev kabul testleri yazıldı; üçünün sol incelemesi sürüyor. İki bağımsız rapor aynı belirsizliği buldu (acil durdurmanın araç silahlandırmasıyla ilişkisi); mimar kararları W3-K1…K6 `docs/gorevler/W3-ortak.md`'de: acil durdurma çekirdekte yalnız atış kontrol izinlerini kapatır, araç `ARM`'ını ve itkiyi kesmez (A4 §3.17 ve A4-K116); `ARM` kestirim `OK` ya da `DEGRADED` iken kabul edilir; kipler kestirimin bileşen geçerliliğine bakar. W3-fizik, W3-gorev, W3-oturum, W3-kart, W3-web sürüyor.
- **W3 başladı (2026-11-21):** dokuz agent paralel çalışıyor: W3-fizik, W3-kestirim, W3-kontrol, W3-gorev (+ bağımsız kabul testleri), W3-oturum, W3-tel-hiz, W3-kart (kartı yalnız bu iş kullanır), W3-web; görev belgeleri `docs/gorevler/W3-*.md`, ortak kurallar ve hedef (ilk çalışan zincir: Z1 kartsız, Z2 kartlı derinlik tutma) `W3-ortak.md`. Birleşik `main` yerelde (macOS, host-debug) 55/55 test geçti. `main` üzerindeki macOS CI koşusu (44896497943) tek testte düştü: `cekirdek.siniri.basliklar`, yalnız host-asan'da, koşucudaki Xcode yolunu sistem kökü saymıyor; düzeltme işi açıldı (`is/ci-mac-baslik`). Windows koşusu (44896495188) sürüyor.
- **Sözleşmeler donduruldu, W2 PR'ları birleşti, K20 (2026-11-21; günceldir):** A1–A8 sürüm 1.0 olarak donduruldu (PR #11, birleştirme commit'i `8df5b4a`; etiketler `sozlesme-A1-v1` … `sozlesme-A8-v1`). PR #8 (`4227060`), #9 (`e3f530c`) ve #10 (`78bbfd1`) `main`'e birleşti; #10'da `tests/CMakeLists.txt` çakışması iki kabul dizini de korunarak çözüldü. `main` üzerinde Windows (`derleme.yml`, koşu 44896495188) ve macOS (`derleme-mac.yml`, koşu 44896497943) koşuları başlatıldı; sonuç henüz yok. Kullanıcı kararı K20 (hızlandırma) karar günlüğüne ve `AGENTS.md`'ye işlendi. Kart başka USB portuna takıldı; seri uç şimdi `/dev/cu.usbmodem21403` (aynı ST-LINK, seri no sonu `…3831`; betikler ilk `usbmodem` ucunu kendisi bulur). Sıradaki: W3 işleri paralel başlıyor (görev belgelerini mimar yazıyor).
- **Fark incelemesi turu ve W2 kod incelemeleri kapandı; dondurma kullanıcı kararını bekliyor (2026-11-20, akşamüstü; günceldir):**
  - **W1 fark incelemesi (sol, iki parça; `40b8d63..6b63c6e`) bitti:** iki parça da "dondurulamaz" dedi; doğrulama turunun 40 bulgusundan 36'sı kapanmış, 4'ü kısmen; yeni 5 kritik, 5 önemli, 1 küçük bulgu. Hepsi mimar kararlarıyla (D-17…D-27; `docs/gorevler/W1-uyum.md` "Fark incelemesi turu") düzeltildi; iskelet 0.12 (D-17: kartın açık ve bekleyen partisi de bekleme noktasıdır). Aynı tura çekirdek uygulayıcısının 6 açık okuması, çekirdek kabul testlerinin 20 belirsizliği ve uygulama sırasında çıkan ek okumalar (kayıt tamamlanmasının kuşak eşleşmesiyle saptanması, `FAIL`'in kuşak taşıması, mandallı `core_ok`, oturum sonu kapanması, atılan genişleticinin nihailiği, tel sayaçlarının dört okuması) eklendi. Bütünleştirme dalı `sozlesme/w1-butunlesme` @ `2a7d178`; kural sayıları A1 129, A2 93 (yeni A2-K97), A3 125, A4 130, A5 111 etkin, A6 61, A7 41, A8 82; mimarın mekanik denetimi temiz (tanımsız atıf 0). Tel biçimi, `dialect_hash` (`0x3939788984A5FF80`), altın çerçeveler ve yapı yerleşimleri değişmedi (yalnız `param_gen` açıklamaları). Sözleşme dalları: `a1-v1` `4c05a84`, `a4-v1` `fb849bb`, `a2a7-v1` `f980fc5`, `a3a6-v1` `f709dd7`, `a8-v1` `a867094`; `a5-v1` değişmedi. Bu turun düzeltmeleri sol'a yeniden incelettirilmedi.
  - **W2 sol kod incelemeleri (üç iş) bitti, üçü de "birleştirilemez" demişti; bulgular düzeltildi, üç PR Windows CI'da yeşil:** PR #10 `is/w2-cekirdek` @ `f62017c`: 4 kritik + 1 önemli (bütünlük denetiminin üzerine yazılan bozulmayı kaybetmesi; kayıt tamamlanmasının durum kodu değişmeyince kaçması; birimlemede taşma; kabul testlerinde yapı-dizi erişimi ve sayaç sarması); birim testleri 84 vaka, kabul testleri 138 (4 IGNORE: gevşetme W4'te); `sizeof(ida_core_t)` 2160 B, fw-release çekirdek kodu ≈ 19,8 KB; çıktı `docs/gorevler/inceleme/W2-cekirdek-sol.md` (PR dalında). PR #9 `is/w2-protokol` @ `05ee0fc`: 2 kritik + 1 önemli (kabul testlerinin "belirsiz" grubu kesin testlere çevrildi; testlerde tür erişimi; rolün doğrulanmadan daraltılması); birim 37, kabul 65 test; ilk gönderim MSVC C4456 (gölgeleme) ile düştü, ikinci gönderimde yeşil. PR #8 `is/w2-depo` @ `edc2642`: 2 kritik + 2 önemli (çekirdek sınırı artık derleyicinin gerçek başlık bağımlılıklarını denetliyor; statik analizde araç/derleme hatası bulgu sayılmıyor, çıkış 70; ardıl kart deseni yalnız STM32U5A9'u serbest bırakıyor; macOS işi ayrı iş akışına (`derleme-mac.yml`) alındı ve yeni gönderim süren macOS koşusunu iptal ediyor); yeni denetimler MSVC altında da koştu ve geçti. Mimar denetimi: çekirdek kodu + güncel kabul testleri yerelde birlikte koşturuldu (host-debug 28/28); depo düzeltmeleri çekirdek dalıyla geçici bir kopyada birleştirilip yeni başlık denetimi gerçek çekirdeğe karşı koşturuldu (33/33).
  - **Durulan nokta (kullanıcı talimatı: "sol gerekli testleri yapınca onları düzelt ve bırak, yeni adım başlatma"):** çalışan agent yok; yeni iş başlatılmadı.
  - **Açık konular (küçük; iş açılmadı):** W2 dallarındaki sözleşme kopyaları bütünleştirme dalının son hâlinden geride (birleştirme sırasında eşitlenecek; `port.h`'de yalnız bir yorum farkı); PR #9 ve #10 dalları depo düzeltmelerini PR birleştirme başvurusu üzerinden alıyor (dalların kendisinde yok); macOS iş akışının iptal davranışı bir `ci-mac` koşusunda gözlenmedi; çekirdek kabul testlerinde yazılı olmayan iki nokta (arka plan çağrısındaki geri adım günlüğünün hangi çağrıda çıkacağı; tanımsız `link_event` taşıyan çağrıda `SESSION` günlüğü); A1'de platformun öteki retlerinin (`LINK_DROPPED`, `LINK_BAD_FIELD`) nihailiği yazılmadı; MP5 sonucunun A1 §8.1 O2 / A4 §4.13'e not olarak işlenmesi; `tools/denetim/bellek-ayirma.sh` MSVC desteği; önceki kullanıcıdan bekleyenler (MP8 Windows koşusu, MP3 temiz koşul yinelemesi ve ötekiler) aynen sürüyor.
- **W2 işleri Windows CI'da yeşil; MP5a sonucu; kullanıcı talimatı (2026-11-20; günceldir):** W2-cekirdek (PR #10): MSVC düzeltmeleri ve A4'ün son kararlarına uyum yapıldı; bağımsız kabul testleri eklendi (137 test; yazar uygulama kaynağını okumadı); Windows CI yeşil (`is/w2-cekirdek` @ `8e6468b`: 79 birim, 137 kabul testi, ASan, statik analiz). Çekirdek uygulaması A4'te 14, test yazarı 20 belirsiz nokta bildirdi; ilk 14'ü A4'te kapatıldı (130 kural, yeni kural yok) ve bütünleştirme dalına girdi (`sozlesme/w1-butunlesme` @ `6b63c6e`); kalanlar (uygulayıcının sonradan bildirdiği 6 ve test yazarının 20 maddesi) açık, sol fark incelemesinin A4 düzeltmeleriyle birlikte ele alınacak. W2-depo (PR #8 @ `f9469f4`) ve W2-protokol (PR #9 @ `0fa569e`) Windows CI yeşil; statik analiz artık `core/` ve `protocol/ida_link/`'i tarıyor. W0-MP5a tamamlandı (dal `olcum/mp5-tel-maliyeti`, PR yok; rapor `docs/olcumler/MP5-tel-maliyeti.md` o dalda): ürün kitaplığının ilk hâli kartta eşikleri aşıyor (ölçüm); prototipteki iyileştirilmiş kopya sağlıyor (ölçüm: E1 0,153 ms, E2 0,264 ms; bayt bayt beslemede sağlamıyor). Mimar değerlendirmesi (ölçüm değil): MAVLink 2 kararı sürer; iyileştirmelerin ürüne taşınması ve kart alım yolunun bayt bayt beslememesi ayrı iştir, henüz başlatılmadı. **Kullanıcı talimatı (2026-11-20):** sol 15:16'dan sonra incelemelerini yapınca bulgular düzeltilir ve orada durulur; yeni adım (dondurma, yeni W2/W3 işi, MP5 iyileştirmelerinin ürüne taşınması, macOS CI koşuları) başlatılmaz.
- **Doğrulama düzeltmeleri bitti; W2 işleri; MP5a ilk sonucu (2026-11-20; günceldir):** sekiz sözleşmenin doğrulama düzeltmeleri bitti ve bütünleştirme dalında yeniden birleşti (`sozlesme/w1-butunlesme`; A1 129, A2 92, A3 125, A4 130, A5 111 etkin, A6 61, A7 41, A8 82 kural; tel biçimi, `dialect_hash` ve altın çerçeveler değişmedi). sol'un kullanım sınırı yeniden doldu (15:16'da açılıyor); dondurmadan önceki sol fark incelemesi o saatte başlayacak. W2-depo (PR #8) ve W2-protokol (PR #9) Windows CI'da yeşil; ikisinin sol kod incelemesi ve macOS koşusu bekliyor. W2-cekirdek ikinci aşama yazıldı (taslak PR #10): çekirdek iskeleti, 70 birim testi; bağımsız kabul testleri ve sol incelemesi bekliyor. W0-MP5a (kartta tel katmanının maliyeti; `docs/gorevler/W0-MP5a.md`) ölçüldü: ilk uygulama iki eşiği de aştı (1 050 B ayrıştırma 0,556 ms, eşik 0,20 ms; periyot toplamı 0,781 ms, eşik 0,50 ms; kartta, `fw-release`, 160 MHz; ölçüm). Mimar değerlendirmesi (ölçüm değil): maliyet biçimden değil uygulamadan geliyor; MAVLink 2 kararı değişmedi; iyileştirme denemeleri kartta ölçülüyor. Rapor ve düzenek dal `olcum/mp5-tel-maliyeti`'nde, henüz commit'lenmedi.
- **W1 doğrulama turu; iskelet 0.11; W2-protokol (2026-11-20; günceldir):** sol doğrulama incelemesi tamamlandı (dört parça; çıktılar bütünleştirme dalında `docs/sozlesmeler/inceleme/W1-dogrulama-sol-{1..4}.md`, henüz commit'lenmedi); dördünün hükmü "dondurulamaz": çapraz incelemenin bulgularının çoğu kapanmış, geriye kritik ve önemli bulgular kalmış. Mimar hepsini karara bağladı (`docs/gorevler/W1-uyum.md` "Doğrulama turu", D-1…D-16; mimar kararlarıdır) ve yazarlar düzeltme turunda: A1 (129 kural), A4 (130), A3 (125) ve A6 (61) tamamlandı ve dallarına gönderildi; A2/A7, A5, A8 sürüyor. İskelet 0.11: yolda bekleyen genişleticinin üçüncü bekleme noktası; taşınan emniyet örneğinin gevşetememesi; otomatik derlemenin kota düzeni. W2-protokol (`ida_link` tel katmanı) yazıldı: taslak PR #9 (taban `is/w2-depo`); Windows CI yeşil (MSVC, 36 birim testi, ASan, kartsız firmware, statik analiz); 38 altın çerçeve ve 136 ayna eşitliği geçiyor; bağımsız kabul testleri yazılıyor. Tel katmanı uygulaması A1'de 7 belirsizlik bildirdi; A1'de kapatıldı. W2-depo: sol kod incelemesi koşuyor.
Sekiz sözleşmenin doğrulama düzeltmeleri bitti ve `sozlesme/w1-butunlesme` dalında yeniden birleşti (A1 129, A2 92, A3 125, A4 130, A5 111 etkin, A6 61, A7 41, A8 82 kural; tel biçimi, `dialect_hash` ve altın çerçeveler değişmedi). Sol fark incelemesi sol'un sınırı açılınca (15:16) başlayacak.
W2-depo (PR #8) ve W2-protokol (PR #9) Windows CI'da yeşil; protokolde 36 birim ve 59 bağımsız kabul testi; ikisinin sol kod incelemesi ve macOS koşusu bekliyor.
W2-cekirdek ikinci aşama yazıldı (taslak PR #10): çekirdek iskeleti, 70 birim testi; bağımsız kabul testleri ve sol incelemesi bekliyor.
W0-MP5a ölçüldü (`docs/gorevler/W0-MP5a.md`): ilk uygulama iki eşiği de aştı (1 050 B ayrıştırma 0,556 ms, eşik 0,20 ms; periyot toplamı 0,781 ms, eşik 0,50 ms; kartta, `fw-release`, 160 MHz; ölçüm). Mimar değerlendirmesi (ölçüm değil): maliyet biçimden değil uygulamadan geliyor; MAVLink 2 kararı değişmedi; iyileştirme denemeleri kartta ölçülüyor.
- **W2-depo CI yeşil; çekirdek tasarım notu; A4 129 kural (2026-11-20; günceldir):** Windows CI üçüncü denemede yeşil (PR #8; MSVC ile host derlemesi, birim testleri, sözleşme başlıkları denetimi, ASan, kartsız firmware bağlaması, statik analiz). Ölçülen: Windows işi soğuk önbellekle 7 dk 3 s, sıcak önbellekle 2 dk; belge işi 11 s. Kalan: sol kod incelemesi, macOS koşusu (`ci-mac`), `main`'e göre yeniden tabanlama. W2-cekirdek birinci aşama (tasarım notu) tamamlandı; A4'te 35 belirsizlik bulundu, mimar hepsini karara bağladı, A4 dondurma öncesi son turda güncellendi (129 kural; dal `sozlesme/a4-v1`). Bu turun değişiklikleri sol doğrulama incelemesinden sonradır; dondurmadan önce sol fark incelemesi yapılacak. İkinci aşama (kod) bütünleştirme dalı yeniden birleştirilince başlar.
- **W2 başladı (2026-11-20; günceldir):** W2'nin ilk işi (W2-depo: depo iskeleti, CMake ön ayarları, denetimler, otomatik derleme dosyaları) dondurma beklenmeden bütünleştirme dalı üzerinde başlatıldı ve yazıldı (yazan Opus); dal `is/w2-depo`, taslak PR #8; sol incelemesi sırada (sol'un kullanım sınırı 10:13'te açılıyor; önce W1 doğrulama incelemesi koşacak). Yerelde (macOS) host ve firmware derlemeleri ve testler geçti; sözleşme başlıkları Apple Clang 17 (C17, C++20) ve arm-none-eabi-gcc 14.3.1 (C17) ile sorunsuz derlendi; MSVC ve Windows sonucu CI'dan beklenecek. CI olanakları ölçüldü (09 §8.2 md.4 kapanıyor): hesap GitHub Free planında; aylık dahil kullanım 12 USD; dal koruma ve ruleset bu planda özel depoda yok (mimar denetimi tek kapı); kota dar olduğu için Windows işi her PR gönderiminde, macOS işi yalnız `ci-mac` etiketiyle, belge denetimi Linux koşucusunda koşar (mimar kararı; `docs/gorevler/W2-depo.md`). Bir PR koşusunun süresi henüz ölçülmedi. W2-protokol (`ida_link` tel katmanı) ve W2-cekirdek (çekirdek iskeleti; iki aşamalı) görev belgeleri yazıldı.
- **W1 uyum turu tamamlandı (2026-11-20; günceldir):** altı yazar raporladı ve altı dal gönderildi (A1 `sozlesme/a1-v1`, A4 `sozlesme/a4-v1`, A5 `sozlesme/a5-v1`, A2/A7 `sozlesme/a2a7-v1`, A8 `sozlesme/a8-v1`, A3/A6 `sozlesme/a3a6-v1`). Tur içinde eklenen kararlar `docs/gorevler/W1-uyum.md`'de: U19 (çalıştırma nesli değişimi A1'de yeni oturumdur), "Tur içinde kabul edilen yorumlar", operatörün onaylı atış isteği `FC_FIRE` panel komut kataloğunda (`FIRE` olayını yalnız çekirdek üretir). Kısmen kalan tek madde A1 U5b: bir çağrıya düşen emniyet örneği sayısının sınırı koşulludur, aşılırsa oturum kapanır; koşullar W3/W4'te ölçülecek. Dallar bütünleştirme dalına yeniden birleştirildi ve sol doğrulama incelemesi başlatılıyor.
- **W1 çapraz uyum (2026-11-20 sabahı; günceldir, aşağıdaki W1 durumunun önüne geçer):** altı sözleşme dalı `sozlesme/w1-butunlesme` dalında birleştirildi. sol'un dört parçalı çapraz uyum incelemesi tamamlandı; dördünün hükmü "dondurulamaz" (toplam 72 bulgu; çoğu iki ya da üç parçada yinelenir). Mimar her bulguyu bir sözleşmeye atadı ve ortak değerleri `docs/gorevler/W1-uyum.md` belgesinde sabitledi (U1–U18). İskelet 0.10 oldu (mimar kararları, kullanıcı kararı değildir): emniyet revizyonunun kapalı artış listesi ve doyma kuralı; fizik sınırındaki beş adımlı işlem sırası; yolda bekleyen genişletici komutun iptali; sıra numaralarının ortak karşılaştırma kuralı; canlı SIL eşitlik güvencesinin kamera kapalı koşullarla sınırlanması; atış kontrol kapsam ifadelerinin daraltılması. Uyum turu altı yazar agent'ta aynı anda başlatıldı (A1, A4, A5, A2/A7, A8, A3/A6; tek tur, her biri kendi dalında).
- **W1 durumu (2026-11-20):** sekiz arayüzün hepsinin sözleşme adayı yazıldı ve sol tarafından incelendi. A4 (117 kural), A8 (69), A5 (106), A2 (88) ve A7 (39), A3 (124) ve A6 (60) kendi iki düzeltme turunu tamamladı; A1 (115 kural; `protocol/ida.xml`, 37 özel mesaj) ikinci ve son düzeltme turunda. Her ikinci incelemede sol "dondurulamaz" dedi; bulgular mimar kararlarıyla giderildi ve son turlar sol tarafından yeniden incelenmedi: kalanlar çapraz uyum incelemesinde sınanacak (`docs/gorevler/W1-capraz.md`). Adaylar `sozlesme/a1-v1`, `a2a7-v1`, `a3a6-v1`, `a4-v1`, `a5-v1`, `a8-v1` dallarında; hiçbiri main'e birleşmedi, PR yok. Bu turda verilen mimar kararları (kullanıcı kararı değildir): emniyette kısıtlama hâkimdir ve tek emniyet revizyonu; girdi tekrarının sınırı A4 portudur ve bit eşitliği güvencesi yalnız SIL'dedir; kayıtlı akış karta yeniden verilmez; A7 örtü denetimi katı cisme göre iki tek yönlü ölçüttür; A1 ve A4'ten hedef seçimi işlemi ile opak atış kontrol alanları çıkarılır (A1'de çıkarıldı, A4 çapraz uyum turunda).
- **İskelet 0.9 (2026-11-20; mimar kararları):** bant ölçütü ikiye ayrıldı (olağan profilde ≤ %60, bağlayıcı üst sınırda ≤ %90; karttan PC'ye en kötü durum hesabı ≈ %84); girdi tekrarının sınırı ve güvencesi; "kısıtlama hâkimdir" ilkesi ve emniyet revizyonu; sürekli çıktıların üretim sırası, eşzamansız güvenli çıktı, olayların zamanlaması, oturum sonunun önceliği, çağrı çakışması; `L_act` payının sözleşme hesabı (uçtan uca en kötü ≈ 9,73 ms, pay ≈ 0,27 ms; tahmin, ölçülmedi; ölçüm 10 ms'yi aşarsa `L_act` üç periyoda çıkar); kamera açıkken 3B görünüm kapalı seçilemez; kamera kademe değerleri A2'de. 0.9 henüz sol tarafından incelenmedi; çapraz uyum incelemesinde başvuru sürümü olarak sınanacak.
- **İskelet 0.8 (2026-11-19):** W0 ölçüm sonuçları ve K17 işlendi. Mimar kararları (kullanıcı kararı değildir): A1 taşıması yalnız ST-LINK sanal seri portu, 3 000 000 baud; kontrol hızının tasarım tabanı 200 Hz (400 Hz hedef olmaktan çıktı); aktüatör yanıtının uygulama gecikmesi sabit iki kontrol periyodu (10 ms) ve geç kalan yanıtın kabul/ret kuralı; kamera çizim yolu seçildi (ana iş parçacığından açık çizim, eşzamanlı geri okuma; Mac'te ön kanıt); kartlı çalışmada sonarın varsayılanı sade kademe; harici yer istasyonu yazılımı ilk sürümde yok; depo düzeni güncellendi; bölümlerin iskelete yönelik 26 önerisi karara bağlandı (§10). sol iskeleti iki kez inceledi; ilk incelemenin 7 bulgusu ve ikincinin 1 yeni kritik bulgusu giderildi (`docs/mimari/inceleme/00-iskelet-08-yanit.md`); geç yanıt kuralı A1 incelemesinde yeniden sınanacak.
- **W1 başladı (2026-11-19):** beş sözleşme işinin hepsi 1.0-aday taslağını verdi (A4 112 kural; A8 69; A5 91; A2 72 ve A7 38; A3 116 ve A6 50). sol ilk incelemelerinde A8 (9 kritik), A4 (12 kritik), A5 (12 kritik), A2/A7 (8 kritik) "dondurulamaz" bulundu; bulgular mimar kararlarıyla düzeltme turlarında gideriliyor: A8 iki turu tamamladı; A4 ilk turu tamamladı; A5 ve A2/A7 ilk turda; A3/A6 ilk incelemesi sol'un kullanım sınırı nedeniyle yarıda kaldı (sınır 2026-11-20 00:08'de açılıyor). A1 sözleşmesi A4 ve A8 adayları üzerine başlatıldı (görev belgesi `docs/gorevler/W1-A1.md`). Adaylar `sozlesme/…-v1` dallarında; hiçbiri main'e birleşmedi. Bütün sözleşmeler ikinci turlarını bitirince sol'a çapraz uyum incelemesi yaptırılacak, sonra mimar dondurur.
- **W0'dan kalanlar:** MP8a düzeltme turu (Windows paketi; kullanıcı Windows'ta çalıştıracak), MP3 temiz koşul yinelemesi (kullanıcının uygun zamanını bekliyor), MP5 (mesaj biçimi maliyeti; A1 sözleşmesinden sonra ya da protokol kütüphanesinin ilk sürümüyle ölçülecek).
- **MP3 tamamlandı ve main'e birleşti (2026-11-19; PR #6; ölçüm prototipi, ürün kodu değil):** rapor `docs/olcumler/MP3-vcp.md`, kod `tools/olcum/mp3/`. Akış (256 B, %80 yük, 600 s): 921 600, 2 000 000 ve 3 000 000 baud geçti (3 Mbaud yinelemede; sabah koşusu kopma nedeniyle geçersiz); 6 000 000 baud geçmedi (veri temiz, PC takvim gecikmesi 18,1 ms > 10 ms). Echo (3 Mbaud): dört profilde kayıp ve hata 0; gidiş-dönüş p50 / p99,9: 64 B'de ≈ 0,8 / 1,1 ms, 256 B'de ≈ 2,2 / 2,5 ms; en uzun 12,5–14,5 ms; üç profil yalnız PC takvim ölçütünden (10 ms) geçmedi, 256 B 400/s geçti. Ölçüm sırasında Mac'te ilgisiz yük vardı; nedensellik kurulamadı; temiz host koşulunda bir yineleme planlandı (kullanıcının uygun zamanını bekliyor). sol son veri incelemesi hükümleri ham kayıtlardan doğruladı (`docs/olcumler/inceleme/MP3-sol-veri.md`). Bilinen araç sınırlaması A1 ve kapsam kararı raporda.
- **Açık bulgular kaydı güncellendi (2026-11-19):** W0 ölçüm bulguları AB-234…AB-247 eklendi; K17 ile üç bulgu kapsam dışı işaretlendi; toplam 247, açık 238 (kritik 38).
- **W0-MP8a (Windows ölçüm paketi) hazırlandı, inceleniyor:** dal `olcum/mp8-windows` (commit'lenmedi); paket Windows'ta henüz çalıştırılmadı; sol incelemesi sürüyor; görev belgesi `docs/gorevler/W0-MP8a.md`.
- **Eşzamanlı agent sınırı kalktı (2026-11-19):** kullanıcı üyeliğini yükseltti; "aynı anda en fazla iki agent" çalışma kuralı (Ö12) artık uygulanmıyor. Kısıtlar: kart tek (kart işleri sırayla), aynı dosyaya dokunan işler aynı anda koşmaz, zamanlamaya duyarlı ölçüm sürerken Mac'e ağır yük bindirilmez.
- **Mimarlık (K19, 2026-11-19):** mimar oturumunu yeniden Fable yürütüyor; K18'in görev dağılımı sürüyor.
- **MP4a tamamlandı (2026-11-18, daraltılmış kapsamla; ölçüm prototipi, ürün kodu değil):** dal `olcum/mp4-dongu-yuku`. D1/D2 geçmedi, D3/D4 geçti. O2 ağır yük 400 Hz: maksimum 3,32238125 ms, aşım 241000/241000; O2 200 Hz: 121000 adımda aşım 0. Ölçülen yer tutucu çekirdek adımıdır ve taşımayı içermez; gerçek çekirdek ve taşıma hızı kesinleşmedi, mevcut kararlar korunur. Mimar, taşmalı histogramların kapsam dışı bırakılıp reddedilmesine karar verdi (Y5). Çözümleyici 103 denetimi geçti; sol kapanış incelemesi kritik/önemli bulgu görmedi (`docs/olcumler/inceleme/MP4-sol-kapanis.md`; küçük bulgu Y8 giderildi). Rapor: `docs/olcumler/MP4-dongu-yuku.md`. Main'e PR #5 ile squash birleşti (b2b9346).
- **Tek kablo kararı (K17, 2026-11-16):** sistem yalnız kartın ST-LINK portuyla çalışır; yükleme, hata ayıklama ve kart–simülatör bağlantısı (A1) sanal seri porttan (USART1) geçer; kullanıcı USB girişi (USB HS) kullanılmaz; MP2 ve kart tarafı USB yığını plandan çıktı; kontrol hızı ve bariyer bütçesi MP3'e göre belirlenir.
- **Araştırma aşaması kapandı.** 13 rapor yazıldı, 13'ü de bağımsız doğrulandı; düzeltmeler 00 numaralı belgede (01–13).
- **Mimari çalışması tamamlandı; kullanıcı 2026-11-14'te bilinen açık bulgularla onayladı (K15).** Belgeler taslak sürüm numaralarını korur; açık bulgular sözleşme ve uygulama işlerinde kapatılır: iskelet 0.8 (`docs/mimari/00-iskelet.md`), dokuz bölüm taslağı, incelemeler ve açık bulgular kaydı (`docs/mimari/acik-bulgular.md`). İskelet 0.7'de mimar itkili torpido kararını geri aldı: torpido itkisiz, kısa menzilli basit simülasyon nesnesidir; 03 kapsamı emniyet zinciri ve arayüzlerle sınırlıdır.
- **Açık bulgular kaydı:** 233 bulgu (AB-001…AB-233); 224'ü açık (33 kritik, 94 önemli, 8 küçük, 89 önem belirtilmemiş), 9'u kapsam dışı. En çok bulgu A1 sözleşmesinde (47), A4 sözleşmesinde (26) ve atış kontrol uygulamasında (23). Belge ayrıca 29 satırlık bölümler arası uyumsuzluk tablosu ve 26 maddelik iskelet önerisi listesi içerir.
- **sol ikinci incelemeleri:** 01 (`inceleme/01-sol-2.md`): koşulsuz kabul yok; açık bulgularla sözleşme aşamasına taşınabilir; 4 kritik, 12 önemli, 1 küçük yeni bulgu. 05 (`inceleme/05-sol-2.md`): koşulsuz kabul yok; 1 kritik, 15 önemli yeni bulgu; yeni belge düzeltme turu önerilmiyor.
- **Süreç kararı (mimar, 2026-11-14):** inceleme-düzeltme döngüsü kesildi; kapanmamış bulgular sözleşme ya da uygulama aşamasında kapatılır.
- **Dalga sırası (kullanıcı onayladı, K15):** ölçüm prototipleri W0, arayüz sözleşmeleri W1; A1 ölçüm sonuçlarıyla tek aşamada donar. `docs/mimari/09-test-ve-is-akisi.md` sürüm 0.2.
- **`tools/sol-incele.sh`:** Claude'un yazdığı belge ya da kodun sol incelemesini başlatan betik (ilk ve ikinci inceleme kipi). **sol yazma denemesi geçti (W0-SOL1, PR #3, 2026-11-15):** sol görev belgesine ve dosya sahipliğine uydu, commit atmadı, çalıştıramadığı ELF testlerini "atlandı" diye dürüstçe raporladı; `ida-inceleyici` 8 bulgu verdi (1 kritik: ELF arşivi denetlenemiyordu), sol bir düzeltme turunda hepsini giderdi; mimar bağımsız koşuyla doğruladı (58 denetim geçti). Sonuç: 09 §2.9'daki sol yazım işleri sol'da kalır; sol'a yazım görevi verilirken gereken araç zinciri ortamına (`PATH`) eklenir. Ürün: `tools/denetim/bellek-ayirma.sh`.
- **Agent yapısı kuruldu:** tanımlar `.claude/agents/` altında.
- **W0 başladı:** ilk iki iş W0-MP1a ve W0-MP6a (`docs/gorevler/`).
- **Kart Mac'e bağlı** (ST-LINK portu, USB hub üzerinden); tek kablo yeterli (K17).
- **Ürün kodu yok;** W0 ölçüm prototipleri `olcum/` dallarında, `tools/olcum/` altında yazılıyor.
- **W0 durumu (2026-11-15):**
  - **W0-MP1a tamamlandı ve main'e birleşti** (PR #1). Rapor `docs/olcumler/MP1-arac-zinciri.md`, kod `tools/olcum/mp1/` (atılabilir prototip). K0, yedek, K1, K2 düzeltilmiş ölçütlere göre geçti; sol iki tur inceledi, kritik bulgu kalmadı. Mimari için bulgular (açık bulgular kaydına işlenecek): STM32U59x/5Ax'te 160 MHz'e geçişte geçici AHB ÷2 ve en az 5 µs bekleme gerekli; Picolibc CubeIDE 14.3.rel1 macOS paketinde yok; ST kart örneklerinin çoğu PLL'i MSIS'ten besliyor; kart revizyonu `REV_ID` 0x3000 ES0553 Rev 6 listesinde yok.
  - **W0-MP6a tamamlandı ve main'e birleşti** (PR #2). Rapor `docs/olcumler/MP6-godot-kamera.md`, kod `tools/olcum/mp6/`. G1–G4 geçti: `force_draw(false, dt)` + `SubViewport` `UPDATE_ONCE` + eşzamanlı geri okuma yolu görünür, örtülü, gizli ve simge durumlarında 300 s boyunca 9000/9000 kare üretti, görünür referansla bit düzeyinde aynı; 640 × 480 geri okuma p99 5,69 ms; olağan çizim döngüsü görünmeyen pencerede kare üretmedi (elendi). Doğrulanmadı: tam ekran durumu, Windows, 5 dakikadan uzun koşular, GDExtension. sol iki tur inceledi, kritik bulgu kalmadı. Mimari için bulgular (açık bulgular kaydına işlenecek): kamera `force_draw` ile ana iş parçacığından çizdirilmeli; `get_image()` RGB8 döndürüyor; asenkron geri okuma gizli pencerede ≈ 70 ms.
  - **W0-MP7a tamamlandı ve main'e birleşti** (PR #4). Sonuç: sade kademe (yalnız doğrudan yol, kutu başına benek) 5 000 ışınla dondurulmuş yakınsama ölçütünü sağlıyor ve tek iş parçacığında p99 0,93 ms (7 m) / 0,79 ms (50 m); tam kademe (eşevreli hücreler, çok yollu yayılım) ölçütü taranan en büyük ışın sayısına (≈ 2,56 milyon) kadar sağlamadı; `N` = 1 280 000'de tek iş parçacığı p99 504,4 ms (bütçe 9,33–25,17 ms); bütçeye giren ışın sayısı tek iş parçacığında 20 000–40 000, 11 iş parçacığında 80 000; azaltılmış ışın sayısının bilinen hatası raporda (7 m: enerji farkı 20 000'de 0,146, 160 000'de 0,057). Tekrar üretilebilirlik geçti. Mimari için: bölüm 05 §2.4.3 saçıcı hücresi kuralı hatalıydı; yeni kural ve yakınsama tanımı `docs/gorevler/W0-MP7a.md` içinde; iskelet §8 sonar kararı (kartlı çalışmada sade kademe varsayılan, tam ayrıntı yalnız kartsız koşuda) W0 sonunda mimarca işlenecek. sol iki tur inceledi; kapanış mimar kararıyla. Rapor `docs/olcumler/MP7-sonar-suresi.md`.
  - **W0-MP4a (kartta 400 Hz döngü yükü, yer tutucu çekirdek adımı):** başladı (2026-11-16), sürüyor; görev belgesi `docs/gorevler/W0-MP4a.md`.
  - Yerel çalışma kopyaları (`.claude/worktrees/`) ve ham veri önbelleği depoya girmez.
- **Tek odak:** simülasyon ortamını kurup STM32U5A9J-DK kartında çalıştırmak (00 belgesi, K8).

### Bölüm durumu

| Bölüm | Yazım | İlk inceleme | Düzeltme | İkinci inceleme |
|---|---|---|---|---|
| 01 kart yazılımı | var | var (kabul yok) | yapıldı (0.2) | var: koşulsuz kabul yok; 4 kritik, 12 önemli, 1 küçük yeni bulgu |
| 02 ortak çekirdek | var | var | yapıldı | var: 4 kritik açık, kabul yok |
| 03 atış kontrol | var | var: 12 kritik açık, kabul yok | yapılmadı | - |
| 04 simülatör çekirdeği | var | var | yapıldı | var: kritik yok, 5 önemli açık |
| 05 sensörler ve sonar | var | var (kabul yok) | yapıldı | var: koşulsuz kabul yok; 1 kritik, 15 önemli yeni bulgu |
| 06 protokol ve HIL | var | var (kabul yok) | yapıldı (0.2) | var: 5 kritik açık, kabul yok |
| 07 görselleştirme | var | var: 8 kritik açık, kabul yok | yapılmadı | - |
| 08 laboratuvar uygulaması | var | - | - | - |
| 09 test ve iş akışı | var | - | - | - |

## Sıradaki adım

00. (2026-11-21 gece, en güncel) sol incelemeleri → tek düzeltme turu → PR #24, #23, #22, #25, #26 sırasıyla birleşir; Z2 koşusu ve ölçüm raporu (`docs/olcumler/W4-z2.md`); `ida_sil`'de `REBOOT`'ta platform saatinin sıfırlanması (köprü bulgusu); sonra sonar, atış kontrol emniyet zinciri (emniyet gevşetmesi dahil), A2/A3 sunucuları, kayıt (A6).
0. (2026-11-21 akşam, en güncel) W3-zincir'in sol incelemesi ve `main`'e birleşmesi; kart dilim 2 (oturum modülünü karta bağlama; Z2: aynı senaryo kartta); W4: sim tarafındaki A1 köprüsü (W4-sensor arayüzünü bildirince), kartsız koşucu, sonra sonar ve atış kontrol emniyet zinciri.
0. (2026-11-21, en güncel) W3 paralel işler: kart yazılımı, fizik çekirdeği, çekirdek kontrol zinciri, web panelleri, tel katmanı hızlandırması; hedef ilk çalışan zincir (SIL'de derinlik tutma, sonra kartta).
0. (2026-11-20 akşamüstü, en güncel; kullanıcı kararını bekler, başlatılmadı) Dondurma (sürüm 1.0, etiketler `sozlesme-A<n>-v1`, `main`'e birleştirme, açık bulgular kaydı) ve öncesinde bu turun farkının sol'a incelettirilip incelettirilmeyeceği; üç W2 PR'ının macOS koşuları (`ci-mac` etiketi; hiç koşmadı) ve birleştirilmeleri; MP5 iyileştirmelerinin ürün tel katmanına taşınması; W3.
0. (2026-11-20, en güncel) Sol fark incelemesi (15:18; `W1-fark-sol-{1,2}.md`) ve W2 kod incelemeleri → bulguların düzeltilmesi → dur; devamı kullanıcı talimatıyla.
0. (2026-11-20, en güncel) Kalan yazar turları (A2/A7, A5, A8) → altı dalın bütünleştirme dalına yeniden birleştirilmesi → sol fark incelemesi (`40b8d63`'ten son hâle) → dondurma; W2-depo ve W2-protokol incelemeleri ve birleştirme; W2-cekirdek ikinci aşama.
0. (2026-11-20, en güncel) Sol doğrulama incelemesi (10:15'ten sonra) → dondurma; W2-depo: Windows CI sonucu, sol incelemesi, düzeltme, `main`'e göre yeniden tabanlama; W2-protokol ve W2-cekirdek W2-depo kaydından sonra.
0. (2026-11-20, en güncel) Sol doğrulama incelemesi (dört parça; çıktılar bütünleştirme dalında `docs/sozlesmeler/inceleme/W1-dogrulama-sol-{1..4}.md`) → kritik bulgu yoksa dondurma (sürüm 1.0, etiketler, PR) → W2.
0. (2026-11-20, güncel) Uyum turu raporlarının denetimi → altı dalın bütünleştirme dalına yeniden birleştirilmesi → sol doğrulama incelemesi → dondurma (sürüm 1.0, etiketler, PR) → W2 (ilk iş `docs/gorevler/W2-depo.md`; taslak hazır).
0. (2026-11-20) Sıra: A1 son düzeltme turu → bütünleştirme dalı (`sozlesme/w1-butunlesme`; altı sözleşme dalı birleştirilir) → sol çapraz uyum incelemesi → uyum turu → doğrulama incelemesi → dondurma (sürüm 1.0, etiketler `sozlesme-A<n>-v1`, PR). Sonra W2.
1. W1 sözleşmeleri (sürüyor): yazım, sol incelemesi, en çok iki düzeltme turu, mimar denetimi, dondurma; A1 (A4'ten sonra).
2. W0'dan kalanlar: MP8a düzeltme turu, MP3 temiz koşul yinelemesi, MP5.
3. W2: depo iskeleti, otomatik derleme, protokol kütüphanesi, çekirdek iskeleti; sonra W3–W6.

### Eski

0. MP3 bitti. Sıra: iskelet 0.8 (mimar; W0 sonuçları ve K17), MP8a incelemesi ve birleştirmesi, MP3 temiz host yinelemesi, MP5 (mesaj biçimi maliyeti), sonra W1 sözleşmeler.
0a. (Eski, 2026-11-18) MP2 (USB HS) K17 ile plandan çıktı. MP4a bitti. Sıra: MP3 (sanal seri port hızı ve kaybı; artık A1'in tek taşıması); MP5 (mesaj biçimi maliyeti; seri port üzerinden); MP8 Windows betiği; W0 sonu: ölçüm sonuçlarının ve K17'nin iskelete (§3, §8) ve açık bulgulara işlenmesi; sonra W1 sözleşmeler.
1. W0 ölçüm prototipleri (`docs/mimari/09-test-ve-is-akisi.md` 2.1 ve 6.1). MP1a, MP6a, MP7a, MP4a bitti. Sırada: MP3, sonra MP5, MP8 betiği, W0 sonu, W1.
2. W1 arayüz sözleşmeleri (A1–A8); açık bulgular bu aşamada kapatılır; iskelet önerileri W1 başında karara bağlanır.
3. Uygulama dalgaları (W2–W6).

## Kodlamanın ilk işleri: ölçümler

Web araştırmasıyla kapanmayan belirsizlikler. Sonuçları çıkmadan ilgili tasarım kararı kesinleşmez (00 belgesi, Bölüm 7).

| Ölçüm | 00 belgesindeki karşılığı |
|---|---|
| Kartta yüksek hızlı USB: çalışıyor mu, gecikme ve kayıp (K17 ile plandan çıktı; yerine sanal seri port hızı ve kaybı: M3) | M1, M2 |
| Godot'da pencere görünmezken kamera görüntüsü üretimi (Mac ve Windows) | M13, M28 |
| Sonar hesap süresi | M11 |
| Kartta 400 Hz döngünün yükü ve en kötü durum süresi | M5 |
| Windows hedef bilgisayarının kaldırdığı kalite kademesi | M12, M20, M21, M30 |

Kart Mac'e tek kabloyla, ST-LINK portundan bağlanır (K17); araç kurulumu için kullanıcı izni gerekir.

## Açık konular

- Windows hedef bilgisayarı: 8 GB bellek, dahili ekran kartı (K16); işlemci bilinmiyor; kalite kademesi tabanı MP8 ile sabitlenecek (Ö11).
- Bölüm 03'ün dar kapsamlı (yalnız emniyet zinciri ve arayüzler) düzeltme turu için kullanıcı yanıtı bekleniyor.
- 00 belgesindeki açık sorular (Bölüm 9). Yarışmaya ve ardıl karta bağlı olanlar kapsam dışıdır (K7, K8).

## Kayıt
### 2026-11-21 (gece) · Z1 A1 üzerinden; kart dilim 2; beş PR incelemede
- W4-kopru: `ida_sim` ile Z1 A1 mesajları üzerinden (süreç içi ve `ida_sil` ile iki süreç) geçti; PR #26. Kart dilim 2 kartta çalıştı; PR #25. PR #22–#24 Windows CI'da yeşil. sol sınırı yeniden doldu; incelemeler 20:31 sonrasına kuruldu. Z2, W4-senaryo ve köprü kabul testleri başladı.

### 2026-11-21 (akşam) · W3 `main`'de, Z1 çalıştı
- PR #13–#19 (tel hızlandırması, parametre testi, kontrol, kestirim, fizik çekirdeği, `ida_link` üst katmanı, web panelleri) `main`'e birleşti. Z1 kartsız zinciri 1,5 m dalışı tamamladı (ölçüm); W3-zincir, W3-kart, W3-viz sol incelemesinde düzeltiliyor, W4-sensor ve W4-sil başladı.

### 2026-11-21 · sözleşmeler donduruldu (1.0), W2 PR'ları birleşti, K20
- A1–A8 sürüm 1.0 (PR #11, `8df5b4a`; etiketler `sozlesme-A<n>-v1`); PR #8, #9, #10 birleşti; Windows ve macOS koşuları başlatıldı (44896495188, 44896497943; sonuç yok); K20; kart seri ucu `/dev/cu.usbmodem21403`.
### 2026-11-20 (akşamüstü) · fark incelemesi turu ve W2 kod incelemeleri kapandı; dondurma kararı bekleniyor
W1 fark incelemesi (sol, iki parça) "dondurulamaz" dedi; mimar kararlarıyla (D-17…D-27, iskelet 0.12) düzeltildi; bütünleştirme dalı `sozlesme/w1-butunlesme` @ `2a7d178`; düzeltmeler sol'a yeniden incelettirilmedi.
W2'nin üç sol kod incelemesi ("birleştirilemez") düzeltildi; PR #8 @ `edc2642`, PR #9 @ `05ee0fc`, PR #10 @ `f62017c` Windows CI'da yeşil; macOS koşuları (`ci-mac`) çalışmadı.
Kullanıcı talimatı gereği çalışma burada durdu; yeni iş başlatılmadı. Ayrıntılar ve açık konular yukarıdaki "Şu an" ilk maddesinde.

### 2026-11-20 · iskelet 0.11; W1 doğrulama turu kararları; W2-protokol
Sol doğrulama incelemesi dört parçada "dondurulamaz" hükmü verdi; mimar bulguları D-1…D-16 olarak karara bağladı (`docs/gorevler/W1-uyum.md`, "Doğrulama turu"). A1 (129), A4 (130), A3 (125) ve A6 (61) düzeltildi; A2/A7, A5, A8 sürüyor.
İskelet 0.11: yolda bekleyen genişleticinin üçüncü bekleme noktası, taşınan emniyet örneğinin gevşetememesi, otomatik derlemenin kota düzeni.
W2-protokol taslak PR #9 (taban `is/w2-depo`): Windows CI yeşil, 36 birim testi, 38 altın çerçeve ve 136 ayna eşitliği geçiyor; bağımsız kabul testleri yazılıyor; A1'de 7 belirsizlik kapatıldı.
W2-depo: sol kod incelemesi koşuyor.
W2-cekirdek (PR #10) Windows CI yeşil (`8e6468b`: 79 birim, 137 kabul testi, ASan, statik analiz); A4'te 14 madde kapatıldı (`sozlesme/w1-butunlesme` @ `6b63c6e`), uygulayıcının 6 ve test yazarının 20 maddesi açık. W2-depo (PR #8) ve W2-protokol (PR #9) yeşil.
W0-MP5a tamamlandı (dal `olcum/mp5-tel-maliyeti`, PR yok): ürün kitaplığının ilk hâli eşikleri aşıyor; iyileştirilmiş kopya E1 0,153 ms, E2 0,264 ms (ölçüm; bayt bayt beslemede sağlamıyor). Mimar değerlendirmesi (ölçüm değil): MAVLink 2 kararı sürer; ürüne taşıma ayrı iş, başlatılmadı.
Kullanıcı talimatı (2026-11-20): sol 15:16'dan sonra incelemelerini yapınca bulgular düzeltilir ve orada durulur; yeni adım başlatılmaz.

### 2026-11-20 · W2 başladı: W2-depo yazıldı (PR #8); protokol ve çekirdek görev belgeleri
W2-depo: Windows CI üçüncü denemede yeşil (PR #8); sözleşme başlıkları MSVC'de sorunsuz derlendi. Windows işi soğuk önbellekle 7 dk 3 s, sıcak önbellekle 2 dk; belge işi 11 s. Kalan: sol kod incelemesi, macOS koşusu (`ci-mac`), yeniden tabanlama.
W2-cekirdek birinci aşama tamamlandı: A4'te 35 belirsizlik bulundu, mimar hepsini karara bağladı; A4 son turda güncellendi (dal `sozlesme/a4-v1`), dondurmadan önce sol fark incelemesi yapılacak.
A4 kural sayısı güncel değeri 129'dur (bu belgedeki eski sayılar önceki turlara aittir).
W2-depo işi `is/w2-depo` dalında Opus tarafından yazıldı; taslak PR #8 (taban `sozlesme/w1-butunlesme`), sol incelemesi bekleniyor. Yerelde macOS host/firmware derlemeleri ve testler geçti; MSVC ve Windows sonucu CI'dan beklenecek. CI olanakları ölçüldü (Free plan, 12 USD dahil kullanım, özel depoda ruleset yok); Windows işi her PR'da, macOS işi `ci-mac` etiketiyle, belge denetimi Linux'ta koşar.
`docs/gorevler/W2-protokol.md` ve `docs/gorevler/W2-cekirdek.md` yazıldı; `W2-depo.md` başlatma notu, kota düzeni ve inceleme kayıtları kararlarıyla güncellendi.

### 2026-11-20 · W1 uyum turu tamamlandı (altı dal); U19 ve tur içi yorumlar; dallar yeniden birleştirildi
### 2026-11-20 · W1 çapraz uyum incelemesi tamamlandı; uyum turu başlatıldı; iskelet 0.10; W2-depo görev belgesi taslağı
### 2026-11-20 · W1: düzeltme turları; iskelet 0.9; çapraz uyum görev belgesi
- sol'un sınırı açıldı; beş bekleyen inceleme koştu (A4, A5, A2/A7 ikinci; A3/A6 ve A1 ilk), ardından A3/A6 ve A1 ikinci incelemeleri.
- A4, A5, A2/A7, A3/A6 iki düzeltme turunu tamamladı ve dallarına gönderildi (A4 `36e0eba`, A5 `69d92ab`, A2/A7 `05784a2`, A3/A6 `931117a`); A1 birinci turu `1cf683c`, ikinci turu sürüyor.
- İskelet 0.9 yazıldı (mimar); çapraz uyum incelemesinin görev belgesi `docs/gorevler/W1-capraz.md`.
- sol'un A1 ve A4'te bulduğu kapsam dışı içerik (hedef seçimi işlemi, hedefe hizalanma görev türü, opak alanlar) A1'den çıkarıldı; A4'ten çapraz uyum turunda çıkarılacak.
### 2026-11-19 · W1: beş sözleşme adayı yazıldı, düzeltme turları sürüyor
- Beş sözleşme işi 1.0-aday taslağını verdi; adaylar `sozlesme/…-v1` dallarında, main'e birleşmedi.
- sol ilk incelemeleri A8, A4, A5 ve A2/A7 için "dondurulamaz" dedi; düzeltme turları sürüyor (A8 iki tur, A4 bir tur tamamlandı). A3/A6 incelemesi sol kullanım sınırı nedeniyle yarım kaldı.
- A1 görev belgesi yazıldı (`docs/gorevler/W1-A1.md`); çapraz uyum incelemesi tüm ikinci turlardan sonra.
### 2026-11-19 · iskelet 0.8; W1 sözleşmeleri başladı
- İskelet 0.8: W0 sonuçları ve K17 işlendi; 26 bölüm önerisi karara bağlandı (mimar kararları).
- sol iskeleti iki kez inceledi; bulgular giderildi (`docs/mimari/inceleme/00-iskelet-08-*.md`).
- W1 görev belgeleri yazıldı (`docs/gorevler/W1-*.md`); beş iş paralel başlıyor (yazan Opus, inceleyen sol).
- MP8a görev belgesine düzeltme turu kararları eklendi (Windows paketi sol incelemesinde kabul edilmedi).

### 2026-11-19 · W0: MP3 birleşti; açık bulgular güncellendi; MP8a hazırlandı
- MP3 (sanal seri port) PR #6 ile main'e birleşti; 3 Mbaud akış ve echo geçti, 6 Mbaud geçmedi.
- Açık bulgular kaydına AB-234…AB-247 eklendi (toplam 247, açık 238).
- W0-MP8a görev belgesi ve paket hazırlandı, sol incelemesi sürüyor.
- Agent sınırı kalktı (AGENTS.md md.7, Ö12 notu).

### 2026-11-19 · mimarlık Fable'a döndü (K19); MP3 kampanyası sürüyor
- K19: mimar oturumunu yeniden Fable (Claude) yürütür; K18'deki Codex devri sona erdi, görev dağılımı sürer.
- MP3 akış koşuları (600 s, çift yönlü, 256 B): 921600 baud GEÇTİ (159 920 çerçeve), 2 000 000 baud GEÇTİ (347 827 çerçeve); 3 ve 6 Mbaud ile echo koşuları kaldı.
- 3 000 000 baud koşusu kart bağlantısı koptuğu için geçersiz (ölçüt ihlali değil); yinelenecek.

### 2026-11-18 · W0: MP3 ara durumu
- MP3 (dal `olcum/mp3-vcp`) ilk teslimi yapıldı; sol ön incelemesi 8 kritik, 3 önemli bulgu verdi, Opus birinci düzeltme turu sürüyor. Kart yeniden bağlandı; kısa kart ölçümü ve uzun kampanya yapılmadı.
- Sıra: düzeltme turu, kısa kart ölçümü, sol incelemesi, uzun kampanya; sonra MP5, MP8, W0 sonu, W1. MP4a tamamlanmış kalır.

### 2026-11-18 · W0: MP4a ölçüm ve inceleme tamamlandı
- MP4a daraltılmış kapsamla bitti; D1/D2 geçmedi, D3/D4 geçti; O2 ağır 400 Hz maks 3,32238125 ms, aşım 241000/241000; O2 200 Hz 121000 adımda aşım 0. Gerçek çekirdek/taşıma hızı kesinleşmedi.
- Mimar kapsamı daralttı: taşmalı histogramlar reddedilir. 103 test geçti; sol kapanışı kritik/önemli bulgu görmedi.
- PR #5 main'e squash birleşti (b2b9346). Sıra: MP3, sonra MP5, MP8, W0 sonu, W1.

### 2026-11-18 · W0: MP4a devam kaydı
- MP4a (ölçüm prototipi, dal `olcum/mp4-dongu-yuku`) tamamlanmadı ve main'e birleşmedi; O2 A/B/C ölçümü bitti, D1/D2 O2 ağır yükte geçmedi, kontrol hızı kesinleşmedi. Ayrıntı "Şu an" bölümünde.
- sol ilk incelemesi 2 kritik, 3 önemli bulgu verdi; PC düzeltmesi, yeniden analiz ve eksik Og koşusu sürüyor.
- K18 mimar devri korunuyor.

### 2026-11-17 · Mimar oturumunun devri (K18)
- Kullanıcı mimar oturumunu Fable'dan Codex'e devretti; kod yazımı Opus'a, bağımsız inceleme sol'a verildi. Karar `AGENTS.md` ve karar günlüğü Bölüm 2.1'e eklendi.
- Bu oturumda uygulama ya da ölçüm başlatılmadı. MP4a'nın mevcut dal ve çalışma kopyalarındaki gerçek ilerlemesi devam etmeden önce doğrulanacak; önceki W0 sırası korunuyor.

### 2026-11-16 · W0: MP7a birleşti, tek kablo kararı (K17), MP4a başladı
- MP7a main'e birleşti (PR #4); sade kademe bütçeye sığıyor, tam kademe ölçütü sağlamadı; bölüm 05 §2.4.3 hücre kuralı düzeltildi (görev belgesinde).
- Kullanıcı ikinci USB-C kablosu olmadığını bildirdi: K17 tek kablo kararı; MP2 plandan çıktı, A1 yalnız sanal seri port.
- MP4a (kartta 400 Hz döngü yükü) başladı; görev belgesi `docs/gorevler/W0-MP4a.md`.
- İskelet §3, §8 ve bölüm 01/06 güncellemesi W0 sonunda mimarca yapılacak.

### 2026-11-15 · W0: sol yazma denemesi geçti
- W0-SOL1 (PR #3) birleşti: `tools/denetim/bellek-ayirma.sh` ve testleri; yazan sol, inceleyen `ida-inceleyici` (8 bulgu, bir düzeltme turu).
- sol dosya sahipliğine uydu, çalıştıramadığını dürüstçe bildirdi; mimar bağımsız koşuyla 58 denetimi doğruladı.
- Sıradaki: MP7a düzeltme ve birleştirme, MP2–MP5, MP8.

### 2026-11-15 · W0: MP6a birleşti, MP7a düzeltme turunda
- MP6a main'e birleşti (PR #2): G1–G4 geçti, sol iki tur, kritik bulgu kalmadı; mimari bulguları açık bulgular kaydına işlenecek.
- MP7a: sol incelemesi sonrası mimar saçıcı hücresi kuralını ve yakınsama büyüklüklerini görev belgesinde yeniden tanımladı; düzeltme turu sürüyor.
- Sıradaki: MP7a düzeltme, ikinci inceleme, birleştirme; sol yazma denemesi, MP2–MP5, MP8.

Yeni kayıt en üste eklenir; eski kayıtlar silinmez.

### 2026-11-15 · W0: MP1a birleşti, MP6a ve MP7a ölçüldü

- MP1a: düzeltme turlarıyla main'e birleşti (PR #1); kritik bulgu kalmadı.
- MP6a: tam kampanya koştu; `force_draw` yolu gizli pencerede 9000/9000 kare, geri okuma p99 ≈ 5,7 ms; sol 2 kritik, 4 önemli bulgu verdi, düzeltme turu sürüyor.
- MP7a: ölçüm bitti; S3 geçmedi (p99 36,6 ms), S2 saçıcı hücresi kuralı için mimar kararı gerekiyor; sol incelemesi sürüyor.
- Sıradaki: MP6a ve MP7a düzeltme ve birleştirme, sol yazma denemesi, MP2–MP5, MP8.

### 2026-11-14 · W0: MP1a incelendi, MP6a yarım, oturum durdu

- MP1a: sol incelemesi 7 bulgu verdi (5 kritik, 2 önemli); en önemlisi 160 MHz geçişinde eksik geçici AHB ÷2 adımı. Düzeltme turu bekliyor.
- MP6a: olağan çizim döngüsü görünmeyen pencerede kare üretmedi; `force_draw` kısa koşularda bit düzeyinde aynı kare üretti; G1–G3 tamamlanamadı, G4 geçti.
- MP7a başlamadan durduruldu; görev belgesi hazır.
- Sıra: MP1a düzeltme turu, MP6a kalan koşular, MP7a; ölçüm bulguları açık bulgular kaydına işlenecek.

### 2026-11-14 · W0: MP1a tamam, oturum durduruldu

- MP1a tamamlandı (dal `olcum/mp1-arac-zinciri` gönderildi, main'e birleştirilmedi); K0, flash yedeği, K1 geçti; K2 turları hatasız, tolerans ölçütü mimarca düzeltilecek; sol incelemesi sonucu işlenmedi.
- MP6a koşuları sürerken durdu (`olcum/mp6-godot-kamera`, commit'siz, rapor yok); MP7a başlamadan durduruldu, görev belgesi hazır.
- MP1a'nın mimari için bulguları (açık bulgulara işlenecek): Picolibc CubeIDE'nin 14.3.rel1 macOS paketinde yok (newlib-nano kullanıldı); ST'nin kart örneklerinin çoğu PLL'i MSIS'ten besliyor, HSE dizisi yalnız USBX örneklerinde; fabrika demosu silindi, yedeği yerel önbellekte.

### 2026-11-14 · mimari onaylandı, W0 başladı

- Kullanıcı mimariyi bilinen açık bulgularla onayladı; sıra W0 → W1 → uygulama (K15). Ölçümler için araç kurulumuna izin verildi.
- Windows hedef bilgisayarı: 8 GB bellek, dahili ekran kartı (K16); işlemci bilinmiyor.
- Kart Mac'e bağlı; STLINK_V3 numaralanıyor (mimarın gözlemi). Mac'te STM32CubeIDE var; cmake, ninja ve Godot kurulu değil.
- İki görev belgesi yazıldı (`docs/gorevler/W0-MP1a.md`, `docs/gorevler/W0-MP6a.md`); işler başlatılıyor.

### 2026-11-14 · dalga sırası önerisi ve sol inceleme betiği

- Mimar önerisi: ölçüm prototipleri W0, arayüz sözleşmeleri W1; A1 ölçüm sonuçlarıyla tek aşamada donar. `09-test-ve-is-akisi.md` sürüm 0.2; `acik-bulgular.md` yalnız dalga etiketleri güncellendi (bulgu içeriği ve sayılar değişmedi). Kullanıcı onayı mimari onayıyla birlikte bekleniyor.
- `tools/sol-incele.sh` eklendi (hafif uygulayıcı yazdı; sol inceledi: 4 bulgu, 1 kritik ve 3 önemli, tek turda giderildi). Düzeltmeden önce gerçek sol çağrısıyla bir kez çalıştı; düzeltmeden sonra yalnız kuru çalıştırma (`DRY_RUN=1`) ve macOS bash 3.2 ile denendi.
- `AGENTS.md` "Belge kuralları"na betiğin kullanımını söyleyen madde eklendi.
- sol'un yazma kipi bu depoda henüz denenmedi (ilk deneme W0'da).

### 2026-11-14 · açık bulgular kaydı, mimari onaya hazır

- `docs/mimari/acik-bulgular.md` yazıldı: 233 bulgu, 224'ü açık (33 kritik, 94 önemli, 8 küçük, 89 önem belirtilmemiş), 9'u kapsam dışı; 29 satırlık uyumsuzluk tablosu ve 26 maddelik iskelet önerisi listesi.
- sol ikinci incelemeleri kayda geçti: `inceleme/01-sol-2.md` ve `inceleme/05-sol-2.md`.
- Mimari çalışması taslak olarak tamamlandı; kullanıcı onayı bekleniyor. Sıradaki: onay, arayüz sözleşmeleri, ölçüm işleri.

### 2026-11-14 · dokuz bölüm tamam, döngü kesildi

- 08 ve 09 yazıldı (kısa biçim, incelenmedi); dokuz bölümün hepsi taslak olarak mevcut.
- 01 (0.2) ve 05 düzeltme turları yapıldı (`inceleme/01-yanit.md`, `inceleme/05-yanit.md`).
- Mimarın süreç kararı: inceleme-düzeltme döngüsü yakınsamadığı ve çok token tükettiği için kesildi; kalan bulgular `acik-bulgular.md` kaydında toplanacak, sözleşme ya da uygulama aşamasında kapatılacak.
- Süren (bu kayda dahil değil): 01 ve 05 sol ikinci incelemesi.

### 2026-11-14 · iskelet 0.7, bölüm 06 düzeltmesi, 03 durdurması

- İskelet 0.7: itkili torpido kararı geri alındı (itkisiz, kısa menzilli basit nesne; itkili torpido ve önleme hesabı kapsam dışı); 03 kapsamı emniyet zinciri ve arayüzlerle sınırlandı.
- 06 düzeltme turu yapıldı (sürüm 0.2, `inceleme/06-yanit.md`). İkinci sol incelemesi (`inceleme/06-sol-2.md`): kabul yok, 5 kritik ve 15 önemli yeni bulgu.
- 07 ilk sol incelemesi (`inceleme/07-sol.md`): kabul yok, 8 kritik ve 15 önemli bulgu.
- 03 düzeltmesi durdu: agent çıktısı güvenlik sınıflandırıcısı tarafından durdurulduğu için dosyaya dokunulmadı; kapsam daraltma için kullanıcı onayı bekleniyor.
- Süren (bu kayda dahil değil): 05 ve 01 düzeltme turları. Bekleyen: 08 ve 09 yazımı.

### 2026-11-13 · bölüm 03, 05, 07 ve düzeltme turları

- İskelet 0.4–0.6 (gövde referans noktası, itkili torpido, sanal emniyet girdileri, kamera bileşeni bağlantısı A8, malzeme değerlerinin yeri).
- 03, 05, 07 taslakları yazıldı; hepsi taslak, kullanıcı onayı yok.
- 02 ve 04 düzeltme turu yapıldı (`inceleme/02-yanit.md`, `04-yanit.md`). İkinci incelemeler: 02 kabul edilmedi (4 kritik yeni bulgu); 04'te kritik bulgu kalmadı, 5 önemli bulgu açık.
- 01, 03, 05, 06 ilk sol incelemesi: dördü de kabul edilmedi.
- Süren (bu kayda dahil değil): 06 ve 03 düzeltme turu, 07 incelemesi. Bekleyen: 01 ve 05 düzeltmesi, 08 ve 09 yazımı.

### 2026-11-13 · mimari taslakları ve ilk incelemeler

- İskelet 0.3'e çıktı (fizik adımı 1,25 ms; bölümler arası kararlar eklendi).
- 01, 02, 04, 06 bölüm taslakları yazıldı; hepsi taslak, kullanıcı onayı yok.
- 02 ve 04 sol incelemesinden geçti, ikisi de kabul edilmedi; bulgular açık, düzeltme turu yapılacak.
- 01 ve 06 incelemesi ile 05 bölümü sürüyor (bu kayda dahil değil).

### 2026-11-12 · agent yapısı ve mimari iskelet

- `.claude/agents/` altında altı agent tanımı eklendi (kritik uygulayıcı, uygulayıcı, hafif uygulayıcı, test yazarı, inceleyici, kayıt).
- Karar günlüğüne K11–K14 (mimar kod yazmaz, commit'i kayıt agent'ı atar, sol kodlayabilir ve yazan/inceleyen farklı aile, effort seviyeleri) ve Ö12 (en fazla iki agent) işlendi; `AGENTS.md` Çalışma yöntemi güncellendi.
- Mimari iskelet `docs/mimari/00-iskelet.md` yazıldı (taslak); bölümler yazılıyor.

### 2026-11-12 · kurallar ve kararlar

- Çalışma kurallarına kayıt ve süreklilik, dış içerik ve gerekçeli görüş kuralları eklendi (`AGENTS.md`).
- Karar günlüğüne K8 (tek odak), K9 (paneller web), K10 (araç parametreleri mimarda) ve Ö11 (kalite kademeleri) işlendi.
- Bu belge oluşturuldu.

### 2026-11-11 · araştırmanın kapanışı

- 12 (Godot entegrasyonu) ve 13 (donanım yol haritası) raporları yazıldı ve doğrulandı; 00 belgesi 01–13 kapsamına güncellendi.
- Proje araştırma ödevi olarak kayda geçti; yarışma şartnamesi araştırması iptal edildi (K7).
- Depo GitHub'a gönderildi.
