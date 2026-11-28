# W5-atis-platform — emniyet zincirinin platform tarafı denetimleri (G-K01–G-K20) ve W5-K5 sim kuralı

Tarih: 2026-11-28. Yazan: mimar. Uygulayan: `ida-kritik-uygulayici` (Fable; emniyet işi, K28). Bağımsız kabul testleri: `ida-test-yazari` (Opus; uygulama kaynağını okumadan; aynı anda başlar). İnceleyen: `ida-inceleyici` (Opus; K27 gereği bir kez denenir; takılırsa sol). İnceleme düzeni K21: bir tam inceleme + bir dar denetim; **emniyet dilimi, incelemesiz birleşmez (K27)**. Dal: `is/w5-atis-platform` (taban `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `W4-ortak.md`, `W5-ortak.md` (özellikle W5-K5, K7, K18, K26(e), K27(a), K43 ve bu işin kararları W5-K68…K70).

## Amaç

Bölüm 03 sürüm 0.3'ün §5 Ek'indeki **platform maddelerini (G-K01–G-K20)** kart platformunda (`protocol/ida_link` üst katmanı ve `firmware/app`) ve SIL koşucusunda izlenebilir biçimde uygulamak ve doğrulamak. Maddelerin bir kısmı bugün uygulanmıştır (daraltıcı mandalı, birleştirme, erteleme, ilk kabul kapısı, olay yuvaları, `thr` kapısı, olay yinelemesi ve sonuç payı, oturum sonu sırası, gönderim kovaları, `core_ok`/IWDG). Bu iş uygulanmış olanı yeniden yazmaz: her madde için (uygulama yeri, test) kanıtını tabloya yazar; **eksik olanı en dar değişiklikle uygular**.

Kapsam sınırı değişmez: hedefleme, atış ölçütü, algılama, takip ve torpido başarımı yazılmaz. Atış kontrol içeriği yalnız emniyet zinciri ve arayüzlerdir.

## Kapsam

1. **G-K izlenebilirlik ve boşluk tablosu** (kabulün 1. maddesi). 20 maddenin her biri için: uygulandı (dosya:satır, test adı) / uygulanamadı (neden). Bilinen boşluk:
   - **G-K14 / W5-K7 (bugün açık):** `protocol/ida_link/src/ust_esleme.c` içinde `IDA_FC_DISARM`/`IDA_FC_ABORT` için `tube ≠ 255` görülünce komut `UST_KOMUT_BOZUK` sayılıyor ve mandala alınmıyor. W5-K7 ve W5-K18 gereği komut **bütün tüplere** uygulanmalı (mandala alınmalı), komut sonucu `ACCEPTED` olmalı ve biçim hatası ayrıca görünmelidir: `IDA_LINK_SAY_CMD_BAD_FIELD` sayacı + platform günlüğü. `LINK_BAD_FIELD` reddi yalnız `op ∉ {2, 3}` işlemlere uygulanır. Aynı kural `arg0`/`arg1` ≠ 0 için de geçerlidir (G-K14 metni: "`tube` ve `arg0`/`arg1` değerlerinden bağımsız"). `op` kapalı liste dışıysa ve diğer bozuk alanlarda davranış A1-K64'teki gibidir.
2. **W5-K5 sim kuralı** (`sim/kopru`): sim bir `FIRE` olayını yalnız **kendisinin karta ilettiği ve kartın kabul ettiği** bir operatör `FC_FIRE` komutuyla (aynı tüp, geçerlilik süresi içinde) eşleştirebiliyorsa uygular; eşleşmeyen `FIRE` reddedilir (`REJ_SAFE`) ve olay olarak kaydedilir. Uygulanmışsa kanıtlanır (dosya:satır + test), eksik parça en dar değişiklikle tamamlanır. Tel biçimi değişmez.
3. **W5-K27(a) — zaman çerçevesiz step denetimi:** platform `ida_core_step`'i `tf.present = 0` ile çağırmaz. Çağrı yolları: `firmware/app/cagirici.c`, `sim/sil/src/kosucu.cpp`, `sim/kopru/src/karsi.cpp`. Ortak bir platform yardımcısı (ör. `ida_link_ust_cagri_gecerli`) `IDA_LINK_CAGRI_STEP` için `tf.present = 1` şartını uygular; çağrı çekirdeğe gitmeden durur, sayılır ve günlüğe yazılır; çekirdeğe sözleşme dışı girdi üretilmez. Çekirdeğe `BAD_INPUT` ürettirilmez (çağrı hiç yapılmaz). Kural `ida_core_tick` çağrılarını etkilemez.
4. **W5-K26(e) kararı — etkin acil durdurma `ARM`'ın ön koşulu değildir (W5-K69; W3-K1 korunur):** A4 §3.17'nin `ARM` satırı kapalı listedir ve emniyet girdilerini içermez; `FC_ARM`'ın ESTOP koşulu atış kontrolüne özgüdür. Acil durdurmanın araç üzerindeki etkisi `W5-kontrol-acil`'dedir: çıktı nötr, kalkışta rampa. Bu dilimde çekirdeğe yeni ret koşulu eklenmez; platform da sonuç üretmez. Karar budur; sözleşme değişikliği yoktur. Mevcut birim testi (`test_md2_emniyet_girdileri_arm_kosulu_degil`) bu kararı doğrular ve değişmez.
5. **Kısa kart/HIL doğrulaması:** kart bağlıysa mevcut Z1 HIL koşusu bir kez (≈ 60 s); kaçırılan parti 0, geçersizlik 0x0, bütçe aşımı 0. Kart yoksa "koşulmadı" yazılır. **Kartta silahlandırma/atış izni denemesi YAPILMAZ** (ayrı iş).

## Kapsam dışı

Çekirdeğin G-Ç maddeleri (W5-atis-emniyet'te bitti; bu iş yalnız W5-K69 satırına dokunur), hedefleme/algılama/takip, sözleşme dosyalarının düzenlenmesi (yalnız W5-ortak not tablosuna satır), `kwp`/`firmware` kart ölçümleri, sim'e torpido modeli eklemek, `tests/kabul/**` (test yazarının), `web/`, `viz/`, `sim/a3`, `sim/a2`, `sim/kayit`.

## Dosya sahipliği

Yazılır:
- `protocol/ida_link/src/**`, `protocol/ida_link/include/ida_link/**` (yalnız gerekli en dar ekleme),
- `protocol/ida_link/test/**` (birim testleri),
- `firmware/app/**` (yalnız G-K10, G-K11, G-K20 ve W5-K70 için gereken en dar değişiklik; davranış değişmiyorsa dokunulmaz),
- `sim/kopru/src/**`, `sim/kopru/test/**` (yalnız W5-K5 ve W5-K70),
- `sim/sil/src/**` (yalnız W5-K70),
- `docs/gorevler/W5-ortak.md` (yalnız W5-K68…K70 ve not tablosu satırı),
- rapor dosyası `docs/gorevler/inceleme/W5-atis-platform-rapor.md`.

Değişmez: `core/include/ida/port.h`, `docs/sozlesmeler/**`, `tests/kabul/**`, `web/`, `viz/`. `protocol/ida_link` ve `firmware/app` davranış değişikliği mevcut testleri değiştirmeden geçmeli; geçmiyorsa **DUR ve raporla** (test değişikliği yalnız test yazarı/mimar kararıyla).

## Kabul

1. G-K01–G-K20 tablosu: her madde uygulandı (dosya:satır, test adı) / uygulanamadı (neden). Uygulanmış maddelerin kanıtı; eksik olanlar bu dilimde eklendi.
2. **G-K14/W5-K7:** `tube` ∈ {0,1,254} ya da `arg0`/`arg1` ≠ 0 olan `FC_DISARM`/`FC_ABORT` mandala alınır ve bütün tüplere uygulanır; komut sonucu `ACCEPTED`; biçim hatası `IDA_LINK_SAY_CMD_BAD_FIELD` ve günlükte görünür; `op` kapalı liste dışı → `LINK_BAD_FIELD`; nihai `NARROWED` kimliği kuralı (G-K15) bozulmaz.
3. **W5-K5:** eşleşen `FIRE` uygulanır; `FC_FIRE` yok, farklı tüp, süresi geçmiş ya da sonucu bilinmeyen durumlarda `REJ_SAFE` ve olay kaydı. Eşleştirme aynı tüp ve geçerlilik süresi içindedir; tel biçimi değişmez.
4. **W5-K70:** `IDA_LINK_CAGRI_STEP` + `tf.present = 0` çağrısı çekirdeğe hiç gitmez; sayaç/günlük artar; `ida_core_tick` etkilenmez; üç çağrı yolunun hepsi aynı yardımcıyı kullanır ve test edilir.
5. **W5-K69 (karar):** yeni davranış eklenmez. Etkin estop basılıyken `ARM` mevcut karara göre (W3-K1, A4 §3.17) kabul edilir; silah durumu değişmez; çıktı nötrdür ve kalkış rampalıdır. `test_md2_emniyet_girdileri_arm_kosulu_degil` değişmeden geçer.
6. Var olan bütün testler değişmeden geçer (`host-debug`; `host-release` sonda bir kez); `fw-release` derlenir (kod boyu farkı raporlanır); W3-K12 statik analiz, W3-K14 taraması ve MSVC `/analyze` kalıpları; `derleme` CI yeşil.
7. Kısa HIL koşusu (kart bağlıysa) temiz; kart yoksa raporlanır.
8. Çağrı süresi: önce/sonra bağıl ölçüm (`host-release`, en az 10 000 tekrar); ölçülmediyse "ölçülmedi" yazılır. ASan'lı ikililer bu Mac'te takılır; `host-asan` yalnız CI'da koşar (W5-K43).

## Bağımsız kabul testleri (`tests/kabul/platform/**`; ayrı agent)

Test yazarı yalnız şunları okur: Bölüm 03 §5 Ek (G-K maddeleri), `docs/sozlesmeler/A1-kart-baglantisi.md` (A1-K63, K64, K67, K72–K83, K107, K120, K124), `docs/sozlesmeler/A4-port.md` (A4-K44–K46, K59, K64, K67, K68, K70, K71, K74, K89, K105–K108, K113, K120, K122, K125), `docs/gorevler/W5-ortak.md` (W5-K5, K7, K18, K26(e), K27(a), K43, K68–K70), `protocol/ida_link/include/ida_link/ust.h` genel API'si ve var olan `tests/kabul/protokol/**` düzeneği. `protocol/ida_link/src/**`, `firmware/app/**`, `sim/kopru/src/**`, `core/src/**` okunmaz.

- Her G-K maddesi için en az bir test (`kabul.platform.<madde>.<kısa ad>`); W5-K5 için sim kuralı testi; W5-K70 için test (zaman çerçevesiz step çağrısının platformdan çıkmadığı ve savunma denetiminin saydığı); W5-K69 için yeni davranış yoktur, W3-K1 doğrulanır.
- Rastgele komut/olay/sonuç dizileriyle en az 200 000 adımlık özellik testleri: (a) daraltıcı hiçbir koşulda atılmaz ve en geç sonraki çağrıda `narrow` alanında gelir; (b) ilk kabul kapısından geçmeyen olay hiçbir zaman uygulanmış sayılmaz; (c) `core_ok = 0` iken IWDG besleme kararı "besleme yok"tur; (d) `SAFETY_CLOSE` yer/kova yokluğundan düşmez; (e) her olay sonucu çekirdeğe tam bir kez verilir.
- Testler uygulama gelmeden yazılır; yer tutucuya karşı düşenler "bekleniyor" diye listelenir, uygulama dalı birleşince geçmeleri gerekir. Bir madde gözlenebilirlik yetmediği için sınanamıyorsa test yazılmaz, neden raporlanır.

## Rapor

PR ve dal ucu; G-K tablosu; uygulanan boşluklar ve gerekçeleri; düşürülerek gösterilen testler; ölçülen bağıl süre; kısa HIL sonucu; sözleşmede belirsiz bulunan yerler ve seçilen okuma; emniyet girdisinin uygulanmadığı ya da geç uygulandığı görülen her yol.
