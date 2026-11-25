# viz/ — görselleştirici (W3-viz, dilim 1).
#
# Her host derlemesinde (hafif, Godot'suz):
#   ida_viz_a2           A2 kodlaması, sahne durumu, ara değerleme, istemci oturumu (soketsiz)
#   ida_sahte_a2_cekirdek sahte A2 kaynağının senaryosu ve sunucu oturumu (soketsiz)
#   host.viz.*           birim testleri (CTest)
# Yalnız IDA_VIZ=ON iken (CI'da kapalı; godot-cpp derlemesi uzun sürer):
#   ida_viz_soket        bloklamayan TCP ve yeniden bağlanan istemci
#   ida_sahte_a2         sahte A2 kaynağı (çalıştırılabilir)
#   ida_viz_eklenti      Godot 4.7.2 GDExtension (godot-cpp 10.0.0-stable), viz/proje/bin/ altına
#   host.viz.yeniden_baglanma  soketli yeniden bağlanma testi

option(IDA_VIZ "Görselleştirici eklentisini (godot-cpp) ve soketli araçları derle" OFF)

if(MSVC AND IDA_VIZ)
  # godot-cpp MSVC'de statik CRT (/MT) ile derlenir (Godot'nun resmî derlemeleri gibi; hedef
  # makinede VC++ çalışma zamanı kurulumu gerekmez). Eklentiye bağlanan tüm viz hedefleri aynı
  # çalışma zamanını kullanmalı (LNK2038). Bu değişken yalnız bu dizindeki hedefleri etkiler.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

function(ida_viz_uyarilar hedef)
  ida_ayar_uygula(${hedef})
  if(NOT MSVC)
    # W3-ortak: yerelde -Wshadow ile de derlenir (MSVC'de C4456/C4457 karşılığı /W4'te).
    target_compile_options(${hedef} PRIVATE -Wshadow)
  endif()
endfunction()

# ---- A2 kitaplığı -----------------------------------------------------------------------
add_library(ida_viz_a2 STATIC
  a2/src/cerceve.cpp
  a2/src/dalga.cpp
  a2/src/kodek.cpp
  a2/src/oturum.cpp
  a2/src/saat.cpp
  a2/src/sahne.cpp
  a2/src/sha256.cpp)
target_include_directories(ida_viz_a2 PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/a2/include"
  "${PROJECT_SOURCE_DIR}/sim/include")
set_target_properties(ida_viz_a2 PROPERTIES POSITION_INDEPENDENT_CODE ON)
ida_viz_uyarilar(ida_viz_a2)

add_library(ida_sahte_a2_cekirdek STATIC
  arac/sahte-a2/senaryo.cpp
  arac/sahte-a2/sahte_sim.cpp)
