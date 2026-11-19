# A3 + A6 — ikinci (son) düzeltme turu yanıtı

Tarih: 2026-11-20. İnceleme: `docs/sozlesmeler/inceleme/A3A6-sol-2.md`. sol, ilk incelemenin 11 maddesinden 6'sını giderilmiş, 5'ini kısmen giderilmiş buldu ve 7 kritik ile 3 önemli yeni bulgu verdi. Düzeltmeler mimarın 2026-11-20 tarihli ikinci tur kararlarına (1–10) göre yapıldı. Birinci tur `17f9994` ile commit'lenmişti.

Belgeler ve şemalar tek bir üreteçten yeniden üretildi; bu yüzden birbirleriyle tutarlıdır. Değişen dosyalar:
- `docs/sozlesmeler/A3-web.md`
- `docs/sozlesmeler/A6-kayit.md`
- `docs/sozlesmeler/sema/a3-kanallar.json`
- `docs/sozlesmeler/sema/a6-kanallar.json`

Kural numaraları korundu. Yeni kurallar:
- A3-K122 (durum revizyonu), A3-K123 (atomik çalıştırma kontrolü), A3-K124 (güncel `run_id` ve `null` kuralı).
- Yeni tablo: A3 Tablo 4.7a′.

Kural sayısı A3'te 124, A6'da 60. A6'ya yeni kural eklenmedi; K37, K45, K46, K54 ve K55 yeniden yazıldı.

## Bulgu tablosu

