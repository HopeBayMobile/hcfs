@if "%CD%" == "C:\Windows" (echo ===
	echo Error: On Windows, use [ net use * \\nas\ubuntu\CloudDataSolution ] to mount here as network drive and execute again.
	echo ===
	pause
	exit)
adb wait-for-device
adb root
adb wait-for-device
adb remount
adb push system /system
pause
