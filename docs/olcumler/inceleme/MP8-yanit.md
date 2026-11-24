# MP8a — sol 1. inceleme yanıtı

| | |
|---|---|
| İnceleme | `docs/olcumler/inceleme/MP8-sol.md` (8 kritik, 1 önemli) |
| Kararlar | `docs/gorevler/W0-MP8a.md`, "Düzeltme turu (mimar, 2026-11-19; 1. tur)" |
| Dal | `olcum/mp8-windows` |
| Uygulayan | `ida-uygulayici` (Opus), 2026-11-19 |

Satır numaraları düzeltmeden sonraki dosyalara göredir ve yaklaşıktır. İşlev adları kesin başvurudur.

| # | Önem | Durum | Yer | Ne yapıldı / gerekçe |
|---|---|---|---|---|
| 1 | kritik | **Giderildi** | `calistir.ps1`: `Sureci-Kapat`, `Godot-Baslat`, ana `finally` bloğu, "Godot ikilisi" adımı | Yol eşitliğiyle süreç bulan `Godot-Surecleri` ve `Artiklari-Kapat` silindi. Kapatma yalnız `Start-Process -PassThru` ile alınıp `$script:AcikSurecler` listesine eklenen süreç nesneleri üzerinden yapılır. `Sureci-Kapat` `-KuruCalistir`'da hiçbir şey yapmaz; ana `finally` bloğu da kuru çalıştırmada süreç listesine bakmaz. Önceki Godot dizini silinemezse betik süreci kapatmaz; adım açık bir iletiyle `HATA` verir. BENIOKU kullanıcıya önceki pencereyi kendisinin kapatmasını söyler |
| 2 | kritik | **Giderildi** | `analiz_win.py`: `KOSUL_DURUM`, `KOSUL_OKUMA`, `kosul_denetle`, `h2_hukmu`, `olcutler` (H4) | Hükümden önce `bilgi.json` ve `ozet.json` denetlenir. H2 için: yol b, `sure_s` 120, 640 × 480, 30 Hz, `geri=esz`, doğru durum, 3600 istek, ölçülen süre 120 ± 1 s. H4 için: `cift=1`, görünür, 60 s, 1800 istek, dolu p50/p99, üretilen > 0. Uymayan kayıt `kosul_disi` olur. Mac'teki 20 s'lik koşu gerçekten `kosul_disi` işaretlendi (`MP8-windows.md` 4. başlık, madde 3) |
| 3 | kritik | **Giderildi** | `analiz_win.py`: `pencere_win`, `ornek_degerlendir`; `calistir.ps1`: `Pencere-Gozlem`, `Kos-Izle` | Örnekler ölçüm aralığıyla eşleştirilir. Eşleştirme `hazir` dosyasındaki Godot saati ve `ozet.json` başlangıç ve bitişiyle yapılır; betik gözlem zamanını `hazir` dosyasının görüldüğü andan sayar, tolerans 1 s. Aralık içindeki hata satırı, bozuk satır ya da doğrulanamayan örnek hükmü "doğrulanmadı" yapar. Hata satırları artık `gecen_s` taşır. Aralığın başı ve sonu dahil en büyük boşluk 15 s'yi aşarsa ya da hiç örnek yoksa kapsam yetersizdir, hüküm "doğrulanmadı" olur. Tek bir "tutulmadı" örneği hükmü "tutulmadı" yapar. Sentetik senaryolarla sınandı (`MP8-windows.md` 4. başlık, madde 4) |
| 4 | kritik | **Giderildi (Windows'ta sınanmadı)** | `calistir.ps1` C# yardımcısı: `Gozlem`, `GozlemIc`, `DisCerceve`, `Ustunde`; `analiz_win.py`: `ornek_degerlendir` | Gözlem sırasında iş parçacığının DPI bağlamı açıkça `PER_MONITOR_AWARE_V2` yapılır ve sonra geri alınır. İki pencerenin görünen dış çerçevesi `DWMWA_EXTENDED_FRAME_BOUNDS` ile okunur; ölçüm çerçevesinin örtününkinin içinde olduğu ve örtünün Z sırasında üstte olduğu denetlenir (`GW_HWNDPREV` yürüyüşü). Bağlam ayarlanamazsa ya da çerçeve okunamazsa örnek "doğrulanamadı" sayılır ve H2 örtülü "geçti" olamaz. 5 nokta denetimi ek gözlem olarak kaldı. Kalan risk (`GetWindow` döngüsü, DPI varsayımı): `MP8-windows.md` 8. başlık, madde 20 |
| 5 | kritik | **Giderildi** | `calistir.ps1`: `Kos-Godot`, `Kos-Izle`, deneme ve kampanya adımları | Ölçümün yazılması (`ozet_var`) ve sürecin temiz bitmesi (`surec_temiz`) ayrı denetlenir. Temiz bitme şunları gerektirir: hata yok, süre aşımı yok, kapatılmak zorunda kalınmadı, çıkış kodu biliniyor ve 0, örtü erken kapanmadı. Sonuç `kos.log` içinde `surec_temiz true\|false <neden>` satırına, `sonuc.json` içinde `surec_temiz` ve `duzenek` alanlarına yazılır. Ölçüm süreci kendiliğinden kapanabilsin diye 10 s ek beklenir. Deneme yalnız ölçüm yazıldıysa, süreç temiz bittiyse ve kare üretildiyse seçilir. Kampanyada süreç temiz bitmezse adım `HATA` olur. Çözümleyici bu durumu `duzenek_calismadi` diye ölçüt sonucundan ayrı yazar |
| 6 | kritik | **Giderildi** | `analiz_win.py`: `referans_gecerli`, `kos_coz_guvenli`, `main` | Görünür referans önceden denetlenir: dosyalar var, koşul uygun, süreç temiz bitmiş, kare üretilmiş. Geçersizse `kos_coz`'a `None` verilir ve H3 "değerlendirilemedi (referans geçersiz: neden)" olur. Her koşu ayrı `try` içinde çözümlenir; `mp8_ozet.json` her durumda yazılır. `referans_bozuk` senaryosunda çıkış 0, diğer koşular çözümlendi. `tools/olcum/mp6/` değişmedi |
| 7 | kritik | **Giderildi** | `analiz_win.py`: `donanim`; `calistir.ps1`: "Donanim kaydi" adımı (`powercfg`) | Zorunlu alanlar anlamca denetlenir. Ekran kartında iki çözünürlük boyutu ve gerçek yenileme hızı (2–1000 Hz) gerekir; 0 ve 4294967295 geçersiz sayılır. Güç durumu yalnız `Online` ya da `Offline` olabilir. Güç planı için `powercfg` çıkış kodu (`$LASTEXITCODE`) 0 olmalı ve GUID bulunmalı. Betik çıkış kodunu `guc_plani_cikis_kodu` alanına yazar; hata metnini veri saymaz. `h1_kotu` senaryosunda dört alan ayrı ayrı listelendi |
| 8 | kritik | **Kısmen** | `MP8-windows.md` 4. başlık ve 6.1 | **W2:** gerçek argüman sırası, ısınma 6 s ve 20 s ölçümle yinelendi: 600/600 kare, ölçülen süre 20,007 s, çıkış 0, MD5 MP6a ile 600/600 eşit. Komut satırı betiğin kurucusundan elle aktarıldı; bu yüzden W2 "kısmi doğrulama" olarak yazıldı. **W1:** "tamamlanmadı". Ortam `pwsh` çağrısını reddediyor ve mimar kararıyla dolaylı yol kullanılmadı. 1. turdaki dolaylı ayrıştırma sonucu kanıt sayılmaz. Yerine yapılanlar: gömülü C#'ın `LangVersion=5` ile derlenmesi (öz denetimli) ve kaba denge denetimi. İkisi de PowerShell ayrıştırmasının yerine geçmez. Mimarın ya da kullanıcının koşturacağı komutlar 6.1'de |
| 9 | önemli | **Giderildi** | `calistir.ps1` C# yardımcısı (`Sahip`, `Sinif`, `GozlemIc`); `BENIOKU.md` "Çıktı nerede" | Başka uygulamaların pencere başlıkları kaydedilmez. Ön plandaki pencere için yalnız sahiplik sınıfı (`olcum`, `ortu`, `diger`, `yok`) ve pencere sınıf adı yazılır; 5 noktada yalnız sahiplik sınıfı yazılır. Başlık yalnız bu çalıştırmanın Godot pencereleri için yazılır. `Win32_Desktop` kaydından kullanıcı ve bilgisayar adı çıkarıldı. BENIOKU çıktının içeriğini kalem kalem anlatır: kullanıcı adı yalnız yollarda geçer |

## Mimar kararından ek iş

- **Küresel süre sınırı 15 dk** (`calistir.ps1`: `$KureselSinirDk`, `Adim -Sinirli`, `Sure-Yeter`, `Siniri-Ertele`).
  - Betik başlangıcından sayılır. Kullanıcı sorusunda beklenen süre ve Godot indirme süresi sınıra eklenir.
  - Gerekçe: indirme ağ hızına bağlıdır. Yavaş bağlantıda ölçümün hiç başlamamasını önlemek için. Bu bir yorumdur; mimar sayacın indirme dahil olmasını isterse tek satırlık değişikliktir.
  - Sınır aşılırsa kalan adımlar `ATLANDI` olur; Özet ve Zip yine yapılır.
  - Ayrıca her koşu, sınıra kadar bitecek süre kalmamışsa başlatılmaz.
- **W1 komutları** `MP8-windows.md` 6.1'de ve `BENIOKU.md`'de.

## Değişmeyenler

- `tools/olcum/mp6/**`: değişmedi. `git status --ignored` temiz; `analiz_win.py` bytecode yazmıyor.
- Godot komut satırları, kampanya planı, döküm sıklığı ve özet sabitleri değişmedi.

## İkinci tur (sol-2, 2026-11-19)

İnceleme: `docs/olcumler/inceleme/MP8-sol-2.md`. Yalnız iki yeni bulgu ele alındı.

| # | Önem | Durum | Yer | Ne yapıldı / gerekçe |
|---|---|---|---|---|
| Y1 | kritik | **Giderildi** | `analiz_win.py`: `referans_gecerli`, `olcutler` (H3) | Görünür referans H3 için yalnız üç koşul birlikte sağlanırsa geçerlidir: (a) dosya ve koşul denetimi geçti; (b) görünür H2 `gecti`, yani pencere durumu doğrulandı; (c) süreç bitişi açıkça `True`. "Doğrulanmadı", "geçmedi" ya da eksik veya bilinmeyen bitiş referansı geçersiz kılar; H3 "değerlendirilemedi (görünür referans geçersiz: neden)" olur. Kurgu senaryolarına `ref_dogrulanmadi` (görünür H2 doğrulanmadı, MAD 0) ve `ref_bitis_eksik` (görünür `kos.log`'da `surec_temiz` yok, MAD 0) eklendi; ikisinde de H3 simge ve örtülü "değerlendirilemedi" çıktı. Önceki `hata_satiri` senaryosunda H3 "geçti" çıkıyordu (bulgunun kendisi); artık "değerlendirilemedi". Diğer sekiz senaryonun sonucu değişmedi |
| Y2 | önemli | **Giderildi (Windows'ta sınanmadı)** | `calistir.ps1`: `Kos-Godot` (hazır bekleme, `finally`, temiz bitiş nedeni), `Kos-Izle` (izleme döngüsü) | İki döngü de her turda `Sure-Yeter 0` ile kalan küresel süreyi denetler. Süre dolunca yeni `sure_siniri` alanı işaretlenir, `kos.log`'a `HATA: kuresel sure siniri doldu…` yazılır ve döngü kesilir. Çalışan ölçüm süreci 10 s ek bekleme yapılmadan, betiğin kendi süreç nesnesiyle kapatılır. Temiz bitiş nedeni `kuresel_sure_siniri` olur; `surec_temiz false` yazılır ve bu temiz bitiş sayılmaz. Kalan koşular `Adim -Sinirli` ile `ATLANDI` yazılır. Betik yine ayrıştırıcıdan geçirilemedi; değişen bloklar satır satır okundu, kaba denge denetimi 0 hata verdi, dosya ASCII |
