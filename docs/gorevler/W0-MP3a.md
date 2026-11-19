# W0-MP3a — ST-LINK VCP bağlantı ölçümü

Tarih: 2026-11-18. Dal: `olcum/mp3-vcp`. Mimar: Codex; uygulayıcı: Opus high; bağımsız inceleme: gpt-6.1-sol high, mimarın çağırdığı `tools/sol-incele.sh`.

## Amaç ve dayanak

K17 uyarınca yalnız ST-LINK VCP / USART1 bağlantısının gerçek hızını, kaybını ve gidiş-dönüş gecikmesini ölç. Bu atılabilir, genel amaçlı seri bağlantı prototipidir; ürün protokolü veya kontrol yazılımı değildir. Dayanaklar: `docs/mimari/09-test-ve-is-akisi.md` §6.1 MP3, `docs/mimari/01-kart-yazilimi.md` K6 ve `docs/research/00-duzeltmeler-ve-kararlar.md` K17/M3. Eski USB HS/yedek taşıma ifadelerini uygulama. Kontrol frekansı veya A1 sözleşmesi bu işte kesinleşmez.

Önce AGENTS, DURUM ve yukarıdaki belgelerin ilgili bölümlerini; MP1 raporu ve saat/yedek/yükleme araçlarını oku. MP4 artık tamamlandı; mevcut kart imajının MP4 olabileceğini varsay, kimliğini kaydet.

## Uygulama ve ilk teslim

1. Kart C, PC C++20; `tools/olcum/mp3/` altında çalış. Genel test çerçevesi: sürüm, koşu kimliği, yön/tür, sıra numarası, uzunluk, belirlenimci veri ve CRC. Boyutları sınırlı tut. Ürün mesajlarını, sonar/algılama, hedef takibi, güdüm veya atış kontrolünü uygulama.
2. USART1 RX circular DMA, TX DMA kullan; pin/saat/GPDMA ayrıntılarını yerel ST kaynaklarından doğrula. Hata sayaçları: UART ORE/FE/NE, DMA hatası/taşması, CRC, eksik/tekrar/sırasız paket. Sessiz tampon kaybını başarı sayma. Son özet ve iki uç sayaçları karşılaştırılmadan koşu geçerli değildir.
3. PC seri portta kısmi okuma/yazma, timeout, kopma, bozuk uzunluk/CRC ve yeniden eşleme davranışı sınırlı ve testli olmalı. macOS ölçülür; Windows çalıştırılmadıysa açık yazılır. İstenen baud, UART saat/BRR'den hesaplanan baud/hata ve işletim sistemi yapılandırma sonucu kaydedilir; gerçek fiziksel baud ölçüldü denmez.
4. Ön tarama adayları: 921600, 2000000, 3000000, 6000000, 12000000 baud. Her adaya kısa bağlantı testi; desteklenmeyen/başlamayan aday başarısız ya da doğrulanmadı olarak ayrılır. Hızı sessizce düşürme.
5. Ölçüm araçları en az 600 saniyelik çift yönlü kampanyayı desteklesin. İlk teslimde uzun kampanyayı BAŞLATMA: derleme, masaüstü hata testleri ve kısa kart denemeleriyle dön. Mimar önce sol incelemesi başlatacak.

## Dondurulmuş kampanya tanımı

İlk inceleme sonrasında uygulanacak: ön taramayı geçen her baud için iki yönde bağımsız sayaçlı 256 bayt veri yükü, çerçeveleme dahil yön başına hesaplanan 8N1 kapasitesinin %80'ini aşmayan sabit hız, en az 600 s. Dolayısıyla sonuç sınırsız doygunluk kapasitesi iddiası değildir. Trafik hızı ve toplam tel baytları raporlanır. Geçen en yüksek baud'da 64 ve 256 baytlık echo veri yükleri, ayrı ayrı 200 ve 400 istek/s, her profil 600 s; kapasiteyi aşan profil hesapla açıklanıp geçmedi sayılır. İki yön toplamını tek yön kapasitesiyle karıştırma.

Gecikme: host monotonik saatinden planlanan gönderim, yazma başlangıcı/tamamlanması ve tam yanıt alınması kaydedilir; planlanan zamana ve yazma başlangıcına göre RTT ayrı raporlanır. İşletim sistemi kuyruğu dahildir. Senkronize olmayan iki saatten tek yön gecikme çıkarma. p50/p99/p99,9/maksimum, örnek sayısı, kayıp/timeout ve süre raporlanır. Kuyruk büyümesini ve bitişteki drain sınırını açık tanımla; sonsuz bekleme yok. Gecikme eşiği henüz yok, ölçüm çıktısıdır.

## Geçerlilik ve kabul

- V1: tekrar üretilebilir derleme, imaj SHA, kaynak durumu, seri uç ve ortam kaydı.
- V2: her geçen hızda 600 s, her iki uç son sayaçlarının eşleşmesi, sıfır veri/CRC/sıra/UART/DMA kaybı; aksi halde o hız geçmez. Kesilen veya özet eksik koşu geçersizdir.
- V3: seçilen hızın dört RTT profilinde süre ve örnek/sayaç muhasebesi doğrulanmış; başarısız profil de dürüstçe raporlanır.
- V4: bozuk/kesik kayıt, eksik son özet, yanlış koşu kimliği, CRC/seq hatası ve kısmi IO negatif testleri yanlış başarı üretmez.

Ham veriler `${IDA_CACHE:-$HOME/.cache/ida}/mp3/` altında; önceki koşuları ezme. Uzun koşular uyku engelleme altında. Rapor `docs/olcumler/MP3-vcp.md`: yöntem, gerçek komutlar (kişisel mutlak yol olmadan), sonuçlar, sınırlamalar ve kaynak/ham veri kimliği. Gerçek ölçülmeyen değerleri tahmin olarak ayır.

## Yetki ve sahiplik

Yazılabilir: `tools/olcum/mp3/**`, `docs/olcumler/MP3-vcp.md`. Mevcut MP1/MP4 dosyaları salt okunur; çağrılabilir. Ürün dizinleri, ortak mimari/karar/DURUM belgeleri ve görev dosyaları değiştirilemez. Commit, stage, push veya başka agent başlatma yok.

Yalnız doğrulanmış mevcut fabrika yedeğiyle dahili flash'a prototip yükleme serbest. Seçenek baytları, harici flash ve ST-LINK firmware güncellemesi yasak. Kart erişimi kaybolursa ölçümü kes ve bildir. Başka kablo/USB HS yok. İlk teslim: yapılan, test kanıtı, doğrulanmayan, tasarımda gördüğün sorunlar; uzun kampanya beklemede. En fazla iki düzeltme turu; ardından mimar kapsamı yeniden değerlendirir.
