# Hero_Broom's File Transfer 🚀

A high-performance file transfer utility built from scratch using C++ and the native Win32 API. 

### ✨ Key Features
* **Massive File Support**: Optimized for stable transfers of 5GB+ files.
* **Breakpoint Resume**: Automatically detects existing data and resumes transfers from where they left off—no more starting over after a disconnect.
* **Real-time Monitoring**: Visual progress bars and dynamic speed tracking (MB/s).
* **Native Win32 UI**: Pure C++ with zero heavy third-party GUI frameworks (like Qt or Electron), ensuring a tiny footprint and lightning-fast startup.
* **Flicker-Free UI**: Custom handling of `WM_CTLCOLORSTATIC` to ensure smooth text updates without background ghosting.

### 🛠️ Technical Implementation
* **Networking**: Powered by Windows Sockets (Winsock2) using reliable TCP protocols.
* **Multithreading**: Utilizes `CreateThread` to keep the UI responsive during intensive I/O operations.
* **Path Handling**: Supports Unicode/Wide Characters for full compatibility with Chinese or special-character file paths.

### 🚀 Usage
1. **Sender**: Select your file, enter the target IP and Port, then click "Confirm and Transfer".
2. **Receiver**: Set your listening port, choose a save path, and click "Confirm and Receive".

### 📦 Development Environment
* **Compiler**: MinGW 32-bit (GCC 9.2.0 or higher)
* **IDE**: Red Panda Dev-C++ 6.7.5
* **Linker Flags**: Requires `-lws2_32` and `-lgdi32`.

### 📥 Download
Check out the [Releases](https://github.com/YOUR_USERNAME/YOUR_REPO_NAME/releases) page to download the standalone `.exe` binary.