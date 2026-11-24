# W5 — ortak kararlar (atış kontrol emniyet zinciri, görev yürütücüsü)

Tarih: 2026-11-24. Bu belge W5 işlerinin ortak kurallarını ve mimar kararlarını toplar. `docs/gorevler/W3-ortak.md` ve `docs/gorevler/W4-ortak.md` geçerliliğini korur. İnceleme düzeni K21'e, hız kuralları K20'ye göredir.

## Kapsam sınırı (kullanıcı kararı; her W5 işinde geçerli)

Atış kontrol içeriği **yalnız emniyet zinciri ve arayüzlerle** sınırlıdır: silahlandırma, atış izni, acil durdurma, sanal emniyet girdileri, bunların durum makinesi, öncelik ve revizyon kuralları, olayların kimliği ve geçerliliği, güvenli duruma dönüş, taşıma, kayıt. Hedefleme çözümü, önleme/vuruş hesabı, atış ölçütü ya da karar eşiği, hedef algılama/sınıflandırma algoritması ve torpido başarımının iyileştirilmesi yazılmaz ve ayrıntılandırılmaz. Güvenlik durdurması dolanılmaz.

## Mimar kararları

Kaynak: mimari Bölüm 03'ün dar düzeltme turu (sürüm 0.2; `docs/mimari/03-atis-kontrol.md` §2.6.1). Hepsi en kısıtlayan okumadır; dondurulmuş sözleşmelerle çelişmez, çeliştiği yerde sözleşme geçerlidir ve çelişki mimara raporlanır.

