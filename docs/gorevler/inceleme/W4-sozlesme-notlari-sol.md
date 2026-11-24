# sol incelemesi: sozlesmeler

## Hüküm

**Düzeltme gerekli: 1 kritik, 1 önemli bulgu.**

İnceleme yalnız `git diff 4bea7b5..255adae -- docs/sozlesmeler/` kapsamındadır. Kaynak karar olarak `docs/gorevler/inceleme/W4-ortak-guncel.md` kullanıldı. Dosya değiştirilmedi; kurulum, derleme ve test yapılmadı.

Eski metinden çıkarılan dört satırın tamamı sürüm satırıdır. Mevcut maddeler, numaralar, tablo satırları, şemalar ve sabitler değiştirilmemiştir.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `docs/sozlesmeler/A3-web.md:943` | **W4-K19(a) muafiyeti uygulama aşamasındaki retle çelişiyor.** Not yalnız A3-K54’ün girişteki 409 reddini kaldırıyor. A3-K57 ise `beklenen_run_id` taşıyan **her** komutu fizik sınırında yeniden karşılaştırıp uyuşmazlıkta uygulamıyor; A3-K124 de eşleşmeyi istisnasız zorunlu tutuyor. Böylece bayat kimlikli acil durdurma girişte kabul edilip dünyada reddedilebilir. Notta bu iki maddeye karşı öncelik açıkça çözülmemiş. | W4-K19(a) notuna, kısıtlayan isteklerde çalıştırma kimliği eşleşmesinin **hem girişte hem uygulama sınırında** aranmadığını; bu kapsamda A3-K57 ve K124’ün eşleşme hükümlerinin uygulanmadığını açıkça ekle. Gevşetenlerin denetimleri korunmalı. | kritik |
| 2 | `docs/sozlesmeler/A5-senaryo.md:1189` | **Yeni sürüm politikası A5-K04’le çelişiyor.** “Anahtar, enum değeri ya da şema sınırı değiştiren her düzeltme minor sürümdür” genellemesi; anahtar silme, zorunlu anahtar ekleme ve anlam değişikliğini de kapsıyor. A5-K04 bunları major artış sayıyor. Kaynak W4-K17 yalnız `GUIDED_SP` enum eklemesini 1.1’e bırakıyor; genel sürüm politikasını değiştirmiyor. | Genellemeyi kaldır; burada listelenen değişikliklerin dosya sürümünü ayrıca gerektirdiğini söyle ve major/minor ayrımı için A5-K04’ü geçerli bırak. | önemli |

## Yeniden hesaplananlar

- **Olay geçmişi:** `65 536 × 8192 = 536 870 912 B = 0,5 GiB`. A3 notundaki yaklaşık değer doğru; nesne ve kayıt üst verisi bu hesaba dahil değildir. `64 MiB / 8192 B = 8192` yalnız içerik kaydıdır.
- **Kuaterniyon örneği:** `(1,0000005; 0; 0; 0)` için `norm − 1 ≈ 5·10⁻⁷`, `norm² − 1 ≈ 1,00000025·10⁻⁶`. A5-K32 toleransını sağlarken bileşen sınırı `1,0000001`’i aşar; nottaki karşı örnek doğrudur.
- **Gönderim tahmini:** `3 Mbaud / 10 = 300 B/ms`. Çağrı boyunca yalnız önceden verilmiş en çok 156 B ilerlediği iyimser modelde, 3,5 ms çağrı için toplam hizmet üst sınırı `156 + 300 × 1,5 = 606 B/periyot` olur. Aynı modelle hizmet ve kararlılık eşikleri yaklaşık **1,94 ms** ve **2,91 ms** çıkar. W4-K16’daki 1,9/2,9 ms aktarımıyla uyumludur; bunlar ölçüm veya koşulsuz hizmet kanıtı değildir.
- 10 ms anahtar gecikmesi, 600/1200 örnek, 5000/10000 ışın ve W4-K16 ölçüm değerlerinde kaynak karardan farklı sayı veya birim saptanmadı. Tahmin ve ölçüm etiketleri ayrılmıştır.

## İskelete ya da diğer bölümlere notlar

- W4-K14 ile K25(4)/K26(5) arasındaki **koşu ortasında oturum bitişi / koşunun bitirilmesi** ayrımı açıkça yazılmıştır. W4-K15’in A1-K83’e, W4-K10’un A1-K68’e getirdiği istisnalar da açıklanmıştır.
- İskelet §4’teki “nötr durumdan çıkış için ayrı bir kural yoktur” genellemesi W4-K3’e göre güncellenmelidir. Sözleşme notunun kısıtlayan hükmü geçerlidir.
- 1.1 listelerinde ertelenen şema ve tel değişiklikleri yapılmış gibi sunulmuyor. Özellikle W4-K19’un alanın verilmemesi ve yanıtta güncel kimlik bölümleri açıkça ertelenmiş.
- **Sonraki sürüme — bulgu değildir:** W4-K23 ek 2’nin kabul döngüsü, toplam çıkış tavanı ve erken retlerde jeton iptali; W4-K27’nin duraklatma ve aktarım sırası kuralları; W4-K28’in dilim kapsamı ve tek işlem yolu kararı.
- HTTP yanıt sırası ve kademeli bağlantı kapanışı [RFC 9112 §9.3.2 ve §9.6](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.3.2), Windows paylaşım davranışı [Microsoft CreateFile belgesi](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew) üzerinden denetlendi. Ses hızı formül adları ve değişkenleri [NPL kaynağıyla](https://resource.npl.co.uk/acoustics/techguides/soundseawater/underlying-phys.html) karşılaştırıldı.
- `docs/olcumler/W4-z2.md` bu çalışma ağacında bulunamadı. Z2 sayılarının karar metnine uygun aktarımı denetlendi; ham ölçüm kanıtı bağımsız doğrulanmış sayılmadı.
- Eklenen içerikte yeni hedefleme, atış ölçütü veya önleme hesabı saptanmadı.