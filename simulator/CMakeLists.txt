cmake_minimum_required(VERSION 3.16)
project(casio_clock_simulator LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

include(FetchContent)
FetchContent_Declare(lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG v8.4.0
  GIT_SHALLOW TRUE)
FetchContent_Declare(lv_drivers
  GIT_REPOSITORY https://github.com/lvgl/lv_drivers.git
  GIT_TAG v8.3.0
  GIT_SHALLOW TRUE)

set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
set(LV_DRV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_drv_conf.h CACHE STRING "" FORCE)
FetchContent_MakeAvailable(lvgl lv_drivers)

add_executable(casio-clock-sim
  main.cpp
  clock_platform_sim.cpp
  ../src/clock_ui.cpp
  ../src/seven_segment.cpp
  ../src/seven_segment_assets.cpp
  ../src/weekday_segment.cpp
  ../src/weekday_segment_assets.cpp
)
target_compile_definitions(casio-clock-sim PRIVATE LV_CONF_INCLUDE_SIMPLE)
target_include_directories(casio-clock-sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(casio-clock-sim PRIVATE lvgl lv_drivers PkgConfig::SDL2)
