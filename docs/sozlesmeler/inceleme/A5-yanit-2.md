# A5 sözleşmesi — sol ikinci incelemesine yanıt (2. ve son düzeltme turu)

İnceleme: `docs/sozlesmeler/inceleme/A5-sol-2.md`. Düzeltmeler mimarın 2. tur kararlarına göre yapıldı; kural numaraları korundu (106 kural, yeni numara yok). Değişen dosyalar: `docs/sozlesmeler/A5-senaryo.md`, `scenarios/sema/a5.schema.json`. `scenarios/ida0.toml`, `scenarios/ornek-havuz.toml` ve `scenarios/ornek-havuz.cozumlenmis.json` değişmedi; davranış özeti aynı kaldı: `209f758fad29d6fe15a9087cfc2357b50f1b468f8e8254b1df8238e28cebf1d2`.

## İlk incelemenin "kısmen" maddeleri ve yeni bulgular

| # | Önem | Durum | Yer | Gerekçe |
|---|---|---|---|---|
| 1 (K63) | kritik | giderildi | A5-K63, A5-K109, A5-K62; 5. başlık; 7. başlık | **Mimar kararı (2. tur).** "Çelişen eşzamanlı atama hatadır" kuralı yalnız doğrulamada saptanabilen çelişkiler (iki zamanlı atama) içindir; K62'de "zamanlı" diye sınırlandı. Koşullu bir atamanın aynı sınırda başka bir atamayla (koşullu ya da zamanlı) çelişmesi doğrulamada bilinemez. Bu durumda emniyet alanlarında kısıtlayan değer uygulanır (acil durdurma basılı, fiş çıkık, pim takılı; "daraltma hâkim"), diğer alanlarda dosya sırasında sonraki; iki halde de `atama_celiskisi` olayı A6'ya yazılır. K109 bu ayrımla yeniden yazıldı. Buna bağlı olarak 1. turda eklediğim "emniyet girdisi olayında `kosul` yasak" kısıtı kaldırıldı (şema dahil): mimarın kuralı koşullu emniyet atamalarını öngörüyor; emniyeti azaltan koşullu atama da K71 uyarısını üretir |
| 4 / Y1 | kritik | giderildi | 3. başlık (genel kural); 3.6 profil tablosu; A5-K16; **Ek E (yeni)** | Altın dosyadaki değerler geçerli sayıldı. DVL'nin dört açısı tam değerleriyle yazıldı: `0.39269908169872414`, `0.07679448708775051`, `0.7853981633974483`, `1.0471975511965976`. Genel kural eklendi: belgedeki her sayısal varsayılan, kullanılan binary64 değerin en kısa gidiş-dönüş ondalığıdır; varsayılan tablolarında yuvarlatılmış gösterim kullanılmaz. Bütün varsayılanlar makinece okunur TOML bloğu olarak Ek E'ye kondu: eksik tablolar, 57 koşulsuz varsayılan, koşullu varsayılanlar, profiller, KE, DY ve kanıtlar. 3.6 tablosu `kod` değişmezleriyle yeniden yazıldı |
| 8 / Y2 | kritik | giderildi | 3.10 "Bileşim"; A5-K84; Ek D.4, **D.5, D.6 (yeni)** | Her GE süreci başlangıçta "iyi" durumdadır: emülatörde sim `t = 0`, sensör arızasında olayın başladığı sınır. İki sayaç dizisi anahtar türetiminde ayrı alt sistem adı taşır: `…/gecis` ve `…/cerceve`. Geçiş dizisinde sayaç = kalış sırası, `d_i = max(1, ⌈−log(u)·m_i⌉)` ns; her geçiş kendi konumunu tüketir. Çerçeve dizisinde sayaç = çerçeve sırası; w0 kayıp ya da başarı, w1 titreşim; bayt bozma `c2 = 1` sayacından. Bilinen sonuç vektörü D.6'da: ilk üç geçiş zamanı ve ilk beş çerçevenin sonucu (iki "iyi", üç "kötü" çerçeve). Philox sözcükleri MP7'nin derlenmiş üreteciyle de karşılaştırıldı. **Sözleşme seçimi:** sensör arızası GE deseni, 05 §2.7'nin adım başına `1 − exp(−h/T)` biçimi yerine olay zamanlı tanımlandı; ortalama kalış aynıdır. Mimarın "her GE süreci" ifadesiyle iki süreci tek tanımda topladım (8. başlık md. 16) |
| 9 / Y3 | kritik | giderildi | Tablo 3.10-A (`donma`, `donma_zaman_damgasi`); 5. başlık; 7. başlık | Dondurulacak önceki çıktı yoksa sensör arıza bitene kadar hiç kayıt üretmez. Bu durum `donma_onceki_cikti_yok` sayacını artırır ve olay olarak yazılır; başlangıç reddedilmez |

