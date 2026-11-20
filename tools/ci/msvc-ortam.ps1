# CI (Windows): Visual Studio'nun x64 geliştirici ortamını sonraki adımlara aktarır.
#
# vswhere ile C++ araçları kurulu en yeni Visual Studio bulunur, vcvars64.bat çalıştırılır
# ve ortaya çıkan ortam değişkenleri GITHUB_ENV'e yazılır. Üçüncü taraf eylem kullanılmaz.
# Sonraki adımlarda cl, link ve MSVC'nin ASan çalışma kitaplığı PATH'tedir.

$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $vs) { throw 'C++ araçları kurulu Visual Studio bulunamadı' }

$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$ortam = & cmd.exe /d /s /c "`"$vcvars`" >nul && set"
if ($LASTEXITCODE -ne 0) { throw "vcvars64.bat başarısız: $LASTEXITCODE" }

# Yalnız vcvars'ın eklediği ya da değiştirdiği değişkenler yazılır; GitHub'ın kendi
# değişkenlerine (GITHUB_*, RUNNER_*, ACTIONS_*) dokunulmaz.
$sayi = 0
foreach ($satir in $ortam) {
    if ($satir -match '^([^=]+)=(.*)$') {
        $ad = $Matches[1]; $deger = $Matches[2]
        if ($ad -match '^(GITHUB_|RUNNER_|ACTIONS_)') { continue }
        if ([Environment]::GetEnvironmentVariable($ad) -ceq $deger) { continue }
        "$ad=$deger" | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8
        $sayi++
    }
}
Write-Host "Aktarılan ortam değişkeni: $sayi"
Write-Host "Visual Studio: $vs"
