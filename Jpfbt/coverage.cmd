@echo off
if "x%VSINSTALLDIR%x" == "xx" (
	echo Run vcvars32 first!
	goto Exit
)

echo on
set "PATH=%PATH%;%VSINSTALLDIR%\Team Tools\Performance Tools"
vsinstr bin\fre\i386\um_test.exe -coverage -verbose -exclude:CustomProlog;ProcNoArgsLargeRet;ProcNoArgsSmallRet;StdcallProcSmallArgsSmallRet;FastcallProcSmallArgsLargeRet;StdcallRecursive
start vsperfmon -coverage -output:jpfbtu.coverage
bin\fre\i386\um_test.exe
vsperfcmd -shutdown

:Exit