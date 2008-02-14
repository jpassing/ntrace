@echo off



echo i386 checked				>> testrun.log
date /T 					>> testrun.log
echo ----------------------------------------- 	>> testrun.log

pushd bin\chk\i386
echo i386 checked
cfix32 -op ..\..\..\testrun.log -u -z .
if ERRORLEVEL 2 (
	echo.
	echo Errors occured [i386 checked]
	echo.
	popd
	goto Exit
)
popd



echo i386 free					>> testrun.log
date /T 					>> testrun.log
echo ----------------------------------------- 	>> testrun.log

pushd bin\fre\i386
echo i386 free
cfix32 -op ..\..\..\testrun.log -u -z .
if ERRORLEVEL 2 (
	echo.
	echo Errors occured [i386 free]
	echo.
	popd
	goto Exit
)
popd


:Exit
