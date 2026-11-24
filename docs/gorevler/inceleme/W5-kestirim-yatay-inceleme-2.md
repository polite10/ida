# Bağımsız dar denetim: W5-kestirim-yatay düzeltme turu (`ae089f4..827c2f4`)

İnceleyen: `ida-inceleyici` (Opus; taze bağlam; sol yerine, kullanıcı kararı K25). Kaynaklar `827c2f4`'ten depo dışına çıkarılıp derlendi; 54 test geçiyor; prob ve on mutasyon koşusu yapıldı; depoda dosya değişmedi.

## Hüküm

İlk incelemenin 1, 3, 4, 5 ve 7. bulguları kapandı. Bulgu 2 yalnız varsayılan parametrelerde kapandı. Düzeltmeler yeni bir K21 yolu açıyor (çalıştırılarak doğrulandı): hız bir kaynağa sıfırlanınca o kaynağın bir sonraki örneği kapıdan kendiliğinden geçiyor (durum artık ona eşit), bu geçiş kuşkuyu kaldırıyor ve destek veriyor; öteki kaynak yeni duruma karşı henüz `KES_N_RED` ret biriktirmemiş oluyor. Bu aralıkta VE koşulu sağlanıyor ve düzeyi `OK`'tan yalnız `KES_OK_MS` alıkoyuyor.

## Düzeltilmesi gerekenler

| # | Yer | Sorun ve dizi | Önem | K21 |
|---|---|---|---|---|
| 1 | `core/src/kestirim/kes_yatay.c:530-533`, `:598-601`; `kestirim.c:694-709` | Yüzey, GNSS 5 Hz + DVL 10 Hz; t ≥ 40 s'de DVL x'te kalıcı +1 m/s. 41,060 s hız DVL'e sıfırlanıyor; 41,155 s ilk GNSS reddi; 41,160 s DVL kabul ediliyor, `kusku_dvl` kalkıyor, geçerlilik 0x3f (yatay hız ve konum geçerli; hız hatası 1,0 m/s); GNSS'in üçüncü reddi 41,555 s. Bu ≈ 0,4 s'lik pencere her çevrimde (≈ 1,4 s) yineleniyor. Varsayılanlarda düzey `OK` olmuyor ama yatay hız geçerliliği 1 m/s hatayla kurulu kalıyor. Aralık içi parametrelerle `OK`: `KES_OK_MS` = 100 ms'de 1239 çağrı, 200 ms'de 819, 300 ms'de 399 (hepsinde hız hatası 1,0 m/s); `KES_N_RED` = 15, `KES_N_RESET` = 20, `KES_RED_MS` = 5000 ve varsayılan `KES_OK_MS` ile 1590 `OK` çağrısı. | önemli | evet (parametreye bağlı) |
| 2 | `core/test/kestirim/test_kestirim_yatay.inc:1713-1750` (`test_y_r2_celisen_kisa_histerezis`) | Test "düzeyi tutanın yalnız süre olmadığını" sınadığını söylüyor ama göstermiyor: 1000 ms'de geçmesinin nedeni sürenin pencereden uzun olması. Tarama `KES_OK_MS` alt ucuna (100) ve büyük `KES_N_RED`/`KES_RED_MS` durumuna inmeli. | önemli | eksik sınama |
| 3 | `kes_yatay.c:435-438`; testler | İlk incelemenin 4. bulgusunun DVL tarafı düzeltilmiş ama hiçbir test sınamıyor (mutasyonda 54 testin hiçbiri düşmedi). | küçük | hayır |
| 4 | `test_kestirim_yatay.inc:824-870` (K4d) | `Y_KONUM_DENETSIZ` döngüden sonra da açık kalıyor; konum GNSS'ten yeniden kurulduktan sonra denetim açılmalı. | küçük | hayır |

## Denetlenip sorun bulunmayanlar

Susan kaynak (kötü DVL gelip gidince `OK` yalnız DVL susarken; ihlal 0); tek kabulle kalkan kuşku (kapı sınırındaki küçük sapmalarda ihlal 0; kalan hata W5-K24 sınıfı); ilk fix'le kurulumun kuşku kurması; bildirilen σ'nın önceki ölçütten gevşek olmaması; grup koşulunun zorlanması; orijin değişiminde aynı çağrıdaki fix'in yeni orijinle işlenmesi. On mutasyondan dokuzu ilgili testi düşürdü (istisna: yukarıdaki 3. satır).
