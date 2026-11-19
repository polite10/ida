# W1 — Çapraz uyum incelemesi ve uyum turu

Tarih: 2026-11-20. Önkoşul: sekiz sözleşmenin (A1–A8) hepsi kendi iki düzeltme turunu bitirmiş olmalı. Bu belge `docs/gorevler/W1-ortak.md` ile birlikte okunur; öncelik sırası oradakiyle aynıdır.

## Amaç

Her sözleşme kendi çalışma ağacında, ötekileri görmeden yazıldı ve incelendi. Tek tek incelemeler iç tutarlılığı sınadı; iki sözleşmenin aynı şeyi farklı tanımladığı yerler sınanmadı. Bu iş, sözleşmeleri tek ağaçta bir araya getirip **aralarındaki** çelişkileri bulur ve giderir. Dondurma bu işten sonradır.

## Akış

1. **Bütünleştirme dalı (kayıt agent'ı):** `main`'den `sozlesme/w1-butunlesme` dalı açılır; altı sözleşme dalı sırayla birleştirilir (`sozlesme/a4-v1`, `a8-v1`, `a5-v1`, `a2a7-v1`, `a3a6-v1`, `a1-v1`). Dallar ayrı dosyalara dokunur; çakışma beklenmez. Çakışma çıkarsa birleştirme durur ve mimara bildirilir.
2. **Çapraz inceleme (sol; çağrıyı mimar başlatır):** bütünleştirme ağacında, aşağıdaki denetim listesiyle. Çıktı: `docs/sozlesmeler/inceleme/W1-capraz-sol.md`. Her bulgu için: iki tarafın yeri (belge + kural numarası), çelişkinin ne olduğu, hangi tarafın değişmesinin önerildiği, önem.
3. **Uyum turu (sözleşmenin yazarı; mimar kararlarıyla):** her bulgu bir sözleşmeye atanır; yazar kendi dalında ve kendi dosyalarında düzeltir. Bir bulgu iki sözleşmeyi de değiştiriyorsa mimar hangi tarafın "kaynak" olduğunu söyler; öteki taraf atıf verir. Tek tur.
4. **Doğrulama incelemesi (sol):** yalnız uyum turunda değişen yerler ve çapraz bulguların kapanışı. Kritik bulgu kalırsa iş mimara döner.
5. **Dondurma:** sürüm `1.0`, etiketler `sozlesme-A<n>-v1`, PR ve birleştirme (kayıt agent'ı); açık bulgu kapanış tabloları `docs/mimari/acik-bulgular.md`'ye işlenir.

## Denetim listesi

Liste tek tek incelemelerden ve yazar raporlarından toplandı; inceleyen bunlarla sınırlı değildir.

### Kapsam sınırı (bütün sözleşmeler)

- Atış kontrol içeriği emniyet zinciri ve arayüzlerle sınırlıdır. A1'den hedef seçimi işlemi (`FC_OP_SELECT`), `fc_ref` ve opak `ext[]` alanı kaldırıldı; **A4'te hâlâ var** (`IDA_FC_SELECT`, `fc_ref`, `ext_len`/`ext[]`, anlamı tanımsız `arg0`/`arg1`). A4, A1'in kapalı işlem tablosuna eşitlenir. Görev öğesi türlerinden `TARGET_ALIGN` A1'den çıkarıldı ve görev parametrelerinin anlamı kapalı tabloya bağlandı; A4'ün görev öğesi tablosu aynı kapalı listeyi taşımalı. Başka hiçbir sözleşmede hedef seçimi, hedefleme çözümü ya da atış ölçütü taşıyabilecek alan ya da opak kanal kalmamalı.

### Emniyet: tek ilke, tek sayaç

- "Kısıtlama hâkimdir" ilkesi A1, A3, A4 ve A5'te aynı sonucu vermeli: aynı sınırda/çağrıda kısıtlayan varsa gevşeten reddedilir; reddedilen ya da bayat bir girdinin kısıtlayıcı içeriği kaybolmaz.
- `emniyet_rev` (A3) ile `safety_rev` (A1) aynı sayaçtır; sahibi dünyadır. Tanım (mimar kararı, 2026-11-20): `uint32`, sim sürecinin başında 1 (0 = platform henüz örnek görmedi), süreç ömrü boyunca tekdüze, yeni çalıştırmada ya da oturumda sıfırlanmaz; `0xFFFFFFFF`'te doyar ve sonrasında her gevşetme kalıcı reddedilir. A3 şu an `u64` yazıyor; bu tanıma çekilir. Artıran olayların listesi tek olmalı: kabul edilen her kısıtlayan komut (durumu değiştirmese de), her fiilî gevşetme (A3'ten gelen), süresi dolan izin, uygulanan `SAFETY_CLOSE`, oturum sonu/kopması, kart sıfırlanması, yeni çalıştırma, `sifirla`. Kartın izin açması ya da yenilemesi artırmaz. A5'in zamanlı ve koşullu emniyet atamaları bu listede nasıl yer alıyor?
- Aynı adımdaki emniyet değişikliklerinin uygulama sırası: A1-K85 (adım başında bütün sanal girdi değişiklikleri) ↔ A3-K95 (kısıtlayan → olaylar → gevşeten). Tek sıra olmalı.
- A5'in `atama_celiskisi` kuralı (emniyet alanında kısıtlayan değer) ile A3/A1 sırası aynı sonucu vermeli.

### A1 ↔ A4

- Her A1 mesaj alanı ile A4 port alanı arasındaki eşleme: ad, tip, birim, aralık, geçersiz değerde davranış. Bir tarafta olup ötekinde karşılığı olmayan alan.
- Sayısal sınırlar: `n_samples` ≤ 16, `n_cmds` ≤ 8, `n_det` ≤ 8, `n_det_batch` ≤ 2, `n_evres` ≤ 4, 12 sonuç yuvası (A4) ↔ A1'in parti üye sınırları, `B_krit` = 520 B, olay sonucu kuyruğu (8), `N_olay`.
- Zaman parametreleri ve aralıkları: `apply_by`, `hold`, `valid_for`, bayatlık sınırları; A1'in eşitsizlikleri A4'ün sabitleriyle birlikte sağlanıyor mu.
- Oturum sonu tick'i (A1-K107) ↔ A4-K11 ("SIL'de ızgara tick'i yapılmaz"): oturum sonu çağrısı ızgara tick'i değildir; A4'te açıkça ayrılmalı.
- Tick takvimi: A1, beklenen TF'den önceki pencerede ızgara tick'i başlatılmamasını ve TF geldiğinde süren çağrının `cagri_cakismasi` olarak sayılmasını istiyor ↔ A4-K18/K19/K23 (başlangıç gecikmesi, bekleyen parti, birleştirme).
- `CORE_COUNTERS` ve `STATUS.health` eşlemesi: A4'ün son adayındaki bütün sayaç ve sağlık alanları A1'de adıyla eşlenmiş mi; dönüş kodları (`IDA_RC_*`, `IDA_BG_*`) iki tarafta aynı mı.
- A1 Tablo 4.6'daki alanlar arası doğrulama (olay türü × çağrı türü, dönüş kodu × çıktı bayrakları) ↔ A4 §4.14.
- SIL kilitli adım (A1): zamanlayıcılar sim zamanında, parti birleştirme yok, parti başına sabit arka plan dilimi ↔ A4-K102/K109/K110 ve A6'nın çağrı kaydı; A8'in "SIL'de sim beklemez" kuralı bu sırayla çelişiyor mu.
- Olay kimliği, taşıma yinelemesi ve ilk kabul kapısı (A4-K67, K106) ↔ A1'in yineleme ve sonuç saklama kuralları.
- Daraltıcı mandallar (A4-K46) ↔ A1'de daraltıcı komutların taşınması ve "hiç kaymaz" sınıfı.
- Ping360: A4 §6'da sade kademe 600 örnek ↔ A1/A5'te iki kademede 1200.

### A4 ↔ A8

- Tip adları ve yapı etiketleri (`ida_det_record_t`, `ida_det_batch_t`); A4 başlığı gerçek `core/include/ida/algilama_kaydi.h` ile derleniyor mu (bugüne kadar yer tutucuyla derlendi).
- Parti başına kayıt sayısı (A8: en çok 4, tahmin) ↔ A4 `n_det` ≤ 8, `n_det_batch` ≤ 2 ↔ A1 kritik bayt sınırı.
- A8'in ikinci düzeltme turu sol tarafından yeniden incelenmedi: A8-sol-2'nin dört bulgusunun (K27/K28, §3.3 `TIME`, K46, K39/K42) kapanışı burada doğrulanır.

### A2 ↔ A8

- Kare zamanı tanımı: A2'nin pozlama sonu + süre gösterimi ↔ A8'in ölçüm zamanı ve pencere alanları.
- Halka el sıkışması ve emeklilik (A2-K86–K89) ↔ A8'in tüketici davranışı: yeni halkayla el sıkışmadan önce eski okumaların bitmesi, yeni halka neslinin bildirilmesi, kamera yolu kapanan tüketicinin dönüşü.
- `SimKareDusmeNedeni` kodları (özellikle `halka_emekli_hazir`) ↔ A8 kare durumları (`SKIPPED_BUSY`, `COMPONENT_DOWN`, …).
- Kamera kademe değerleri yalnız A2'de tanımlıdır (tam 640×480/30 Hz, sade 320×240/15 Hz; tahmin); A8, A3, A5, A6 atıf vermeli, yinelememeli.

### A6 ↔ A4, A1

- Girdi tekrarı A4 port sınırındadır: A6'nın çağrı kaydındaki alan alan kodlama A4 §3'ün alan listesiyle birebir mi; `bg_slices`, background çağrılarının `now_w` değeri, init parametreleri ve parametre blobu kayıtta mı; A4-K24/K109 ile A6-K52/K54/K55 aynı şeyi söylüyor mu.
- Güvence yalnız SIL'de: A1, A3, A4 ve A6'nın tekrarlanabilirlik cümleleri aynı sınırı çiziyor mu (A1-K102/K114 SIL bayt eşitliği dahil).
- HIL'de `bg_slices`'ın PC'ye taşınması gerekmediği A1 ve A4'te de yazılı mı.

### A3/A6 ↔ ötekiler

- Kanal ve olay kataloğu: A1'in sayaç ve olayları (kaçırılan son tarih, sim gecikmesi, `REJECT_CONFIG` + eşitsizlik numarası, kayan üyeler, atılan `CALL_TRACE`), A2'nin yenileri (`halka_emekli_hazir`, `emekli_halka_yaniti`, `tuketici_kamera_kapali`), A5'inkiler (`atama_celiskisi`, `donma_onceki_cikti_yok`), A4 sağlık bayrakları ve sayaçları A3/A6'da taşınabiliyor mu.
- A3'ün senaryo uçları ↔ A5 doğrulama raporu ve çözümlenmiş senaryo özeti.
- Kalite kademesi seçimleri (iskelet §6) A3, A5, A2 ve A1'de aynı adlarla.

### A5 ↔ ötekiler

- Anahtar türetimi etiketleri: A1'in SIL nonce'ları için A5'te etiket tanımlı mı; Philox kullanım yerleri çakışmıyor mu.
- ESC nötr değeri ve aralığı (A5) ↔ A1'in sim tarafı doğrulama tablosu ↔ A4'ün itki komutu aralığı.
- Varlık ve manifest atıfları: A5 ↔ A7 (dosya adları, rol düğümleri, CoACD ayarları); A5 gövde/varlık kimlikleri ↔ A2 `GovdeBilgisi`.
- Parametre adları ve sınıfları: A5'in `otopilot` bloğu ↔ A4 parametre tablosu.

### Ortak tanımlar (bütün sözleşmeler)

- Zaman: birim (ns), tip (`int64`), geçerli aralık (A4: 0…2⁶²), sim zamanı ile duvar zamanının adları; sıra numarası genişlikleri ve seri aritmetiği.
- Çerçeveler ve birimler: NED, gövde ekseni, sensör çerçevesi, açı birimi; iskelet §3.1 ile aynı.
- Sürümleme: her sözleşmenin sürüm alanı, uyumluluk kuralı ve el sıkışmalarda öteki sözleşmelerin sürümünün nasıl taşındığı.
- "A<m>'den beklenen" listelerinin her maddesi karşı sözleşmede karşılanmış mı; karşılanmayan her madde bulgudur.

## İskelete işlenecekler (mimar; bu işin parçası değil, bilgi için)

İskelet 0.9'da düzeltilecekler: kamera kademe değerlerinin yeri (A2); kamera açıkken 3B görünüm kapalı olamaz; %60 bant hedefinin olağan profil için olduğu, bağlayıcı üst sınır ölçütünün her yönde kapasitenin %90'ı olduğu (karttan PC'ye bağlayıcı en kötü durum %84,3); uçtan uca payın 0,267 ms'ye inmesi (tahmin) ve `L_act` değerlendirmesi; girdi tekrarı güvencesinin yalnız SIL'de verilmesi ve kayıtlı bayt akışının karta yeniden verilmemesi; A1'in iskelete önerileri.