| # | Önem | Durum | Yer | Gerekçe |
|---|---|---|---|---|
| Y1 (ilk 1, 2 kısmen) | kritik | giderildi | A3-K92, K94, K95, K117 (yeniden yazıldı); §7 K89–K99 satırı; A3 §9 AB-057; A3 §10 A1 md.9; `yuk_emniyet_girdiler.emniyet_rev` açıklaması | `emniyet_rev` artık uygulanan **her kısıtlayan komutta** artıyor, yetkili durum zaten kısıtlı olsa bile. Her fiilî gevşetmede ve listedeki öteki olaylarda da artıyor. Böylece sonradan kabul edilen her kısıtlama önceki bütün gevşetme jetonlarını ve bekleyen gevşetme isteklerini geçersizleştiriyor. İncelemedeki sıra (rev 7'de "pimi çıkar" jetonu → "pimi tak" N'de → eski çıkarma N+1'de) 409 `emniyet_rev_uyusmuyor` ile sonuçlanıyor. Bu sıra uygunluk tablosuna ve aşağıdaki modele girdi. A1'deki `safety_rev` aynı sayaçtır ve artış kuralı dünyanındır (A1'den beklenenlere yazıldı). |
| Y2 (ilk 1 kısmen) | kritik | giderildi | A3-K54, K61 (geçiş cümlesi), K67, K122, K123 (yeni); `calistirma/durum.durum_rev`, `$defs/durum_rev`, `yanit_durum.durum_rev`; bütün çalıştırma kontrolü istek şemalarında zorunlu `beklenen_durum_rev`; hata kodu `durum_rev_uyusmuyor`; §7 K50–K60 ve K122–K124 satırları | Dünya tarafında ikinci bir tekdüze sayaç (`durum_rev`) tutuluyor; iki sayaç da sim sürecinin ömrü boyunca artar, yeni çalıştırmada sıfırlanmaz. Çalıştırma kontrolü komutları (`senaryo/yukle`, `calistirma/*`, `izleme/*`) zorunlu `beklenen_durum_rev` taşır. Revizyon, `beklenen_run_id`, durum ve kip önkoşulları geçişle aynı atomik adımda yeniden denetlenir. Aynı revizyonla yarışan komutlardan yalnız ilki uygulanır, ötekiler 409 ve güncel revizyonu alır; iki istemcinin iki `sifirla`'sı da buna dahil. §7'deki "yalnız biri uygulanır" kabulü A3-K123'e bağlandı. |
| Y3 (ilk 9 kısmen) | kritik | giderildi | A3-K121 (yeniden yazıldı); Tablo 3.5b (yeniden hesaplandı, üç sütun); olay şemaları ve `$defs/sinirli_nesne` | (a) Olay ayrıntısındaki her tamsayıda `minimum`/`maximum` var ve sınırlar ±(2⁵³ − 1) içinde; `sinirli_nesne` değerleri de dahil. (b) Serileştirme politikası sabit: ASCII dışı karakterler ham UTF-8 yazılır, `\u` kaçışı yok; serbest dizeler `^[^\u0000-\u001f]*$` desenini taşır. (c) Tablo yeniden hesaplandı: kod noktası başına en çok 4 bayt, `"` ve `\` için 2 bayt. En büyük olay `komut_sonucu`, 4966 bayt ≤ 8192; alan sınırlarını daraltmak gerekmedi. (d) Denetim betiği her olay türü için şemaya uyan en kötü örneği gerçekten üretip boyunu ölçüyor; 23 türün hepsinde ölçülen boy hesapla aynı. Şemada sınırsız bir sayısal alan, dize, dizi ya da nesne kalırsa betik başarısız oluyor; bu, üst sınırı silinmiş bir kopyayla sınandı. |
| Y4 (ilk 7 kısmen) | kritik | giderildi | A3-K52 (yeniden yazıldı); `yuk_a3_komut.govde` açıklaması; §7 K50–K60 satırı | `a3/komut` kanalına yalnız uç şemasından geçen istekler yazılıyor. Geçerli JSON olup şemaya uymayan istekler (ör. 65 karakterlik ya da nesne tipinde `istek_kimligi`) `a3/gecersiz_istek`'e ham ön ek, uzunluklar ve ret nedeniyle gidiyor. |
| Y5 (ilk 4 kısmen) | kritik | giderildi | A6-K54, K55 (yeniden yazıldı); Tablo 4.6 (c); `yuk_cekirdek_baslatma` (`cagri_sirasi`, `init_kusagi`), `yuk_cekirdek_cagri` (`init_kusagi`); §7 K54–K55 satırı | `cagri_sirasi` çalıştırma genelinde tekdüze; init çağrılarını da sayıyor ve init'te sıfırlanmıyor. Ayrıca açık bir `init_kusagi` alanı var. Tekrar iki kanalı `cagri_sirasi` sırasıyla birlikte yürütüyor: init kaydı çekirdeği yeniden başlatıyor ve init dönüş kodu da karşılaştırılıyor. Sırada boşluk, kuşak uyuşmazlığı ya da ilk fark tekrarı durduruyor. |
| Y6 | kritik | giderildi | A6-K37, A3-K60 (yeniden yazıldı); `yuk_a3_komut_sonucu` (`kaynak_run_id`, `kaynak_komut_no`); `$defs/olay_komut_sonucu`; §7 K37 satırı | Tekrarda uygulanan komutlar güncel süreçten yeni `komut_no` alıyor; kaynak kimliği ayrı alanlarda: `(kaynak_run_id, kaynak_komut_no)`. `komut_no` ile sorgu yalnız güncel sürecin numaralarını kapsıyor. |
| Y7 | kritik | giderildi | A3-K54, K63, K67, K124 (yeni); Tablo 4.7a′ (yeni); `$defs/run_id_ya_da_null`; bütün `beklenen_run_id` alanları | Çalıştırma yokken `beklenen_run_id` `null` olmak zorunda; çalıştırma varken güncel kimliğe eşit olmak zorunda. Şema ve Tablo 4.7a′ aynı kabul kümesini taşıyor. `yuklendi` durumunda hiç çalıştırma başlamamışken `sifirla` `null` ile geçerli. |
| Y8 | önemli | giderildi | A6 §3.5, K46 (yeniden yazıldı); `$defs/ust_veri_ida_kayit` (açılış), `$defs/ust_veri_ida_kayit_guncelleme` (yeni; kapalı liste: `kart_yazilimi_ozeti`, `a2_surumu`, `a8_surumu`); `x_ida_ust_veri`; §7 K46 satırı | Açılış ve kısmi güncelleme kayıtları ayrı şemalarla tanımlandı; şemalar fiziksel kayıtları doğruluyor. Birleştirme kuralı: güncelleme yalnız boş ya da olmayan anahtarı doldurur; dolu bir anahtarı farklı değerle yazan güncelleme ve ikinci bir açılış kaydı dosyayı `bozuk` yapar. |
| Y9 | önemli | giderildi | Tablo 3.3 (üretildi); `tekrar_duzeyi`; A6 §9 U-28 | A1 bayt kanalları (`a1/sim_kart`, `a1/kart_sim`), `hil/enjeksiyon`, `hil/oturum` ve `hil/zaman_esleme` `yeniden_kurma` düzeyine alındı; `girdi` yalnız `cekirdek/*` (ve `kayit/eksik`) için. Denetim betiği bunu denetliyor. |
| Y10 | önemli | giderildi | A6-K31, K45; §3.5 kapanış tablosu; katalog | `bitis_durumu` izinli değerlerine `yazma_hatasi` eklendi; dört yer aynı listeyi taşıyor. |

## Uygulama yorumları ve çapraz uyum notları

1. **`durum_rev`'in arttığı anlar.** Karar "her geçişte artar" diyor. Aynı revizyonla yarışan komutlardan yalnız ilkinin uygulanması için, durumu değiştirmeyen komutlar (`hiz`, `izleme/ara`, `yuklendi`'de `sifirla`, `senaryo/yukle`) da geçiş sayıldı. Uygulanan her çalıştırma kontrolü komutu ve her iç geçiş sayacı bir artırır (A3-K122). `izleme/*` da çalıştırma kontrolü komutu sayıldı.
2. **`beklenen_durum` ve `beklenen_senaryo_ozeti` kaldırıldı.** `durum_rev` bu iki alanın işlevini kapsıyor: `senaryo/yukle` de revizyonu artırıyor.
3. **Kartın `SAFETY_OPEN`'ı sayacı artırmaz (A3-K117).** Kararın "her fiilî gevşetmede artar" ifadesi A3'ten gelen gevşetmeler olarak uygulandı: sanal girdinin azaltılması ve izni genişleten yer istasyonu komutunun emülatöre verilmesi. A1'in kendi kısıtlayan olayları (süresi dolan iznin kapanması, `SAFETY_CLOSE`; A1-K86, K87) sayacı artırır. Kartın izin açması ya da yenilemesi artırsaydı, A1-K86 güncel `safety_rev` damgası istediği için kart her izin yenilemesinde kendi damgasını geçersizleştirirdi. **Mimar başka türlü karar verirse A1-K86'nın yenileme düzeni de değişmelidir.**
4. **A1-K85 ile A3-K95 arasında sıra farkı (çapraz uyum incelemesine).** A1-K85 adımın başında bütün sanal girdi değişikliklerini uygular. A3-K95 ise gevşeten sanal girdi değişikliğini o adımın tek seferlik olaylarından sonraya koyar. A3-K95 daha tutucudur; iki sözleşmenin aynı sırayı yazması gerekir. A3 §8 md.12 ve A1'den beklenenler md.9'a yazıldı.
5. **Gevşetme ile sıfırlama.** `sifirla` ve yeni çalıştırma `emniyet_rev`'i artırır ve güncel `run_id`'yi değiştirir; eski çalıştırmanın jetonları ve bekleyen gevşetmeleri iki ayrı denetimde reddedilir.

## Yürütülebilir model (A3-K92…K95, K117…K119, K94, K122…K124)

Model Python'la yazıldı ve scratchpad'de duruyor; depoya girmez. Kuralları ürün kodundan bağımsız olarak doğrudan modelliyor ve bütün kısa geliş sıralarını tek tek numaralandırıyor.

- **Emniyet modeli.** Alfabe:
  - `J`: "pimi çıkar" için jeton al.
  - `G`: son kullanılmamış jetonla ve onun revizyonuyla gevşetme gönder (eşzamanlı denetim; tek kullanımlık jeton).
  - `K`: "pimi tak".
  - `S`: `sifirla` ve ardından yeni çalıştırma.
  - `B`: adım sınırı. Sınırda sıra: çalıştırma kontrolü → kısıtlayanlar (her biri revizyonu artırır) → gevşetenler (aynı sınırda kısıtlama varsa `kisitlama_hakim`; revizyon tutmazsa `emniyet_rev_uyusmuyor`).

  Uzunluğu 1–8 olan **488 280** dizi tarandı. Değişmezler:
  - (D1) Uygulanan hiçbir gevşetmenin jetonundan sonra kabul edilmiş ve gevşetmeden önce ya da aynı sınırda uygulanmış bir kısıtlama yoktur.
  - (D2) Kısıtlama uygulanan her sınırın sonunda pim takılıdır.

  Sonuç: **hiçbir ihlal yok**.
- **Özel sıralar.**
  - İncelemedeki ardışık sınır sırası (jeton → "pimi tak" N'de → eski çıkarma N'den sonra): 409, pim takılı kalır.
  - Aynı sınır (çıkarma isteği önce kuyruğa girer, "pimi tak" sonra): `kisitlama_hakim`, pim takılı kalır.
  - İki sınır sonra gönderilen eski jetonlu çıkarma: 409.
- **Durum modeli.** Alfabe: iki istemcinin gözlemi (`GET /durum`); her istemci için `baslat`, `duraklat`, `surdur`, `durdur`, `sifirla`; adım sınırı. Kurallar: eşzamanlı denetim ve sınırda atomik yeniden denetim; Tablo 4.7a'nın alt kümesi.

  Uzunluğu 1–6 olan **5 229 042** dizi tarandı. Değişmezler:
  - (R1) Aynı `beklenen_durum_rev` ile hiçbir iki komut uygulanmaz.
  - (R3) Güncel `run_id` `null` ancak ve ancak `yuklendi`.

  Sonuç: **hiçbir ihlal yok**.
- **Özel sıralar.**
  - Aynı sınırda iki `sifirla`: biri uygulandı, öteki `durum_rev_uyusmuyor`.
  - Ardışık sınırlarda iki `sifirla`: ikincisi eşzamanlı denetimde 409.
  - Çalıştırma yokken `null` `run_id` ile `sifirla`: kabul edildi.
- **Modelin duyarlılığı (mutasyon).** Modelin hataları gerçekten yakaladığını göstermek için kurallar bilerek eskiye döndürüldü:
  - Birinci tur kuralı geri kondu ("kısıtlama yalnız durumu değiştirirse revizyonu artırır"): uzunluk 1–7'de **122** dizi D1'i bozuyor.
  - `durum_rev` denetimi kaldırıldı: uzunluk 1–5'te **9 640** dizi R1'i bozuyor.

  İkisi de geçerli kurallarla **0**'dır.
- **Modelin kapsamadıkları.** 10 s'lik jeton süresi; yer istasyonu komutları (aynı kurallar, `kisitlama_hakim` daha geniş); duraklatılmış durumda hemen uygulama (sınırın hemen gelmesine eşdeğer); birden çok sanal girdi.

## sol'un karşı örneklerinin yeniden hesabı

Bütün değerler boşluksuz JSON üzerinden hesaplandı.

- **16 × 10⁶⁰⁰ tamsayı.** Yeni şemada geçersiz: `sinirli_nesne` değerleri en çok ±(2⁵³ − 1). Bu örnek (yalnız `komut_no`, `sonuc` ve 16 değerli `sonuc_ayrinti` ile) eski şemada 10 254 bayt tutardı; sol bütün alanlarla 10 388 bayt hesaplamıştı. Betik artık sınırsız tamsayı görünce başarısız oluyor.
- **16 × 64 U+1F600 değer.**
  - Ham UTF-8 ile (yeni politika): `sonuc_ayrinti` tek başına 4 705 bayt; yalnız `komut_no` ve `sonuc` ile birlikte 4 766 bayt.
  - Bütün alanlar en kötü değerleriyle doluyken `komut_sonucu`: **4 966 bayt** (Tablo 3.5b). sol'un 4 900 baytlık değeri daha az alan içeren bir örnekti.
  - Aynı nesne `\u` kaçışıyla 12 897 / 12 958 bayt tutardı; bu kaçış artık üretilmiyor (A3-K121).

## Yeniden doğrulama (2026-11-20)

- **JSON:** iki dosya da geçerli UTF-8 ve geçerli JSON.
- **`$ref` ve desenler:** bütün yerel `$ref`'ler çözüldü, `pattern`'ler derlendi. A3'te 88, A6'da 3 `$defs` tanımı var.
- **Sayılar:** A3'te 24 kanal, 23 yük, 23 olay türü, 46 uç, 29 hata kodu; A6'da 54 kanal, 36 yük.
- **A3 ↔ A6:** 20 ortak kanal ve 19 ortak yük derin eşit.
- **Olay ayrıntısı:** 23 olay türünde üretilen en kötü örnek hesapla aynı ve ≤ 8192 bayt.
- **Belge ↔ şema:** belgelerde geçen her kanal, yük, `$defs` adı ve hata kodu şemada var; şemadaki 58 kanalın ve 40 yükün her biri belgelerde geçiyor.
- **Tablolar:** iki belgedeki bütün tabloların sütun sayıları tutarlı.
- **Betiğin kendi sınaması:** bilerek bozulmuş bir kopyada betik bütün bozulmaları yakaladı (5 hata iletisi). Bozulmalar: ortak yük alanının tipi değiştirildi, `$ref` kırıldı, `kanal_id` yinelendi, sınırlı nesnenin tamsayı üst sınırı silindi.

**Doğrulanmadı:**
- JSON Schema 2020-12 meta-şemasına karşı resmî doğrulayıcıyla denetim (kurulu doğrulayıcı yok).
- A4 alanlarının gerçek kodlanmış boyları (A6 §8 md.9).
