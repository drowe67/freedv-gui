#!/bin/sh

function delete_pkg_if_universal2_downloaded() {
    ARCH=$1
    cd $ARCH
    for i in `ls *.whl | awk -F - '{ print $1; }'`; do
        if [ -e ../$i-*universal2.whl ]; then 
            FILE=$i-*.whl
            echo "Removing $FILE"
            rm $FILE
        fi
    done
    cd ..
}

function create_universal2_wheel() {
    cd arm64
    for i in `ls *.whl | awk -F - '{ print $1; }'`; do
        echo "Creating universal2 wheel for $i"
        ARM_WHEEL=$i-*whl
        X86_WHEEL=../x86_64/$i-*whl
        delocate-merge -w .. $ARM_WHEEL $X86_WHEEL
    done
    cd ..
}

mkdir pkg-tmp
cd pkg-tmp
../Python.framework/Versions/Current/bin/python3 -m venv pkg-venv
. ./pkg-venv/bin/activate

pip3 install delocate 
cp ../delocating.py.patched pkg-venv/lib/python3.12/site-packages/delocate/delocating.py # >0.10.7 has bugs with numpy, see https://github.com/matthew-brett/delocate/issues/229

# Download RADE dependencies appropriate for both architectures (x86_64 and arm64)

mkdir arm64
pip3 download --platform macosx_11_0_arm64 --python-version 3.12 --only-binary=:all: filelock typing-extensions>=4.10.0 setuptools sympy>=1.13.3 networkx jinja2 fsspec optree>=0.13.0 opt-einsum>=3.3 numpy>=2 matplotlib
wget -O arm64/torch-2.7.1a0+gite2d141d-cp312-cp312-macosx_11_0_arm64.whl https://k6aq.net/freedv-build/torch-2.7.1a0+gite2d141d-cp312-cp312-macosx_11_0_arm64.whl
mv *arm64.whl arm64

mkdir x86_64
pip3 download --platform macosx_11_0_x86_64 --python-version 3.12 --only-binary=:all: filelock typing-extensions>=4.10.0 setuptools sympy>=1.13.3 networkx jinja2 fsspec optree>=0.13.0 opt-einsum>=3.3 numpy>=2 matplotlib
wget -O x86_64/torch-2.7.1a0+gite2d141d-cp312-cp312-macosx_11_0_x86_64.whl https://k6aq.net/freedv-build/torch-2.7.1a0+gite2d141d-cp312-cp312-macosx_11_0_x86_64.whl
mv *x86_64.whl x86_64

delete_pkg_if_universal2_downloaded "x86_64"
delete_pkg_if_universal2_downloaded "arm64"
create_universal2_wheel

rm -rf arm64 x86_64
cd ..