- **W5-K1 — Kilitli arızadan çıkış (AB-135).** `FC_RESET` kabul edilir (A4 §3.17'nin ret nedenleri kapalı listedir; "koşullar temiz" ön koşulu ret nedeni olamaz); arızanın nedeni aynı çağrının sonunda sürüyorsa atış kontrol aynı çağrıda yeniden `FAULT_LATCHED` olur. `FC_RESET` hiçbir izni açmaz ve silahlandırmaz.
- **W5-K2 — Olay sonucuna göre geçişler (AB-136).** `SAFETY_OPEN` için `APPLIED` dışındaki her sonuç (ret, süre dolumu, sonucu bilinmeyen) → o tüp için `SAFETY_CLOSE` ve `SAFE`. `FIRE` için `REJ_TUBE_EMPTY`, `EXPIRED_NO_ACK` ya da sonucu bilinmeyen → `FAULT_LATCHED`; öteki retler → `SAFE`. Sonucu bilinmeyen bir genişletici olay hiçbir zaman "uygulanmış" sayılmaz.
- **W5-K3 — Durumlar ve kabul edilen komutlar (AB-138).** İç durumlar A4 §3.16'nın altı değerine eşlenir. `FC_ARM` yalnız `SAFE`'te; `FC_FIRE` yalnız `ARMED`'de ve silahlı tüp için; `FC_TEST_ENTER` yalnız `SAFE`'te kabul edilir. Süreli izin yalnız kabul edilmiş bir operatör `FC_FIRE`'ından sonra açılır (kart kendiliğinden izin açmaz, kendiliğinden atış olayı üretmez). İzin penceresi dolunca ve değerlendirme bitince `SAFE`'e dönülür. `TEST` durumunda `FIRE` üretilmez. Daraltıcı komutlar (`FC_DISARM`, `FC_ABORT`) her durumda kabul edilir.
- **W5-K4 — Emniyet örneğinin bayatlığı ve çelişki (AB-139).** Emniyet örneği `T_emn_bayat` süresince yenilenmezse acil durdurma basılı sayılır (kısıtlayan varsayım). `T_emn_bayat` emniyet sınıfı bir parametredir (W5-K6); varsayılanı 100 ms'dir (20 kontrol periyodu; **tahmin**: kartta Z2 koşularındaki en uzun emniyet örneği aralığı ölçülerek doğrulanacak; ölçüm çıkmadan kesinleşmiş sayılmaz). Gerçek çelişki yalnız iki durumdur ve ikisi de `FAULT_LATCHED`'dır: kapatma `APPLIED` olduktan sonra açılmamış tüpte izin açık görünmesi; `FIRE` `APPLIED` olduktan sonra tüpün dolu görünmesi. "Fiş takılı ve acil durdurma basılı" çelişki değildir (kısıtlayan olağan durum).
- **W5-K5 — Emniyet iddiasının sınırı ve sim'in atış olayını kabulü (AB-148).** "Tek kart arızasına dayanıklılık" iddiası yalnız en az bir sanal engelin kapalı olduğu işletim durumları için yapılır; bütün engeller açıkken kart yazılımındaki tek bir hata simüle atışa yol açabilir ve bu, belgede sınır olarak yazılır. Ek kural (sim tarafı; tel değişmez): sim, bir `FIRE` olayını yalnız kendisinin karta ilettiği ve kartın kabul ettiği bir operatör `FC_FIRE` komutuyla (aynı tüp, geçerlilik süresi içinde) eşleştirebiliyorsa uygular; eşleşmeyen `FIRE` reddedilir (`REJ_SAFE`) ve olay olarak kaydedilir. A1'in küçük sürüm notudur (A1-K85–K89).
- **W5-K6 — Emniyet zinciri parametrelerinin sınıfı (AB-150).** Emniyet zinciri parametrelerinin hepsi `IDA_PCLS_B` sınıfındadır (yalnız silahsızken değişir); silahlıyken değiştirme isteği reddedilir.
- **W5-K7 — Biçimi bozuk daraltıcı komut (A1-K64 ile iskelet §3.2 ve A4 §3.17 arasındaki gerilim).** `FC_DISARM` ve `FC_ABORT` hiçbir biçim denetimiyle reddedilmez: `tube` alanı geçersiz ya da 255 dışı bir değer taşıyorsa komut **bütün tüpler için** (en kısıtlayan kapsamla) uygulanır ve biçim hatası sonuçta ayrıca bildirilir. A1-K64'ün küçük sürüm notudur.

- **W5-K8 — Kilitli arıza hiçbir güvenli duruma dönüşle temizlenmez (Bölüm 03 sol incelemesi #1).** Oturum sonu, kalp atışı kaybı, emniyet örneğinin bayatlaması, acil durdurma, fiş/pim ve öteki bütün "güvenli duruma dön" geçişleri silahlandırmayı ve izinleri kaldırır ama var olan `FAULT_LATCHED` durumunu KORUR; kilitli arızadan yalnız `FC_RESET` ile çıkılır (W5-K1). `SAFE`'e geçiş anlatan her kural bu istisnayla okunur (A4 §3.16, A4-K73).
- **W5-K9 — `TEST` durumunda izin açılmaz (sol #2).** `TEST` durumunda `SAFETY_OPEN` de `FIRE` de üretilmez; kuru sınama yalnız iç durumun ve geri okumaların gözlenmesidir. İzin açmanın tek yolu `ARMED` durumunda kabul edilmiş operatör `FC_FIRE`'ıdır (W5-K3).
- **W5-K4 eki (sol #4).** Bayatlık iki ölçütle ayrı ayrı izlenir (A4-K34, K36): (i) son kabul edilen emniyet örneğinin alımından beri geçen süre; (ii) örneğin ölçüm zamanının (`t_meas`) ilerlememesi: `seq` ve alım zamanı yenilense de ölçüm zamanı `T_emn_bayat` boyunca ilerlemiyorsa örnek bayattır. İkisinden biri sağlanırsa acil durdurma basılı sayılır.
- **W5-K10 — Daraltmanın gerçekleştiği çağrı ve sonuç kodları (sol #5, #6).** Daraltıcı etki her durumda uygulanır; bu, aynı çağrıdaki komutların SONUÇ KODUNDAN ayrıdır: sonuç kodu A4 §3.17'nin öncelik sırasıyla verilir (yineleme denetimi önce: yinelenen komut `DUPLICATE`; reddedilen çağrıdaki daraltıcı olmayan komut `CALL_REJECTED`; kabul edilen çağrıda daraltmayla karşılaşan yeni genişletici `NARROWED`; A4-K45, K48, K113, K114). Bir tüpün fişi ya da pimi öteki tüpü yalnız SÜREGELEN kısıtlama olarak etkilemez; kısıtlayana dönüşün gerçekleştiği çağrıda bütün tüplerin yeni genişleticileri engellenir (A4-K44, K116; iskelet §3.2), sonraki çağrılarda kapsam tüpe özgüdür.

## Sözleşmelerin küçük sürümüne yazılacak notlar

| Sözleşme | Madde | Not | Kaynak |
|---|---|---|---|
| A1 | A1-K64 | Biçimi bozuk daraltıcı komut bütün tüplere uygulanır | W5-K7 |
| A1 | A1-K85–K89 | Sim `FIRE`'ı yalnız kabul edilmiş operatör `FC_FIRE`'ıyla eşleşiyorsa uygular | W5-K5 |
| A4 | §3.16, §3.17 | Durum eşlemesi, kabul edilen durumlar, `FC_RESET` davranışı, olay sonucuna göre geçişler | W5-K1, K2, K3 |
| A4 | parametre tablosu | `T_emn_bayat` (sınıf B; varsayılan tahmin) | W5-K4, K6 |

## Açık konular

- `T_emn_bayat` varsayılanı ölçülmedi (W5-K4).
- Bölüm 03'ün kapsam dışı bırakılan 12 bulgusu (hedefleme, algılama, takip) kullanıcı kararıyla açık kalır ve ele alınmaz.
- Bölüm 03'ün başka işe bağlı üç bulgusu: AB-149 (atış kontrolün iç eşzamanlılığı), AB-151 (M5 ölçümü), AB-153'ün kalanı.
