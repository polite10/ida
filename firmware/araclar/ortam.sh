# shellcheck shell=sh
# firmware/araclar/ortam.sh — kart araçlarının yolları (W3-kart). Betiklerin içinden okunur.
#
# Her yol ortam değişkeniyle verilebilir; verilmezse macOS'ta STM32CubeIDE'nin standart
# kurulum yerindeki eklentilerde aranır (MP1–MP5 prototiplerinin yaklaşımı). Kişisel yol
# dosyaya yazılmaz.
#   ARM_GCC_BIN    arm-none-eabi araç zincirinin bin dizini (GNU Tools for STM32 14.3.rel1)
#   CUBEPROG_BIN   STM32_Programmer_CLI'nin bulunduğu dizin
#   CUBEIDE_PLUGINS  aramada kullanılan eklenti dizini
#   IDA_SERI_UC    kartın sanal seri portu; verilmezse ilk /dev/cu.usbmodem* aygıtı
#   IDA_CACHE      fabrika yedeğinin ve kayıtların kökü (varsayılan $HOME/.cache/ida)
: "${IDA_CACHE:=$HOME/.cache/ida}"
: "${CUBEIDE_PLUGINS:=/Applications/STM32CubeIDE.app/Contents/Eclipse/plugins}"

_ida_bul() { # $1: eklenti adı deseni, $2: ikili adı
    for _d in "$CUBEIDE_PLUGINS"/$1; do
        if [ -x "$_d/tools/bin/$2" ]; then echo "$_d/tools/bin"; return 0; fi
    done
    return 1
}

if [ -z "${ARM_GCC_BIN:-}" ]; then
    ARM_GCC_BIN=$(_ida_bul 'com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.*' arm-none-eabi-gcc || true)
fi
if [ -z "${CUBEPROG_BIN:-}" ]; then
    CUBEPROG_BIN=$(_ida_bul 'com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.*' STM32_Programmer_CLI || true)
fi
CLI="$CUBEPROG_BIN/STM32_Programmer_CLI"
if [ -z "${IDA_SERI_UC:-}" ]; then
    IDA_SERI_UC=$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1 || true)
fi
export IDA_CACHE ARM_GCC_BIN CUBEPROG_BIN CLI IDA_SERI_UC
unset _d
