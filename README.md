# rs-simple-tools

- For Windows 10 ONLY

## Building
### dependencies

#### git for windows @latest
- https://gitforwindows.org/

#### cmake @latest
- https://cmake.org/download/

#### librealsense @v2.35.2
- Execute floowing commands on command prompt
```
git submodule update --init --recursive
cd third-party/realsense2
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=false -DBUILD_GRAPHICAL_EXAMPLES=false -DBUILD_SHARED_LIBS=false -DCMAKE_INSTALL_PREFIX="C:\Workspace\third-party\librealsense"
cmake --build . --target install
cd ../../../
```
### tools
```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target ALL_BUILD
```

Built tools will be in `build\tools\***\Release`
