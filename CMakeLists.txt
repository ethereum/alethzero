cmake_minimum_required(VERSION 2.8.12)

# Figure out environment.
set(ETH_CMAKE_DIR   "${CMAKE_CURRENT_LIST_DIR}/../cpp-ethereum-cmake"   CACHE PATH "The the path to the cmake directory")
set(ETH_DIR         "${CMAKE_CURRENT_LIST_DIR}/../cpp-ethereum"         CACHE PATH "The path to the cpp-ethereum directory")
set(BUILD_DIR_NAME  "build"                                             CACHE STRING "The name of the build directory in cpp-ethereum")
set(ETH_BUILD_DIR   "${ETH_DIR}/${BUILD_DIR_NAME}")

# A place where should we look for *.cmake files
list(APPEND CMAKE_MODULE_PATH ${ETH_CMAKE_DIR})

# set cmake_policies
include(EthPolicy)
eth_policy()

# project name and version should be set after cmake_policy CMP0048
set(PROJECT_VERSION "0.9.42")
project(alethzero VERSION ${PROJECT_VERSION})

# A place where we should look for ethereum libraries
set(CMAKE_LIBRARY_PATH ${ETH_BUILD_DIR})

set(GUI 1)		# TODO: remove once packages are all changed to optional

# Let's find our dependencies
include(EthDependencies)

# Figure out what compiler and system are we using
include(EthCompilerSettings)

# Include helper macros
include(EthExecutableHelper)

# Include utils
include(EthUtils)

# Include a directory with BuildInfo.h
include_directories(${ETH_BUILD_DIR})

set(CMAKE_INCLUDE_CURRENT_DIR ON)
file(GLOB_RECURSE SRC_LIST "src/*.cpp" "plugins/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h" "plugins/*.h")

eth_add_resources("${CMAKE_CURRENT_SOURCE_DIR}/JSResources.cmake" "JSRES" "${ETH_DIR}")

eth_name(EXECUTABLE AlethZero)

# required for qt5_wrap_ui below, TODO: remove this
# find_package(Qt5Core)
find_package(Qt5Widgets)

include_directories(src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../cpp-ethereum)

# eth_add_executable is defined in cmake/EthExecutableHelper.cmake
eth_add_executable(${EXECUTABLE}
	ICON alethzero
	UI_RESOURCES src/Main.ui src/GetPassword.ui src/alethzero.icns ${JSRES}
	WIN_RESOURCES src/alethzero.rc
)

eth_use(${EXECUTABLE} REQUIRED Qt::Core Qt::Widgets Qt::WebEngineWidgets EthCore WebThreeJsonRpc)
eth_use(${EXECUTABLE} OPTIONAL Solidity)

# required because AUTOUIC can't handle multiple ui includes in one cpp file
qt5_wrap_ui(ui_Main.h src/Main.ui)
qt5_wrap_ui(ui_GetPassword.h src/GetPassword.ui)

eth_install_executable(${EXECUTABLE})

eth_package(${EXECUTABLE}
	NAME "Ethereum"
	DESCRIPTION "The Ethereum Toolset"
	WIN_ICON "${CMAKE_CURRENT_SOURCE_DIR}/src/alethzero.ico"
	OSX_ICON "${CMAKE_CURRENT_SOURCE_DIR}/src/alethzero.icns"
)

