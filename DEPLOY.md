# Windows

We need to create an app manifest file, as per https://docs.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion. NB : the Application Id="[xxx]" has some weird constraints.
NB : unless otherwise specified, the following is run under the cmd terminal, not powershell

Build with :
C:\Qt\5.15.1\msvc2019_64\bin\qmake.exe "C:\Dev\video-simili-duplicate-cleaner\QtProject\app\app.pro" -config release
nmake
nmake clean # to remove all the leftover files and keep only executable

Prepare all dependencies for packaging :
C:\Qt\5.15.1\msvc2019_64\bin\windeployqt.exe --release "C:\Dev\release\release\Video simili duplicate cleaner.exe"

Copy app manifest and icons referenced in the manifest into folder within which is the executable, then package all :
MakeAppx pack /d release /p "Video simili duplicate cleaner.msix"

NB : to check what was put into the package, we can simply unpack it to a folder :
MakeAppx unpack /v /p "Video simili duplicate cleaner.msix" /d "extracted-package"

(PowerShell)Create certificate :
New-SelfSignedCertificate -Type Custom -Subject "CN=4718DAC3-F3E7-40DE-AF8E-C3EB08A4F6AB" -KeyUsage DigitalSignature -FriendlyName "CertifVideoSimili" -CertStoreLocation "Cert:\CurrentUser\My" -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")

(PowerShell) Export the certificate which is apparently needed later
$password = ConvertTo-SecureString -String <certificate password> -Force -AsPlainText 
Export-PfxCertificate -cert "Cert:\CurrentUser\My\<certificate hash>" -FilePath C:\Dev\CertifVideoSimili.pfx -Password $password

Sign the package :
SignTool sign /fd SHA256  /a /f C:\Dev\CertifVideoSimili.pfx /p <certificate password> "Video simili duplicate cleaner.msix"

Now import the certificate to the computer's trusted certificates, with "Manage computer certificates", go to Trusted People part, click on certificates, and "Action", "All Tasks", import -> then select the exported certificate file, and import it.

Then we can run the package to install the app. If wanting to upload to windows store, do not sign the package before uploading.