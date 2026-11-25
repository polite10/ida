# Bağımsız inceleme: W5-kayit dilim 1 (`sim/kayit`, PR #52, uç `b8ffff8`)

Tarih: 2026-11-25. İnceleyen: `ida-inceleyici` (Opus; taze bağlam; sol sınırı dolu olduğu için sol yerine, K25/K27; sol inceleme borcu listesindedir). Dilim emniyet dışıdır (K23): tek tam inceleme, tek düzeltme turu. İlk iki ÖNEMLİ bulgu inceleyicinin deney programıyla yeniden üretildi.

## Hüküm

"Düzeltme turundan sonra birleştirilebilir." KRİTİK bulgu yok; üç ÖNEMLİ, altı KÜÇÜK. Üç ÖNEMLİ bulgunun ortak sonucu: dosya kendisi hakkında gerçekte olduğundan yüksek bir tekrar güvencesi (`fizik`) yazıyor ya da okutuyordu.

## Bulgular ve sonuçları

Düzeltmeler aynı PR dalında tek turda yapıldı (yazan: `ida-kritik-uygulayici`). "Test" sütunu `sim.kayit.` önekiyle CTest adıdır.

| # | Önem | Yer (uç `b8ffff8`) | Sorun | Sonuç | Test |
|---|---|---|---|---|---|
| 1 | ÖNEMLİ | `sim/kayit/src/yazici.cpp:671-679`, `:700-703`, `:532-606`, `:470-473` | Erken kapanışta (sıra sınırı, yazma hatası) öteki üreticilerin halkalarındaki iletiler `kapali_iken_atilan` sayılıp atılıyor, `kayit/eksik` oluşmuyor; dosya `kayit_eksik=false`, `fizik`, önek 95 yazıyor (A6-K33/K12) | Düzeltildi: kapanış kaydından önce bütün halkalar boşaltılır; yazılamayan her ileti kanal başına `kayit/eksik`'e çevrilir; önek kalan iletilerin en küçük adımını kapsamaz; sıra yedeği büyütüldü (2·üretici·kanal·2 + üretici·kanal + 16). Kapanıştan sonra halkaya giren ileti sayaçta kalır | `erken_kapanis_cok_uretici`, `sira_siniri`, `hata_kapanisi` |
| 2 | ÖNEMLİ | `yazici.cpp:621-631`, `:593` | `kapat(alanlar)` sırasında yazma hatası çıkarsa kapanış kaydı eski taslağı kullanıyor: `canli_mudahale_adimi` kayboluyor, güvence `fizik` (A6-K53'e göre `yok`) | Düzeltildi: `kapat` alanlarını girişte taslağa yazar; her kapanış yolu onları kullanır | `kapat_sirasinda_hata` |
| 3 | ÖNEMLİ | `sim/kayit/src/okuyucu.cpp:1160-1197`; `mcap_yazim.cpp:69-76`, `:188` | Tek baytlık bozulma zorunlu bir eki ya da üst veri değerini sessizce değiştiriyor; dosya `saglam` ve `fizik` okunuyor | Düzeltildi: W5-K39 (aşağıda) | `bozulma_ek_ust_veri`, `guvence_kisitlari`, `rastgele_bozulma`, `hata_kapanisi` |
| 4 | KÜÇÜK | `yazici.cpp:655-657` | Güncelleme kaydından sonra boşaltma sonucu denetlenmeden `saglam_boyut` ilerliyor; kapanış kuyruğu yazılamıyor | Düzeltildi | `guncelleme_teslim_hatasi` |
| 5 | KÜÇÜK | `yazici.cpp:580-582` | Sıra numarası tükenirse `kayit/eksik` kaydı sayılmadan atlanıyor | Düzeltildi: `YaziciSayaclari::bildirilemeyen_dusen`; kapanış kaydı o kaybı yine hesaba katar. Yedeğin en ağır durumu karşıladığı testle sabitlendi | `sira_yedegi` |
| 6 | KÜÇÜK | `okuyucu.cpp:1056-1079` | Sonu kapalı dosyada bozuk kayıt uzunluğu taramayı Footer'dan önce durdurursa dosya "kapanmamış" sayılıyor | Düzeltildi: `bozuk` (kapanmış) | `bozulma_parca` |
| 7 | KÜÇÜK | `sim/kayit/src/ust_veri.cpp:137-143` | Beklenen düzey kipi yalnız `girdi` için gözetiyor; fiziğin koşmadığı kipler `fizik` alabiliyor | Düzeltildi: `beklenen_duzey()` (eşleme aşağıda; mimar onayı bekler) | `guvence`, `guvence_kisitlari` |
| 8 | KÜÇÜK | `okuyucu.cpp:751-753` | `kayit/eksik` kanalı okunamıyorsa (A6-K49) eksikler bilinmeden güvence veriliyor | Düzeltildi: `izleme` | `guvence_kisitlari` |
| 9 | KÜÇÜK | `sim/kayit/test/mcap_kitaplik_okuma.cpp` | Testteki ikinci kitaplık kopyası makroyla başka ad alanına alınıyor (kırılgan) | Bırakıldı; gerekçe dosyada: `reader.inl`, `types.inl`'deki `constexpr OpCodeString`'e bağlı (yalnız okuyucu derlenemiyor); tek birimde tam kitaplık testi ürünün derlenmiş sarmalayıcısından koparır | — |

## W5-K39 — veri bölümü CRC'si ve okuyucunun güvence kısıtları (mimar kararı)

A6'nın küçük sürüm notu olarak yazılacak; §3.2'deki "Data End kitaplık varsayılanı" cümlesinden bilinçli sapmadır.

- (a) Yazıcı veri bölümü CRC'sini her kapanış yolunda yazar (`enableDataCRC`; hata kapanışının elle yazılan kuyruğu son sağlam noktadaki CRC ara durumundan sürdürülür). `DataEnd{0}` yazılmaz. Kapsam, kitaplığın davranışıdır (`foxglove/mcap` 2.1.3, `writer.inl`): dosyanın ilk baytından (baştaki sihirli baytlar dahil) Data End kaydının başına kadar.
- (b) Okuyucu `data_section_crc`'yi doğrular; tutmuyorsa dosya `bozuk`. Değer 0 ise (yazılmamış) dosya `izleme`'den yüksek güvence veremez ve neden `DosyaOzeti::guvence_kisitlari`'nda bildirilir.
- (c) §3.6'daki zorunlu eklerden biri yoksa ya da açılış üst verisinin zorunlu anahtarı eksikse güvence `izleme`'dir.
- (d) Okuyucu öneki dosyanın kendi içeriğiyle sınırlar: düzeyin kanallarında görülen ilk `kayit/eksik` adımı; dosyada yazılmış son fizik adımı (`fizik/durum` ve `fizik/adim_sonucu` iletilerindeki en büyük `adim_no`); erken kapanmış kayıt öneksiz güvence veremez. Okuyucu güvenceyi hiçbir zaman kapanış kaydındakinin üstüne çıkarmaz.

## A6'ya yazılacak sözleşme notları

1. A6-K03 "1 MiB = kitaplığın `DefaultChunkSize` değeri" yanlıştır: mcap 2.1.3'te `DefaultChunkSize` 768 KiB'dır. Hedef 1 MiB kalır; sayı yazıcıya açıkça verilir.
2. §3.2 Data End: veri bölümü CRC'si zorunludur (W5-K39); kapsamı yukarıdaki (a)'dadır.
3. A6-K31 "dosyayı özetiyle kapatmayı dener": hata yolunda dosya özet bölümü olmadan kapatılır (kesilen dosyaya kitaplığın özeti yazılamaz; MCAP'te özet isteğe bağlıdır). Sapma.
4. Tablo 4.6'ya göre kip × kademe için beklenen en yüksek düzey (bulgu 7; kısıtlayan okuma):

   | kip | `olaylar` | `telemetri` | `tam` |
   |---|---|---|---|
   | `sil` | izleme | fizik | girdi |
   | `hil_1x`, `hil_bariyer` | izleme | fizik | fizik |
   | `tekrar_fizik` | izleme | fizik | fizik |
   | `tekrar_girdi_sil` | izleme | izleme | izleme |
   | `tekrar_yeniden_kurma` | izleme | izleme | izleme |

5. (Düzeltme turunda görüldü) `yuk_kayit_eksik.neden` kapalı listesinde sıra sınırı için değer yoktur (0 `halka_tasmasi`, 1 `yazma_hatasi`, 2 `disk_dolu`). Sınır kapanışında halkalarda kalan iletiler `yazma_hatasi` ile bildirilir (A6-K10'daki "yazılamadı" kullanımıyla aynı); ayrı bir değer küçük sürüm adayıdır.
6. (Düzeltme turunda görüldü) Erken kapanışta ve hata kapanışında `kayit/eksik` aralığı (`ilk_adim`, `son_adim`, zamanlar) birden çok üreticinin iletilerini kapsayabildiği için "ilk/son düşen" değil en küçük/en büyük değerdir; önek `ilk_adim`'den kurulduğu için bu gereklidir.

## İnceleyicinin doğrulamadıkları

Tam test turu; Windows derlemesi ve testleri; zstd DLL varsayımı; `TerminateProcess` yolu; TSan; Windows'ta `steady_clock`'ın sistem çağrısı yapıp yapmadığı; performans rakamları. (PR'ın ilk CI koşusu, uç `b8ffff8`: "Windows x64" ve "Windows x64 (ASan)" işleri geçti; koşu `45580414819`.)
