copy /Y bin\chk\i386\testfbt_km* %SystemDrive%\drv32\

sc stop cfixkr_testfbt_km_wrk

pushd %SystemDrive%\drv32\
cfix32 -b -kern testfbt_km_wrk.sys
popd