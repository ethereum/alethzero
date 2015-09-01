cmake_minimum_required(VERSION 3.0.0)

# A place where should we look for *.cmake files
set(ETH_CMAKE_DIR   "${CMAKE_CURRENT_LIST_DIR}/../cmake"   CACHE PATH "The the path to the cmake directory")
list(APPEND CMAKE_MODULE_PATH ${ETH_CMAKE_DIR})

# Set cmake_policies
include(EthPolicy)
eth_policy()

# project name and version should be set after cmake_policy CMP0048
set(PROJECT_VERSION "0.9.43")
project(alethzero VERSION ${PROJECT_VERSION})

# Let's find our dependencies
include(EthDependencies)

# Figure out what compiler and system are we using
include(EthCompilerSettings)

# Include helper macros
include(EthExecutableHelper)

# Include utils
include(EthUtils)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
file(GLOB_RECURSE SRC_LIST "src/*.cpp" "plugins/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h" "plugins/*.h")
file(GLOB_RECURSE UI_RESOURCES "src/*.ui" "plugins/*.ui")

eth_add_resources("${CMAKE_CURRENT_SOURCE_DIR}/JSResources.cmake" "JSRES" "${CMAKE_CURRENT_SOURCE_DIR}/..")

eth_name(EXECUTABLE AlethZero)

# required for qt5_wrap_ui below, TODO: remove this
find_package(Qt5Widgets)

include_directories(src)

# eth_add_executable is defined in cmake/EthExecutableHelper.cmake
eth_add_executable(${EXECUTABLE}
	ICON alethzero
	UI_RESOURCES ${UI_RESOURCES} src/alethzero.icns ${JSRES}
	WIN_RESOURCES src/alethzero.rc
)

eth_use(${EXECUTABLE} REQUIRED Qt::Core Qt::Widgets Qt::WebEngineWidgets Eth::ethereum Web3::web3jsonrpc Eth::natspec Solidity)

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
