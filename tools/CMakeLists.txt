set(PRECOMPILED_LIBS "C:/Workspace/third-party")

find_package(realsense2 REQUIRED HINTS ${PRECOMPILED_LIBS}/librealsense2)

add_subdirectory(rs_serial)
