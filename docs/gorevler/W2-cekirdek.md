# W2-cekirdek — Ortak çekirdeğin iskeleti (çağrı çerçevesi, tipler, parametreler, test düzeneği)

Tarih: 2026-11-20. Yazan: `ida-uygulayici` (Opus; K18). Kabul testleri: `ida-test-yazari` (kodu yazandan bağımsız; ikinci aşamadan sonra). İnceleyen: sol. Dal: `is/w2-cekirdek` (taban: `is/w2-depo`'nun kayda geçmiş son hâli). Kayıt: `ida-kayit`.

Bu iş `core/` altında, A4 sözleşmesinin **çağrı çerçevesini** uygulayan ve iç modülleri henüz yer tutucu olan çekirdeği kurar. W3'ün kestirim, güdüm ve kontrol işleri ile W4'ün atış kontrol (emniyet zinciri ve arayüzler) işi bu iskeletin içine, birbirinin dosyasına dokunmadan yerleşir; kart yazılımı, SIL koşucusu ve `ida_link` dönüştürücüsü de gerçek modüller gelmeden önce bu çekirdeği bağlayıp çağırabilir. Başvuru: `docs/sozlesmeler/A4-port.md` (bütünü; özellikle §3.1, §3.9–§3.13, §4.1–§4.3, §4.9, §4.11, §4.12, §4.14, §4.15, §7), `core/include/ida/port.h`, `core/include/ida/algilama_kaydi.h`, iskelet §3.1, §3.2, §4, §5; `docs/mimari/02-ortak-cekirdek.md` yalnız iç yapı için fikir kaynağıdır (taslak; iskelet ve A4 ile çeliştiği yerde geçersiz). Sözleşme ile bu belge çelişirse sözleşme geçerlidir ve çelişki raporlanır.

İş iki aşamalıdır. **Birinci aşama yalnız tasarım notudur; kod yazılmaz.** Mimar notu denetler; ikinci aşama mimarın yanıtından sonra başlar.

## Birinci aşama: tasarım notu (`core/TASARIM.md`)

Kısa ve somut (hedef: 300 satırı geçmez). İçerik:

1. **Modül sınırları.** `core/src/` altındaki iç modüller (çağrı çerçevesi, emniyet durumu ve daraltma, parametreler, zaman ve seri aritmetik, matematik yardımcıları, kestirim, güdüm, kontrol, görev, atış kontrol, telemetri ve günlük) ve her birinin iç başlığındaki işlevlerin listesi (imza düzeyinde; gövde yok). Hangi W3/W4 işinin hangi dosyaların sahibi olacağı; iki işin aynı dosyaya dokunmasını gerektiren yer kalmamalı.
2. **`ida_core_t` yerleşimi.** Alt yapıların sırası, tahmini boyu, `ida_core_info`'nun bildireceği değerler (A4-K98); değişebilir genel durum olmadığının nasıl sağlandığı (A4-K22).
3. **Kural dağılımı tablosu.** A4'ün her davranış kuralı (A4-K01 … son kural) için tek satır: (i) bu işte tam uygulanır, (ii) bu işte yer tutucudur ve W3/W4'te hangi işe aittir, (iii) çekirdeğin değil platformun kuralıdır. Tablo kuralları tek tek sayar; aralıkla geçiştirmez.
4. **Yer tutucu çekirdeğin dışarıdan görünen davranışı.** İç modüller boşken step ve tick'in `OK` dönüşünde çıktının her alanına ne yazıldığı ve bunun A4'ün hangi tanımlı durumuna karşılık geldiği (kuralla). İlke: yer tutucu çekirdek hiçbir zaman itki üretmez, hiçbir genişletici olay (`SAFETY_OPEN`, `FIRE`) üretmez ve sözleşmede tanımlı olmayan hiçbir ret nedeni ya da durum uydurmaz. Sözleşme böyle bir durumu tanımlamıyorsa bu bir bulgudur: not, seçenekleri ve önerini yazar, kendi başına karar vermez.
5. **Belirsizlikler.** A4'te uygulamayı iki biçimde okunabilir bırakan her yer (kural numarasıyla).

Atış kontrolün iç durum makinesi, hedefle ilgili hiçbir hesap ve fırlatma mantığı bu notun konusu değildir; atış kontrol modülü notta yalnız sınırıyla (çağrı çerçevesinin ondan ne istediği: emniyet durumu sorgusu ve adım çağrısı) yer alır.

## İkinci aşama: uygulama

Kapsam, mimarın birinci aşamaya yanıtıyla kesinleşir; aşağıdaki çerçeve değişmez.

### 1. Dil ve sınırlar

ISO C17, `-pedantic-errors`, uzantı yok; bellek ayırma ve özyineleme yok (A4-K97); değişebilir genel ya da statik durum yok (A4-K22); platform başlığı (`stm32*`, CMSIS, RTOS) yok; işletim sistemi, saat, dosya, standart giriş/çıkış yok; kayan nokta `float` (binary32), W2-depo'nun kayan nokta politikasıyla (`-ffp-contract=off`, `-fno-fast-math`). Aynı kaynak host ve firmware köklerinde derlenir.

### 2. Tam uygulanacaklar

- **Başlatma ve bilgi:** `ida_core_info`, `ida_core_init` ve init'in değerlendirme sırası (A4 §3.11); sürüm ve boyut bildirimi (A4-K98).
- **Çağrı çerçevesi:** `ida_core_step` ve `ida_core_tick`'in A4-K113 değerlendirme sırası ((a1), (a2), (b), (b2), (c), (d), (e)); birlikte oluşan koşulların önceliği; sıra numarası denetimi (seri aritmetik; A4-K25); her dönüş kodu için çıktının bütün alanlarının yazılması (A4-K27, A4-K28, §4.14 tablosunun hata satırları); reddedilen çağrıda komut sonuçları (A4-K114); `ida_core_background`'ın değerlendirme sırası (A4-K117).
- **Emniyet durumu ve daraltma ((b) adımı):** mandal bitleri, `SESSION_END` ve emniyet örneklerinin kısıtlayıcı içeriği (A4-K116); uygulanan daraltmanın geri alınmaması ve yinelenmesinin ek etki üretmemesi. Bu adım, sonradan reddedilen çağrıda da çalıştığı için iskeletin parçasıdır; genişletme yönü (silahlandırma, izin açma, atış) bu işte **yoktur** ve yer tutucu çekirdekte hiçbir yolla gerçekleşmez.
- **Zaman ve seri aritmetik yardımcıları** (iskelet §3.1; A4 §3.1, §4.2) ve **matematik yardımcıları** (`mat`: vektör, kuaterniyon, küçük sabit boyutlu matris; iskelet §3.1'in kuaterniyon ve çerçeve kuralı). Yalnız W3'ün ilk işlerinin gerek duyacağı çekirdek küme; liste tasarım notunda.
- **Parametreler:** tablo altyapısı, kodlanmış blok (A4 §3.10), blok doğrulaması ve varsayılanlara düşüş (A4 §4.9), `param_count`/`desc`/`find`/`get`. Parametre listesi bu işte yalnız A4'ün kendisinin adlandırdığı parametreleri içerir; modül parametreleri kendi işlerinde eklenir ve eklemenin tek dosyada tek satır olması sağlanır.

### 3. Yer tutucular

Kestirim, güdüm, kontrol, görev, atış kontrol ve arka plan işi: iç başlık + boş gövde. Yer tutucunun görünen davranışı birinci aşamada onaylanan biçimdedir.

### 4. Birim testleri (uygulayıcı yazar; Unity, CTest)

A4 §7'nin bu işte tam uygulanan kurallara düşen satırları; ayrıca: (i) §3.11 öncelik tablosunun her hücresi için bir vaka; (ii) belirlenimcilik: aynı init ve aynı çağrı dizisiyle iki örneğin çıktı dizilerinin bayt bayt eşitliği (A4-K24), iki örnek aynı süreçte iç içe çağrılırken de; (iii) hata dönüşlerinde çıktının hiçbir alanının önceki çağrıdan kalmadığının gösterimi (çıktı tamponu çağrıdan önce bilinen bir desenle doldurulur; A4-K27); (iv) NULL işaretçi satırlarının her biri (§4.15). Testler `host-asan`'da da koşar ve firmware araç zinciriyle derlenebilir.

## Kapsam dışı

Kestirim, güdüm, kontrol, görev mantığı (W3); atış kontrolün emniyet zinciri durum makinesi ve arayüzleri (W4; içerik emniyet zinciri ve arayüzlerle sınırlıdır, hedefleme çözümü ve atış ölçütü hiçbir işte ayrıntılandırılmaz); platform (kart yazılımı, SIL koşucusu) ve platformun kuralları; `ida_link`; `port.h`, `algilama_kaydi.h` ve sözleşme metninde değişiklik; kartta çalıştırma ve süre ölçümü.

## Kabul

1. Tasarım notu mimarca onaylanmıştır ve kural dağılımı tablosu A4'ün bütün kurallarını kapsar.
2. `host-debug`, `host-asan`, `fw-debug`, `fw-release` ön ayarlarında uyarısız derlenir; birim testleri host'ta geçer; bellek ayırma denetimi ve çekirdek sınırı denetimi geçer; çekirdek nesnelerinde yazılabilir veri bölümüne düşen simge yoktur (simge tablosundan gösterilir).
3. "Tam uygulanır" denen her kural için en az bir test vardır; raporda kural → test adı tablosu verilir.
4. Yer tutucu çekirdek hiçbir girdi dizisinde `thr_cmd` ≠ 0, `SAFETY_OPEN` ya da `FIRE` üretmez; bunu belirlenimci sözde rastgele girdi dizileriyle (geçerli ve geçersiz karışık, sabit tohum) koşan bir test gösterir.
5. Firmware kökünde çekirdeğin kapladığı yer ve `sizeof(ida_core_t)` raporda.

Bağımsız kabul testleri (`tests/kabul/cekirdek/`; `ida-test-yazari`): yalnız A4, `port.h` ve bu belge okunarak yazılır; uygulama kaynağı ve tasarım notunun iç yapı bölümleri okunmaz. Kapsamı: §3.11 değerlendirme sırası ve öncelik tablosu, §4.14'ün hata satırları, §4.15 önkoşulları.

## Rapor

Birinci aşama: notun yolu, bulgular ve belirsizlikler (ayrı başlık), mimardan beklenen kararlar. İkinci aşama: yazılan dosyalar; her kabul maddesinin sonucu (komut ve çıktı özetiyle); kural → test tablosu; boyutlar; sözleşmede bulunan çelişkiler (sözleşmeye dokunulmaz); doğrulanamayanlar; çalışma dizini ve dal. Commit atılmaz.
