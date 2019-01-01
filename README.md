# WhatsappTray
Extension for the Whatsapp Desktop Client, that enables minimize-to-tray and close-to-tray functionality.

## Requirements:
**Whatsapp Desktop Client**:
You can download that in the [Whatsapp Official Page](https://www.whatsapp.com/download/)

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
- Or Relativ "WHATSAPP_STARTPATH=.\..\WhatsApp.exe"

## Thanks to:
Nikolay Redko and J.D. Purcell for creating the RBTray-Software (http://rbtray.sourceforge.net/)
