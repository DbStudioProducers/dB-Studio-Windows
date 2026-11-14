# ASIO host support — licensing decision

The project owner has confirmed that no Steinberg proprietary ASIO license agreement is currently in place. The dB Studio is a proprietary project. Therefore, ASIO support is disabled in all distributable builds and CI until the applicable licensing route is reviewed and approved.

JUCE documents that enabling `JUCE_ASIO` makes the program depend on ASIO SDK source files. JUCE's bundled ASIO headers do not remove the licensing obligation: the ASIO SDK license offers the proprietary Steinberg license or GPLv3. The GPLv3 alternative is not assumed compatible with this proprietary project.

CMake exposes `DBSTUDIO_ENABLE_ASIO` but deliberately fails if it is enabled while the license gate is closed. The default build uses Windows audio backends supported by JUCE, including WASAPI and DirectSound. ASIO4ALL will not be listed in this build. This is an intentional legal/supply-chain decision, not a failure to detect the driver.

If the project later obtains a signed Steinberg agreement, the gate can be reviewed and ASIO can be enabled with JUCE's bundled headers or a separately licensed SDK setup. Do not copy or redistribute third-party SDK headers or drivers in this repository or release packages.
