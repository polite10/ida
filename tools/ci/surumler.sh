#!/bin/bash
# CI: koşucunun ve araçların sürümlerini günlüğe yazar (09 §2.6: derleyici sürümleri
# günlüğe yazılır). Bulunmayan araç "yok" olarak yazılır; betik başarısız olmaz.
# Windows'ta MSVC geliştirici ortamı etkin olmalıdır (tools/ci/msvc-ortam.ps1).

export LC_ALL=C

satir() { printf '%-22s %s\n' "$1" "$2"; }
# Windows araçları CRLF yazabilir; satır sonu ayıklanır.
ilk() { "$@" 2>&1 | tr -d '\r' | head -1; }

satir "işletim sistemi" "$(uname -srm)"
case "$(uname -s)" in
    Darwin)
        satir "macOS" "$(sw_vers -productVersion) ($(sw_vers -buildVersion))"
        satir "Xcode" "$(xcodebuild -version 2>/dev/null | tr '\n' ' ')"
        satir "Apple Clang" "$(ilk clang --version)" ;;
    MINGW*|MSYS*|CYGWIN*)
        satir "MSVC (cl)" "$(cl 2>&1 | tr -d '\r' | head -1)"
        satir "VS sürümü" "${VSCMD_VER:-yok}"
        satir "VC araç takımı" "${VCToolsVersion:-yok}"
        satir "Windows SDK" "${WindowsSDKVersion:-yok}"
        satir "llvm-nm" "$(ilk llvm-nm --version 2>/dev/null || echo yok)" ;;
esac
satir "CMake" "$(ilk cmake --version)"
satir "Ninja" "$(ninja --version 2>&1)"
py=$(command -v python || command -v python3 || echo python)
satir "Python" "$(ilk "$py" --version 2>/dev/null || echo yok)"
satir "git" "$(ilk git --version)"
if [ -n "${ARM_GCC_BIN-}" ]; then
    gcc_yolu="$ARM_GCC_BIN/arm-none-eabi-gcc"
    [ -f "$gcc_yolu" ] || gcc_yolu="$gcc_yolu.exe"
    satir "arm-none-eabi-gcc" "$(ilk "$gcc_yolu" --version)"
fi
satir "clang-tidy" "$(command -v clang-tidy >/dev/null && clang-tidy --version 2>&1 | grep -m1 -i version || echo yok)"
satir "cppcheck" "$(if [ -n "${CPPCHECK-}" ]; then ilk "$CPPCHECK" --version; else echo yok; fi)"
if [ -n "${VCPKG_ROOT-}" ]; then
    satir "vcpkg" "$(ilk "$VCPKG_ROOT/vcpkg" --version 2>/dev/null || echo yok)"
    satir "vcpkg commit" "$(git -C "$VCPKG_ROOT" rev-parse --short HEAD 2>/dev/null || echo yok)"
fi
exit 0
