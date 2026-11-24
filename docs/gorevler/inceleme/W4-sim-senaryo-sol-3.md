# sol incelemesi: sunucu

## Hüküm

**Kapanmadı** (`sim/sunucu/src/main.cpp:133`; karşı örnek: `ida_sim --acil -1e-999`). `c5848e3` dosyalı koşudaki önceki kaçışı kapatıyor; yerleşik koşuda negatif girdinin alt taşmayla kabul edilmesi kalmış. Dosya değiştirilmedi; derleme/test çalıştırılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `sim/sunucu/src/main.cpp:133` | `--acil -1e-999`, `strtod` alt taşmasıyla sıfıra dönüşür. `isfinite(x)` doğru, `x < 0` yanlış olur; kullanım hatası yerine 0 saniyede acil durdurma planlanır (`sim/sunucu/src/senaryo_plani.cpp:425`). | Dönüşüm öncesinde `errno` sıfırla; `ERANGE` sonucunu kullanım hatası olarak reddet. Karşı örneği CLI testine ekle. | kritik |

## Yeniden hesaplananlar

- Alt taşmada sıfır ve `ERANGE` dönüşü [Microsoft’un birincil belgesinden](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/strtod-strtod-l-wcstod-wcstod-l?view=msvc-170) canlı doğrulandı. Mevcut kod `errno` denetlemiyor.
- Geçerli `--acil 12 --acil-kalk 18` korunmuş: başlangıç sınırı **9600**, bitiş **14400**, etkin süre **4800 fizik adımı**.

## İskelete ya da diğer bölümlere notlar

- **(a)** Dosyalı koşuda belirtilen yineleme/NaN örnekleri; boş/eksik değer, `--acil=`, çok büyük sayı, `1e999`, onaltılık ve boşluklu yazımlar, yalnız `--acil-kalk` ve yinelenen `--ariza` sessizce geçmiyor: ayrıştırıcı veya kalıcı bayrak denetimi çıkış **1** üretiyor (`main.cpp:123`, `sunucu.cpp:568`).
- **(c)** Yakalanan hatalar oturumdan (`sunucu.cpp:1287`) ve CSV açılışından (`:1471`) önce dönüyor. Yukarıdaki yerleşik karşı örnek ise hata üretmediğinden oturum yoluna ilerliyor.
- `docs/gorevler/W4-sim-senaryo.md` bu commit’te bulunamadı; görev uyumu verilen kullanıcı kararı üzerinden değerlendirildi.