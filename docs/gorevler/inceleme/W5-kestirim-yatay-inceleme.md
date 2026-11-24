# Bağımsız inceleme: W5-kestirim-yatay (`95cb2ca..ae089f4`)

İnceleyen: `ida-inceleyici` (Opus; taze bağlam). sol'un kullanım sınırı dolduğu için kullanıcı kararı K25 ile sol yerine yapıldı; sol inceleme borcu listesindedir. 1–3 ve 8. satırlardaki diziler inceleyicinin geçici prob programlarıyla çalıştırılarak doğrulandı; depoda dosya değişmedi.

## Hüküm

Düzeltme gerekli. Ölçüm modelleri, Jacobian'lar, beyazlatma, kovaryans güncellemeleri, yerel düzlem dönüşümü, belirlenimcilik, dinamik bellek ve `double` yönünden sorun bulunmadı. Sorun düzey mantığında: kapıdan geçemeyen ölçümle yapılan sıfırlama (`KES_N_RESET` yolu) desteği yeniliyor ve OK histerezisini baştan başlatmıyor; yatay hız geçerliliği kaynaklar arasında VEYA ile kuruluyor. Tutarsız ölçüm altında düzey `OK` kalıyor (K21: yanlış gevşetme).

## Düzeltilmesi gerekenler

| # | Yer | Sorun ve dizi | Önem | K21 |
|---|---|---|---|---|
| 1 | `core/src/kestirim/kes_yatay.c:442-464` (DVL), `:523-534` (GNSS hızı), `:593-602` (GNSS konumu); `kestirim.c:705-713` | Sıfırlama yolu reddedilmiş ölçümle `destek_ns = now` yazıyor, `red_*`'ı temizliyor, `ok_aday`'a dokunmuyor. `KES_N_RESET`·T < min(`KES_RED_MS`, `T_*_STALE`) olduğunda düzey hiç düşmüyor. Çalıştırıldı: dalış, yalnız DVL 10 Hz, t ≥ 80 s'de x'te kalıcı +1 m/s sapma → 80–200 s'de OK dışı adım 0; OK iken hız hatası 1,007 m/s (`SIG_HVEL` 0,2), konum hatası 118 m (σ_h 8 m). 5 Hz'de düzey 80,97 s'de düşüyor, sıfırlamadan 2 s sonra OK'a dönüyor ve 118 m hatayla OK kalıyor. `KES_N_RESET` = 2 ya da 10 Hz GNSS'te konum için de aynı yol (okuyarak). | kritik | evet |
| 2 | `kestirim.c:672-681` | `yatay_hiz` = `hiz_dvl \|\| hiz_gnss`. İki nitelikli kaynak sürekli çelişince birinin reddi düzeyi etkilemiyor. Çalıştırıldı: yüzey, GNSS 5 Hz + DVL 10 Hz, t ≥ 40 s'de DVL x'te +0,5 m/s → 40–100 s'de OK dışı adım 0; 43 hız sıfırlaması; OK iken hız hatası 0,515 m/s. | kritik | evet |
| 3 | `kestirim.c:819-820`; `ida_kes_param_grup` | `ORIJIN_LAT`/`ORIJIN_LON` değişince `nav.origin_*` yeni değeri alıyor, `p` eski orijine göre kalıyor; `konum_var` ve `ok_aday` sıfırlanmıyor. Çalıştırıldı: OK iken `ORIJIN_LAT` += 2000 (≈ 22 m) → 230 adım OK sürüyor, `p_ned` 22 m kaymış. | önemli | evet |
| 4 | `kes_yatay.c:365`, `:491-493` | Nitelendirme yeniden başlarken `red_isle(false)` süren ret koşulunu kabul edilmiş örnek olmadan siliyor; konum DVL ölü hesabıyla yeniden geçerli olabiliyor (varsayılanlarda pencere `KES_OK_MS`'ten kısa; parametreye bağlı). Koşturulmadı. | küçük | parametreye bağlı |
| 5 | `kes_yatay.c:381-401` | Sensör kovaryansı çok büyük ama FOM küçükse beyazlatma NIS'i ≈ 0 yapıyor: bilgisiz örnek kapıdan geçip desteği yeniliyor. Koşturulmadı. | küçük | kısmen |
| 6 | `kes_yatay.c:436,518,552,588` | Destek zamanı ölçüm zamanı değil füzyon anının duvar saati; bayatlık sınırı en çok ≈ 200 ms + taşıma süresi kadar uzuyor (dilim 1 ile aynı kalıp). | küçük | hayır |
| 7 | `kes_ic.h` (seyreltme yorumu) | "Manyetometre ≤ 25 + basınç ≤ 12 ≤ 32" yanlış (toplam 37); en uç hızlarda ve D = 200 ms'de `olc` kuyruğu taşar. | küçük | hayır |
| 8 | `core/test/kestirim/test_kestirim_yatay.inc` | 1–3'ü yakalayan test yok (10 Hz DVL ile ≥ `N_RESET` ret; çelişen iki kaynak; orijin değişimi). K5'in kalıcı GNSS sıçraması alt testi düzeyin izlediği yolu denetlemiyor. Yalnız GNSS'li hızlanmada hata / ufuk σ'sı = 3,4 (düzeyin baktığı σ = 0,048). | önemli | eksik sınama |

## Denetlenip sorun bulunmayanlar

Jacobian'lar (elle türetildi); beyazlatma ve sıralı güncelleme; kovaryansın pozitif tanımlılığı; yerel düzlem (ikinci mertebe terimleri, ±180° sarma, sınırlar); A4-K33 örnek kabulü (NaN, geçersiz, geri giden `t_meas` kestirime ulaşmıyor); oturum ve yeniden hizalamada eski desteğin taşınmaması; geçerlilik biti düşük örnek; histerezis sayacının öteki yollarda sıfırlanması; belirlenimcilik; `double` yok; seyreltmenin A4-K36 ile çelişmemesi; gevşetilen 6 m konum sınırı (hata örtmüyor). Doğrulanmadı: yığın zincirinin toplamı; Z1 değişim raporu; `fw-release`.
