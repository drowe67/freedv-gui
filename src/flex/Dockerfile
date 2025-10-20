FROM ubuntu:24.04 AS freedv-waveform

# Install prerequisites to allow AppImages to run
RUN apt-get update && apt-get install libexpat1

COPY FreeDV-FlexRadio.AppImage /app/FreeDV-FlexRadio.AppImage

RUN \
    cd /app && \
    ./FreeDV-FlexRadio.AppImage --appimage-extract && \
    mv squashfs-root/* . && \
    rm -rf FreeDV-FlexRadio.AppImage squashfs-root

LABEL com.flexradio.waveform.name=freedv-waveform
LABEL com.flexradio.waveform.version=1.0.0

ENV APPDIR=/app
ENTRYPOINT exec /app/AppRun
