# ST ve Arm kaynakları (W3-kart, dilim 1): CMSIS 6 çekirdek başlıkları, STM32U5 CMSIS aygıt
# başlıkları ve STM32U5 HAL sürücüsü.
#
# Sabitleme: her kaynak, sürüm etiketinin işaret ettiği commit'in GitHub arşividir; arşivin
# SHA-256'sı bu dosyadadır (Unity ile aynı yöntem: cmake/IdaBagimliliklar.cmake). Hareketli
# etiket ya da dal yoktur; sürüm yükseltme ayrı iştir. Git alt modülü yerine CPM seçildi:
# otomatik derlemenin checkout adımı alt modül çekmez ve firmware derlemesi orada da çalışmalıdır
# (firmware/third_party/BENIOKU.md).
#
#   STM32U5 HAL       stm32u5xx-hal-driver v1.6.3  2552682ee5a0d6f826a6435750fc9267be65868b
#   CMSIS aygıt       cmsis-device-u5      v1.4.3  624374fa1e21ca195d6f2102ac0caaa50d0ea4c8
#   CMSIS çekirdek    CMSIS_6              v6.2.0  6f0a58d01aa9bd2feba212097f9afe7acd991d52
#
# HAL ve aygıt commit'leri STM32CubeU5 v1.9.0'ın alt modül olarak sabitlediği commit'lerdir
# (MP1-MP5 ölçüm prototiplerinde kartta kullanılan kaynaklarla aynı; tools/olcum/mp1/kaynak-indir.sh).
#
#   ida_fw_st_kur()  -> ida_fw_hal (statik; HAL + system_stm32u5xx.c), içerme yolları
#                       SYSTEM olarak dışa açılır (üçüncü taraf başlıkları İDA uyarı politikasına
#                       girmez).
#
# HAL yapılandırması (stm32u5xx_hal_conf.h) sabitlenmiş şablondan derleme dizinine üretilir:
# yalnız aşağıdaki modüller açılır; HSE_VALUE şablonun değeridir (16 MHz, kartın kristali).

include_guard(GLOBAL)

function(ida_fw_st_kur)
  if(TARGET ida_fw_hal)
    return()
  endif()

  CPMAddPackage(
    NAME ida_cmsis6
    URL "https://github.com/ARM-software/CMSIS_6/archive/6f0a58d01aa9bd2feba212097f9afe7acd991d52.tar.gz"
    URL_HASH SHA256=20b1bfbf1e43539b6300fa6f8ee44a01ab8f91ab8b1816a0d1cf7b85a23d4c48
    DOWNLOAD_ONLY YES)
  CPMAddPackage(
    NAME ida_cmsis_device_u5
    URL "https://github.com/STMicroelectronics/cmsis-device-u5/archive/624374fa1e21ca195d6f2102ac0caaa50d0ea4c8.tar.gz"
    URL_HASH SHA256=2acf79a2ae6a6773a2213ca61117febda7054c19682bf26b0b98ba5bd87b6354
    DOWNLOAD_ONLY YES)
  CPMAddPackage(
    NAME ida_stm32u5_hal
    URL "https://github.com/STMicroelectronics/stm32u5xx-hal-driver/archive/2552682ee5a0d6f826a6435750fc9267be65868b.tar.gz"
    URL_HASH SHA256=bfec36e28f032d0c1742a94b1f1a3567b98f5a28750b2d37c19c749471322d21
    DOWNLOAD_ONLY YES)

  set(_cekirdek "${ida_cmsis6_SOURCE_DIR}/CMSIS/Core/Include")
  set(_aygit "${ida_cmsis_device_u5_SOURCE_DIR}")
  set(_hal "${ida_stm32u5_hal_SOURCE_DIR}")
  foreach(_d "${_cekirdek}/core_cm33.h" "${_aygit}/Include/stm32u5a9xx.h"
             "${_hal}/Inc/stm32u5xx_hal_conf_template.h")
    if(NOT EXISTS "${_d}")
      message(FATAL_ERROR "ida_fw_st_kur: beklenen dosya yok: ${_d}")
    endif()
  endforeach()

  # HAL yapılandırması: şablondaki bütün modüller kapatılır, yalnız gerekenler açılır.
  set(_moduller CORTEX RCC GPIO PWR FLASH ICACHE RNG EXTI DMA)
  file(READ "${_hal}/Inc/stm32u5xx_hal_conf_template.h" _conf)
  string(REGEX REPLACE "#define (HAL_[A-Z0-9]+_MODULE_ENABLED)" "/* #define \\1 */" _conf "${_conf}")
  string(REPLACE "/* #define HAL_MODULE_ENABLED */" "#define HAL_MODULE_ENABLED" _conf "${_conf}")
  foreach(_m IN LISTS _moduller)
    string(REPLACE "/* #define HAL_${_m}_MODULE_ENABLED */" "#define HAL_${_m}_MODULE_ENABLED"
           _conf "${_conf}")
  endforeach()
  set(_uretilen "${CMAKE_CURRENT_BINARY_DIR}/st_uretilen")
  file(WRITE "${_uretilen}/stm32u5xx_hal_conf.h.yeni" "${_conf}")
  file(COPY_FILE "${_uretilen}/stm32u5xx_hal_conf.h.yeni" "${_uretilen}/stm32u5xx_hal_conf.h"
       ONLY_IF_DIFFERENT)

  set(_kaynaklar hal hal_cortex hal_rcc hal_rcc_ex hal_gpio hal_pwr hal_pwr_ex hal_flash
                 hal_flash_ex hal_icache hal_rng hal_rng_ex)
  set(_c "")
  foreach(_s IN LISTS _kaynaklar)
    list(APPEND _c "${_hal}/Src/stm32u5xx_${_s}.c")
  endforeach()
  list(APPEND _c "${_aygit}/Source/Templates/system_stm32u5xx.c")

  add_library(ida_fw_hal STATIC ${_c})
  target_include_directories(ida_fw_hal SYSTEM PUBLIC
    "${_uretilen}" "${_hal}/Inc" "${_aygit}/Include" "${_cekirdek}")
  target_compile_definitions(ida_fw_hal PUBLIC STM32U5A9xx USE_HAL_DRIVER)
  set_target_properties(ida_fw_hal PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
  # Üçüncü taraf kodu: İDA uyarı politikası uygulanmaz; kayan nokta politikası aynıdır.
  target_compile_options(ida_fw_hal PRIVATE -w -ffp-contract=off -fno-fast-math)
endfunction()
