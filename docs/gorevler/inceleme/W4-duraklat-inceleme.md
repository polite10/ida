# Bağımsız inceleme: W4-duraklat (`4b7cbb0..d9b5ab6`)

İnceleyen: `ida-inceleyici` (Opus; taze bağlam). sol'un kullanım sınırı dolduğu için kullanıcı kararı K25 ile sol yerine yapıldı; sol inceleme borcu listesindedir. Fark okundu; kaynak ağacın dışında Debug derleme yapıldı; köprü `duran_*` (6) ve bütünleme `duraklat_*` (11) testleri ile iki prob koşturuldu.

## Hüküm

K21 sınıfında (bir emniyet girdisinin uygulanmaması, geç uygulanması ya da yanlış gevşetme) bulgu yok. Denetlenen yollar: duraklatmaya giriş ve çıkış turları, aynı turda "acil bas + sürdür/adımla", `adimla`'nın iç geçişi, HIL duraklatmasında `SESSION_CLOSE`, `SAFETY_CLOSE` ve eşzamansız güvenli çıktı, daraltıcıların revizyonu, revizyon denetiminin üç depoda da yapılması, W4-K3. Tek yol iddiası doğru: duran kip `sinir_turu(b, duran)` ile `Kopru::sinir_isle` üzerinden geçiyor; ikinci bir emniyet ya da tüketim döngüsü kalmamış.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Önem | K21 |
|---|---|---|---|---|
| 1 | `tests/butunleme/test_duraklat.cpp:829-830` (`duraklat_hil_ilk_tf`) | `(calistirma_gecersiz & kGecEszamansiz) == 0` koşulu koşunun tamamında aranıyor; sürdürmeden sonra karşı taraf `batch_seq` ≠ 0 taşıyan eşzamansız çıktı gönderirse bit meşru olarak kuruluyor. Yüke bağlı yarış: Debug'da 4 koşudan 1'i kaldı ("uygulanan 351, ilk parti öncesi 347"). | düşük | hayır |
| 2 | `sim/sunucu/src/sunucu.cpp:2619` | `adimla` tamamlanınca dünya iş parçacığı sınır döngüsünün içinde `gecis_kilidi()` alıyor; `a3_baglanti.hpp:180-184` ve W4-K27 (5) "sınır döngüsünde kilit alınmaz" diyor. Kilit yalnız SIL'de ve kısa süre tutuluyor. | düşük | hayır |
| 3 | `tests/butunleme/test_duraklat.cpp:451, 488, 516, 859` | "Önce nötr dışı komut yürürlükteydi" ön koşulu duraklatmadan önceki adımda denetleniyor; çıkış turunun girdisinin kısıtlama olmasaydı nötr dışı olacağı sabitlenmemiş (bugün boş geçmiyor: müdahalesiz koşuda 3999–4012 adımları nötr dışı). | düşük | hayır |
| 4 | `sim/sunucu/BENIOKU.md:126` | "Müdahalesiz duraklatma/adımlama çıktıyı değiştirmez" iddiası fazla genel: giriş turunda alınmış, yuvası ileride bekleyen bir `SAFETY_CLOSE` duran turda hemen uygulanır ve çıktı duraklatmasız koşudan ayrılır (davranış kabul edildi; yalnız iddia dar yazılmalı). | düşük | hayır |

## Denetlenip sorun bulunmayanlar

Aynı turda acil bas + sürdür/adımla (fizik nötr girdiyle koşuyor; HIL çıkış turu kart çıktılarını tüketiyor); W4-K3 (bas → bırak → sürdür); genişletici bekleyen komutun revizyon denetimi üç depoda (prob: araya bas → bırak girince `arm` tele çıkmıyor); kısıtlayanların `mesgul` almaması ve düşmemesi; HIL duran turunda izin ve revizyon kapanışları, daraltıcının sürdürmede ilk TF'den önce yazılması; `adimla`'nın tam 4n fizik adımı ve zaman çizelgesinin her sınırda bir kez yürümesi; iş parçacığı yarışları; belirlenimcilik ve bellek (duraklatma kullanılmayan dört koşu tabanla bayt bayt aynı). Doğrulanmayan risk: uzun HIL duraklatmasında olay sonuçları tele çıkmadığı için sonuç kuyruğu (8) taşarsa sim oturumu protokol hatasıyla bitirir (A1-K111; kısıtlayan yönde). Node testi, MSVC, `host-asan` ve kart koşusu yapılmadı.
