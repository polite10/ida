# mat/ modülünün kaynakları. Sahibi: W2 çekirdek iskeleti.
# core/CMakeLists.txt bu dosyayı içerir; modüle yeni kaynak yalnız buraya eklenir.
target_sources(ida_core PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/mat.c"
  "${CMAKE_CURRENT_LIST_DIR}/ida_math.c"
)
