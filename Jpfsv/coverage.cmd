@echo off
if "x%VSINSTALLDIR%x" == "xx" (
	echo Run vcvars32 first!
	goto Exit
)

echo on
set "PATH=%PATH%;%VSINSTALLDIR%\Team Tools\Performance Tools"
vsinstr ..\bin\fre\i386\jpfsv.dll -coverage -verbose 
start vsperfmon -coverage -output:jpfsv.coverage
..\bin\fre\i386\fsvtest.exe
vsperfcmd -shutdown

:Exit