# sonar/ modülünün kaynakları. Sahibi: W4 sonar işi.
# core/CMakeLists.txt bu dosyayı içerir; modüle yeni kaynak yalnız buraya eklenir.
target_sources(ida_core PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/sonar.c"
)
