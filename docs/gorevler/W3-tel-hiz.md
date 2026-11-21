# W3-tel-hiz — MP5 iyileştirmelerinin ürün tel katmanına taşınması

Tarih: 2026-11-21. Yazan: `ida-uygulayici` (Opus). İnceleyen: sol (tek tur). Dal: `is/w3-tel-hiz`. Ortak kurallar: `docs/gorevler/W3-ortak.md`.

## Amaç

W0-MP5a ölçümü, ürün tel katmanının ilk hâlinin kartta süre eşiklerini aştığını, prototipteki iyileştirilmiş kopyanın sağladığını gösterdi (ölçüm: 1 050 B ayrıştırma 0,153 ms, periyot toplamı 0,264 ms; `fw-release`, 160 MHz; bayt bayt beslemede sağlamıyor). Bu iş iyileştirmeleri ürün koduna taşır.

Okunacaklar: `olcum/mp5-tel-maliyeti` dalındaki `docs/olcumler/MP5-tel-maliyeti.md` ve `tools/olcum/mp5/kopya/` (`tel_iyi.c`, `tel_crc_tablo.c`, `cesitler.txt`); dalı kendi çalışma ağacına birleştirme, dosyaları `git show origin/olcum/mp5-tel-maliyeti:<yol>` ile oku. `protocol/ida_link/` (tel katmanı, testleri, `olcum/`); A1 §3'ün çerçeve biçimi.

## Kapsam

1. Rapordaki iyileştirmeleri (CRC tablosu, blok hâlinde ayrıştırma, raporun kazanç gösterdiği öteki değişiklikler) `protocol/ida_link/src/tel.c` ve gerekiyorsa yeni kaynak dosyalarına taşı. Dışa açık arayüz ve davranış değişmez.
2. Blok besleme yolu: alıcıya bayt bloğu veren işlev birincil yoldur; bayt bayt besleme desteklenir ama `BENIOKU.md`'de kartta kullanılmaması gerektiği ölçümle birlikte yazılır.
3. Tablo boyu (flash) ve hız arasındaki seçim rapordaki ölçüme göre yapılır ve gerekçesi yazılır.
4. `protocol/ida_link/olcum/` altındaki host ölçümüyle önce ve sonra karşılaştırması.

## Kapsam dışı

Tel biçimi, `dialect_hash`, altın çerçeveler (değişmez); oturum ve parti katmanı (`W3-oturum` işi aynı dizinde **yeni** dosyalar yazıyor; onlara dokunma); kartta ölçüm (kartı `W3-kart` işi kullanıyor; kart ölçümü o işin ikinci diliminde yapılır).

## Dosya sahipliği

Yazılır: `protocol/ida_link/src/tel.c`, tel katmanının iç başlıkları, yeni `tel_*.c` dosyaları, `protocol/ida_link/test/test_tel.c`, `protocol/ida_link/olcum/**`, `protocol/ida_link/BENIOKU.md`.

## Kabul

1. Var olan 37 birim testi, 65 kabul testi (`tests/kabul/protokol`), 38 altın çerçeve ve ayna eşitlikleri değişmeden geçer.
2. Eski ve yeni ayrıştırıcının aynı bayt akışında (altın çerçeveler + sabit tohumlu rastgele ve bozulmuş akışlar; her blok boyu bölünmesiyle) aynı mesaj ve sayaç dizisini ürettiğini gösteren fark testi.
3. Host ölçümü: önce ve sonra (bu Mac, komut ve sayılarla; ölçüm).
4. `host-debug`, `host-asan`, `fw-debug`, `fw-release`; `fw-release`'te kod ve sabit veri boyu farkı raporlanır.
