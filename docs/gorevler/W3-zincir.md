# W3-zincir — çekirdeğin bütünleştirilmesi ve ilk kapalı çevrim (Z1)

Tarih: 2026-11-21. Yazan: `ida-uygulayici` (Opus). İnceleyen: sol (tek tur; araç silahlandırmasına dokunan kısım için görev işinin ikinci turuyla birlikte). Dal: `is/w3-zincir`. Ortak kurallar ve mimar kararları: `docs/gorevler/W3-ortak.md` (W3-K1…K12).

## Amaç

W3'te ayrı dallarda yazılan kestirim, kontrol ve görev modüllerini birbirine bağlamak, güdümlü hedef komutunu eklemek ve aracı fizik çekirdeğiyle **aynı süreçte, kapalı çevrimde** koşturmak: Z1 hedefi (araç silahsız hâlden kalkar, 1,5 m'ye dalar, derinliği ve yönü tutar; bir itici zayıflatıldığında toparlar). Bu, sistemin ilk uçtan uca çalışmasıdır.

## Başlangıç durumu

- `main`'de (bu iş başladığında): fizik (`sim/fizik`), kestirim, kontrol, tel katmanı. Başlamadan önce `main`'de hangilerinin bulunduğunu denetle; eksik olan varsa DUR ve raporla.
- Henüz `main`'de olmayan iki dal bu işte birleştirilir: `origin/is/w3-gorev` (görev modülü) ve `origin/is/w3-gorev-kabul` (bağımsız kabul testleri ve çekirdek kabul düzeneğinin düzeltmesi). Kendi dalını `origin/main`'den aç ve bu ikisini `git merge` ile al (commit atmak bu iş için serbesttir: yalnız bu iki birleştirme commit'i; başka commit atma).
- Oturum katmanı (`ida_link` üst katmanı) ve kart yazılımı bu işin dışındadır.

## Kapsam

### 1. Bağlama

- Görev modülündeki geçici tanımlar kaldırılır: `ida_gor_kon_istek_t` → `ida_kon_istek_t`; `IDA_GOR_G_*` → `IDA_KES_G_*`; `gecerlilik_al` gövdesi → `ida_kes_gecerlilik`; `FAILSAFE_RESET` kabulünde `ida_kes_ariza_temizle`.
- Kontrolde `istek_al` gövdesi güdümün erişimcisini (`ida_gud_kon_istegi`), itici arıza maskesi `ida_gor_itici_disi`'ni kullanır.
- `core/TASARIM.md` güncellenir: modül tablosu (kestirim, kontrol, görev artık yer tutucu değil), boyutlar, kanca imzaları (görev kancaları çekirdek işaretçisi alıyor).

### 2. Güdümlü hedef (W3-K7)

- `GUIDED_SP` komutu (A4-K50, §3.17): maske, alan aralıkları, ret nedenleri ve sırası yalnız sözleşmedeki gibi. Hedef güdümde saklanır ve kontrol isteğine çevrilir: derinlik, yön; konum ve hız alanları kabul edilir ama yatay geçerlilik yokken o kanal sönümlü, sıfır hedefli davranışa iner.
- `GUIDED` kipine giriş: tutum, yön ve derinlik geçerliliği (yatay istenmez).
- `MANUAL` komutu, görev öğeleri ve `AUTO` bu işte yoktur.

### 3. Kapalı çevrim düzeneği (`tests/zincir/`)

- Tek süreçte: `Dunya` (fizik) + sensör üretimi + gerçek `ida_core` + platformun en küçük taklidi. Ağ, protokol ve kart yok.
- **Sensör üretimi** (düzeneğin içinde, sade; ayrıntılı sensör modelleri sonraki iştir): fizik durumundan IMU (özgül kuvvet ve açısal hız; sabit sapma ve beyaz gürültü), basınç (derinlikten), manyetometre (yönelimden; yerel alan sabit), batarya ve sızıntı (sağlıklı sabit değerler), emniyet örneği. Örnek hızları ve zaman damgaları A4 kurallarına uygun; gürültü tohumu sabit. Sensör çerçeveleri ve montaj A5/`scenarios/ida0.toml` ile uyumlu.
- **Platform taklidi:** 200 Hz çağrı; her çağrıda `ida_in_t` kurulur; çıktıdaki normalleştirilmiş itki isteği A4-K54'e göre darbe genişliğine çevrilir ve iki kontrol periyodu sonra (`L_act`) fiziğe uygulanır; silahsız ve güvenli çıktıda nötr. Motor sürücünün 2 s nötr beklemesi senaryonun başındadır.
- **İtici arızası:** bir kanalın itkisini ölçekleyen arıza. Fizik arayüzünde yoksa düzenek o kanalın darbe genişliğini nötre doğru ölçekleyerek uygular ve bunu raporlar; fizik koduna dokunulmaz.
- **Senaryo (Z1):** 0–2 s nötr; hizalama; `ARM`; `SET_MODE(GUIDED)`; `GUIDED_SP` derinlik 1,5 m, yön +90°; 40 s tutma; bir dikey iticide itki × 0,4; 40 s tutma; `DISARM`.
- **Çıktı:** her kontrol adımında gerçek ve kestirilen derinlik, yön, yalpa, baş-kıç, sekiz itki isteği, kip, silah durumu, arıza düzeyi CSV olarak (`ida_zincir_deneme` aracı); mimarın gözle bakması ve web panellerine sonradan beslenmesi için.

### 4. Ayar

Kazançlar ve sınırlar gerçek fizikle ayarlanır (kontrol ve kestirim parametrelerinin varsayılanları). Ayarın neye göre yapıldığı raporda; değerler "bu fizik modeline göre ayarlandı" diye işaretlenir.

## Dosya sahipliği

Yazılır: `core/src/gorev/**`, `core/src/gudum/**`, `core/src/kontrol/kontrol.c` (yalnız `istek_al` ve itici maskesi bağlantısı), parametre varsayılanları (`*_param.def`), `core/TASARIM.md`, `core/test/**` (bağlamanın gerektirdiği uyarlamalar), `tests/zincir/**`, `tests/CMakeLists.txt` (tek satır). `tests/kabul/**` test yazarınındır: değiştirme; bir kabul testi kalıyorsa nedenini (uygulama mı, testin sentetik sensör akışı mı) kural numarasıyla raporla. `sim/fizik/**`, `core/src/kestirim/**` ve çağrı çerçevesi değiştirilmez; değişiklik gerekiyorsa en küçük değişikliği öner ve raporla (uygulama, mimar onayıyla).

## Kabul

1. Bağlama: geçici tanım kalmadı (arama ile gösterilir); `host-debug` tam koşu, `tests/kabul/cekirdek` ve `tests/kabul/gorev` dahil. Kalan kabul testi varsa her biri için neden.
2. `GUIDED_SP`: A4 §7'nin ilgili satırları (yanlış kipte `BAD_MODE`; maske 0, 0x80, aralık dışı derinlik/irtifa/yön → `OUT_OF_RANGE`; maskesiz alandaki aralık dışı değer sonucu değiştirmez).
3. **Z1, CTest olarak** (sabit tohum, belirlenimci): `ARM` kabul edilir; derinlik 1,5 m'ye belirtilen sürede ve aşmayla oturur; tutmada derinlik ve yön hatası sınır içinde; itici arızasından sonra en büyük sapma ve toparlanma süresi sınır içinde; hiçbir adımda sayısal çökme ya da beklenmeyen arıza eylemi yok; `DISARM` sonrası itki sıfır. Sınırlar testte sabit yazılır ve raporda gerekçesiyle listelenir.
4. Emniyet değişmezleri kapalı çevrimde de izlenir: silahsızken ve güvenli çıktıda fiziğe giden her darbe genişliği nötr; `armed` yalnız kabul edilmiş `ARM` ile 1 olur.
5. İki koşu bit düzeyinde aynı CSV'yi üretir (aynı ikili, aynı makine).
6. `fw-debug` ve `fw-release` derlenir; çekirdeğin kod boyu, `sizeof(ida_core_t)` ve en büyük yığın çerçevesi raporlanır. Statik analiz yerelde temiz (W3-K12).
7. Bir koşunun süresi (ölçüm, bu Mac): 90 s'lik senaryonun duvar saati süresi.

## Rapor

W3-ortak'taki yedi başlık; ayrıca Z1 koşusunun özet sayıları (oturma süresi, aşma, tutma hatası, arıza sonrası sapma) ve CSV dosyasının yolu.
