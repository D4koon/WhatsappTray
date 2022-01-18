# WhatsappTray
Extension for the Whatsapp Desktop Client, that enables minimize-to-tray and close-to-tray functionality.

## Requirements:
- **WhatsApp Desktop Client (64 bit)**:
  - Download from the [Official WhatsApp Page](https://www.whatsapp.com/download/)
- Windows 8 or higher

## Features:
- Minimize/Close button sends WhatsApp window to tray
- Toggle WhatsApp window by clicking on the tray icon
- Show unread messages in tray-icon
- Esc key sends WhatsApp window to tray (optional)

## Configuration:
WhatsappTray can be configured through:
- The **right-click-menue** in the tray. (Changes the values in *appData.ini*)
- The **appData.ini** which is automatically created in the installation-folder after the first start of WhatsappTray

### Advanced Configuration
#### WHATSAPP_STARTPATH
By default WhatsappTray uses the shortcut that was created when WhatsApp was installed to start WhatsApp.
If you want to set a different path to the WhatsApp binary(exe), you can do so by using the 'WHATSAPP_STARTPATH' config in the appData.ini
- *Absolute:* "WHATSAPP_STARTPATH=C:\Users\Dakoon\AppData\Local\WhatsApp\WhatsApp.exe"
- *Relative to the folder in which WhatsappTray.exe lies:* "WHATSAPP_STARTPATH=.\..\WhatsApp.exe"
- Support for variables *%UserProfile%* and *%AppData%*

#### Other
- Close to tray feature can also be activated by passing "--closeToTray" to WhatsappTray

## Silent install
Start a command line in the same folder where the .exe is located and start the .exe file with the parameters /Silent to install WhatsApp Tray without user input.

## Notice:
**DONT** start the application with administrative rights unless your Whatsapp is also running with administrative rights.
Otherwise it will **not work** because of communication problems between the two applications.

## Thanks to:
Nikolay Redko and J.D. Purcell for creating the RBTray software (http://rbtray.sourceforge.net/)
