# İDA overlay port'u: toml++ 3.4.0 + YALNIZ üst akımda birleşmiş düzeltmeler (W4-mac-ci).
#
# Bu dosya microsoft/vcpkg'nin ports/tomlplusplus/portfile.cmake'inin (3.4.0#1) kopyasıdır;
# tek fark PATCHES listesine eklenen dört üst akım yamasıdır. Kaynak arşiv ve SHA512 değişmedi
# (v3.4.0). Yamalar marzer/tomlplusplus'ta birleşmiş commit'lerin include/ kısmıdır; her
# yamanın başında commit adresi yazılıdır. Kendi yazdığımız yama yoktur.
#   0001 https://github.com/marzer/tomlplusplus/commit/c4369ae1d8955cae20c4ab40b9813ef4b60e48be (PR #242)
#   0002 https://github.com/marzer/tomlplusplus/commit/e4bcc150894a3d0fc9e115263bc573aa1394b1ae (PR #301)
#   0003 https://github.com/marzer/tomlplusplus/commit/1e8829b793b66ad17011732a146b8077d379b011 (PR #305, #295)
#   0004 https://github.com/marzer/tomlplusplus/commit/1bf29cf3415908ffab1f59321ac03f9511dabb8d (PR #321)
# Gerekçe: 3.4.0'da bozuk girdi TOML_UNREACHABLE / TOML_ASSERT_ASSUME yollarına ulaşıyor
# (tanımsız davranış); düzeltmeler v3.5.0'da, ancak 3.5.x TOML 1.1.0'ı koşulsuz ayrıştırır
# (A5-K01 TOML 1.0.0 ister) ve vcpkg kaydında 3.5.x port'u yoktur. vcpkg 3.5.x'i (ya da A5
# TOML 1.1'i) aldığında bu overlay kaldırılır.
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO marzer/tomlplusplus
    REF "v${VERSION}"
    SHA512 c227fc8147c9459b29ad24002aaf6ab2c42fac22ea04c1c52b283a0172581ccd4527b33c1931e0ef0d1db6b6a53f9e9882c6d4231c7f3494cf070d0220741aa5
    HEAD_REF master
    PATCHES
        fix-android-fileapi.patch
        0001-pr242-dotted-key-depth.patch
        0002-pr301-table-header-key-start.patch
        0003-pr305-non-ascii-whitespace-ub.patch
        0004-pr321-parser-assert-assume.patch
)

vcpkg_configure_meson(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -Dbuild_tests=false
        -Dbuild_examples=false
)

vcpkg_install_meson()
vcpkg_fixup_pkgconfig()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/tomlplusplus)
# Fixup link lib name and multi-config
find_library(lib NAMES tomlplusplus PATHS "${CURRENT_PACKAGES_DIR}/lib" NO_DEFAULT_PATH REQUIRED)
cmake_path(GET lib FILENAME name)
vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/share/tomlplusplus/tomlplusplusConfig.cmake"
    [[(IMPORTED_LOCATION "..PACKAGE_PREFIX_DIR./lib/)[^"]*"]]
    " \\1${name}\""
    REGEX
)
if(NOT VCPKG_BUILD_TYPE)
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/share/tomlplusplus/tomlplusplusConfig.cmake"
        [[IMPORTED_LOCATION ("..PACKAGE_PREFIX_DIR.)(/lib/[^"]*")]]
        [[IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
          IMPORTED_LOCATION_DEBUG \1/debug\2
          IMPORTED_LOCATION_RELEASE \1\2]]
        REGEX
    )
endif()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
