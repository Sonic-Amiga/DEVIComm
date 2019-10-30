# SecureDeviceGrid<sup>TM</sup>
[logo]: http://securedevicegrid.com/wp-content/uploads/2016/05/logo_mark@2x.png "SecureDeviceGrid"
[A secure and powerful IoT platform for connectivity, software update, device management & realtime data analysis.](http://securedevicegrid.com)

## Sandbox chat clients

These clients can pair, connect, and send messages:

*   [Linux 64 bit (Build on Ubuntu 15.04)](./binaries/sandbox/ubuntu/sandbox-demo)
*   [OSX for Mac](./binaries/sandbox/macosx/Sandbox-demo)
*   [Windows](./binaries/sandbox/windows/win32-sandbox.zip)
*   [Android 4+](./binaries/sandbox/android/sandbox-demo.apk)
*   [iOS 8+ (xcode project)](./apps/MDGChat-iOS/)

The desktop clients are ready-to-run executables. They store their
local state on the filesystem in the directory where they are
launched. You can run multiple copies on one machine, provided you
keep those directories distinct.

The above Android/iOS applications needs to be installed on a phone
or simulator. If you are a developer for the given platform, you know
what to do.

## Embedded device examples and client library binaries.

These examples are for embedded boards. We don't provide complete
binaries for these development boards, as you have to compile for the
actual board you have. Instead we provide example programs
similar to the above binaries, but in source code as demo apps. The
example app includes a binary version of the communications library
for the platform.

*   [Broadcom WICED on NetX Duo (Using WICED-SDK v3.1.2)](./binaries/release_wiced.zip)
*   [TI CC3200 Simplelink SDK (Using CC3200SDK v1.1.0)](./binaries/release_cc3200.zip)
*   Raspberry Pi
*   Intel Edison

## Quick start guide

Here are the steps to get started using the client libraries.

*   Run two clients. If you run them on the same machine, start them in each their directory.
*   In either client (or both) execute `/email youremail@example.org <Your Name>` to register your client for the conference price-draw. This is optional.
*   On one of them, execute `/op` to open it for pairings. This should get you an OTP (one time password).
*   On the other, execute `/pair <OTP>` to pair with the first one.
*   Both acknowledges that the pairing took place by printing "Pairing completed" and stating the peerID of the other party. PeerID's are 32 bytes of data printed as hex.
*   You can now place a call from one to the other using `/pcr <peerID>`. This works in either direction. Both clients should print out "Routing started" and a connection ID (a small integer starting at zero).
*   You can now send a message through the secure tunnel from one client to the other using `/send 0 <Message>` where "0" is the connection ID you got when you executed `/pcr`.

## Licensing

You may use these examples and demos for evaluation purposes only.
Using SecureDeviceGrid<sup>TM</sup> in your product requires a
license. [Please contact us.](http://securedevicegrid.com)
