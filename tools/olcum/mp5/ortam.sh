# shellcheck shell=sh
# W0-MP5a: ortam değişkenleri (MP4a'nın ortam.sh'ı; araçlar STM32CubeIDE paketinden). Betiklerin içinden okunur.
# Her yol ortam değişkeniyle ezilebilir; kişisel yol dosyaya yazılmaz.
: "${IDA_CACHE:=$HOME/.cache/ida}"
: "${CUBEIDE_PLUGINS:=/Applications/STM32CubeIDE.app/Contents/Eclipse/plugins}"

_mp5_bul() { # $1: eklenti adı deseni, $2: ikili adı
    for _d in "$CUBEIDE_PLUGINS"/$1; do
        if [ -x "$_d/tools/bin/$2" ]; then echo "$_d/tools/bin"; return 0; fi
    done
    return 1
}

: "${ARM_GCC_BIN:=$(_mp5_bul 'com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.macosaarch64_*' arm-none-eabi-gcc)}"
: "${CUBEPROG_BIN:=$(_mp5_bul 'com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.macosaarch64_*' STM32_Programmer_CLI)}"
CLI="$CUBEPROG_BIN/STM32_Programmer_CLI"
# Ham seri kayıtlar ve derleme kayıtları: çalışma kopyasında, git'in yok saydığı dizin.
: "${MP5_VERI:=$(cd "$(dirname "$0")" && pwd)/veri}"
export IDA_CACHE ARM_GCC_BIN CUBEPROG_BIN CLI MP5_VERI
for _x in "$CLI" "$ARM_GCC_BIN/arm-none-eabi-gcc"; do
    [ -x "$_x" ] || echo "ortam.sh: bulunamadı: $_x" >&2
done
unset _x _d
