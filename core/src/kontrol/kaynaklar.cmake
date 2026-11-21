# kontrol/ modülünün kaynakları. Sahibi: W3 kontrol işi.
# core/CMakeLists.txt bu dosyayı içerir; modüle yeni kaynak yalnız buraya eklenir.
target_sources(ida_core PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/kontrol.c"
  "${CMAKE_CURRENT_LIST_DIR}/tahsis.c"
)
