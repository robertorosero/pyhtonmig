; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

; This is the whole ball of wax for an Inno installer for Python.
; To use, download Inno Setup from http://www.jrsoftware.org/isdl.htm/,
; install it, and double-click on this file.  That launches the Inno
; script compiler.  The GUI is extemely simple, and has only one button
; you may not recognize instantly:  click it.  You're done.  It builds
; the installer into PCBuild/Python-2.2a1.exe.  Size and speed of the
; installer are competitive with the Wise installer; Inno uninstall
; seems much quicker than Wise (but also feebler, and the uninstall
; log is in some un(human)readable binary format).
;
; What's Done
; -----------
; All the usual Windows Python files are installed by this now.
; All the usual Windows Python Start menu entries are created and
; work fine.
; .py, .pyw, .pyc and .pyo extensions are registered.
;     PROBLEM:  Inno uninstall does not restore their previous registry
;               associations (if any).  Wise did.  This will make life
;               difficult for alpha (etc) testers.
; The Python install is fully functional for "typical" uses.
;
; What's Not Done
; ---------------
; None of "Mark Hammond's" registry entries are written.
; No installation of files is done into the system dir:
;     The MS DLLs aren't handled at all by this yet.
;     Python22.dll is unpacked into the main Python dir.
;
; Inno can't do different things on NT/2000 depending on whether the user
; has Admin privileges, so I don't know how to "solve" either of those,
; short of building two installers (one *requiring* Admin privs, the
; other not doing anything that needs Admin privs).
;
; Inno has no concept of variables, so lots of lines in this file need
; to be fiddled by hand across releases.  Simplest way out:  stick this
; file in a giant triple-quoted r-string (note that backslashes are
; required all over the place here -- forward slashes DON'T WORK in
; Inno), and use %(yadda)s string interpolation to do substitutions; i.e.,
; write a very simple Python program to *produce* this script.

[Setup]
AppName=Python and combined Win32 Extensions
AppVerName=Python 2.2.2 and combined Win32 Extensions 150
AppId=Python 2.2.2.150
AppVersion=2.2.2.150
AppCopyright=Python is Copyright � 2001 Python Software Foundation. Win32 Extensions are Copyright � 1996-2001 Greg Stein and Mark Hammond.

; Default install dir; value of {app} later (unless user overrides).
; {sd} = system root drive, probably "C:".
DefaultDirName={sd}\Python22
;DefaultDirName={pf}\Python

; Start menu folder name; value of {group} later (unless user overrides).
DefaultGroupName=Python 2.2

; Point SourceDir to one above PCBuild = src.
; means this script can run unchanged from anyone's CVS tree, no matter
; what they called the top-level directories.
SourceDir=.
OutputDir=..
OutputBaseFilename=Python-2.2.2-Win32-150-Setup

AppPublisher=PythonLabs at Digital Creations
AppPublisherURL=http://www.python.org
AppSupportURL=http://www.python.org
AppUpdatesURL=http://www.python.org

AlwaysCreateUninstallIcon=true
ChangesAssociations=true
UninstallLogMode=new
AllowNoIcons=true
AdminPrivilegesRequired=true
UninstallDisplayIcon={app}\pyc.ico
WizardDebug=false

; The fewer screens the better; leave these commented.

Compression=bzip
InfoBeforeFile=LICENSE.txt
;InfoBeforeFile=Misc\NEWS

; uncomment the following line if you want your installation to run on NT 3.51 too.
; MinVersion=4,3.51

[Types]
Name: normal; Description: Select desired components; Flags: iscustom

[Components]
Name: main; Description: Python and Win32 Extensions; Types: normal
Name: docs; Description: Python documentation (HTML); Types: normal
Name: tk; Description: TCL/TK, tkinter, and Idle; Types: normal
Name: tools; Description: Python utility scripts (Tools\); Types: normal
Name: test; Description: Python test suite (Lib\test\); Types: normal

[Tasks]
Name: extensions; Description: Register file associations (.py, .pyw, .pyc, .pyo); Components: main; Check: IsAdminLoggedOn

[Files]
; Caution:  Using forward slashes instead screws up in amazing ways.
; Unknown:  By the time Components (and other attrs) are added to these lines, they're
; going to get awfully long.  But don't see a way to continue logical lines across
; physical lines.

Source: LICENSE.txt; DestDir: {app}; CopyMode: alwaysoverwrite
Source: README.txt; DestDir: {app}; CopyMode: alwaysoverwrite
Source: News.txt; DestDir: {app}; CopyMode: alwaysoverwrite
Source: *.ico; DestDir: {app}; CopyMode: alwaysoverwrite; Components: main

