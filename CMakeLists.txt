cmake_minimum_required(VERSION 3.5.0)
project(VideoPlayer)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_WIN32_EXECUTABLE FALSE)

set(GTK3_INCLUDE_DIRS "C:/Users/David/Documents/DevProjects/msys64/mingw64/include")
set(GTK3_LIBRARY_DIRS "C:/Users/David/Documents/DevProjects/msys64/mingw64/lib")
set(ENV{PKG_CONFIG_PATH} "C:/Users/David/Documents/DevProjects/msys64/mingw64/lib/pkgconfig")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

include_directories(${GTK4_INCLUDE_DIRS})
link_directories(${GTK4_LIBRARY_DIRS})
add_definitions(${GTK4_CFLAGS_OTHER})

find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
find_library(AVCODEC_LIBRARY avcodec)

find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
find_library(AVFORMAT_LIBRARY avformat)

find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
find_library(AVUTIL_LIBRARY avutil)

find_path(AVUTIL_INCLUDE_DIR libavutil/imgutils.h)
find_library(AVUTIL_LIBRARY imgutils)

find_path(AVDEVICE_INCLUDE_DIR libavdevice/avdevice.h)
find_library(AVDEVICE_LIBRARY avdevice)

find_path(SWSCALE_INCLUDE_DIR libswscale/swscale.h)
find_library(SWSCALE_LIBRARY swscale)

find_path(GTK3_INCLUDE_DIRS SDL2/SDL.h)
find_library(SDL_LIBRARY SDL2)

#find_path(GTK3_INCLUDE_DIRS portaudio.h)
#find_library(PORTAUDIO_LIBRARY portaudio)

add_executable(videoplayer test.c)
#add_executable(videoplayer FinalProject.c)
#add_executable(videoplayer main.c)


#set_target_properties(videoplayer PROPERTIES WIN32_EXECUTABLE FALSE)  # If you dont want console to run with your .exe
target_link_libraries(videoplayer ${GTK4_LIBRARIES} ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${AVDEVICE_LIBRARY} ${SWSCALE_LIBRARY} ${SDL_LIBRARY} ${SDL2_LIBRARIES} ${PORTAUDIO_LIBRARY})
target_include_directories(videoplayer PRIVATE ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${AVDEVICE_INCLUDE_DIR} ${SWSCALE_INCLUDE_DIR} ${SDL2_INCLUDE_DIRS})
