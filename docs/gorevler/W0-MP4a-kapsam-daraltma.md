# W0-MP4a — çözümleyici kapsam daraltması (mimar kararı)

Tarih: 2026-11-18. Mimar: Codex. Yazan: Opus (`high`). İnceleyen: `gpt-6.1-sol` (`high`); yeni kapsamın kapanışını mimar sol'a doğrulatır. Bu dosyayı mimarın açık izniyle uygulayıcı kaydetti; içerik mimarın 2026-11-18 kararıdır, uygulayıcının önerisi değildir.

## Karar

- `docs/olcumler/inceleme/MP4-sol-3.md` sonucunda Y5 kritik kaldı. `docs/mimari/09-test-ve-is-akisi.md` §2.4 uyarınca aynı kapsamda üçüncü düzeltme turu yapılmaz.
- **Kapsam daraltılır:** atılabilir MP4a çözümleyicisi (`tools/olcum/mp4/pc/mp4_seri`) **histogram taşması içeren kampanyaları desteklemez.** Y5'in önerdiği genel taşma tutarlılık formülü geliştirilmez; yalnız fail-closed daraltma yapılır.
- Mevcut, taşma içermeyen gerçek veri desteği korunur.

## Yeni kabul sınırı

1. Beklenen herhangi bir fazda `F.tasan > 0` (adım süresi 20 ms histogram aralığının dışında) **ya da** gecikme histogramında taşma (`G.lat_tasan > 0`) varsa kampanya **geçersizdir**: çıkış kodu 1, sayısal özet basılmaz, histogram dışa aktarımı yapılmaz.
2. Geçerli-taşmalı kayıt desteği ve buna bağlı başarı testleri kaldırılır; taşmalı kayıtların reddi test edilir (Y5'in karşı örnekleri dahil).
3. Taşmasız kampanyalar için önceki bütün denetimler (ilk ve ikinci düzeltme turları) aynen geçerlidir.
4. Gerçek kayıtlar (`v3` ile geçerli olan beş kayıt) yeni dar kapsamla yeniden analiz edilir; ham veri ve imajlar değişmez; yeni kart koşusu yapılmaz.
5. `MP4-sol-3.md` Y6 ve Y7 belge iddiaları raporda düzeltilir: FNV eşleşmesi tekil çıktıların bit eşitliğinin kanıtı değildir; 400 Hz ve 200 Hz TIM2 periyot değerleri ayrı yazılır.

## Sahiplik

Yazılabilir: `tools/olcum/mp4/pc/**`, `tools/olcum/mp4/test/**`, `tools/olcum/mp4/kos.sh`, `docs/olcumler/MP4-dongu-yuku.md`, yeni `docs/olcumler/inceleme/MP4-kapsam-yanit.md` ve bu görev dosyası. Firmware, adım kodu, imajlar, ham veri ve önceki inceleme/yanıt dosyaları değişmez. Commit/add/push/merge ve alt agent yok.
