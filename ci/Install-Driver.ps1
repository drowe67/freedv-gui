# Make sure we are running as Admin
If (-NOT ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")){   
    $arguments = "& '" + $myinvocation.mycommand.definition + "'"
    Start-Process powershell -Verb runAs -ArgumentList $arguments
    Break
    }

$certName = $args[0]
$infName = $args[1]
$hwId = $args[2]

Import-Certificate -FilePath $certName -CertStoreLocation Cert:\LocalMachine\root
Import-Certificate -FilePath $certName -CertStoreLocation Cert:\LocalMachine\TrustedPublisher
& "$Env:WindowsSdkDir\Tool\$Env:WindowsSDKVersion\$Env:Platform\devgen.exe" /add /bus ROOT /hardwareid $hwId
pnputil /add-driver $infName /install
