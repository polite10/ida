# Ortak çekirdek: ida_core (statik, ISO C17, uzantı yok, işletim sistemi bağımlılığı yok).
# Aynı dosya PC kökünden (host-*) ve firmware kökünden (fw-*) alınır (09 §2.6).
#
# Modüller core/src/<modül>/ altındadır; her modülün kaynak listesi kendi kaynaklar.cmake
# parçasındadır (core/TASARIM.md §1). Modüle kaynak eklemek o parçada tek satırdır; bu dosyaya
# yalnız yeni modül eklenirken dokunulur. src/iskelet.c W2-depo'nun firmware yer tutucusunun
# çağırdığı işlevi sağlar (firmware/yer_tutucu/main.c).

add_library(ida_core STATIC
  src/iskelet.c)
target_include_directories(ida_core
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(IDA_CEKIRDEK_MODULLERI
  cerceve emniyet olay param mat telemetri
  kestirim gudum gorev kontrol atis sonar)
foreach(_modul IN LISTS IDA_CEKIRDEK_MODULLERI)
  include("${CMAKE_CURRENT_SOURCE_DIR}/src/${_modul}/kaynaklar.cmake")
endforeach()

ida_ayar_uygula(ida_core CEKIRDEK)
target_link_libraries(ida_core INTERFACE $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)  # float aşkın işlevleri (fmodf; newlib libm), çekirdeği bağlayan her hedefe
if(CMAKE_CROSSCOMPILING)
  # Yığın kullanımı raporu (09 §2.8); .su dosyaları nesnelerin yanında üretilir.
  target_compile_options(ida_core PRIVATE -fstack-usage)
endif()

# Çekirdek yalnız kendi başlıklarına ve standart C kitaplığına bağımlıdır: yapılandırmanın
# sonunda (include yolu, bağlantı, doğrudan içermeler) ve derlemeden sonra gerçek başlık
# bağımlılıkları üzerinden (CTest: cekirdek.siniri.basliklar) denetlenir.
ida_cekirdek_siniri_kur(ida_core
  KOKLER "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(BUILD_TESTING OR NOT DEFINED BUILD_TESTING)
  add_subdirectory(test)
endif()