Source: python.exe; DestDir: {app}; CopyMode: alwaysoverwrite; Components: main
Source: pythonw.exe; DestDir: {app}; CopyMode: alwaysoverwrite; Components: main
Source: w9xpopen.exe; DestDir: {app}; CopyMode: alwaysoverwrite; Components: main


Source: DLLs\tcl83.dll; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: tk
Source: DLLs\tk83.dll; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: tk
Source: tcl\*.*; DestDir: {app}\tcl; CopyMode: alwaysoverwrite; Components: tk; Flags: recursesubdirs

Source: sysdir\python22.dll; DestDir: {sys}; CopyMode: alwaysskipifsameorolder; Components: main; Flags: sharedfile restartreplace
Source: sysdir\PyWinTypes22.dll; DestDir: {sys}; CopyMode: alwaysskipifsameorolder; Components: main; Flags: restartreplace sharedfile
Source: sysdir\pythoncom22.dll; DestDir: {sys}; CopyMode: alwaysskipifsameorolder; Components: main; Flags: restartreplace sharedfile

Source: DLLs\_socket.pyd; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: main
Source: libs\_socket.lib; DestDir: {app}\libs; CopyMode: alwaysoverwrite; Components: main

Source: DLLs\_sre.pyd; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: main
Source: libs\_sre.lib; DestDir: {app}\libs; CopyMode: alwaysoverwrite; Components: main

Source: DLLs\_symtable.pyd; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: main
Source: libs\_symtable.lib; DestDir: {app}\libs; CopyMode: alwaysoverwrite; Components: main

Source: DLLs\_testcapi.pyd; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: main
Source: libs\_testcapi.lib; DestDir: {app}\libs; CopyMode: alwaysoverwrite; Components: main

Source: DLLs\_tkinter.pyd; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: tk
Source: libs\_tkinter.lib; DestDir: {app}\libs; CopyMode: alwaysoverwrite; Components: tk

Source: DLLs\bsddb.pyd; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: main
Source: libs\bsddb.lib; DestDir: {app}\libs; CopyMode: alwaysoverwrite; Components: main

Source: DLLs\mmap.pyd; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: main
Source: libs\mmap.lib; DestDir: {app}\libs; CopyMode: alwaysoverwrite; Components: main

Source: DLLs\parser.pyd; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: main
Source: libs\parser.lib; DestDir: {app}\libs; CopyMode: alwaysoverwrite; Components: main

Source: DLLs\pyexpat.pyd; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: main
Source: libs\pyexpat.lib; DestDir: {app}\libs; CopyMode: alwaysoverwrite; Components: main

Source: DLLs\select.pyd; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: main
Source: libs\select.lib; DestDir: {app}\libs; CopyMode: alwaysoverwrite; Components: main

Source: DLLs\unicodedata.pyd; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: main
Source: libs\unicodedata.lib; DestDir: {app}\libs; CopyMode: alwaysoverwrite; Components: main

Source: DLLs\_winreg.pyd; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: main
Source: libs\_winreg.lib; DestDir: {app}\libs; CopyMode: alwaysoverwrite; Components: main

Source: DLLs\winsound.pyd; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: main
Source: libs\winsound.lib; DestDir: {app}\libs; CopyMode: alwaysoverwrite; Components: main

Source: DLLs\zlib.pyd; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: main
Source: libs\zlib.lib; DestDir: {app}\libs; CopyMode: alwaysoverwrite; Components: main

Source: libs\python22.lib; DestDir: {app}\libs; CopyMode: alwaysoverwrite; Components: main

Source: DLLs\expat.dll; DestDir: {app}\DLLs; CopyMode: alwaysoverwrite; Components: main



