# param/ modülünün kaynakları. Sahibi: W2 çekirdek iskeleti.
# core/CMakeLists.txt bu dosyayı içerir; modüle yeni kaynak yalnız buraya eklenir.
target_sources(ida_core PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/param.c"
  "${CMAKE_CURRENT_LIST_DIR}/param_komut.c"
  "${CMAKE_CURRENT_LIST_DIR}/ozet.c"
  "${CMAKE_CURRENT_LIST_DIR}/crc32.c"
)
