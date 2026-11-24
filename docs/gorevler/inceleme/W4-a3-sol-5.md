# sol incelemesi: a3

## Hüküm

**9546be8 için erişim ve emniyet kapsamı KAPANMADI.**

Önceki somut karşı örneklerde **#2a kapandı, #2b kapandı, #3a kapandı (`Z\r\n` chunked hatası), #3b kapandı.** Yaş sırası ve ilk okuma koruması uygulanıyor; jetonsuz istek öne alınmıyor; chunked hatası ilgili istemcinin jetonlarını siliyor; kimliksiz istek diğer istemcileri etkilemiyor.

Buna rağmen aşağıdaki iki kritik yol kalıyor. Bulgular statik kod izlemesine dayanır; derleme veya test çalıştırılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `sim/a3/src/sunucu.cpp:185`, `:199`; `sim/a3/src/cekirdek.cpp:566` | **Yeni bağlantıların sürekli reddi mümkün.** Desteklenen `azami_http=4` ayarında her kabul turunun sırası dört yeni jetonsuz saldırgan bağlantısı, ardından meşru bağlantı olsun. İlk dördü eski bağlantıları değiştirir; henüz okuma fırsatı bulmadıklarından korunurlar. Geçerli Bearer taşıyan kısıtlayan isteği hazır olan meşru bağlantı okunmadan 503 ile kapatılır; mandala ulaşmaz. Aynı sıra her turda kurulursa yeniden denemeler de reddedilir. Mevcut soket testi meşru bağlantıyı saldırganlardan **önce** kuruyor (`test/test_komut.cpp:1453`). | Tahliye edilebilen bağlantılar tükendiğinde yeni bağlantıları kabul edip reddetmeye devam etmeden okuma turuna dön. Saldırgan bağlantıları önce gelen ve meşru istemcinin yeniden denediği belirlenimci test ekle. | kritik |
| 2 | `sim/a3/src/cekirdek.cpp:1079`, `:1084`, `:1095` | **(4′) erken erişim reddinde atlanıyor.** A istemcisi T jetonunu aldıktan sonra geçerli Bearer, geçerli `Ida-Istemci`, yanlış `Origin` ve tam çerçevelenmiş fakat bozuk JSON ile `/api/v1/emniyet/degistir` isteği göndersin. Başlık aşamasında Bearer doğrulanmıştır (`:693`), ancak Origin reddi jeton işleminden önce döner. T silinmez; aynı run/revizyonda, 10 saniye içinde düzgün istekte kullanılabilir. Yanlış/eksik Host yolu da aynı sonucu verir. | Geçerli Bearer ve geçerli istemci kimliği bulunan gevşetme isteğinin bütün ret yollarında (4′) uygulanmalı; erişim reddi korunurken jeton geçersizleştirme atlanmamalı. | kritik |

## Yeniden hesaplananlar

- HTTP çıkış tavanı: **4 MiB + 16 MiB + 64 KiB = 21.037.056 B**.
- Standart 403 yanıtı **231 B**; **18.158 × 231 = 4.194.498 B**, 4 MiB eşiğini aşar. Sonraki jetonsuz istekler öne alınmaz; sıra 33 isteğe çıktığında kapanır. Eski sınırsız büyüme karşı örneği kapanmıştır.
- Kabul karşı örneği tur başına **4 saldırgan + 1 meşru bağlantı** gerektirir; 64 kabul bütçesine sığar. Bunlar hesap ve kod çıkarımıdır, ölçüm değildir.

## İskelete ya da diğer bölümlere notlar

`Ida-Istemci` adı küçük harfe çevriliyor, dış SP/HTAB kırpılıyor; yinelenen, iç boşluklu veya 20 karakterden uzun kimlik geçersiz sayılıyor. Başka kimlikle sunulan jeton kullanılmıyor ve tüketilmiyor (`http.cpp:139`, `:149`; `cekirdek.cpp:1205`, `:1039`). Başlık davranışı canlı [RFC 9112 §5](https://www.rfc-editor.org/rfc/rfc9112.html#section-5) kaynağıyla karşılaştırıldı.

`docs/gorevler/W4-a3.md` bulunamadı; hüküm W4-K23 ve ekine göredir. #1a/#1b yeniden değerlendirilmedi. **Model dışı not:** statik köke yazabilme ve Bearer sahibinin kendi hizmetini bozması bulgu sayılmadı.