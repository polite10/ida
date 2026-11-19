# A1 — sol ikinci incelemesine yanıt (düzeltme turu 2)

| | |
|---|---|
| İnceleme | `docs/sozlesmeler/inceleme/A1-sol-2.md` (ilk 13 maddeden 7'si giderildi, 6'sı kısmen; 10 kritik yeni bulgu) |
| Düzeltilen | `docs/sozlesmeler/A1-kart-baglantisi.md` (1.0-aday, düzeltme turu 2), `protocol/ida.xml` |
| Dayanak | Mimarın düzeltme turu 2 kararları (10 madde); A4 düzeltme turu 2 adayı (`docs/sozlesmeler/A4-port.md`, `core/include/ida/port.h`, commit `36e0eba`; salt okunur) |
| Tarih | 2026-11-20 |

Kural numaraları korundu; bu turda eklenen kurallar A1-K116 … A1-K122'dir (toplam 122 kural). Eşitsizlikler E1–E19'dur (E19 yeni).

## Yeni bulgular

| # | Önem | Durum | Yer | Gerekçe |
|---|---|---|---|---|
| 1 | kritik | düzeltildi | Başlık "Atış kontrol sınırı"; 3.6 Tablo 3.6a; A1-K64, yeni K122; `ida.xml` `IDA_MI_TYPE`, `IDA_CMD_MISSION_ITEM.p`; 8.2 A4 md. 9; Ek A | `IDA_MI_TYPE_TARGET_ALIGN` çıkarıldı. Görev öğesi türleri kapalı tablodur (1–7); her türün `frame`, `depth` ve `p[0..3]` anlamı, birimi ve aralığı tek tek tanımlı; tanımsız parametre tam 0 olmak zorunda; kart her koşulu denetler ve aykırıyı `LINK_BAD_FIELD` ile reddeder; hiçbir parametre çekirdek uygulamasına bırakılmadı. Kalan türler seyir ve dalış görevleridir. Yorumum: `SCAN_LINES` ve `SCAN_POINT`'i yalnız yol ve bekleme tanımı olarak tuttum (hedefe ya da algılamaya göre davranış taşımazlar); mimar bunları da çıkarmak isterse tablodan iki satır silinir. |
| 2 | kritik | düzeltildi | 3.2 `dialect_hash`; 7.3 | Özet en son adımda yeniden üretildi: **37 + 334 + 186 = 557 satır, `0x104154A1E68773C8`**. Betik artık M/F/E satır sayılarını XML'deki mesaj, alan ve numaralandırma girdisi sayılarıyla karşılaştırıyor; aynı betik önceki XML için sol'un değerini (534 satır, `0x4B9E97C1439CF7FE`) yeniden üretti. Hatanın nedeni: ilk turda özet `TX_OVERFLOW` girdisi eklenmeden önce hesaplanmıştı. |
| 3 | kritik | düzeltildi | A1-K106 (yeniden yazıldı); `ida.xml` `IDA_SAFETY.safety_rev`, `IDA_EVENT.safety_rev`; 8.2 A3 md. 0; H49 | Tek sayaç, sahibi dünya: `uint32`, sim süreci başında 1 (0 = platform bu oturumda örnek görmedi), süreç boyunca tekdüze, çalıştırma ve oturumda sıfırlanmaz. Artıranlar: kabul edilen her kısıtlayan komut (durumu değiştirmese de), her fiilî gevşetme, süresi dolan izin, uygulanan `SAFETY_CLOSE`, oturum sonu ya da kopması, kart sıfırlanması, yeni çalıştırma, `sifirla`; kartın izin açması ya da yenilemesi artırmaz. `0xFFFFFFFF`'te doyar; sonra her gevşetme ve izin açan olay sim yeniden başlayana kadar reddedilir. Platform her oturumda son gördüğü revizyonu 0'a çeker; 0 damgalı izin olayı reddedilir. A3'e genişlik ve başlangıç değeri yazıldı (A3 `u64`'ten bu tanıma çekilecek). |
| 4 | kritik | düzeltildi | Tablo 4.6a (dönüş kodu × çıktı bayrakları; kaynak A4 §4.14 tur 2), Tablo 4.6b (olay türü × çağrı türü; A4-K64), yeni A1-K118, A1-K83; `IDA_ACT_FLAGS` 0x03FF | `TICK_TF`/`TICK_GRID` ile gelen `SAFETY_OPEN`/`FIRE` reddedilir; `core_rc` hata iken `THR_VALID = 1` reddedilir; izin açan olay ancak aynı partinin yanıtı geçerli ve tutarlıysa (`OK`, step; `FIRE` için `ARMED = 1`, `IS_SAFE = 0`) uygulanır, değilse kalıcı `REJ_SAFE`. Çelişkili çerçeve K109'un tek ret ve etkisizlik yoluna girer. |
| 5 | kritik | düzeltildi | A1-K81, K111 (yeniden yazıldı); E8, E16; 3.9 (`T_bekle` 30 ms, aralık 10–35 ms; `T_sonuç_pay` 95 ms) | (a) Yeni sonuçlar kuyruğa girer, hiç düşmez; ilk dördü karar anında içinde bulunulan pencereye hemen yazılır, kalanı sonraki pencerede. (b) Saklı sonuç talepleri kuyruğa girmez; partide en çok 1, yeni sonuçlardan sonra; sığmayan atılır ve sayılır. E16 yalnız (a) için: her giriş kartta açık bir olaya aittir (≤ `N_olay` = 8). E8 aynı kanıttan: uygulanan bir olayın sonucu `t_d + apply_by + 2·T_bekle + P + b + RTT_maks`'tan önce kartta. İki partilik teslim için gereken `2·T_bekle + 20,36 ≤ T_sonuç_pay < 100` koşulu `T_bekle` aralığını 10–35 ms'ye indirmeyi gerektirdi (varsayılan 30 ms; iskeletin başlangıç değeri 50 ms tahmindi). Köşe sınaması: 70 + 5 + 14,5 + 0,86 = 90,36 ≤ 95 < 100. Reddedilen olayın sonucu geç kalabilir; çekirdek o zaman olayı "sonucu bilinmiyor" sayıp güvenli duruma geçer (A4-K70). |
| 6 | kritik | düzeltildi | Yeni A1-K117; 3.9 bütçe tablosu ("Nitelik" sütunu: ölçüm / hesap / ölçümden türetilmiş tahmin / tahmin); E18; `ida.xml` `IDA_ACT_FLAGS_CALL_CONFLICT`; 3.11a `cagri_cakismasi`; 8.2 A4 md. 10 | Izgara tick'i, oturumda TF akarken yalnız beklenen TF `E + 1 ms`'ye kadar gelmezse başlar; `[E − 1 ms, E + 1 ms)` içinde hiç başlamaz. TF geldiğinde başka çağrı sürüyorsa parti `CALL_CONFLICT` taşır, sayılır ve çalıştırmayı geçersiz kılar. E18 "çakışmasız partiler için" yazıldı: 9,733 ms (pay 0,267 ms, tahmin). Çakışmalı partinin en kötüsü açıkça veriliyor: tick sürerken 10,233 ms, aşan step sürerken 13,233 ms; ikisi de güvence dışı. |
| 7 | kritik | düzeltildi | A1-K110 (yeniden yazıldı), yeni K119 (sim komut kovası), K120 (platform olay kovaları); A1-K61, K35, K80; E17; 3.9 (`S_ii`, `Q_ii` 64, kovalar) | Geliş ve hizmet aynı saatte (kart duvarı), bütün aralıklar için. Geliş: komut sonuçları ≤ 36 + 1,2·τ (sim kovası + titreşim + erteleme; birleştirme, mandal sonuçları ve `DUPLICATE` dahil); yeni olaylar ≤ 7 + 1,4·τ (iki kova); yinelemeler ≤ 8 + 0,533·τ; bayt olarak α(τ) ≤ 2 154 + 141,5·τ. Hizmet: (ii)'ye ayrılmış 200 B/ms, gecikme payı 2 ms. Sınırlar: kuyruk ≤ 2 437 B ≤ 59 çerçeve, bu yüzden `Q_ii` = 64; gecikme ≤ 12,8 ms. `SAFETY_CLOSE` kendi kovasında atılmaz, bekletilir (bu benim tasarım seçimimdir; kapatıcının düşürülmesini istemedim). |
| 8 | kritik | düzeltildi | Yeni A1-K116; E15, E19; 3.10; O13 | Bant ölçütü kural oldu (mimar kararı 2026-11-20; iskelet 0.9'a işlenecek). Olağan profil ortalaması %60'ın altında: %19,2 (aşağı) ve %23,6 (yukarı). Bağlayıcı üst sınır %90'ın altında: aşağı yön uzun dönem %60,0, tek periyot %80; yukarı yön uzun dönem **%85,5**. Bu, E17'deki geliş hızlarının aynısıyla hesaplandı: zorunlu sınıf 141,5 B/ms, ayrılmış hizmet 200 B/ms; kaybedilebilir sınıfa 143,5 ≥ 100 B/ms kalır. Kovalar geliş hızını değiştirdiği için oran %84,27'den %85,5'e çıktı; bu yüzden `R_iii` 600'den 500 B/periyota indirildi. İlk turun varsayımlarıyla %84,27 de %90'ın altındadır. |
| 9 | kritik | düzeltildi | Yeni A1-K121; A1-K102 (yeniden yazıldı); 8.2 A4 md. 11 | SIL kilitli adım dört koşulla: (i) bütün A1 zamanlayıcıları sim zamanıyla sayılır, yalnız `T_SIL` gerçek duvar saatiyle; (ii) sıra "parti → çağrı → yanıt → sonraki parti"; birleştirme ve çakışma oluşmaz; (iii) parti başına sabit `N_bg` arka plan dilimi, kayda yazılır; (iv) tek HELLO. K102 bu koşullar altında TCP açılışından kapanışına kadar iki yöndeki bütün çerçeve dizisinin (başlık `seq` ve sağlamalar dahil) eşitliğini iddia eder; HIL'de iddia yok. Bu sınır A6'nın "yalnız SIL'de, A4 port sınırında" kararıyla aynıdır. Not: (iii), A4-K95'in "IDLE dönene kadar" kuralıyla çelişir; A4'ten değişiklik beklenir. |
| 10 | kritik | düzeltildi | 3.7 `IDA_CORE_COUNTERS`, `IDA_STATUS.health`; `ida.xml` 46067, `IDA_HEALTH`; Ek A; Tablo 4.6 | `CORE_COUNTERS` artık dizi değil: 22 adlı `uint32_t` alan, A4 tur 2'deki adlarla (`sample_conflict`, `sample_dup_dropped`, `narrow_from_rejected`, `bg_bad_input` dahil). Yük 88 B, çerçeve 100 B, `CRC_EXTRA` 23. `health` sekiz bit (yeni `sample_conflict` 64, `bg_bad_input` 128). `IDA_RC_*` 0–8 A4 ile aynı. `IDA_BG_BAD_INPUT` telde taşınmaz; A4-K117 gereği dilim sayılmaz ve sayaç ile sağlık bayrağında görünür. |

## İlk incelemeden "kısmen" kalanlar

1 (kapsam), 4 (emniyet sayacı), 5 (alanlar arası doğrulama), 9 (sonuç kuyruğu), 10 (kart kuyruğu) ve 13 (SIL eşitliği) bu turdaki yeni bulgu 1, 3, 4, 5, 7 ve 9 ile kapatıldı.

## Yeniden üretim ve doğrulama

- **Şema ve derleme:** `mavgen` (pymavlink 2.4.50, `lxml` ile şema doğrulaması) 37 mesaj buldu. C başlıkları `-std=c17 -pedantic-errors -Wall -Wextra` ve C++20 ile uyarısız derlendi.
- **Değişen boylar:** `IDA_CORE_COUNTERS` 88 / 100 B, `CRC_EXTRA` 23. Diğer mesajların boyları ve `CRC_EXTRA` değerleri değişmedi.
- **Altın diziler:** 38 çerçeve. Bağımsız kodlayıcı ile pymavlink 38/38 bayt bayt aynı; üretilmiş C başlıklarıyla 4 çerçeve aynı.
- **Kanonik özet:** en son adımda üretildi; 557 satır, `0x104154A1E68773C8`.
- **Parametre köşeleri:** beş değişken aralığın 32 köşesinde E2–E10 ve E16 sınandı; ihlal yok. E1, E11–E15, E17–E19 sabit değerlerle 3.9'da hesaplı.

## Kuyruk modeli sınaması

Model `kuyruk_model.py`, scratchpad'de; depoya girmedi. Standart kitaplıkla yazıldı; 200 rastgele deneme ve en kötü patlama dizileri koşuldu. **Model kabadır:** taşıma gecikmesi ve titreşim tek bir üst sınırla temsil edildi; bir kanıt değil, karşı örnek aramasıdır.

| Eşitsizlik | Model | Sonuç | Kanıttaki sınır |
|---|---|---|---|
| E17 | Kartın (ii) kuyruğu. Komut gelişi sim kovası + titreşim; olay kovaları; 8 açık olayın 15 ms'lik yinelemeleri aynı fazda. Hizmet: tek çerçeve, (i) > (ii) > (iii) öncelik, (iii) hep dolu | En çok 34 çerçeve, 1 348 B; en uzun gecikme 5,9 ms | 59 çerçeve, 12,8 ms; `Q_ii` 64. Karşı örnek bulunmadı. |
| E16 / E8 | Barierde bekleme 0–35 ms; adım ve ızgara tick'i olayları, kova ve 8 yer; ilk dört sonuç hemen, kalanı sonraki TF'de | Kuyruk en çok 6 giriş; açık olmayan olaya ait giriş 0; uygulanan sonucun karta varışı `t_d + apply_by` + en çok 48,4 ms | Kuyruk 8; analitik en kötü + 90,36 ≤ 95 ms. Karşı örnek bulunmadı. |
| E18 | Titreşimli ve titreşimsiz TF akışı, K117 tick kuralı, step ≤ 3,5 ms, tick ≤ 0,5 ms | Titreşimsiz akışta 0 çakışma; yasak pencerede tick başlatma iddiası (assert) hiç bozulmadı | — |

E16/E8 satırında modelin en kötü kipi her olayı reddettiği için (bekleme hep 35 ms) uygulanan olay kümesi yalnız rastgele denemelerden geldi; analitik sınır modelin bulduğundan büyüktür.

E18'in titreşimli akışında çakışma çıktı: zamanında gelen TF'de 298, geç gelen TF'de 301, sıkışık gelen TF'de 2 253. Bu beklenen sonuçtur: önceki TF geç geldiyse ya da TF'ler sıkıştıysa çakışma olur; K117 bunları sayar ve partiyi geçersiz kılar. E18 yalnız çakışmasız partileri kapsar.

## Mimarın bilmesi gerekenler

1. `T_bekle`'nin varsayılanı 50 ms'den 30 ms'ye, aralığı 10–35 ms'ye indi (madde 5'in kanıtı). İskelet §4'teki "başlangıç değeri 50 ms, tahmin" ifadesi iskelet 0.9'da güncellenmeli.
2. `R_iii` 600'den 500 B/periyota indi (madde 8).
3. Platform olay kovaları yeni bir A1 kuralıdır (K120) ve A4-K67'nin `DROPPED_NO_SLOT` anlamını genişletir.
4. SIL'de sabit `N_bg`, A4-K95 ile çelişir.
5. Tick takvimi A4-K18'in 10 ms mutlak ızgarasını oturum içinde değiştirir.
6. 3, 4 ve 5 için "A4'ten beklenen" maddeleri yazıldı (8.2 A4 md. 9–12).
