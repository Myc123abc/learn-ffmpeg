project(ffmpeg)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LIBS
    "${CMAKE_CURRENT_SOURCE_DIR}/bin/avcodec.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/bin/avdevice.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/bin/avfilter.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/bin/avformat.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/bin/avutil.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/bin/swresample.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/bin/swscale.lib"
)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(${PROJECT_NAME} INTERFACE ${LIBS})