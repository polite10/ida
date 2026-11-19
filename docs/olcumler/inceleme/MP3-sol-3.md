# MP3a — sol kapanış incelemesi (düzeltme turu 2)

Tarih: 2026-11-19. Kapsam: `MP3-sol-2.md` Y1–Y4, `MP3-yanit-2.md`, `docs/gorevler/W0-MP3a-duzeltme-2.md` ve bunların hedef kodu. `MP3-vcp.md` başındaki **ikinci tur** sonuçları esas alındı; tarihli geçmiş yeni sonuç sayılmadı.

## Hüküm

**Dört bulgunun koşulsuz kapandığı kabul edilemez.** Y1'in eski çift sayım hatası kaldırılmış; Y2 ve Y3'ün karşı örnekleri kapanmış. Y4'ün kuyruk ölçümü ve R'den yeniden üretimi, **B.azami_bekleyen=64** korunurken önceki karşı örneği doğru reddediyor. Ancak 64 sınırı kampanya profilinde sabitlenmemiş: sınırı 100 yapan kayıt, gerçekten 100 bekleyeniyle birlikte hâlâ `--kampanya` altında `GECTI` olabiliyor. Bu turda bir **önemli** açık kaldı (A1).

Standart 64 sınırını koruyan kayıtlarda yeni bir yanlış `GECTI` karşı örneği bulmadım. Bu, bütün girdiler için doğruluk kanıtı değildir. Mimar kabul kapsamını dondurulmuş 64 parametresine daraltırsa ve **64'ten küçük/büyük bütün kuyruk ayarları bağımsız kampanya ön denetiminde reddedilirse**, Y4'ün geri kalanı bu dar kapsam için kapanır; A1 genel çözümleyicinin bilinen sınırlaması olarak kalır. Böyle bir ön denetim uygulanmış veya kapsam kararı verilmiş sayılmadı. İkinci ve son olağan düzeltme turudur; üçüncü uygulama turu başlatılmadı.

## Y1–Y4 durumu

| Madde | Durum | Dayanak |
|---|---|---|
| Y1 — TC/yeniden yükleme dönemi | **Yazılım hatası kapandı; donanım varsayımları açık** | `ortak/mp3_halka.c:18–42` kesme sayısını kullanmıyor; `(N−BNDT−FIFOL) mod N` ve zaman/ilerleme sınırları var. `fw/platform.c:224–256` HT/TC kesmelerini kullanmıyor, BNDT/FIFOL/BNDT görüntüsünü deniyor; `:267–287` kopya sonrası yeniden denetliyor. Bağımsız 514 sınır denetimi geçti. V1–V3, `ortak/mp3_halka.h:17–24` içinde açıkça doğrulanmamış; inceleme bunları donanımda doğrulamadı. |
| Y2 — temsil aralıkları/taşma | **Kapandı** | `pc/kayit.cpp:351–391` B/P/K temsil sınırları; `:452–468` denetimli toplama/çarpma; `:620–621` taşmayı reddetme. Eski UINT64_MAX örneklerini ve sınır komşularını yeniden çalıştırdım: yanlış başarı yok. |
| Y3 — akış kronolojisi | **Kapandı** | `pc/kayit.cpp:520–550` PC son plan/yazma ve kart son plan/gecikme/süre karşılaştırmaları; `ortak/mp3_kart.c:315–332` gecikme ve son zaman aynı damgadan. Kart planının µs'ye aşağı yuvarlanması firmware ile aynı; kaynakta açıklanan ek tolerans sıfır. Alt/üst sınır ve komşuları yeniden denendi. |
| Y4 — echo kuyruk zaman çizelgesi | **64 parametresinde kapandı; genel 64 sınırı iddiası açık** | `pc/oturum.cpp:481–504` yanıt/timeout çıkarma ve ekleme sonrası azami ölçümü; `pc/kayit.cpp:559–618` R olaylarından yeniden üretim. Önceki 100 bekleyen örneği, B=64 ve P=0/1/63/64/65 için reddedildi. B/P sınırları 100 yapılınca A1 oluştu. |

Y1'deki eski `(yarim=2, BNDT=0, FIFOL=0)` dönem uyuşmazlığı artık modele girdi olarak taşınmıyor. Her FIFOL=0…255 için blok sonu BNDT=0 ve yeniden yükleme BNDT=N aynı toplamı verdi; önceki bayttan blok sonuna geçiş yalnız bir bayt ilerledi. Kararsız BNDT görüntüsü referansı değiştirmedi; uzun yoklama aralığı BELIRSIZ verdi. Bunlar ortak C işlevinin denetimleridir, DMA yazmaçlarının gerçek birlikte görünürlüğünün kanıtı değildir. Modelin de kaynak okuması ve FIFOL/BNDT güncellemesini birlikte kabul ettiği kodda açıktır (`test/mp3_test.cpp:126–177`).

## A1 — Kampanya profilinin 64 bekleyen sınırı kaydın kendisinden değiştirilebiliyor

**Önem: önemli.** Yer: `pc/kayit.cpp:366`, `:204–210`, `:609–616`; üretim yolu `pc/mp3_vcp.cpp:152–155`.

