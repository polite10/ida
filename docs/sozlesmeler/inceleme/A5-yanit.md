# A5 sözleşmesi — sol incelemesine yanıt (1. düzeltme turu)

İnceleme: `docs/sozlesmeler/inceleme/A5-sol.md`. Düzeltmeler mimarın W1-A5 düzeltme turu kararlarına göre yapıldı. Değişen dosyalar: `docs/sozlesmeler/A5-senaryo.md`, `scenarios/sema/a5.schema.json`, `scenarios/ida0.toml`, `scenarios/ornek-havuz.toml`; yeni dosya `scenarios/ornek-havuz.cozumlenmis.json`.

Kural numaraları sözleşmenin bu sürümüne göredir: 106 kural; A5-K96…K112 yeni.

| # | Önem | Durum | Yer | Gerekçe |
|---|---|---|---|---|
| 1 | kritik | giderildi | A5-K109, K62, K63, K64; Tablo 3.10-A; şema `olay` koşulları | Olaylar üç çeşide ayrıldı: kalıcı atama, anlık komut, geçici arıza. Emniyet girdisi olayı yalnız kalıcı atamadır: `sure_ns` ve `kosul` taşıyamaz, değer bir sonraki atamaya kadar kalır. Geçici arıza bitince kalkar. Aynı sınırda önce bitişler, sonra başlangıçlar dosya sırasıyla uygulanır. Aynı hedefe aynı sınırda çelişen iki atama doğrulama hatasıdır; koşullu atamada çalışma anında sonraki geçerli olur ve A6'ya yazılır |
| 2 | kritik | giderildi | A5-K30 | Uzunluk eşitliği yalnız dört gerilim tablosu arasında ve iki havalanma tablosu arasında aranıyor; örnek geçiyor |
| 3 | kritik | giderildi | A5-K45; `scenarios/ornek-havuz.toml` | Orijin şu durumlarda zorunlu: etkin GNSS, etkin manyetometre ya da dünya dönüşü açık etkin IMU. Örneğe mimarın konumu (41,0° K, 29,0° D, 0 m; radyan) ve `B_n` eklendi. Manyetometre etkin, GNSS kapalı. "Varsayılan konum" şema varsayılanı olarak değil örnek değer olarak işlendi; böylece zorunluluk kuralı anlamını korur |
| 4 | kritik | giderildi | A5-K16, K98, K99, K100, K101; 3.6 profil tablosu; `scenarios/ornek-havuz.cozumlenmis.json` | Çözümleme sırası sabitlendi. Koşullu varsayılanlar bir tabloyla verildi; HIL'de SIL alanları yazılmıyor. Eksik tablolar için üretilecek nesneler açıkça yazıldı; L0 nesnesi somut. Profildeki "—" değerler kaldırıldı: her biri sayı, KE (kalibre edilmedi; ideal değer, `meta.kalibre_edilmedi` listesine girer), DY ya da DS. `arac.dosya` çözümlenmiş biçimden çıkarılıyor. Altın örnek, kanonik baytlarıyla eklendi |
| 5 | kritik | giderildi | şema `$defs/cozumlenmis_senaryo`, `*_cozum` tanımları; A5-K07 | Çözümlenmiş biçimin zorunlulukları şemaya işlendi: koşulsuz varsayılanı olan her anahtar zorunlu, koşullu olanlar `if/then` ile; `otopilot` ve çözümleyici üst verisi zorunlu. `x_` anahtarı her tabloda kabul ediliyor; anahtarlı koleksiyonda girdi sayılmıyor (şemada `patternProperties`); kullanıcı adları `x_` ile başlayamaz |
| 6 | kritik | giderildi | A5-K17, K100; 3.14 | Davranış özeti kaynak baytlarından bağımsız: kaynak özetleri çözümlenmiş nesnede yok, raporda ve A6'da ayrı üst veri. Uygunluk testi buna göre yazıldı. Sınandı: anahtar sırası, satır içi tablo, sayı yazımı (`3`/`3.0`, `0.30`/`3e-1`, `-0.0`, `0x1`) değiştirilmiş eşdeğer dosya aynı özeti verdi (`209f758f…`); olay sırası değiştirilince özet değişti. Olay sırası anlamlıdır (A5-K62) |
| 7 | kritik | giderildi | A5-K96 (sınır vektörleriyle), A5-K97 | Ondalık → binary64 en yakına, eşitlikte çifte; taşma, alt taşma, `inf`/`nan` ret; alt normal kabul; −0 → +0. `x_` değerlerinde tarih ve saat türleri reddediliyor. Vektörler başvuru uygulamasıyla hesaplandı; JCS sayı dizilimi RFC 8785 örnekleriyle (`1e+21`, `1e-7`, `0.000001`) uyumlu |
| 8 | kritik | giderildi | A5-K83, K84, K110, K111, K112; Ek D | Philox4x32-10, anahtar sözcük yerleşimi, sayaç düzeni (64 bit sıra + kanal/bileşen/çekiliş + ayrılmış sözcük), dönüşüm formülleri ve işlem sırası, bilinen yanıt vektörleri. Random123'ün yayımlanmış üç vektörünü hem Python başvuru uygulaması hem MP7 prototipinin `rng.hpp`'si (derlenerek) bit bit üretti. A5 anahtar vektörlerini MP7 çekirdeği aynı platformda aynı üretti. `fizik/dalga` anahtar özeti sol'unkiyle aynı. Normal dönüşümün platformlar arası eşitliği garanti edilmiyor (sınır olarak yazıldı); doğrulanmadı: Windows'ta aynı vektörler |
| 9 | kritik | giderildi | Tablo 3.10-A; A5-K68, K69, K102; 9. başlık AB-093, AB-094 | Desteklenen her tür için zorunlu alanlar ve kesin etki yazıldı: `yanlis_gecerlilik` biti tersine çevirir; `kesme` düzgün çekilen uzunluğa keser; `bayt_bozma` tek bayt XOR'lar. Tanımlanamayanlar "bu sürümde desteklenmiyor" ile reddediliyor: `nan`, `aralik_disi`, `kismi`, `tutukluk`, `dolanma`, `ani_cokme`, `dusuk_batarya`, `gecikme_sicramasi`, `cop_bayt`. AB-093 ve AB-094 "açık"a çekildi |
| 10 | kritik | giderildi | A5-K23, K24; Ek A trimli vektörler | Trim hacimsiz nokta kütledir; boş duruma torpidolardan önce eklenir; kütleye, kütle merkezine ve atalete katılır. 0,5 kg / (0,10; 0; 0,05) m için üç durumun vektörü eklendi |
| 11 | kritik | giderildi | A5-K104, K105 | Polinom: 1001 noktada bölen sonlu ve pozitif, `F > 0`. Transfer fonksiyonu: önde gelen ve son payda katsayısı sıfırdan farklı, kesin uygun, DC kazanç 1 (1e-12), Routh–Hurwitz kararlı. Bozuk örnekte ikisi de yakalandı |
| 12 | kritik | giderildi | A5-K79; `meta.olcum_kanitlari` | Kartlı kipte ışın sayısı yalnız {20 000, 40 000, 80 000, 160 000}; başka değer ret. Çözümlenmiş senaryo ve rapor MP7 §4.6'ya atıf taşıyor. SIL'de 1…10⁷ serbest, ölçülmemiş değer uyarı veriyor. Ölçüm sınırı olarak yazıldı. Not: MP7'nin gerçekleşen ışın sayıları 40 014 ve 160 008; adlarla eşleme açık kalanlarda (md. 13) |
| 13 | önemli | giderildi | A5-K108; 9. başlık AB-089 | AB-089 "uygulamaya devredildi" yapıldı. Doğrulanmamış soğurma kullanımı `meta.dogrulanmamis_varsayimlar` listesine giriyor ve raporda zorunlu uyarı üretiyor (örnek senaryoda iki uyarı) |

