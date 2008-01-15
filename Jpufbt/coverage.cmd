@echo off
if "x%VSINSTALLDIR%x" == "xx" (
	echo Run vcvars32 first!
	goto Exit
)

echo on
set "PATH=%PATH%;%VSINSTALLDIR%\Team Tools\Performance Tools"
vsinstr bin\fre\i386\ufbttest.exe -coverage -verbose -exclude:JpqlpcSendReceive
vsinstr bin\fre\i386\jpufag.dll -coverage -verbose -exclude:JpqlpcSendReceive
start vsperfmon -coverage -output:jpufbt.coverage
bin\fre\i386\ufbttest.exe
vsperfcmd -shutdown

:Exit