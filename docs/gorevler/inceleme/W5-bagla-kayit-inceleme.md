# W5-bagla-kayit incelemesi (PR #55, uç `844fadb`)

Tarih: 2026-11-25. İnceleyen: `ida-inceleyici` (Opus; K23 tek tam inceleme). Yalnız okuyarak yapıldı; derleme ve test koşturulmadı. Aşağıdaki metin inceleyicinin raporudur (agent çıktısı; mimar kararı değildir).

**Mimar kararı (düzeltme turu):** 1, 2, 3, 4, 5, 6, 7 (yalnız `tekrar_guvencesi`), 9, 10, 11, 12, 13 bu turda düzeltilir. 7'nin öteki iki maddesi (A3 olay tür 15 "açıldı"; A3 açıkken aynı adlı dosyada `baslat` yanıtı) ve 8 (test kapsamı) açık bulgulara yazılır.

---

**Hüküm:** PR #55 düzeltme turundan sonra birleştirilebilir. W5-K41'i delen bir yol var (bulgu 1). Emniyet zincirine dokunan bir bulgu yok.

İnceleme yalnız okuyarak yapıldı (`git diff origin/main...HEAD`, uç `844fadb`). Derleme ve test koşturulmadı. "doğrulandı" kodu okuyarak izlendi, çalıştırılarak değil.

## Bulgular (önem sırasıyla)

1. **Önemli — erken kapanan dosya `fizik` güvencesi yazabiliyor (W5-K41 delinir). Doğrulandı.**
   - **Kusur:** Tavan dosyaya yalnız kapanış taslağı üzerinden gidiyor (`sim/sunucu/src/kayit_baglanti.cpp:277-281`, `:283-292`). İlk taslak ancak 1 s (duvar) sonra yazılıyor: `sim/sunucu/src/sunucu.cpp:4309` `kayit_denetim_duvar_ = gercek();`, açılışta taslak çağrısı yok. Yazıcı erken kapanışta taslağı okuyor: `sim/kayit/src/yazici.cpp:731` (`hata_kapanisi`: `disk_dolu`, `yazma_hatasi`) ve `:860` (`sinira_ulasti`). İlk taslaktan önce taslak varsayılan değerde, `guvence_tavani` boş. `guvence_belirle` bu durumda kademe ve kipin verdiği `fizik` düzeyini, `erken_son_adim` önekiyle yazıyor. Okuyucu (`sim/kayit/src/okuyucu.cpp:1170`) dosyada `kayit/eksik` dışında bir eksik göremediği için `fizik`'i koruyor.
   - **Senaryo:** release SIL'de 90 s'lik koşu 1 s'den kısa sürüyor; BENIOKU da "1 Hz tur hiç oluşmaz" diyor. Disk koşu sırasında dolarsa ve kuyruk sığarsa dosya `bitis_durumu=disk_dolu`, `tekrar_guvencesi=fizik`, `guvence_on_eki_son_adim=N` ile kapanır. Oysa `fizik/adim_sonucu` ve `a3/komut` hiç üretilmedi.
   - **Aynı delik, ikinci yol:** `kayit_kapat` içinde son sayaç yayını ile `kapat` aynı `try` içinde (`sunucu.cpp:4335-4337`). Yayın istisna atarsa ya da `kos()` iç `catch(...)` yoluna düşerse dosyayı `~Yazici` taslakla kapatır. Taslakta tavan yoksa öneksiz `fizik` yazılır.
   - **Düzeltme:** `KayitBaglanti::ac`'ta, `Yazici::ac`'ın hemen ardından ve ilk üretici çağrısından önce tavanlı bir taslak yazılmalı: `taslak_guncelle(KapanisAlanlari{bitis=hata})` ya da `kayit_baslat` sonunda `kayit_taslak_hemen()`. Daha sağlamı tavanı `kayit::Ayar`'a koymak; yazıcı böylece her kapanış yolunda kullanır. Son sayaç yayını ayrı `try` içine alınmalı. Bir test erken kapanışı zorlamalı (ör. sınamaya açık `ilk_sira` ile `sinira_ulasti`) ve `izleme` beklemeli.
   - Tavan mantığının kendisi doğru, yalnız aşağı çekiyor: `sim/kayit/src/ust_veri.cpp:193-199`, `yazici.cpp:516-517`.

2. **Önemli — ön denetimde kalan çalıştırma `durduruldu` diye kapanıyor. Doğrulandı.**
   - **Kusur:** `sunucu.cpp:4328` `calistirma_sonu == "yuklendi"` gördüğünde `durduruldu` yazıyor. Ön denetimde kalma yolu da `calistirma_sonu = "yuklendi"` yapıyor (`sunucu.cpp:4699-4701`, `baslat` `on_denetim_kaldi` ile reddediliyor). A6-K45 bu durum için `on_denetim_kaldi` istiyor. `durduruldu` yalnız hazırlıkta `durdur` içindir (`a3_durdur_` dalı).
   - **Düzeltme:** Dize yerine bayrak kullanılmalı: `a3_durdur_` ise `durduruldu`, ön denetimde kaldıysa `on_denetim_kaldi`.
   - A3 `durdur` → `durduruldu` eşlemesi kodda doğru (`sunucu.cpp:4794` ile `:4328`). Ama ne bu ne de `on_denetim_kaldi` sınanıyor; ikisine birer test eklenmeli.

