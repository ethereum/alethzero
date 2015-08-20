cmake_minimum_required(VERSION 2.8.12)

cmake_policy(SET CMP0015 NEW)
# let cmake autolink dependencies on windows
cmake_policy(SET CMP0020 NEW)
# this policy was introduced in cmake 3.0
# remove if, once 3.0 will be used on unix
if (${CMAKE_MAJOR_VERSION} GREATER 2)
	cmake_policy(SET CMP0043 OLD)
endif()

set(PROJECT_VERSION "0.9.42")
if (${CMAKE_VERSION} VERSION_GREATER 3.0)
	cmake_policy(SET CMP0042 OLD) 	# fix MACOSX_RPATH
	cmake_policy(SET CMP0048 NEW) 	# allow VERSION argument in project()
	project(alethzero VERSION ${PROJECT_VERSION})
else()
	project(alethzero)
endif()

# Figure out environment.
set(ETH_CMAKE_DIR   "${CMAKE_CURRENT_LIST_DIR}/../cpp-ethereum-cmake"   CACHE PATH "The the path to the cmake directory")
set(ETH_DIR         "${CMAKE_CURRENT_LIST_DIR}/../cpp-ethereum"         CACHE PATH "The path to the cpp-ethereum directory")
set(BUILD_DIR_NAME  "build"                                             CACHE STRING "The name of the build directory in cpp-ethereum")
set(ETH_BUILD_DIR   "${ETH_DIR}/${BUILD_DIR_NAME}")

# A place where should we look for *.cmake files
list(APPEND CMAKE_MODULE_PATH ${ETH_CMAKE_DIR})

# A place where we should look for ethereum libraries
set(CMAKE_LIBRARY_PATH ${ETH_BUILD_DIR})

set(GUI 1)		# TODO: remove once packages are all changed to optional

# Let's find our dependencies
include(EthDependencies)

# Figure out what compiler and system are we using
include(EthCompilerSettings)

# Include helper macros
include(EthExecutableHelper)

# Include a directory with BuildInfo.h
include_directories(${ETH_BUILD_DIR})

set(CMAKE_INCLUDE_CURRENT_DIR ON)
file(GLOB_RECURSE SRC_LIST "src/*.cpp" "plugins/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h" "plugins/*.h")

if (APPLE)
	set(EXECUTABLE AlethZero)
else ()
	set(EXECUTABLE alethzero)
endif ()

# required for qt5_wrap_ui below, TODO: remove this
# find_package(Qt5Core)
find_package(Qt5Widgets)

include_directories(src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../cpp-ethereum)

# eth_add_executable is defined in cmake/EthExecutableHelper.cmake
eth_add_executable(${EXECUTABLE}
	ICON alethzero
	UI_RESOURCES src/Main.ui src/GetPassword.ui src/GasPricing.ui src/alethzero.icns
	WIN_RESOURCES src/alethzero.rc
)

eth_use(${EXECUTABLE} REQUIRED Qt::Core Qt::Widgets Qt::WebEngineWidgets EthCore WebThreeJsonRpc)
eth_use(${EXECUTABLE} OPTIONAL Solidity)

# required because AUTOUIC can't handle multiple ui includes in one cpp file
qt5_wrap_ui(ui_Main.h src/Main.ui)
qt5_wrap_ui(ui_GetPassword.h src/GetPassword.ui)
qt5_wrap_ui(ui_GasPricing.h src/GasPricing.ui)

eth_package(${EXECUTABLE}
	NAME "Ethereum"
	DESCRIPTION "The Ethereum Toolset"
	WIN_ICON "${CMAKE_CURRENT_SOURCE_DIR}/src/alethzero.ico"
	OSX_ICON "${CMAKE_CURRENT_SOURCE_DIR}/src/alethzero.icns"
)

