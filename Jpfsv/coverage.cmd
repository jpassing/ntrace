@echo off
if "x%VSINSTALLDIR%x" == "xx" (
	echo Run vcvars32 first!
	goto Exit
)

echo on
set "PATH=%PATH%;%VSINSTALLDIR%\Team Tools\Performance Tools"
vsinstr ..\bin\fre\i386\jpfsv.dll -coverage -verbose 
start vsperfmon -coverage -output:jpfsv.coverage

pushd ..\bin\fre\i386\
cfix32 testfsv.dll
vsperfcmd -shutdown
popd 

:Exit