@if "%CD%" == "C:\Windows" (echo ===
	echo Error: On Windows, use [ net use * \\nas\ubuntu\CloudDataSolution ] to mount here as network drive and execute again.
	echo ===
	pause
	exit)
@ echo Device will reboot after install hcfs
adb root
adb remount
adb push hopebay /system/bin
adb shell busybox --install /system/xbin
adb push system /system
adb reboot
pause
