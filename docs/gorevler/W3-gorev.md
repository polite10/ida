# W3-gorev — çekirdekte kipler, silahlandırma, arıza güvenliği ve güdüm

Tarih: 2026-11-21. Yazan: `ida-uygulayici` (Opus). Bağımsız kabul testleri: `ida-test-yazari` (emniyete dokunduğu için). İnceleyen: sol. Dal: `is/w3-gorev`. Ortak kurallar: `docs/gorevler/W3-ortak.md`.

## Amaç

`core/src/gorev/` ve `core/src/gudum/` yer tutucularını doldurmak: aracın kipleri, araç silahlandırması (arm/disarm), arıza güvenliği eylemleri ve basit güdüm. Bu modül `armed` ve `mode` alanlarına **genişleten yönde yazan tek yerdir** (`core/TASARIM.md` §1); bu yüzden iş emniyet kapsamındadır.

Okunacaklar: `core/TASARIM.md` (özellikle §1, §4, kancalar `ida_gor_komut`, `ida_gor_adim`); A4 sözleşmesi: komutlar ve komut sonuçları, ret nedenleri tablosu, emniyet durumu ve kısıtlama kuralları (A4-K46, K81, K82, K113–K116 ve çevresi), durum çıktısı; `protocol/komutlar.json`; iskelet §3.2; `docs/mimari/02-ortak-cekirdek.md` §2.4, §2.6, §4.

## Kapsam (dilim 1 = madde 1–4)

1. **Kip makinesi:** 02 §2.6'daki kipler; her kip × olay çifti tanımlı ve tablo olarak kodlu. Z1 için en az: silahsız, elle (doğrudan istek), derinlik ve yön tutma.
2. **Silahlandırma:** arm ve disarm komutlarının işlenmesi; arm'ın ön koşulları (kestirim düzeyi, emniyet kısıtı yok, bağlantı durumu) ve ön koşul tutmuyorsa A4'teki ret nedeniyle ret. Kısıtlayan her şey aynı adımda genişletenden önce uygulanır. Emniyet modülünün uyguladığı daraltma bu modülce geri alınamaz.
3. **Arıza güvenliği:** 02 §4.2 ve §4.3'teki kip × arıza matrisi: sensör kaybı, kestirim bozulması, bağlantı kaybı, düşük batarya, sızıntı için eylemler (tut, yüzeye çık, silahsızlan). Eylem yalnız kısıtlayan yöndedir.
4. **Kontrol isteği:** etkin kipe göre `kontrol.h`'deki istek yapısını doldurur (yapıyı `W3-kontrol` işi tanımlıyor; o iş bitmeden gerekiyorsa aynı alanları kendi başlığında geçici tanımla ve raporla).
5. **Güdüm (dilim 2):** yol noktası listesi, görüş hattı güdümü, derinlik profili; görev öğelerinin yüklenmesi ve ilerlemesi.
6. Parametreler `gorev_param.def` ve `gudum_param.def`'te.

Emniyet ilkesi: tanımsız kip, tanımsız komut ya da çelişen girdi her zaman silahsız ve sıfır itki yönünde çözülür. Belirsizlikte kısıtlayan okuma seçilir ve `EMNİYET` diye raporlanır.

## Kapsam dışı

Atış kontrol (silah silahlandırması `FC_ARM`, atış izni, durum makinesi W4'tedir; bu işte hiçbir genişletici olay üretilmez); kestirim; kontrol yasaları; emniyet modülü ve çağrı çerçevesi (dokunulmaz).

## Dosya sahipliği

Yazılır: `core/src/gorev/**`, `core/src/gudum/**`, `core/test/gorev/**`, `core/test/gudum/**`. Bağımsız kabul testleri: `tests/kabul/gorev/**` (test yazarı).

## Kabul

1. Kip tablosu birim testiyle eksiksiz taranır (her kip × olay).
2. Arm ön koşullarının her biri için: koşul tutmazken ret ve doğru neden; tutarken kabul.
3. Arıza matrisinin her hücresi için bir vaka; hiçbir arıza eylemi `armed` ya da `mode`'u genişletmez.
4. Rastgele komut ve arıza dizilerinde (sabit tohum) değişmezler: emniyet kısıtı varken `armed` hiçbir adımda doğru olmaz; disarm ile aynı adımda gelen arm etkisizdir; yinelenen komut ek etki üretmez.
5. Belirlenimcilik; `fw-release` kod boyu.
6. Var olan çekirdek testleri geçer; W2 kabul testlerinde "gevşetme W4'te" diye atlanan dört test atlanmış kalır.

Bağımsız kabul testleri yalnız A4, `port.h`, `komutlar.json` ve bu belgeden yazılır; uygulama kaynağı okunmaz. Kapsamı kabul md.2–4.
