@echo off

if x%SDKBASE%x == xx (
	echo.
	echo SDKBASE not set. Set to base directory of Windows SDK.
	echo Note that the path must not contain any spaces (required by build.exe)
	echo.
)

