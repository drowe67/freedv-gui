# FlexRadio waveform support

This folder contains source code for the FlexRadio FreeDV waveform, supporting the 8000 and Aurora series directly on the radio
hardware (and 6000 series when run on a separate Raspberry Pi).a

## Building the waveform

Run `cmake` on the top-level CMakeLists.txt as usual (see top-level README.md), then run

```
$ make -j$(nproc) freedv-flex
```

*Note: freedv-flex is not built by default when running "make" or using `build_linux.sh`.*

To generate the AppImage, you can then change to the top-level `appimage` folder and run `./make-appimage-waveform.sh`.
Note that AppImage generation is a prerequisite for generating the Docker container, which can be done by
executing the following from this folder:

```
$ cp ../../appimage/FreeDV-FlexRadio-...-aarch64.AppImage .
$ ./FreeDV-FlexRadio-...-aarch64.AppImage --appimage-extract
$ docker buildx build --output type=oci,compression=gzip,dest=freedv-waveform.tar.gz --tag=freedv-waveform .
```

## Executing the waveform

The waveform can be run similarly to the main FreeDV application. From your build folder:

```
$ PYTHONPATH=$(pwd)/rade_src src/flex/freedv-flex
```

By default, it will listen on the network until it receives a broadcast packet from a supported radio,
then connect to that radio. To override this, use the `SSDR_RADIO_ADDRESS` environment variable with
your radio's IP address:

```
$ SSDR_RADIO_ADDRESS=192.168.1.2 PYTHONPATH=$(pwd)/rade_src src/flex/freedv-flex
```

To ensure proper reporting to FreeDV Reporter by the waveform, make sure that your callsign is properly 
configured in SmartSDR (i.e. it doesn't say "FLEX" when first starting).

## Questions/issues

Please contact the [FreeDV mailing list](https://groups.google.com/g/digitalvoice).
