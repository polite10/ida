# MP4a — mimar kapsam daraltmasına ve sol üçüncü incelemesine yanıt

Tarih: 2026-11-18. Yazan: `ida-uygulayici` (Opus, `high`). Girdiler: mimar kararı (`docs/gorevler/W0-MP4a-kapsam-daraltma.md`, mimarın izniyle uygulayıcı kaydetti) ve `docs/olcumler/inceleme/MP4-sol-3.md` (değiştirilmedi). Önceki yanıtlar (`MP4-yanit.md`, `MP4-yanit-2.md`) yeniden yazılmadı. Yeni kapsamın kapanışını mimar sol'a doğrulatacak; bu dosya kabul kararı değildir.

Değişen dosyalar: `tools/olcum/mp4/pc/mp4_seri.cpp`, `tools/olcum/mp4/test/analiz_test.py`, `docs/olcumler/MP4-dongu-yuku.md`, yeni `docs/gorevler/W0-MP4a-kapsam-daraltma.md` ve bu dosya. `kos.sh` bu turda değişmedi (taşma reddi çözümleyicinin içindedir). Firmware, adım kodu, imajlar ve ham veri değişmedi; karta erişilmedi.

## Y5 (kritik) — kapsam daraltmasıyla kapatma önerisi

**Mimar kararı:** Y5'in genel taşma tutarlılık formülü (`(n−1)·min + max ≤ top ≤ min + (n−1)·max`) **geliştirilmedi**. Bunun yerine çözümleyici histogram taşması içeren kampanyaları desteklemez (fail-closed).

**Değişiklik** (`mp4_seri.cpp`, faz denetimi):
- Beklenen herhangi bir fazda `F.tasan ≠ 0` → `KAPSAM DIŞI: adım süresi histogram taşması var`; `G.lat_tasan ≠ 0` → `KAPSAM DIŞI: gecikme histogramı taşması var`; `max ≥ 20 ms aralığı` (taşan 0 bildirilse de) → `KAPSAM DIŞI`. Üçünde de kampanya geçersiz: çıkış 1, sayısal özet basılmaz, histogram dosyası yazılmaz (ikinci turdaki Y4 akışı).
- Önceki turun tümü-taşmış kayıt yolu (boş H ile `tasan = n` kabulü, taşan örneklerin toplam sınırına katılması) kaldırıldı. Artık adım süresi histogramı boş olamaz, toplamı tam olarak `n`'dir; en küçük ve en büyük değer ilk ve son kutudadır; toplam süre yalnız histogram kutularının sınırları içinde olmalıdır. Gecikme histogramının toplamı tam olarak `n`'dir.
- 10 µs CSV'sindeki taşma satırı kaldırıldı. Yüzdelik işlevlerindeki "belirlenemedi" dönüşü, toplam = n zorunlu olduğundan artık erişilemez bir savunma dalıdır.

