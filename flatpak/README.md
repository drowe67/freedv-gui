# Flatpak packaging

This folder contains the files necessary to generate a Flatpak package for FreeDV. Flatpak
is a container-based packaging scheme available on most common Linux distros that enables
the generation of a single package that works on multiple systems with no modifications
required.

## Setting up a Flatpak build environment

Execute the following command (Debian/Ubuntu):

```
$ git config --global protocol.file.allow always
$ sudo apt install flatpak flatpak-builder gnome-software-plugin-flatpak
$ flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
$ flatpak install flathub org.freedesktop.Platform//23.08 org.freedesktop.Sdk//23.08 
```

On other distros, the packages should be similar.

## Generating a Flatpak package

```
$ flatpak-builder flatpak-build org.freedv.freedv.yml
```

## Installing and testing the generated package

```
$ flatpak-builder --user --install --force-clean flatpak-build org.freedv.freedv.yml
$ flatpak run org.freedv.freedv
```

## Additional notes

The Flatpak build container has no network access, so any new dependencies added to FreeDV will
need to be downloaded and built as a separate module to ensure that the package can still be built.
