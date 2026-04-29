# Code Signing HOWTO

## Introduction

Code signing is a process by where a signature is embedded into an application's executable file. This signature can be validated by supported operating systems to ensure that the file is from a known source and hasn't been tampered with, and in fact is required for applications to run at all on some platforms (for example, macOS). This document contains information on how code signing works in the FreeDV project.

## Windows Code Signing

### Introduction

Windows has a built-in anti-virus/anti-malware mechanism called Windows Defender. As part of the protection the operating system provides,
it checks any .exe, .dll or installer files that are downloaded from the internet for a valid certificate. If one does not exist, Windows
displays a SmartScreen validation error message that requires additional steps from the user in order to bypass and continue installation.
Many users rightfully will not proceed with installation when faced with this message, assuming that the installer and/or FreeDV application
is infected with a virus.

To improve the FreeDV user experience, it was determined that the project should purchase a code signing certificate and begin signing
official releases.

### Prerequisites:

* Pending EV code signing certificate order with [SignMyCode.com](https://signmycode.com) or another Sectigo reseller.
    * NOTE: this costs a fair bit of money, so it's more cost effective to purchase 3 year validity instead of 1.
    * There are specific requirements for the private/public key in order for the EV certificate to behave properly, so it is *not* recommended to bring your own key. Currently (as of 2023) Sectigo issues RSA 4096 bit keys onto their SafeNet tokens.
    * EV certificates require a legally registered entity to issue, so (as of 2023) our fiscal sponsor ([Software Freedom Conservancy](https://sfconservancy.org/)) has been the entity on the certificate.
* Linux machine (Windows packages are currently generated using LLVM MinGW)
    * Required packages: pcscd, pcsc-tools, libfuse2*, osslsigncode, opensc, opensc-pkcs11, libengine-pkcs11-openssl, gnutls-bin

### Installing SafeNet Authentication Client on Linux

Download SafeNet Authentication Client from [here](https://comodoca.my.salesforce.com/sfc/p/1N000002Ljih/a/3l000000GBKA/jByoGtRgjuh1HrkxbtiH5QE2asIHbqCTQJcCLBqd8.o) and install the Ubuntu package as follows:

```
$ unzip "SafeNet Authentication Client 10.8 R1 GA Linux.zip"
$ cd "SAC_10_8_R1 GA/Installation/Standard/Ubuntu-2204"
$ sudo dpkg -i safenetauthenticationclient_10.8.1050_amd64.deb
```

### Locating signing key and certificate on token

At the terminal, enter `p11tool --list-all --provider /usr/lib/libeToken.so`. Look for something 
like the following:

```
Object 0:
	URL: pkcs11:model=ID%20Prime%20MD;manufacturer=Gemalto;serial=CB64B873EE27EF1B;token=Software%20Freedom%20Conservancy%2C%20In;id=%87%7F%E9%6E%9E%86%84%43;object=Sectigo_20230908155653;type=cert
	Type: X.509 Certificate (RSA-4096)
	Expires: Mon Sep  7 16:59:59 2026
	Label: Sectigo_20230908155653
	ID: 87:7f:e9:6e:9e:86:84:43
```

Save the URLs to files for later use, e.g.

```
echo -n "pkcs11:model=ID%20Prime%20MD;manufacturer=Gemalto;serial=CB64B873EE27EF1B;token=Software%20Freedom%20Conservancy%2C%20In;id=%87%7F%E9%6E%9E%86%84%43;type=private" > ~/key.url
echo -n "pkcs11:model=ID%20Prime%20MD;manufacturer=Gemalto;serial=CB64B873EE27EF1B;token=Software%20Freedom%20Conservancy%2C%20In;id=%87%7F%E9%6E%9E%86%84%43;object=Sectigo_20230908155653;type=cert" > ~/cert.url
```

### Signing binaries manually

Use something like the following command:

```
osslsigncode sign -pkcs11engine /usr/lib/x86_64-linux-gnu/engines-3/pkcs11.so -pkcs11module /usr/lib/libeToken.so -certs [path to exported or provided certificate] -key `cat key.url` -in FreeDV-1.8.12-windows-x86_64.exe -out FreeDV-1.8.12-windows-x86_64-signed.exe
```

You will be asked for the token's's PIN in order to complete the signature process. To verify that the file is correctly signed, copy it to a Windows machine and view the file's properties (under the "Digital Signatures" tab); the subject should match what was provided either for the CSR submitted to Sectigo/other Certificate Authority or what was entered when generating the self-signed certificate above:

![](./doc/digitally-signed.png)

Notes:

* The file specified by `-out` must not already exist. Otherwise, osslsigncode will error out.
* libeToken.so *must* be specified for osslsigncode. Other PKCS11 modules may work but haven't been tested.

### Signing using CMake

To build a signed Windows version of FreeDV, pass in `-DSIGN_WINDOWS_BINARIES=1` as well as files containing the intermediare/root certificates, PKCS#11 key and certificate URLs. For example:

```
$ mkdir build
$ cd build
$ cmake -DSIGN_WINDOWS_BINARIES=1 -DPKCS11_KEY_FILE=~/key.url` -DPKCS11_CERTIFICATE_FILE=~/cert.url -DINTERMEDIATE_CERT_FILE=~/cacerts.crt -DCMAKE_TOOLCHAIN_FILE=/home/mooneer/freedv-gui/cross-compile/freedv-mingw-llvm-x86_64.cmake ..
$ make
$ make package
```

Other optional variables that can be set are as follows:

* `PKCS11_ENGINE` / `PKCS11_MODULE`: Paths to the PKCS11 engine and module libraries on your system. This is mainly used for those who aren't compiling on amd64 and/or aren't using a SafeNet token.
* `TIMESTAMP_SERVER`: If you prefer an alternate timestamping server than the default.
* `SIGN_HASH`: If you prefer a different hashing algorithm than the default SHA256. (Note: the timestamping server will automatically use this hashing algorithm or stronger.)

You will be prompted for your token's PIN several times during the build process. When done, the installer as well as freedv.exe will be signed with the provided certificate.

*NOTE: The PIN prompts can be auto-filled by appending `?pin-value=xxxxxx` to the key's URL (where `xxxxxx` is your token's PIN). The best practice is to exclude the `?pin-value=xxxxxx` and manually enter the PIN each time, however.*

### Auto-building signed binaries for all supported architectures

You can auto-build installers for all supported architectures (x86_64, aarch64)
by using the `build_signed_windows_release.sh` script as follows:

```
$ ./build_signed_windows_release.sh ~/key.url ~/cert.url ~/intermediate-certs.crt
```

A `build_windows` directory will be created with installers for each architecture
when complete. This may take quite a while (for example, ~1 hour on a 2019 MacBook Pro).

*NOTE: Ensure that LLVM MinGW and osslsigncode are in your PATH before executing the above command.*

### Using a remotely hosted USB key

It is possible to connect to a remote server containing a USB token. This requires installing [pkcs11-proxy](https://github.com/iksaif/pkcs11-proxy) on both machines and setting needed environment on the build machine:

```
# On server with USB token, install SafeNet and all other needed packages and then run:
$ PKCS11_DAEMON_SOCKET="tcp://127.0.0.1:2345" pkcs11-daemon /usr/lib/libeToken.so

# On build machine
$ ssh -Nf -o "ServerAliveInterval 60" -L 2345:127.0.0.1:2345 username@server-name
$ mkdir build
$ cmake -DENABLE_LTO=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSIGN_WINDOWS_BINARIES=1 -DPKCS11_MODULE=/usr/lib/libpkcs11-proxy.so -DPKCS11_CERTIFICATE_FILE=~/cert.url -DPKCS11_KEY_FILE=~/key.url -DINTERMEDIATE_CERT_FILE=~/cacerts.crt -DCMAKE_TOOLCHAIN_FILE=/home/mooneer/freedv-gui/cross-compile/freedv-mingw-llvm-x86_64.cmake ..
$ PKCS11_PROXY_SOCKET="tcp://127.0.0.1:2345" make -j$(nproc) package
```

### Troubleshooting:

#### I'm running a VMWare VM and the system doesn't detect my token 

Follow the instructions [here](https://support.yubico.com/hc/en-us/articles/360013647640-Troubleshooting-Device-Passthrough-with-VMware-Workstation-and-VMware-Fusion) to update your VM's .vmx file to allow the VM to take full control. This is a problem at least on macOS hosts, not sure on other platforms.

#### "PC/SC not available. Smart card (CCID) protocols will not function." message on console

Ensure that pcscd is running and enabled in systemctl:

```
$ sudo systemctl start pcscd
$ sudo systemctl enable pcscd
Synchronizing state of pcscd.service with SysV service script with /lib/systemd/systemd-sysv-install.
Executing: /lib/systemd/systemd-sysv-install enable pcscd
Created symlink /etc/systemd/system/sockets.target.wants/pcscd.socket → /lib/systemd/system/pcscd.socket.
$
```

#### File appears to sign successfully but fails to verify

This is likely due to a problem with the certificate. Open a technical support case with your certificate 
provider as they may need to reissue.

#### Windows Defender SmartScreen popups appear despite use of EV certificate

Some things to check:

1. Ensure that `signtool.exe /v /debug /pa file.exe` validates the signed file successfully. (Or use the equivalent `osslsigncode verify -CAfile RootCertificateBundle.crt --TSA-CAfile /usr/lib/ssl/certs/ca-certificates.crt -in file.exe` on Linux/macOS.)
2. The timestamp hash should be the same as the file hash for SmartScreen to properly accept the signed file. If not, the file will need to be re-signed (and CMakeLists.txt possibly updated).
3. OV certificates may need a couple of days at minimum to be accepted by Microsoft. It may be possible to accelerate this by sending the signed file to Microsoft for analysis.
4. Problems that will definitely require a re-issue:
    * [ECDSA keys do not work well](https://vcsjones.dev/authenticode-and-ecc/) for code signing. Yes, this is still a problem in 2023 despite the date of the blog post. If you use your certificate vendor's token instead of your own, this isn't likely to be the problem but still worth noting here in case you're tempted to buy a YubiKey FIPS token (which supposedly works but never fully removed SmartScreen popups for the FreeDV project).
    * If you're still tempted to bring your own token, note that the [CA Forum mandates a minimum RSA key length of 3072 bits](https://knowledge.digicert.com/alerts/code-signing-new-minimum-rsa-keysize.html). Anything shorter than that will definitely cause problems. (4096 bit RSA appears to be the current standard for code signing certificates as of 2026.)

Additionally, as of 2024, [Microsoft has changed how SmartScreen works](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation). For the purposes of avoiding the SmartScreen popup, there is effectively no longer any difference between EV and OV certificates. This means that it may take some downloads after issuance of a new certificate before it becomes trusted by Microsoft.

## macOS Notarization

macOS notarization is effectively required by Apple in order to execute applications on that platform. This is done using the `notarytool` application. The current CMake scripts will automatically run this tool during the build process (replace everything in square brackets as appropriate):

```
$ xcrun notarytool store-credentials --apple-id "[Apple account username]" --password "[Apple password]" --team-id "[team ID]" [profile name] # Execute this once to save a profile to pass to builds
$ BUILD_TYPE=Release UT_ENABLE=0 UNIV_BUILD=1 BUILD_DEPS=1 CODESIGN_IDENTITY=[team ID] CODESIGN_KEYCHAIN_PROFILE=[profile name] ./build_osx.sh
```

Note that the above requires a macOS system to build (i.e. cross-compiling is not supported).

## Linux AppImage signing

For signing Linux AppImages, simply generate a private/public key pair in GPG and execute the following prior to generating the AppImage:

```
$ export LDAI_SIGN=1
$ export LDAI_SIGN_KEY="[GPG key ID]"
$ ./make-appimage.sh
```

Be sure to make backups of your public and private keys as it's impossible to regenerate one from the other if lost.

## Signing from GitHub Actions

The current GitHub Actions scripts for FreeDV support automated code signing and notarization of binaries on each push to the repository. To enable this, GitHub secrets need to be configured for each platform that requires code signing:

| Secret Name | Description | Required for Platform |
|---|---|---|
| `FREEDV_LINUX_CODE_SIGNING_KEY` | PEM encoded text containing the private key to use for signing Linux AppImages. | Linux |
| `FREEDV_LINUX_CODE_SIGNING_KEY_ID` | GPG key ID for the key provided by `FREEDV_LINUX_CODE_SIGNING_KEY`. | Linux |
| `FREEDV_LINUX_CODE_SIGNING_PUBLIC_KEY` | PEM encoded text containing the public key to use for signing Linux AppImages. | Linux |
| `FREEDV_MACOS_CODE_SIGNING_CERTIFICATE` | Apple Distribution certificate for the Apple account to use for notarization in PEM format. | macOS |
| `FREEDV_MACOS_CODE_SIGNING_PASSWORD` | Password to the Apple developer account being used for notarization. | macOS |
| `FREEDV_MACOS_CODE_SIGNING_USERNAME` | Username for the Apple developer account being used for notarization. | macOS |
| `FREEDV_WINDOWS_CODE_SIGNING_CERT` | PKCS11 URL for the certificate to use on the USB token. | Windows |
| `FREEDV_WINDOWS_CODE_SIGNING_INTERMEDIATE_CERTS` | PEM encoded text containing the intermediate certificates to embed into the signature. | Windows |
| `FREEDV_WINDOWS_CODE_SIGNING_KEY` | PKCS11 URL for the public/private key to use on the USB token. | Windows |
| `FREEDV_WINDOWS_CODE_SIGNING_SSH_HOST` | Hostname to connect to for the required SSH tunnel to the server with the USB token attached. | Windows |
| `FREEDV_WINDOWS_CODE_SIGNING_SSH_KEY` | Private key to use for connecting to `FREEDV_WINDOWS_CODE_SIGNING_SSH_HOST`. | Windows |
| `FREEDV_WINDOWS_CODE_SIGNING_SSH_PORT` | Port to use for connecting to the server with the USB token (i.e. 22 if the server is directly on the internet). | Windows |
| `FREEDV_WINDOWS_CODE_SIGNING_SSH_USER` | Username to use for connecting the required SSH tunnel to the server with the USB token attached. | Windows |

Additionally, for macOS, the team ID must be specified by adding a variable named `FREEDV_MACOS_CODE_SIGNING_TEAM_ID` with the appropriate value.

GitHub secrets are added by going to the repository's settings and clicking on "Actions" under "Secrets and variables". Note that for each platform you want to support signing or notarization for, all secrets for that platform must be provided (for example, `FREEDV_MACOS_CODE_SIGNING_CERTIFICATE`, `FREEDV_MACOS_CODE_SIGNING_USERNAME` and `FREEDV_MACOS_CODE_SIGNING_PASSWORD` must all be provided to notarize generatred macOS binaries).

Currently automated code signing is not done on the main GitHub repository but on a fork of it owned by one of the PLT members. Builds off of `master` in that fork are then added to GitHub releases in the main repo. In the future, automated code signing may be done on the main repo instead.

## Sources

* [Yubico GitHub issue referring to osslsigncode](https://github.com/Yubico/yubico-piv-tool/issues/21)
* [StackOverflow post on configuring CMake for code signing](https://stackoverflow.com/questions/72504366/how-to-sign-windows-binaries-and-nsis-installers-when-building-with-cmake-cpac)
* [linuxdeploy-plugin-appiamge repo containing AppImage code signing info](https://github.com/linuxdeploy/linuxdeploy-plugin-appimage)
* [Notarizing macOS software before distribution](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)
* [Customizing the notarization workflow](https://developer.apple.com/documentation/security/customizing-the-notarization-workflow?language=objc)
