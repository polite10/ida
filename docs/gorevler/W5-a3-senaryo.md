# W5-a3-senaryo — sim'de senaryo ve ön denetim uçları

Tarih: 2026-11-25. Yazan: `ida-hafif-uygulayici` (Sonnet; K28). İnceleyen: `ida-inceleyici` (Opus; emniyet dışı dilim; K23, K27). Dal: `is/w5-a3-senaryo` (taban `origin/main`; PR #55 `main`'e girdikten sonra başlar: ikisi de `sim/sunucu/src/sunucu.cpp`'ye dokunur). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `W4-ortak.md`, `W5-ortak.md` (W5-K43).

**Sınıf düzeltmesi (mimar, 2026-11-25 gece):** bu dilim emniyet dışı yazılmıştı; yanlıştı. Yeniden kurulum sanal emniyet girdilerinin ve `emniyet_rev` sayacının ne zaman başlangıç değerine döndüğünü belirliyor. Dilim emniyet sınıfındadır (K21): tam inceleme + dar denetim; incelemesiz birleşmez (K27). İlk bulgu (mimar; sözleşmeden, kodda doğrulanmadı): PR #57 `emniyet_rev`'i yeniden kurulumda 1'e döndürüyor; A3-K117 ve A3-K67 sayacın süreç boyunca geri dönmemesini ister.

## Amaç

`ida_sim` senaryoyu yalnız komut satırından alıyor. A3'ün senaryo ve ön denetim uçları sunucuda yok; panel (W5-web-komut) bu yüzden senaryo listeleyemiyor ve yükleyemiyor. Bu dilim uçları ekler: kullanıcı sim'i yeniden başlatmadan tarayıcıdan senaryo değiştirir.

## Okunacaklar

`docs/sozlesmeler/A3-web.md` §3.6, §3.8, §3.9, §4.7 (Tablo 4.7a), §4.9 (A3-K76…K80); A5 sözleşmesinin doğrulama raporu ve kanonik biçim kuralları (A5-K02, K17); `sim/sunucu/BENIOKU.md`, `sim/a3/src/cekirdek.cpp` (uçların dağıtımı), `sim/sunucu/src/sunucu.cpp` (`sifirla` yolu ve dünyanın kurulumu), `sim/senaryo/`.

## Kapsam

1. `GET /senaryolar`, `GET /senaryolar/{ad}`, `POST /senaryo/dogrula`, `POST /senaryo/yukle`, `GET /senaryo/cozumlenmis`, `GET /on_denetim`, `GET /on_denetim/{rapor_kimligi}`.
2. **Dünyanın yeniden kurulması.** `ida_sim` bugün süreç başına tek çalıştırma koşuyor: `durduruldu` durumunda `sifirla` `gecersiz_durum` ile reddediliyor (`sim/sunucu/src/a3_baglanti.cpp`, `sunucu.cpp`; yorum "A3-K67 bu dilimde yok"; W5-web-komut raporunda bulundu). Bu dilim A3-K67'yi uygular: `sifirla` Tablo 4.7a'daki durumlardan `yuklendi`'ye döner ve dünya baştan kurulur; yeni çalıştırma yeni `run_id` alır. `yukle` aynı kurulum yolunu yeni senaryoyla kullanır; geçersiz senaryo durumu değiştirmez (A3-K77). Kartlı kipte (HIL) yeniden kurulumun kart oturumuna etkisi sözleşmeden okunur; kartsız sınanamayan kısım raporda "koşturulmadı" diye yazılır.
3. Senaryo dizini: yeni seçenek `--senaryo-dizini` (varsayılan `scenarios/`). Ad kalıbı A3-K78; dizin dışına çıkan her ad reddedilir.
4. Testler: uçların her biri; geçersiz ad (`../x.toml`, mutlak yol, kalıp dışı ad, sembolik bağ); geçersiz senaryoda durumun değişmediği; panelin A3 istemcisiyle gerçek sim'e karşı listele → doğrula → yükle → başlat (var olan Node uçtan uca test kalıbı).

## Mimar kararları

- **W5-K47 — Yükleme belirlenimi bozmaz.** `yukle` ile yüklenen senaryonun koşusu, aynı senaryo komut satırından verildiğinde üretilenle aynı CSV'yi ve aynı A1 akışını üretir; test bunu karşılaştırır. Tutmuyorsa kusur raporlanır, eşik gevşetilmez.
- **W5-K48 — Komut satırı davranışı değişmez.** `--a3-port` verilmeyen ve `yukle` çağrılmayan koşuların çıktısı bayt bayt aynı kalır.
- **W5-K49 — A2 ve kayıt.** Yeni senaryo yüklendiğinde A2 çalıştırma nesli artar (görselleştirici eski sahneyi bırakır). Kayıt açıksa çalıştırma başına bir dosya kuralı (A6) sürer; ön denetim raporu, PR #55'in ürettiği kayıt ekiyle aynı içeriği verir (iki ayrı üretici yazılmaz).
- **W5-K50 — Yapılmayan denetim "geçti" yazmaz.** Ön denetim raporunda gerçekten ölçülmeyen madde `atlandi` ve nedeniyle yazılır.

## Kapsam dışı

`PUT /senaryolar/{ad}` (dosya yazma); hata enjeksiyonu, kayıt listesi ve izleme uçları; `yeniden_kostur`; `web/src` (panel ayrı dilimde; uyumsuzluk bulunursa raporlanır); `core/`, `firmware/`, `sim/kopru`. Senaryosuz açılış (`--senaryo` olmadan `--a3-port`) yalnız sözleşme bunu tanımlıyorsa eklenir; tanımlamıyorsa eklenmez ve raporda belirtilir.

## Kabul

1. Yeni testler `host-debug` ve `host-release`'de geçer; tam tur gönderimden önce bir kez. `build/host-asan` altında test koşturulmaz.
2. W5-K47 ve W5-K48 testle gösterilir.
3. Dizin dışı erişim denemelerinin hepsi 400 ile reddedilir ve dosya sistemine dokunmaz (testle).
4. Node uçtan uca testi Windows ve macOS CI'da koşar (atlanmaz) ve geçer.
5. `yukle` sırasında dünya döngüsünün durduğu süre ölçülür ve raporlanır (ölçüm; bu Mac).

## Rapor

PR ve dal ucu; kabul maddeleri; ölçüm; seçilen okumalar; `web/src` ile bulunan uyumsuzluk; koşturulmayanlar.