Source: Lib\*.py; DestDir: {app}\Lib; CopyMode: alwaysoverwrite; Components: main
Source: Lib\compiler\*.*; DestDir: {app}\Lib\compiler; CopyMode: alwaysoverwrite; Components: main; Flags: recursesubdirs
Source: Lib\distutils\*.*; DestDir: {app}\Lib\distutils; CopyMode: alwaysoverwrite; Components: main; Flags: recursesubdirs
Source: Lib\email\*.*; DestDir: {app}\Lib\email; CopyMode: alwaysoverwrite; Components: main; Flags: recursesubdirs
Source: Lib\encodings\*.*; DestDir: {app}\Lib\encodings; CopyMode: alwaysoverwrite; Components: main; Flags: recursesubdirs
Source: Lib\hotshot\*.*; DestDir: {app}\Lib\hotshot; CopyMode: alwaysoverwrite; Components: main; Flags: recursesubdirs
Source: Lib\lib-old\*.*; DestDir: {app}\Lib\lib-old; CopyMode: alwaysoverwrite; Components: main; Flags: recursesubdirs
Source: Lib\xml\*.*; DestDir: {app}\Lib\xml; CopyMode: alwaysoverwrite; Components: main; Flags: recursesubdirs
Source: Lib\hotshot\*.*; DestDir: {app}\Lib\hotshot; CopyMode: alwaysoverwrite; Components: main; Flags: recursesubdirs
Source: Lib\test\*.*; DestDir: {app}\Lib\test; CopyMode: alwaysoverwrite; Components: test; Flags: recursesubdirs
Source: Lib\tkinter\*.py; DestDir: {app}\Lib\tkinter; CopyMode: alwaysoverwrite; Components: tk; Flags: recursesubdirs

Source: Lib\site-packages\README.txt; DestDir: {app}\Lib\site-packages; CopyMode: alwaysoverwrite; Components: main

Source: Lib\site-packages\PyWin32.chm; DestDir: {app}\Lib\site-packages; CopyMode: alwaysoverwrite; Components: docs
Source: Lib\site-packages\win32\*.*; DestDir: {app}\Lib\site-packages\win32; CopyMode: alwaysoverwrite; Components: main; Flags: recursesubdirs
Source: Lib\site-packages\win32com\*.*; DestDir: {app}\Lib\site-packages\win32com; CopyMode: alwaysoverwrite; Components: main; Flags: recursesubdirs
Source: Lib\site-packages\win32comext\*.*; DestDir: {app}\Lib\site-packages\win32comext; CopyMode: alwaysoverwrite; Components: main; Flags: recursesubdirs

Source: include\*.h; DestDir: {app}\include; CopyMode: alwaysoverwrite; Components: main

Source: Tools\idle\*.*; DestDir: {app}\Tools\idle; CopyMode: alwaysoverwrite; Components: tk; Flags: recursesubdirs

Source: Tools\pynche\*.*; DestDir: {app}\Tools\pynche; CopyMode: alwaysoverwrite; Components: tools; Flags: recursesubdirs
Source: Tools\scripts\*.*; DestDir: {app}\Tools\Scripts; CopyMode: alwaysoverwrite; Components: tools; Flags: recursesubdirs
Source: Tools\webchecker\*.*; DestDir: {app}\Tools\webchecker; CopyMode: alwaysoverwrite; Components: tools; Flags: recursesubdirs
Source: Tools\versioncheck\*.*; DestDir: {app}\Tools\versioncheck; CopyMode: alwaysoverwrite; Components: tools; Flags: recursesubdirs

Source: Doc\*.*; DestDir: {app}\Doc; CopyMode: alwaysoverwrite; Flags: recursesubdirs; Components: docs


[Icons]
Name: {group}\Python (command line); Filename: {app}\python.exe; WorkingDir: {app}; Components: main
Name: {group}\Python Manuals; Filename: {app}\Doc\index.html; WorkingDir: {app}; Components: docs
Name: {group}\Win32 Extensions Help; Filename: {app}\Lib\site-packages\PyWin32.chm; WorkingDir: {app}\Lib\site-packages; Components: docs
Name: {group}\Module Docs; Filename: {app}\pythonw.exe; WorkingDir: {app}; Parameters: """{app}\Tools\Scripts\pydoc.pyw"""; Components: tools
Name: {group}\IDLE (Python GUI); Filename: {app}\pythonw.exe; WorkingDir: {app}; Parameters: """{app}\Tools\idle\idle.pyw"""; Components: tools

