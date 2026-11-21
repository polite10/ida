# sim/ — simülatör sunucusu (C++20). Yalnız PC kökünde (IDA_HEDEF=host) derlenir.
# Alt dizinler kendi işlerinde eklenir.

add_subdirectory(fizik)  # fizik çekirdeği (W3-fizik)
add_subdirectory(senaryo)  # A5 çözümleyici ve doğrulayıcı (W4-senaryo)
