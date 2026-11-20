# Sözleşme başlıkları derleme denetimi (W2-depo §2; kabul md.3).
#
# Başlıkların kendisi değiştirilmez; her biri ayrı çeviri biriminde, İDA uyarı politikasıyla
# (uyarı hatadır) derlenir. Başlıklardaki static_assert yerleşim denetimleri de böylece her
# derleyicide koşar.
#
#   C17   : port.h, algilama_kaydi.h (tek başına), port.h + üretilen MAVLink başlığı
#           -> host Clang, MSVC, arm-none-eabi-gcc
#   C++20 : port.h, algilama_kaydi.h, a2_wire.h, port.h + üretilen MAVLink başlığı
#           -> host Clang, MSVC
#
# Hedef bir nesne kitaplığıdır (yalnız derleme; bağlama yok) ve ALL'a girer: herhangi bir
# başlık bir derleyicide derlenmezse derlemenin kendisi başarısız olur.
#
# IDA_MAVLINK_DENETIMI kapatılırsa MAVLink birleşimi atlanır (üreteç kurulamayan ortam için).

option(IDA_MAVLINK_DENETIMI "port.h ile üretilen MAVLink başlığını aynı çeviri biriminde derle" ON)

set(_c17 c17_port.c c17_algilama_kaydi.c)
set(_cxx20 cxx20_port.cpp cxx20_algilama_kaydi.cpp cxx20_a2_wire.cpp)
if(IDA_MAVLINK_DENETIMI)
  list(APPEND _c17 c17_port_mavlink.c)
  list(APPEND _cxx20 cxx20_port_mavlink.cpp)
endif()

if(IDA_HEDEF STREQUAL "fw")
  set(_kaynaklar ${_c17})
else()
  set(_kaynaklar ${_c17} ${_cxx20})
endif()

add_library(ida_sozlesme_basliklari OBJECT ${_kaynaklar})
target_include_directories(ida_sozlesme_basliklari PRIVATE
  "${PROJECT_SOURCE_DIR}/core/include"
  "${PROJECT_SOURCE_DIR}/sim/include")
ida_ayar_uygula(ida_sozlesme_basliklari CEKIRDEK)

if(IDA_MAVLINK_DENETIMI)
  include(IdaMavlink)
  ida_mavlink_uret(ida_mavlink_uretilen)
  target_link_libraries(ida_sozlesme_basliklari PRIVATE ida_mavlink_uretilen)
  add_dependencies(ida_sozlesme_basliklari ida_mavlink_uretilen_uret)
endif()
