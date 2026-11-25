# W5-bagla-a2 — A2 sunucusunun `ida_sim`'e bağlanması (görselleştirici gerçek sim'e bağlanır)

Tarih: 2026-11-25. Yazan: `ida-kritik-uygulayici` (K27). İnceleyen: sol (emniyet dışı dilim; K23: tek tam inceleme, tek düzeltme turu; sol sınırı doluyken K25/K27: `ida-inceleyici` bir kez denenir, takılırsa iş "sol inceleme borcu"na yazılır). Dal: `is/w5-bagla-a2` (taban `origin/main`; W4-sunucu-kucuk (#48) ve W5-a2-sunucu (#49) `main`'e girdikten sonra başlar). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `W4-ortak.md`, `W5-ortak.md`.

## Amaç

`ida_a2` kitaplığı (`sim/a2`, PR #49) sahte kaynakla sınandı. Bu iş onu `ida_sim`'in dünya döngüsüne bağlar: operatörün 3B görünümü (`viz/`, Godot) sahte kinematik kaynak yerine gerçek simülasyonu gösterir. Kamera hattı yine yoktur (`kamera_sayisi = 0`).

Okunacaklar: `sim/a2/BENIOKU.md` ve `sim/a2/include/ida/a2/*.hpp`; PR #49'un gövdesi ("Açık okumalar") ve aşağıdaki mimar kararları; `docs/gorevler/W5-a2-sunucu.md`; A2 sözleşmesi A2-K01, K02, K05, K20–K24, K47, K64, K65, K79, K80, K93, K96, §6; `docs/gorevler/W4-bagla-a3.md` ve `sim/sunucu/src/a3_baglanti.*` (aynı türden bir bağlamanın örneği); `sim/sunucu/BENIOKU.md`; W4-K27, W4-K28, W4-K30.

## Mimar kararları (kayıtta `W5-ortak.md`'ye işlenir)

- **W5-K31 — A2 bu dilimde isteğe bağlı açılır.** `ida_sim` A2 dinleyicisini yalnız `--a2-port <n>` verildiğinde açar (A3'teki `--a3-port` ile aynı düzen); seçenek yokken sahiplik kilidi alınmaz, dinleyici açılmaz, davranış ve çıktılar bu iş öncesiyle bayt bayt aynıdır. Gerekçe: var olan testler ve kart koşuları eşzamanlı çalışır; varsayılanın açık olması (A2-K01'in varsayılan portu) kilit ve port yarışına yol açar. Varsayılanın açılması laboratuvar uygulaması süreçleri başlatır hâle gelince yeniden ele alınır; A2'nin küçük sürüm notu. `kalite.gorunum = "kapali"` iken `--a2-port` verilirse dinleyici açılmaz ve tek satır bilgi yazılır (A2-K02).
- **W5-K32 — A2 gövde kimliği bağlama katmanında, süreç boyunca tekdüze artan sayaçtan verilir** (A2-K23, K92: kimlik yeniden kullanılmaz). Dünyanın kendi gövde numarası A2'ye taşınmaz; eşleme bağlama katmanında tutulur.
- **W5-K33 — A3 `calistirma/sifirla` A2'de hiçbir şeye eşlenmez.** Bugün yalnız `yuklendi` durumunda kabul edilir ve durum revizyonunu artırır (A3-K67); sim zamanında süreksizlik üretmez. A2 `sifirla()` çağrılmaz; her `baslat` bir `yeni_calistirma`'dır (A2-K80 (1)). Dünyayı sıfırlayan bir işlem geldiğinde yeniden ele alınır.
- **W5-K34 — PR #49'un açık okumaları kabul edildi** (1–13; rapordaki numaralarla). Sözleşme notu gerektirenler: `merhaba` alan hatalarında `ret(protokol)` (§3.3 tablosu A2-K06'nın önüne geçer); oturumun ilk `durum`u duraklatılmışken de gider (A2-K24'e istisna); `T − h` ve `T` iletileri doğum ve ölümde de gider; takvime denk gelen sınır iletisi seyreltilmez; yayın taşmasında oturum `veda(tampon_tasti)` ile yeniden eşitlenir.

## Kapsam

1. **Sunucunun ömrü:** `--a2-port` (1024…65535; aralık dışı değerle süreç başlamaz, A2-K93) verildiğinde süreç başına bir `ida::a2::Sunucu`; süreç kapanırken (olağan, hata, sinyal: #48'in tek kapanış yolu) `kapaniyor()` ve `durdur()`. Kilit ya da port alınamazsa sim A2'siz sürer (A2-K05, K79) ve nedeni tek satır yazar.
2. **Çalıştırma tanımı:** dünya kurulduktan sonra, ilk sınırdan önce `yeni_calistirma`: kip, `kalite.gorunum` kademesi, senaryonun varlık kümesinin A7 manifest yolu ve özeti, ortam ve malzeme, dalga kümesi (dalga modeli yoksa 0 bileşenli küme), yaşayan gövdeler ve sınır 0 durumu. Çalıştırma bitince `calistirma_durdu()`.
3. **Durum akışı:** her fizik sınırında, yalnız duran olmayan turda ve sınır başına bir kez `adim_siniri`. Gövde durumu dünyanın yetkili durumundan: `p_n`, `q_nb`, `w_b` ve **`v_n = R(q_nb) · v_b`** (dünya gövde çerçevesinde hız tutar; A2 NED hızı ister; dönüşümün yanlışı derleme hatası vermez). Dünyada bugün hangi gövdeler varsa (araç; senaryodaki hedefler ve atılmış torpido dünyada gövde olarak yaşıyorsa onlar da) hepsi yayınlanır; yaşamayanlar için doğum/ölüm olayları. Dünyada gövde olarak bulunmayan şey uydurulmaz; ne yayınlandığı raporlanır.
4. **Duraklatma (W4-duraklat):** duraklatma girişinde `duraklat()`, çıkışında `surdur()`; duran turlarda ve adımlamada davranış A2-K24'e göre (adımlanan her sınır bir `adim_siniri`'dir).
5. **Yerleşim ve maliyet:** yayın çağrıları dünya döngüsünde emniyet girdilerinin uygulanmasından ve karta gidecek partinin hazırlanmasından **sonra** gelir; hiçbir dalda bu işlemleri geciktirmez ya da koşula bağlamaz. A2 yüzünden hiçbir bekleme eklenmez (A2-K47).
6. **Sayaçlar:** A2 durumu ve sayaçları kapanış özet satırına (#48) eklenir. A3 kataloğunda A2 için bir kanal ya da alan varsa doldurulur; yoksa eklenmez ve raporlanır (katalog değişikliği ayrı iş).
7. **Çalıştırma betiği:** `viz/calistir.sh`'e gerçek sim'e bağlanan kip (`--sim <senaryo>`: `ida_sim`'i `--a2-port` ile SIL'de gerçek zamana yakın hızda başlatır, Godot'yu aynı porta bağlar; "SAHTE VERİ" etiketi görünmez). Betiğin sahte kaynaklı var olan davranışı değişmez. `viz/` altında başka değişiklik yapılmaz; istemcide bir uyumsuzluk çıkarsa raporlanır.

## Kapsam dışı

Kamera hattı; atlama ve tekrar; A2'nin varsayılan açılması; A3 kataloğu ve `web/`; `sim/a2` kitaplığında davranış değişikliği (bir hata bulunursa en dar düzeltme yapılır ve raporda ayrıca işaretlenir); `sim/kopru`, `core/`, `protocol/`, `firmware/`, `tests/kabul/**`, `docs/sozlesmeler/`.

## Kabul (testleri uygulayıcı yazar)

1. **Varsayılan değişmedi:** `--a2-port` yokken yerleşik `z1`, `scenarios/z1.toml` ve görev senaryosunun CSV çıktıları bu iş öncesi ikiliyle bayt bayt aynı; kilit dosyası oluşmuyor, port açılmıyor.
2. **A2 belirlenimciliği etkilemez (A2-U10):** SIL'de aynı senaryo dört koşulda (A2 kapalı; A2 açık, istemci yok; istemci bağlı ve okuyor; istemci bağlı ve okumuyor / ortada kapanıyor) bayt bayt aynı CSV'yi verir. Kartsız HIL'de (sözde uçbirim, sahte kart) A2 açık ve donuk istemciyle kaçırılan parti ve bütçe aşımı sayaçları A2 kapalı koşudan farklı değil (eşikler ortak zamanlama yardımcısıyla; W4-K24, K29).
3. **Gerçek istemciyle uçtan uca:** bir bütünleme testinde `viz/a2` istemci oturumu gerçek soketle `ida_sim`'e (ya da süreç içi `Calistirici`'ya) bağlanır; el sıkışma tamamlanır; alınan `durum` iletilerindeki araç pozu aynı sınırın CSV/dünya durumuyla bit düzeyinde aynıdır; `v_n` için: (a) bilinen `q` ve `v_b` ile birim testi (en az üç yönelim; biri eksenle hizalı olmayan), (b) hareketli bir koşuda ardışık `p_n` farkı ile `v_n · h` arasındaki fark bir adımın ivme teriminden küçük.
4. **Duraklatma:** duraklat → `duraklat` olayı, `durum` yok, nabız sürer; sürdür → akış kaldığı sınırdan; adımla → her adım bir `durum`.
5. **Kapanış ve yeniden bağlanma:** koşunun ortasında bağlanan istemci tutarlı tam durum alır; sinyalle kapanışta istemci `veda(sim_kapaniyor)` görür; A2'siz durumlar (port dolu, kilit başkasında) koşuyu bozmaz ve özet satırında görünür. Testler sabit 47210 portunu ve gerçek kilit dosyasını kullanmaz.
6. **Gerçek Godot ile bir koşu (elle; CI testi değil):** bu Mac'te Godot ikilisi varsa `viz/calistir.sh --sim scenarios/gorev-havuz.toml` (ya da çalışan en uygun senaryo) ile en az 60 s; aracın hareketi görünür, protokol hatası ve yeniden bağlanma 0; ekran görüntüsü yolu ve istemci sayaçları raporlanır. İkili yoksa "koşturulmadı" yazılır.
7. `host-debug` + `host-release` tam tur (sonda bir kez); `host-asan` yalnız derleme; W3-K14; MSVC kalıpları.

## Rapor

PR ve dal ucu; kabul maddelerinin sonucu ve kanıtı; yayın çağrısının dünya döngüsündeki yeri (dosya:satır) ve ölçülen ek süre (ölçüm, bu Mac); dünyada hangi gövdelerin yayınlandığı; A3 tarafında A2 durumunun taşınıp taşınmadığı; `sim/a2`'de dokunulan yer varsa gerekçesi; belirsizlikler ve seçilen okuma (K20).
