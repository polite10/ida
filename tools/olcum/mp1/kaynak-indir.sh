#!/bin/sh
# W0-MP1a: ST kaynaklarını sabit commit'le $IDA_CACHE/src altına indirir (depoya girmez).
# STM32CubeU5 v1.9.0'ın alt modül olarak sabitlediği commit'ler kullanılır.
set -eu
. "$(dirname "$0")/ortam.sh"
mkdir -p "$MP1_SRC"

CUBE_TAG=v1.9.0
CUBE_SHA=d88042df24f16799957c24f00c2b234c9e306188
HAL_SHA=2552682ee5a0d6f826a6435750fc9267be65868b   # stm32u5xx-hal-driver (etiket v1.6.3)
DEV_SHA=624374fa1e21ca195d6f2102ac0caaa50d0ea4c8   # cmsis-device-u5 (etiket v1.4.3)
BSP_SHA=a5723a4ff404cb659b3c095d59527a0b0c2062f0   # stm32u5x9j-dk-bsp

sha_ile() { # $1 dizin, $2 url, $3 commit
    if [ ! -d "$1/.git" ]; then
        git init -q "$1"
        git -C "$1" remote add origin "$2"
    fi
    if [ "$(git -C "$1" rev-parse -q --verify HEAD 2>/dev/null || true)" != "$3" ]; then
        git -C "$1" fetch -q --depth 1 origin "$3"
        git -C "$1" checkout -q --detach FETCH_HEAD
    fi
    test "$(git -C "$1" rev-parse HEAD)" = "$3"
    echo "$1 @ $3"
}

# STM32CubeU5: yalnız CMSIS çekirdek başlıkları ve DK örnekleri (seyrek checkout).
CUBE="$MP1_SRC/STM32CubeU5"
if [ ! -d "$CUBE/.git" ]; then
    git clone -q --filter=blob:none --no-checkout --depth 1 --branch "$CUBE_TAG" \
        https://github.com/STMicroelectronics/STM32CubeU5.git "$CUBE"
fi
git -C "$CUBE" sparse-checkout set --no-cone \
    /Drivers/CMSIS/Core/Include/ \
    /Drivers/CMSIS/LICENSE.txt \
    /Projects/STM32U5x9J-DK/Templates/TrustZoneDisabled/ \
    /Projects/STM32U5x9J-DK/Examples/RCC/RCC_ClockConfig/ \
    /Projects/STM32U5x9J-DK/Examples/GPIO/GPIO_IOToggle/ \
    /Projects/STM32U5x9J-DK/Examples/BSP/Src/main.c \
    /Projects/STM32U5x9J-DK/Examples/DSI/DSI_VideoMode_SingleBuffer/Src/main.c \
    /Projects/STM32U5x9J-DK/Applications/USBX/Ux_Host_CDC_ACM/Core/Src/main.c
git -C "$CUBE" checkout -q "$CUBE_SHA"
test "$(git -C "$CUBE" rev-parse HEAD)" = "$CUBE_SHA"
echo "$CUBE @ $CUBE_TAG $CUBE_SHA"

sha_ile "$MP1_SRC/stm32u5xx-hal-driver" https://github.com/STMicroelectronics/stm32u5xx-hal-driver.git "$HAL_SHA"
sha_ile "$MP1_SRC/cmsis-device-u5"      https://github.com/STMicroelectronics/cmsis-device-u5.git      "$DEV_SHA"
sha_ile "$MP1_SRC/stm32u5x9j-dk-bsp"    https://github.com/STMicroelectronics/stm32u5x9j-dk-bsp.git    "$BSP_SHA"
