# tests/z2 — W4-z2 (Z2): Z1 senaryosunun kartta koşusunu (`ida_sim` kitaplığı, HIL 1x, seri) ve
# aynı senaryonun SIL karşılığını W3-zincir sınırlarıyla değerlendiren ölçüm programı.
# Kart gerektirdiği için CTest'e eklenmez; kullanım: docs/olcumler/W4-z2.md.
#
# Fizik Eigen'e (vcpkg) bağlıdır; vcpkg etkin değilse dizin atlanır (tests/butunleme ile aynı).

if(NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake$")
  message(STATUS "vcpkg etkin değil (VCPKG_ROOT yok): tests/z2 atlandı")
  return()
endif()

add_executable(ida_z2 ida_z2.cpp)
target_link_libraries(ida_z2 PRIVATE ida_sunucu ida_zincir ida_core)
ida_ayar_uygula(ida_z2)
if(NOT MSVC)
  target_compile_options(ida_z2 PRIVATE -Wshadow)
endif()
