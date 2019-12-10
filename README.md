# TimeSyncServer
Ntp like server, used to synchronize clients time fast and precisely

# build
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

# install
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
