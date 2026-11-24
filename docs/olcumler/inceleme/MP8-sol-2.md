# sol ikinci incelemesi: windows

## Hüküm

**Kabul edilmez:** H3’te yanlış “geçti” yolu kaldı. W1 de tamamlanmamış durumda. Dört hedef dosya baştan sona okundu; dosya değiştirilmedi, kurulum veya çalıştırma yapılmadı.

## İlk incelemedeki maddelerin durumu

| # | Durum | Not |
|---|---|---|
| 1 | giderildi | Temizlik yalnız kaydedilen süreç nesnelerini kullanıyor; kuru çalıştırma korunmuş. |
| 2 | giderildi | H2/H4 koşulları, süre, istek sayısı ve dolu yüzdelikler denetleniyor. |
| 3 | giderildi | Hatalar ve başlangıç/bitiş dahil gözlem boşlukları hükme giriyor. |
| 4 | giderildi | Dikdörtgen kapsaması, Z sırası ve DPI doğrulanabilirliği aranıyor; Windows davranışı sınanmamış. |
| 5 | giderildi | Temiz bitiş ayrı denetleniyor; hata, öldürülme, zaman aşımı ve bilinmeyen çıkış kabul edilmiyor. |
| 6 | giderildi | İlk bulgudaki eksik `kare.csv` referansı eleniyor; diğer koşular sürdürülebiliyor. |
| 7 | giderildi | İki boyut, gerçek yenileme değeri, bilinen güç durumu ve başarılı güç planı sorgusu aranıyor. |
| 8 | kısmen | W1 doğru biçimde tamamlanmadı; W2 kısmi yazılmış. Bildirilen Mac koşusunun ham verisi bağımsız doğrulanmadı. |
| 9 | giderildi | Başka uygulamaların başlıkları kaydedilmiyor; BENIOKU düzeltilmiş. |

## Yeni bulgular

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem | Kapatılacağı yer |
|---|---|---|---|---|---|
| Y1 | `tools/olcum/windows/analiz_win.py:324`, `:417` | **H3 geçersiz görünür referansla geçebilir.** `referans_gecerli`, referansın dış/iç pencere doğrulamasını değerlendirmiyor; bilinmeyen süreç bitişini de kabul ediyor. Görünür H2 `dogrulanmadi` veya durum nedeniyle geçersizken simge/örtülü H3, düşük MAD ile `gecti` olabilir. | H3 öncesinde görünür referansın durumunun doğrulandığını ve süreç bitişinin açıkça `True` olduğunu zorunlu kıl. | kritik | MP8 çözümleyicisi |
| Y2 | `tools/olcum/windows/calistir.ps1:433`, `:539`, `:1253` | **15 dakika sınırı çalışan koşuda uygulanmıyor.** `Sure-Yeter` kampanya için 216 s ayırırken hazır beklemesi 180 s, ardından izleme 240 s sürebiliyor. Küresel sınır yalnız adımlar arasında denetleniyor. | Hazır bekleme ve izleme döngülerini kalan küresel süreyle sınırla; aşımı kaydet, kalan koşuları atla. | önemli | MP8 çalıştırıcısı |

## Yeniden hesaplananlar

- `120 × 30 = 3600`, `60 × 30 = 1800`, `20 × 30 = 600` kare. <%1 tek başına en fazla 35 düşmeye izin verir; kesintisiz kimlik şartı ayrıca uygulanıyor.
- 36 döküm: RGB8 **33.177.600 B**, RGBA8 **44.236.800 B**; ikisi de `50MB` altında.
- `2/255 ≈ 0,00784314`; MAD eşiği `2.0` doğru.
- SHA-256 sabiti [resmî varlık kaydıyla](https://github.com/godotengine/godot-builds/releases/expanded_assets/4.7.2-stable) eşleşiyor. `SHA512-SUMS.txt` içeriğine erişilemedi; SHA-512 bağımsız doğrulanmadı.

## İskelete ya da diğer bölümlere notlar

- Elle okumada açık ayraç/tırnak hatası, yanlış here-string kapanışı, PS7 işleci, kapsam veya tanım sırası hatası saptamadım. C#5/.NET Framework açısından açık uyumsuzluk görmedim; bunlar gerçek ayrıştırma/derleme kanıtı değildir.
- `ExitCode` öncesindeki `HasExited` denetimi ve erken `Handle` erişimi nedeniyle yalnız koşulsuz `WaitForExit` bulunmamasını hata saymıyorum. [Microsoft açıklaması](https://learn.microsoft.com/en-us/dotnet/api/system.diagnostics.process.exitcode?view=netframework-4.8.1)
- DPI çağrısı 1607 öncesinde yok; V2 bağlamı 1703 öncesinde yok. Kod istisna/NULL durumunda geometriyi doğrulanamaz yapıyor; bu konuda yazarın gerekçesi doğru. [API](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setthreaddpiawarenesscontext), [V2 sürüm koşulu](https://learn.microsoft.com/en-us/windows/win32/hidpi/dpi-awareness-context)
- MP8a kısa ön turdur; bölüm 07’deki 30 dakikalık P5 kabulünü veya kalite kademesi tabanını tamamlamaz.