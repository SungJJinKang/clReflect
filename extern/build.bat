
pushd .
mkdir llvm-build_x64
cd llvm-build_x64
cmake -G "Visual Studio 15 2017" -DCMAKE_GENERATOR_PLATFORM=x64 -DLLVM_ENABLE_PROJECTS="clang" ..\llvm\llvm
popd