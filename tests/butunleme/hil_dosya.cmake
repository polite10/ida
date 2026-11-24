# Komut satırında --kip olmadan, kipi dosyadan seçilen HIL koşusunun ölçüm özetinin yazıldığını
# sınar (W4-sim-senaryo sol #5). Geçici bir depo kökü kurulur: scenarios/z1.toml'dan kip hil_1x
# ve kısa süreli bir senaryo, araç dosyası ve A7 manifesti.
#
#   cmake -DPROGRAM=<ida_sim> -DKOK=<depo kökü> -DGECICI=<dizin> -P hil_dosya.cmake
file(REMOVE_RECURSE "${GECICI}")
file(MAKE_DIRECTORY "${GECICI}/scenarios" "${GECICI}/assets/ornek")
file(COPY "${KOK}/scenarios/ida0.toml" DESTINATION "${GECICI}/scenarios")
file(COPY "${KOK}/assets/ornek/manifest.json" DESTINATION "${GECICI}/assets/ornek")
file(READ "${KOK}/scenarios/z1.toml" _m)
string(REPLACE "kip = \"sil\"" "kip = \"hil_1x\"" _m "${_m}")
string(REPLACE "sure_ns = 90_000_000_000" "sure_ns = 2_000_000_000" _m "${_m}")
string(REPLACE "sil_azami_hiz = true" "" _m "${_m}")
file(WRITE "${GECICI}/scenarios/hil.toml" "${_m}")
execute_process(
  COMMAND "${PROGRAM}" --senaryo scenarios/hil.toml --karsi pty --kok "${GECICI}"
  RESULT_VARIABLE _kod
  OUTPUT_VARIABLE _cikti
  ERROR_VARIABLE _hata)
if(NOT "${_kod}" STREQUAL "0")
  message(FATAL_ERROR "çıkış kodu ${_kod}\n${_hata}")
endif()
foreach(_aranan "ida_sim: hil: TF" "ida_sim: hil: yanıt süresi" "ida_sim: hil döngü")
  string(FIND "${_hata}" "${_aranan}" _yer)
  if(_yer EQUAL -1)
    message(FATAL_ERROR "HIL ölçüm özetinde '${_aranan}' yok:\n${_hata}")
  endif()
endforeach()
message(STATUS "dosyadan seçilen HIL kipinin ölçüm özeti yazıldı")