## Kendi denetimim (ek düzeltmeler)

| Yer | Düzeltme |
|---|---|
| Ek D.4 `gec_teslim` | Titreşim kayıt başınadır; sayaç `c2 = 2 << 16` oldu (kanal alanı kaldırıldı). Çok kanallı kayıtta belirsizliği giderir |
| Ek D.4 protokol | `iki_yon` olaylarında iki yönün sayaçları çakışıyordu; `c2 = yön << 24` oldu. Çerçeve sırası "olay başladıktan sonra o yönde `mesaj_turu`'na uyan çerçeveler" diye kesinleşti |
| Ek D.4 hedef yaw | `q_z(ψ)` açık yazıldı |
| Ek D.4 giriş | Kanal aralığı (0–254) ve `0xFF` ayrımı yazıldı; 1. turdaki `0xFE` GE kodu artık kullanılmıyor |
| A5-K16 | Adım sırası Ek E ile eşitlendi: koşulsuz varsayılanlar koşullulardan önce; koşul 4. adımdan sonraki değere bakar. Altın dosya bu sırayla da aynı |

## Doğrulama (betikler depo dışında, yalnız gösterim)

- **Bağımsız yeniden çözümleme (Y1'in istediği sınama).** Varsayılanları yalnız belgenin Ek E bloğundan okuyan, başvuru profillerini, şemayı ve önceki denetim betiğini kullanmayan ayrı bir çözümleyici yazdım. JCS dizilimi de ayrı yazıldı. Sonuçlar:
  - `ornek-havuz.toml`: `209f758f…ebf1d2`, 9 477 bayt; altın dosyayla bayt bayt aynı.
  - Eşdeğer yazılmış ikinci örnek (anahtar sırası, satır içi tablo, sayı yazımı değiştirilmiş): aynı özet ve aynı baytlar.
  - Açık su, JONSWAP, HIL bariyer, tam sonar (40 000), sinüs hedef, GE arızası, L2 GE profili, koşullu emniyet ataması ve DVL `c_ayar` geçersiz kılması içeren üçüncü senaryo: başvuru çözümleyicisiyle aynı özet (`2bedbd2d…87647`). Bu senaryo tabloların çoğunu sınar.
- **Duyarlılık.** Ek E'de yalnız `isin_acisi` `0.3927` yapılınca özet değişti (`c9cd7e0a…`). Betik farkı yakalıyor.
- **Tutarlılık.**
  - Ek E `[kosulsuz]` şemadaki bütün `default` değerleriyle (57 anahtar) bire bir eşit.
  - Ek E `[profil.*]` ve `[kalibre_edilmedi]` başvuru profilleriyle eşit.
  - 3.6 tablosundaki 96 değişmez Ek E ile eşit.
  - Diğer tabloların varsayılan sütunlarını elle karşılaştırdım; fark yok.
- **Örnekler.** Şema metaşemaya uygun. `ida0.toml` şemaya uyuyor; altın dosya `$defs/cozumlenmis_senaryo`'ya uyuyor ve yeniden üretimle bayt bayt aynı.
- **Bozuk örnekler.** Yedi dosya da beklenen kurallarla yakalandı (1. turla aynı sonuç). b5 yine K109 ile reddediliyor; gerekçesi koşul değil `sure_ns`.
- **Doğrulanmadı.**
  - A4/A7'ye bağlı kurallar.
  - Windows'ta Ek D vektörlerinin `log` içeren kısımları.
  - Çalışma anı kuralları; doğrulayıcı ve modeller yazılmadığı için sınanamadı: K63'teki koşullu atama çelişkisi ve Y3'teki donma davranışı.
