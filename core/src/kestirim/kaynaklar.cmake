# kestirim/ modülünün kaynakları. Sahibi: W3 kestirim işi.
# core/CMakeLists.txt bu dosyayı içerir; modüle yeni kaynak yalnız buraya eklenir.
target_sources(ida_core PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/kestirim.c"
)
