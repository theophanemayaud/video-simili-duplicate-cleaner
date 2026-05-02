# Windows

## Install dependencies

See DEPENDENCIES.md

## Package the app

We have a app manifest file, as per https://docs.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion. NB : the Application Id="[xxx]" has some weird constraints.
NB : unless otherwise specified, the following is run under the cmd terminal, not powershell

This manifest is under `QtProject/release-build/windows/appxmanifest.xml`

We also need some icons which are also prepared there.

Below steps are also performed automatically when running npm run win-binaries or sub steps defined in the `QtProject/release-build/windows/package.json`.

### Standalone executable: When qt is statically linked via vcpkg management

vcpkg directly statically links all required dependencies, so nothing extra should be needed for the executable to be standalone. 

### Create the installer / msix

Copy app manifest and icons referenced in the manifest into folder within which is the executable, then package all :
MakeAppx pack /d release /p "Video simili duplicate cleaner.msix"

See https://docs.microsoft.com/fr-fr/windows/msix/package/create-app-package-with-makeappx-tool for more information

NB : to check what was put into the package, we can simply unpack it to a folder :
MakeAppx unpack /v /p "Video simili duplicate cleaner.msix" /d "extracted-package"

NB if wanting to upload to windows store, do not sign the package before uploading and upload the package obtained here.

### Sign the package

(PowerShell)Create certificate :
```
New-SelfSignedCertificate -Type Custom -Subject "CN=4718DAC3-F3E7-40DE-AF8E-C3EB08A4F6AB" -KeyUsage DigitalSignature -FriendlyName "CertifVideoSimili" -CertStoreLocation "Cert:\CurrentUser\My" -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
```
This outputs the Thumbprint which you can save as the certificate hash.

(PowerShell) Export the certificate which is apparently needed later
```
$password = ConvertTo-SecureString -String <certificate password> -Force -AsPlainText
Export-PfxCertificate -cert "Cert:\CurrentUser\My\<certificate hash>" -FilePath C:\Dev\CertifVideoSimili.pfx -Password $password
```

Sign the package :
```
SignTool sign /fd SHA256  /a /f C:\Dev\CertifVideoSimili.pfx /p <certificate password> "Video simili duplicate cleaner.msix"
```

See more info in https://docs.microsoft.com/fr-fr/windows/msix/package/sign-app-package-using-signtool

### Install the certificate to test install the app.
Now import the certificate to the computer's trusted certificates, with "Manage computer certificates", go to Trusted People part, click on certificates, and "Action", "All Tasks", import -> then select the exported certificate file, and import it.

Then we can run the package to install the app and test it. If wanting to upload to windows store, do not sign the package before uploading.
