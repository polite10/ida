# Ortak çekirdek: ida_core (statik, ISO C17, uzantı yok, işletim sistemi bağımlılığı yok).
# Aynı dosya PC kökünden (host-*) ve firmware kökünden (fw-*) alınır (09 §2.6).
#
# Bu sürümde çekirdek boştur: yalnız sözleşme başlıklarını (port.h, algilama_kaydi.h)
# derleyen bir yer tutucu kaynak vardır; çekirdek iskeleti işi (W2) gerçek kaynakları ekler.

add_library(ida_core STATIC
  src/iskelet.c)
target_include_directories(ida_core PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
ida_ayar_uygula(ida_core CEKIRDEK)
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
