# sol incelemesi: gudum

## Hüküm

**İki emniyet bulgusu tamamen kapanmadı.** İnceleme yalnız ilk rapordaki 1 ve 2 numaralı bulgularla sınırlıdır; testler okundu, çalıştırılmadı.

1. **Kapanmadı:** daraltma yollarında kuşak artırılıyor; fakat tam sayaç turunda eski kuşak yeniden eşleşebiliyor (#1).
2. **Kapanmadı:** olağan DISARM → ARM, oturum yenileme ve kip daralması türevleri kapanmış; sayaç turu karşı örneği kalıyor (#1).
3. **Kapanmadı:** HOLD girişinde referanslar ve öteleme integralleri yenileniyor; derinleşme yasağı sürerken eski referanstan yeniden dalış isteği üretilebiliyor (#2).
4. **Kapandı:** değişikliklerde komutsuz silahlandırma veya kip genişlemesi bulunmadı (`core/src/gorev/gorev.c:308`, `core/src/gorev/kip.c:30`).

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `core/src/emniyet/emniyet.c:29`; `core/src/gudum/gudum.c:158`, `:286` | **Tam sayaç turu eski hedefi canlandırır.** Saklı GUIDED_SP varken, zaman ilerletmeden 2³² adet `out=NULL` DISARM çağrısı yapın. Güdüm çalışmaz; kuşak başlangıç değerine döner. Sonraki geçerli ARM çağrısında eski hedef eşitlik denetimini geçer. Tümleyen de doğru olduğundan bunu yakalamaz. Bu, statik API karşı örneğidir; donanımda erişilebilirliği ölçülmedi. | Daraltmada hedef geçersizliğini kalıcı mandallayın veya kuşak yeniden kullanılmadan güvenli kapanma uygulayın. Yalnız sayaç genişliğini artırmak mutlak güvence değildir. | kritik |
| 2 | `core/src/gorev/gorev.c:230`; `core/src/gudum/gudum.c:196`; `core/src/kontrol/kontrol.c:419` | **Derinleşme yasağı yalnız başlangıçta referansı yeniliyor.** Yasak sürerken araç referansın 0,25 m/s sınırından hızlı yükselip durursa `d_ref` ölçülen derinliğin gerisinde kalır. İstek mevcut derinliğe kırpılmış olsa da kontrol yeniden pozitif dalış hızı üretir. Aynı yol görev tutma hedefinde de vardır (`core/src/gorev/istek.c:29`). | Yasağı kontrolün ürettiği dikey hız isteğine kadar taşıyın; yasak boyunca pozitif dalış isteğini engelleyin ve integral yönetimini buna bağlayın. Hareketli derinlik karşı örneğini ekleyin. | kritik |

## Yeniden hesaplananlar

- **Sayaç:** `(k + 2³²) mod 2³² = k`; tümleyen eşleşmesi korunur. İşaretsiz aritmetik kuralı birincil [WG14 N1570 §6.2.5/9](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) üzerinden denetlendi.
- **Derinleşme yasağı:** başlangıç derinliği 3 m; 200 adımda ölçülen derinlik 2 m’ye inerken referans yalnız `200×0,25×0,005=0,25 m` azalır: `d_ref=2,75 m`. Araç durduğunda `w_sp=0,8×0,75−0,25=+0,35 m/s`. İntegral sıfır olsa bile kuvvet isteği `120×0,35+1,82+80×0,35=71,82 N` aşağıdır.
- **Referans toplamı:** mevcut kod üç ayrı sayaç toplamıyor; `gor.ref_kusak + gud.iptal` kullanıyor (`gudum.c:280`). Olağan akışta ikisi de yalnız artıyor; zıt değişimle gizlenme bulunmadı. Bozulma sıfırlaması DISARM mandalıyla, yeniden kurulum kontrol belleğinin temizlenmesiyle birlikte.
- **Açısal integraller:** korunmaları geçişte artık moment üretebilir; örneğin sıfır hız hatasında `i_r=4 N·m`, 4 N·m moment ister (`kontrol.c:488`). Bu tek başına HOLD ihlali kanıtı değildir; mevcut derinlik testleri yön geçişinin güvenliğini doğrulamıyor.

## İskelete ya da diğer bölümlere notlar

- Mandal ve `cmds[]` DISARM, SESSION_END, daraltıcı kip, görev silahsızlandırması ve kip düşmesi yollarında kuşak bağlantısı mevcut. `DUP_SEQ`, `OLD_SEQ`, `BAD_INPUT` ve `out=NULL` dönüşlerinden önce uygulanıyor. `NOT_INIT` durum okumuyor; başarılı yeniden kurulum hedefleri ve kontrolü birlikte temizliyor.
- Tümleyen bozulması üzerine yazılmadan önce kaydediliyor (`core/src/cerceve/cerceve.c:521`).
- N2176 kaynağına erişim zaman aşımına uğradı; doğrulanmış sayılmadı. Dosya değiştirilmedi, kurulum yapılmadı.