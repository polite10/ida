# A4-K22 denetimi: bir statik kitaplığın nesnelerinde yazılabilir veri bölümüne düşen tanımlı
# simge var mı (simge tablosundan).
#
#   cmake -DNM=<nm> -DARSIV=<.a> -DBICIM=macho|elf -P yazilabilir_veri.cmake
#
# macho (Apple nm -m): bölümü (__DATA, ... ya da (__DATA_CONST, ... olan tanımlı simge
#   yasaktır (işaretçi taşıyan sabit tablolar da PIC'te __DATA_CONST'a düşer).
# elf (arm-none-eabi-nm): türü D, d, B, b, C, G, g, S, s olan simge yasaktır.
# Bulgu varsa her biri "yazılabilir veri simgesi: <satır>" diye basılır ve çıkış hatalıdır.

if(NOT NM OR NOT ARSIV OR NOT BICIM)
  message(FATAL_ERROR "NM, ARSIV ve BICIM verilmeli")
endif()

if(BICIM STREQUAL "macho")
  execute_process(COMMAND "${NM}" -m "${ARSIV}"
    OUTPUT_VARIABLE _cikti ERROR_VARIABLE _hata RESULT_VARIABLE _rc)
else()
  execute_process(COMMAND "${NM}" "${ARSIV}"
    OUTPUT_VARIABLE _cikti ERROR_VARIABLE _hata RESULT_VARIABLE _rc)
endif()
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "nm çalıştırılamadı (${_rc}): ${_hata}")
endif()

string(REPLACE "\n" ";" _satirlar "${_cikti}")
set(_bulgu 0)
set(_simge 0)
foreach(_s IN LISTS _satirlar)
  if(_s STREQUAL "" OR _s MATCHES ":$")
    continue()
  endif()
  if(BICIM STREQUAL "macho")
    if(_s MATCHES "\\(undefined\\)")
      continue()
    endif()
    math(EXPR _simge "${_simge} + 1")
    if(_s MATCHES "\\(__DATA")
      message("yazılabilir veri simgesi: ${_s}")
      math(EXPR _bulgu "${_bulgu} + 1")
    endif()
  else()
    if(_s MATCHES "^ +U ")
      continue()
    endif()
    math(EXPR _simge "${_simge} + 1")
    if(_s MATCHES "^[0-9a-fA-F]+ [DdBbCGgSs] ")
      message("yazılabilir veri simgesi: ${_s}")
      math(EXPR _bulgu "${_bulgu} + 1")
    endif()
  endif()
endforeach()

if(_bulgu GREATER 0)
  message(FATAL_ERROR "${_bulgu} yazılabilir veri simgesi bulundu (${ARSIV})")
endif()
if(_simge EQUAL 0)
  message(FATAL_ERROR "hiç tanımlı simge okunamadı (${ARSIV})")
endif()
message("yazılabilir veri simgesi yok (${_simge} tanımlı simge denetlendi)")
