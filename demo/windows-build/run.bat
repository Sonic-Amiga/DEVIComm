rem This helper script uses unzip from http://gnuwin32.sourceforge.net/packages/unzip.htm
if not exist tdg_lib.dll unzip -j ..\..\binaries\sandbox\windows\win32-sandbox.zip win32-sandbox\tdg_lib.dll
Debug\chat_demo.exe
