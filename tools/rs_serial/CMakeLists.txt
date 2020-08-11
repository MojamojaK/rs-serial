
add_executable(rs_serial rs_serial.cpp)
target_compile_options(rs_serial PRIVATE /MT)
target_link_options(rs_serial PRIVATE /INCREMENTAL:NO /NODEFAULTLIB:MSVCRT)
target_include_directories(rs_serial PRIVATE ${realsense2_INCLUDE_DIR})
target_link_libraries(rs_serial ${realsense2_LIBRARY})
