@echo off

copy /Y ..\Cfix\bin\chk\i386\jpdiag.dll bin\chk\i386\jpdiag.dll
copy /Y ..\Cfix\bin\fre\i386\jpdiag.dll bin\fre\i386\jpdiag.dll


echo i386 checked				>> testrun.log
time /T 					>> testrun.log
echo ----------------------------------------- 	>> testrun.log

pushd bin\chk\i386
echo i386 checked
cfix32 -out ..\..\..\testrun.log -u -z .
if ERRORLEVEL 2 (
	echo.
	echo Errors occured [i386 checked]
	echo.
	popd
	goto Exit
)
popd



echo i386 free					>> testrun.log
time /T 					>> testrun.log
echo ----------------------------------------- 	>> testrun.log

pushd bin\fre\i386
echo i386 free
cfix32 -out ..\..\..\testrun.log -u -z .
if ERRORLEVEL 2 (
	echo.
	echo Errors occured [i386 free]
	echo.
	popd
	goto Exit
)
popd

echo -----------------------------------------  >> testrun.log

:Exit
