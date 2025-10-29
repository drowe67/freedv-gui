# FreeDV console application

This folder contains the source code for a console application that supports receiving int16 audio samples 
on standard input and outputs int16 audio whenever there's a valid RADE signal (at whatever sample rates are
best for your use case). This is intended for integration with an existing SDR radio (i.e. WebSDR or ka9q-radio).

## Building

Run `cmake` on the top-level CMakeLists.txt as usual (see top-level README.md), then run

```
$ make -j$(nproc) freedv-ka9q
```

*Note: freedv-ka9q is not built by default when running "make" or using `build_linux.sh`.*

## Running

Running without any arguments will use a default of 8000 Hz for input samples and 16000 Hz for output samples.
You can override these if preferred. For example, from the `build_linux` folder:

```
$ PYTHONPATH=$(pwd)/rade_src src/integrations/ka9q/freedv-ka9q -i 48000 -o 48000
```

This tool also supports FreeDV Reporter reporting. You must specify your callsign, grid square and reporting
frequency for the tool to actually connect to the FreeDV Reporter server. See below for more information.

## Command line arguments

```
mooneer@fedora:~/freedv-gui/build_linux$ src/integrations/ka9q/freedv-ka9q -h
13:21:37 INFO /home/mooneer/freedv-gui/src/integrations/ka9q/main.cpp:139: freedv-ka9q version 2.0.3-dev-645cd
13:21:37 INFO /home/mooneer/freedv-gui/src/integrations/ka9q/main.cpp:101: Usage: src/integrations/ka9q/freedv-ka9q [-i|--input-sample-rate RATE] [-o|--output-sample-rate RATE] [-c|--reporting-callsign CALLSIGN] [-l|--reporting-locator LOCATOR] [-f|--reporting-freq-hz FREQUENCY_IN_HERTZ] [-h|--help] [-v|--version]
13:21:37 INFO /home/mooneer/freedv-gui/src/integrations/ka9q/main.cpp:102:     -i|--input-sample-rate: The sample rate for int16 audio samples received over standard input (default 8000 Hz).
13:21:37 INFO /home/mooneer/freedv-gui/src/integrations/ka9q/main.cpp:103:     -o|--output-sample-rate: The sample rate for int16 audio samples output over standard output (default 16000 Hz).
13:21:37 INFO /home/mooneer/freedv-gui/src/integrations/ka9q/main.cpp:104:     -c|--reporting-callsign: The callsign to use for FreeDV Reporter reporting.
13:21:37 INFO /home/mooneer/freedv-gui/src/integrations/ka9q/main.cpp:105:     -l|--reporting-locator: The grid square/locator to use for FreeDV Reporter reporting.
13:21:37 INFO /home/mooneer/freedv-gui/src/integrations/ka9q/main.cpp:106:     -f|--reporting-frequency-hz: The frequency to report for FreeDV Reporter reporting, in hertz. (Example: 14236000 for 14.236MHz)
13:21:37 INFO /home/mooneer/freedv-gui/src/integrations/ka9q/main.cpp:107:     -h|--help: This help message.
13:21:37 INFO /home/mooneer/freedv-gui/src/integrations/ka9q/main.cpp:108:     -v|--version: Prints the application version and exits.
13:21:37 INFO /home/mooneer/freedv-gui/src/integrations/ka9q/main.cpp:109: 
13:21:37 INFO /home/mooneer/freedv-gui/src/integrations/ka9q/main.cpp:110: Note: Callsign, locator and frequency must all be provided for the application to connect to FreeDV Reporter.
mooneer@fedora:~/freedv-gui/build_linux$ 
```