[Registry]
; Register .py
Tasks: extensions; Root: HKCR; Subkey: .py; ValueType: string; ValueName: ; ValueData: Python File; Flags: uninsdeletevalue
Tasks: extensions; Root: HKCR; Subkey: .py; ValueType: string; ValueName: Content Type; ValueData: text/plain; Flags: uninsdeletevalue
Tasks: extensions; Root: HKCR; Subkey: Python File; ValueType: string; ValueName: ; ValueData: Python File; Flags: uninsdeletekey
Tasks: extensions; Root: HKCR; Subkey: Python File\DefaultIcon; ValueType: string; ValueName: ; ValueData: {app}\Py.ico
Tasks: extensions; Root: HKCR; Subkey: Python File\shell\open\command; ValueType: string; ValueName: ; ValueData: """{app}\python.exe"" ""%1"" %*"

; Register .pyc
Tasks: extensions; Root: HKCR; Subkey: .pyc; ValueType: string; ValueName: ; ValueData: Python CompiledFile; Flags: uninsdeletevalue
Tasks: extensions; Root: HKCR; Subkey: Python CompiledFile; ValueType: string; ValueName: ; ValueData: Compiled Python File; Flags: uninsdeletekey
Tasks: extensions; Root: HKCR; Subkey: Python CompiledFile\DefaultIcon; ValueType: string; ValueName: ; ValueData: {app}\pyc.ico
Tasks: extensions; Root: HKCR; Subkey: Python CompiledFile\shell\open\command; ValueType: string; ValueName: ; ValueData: """{app}\python.exe"" ""%1"" %*"

; Register .pyo
Tasks: extensions; Root: HKCR; Subkey: .pyo; ValueType: string; ValueName: ; ValueData: Python CompiledFile; Flags: uninsdeletevalue

