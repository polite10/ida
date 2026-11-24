# Windows ölçüm paketi (W0-MP8a)

Bu paket Windows bilgisayarınızda iki şey yapar:

1. Donanımı kaydeder: işlemci, bellek, ekran kartı ve sürücüsü, ekran, Windows sürümü, güç durumu.
2. Godot 4.7.2 ile kısa bir kamera ölçümü yapar. Pencere görünürken, simge durumundayken ve başka bir pencerenin altındayken kare üretilip üretilmediğini ölçer.

Yönetici yetkisi istemez ve hiçbir şey kurmaz. Sistem ayarlarına (güç planı, ekran koruyucu, kayıt defteri, sürücüler) dokunmaz. İnternetten yalnız Godot'yu indirir.

## Başlamadan önce

- Deponun tamamı bilgisayarda olmalı. Paket `tools/olcum/mp6/proje` klasörünü kullanır; yalnız `windows` klasörünü kopyalamak yetmez.
- Bilgisayar **prize takılı** olsun.
- Açık programları kapatın; özellikle oyunlar, video ve tarayıcıda oynayan medya.
- İnternet bağlantısı gerekir. Yalnız ilk çalıştırmada yaklaşık 86 MB indirilir.

## Çalıştırma (tek komut)

1. Depo klasörünü Dosya Gezgini'nde açın.
2. Adres çubuğuna `powershell` yazıp Enter'a basın. Mavi bir PowerShell penceresi açılır.
3. Şu komutu yapıştırıp Enter'a basın:

```
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\olcum\windows\calistir.ps1
```

`-ExecutionPolicy Bypass` yalnız bu komut için geçerlidir ve sistem ayarını değiştirmez. "running scripts is disabled" (betik çalıştırma devre dışı) hatası alırsanız komutu bu biçimiyle yazdığınızdan emin olun. Hata sürerse ekran görüntüsünü mimara gönderin.

Başta iki soru gelebilir:

- **Prize takılı değil uyarısı:** şarj aletini takıp Enter'a basın.
- **Godot'nun özeti bilinmiyor sorusu:** normalde gelmez. Gelirse `H` yazın ve mimara haber verin.

## Ne kadar sürer, bu sırada ne yapılmalı

- Ölçüm yaklaşık **10 dakika** sürer (tahmin). İlk çalıştırmada indirme süresi buna eklenir.
- Betik en çok **15 dakika** çalışır. Sizin bir soruyu yanıtlamanız için beklediği süre ve indirme süresi bu sınıra eklenir. Sınır dolarsa kalan ölçümler "ATLANDI" diye yazılır, ama özet ve zip yine oluşturulur.
- **Bu sürede bilgisayara dokunmayın:** fare, klavye ve pencereler dahil. Ekranda küçük Godot pencereleri açılıp kapanır, bir tanesi simge durumuna küçülür, bir başkası onu örter. Bunlar ölçümün parçasıdır.
- Kapağı kapatmayın. Betik çalışırken bilgisayarın uykuya girmesini ve ekranın kapanmasını erteler; bunun için ayar değiştirmez. Ekran koruyucunuz 15 dakikadan kısa sürede açılacak biçimde ayarlıysa betik bir uyarı yazar. Ayarı değiştirmez; ekran koruyucu açılırsa ölçüm yine sürer ve bu durum kayda geçer.

## Çıktı nerede, mimara ne verilecek

Betik bitince en altta şuna benzer bir satır yazar:

```
Mimara verilecek dosya:
  C:\Users\<kullanıcı>\Desktop\IDA-MP8-20261120-101500.zip
```

