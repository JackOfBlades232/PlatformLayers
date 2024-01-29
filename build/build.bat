@echo off
@REM pushd build\Windows
@REM cl %COMPILER_FLAGS% ..\..\Windows\*.c ..\..\GameLibs\*.c ..\..\TestGames\game_%GAME%.c user32.lib gdi32.lib ole32.lib
pushd build
@REM set COMPILER_FLAGS=/Zi /O2 /Oi /Ob3 /Ot
set COMPILER_FLAGS=/Zi /D USE_ASSERTIONS=1
set GAME=jailbreak
cl %COMPILER_FLAGS% ..\Windows\*.c ..\GameLibs\*.c ..\TestGames\game_%GAME%.c user32.lib gdi32.lib ole32.lib
popd
@echo on
