FROM alpine:latest AS freedv-waveform

COPY FreeDV-FlexRadio.AppImage /app/FreeDV-FlexRadio.AppImage

LABEL com.flexradio.waveform.name=freedv-waveform
LABEL com.flexradio.waveform.version=1.0.0

ENTRYPOINT exec /app/FreeDV-FlexRadio.AppImage