Görev Y4 için sınırı 64 olarak veriyor. Çözümleyici ise `B.azami_bekleyen` değerini 100000'e kadar kabul edip bütün kuyruk karşılaştırmalarında bu değeri kullanıyor. `profil_adi()` yalnız yük ve istek hızına baktığı için 64'ten farklı sınır da `mp3-echo-v1` oluyor. CLI de `--azami-bekleyen 100` değerini kabul ediyor; dolayısıyla bu yalnız CRC'si yeniden üretilmiş kayıtla sınırlı bir giriş yolu değildir. CLI ile yeni koşu başlatılmadı; bu yol kaynak incelemesiyle saptandı.

**Çalıştırılan karşı örnek:** İkinci turun `tara-2000000-echo64-200.kayit` kaydı bellekten alındı. Her R yanıtı `max(eski_yanit, 500000000 + sira × 1000000)` yapıldı; P son yanıtı yeni azamiye eşitlendi. Bağımsız olay taramasında azami bekleyen **100** bulundu. Önce yalnız bu değişikliklerle B=64 altında `GECERSIZ` alındı. Ardından yalnız `B.azami_bekleyen=100` ve `P.azami_bekleyen_gozlenen=100` yapılıp değişen satırların CRC-32'si yenilendi:

```text
mp3_vcp analiz /dev/stdin --kampanya
istek=1000 gönderilen=1000 yanıtlı=1000 zaman_asimi=0
... kuyruk sınırı 100, gözlenen azami bekleyen 100
SONUC: GECTI (profil mp3-echo-v1)
çıkış kodu: 0
```

Kaynak kayıt gerçek kısa tarama kaydıdır; değiştirilen yanıt zamanları bir donanım ölçümü değildir. Değişiklikler yalnız bellekte yapıldı, ham kayıt korunmuştur. Temiz kayıtta yalnız B sınırını 1, 32, 63, 64, 65 veya 100 yapmak her seferinde `--kampanya` altında `GECTI` verdi. Böylece hem düşük hem yüksek özelleştirmelerin profil denetiminden geçtiği ayrıca doğrulandı; düşük sınır denemeleri 64 üzerinde bekleyen olduğunu iddia etmez.

**Gerekli sonuç:** Dondurulmuş kampanya kabulü 64'ü kayıt ayarından bağımsız denetlemeli veya 64 dışındaki ayarları kampanya profili dışında tutmalıdır. Mimar yalnız B=64 parametreli koşuları kabul kapsamına alacaksa bu sınırlama açıkça kayda geçmelidir. B=64 altında yeniden üretim sınırının atlandığını gösteren ayrı bir örnek bu incelemede bulunmadı.

## Çalıştırılan doğrulamalar

- Kullanılan PC ikilisi SHA-256: `477a92ee8c8ae107e8fd2fef6825d002866399f0c0f7e36efdaf9a8f0bd40598`; güncel `MP3-vcp.md` kimliğiyle aynı.
- `$IDA_CACHE/mp3/kos/20261119-005626-tara/` içindeki on kayıt mevcut çözümleyiciyle `--kampanya` altında yeniden değerlendirildi: dokuz `GECTI`, 12 Mbaud akış `GECMEDI`. Güncel raporla uyumlu.
- Y2: P `tx_gec_azami_ns`, `t_son_yaz_ns` ve K `tx_gec_azami_us`, `tx_son_us` alanlarında UINT64_MAX, UINT64_MAX−1, INT64_MAX+1 ve u32 sınır komşuları denendi. Temsil sınırını aşanlar `GECERSIZ`; temsil edilebilir aşırı gecikme `GECMEDI`; yanlış `GECTI` yok. Akış denemelerinde kampanya ve en az 10 s beklentisi kullanıldı.
- Y3: 2 Mbaud kaydında son kart planı 9 999 825 µs, azami gecikme 13 µs. Son kart zamanı 0, 9 999 824 ve 9 999 839 için `GECERSIZ`; 9 999 825 ve 9 999 838 için `GECTI` alındı.
- Y4: 100 bekleyen karşı örneği B=64 altında ve P=0/1/63/64/65 değerleriyle `GECERSIZ`; B=P=100 altında yukarıdaki A1 sonucu.
- Ortak `mp3_halka.c`, geçici dizinde oluşturulan bağımsız küçük C denetleyicisiyle UBSan altında derlenip çalıştırıldı: **514 denetim, 0 hata**, UBSan tanısı yok. Kaynak kod değiştirilmedi.

219/219 masaüstü ve 219/219 UBSan tam testleri bu kapanış incelemesinde yeniden çalıştırılmadı; mimarın bağımsız çalıştırıp geçirdiği bilgisi ile `MP3-yanit-2.md` test kapsamı okundu. Buradaki 514 denetim ayrı ve daha dar bir sınır doğrulamasıdır.

## Sınırlamalar

Kart erişimi, firmware yükleme ve uzun kampanya yapılmadı. Kaynak, betik veya ham kayıt değiştirilmedi; yalnız bu inceleme raporu yazıldı. V1–V3 donanımda doğrulanmadı; bunların gerçekleşmediğine ilişkin bir kart hatası bulgusu çıkarılmadı. Özellikle başlığın V2 bozulduğunda CRC'nin her durumda sessiz başarıyı önleyeceği açıklaması bu incelemenin doğruladığı bir garanti değildir (`ortak/mp3_halka.h:19–21`). 600 s kampanyanın eksikliği bilinen sonraki aşamadır, yeni bulgu sayılmadı. Ürün, sonar, hedef ve atış mantığı kapsam dışı tutuldu.
