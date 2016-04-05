@if "%CD%" == "C:\Windows" (echo ===
	echo Error: On Windows, use [ net use * \\nas\ubuntu\CloudDataSolution ] to mount here as network drive and execute again.
	echo ===
	pause
	exit)
@prompt $$ 
adb wait-for-device
adb root
adb wait-for-device
adb disable-verity | findstr /I "reboot"
if %errorlevel% == 0 (
	adb reboot
)

adb wait-for-device
adb root
adb wait-for-device
adb remount
adb push hopebay /system/bin
adb shell busybox --install /system/xbin
pause