**Bu `.zip` dosyasını mimara verin.** Aynı adlı klasör de masaüstündedir. Masaüstü bulunamazsa ikisi de betiğin yanına (`tools\olcum\windows\`) yazılır.

Klasörde şunlar bulunur:

| Dosya ya da klasör | İçerik |
|---|---|
| `betik.log` | Betiğin adım adım günlüğü |
| `donanim.txt`, `donanim.json` | Donanım kaydı |
| `deneme_N_<yöntem>\` | Çizim yöntemi denemeleri. Önce varsayılan (Forward+), açılmazsa `mobile`, o da olmazsa `opengl3` |
| `kampanya_<yöntem>\` | Ölçüm koşuları: `b_gorunur`, `b_simge`, `b_ortulu`, `okuma_640x480_esz` |
| `sonuc.json`, `sonuc.txt` | Adımların ve koşuların özeti |

Her koşu klasöründe şunlar vardır:

- Godot'nun günlükleri: `godot_log.txt`, `godot_stdout.txt`, `godot_stderr.txt`
- Kare ve durum kayıtları: `kare.csv`, `durum.csv`, `ozet.json`
- Pencere gözlemi: `pencere_win.jsonl`
- Süreç örnekleri: `surec_win.csv`
- Seyrek ham kare dökümleri: `kare_*.rgb`; toplamı 50 MB'ı geçmez

Klasörde şu bilgiler bulunur:

- **Donanım ve sistem:** işlemci, bellek modülleri, ekran kartı ve sürücüsü, ekranlar, Windows sürümü, bilgisayarın üreticisi ve modeli, güç durumu ve güç planı, ekran koruyucunun açık olup olmadığı ve süresi, koşu öncesi işlemci yükü ve boş bellek.
- **Kullanıcı adı:** yalnız dosya yollarının içinde geçer (ör. `C:\Users\<kullanıcı adı>\...`). Bu yollar betiğin ve Godot'nun günlüklerine yazılır.
- **Pencere bilgileri:** yalnız bu betiğin açtığı Godot pencerelerinin başlığı, konumu ve durumu.
- **Başka uygulamalar:** pencere başlıkları kaydedilmez. Öndeki pencere için yalnız iki şey yazılır: bu betiğin açtığı bir pencere olup olmadığı ve Windows pencere sınıf adı (ör. `Chrome_WidgetWin_1`). Sınıf adı belge ya da sayfa adı içermez.

Klasörü kendiniz silebilirsiniz.

## Bir şey ters giderse

- Betik hiçbir durumda sessizce "başarılı" demez. Sonunda her adımı `TAMAM`, `UYARI`, `ATLANDI`, `KISMI` ya da `HATA` olarak listeler. Bir adım başarısız olsa da sonrakiler denenir.
- **"Unable to initialize video driver" penceresi çıkarsa** "Tamam"a basın. Betik bir sonraki çizim yöntemine geçer.
- Godot penceresi takılı kalırsa betik süresi dolunca onu kendisi kapatır. Siz kapatmayın.
- Betik yalnız kendi açtığı Godot pencerelerini kapatır. Başka bir Godot penceresi açıksa (ör. önceki yarım kalmış bir çalıştırmadan) betiğe dokunmaz. Böyle bir pencere varsa başlamadan önce siz kapatın.
- Betiği yarıda kesmek için PowerShell penceresinde **Ctrl+C**'ye basın. Betik açtığı Godot pencerelerini kapatır.
- Özette bir koşu `duzenek=calismadi` görünebilir. Bu, ölçüm düzeneğinin o koşuda düzgün bitmediğini gösterir: Godot hata verdi, süresi aştı ya da kapatılmak zorunda kalındı. Bu bir ölçüm sonucu değildir.
- Ne olursa olsun, oluşan `.zip` dosyasını, yoksa klasörü mimara verin. Klasör de yoksa PowerShell penceresinin ekran görüntüsünü gönderin.

## Bilgisayarda kalanlar

- **Godot:** `%USERPROFILE%\.cache\ida\godot\4.7.2\` altına inen zip ve açılmış hali, yaklaşık 300 MB. `IDA_CACHE` ortam değişkeni tanımlıysa onun altındadır. Yeniden çalıştırmada tekrar indirilmez. İsterseniz silebilirsiniz.
- **Godot'nun gölgelendirici önbelleği:** `%APPDATA%\ida_mp6\`. Silinebilir.
- **Projenin içe aktarma önbelleği:** Godot ilk açılışta `tools\olcum\mp6\proje\.godot\` klasörünü oluşturur.

## Teknik ayrıntı (mimar için)

### Godot ikilisi

- Dosya: `Godot_v4.7.2-stable_win64.exe.zip` (standart sürüm, .NET değil).
- Adres: `https://github.com/godotengine/godot-builds/releases/download/4.7.2-stable/` (resmî sürüm sayfası `github.com/godotengine/godot-builds/releases/tag/4.7.2-stable`).
- Betik iki özeti de denetler; biri uyuşmazsa Godot'yu çalıştırmaz:
  - **SHA-512:** `83decd58fdf67b9d657958a1ae6bf1929c20785315a81effe245874cdc57acb709bf868e00778a96984338c1b29dafdb453c6847747694621c6ecf5da2259993`. Kaynak: aynı sürümün `SHA512-SUMS.txt` varlığı. Bu dosya 2026-11-19'da indirildi; MP6a'da indirilen kopyayla bayt bayt aynı. Kendi GitHub özeti `sha256:b8bdff67…a5c7c0`.
  - **SHA-256:** `731980f9608d61333e5baf54a2ef17210acc7a538446c0cb9969f002aca1e953`. Kaynak: GitHub sürüm API'sinin bu varlık için yayımladığı `digest` alanı (`gh api repos/godotengine/godot-builds/releases/tags/4.7.2-stable`, 2026-11-19).

### Komut satırları ve kampanya

- Komut satırları `calistir.ps1 -KuruCalistir` ile görülür. Bu kip hiçbir şey indirmez, Godot'yu başlatmaz ve klasör oluşturmaz.
- Kampanya yalnız yol (b) ile koşar (`force_draw`). Koşular sırasıyla şunlardır:
  - görünür (referans), simge durumu ve örtülü: her biri 120 s, 640 × 480, 30 Hz
  - görünür pencerede 60 s eşzamanlı geri okuma (`--geri=esz --cift=1`)
- Pencere durumunu Godot projesi kendisi kurar:
  - Simge durumu: proje içinden `window_set_mode(MINIMIZED)`.
  - Örtülü: ikinci bir Godot süreci (`--rol=ortu`) her zaman üstte açılır.
- Dış gözlem 10 s'de bir yapılır (`user32`/`dwmapi`). Gözlem sırasında iş parçacığının DPI bağlamı açıkça `PER_MONITOR_AWARE_V2` yapılır. Kaydedilenler:
  - ölçüm ve örtü pencerelerinin `IsIconic` ve `IsWindowVisible` durumu
  - görünen dış çerçeveleri (`DWMWA_EXTENDED_FRAME_BOUNDS`)
  - örtülü durumda dikdörtgen kapsaması ve Z sırası (`GetWindow(GW_HWNDPREV)`)
  - ön plandaki pencerenin sahiplik sınıfı ve sınıf adı
  - istemci alanındaki 5 noktada en üstteki pencerenin sahiplik sınıfı (`WindowFromPoint`; ek gözlem)
- Her koşu için iki ayrı kayıt tutulur:
  - ölçüm yazıldı mı (`ozet.json`)
  - süreç temiz bitti mi (`kos.log` içinde `surec_temiz`: hata yok, süre aşımı yok, kapatılmadı, çıkış kodu 0)

### Çözümleme

```
python3 -I tools/olcum/windows/analiz_win.py <açılmış IDA-MP8-... klasörü>
```

`analiz_win.py`, `tools/olcum/mp6/analiz.py` dosyasını değiştirmeden kullanır. Çıktı klasöre `mp8_ozet.json` olarak yazılır ve ekrana Markdown tablo basılır.

### Test parametreleri

Bunlar yalnız deneme ve yineleme içindir; kullanıcı çalıştırmasında gerekmez:

- `-GodotExe`: hazır bir Godot ikilisi kullanır; indirme ve özet denetimi atlanır.
- `-Cizim varsayilan|mobile|opengl3`: çizim yöntemi denemesini atlar.
- `-Sure`, `-OkumaSure`, `-Isinma`, `-Durumlar`: süreleri ve koşulacak durumları değiştirir.
- `-CiktiKok`: çıktı klasörünün yeri.
- `-Sorma`: kullanıcıya soru sormaz.

### Mac'te mimar ya da kullanıcı tarafından yapılacak denetim (W1)

Agent ortamında `pwsh` çalıştırılamadı. Depo kökünden şu komutlar koşturulmalı (PowerShell 7 gerekir):

```
pwsh -NoProfile -File tools/olcum/windows/mac_denetim.ps1 tools/olcum/windows/calistir.ps1
pwsh -NoProfile -File tools/olcum/windows/calistir.ps1 -KuruCalistir
```

İsteğe bağlı olarak PSScriptAnalyzer ile 5.1 uyumluluk denetimi de yapılabilir. Modül önce geçici bir dizine indirilir (`Save-Module`), kurulmaz:

```
pwsh -NoProfile -Command "Save-Module PSScriptAnalyzer -Path /tmp/psmodul"
pwsh -NoProfile -File tools/olcum/windows/mac_denetim.ps1 tools/olcum/windows/calistir.ps1 /tmp/psmodul
```

Beklenen sonuçlar:

- `mac_denetim.ps1` sonunda `SONUC: sorun 0` yazar.
- `-KuruCalistir` hiçbir şey indirmez, Godot'yu başlatmaz, klasör oluşturmaz ve hiçbir sürece dokunmaz. Bütün adımları `KURU:` satırlarıyla listeler. Mac'te Windows'a özgü adımlar `ATLANDI` ya da `KURU` olarak geçer.
