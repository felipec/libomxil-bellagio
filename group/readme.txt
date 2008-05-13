Bellagio OpenMAX Symbian port

1. Overall Description

This port is based on the ST microelectronics open source OpenMAX implementation called Bellagio (also some contribution from NRC)
- Bellagio OpenMax from ST Microelectronics publicly available from SourceForge

2. Features
- OpenMAX core
- Three components (Components use Symbian ECOM plugin architecture for dynamic loading):
	- Volume component
	- Audio mixer component
	- Symbian audio output stream component
- Loader (Loader uses Symbian ECOM plugin architecture for dynamic loading):
	- Symbian loader for running st components in Symbian host
- Test exe for audio output component
- Test exe for audio mixer component
- Test exe for component enumeration

3. Build instructions

3.1 Environment
- S60 3rd edition FP1 SDK publicly available from Forum Nokia ( http://www.forum.nokia.com )
- Open C SDK plug-in publicly available from Forum Nokia 

3.2 Compiling
- Extract the zip file somewhere to your epocroot directory

To build the core, loaders, and components:
- Go to the directory libomxil\group and type "bldmake bldfiles"
- Then type "abld build armv5 urel"
- Then type "abld test build armv5 urel"

3.3 Installing into a Nokia device
- Go to the directory libomxil\sis
- Create your own keys to the same directory for signing the package:

*******************************************************************************************************
An example of creating a privatekey mykey.key and self-signed certificate mycert.cer is presented below:

makekeys -cert -password yourpassword -len 2048 -dname "CN=Test User  OU=Development OR=Company  CO=FI EM=test@company.com" mykey.key mycert.cer
******************************************************************************************************* 

- use the createandsign.bat script file in the same directory.

The script file takes 4 command line parameters which are:
1) path to the symbian SDK root directory (used to be called EPOCROOT, the level where epoc32 directory resides)
2) certificate file name
3) key file name
4) password for the signing (created when generating the keys and certificates)

example call:

createandsign.bat c:\Symbian\9.2\S60_3rd_FP1 mycert.cer mykey.key yourpassword

You're ready to install the sis file into your phone.

The main pkg file libomxil.pkg contains a lot of embedded sis files, which 
you can comment in or out for a package suitable for your needs. For example 
there are possibilities to include several loaders or components.

Note1: Remember to install the open C sis file to your phone, because the port
is dependent of it! 

4. Todo

9.5.2008 Jaska Uimonen