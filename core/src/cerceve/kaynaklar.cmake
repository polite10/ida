# cerceve/ modülünün kaynakları. Sahibi: W2 çekirdek iskeleti.
# core/CMakeLists.txt bu dosyayı içerir; modüle yeni kaynak yalnız buraya eklenir.
target_sources(ida_core PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/cerceve.c"
  "${CMAKE_CURRENT_LIST_DIR}/komut.c"
  "${CMAKE_CURRENT_LIST_DIR}/cikti.c"
  "${CMAKE_CURRENT_LIST_DIR}/ornek.c"
  "${CMAKE_CURRENT_LIST_DIR}/imu_zaman.c"
)
