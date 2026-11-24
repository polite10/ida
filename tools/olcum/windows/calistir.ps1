#Requires -Version 5.1
<#
.SYNOPSIS
  IDA W0-MP8a olcum paketi: Windows hedef bilgisayarinin donanim kaydi ve Godot kamera on turu.

.DESCRIPTION
  Kullanici calistirir (bkz. BENIOKU.md). Yonetici yetkisi istemez, hicbir sey kurmaz,
  sistem ayari degistirmez. Godot 4.7.2 resmi Windows ikilisini kullanici profili altindaki
  onbellege indirir (yalniz Godot), ozetini betikte sabit degerlerle karsilastirir.
  Godot projesi tools/olcum/mp6/proje yerinden cagrilir (kopyalanmaz).

  Adimlar: on denetim, cikti klasoru, donanim kaydi, guc ve ekran denetimi, Godot ikilisi,
  pencere gozlem yardimcisi (user32), uyku engeli, cizim yontemi denemesi
  (varsayilan -> mobile -> opengl3), kampanya (yol b: gorunur, simge, ortulu; ardindan
  gorunur pencerede esz. geri okuma), kare dokumu boyut denetimi, ozet, zip.

  Surecler: betik yalniz kendi baslattigi Godot sureclerini (Start-Process ile aldigi surec
  nesneleriyle) kapatir; ayni ikiliden calisan baska surecler sahiplik sayilmaz ve dokunulmaz.
  Kuru calistirma hicbir surece dokunmaz.
  Kuresel sure siniri 15 dk (kullanici beklemesi ve indirme suresi eklenir); asilirsa kalan
  adimlar ATLANDI yazilir, Ozet ve Zip yine yapilir.
  Her kosu icin iki ayri denetim kaydedilir: olcum yazildi mi (ozet.json) ve surec temiz bitti
  mi (hata yok, sure asimi yok, kapatilmadi, cikis kodu 0).

  Bu dosya bilerek yalniz ASCII karakter icerir (Windows PowerShell 5.1, BOM'suz dosyayi
  sistem kod sayfasiyla okur).

.PARAMETER KuruCalistir
  Hicbir sey indirmez, Godot'yu baslatmaz, klasor olusturmaz; adimlari, yollari ve
  Godot komut satirlarini yazar.

.PARAMETER CiktiKok
  Cikti klasorunun olusturulacagi dizin. Verilmezse masaustu; masaustu yoksa betigin yani.

.PARAMETER GodotExe
  (Test icin.) Hazir bir Godot ikilisi; verilirse indirme ve ozet denetimi ATLANIR ve
  bu gunluge yazilir.

.PARAMETER Cizim
  (Test ya da yineleme icin.) varsayilan | mobile | opengl3. Verilirse cizim yontemi
  denemesi atlanir ve kampanya bu yontemle kosar.

.PARAMETER Sure
  Durum kosusu suresi (s). Varsayilan 120 (gorev belgesi).

.PARAMETER OkumaSure
  Geri okuma kosusu suresi (s). Varsayilan 60. 0 verilirse kosulmaz.

.PARAMETER Isinma
  Her kosunun isinma suresi (s; kayda girmez). Varsayilan 6.

.PARAMETER Durumlar
  Kosulacak pencere durumlari. Varsayilan: gorunur simge ortulu (gorunur referanstir, once kosar).

.PARAMETER Sorma
  Kullaniciya soru sorma (prize takili degil uyarisi beklemez; ozet bilinmiyorsa Godot calistirilmaz).
