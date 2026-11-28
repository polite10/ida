# W5-viz-gorsel — 3B görünümün görsel kalitesi: deniz, su altı, araç modeli

Tarih: 2026-11-25. Yazan: `ida-hafif-uygulayici` (Sonnet; K28). İnceleyen: `ida-inceleyici` (Opus; K23, K27). **Sınıf: emniyet dışı** (K23: tek tam inceleme + tek düzeltme turu; CI yeşilse birleşmeye hazır). Dal: `is/w5-viz-gorsel` (taban `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `W4-ortak.md`, `W5-ortak.md` (W5-K43).

## Amaç

Kullanıcı 2026-11-25 gecesi görselleştiriciyi gerçek sim'le çalıştırıp baktı (`viz/calistir.sh --sim scenarios/gorev-havuz-3b.toml`) ve "ne denizaltı ne de deniz düzgün görünüyor" dedi. Gözlenen ekran (bu Mac, 120 fps, "SIL · 3B sade · kamera takip", derinlik 1,5 m): arka plan düz siyah, havuz tabanı ışıksız düz beyaz bir düzlem, solda gri bir kavis; araç kutu ve silindirlerden oluşan yer tutucu model. Su rengi, su altı sisi, yüzey ve derinlik hissi yok.

`viz/proje` içinde su yüzeyi, dalga ve su altı optiği gölgelendiricileri zaten var (`su.gd`, `golgelendiriciler/`); `ana.gd` su altı arka planını ve sisini A2 `Environment` içeriğinden (`b_sonsuz`, `beta_b`, güneş ışınımı) kurar. Bu iş önce görüntünün neden böyle olduğunu bulur, sonra 3B görünümü operatörün aracı ve ortamı okuyabileceği hâle getirir.

## Okunacaklar

`docs/mimari/07-gorsellestirme.md` §2.2, §2.7, §5 (kademe tablosu), §6; `docs/sozlesmeler/A2-gorsellestirici.md` (`Environment`, `su_optigi`, kademe), `docs/sozlesmeler/A7-sahne.md` (varlık kümesi, manifest, örtü denetimi); `viz/BENIOKU.md`; `viz/proje/betikler/{ana,su,varlik_yukleyici,operator_kamerasi}.gd`; `viz/proje/golgelendiriciler/*`; `viz/arac/model-uret/uret.py`; `assets/BENIOKU.md`, `assets/{havuz,acik_su}/manifest.json`; `sim/sunucu/src/a2_baglanti.cpp` (ortamın senaryodan okunması); `scenarios/gorev-havuz-3b.toml`, `scenarios/ida0.toml` (araç boyutları).

## Kapsam

1. **Tanı (önce):** `gorev-havuz-3b.toml` ile su altı görüntüsünün neden siyah ve ışıksız çıktığını bul: sim'in gönderdiği `Environment` değerleri (havuz ortamının çözümlenmiş varsayılanları), görselleştiricinin bunları dönüştürmesi (`isik_olcegi`, `b_sonsuz` → arka plan, sis), havuz varlığının malzemesi ve ışıkları (A7 `KHR_lights_punctual`). Bulguyu değerlerle raporla. Kusur görselleştiricideyse düzelt; senaryodaysa senaryoyu düzelt; sim'in okuduğu değerlerdeyse ya da sözleşme belirsizse düzeltme, mimara raporla.
2. **Su altı görünümü (iki kademe):** sade kademede de su altında olunduğu anlaşılmalı: su rengi arka plan, mesafeyle sönen sis (07 §5: sade = zayıflama + geri saçılım), yukarı bakınca su yüzeyinin altı. Tam kademede var olan son işlem gölgelendiricisi ve dalgalı yüzey çalışır durumda olmalı. Kademe ayrımı 07 §5 tablosuna uyar; sade kademe ekran/derinlik dokusu okumaz (düşük donanım; K16).
3. **Havuz sahnesi:** taban ve duvarlar ışık alır ve ayırt edilir (basit doku ya da ızgara deseni kabul; ölçek hissi versin). Havuz lambaları ya da yeterli ortam ışığı.
4. **Açık deniz sahnesi:** `assets/acik_su` kümesiyle gösterilebilir bir görev senaryosu `scenarios/gorev-acik-su-3b.toml` (`gorev-havuz-3b.toml`'un açık su eşi; görev öğeleri aynı mantıkla; `kalite.gorunum = "tam"`). Gökyüzü, ufuk, dalgalı yüzey, yüzeyde ve dalışta geçiş. Sim'de koşmalı ve görselleştirici sahneyi kurmalı.
5. **Araç modeli:** `ida0.glb`'yi `uret.py` ile daha okunur bir gövdeye çevir: akış biçimli gövde, iticiler ve kanatçıklar ayırt edilir, üst yüz ile alt yüz ve baş ile kıç renk/işaretle ayrılır. Boyutlar `scenarios/ida0.toml`'daki fizik değerleriyle tutarlı kalır; A7 manifesti ve örtü denetimi geçer; `--denetle` çıktısı güncellenir. Model basit geometriden üretilmeye devam eder (ikili dosya elle çizilmez, dış model indirilmez).
6. **Ölçüm:** bu Mac'te sade ve tam kademede kare süresi (`--olcum`), değişiklik öncesi ve sonrası. Windows hedef makinesi için tahmin yazılırsa tahmin olduğu belirtilir.

## Mimar kararları

- **W5-K60 — Sözleşme değişmez.** A2 ve A7 1.0 dondurulmuştur. Görsel düzeltme sözleşmenin verdiği alanlarla yapılır. Bir alanın eksik ya da yanlış olduğu görülürse iş durmaz: görselleştirici tarafında makul okuma seçilir, rapora yazılır.
- **W5-K61 — Görüntü fizikle çelişmez.** Araç, hedef ve yüzey sim'in bildirdiği konumda çizilir; görsel süs (köpük, parçacık, kostik) fizik durumunu değiştirmiş gibi görünmez. Sade kademede dalga çizilmez (07 §5); tam kademede yüzey `a2.su_yuksekligi` ile aynıdır.
- **W5-K62 — Dış bağımlılık yok.** Okyanus eklentisi kullanılmaz (07 §2.7); teknik uyarlanırsa kaynağı ve lisansı (MIT) dosyada yazılır. Doku ve model depoda üretilir.
- **W5-K63 — Sade kademe hafif kalır.** Sade kademenin kare süresi bu değişiklikle bu Mac'te belirgin artarsa (ölçüm; artış raporlanır) ek maliyet yalnız tam kademeye taşınır.

## Kapsam dışı

`sim/**` (tanıda okunur, değiştirilmez), `core/`, `firmware/`, `protocol/`, `web/`, `docs/sozlesmeler/`; kamera sensörü hattı ve kare halkası; torpido ve hedef gövdelerinin sim'de yayınlanması; Windows başlatıcısı (W5-viz-windows'ta).

## Kabul

1. `host-debug` tam tur yerelde bir kez; `IDA_VIZ=ON` ile `host.viz.*` ve `varlik_godot` geçer; `uret.py --denetle` depodakiyle aynı. Windows ve macOS CI yeşil.
2. `viz/calistir.sh --sim scenarios/gorev-havuz-3b.toml` ve `viz/calistir.sh --sim scenarios/gorev-acik-su-3b.toml` koşar; her biri için `--ekran-goruntusu` ile su üstü, su altı ve yüzey geçişi anlarından ekran görüntüleri alınır, PR'a eklenir ya da yolları rapora yazılır.
3. Tanı bulgusu (madde 1) değerleriyle raporda.
4. Kare süresi ölçümleri (madde 6) önce/sonra, kademe başına.

## Rapor

PR ve dal ucu; tanı bulgusu; ekran görüntülerinin yolları; ölçümler; sözleşmede eksik bulunan alanlar ve seçilen okuma; yapılamayanlar ve nedeni.
