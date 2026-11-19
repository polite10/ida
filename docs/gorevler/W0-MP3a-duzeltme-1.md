# W0-MP3a — İlk ön inceleme düzeltmeleri

Tarih: 2026-11-18. Uygulayıcı Opus high, inceleyen sol high. Ana görev `W0-MP3a.md` geçerlidir. İnceleme: `docs/olcumler/inceleme/MP3-sol-on.md` (8 kritik, 3 önemli). Bu birinci düzeltme turudur.

## Görev

On bir bulguyu kanıtlarıyla değerlendir ve gider; haklı bulmadığını kaynak ve testle açıkla. Mevcut test derleme hatasını düzelt, geçici dosya veya eski ikiliyi kanıt sayma. Başarısız derleme/test betikte sıfır çıkış veremez. Baud geri hesabını yazılabilir BRR'den yap; test beklentisini aynı hatalı işlevden üretme. DMA alımında RAM'e tamamlanmış baytları tüket; FIFO, yarım/tam tur ve düğüm geçişlerini doğru ele al. Yeni güvenceyi yalnız yorumla değil test ve kısa kart denemesiyle destekle.

## Mimarın sabitlediği ölçüm koşulları

- %80 sınırı akış kampanyası için sabittir; CLI bunu yükseltip kampanyayı geçiremez. Düşük deneysel oran seçilebilir, ama dondurulmuş kampanya profiliyle aynı sonuç sayılmaz. Echo fiziksel kapasite sınırı %100'dür; akışın %80 sınırıyla karıştırma. Profil sürümünü kayda yaz, çözümlemede doğrula.
- Takvim toleransı bu prototipin ölçüm geçerliliği içindir, kontrol sistemi gecikme gereği değildir: host yazma başlangıcının planlanan zamandan gecikmesi en fazla 10 ms; son yazma tamamlanması planlanan trafik penceresini en fazla max(10 ms, pencerenin %1'i) aşabilir. Kartın akış gönderim takvimi için de 10 ms azami gecikme ve %1 pencere sınırını uygula; gereken alanları kaydet. Aşım varsa sayaçlar eşleşse bile profil GEÇMEDİ. Ölçüm takvimi çözülemiyorsa GEÇERSİZ. Gerçekleşen gönderim süresi ve tel hızı planlanandan ayrı raporlanır. Bu sınırlar mimar seçimidir, ölçüm sonucu veya kullanıcı kararı değildir.
- Echo bekleyenleri gerçek gönderilen isteklerden say; atlanan sıra kuyruk işgal etmez. Timeout ve geç cevap muhasebesi çift sayım yapmasın.
- Kayıt doğrulamasında R zaman çizelgesi, P/K özetleri, kabul/OS sonucu, BRR ve bayt sayıları çapraz kontrol edilir. Kontrol çerçeveleri dahil sayaç kapsamını açık tanımla. Sol karşı örneklerini negatif testle göster; geçerli koşuları da reddetmediğini test et.

Kart kullanıcı tarafından yeniden bağlandı, seri uç tekrar görünüyor. Düzeltme/testlerden sonra yedeği doğrula, önceki imaj kimliğini kaydet ve yalnız MP3 dahili flash imajını yükleyerek kısa ön taramayı yap. Uzun kampanya hâlâ başlatılmaz. Kart bağlantısı kesilirse dürüstçe raporla.

Yazılabilir ana göreve ek olarak `docs/olcumler/inceleme/MP3-yanit-1.md`. Her bulgu için değişiklik ve test kanıtı yaz. Raporu güncelle, eski ilk teslim sonuçlarını tarihli geçmiş olarak koru. Mimari/DURUM/karar/görev belgelerini değiştirme. Commit veya başka agent yok. Bitince gerçek test toplamı, kısa tarama sonucu, kalan belirsizlikler ve değişen dosyaları bildir.
