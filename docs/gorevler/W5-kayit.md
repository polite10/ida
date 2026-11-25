# W5-kayit — kayıt kitaplığı (A6), dilim 1: yazıcı, okuyucu, kurtarma

Tarih: 2026-11-25. Yazan: `ida-kritik-uygulayici` (K27). İnceleyen: sol (emniyet dışı dilim; K23; sol sınırı doluyken K25/K27). Dal: `is/w5-kayit` (taban `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `W4-ortak.md`, `W5-ortak.md`.

## Amaç

Bugün bir çalıştırmadan geriye yalnız CSV kalıyor. Bu iş A6 sözleşmesinin kayıt dosyasını üreten ve okuyan kitaplığı (`sim/kayit/`, `ida_kayit`) yazar: MCAP dosyası, kanal ve şema kataloğu, kayıt kademeleri, üreticileri bekletmeyen yazım yolu, üst veri, kapanış, bütünlük denetimi, kurtarma okuması ve `izleme` düzeyi için sıralı okuyucu. Veri kaynağı soyuttur: bu dilimde kitaplık sahte üreticilerle sınanır. `ida_sim`'e bağlanması (fizik adımı, A1 baytları, çekirdek çağrıları kancaları), tekrar sürücüleri ve A3 uçları **sonraki dilimlerdir** (`sim/sunucu` şu an W5-bagla-a2'de).

Okunacaklar: `docs/sozlesmeler/A6-kayit.md` §1–§3.3, §3.5, §3.6, §4.1–§4.5, §4.8–§4.10, §5, §6, §7 ve bu dilimde yazılan kanalların §3.4 yük düzenleri; `docs/sozlesmeler/sema/a6-kanallar.json` (katalog; yazıcıya gömülür: A6-K07); A3 §3.1 (tel tipleri; A6-K08); `sim/a3/src/kodlayici.hpp` ve kataloğundan tablo üreten düzen (A6-K08: "A3 ile aynı kodlayıcı"); `sim/a2/src/halka.hpp` ve `sim/a3/src/halka.hpp` (tek üretici–tek tüketici halka örnekleri); `docs/mimari/08-laboratuvar-uygulamasi.md` §3.5 ve `04` §2.12 (fikir kaynağı; bağlayıcı değil); karar günlüğünde Ö7 ve Ç11 (MCAP C++ doğrudan).

## Mimar kararları (kayıtta `W5-ortak.md`'ye işlenir)

- **W5-K35 — Bağımlılıklar.** Yazıcı `foxglove/mcap` C++ kitaplığıdır (A6-K03); sıkıştırma zstd. İkisi de vcpkg'den alınır ve sürümleri `overrides` ile sabitlenir; sürüm ve lisans birincil kaynaktan doğrulanır ve raporlanır. lz4 kullanılmaz: kitaplık lz4'süz derlenebiliyorsa öyle derlenir, derlenemiyorsa lz4 yalnız derleme bağımlılığı olarak kalır ve raporlanır. vcpkg portu yoksa ya da Windows'ta derlenmiyorsa depo içi overlay port yazılır (toml++ örneği). Rust tabanlı araç ya da kitaplık yok.
- **W5-K36 — Kodlayıcının paylaşımı.** İleti yükleri A3 ile aynı kodlayıcıyla üretilir (A6-K08). `ida_kayit`, `ida_a3`'ün kodlayıcısını yeniden kullanır; bunun için `sim/a3`'te yalnız var olan kodlayıcının ayrı bir hedefe/başlığa açılması gibi **davranış değiştirmeyen** en dar düzenleme yapılabilir (A3 testleri değişmeden geçer). Bu mümkün değilse aynı üretim düzeni `a6-kanallar.json` için `sim/kayit` içinde kurulur ve iki kodlayıcının aynı tel tipleri için aynı baytları verdiği bir testle gösterilir. Elle yazılmış alan sırası olmaz.
- **W5-K37 — Bu dilimde yazılmayan kanallar.** `kamera/kare` ve kamera pencereleme (A6-K19) yoktur; öteki kanalların hepsi için şema, kanal kaydı ve kodlama kitaplıkta bulunur (üreticisi henüz yoksa da), kademe süzgeci genel yazılır.

## Kapsam

1. **Dosya (A6-K01–K04, K12, §3.1, §3.2):** `<dizin>/<run_id>.mcap`; var olan dosya ezilmez; Header `profile = "ida"`; her (yük, sürüm) için şema kaydı (kanonik JSON gömülü katalogdan, A6-K07), her kanal için kanal kaydı ve `metadata`; zstd parçalar, hedef 1 MiB, parça CRC'si, indeksler ve özet; açık parça en geç 1 s duvar saatinde kapatılır; `sequence` sınırında `sinira_ulasti` ile kapanış.
2. **Yazım yolu (A6-K05, K06):** dosyaya tek iş parçacığı yazar; üretici başına sabit boylu tek üretici–tek tüketici halka; üretici çağrısı **engellemez, bellek ayırmaz, sistem çağrısı yapmaz**; halka doluysa ileti atılır, düşen iletinin kanalı, sim zamanı ve adımı sayılır ve `kayit/eksik` olarak dosyaya yazılır (§4.5).
3. **İletiler ve zaman (A6-K08–K11, K13):** `log_time`, `publish_time` (kayıt duvar saati: açılışta 0), `sequence`; negatif `log_time` yazılmaz ve bildirilir.
4. **Kademe (A6-K16, K17, K18, K20):** `olaylar`, `telemetri`, `tam`; kademe çalıştırma boyunca değişmez; `hil/bariyer`'in `olaylar` kademesindeki süzgeci; her kademede bulunması zorunlu kanallar hiçbir kademede süzülmez.
5. **Üst veri ve ekler (§3.5, §3.6):** `ida_kayit` (açılış) ve `ida_kayit_kapanis` (kapanış) kayıtları; alanlar çağıran taraftan yapı olarak alınır; ekler dosyaya sözleşmedeki adlarla yazılır.
6. **Kapanış ve bütünlük (§4.8, §4.10):** düzgün kapanış, hata kapanışı, yarım dosya; `uncompressed_crc` her parçada; okuyucuda CRC doğrulaması ve "bozuk dosya" davranışı (A6-K59); kapanmamış dosyanın kurtarma okuması (A6-K60).
7. **Okuyucu (`izleme` düzeyi için):** dosyayı açar, sürüm uyumluluğunu denetler (§4.9), iletileri `log_time`, eşitlikte `sequence` sırasıyla verir (A6-K11), kanal/kademe/üst veri/ek sorguları, eksik kayıt bildirimleri ve önek güvencesinin sınırı (A6-K33: ilk eksik adım).
8. **`ida_kayit_dok`:** bir kayıt dosyasının özetini (üst veri, kanallar, ileti sayıları, eksikler, bütünlük sonucu) yazan küçük program; testlerde ve elle denetimde kullanılır.

## Kapsam dışı

`ida_sim`'e bağlama ve kayıt kancaları; tekrar sürücüleri (`fizik`, `girdi`, `yeniden_kurma`; §4.6, §4.7); A3 kayıt/izleme uçları; dışa aktarıcı; kamera kareleri; `sim/sunucu`, `sim/kopru`, `sim/a2`, `core/`, `protocol/`, `firmware/`, `web/`, `viz/`, `docs/sozlesmeler/`. Atış çözümü, takip ve hedefleme verisi kaydedilmez (A6 §1; atış kontrol sınırı).

## Dosya sahipliği

Yazılır: `sim/kayit/**`, `sim/CMakeLists.txt`'e tek satır, `vcpkg.json` (yalnız bağımlılık ve `overrides` ekleme), gerekiyorsa depo içi overlay port dizini, W5-K36'daki dar `sim/a3` düzenlemesi.

## Kabul (testleri uygulayıcı yazar; CTest, var olan adlandırma düzeniyle)

1. **Biçim:** yazılan dosya `mcap` kitaplığının kendi okuyucusuyla hatasız açılır; sihirli baytlar, `profile`, şema adları ve kodlama adları §3.2'deki gibi; şema `data`'sı kataloğun kanonik JSON'ıyla bayt bayt aynı (A6-K07); kanal `metadata`'sı katalogla aynı.
2. **Gidiş-dönüş:** her kanalın yükü için yaz → oku bayt eşitliği ve alan değerleri; A3 kodlayıcısıyla aynı baytlar (W5-K36); `log_time`/`publish_time`/`sequence` kuralları; okuyucunun sırası (A6-K11) dosya sırası bozukken de doğru.
3. **Kademe:** üç kademede hangi kanalların yazıldığı Tablo 3.3 ile birebir; zorunlu kanallar her kademede; `hil/bariyer` süzgeci.
4. **Üretici beklemez:** yazıcı durdurulmuş, disk yavaş (yazım sarmalayıcısıyla yapay gecikme) ve halka doluyken üretici çağrısının süresi (p99 ve en kötü; ölçüm, bu Mac; eşik ortak zamanlama yardımcısıyla, W4-K24/K29) sağlıklı koşudan ayırt edilemez; düşen her ileti sayılır ve `kayit/eksik`'te görünür; hiçbir ileti sessizce kaybolmaz (yazılan + düşen = üretilen).
5. **Çökme ve kurtarma:** yazıcı süreci yazım ortasında öldürülür (ayrı süreç): kurtarma okuması kapanmış bütün parçaları verir, kayıp en çok son 1 s'lik açık parçadır (enjekte edilen saatle); okuyucu dosyayı "kapanmamış" olarak işaretler.
6. **Bozulma:** bir parçanın bir baytı çevrilir → okuyucu CRC hatasını saptar ve A6-K59'daki davranışı uygular; üst veri ya da ek bozulursa ilgili hata; rastgele kesilmiş ve bozulmuş dosyalarla (sabit tohum, en az 10 000 vaka) okuyucu çökmez, sınırsız bellek ayırmaz, kilitlenmez.
7. **Sınırlar:** var olan dosya ezilmez (A6-K01); negatif `log_time` yazılmaz (A6-K10); `sequence` sınırı enjekte edilen sayaçla (A6-K12); kademe çalıştırma içinde değiştirilemez (A6-K20).
8. `host-debug` + `host-release` tam tur (sonda bir kez); `host-asan` yalnız derleme (bu Mac'te ASan'lı ikili koşturulmaz); W3-K14 taraması; MSVC kalıpları (kitaplık başlıkları MSVC `/analyze` altında uyarı veriyorsa nasıl ele alındığı raporlanır); Windows CI süresine etkisi PR koşusundan okunur.

## Rapor

PR ve dal ucu; seçilen sürümler ve lisansların kaynağı; kabul maddelerinin sonucu ve kanıtı; ölçülen üretici çağrısı süreleri ve yazım hızı (ölçüm, bu Mac); dosya boyutu örneği (ör. 60 s `telemetri` kademesinde sahte veriyle; ölçüm); W5-K36'da seçilen yol; sözleşmede bulunan belirsizlikler ve seçilen okuma (K20); sonraki dilim (bağlama) için kanca noktaları ve üreticilerin listesi.
