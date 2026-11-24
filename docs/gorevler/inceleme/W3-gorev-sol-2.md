# sol incelemesi: gorev

## Hüküm

**Kabul edilmemeli: 2 kritik bulgu.** İkinci tur, belirtilen emniyet kapsamıyla sınırlı tutuldu. Karşı örnekler statik olarak izlendi; test çalıştırılmadı, dosya değiştirilmedi.

1. **Bulgu 1: kapanmadı.** Eski ARM açığı iki komut sırasında da engelleniyor; ancak sözleşmede bulunmayan `BAD_MODE` koşulu eklenmiş.
2. **Bulgu 5: kapanmadı.** `fs_level=6` vakası düzelmiş; örnek işlenirken üzerine yazılan bozulma güvenli mandala ulaşmayabiliyor.
3. **W3-K9 bağlantısı: kapandı.** `cerceve.c:456` emniyet bayrağını değiştirmeden birleşik kısıt oluşturuyor. Ret yolu `cerceve.c:606` üzerinden `CALL_REJECTED` veriyor; `cikti.c:156` kabul koşuluyla eski görev bayrağının sızmasını engelliyor. Saptanmış daraltmada genişletici etki yolu bulunmadı.
4. **Komutsuz genişleme: kapandı.** `armed=1` yazımı yalnız kabul edilen ARM yolunda (`gorev.c:290`); GUIDED/AUTO geçişleri yalnız kabul edilen kip komutunda (`gorev.c:336`). Adım yolunda böyle bir genişleme bulunmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `core/src/gorev/gorev.c:326` | Süregelen GCS kaybında, geçerli bileşenlerle `DEPTH_HOLD→GUIDED` isteği yalnız hedef sütunun arıza düzeyi ağırlaştığı için `BAD_MODE` alıyor. A4 §3.17 SET_MODE satırı yalnız kestirim ve AUTO görev ön koşullarını sayıyor; A4-K124 ek ret koşulunu yasaklıyor. B1 testleri komut sonuçlarını denetlemediğinden bunu kaçırıyor (`core/test/gorev/test_gorev_sol.c:191`). | ARM korumasını sürdürerek sözleşmeye uygun çözüm belirlenmeli; mevcut ek koşul korunacaksa mimar tarafından sözleşmenin küçük sürümünde açıkça tanımlanmalı. İki sırada komut sonuçları da sınanmalı. | kritik |
| 2 | `core/src/gorev/gorev.c:169`, `core/src/gorev/gorev.c:76`, `core/src/cerceve/cerceve.c:413` | **B5 bozulma kanıtını geç denetliyor.** Karşı örnek: silahlı DEPTH_HOLD, `fs_level=NONE`, `batt_var` ham baytı 2; aynı kabul edilen çağrıda geçerli BATT örneği. Çerçeve bütünlüğü bozulmayı görüp `core_ok=0` yapar (`cerceve.c:579`), fakat örnek `batt_var=true` yazar. Sonraki görev denetimi tutarlı bulur; `bozuk` kurulmaz, `armed=1` kalır ve istek KAPALI olmak zorunda değildir. | Görev tutarsızlığı örnek yazımlarından önce mandallanıp güvenli adım dalına taşınmalı. Üzerine yazılan alan bozulmasını da gerçek çerçeve sırasıyla sınayın. | kritik |

## Yeniden hesaplananlar

- Süregelen GCS kaybı: DEPTH_HOLD `WARN=1`, GUIDED `HOLD=2`; `2>1` yeni `BAD_MODE` reddini tetikliyor. İki komut sırasında ARM mevcut kipte kabul ediliyor; kip değişmiyor. Koşul ilk kez aynı çağrıda görülürse `0→1` adım daraltması iki genişleticiyi de engelliyor.
- Doğrudan `fs_level=6` vakası: yeniden kurulum ardından `bozuk=true`, düzey `DISARM=5`, `armed=0`, istek KAPALI. Kurulmuş `bozuk` izi yalnız yeniden kurulumla kalkıyor; `SESSION_START` ve `FAILSAFE_RESET` temizlemiyor. Eksik olan, tablodaki bozulmanın bu mandala ulaşması.

## İskelete ya da diğer bölümlere notlar

- W3 görev belgeleri çalışma ağacında bulunmadığından `origin/main` sürümleri okundu.
- Rastgele test gerçek dağıtıcıyı kullanıyor; çerçevenin bütünlük→örnek→görev sırasını çalıştırmadığından ikinci bulguyu dışlamıyor.
- [C17 N2176 PDF](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2176.pdf) açılmadı; bu kaynak doğrulanmış sayılmadı. Bulgular depo içi sözleşme ve kontrol akışına dayanıyor.