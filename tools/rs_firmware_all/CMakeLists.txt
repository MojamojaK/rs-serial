
add_executable(rs_firmware_all rs_firmware_all.cpp)
set_target_properties(rs_firmware_all PROPERTIES COMPILE_FLAGS -DBUILDER_STATIC_DEFINE)
target_compile_options(rs_firmware_all PRIVATE /MT)
target_link_options(rs_firmware_all PRIVATE /INCREMENTAL:NO /NODEFAULTLIB:MSVCRT)
target_include_directories(rs_firmware_all PRIVATE ${realsense2_INCLUDE_DIR})
message(STATUS ${realsense2_LIBRARY})
target_link_libraries(rs_firmware_all ${realsense2_LIBRARY} -static)
