## How to Build Locally on Windows

### Requirements
- [Visual Studio Express 2015 for Windows Desktop](https://www.visualstudio.com/vs/older-downloads/)
- [CMake](https://cmake.org/download/)
- [Qt 5.9.2](https://www.qt.io/download-open-source/)

### Building

#### Using CMake to Create a Visual Studio Project
1. Launch CMake GUI
1. In `Where is the source code`, navigate to `$kumoworks/sources`
1. In `Where to build the binaries`, navigate to `$kumoworks/build`
1. Click on Configure and select Visual Studio 14 2015 Win64.
1. If Qt was installed to a directory other than the default, and the error `Specify QTDIR properly` appears, navigate to the `QTDIR` install folder and specify the path to `msvc2015_64`. Rerun Configure.
1. Click Generate
#### Building with MSVC
1. Open `$kumoworks/build/KumoWorks.sln` and change to `Debug` or `Release` in the top bar.
1. The output will be in the corresponding folder in `$kumoworks/build/`

#### Creating Translation Files
Qt translation files are generated first from the source code to .ts files, then from .ts files to a .qm file (in `$kumoworks/sources/loc/` ).  These files can be created in Visual Studio if the `translation_KumoWorks` project and `Build translation_KumoWorks only` (`translation_KumoWorks`のみをビルド」) is used.

#### Copy configuration files
- Create a folder `config` in the application's current directory.
- Copy `$kumoworks/sources/loc/` to `config/loc/`
- Copy `$kumoworks/misc/ini/` to `config/ini/`
- Copy `$kumoworks/misc/licenses/` to `config/licenses`

## How to Build and Create Installer on Windows

### Requirements
- [Visual Studio Express 2015 for Windows Desktop](https://www.visualstudio.com/vs/older-downloads/)
- [CMake](https://cmake.org/download/)
- [Qt 5.9.2](https://www.qt.io/download-open-source/)
- [Anaconda](https://www.anaconda.com/download/)
- [Inno Setup](http://www.jrsoftware.org/isinfo.php)

### Run the Batch File
Via Command Prompt:

```
> cd windows
> kw_setup.bat
```
If you would do not specify the environment variable `QTDIR` then add a path to Qt as the first argument like as follows:

`> kw_setup.bat "C:\Qt\Qt5.9.2\5.9.2\msvc2015_64"`

It will automatically cmake, build the MSVC solution and will create an installer in `$kumoworks/windows/Output` .