# Görselleştiriciyi tek komutta açar (Windows): sahte A2 kaynağıyla (W3-viz kapsam md. 7) ya da
# `-Sim` ile gerçek simülasyonla (W5-bagla-a2 kapsam 7). `viz/calistir.sh`'in Windows eşidir
# (W5-viz-windows); Windows PowerShell 5.1 ve PowerShell 7 ile çalışır.
#
#   viz\calistir.ps1 [-Kademe sade|tam] [-A2Port <n>] [-Cozunurluk <G>x<Y>] [-Derle]
#                    [-YalnizSahte] [-Ek <görselleştiriciye ek seçenekler>]
#   viz\calistir.ps1 -Sim <senaryo.toml> [-Hiz <çarpan>] [-Kok <dizin>] [-A2Port <n>]
#                    [-Cozunurluk <G>x<Y>] [-Derle] [-Ek <ek seçenekler>]
#
# Sahte kaynak kipinde varsayılan kademe `sade`'dir (düşük donanım; K16). `-Sim` kipinde görünüm
# kademesi senaryonun `kalite.gorunum` alanındandır (`-Kademe` kullanılmaz). Senaryo yolu depo
# köküne göredir (`-Kok` ile başka kök verilebilir).
#
# `-Sim` kipi: `ida_sim.exe --senaryo <senaryo> --a2-port <n> --sil-hiz <çarpan>` başlatılır, A2
# dinleyicisinin açıldığı sim'in günlüğünden görülünce Godot aynı porta bağlanır. Pencere kapanınca
# sim'e TEK konsol olayı (Ctrl-C) gönderilir: ida_sim.exe düzgün kapanır ve özet satırını yazar;
# ikinci olayda hemen çıkardı (sim/sunucu/src/sinyal.cpp), bu yüzden betik olayı yinelemez.
# Terminalde elle Ctrl-C basılırsa olay sim'e zaten doğrudan ulaşır; betik yalnız sim'in kapanışını
# (en çok 10 s) bekler, süre dolarsa süreci sonlandırır.
#
# Ortam değişkenleri:
#   GODOT            Godot 4.7.2 ikilisi (Godot_v4.7.2-stable_win64.exe). Verilmezse
#                    %IDA_CACHE% (yoksa %LOCALAPPDATA%\ida) altında godot\4.7.2\ denenir.
#   IDA_VIZ_DERLEME  derleme dizini (varsayılan <depo>\build\viz; IDA_VIZ=ON, Release)
#   IDA_SIM          -Sim: `ida_sim.exe` ikilisi. Verilmezse <depo>\build\host-release, sonra
#                    host-debug altındaki denenir.
#   IDA_SIM_SECENEKLER  -Sim: ida_sim'e ek seçenekler (boşlukla ayrılır; ör. "--sonar")
#
# Eklenti ve sahte kaynak derlenmemişse (ya da -Derle verilirse) önce derlenir: MSVC ortamı
# (Visual Studio 2022 Build Tools, "C++ ile masaüstü geliştirme" + CMake/Ninja bileşenleri) yoksa
# vswhere ile bulunup etkinleştirilir. Godot projesi hiç içe aktarılmamışsa bir kez başsız içe
# aktarılır.
[CmdletBinding()]
param(
    [ValidateSet('sade', 'tam')][string]$Kademe = 'sade',
    [int]$A2Port = 47210,
    [string]$Cozunurluk = '1920x1080',
    [switch]$Derle,
    [switch]$YalnizSahte,
    [string]$Sim = '',
    [double]$Hiz = 1,
    [string]$Kok = '',
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$Ek = @()
)

$ErrorActionPreference = 'Stop'

$Burada = Split-Path -Parent $MyInvocation.MyCommand.Path
$DepoKok = (Resolve-Path (Join-Path $Burada '..')).Path
if (-not $Kok) { $Kok = $DepoKok }
$Proje = Join-Path $Burada 'proje'
if ($env:IDA_VIZ_DERLEME) { $Derleme = $env:IDA_VIZ_DERLEME } else { $Derleme = Join-Path $DepoKok 'build\viz' }
if ($env:GODOT) {
    $Godot = $env:GODOT
} else {
    if ($env:IDA_CACHE) { $Onbellek = $env:IDA_CACHE } else { $Onbellek = Join-Path $env:LOCALAPPDATA 'ida' }
    $Godot = Join-Path $Onbellek 'godot\4.7.2\Godot_v4.7.2-stable_win64.exe'
}

function Hata([string]$m, [int]$kod = 1) {
    [Console]::Error.WriteLine("calistir.ps1: $m")
    exit $kod
}

