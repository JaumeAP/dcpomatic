set PATH=%PATH%;c:\users\ci\bin_v2.18.x;c:\users\ci\workspace\dcpomatic\bin;c:\users\ci\workspace\dcpomatic\lib
xcopy ..\libdcp\tags build\tags\
copy ..\libdcp\ratings build\
copy ..\openssl\apps\openssl.exe build\src\tools
xcopy fonts build\fonts\
move build\fonts\fonts.conf.windows build\fonts\fonts.conf
build\src\tools\dcpomatic2.exe --cucumber
