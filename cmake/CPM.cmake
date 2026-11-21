# CPM.cmake önyüklemesi: sürüm ve SHA-256 sabit (W2-depo §3; hareketli etiket yok).
# Kaynak: https://github.com/cpm-cmake/CPM.cmake/releases/tag/v0.43.2 (CPM.cmake dosyası).
# CPM_SOURCE_CACHE ortam değişkeni verilirse indirilen kaynaklar orada paylaşılır.

include_guard(GLOBAL)

set(IDA_CPM_SURUMU "0.43.2")
set(IDA_CPM_SHA256 "49a3bef91ceb65bb66d57255e12d1ffd22abc2f6408fa9fe4534c544a2f232aa")

if(CPM_SOURCE_CACHE)
  set(_ida_cpm_dosyasi "${CPM_SOURCE_CACHE}/cpm/CPM_${IDA_CPM_SURUMU}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
  set(_ida_cpm_dosyasi "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${IDA_CPM_SURUMU}.cmake")
else()
  set(_ida_cpm_dosyasi "${CMAKE_BINARY_DIR}/cmake/CPM_${IDA_CPM_SURUMU}.cmake")
endif()

if(NOT EXISTS "${_ida_cpm_dosyasi}")
  file(DOWNLOAD
    "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${IDA_CPM_SURUMU}/CPM.cmake"
    "${_ida_cpm_dosyasi}"
    EXPECTED_HASH SHA256=${IDA_CPM_SHA256}
    TLS_VERIFY ON
    STATUS _ida_cpm_durum)
  list(GET _ida_cpm_durum 0 _ida_cpm_kod)
  if(NOT _ida_cpm_kod EQUAL 0)
    file(REMOVE "${_ida_cpm_dosyasi}")
    message(FATAL_ERROR "CPM.cmake indirilemedi: ${_ida_cpm_durum}")
  endif()
endif()

# Önbellekte duran dosya da aynı özetle doğrulanır.
file(SHA256 "${_ida_cpm_dosyasi}" _ida_cpm_ozet)
if(NOT _ida_cpm_ozet STREQUAL IDA_CPM_SHA256)
  message(FATAL_ERROR "CPM.cmake özeti uyuşmuyor: ${_ida_cpm_dosyasi} (${_ida_cpm_ozet})")
endif()

include("${_ida_cpm_dosyasi}")
