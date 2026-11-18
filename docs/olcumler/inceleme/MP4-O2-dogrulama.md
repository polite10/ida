# MP4a — tamamlanan O2 kaydının bağımsız doğrulaması

İnceleyen: `gpt-6.1-sol`, `high` (Codex alt agent). İnceleme: 2026-11-17. Mimar tarafından çıktının kayda aktarılması: 2026-11-18. Bu belge ilk kod incelemesinin ardından tamamlanan gerçek O2 verisini kapsar; Og kampanyasını veya henüz kapanmamış çözümleyici bulgularını onaylamaz.

## Kanıt

- Ham kayıt: `${IDA_CACHE:-$HOME/.cache/ida}/mp4/kos/o2-20261117-190043.csv`.
- CSV SHA-256: `09809edaf837670d4e6f300b8f5fce66df171b7a7070890025fb4ff7ecb06d85`.
- İmaj kimliği: `20261117-185727-o2-a11153bb`.
- ELF SHA-256: `9ad49a48b1c14049b9b0c34083c084d139b8dc371037dc900a599a621cb9948f`; koşu başı ve sonu aynı, ilk incelemede doğrulanan ELF ile eşleşiyor.

## Bağımsız sonuç

Sol, mevcut çözümleyicinin başarılı sonucuna dayanmak yerine CRC, sıra, faz, şema, histogram, özet ve süre denetimlerini bağımsız hesapladı. Gerçek kayıtta kritik sorun bulmadı.

- Açılış öncesi bir satır dışarıda; ardından 3539 satır, sıra 0–3538. CRC/sıra hatası ve yeniden açılış yok.
- Son satır: `T ... adim=725000 fazlar=7 d_atlanan=0`.
- Her fazda 241000 örnek; adım ve gecikme histogram toplamları tam. Histogram/kayıt taşması ve bildirilen CRC hatası yok.
- F/G toplamları, min/max, histogram kutuları, Q parça toplamları/maksimumları ve E satır sayıları tutarlı.

| Faz | Süre alt sınırı (s) | En uzun adım (µs) | Aşım | Atlanan tetik |
|---|---:|---:|---:|---:|
| A | 602,49750004375 | 1338,91875 | 0 | 0 |
| B | 821,4287758375 | 3322,38125 | 241000 | 87573 |
| C | 602,4974999625 | 1301,73125 | 0 | 0 |

**D1 geçmedi:** A sağlıyor, B sağlamıyor. **D2 geçmedi:** ağır faz maksimumu 3,32238125 ms > 1 ms. B'de `asim_dwt=asim=241000`. Atlanan tetik sayısının `tik_aralik−n` değerinden bir büyük olması, ilk kayıtlı adımın önceki geçiş adımına göre sayılan giriş sınırını içeriyor; veri bozukluğu değil.

Faz raporlarındaki en büyük `yigin_tepe=3452 B`, en büyük `yigin_kayit=2500 B`; tüm `yigin_kayit_tasti=0`. Bunlar ilk incelemedeki yığın kapsamı düzeltmesiyle birlikte yorumlanır.

Sol bu doğrulamada dosya değiştirmedi, karta erişmedi; önceki ELF ve test denetimlerini tekrarlamadı. Sonuç, çözümleyicinin bilinen bozuk veri kabul açıklarını kapatmaz; bunların düzeltmesi ve ikinci kod incelemesi ayrıca gereklidir.
