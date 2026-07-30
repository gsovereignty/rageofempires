## macOS compatibility

Default macOS builds compile the locked SDL3 source and set both app and
embedded SDL3 slices to a macOS 11.0 minimum deployment version. This is the
supported distributable configuration.

Developer builds using `-DAOE_BUILD_SDL3=OFF` use an installed SDL3 package.
CMake does not claim macOS 11 compatibility for that configuration: its
minimum OS and available architectures follow the selected SDK and SDL3
package unless the developer explicitly supplies compatible
`CMAKE_OSX_DEPLOYMENT_TARGET` and `CMAKE_OSX_ARCHITECTURES` values.
