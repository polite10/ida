# MP7 — sol incelemesine yanıt (düzeltme turu 1)

İnceleme: `docs/olcumler/inceleme/MP7-sol.md`. Düzeltmeler, görev belgesinin "İlk turdan sonra: düzeltme turu" başlığındaki mimar tanımıyla yapıldı (`docs/gorevler/W0-MP7a.md`). Rapor: `docs/olcumler/MP7-sonar-suresi.md`.

| # | Önem | Durum | Yer | Gerekçe |
|---|---|---|---|---|
| 1 | kritik | Giderildi | `tools/olcum/mp7/kaynak/main.cpp` (`buyukluk`, `komut_yakinsama`); rapor §4.3 | `V/E²` ve geometrik enerji oranı kaldırıldı. Mimarın dondurduğu büyüklükler uygulandı: `δ_E = Σ|Ē_2N − Ē_N| / Σ Ē_2N`, `δ_V` aynı ifade ham varyansla, `δ_C = |C̄_2N − C̄_N|`. Ölçüt: nokta değeri ve bootstrap %95 GA üst ucu ≤ 0,05, bir sonraki ikiye katlamada da. `V/E²` yalnız tanılayıcı. S2 yeniden ölçüldü; ilk turun "biçimsel geçti" hükmü geri çekildi. |
| 2 | kritik | Giderildi | `kaynak/sonar.cpp` (`B_yon`) | Koni dışı eleme genliğin işaretinden ayrıldı: koni içindeki negatif yan lob genlikleri işaretiyle korunuyor, görünürlük sorguları da yapılıyor. Süre ve yakınsama yeniden ölçüldü. |
| 3 | kritik | Giderildi | `kaynak/sonar.cpp` (sade dalı) | Sade kademede kutu enerjisi `Σ w_j`, `w_j = σ_j dA_j G_j` (mimar tanımı md.2). Birleşik kural kaldırıldı. Not: tanımdaki "benek çarpanı yok" ile `δ_V` ölçütü sade kademede sağlanamıyor (rapor §4.3 ve §7); 05'in kutu başına üstel beneğiyle ayrı bir koşu da raporlandı. |
| 4 | kritik | Giderildi | `kaynak/sonar.cpp` (`kesis`, toplama tablosu) | Hücreler gövdeye sabit ortonormal yüzey koordinatlarında (u = e1 yönü, w = n × u), kenarı `Δ_h` = 1 cm. Hücre içinde enerji ağırlıkları toplanıyor, alan toplamın karekökü (`p_c = s_c √(Σ w_j) g_c e^{iφ_c}`). Bu yüzden hücre alanıyla normalize etmeye gerek kalmadı; üçgen sınırında kırpılan hücrenin alanı, ona düşen ışınların ağırlıklarıyla örtük olarak hesaplanıyor. |
| 5 | önemli | Giderildi | `kaynak/main.cpp` (`komut_yakinsama`) | Kutu kümesi `S`, sınanan koşulardan bağımsız bir referansla önceden sabitleniyor: taranan en büyük `N`, ayrı tohumlar (5000–5031); açı başına en yüksek ortalama enerjinin 10⁻³ katından büyük kutular. Kalan sınır: bootstrap, `S` sabitken gerçekleşimler üzerinde koşullu aralık verir (rapor §6). |
| 6 | önemli | Giderildi | rapor §4.2 ve §4.4 | Sahne boyutu etkisi menzile göre ayrı ayrı ve yeni ölçümden verildi. "`N ≥ 2000`'de tek ping 1,25 ms'yi aşar" genellemesi kaldırıldı; dünya iş parçacığından ayırma 05'in mimari seçimine ve ölçülen ağır hücrelere dayandırıldı. |

İnceleme sonu notu ("doğrulanmayanlar"a ekler) uygulandı: model sapmalarının bu turda giderilmiş olması, güven aralıklarının koşullu kapsamı ve p99'un bu tarama iş yüküne (açı ve hedef dizisi) ait olması rapor §6'ya eklendi.

## İkinci tur

Mimarın 2. ve son tur kapsamı (görev belgesi, "Düzeltme turundan sonra" başlığı) uygulandı; sol'un altı maddesi 1. turda giderilmişti, bu tur yeni madde açmadı.

| Kalem | Durum | Yer |
|---|---|---|
| Sade kademe: `Σw` + 05'in kutu başına beneği (gerçekleşim tohumundan, `N`'den bağımsız) ana kod yolu | Uygulandı | `tools/olcum/mp7/kaynak/sonar.hpp` (`sade_benek = true`), `main.cpp` (`--benek` varsayılanı 1); rapor §4.3, §4.4 |
| Dondurulmuş ölçüt değişmedi; tam kademe S2 ve S3 "geçmedi" | Korundu | rapor Özet, §4.3, §4.4 |
| Betimleyici tablo "azaltılmış ışın sayısının bilinen hatası" (N = 20 000 … 160 000; 7 m, 50 m) | Eklendi | `main.cpp` (`--hata_tablosu 1`), `tools/olcum/mp7/hata.sh`; rapor §4.6 |
| İskelet §8 sonar kararı için girdi paragrafı (karar cümlesi yok) | Eklendi | rapor §9 |

## Kapanış (2026-11-16)

sol'un ikinci incelemesi (`docs/olcumler/inceleme/MP7-sol-2.md`, "Yeni bulgular") iki düzeltme turundan sonra mimara döndü; kapanış kararı görev belgesindedir (`docs/gorevler/W0-MP7a.md`, "Kapanış"). Yeni ölçüm yapılmadı, rakam değişmedi.

| # | Karar | Yer |
|---|---|---|
| md.1 | Mimar uygulanan değişkeyi (gövdeye sabit hücre; (gövde, hücre, yol türü, gerçekleşim) anahtarı) tanım olarak kabul etti; rapora işlendi, yeni ölçüm yok | rapor §3 ("Model tanımı: uygulanan değişke"), hücre kuralı maddesi |
| md.2 | Rapor metni düzeltildi: eşli tablo "ortak benek alanında, sonlu referansa göre sayısal fark"; "gürültü tabanı" satırı ve yorumları "gözlenen iki bağımsız küme farkı" biçimine sınırlandı | rapor §4.6, §9 (ii) |
| md.3 | Rapor metni düzeltildi: "N > 1 280 000 gerekir" ve "daha büyük N'de geçemez" türü genellemeler çıkarıldı, kanıt taranan yapılandırmalar ve N = 1 280 000 destek koşusuyla sınırlandı; "Doğrulanmayanlar"a iki madde eklendi | rapor Özet (S3), §4.4, §6, §9 (i) |
