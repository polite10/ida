# olay/ modülünün kaynakları. Sahibi: W2 çekirdek iskeleti (genişletici olay üretimi W4 atış kontrol işinin).
# core/CMakeLists.txt bu dosyayı içerir; modüle yeni kaynak yalnız buraya eklenir.
target_sources(ida_core PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/olay.c"
)
