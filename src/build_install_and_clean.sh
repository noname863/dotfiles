# /usr/bash

cd `dirname "$0"`

export CC=clang
export CXX=clang++

# gathering build dependencies
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# called to reset cmake from system, to one installed by pip
hash -r

# build
export CONAN_HOME=`pwd`/.conan
conan config install `pwd`/conan_profiles -tf profiles
mkdir -p build
conan install . -s build_type=Release -of build -b missing -pr:h profile -pr:b profile

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# install
cmake --install build

# clean

deactivate
rm -rf .venv
rm -rf .conan
rm -rf build
