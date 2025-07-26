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

### Standalone executable: Old method before using qt statically linked via vcpkg
Build with :
C:\Qt\5.15.1\msvc2019_64\bin\qmake.exe "C:\Dev\video-simili-duplicate-cleaner\QtProject\app\app.pro" -config release
nmake
nmake clean # to remove all the leftover files and keep only executable

Prepare all dependencies for packaging :
C:\Qt\5.15.1\msvc2019_64\bin\windeployqt.exe --no-compiler-runtime --release "C:\Dev\release\release\Video simili duplicate cleaner.exe"

Option --no-compiler-runtime is because when distributing through the MS Store, the vc redistributables are made available when specified in the appmanifest, as per https://docs.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-prepare "Your app uses a VCLibs framework package". To check the version needed, go into the developer machine and check under `C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Redist\MSVC` see what version to use, and therefore make sure the minversion specified in the appmanifest is below the required version. However for testing, in a clean machine not via the store, these redistributables need to be installed for the app to launch. On the target test machine, run `vc_redist.x64.exe` copied over from `C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Redist\MSVC\14.29.30036`


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
