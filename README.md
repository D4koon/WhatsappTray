# WhatsappTray
Extension for the Whatsapp Desktop Client, that enables minimize-to-tray and close-to-tray functionality.

## Requirements:
**Whatsapp Desktop Client**:
You can download that in the [Whatsapp Official Page](https://www.whatsapp.com/download/)

## Silent install
Start a command line in the same folder where the .exe is located and start the .exe file with the parameters /Silent to install WhatsApp Tray without user input.

## Notice:
**DONT** start the application with adminstrative rights unless your Whatsapp is also running with adminstrative rights.
Otherwise it will **not work** because of communicationproblems between the two applications.

## Usage:
By starting WhatsappTray, WhatsApp will be started and from this point on the minimize-button in the WhatsApp-client sends the apllication to the tray. 

*Remember that you need to run, from this point on, the app from the WhatsappTray shortcut.*

If you want to minimize WhatsApp to tray with the close(X)-button, you have 2 options:
- Start WhatsappTray and minimize it. Then rightclick the trayicon and select the "Close to tray"-option.
- Start WhatsappTray with "--closeToTray"

If you want to set your own path for the WhatsApp-binary(exe), you can do so by using the 'WHATSAPP_STARTPATH'-config in the appData.ini
- For Example "WHATSAPP_STARTPATH=C:\Users\Dakoon\AppData\Local\WhatsApp\WhatsApp.exe"
- Or Relativ to the folder in which WhatsappTray.exe lies "WHATSAPP_STARTPATH=.\..\WhatsApp.exe"

For a portable config it is necessary to change the folder in which whatsapp stores data.
This directory is usually C:\Users\<username>\AppData\Roaming\WhatsApp\\IndexedDB\\file__0.indexeddb.leveldb
This can be configured with the WHATSAPP_ROAMING_DIRECTORY-config in the appData.ini
The set path has to replace 'C:\Users\<username>\AppData\Roaming\'. This path has to cotain the WhatsApp-folder.
- For Example "WHATSAPP_ROAMING_DIRECTORY=C:\Users\Dakoon\AppData\Roaming\
- Or Relativ to the folder in which WhatsappTray.exe lies "WHATSAPP_ROAMING_DIRECTORY=.\..\WhatsApp.exe"

## Thanks to:
Nikolay Redko and J.D. Purcell for creating the RBTray-Software (http://rbtray.sourceforge.net/)
