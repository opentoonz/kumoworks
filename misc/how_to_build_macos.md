## How to Build Locally on macOS

### Requirements
- CMake 3.12.1
- [Qt 5.9.2](https://www.qt.io/download-open-source/)
- Xcode 9.2

### Building
```
> mkdir build
> cd build
> cmake ../sources/
> make
> sh ../macOS/macdeploy.sh
> open KumoWorks.app
```

## How to Generate the Installer

### Requirements
- Xcode (`pkgbuild, productbuild`)
- Qt
- gsed

### Run script

`./macOS/macinstaller.rb [BUNDLE_PATH] [VERSION(floatnum)]`

Where;
- BUNDLE_PATH: path to built bundle
- VERSION: version of the package

Example
```
./macOS/macinstaller.rb ./build/KumoWorks.app 0.0.1
```