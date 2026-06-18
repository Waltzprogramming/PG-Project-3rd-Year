@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b %errorlevel%
cl.exe /nologo /std:c++20 /EHsc /O2 /MD /W3 /I"C:\Users\sasuk\source\repos\PG\Menu\RaylibMenu\vcpkg_installed\x64-windows\include" "C:\Users\sasuk\source\repos\PG\Menu\RaylibMenu\menu_raylib.cpp" /Fe:"C:\Users\sasuk\source\repos\PG\Menu\RaylibMenu\bin\PaperPinixRaylibMenu.exe" /link /LIBPATH:"C:\Users\sasuk\source\repos\PG\Menu\RaylibMenu\vcpkg_installed\x64-windows\lib" raylib.lib opengl32.lib gdi32.lib winmm.lib shell32.lib user32.lib