3. **Önemli — `on_denetim_raporu.json` yapılmamış denetimi "geçti" gösteriyor. Doğrulandı.**
   - **Kusur:** `kayit_baglanti.cpp:203-211` `kayit_disk_alani` maddesine `durum: gecti`, rapora `sonuc: gecti` yazıyor. A3 §3.8 tablosunda bu madde "kayıt dizininde boş alan ≥ 1 GiB" demek. Kod boş alanı hiç ölçmüyor; `olculen`, `sinir`, `birim` yok. Tek maddeli raporun `sonuc: gecti` demesi okuyana ön denetimin tamamen geçtiği izlenimini veriyor.
   - **Düzeltme:**
     - Boş alan `std::filesystem::space(dizin).available` ile ölçülmeli: `olculen`, `sinir = 1 GiB`, `birim` doldurulmalı. Sınırın altındaysa madde `kaldi` olmalı ve çalıştırma başlamamalı.
     - Yapılmayan maddeler (`surum_uyumu`, `kontrol_hizi`, `a1_bant_payi`…) şemanın izin verdiği `atlandi` ile ve nedeni yazılarak eklenmeli.
     - `senaryo_gecerli` dürüstçe `gecti` olarak yazılabilir; A5 doğrulaması gerçekten yapıldı.

4. **Önemli, şimdilik gizli — `bilinmiyor` sürüm anahtarları güvenceyi sınırlamıyor. Doğrulandı (gizli); bugün yalnız bulgu 1 yolunda etkili.**
   - **Kusur:** `kayit_baglanti.cpp:239-244` `sim_surumu`, `derleme_bayraklari`, `rng_surumu`, `dalga_listesi_ozeti` alanlarına `bilinmiyor` yazıyor. `bagimliliklar` yalnız `mcap`, `zstd` içeriyor (Eigen yok). Tablo 4.6 (b) `fizik` için "aynı `sim_surumu`… `bagimliliklar`, `rng_surumu`" istiyor; iki dosyadaki `bilinmiyor`=`bilinmiyor` eşitlik denetiminden geçer. `tavan_uygula` (`:329-341`) yalnız eksik kanala bakıyor. Sonraki dilim `fizik/adim_sonucu` ve `a3/komut`'u ürettiğinde tavan kalkar ve dosya bilinmeyen sürümle `fizik` iddia eder. Bulgu 1 yolunda bu bugün de oluyor. W5-K39'un "zorunlu üst veri eksikse `izleme`" kuralını dolanıyor.
   - **Düzeltme:** `tavan_uygula`, bu anahtarlardan biri `bilinmiyor` olduğunda da `izleme` tavanı koymalı (neden örn. `ust_veri_bilinmiyor:sim_surumu+…`).

5. **Önemli — A3 kapalıyken `olay` ve `ariza/etkin` hiç yazılmıyor. Doğrulandı.**
   - **Kusur:** Dünyada bu veri var, ama:
     - arıza tabloları yalnız `a3_baslat` içinde kuruluyor (`sunucu.cpp:2646-2654`);
     - `a3_ariza_izle` yalnız `a3_yayinla`'dan çağrılıyor;
     - `a3_olay` `yay_ == nullptr` ise dönüyor (`sunucu.cpp:2423-2425`). Bu yüzden `kayit_yayinla`'nın (`:4238`) hesapladığı geçerlilik olayları (tür 2, 3) düşüyor.
   - Kayıtsız kalan kanallar A6-K18'de zorunlu ("olaylar; … arıza durumu değişimleri"). Görev kabulü "verisi olanların hiçbiri atlanmaz" diyor. BENIOKU bunu bilinçli bir okuma olarak yazmış; mimar kararı gerekiyor.
   - **Düzeltme:** Arıza tablosu kayıt açıkken de kurulmalı. `a3_olay` kayda `yay_`'dan bağımsız yazmalı; `adim_no` `a3d_`'den değil sınırdan alınmalı.

6. **Önemli (W5-K43 ihlali) — HIL testinde hıza bağlı eşik. Doğrulandı.**
   - `tests/butunleme/test_kayit.cpp:753` `tf_sayisi >= 2300u` doğrudan yazılmış. Öbür testlerdeki aynı eşik `ida::test::zamanlama` ile sarılı (ör. `test_butunleme.cpp:666`). Yüklü CI'da kayıtla ilgisiz bir kırmızıya yol açar.
   - **Düzeltme:** Eşik `zamanlama` içine alınmalı ya da kaldırılmalı.

