# MP6 sol incelemesine yanıt (1. düzeltme turu)

| | |
|---|---|
| İnceleme | `docs/olcumler/inceleme/MP6-sol.md` |
| Rapor | `docs/olcumler/MP6-godot-kamera.md` |
| Kararlar | `docs/gorevler/W0-MP6a.md`, "Tam kampanyadan sonra: düzeltme turu" (mimar, 2026-11-15) |
| Yanıtlayan | `ida-uygulayici` (Opus), 2026-11-15 |
| Yeni veri | `${IDA_CACHE:-$HOME/.cache/ida}/mp6/ham/20261115c/` (`b_gorunur`, `b_gorunur_2`, `okuma_640x480_esz`) |

## Bulgular

| # | Önem | Durum | Yer | Ne yapıldı / gerekçe |
|---|---|---|---|---|
| 1 | kritik | **Giderildi** | Rapor §2, §4.1, §4.2, §5 (G1, G3); `analiz.py` (`--ref`) | `b_gorunur` iki kez yinelendi. 1. deneme (`20261115c/b_gorunur`) son 7,9 s'de olağan döngü çizmediği için geçersiz sayıldı. 2. deneme (`20261115c/b_gorunur_2`) 299/299 kayıt aralığında (300,0 s) görünür kaldı; pencere 19/19 dış örnekte ekranda, 9000/9000 kare üretildi. G1'in kanıtı ve G3'ün referansı bu koşudur. Tam kampanyanın `b_ortulu`, `b_gizli`, `b_simge` ve `b_tamekran` koşuları bu referansla yeniden karşılaştırıldı: her biri MD5 9000/9000, karşılaştırılmayan 0, ortalama mutlak fark 0 (300 örnek). Eski `b_gorunur` koşusundaki 10,1 s'lik ve 1. denemedeki 7,9 s'lik aralığın nedeni açıklanamadı; "doğrulanmadı"da kaldı (§8) |
| 2 | kritik | **Giderildi** | Rapor §4.4 tablo ve notlar, §4.2 | Değerler ham özetten (`ozet_tablo.json`) alındı. İstekten geri çağrıya: p50/p99/maks 7,40 / 58,86 / 68,31 ms. İstekten hazır olmaya: 7,77 / 68,00 / 70,51 ms. Önceki metindeki "p99 68 ms" ikinci büyüklüğün p99'uydu ve hangi büyüklük olduğu yazılmamıştı. İki büyüklük artık ayrı adlandırılıyor |
| 3 | önemli | **Giderildi** (bir kısıtla) | `proje/olcum.gd` (`t_hazir`), rapor §2 ve §4.4 | Hazır olma zamanı artık ilk okumadan hemen sonra kaydediliyor. İkinci okumanın adı "yeni çizim olmadan yinelenen eşzamanlı geri okuma" oldu; "saf aktarım" ve "büyük kısmı çizim beklemesi" ifadeleri kaldırıldı. Raporda ilk okumanın içeriği ve ölçülmeyen kısmı yazıldı. `okuma_640x480_esz` (120 s) yinelendi. **Kısıt:** yinelemede kullanıcı tam ekran bir uygulamaya geçti ve durum yalnız 15/120 aralıkta tutuldu. Değerler raporda ayrı satırda ve koşul belirtilerek duruyor. G4 için görünür pencerede ölçülmüş tam kampanya değeri kullanıldı; ilk okuma süresi hazır zamanı hatasından etkilenmez. Görünür pencerede düzeltilmiş istekten hazır olmaya gecikme doğrulanmadı (§8). Mimar bu koşuya ikinci deneme yetkisi vermediği için yinelenmedi |
| 4 | önemli | **Giderildi** | `analiz.py` (`--ref`, `karsilastirilmayan`, `referans` sütunu); rapor §2, §4.1 | Referans artık açıkça seçilebiliyor; tablo koşu başına gerçek referansı ve karşılaştırılan / karşılaştırılmayan kare sayısını yazıyor. Rapordaki bütün içerik karşılaştırmaları `20261115c/b_gorunur_2` ile yapıldı (`ozet_tablo_ref_20261115c.json`). Önceki 2930/2930 sayısının nedeni şuydu: `a_tamekran` varsayılan olarak `a_gorunur` ile karşılaştırılmış ve 5875 kare karşılaştırılamamıştı. Yeni referansla `a_tamekran` 8805/8805, karşılaştırılmayan 0. Ancak bu koşu zaten geçersizdir |
| 5 | önemli | **Giderildi** | Rapor §4.1, §4.2, §6, §8; `analiz.py` (`ortu_tam_ekran`); `proje/olcum.gd` (örtü kipi 15 s'de bir) | Mimar kararıyla `a_gorunur` ve `a_tamekran` yinelenmedi. Raporda "geçersiz, yinelenmedi; yol (a) tam ekran sonucu doğrulanmadı" yazıyor. §6'daki genelleme örtülü, gizli ve simge durumlarına indirildi. `b_tamekran` için tam ekran örtünün sürdüğünün dış kanıtı çözümlemeye eklendi: 19/19 örnekte ekranda yalnız örtünün pencereleri vardı, örtü ekran genişliğindeydi ve ölçüm penceresi ekranda değildi. Örtünün kendi kip kaydı o koşuda yalnız başlangıçtaydı (kip 3); sonraki koşular için 15 s'de bir yazılıyor. `a_tamekran`'da tam ekran örtü 0/19 örnekte görüldü |
| 6 | önemli | **Giderildi** | `analiz.py` (tabloda "küre bulunamadı"); rapor §4.1, §5 G3 | Konumu bulunamayan kare sayısı her koşu için tabloda var; bütün koşularda 0. G3 metni bütün karelerde konumun bulunmasını şart koşuyor; şart sağlandı (9000/9000) |

## İnceleme notlarından uygulananlar

| Not | Durum | Yer |
|---|---|---|
| "Durum tutuldu 299/299" kayıt aralığı sayısı olarak yazılsın, süre zaman damgalarından hesaplansın | Giderildi | `analiz.py` (`aralik`, `sure_s`, `en_uzun_tutulmayan_s`); rapor §2, §4.1 |
| 07 §2.4'teki iki zamanlı çizim düzeni doğrulanmayanlara eklensin | Giderildi | Rapor §6, §8 |
| Asenkron kuyruk hesabı tam gecikme modeli değildir | Giderildi | Rapor §4.3 |
| Asenkron koşu sonunda gelmeyen 2 istek kayıp kanıtı değildir | Giderildi | Rapor §4.3 |
| Dış gözlem saati takvim saatidir | Giderildi | Rapor §7 |
| Sürücü içi kopyalar ölçülmedi | Giderildi | Rapor §4.4 |
| SHA-512 eşleşmesi bağımsız doğrulanmadı | Giderildi (kayda geçti) | Rapor §1, §8 |
| Ö5-k, M28, M30 kapanmış sayılmasın | Giderildi | Rapor §6 |

## Yeni gözlem

`b_gorunur` 1. denemesindeki çizimsiz aralığın nedeni bilinmiyor. 2. denemeden önce iki şey değişti:

- `caffeinate -u -t 2` ile kullanıcı etkinliği bildirildi; geçicidir, ayar değiştirmez.
- `pencere.js` ekran koruyucu ve kilit gibi üst katman pencerelerini de kaydetmeye başladı.

2. denemede üst katman penceresi görülmedi ve çizimsiz aralık olmadı. Ekran koruyucunun boşta süresi 600 s. Önceki aralıkların ekran koruyucudan kaynaklandığı bir çıkarımdır, doğrulanmadı.

## İkinci tur

İnceleme: `docs/olcumler/inceleme/MP6-sol-2.md`, "Yeni bulgular". Kararlar: `docs/gorevler/W0-MP6a.md`, "İkinci sol incelemesinden sonra" (mimar). Bu turda yeni koşu yapılmadı.

| # | Önem | Durum | Yer | Ne yapıldı / gerekçe |
|---|---|---|---|---|
| 1 | önemli | **Giderildi** (hüküm indirildi) | `analiz.py` (sayaç `ortu_tek_pencere_boyut_dogrulanmadi` ve açıklaması); rapor §2, §4.1, §4.2, §5 G3, §6, §8 | Çözümleme gerçek ekran sınırlarını ölçmüyor: `sag`, listedeki pencerelerin en sağ kenarından türetiliyor. Bu yüzden "başka uygulama tam ekranda" durumunun sürdüğü kanıtlanmış sayılmaz. Mimar kararıyla ekran sınırı ölçümü eklenmedi, çünkü yeni veri yok. Sayacın adı "ekrandaki tek pencere örtüydü (boyutu doğrulanmadı)" oldu. Yol (b)'nin tam ekran sonucu "durum doğrulanmadı"ya indi: 9000/9000 kare ve bit düzeyinde eşitlik yalnız gözlem olarak kalıyor. Birinci turdaki md.5 yanıtında yer alan "tam ekran örtünün sürdüğünün dış kanıtı" ifadesi bu nedenle geri alındı. Tam ekran Windows turuyla birlikte yeniden ölçülecek. G2'nin üç zorunlu durumu etkilenmiyor |
| 2 | önemli | **Giderildi** | Rapor §5 (G1, G3, G4), §8 | G1 ve G3 hükmüne sapma eklendi: görünür referans ve yinelenen koşular pilde alındı, yani görev belgesindeki "Mac prize takılı" şartı sağlanmadı. Mimar pildeki kanıtı G1 ve G3 için kabul etti (görev belgesi, 2026-11-15). Gerekçe: koşular `caffeinate -dims` altında kesintisiz tamamlandı; kare sayımı, kimlik sürekliliği ve bit düzeyinde eşitlik güç kaynağına bağlı değil. G4 yalnız prizdeki gece kampanyasına (`20261114b`) dayanıyor |