# Argüman dizgesi: boşluk ya da tırnak içeren değerler çift tırnağa alınır.
function ArgDizgesi([string[]]$liste) {
    ($liste | ForEach-Object {
        if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ }
    }) -join ' '
}

if (-not (Test-Path -LiteralPath $Godot -PathType Leaf)) {
    [Console]::Error.WriteLine("calistir.ps1: Godot 4.7.2 ikilisi bulunamadı: $Godot")
    Hata 'GODOT ortam değişkeniyle yolu verin.'
}
if ($Sim -and $YalnizSahte) { Hata '-YalnizSahte -Sim ile birlikte kullanılamaz' 2 }

$Dll = Join-Path $Proje 'bin\ida_viz.dll'
$SahteExe = Join-Path $Derleme 'viz\ida_sahte_a2.exe'

function Msvc-Ortami {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) { return }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { Hata 'MSVC bulunamadı: Visual Studio 2022 Build Tools (C++ araçları) kurun.' }
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { Hata 'C++ araçları kurulu Visual Studio bulunamadı.' }
    $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
    $ortam = & cmd.exe /d /s /c "`"$vcvars`" >nul && set"
    if ($LASTEXITCODE -ne 0) { Hata "vcvars64.bat başarısız: $LASTEXITCODE" }
    foreach ($satir in $ortam) {
        if ($satir -match '^([^=]+)=(.*)$') { Set-Item -Path ("env:" + $Matches[1]) -Value $Matches[2] }
    }
}

function Eklenti-Derle([string[]]$hedefler) {
    Msvc-Ortami
    Write-Host "calistir.ps1: eklenti derleniyor ($Derleme)"
    & cmake -S $DepoKok -B $Derleme -G Ninja -DIDA_HEDEF=host -DCMAKE_BUILD_TYPE=Release -DIDA_VIZ=ON | Out-Null
    if ($LASTEXITCODE -ne 0) { Hata "cmake yapılandırması başarısız ($LASTEXITCODE)" }
    & cmake --build $Derleme --target @hedefler
    if ($LASTEXITCODE -ne 0) { Hata "derleme başarısız ($LASTEXITCODE)" }
}

if ($Sim) {
    if ($Derle -or -not (Test-Path -LiteralPath $Dll)) { Eklenti-Derle @('ida_viz_eklenti') }
} elseif ($Derle -or -not (Test-Path -LiteralPath $Dll) -or -not (Test-Path -LiteralPath $SahteExe)) {
    Eklenti-Derle @('ida_viz_eklenti', 'ida_sahte_a2')
}

if (-not (Test-Path -LiteralPath (Join-Path $Proje '.godot\extension_list.cfg'))) {
    Write-Host 'calistir.ps1: Godot projesi ilk kez içe aktarılıyor'
    try {
        Start-Process -FilePath $Godot -ArgumentList (ArgDizgesi @('--headless', '--path', $Proje, '--import')) `
            -Wait -NoNewWindow | Out-Null
    } catch { }
}

function Godot-Calistir([string[]]$ekArgs) {
    $a = @('--path', $Proje, '--resolution', $Cozunurluk, '--', '--a2-port', "$A2Port") + $ekArgs + $Ek
    # Godot'nun GUI ikilisi konsolu beklemez; -Wait pencere kapanana dek bekler.
    $p = Start-Process -FilePath $Godot -ArgumentList (ArgDizgesi $a) -Wait -NoNewWindow -PassThru
    return $p.ExitCode
}

# Günlük dosyasını, yazan süreç açıkken okur.
function Gunluk-Oku([string]$yol) {
    try {
        $fs = [IO.File]::Open($yol, 'Open', 'Read', 'ReadWrite')
        try { $sr = New-Object IO.StreamReader($fs); return $sr.ReadToEnd() } finally { $fs.Dispose() }
    } catch { return '' }
}

if (-not $Sim) {
    # ------------------------------------------------------------------ sahte kaynak
    $manifest = Join-Path $DepoKok 'assets\acik_su\manifest.json'
    $sahteArg = ArgDizgesi @('--manifest', $manifest, '--kademe', $Kademe, '--a2-port', "$A2Port")
    $sahte = Start-Process -FilePath $SahteExe -ArgumentList $sahteArg -NoNewWindow -PassThru
    try {
        if ($YalnizSahte) {
            $sahte.WaitForExit()
            exit $sahte.ExitCode
        }
        exit (Godot-Calistir @('--sahte'))
    } finally {
        # Windows'ta SIGTERM yoktur: sahte kaynak (kinematik, durumsuz) doğrudan sonlandırılır.
        if (-not $sahte.HasExited) { try { $sahte.Kill() } catch { } }
    }
}

