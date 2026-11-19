# W0-MP3a — Uzun kampanya yürütme planı

Tarih: 2026-11-19. Bu belge hazırlık planıdır; yürütme başlangıcı için mimarın ikinci sol incelemesini değerlendirmesi gerekir. Ana görev ve ilk düzeltme belgesinin ölçütleri aynen korunur. Ürün protokolü veya kontrol yazılımı yoktur.

## Önkoşul

`docs/olcumler/inceleme/MP3-sol-2.md` tamamlanmalı; kritik/önemli ölçüm geçerliliği bulguları kapanmalı veya mimar tarafından gerekçeli bir kapsam kararı verilmelidir. İnceleme başarısız ya da kota nedeniyle eksikse bu belge kendiliğinden yürütme yetkisi vermez.

## Sıra

1. Mevcut kaynak/imaj/PC aracı kimliğini rapordakiyle karşılaştır. Karta `kimlik` sor; çalıştırma ortamını ve güç durumunu kaydet. Kaynak değişmemişse gereksiz derleme ve yükleme yapma. Kaynak değiştiyse önce test ve imaj eşlemesini tamamla.
2. Ön taramada geçen adaylar: 921600, 2000000, 3000000, 6000000 baud. Her biri için ayrı 600 s çift yönlü 256 B akış koşusu; `--en-az-sn 600 --kampanya`. %80 ve takvim kuralları sabit. Kısa taramada başarısız 12 Mbaud uzun aday değildir, kısa sonucu raporda korunur.
3. Her koşunun son özetini ve çıkış kodunu hemen denetle. Geçersiz/kesilen koşu tamamlanmış kabul edilmez. Kesinti nedenini giderip yeni dizinde tekrar yapılabilir; eski kayıt silinmez. Ölçüt ihlalini geçene kadar tekrar ederek gizleme: geçmedi sonucu geçerlidir, ilk koşu ve tekrarlar birlikte raporlanır.
4. **Uzun akış koşularını geçen** en yüksek adayı seç. Kısa taramanın en yükseğini otomatik seçme. Hiçbiri geçmezse echo kampanyası için hız uydurma; sonuçlarla mimara dön.
5. Seçilen hızda 64 ve 256 B yük × 200 ve 400 istek/s: dört ayrı 600 s echo koşusu. Kapasiteyi aşan profil hesapla geçmedi sayılır. Gecikme eşiği icat edilmez; istatistikler, kayıp ve takvim geçerliliği ayrı sunulur.
6. Ham kayıtların tümünü ayrıca çözümle; sayaçlar, süreler, profil ve imaj kimliklerini doğrula. Koşu SHA-256 manifestini ham veri dizinine yaz. Raporu gerçek sonuçlarla güncelle; kısa/uzun sonuçları ve eski kayıtları ayır. Kontrol hızı veya nihai ürün başarımı kararı çıkarma.

Mevcut `kampanya.sh` en yüksek hızı baştan parametre alır; bu planın 4. adımı için koşular tek tek `kos.sh` ile yürütülür. Betiği bu amaçla yeniden yazmak gerekmez. Kayıt yolları benzersiz olmalı; koşular uyku engeli altında ve sırayla yürütülür. Aynı karta iki süreç erişmez.

## Sahiplik ve teslim

Uygulayıcı Opus high; kod değişikliği gerekmiyorsa yalnız ölçüm yürütme ve `docs/olcumler/MP3-vcp.md` rapor güncellemesi. Hata çıkarsa ölçüm aracını sessizce değiştirip eski veriyi yeni imajla eşleme; mimara bildir. Commit/push/başka agent yok. Sonuçta V1–V4, her adayın hükmü ve dört RTT profili açık olmalı. Sol son veri incelemesini mimar başlatır; kayıt agent'ı sonrasında kayıt/PR işini yapar.
