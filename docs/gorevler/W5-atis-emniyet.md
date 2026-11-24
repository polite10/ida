# W5-atis-emniyet — atış kontrolün emniyet zinciri (çekirdek)

Tarih: 2026-11-24. Yazan: `ida-uygulayici` (Opus). Bağımsız kabul testleri: `ida-test-yazari` (uygulama kaynağını okumadan; aynı anda başlar). İnceleyen: sol (K21: bir tam inceleme + bir dar denetim; üçüncü tur yalnız bir emniyet girdisinin uygulanmaması, geç uygulanması ya da yanlış gevşetme bulgusunda). Dal: `is/w5-atis-emniyet` (taban `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `docs/gorevler/W4-ortak.md`, `docs/gorevler/W5-ortak.md` (W5-K1…K11 ve kapsam sınırı).

## Kapsam sınırı (kullanıcı kararı; kesin)

Bu iş **yalnız emniyet zincirini** yazar: silahlandırma, süreli atış izni, acil durdurma ve sanal emniyet girdilerinin geri okuması, bunların durum makinesi, öncelik ve sonuç kodları, olayların üretimi/kimliği/geçerliliği, güvenli duruma dönüş ve kilitli arıza. **Yazılmaz ve ayrıntılandırılmaz:** hedefleme çözümü, önleme/vuruş hesabı, atış ölçütü ya da karar eşiği, hedef algılama/sınıflandırma/takip, torpido güdümü ya da başarımı. Çekirdek kendiliğinden atış kararı üretmez: `FIRE` olayı yalnız kabul edilmiş bir operatör `FC_FIRE` komutunun taşınmasıdır (W5-K3); araya otomatik bir ölçüt, tahmin ya da zamanlama iyileştirmesi konmaz. Algılama girdisi bu işte okunmaz; `TRACKING` ve `SOLUTION_VALID` durumlarına giriş yolu açılmaz (A4 §3.16'daki değerler eşlemede yer alır, erişilemez kalır).

## Amaç

`core/src/atis/` bugün yer tutucudur: durum hep `SAFE`, hiçbir genişletici olay adayı üretilmez. Bu iş onu, mimari Bölüm 03 sürüm 0.3'ün sonundaki **"Ek: uygulama gereksinimleri (emniyet zinciri)"** listesinin çekirdek maddelerine (G-Ç01–G-Ç53) göre gerçek emniyet durum makinesiyle değiştirir.

Okunacaklar: `docs/mimari/03-atis-kontrol.md` (0.3; özellikle §2.6, §2.6.1, §2.7, §4 ve Ek); `docs/gorevler/W5-ortak.md`; `docs/sozlesmeler/A4-port.md` (§3.5 sonuç kodları, §3.16 durum ve engel maskesi, §3.17 komut tablosu ve ret nedenleri, A4-K34, K36, K39–K48, K59–K73, K81, K86, K89, K90, K93, K105–K109, K113, K114, K116, K122, K123, K128 ve 1.0.1 notları); `docs/sozlesmeler/A1-kart-baglantisi.md`'nin emniyet örneği ve olay maddeleri (A1-K56, K64, K73, K80–K89; 1.0.1 notları); `core/src/atis/**`, `core/src/emniyet/**`, `core/src/olay/**`, `core/src/cerceve/**` (çağrı çerçevesinin modülden istedikleri), `core/TASARIM.md`.

## Kapsam

1. **Durum makinesi** (G-Ç ilgili maddeler; W5-K1, K2, K3, K8, K9, K11): `SAFE`, `ARMED`, izin açık/atış bekleyen durum(lar), değerlendirme sonu, `TEST`, `FAULT_LATCHED`; A4 §3.16'nın altı değerine eşleme; tüp başına durum; kabul edilen komutlar ve ret nedenleri A4 §3.17'nin kapalı listesinden; daraltıcı komutlar her durumda.
2. **Öncelik ve sonuç kodları** (W5-K10): daraltıcı etkinin uygulanması ile komutun sonuç kodu ayrıdır; yineleme denetimi, `CALL_REJECTED`, `NARROWED`, `DUPLICATE` önceliği sözleşmeden.
3. **Emniyet girdilerinin geri okuması** (W5-K4 ve eki; W5-K8): acil durdurma, fiş, pim; tüp başına kısıtlama ve "kısıtlayana dönüşün gerçekleştiği çağrı" kuralı; bayatlığın iki ölçütü; `T_emn_bayat` parametresi (sınıf B; varsayılan 250 ms, **tahmin**: `atis_param.def`'te ve `core/TASARIM.md`'de öyle yazılır); çelişki tanımı.
4. **Olaylar** (A4-K60–K70, K106; W5-K2, K11): `SAFETY_OPEN`, `SAFETY_CLOSE`, `FIRE` adaylarının üretimi, kimlik sayacı ve son 16 kimlik kuralı (A4-K62), `apply_by_ns`/`hold_ns`, sonuçların işlenmesi, sonucu bilinmeyen olay, olay kimliği kullanılamazken davranış.
5. **Güvenli duruma dönüş ve kilitli arıza** (W5-K1, K8): oturum sonu, kalp atışı kaybı, bayatlık, acil durdurma, fiş/pim; `FAULT_LATCHED`'ın korunması; `FC_RESET`.
6. **Parametreler** (W5-K6): emniyet zinciri parametrelerinin hepsi `IDA_PCLS_B`.
7. **Gözlenebilirlik:** durum, engel maskesi (`inhibit`), sayaçlar ve günlük kayıtları sözleşmedeki alanlarla (yeni alan yok).
8. Her G-Ç maddesi için kodda bir izlenebilirlik notu (madde kimliği yorumda) ve en az bir birim testi (`core/test/atis/**`).

## Kapsam dışı

Platform tarafı denetimler ve kapılar (G-K01–G-K20: `firmware/`, `protocol/ida_link`, SIL koşucusu): ayrı iş (W5-atis-platform). Sim tarafındaki ek kabul denetimi (W5-K5: `sim/kopru`): ayrı iş. Sözleşmelerin 1.0.x notları: ayrı belge işi. `tests/kabul/**` (test yazarının). `protocol/`, `firmware/`, `sim/`, `web/`.

## Dosya sahipliği

Yazılır: `core/src/atis/**`, `core/test/atis/**`, `core/TASARIM.md`'nin atış kontrol bölümü, gerekiyorsa `core/src/cerceve/**` içinde yalnız modülün çağrılma noktaları (çerçevenin öteki davranışı değişmez; değişmesi gerekiyorsa DUR ve raporla). A4 portu (`core/include/ida/port.h`) DEĞİŞMEZ; bir gereksinim portta karşılığı olmayan bir alan istiyorsa uygulanmaz, raporlanır.

## Kabul

1. G-Ç01–G-Ç53'ün her biri için: uygulandı (dosya:satır, test adı) / uygulanamadı (neden: port, çerçeve, sözleşme çelişkisi) tablosu.
2. **Emniyet değişmezleri (birim testleriyle; rastgele komut/olay/geri okuma dizileri üzerinde, sabit tohum, en az 200 000 adım):** (a) acil durdurma basılı ya da bayatken hiçbir çağrıda genişletici olay adayı üretilmez; (b) `FIRE` adayı yalnız `ARMED` durumunda kabul edilmiş operatör `FC_FIRE`'ından ve o tüpün izni `APPLIED` olduktan sonra, izin penceresi içinde üretilir; (c) `FAULT_LATCHED` yalnız `FC_RESET` ile ve neden sürmüyorsa terk edilir; (d) `TEST`'te hiçbir genişletici olay üretilmez; (e) daraltıcı komut hiçbir durumda ve biçimde reddedilmez (etkisi uygulanır); (f) sonucu bilinmeyen genişletici olay hiçbir zaman uygulanmış sayılmaz; (g) silahlıyken emniyet parametresi değişmez.
3. Var olan bütün testler değişmeden geçer (`kabul.cekirdek`, `kabul.gorev`, `zincir.z1`, köprü ve bütünleme testleri; Z1 çıktısı bayt bayt aynı: Z1'de atış kontrol komutu yok).
4. Çağrı süresi: `host-release`'te step çağrısının süresi bu iş öncesi/sonrası (bağıl ölçüm; en az 10 000 tekrar); kart ölçümü ayrı iş, burada "tahmin" yazılmaz, yalnız ölçülen bağıl fark yazılır.
5. `host-debug` + `host-release` tam tur (sonda bir kez); `fw-release` derleme (kod boyu farkı raporlanır); W3-K12 statik analiz, W3-K14 taraması; MSVC `/analyze` kalıpları.

## Bağımsız kabul testleri (ayrı agent; `tests/kabul/atis/**`)

Test yazarı yalnız şunları okur: Bölüm 03 0.3'ün Ek'i (G-Ç maddeleri), `docs/gorevler/W5-ortak.md`, A4 portu (`core/include/ida/port.h`, `docs/sozlesmeler/A4-port.md`), var olan `tests/kabul/cekirdek/**` düzeneği. `core/src/**` okunmaz. Her G-Ç maddesi için en az bir kabul testi (ad: `kabul.atis.<madde>.<kısa ad>`), yukarıdaki yedi emniyet değişmezi için rastgele dizili özellik testleri ve Bölüm 03 §7'deki arıza senaryolarından emniyet zincirine ait olanlar (F1, F3, F7, F8, F10–F15). Testler uygulama gelmeden yazılır; yer tutucuya karşı düşen testler "bekleniyor" diye listelenir, uygulama dalı birleşince geçmeleri gerekir. Bir G maddesi sınanamıyorsa (port gözlenebilirliği yetmiyorsa) test yazılmaz, neden raporlanır.
