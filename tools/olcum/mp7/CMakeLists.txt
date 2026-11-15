cmake_minimum_required(VERSION 3.20)
project(mp7_sonar_suresi LANGUAGES CXX)

# W0-MP7a ölçüm prototipi (atılabilir). Ürün koduna taşınmaz.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Embree 4.4.x: Homebrew ya da IDA_CACHE altındaki kaynak derlemesi.
# Yol CMAKE_PREFIX_PATH ile verilir (kosu.sh bunu ayarlar).
find_package(embree 4 REQUIRED)

add_executable(mp7
  kaynak/main.cpp
  kaynak/sahne.cpp
  kaynak/sonar.cpp
  kaynak/havuz.cpp)
target_include_directories(mp7 PRIVATE kaynak)
target_link_libraries(mp7 PRIVATE embree)

# 09 §2.6 PC başlangıç politikası: FMA birleştirme ve fast-math kapalı.
target_compile_options(mp7 PRIVATE -O2 -ffp-contract=off -fno-fast-math -Wall -Wextra)
