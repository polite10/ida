# MP8a: calistir.ps1'in Mac'te (pwsh) yapilabilen duragan denetimi. Windows'ta calistirmak gerekmez.
# Kullanim: pwsh -NoProfile -File tools/olcum/windows/mac_denetim.ps1 tools/olcum/windows/calistir.ps1 [PSScriptAnalyzer_modul_dizini]
#   1) PowerShell ayristirmasi (hata sayisi)
#   2) PS 7'ye ozgu sozdizimi (??, ??=, ?., ?[], uclu islec, && / ||) AST taramasi
#   3) ASCII disi karakter (PS 5.1 BOM'suz dosyayi sistem kod sayfasiyla okur)
#   4) Gomulu C# yardimcisinin C# 5 olarak ayristirilmasi (PS 5.1 Add-Type .NET Framework csc kullanir)
#      ve Add-Type ile derlenmesi (P/Invoke cagrilmaz)
#   5) Istege bagli: PSScriptAnalyzer PSUseCompatibleSyntax (hedef 5.1) ve PSUseCompatibleCmdlets
param([string]$Betik, [string]$ModulDizini = '')
$ErrorActionPreference = 'Stop'
$sorun = 0
$tok = $null; $err = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($Betik, [ref]$tok, [ref]$err)
'1) ayristirma hatasi: ' + $err.Count
foreach ($e in $err) { '   ' + $e.Extent.StartLineNumber + ': ' + $e.Message; $sorun++ }

$ozel = $ast.FindAll({ param($n)
    $n -is [System.Management.Automation.Language.TernaryExpressionAst] -or
    $n -is [System.Management.Automation.Language.PipelineChainAst] -or
    ($n -is [System.Management.Automation.Language.BinaryExpressionAst] -and [string]$n.Operator -eq 'QuestionQuestion') -or
    ($n -is [System.Management.Automation.Language.AssignmentStatementAst] -and [string]$n.Operator -eq 'QuestionQuestionEquals') -or
    ($n -is [System.Management.Automation.Language.MemberExpressionAst] -and $n.NullConditional) -or
    ($n -is [System.Management.Automation.Language.IndexExpressionAst] -and $n.NullConditional)
}, $true)
'2) PS 7 ye ozgu dugum: ' + @($ozel).Count
foreach ($o in $ozel) { '   ' + $o.Extent.StartLineNumber + ': ' + $o.Extent.Text; $sorun++ }

$bayt = [System.IO.File]::ReadAllBytes((Resolve-Path $Betik))
$asciiDisi = @($bayt | Where-Object { $_ -gt 126 -or ($_ -lt 32 -and $_ -ne 10 -and $_ -ne 13) }).Count
'3) ASCII disi bayt: ' + $asciiDisi
if ($asciiDisi -gt 0) { $sorun++ }

$atama = $ast.Find({ param($n) $n -is [System.Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -eq '$W32Kaynak' }, $true)
$cs = $atama.Right.Expression.Value
$opt = New-Object Microsoft.CodeAnalysis.CSharp.CSharpParseOptions([Microsoft.CodeAnalysis.CSharp.LanguageVersion]::CSharp5)
function Csharp5-Tani([string]$Kod) {
    $t = [Microsoft.CodeAnalysis.CSharp.CSharpSyntaxTree]::ParseText($Kod, $opt, '', $null, [System.Threading.CancellationToken]::None)
    return @($t.GetDiagnostics())
}
$oz = @(Csharp5-Tani 'class A { string F(int x) { var s = $"{x}"; return s?.Trim(); } }')
'4) oz denetim (C# 6 ozelligi C#5 ayristirmasinda hata vermeli): ' + $oz.Count + ' tanilama'
if ($oz.Count -eq 0) { '   UYARI: oz denetim hata yakalamadi; C#5 denetimi guvenilir degil'; $sorun++ }
$diag = @(Csharp5-Tani $cs)
'   gomulu C# (' + $cs.Length + ' karakter) C#5 tanilama: ' + $diag.Count
foreach ($d in $diag) { '   ' + $d.ToString(); $sorun++ }
Add-Type -TypeDefinition $cs -Language CSharp
'   Add-Type derlendi: ' + [bool]('IdaMp8W32' -as [type])

if ($ModulDizini) {
    Import-Module (Join-Path $ModulDizini 'PSScriptAnalyzer')
    $ayar = @{ Rules = @{
            PSUseCompatibleSyntax  = @{ Enable = $true; TargetVersions = @('5.1') }
            PSUseCompatibleCmdlets = @{ compatibility = @('desktop-5.1.14393.206-windows') }
        } }
    $bulgu = @(Invoke-ScriptAnalyzer -Path $Betik -Settings $ayar -IncludeRule PSUseCompatibleSyntax, PSUseCompatibleCmdlets)
    '5) PSScriptAnalyzer uyumluluk bulgusu (5.1): ' + $bulgu.Count
    foreach ($b in $bulgu) { '   ' + $b.Line + ' ' + $b.RuleName + ': ' + $b.Message; $sorun++ }
    $genel = @(Invoke-ScriptAnalyzer -Path $Betik -Severity Error, Warning -ExcludeRule PSAvoidUsingWriteHost, PSUseApprovedVerbs, PSUseShouldProcessForStateChangingFunctions, PSUseSingularNouns)
    '   genel kural bulgusu (Error/Warning; Write-Host ve fiil adlari haric): ' + $genel.Count
    foreach ($b in $genel) { '   ' + $b.Line + ' ' + $b.RuleName + ': ' + $b.Message }
} else {
    '5) PSScriptAnalyzer: atlandi (modul dizini verilmedi)'
}
'SONUC: sorun ' + $sorun
if ($sorun -gt 0) { exit 1 }
exit 0
