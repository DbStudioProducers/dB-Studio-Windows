# ASIO host support — dependency and licensing gate

The desktop application uses JUCE's ASIO host backend only when configured with `DBSTUDIO_ENABLE_ASIO=ON`. The Steinberg ASIO SDK `common` directory must be supplied locally through `DBSTUDIO_ASIO_SDK_COMMON_DIR` and contain `iasiodrv.h`. CMake fails early with an explicit message if the header is absent.

Example (internal development build):

```powershell
cmake -S . -B build -A x64 -DDBSTUDIO_ENABLE_ASIO=ON -DDBSTUDIO_ASIO_SDK_COMMON_DIR="C:/ASIOSDK/common"
cmake --build build --config Release --target dBStudio
```

The default remains OFF so clean CI builds and the Windows 8.1 legacy build do not silently fail when the proprietary SDK is not provisioned. This means builds produced by the current public CI do not expose ASIO until a licensed SDK is provided to the build environment.

ASIO4ALL is a driver, not the SDK headers required to compile an ASIO host. The host enumerates ASIO drivers only in an ASIO-enabled build and when the driver is installed/registered in Windows.

Steinberg's current developer information describes licensing requirements for ASIO technology. Before distributing a proprietary ASIO-enabled binary, the project owner must obtain and comply with the applicable Steinberg ASIO license. The SDK also offers a GPLv3 alternative; that is not automatically compatible with a proprietary/closed-source distribution. Do not copy SDK headers into this repository or artifacts without confirming the applicable license.

No SDK, driver, installer, or third-party binary is bundled by this project. The app must continue to support Windows audio backends when ASIO is disabled.
