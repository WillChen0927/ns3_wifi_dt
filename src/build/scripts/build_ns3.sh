cd ../../ns-3/ns-3-allinone/ns-3.44
mkdir build
cd build
cmake .. -DNS3_ENABLE_EXAMPLES=ON -DNS3_ENABLE_TESTS=ON -DNS3_ENABLE_PYTHON_BINDINGS=ON
make -j$(nproc)