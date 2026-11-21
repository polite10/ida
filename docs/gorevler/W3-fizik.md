# W3-fizik — simülatörün fizik çekirdeği (dilim 1: su altı ve yüzey geçişli tek araç)

Tarih: 2026-11-21. Yazan: `ida-uygulayici` (Opus). İnceleyen: sol (tek tur). Dal: `is/w3-fizik`. Ortak kurallar: `docs/gorevler/W3-ortak.md`.

## Amaç

`sim/fizik/` altında, başsız ve belirlenimci çalışan fizik kitaplığını (`ida_fizik`) kurmak: İDA-0 aracının altı serbestlik dereceli hareketi, hidrostatik (yüzey geçişi dahil), sekiz itici ve motor sürücü modeli, sabit adımlı sayısal çözüm. Z1 ve Z2 hedeflerinin "deniz ve araç" yarısıdır.

Okunacaklar: iskelet §3.1 (çerçeveler, birimler, kuaterniyon), §4 (zaman), §5 (İDA-0); `docs/mimari/04-simulator-cekirdegi.md` §2.1–§2.4, §2.6, §2.7, §2.10, §2.13, §3, §7; A5 sözleşmesi §3'ün araç ve ortam alanları, Ek A (kütle durumları), Ek E (normatif varsayılanlar); `docs/research/00-duzeltmeler-ve-kararlar.md` 04 düzeltmeleri.

## Kapsam (dilim 1)

1. **Katı cisim dinamiği:** gövde ve dünya (NED) çerçeveleri, kuaterniyon; katı cisim ve eklenmiş kütle, Coriolis ve merkezcil terimler, doğrusal ve karesel sönüm, geri getirici kuvvet ve momentler (04 §2.3). Sayı tipi `double`.
2. **Hidrostatik ve yüzey geçişi:** düz su yüzeyi; batık hacmin derinlikle değişimi ve buna bağlı kaldırma kuvveti ile kaldırma merkezi (04 §2.4'ün dalgasız hâli). Araç yüzeyde yarı batık dengede durabilmeli ve dikey itkiyle dalabilmelidir.
3. **İticiler ve motor sürücü:** sekiz iticinin konum ve yönlerinden kuvvet-moment; darbe genişliği (µs) → itki eğrisi, ölü bölge, doyma, birinci dereceden gecikme; ileri-geri asimetrisi (04 §2.7). Komut, uygulanacağı fizik adımıyla birlikte verilir.
4. **Çevre:** sabit ve düzgün akıntı (araç hızı akıntıya göre bağıl alınır). Dalga ve rüzgâr arayüzde yer tutar, gövdesi sonraki dilimdir.
5. **Sayısal çözüm:** sabit adım (varsayılan 400 Hz), 04 §2.10'daki şema; adım içinde sıra 04 §2.1 ve iskelet §3.2'deki gibi.
6. **Parametreler:** `AracParametreleri` ve `OrtamParametreleri` yapıları; İDA-0 varsayılanları A5 Ek E ve 04 §2.6'dan, üç kütle durumuyla (A5 Ek A). Alan adları A5 ile bire bir eşlenir (TOML okuyucu bu işte yoktur; sonraki iş aynı yapıları doldurur).
7. **Arayüz:** tek bir `Dunya` sınıfı: kurulum (parametreler, başlangıç durumu), `adim()`, itici komutu verme, durum anlık görüntüsü (konum, yönelim, doğrusal ve açısal hız, gövde çerçevesinde özgül kuvvet ve açısal ivme, itici başına gerçekleşen itki, batık oran). İş parçacığı, dosya, saat ve ağ kullanılmaz.
8. **Deneme aracı:** `ida_fizik_deneme`: açık çevrim bir darbe genişliği profilini koşturur ve durumu CSV olarak yazar (mimarın gözle bakması için).

## Kapsam dışı

Dalga ve rüzgâr gövdesi; çarpışma ve temas; torpido, fırlatma ve hedefler; batarya ve güç modeli; sensör modelleri; zaman yöneticisinin gerçek zaman ve bariyer kısmı; A1/A2/A3 bağlantıları; senaryo dosyası okuma; görselleştirme.

## Dosya sahipliği

Yazılır: `sim/fizik/**`, `sim/CMakeLists.txt` (yoksa oluştur). Bağımlılık: Eigen (vcpkg manifestine açık sürümle; `EIGEN_FAST_MATH=0`). Başka bağımlılık eklenmez.

## Kabul (testleri uygulayıcı yazar; hepsi CTest'te)

1. **Hidrostatik denge:** araç durgun bırakılınca beklenen su hattında dengelenir; batık oran parametrelerden hesaplanan değerle uyuşur.
2. **Serbest sönüm:** dalıp çıkma, yalpa ve baş-kıç salınımlarının periyodu ve sönümü doğrusal modelin analitik değeriyle belirtilen tolerans içinde.
3. **Son hız:** sabit ileri itkide ulaşılan hız, sönüm katsayılarından hesaplanan değerle uyuşur.
4. **Enerji:** itki yokken toplam mekanik enerji artmaz.
5. **Adım yarılama:** h, h/2, h/4 ile aynı senaryoda hata beklenen mertebeyle küçülür.
6. **Belirlenimcilik:** aynı girdiyle iki koşu bit düzeyinde aynı durum dizisini verir (aynı ikili, aynı makine).
7. **İtici geometrisi:** yalnız ileri, yalnız yan, yalnız dikey ve yalnız dönüş komutları beklenen eksende net kuvvet-moment üretir; çapraz bileşenler tolerans altında.
8. **Uzun koşu:** 10 dakikalık simülasyonda kuaterniyon normu ve durum sonlu kalır.
9. **Süre (ölçüm):** bu Mac'te tek adımın ortalama ve en kötü süresi raporlanır (hedef değil, bilgi).
10. `host-debug` ve `host-asan` temiz; kod MSVC'de derlenecek biçimde yazılmıştır (ortak belgedeki uyarı listesi).

Toleranslar testte sabit olarak yazılır ve raporda gerekçesiyle listelenir. Parametrelerde 04 ile A5 çelişirse A5 geçerlidir; fark raporlanır.
