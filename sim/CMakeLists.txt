# sim/ — simülatör sunucusu (C++20). Yalnız PC kökünde (IDA_HEDEF=host) derlenir.
# Alt dizinler kendi işlerinde eklenir.

add_subdirectory(fizik)  # fizik çekirdeği (W3-fizik)
add_subdirectory(senaryo)  # A5 çözümleyici ve doğrulayıcı (W4-senaryo)
add_subdirectory(sil)    # kartsız koşucu (W4-sil)
add_subdirectory(sensor)  # sensör kümesi (W4-sensor)
add_subdirectory(kopru)  # A1 köprüsü (W4-kopru)
add_subdirectory(sunucu) # başsız ida_sim (W4-kopru)
add_subdirectory(sonar)   # sonar kümesi (W4-sonar)
add_subdirectory(a3)       # A3 sunucusu (W4-a3)
add_subdirectory(a2)       # A2 sunucusu (W5-a2-sunucu)
add_subdirectory(kayit)      # A6 kayıt kitaplığı (W5-kayit)
