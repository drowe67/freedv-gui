# Code Signing Certificate HOWTO

## Prerequisites:

* Linux machine (Windows packages are currently generated using LLVM MinGW)
    * Required packages: pcscd, pcsc-tools, libfuse2*
* YubiKey 5 FIPS
    * [YubiKey Manager](https://www.yubico.com/support/download/yubikey-manager/) must be installed, including `ykman`:

```
sudo apt-add-repository ppa:yubico/stable
sudo apt update
sudo apt install yubikey-manager
```
    
## Initial YubiKey Setup

First, you'll need to configure a PIN (this only needs to be done once for each new YubiKey):

1. Start YubiKey Manager and verify that the YubiKey is detected.
2. Go to Applications and choose PIV.
3. Click on the "Configure PINs" button.
4. Click on the "Change PIN" button.
5. Click on the "Use Default" checkbox next to "Current PIN", or else enter the existing PIN if there is one.
6. Enter a new PIN in "New PIN" and "Confirm new PIN", then click "Change PIN".

## Generating CSR for codesigning cert

1. Start YubiKey Manager and verify that the YubiKey is detected.
2. Go to Applications and choose PIV.
3. Click on the "Configure Certificates" button.
4. Ensure that "Authentication" is selected and "Slot 9a" is displayed, then click the "Generate" button.
5. Choose "Certificate Signing Request" and then click "Next".
6. Choose "ECCP384" for "Algorithm", then click "Next".
    * NOTE: this can only be "ECCP384" or "ECCP384" for code signing certificates.
7. Enter "freedv-gui" for "Subject" and click "Next".
8. Verify that the settings appear like the below screenshot, then click "Generate":

![](./doc/csr-generation.png)

9. Choose a location to save the CSR to, then click "Save".
10. Enter your YubiKey's management key (you can choose "Use default" for the management key) and PIN.
121 Open the location you entered in (9) and copy/paste the contents into the Certificate Authority's website (where prompted for the CSR).

## Generating attestation certificate

1. Retrieve attestation certificate:

```
ykman piv keys attest 9a attestation-cert-2023_2026.crt
```

2. Retrieve intermediate certificate:

```
ykman piv certificates export f9 intermediate-cert-2023_2026.crt
```

3. Open both of the files generated above and copy/paste where prompted into the Certificate Authority's website.

## Troubleshooting:

### I'm running a VMWare VM and YubiKey Manager doesn't detect my YubiKey 

Follow the instructions [here](https://support.yubico.com/hc/en-us/articles/360013647640-Troubleshooting-Device-Passthrough-with-VMware-Workstation-and-VMware-Fusion) to update your VM's .vmx file to allow the VM to take full control. This is a problem at least on macOS hosts, not sure on other platforms.

### "PC/SC not available. Smart card (CCID) protocols will not function." message on console

Ensure that pcscd is running and enabled in systemctl:

```
$ sudo systemctl start pcscd
$ sudo systemctl enable pcscd
Synchronizing state of pcscd.service with SysV service script with /lib/systemd/systemd-sysv-install.
Executing: /lib/systemd/systemd-sysv-install enable pcscd
Created symlink /etc/systemd/system/sockets.target.wants/pcscd.socket → /lib/systemd/system/pcscd.socket.
$
```
