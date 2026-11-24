# sol incelemesi: atis

## Hüküm

**İlk dört bulgu kapandı. `81af064..83b2f20` farkında yeni bir K21 sınıfı yol saptanmadı.** Hedef bütünüyle okundu; çağrı dizileri kaynak üzerinden izlendi. Test/derleme çalıştırılmadı, dosya değiştirilmedi.

1. **#1 kapandı — `core/src/atis/atis_emniyet.c:294`.** `now=2000 ms, t_rx=2000 ms, t_meas=100 ms` kabulünden sonra `now=2015 ms, t_rx=1700 ms, t_meas=101 ms` kabul edilirse izler güncellendikten sonra bayatlık yeniden hesaplanıyor: aynı çağrıda `ESTOP`, `SAFETY_CLOSE(ALL)`, daraltma ve güvenli durum oluşuyor. İkinci ölçütte, `t_meas` sabitken artan `seq` ve taze `t_rx` yalnız alım izini yeniliyor; son ilerlemeden 255 ms sonraki çağrı daraltıyor. O çağrıdaki taze örnek daraltmayı geri alamıyor (`:140`, `:303`).

2. **#2 kapandı — `core/src/atis/atis_emniyet.c:64`; `core/src/atis/atis.c:453`.** `FIRE APPLIED` → `t_meas>t_apply, LOADED=1` → kilit; ardından kapanma `APPLIED`, `PERMIT_OPEN=0`, `LOADED=1` ve yeni `FC_RESET` → kabul edilse de aynı çağrıda yeniden kilit. `SESSION_END` tekleme nedenini koruyor (`atis_emniyet.c:215`); sonraki oturumda dolu örnek nedeni koruyor, boş örnek kaldırıyor. Ardından `FC_RESET` güvenli duruma geçiriyor. Bu dizide yeni yanlış kilit veya kilitten kaçış bulunmadı.

3. **#3 kapandı — `core/src/atis/atis_emniyet.c:170`.** `SAFE` durumunda `reset_kabul` veya `bu_cagri_kilit` ham baytı `0x03` → olağan çağrı: denetim sıfırlamadan önce çalışıyor; bütünlük kilidi, `FAULT_LATCHED` ve `SAFETY_CLOSE(ALL)` kuruluyor. Çerçevenin ön denetimi de bozulmayı `core_ok=0` ve günlüğe taşıyor (`core/src/cerceve/cerceve.c:560`).

4. **#4 kapandı — `core/src/atis/atis.c:109`.** Bekleyen `FIRE` → `FIRE_PENDING`; sonuç `APPLIED`, henüz uygulama sonrası örnek yok ve izin geçerli → `PERMIT_OPEN`; boş örnekle `ASSESS` → izin sürüyorsa yine `PERMIT_OPEN`; izin dolunca → `ARMED`. Eşleme A4 §3.16’nın bekleyen sonuç/açık izin anlamlarını karşılıyor.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| — | — | İstenen dar kapsamda düzeltme gerektiren bulgu saptanmadı. | — | — |

## Yeniden hesaplananlar

- **Bayatlık:** `2015−1700=315 ms >250 ms`; çağrı başındaki eski iz yalnız `15 ms` yaşındadır. Sabit ölçüm zamanında taze alım izinin yaşı sıfır olsa bile ilerleme izinin `255 ms` olması ikinci ölçütü tetikler.

- **W5-K20 — bağlantı ve örnek sırası:** Gevşek durum → `SESSION_END` ve gevşek örnek: etkin alanlar kısıtlı, tüpler boş, FC örnek izleri kurulmamış kalır (`atis_emniyet.c:267`). Sonraki `SESSION_START` ve gevşek örnek: izler kabul edilir, etkin alanlar gevşemez (`:307`); sonraki olağan taze örnek gevşetir. Aynı çağrıdaki kısıtlayan ve gevşek örneklerin sırası sonucu değiştirmez: `cagri_kisit` ilgili alanı maskeler. `inst≠0` kısıtlayabilir; gevşetemez ve FC tazelik izini yenileyemez (`:264`).

- **Önceki çağrıda bayatlık ve bekleyen izin:** Son örnek `1000 ms`, OPEN çıkışı `1240 ms`; `1255 ms` çağrısı bayatlığı saptayıp isteği siler ve FC’yi güvenli duruma alır. `1260 ms` çağrısında taze gevşek örnek ve eski OPEN’ın `APPLIED` sonucu birlikte gelirse `ESTOP` kalkar; sonuç, FC artık `FIRE_AUTH` olmadığı için izni yeniden kuramaz (`atis_emniyet.c:385`). Eski istekten `FIRE` veya `SAFETY_OPEN` çıkmaz. Taze çağrıda yeni `FC_ARM→FC_FIRE` kabulüyle OPEN üretilebilmesi K20’nin izin verdiği olağan gevşetmedir.

- **W5-K21 — DET_STALE:** Süre tabanı `t₀` ise `t₀+2000 ms` anında bit kapalı, `t₀+2005 ms` çağrısında açıktır. `SESSION_END` ve `SESSION_START` tabanı yeniden kurar; bu davranış A4-K118/K122’ye uygundur (`atis_emniyet.c:175`, `:255`). İncelenen kapsamda bitin gereğinden uzun kapalı kaldığı yol bulunmadı.

- **Parametreler:** `T_assess`, `T_tup_bos`, `T_hb_fc` sınıf B’dir (`core/src/atis/atis_param.def:26`). Silahlıyken geçerli `PARAM_SET` → `ARMED`; silahsızken bekletilen değer → silahlıyken veya `ARM` ile aynı çağrıda `PARAM_COMMIT` → `ARMED` (`core/src/param/param_komut.c:43`, `:173`). Aralık dışı SET, bekleyen değere yazılmadan `PARAM_INVALID` alır; kalıcı blokta da aralık denetlenir. Aralıklar sırasıyla **200–5000**, **100–2000**, **200–10000 ms**; varsayılanlar **1000, 500, 2500 ms**’dir. En büyük dönüşüm `10000×10⁶=10¹⁰ ns`, `int64_t` sınırının altındadır. Tamsayı sınırları canlı [WG14 N1570 §6.5 ve §7.20.1.1](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) üzerinden denetlendi; N2176 erişimi zaman aşımına uğradı, doğrulanmış sayılmadı.

## İskelete ya da diğer bölümlere notlar

Yerel `origin/main:docs/gorevler/W5-ortak.md` ve canlı GitHub `main` kopyası K21’de bitiyor; K22 metni bulunamadı. K22 için kullanıcının verdiği kural esas alındı: çekirdeğin kendi daraltmasındaki `NARROWED` korunmuş; geri alınan K17’ye dayanılarak bulgu yazılmadı.