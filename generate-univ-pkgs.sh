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
# Note: PyTorch 2.2.2 is the last one with binaries for both x86 and ARM.

mkdir arm64
pip3 download --platform macosx_11_0_arm64 --python-version 3.12 --only-binary=:all: numpy==1.26.4 torch==2.2.2 torchaudio==2.2.2 matplotlib --extra-index-url https://download.pytorch.org/whl/cpu
mv *arm64.whl arm64

mkdir x86_64
pip3 download --platform macosx_11_0_x86_64 --python-version 3.12 --only-binary=:all: numpy==1.26.4 torch==2.2.2 torchaudio==2.2.2 matplotlib --extra-index-url https://download.pytorch.org/whl/cpu
mv *x86_64.whl x86_64

delete_pkg_if_universal2_downloaded "x86_64"
delete_pkg_if_universal2_downloaded "arm64"
create_universal2_wheel

rm -rf arm64 x86_64
cd ..
