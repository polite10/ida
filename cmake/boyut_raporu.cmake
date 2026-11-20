# Firmware ELF'inin boyut raporunu yazar (cmake -P ile; IdaFirmware.cmake çağırır).
#   -DSIZE=<arm-none-eabi-size> -DELF=<dosya.elf> -DCIKTI=<rapor.txt>
foreach(_d SIZE ELF CIKTI)
  if(NOT DEFINED ${_d})
    message(FATAL_ERROR "boyut_raporu.cmake: ${_d} verilmedi")
  endif()
endforeach()
# Rapor makineden bağımsız olsun diye size ELF'in dizininde, yalnız dosya adıyla çağrılır.
get_filename_component(_dizin "${ELF}" DIRECTORY)
get_filename_component(_ad "${ELF}" NAME)
execute_process(COMMAND "${SIZE}" -B "${_ad}" WORKING_DIRECTORY "${_dizin}"
  OUTPUT_VARIABLE _ozet RESULT_VARIABLE _rc1)
execute_process(COMMAND "${SIZE}" -A -x "${_ad}" WORKING_DIRECTORY "${_dizin}"
  OUTPUT_VARIABLE _bolumler RESULT_VARIABLE _rc2)
if(NOT _rc1 EQUAL 0 OR NOT _rc2 EQUAL 0)
  message(FATAL_ERROR "boyut_raporu.cmake: size başarısız (${_rc1}, ${_rc2})")
endif()
file(WRITE "${CIKTI}" "# Boyut raporu (Berkeley biçimi)\n${_ozet}\n# Bölümler (System V biçimi)\n${_bolumler}")
