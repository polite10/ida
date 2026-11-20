# vcpkg bağımlılık düzeneğinin duman testi: tek gerçek bağımlılık (Eigen, vcpkg.json).
# Seçim gerekçesi: 09 §2.6'da sim fiziğinin beklenen bağımlılığıdır, başlık-yalnızdır
# (iki işletim sisteminde de derleme süresi küçük) ve vcpkg'nin manifest + baseline +
# overrides zincirini uçtan uca sınar. Embree, Jolt, MCAP, zstd kendi işlerinde eklenir.

if(NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake$")
  message(STATUS "vcpkg etkin değil (VCPKG_ROOT yok): bağımlılık duman testi atlandı")
  return()
endif()

find_package(Eigen3 CONFIG REQUIRED)
message(STATUS "Eigen3 ${Eigen3_VERSION} (vcpkg)")

add_executable(ida_duman_eigen eigen_duman.cpp)
target_link_libraries(ida_duman_eigen PRIVATE Eigen3::Eigen)
# 09 §2.6: Eigen'de EIGEN_FAST_MATH=0.
target_compile_definitions(ida_duman_eigen PRIVATE EIGEN_FAST_MATH=0)
ida_ayar_uygula(ida_duman_eigen)
add_test(NAME host.duman.eigen COMMAND ida_duman_eigen)
