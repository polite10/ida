# ida_link maliyet ölçümü (W2-protokol §5) ve W3-tel-hiz önce/sonra karşılaştırması.
# Çekirdek döngü (olcum_cekirdek.c) saat ve işletim sistemi kullanmaz; iki kökte de derlenir
# (kartta koşturma ve MP5 ölçümü ayrı iş). PC sürücüsü (olcum_host.c) yalnız host kökünde:
# ida_link_olcum [yineleme].
#
# ida_link_tel_eski: W3-tel-hiz öncesi tel katmanının değiştirilmemiş kopyası (tel_eski.c;
# açık işlevleri ida_link_eski_ önekli). Ürüne bağlanmaz; yalnız önce/sonra ölçümü ve fark
# testi (tel_fark.c, kabul md. 2) içindir. İki kökte de derlenir (kartta önce/sonra ölçümü
# W3-kart işinin elindedir).

add_library(ida_link_tel_eski STATIC tel_eski.c)
target_include_directories(ida_link_tel_eski PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ida_link_tel_eski PUBLIC ida_link_uretilen_basliklar
  PRIVATE ida_mavlink_uretilen)
# tel.h'nin include yolu (ida_link'e bağlanmadan; eski kopya ürün kitaplığına bağımlı değildir).
target_include_directories(ida_link_tel_eski PUBLIC "${PROJECT_SOURCE_DIR}/protocol/ida_link/include")
ida_ayar_uygula(ida_link_tel_eski CEKIRDEK)

add_library(ida_link_olcum_cekirdek STATIC olcum_cekirdek.c)
target_include_directories(ida_link_olcum_cekirdek PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ida_link_olcum_cekirdek
  PUBLIC ida_link ida_link_tel_eski ida_link_altin_veri
  PRIVATE ida_mavlink_uretilen)
ida_ayar_uygula(ida_link_olcum_cekirdek CEKIRDEK)

if(NOT CMAKE_CROSSCOMPILING)
  add_executable(ida_link_olcum olcum_host.c)
  target_link_libraries(ida_link_olcum PRIVATE ida_link_olcum_cekirdek)
  ida_ayar_uygula(ida_link_olcum CEKIRDEK)
  # Duman: program kurulur, beklenen çerçeve sayısını teslim eder, eski ve yeni uygulamanın
  # özetleri aynıdır (süre değerlendirilmez).
  add_test(NAME protokol.olcum.duman COMMAND ida_link_olcum 10)

  # Fark testi (W3-tel-hiz kabul md. 2): eski ve yeni tel katmanı aynı girdide aynı sonucu
  # verir. CRC tablosu denetimi için src/'deki iç başlık da görülür.
  if(BUILD_TESTING OR NOT DEFINED BUILD_TESTING)
    ida_unity_kur()
    add_executable(ida_link_tel_fark tel_fark.c)
    target_include_directories(ida_link_tel_fark PRIVATE
      "${PROJECT_SOURCE_DIR}/protocol/ida_link/src")
    target_link_libraries(ida_link_tel_fark PRIVATE
      ida_link ida_link_tel_eski ida_link_altin_veri ida_mavlink_uretilen ida_unity)
    ida_ayar_uygula(ida_link_tel_fark CEKIRDEK)
    add_test(NAME protokol.fark.tel COMMAND ida_link_tel_fark)
  endif()
endif()
