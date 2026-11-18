# W0-MP4a — 2026-11-17 devam görevi

Mimar: Codex. Uygulayıcı: Opus (`high`). Bağımsız inceleyen: `gpt-6.1-sol` (`high`); incelemeyi mimar başlatır. Kullanıcının 2026-11-17 talimatıyla mevcut MP4a işi kaldığı yerden devralınır. Ana görev `docs/gorevler/W0-MP4a.md`; bu ekin düzeltmeleri önceliklidir.

## Mevcut durum ve sınırlar

- `olcum/mp4-dongu-yuku` çalışma kopyasında kod ve taslak rapor var, commit edilmemiş. Mevcut çalışmayı koru; sıfırdan yazma.
- Önceki ham veriler `${IDA_CACHE:-$HOME/.cache/ida}/mp4/kos/` altında. `o2-20261116-011044-analiz.txt` kayıt kaybı, `uart_tasma=454` ve analiz çıkış kodu 1 bildiriyor; kabul kanıtı sayılamaz. Kısa koşu bir tam uzun kampanyanın yerine geçmez. Önceki rapordaki kanıtlanmamış tamamlanma/sağlama karşılaştırma cümlelerini doğrula veya düzelt.
- Bu iş genel işlemci yükü ölçümü, zamanlama, kayıt bütünlüğü ve raporlama ile sınırlı. Mevcut yer tutucu yükün algoritmalarını optimize etme ya da yeteneklerini geliştirme; silah/atış kontrolü ekleme. Başarısız performans sonucu geçerli sonuçtur.
- Kart erişimi öncesi seri portun başka süreçte açık olmadığını kontrol et. Karta yalnız ST-LINK üzerinden eriş; yedeği doğrulamadan yükleme yapma. Seçenek baytları, harici flash ve ST-LINK firmware'i değiştirilmez.
- Kayıt ve derleme çıktıları depoya girmez. Mevcut ham verileri koru; yeni koşular ayrı zaman damgaları taşır.

## Tamamlanacaklar

1. Mevcut kodu ve ham verileri denetle. Uzun koşu çıktısındaki kaybın nedenini bul; ölçüm dışındaki sonuç dökümünü bütünlük korunacak şekilde düzelt. Ağır yükte periyot aşımları gizlenmesin, ölçüm zamanına seri çıktı işi eklenmesin.
2. Tetik gecikmesinin taşan histogramı, kesme/sayaç yarışları, eksik fazın başarılı sayılması ve ölçüm süresinin ağır yükte uzaması gibi düzenek sorunlarını kontrol et. Geçerli veriden yüzdelik verilemiyorsa sayı uydurma. Ana görevdeki gecikme tanımından farklı ölçüm varsa raporda açık ayır.
3. Ana görevdeki bir yazım hatası düzeltilmiştir: **200 Hz periyot 5 ms'dir; 2,5 ms 400 Hz'e aittir.** D1/D2 eşikleri değişmez. 200 Hz'e dair çıkarım, 200 Hz'te gerçekten koşulmadıysa hesap olarak etiketlenir.
4. `-O2` ve `-Og` için olağan ve ağır yükte en az 240.000 kayıtlı adım ve en az 10 dakika veriyi eksiksiz al. Faz C mevcut ek ölçümdür; korunabilir ama zorunlu A/B'nin yerini tutmaz. Sabit koşu süresinin ağır yükte fazları tamamlamayabileceğini hesaba kat. Uzun koşuları `caffeinate -dims` altında yap; her koşunun başı/sonu ve imaj SHA-256'sını kaydet.
5. D3 ELF denetimi, yığın tepe kullanımı, kod/RAM boyutları ve histogramları tamamla. Düzenek/çözümleyici değişikliklerini hedefli testlerle doğrula; eksik/bozuk veri için başarısızlık davranışını sına.
6. `docs/olcumler/MP4-dongu-yuku.md` raporunu gerçek verilerle tamamla. D1–D4'ü ayrı hükme bağla; ön bilgi sınırını, taşımayı içermediğini ve gerçek çekirdekle tekrar gereğini koru. Önerileri kullanıcı kararı gibi yazma.

## Sahiplik ve teslim

Yalnız `tools/olcum/mp4/**` ve `docs/olcumler/MP4-dongu-yuku.md` düzenlenir. İnceleme düzeltme turunda mimarın açıkça verdiği yanıt dosyası ayrıca yazılabilir. Ana görevdeki diğer yasaklar geçerli. `AGENTS.md`, görev belgeleri ve mimari belgeler düzenlenmez. Commit/add/push/merge veya alt agent başlatma yok.

Teslim: değişenler, çalıştırılan komutlar, gerçek ölçüm/kabul sonucu, doğrulanmayanlar, ham veri yolları ve koşuların imajla eşleşmesi. Karta erişim kesilirse durumu dürüstçe bildir. Son inceleme ve kapanış mimarda.
