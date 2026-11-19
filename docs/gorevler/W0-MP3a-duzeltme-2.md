# W0-MP3a — İkinci ve son olağan düzeltme turu

Tarih: 2026-11-19. Opus high uygular; sol high bağımsız inceler. Önce `docs/olcumler/inceleme/MP3-sol-2.md` oku. Y1–Y4 kritik bulguları kapat. Ana görev, ilk düzeltme ve sahiplik kuralları geçerli.

- Y1: DMA TC bayrağı, kesme hizmeti ve LLI yazmaç yeniden yüklemesini ayrı olaylar olarak ele al. Atomiklik varsayma; belirsiz snapshot'tan veri tüketme. Sınırda konum artışının iki kez sayılmasını engelle; test modeli incelemenin `(yarim=2, BNDT=0, FIFOL=0)` karşı örneğini ve olay sıralamalarını içersin. Yalnız monotonluk zorlaması hata gizleyemez. Gerekirse daha basit, doğrulanabilir konum muhasebesi kullan; kaynakla doğrulanmamış donanım varsayımlarını açık yaz.
- Y2: telde uint32 alanlar ve host zaman alanları temsil aralıklarıyla doğrulanır. Bütün çarpma/toplama/daraltmalar taşmaya karşı denetlenir. UINT64_MAX ve sınır komşuları negatif test olur. Eski on bir bulguda gerileme yapma.
- Y3: akışın PC ve kart son zamanlarını son plan, ölçülen azami gecikme ve toplam süreyle karşılaştır. ns/us yuvarlama toleransı açık ve sınırlı olur; imkânsız kronoloji GEÇERSİZ sayılır.
- Y4: bekleyen istek zaman çizelgesini yazma, yanıt ve timeout olaylarından tanımla. Ekleme sonrası azami doluluğu ölç; çözümleyici R kayıtlarından bağımsız yeniden üretsin ve P ile karşılaştırsın. Yanıtın yazma çağrısı tamamlanmadan gelebileceğini dikkate al; doğru koşuyu sırf olay yarışı nedeniyle reddetme. Sınır 64, aynı zamanlı olay sırası ve timeout semantiği açık olmalı. Sol'un 100 bekleyen karşı örneği reddedilmeli.

Masaüstü testlerini ve UBSan'ı çalıştır. Kaynak/firmware değişirse doğrulanmış yedekle yeni imajı yükle, kimlikleri kaydet ve kısa ön taramayı yeniden yap. Eski ham veriyi ezme; kuyruk ölçümü değiştiği için eski sonuçları yeni kodla elde edilmiş gibi sunma. Uzun kampanya başlatılmaz.

Yazılabilir: ana görev dosyaları ve `docs/olcumler/inceleme/MP3-yanit-2.md`. Raporun başına güncel sonucu ekle, geçmiş kaydı koru. Yanıt her Y maddesinin kanıtını içersin. Bu ikinci olağan düzeltme turudur; yeni kritik/önemli bulgu kalırsa mimar kapsamı değerlendirecek, kendiliğinden üçüncü tura girme. Commit/push/başka agent yok.
