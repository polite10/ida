# W3-kontrol — çekirdekte kontrol ve itki tahsisi

Tarih: 2026-11-21. Yazan: `ida-uygulayici` (Opus). İnceleyen: sol (tek tur). Dal: `is/w3-kontrol`. Ortak kurallar: `docs/gorevler/W3-ortak.md`.

## Amaç

`core/src/kontrol/` yer tutucusunu doldurmak: kestirilen durum ve istenen değerlerden sekiz iticinin normalleştirilmiş itki isteğini üreten denetleyiciler ve itki tahsisi. Z1 için derinlik, yalpa/baş-kıç ve yön tutma gerekir.

Okunacaklar: `core/TASARIM.md`; A4 sözleşmesi: aktüatör çıktısı (`thr_cmd`, `thr_valid`), silahsız ve güvenli çıktı kuralları, çıktı kapısı; `core/include/ida/port.h`; iskelet §3.1, §3.2, §5; `docs/mimari/02-ortak-cekirdek.md` §2.5, §2.7; A5 Ek E'deki itici geometrisi (tahsis matrisi buradan türetilir ve parametre olarak tutulur).

## Kapsam

1. **Denetleyiciler:** derinlik, yalpa, baş-kıç, yön (dış döngü) ve açısal hız (iç döngü); ileri ve yan hız ya da doğrudan kuvvet isteği. Integral sarmasına karşı önlem, çıkış ve değişim hızı sınırları, istenen değerde basamak yumuşatma.
2. **İtki tahsisi:** altı eksenli kuvvet-moment isteğinden sekiz itici isteğine; doymada eksen önceliği (02 §2.5'teki sıra; yoksa: önce yalpa/baş-kıç ve derinlik, sonra yön, sonra yatay öteleme); bir iticinin devre dışı sayılması durumunda yeniden tahsis.
3. **Güvenli davranış:** silahsızken, emniyet kısıtı varken ve kestirim yetersizken itki isteği sıfırdır; bu kararın girdileri çerçeveden ve görev modülünden gelir, bu modül yalnız uygular. Bu madde `EMNİYET` kapsamındadır: kuşkuda sıfır itki.
4. **Arayüz:** görev/güdüm modülünden gelen istek yapısı (kip, istenen derinlik, yön, hızlar) `kontrol.h`'de tanımlanır; `W3-gorev` işi aynı anda yazıldığı için yapı sade tutulur ve raporda tam tanımı verilir.
5. Kazançlar ve sınırlar `kontrol_param.def`'te; varsayılanlar İDA-0 için elle ayarlanmış başlangıç değerleridir (ayar Z1'de fizikle birlikte yapılacak; "tahmin" olarak işaretle).

## Kapsam dışı

Kestirim, kipler ve görev, güdüm; normalleştirilmiş istekten darbe genişliğine çeviri (platformun işi); fizik modeli (test için küçük bir doğrusal bitki modeli testin içinde yazılır, `sim/` kullanılmaz).

## Dosya sahipliği

Yazılır: `core/src/kontrol/**`, `core/test/kontrol/**`.

## Kabul

1. **Tahsis:** tek eksenli her istek beklenen itici desenini verir; tahsis edilen itkilerin ürettiği kuvvet-moment isteğe eşittir (doyma yokken, tolerans içinde).
2. **Doyma:** aşırı istekte hiçbir çıktı [−1, 1] dışına çıkmaz ve öncelik sırası korunur.
3. **Basamak yanıtı:** testteki doğrusal bitki modelinde derinlik ve yön basamağında aşma ve oturma süresi belirtilen sınırlar içinde; kalıcı hata sıfıra gider.
4. **Sarma:** uzun süreli doymadan sonra toparlanma süresi sınır içinde.
5. **Güvenli çıktı:** silahsız, kısıtlı ve kestirimi yetersiz durumların her birinde bütün itki istekleri tam sıfırdır; rastgele girdi dizisiyle (sabit tohum) de gösterilir.
6. **Belirlenimcilik** ve `fw-release` kod boyu raporu.
7. Var olan çekirdek testleri geçmeye devam eder; yer tutucuya bağlı bir test artık geçmiyorsa testi değiştirme, raporla.