; Register .pyw
Tasks: extensions; Root: HKCR; Subkey: .pyw; ValueType: string; ValueName: ; ValueData: Python NoConFile; Flags: uninsdeletevalue
Tasks: extensions; Root: HKCR; Subkey: .pyw; ValueType: string; ValueName: Content Type; ValueData: text/plain; Flags: uninsdeletevalue
Tasks: extensions; Root: HKCR; Subkey: Python NoConFile; ValueType: string; ValueName: ; ValueData: Python File (no console); Flags: uninsdeletekey
Tasks: extensions; Root: HKCR; Subkey: Python NoConFile\DefaultIcon; ValueType: string; ValueName: ; ValueData: {app}\Py.ico
Tasks: extensions; Root: HKCR; Subkey: Python NoConFile\shell\open\command; ValueType: string; ValueName: ; ValueData: """{app}\pythonw.exe"" ""%1"" %*"


; Python Registry Keys
Root: HKLM; Subkey: SOFTWARE\Python; Flags: uninsdeletekeyifempty; Check: IsAdminLoggedOn
Root: HKLM; Subkey: SOFTWARE\Python\PythonCore; Flags: uninsdeletekeyifempty
Root: HKLM; Subkey: SOFTWARE\Python\PythonCore\2.2; Flags: uninsdeletekeyifempty
Root: HKLM; Subkey: SOFTWARE\Python\PythonCore\2.2\PythonPath; ValueData: "{app}\Lib;{app}\DLLs"; Flags: uninsdeletekeyifempty
Root: HKLM; Subkey: SOFTWARE\Python\PythonCore\2.2\PythonPath\win32; ValueData: "{app}\lib\site-packages\win32;{app}\lib\site-packages\win32\lib"; Flags: uninsdeletekey
Root: HKLM; Subkey: SOFTWARE\Python\PythonCore\2.2\PythonPath\win32com; ValueData: C:\Python\lib\site-packages; Flags: uninsdeletekey
Root: HKLM; Subkey: SOFTWARE\Python\PythonCore\2.2\Modules; Flags: uninsdeletekeyifempty
Root: HKLM; Subkey: SOFTWARE\Python\PythonCore\2.2\Modules\pythoncom; ValueData: {sys}\pythoncom22.dll; Flags: uninsdeletekey
Root: HKLM; Subkey: SOFTWARE\Python\PythonCore\2.2\Modules\pywintypes; ValueData: {sys}\PyWinTypes22.dll; Flags: uninsdeletekey
Root: HKLM; Subkey: SOFTWARE\Python\PythonCore\2.2\InstallPath; ValueData: {app}; Flags: uninsdeletekeyifempty; ValueType: string
Root: HKLM; Subkey: SOFTWARE\Python\PythonCore\2.2\InstallPath\InstallGroup; ValueData: {group}; Flags: uninsdeletekey
Root: HKLM; Subkey: SOFTWARE\Python\PythonCore\2.2\Help; Flags: uninsdeletekeyifempty
Root: HKLM; Subkey: SOFTWARE\Python\PythonCore\2.2\Help\Main Python Documentation; ValueType: string; ValueData: {app}\Doc\index.html; Flags: uninsdeletekey; Components: docs
Root: HKLM; Subkey: SOFTWARE\Python\PythonCore\2.2\Help\Python Win32 Documentation; ValueType: string; ValueData: {app}\lib\site-packages\PyWin32.chm; Flags: uninsdeletekey; Components: docs

[_ISTool]
EnableISX=true


[Code]
Program Setup;

Function IsAdminNotLoggedOn(): Boolean;
begin
  Result := Not IsAdminLoggedOn();
end;

begin
end.




[UninstallDelete]
Name: {app}\Lib\compiler\*.pyc; Type: files
Name: {app}\Lib\compiler\*.pyo; Type: files
Name: {app}\Lib\compiler; Type: dirifempty
Name: {app}\Lib\distutils\command\*.pyc; Type: files
Name: {app}\Lib\distutils\command\*.pyo; Type: files
Name: {app}\Lib\distutils\command; Type: dirifempty
Name: {app}\Lib\distutils\*.pyc; Type: files
Name: {app}\Lib\distutils\*.pyo; Type: files
Name: {app}\Lib\distutils; Type: dirifempty
Name: {app}\Lib\email\test\*.pyc; Type: files
Name: {app}\Lib\email\test\*.pyo; Type: files
Name: {app}\Lib\email\test; Type: dirifempty
Name: {app}\Lib\email\*.pyc; Type: files
Name: {app}\Lib\email\*.pyo; Type: files
Name: {app}\Lib\email; Type: dirifempty
Name: {app}\Lib\encodings\*.pyc; Type: files
Name: {app}\Lib\encodings\*.pyo; Type: files
Name: {app}\Lib\encodings; Type: dirifempty
Name: {app}\Lib\hotshot\*.pyc; Type: files
Name: {app}\Lib\hotshot\*.pyo; Type: files
Name: {app}\Lib\hotshot; Type: dirifempty
Name: {app}\Lib\lib-old\*.pyc; Type: files
Name: {app}\Lib\lib-old\*.pyo; Type: files
Name: {app}\Lib\lib-old; Type: dirifempty
Name: {app}\Lib\tkinter\*.pyc; Type: files
Name: {app}\Lib\tkinter\*.pyo; Type: files
Name: {app}\Lib\tkinter; Type: dirifempty
Name: {app}\Lib\test\*.pyc; Type: files
Name: {app}\Lib\test\*.pyo; Type: files
Name: {app}\Lib\test; Type: dirifempty
Name: {app}\Lib\xml\dom\*.pyc; Type: files
Name: {app}\Lib\xml\dom\*.pyo; Type: files
Name: {app}\Lib\xml\dom; Type: dirifempty
Name: {app}\Lib\xml\parsers\*.pyc; Type: files
Name: {app}\Lib\xml\parsers\*.pyo; Type: files
Name: {app}\Lib\xml\parsers; Type: dirifempty
Name: {app}\Lib\xml\sax\*.pyc; Type: files
Name: {app}\Lib\xml\sax\*.pyo; Type: files
Name: {app}\Lib\xml\sax; Type: dirifempty
Name: {app}\Lib\xml\*.pyc; Type: files
Name: {app}\Lib\xml\*.pyo; Type: files
Name: {app}\Lib\xml; Type: dirifempty

Name: {app}\Lib\site-packages\win32; Type: filesandordirs
Name: {app}\Lib\site-packages\win32com; Type: filesandordirs
Name: {app}\Lib\site-packages\win32comext; Type: filesandordirs
Name: {app}\Lib\site-packages\pythoncom.py*; Type: files
Name: {app}\Lib\site-packages; Type: dirifempty

Name: {app}\Lib\*.pyc; Type: files
Name: {app}\Lib; Type: dirifempty

Name: {app}\Tools\pynche\*.pyc; Type: files
Name: {app}\Tools\pynche\*.pyo; Type: files
Name: {app}\Tools\pynche; Type: dirifempty

Name: {app}\Tools\idle\*.pyc; Type: files
Name: {app}\Tools\idle\*.pyo; Type: files
Name: {app}\Tools\idle; Type: dirifempty

Name: {app}\Tools\scripts\*.pyc; Type: files
Name: {app}\Tools\scripts\*.pyo; Type: files
Name: {app}\Tools\scripts; Type: dirifempty

Name: {app}\Tools\versioncheck\*.pyc; Type: files
Name: {app}\Tools\versioncheck\*.pyo; Type: files
Name: {app}\Tools\versioncheck; Type: dirifempty

Name: {app}\Tools\webchecker\*.pyc; Type: files
Name: {app}\Tools\webchecker\*.pyo; Type: files
Name: {app}\Tools\webchecker; Type: dirifempty

Name: {app}\Tools; Type: dirifempty

