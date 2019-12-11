cmake_minimum_required(VERSION 3.10.2)
project (tssd)
add_executable(tssd src/main.cpp)

# user configuration with default value for install
set(SYSTEMD_SERVICES_INSTALL_DIR "/etc/systemd/system" CACHE STRING "location where systemd unit files (.service) are installed")
set(SYSTEMD_SERVICES_PID_FILES_DIR "/var/run" CACHE STRING "location where systemd pid lock files are placed")

# calculated values for install
set(SERVICE_EXE_PATH ${CMAKE_INSTALL_PREFIX})
set(SYSTEMD_UNIT_FILE ${CMAKE_BINARY_DIR}/tssd.service)
set(SYSTEMD_SERVICES_PID_FILE ${SYSTEMD_SERVICES_PID_FILES_DIR}/tssd.pid)

# replace value in the service template and create the final version to be used
configure_file(tssd.service.template ${SYSTEMD_UNIT_FILE})

install(TARGETS tssd RUNTIME DESTINATION bin)
install(FILES ${SYSTEMD_UNIT_FILE} DESTINATION ${SYSTEMD_SERVICES_INSTALL_DIR})
