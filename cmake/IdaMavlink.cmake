# protocol/ida.xml'den MAVLink 2 C başlıklarını üretir (W2-depo §2, sözleşme başlıkları
# denetimi; protokol kütüphanesi de kullanır).
#
#   ida_mavlink_uret(<hedef-adı>)
#     <hedef-adı>: üretilen başlıkları SYSTEM include yolu olarak taşıyan arayüz hedefi.
#     Başlıklar <ikili-dizin>/protocol/uretilen/mavlink/ altına üretilir; içerme biçimi:
#         #include "mavlink/ida/mavlink.h"
#     Üretilen dialect dizininin adı (`ida`), çekirdeğin `ida/` include önekiyle çakışmasın
#     diye `mavlink/` altına konur.
#
# Üretim derleme ağacında bir kez tanımlanır: ilk çağrı tek bir üretim komutu, bir üretim
# hedefi (`ida_mavlink_uret`) ve paylaşılan arayüz hedefi (`ida_mavlink_basliklari`) kurar.
# Sonraki çağrılar (başka dizinlerden de) yalnız <hedef-adı>'nı bu paylaşılan hedefe bağlar;
# <hedef-adı> zaten varsa hiçbir şey yapmaz. Arayüz hedefinin üretim bağımlılığı onu
# kullanan hedeflere geçer (CMake 3.19+: arayüz kitaplığına eklenen bağımlılık).
#
# Üreteç Python 3 ve pymavlink'tir (yalnız derleme zamanında; iskelet §7).

include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/IdaBagimliliklar.cmake")

function(_ida_mavlink_ortak_kur)
  if(TARGET ida_mavlink_basliklari)
    return()
  endif()
  find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
  ida_pymavlink_kur()

  get_filename_component(_xml "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../protocol/ida.xml" ABSOLUTE)
  set(_betik "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/mavgen_calistir.py")
  set(_kok "${CMAKE_BINARY_DIR}/protocol/uretilen")
  set(_cikti "${_kok}/mavlink")
  set(_damga "${_cikti}/ida/mavlink.h")

  add_custom_command(
    OUTPUT "${_damga}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_cikti}"
    COMMAND "${Python3_EXECUTABLE}" -I "${_betik}" "${IDA_PYMAVLINK_KOK}" "${_cikti}" "${_xml}"
    DEPENDS "${_xml}" "${_betik}"
    COMMENT "MAVLink C başlıkları üretiliyor (protocol/ida.xml, pymavlink ${IDA_PYMAVLINK_SURUMU})"
    VERBATIM)
  add_custom_target(ida_mavlink_uret DEPENDS "${_damga}")

  add_library(ida_mavlink_basliklari INTERFACE)
  # Üretilen kod İDA uyarı politikasının dışındadır (sistem başlığı).
  target_include_directories(ida_mavlink_basliklari SYSTEM INTERFACE "${_kok}")
  add_dependencies(ida_mavlink_basliklari ida_mavlink_uret)
endfunction()

function(ida_mavlink_uret hedef)
  _ida_mavlink_ortak_kur()
  if(TARGET ${hedef} OR hedef STREQUAL "ida_mavlink_basliklari")
    return()
  endif()
  add_library(${hedef} INTERFACE)
  target_link_libraries(${hedef} INTERFACE ida_mavlink_basliklari)
endfunction()
