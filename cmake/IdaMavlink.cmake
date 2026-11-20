# protocol/ida.xml'den MAVLink 2 C başlıklarını üretir (W2-depo §2, sözleşme başlıkları denetimi).
#
#   ida_mavlink_uret(<hedef-adı>)
#     <hedef-adı>: üretilen başlıkları SYSTEM include yolu olarak taşıyan INTERFACE hedefi.
#     Başlıklar <ikili-dizin>/protocol/uretilen/mavlink/ altına üretilir; içerme biçimi:
#         #include "mavlink/ida/mavlink.h"
#     Üretilen dialect dizininin adı (`ida`), çekirdeğin `ida/` include önekiyle çakışmasın
#     diye `mavlink/` altına konur.
#
# Üreteç Python 3 ve pymavlink'tir (yalnız derleme zamanında; iskelet §7). Protokol
# kütüphanesi işi (W2) bu işlevi kullanabilir ya da kendi üretim adımını kurabilir.

include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/IdaBagimliliklar.cmake")

function(ida_mavlink_uret hedef)
  find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
  ida_pymavlink_kur()

  set(_xml "${PROJECT_SOURCE_DIR}/protocol/ida.xml")
  set(_kok "${CMAKE_BINARY_DIR}/protocol/uretilen")
  set(_cikti "${_kok}/mavlink")
  set(_damga "${_cikti}/ida/mavlink.h")

  add_custom_command(
    OUTPUT "${_damga}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_cikti}"
    COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/cmake/mavgen_calistir.py"
            "${IDA_PYMAVLINK_KOK}" "${_cikti}" "${_xml}"
    DEPENDS "${_xml}" "${PROJECT_SOURCE_DIR}/cmake/mavgen_calistir.py"
    COMMENT "MAVLink C başlıkları üretiliyor (protocol/ida.xml, pymavlink ${IDA_PYMAVLINK_SURUMU})"
    VERBATIM)
  add_custom_target(${hedef}_uret DEPENDS "${_damga}")

  add_library(${hedef} INTERFACE)
  # Üretilen kod İDA uyarı politikasının dışındadır (sistem başlığı).
  target_include_directories(${hedef} SYSTEM INTERFACE "${_kok}")
  add_dependencies(${hedef} ${hedef}_uret)
endfunction()
