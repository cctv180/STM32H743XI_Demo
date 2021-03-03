rd /Q /S MDK-ARM\Objects
rd /Q /S MDK-ARM\Listings
rd /Q /S MDK-ARM\DebugConfig
rd /Q /S MDK-ARM\project

del /Q MDK-ARM\*.bak
del /Q MDK-ARM\*.dep
del /Q MDK-ARM\*.uvguix.*
del /Q MDK-ARM\*.scvd
#del /Q MDK-ARM\JLink*
del /Q MDK-ARM\JLinkLog.txt
del /Q MDK-ARM\*.uvgui.*

rd  /Q /S EmbeddedProject1\VisualGDB
rd  /Q /S EmbeddedProject1\.vs
del /Q EmbeddedProject1\*.vcxproj.user

rd /Q /S EWARM\bin
rd /Q /S EWARM\obj
rd /Q /S EWARM\settings
rd /Q /S EWARM\.vs

del /Q   EWARM\settings\*.crun
del /Q   EWARM\settings\*.dnx
del /Q   EWARM\settings\*.cspy
del /Q   EWARM\settings\*.general.xcl
del /Q   EWARM\settings\*.wsdt
del /Q   EWARM\settings\*.cspy.bat
del /Q   EWARM\settings\*.dbgdt
del /Q   EWARM\settings\*.ps1
del /Q   EWARM\stm32f4.dep
del /Q   EWARM\*.sln
del /Q   EWARM\*.vcxproj
del /Q   EWARM\*.filters
del /Q   EWARM\*.opendb
del /Q   EWARM\*VC.db
del /Q   EWARM\IAR_Gen.lua
del /Q   EWARM\*.user

exit