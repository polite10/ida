# gorev/ modülünün kaynakları. Sahibi: W3 güdüm ve görev işi.
# core/CMakeLists.txt bu dosyayı içerir; modüle yeni kaynak yalnız buraya eklenir.
target_sources(ida_core PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/gorev.c"
  "${CMAKE_CURRENT_LIST_DIR}/kip.c"
  "${CMAKE_CURRENT_LIST_DIR}/ariza.c"
  "${CMAKE_CURRENT_LIST_DIR}/istek.c"
)
