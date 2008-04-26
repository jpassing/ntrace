rem bin
@rd /S /Q bin\chk
@rd /S /Q bin\fre

@for /f "delims=" %%i in ('dir /ad/s/b obj*') do @rd /S /Q  %%i
@for /f "delims=" %%i in ('dir /s/b *.log') do @del  %%i
@for /f "delims=" %%i in ('dir /s/b *.err') do @del  %%i
@for /f "delims=" %%i in ('dir /s/b *.wrn') do @del  %%i

@del  Jpufbt\jpufbt\jpufbtmsg.mc
