FROM ubuntu:24.04 AS freedv-waveform

# Install prerequisites to allow AppImages to mount
RUN apt-get update && apt-get install libfuse2t64

COPY FreeDV-FlexRadio.AppImage /app/FreeDV-FlexRadio.AppImage

LABEL com.flexradio.waveform.name=freedv-waveform
LABEL com.flexradio.waveform.version=1.0.0

ENTRYPOINT exec /app/FreeDV-FlexRadio.AppImage