target_include_directories(ida_sahte_a2_cekirdek PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/arac/sahte-a2")
target_link_libraries(ida_sahte_a2_cekirdek PUBLIC ida_viz_a2)
ida_viz_uyarilar(ida_sahte_a2_cekirdek)

# ---- birim testleri ---------------------------------------------------------------------
foreach(_t sha256 kodek cerceve sahne oturum dalga)
  add_executable(ida_viz_test_${_t} test/test_${_t}.cpp)
  target_link_libraries(ida_viz_test_${_t} PRIVATE ida_sahte_a2_cekirdek)
  target_include_directories(ida_viz_test_${_t} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test")
  ida_viz_uyarilar(ida_viz_test_${_t})
  add_test(NAME host.viz.${_t} COMMAND ida_viz_test_${_t})
endforeach()
target_compile_definitions(ida_viz_test_dalga PRIVATE
  "IDA_VIZ_DALGA_GOLGELENDIRICI=\"${CMAKE_CURRENT_SOURCE_DIR}/proje/golgelendiriciler/dalga.gdshaderinc\"")

if(NOT IDA_VIZ)
  return()
endif()

# ---- soketli araçlar --------------------------------------------------------------------
add_library(ida_viz_soket STATIC a2/soket/soket.cpp a2/soket/baglanti.cpp)
target_include_directories(ida_viz_soket PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/a2/soket")
target_link_libraries(ida_viz_soket PUBLIC ida_viz_a2)
if(WIN32)
  target_link_libraries(ida_viz_soket PUBLIC ws2_32)
endif()
set_target_properties(ida_viz_soket PROPERTIES POSITION_INDEPENDENT_CODE ON)
ida_viz_uyarilar(ida_viz_soket)

add_executable(ida_sahte_a2 arac/sahte-a2/ana.cpp)
target_link_libraries(ida_sahte_a2 PRIVATE ida_sahte_a2_cekirdek ida_viz_soket)
ida_viz_uyarilar(ida_sahte_a2)

add_executable(ida_viz_test_yeniden_baglanma test/test_yeniden_baglanma.cpp)
target_link_libraries(ida_viz_test_yeniden_baglanma PRIVATE ida_sahte_a2_cekirdek ida_viz_soket)
target_include_directories(ida_viz_test_yeniden_baglanma PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test")
ida_viz_uyarilar(ida_viz_test_yeniden_baglanma)
add_executable(ida_viz_test_okuma_butcesi test/test_okuma_butcesi.cpp)
target_link_libraries(ida_viz_test_okuma_butcesi PRIVATE ida_sahte_a2_cekirdek ida_viz_soket)
target_include_directories(ida_viz_test_okuma_butcesi PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test")
ida_viz_uyarilar(ida_viz_test_okuma_butcesi)
add_test(NAME host.viz.okuma_butcesi COMMAND ida_viz_test_okuma_butcesi)
# Godot başsız: varlık yükleyicisi (A7-K53 olumsuz kümeleri, REF, özet, önbellek, malzeme).
# Godot ikilisi yoksa betik 77 döner, test atlanır.
add_test(NAME host.viz.varlik_godot COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/test/varlik_godot.sh")
set_tests_properties(host.viz.varlik_godot PROPERTIES SKIP_RETURN_CODE 77
  FAIL_REGULAR_EXPRESSION "DENETİM BAŞARISIZ|SCRIPT ERROR")
add_test(NAME host.viz.yeniden_baglanma COMMAND ida_viz_test_yeniden_baglanma)

# ---- godot-cpp 10.0.0-stable (09 §2.6, 07 §2.10) ----------------------------------------
# Etiket 10.0.0-stable'ın işlemesi 507ed9d8 (GitHub API, 2026-11-21); arşiv özeti bu iş
# sırasında indirilen arşivden alındı. API sürümü Godot 4.7 (godot-cpp'nin kendi
# extension_api-4-7.json dosyası), tek duyarlık (Godot resmî ikilisiyle aynı).
set(GODOTCPP_API_VERSION "4.7" CACHE STRING "" FORCE)
set(GODOTCPP_PRECISION "single" CACHE STRING "" FORCE)
set(GODOTCPP_TARGET "template_debug" CACHE STRING "" FORCE)
set(GODOTCPP_SYSTEM_HEADERS ON CACHE BOOL "" FORCE)
set(GODOTCPP_WARNING_AS_ERROR OFF CACHE BOOL "" FORCE)
CPMAddPackage(
  NAME godot-cpp
  URL "https://github.com/godotengine/godot-cpp/archive/507ed9d840c01a3c5b2a39af8bb4000bfac30bf5.tar.gz"
  URL_HASH SHA256=30da4ac295997061a6af81ae531510efd4e6a86e506eb4466edc0898ef3fe048
  EXCLUDE_FROM_ALL YES)

add_library(ida_viz_eklenti SHARED
  eklenti/src/a2_istemci.cpp
  eklenti/src/kayit.cpp)
target_link_libraries(ida_viz_eklenti PRIVATE godot::cpp ida_viz_soket)
ida_viz_uyarilar(ida_viz_eklenti)
if(NOT MSVC)
  # godot-cpp istisnasız derlenir; eklenti de öyle.
  target_compile_options(ida_viz_eklenti PRIVATE -fno-exceptions)
else()
  # godot-cpp başlıkları SYSTEM'dir; MSVC'de /W4 /WX onların uyarılarını hata yapmasın.
  target_compile_options(ida_viz_eklenti PRIVATE /external:W0)
endif()
set(_viz_bin "${CMAKE_CURRENT_SOURCE_DIR}/proje/bin")
set_target_properties(ida_viz_eklenti PROPERTIES
  OUTPUT_NAME "ida_viz"
  LIBRARY_OUTPUT_DIRECTORY "${_viz_bin}"
  RUNTIME_OUTPUT_DIRECTORY "${_viz_bin}"
  LIBRARY_OUTPUT_DIRECTORY_DEBUG "${_viz_bin}"
  LIBRARY_OUTPUT_DIRECTORY_RELEASE "${_viz_bin}"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${_viz_bin}"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${_viz_bin}"
  CXX_VISIBILITY_PRESET hidden)