**Ek kararlar (mimar):**

| Konu | Durum | Yer |
|---|---|---|
| Kamera / görünüm gerekçesi | işlendi | A5-K80: kareleri görselleştirici süreci üretir |
| Ping360 örnek üst sınırı iki kademede 1200 | işlendi | A5-K78; Ek C A1 md. 2 (eski 600/1200 çelişkisi kapandı) |
| Montajlar, 15,6 V, havuz 25 × 12,5 × 3,0 m, JONSWAP `gamma` 3,3, cos-2s s = 10, sızıntı 2 prob | işlendi, [MV] etiketiyle | 3.6, 3.9, `ida0.toml`, `ornek-havuz.toml`. Montajların yönelimini tanımlamak için sensör çerçevesi kabulleri yazıldı (A5-K89); böylece bütün yönelimler birim dönüş |
| Sigorta ve telsiz anteni modeli yok | işlendi | Anahtarlar şemada kaldı; verilirse A5-K102 "desteklenmiyor". Telsiz marj tablosu da aynı. `fiziksel` ve `kablo_uzunlugu` kaldırıldı (modelleri yok) |
| Değeri olmayan kalibrasyon parametreleri ideal, "kalibre edilmedi" | işlendi | A5-K101; `meta.kalibre_edilmedi` |
| Onaylanan seçimler (sabit torpido ve fırlatıcı, atış komutu yok, 2⁵³ sınırı, efemeris 2 saat) | işlendi | A5-K29, K73, K10, 3.6 |

**Mimara bildirilen çelişki:** Kararlarda `O_b` "çerçevenin geometrik merkezi" olarak verildi; 04 §2.6 `O_b`'yi "boş aracın ağırlık merkezi" diye tanımlıyor ve `r_g = 0` alıyor. Sözleşme iki tanımın İDA-0'da çakıştığını mimari varsayım olarak yazdı (2. başlık; 8. başlık md. 15). Ağırlık merkezi geometrik merkezde değilse `arac.kutle.r_g` sıfırdan farklı yazılmalıdır.

**Doğrulama (bu turda yapıldı; betikler depo dışında):**
- Şema JSON Schema 2020-12 metaşemasına uygun (jsonschema 4.23.0).
- `ida0.toml` tek başına şemaya uyuyor.
- `ornek-havuz.toml` geçerli; yalnız iki K108 uyarısı var.
- Altın dosya `$defs/cozumlenmis_senaryo`'ya uyuyor; yeniden üretilen kanonik baytlarla bayt bayt aynı. SHA-256: `209f758fad29d6fe15a9087cfc2357b50f1b468f8e8254b1df8238e28cebf1d2`.
- Bozuk dosyalar beklenen kurallarla yakalandı:
  - b1: K05, K06
  - b2: K42, K66, K79, K80
  - b3: K09, K10, K29
  - b4: K15
  - b5: K109
  - b6: K45, K62, K68, K79, K102 (×3), K104, K105; ayrıca K07 ve K71 uyarıları
  - b7: K96 (×2), K97
- Doğrulanmayanlar:
  - A7 ve A4'e bağlı kurallar (K18, K19, K36, K51 türetimi, K52).
  - Satır numaraları: `tomllib` konum vermiyor.
  - Ek D'nin normal değerlerinin Windows'ta tekrarı.
  - Doğrulayıcının C++ uygulaması.
