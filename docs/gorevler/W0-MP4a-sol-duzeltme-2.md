# W0-MP4a — ikinci ve son düzeltme turu

Tarih: 2026-11-18. Mimar: Codex. Yazan: Opus (`high`). İnceleyen: `gpt-6.1-sol` (`high`). Girdi: `docs/olcumler/inceleme/MP4-sol-2.md`. Bu görev sürmekte olan kart kampanyası bittikten sonra uygulanır; kart koşusu kesilmez.

İlk incelemenin #1, #3, #4, #5 maddeleri kapandı; #2 altında üç kritik ve bir önemli açık kaldı. Bu tur yalnız bu açıklara ve bunların regresyonlarına ayrılır. Mevcut firmware, hesap yükü ve ham veriler korunur; kartta yeniden ölçüm gerekmiyor.

## Düzeltmeler ve kabul kanıtı

1. **Y1:** Bütün örnekler histogramın dışında olduğunda da toplam süre alt/üst sınırı ve minimum/maksimum tutarlılığı denetlensin. Geçerli tümü-taşmış kayıt destekleniyorsa bunu doğru aralık ve yüzdelik sınırlarıyla koru; olanaksız toplam/minimum reddedilsin. Eksik H tablosu, `tasan=n`, sınırdaki tek örnek ve taşma olmayan olağan kampanya için hedefli testler ekle.
2. **Y2:** Yığın kullanımına hedef/ölçülen imajla gerekçelendirilmiş, bozuk kayıt tarafından gevşetilemeyen bir üst sınır uygula. Kaynak olarak gerçek ELF/linker bellek düzenini kullan; sınırın nasıl türetildiğini belgele. `yigin_tepe < yigin_kayit` gibi kapsama aykırı ilişkileri de denetle. 4294967295 B ve 1 B/3000 B karşı örnekleri reddedilmeli; gerçek O2/Og değerleri geçmeli. Firmware alanı ekleme.
3. **Y3:** `lat_temiz_n=n` ise iki kümenin maksimumu eşit olmalı; sıfır, alt küme ve bütün küme durumlarını sınayan testler ekle. Gerçek O2 A kaydındaki maksimumu 86'dan 59'a indirerek elde edilen bozuk kayıt reddedilmeli.
4. **Y4:** Histogram dosyaları ancak bütün kampanya denetimleri, T kontrolü dahil, geçtikten sonra yazılsın. Eksik T, yanlış T ve başka fazdaki hata için yeni histogram dosyası üretilmediğini dosya sistemi üzerinden sınayan test ekle. Önceden var olan kullanıcı dosyalarını silme/ezme davranışını kontrollü tut; başarısız analizi geçerli çıktı gibi sunma.
5. Bütün önceki testler ve yeni hedefli testler geçsin. Aynı ölçülmüş firmware imajlarını koruyarak tamamlanmış gerçek kısa/O2/Og ve yapılmışsa 200 Hz kayıtlarını yeni çözümleyiciyle ayrı v3 analiz çıktısına işle; önceki kayıt/analizleri ezme.
6. `docs/olcumler/inceleme/MP4-yanit-2.md` dosyasında Y1–Y4 için değişiklik, doğrulama komutu ve gerçek sonucu yaz. Önceki yanıt/inceleme dosyalarını yeniden yazma. Ana ölçüm raporunu nihai tanım ve sonuçlarla eşle.

## Sahiplik ve kapanış

Yazılabilir: `tools/olcum/mp4/pc/**`, `tools/olcum/mp4/test/**`, `tools/olcum/mp4/kos.sh`, `docs/olcumler/MP4-dongu-yuku.md`, `docs/olcumler/inceleme/MP4-yanit-2.md`. Diğer dosyalarda zorunlu değişiklik gerekirse mimara gerekçeyi bildir; kendin kapsamı genişletme. Commit/add/push/merge ve alt agent yok.

Bu ikinci düzeltme turudur. Sonrasında kalan kritik bulgu varsa aynı kapsamda üçüncü düzeltme turuna kendiliğinden girilmez; `09-test-ve-is-akisi.md` §2.4 uyarınca mimara döner. Hedef başarısız ölçümü başarılı göstermek değil, geçerli veriden doğru ve sınırlı sonuç üretmektir.
