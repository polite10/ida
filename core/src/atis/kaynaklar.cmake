# atis/ modülünün kaynakları. Sahibi: W4 atış kontrol işi.
# core/CMakeLists.txt bu dosyayı içerir; modüle yeni kaynak yalnız buraya eklenir.
target_sources(ida_core PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/atis.c"
)
