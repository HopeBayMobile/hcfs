@if "%CD%" == "C:\Windows" (echo ===
	echo Error: On Windows, use [ net use * \\nas\ubuntu\CloudDataSolution ] to mount here as network drive and execute again.
	echo ===
	pause
	exit)
adb wait-for-device
adb pull /system/bin/app_process32 armeabi/app_process
adb pull /system/bin/linker armeabi
adb pull /system/lib/libc.so armeabi
adb forward tcp:5678 tcp:5678
..\..\..\..\HCFS_android\android-ndk-r10e-win64\toolchains\arm-linux-androideabi-4.9\prebuilt\windows-x86_64\bin\arm-linux-androideabi-gdb ^
-x gdb.setup
pause