# ---------------------------------------------------------------------- gerçek sim (W5-bagla-a2)
$SimExe = $env:IDA_SIM
if (-not $SimExe) {
    foreach ($d in @('host-release', 'host-debug')) {
        $aday = Join-Path $DepoKok "build\$d\sim\sunucu\ida_sim.exe"
        if (Test-Path -LiteralPath $aday) { $SimExe = $aday; break }
    }
}
if (-not $SimExe -or -not (Test-Path -LiteralPath $SimExe)) {
    [Console]::Error.WriteLine('calistir.ps1: ida_sim.exe bulunamadı (build\host-release ya da build\host-debug).')
    [Console]::Error.WriteLine('  Derleyin (cmake --preset host-release; cmake --build --preset host-release --target ida_sim)')
    Hata 'ya da IDA_SIM ortam değişkeniyle yolu verin.'
}

Add-Type -Namespace Ida -Name Konsol -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError = true)]
public static extern bool GenerateConsoleCtrlEvent(uint olay, uint grup);
[System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError = true)]
public static extern bool SetConsoleCtrlHandler(System.IntPtr isleyici, bool ekle);
'@

$Gunluk = [IO.Path]::Combine([IO.Path]::GetTempPath(), "ida_sim_a2.$([guid]::NewGuid().ToString('N')).err")
$Cikti = [IO.Path]::Combine([IO.Path]::GetTempPath(), "ida_sim_a2.$([guid]::NewGuid().ToString('N')).out")
$simArg = @('--senaryo', $Sim, '--kok', $Kok, '--a2-port', "$A2Port", '--sil-hiz', "$Hiz", '--gunluk', '1')
if ($env:IDA_SIM_SECENEKLER) { $simArg += ($env:IDA_SIM_SECENEKLER -split '\s+' | Where-Object { $_ }) }
$simSurec = Start-Process -FilePath $SimExe -ArgumentList (ArgDizgesi $simArg) -NoNewWindow -PassThru `
    -RedirectStandardError $Gunluk -RedirectStandardOutput $Cikti

$normal = $false
$kod = 0
try {
    # Hazır olma işareti: "A2 127.0.0.1:<port> adresinde dinliyor" (sabit uyku yok; en çok 60 s).
    $hazir = $false
    for ($i = 0; $i -lt 600; $i++) {
        $metin = Gunluk-Oku $Gunluk
        if ($metin.Contains("A2 127.0.0.1:$A2Port adresinde dinliyor")) { $hazir = $true; break }
        if ($simSurec.HasExited -or $metin.Contains("A2'siz çalışılıyor") -or $metin.Contains('dinleyici açılmadı')) { break }
        Start-Sleep -Milliseconds 100
    }
    if (-not $hazir) {
        [Console]::Error.WriteLine("calistir.ps1: ida_sim A2 dinleyicisini açmadı; sim günlüğü ($Gunluk):")
        [Console]::Error.WriteLine((Gunluk-Oku $Gunluk))
        $kod = 1
    } else {
        Write-Host "calistir.ps1: ida_sim çalışıyor (senaryo $Sim, hız $Hiz, A2 127.0.0.1:$A2Port; günlük $Gunluk)"
        $kod = Godot-Calistir @()
        $normal = $true
    }
} finally {
    if ($simSurec -and -not $simSurec.HasExited) {
        if ($normal) {
            # Pencere kapandı, terminalde Ctrl-C basılmadı: sim'e TEK kapanış olayı. Bu süreç olayı
            # yoksayar (işleyici yok sayma bayrağı); sim ve Godot bayrağı bu noktadan önce
            # başladıkları için miras almaz (Godot zaten çıkmıştır).
            [void][Ida.Konsol]::SetConsoleCtrlHandler([IntPtr]::Zero, $true)
            [void][Ida.Konsol]::GenerateConsoleCtrlEvent(0, 0)
        }
        # Klavyeden Ctrl-C ile gelindiyse olay sim'e doğrudan ulaşmıştır; yalnız kapanışı bekle.
        if (-not $simSurec.WaitForExit(10000)) { try { $simSurec.Kill() } catch { } }
        [void][Ida.Konsol]::SetConsoleCtrlHandler([IntPtr]::Zero, $false)
    }
    Write-Host "calistir.ps1: sim günlüğü ($Gunluk):"
    Write-Host (Gunluk-Oku $Gunluk)
}
exit $kod
