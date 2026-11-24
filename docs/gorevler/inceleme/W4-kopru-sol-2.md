# sol incelemesi: kopru

## Hüküm

**Kabul edilmemeli.** `f66d49c` üzerindeki ikinci dar emniyet denetiminde **#1, #2, #3 ve #11 kapanmadı; #4 kapandı.**

**#4 kapandı:** `sim/kopru/src/kopru.cpp:293` doğrulanmış farklı açılışta uygulama sayacı tabanını sıfırlıyor; aynı açılışta koruyor. Oturum sonunda bekleyen çıktılar temizleniyor (`:148`); eski oturumlu aktüatörler yeni tabana ulaşmadan reddediliyor (`:403`). Yeni test gerçek uygulanan PWM’yi de denetliyor.

İnceleme salt okunur yapıldı. Testler ve aşağıdaki karşı örnekler çalıştırılmadı; sonuçlar kaynak akışından çıkarıldı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `sim/kopru/src/kopru_parti.cpp:337` | **Kapanmadı: en yeni yetkili revizyon seçilmiyor.** Aynı pencerede acil durdurma bas–bırak–bas ile revizyonlar 2–3–4 olsun. Gevşetme gecikmesi nedeniyle köprüye ulaşan son örnek `(estop=1, rev=3)` olsun; revizyon 4’ün sensör kaydı atlansın. Kod, yetkili geçmişteki revizyon 4’ü örneğin revizyonundan büyük olduğu için atlıyor ve `(1,2)` gönderiyor. W4-K12(3)’ün istediği en yeni tam eşleşme `(1,4)`tür. Alan birleşimi düzelmiş olsa da revizyon seçimi kapanmamış. | Seçimi örneğin revizyonuyla üstten sınırlama; mevcut yetkili geçmişte W4-K12’nin tam eşleşme ve ardından kısıtlayıcılık önceliğini uygula. Atlanan kayıtlı üç değişim vakasını ekle. | kritik |
| 2 | `sim/kopru/src/kopru_sinir.cpp:158`; `sim/kopru/src/kopru_sinir.cpp:344` | **Kapanmadı: tutarsızlık nedeniyle reddedilen yanıt geçmişte kalıyor.** `FIRE` ile birlikte gelen `IS_SAFE=1` yanıt önce `son_out_seq`, `gecen_yanit`, yanıt süreleri ve birleşim geçmişine işleniyor. Fizik sınırında reddedilince bu değişiklikler kaldırılmıyor; `yanit(n)` reddedilmiş yanıtı döndürmeyi sürdürüyor. Yinelenen/geç yanıtın eski ezme yolu düzeltilmiş, tutarsız yanıt yolu kalmış. | Olay–yanıt toplu doğrulaması tamamlanmadan yanıtı kabul edilmiş geçmişe geçirme. Bekleyen alım ile kabul edilmiş yanıtı ayır. | kritik |
| 3 | `sim/kopru/src/kopru_sinir.cpp:340`; `sim/kopru/src/kopru_sinir.cpp:364`; `sim/kopru/src/kopru_sinir.cpp:477` | **Kapanmadı: eksik olay kümesiyle izin açılabiliyor.** HIL’de aynı partinin `SAFETY_OPEN`’ı ve `n_events=2, IS_SAFE=1` yanıtı ulaşsın; `FIRE` çerçevesi kaybolsun. `a_n`’de yuva eksik olmasına rağmen `SAFETY_OPEN` uygulanıyor. `FIRE` yinelemesi 15 ms sonra gelince yanıt tutarsız bulunuyor; önceden açılmış izin kapanmıyor. Böylece birlikte reddedilmesi gereken kümenin genişletici etkisi kalıyor. | Sayılan olay kümesi tamamlanıp topluca doğrulanmadan hiçbir genişletici etki uygulama. Eksik kümede kısıtlayan davranışı koru; parçalı teslim ve yineleme vakasını ekle. | kritik |
| 11 | `sim/kopru/src/kopru.cpp:407`; `sim/kopru/src/kopru.cpp:355`; `sim/kopru/src/kopru_sinir.cpp:344` | **Kapanmadı: tutarsız yanıt canlılığı yenileyebiliyor.** `FIRE` önceden alınmışken aynı partinin `IS_SAFE=1` yanıtı `yanit_al()` tarafından başarılı döndürülüyor ve `gecerli(true)` ile `son_rx_gecerli` yenileniyor. Tablo 4.6b/K118 reddi ancak sonraki fizik sınırında gerçekleşiyor. Eski `sess` ve bağımsız alan hatalarının yolları düzelmiş; toplu V4 doğrulaması hâlâ canlılık güncellemesinden sonra. | Canlılığı yalnız gerekli toplu doğrulamadan geçmiş yanıtla yenile; sonradan reddedilecek yanıtı geçerli trafik sayma. | kritik |

## Yeniden hesaplananlar

- `P/h = 5/1,25 = 4`; `L_act/h = 10/1,25 = 8` fizik adımı.
- #3’te izin `t_n+10 ms`’de açılır. İlk yineleme `t_n+15 ms`’dedir; sonraki sınırda işlenmesi bile `apply_by=20 ms` içindedir. Tutarsızlık reddine rağmen izin, `hold=200 ms` için `t_n+210 ms`’ye kadar açık kalabilir.
- Eski taban 1000 iken yeni sayaç 1’in seri farkı `4.294.966.297 > 2³¹−1` olur; #4’teki açılışa bağlı sıfırlama eski engeli kaldırıyor.

## İskelete ya da diğer bölümlere notlar

- `origin/main:docs/gorevler/W4-ortak.md` erişildi; W4-K3/K4/K5/K6/K12 esas alındı. #5–#10 ve #12–#14 yeniden değerlendirilmedi.
- Eklenen toplu tutarlılık testi olayları birlikte teslim ediyor; #3’teki kayıp olay ve geç yineleme sırasını kapsamıyor.
- MAVLink 2’nin sondaki sıfır baytlarını kırpma kuralı canlı [resmî serileştirme belgesinden](https://mavlink.io/en/guide/serialization.html) doğrulandı. Windows, kart ve çalışma zamanı davranışı bu incelemede ölçülmedi.