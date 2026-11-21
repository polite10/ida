# ida_link simge tablosu denetimi (cmake -P; protokol.simge.* testleri).
#
#   cmake -DNM=<nm> -DARSIV=<kitaplık .a> -P simge_denetimi.cmake
#
# Denetlenen (W2-protokol §1, §2; kabul md. 6; A1-K101):
#   1. Değişebilir genel ya da static durum yok: yazılabilir veri (D/d), sıfır dolgulu veri
#      (B/b) ve ortak (C) simgesi tanımlı değil. Salt okunur veri (R/r, Mach-O'da S/s) serbest.
#   2. MAVLink kanal durumu yok: kanal dizileri (m_mavlink_status, m_mavlink_buffer) ve
#      onlara dokunan kitaplık işlevleri (mavlink_get_channel_*, mavlink_parse_char,
#      mavlink_frame_char, mavlink_finalize_message*, *_chan, *_send) ne tanımlı ne başvurulu.
#   3. Dış başvurular yalnız izin listesinde: bellek kopyalama/doldurma, yığın koruyucu ve
#      derleyicinin yardımcıları. İşletim sistemi, saat, dosya, stdio ve ayırma başvurusu yok.
# Mach-O'nun ek alt çizgisi soyulur. nm hatası ya da tanınmayan satır başarısızlıktır.
foreach(_d NM ARSIV)
  if(NOT DEFINED ${_d})
    message(FATAL_ERROR "simge_denetimi.cmake: ${_d} verilmedi")
  endif()
endforeach()

execute_process(COMMAND "${NM}" "${ARSIV}"
  OUTPUT_VARIABLE _cikti ERROR_VARIABLE _hata RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "nm başarısız (${_rc}): ${_hata}")
endif()

set(_izinli_dis
  "^(memcpy|memset|memmove|memcmp|bzero|__bzero|__memcpy_chk|__memset_chk|__memmove_chk|__stack_chk_fail|__stack_chk_guard|__aeabi_mem(cpy|set|move|clr)[0-9]*|__aeabi_u?(i|l)div(mod)?|__aeabi_l(asr|lsl|lsr)|__aeabi_ul?cmp)$")
set(_yasak_ad
  "(m_mavlink_status|m_mavlink_buffer|mavlink_get_channel_|mavlink_reset_channel|mavlink_parse_char|mavlink_frame_char|mavlink_finalize_message|_mav_finalize_message_chan_send|_chan$|_send$|mavlink_set_proto_version|mavlink_get_proto_version)")

set(_bulgu "")
set(_n_tanimli 0)
set(_n_dis 0)
string(REPLACE "\r" "" _cikti "${_cikti}")
string(REPLACE "\n" ";" _satirlar "${_cikti}")
foreach(_s IN LISTS _satirlar)
  if(_s STREQUAL "" OR _s MATCHES ":$")
    continue()   # boş satır ya da arşiv üyesi başlığı
  endif()
  if(NOT _s MATCHES "^([0-9a-fA-F]+)? +([A-Za-z?-]) (.+)$")
    string(APPEND _bulgu "  tanınmayan nm satırı: ${_s}\n")
    continue()
  endif()
  set(_tur "${CMAKE_MATCH_2}")
  set(_ad "${CMAKE_MATCH_3}")
  if(APPLE AND _ad MATCHES "^_")
    # Yalnız bir alt çizgi (REGEX REPLACE "^_" hepsini soyardı).
    string(SUBSTRING "${_ad}" 1 -1 _ad)
  endif()
  if(_ad MATCHES "${_yasak_ad}")
    string(APPEND _bulgu "  MAVLink kanal simgesi (A1-K101): ${_tur} ${_ad}\n")
  endif()
  if(_tur MATCHES "^[UuwvWV]$")
    if(_tur MATCHES "^[Uu]$")
      math(EXPR _n_dis "${_n_dis} + 1")
      if(NOT _ad MATCHES "${_izinli_dis}")
        string(APPEND _bulgu "  izin listesinde olmayan dış başvuru: ${_ad}\n")
      endif()
    endif()
    continue()
  endif()
  math(EXPR _n_tanimli "${_n_tanimli} + 1")
  if(_tur MATCHES "^[DdBbCcGg]$")
    string(APPEND _bulgu "  değişebilir durum simgesi: ${_tur} ${_ad}\n")
  endif()
endforeach()

if(_n_tanimli EQUAL 0)
  string(APPEND _bulgu "  hiç tanımlı simge okunamadı (nm çıktısı beklenmedik)\n")
endif()
if(NOT _bulgu STREQUAL "")
  message(FATAL_ERROR "[SIMGE_DENETIMI] ${ARSIV}:\n${_bulgu}")
endif()
message(STATUS "simge denetimi temiz: ${_n_tanimli} tanımlı, ${_n_dis} dış başvuru; "
               "yazılabilir veri yok, kanal simgesi yok")
