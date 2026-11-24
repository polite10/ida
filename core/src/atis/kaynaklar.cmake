# atis/ modülünün kaynakları. Sahibi: W5-atis-emniyet (atış kontrol emniyet zinciri).
# core/CMakeLists.txt bu dosyayı içerir; modüle yeni kaynak yalnız buraya eklenir.
target_sources(ida_core PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/atis.c"
  "${CMAKE_CURRENT_LIST_DIR}/atis_emniyet.c"
)
