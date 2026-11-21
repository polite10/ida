# W4 — ortak kararlar ve notlar

Tarih: 2026-11-21. Yazan: mimar. W3'ün ortak kuralları (`docs/gorevler/W3-ortak.md`: K20 çalışma kuralları, mimar kararları W3-K1…K14, rapor biçimi) W4 işlerinde de geçerlidir; bu belge yalnız W4'te eklenenleri taşır.

## Mimar kararları

- **W4-K1 — ses hızı kimde hesaplanır.** A5 §3.9'da `ortam.su.ses_hizi`'nin varsayılanı "formülden"dir ve çözümlenmiş senaryoda bulunmaz; çözümleyici bu değeri **yazmaz** (altın dosya ve davranış özeti değişmez). Formül bir sim modelidir (05 §2.4.2: tatlı suda UNESCO/Chen–Millero, deniz suyunda Mackenzie) ve sim tarafında, sensör ve sonar modellerinin ortak kullandığı tek bir işlevde durur. Bu işlevi W4-sonar yazar; `SensorKumesi::kur`, `ses_hizi` verilmemişse hata vermek yerine bu işlevi kullanır (W4-sonar'ın `sim/sensor`'a yapacağı tek değişiklik budur). O zamana dek `ornek-havuz` + etkin DVL birleşimi `sensor_kur`'da reddedilir; bilinen ara durumdur. Kaynak: W4-senaryo raporu, başlık 5.
- **W4-K2 — W4-kopru kabul md.2'nin okunuşu.** "Çekirdeğin reddettiği örnek sayısı 0" ifadesi `VALID = 0` işaretli örnekleri kapsamaz (A4-K33 gereği tanım olarak reddedilirler; ör. çözümsüz GNSS). Ölçüt: `VALID` bayraklı reddedilen örnek 0, gelecek damgalı örnek 0. Görev belgesindeki ifade mimarın hatasıdır.

## Sözleşmelerin küçük sürümüne (1.0.x) yazılacak notlar

Uygulama sırasında bulunanlar; sözleşme dosyaları şimdi değiştirilmez, notlar toplu işlenir.

| Sözleşme | Not | Kaynak |
|---|---|---|
| A5 | `scenarios/sema/a5.schema.json` `yonelim` elemanlarını ±1,0000001 ile sınırlıyor; A5-K32 normda 1e-6 (kamerada norm²'de 1e-5) tolerans tanır. Metin geçerli; şema düzeltilir. | W4-senaryo |
| A5 | §7 uygunluk tablosu K114 satırındaki "kalibrasyon sürümü 0 ve 65 536" şema aralık denetimine takılır; bulgu A5-K10 olarak raporlanır, K114 değil. Tablo düzeltilir. | W4-senaryo |
| A4 | §3.6 SIL'de `param_gen` = 0 diyor; A4-K82 kuşakların arttığını söylüyor: SIL'de depoda blok varsa hangisinin bildirileceği netleştirilir. | W4-sil (sol incelemesinde soruldu) |
| A1 / A4 | `REBOOT` sonrası SIL koşucusunun platform saati: kart yeniden açılışının karşılığı olarak sıfırlanır mı (A4-K11 "saat geri gitmez" ile ilişkisi). | W4-kopru bulgusu; W4-sil düzeltme turunda karara bağlanacak |
| A1 | W3-K11: nihai (atılmış) kimliğin daraltıcı olarak yeniden gelmesi. | W3-oturum |

W3'te biriken öteki notlar (`SAFETY_CLOSE` `safety_rev`, `yuk_ping1d` menzil penceresi, A7-K22 Godot davranışı, A2-K15/K62) ilgili işlerin raporlarındadır; toplu işlemede bu tabloya alınır.

## Bilinen açık konular (W4 içinde izlenir)

- Kart: tel katmanının oturumdaki maliyeti 485 ns/B; periyot başına en kötü 0,63 ms (MP5 eşiği 0,50 ms; 394 600 periyodun 4'ünde aşıldı; ölçüm, kart, W3-kart dilim 2). `PARAM_SAVE`'i işleyen step 2,5 ms (bütçe 3,5 ms). İkisi de Z2 ölçümünden sonra değerlendirilir; şimdilik iş açılmadı.
- HIL 1x zamanlayıcısı yüklü Mac'te 10–24 ms takılma gösterdi (kartsız deneme; W4-kopru); temiz makinede ve Windows'ta ölçülecek.
- SHA-256'nın üç kopyası var (`sim/sensor`, `sim/sil`, `sim/senaryo`); ortak yere toplama W4 sonunda ayrı küçük iştir.
