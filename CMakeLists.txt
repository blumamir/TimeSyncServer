cmake_minimum_required(VERSION 3.10.2)
project (tssd)
add_executable(tssd src/main.cpp)

install(TARGETS tssd RUNTIME DESTINATION bin)
install(FILES tssd.service DESTINATION /etc/systemd/system)