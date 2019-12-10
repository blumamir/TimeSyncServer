# TimeSyncServer
Ntp like server, used to synchronize clients time fast and precisely
[![Build Status](https://travis-ci.com/BlumAmir/TimeSyncServer.svg?branch=master)](https://travis-ci.com/BlumAmir/TimeSyncServer)

# Build
To build the project from sources using CMake
```
git clone https://github.com/BlumAmir/TimeSyncServer.git
cd TimeSyncServer
mkdir build
cd build
cmake .. .
make
```
Replace cmake with ccmake if you need to configure non standart values for installation directories.

# Install
The server is build to run as systemd deamon service. Service name is `tssd` (time sync server daemon).
To install the server as daemon after built with make, run:
```
sudo make install
```

To run the server:
```
sudo systemctl start tssd
```

Configures the system to start the service at next reboot:
```
sudo systemctl enable tssd
```

# Clients
This project is a time sync **server** which serves time sync **clients**. Currently client library is availible for arduino espressif boards [here](https://github.com/BlumAmir/TimeSyncClientArduino)
