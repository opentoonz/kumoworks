@echo off

rem Based on https://github.com/opentoonz/opentoonz_installer_windows

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"

pushd ..

rem QTDIR by default
if "%~1"=="" (
    set QT_PATH_ARG=%QTDIR%
) else (
    set QT_PATH_ARG=%~1
)

echo "QT_PATH_ARG=%QT_PATH_ARG%"

mkdir build
pushd build
@echo on
cmake ../sources -G "Visual Studio 14 Win64" -DQTDIR="%QT_PATH_ARG%"
@echo off
if errorlevel 1 exit /b 1

MSBuild /m KumoWorks.sln /p:Configuration=Release
if errorlevel 1 exit /b 1
popd
popd

rem clean
rmdir /S /Q winInstallerTmp

rem Program Files
mkdir winInstallerTmp\program\config

copy /Y ..\build\Release\KumoWorks.exe winInstallerTmp\program
xcopy /YEI ..\sources\loc winInstallerTmp\program\config\loc
xcopy /YEI ..\misc\ini winInstallerTmp\program\config\ini
xcopy /YEI ..\misc\licenses winInstallerTmp\program\config\licenses
copy /Y "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\redist\x64\Microsoft.VC140.CRT\*.dll" winInstallerTmp\program
copy /Y "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\redist\x64\Microsoft.VC140.OpenMP\*.dll" winInstallerTmp\program

copy /Y ..\macOS\English.lproj\License.rtf winInstallerTmp\license.rtf
copy /Y ..\macOS\Japanese.lproj\License.rtf winInstallerTmp\license_ja.rtf

@echo on
"%QT_PATH_ARG%\bin\windeployqt.exe" --release winInstallerTmp\program\KumoWorks.exe
@echo off
if errorlevel 1 exit /b 1

rem Download and install innosetup-5.5.9-unicode.exe
rem from http://www.jrsoftware.org/isdl.php in advance.

python filelist.py
"C:\Program Files (x86)\Inno Setup 5\ISCC.exe" setup.iss
if errorlevel 1 exit /b 1

exit /b 0
