# sol incelemesi: sunucu

## Hüküm

**Kapanmadı.** `8892b1c` commit’inde önceki #1 bulgunun bir yolu açık kalmış. İnceleme yalnız bu emniyet düzeltmesini kapsar; dosya değiştirilmedi, derleme/test çalıştırılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `sim/sunucu/src/main.cpp:123`; `sim/sunucu/src/sunucu.cpp:566` | Seçeneğin verilmesi yerine **son değerin ≥ 0 olması** denetleniyor. Karşı örnek: `ida_sim --senaryo scenarios/z1.toml --acil 12 --acil -1`. Son değer −1 olduğundan kullanım hatası oluşmuyor; dosya planında acil olayı bulunmuyor. Tek başına `--acil nan` ve `--acil-kalk` için aynı değerler de denetimi aşıyor. | İki seçenek için kalıcı `*_verildi` bayrakları tut; dosyalı koşuda değerden ve yinelemeden bağımsız çıkış 1 üret. Negatif/NaN ve yinelenen seçenek karşı örneklerini testlere ekle. | kritik |

## Yeniden hesaplananlar

- Yinelenen atamada `12 → −1`; `−1 ≥ 0` yanlış olduğundan ret atlanıyor. `strtod`’un `nan` kabulü [Microsoft belgesinden](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/strtod-strtod-l-wcstod-wcstod-l?view=msvc-170) canlı doğrulandı; `NaN ≥ 0` da yanlış.
- Yerleşik plandaki `[12,18)` saniye aralığı: `6 / 0,00125 = 4800` fizik adımı; mevcut test beklentisi doğru.

## İskelete ya da diğer bölümlere notlar

- Normal pozitif değerler dosyadan önce/sonra verilince reddediliyor. `--ariza` bayrağı yinelemelerde korunuyor. Eşittirli yazım çıkış 1; `z1/bos` yerleşik ad, `.toml` dosya olarak ayrılıyor. Seçenekleri ortamdan okuyan yol yok.
- Yerleşik Z1’in acil ve arıza olaylarını oluşturma yolları korunmuş (`sim/sunucu/src/senaryo_plani.cpp:425`, `:460`).
- Yakalanan hata `sim/sunucu/src/sunucu.cpp:1096` üzerinden oturumdan (`:1266`) ve CSV dosyasının açılmasından (`:1450`) önce dönüyor. Kullanım 1, geçersiz senaryo 3 olarak ayrılıyor (`sim/sunucu/src/main.cpp:161`, `:167`).
- `docs/gorevler/W4-sim-senaryo.md` bu commit’te yok; görev uyumu verilen kullanıcı kararı üzerinden değerlendirildi.