#>
[CmdletBinding()]
param(
    [switch]$KuruCalistir,
    [string]$CiktiKok = '',
    [string]$GodotExe = '',
    [ValidateSet('varsayilan', 'mobile', 'opengl3')][string]$Cizim,
    [ValidateRange(0, 3600)][int]$Sure = 120,
    [ValidateRange(0, 3600)][int]$OkumaSure = 60,
    [ValidateRange(1, 120)][int]$Isinma = 6,
    [ValidateSet('gorunur', 'simge', 'ortulu')][string[]]$Durumlar = @('gorunur', 'simge', 'ortulu'),
    [switch]$Sorma
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

# ---------------------------------------------------------------------------
# Sabitler
# ---------------------------------------------------------------------------
$PaketSurumu = 'W0-MP8a 2'
$GodotSurum = '4.7.2'
$ZipAdi = 'Godot_v4.7.2-stable_win64.exe.zip'
$ExeAdi = 'Godot_v4.7.2-stable_win64.exe'
$IndirmeUrl = 'https://github.com/godotengine/godot-builds/releases/download/4.7.2-stable/' + $ZipAdi
# Kaynak: resmi surum varliklari (github.com/godotengine/godot-builds, 4.7.2-stable):
#   SHA-512: ayni surumun SHA512-SUMS.txt dosyasi;
#   SHA-256: GitHub surum API'sinin bu varlik icin yayimladigi "digest" alani.
# Ayrinti ve dogrulama tarihi: tools/olcum/windows/BENIOKU.md, docs/olcumler/MP8-windows.md.
# Ikisi de bos birakilirsa betik ozeti "bilinmiyor" sayar, kullaniciya gosterir ve onay ister.
$BeklenenSha512 = '83decd58fdf67b9d657958a1ae6bf1929c20785315a81effe245874cdc57acb709bf868e00778a96984338c1b29dafdb453c6847747694621c6ecf5da2259993'
$BeklenenSha256 = '731980f9608d61333e5baf54a2ef17210acc7a538446c0cb9969f002aca1e953'

$DenemeSure = 5
$DenemeIsinma = 3
$HazirAsimiDeneme = 120
$HazirAsimiKampanya = 180
$GecisAzami = 30          # olcum.gd: durum 30 s icinde olusmazsa olcum yine baslar
$GozlemAraligi = 10       # dis gozlem araligi (s)
$KareOrnek = 300          # her 300. kare ham dokulur (120 s x 30 Hz -> 12 dokum / kosu)
$DokumSiniriBayt = 50MB
# Kuresel sure siniri: betik baslangicindan itibaren 15 dk. Kullanici sorusunda beklenen sure ve
# Godot indirme suresi (ag hizina bagli, betigin denetiminde degil) sinira eklenir.
# Asilirsa kalan adimlar ATLANDI yazilir; Ozet ve Zip her durumda yapilir.
$KureselSinirDk = 15

$CizimSecenekleri = [ordered]@{
    'varsayilan' = @()
    'mobile'     = @('--rendering-method', 'mobile')
    'opengl3'    = @('--rendering-driver', 'opengl3')
}

# ---------------------------------------------------------------------------
# Durum degiskenleri
# ---------------------------------------------------------------------------
$script:GunlukYolu = $null
$script:GunlukTampon = New-Object System.Collections.Generic.List[string]
$script:Utf8 = New-Object System.Text.UTF8Encoding($false)
$script:AdimSonuclari = New-Object System.Collections.Generic.List[object]
$script:AdimIsareti = $null
$script:AdimIletisi = ''
$script:Kok = $null
$script:Exe = $null
$script:GodotBilgi = [ordered]@{ kaynak = ''; zip = ''; exe = ''; sha512 = ''; sha256 = ''; ozet_denetimi = 'yapilmadi' }
$script:W32Hazir = $false
$script:UykuEngellendi = $false
$script:SecilenCizim = $null
$script:Denemeler = New-Object System.Collections.Generic.List[object]
$script:Kosular = New-Object System.Collections.Generic.List[object]
$script:AcikSurecler = New-Object System.Collections.Generic.List[object]
$script:OrtuSurec = $null
$script:SureSiniri = $null
$script:KampanyaBaslangic = $null
$script:KampanyaBitis = $null
$script:ZipYolu = $null

$WindowsMu = $false
if ($PSVersionTable.PSEdition -eq 'Desktop') {
    $WindowsMu = $true
} else {
    $iw = Get-Variable -Name IsWindows -ValueOnly -ErrorAction SilentlyContinue
    if ($iw) { $WindowsMu = $true }
}

$BetikDizini = $PSScriptRoot
$ProjeDizini = [System.IO.Path]::GetFullPath((Join-Path (Join-Path (Split-Path -Parent $BetikDizini) 'mp6') 'proje'))
if ($env:IDA_CACHE) { $OnbellekKok = $env:IDA_CACHE } else { $OnbellekKok = Join-Path (Join-Path $HOME '.cache') 'ida' }
$GodotOnbellek = Join-Path (Join-Path $OnbellekKok 'godot') $GodotSurum
$ZamanDamgasi = Get-Date -Format 'yyyyMMdd-HHmmss'
$KlasorAdi = 'IDA-MP8-' + $ZamanDamgasi

# ---------------------------------------------------------------------------
# Yardimci islevler
# ---------------------------------------------------------------------------
function Zaman {
    return (Get-Date -Format "yyyy-MM-dd'T'HH:mm:ss.fffzzz")
}

function Yaz-Gunluk {
    param([string]$Mesaj, [string]$Duzey = 'BILGI')
    $satir = '{0} [{1}] {2}' -f (Zaman), $Duzey, $Mesaj
    if ($Duzey -eq 'HATA') {
        Write-Host $satir -ForegroundColor Red
    } elseif ($Duzey -eq 'UYARI') {
        Write-Host $satir -ForegroundColor Yellow
    } else {
        Write-Host $satir
    }
    if ($script:GunlukYolu) {
        try {
            [System.IO.File]::AppendAllText($script:GunlukYolu, $satir + "`r`n", $script:Utf8)
        } catch {
            Write-Host ('gunluk dosyasina yazilamadi: ' + $_.Exception.Message) -ForegroundColor Red
        }
    } else {
        $script:GunlukTampon.Add($satir)
    }
}

function Yaz-Metin {
    param([string]$Yol, [string]$Metin)
    [System.IO.File]::WriteAllText($Yol, $Metin, $script:Utf8)
}

function Ekle-Satir {
    param([string]$Yol, [string]$Satir)
    [System.IO.File]::AppendAllText($Yol, $Satir + "`n", $script:Utf8)
}

function Isaretle {
    param([string]$Durum, [string]$Ileti)
    $script:AdimIsareti = $Durum
    $script:AdimIletisi = $Ileti
    $duzey = 'UYARI'
    if ($Durum -eq 'HATA') { $duzey = 'HATA' }
    Yaz-Gunluk ($Durum + ': ' + $Ileti) $duzey
}

function Siniri-Ertele {
    # Kullanici beklemesi ve indirme suresi kuresel sinira eklenir.
    param([double]$Saniye)
    if ($null -ne $script:SureSiniri) { $script:SureSiniri = $script:SureSiniri.AddSeconds($Saniye) }
}

function Adim {
    # -Sinirli: kuresel sure siniri asildiysa adim calistirilmaz, ATLANDI yazilir.
    param([string]$Ad, [switch]$Sinirli, [scriptblock]$Is)
    Yaz-Gunluk ('==== ADIM: ' + $Ad)
    $script:AdimIsareti = $null
    $script:AdimIletisi = ''
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $durum = 'TAMAM'
    $ileti = ''
    try {
        if ($Sinirli -and ($null -ne $script:SureSiniri) -and ((Get-Date) -gt $script:SureSiniri)) {
            Isaretle 'ATLANDI' ('kuresel sure siniri (' + $KureselSinirDk + ' dk) asildi')
        } else {
            & $Is | Out-Null
        }
        if ($script:AdimIsareti) {
            $durum = $script:AdimIsareti
            $ileti = $script:AdimIletisi
        }
    } catch {
        $durum = 'HATA'
        $ileti = $_.Exception.Message
        Yaz-Gunluk ('ADIM HATASI [' + $Ad + ']: ' + $ileti) 'HATA'
        try { Yaz-Gunluk ((($_ | Out-String) -replace '\s+$', '')) 'HATA' } catch { }
        if ($_.ScriptStackTrace) { Yaz-Gunluk ('yigin: ' + ($_.ScriptStackTrace -replace "`r?`n", ' | ')) 'HATA' }
    }
    $sw.Stop()
    $s = [Math]::Round($sw.Elapsed.TotalSeconds, 1)
    $script:AdimSonuclari.Add([pscustomobject]@{ ad = $Ad; durum = $durum; ileti = $ileti; sure_s = $s })
    $duzey = 'BILGI'
    if ($durum -eq 'HATA') { $duzey = 'HATA' } elseif ($durum -ne 'TAMAM') { $duzey = 'UYARI' }
    Yaz-Gunluk ('==== ADIM SONU: ' + $Ad + ' -> ' + $durum + ' (' + $s + ' s)') $duzey
}

function Godot-Yolu {
    # Godot'ya verilen yollar ileri egik cizgiyle (C:/...) yazilir.
    param([string]$Yol)
    return ($Yol -replace '\\', '/')
}

function Arg-Tirnakla {
    # CommandLineToArgvW kurallari: bosluk ya da tirnak iceren arguman tirnaklanir.
    param([string]$A)
    if ($A -eq '') { return '""' }
    if ($A -notmatch '[\s"]') { return $A }
    $s = $A -replace '(\\*)"', '$1$1\"'
    $s = $s -replace '(\\+)$', '$1$1'
    return '"' + $s + '"'
}

function Arglari-Birlestir {
    param([string[]]$Liste)
    $parcalar = @()
    foreach ($a in $Liste) { $parcalar += (Arg-Tirnakla $a) }
    return ($parcalar -join ' ')
}

function Olcum-Arglari {
    param([string]$Klasor, [string]$Durum, [int]$SureS, [int]$IsinmaS, [string[]]$CizimArg, [string[]]$EkArg)
    $liste = @('--path', (Godot-Yolu $ProjeDizini), '--position', '160,160', '--resolution', '480x360')
    if ($Durum -eq 'gorunur') { $liste += '--always-on-top' }
    $liste += @('--log-file', (Godot-Yolu (Join-Path $Klasor 'godot_log.txt')))
    if ($CizimArg) { $liste += $CizimArg }
    $liste += @('--', '--yol=b', ('--durum=' + $Durum), ('--sure=' + $SureS), ('--isinma=' + $IsinmaS), ('--cikti=' + (Godot-Yolu $Klasor)))
    if ($EkArg) { $liste += $EkArg }
    return $liste
}

function Ortu-Arglari {
    param([string]$Klasor, [int]$SureS, [string[]]$CizimArg)
    # Olcum penceresini (160,160; 480x360) tamamen kapsayan, her zaman ustte ikinci Godot sureci.
    $liste = @('--path', (Godot-Yolu $ProjeDizini), '--always-on-top', '--position', '40,40', '--resolution', '1100x900')
    $liste += @('--log-file', (Godot-Yolu (Join-Path $Klasor 'ortu_godot_log.txt')))
    if ($CizimArg) { $liste += $CizimArg }
    $liste += @('--', '--rol=ortu', ('--sure=' + ($SureS + 120)))
    return $liste
}

function Json-Yaz {
    param([string]$Yol, $Nesne)
    Yaz-Metin $Yol (ConvertTo-Json -InputObject $Nesne -Depth 8)
}

function Cim-Al {
    # Get-CimInstance; olmazsa Get-WmiObject. Hata yukari firlatilir.
    param([string]$Sinif, [string]$Ad = 'root\cimv2', [string]$Suzgec = '')
    try {
        if ($Suzgec) { return @(Get-CimInstance -Namespace $Ad -ClassName $Sinif -Filter $Suzgec -ErrorAction Stop) }
        return @(Get-CimInstance -Namespace $Ad -ClassName $Sinif -ErrorAction Stop)
    } catch {
        $ilk = $_.Exception.Message
        try {
            if ($Suzgec) { return @(Get-WmiObject -Namespace $Ad -Class $Sinif -Filter $Suzgec -ErrorAction Stop) }
            return @(Get-WmiObject -Namespace $Ad -Class $Sinif -ErrorAction Stop)
        } catch {
            throw ('CIM ' + $Sinif + ': ' + $ilk + ' / WMI: ' + $_.Exception.Message)
        }
    }
}

function Ortam-Yuku {
    # Kosu oncesi arka plan yuku: toplam islemci % ve bos bellek (MB). Yalniz tani icin.
    $s = ''
    try {
        $p = @(Cim-Al 'Win32_PerfFormattedData_PerfOS_Processor' 'root\cimv2' "Name='_Total'")
        if ($p.Count -gt 0) { $s += 'islemci_yuzde=' + $p[0].PercentProcessorTime + ' ' }
    } catch { $s += 'islemci_yuzde=? ' }
    try {
        $o = @(Cim-Al 'Win32_OperatingSystem')
        if ($o.Count -gt 0) { $s += 'bos_bellek_mb=' + [Math]::Round([double]$o[0].FreePhysicalMemory / 1024) }
    } catch { $s += 'bos_bellek_mb=?' }
    return $s
}

function Sureci-Kapat {
    # Yalniz bu betigin kendi baslattigi surec nesnesini kapatir (Start-Process -PassThru ile
    # alinan nesne; yurutulebilir dosya yolu sahiplik sayilmaz). Kuru calistirmada hicbir sey yapmaz.
    # Donus: surec kapatilmak zorunda kalindiysa $true. Cagiran donusu her zaman yakalar.
    param($Surec, [string]$Ad)
    if ($KuruCalistir) { return $false }
    if ($null -eq $Surec) { return $false }
    $olduruldu = $false
    try {
        if (-not $Surec.HasExited) {
            Yaz-Gunluk ('surec kapatiliyor: ' + $Ad + ' pid=' + $Surec.Id) 'UYARI'
            $Surec.Kill()
            $olduruldu = $true
            [void]$Surec.WaitForExit(5000)
        }
    } catch {
        Yaz-Gunluk ('surec kapatilamadi: ' + $Ad + ': ' + $_.Exception.Message) 'HATA'
    }
    return $olduruldu
}

function Sure-Yeter {
    # Kuresel sure siniri: kalan sure GerekenS saniyeden azsa $false.
    param([int]$GerekenS)
    if ($null -eq $script:SureSiniri) { return $true }
    $kalan = ($script:SureSiniri - (Get-Date)).TotalSeconds
    return ($kalan -ge $GerekenS)
}

function Godot-Baslat {
    param([string[]]$Arglar, [string]$Klasor, [string]$Onek)
    $argMetin = Arglari-Birlestir $Arglar
    Yaz-Gunluk ('baslat: ' + $script:Exe + ' ' + $argMetin)
    $p = Start-Process -FilePath $script:Exe -ArgumentList $argMetin -RedirectStandardOutput (Join-Path $Klasor ($Onek + 'godot_stdout.txt')) -RedirectStandardError (Join-Path $Klasor ($Onek + 'godot_stderr.txt')) -PassThru
    $null = $p.Handle
    $script:AcikSurecler.Add($p)
    return $p
}

function Bant-Satirlari {
    # Godot'nun cizim yontemi bandi ("... - Forward+ - Using Device ...") ve geri dusme uyarilari.
    param([string]$Klasor, [string]$Onek)
    $bant = @()
    $uyari = @()
    foreach ($ad in @('godot_log.txt', 'godot_stdout.txt', 'godot_stderr.txt')) {
        $f = Join-Path $Klasor ($Onek + $ad)
        if (-not (Test-Path -LiteralPath $f)) { continue }
        try {
            foreach ($m in @(Select-String -LiteralPath $f -Pattern 'Using Device' -SimpleMatch)) { $bant += ($ad + ': ' + $m.Line.Trim()) }
            foreach ($m in @(Select-String -LiteralPath $f -Pattern 'switching to|Unable to initialize|seem not to support|Invalid renderer|Unknown rendering')) { $uyari += ($ad + ': ' + $m.Line.Trim()) }
        } catch { $uyari += ($ad + ': okunamadi: ' + $_.Exception.Message) }
    }
    return @{ bant = $bant; uyari = $uyari }
}

function Pencere-Gozlem {
    # Bir dis gozlem satiri. gecen_s: olcum surecinin 'hazir' dosyasinin goruldugu andan beri (s).
    param([int]$OlcumNo, [int]$OrtuNo, [string]$Durum, [string]$Dosya, [double]$GecenS)
    if (-not $script:W32Hazir) { return }
    $inv = [System.Globalization.CultureInfo]::InvariantCulture
    $gs = ([Math]::Round($GecenS, 2)).ToString($inv)
    try {
        $j = [IdaMp8W32]::Gozlem([uint32]$OlcumNo, [uint32]$OrtuNo)
        Ekle-Satir $Dosya ('{"t":"' + (Zaman) + '","gecen_s":' + $gs + ',"durum":"' + $Durum + '","w32":' + $j + '}')
    } catch {
        Ekle-Satir $Dosya ('{"t":"' + (Zaman) + '","gecen_s":' + $gs + ',"durum":"' + $Durum + '","hata":"gozlem basarisiz"}')
        Yaz-Gunluk ('dis gozlem hatasi: ' + $_.Exception.Message) 'UYARI'
    }
}

function Surec-Ornek {
    param($Surec, [string]$Dosya, [double]$GecenS)
    try {
        $Surec.Refresh()
        if ($Surec.HasExited) { return }
        $inv = [System.Globalization.CultureInfo]::InvariantCulture
        $oncelik = '?'
        try { $oncelik = [string]$Surec.PriorityClass } catch { $oncelik = '?' }
        $yanit = '?'
        try { $yanit = [string]$Surec.Responding } catch { $yanit = '?' }
        $satir = (Zaman) + ',' + ([Math]::Round($GecenS, 2)).ToString($inv) + ',' + $Surec.Id + ',' + $Surec.TotalProcessorTime.TotalSeconds.ToString('F2', $inv) + ',' + $Surec.WorkingSet64 + ',' + $oncelik + ',' + $yanit
        Ekle-Satir $Dosya $satir
    } catch {
        Yaz-Gunluk ('surec ornegi alinamadi: ' + $_.Exception.Message) 'UYARI'
    }
}

function Kos-Godot {
    # Tek olcum kosusu. Iki ayri denetim: (1) ozet.json var mi (olcum yazildi mi),
    # (2) surec temiz bitti mi (hata yok, sure asimi yok, kapatilmadi, cikis kodu 0).
    param(
        [string]$Ad, [string]$Klasor, [string]$Durum, [int]$SureS, [int]$IsinmaS,
        [string]$CizimAd, [string[]]$CizimArg, [string[]]$EkArg, [int]$HazirAsimS
    )
    $sonuc = [ordered]@{
        ad = $Ad; klasor = $Klasor; durum = $Durum; sure_s = $SureS; isinma_s = $IsinmaS
        cizim = $CizimAd; cizim_arg = ($CizimArg -join ' '); ek_arg = ($EkArg -join ' ')
        baslangic = (Zaman); hazir = $false; hazir_s = $null; ozet_var = $false
        istek = $null; uretildi = $null; durum_dogrulandi_ic = $null; durum_dogrulama_ic = ''
        cikis_kodu = $null; sure_asildi = $false; sure_siniri = $false; olduruldu = $false; ortu_erken_kapandi = $false
        surec_temiz = $false; duzenek = ''; olcum_pid = $null; ortu_pid = $null
        dis_gozlem = $script:W32Hazir; bant = @(); uyari = @(); ortu_bant = @(); hata = ''; bitis = $null
    }
    New-Item -ItemType Directory -Force -Path $Klasor | Out-Null
    $kosLog = Join-Path $Klasor 'kos.log'
    Ekle-Satir $kosLog ('baslangic ' + (Zaman) + ' yol=b durum=' + $Durum + ' sure=' + $SureS + ' cizim=' + $CizimAd)
    if ($WindowsMu) { Ekle-Satir $kosLog ('ortam_yuku ' + (Ortam-Yuku)) }
    $arglar = Olcum-Arglari $Klasor $Durum $SureS $IsinmaS $CizimArg $EkArg
    Ekle-Satir $kosLog ('komut ' + $script:Exe + ' ' + (Arglari-Birlestir $arglar))
    $p = $null
    $script:OrtuSurec = $null
    try {
        $p = Godot-Baslat $arglar $Klasor ''
        $sonuc.olcum_pid = $p.Id
        Ekle-Satir $kosLog ('olcum_pid ' + $p.Id)
        # Isinmanin bitmesini bekle (olcum.gd 'hazir' dosyasini yazar; shader derlemesi dahil).
        $hazirDosya = Join-Path $Klasor 'hazir'
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        while ($true) {
            if (Test-Path -LiteralPath $hazirDosya) { break }
            if ($p.HasExited) { break }
            if ($sw.Elapsed.TotalSeconds -gt $HazirAsimS) { break }
            if (-not (Sure-Yeter 0)) { $sonuc.sure_siniri = $true; break }
            Start-Sleep -Milliseconds 500
        }
        if ((-not $sonuc.sure_siniri) -and (Test-Path -LiteralPath $hazirDosya)) {
            Kos-Izle $p $sonuc $sw $kosLog $Klasor $Durum $SureS $CizimArg
        } else {
            if ($sonuc.sure_siniri) {
                $sonuc.hata = 'kuresel sure siniri doldu (hazir beklenirken)'
            } elseif ($p.HasExited) {
                $sonuc.hata = 'Godot hazir olmadan kapandi'
            } else {
                $sonuc.hata = 'hazir dosyasi ' + $HazirAsimS + ' s icinde gelmedi'
            }
            Ekle-Satir $kosLog ('HATA: ' + $sonuc.hata)
        }
    } catch {
        $sonuc.hata = 'Godot baslatma ya da izleme hatasi: ' + $_.Exception.Message
        Ekle-Satir $kosLog ('HATA: ' + $sonuc.hata)
    } finally {
        if ($p) {
            # Olcum sureci kendiliginden biter; sure asimi yoksa kapanmasi icin 10 s daha beklenir.
            if ((-not $p.HasExited) -and (-not $sonuc.sure_asildi) -and (-not $sonuc.sure_siniri)) { [void]$p.WaitForExit(10000) }
            $sonuc.olduruldu = Sureci-Kapat $p ($Ad + ' olcum')
            try { $sonuc.cikis_kodu = $p.ExitCode } catch { $sonuc.cikis_kodu = $null }
        }
        # Ortu sureci tasarim geregi olcum bitince kapatilir; bu bir hata sayilmaz.
        $null = Sureci-Kapat $script:OrtuSurec ($Ad + ' ortu')
        Ekle-Satir $kosLog ('cikis_kodu ' + $sonuc.cikis_kodu + ' ' + (Get-Date -Format 'HH:mm:ss'))
        $sonuc.bitis = (Zaman)
    }

    $ozetYol = Join-Path $Klasor 'ozet.json'
    if (Test-Path -LiteralPath $ozetYol) {
        try {
            $oz = Get-Content -LiteralPath $ozetYol -Raw | ConvertFrom-Json
            $sonuc.ozet_var = $true
            $sonuc.istek = $oz.sayac.istek
            $sonuc.uretildi = $oz.sayac.uretildi
            $sonuc.durum_dogrulandi_ic = $oz.durum_dogrulandi
            $sonuc.durum_dogrulama_ic = $oz.durum_dogrulama
        } catch {
            if (-not $sonuc.hata) { $sonuc.hata = 'ozet.json okunamadi: ' + $_.Exception.Message }
        }
    } else {
        if (-not $sonuc.hata) { $sonuc.hata = 'ozet.json yok (olcum tamamlanmadi)' }
    }

    # Surecin temiz bitisi, ozetin varligindan ayri denetlenir.
    $neden = @()
    if ($sonuc.hata) { $neden += 'hata' }
    if ($sonuc.sure_asildi) { $neden += 'sure_asimi' }
    if ($sonuc.sure_siniri) { $neden += 'kuresel_sure_siniri' }
    if ($sonuc.olduruldu) { $neden += 'kapatildi' }
    if ($null -eq $sonuc.cikis_kodu) {
        $neden += 'cikis_kodu_bilinmiyor'
    } elseif ($sonuc.cikis_kodu -ne 0) {
        $neden += ('cikis_kodu_' + $sonuc.cikis_kodu)
    }
    if ($sonuc.ortu_erken_kapandi) { $neden += 'ortu_erken_kapandi' }
    if ($neden.Count -eq 0) {
        $sonuc.surec_temiz = $true
        $sonuc.duzenek = 'calisti'
        Ekle-Satir $kosLog 'surec_temiz true'
    } else {
        $sonuc.surec_temiz = $false
        $sonuc.duzenek = 'calismadi: ' + ($neden -join ',')
        Ekle-Satir $kosLog ('surec_temiz false ' + ($neden -join ','))
    }

    $b = Bant-Satirlari $Klasor ''
    $sonuc.bant = $b.bant
    $sonuc.uyari = $b.uyari
    if ($script:OrtuSurec) { $sonuc.ortu_bant = (Bant-Satirlari $Klasor 'ortu_').bant }
    Yaz-Gunluk ($Ad + ': istek=' + $sonuc.istek + ' uretildi=' + $sonuc.uretildi + ' ic_dogrulama=' + $sonuc.durum_dogrulandi_ic + ' (' + $sonuc.durum_dogrulama_ic + ') cikis=' + $sonuc.cikis_kodu + ' duzenek=' + $sonuc.duzenek)
    foreach ($x in $sonuc.bant) { Yaz-Gunluk ($Ad + ': bant: ' + $x) }
    foreach ($x in $sonuc.uyari) { Yaz-Gunluk ($Ad + ': godot uyarisi: ' + $x) 'UYARI' }
    if ($sonuc.hata) { Yaz-Gunluk ($Ad + ': ' + $sonuc.hata) 'HATA' }
    return [pscustomobject]$sonuc
}

function Kos-Izle {
    # Isinmadan sonra: ortu sureci (gerekirse), dis gozlem ve surec ornegi; olcum kendiliginden biter.
    # Gozlem zamani (gecen_s) 'hazir' dosyasinin goruldugu andan baslar; cozumleyici bunu
    # hazir dosyasindaki Godot saatiyle olcum baslangic/bitisine esler.
    param($P, $Sonuc, $Sw, [string]$KosLog, [string]$Klasor, [string]$Durum, [int]$SureS, [string[]]$CizimArg)
    $izle = [System.Diagnostics.Stopwatch]::StartNew()
    $Sonuc.hazir = $true
    $Sonuc.hazir_s = [Math]::Round($Sw.Elapsed.TotalSeconds, 1)
    Ekle-Satir $KosLog ('hazir ' + (Get-Date -Format 'HH:mm:ss'))
    Ekle-Satir $KosLog 'gozlem_zaman_koku hazir_dosyasi_goruldu'
    Yaz-Gunluk ($Sonuc.ad + ': isinma bitti (' + $Sonuc.hazir_s + ' s); durum ' + $Durum + ' kuruluyor')
    $ortuNo = 0
    if ($Durum -eq 'ortulu') {
        $oArg = Ortu-Arglari $Klasor $SureS $CizimArg
        Ekle-Satir $KosLog ('ortu_komut ' + $script:Exe + ' ' + (Arglari-Birlestir $oArg))
        $script:OrtuSurec = Godot-Baslat $oArg $Klasor 'ortu_'
        $ortuNo = $script:OrtuSurec.Id
        $Sonuc.ortu_pid = $ortuNo
        Ekle-Satir $KosLog ('ortu_pid ' + $ortuNo)
    } else {
        Ekle-Satir $KosLog 'ortu_pid yok'
    }
    $pencereDosya = Join-Path $Klasor 'pencere_win.jsonl'
    $surecDosya = Join-Path $Klasor 'surec_win.csv'
    Ekle-Satir $surecDosya 't,gecen_s,pid,islemci_s,calisma_kumesi_b,oncelik,yanit_veriyor'
    if (-not $script:W32Hazir) { Ekle-Satir $KosLog 'dis_gozlem yok' }
    $sinirS = $SureS + $GecisAzami + 90
    $sonGozlem = -1000.0
    while (-not $P.HasExited) {
        $g = $izle.Elapsed.TotalSeconds
        if (($g - $sonGozlem) -ge $GozlemAraligi) {
            Pencere-Gozlem $P.Id $ortuNo $Durum $pencereDosya $g
            Surec-Ornek $P $surecDosya $g
            $sonGozlem = $g
        }
        if (($ortuNo -ne 0) -and (-not $Sonuc.ortu_erken_kapandi) -and $script:OrtuSurec.HasExited) {
            $Sonuc.ortu_erken_kapandi = $true
            Ekle-Satir $KosLog ('HATA: ortu sureci olcumden once kapandi (gecen_s ' + [Math]::Round($g, 1) + ')')
            Yaz-Gunluk ($Sonuc.ad + ': ortu sureci olcumden once kapandi') 'HATA'
        }
        if ($g -gt $sinirS) {
            $Sonuc.sure_asildi = $true
            Ekle-Satir $KosLog 'HATA: sure asildi, surec kapatiliyor'
            Yaz-Gunluk ($Sonuc.ad + ': sure asildi (' + [Math]::Round($g) + ' s), surec kapatiliyor') 'HATA'
            break
        }
        if (-not (Sure-Yeter 0)) {
            $Sonuc.sure_siniri = $true
            Ekle-Satir $KosLog ('HATA: kuresel sure siniri doldu, surec kapatiliyor (gecen_s ' + [Math]::Round($g, 1) + ')')
            Yaz-Gunluk ($Sonuc.ad + ': kuresel sure siniri doldu, surec kapatiliyor') 'HATA'
            break
        }
        Start-Sleep -Milliseconds 500
    }
}

function Dokumler {
    # Ham kare dokumleri (olcum.gd: kare_NNNNNNN.rgb ya da .rgba).
    return @(Get-ChildItem -LiteralPath $script:Kok -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like 'kare_*.rgb' -or $_.Name -like 'kare_*.rgba' })
}

