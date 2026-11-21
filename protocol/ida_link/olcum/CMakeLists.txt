# ida_link maliyet ölçümü (W2-protokol §5). Çekirdek döngü (olcum_cekirdek.c) saat ve
# işletim sistemi kullanmaz; iki kökte de derlenir (kartta koşturma ve MP5 ölçümü ayrı iş).
# PC sürücüsü (olcum_host.c) yalnız host kökünde: ida_link_olcum [yineleme].

add_library(ida_link_olcum_cekirdek STATIC olcum_cekirdek.c)
target_include_directories(ida_link_olcum_cekirdek PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ida_link_olcum_cekirdek
  PUBLIC ida_link ida_link_altin_veri
  PRIVATE ida_mavlink_uretilen)
ida_ayar_uygula(ida_link_olcum_cekirdek CEKIRDEK)

if(NOT CMAKE_CROSSCOMPILING)
  add_executable(ida_link_olcum olcum_host.c)
  target_link_libraries(ida_link_olcum PRIVATE ida_link_olcum_cekirdek)
  ida_ayar_uygula(ida_link_olcum CEKIRDEK)
  # Duman: program kurulur, beklenen çerçeve sayısını teslim eder (süre değerlendirilmez).
  add_test(NAME protokol.olcum.duman COMMAND ida_link_olcum 10)
endif()
