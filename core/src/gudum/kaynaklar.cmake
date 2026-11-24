# gudum/ modülünün kaynakları. Sahibi: W3 güdüm ve görev işi.
# core/CMakeLists.txt bu dosyayı içerir; modüle yeni kaynak yalnız buraya eklenir.
target_sources(ida_core PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/gudum.c"
  "${CMAKE_CURRENT_LIST_DIR}/gudum_oto.c"
)
