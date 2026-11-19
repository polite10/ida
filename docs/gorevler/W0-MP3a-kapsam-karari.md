# W0-MP3a — Mimar kapsam kararı ve kampanya başlangıcı

Tarih: 2026-11-19. Bu kullanıcı kararı değil, iki olağan düzeltme turundan sonra mimarın ölçüm kapsamı kararıdır. Dayanak `docs/olcumler/inceleme/MP3-sol-3.md`.

## Kabul sınırı

MP3 yalnız dondurulmuş kampanya ayarlarında değerlendirilecektir. Echo için **azami bekleyen 64**, yanıt timeout **1000 ms**; bu parametreler her koşu komutunda açıkça verilir ve her ham kaydın B alanından bağımsız olarak denetlenir. 64 dışı kuyruk ayarı bu işin kabul kapsamı dışındadır; çözümleyicinin GECTI yazması böyle bir kaydı kampanya sonucu yapmaz. Deneysel/custom profillerin genel doğruluğu iddia edilmez.

Sol kapanışındaki A1 önemli bulgusu düzeltilmiş sayılmaz; kapsam dışında kalan araç sınırlaması olarak raporda açık kalır. Y1'in eski dönem çift sayım hatası, Y2 ve Y3 kapanmıştır; Y4 standart B64 için kapanmıştır. 219/219 test mimarca bağımsız doğrulanmıştır. Sol ayrıca 514 DMA sınır denetimini UBSan altında geçirmiştir. Donanım V1–V3 varsayımları ölçümün sınırlamasıdır; CRC'nin her olası donanım hatasını yakalayacağı garantisi verilmez.

## Yürütme yetkisi

`W0-MP3a-kampanya.md` planı **bu kapsamla şimdi yürütülebilir**. İlk dört uzun akış adayının sonuçları alındıktan sonra geçen en yüksek hızda dört echo koşusuna geçilir. Süre 600 s, `--en-az-sn 600 --kampanya`, akış256 B ve sabit %80. Echo komutlarına açıkça `--azami-bekleyen 64 --zaman-asimi-ms 1000` eklenir (seçenek adını CLI yardımından doğrula; adı farklıysa aynı anlamdaki mevcut seçenek kullanılır). Kaynak kodu değiştirilmez.

Her kayıt için ikinci çözümlemenin yanında: B profil/mod/baud/yük/hız/süre ve echo kuyruk/timeout değerlerini bu görevden bağımsız sabit beklentilerle karşılaştır; başarısızlıkta GECTI hükmüne güvenme. Kullanılan komut, B başlığı ve SHA-256 raporun kanıtıdır. Son bağımsız veri incelemesi sol'a verilecektir.

Opus yalnız ölçümleri yürütür, ham veri manifestini ve `docs/olcumler/MP3-vcp.md` güncel sonuçlarını hazırlar. Kod/firmware değişikliği, üçüncü olağan düzeltme turu, commit veya alt-agent yok. Kopma/çökme/araç hatasında durumu bildir; başarısız ölçümü silme. Tarihli önceki raporları koru. V1–V4'ü daraltılmış kapsamla ayrı değerlendir.
