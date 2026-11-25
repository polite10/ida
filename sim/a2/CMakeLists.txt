# sim/a2 — A2 sunucusu, sim tarafı (W5-a2-sunucu dilim 1): sim ↔ görselleştirici.
#
#   ida_a2              kitaplık (C++20); dış bağımlılık yok; `viz/`, `sim/a3`, `sim/kopru` ve
#                       `sim/sunucu` hedeflerine bağlanmaz (W5-K29)
#   ida_a2_sahte_dunya  sahte veri kaynağı (fizik değil)
#   ida_a2_sahte        kitaplığı sahte kaynakla çalıştıran program
#   ida_a2_test         CTest testleri (sim.a2.*); karşı uç olarak `viz/a2`'nin kodeğini ve
#                       istemci oturumunu (`ida_viz_a2`) kullanır

find_package(Threads REQUIRED)

# GCC/Clang'da -Wshadow (MSVC /W4'teki C4456–C4459'un karşılığı).
function(ida_a2_ayar hedef)
  ida_ayar_uygula(${hedef})
  if(NOT MSVC)
    target_compile_options(${hedef} PRIVATE -Wshadow)
  endif()
endfunction()

add_library(ida_a2 STATIC
  src/cekirdek.cpp
  src/kilit.cpp
  src/kodlayici.cpp
  src/saat.cpp
  src/sha256.cpp
  src/soket.cpp
  src/sunucu.cpp
  src/yayinci.cpp)
target_include_directories(ida_a2
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/sim/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ida_a2 PUBLIC Threads::Threads)
if(WIN32)
  target_link_libraries(ida_a2 PUBLIC ws2_32)
endif()
ida_a2_ayar(ida_a2)

add_library(ida_a2_sahte_dunya STATIC sahte/sahte_dunya.cpp)
target_include_directories(ida_a2_sahte_dunya PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/sahte")
target_link_libraries(ida_a2_sahte_dunya PUBLIC ida_a2)
ida_a2_ayar(ida_a2_sahte_dunya)

add_executable(ida_a2_sahte sahte/main.cpp)
target_include_directories(ida_a2_sahte PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ida_a2_sahte PRIVATE ida_a2_sahte_dunya)
ida_a2_ayar(ida_a2_sahte)

add_subdirectory(test)
