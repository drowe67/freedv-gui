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

# Some runners in the GH environment will emit "Access Denied" for Import-Certificate
# if the proper TrustedPublisher Registry key doesn't exist first. Create prior to 
# attempting import.
# See https://learn.microsoft.com/en-us/answers/questions/1679945/(import-certificate)-unauthorizedaccessexception-e
$Key = "HKLM:\Software\Microsoft\SystemCertificates\TrustedPublisher"
If (-Not (Test-Path $Key)) {
    New-Item -Path $Key -ItemType RegistryKey -Force
}
`
Import-Certificate -FilePath $certName -CertStoreLocation Cert:\LocalMachine\TrustedPublisher

& "$Env:WindowsSdkDir\Tools\$Env:WindowsSDKVersion\$Env:Platform\devgen.exe" /add /bus ROOT /hardwareid $hwId
pnputil /add-driver $infName /install