function Ozet-Hesapla {
    param([string]$Yol)
    $h512 = (Get-FileHash -LiteralPath $Yol -Algorithm SHA512).Hash.ToLowerInvariant()
    $h256 = (Get-FileHash -LiteralPath $Yol -Algorithm SHA256).Hash.ToLowerInvariant()
    return @{ sha512 = $h512; sha256 = $h256 }
}

function Ozet-Uyuyor {
    param($Oz)
    if (-not $BeklenenSha512 -and -not $BeklenenSha256) { return $null }   # bilinmiyor
    $tamam = $true
    if ($BeklenenSha512 -and ($Oz.sha512 -ne $BeklenenSha512.ToLowerInvariant())) { $tamam = $false }
    if ($BeklenenSha256 -and ($Oz.sha256 -ne $BeklenenSha256.ToLowerInvariant())) { $tamam = $false }
    return $tamam
}

function Indir {
    param([string]$Url, [string]$Hedef)
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    } catch { }
    try {
        Invoke-WebRequest -Uri $Url -OutFile $Hedef -UseBasicParsing
    } catch {
        Yaz-Gunluk ('Invoke-WebRequest basarisiz: ' + $_.Exception.Message + ' ; WebClient deneniyor') 'UYARI'
        $wc = New-Object System.Net.WebClient
        try { $wc.DownloadFile($Url, $Hedef) } finally { $wc.Dispose() }
    }
}

