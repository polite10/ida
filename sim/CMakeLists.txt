# sim/ — simülatör sunucusu (C++20). Yalnız PC kökünde (IDA_HEDEF=host) derlenir.
# Alt dizinler kendi işlerinde eklenir.

add_subdirectory(fizik)  # fizik çekirdeği (W3-fizik)
add_subdirectory(sensor)  # sensör kümesi (W4-sensor)
add_subdirectory(sil)    # kartsız koşucu (W4-sil)
add_subdirectory(kopru)  # A1 köprüsü (W4-kopru)
add_subdirectory(sunucu) # başsız ida_sim (W4-kopru)
