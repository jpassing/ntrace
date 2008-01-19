@echo off

if x%SDKBASE%x == xx (
	echo.
	echo SDKBASE not set. Set to base directory of Windows SDK.
	echo Note that the path must not contain any spaces - required by build.exe
	echo.
	goto exit
)

if NOT x%DDKBUILDENV%x == xx (
	echo.
	echo Build environment found. Execute this command in a normal shell, not in a WDK shell.
	echo.
	goto exit
)


cmd /C ddkbuild -WLHXP checked . -cZ
cmd /C ddkbuild -WLHXP free . -cZ

:exit