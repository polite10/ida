# W5-bagla-a2 incelemesi (PR #51)

- İnceleyen: `ida-inceleyici` (Opus; sol sınırı dolu, K25/K27; sol inceleme borcunda).
- İncelenen uç: `a865fcb`.
- Hüküm: birleştirilebilir; K21 sınıfı bulgu yok; dört küçük bulgu.
- Sınıf: emniyet zincirine dokunmaz (K23): tek inceleme, tek düzeltme turu.

## Bulgular ve sonuçları

| Bulgu | Sonuç |
|---|---|
| K-1 `viz/calistir.sh --sim`: terminalde Ctrl-C hem `ida_sim`'e hem betiğe gider; betiğin tuzağı ikinci SIGINT yollar, sim ikinci sinyalde `_exit` ile çıkar, veda ve özet satırı kaybolur | Düzeltildi: INT/TERM tuzağı sinyali yinelemez, yalnız sim'in bitmesini bekler (en çok 10 s; dolarsa tek TERM). Godot kendi kapanırsa sim'e tek SIGINT gider (eskisi gibi). |
| K-2 `--sil-hiz` ile senaryonun `sil_hiz_carpani`/`sil_azami_hiz` alanları | Mimar kararı W5-K38: `--sil-hiz` komut satırı geçersiz kılmasıdır ve kalır; senaryo alanları hâlâ uygulanmaz (ayrı iş). Bayrak verilince uyarı metni gerçek durumu söyler; bayraksız metin ve çıktılar aynı. `sim/sunucu/BENIOKU.md`'ye yazıldı. |
| K-3 A3 `kademe_gorunum` A2 nesnesi var ama dinlemiyorken (kilit başkasında, port dolu, tanım yok) senaryonun kademesini yazıyordu | Düzeltildi: yalnız A2 dinliyorken yazılır, yoksa 0; `butunleme.a2_kapanis` port dolu ve kilit başkasında durumlarını A3 anlık görüntüsünden doğrular. |
| K-4 yalnız sınama seçenekleri yardım metninde; `--a2-port` ile `--a2-sinama-bos-port` birlikte verilince sessizce biri kazanıyordu | Sınama seçenekleri yardım metninden çıkarıldı (seçenekler ve BENIOKU notu kalır); birlikte verilmesi kullanım hatası (`butunleme.ida_sim_a2_port_cakisma`, `_ters`). `viz/BENIOKU.md` değişikliği kabul edildi. |
| Gösterilebilir senaryo: `gorev-havuz.toml` ikili dosyası olmayan `assets/ornek` kümesini gösterir | Yeni `scenarios/gorev-havuz-3b.toml` (`havuz` kümesi); var olan senaryolar değişmedi. `butunleme.senaryo_3b_dosyasi` ve `butunleme.a2_3b_senaryo`. |

## Açık kalanlar (düzeltilmedi; açık bulgu)

a. A2 dinleyicisi `yeni_calistirma`'dan önce açılır: A3 bekleme kipinde ya da uzun hazırlıkta erken bağlanan istemci el sıkışmayı tamamlayamaz, zaman aşımı/yeniden bağlanma döngüsüne girebilir; testi yok.
b. `sim/a2` G/Ç iş parçacığında istisna yakalanmıyor: orada fırlayan istisna bütün `ida_sim` sürecini `std::terminate` ile düşürür (HIL'de kart kendi bağlantı kaybı korumasıyla kalır; K21 sayılmadı).
c. İstemci bağlıyken gerçek SIGINT'li süreç testi yok.
d. Varsayılanın (`--a2-port` yokken) bayt karşılaştırması elle yapıldı, CI testi değil.

## İnceleyicinin doğrulamadıkları

Bütünleme testleri, Windows derlemesi, Godot koşusu, K-1'deki yarış.
