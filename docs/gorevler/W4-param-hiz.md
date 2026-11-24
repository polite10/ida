# W4-param-hiz — `PARAM_SAVE` çağrısının çekirdekteki maliyetini düşürmek

Tarih: 2026-11-23. Yazan: `ida-uygulayici` (Opus). İnceleyen: sol (tek tur). Dal: `is/w4-param-hiz` (taban: `origin/is/w3-zincir`; `main`'e girdiyse `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md` (W3-K12 yerel statik analiz ve W3-K14 zorunlu) ve `docs/gorevler/W4-ortak.md` (W4-K16).

## Amaç

Kartta `PARAM_SAVE`'i işleyen çekirdek çağrısı 3,0–3,6 ms sürüyor (ölçüm, kart; `docs/olcumler/W4-z2.md` ve W3-kart dilim 2 raporu): step bütçesini (3,5 ms) bir kez aştı ve W4-K16'nın çağrı turu koşulunu (≤ 1,9 ms) aşan tek bilinen çağrı. Kaynak flash değil, çekirdekteki blok kodlaması (`ida_prm_blok_kodla`: şema özeti + bit bit hesaplanan CRC; süre parametre sayısıyla büyüyor). Hedef: bu çağrının kartta **1,0 ms'nin altına** inmesi (ölçüm kart agent'ınca yapılacak; bu işte host'ta bağıl ölçüm yeter).

Okunacaklar: A4'ün parametre ve kalıcı blok kuralları (§3.10 parametre bloğu ve CRC'si; A4-K78–K86; şema özeti tanımı), `core/src/` altındaki parametre modülü ve `ida_prm_blok_kodla`, `core/TASARIM.md`'nin ilgili bölümü, `sim/kopru/src/param_blogu.*` ve `sim/sil/src/depo.cpp` (aynı blok biçimini PC'de üreten/okuyan iki yer; yalnız okunur: biçim ve bayt düzeni **değişmez**).

## Kapsam

1. **Şema özeti:** parametre tablosu derleme zamanında sabit olduğundan özet her kodlamada yeniden hesaplanmaz: init'te bir kez hesaplanıp saklanır (ya da derleme zamanında üretilir; hangisi daha sade ve denetlenebilirse). Özetin değeri ve tanımı değişmez.
2. **CRC:** bit bit hesap yerine tablolu (bayt başına) hesap; tablo ROM'da (`const`) durur; çokterim, başlangıç ve son XOR aynı. Bellek bedeli (1 KB) raporlanır. Çekirdeğin başka yerlerinde aynı bit bit CRC kullanılıyorsa (protokol modülü hariç: `protocol/` bu işin dışında) aynı işleve bağlanır.
3. **Kodlamanın yapısı:** gereksiz ikinci geçiş, bayt bayt kopya ya da yinelenen doğrulama varsa sadeleştirilir; davranış ve çıktı baytları aynı kalır.
4. Çağrı başına en kötü süreyi büyüten başka bir parametre işlemi görülürse (ör. `PARAM_COMMIT` doğrulaması) raporlanır; kapsam dışı değişiklik yapılmaz.

## Kapsam dışı

Blok biçiminin, CRC tanımının ya da şema özetinin değişmesi; `protocol/`, `firmware/`, `sim/` altındaki kod; kayıt protokolü (A4-K83) ve emniyet kuralları (silahlıyken `PARAM_SAVE` reddi aynen kalır).

## Dosya sahipliği

Yazılır: `core/src/` altında parametre modülü ve (varsa) ortak CRC yardımcıları, `core/test/**` içinde ilgili birim testleri, `core/TASARIM.md`. Kabul testleri (`tests/kabul/**`) değiştirilmez.

## Kabul

1. **Bayt eşitliği:** aynı parametre durumları için kodlanan blok, değişiklikten önceki uygulamanın ürettiğiyle bayt bayt aynı (eski işlev testte referans olarak tutulur ya da altın vektörler üretilir: varsayılanlar, her tipten uç değerler, rastgele 1 000 durum; sabit tohum). `sim/kopru` ve `sim/sil`'in blok testleri değişmeden geçer.
2. **CRC:** bilinen vektör (`"123456789"` → `0xCBF43926`), boş girdi, tek bayt, 8 KB; bit bit referansla 10 000 rastgele girdide eşitlik.
3. **Şema özeti:** önceki değerle aynı; parametre tablosu değişirse değiştiğini gösteren test (özetin gerçekten tablodan türediği).
4. **Süre (host, bağıl; ölçüm):** kodlama süresi önce/sonra (`host-release`; en az 10 000 tekrar; ortanca ve en kötü); kartta beklenen kazanç "tahmin" diye yazılır.
5. `host-debug` bütün testler (`kabul.cekirdek`, `kabul.gorev`, `zincir.z1` dahil), `fw-release` derleme (kod boyu farkı raporlanır), W3-K12 statik analiz rc = 0, W3-K14 taraması, MSVC `/analyze` kalıpları (C6385/C6386 dizi sınırı, C6011, C6001, C6262).
