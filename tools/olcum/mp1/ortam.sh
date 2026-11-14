# shellcheck shell=sh
# W0-MP1a ölçüm prototipi: ortam değişkenleri. Kaynak olarak okunur:  . tools/olcum/mp1/ortam.sh
# Atılabilir prototip; ürün koduna taşınmaz (09 §2.1).
#
# Araçlar STM32CubeIDE paketinin içinden bulunur (Homebrew arm-none-eabi kullanılmaz; 09 §2.6).
# Her yol ortam değişkeniyle ezilebilir.

: "${IDA_CACHE:=$HOME/.cache/ida}"
: "${CUBEIDE_PLUGINS:=/Applications/STM32CubeIDE.app/Contents/Eclipse/plugins}"

_mp1_bul() { # $1: eklenti adı deseni, $2: ikili adı
    for _d in "$CUBEIDE_PLUGINS"/$1; do
        if [ -x "$_d/tools/bin/$2" ]; then echo "$_d/tools/bin"; return 0; fi
    done
    return 1
}

: "${ARM_GCC_BIN:=$(_mp1_bul 'com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.macosaarch64_*' arm-none-eabi-gcc)}"
: "${CUBEPROG_BIN:=$(_mp1_bul 'com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.macosaarch64_*' STM32_Programmer_CLI)}"
: "${STLINK_GDB_BIN:=$(_mp1_bul 'com.st.stm32cube.ide.mcu.externaltools.stlink-gdb-server.macosaarch64_*' ST-LINK_gdbserver)}"

CLI="$CUBEPROG_BIN/STM32_Programmer_CLI"
GDBSRV="$STLINK_GDB_BIN/ST-LINK_gdbserver"
GDB="$ARM_GCC_BIN/arm-none-eabi-gdb"

# Kaynaklar (depoya girmez): kaynak-indir.sh ile indirilir.
MP1_SRC="$IDA_CACHE/src"
MP1_VERI="$IDA_CACHE/mp1"

export IDA_CACHE CUBEIDE_PLUGINS ARM_GCC_BIN CUBEPROG_BIN STLINK_GDB_BIN CLI GDBSRV GDB MP1_SRC MP1_VERI

for _x in "$CLI" "$GDBSRV" "$GDB" "$ARM_GCC_BIN/arm-none-eabi-gcc"; do
    [ -x "$_x" ] || echo "ortam.sh: bulunamadı: $_x" >&2
done
unset _x _d
