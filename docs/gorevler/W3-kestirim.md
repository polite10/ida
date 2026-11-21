# W3-kestirim — çekirdekte seyrüsefer kestirimi (dilim 1: yönelim ve derinlik)

Tarih: 2026-11-21. Yazan: `ida-uygulayici` (Opus). İnceleyen: sol (tek tur). Dal: `is/w3-kestirim`. Ortak kurallar: `docs/gorevler/W3-ortak.md`.

## Amaç

`core/src/kestirim/` yer tutucusunu gerçek kestirimle doldurmak: çekirdek, A4 portundan aldığı zaman damgalı sensör örneklerinden aracın yönelimini, derinliğini, hızını ve konumunu kestirir ve `nav` çıktısını üretir. Z1 için yönelim ve derinlik yeterlidir; öncelik sırası aşağıdadır.

Okunacaklar: `core/TASARIM.md` (modül sınırları, kancalar, parametre düzeni); A4 sözleşmesi: sensör örnek tipleri ve kabul kuralları, `nav` çıktısı ve `est_level` anlamı, zaman kuralları; `core/include/ida/port.h`; iskelet §3.1, §4; `docs/mimari/02-ortak-cekirdek.md` §2.3, §4.1.

## Kapsam (öncelik sırasıyla; dilim 1 = Ö1 ve Ö2)

- **Ö1 Yönelim:** jiroskop, ivmeölçer ve manyetometreden kuaterniyon; jiroskop sapması kestirimi; manyetik bozulmada ve yüksek ivmede düzeltmenin kısılması.
- **Ö2 Derinlik ve dikey hız:** basınç örneğinden; IMU ile birleştirilmiş.
- **Ö3 Gövde hızı ve ölü hesap konum:** DVL hızı varken; DVL yokken belirsizliğin büyümesi.
- **Ö4 GNSS düzeltmesi:** yalnız yüzeyde, geçerli örnek varken.
- Her kademe için `est_level` ve geçerlilik bayrakları A4'teki anlamıyla; bayat ya da reddedilmiş örnek kestirimi bozmaz (kabul kapısı çerçevededir; bu modül kapıdan geçmiş örneği kullanır).
- Örneklerin ölçüm zamanı ile adım zamanı arasındaki fark A4'ün zaman kurallarıyla ele alınır.
- Modül parametreleri `kestirim_param.def`'e eklenir.

Yöntem 02 §2.3'teki tasarımdır; orada iki seçenek bırakılmışsa kartta daha ucuz olanı seç ve "Okumalar"a yaz.

## Kapsam dışı

Güdüm, kontrol, kipler; sonar ve kamera algılama kayıtlarının kullanımı; atış kontrol; çağrı çerçevesi, emniyet ve parametre altyapısı (W2'de tam; dokunulmaz).

## Dosya sahipliği

Yazılır: `core/src/kestirim/**`, `core/test/kestirim/**`. Çerçevenin kanca imzaları değişmez; değişmesi gerekiyorsa yazma, raporla.

## Kabul

1. **Durgun:** gürültülü durgun girdide yönelim ve derinlik hatası ile sapma kestirimi belirtilen sınırlar içinde yakınsar.
2. **Hareketli:** sabit dönüş, salınımlı yalpa ve basamak derinlik değişimi içeren sentetik yörüngelerde (testin içinde üretilir, sabit tohum) hata sınırları sağlanır.
3. **Bozulma:** manyetometre sıçraması ve kısa süreli yüksek ivme yönelimi belirtilen sınırdan fazla bozmaz.
4. **Eksik sensör:** basınç ya da manyetometre kesilince çıktı sonlu kalır, ilgili geçerlilik düşer, sensör dönünce toparlar.
5. **Belirlenimcilik** (A4-K24): aynı girdi dizisi bayt bayt aynı çıktı.
6. **Kaynak:** `fw-release` derlemesinde modülün kod boyu ve `ida_core_t` içindeki payı raporlanır; adım süresi kartta ölçülmediği için süre rakamı verilmez.
7. Var olan çekirdek birim ve kabul testleri (`core/test`, `tests/kabul/cekirdek`) geçmeye devam eder; yer tutucu davranışına bağlı bir test artık geçmiyorsa testi değiştirme, raporla.
