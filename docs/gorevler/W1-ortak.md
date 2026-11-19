# W1 — Arayüz sözleşmeleri: ortak kurallar

Tarih: 2026-11-19. Bu belge W1 dalgasındaki bütün sözleşme işleri için geçerlidir; her işin kendi görev belgesi (`docs/gorevler/W1-A*.md`) yalnız o sözleşmeye özgü olanı ekler. İkisi çelişirse işin kendi belgesi geçerlidir.

## Sözleşme nedir

Bir arayüzün iki tarafını birbirinden bağımsız yazılabilir kılan belgedir: bir uygulayıcı yalnız sözleşmeyi okuyarak kendi tarafını yazabilmeli, bir test yazarı yalnız sözleşmeyi okuyarak uygunluk testini yazabilmelidir. Mimari bölümleri (01–09) gerekçe ve tasarım anlatır; sözleşme **kural** yazar. Bölümlerdeki bir şey sözleşmeye girmediyse bağlayıcı değildir.

## Öncelik sırası (çelişkide)

1. Kullanıcının kesin kararları (`AGENTS.md`; 00 belgesi Bölüm 2.1, K1–K19).
2. `docs/mimari/00-iskelet.md` **sürüm 0.8** (özellikle §3, §3.1, §3.2, §4, §8, §10).
3. Ölçüm raporları (`docs/olcumler/`) ve bu görev belgeleri.
4. Mimari bölümleri 01–09 (taslak; iskelet 0.8'den eski; çeliştikleri yerde iskelet geçerlidir ve bölüm yeniden yazılmaz).
5. Ham araştırma raporları yalnız 00 belgesinin düzeltmeleriyle birlikte.

Bölümlerin 0.8'le açıkça eskiyen yerleri: USB yüksek hız taşıması ve USB yığını (kaldırıldı; A1 yalnız ST-LINK sanal seri portu, 3 000 000 baud), 400 Hz kontrol hızı (tasarım tabanı 200 Hz), "IMU örneği tetikler" (tetik parti kapanışıdır), bariyerin eski ifadesi (sabit `L_act` = iki periyot; geç kalan yanıt kuralı), kameranın olağan çizim döngüsü, sonarın saçıcı kuralı, harici yer istasyonu uç noktası (ilk sürümde yok).

## Çıktılar

Her iş şunları üretir (kendi dalında; dal adı işin belgesinde):

1. **Sözleşme belgesi:** `docs/sozlesmeler/A<n>-<ad>.md`, sürüm `1.0-aday`. Yapı:
   1. Kapsam ve taraflar (kim üretir, kim tüketir; kapsam dışı olanlar)
   2. Terimler ve kabuller (iskelet §3.1'e atıfla; yinelenmez, yalnız bu sözleşmeye özgü olanlar)
   3. Veri tanımları: her mesaj, yapı, dosya alanı ya da işlev için tablo (ad, tip, birim, geçerli aralık, zorunlu mu, anlamı, geçersiz değerde davranış)
   4. Davranış kuralları: sıra, zamanlama, durum makineleri, oturum açma ve kapama, sürümleme ve uyumluluk; her kural **numaralı ve sınanabilir** tek cümle (`A<n>-K<nn>`)
   5. Hata durumları: her biri için algılama koşulu ve iki tarafın zorunlu davranışı
   6. Kalite kademelerinin etkisi (varsa)
   7. Uygunluk: her kuralı hangi tür testin sınayacağı (birim, sözleşme, SIL, kartlı); test kodu yazılmaz, yalnız sınama yöntemi ve gözlenecek büyüklük
   8. Açık kalanlar: ölçüm ya da başka sözleşme bekleyen noktalar; her biri "tahmin" ya da "ölçülecek" etiketli ve neyin sonucuyla kapanacağı yazılı
   9. Açık bulgu kapanış tablosu (aşağıda)
2. **Makinece okunur tanım** (işin belgesi hangisini istediğini söyler): şema, başlık dosyası ya da örnek dosya. Bu dosyalar sözleşmenin parçasıdır; belgeyle çelişemez. Uygulama kodu (işlev gövdesi, sunucu, ayrıştırıcı) yazılmaz.
3. **Yanıt ve özet:** son iletide kısa rapor.

## Açık bulgu kapanış tablosu

`docs/mimari/acik-bulgular.md` içinde işin belgesinde adı geçen başlıkların altındaki **her** bulgu için bir satır: `AB-nnn` · durum · sözleşmede nerede (kural numarası ya da bölüm) · tek cümle gerekçe. Durum değerleri: **kapandı** (sözleşme bulguyu çözen kuralı içeriyor), **uygulamaya devredildi** (sözleşmenin konusu değil; hangi uygulama işine ait olduğu yazılır), **kapsam dışı** (K17 ya da iskelet 0.8 ile geçersiz; gerekçesiyle), **açık** (çözülemedi; neden ve neyi bekliyor). "Kritik" önemdeki bir bulgu "açık" bırakılırsa son raporda ayrıca listelenir. Bulgu metni silinmez ve `acik-bulgular.md` bu işte değiştirilmez; tabloyu mimar kayda işletir.

## Yazım kuralları

- Kural cümleleri kesin kiple yazılır ("gönderir", "reddeder"); "olabilir", "tercihen", "uygun biçimde" gibi ifadeler kural değildir.
- Her sayısal değer ya bir ölçüme (rapor ve başlık atfıyla), ya bir standarda/birincil kaynağa (bağlantıyla), ya da "mimari seçimi (tahmin)" etiketine dayanır. Ölçülmemiş süre, hız ya da boyut rakamı ölçülmüş gibi yazılmaz.
- Dış olgular (protokol alan boyutları, kitaplık davranışı, dosya biçimi kuralları) birincil kaynaktan doğrulanır; doğrulanamayan "doğrulanmadı" diye işaretlenir.
- Başka bir sözleşmenin konusu yinelenmez; atıf verilir. Başka sözleşmeden beklenen şey "A<m>'den beklenen" başlığıyla listelenir (o sözleşmenin yazarı ve mimar bunu görür).
- Belgeler Türkçe; alan, mesaj ve işlev adları ASCII, İngilizce ya da Türkçe (iş içinde tutarlı), küçük harf ve alt çizgi.
- Kişisel makine yolu yazılmaz. Kapsam dışı: ardıl kart, başka mikrodenetleyici, gerçek sensör tedariki, yarışma kuralları, Rust.
- **Atış kontrol sınırı:** atışla ilgili her içerik emniyet zinciri ve arayüzlerle sınırlıdır (algılama kaydının biçimi, durum ve olay mesajları, izin ve engel kuralları, sanal emniyet girdileri). Torpidonun menzilini, hızını ya da etkinliğini artıran, hareketli hedefe önleme ya da atış çözümü ayrıntılandıran içerik yazılmaz. Böyle bir içerik gerektiğini düşünürsen yazmadan dur ve bildir.

## Süreç

- Yazan Opus (`ida-uygulayici`), inceleyen sol (`tools/sol-incele.sh`; çağrıyı mimar başlatır). En çok iki düzeltme turu; ikinci turdan sonra kritik bulgu kalırsa iş mimara döner.
- Commit atma, `git add` yapma, `git push` yapma, başka agent başlatma. Dalı oluşturabilirsin; commit'i kayıt agent'ı atar.
- Aynı anda başka sözleşme işleri de yazılıyor. Yalnız kendi dosya sahipliğindeki yollara yaz; başka sözleşmenin dosyasına dokunma.
- Kabulden sonra mimar sözleşmeyi dondurur (sürüm `1.0`, etiket `sozlesme-A<n>-v1`); dondurulmuş sözleşmede değişiklik ayrı bir iştir.

## Son rapor

Kısa: yazılan dosyalar; kural sayısı; açık bulgu tablosunun özeti (kaç kapandı, devredildi, kapsam dışı, açık; açık kalan kritikler tek tek); başka sözleşmelerden beklenenler; iskeletle ya da görev belgesiyle gördüğün çelişki; çalışma dizini ve dal adı.