**Testler** (`python3 -I tools/olcum/mp4/test/analiz_test.py`): **103 geçti, 0 başarısız.**
- Kaldırılan başarı testleri (her biri ret testine dönüştü): "kısmi taşma, veri geçerli, p99,9 belirlenemedi"; "bütün örnekler taşmış ama tutarlı → geçerli"; "sınırdaki tek taşan örnek → geçerli".
- Yeni/dönüştürülmüş ret testleri (`KAPSAM DIŞI` ve sayısal özet yok): kısmi taşma (5 örnek); tümü taşmış ve tutarlı; Y1'in eski karşı örneği; tümü taşmış ve toplam küçük; tam aralık üstünde tek taşan; aralığın son çevriminde en büyük ama `tasan=1`; **Y5'in dört toplamı** (`795300000000`, `964000000000` ve sınır toplamları `795300700000`, `963999300000`; n = tasan = 241.000, sol'un beklentileriyle: 400 Hz, maske 7, 241.000, en az 240.000 / 600 s, yığın sınırı 1.586.416); gecikme histogramı taşması (`lat_tasan=1`); `tasan=0` ama en büyük = 3.200.000 çevrim.
- Korunan destek: taşmasız ve en büyük değer aralığın son kutusunda olan kampanya geçerli; en büyük = 3.199.999 çevrim (aralığın son çevrimi) geçerli; taşmasız olağan kampanya geçerli.
- Y4 dosya sistemi testine eklendi: taşmalı kampanyada hiç histogram dosyası oluşmuyor.
- Önceki turların bütün diğer testleri (ilk tur, Y2–Y4, gerçek O2 kopyası üzerindeki bozmalar, PTY kaydı, D3 denetleyicisi) değişmeden geçti.

**Gerçek kayıtlar** (yeni dar kapsamla, ayrı çıktı `<kayıt>-analiz-v4-kapsam-20261118.txt`, histogramlar `<kayıt>-hist-v4-kapsam-20261118-*`; komutlar her çıktının başında):

| Kayıt | Taşma (`tasan`, `lat_tasan`) | v4-kapsam sonucu |
|---|---|---|
| `o2-20261117-190043` | 0, 0 (3 faz) | GEÇERLİ; sayısal satırlar v3 ile birebir aynı |
| `og-20261118-005058` | 0, 0 (3 faz) | GEÇERLİ; aynı |
| `o2-200-20261118-013802` | 0, 0 | GEÇERLİ; aynı |
| `og-200-20261118-014813` | 0, 0 | GEÇERLİ; aynı |
| `kisa-20261117-185942` | 0, 0 (3 faz) | GEÇERLİ (kabul kampanyası değil); aynı |
| `og-20261117-193512` | — | GEÇERSİZ (fazlar ve T yok); histogram dosyası oluşmadı |

Toplam 33 v4-kapsam histogram dosyası yazıldı. Karşılaştırma: v3 ve v4-kapsam çıktılarının `=== faz`, `adım süresi`, `kaçırılan`, `tetik gecikmesi`, `önceki adım`, `parça`, `S`, `sağlama`, `yığın` satırları `diff` ile; beş geçerli kayıtta fark yok. Taşma değerleri ham CSV'lerdeki F ve G satırlarından ayrıca sayıldı (hepsi 0). 2026-11-16 kayıtları önceki turlardaki gerekçeyle (eski biçim, imaj ELF'i yok) yeniden koşulmadı; geçersizlikleri değişmez.

## Y6 (önemli) — FNV eşleşmesi bit eşitliği değildir

**Değişiklik** (rapor §4.5): "bit düzeyinde aynı çıktıyı üretti" cümlesi "her fazdaki 32 bit FNV-1a sağlama toplamları eşleşti; bu tekil çıktıların bit eşitliğinin kanıtı değildir (tekil çıktı baytları kayıtta yok; 32 bitlik eşleşme farklı dizilerle de olabilir, dışlanmadı); bit eşitliği doğrulanmadı" olarak değiştirildi. Bölüm başlığı "belirlenimciliğin kanıtı" yerine "tutarlılık göstergesi" diyor; 200 Hz paragrafına da "bit eşitliği doğrulanmadı" eklendi. Önceki yanıt dosyalarında bu iddia yoktu.

## Y7 (önemli) — 400 Hz ve 200 Hz TIM2 periyotları ayrı

**Değişiklik** (rapor §1 ve §4.1): ham kayıtların açılış K satırlarından:

| Koşu | Beklenen (çevrim) | Ölçülen (çevrim) |
|---|---:|---:|
| -O2, 400 Hz | 400.000 | 399.998 |
| -Og, 400 Hz | 400.000 | 400.000 |
| Kısa, 400 Hz | 400.000 | 399.998 |
| -O2-200, 200 Hz | 800.000 | 799.998 |
| -Og-200, 200 Hz | 800.000 | 800.001 |

Komut: `LC_ALL=C grep -ao 'tim2_periyot_cyc=[0-9]* beklenen=[0-9]*' ${IDA_CACHE}/mp4/kos/<kayıt>.csv`. Çözümleyici ±16 çevrim kabul eder; değerler zaten doğru kabul ediliyordu, hata yalnız rapordaydı.

## Çalıştırılmayan ya da doğrulanmayanlar

- Yeni kart koşusu, yükleme ve `kos.sh` ile uçtan uca koşu yapılmadı (görev gereği). `kos.sh` bu turda değişmedi.
- Taşmalı bir **gerçek** kayıt yok; kapsam reddi yalnız yapay ve Y5 karşı örnekleriyle sınandı.
- Kapsam daraltmasıyla taşmalı kampanyalar analiz edilemez. İleride bir ölçüm 20 ms'yi aşarsa bu çözümleyici sonucu reddeder; bu durumda histogram aralığı firmware'de büyütülüp yeniden ölçülmeli ya da çözümleyici kapsamı mimarca yeniden ele alınmalıdır (öneri, karar değil).
- Önceki yanıtlardaki diğer doğrulanmayanlar (kesme → adım başı gecikme yüzdelikleri, aynı imajın iki uzun koşusu, mutlak saat, `yigin_kayit` çerçeve payı) geçerliliğini korur.