# Windows yardimcisi (user32/dwmapi/kernel32). C# 5 ile uyumlu yazildi (PS 5.1 Add-Type).
# Gizlilik: baska uygulamalarin pencere basliklari kaydedilmez; on plandaki pencere icin yalniz
# bu calistirmanin sureclerine ait olup olmadigi ve pencere sinif adi yazilir.
$W32Kaynak = @'
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;

public static class IdaMp8W32
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X; public int Y; }

    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] private static extern bool IsIconic(IntPtr hWnd);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] private static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] private static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);
    [DllImport("user32.dll")] private static extern IntPtr WindowFromPoint(POINT p);
    [DllImport("user32.dll")] private static extern IntPtr GetAncestor(IntPtr hWnd, uint gaFlags);
    [DllImport("user32.dll")] private static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] private static extern IntPtr GetWindow(IntPtr hWnd, uint uCmd);
    [DllImport("user32.dll")] private static extern IntPtr SetThreadDpiAwarenessContext(IntPtr dpiContext);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "GetWindowTextW")]
    private static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "GetClassNameW")]
    private static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);
    [DllImport("user32.dll", EntryPoint = "GetWindowLongW")] private static extern int GetWindowLong(IntPtr hWnd, int nIndex);
    [DllImport("dwmapi.dll")] private static extern int DwmGetWindowAttribute(IntPtr hwnd, int dwAttribute, out int pvAttribute, int cbAttribute);
    [DllImport("dwmapi.dll", EntryPoint = "DwmGetWindowAttribute")]
    private static extern int DwmGetWindowAttributeRect(IntPtr hwnd, int dwAttribute, out RECT pvAttribute, int cbAttribute);
    [DllImport("kernel32.dll")] private static extern uint SetThreadExecutionState(uint esFlags);

    private const int GWL_EXSTYLE = -20;
    private const int WS_EX_TOPMOST = 0x8;
    private const uint GA_ROOT = 2;
    private const uint GW_HWNDPREV = 3;
    private const int DWMWA_EXTENDED_FRAME_BOUNDS = 9;
    private const int DWMWA_CLOAKED = 14;
    private const uint ES_CONTINUOUS = 0x80000000;
    private const uint ES_SYSTEM_REQUIRED = 0x1;
    private const uint ES_DISPLAY_REQUIRED = 0x2;

    // Ayar degistirmez: yalniz bu is parcacigi suresince uyku ve ekran kapanmasini erteler.
    public static uint UykuEngelle() { return SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED); }
    public static uint UykuSerbest() { return SetThreadExecutionState(ES_CONTINUOUS); }

    public static List<IntPtr> Pencereler(uint pid)
    {
        List<IntPtr> liste = new List<IntPtr>();
        if (pid == 0) { return liste; }
        EnumWindows(delegate (IntPtr h, IntPtr l)
        {
            uint p;
            GetWindowThreadProcessId(h, out p);
            if (p == pid) { liste.Add(h); }
            return true;
        }, IntPtr.Zero);
        return liste;
    }

    private static uint SahipPid(IntPtr h)
    {
        uint p = 0;
        if (h != IntPtr.Zero) { GetWindowThreadProcessId(h, out p); }
        return p;
    }

    private static string Sahip(IntPtr h, uint olcumPid, uint ortuPid)
    {
        if (h == IntPtr.Zero) { return "yok"; }
        uint p = SahipPid(h);
        if (olcumPid != 0 && p == olcumPid) { return "olcum"; }
        if (ortuPid != 0 && p == ortuPid) { return "ortu"; }
        return "diger";
    }

    private static string Baslik(IntPtr h)
    {
        StringBuilder sb = new StringBuilder(512);
        GetWindowText(h, sb, sb.Capacity);
        return sb.ToString();
    }

    private static string Sinif(IntPtr h)
    {
        StringBuilder sb = new StringBuilder(256);
        GetClassName(h, sb, sb.Capacity);
        return sb.ToString();
    }

    private static string Kacis(string s)
    {
        StringBuilder sb = new StringBuilder();
        foreach (char c in s)
        {
            if (c == '"') { sb.Append("\\\""); }
            else if (c == '\\') { sb.Append("\\\\"); }
            else if (c < 0x20 || c > 0x7e) { sb.Append("\\u" + ((int)c).ToString("x4", CultureInfo.InvariantCulture)); }
            else { sb.Append(c); }
        }
        return sb.ToString();
    }

    private static string B(bool b) { return b ? "true" : "false"; }

    private static string RectJson(RECT r)
    {
        return string.Format(CultureInfo.InvariantCulture, "[{0},{1},{2},{3}]", r.Left, r.Top, r.Right, r.Bottom);
    }

    // Pencerenin gorunen dis cercevesi (gorunmez yeniden boyutlandirma kenarlari haric).
    private static bool DisCerceve(IntPtr h, out RECT r)
    {
        r = new RECT();
        if (h == IntPtr.Zero) { return false; }
        try { return DwmGetWindowAttributeRect(h, DWMWA_EXTENDED_FRAME_BOUNDS, out r, Marshal.SizeOf(typeof(RECT))) == 0; }
        catch (Exception) { return false; }
    }

    // 'ust' penceresi, Z sirasinda 'alt' penceresinin ustunde mi (alt'tan GW_HWNDPREV ile yukari yurunur).
    private static bool Ustunde(IntPtr ust, IntPtr alt)
    {
        if (ust == IntPtr.Zero || alt == IntPtr.Zero) { return false; }
        IntPtr h = GetWindow(alt, GW_HWNDPREV);
        int sayac = 0;
        while (h != IntPtr.Zero && sayac < 100000)
        {
            if (h == ust) { return true; }
            h = GetWindow(h, GW_HWNDPREV);
            sayac++;
        }
        return false;
    }

    // Yalniz bu calistirmanin pencereleri icin cagrilir (baslik dahil).
    private static string PencereJson(IntPtr h)
    {
        RECT r;
        GetWindowRect(h, out r);
        RECT d;
        bool dOk = DisCerceve(h, out d);
        int cloak = -1;
        try { int v; if (DwmGetWindowAttribute(h, DWMWA_CLOAKED, out v, 4) == 0) { cloak = v; } } catch (Exception) { cloak = -1; }
        bool enUstte = (GetWindowLong(h, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
        return string.Format(CultureInfo.InvariantCulture,
            "{{\"hwnd\":{0},\"baslik\":\"{1}\",\"sinif\":\"{2}\",\"gorunur\":{3},\"simge\":{4},\"en_ustte\":{5},\"cloak\":{6},\"pencere\":{7},\"dis_cerceve\":{8}}}",
            h.ToInt64(), Kacis(Baslik(h)), Kacis(Sinif(h)), B(IsWindowVisible(h)), B(IsIconic(h)), B(enUstte), cloak, RectJson(r), dOk ? RectJson(d) : "null");
    }

    // Surecin ana penceresi: basligi "IDA MP6a" ile baslayan, tercihen gorunur ve en buyuk pencere.
    private static IntPtr AnaPencere(List<IntPtr> liste)
    {
        IntPtr secilen = IntPtr.Zero;
        long enIyi = -1;
        foreach (IntPtr h in liste)
        {
            RECT r;
            GetWindowRect(h, out r);
            long puan = (long)Math.Max(0, r.Right - r.Left) * (long)Math.Max(0, r.Bottom - r.Top);
            if (Baslik(h).StartsWith("IDA MP6a", StringComparison.Ordinal)) { puan += 4000000000000L; }
            if (IsWindowVisible(h)) { puan += 2000000000000L; }
            if (puan > enIyi) { enIyi = puan; secilen = h; }
        }
        return secilen;
    }

    private static string ListeJson(List<IntPtr> liste)
    {
        StringBuilder sb = new StringBuilder("[");
        for (int i = 0; i < liste.Count; i++)
        {
            if (i > 0) { sb.Append(","); }
            sb.Append(PencereJson(liste[i]));
        }
        sb.Append("]");
        return sb.ToString();
    }

    // Bir gozlem. Koordinatlar icin is parcacigi DPI baglami acikca PER_MONITOR_AWARE_V2 yapilir
    // ve sonra geri alinir; ayarlanamazsa "dpi_baglami":false yazilir (geometri dogrulanamaz).
    public static string Gozlem(uint olcumPid, uint ortuPid)
    {
        IntPtr eski = IntPtr.Zero;
        bool dpiOk = false;
        try
        {
            eski = SetThreadDpiAwarenessContext(new IntPtr(-4));
            dpiOk = eski != IntPtr.Zero;
        }
        catch (Exception) { dpiOk = false; }
        try
        {
            return GozlemIc(olcumPid, ortuPid, dpiOk);
        }
        finally
        {
            if (dpiOk)
            {
                try { SetThreadDpiAwarenessContext(eski); } catch (Exception) { }
            }
        }
    }

    private static string GozlemIc(uint olcumPid, uint ortuPid, bool dpiOk)
    {
        List<IntPtr> olcum = Pencereler(olcumPid);
        List<IntPtr> ortu = Pencereler(ortuPid);
        IntPtr ana = AnaPencere(olcum);
        IntPtr ortuAna = AnaPencere(ortu);
        IntPtr on = GetForegroundWindow();
        StringBuilder sb = new StringBuilder("{");
        sb.Append("\"olcum_pid\":" + olcumPid.ToString(CultureInfo.InvariantCulture));
        sb.Append(",\"ortu_pid\":" + ortuPid.ToString(CultureInfo.InvariantCulture));
        sb.Append(",\"dpi_baglami\":" + B(dpiOk));
        sb.Append(",\"on_plan_sahibi\":\"" + Sahip(on, olcumPid, ortuPid) + "\"");
        sb.Append(",\"on_plan_sinif\":\"" + (on == IntPtr.Zero ? "" : Kacis(Sinif(on))) + "\"");
        sb.Append(",\"ana\":" + (ana == IntPtr.Zero ? "null" : PencereJson(ana)));
        sb.Append(",\"ortu_ana\":" + (ortuAna == IntPtr.Zero ? "null" : PencereJson(ortuAna)));

        // Tam ortulme geometrisi: olcum penceresinin dis cercevesi ortunun dis cercevesi icinde mi
        // ve ortu Z sirasinda ustte mi. Ayni (per-monitor) koordinat duzeninde.
        if (ortuPid != 0)
        {
            RECT a;
            RECT o;
            bool aOk = DisCerceve(ana, out a);
            bool oOk = DisCerceve(ortuAna, out o);
            bool kapsiyor = aOk && oOk && o.Left <= a.Left && o.Top <= a.Top && o.Right >= a.Right && o.Bottom >= a.Bottom;
            bool ustte = Ustunde(ortuAna, ana);
            bool dogrulanabilir = dpiOk && aOk && oOk;
            sb.Append(string.Format(CultureInfo.InvariantCulture,
                ",\"geometri\":{{\"dogrulanabilir\":{0},\"kapsiyor\":{1},\"ortu_ustte\":{2},\"olcum_dis\":{3},\"ortu_dis\":{4}}}",
                B(dogrulanabilir), B(kapsiyor), B(ustte), aOk ? RectJson(a) : "null", oOk ? RectJson(o) : "null"));
        }
        else
        {
            sb.Append(",\"geometri\":null");
        }

        // Ek gozlem: olcum penceresinin istemci alanindaki 5 noktada en ustteki pencerenin sahibi.
        int nOlcum = 0, nOrtu = 0, nDiger = 0, nToplam = 0;
        StringBuilder nokta = new StringBuilder("[");
        if (ana != IntPtr.Zero && !IsIconic(ana))
        {
            RECT c;
            if (GetClientRect(ana, out c) && c.Right > 0 && c.Bottom > 0)
            {
                double[,] oran = new double[,] { { 0.5, 0.5 }, { 0.1, 0.1 }, { 0.9, 0.1 }, { 0.1, 0.9 }, { 0.9, 0.9 } };
                for (int i = 0; i < 5; i++)
                {
                    POINT p = new POINT();
                    p.X = (int)(c.Right * oran[i, 0]);
                    p.Y = (int)(c.Bottom * oran[i, 1]);
                    ClientToScreen(ana, ref p);
                    IntPtr w = WindowFromPoint(p);
                    IntPtr kok = w == IntPtr.Zero ? IntPtr.Zero : GetAncestor(w, GA_ROOT);
                    string sahip = Sahip(kok, olcumPid, ortuPid);
                    nToplam++;
                    if (sahip == "olcum") { nOlcum++; }
                    else if (sahip == "ortu") { nOrtu++; }
                    else { nDiger++; }
                    if (i > 0) { nokta.Append(","); }
                    nokta.Append(string.Format(CultureInfo.InvariantCulture, "{{\"x\":{0},\"y\":{1},\"sahip\":\"{2}\"}}", p.X, p.Y, sahip));
                }
            }
        }
        nokta.Append("]");
        sb.Append(",\"noktalar\":" + nokta.ToString());
        sb.Append(string.Format(CultureInfo.InvariantCulture, ",\"nokta_toplam\":{0},\"nokta_olcum\":{1},\"nokta_ortu\":{2},\"nokta_diger\":{3}", nToplam, nOlcum, nOrtu, nDiger));
        sb.Append(",\"olcum_pencereleri\":" + ListeJson(olcum));
        sb.Append(",\"ortu_pencereleri\":" + ListeJson(ortu));
        sb.Append("}");
        return sb.ToString();
    }
}
'@

# ---------------------------------------------------------------------------
# Akis
# ---------------------------------------------------------------------------
$genelBaslangic = Get-Date
Yaz-Gunluk ('IDA ' + $PaketSurumu + ' basliyor. Kuru calistirma: ' + [bool]$KuruCalistir)
if (-not $KuruCalistir) {
    $script:SureSiniri = $genelBaslangic.AddMinutes($KureselSinirDk)
    Yaz-Gunluk ('kuresel sure siniri: ' + $KureselSinirDk + ' dk (kullanici beklemesi ve indirme suresi eklenir)')
}

try {
    Adim 'On denetim' {
        Yaz-Gunluk ('PowerShell ' + $PSVersionTable.PSVersion + ' (' + $PSVersionTable.PSEdition + '); Windows: ' + $WindowsMu)
        Yaz-Gunluk ('dil kipi: ' + $ExecutionContext.SessionState.LanguageMode)
        Yaz-Gunluk ('betik dizini: ' + $BetikDizini)
        Yaz-Gunluk ('Godot projesi: ' + $ProjeDizini)
        Yaz-Gunluk ('Godot onbellegi: ' + $GodotOnbellek)
        Yaz-Gunluk ('parametreler: Sure=' + $Sure + ' OkumaSure=' + $OkumaSure + ' Isinma=' + $Isinma + ' Durumlar=' + ($Durumlar -join ',') + ' Cizim=' + $Cizim + ' GodotExe=' + $GodotExe + ' Sorma=' + [bool]$Sorma)
        if (-not (Test-Path -LiteralPath (Join-Path $ProjeDizini 'project.godot'))) {
            throw ('Godot projesi bulunamadi: ' + $ProjeDizini + ' (tools/olcum/mp6/proje gerekli; depo klasorunun tamami kopyalanmali)')
        }
        if (-not $WindowsMu) { Yaz-Gunluk 'Windows degil: Windows a ozgu adimlar atlanacak (yalniz Mac uzerinde deneme icin).' 'UYARI' }
        if (-not $Durumlar -or $Durumlar[0] -ne 'gorunur') {
            Yaz-Gunluk 'UYARI: ilk durum gorunur degil; gorunur referans olmadan H3 hesaplanamaz.' 'UYARI'
        }
    }

    if ($KuruCalistir) {
        Adim 'Cikti klasoru (kuru)' {
            $k = $CiktiKok
            if (-not $k) { $k = [Environment]::GetFolderPath('Desktop') }
            if (-not $k -or -not (Test-Path -LiteralPath $k)) { $k = $BetikDizini }
            Yaz-Gunluk ('KURU: cikti klasoru olusturulacak: ' + (Join-Path $k $KlasorAdi))
            Yaz-Gunluk ('KURU: zip olusturulacak: ' + (Join-Path $k ($KlasorAdi + '.zip')))
            if ($k -match 'OneDrive') { Yaz-Gunluk 'not: cikti klasoru OneDrive altinda; esitleme olcum sirasinda arka plan yuku olusturabilir (kayda gecer).' 'UYARI' }
            $script:Kok = Join-Path $k $KlasorAdi
        }
    } else {
        Adim 'Cikti klasoru' {
            $k = $CiktiKok
            if (-not $k) { $k = [Environment]::GetFolderPath('Desktop') }
            if (-not $k -or -not (Test-Path -LiteralPath $k)) { $k = $BetikDizini }
            $yol = Join-Path $k $KlasorAdi
            New-Item -ItemType Directory -Force -Path $yol | Out-Null
            $script:Kok = (Resolve-Path -LiteralPath $yol).ProviderPath
            $script:GunlukYolu = Join-Path $script:Kok 'betik.log'
            [System.IO.File]::WriteAllText($script:GunlukYolu, (($script:GunlukTampon.ToArray()) -join "`r`n") + "`r`n", $script:Utf8)
            $script:GunlukTampon.Clear()
            Yaz-Gunluk ('cikti klasoru: ' + $script:Kok)
            if ($k -match 'OneDrive') { Yaz-Gunluk 'not: cikti klasoru OneDrive altinda; esitleme olcum sirasinda arka plan yuku olusturabilir.' 'UYARI' }
        }
    }

    Adim 'Donanim kaydi' -Sinirli {
        if ($KuruCalistir) {
            Yaz-Gunluk ('KURU: donanim kaydi yazilacak: ' + (Join-Path $script:Kok 'donanim.txt') + ' ve donanim.json')
            Yaz-Gunluk 'KURU: kaynaklar: Win32_Processor, Win32_ComputerSystem, Win32_PhysicalMemory, Win32_OperatingSystem, Win32_VideoController, System.Windows.Forms.Screen, SystemInformation.PowerStatus, Win32_Battery, powercfg /getactivescheme, Win32_Desktop'
            return
        }
        $d = [ordered]@{ zaman = (Zaman); windows = $WindowsMu; hatalar = @() }
        if (-not $WindowsMu) {
            $d.not = 'Windows degil; donanim kaydi yalniz Windows ta yapilir.'
            Json-Yaz (Join-Path $script:Kok 'donanim.json') $d
            Yaz-Metin (Join-Path $script:Kok 'donanim.txt') ("Windows degil; donanim kaydi yapilmadi.`r`n")
            Isaretle 'ATLANDI' 'Windows degil'
            return
        }
        $hatalar = New-Object System.Collections.Generic.List[string]
        # Islemci
        try {
            $d.islemci = @(Cim-Al 'Win32_Processor' | ForEach-Object {
                [ordered]@{ ad = ([string]$_.Name).Trim(); uretici = $_.Manufacturer; tanim = $_.Description
                    cekirdek = $_.NumberOfCores; mantiksal_islemci = $_.NumberOfLogicalProcessors
                    azami_mhz = $_.MaxClockSpeed; l2_kb = $_.L2CacheSize; l3_kb = $_.L3CacheSize; soket = $_.SocketDesignation }
            })
        } catch { $hatalar.Add('islemci: ' + $_.Exception.Message) }
        $d.islemci_ortam = [ordered]@{ PROCESSOR_IDENTIFIER = $env:PROCESSOR_IDENTIFIER; ProcessorCount = [Environment]::ProcessorCount; is64 = [Environment]::Is64BitOperatingSystem }
        # Bilgisayar ve bellek
        try {
            $cs = @(Cim-Al 'Win32_ComputerSystem')[0]
            $d.bilgisayar = [ordered]@{ uretici = $cs.Manufacturer; model = $cs.Model; sistem_turu = $cs.SystemType; toplam_bellek_bayt = $cs.TotalPhysicalMemory }
        } catch { $hatalar.Add('bilgisayar: ' + $_.Exception.Message) }
        try {
            $d.bellek_modulleri = @(Cim-Al 'Win32_PhysicalMemory' | ForEach-Object {
                [ordered]@{ kapasite_bayt = $_.Capacity; hiz_mts = $_.Speed; yapilandirilmis_hiz = $_.ConfiguredClockSpeed
                    uretici = $_.Manufacturer; parca_no = ([string]$_.PartNumber).Trim(); yuva = $_.DeviceLocator; smbios_tur = $_.SMBIOSMemoryType }
            })
        } catch { $hatalar.Add('bellek modulleri: ' + $_.Exception.Message) }
        # Isletim sistemi
        try {
            $os = @(Cim-Al 'Win32_OperatingSystem')[0]
            $d.windows_surumu = [ordered]@{ ad = $os.Caption; surum = $os.Version; derleme = $os.BuildNumber; mimari = $os.OSArchitecture
                dil = $os.OSLanguage; son_acilis = [string]$os.LastBootUpTime; bos_bellek_kb = $os.FreePhysicalMemory; toplam_gorunur_bellek_kb = $os.TotalVisibleMemorySize }
        } catch { $hatalar.Add('isletim sistemi: ' + $_.Exception.Message) }
        $d.os_ortam = [string][Environment]::OSVersion
        # Ekran karti
        try {
            $d.ekran_karti = @(Cim-Al 'Win32_VideoController' | ForEach-Object {
                [ordered]@{ ad = $_.Name; surucu_surum = $_.DriverVersion; surucu_tarih = [string]$_.DriverDate
                    video_islemci = $_.VideoProcessor; bellek_bayt_wmi = $_.AdapterRAM
                    cozunurluk_yatay = $_.CurrentHorizontalResolution; cozunurluk_dikey = $_.CurrentVerticalResolution
                    yenileme_hz = $_.CurrentRefreshRate; bit = $_.CurrentBitsPerPixel; durum = $_.Status; pnp = $_.PNPDeviceID }
            })
            $d.ekran_karti_notu = 'bellek_bayt_wmi 32 bitlik alandir (4 GB ustunu gostermez); dahili kartta paylasilan bellegi yansitmaz.'
        } catch { $hatalar.Add('ekran karti: ' + $_.Exception.Message) }
        # Ekranlar (mantiksal koordinatlar; olcekleme etkisi altinda olabilir)
        try {
            Add-Type -AssemblyName System.Windows.Forms
            $d.ekranlar = @([System.Windows.Forms.Screen]::AllScreens | ForEach-Object {
                [ordered]@{ ad = $_.DeviceName; birincil = $_.Primary; x = $_.Bounds.X; y = $_.Bounds.Y; genislik = $_.Bounds.Width; yukseklik = $_.Bounds.Height
                    calisma_alani = ('' + $_.WorkingArea.Width + 'x' + $_.WorkingArea.Height); bit = $_.BitsPerPixel }
            })
            $ps = [System.Windows.Forms.SystemInformation]::PowerStatus
            $d.guc = [ordered]@{ kaynak = [string]$ps.PowerLineStatus; pil_durumu = [string]$ps.BatteryChargeStatus; pil_yuzde = $ps.BatteryLifePercent; pil_kalan_s = $ps.BatteryLifeRemaining }
        } catch { $hatalar.Add('ekranlar/guc: ' + $_.Exception.Message) }
        try {
            $d.piller = @(Cim-Al 'Win32_Battery' | ForEach-Object { [ordered]@{ ad = $_.Name; durum_kodu = $_.BatteryStatus; yuzde = $_.EstimatedChargeRemaining } })
        } catch { $hatalar.Add('pil: ' + $_.Exception.Message) }
        # Guc plani (yalniz okunur sorgu). Cikis kodu ayrica kaydedilir; hata metni veri sayilmaz.
        try {
            $pcSatirlar = @(& powercfg.exe /getactivescheme)
            $pcKod = $LASTEXITCODE
            $d.guc_plani_cikis_kodu = $pcKod
            $pc = (($pcSatirlar | ForEach-Object { [string]$_ }) -join ' ').Trim()
            $d.guc_plani_ham = $pc
            $m = [regex]::Match($pc, '([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})\s*\(([^)]*)\)')
            if (($pcKod -eq 0) -and $m.Success) {
                $d.guc_plani = [ordered]@{ guid = $m.Groups[1].Value; ad = $m.Groups[2].Value }
            } else {
                $hatalar.Add('guc plani: powercfg cikis kodu ' + $pcKod + ', GUID bulundu: ' + $m.Success)
            }
        } catch { $hatalar.Add('guc plani: ' + $_.Exception.Message) }
        # Ekran koruyucu (yalniz okunur; kullanici/bilgisayar adi yazilmaz)
        try {
            $d.ekran_koruyucu = @(Cim-Al 'Win32_Desktop' | Where-Object { $_.Name -like ('*' + $env:USERNAME) } | ForEach-Object {
                [ordered]@{ etkin = $_.ScreenSaverActive; zaman_asimi_s = $_.ScreenSaverTimeout; parola = $_.ScreenSaverSecure }
            })
        } catch { $hatalar.Add('ekran koruyucu: ' + $_.Exception.Message) }
        try {
            $dsk = Get-PSDrive -Name ($script:Kok.Substring(0, 1)) -ErrorAction Stop
            $d.cikti_diski_bos_gb = [Math]::Round($dsk.Free / 1GB, 1)
        } catch { $hatalar.Add('disk: ' + $_.Exception.Message) }
        $d.ortam_yuku = Ortam-Yuku
        $d.hatalar = $hatalar.ToArray()
        Json-Yaz (Join-Path $script:Kok 'donanim.json') $d

        # Insanca okunur ozet
        $t = New-Object System.Collections.Generic.List[string]
        $t.Add('IDA W0-MP8a donanim kaydi  ' + $d.zaman)
        foreach ($c in @($d.islemci)) { if ($c) { $t.Add('Islemci      : ' + $c.ad + ' | cekirdek ' + $c.cekirdek + ' | is parcacigi ' + $c.mantiksal_islemci + ' | azami ' + $c.azami_mhz + ' MHz') } }
        if ($d.bilgisayar) { $t.Add('Bilgisayar   : ' + $d.bilgisayar.uretici + ' ' + $d.bilgisayar.model + ' | bellek ' + [Math]::Round([double]$d.bilgisayar.toplam_bellek_bayt / 1GB, 2) + ' GB') }
        foreach ($b in @($d.bellek_modulleri)) { if ($b) { $t.Add('Bellek modulu: ' + [Math]::Round([double]$b.kapasite_bayt / 1GB, 1) + ' GB ' + $b.hiz_mts + ' MT/s ' + $b.uretici + ' ' + $b.parca_no + ' (' + $b.yuva + ')') } }
        foreach ($g in @($d.ekran_karti)) { if ($g) { $t.Add('Ekran karti  : ' + $g.ad + ' | surucu ' + $g.surucu_surum + ' (' + $g.surucu_tarih + ') | ' + $g.cozunurluk_yatay + 'x' + $g.cozunurluk_dikey + ' @ ' + $g.yenileme_hz + ' Hz') } }
        foreach ($e in @($d.ekranlar)) { if ($e) { $t.Add('Ekran        : ' + $e.ad + ' ' + $e.genislik + 'x' + $e.yukseklik + ' (mantiksal) birincil=' + $e.birincil) } }
        if ($d.windows_surumu) { $t.Add('Windows      : ' + $d.windows_surumu.ad + ' ' + $d.windows_surumu.surum + ' derleme ' + $d.windows_surumu.derleme + ' ' + $d.windows_surumu.mimari) }
        if ($d.guc) { $t.Add('Guc          : kaynak ' + $d.guc.kaynak + ' | pil ' + $d.guc.pil_durumu + ' ' + $d.guc.pil_yuzde) }
        if ($d.guc_plani) { $t.Add('Guc plani    : ' + $d.guc_plani.ad + ' (' + $d.guc_plani.guid + ')') } else { $t.Add('Guc plani    : ' + $d.guc_plani_ham) }
        foreach ($k in @($d.ekran_koruyucu)) { if ($k) { $t.Add('Ekran koruyucu: etkin=' + $k.etkin + ' zaman_asimi_s=' + $k.zaman_asimi_s) } }
        $t.Add('Ortam yuku   : ' + $d.ortam_yuku)
        foreach ($h in $d.hatalar) { $t.Add('HATA         : ' + $h) }
        Yaz-Metin (Join-Path $script:Kok 'donanim.txt') (($t.ToArray() -join "`r`n") + "`r`n")
        foreach ($x in $t) { Yaz-Gunluk ('donanim: ' + $x) }
        if ($hatalar.Count -gt 0) { Isaretle 'KISMI' ('donanim kaydinda ' + $hatalar.Count + ' alt adim basarisiz (donanim.json hatalar alani)') }
    }

    Adim 'Guc ve ekran denetimi' -Sinirli {
        if ($KuruCalistir) { Yaz-Gunluk 'KURU: prize takili degilse uyari verilip Enter beklenecek; ekran koruyucu acik ise uyari yazilacak.'; return }
        if (-not $WindowsMu) { Isaretle 'ATLANDI' 'Windows degil'; return }
        Add-Type -AssemblyName System.Windows.Forms
        $durum = [string][System.Windows.Forms.SystemInformation]::PowerStatus.PowerLineStatus
        Yaz-Gunluk ('guc kaynagi: ' + $durum)
        if ($durum -ne 'Online') {
            Yaz-Gunluk 'Bilgisayar prize takili gorunmuyor. Lutfen sarj aletini takin.' 'UYARI'
            if (-not $Sorma) {
                $bekleme = [System.Diagnostics.Stopwatch]::StartNew()
                [void](Read-Host 'Sarj aletini takip Enter a basin (takilamiyorsa yine Enter; durum kayda gecer)')
                Siniri-Ertele $bekleme.Elapsed.TotalSeconds
                $durum =[string][System.Windows.Forms.SystemInformation]::PowerStatus.PowerLineStatus
                Yaz-Gunluk ('guc kaynagi (yeniden): ' + $durum)
            }
            if ($durum -ne 'Online') { Isaretle 'UYARI' ('olcum prize takili olmadan yapiliyor: ' + $durum) }
        }
        try {
            $kor = @(Cim-Al 'Win32_Desktop' | Where-Object { $_.Name -like ('*' + $env:USERNAME) })
            foreach ($k in $kor) {
                if ($k.ScreenSaverActive -and [int]$k.ScreenSaverTimeout -gt 0 -and [int]$k.ScreenSaverTimeout -le 900) {
                    Yaz-Gunluk ('Ekran koruyucu acik (' + $k.ScreenSaverTimeout + ' s). Olcum sirasinda devreye girebilir; ayar degistirilmiyor, durum kayda gecer.') 'UYARI'
                }
            }
        } catch { Yaz-Gunluk ('ekran koruyucu okunamadi: ' + $_.Exception.Message) 'UYARI' }
    }

    Adim 'Godot ikilisi' -Sinirli {
        if ($GodotExe) {
            if ($KuruCalistir) { Yaz-Gunluk ('KURU: -GodotExe kullanilacak (indirme ve ozet denetimi atlanir): ' + $GodotExe); $script:Exe = $GodotExe; return }
            if (-not (Test-Path -LiteralPath $GodotExe)) { throw ('-GodotExe bulunamadi: ' + $GodotExe) }
            $script:Exe = (Resolve-Path -LiteralPath $GodotExe).ProviderPath
            $script:GodotBilgi.kaynak = 'parametre (-GodotExe)'
            $script:GodotBilgi.exe = $script:Exe
            $script:GodotBilgi.ozet_denetimi = 'ATLANDI (-GodotExe)'
            Isaretle 'UYARI' ('-GodotExe verildi; indirme ve ozet denetimi ATLANDI: ' + $script:Exe)
            return
        }
        $zip = Join-Path $GodotOnbellek $ZipAdi
        $appDizin = Join-Path $GodotOnbellek 'app'
        $exe = Join-Path $appDizin $ExeAdi
        if ($KuruCalistir) {
            Yaz-Gunluk ('KURU: indirme adresi: ' + $IndirmeUrl)
            Yaz-Gunluk ('KURU: onbellek zip: ' + $zip + ' (var: ' + (Test-Path -LiteralPath $zip) + ')')
            Yaz-Gunluk ('KURU: beklenen SHA-512: ' + $BeklenenSha512)
            Yaz-Gunluk ('KURU: beklenen SHA-256: ' + $BeklenenSha256)
            Yaz-Gunluk ('KURU: acilacak ikili: ' + $exe)
            $script:Exe = $exe
            return
        }
        if (-not $WindowsMu) { throw 'Windows degil ve -GodotExe verilmedi: Windows ikilisi burada calismaz.' }
        New-Item -ItemType Directory -Force -Path $GodotOnbellek | Out-Null
        $script:GodotBilgi.kaynak = $IndirmeUrl
        $script:GodotBilgi.zip = $zip
        $oz = $null
        if (Test-Path -LiteralPath $zip) {
            Yaz-Gunluk ('onbellekte zip var, yeniden indirilmeyecek: ' + $zip)
            $oz = Ozet-Hesapla $zip
            if ((Ozet-Uyuyor $oz) -eq $false) {
                $bozuk = $zip + '.uyusmayan-' + $ZamanDamgasi
                Yaz-Gunluk ('onbellekteki zip ozeti uyusmuyor; kenara aliniyor: ' + $bozuk) 'UYARI'
                Move-Item -LiteralPath $zip -Destination $bozuk
                $oz = $null
            }
        }
        if (-not (Test-Path -LiteralPath $zip)) {
            $parca = $zip + '.part'
            if (Test-Path -LiteralPath $parca) { Remove-Item -LiteralPath $parca -Force }
            Yaz-Gunluk ('indiriliyor (yaklasik 86 MB): ' + $IndirmeUrl)
            $indirme = [System.Diagnostics.Stopwatch]::StartNew()
            try {
                Indir $IndirmeUrl $parca
            } finally {
                Siniri-Ertele $indirme.Elapsed.TotalSeconds
                Yaz-Gunluk ('indirme suresi: ' + [Math]::Round($indirme.Elapsed.TotalSeconds) + ' s (kuresel sinira eklendi)')
            }
            Move-Item -LiteralPath $parca -Destination $zip
            $oz = Ozet-Hesapla $zip
        }
        $script:GodotBilgi.sha512 = $oz.sha512
        $script:GodotBilgi.sha256 = $oz.sha256
        Yaz-Gunluk ('SHA-512: ' + $oz.sha512)
        Yaz-Gunluk ('SHA-256: ' + $oz.sha256)
        $uyum = Ozet-Uyuyor $oz
        if ($null -eq $uyum) {
            $script:GodotBilgi.ozet_denetimi = 'BILINMIYOR'
            Yaz-Gunluk 'Beklenen ozet bu betikte yok (bilinmiyor). Yukaridaki ozetler kayda gecti.' 'UYARI'
            if ($Sorma) { throw 'ozet bilinmiyor ve -Sorma verildi: Godot calistirilmayacak' }
            $bekleme = [System.Diagnostics.Stopwatch]::StartNew()
            $c = Read-Host 'Godot yine de calistirilsin mi? Evet icin E yazip Enter'
            Siniri-Ertele $bekleme.Elapsed.TotalSeconds
            if ($c -ne 'E' -and $c -ne 'e') { throw 'kullanici onaylamadi: Godot calistirilmayacak' }
            $script:GodotBilgi.ozet_denetimi = 'BILINMIYOR (kullanici onayladi)'
        } elseif (-not $uyum) {
            $script:GodotBilgi.ozet_denetimi = 'UYUSMADI'
            throw ('indirilen dosyanin ozeti beklenenle uyusmuyor; Godot calistirilmayacak. Dosya: ' + $zip)
        } else {
            $script:GodotBilgi.ozet_denetimi = 'UYUSTU (SHA-512 ve SHA-256)'
            Yaz-Gunluk 'ozet denetimi: SHA-512 ve SHA-256 beklenen degerlerle ayni.'
        }
        $script:Exe = $exe
        if (Test-Path -LiteralPath $appDizin) {
            try {
                Remove-Item -LiteralPath $appDizin -Recurse -Force
            } catch {
                throw ('onceki Godot dizini silinemedi (onceki bir calistirmadan kalan Godot penceresi acik olabilir; kapatip yeniden calistirin): ' + $_.Exception.Message)
            }
        }
        New-Item -ItemType Directory -Force -Path $appDizin | Out-Null
        try {
            Add-Type -AssemblyName System.IO.Compression
            Add-Type -AssemblyName System.IO.Compression.FileSystem
            [System.IO.Compression.ZipFile]::ExtractToDirectory($zip, $appDizin)
        } catch {
            Yaz-Gunluk ('ZipFile ile acilamadi: ' + $_.Exception.Message + ' ; Expand-Archive deneniyor') 'UYARI'
            Expand-Archive -LiteralPath $zip -DestinationPath $appDizin -Force
        }
        if (-not (Test-Path -LiteralPath $exe)) { throw ('zip acildi ama ikili yok: ' + $exe) }
        $script:GodotBilgi.exe = $exe
        try {
            $imza = Get-AuthenticodeSignature -LiteralPath $exe
            $script:GodotBilgi.imza = ([string]$imza.Status + ' ' + [string]$imza.SignerCertificate.Subject)
            Yaz-Gunluk ('ikili imzasi (yalniz bilgi): ' + $script:GodotBilgi.imza)
        } catch { }
        Yaz-Gunluk ('Godot hazir: ' + $exe)
    }

    Adim 'Pencere gozlem yardimcisi (user32)' -Sinirli {
        if ($KuruCalistir) { Yaz-Gunluk 'KURU: C# yardimcisi Add-Type ile derlenecek (EnumWindows, IsIconic, IsWindowVisible, WindowFromPoint, GetForegroundWindow, DwmGetWindowAttribute, SetThreadExecutionState).'; return }
        if (-not $WindowsMu) { Isaretle 'ATLANDI' 'Windows degil: dis gozlem yok'; return }
        if ([string]$ExecutionContext.SessionState.LanguageMode -ne 'FullLanguage') { throw ('dil kipi ' + $ExecutionContext.SessionState.LanguageMode + ': Add-Type kullanilamaz, dis gozlem yok') }
        if (-not ('IdaMp8W32' -as [type])) { Add-Type -TypeDefinition $W32Kaynak -Language CSharp }
        $deneme = [IdaMp8W32]::Gozlem([uint32]$PID, [uint32]0)
        Yaz-Gunluk ('yardimci calisiyor; ornek gozlem uzunlugu ' + $deneme.Length)
        $script:W32Hazir = $true
    }

    Adim 'Uyku engeli' -Sinirli {
        if ($KuruCalistir) { Yaz-Gunluk 'KURU: SetThreadExecutionState(ES_CONTINUOUS|ES_SYSTEM_REQUIRED|ES_DISPLAY_REQUIRED) cagrilacak; ayar degismez, betik bitince geri alinir.'; return }
        if (-not $script:W32Hazir) { Isaretle 'ATLANDI' 'yardimci yok'; return }
        $r = [IdaMp8W32]::UykuEngelle()
        if ($r -eq 0) { Isaretle 'UYARI' 'SetThreadExecutionState basarisiz dondu' } else { $script:UykuEngellendi = $true; Yaz-Gunluk 'uyku ve ekran kapanmasi betik suresince erteleniyor (ayar degismedi).' }
    }

    Adim 'Cizim yontemi denemesi' -Sinirli {
        $sira = @('varsayilan', 'mobile', 'opengl3')
        if ($Cizim) {
            $script:SecilenCizim = $Cizim
            Isaretle 'ATLANDI' ('-Cizim ' + $Cizim + ' verildi; deneme yapilmadan bu yontem kullanilacak')
            return
        }
        $i = 0
        foreach ($ad in $sira) {
            $i++
            $klasor = Join-Path $script:Kok ('deneme_' + $i + '_' + $ad)
            $carg = $CizimSecenekleri[$ad]
            if ($KuruCalistir) {
                Yaz-Gunluk ('KURU: deneme ' + $i + ' (' + $ad + '): ' + $script:Exe + ' ' + (Arglari-Birlestir (Olcum-Arglari $klasor 'gorunur' $DenemeSure $DenemeIsinma $carg @('--ornek=0'))))
                continue
            }
            if (-not (Sure-Yeter ($DenemeSure + $DenemeIsinma + 60))) {
                Yaz-Gunluk ('kuresel sure siniri (' + $KureselSinirDk + ' dk): deneme ' + $ad + ' baslatilmadi') 'HATA'
                break
            }
            Yaz-Gunluk ('deneme ' + $i + ': ' + $ad + ' (gorunur, ' + $DenemeSure + ' s)')
            $r = Kos-Godot -Ad ('deneme_' + $ad) -Klasor $klasor -Durum 'gorunur' -SureS $DenemeSure -IsinmaS $DenemeIsinma -CizimAd $ad -CizimArg $carg -EkArg @('--ornek=0') -HazirAsimS $HazirAsimiDeneme
            $script:Denemeler.Add($r)
            if ($r.ozet_var -and $r.surec_temiz -and ($r.uretildi -gt 0)) {
                $script:SecilenCizim = $ad
                Yaz-Gunluk ('cizim yontemi secildi: ' + $ad + ' (bant: ' + ($r.bant -join ' / ') + ')')
                break
            }
            Yaz-Gunluk ('deneme ' + $ad + ' basarisiz: uretildi=' + $r.uretildi + ' duzenek=' + $r.duzenek + ' ' + $r.hata + ' ; sonraki yontem denenecek') 'UYARI'
            Start-Sleep -Seconds 2
        }
        if ($KuruCalistir) { $script:SecilenCizim = 'varsayilan'; Yaz-Gunluk 'KURU: ilk basarili deneme kampanyada kullanilir (asagida varsayilan ile gosteriliyor).'; return }
        if (-not $script:SecilenCizim) { throw 'hicbir cizim yontemiyle kare uretilemedi; kampanya kosulmayacak' }
        if ($script:SecilenCizim -ne 'varsayilan') { Isaretle 'UYARI' ('varsayilan cizim yontemi calismadi; ' + $script:SecilenCizim + ' kullaniliyor') }
    }

    # Kampanya: yol b; durum kosulari ve gorunur pencerede esz. geri okuma kosusu.
    $plan = New-Object System.Collections.Generic.List[object]
    foreach ($d in $Durumlar) {
        $plan.Add(@{ ad = ('b_' + $d); durum = $d; sure = $Sure; ek = @('--ornek=' + $KareOrnek) })
    }
    if ($OkumaSure -gt 0) {
        $plan.Add(@{ ad = 'okuma_640x480_esz'; durum = 'gorunur'; sure = $OkumaSure; ek = @('--geri=esz', '--cift=1', '--ornek=0') })
    }
    $script:KampanyaBaslangic = Get-Date
    foreach ($k in $plan) {
        Adim ('Kampanya: ' + $k.ad) -Sinirli {
            if (-not $script:SecilenCizim) { Isaretle 'ATLANDI' 'cizim yontemi yok'; return }
            $kd = Join-Path $script:Kok ('kampanya_' + $script:SecilenCizim)
            $klasor = Join-Path $kd $k.ad
            $carg = $CizimSecenekleri[$script:SecilenCizim]
            if ($KuruCalistir) {
                Yaz-Gunluk ('KURU: ' + $k.ad + ' (' + $k.sure + ' s): ' + $script:Exe + ' ' + (Arglari-Birlestir (Olcum-Arglari $klasor $k.durum $k.sure $Isinma $carg $k.ek)))
                if ($k.durum -eq 'ortulu') { Yaz-Gunluk ('KURU: ortu: ' + $script:Exe + ' ' + (Arglari-Birlestir (Ortu-Arglari $klasor $k.sure $carg))) }
                if ($k.durum -eq 'simge') { Yaz-Gunluk 'KURU: simge durumunu Godot projesi kendisi kurar (window_set_mode MINIMIZED); dis gozlem IsIconic ile.' }
                Yaz-Gunluk ('KURU: dis gozlem her ' + $GozlemAraligi + ' s: ' + (Join-Path $klasor 'pencere_win.jsonl') + ', surec ornegi: surec_win.csv')
                return
            }
            if (-not (Sure-Yeter ($k.sure + $Isinma + $GecisAzami + 60))) {
                Isaretle 'ATLANDI' ('kuresel sure siniri (' + $KureselSinirDk + ' dk): kosu baslatilmadi')
                return
            }
            $r = Kos-Godot -Ad $k.ad -Klasor $klasor -Durum $k.durum -SureS $k.sure -IsinmaS $Isinma -CizimAd $script:SecilenCizim -CizimArg $carg -EkArg $k.ek -HazirAsimS $HazirAsimiKampanya
            $script:Kosular.Add($r)
            if (-not $r.ozet_var) { throw ($k.ad + ': olcum yazilmadi: ' + $r.hata + ' (duzenek: ' + $r.duzenek + ')') }
            if (-not $r.surec_temiz) { Isaretle 'HATA' ('olcum yazildi ama surec temiz bitmedi; duzenek ' + $r.duzenek) }
            Start-Sleep -Seconds 3
        }
    }
    $script:KampanyaBitis = Get-Date

    Adim 'Kare dokumu boyut denetimi' -Sinirli {
        if ($KuruCalistir) { Yaz-Gunluk ('KURU: ham kare dokumleri (*.rgb, *.rgba) toplami ' + ($DokumSiniriBayt / 1MB) + ' MB ile sinirlanir; asarsa her kosuda tek sirali dokumler silinir.'); return }
        $dosyalar = @(Dokumler)
        $toplam = 0
        foreach ($f in $dosyalar) { $toplam += $f.Length }
        Yaz-Gunluk ('ham kare dokumu: ' + $dosyalar.Count + ' dosya, ' + [Math]::Round($toplam / 1MB, 1) + ' MB')
        $tur = 0
        while ($toplam -gt $DokumSiniriBayt -and $tur -lt 5) {
            $tur++
            foreach ($grup in ($dosyalar | Group-Object DirectoryName)) {
                $sirali = @($grup.Group | Sort-Object Name)
                for ($j = 1; $j -lt $sirali.Count; $j += 2) { Remove-Item -LiteralPath $sirali[$j].FullName -Force }
            }
            $dosyalar = @(Dokumler)
            $toplam = 0
            foreach ($f in $dosyalar) { $toplam += $f.Length }
            Isaretle 'UYARI' ('dokumler seyreltildi (tur ' + $tur + '): ' + $dosyalar.Count + ' dosya, ' + [Math]::Round($toplam / 1MB, 1) + ' MB')
        }
    }
} finally {
    # Yalniz bu calistirmanin kendi baslattigi surecler; kuru calistirmada hicbir surece dokunulmaz.
    if (-not $KuruCalistir) {
        foreach ($p in $script:AcikSurecler) { $null = Sureci-Kapat $p 'betik sonu' }
    }
    if ($script:UykuEngellendi) {
        try { [void][IdaMp8W32]::UykuSerbest(); Yaz-Gunluk 'uyku engeli kaldirildi.' } catch { Yaz-Gunluk ('uyku engeli kaldirilamadi: ' + $_.Exception.Message) 'UYARI' }
    }
}

Adim 'Ozet' {
    $hataSay = @($script:AdimSonuclari | Where-Object { $_.durum -eq 'HATA' }).Count
    if ($KuruCalistir) {
        Yaz-Gunluk ('KURU: ' + (Join-Path $script:Kok 'sonuc.json') + ' ve sonuc.txt yazilacak.')
        return
    }
    $ks = $null
    if ($script:KampanyaBaslangic -and $script:KampanyaBitis) { $ks = [Math]::Round(($script:KampanyaBitis - $script:KampanyaBaslangic).TotalSeconds, 0) }
    $s = [ordered]@{
        paket = $PaketSurumu; baslangic = $genelBaslangic.ToString('o'); bitis = (Get-Date).ToString('o')
        toplam_s = [Math]::Round(((Get-Date) - $genelBaslangic).TotalSeconds, 0); kampanya_s = $ks
        windows = $WindowsMu; powershell = ([string]$PSVersionTable.PSVersion + ' ' + $PSVersionTable.PSEdition)
        parametreler = [ordered]@{ Sure = $Sure; OkumaSure = $OkumaSure; Isinma = $Isinma; Durumlar = $Durumlar; Cizim = $Cizim; GodotExe = $GodotExe; KareOrnek = $KareOrnek }
        godot = $script:GodotBilgi; dis_gozlem = $script:W32Hazir; uyku_engeli = $script:UykuEngellendi
        cizim_denemeleri = $script:Denemeler.ToArray(); secilen_cizim = $script:SecilenCizim
        kosular = $script:Kosular.ToArray(); adimlar = $script:AdimSonuclari.ToArray(); hatali_adim = $hataSay
        not = 'Olcum olcutleri (H1-H5) bu betikte degerlendirilmez; tools/olcum/windows/analiz_win.py ile mimar degerlendirir.'
    }
    Json-Yaz (Join-Path $script:Kok 'sonuc.json') $s
    $t = New-Object System.Collections.Generic.List[string]
    $t.Add('IDA ' + $PaketSurumu + ' sonuc  ' + (Zaman))
    $t.Add('Godot: ' + $script:GodotBilgi.exe + ' | ozet denetimi: ' + $script:GodotBilgi.ozet_denetimi)
    $t.Add('Dis gozlem (user32): ' + $script:W32Hazir + ' | secilen cizim yontemi: ' + $script:SecilenCizim)
    foreach ($r in $script:Denemeler) { $t.Add('deneme ' + $r.cizim + ': hazir=' + $r.hazir + ' uretildi=' + $r.uretildi + '/' + $r.istek + ' duzenek=' + $r.duzenek + ' bant=' + ($r.bant -join ' / ') + ' ' + $r.hata) }
    foreach ($r in $script:Kosular) { $t.Add('kosu ' + $r.ad + ': uretildi=' + $r.uretildi + '/' + $r.istek + ' ic_dogrulama=' + $r.durum_dogrulandi_ic + ' (' + $r.durum_dogrulama_ic + ') cikis=' + $r.cikis_kodu + ' duzenek=' + $r.duzenek + ' ' + $r.hata) }
    $t.Add('Not: duzenek=calismadi, olcum duzeneginin calismadigini gosterir; olcut sonucu (gecti/gecmedi) degildir.')
    $t.Add('Kampanya suresi (s): ' + $ks)
    foreach ($a in $script:AdimSonuclari) { $t.Add('adim ' + $a.durum + ' : ' + $a.ad + ' ' + $a.ileti) }
    Yaz-Metin (Join-Path $script:Kok 'sonuc.txt') (($t.ToArray() -join "`r`n") + "`r`n")
}

Adim 'Zip' {
    if ($KuruCalistir) { Yaz-Gunluk ('KURU: ' + $script:Kok + ' klasoru ' + $script:Kok + '.zip olarak sikistirilacak.'); return }
    $zipYol = $script:Kok + '.zip'
    if (Test-Path -LiteralPath $zipYol) { Remove-Item -LiteralPath $zipYol -Force }
    Yaz-Gunluk ('zip olusturuluyor: ' + $zipYol + ' (bundan sonraki gunluk satirlari zip te yoktur)')
    try {
        Add-Type -AssemblyName System.IO.Compression
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [System.IO.Compression.ZipFile]::CreateFromDirectory($script:Kok, $zipYol, [System.IO.Compression.CompressionLevel]::Optimal, $true)
    } catch {
        Yaz-Gunluk ('ZipFile basarisiz: ' + $_.Exception.Message + ' ; Compress-Archive deneniyor') 'UYARI'
        if (Test-Path -LiteralPath $zipYol) { Remove-Item -LiteralPath $zipYol -Force }
        Compress-Archive -LiteralPath $script:Kok -DestinationPath $zipYol -Force
    }
    $script:ZipYolu = $zipYol
    Yaz-Gunluk ('zip boyutu: ' + [Math]::Round((Get-Item -LiteralPath $zipYol).Length / 1MB, 1) + ' MB')
}

# ---------------------------------------------------------------------------
# Kapanis: sessizce "basarili" denmez; her adimin durumu yazilir.
# ---------------------------------------------------------------------------
$hatali = @($script:AdimSonuclari | Where-Object { $_.durum -eq 'HATA' })
$uyarili = @($script:AdimSonuclari | Where-Object { $_.durum -ne 'HATA' -and $_.durum -ne 'TAMAM' })
Write-Host ''
Write-Host '================ ADIM OZETI ================'
foreach ($a in $script:AdimSonuclari) {
    $renk = 'Gray'
    if ($a.durum -eq 'HATA') { $renk = 'Red' } elseif ($a.durum -ne 'TAMAM') { $renk = 'Yellow' }
    Write-Host ('{0,-8} {1}  {2}' -f $a.durum, $a.ad, $a.ileti) -ForegroundColor $renk
}
Write-Host '============================================'
if ($KuruCalistir) {
    Yaz-Gunluk ('KURU CALISTIRMA bitti: hicbir sey indirilmedi, Godot baslatilmadi, klasor olusturulmadi. Adim sayisi ' + $script:AdimSonuclari.Count + ', hatali ' + $hatali.Count + '.')
} else {
    Yaz-Gunluk ('Hatali adim: ' + $hatali.Count + ' ; uyarili/atlanan adim: ' + $uyarili.Count + ' ; toplam sure ' + [Math]::Round(((Get-Date) - $genelBaslangic).TotalSeconds) + ' s')
    Yaz-Gunluk 'Olcum olcutleri burada degerlendirilmez; sonuclar mimar tarafindan cozumlenecek.'
    if ($script:ZipYolu) {
        Write-Host ''
        Write-Host 'Mimara verilecek dosya:' -ForegroundColor Cyan
        Write-Host ('  ' + $script:ZipYolu) -ForegroundColor Cyan
    } else {
        Write-Host ''
        Write-Host 'Zip olusturulamadi. Mimara su klasoru verin:' -ForegroundColor Yellow
        Write-Host ('  ' + $script:Kok) -ForegroundColor Yellow
    }
}
if ($hatali.Count -gt 0) {
    Write-Host ('SONUC: ' + $hatali.Count + ' adim HATA verdi (yukaridaki ozet ve betik.log).') -ForegroundColor Red
    exit 1
}
if ($uyarili.Count -gt 0) {
    Write-Host ('SONUC: hata yok; ' + $uyarili.Count + ' adim uyari ya da atlama ile bitti (yukaridaki ozet).') -ForegroundColor Yellow
} else {
    Write-Host 'SONUC: butun adimlar hatasiz tamamlandi. Bu, olcum olcutlerinin gectigi anlamina gelmez.'
}
exit 0