7. **Küçük — A3 yüzü kaydı eksik gösteriyor. Doğrulandı.**
   - Kayıt açıkken `calistirma/durum.tekrar_guvencesi` hâlâ 0, yani `yok` (`sunucu.cpp:3479`; yorum da bayatladı). 1 (`izleme`) olmalı.
   - A3 olay tür 15 `kayit_durumu` "açıldı" hiç üretilmiyor (yalnız durum değişiminde: `kayit_denetim`).
   - A3 açıkken aynı adlı dosya varsa `baslat` yanıtsız kalıyor; ön denetim `kayit_disk_alani kaldi` yazılmıyor (A6-K01). Bu pratikte yalnız `kayit_run_id` ile oluşur.

8. **Küçük — testler iddia ettiklerinden dar. Doğrulandı.**
   - `kayit_icerik` "poz bit düzeyinde aynı" diyor, ama yalnız `p_n[2]` ile `satir.derinlik`'i karşılaştırıyor (`test_kayit.cpp:453`). Yönelim sınanmıyor. Yapılan karşılaştırma gerçek: 2 000 satırın hepsi bit düzeyinde eşleşmeli, sayım da doğru.
   - A3 çıktısının kayıt kapalıyken öncekiyle aynı kaldığına dair otomatik test yok. Kod okumasında sıra, koşul ve içerik değişmemiş görünüyor (aşağıda).

9. **Küçük — ölçüm ve belge doğruluğu. Doğrulandı.**
   - A3 açıkken kayıt çağrıları `a3_yayinla` içindeki `A3Hizmet` ölçümüne giriyor; `a3_sinir_en_kotu_ns` kayıt süresini de kapsıyor.
   - `sim/sunucu/BENIOKU.md` "Yerleşim" maddesi yalnız taslak kilidini istisna sayıyor. Oysa 1 Hz yolu dünya iş parçacığında bellek ayırıyor: `kayit_alanlari`, `kayit_sayac_listesi`, `eksik_kanallar`.

10. **Küçük — elle yazılmış sabitler. Doğrulandı.** `kayit_baglanti.cpp:223-247` `fizik_adimi_ns "1250000"`, `kontrol_hizi_hz "200"`, `l_act_ns`, `a1_proto_surumu "1.0"` değerlerini elle yazıyor. `kH` ve köprü sabitlerinden türetilmeli.

11. **Küçük — özet satırında kapanmayan parantez. Doğrulandı.** `sim/sunucu/src/main.cpp:483` çıktısı "güvence izleme (kanal_uretilmiyor:…; üretilen" biçiminde; parantez kapanmıyor.

12. **Küçük, çıkarım — öğe türü eşlemesi.** `sunucu.cpp:4263` `gcs` ve `guided` öğelerini `tur=4` (`diger`) yapıyor. A6'da `3=gorev` var; mimarın onayı gerekir.

13. **Küçük — test çıktısı birikiyor.** `butunleme.ida_sim_kayit_ozet` sabit `kayit_ozet` derleme dizinine yazıyor; her koşuda bir `.mcap` birikir (`tests/butunleme/CMakeLists.txt`).

## Bakılıp temiz bulunanlar

- **Emniyet zinciri:** Kayıt çağrıları emniyet uygulamasından ve parti yazımından sonra geliyor; hiçbir emniyet dalına koşul eklenmemiş. A3 kapalıyken yeni çağrılan `a3_gecerlilik` yalnız A3 durum alanlarını değiştiriyor. Dilim acil durdurma mantığına dokunmuyor.
- **A3 yayınının ayrıştırılması:** `fizik_yayinla`, `kart_yayinla`, `cekirdek_yayinla` ayrıldıktan sonra kayıt kapalıyken sıra, koşul ve alanlar aynı.
- **İş parçacıkları:** `a3_sonuc` ve `kayit_taslak_hemen` dünya iş parçacığından çağrılıyor; A3 HTTP iş parçacığı yalnız kuyruğa yazıyor. Tek üretici kuralı korunuyor, yarış yok.
- **Kapanış:** Tek kapanış yolu olağan, sinyal ve istisna dallarında çalışıyor ve bayrakla bir kez çağrılıyor. Açılış yarıda kalırsa çalıştırma başlamıyor. Var olan dosya ezilmiyor ve bu sınanıyor.
- **Tavan:** `guvence_belirle` ve yazıcıdaki tavan yalnız aşağı çekiyor.
- **Testler:**
  - `kayit_belirlenim` gerçekten CSV'yi ve iki yönlü A1 akışını karşılaştırıyor.
  - POSIX korumaları kaynakla CMake arasında tutarlı.
  - `pgrep -f` döngüsü yok. `kill9` testindeki 20 ms yoklama olaya bağlı bekleyiş; ölçüt değil.
- **Taşınabilirlik ve belgeler:**
  - MSVC'de derlenmeyecek kalıp yok.
  - Kişisel makine yolu yok.
  - BENIOKU'daki rakamlar ölçüm ya da tahmin diye işaretli.

## Doğrulanmayanlar

- Derleme ve testler koşturulmadı.
- Bulgu 1'in senaryosu çalıştırılarak denenmedi.
- Windows derlemesi denenmedi.
