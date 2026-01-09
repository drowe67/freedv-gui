# FlexRadio waveform support

This folder contains source code for the FlexRadio FreeDV waveform, supporting the 8000 and Aurora series directly on the radio
hardware (and 6000 series when run on a separate Raspberry Pi).

## Building the waveform

Run `cmake` on the top-level CMakeLists.txt as usual (see top-level README.md), then run

```
$ make -j$(nproc) freedv-flex
```

*Note: freedv-flex is not built by default when running "make" or using `build_linux.sh`.*

To generate the AppImage, you can then change to the top-level `appimage` folder and run `./make-appimage.sh freedv-flex`.
Note that AppImage generation is a prerequisite for generating the Docker container/waveform file, which can be done by
executing the following from this folder:

```
$ cp ../../appimage/FreeDV-FlexRadio-...-aarch64.AppImage .
$ ./FreeDV-FlexRadio-...-aarch64.AppImage --appimage-extract
$ docker buildx build --output type=oci,compression=gzip,dest=freedv-waveform.tar.gz --tag=freedv-waveform .
```

## Executing the waveform

The waveform can be run similarly to the main FreeDV application. From your build folder:

```
$ PYTHONPATH=$(pwd)/rade_integ_src src/integrations/flex/freedv-flex
```

By default, it will listen on the network until it receives a broadcast packet from a supported radio,
then connect to that radio. This may not be the radio you expect if you have multiple on your network.
To override this, use the `SSDR_RADIO_ADDRESS` environment variable with your desired radio's IP address:

```
$ SSDR_RADIO_ADDRESS=192.168.1.2 PYTHONPATH=$(pwd)/rade_src src/integrations/flex/freedv-flex
```

To ensure proper reporting to FreeDV Reporter by the waveform, make sure that your callsign is properly 
configured in SmartSDR (i.e. it doesn't say "FLEX" when first starting).

## Full list of options

```
13:42:10 INFO /home/mooneer/freedv-gui/src/integrations/flex/main.cpp:96: Usage: src/integrations/flex/freedv-flex [-d|--disable-reporting] [-l|--reporting-locator LOCATOR] [-m|--reporting-message MESSAGE] [-r|--rx-volume DB] [-t|--spot-timeout SEC] [-h|--help] [-v|--version]
13:42:10 INFO /home/mooneer/freedv-gui/src/integrations/flex/main.cpp:97:     -d|--disable-reporting: Disables FreeDV Reporter reporting.
13:42:10 INFO /home/mooneer/freedv-gui/src/integrations/flex/main.cpp:98:     -l|--reporting-locator: Overrides grid square/locator from radio for FreeDV Reporter reporting.
13:42:10 INFO /home/mooneer/freedv-gui/src/integrations/flex/main.cpp:99:     -m|--reporting-message: Sets reporting message for FreeDV Reporter reporting.
13:42:10 INFO /home/mooneer/freedv-gui/src/integrations/flex/main.cpp:100:     -r|--rx-volume: Increases or decreases receive volume by the provided dB figure.
13:42:10 INFO /home/mooneer/freedv-gui/src/integrations/flex/main.cpp:101:     -t|--spot-timeout: Timeout for reported spots (default: 600s or 10min).
13:42:10 INFO /home/mooneer/freedv-gui/src/integrations/flex/main.cpp:102:     -h|--help: This help message.
13:42:10 INFO /home/mooneer/freedv-gui/src/integrations/flex/main.cpp:103:     -v|--version: Prints the application version and exits.
```

## Questions/issues

Please contact the [FreeDV mailing list](https://groups.google.com/g/digitalvoice).
