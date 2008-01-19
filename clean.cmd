rem bin
@rd /S /Q bin\chk
@rd /S /Q bin\fre

rem top level logs
@del *.log
@del *.err
@del *.wrn

rem project logs
@for /D %%d IN (*) DO @del %%d\*.log
@for /D %%d IN (*) DO @del %%d\*.err
@for /D %%d IN (*) DO @del %%d\*.wrn
@for /D %%d IN (*) DO @rd /S /Q %%d\bin

rem subproject intermediate dirs
@for /D %%d IN (*) DO @for /D %%s IN (%%d\*) DO @rd /S /Q %%s\objchk_wxp_x86
@for /D %%d IN (*) DO @for /D %%s IN (%%d\*) DO @rd /S /Q %%s\objfre_wxp_x86