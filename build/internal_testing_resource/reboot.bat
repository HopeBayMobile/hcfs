@if "%CD%" == "C:\Windows" (echo ===
	echo Error: On Windows, use [ net use * \\nas\ubuntu\CloudDataSolution ] to mount here as network drive and execute again.
	echo ===
	pause
	exit)
@prompt $$ 
adb wait-for-device
call install_busybox.bat
adb shell killall -9 hcfs
adb reboot
pause