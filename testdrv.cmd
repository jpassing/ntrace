copy /Y bin\chk\i386\testfbt_km* %SystemDrive%\drv\

sc stop cfixkr_testfbt_km_wrk

pushd %SystemDrive%\drv\
cfix32 -b -kern testfbt_km_wrk.sys
popd