cmake_minimum_required(VERSION 3.10.2)
project (tssd)
add_executable(tssd src/main.cpp)

set(SERVICE_EXE_PATH ${CMAKE_INSTALL_PREFIX})
set(SYSTEMD_UNIT_FILE ${CMAKE_BINARY_DIR}/tssd.service)

configure_file(tssd.service.template ${SYSTEMD_UNIT_FILE})

install(TARGETS tssd RUNTIME DESTINATION bin)
install(FILES ${SYSTEMD_UNIT_FILE} DESTINATION /etc/systemd/system)