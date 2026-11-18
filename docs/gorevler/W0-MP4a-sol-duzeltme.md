# W0-MP4a — sol ilk inceleme düzeltmesi

Tarih: 2026-11-17. Mimar: Codex. Uygulayıcı: Opus (`high`). İnceleyen: `gpt-6.1-sol` (`high`). Bu görev ilk uygulama/ölçüm turu bittikten sonra verilir; devam eden kart koşusunu kesmez.

## Girdi ve kapsam

### 2026-11-18 devam notu

Kullanıcı devam talimatı verdi. İlk Opus turu uyku hatasıyla yarıda kaldı; artık çalışan kart ölçüm süreci yok ve seri port yeniden mevcut. `og-20261117-193512` koşusu `Device not configured` ile kesildi, fazlar tamamlanmadı; kabul kanıtı değildir. O2 `o2-20261117-190043` tam kampanyası sol tarafından bağımsız CRC/sıra/histogram/özet/süre denetimleriyle doğrulandı; korunacak ve yeniden koşulmayacak.

Bu turda önce aşağıdaki PC düzeltmeleri ve testler yapılır, O2 yeniden analiz edilir. Ardından mevcut, kimliği ve SHA-256'sı kayıtlı Og imajıyla eksik uzun kampanya yeniden alınır (bu eksik ölçümün tamamlanmasıdır; firmware kaynağını değiştirme). Mevcut 200 Hz imajlarıyla ek koşular da tamamlanabilir; öncelik zorunlu Og A/B ölçümlerindedir. Donanım yükleme yetkisi ve yedek doğrulama koşulu ana görevdeki gibi geçerli. Karta erişim kesilirse dürüstçe raporla; eksik koşuyu başarılı sayma.

Rapor başlığı şu anda sonuçları kesin gösterirken altı `SONUCLAR` yer tutucusuyla bitiyor; rapor başlığı da gerçek tamamlanma durumuyla eşleşmeli. O2 D1/D2 geçmedi; D3 önceki ELF denetiminde geçti; D4 ve kampanya bütünlüğü tamamlanmış veri kapsamıyla yazılmalı. Kapanış incelemesini mimar başlatacak.

- `docs/olcumler/inceleme/MP4-sol.md`: iki kritik, üç önemli bulgu ve bağımsız tekrar üretme komutu.
- `docs/gorevler/W0-MP4a-devralma.md` ve ana görev geçerli.
- Mevcut uzun koşuların ham verileri korunur. Bulunan kusurlar PC çözümleyicisi ve raporlama kapsamındadır; sırf bu bulgular nedeniyle firmware değiştirme/yükleme veya koşu tekrarı yok. Gerçek veri incelemesinde yeni bir geçersizlik çıkarsa bunu ayrıca bildir.
- Hesap yükü `adim/**` değiştirilmez; algoritma geliştirme/optimizasyon kapsam dışı.

## İstenen düzeltmeler

1. İncelemedeki #1: kampanya başlığı ve beklenen hız/faz maskesi/adım hedefi doğrulansın; zorunlu A/B kampanyası bildirilen boş maskeyle küçültülemesin. B2 sayısı, zorunlu alanları ve saat/UART koşulları doğrulansın. Eski gerçek kayıtların kabulü, bilinen imajın gerçek koşu ayarları açıkça verilerek yapılmalı; doğrulama gevşetilmemeli.
2. #2: G/Q/S zorunlu alanları, kesin sayı ayrıştırması, histogram ve özet tutarlılığı, taşma ve sayaç sınırları doğrulansın. Bozuk girdi için başarılı veya sayısal olarak yanıltıcı rapor üretilmesin.
3. #3: `duvar_cyc` mevcut firmware'de ilk adım başlangıcından son adım başlangıcına kadardır. Bunu muhafazakâr süre alt sınırı olarak doğru adlandır. Son adımın kesin süresi elde yok; gerekiyorsa `[duvar+min, duvar+max]` aralığı kullan. 600 saniye koşulunun bu alt sınırla sağlandığını doğrula.
4. #4: yığın ölçümlerinin gerçek kapsamını yaz; açılıştan beri monoton tepe iddiasını kaldır. Koşu özeti için fazlarda gözlenen tepelerin maksimumunu kullan; geçiş adımlarının ve ölçüm çevresinin kapsama dahil olduğunu belirt.
5. #5: `≤` yüzdelik üst sınırlarını gösterilen hassasiyette yukarı yuvarla; testi de düzelt.
6. İncelemenin bağımsız olumsuz örneklerini ve ek sınır durumlarını teste ekle. Önceki 31 testin yanında yeni testler de geçmeli. Gerçek kısa ve bütün uzun kayıtları düzeltilmiş çözümleyiciyle yeniden analiz et. Eski analiz çıktısını sessizce ezme; düzeltme sonrası ayrı analiz çıktıları tut.
7. `docs/olcumler/MP4-dongu-yuku.md` raporunu son sonuçlar ve tanımlarla eşle. `docs/olcumler/inceleme/MP4-yanit.md` dosyasında her bulguya değişiklik/kanıt/komut ile yanıt ver. Çalıştırılmayan kontrol açıkça belirtilir.

## Sahiplik ve bitiş

Bu turda yazılabilir: `tools/olcum/mp4/pc/**`, `tools/olcum/mp4/test/**`, `tools/olcum/mp4/kos.sh`, `docs/olcumler/MP4-dongu-yuku.md`, `docs/olcumler/inceleme/MP4-yanit.md`. Diğer dosyaları değiştirme. Ölçüm altyapısının başka dosyasında zorunlu bir değişiklik gerekiyorsa önce gerekçeyi mimara bildir.

Commit/add/push/merge ve alt agent yok. İş bittiğinde testler, yeniden analizler, D1–D4 hükümleri, açık kalanlar ve değişen dosyalar bildirilir. Kritik bulguların kapanışını sol doğrular; bu dosya kabul kararı değildir.
