ARG FED_REL=34

FROM fedora:${FED_REL}

# Build & Test Linux: cmake build-essential git libspeexdsp-dev libsamplerate0-dev octave-common octave gnuplot sox ca-cacert octave-signal
# Build OpenOCD: automake libtool libusb-1.0-0-dev wget which?
# Build & Test STM32: libc6-i386 p7zip-full python3-numpy bc  
# tar: bzip2
# arm-none-eabi-gdb: ncurses-compat-libs

RUN dnf -y install --setopt=install_weak_deps=False @development-tools cmake git speexdsp-devel libsamplerate-devel octave octave-signal gnuplot sox python3-numpy automake libtool libusb1-devel wget bc glibc.i686 which bzip2 ncurses-compat-libs gcc gcc-c++ && useradd -m build 

# specific for windows mingw build
RUN dnf install -y dnf-plugins-core
RUN dnf -y copr enable hobbes1069/mingw
RUN dnf install -y mingw{32,64}-filesystem mingw{32,64}-binutils mingw{32,64}-gcc mingw{32,64}-crt mingw{32,64}-headers mingw32-nsis
RUN dnf install -y mingw{32,64}-speex mingw{32,64}-wxWidgets3 mingw{32,64}-hamlib mingw{32,64}-portaudio mingw{32,64}-libsndfile mingw{32,64}-libsamplerate.noarch svn


# finally, this is the build script.
COPY entrypoint.sh /

USER build

ENTRYPOINT ["/entrypoint.sh"]
