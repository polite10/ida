# ida_link_ust simge tablosu denetimi (cmake -P; protokol.simge.ida_link_ust).
#
#   cmake -DNM=<nm> -DARSIV=<libida_link_ust.a> -DTEL=<libida_link.a> -DMACHO=<0|1>
#         -P simge_denetimi_ust.cmake
#
# Denetlenen (A1-K101; W3-ortak "değişebilir genel durum yok"):
#   1. Yazılabilir veri (D/d), sıfır dolgulu veri (B/b), ortak (C) ya da küçük veri (G/g)
#      simgesi tanımlı değil.
#   2. MAVLink kanal durumu ve ona dokunan kitaplık işlevleri ne tanımlı ne başvurulu.
#   3. Dış başvurular yalnız: aynı arşivde tanımlı simgeler, tel katmanının (ida_link) tanımlı
#      simgeleri, bellek kopyalama/doldurma, yığın koruyucu ve derleyici yardımcıları.
#      İşletim sistemi, saat, dosya, stdio ve ayırma başvurusu yok.
foreach(_d NM ARSIV TEL)
  if(NOT DEFINED ${_d})
    message(FATAL_ERROR "simge_denetimi_ust.cmake: ${_d} verilmedi")
  endif()
endforeach()

set(_izinli_dis
  "^(memcpy|memset|memmove|memcmp|bzero|__bzero|__memcpy_chk|__memset_chk|__memmove_chk|__stack_chk_fail|__stack_chk_guard|__aeabi_mem(cpy|set|move|clr)[0-9]*|__aeabi_u?(i|l)div(mod)?|__aeabi_l(asr|lsl|lsr|mul)|__aeabi_ul?cmp)$")
set(_yasak_ad
  "(m_mavlink_status|m_mavlink_buffer|mavlink_get_channel_|mavlink_reset_channel|mavlink_parse_char|mavlink_frame_char|mavlink_finalize_message|_mav_finalize_message_chan_send|_chan$|_send$|mavlink_set_proto_version|mavlink_get_proto_version)")

# nm çıktısını (tür, ad) listelerine ayırır.
function(_nm_oku arsiv tanimli_cikti dis_cikti bulgu_cikti)
  execute_process(COMMAND "${NM}" "${arsiv}"
    OUTPUT_VARIABLE _cikti ERROR_VARIABLE _hata RESULT_VARIABLE _rc)
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "nm başarısız (${_rc}): ${_hata}")
  endif()
  set(_tanimli "")
  set(_dis "")
  set(_bulgu "")
  string(REPLACE "\r" "" _cikti "${_cikti}")
  string(REPLACE "\n" ";" _satirlar "${_cikti}")
  foreach(_s IN LISTS _satirlar)
    if(_s STREQUAL "" OR _s MATCHES ":$")
      continue()
    endif()
    if(NOT _s MATCHES "^([0-9a-fA-F]+)? +([A-Za-z?-]) (.+)$")
      string(APPEND _bulgu "  tanınmayan nm satırı: ${_s}\n")
      continue()
    endif()
    set(_tur "${CMAKE_MATCH_2}")
    set(_ad "${CMAKE_MATCH_3}")
    # Mach-O'nun ek alt çizgisi yalnız hedef Mach-O iken soyulur (MACHO yapılandırmadan gelir;
    # -P kipinde APPLE derleyen makineyi gösterir, kart arşivinde yanlış olurdu).
    if(MACHO AND _ad MATCHES "^_")
      string(SUBSTRING "${_ad}" 1 -1 _ad)
    endif()
    if(_ad MATCHES "${_yasak_ad}")
      string(APPEND _bulgu "  MAVLink kanal simgesi (A1-K101): ${_tur} ${_ad}\n")
    endif()
    if(_tur MATCHES "^[Uu]$")
      list(APPEND _dis "${_ad}")
    elseif(NOT _tur MATCHES "^[wvWV]$")
      list(APPEND _tanimli "${_ad}")
      if(_tur MATCHES "^[DdBbCcGg]$")
        string(APPEND _bulgu "  değişebilir durum simgesi: ${_tur} ${_ad}\n")
      endif()
    endif()
  endforeach()
  set(${tanimli_cikti} "${_tanimli}" PARENT_SCOPE)
  set(${dis_cikti} "${_dis}" PARENT_SCOPE)
  set(${bulgu_cikti} "${_bulgu}" PARENT_SCOPE)
endfunction()

_nm_oku("${ARSIV}" _ust_tanimli _ust_dis _bulgu)
_nm_oku("${TEL}" _tel_tanimli _tel_dis _tel_bulgu)

list(LENGTH _ust_tanimli _n_tanimli)
if(_n_tanimli EQUAL 0)
  string(APPEND _bulgu "  hiç tanımlı simge okunamadı (nm çıktısı beklenmedik)\n")
endif()
set(_n_dis 0)
list(REMOVE_DUPLICATES _ust_dis)
foreach(_ad IN LISTS _ust_dis)
  if(_ad MATCHES "${_izinli_dis}")
    math(EXPR _n_dis "${_n_dis} + 1")
    continue()
  endif()
  list(FIND _ust_tanimli "${_ad}" _i)
  if(NOT _i EQUAL -1)
    continue()
  endif()
  list(FIND _tel_tanimli "${_ad}" _i)
  if(NOT _i EQUAL -1 AND _ad MATCHES "^ida_link_")
    math(EXPR _n_dis "${_n_dis} + 1")
    continue()
  endif()
  string(APPEND _bulgu "  izin listesinde olmayan dış başvuru: ${_ad}\n")
endforeach()

if(NOT _bulgu STREQUAL "")
  message(FATAL_ERROR "[SIMGE_DENETIMI] ${ARSIV}:\n${_bulgu}")
endif()
message(STATUS "simge denetimi temiz (ida_link_ust): ${_n_tanimli} tanımlı, ${_n_dis} dış "
               "başvuru; yazılabilir veri yok, kanal simgesi yok")
