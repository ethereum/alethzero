cmake_minimum_required(VERSION 3.0)

cmake_policy(SET CMP0015 NEW)
# let cmake autolink dependencies on windows
cmake_policy(SET CMP0020 NEW)

cmake_policy(SET CMP0028 OLD)
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
set(ETH_CMAKE_DIR       "${CMAKE_CURRENT_LIST_DIR}/../cpp-ethereum-cmake"   CACHE PATH "The the path to the cmake directory")
set(ETH_DIR             "${CMAKE_CURRENT_LIST_DIR}/../cpp-ethereum"         CACHE PATH "The path to the cpp-ethereum directory")
set(ETH_BUILD_DIR_NAME  "build"                                             CACHE STRING "The name of the build directory in cpp-ethereum")
set(ETH_BUILD_DIR       "${ETH_DIR}/${ETH_BUILD_DIR_NAME}")
set(SOL_DIR             "${CMAKE_CURRENT_LIST_DIR}/../solidity"             CACHE PATH "The path to the solidity directory")
set(SOL_BUILD_DIR_NAME  "build"                                             CACHE STRING "The name of the build directory in soliditiy")
set(SOL_BUILD_DIR       "${SOL_DIR}/${SOL_BUILD_DIR_NAME}")

# A place where should we look for *.cmake files
list(APPEND CMAKE_MODULE_PATH ${ETH_CMAKE_DIR})

# A place where we should look for ethereum libraries 
set(CMAKE_LIBRARY_PATH ${ETH_BUILD_DIR} ${CMAKE_LIBRARY_PATH})
set(CMAKE_LIBRARY_PATH ${SOL_BUILD_DIR} ${CMAKE_LIBRARY_PATH})

set(GUI 1)		# TODO: remove once packages are all changed to optional
set(JSONRPC 1)	# TODO: remove once packages are all changed to optional

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

# eth_add_executable is defined in cmake/EthExecutableHelper.cmake
eth_add_executable(${EXECUTABLE} 
	ICON alethzero
	UI_RESOURCES ${UI_FILES} src/alethzero.icns ${JSRES}
	WIN_RESOURCES src/alethzero.rc
)

# required for qt5_wrap_ui below, TODO: remove this
# find_package(Qt5Core)
find_package(Qt5Widgets)

include_directories(src)
include_directories(${ETH_DIR})
include_directories(${SOL_DIR})

eth_use_QtCore(${EXECUTABLE} REQUIRED)
eth_use_QtWidgets(${EXECUTABLE} REQUIRED)
eth_use_QtWebEngineWidgets(${EXECUTABLE} REQUIRED)
eth_use_EthCore(${EXECUTABLE})
eth_use_Solidity(${EXECUTABLE} REQUIRED)
eth_use_WebThreeJsonRpc(${EXECUTABLE} REQUIRED)

# required because AUTOUIC can't handle multiple ui includes in one cpp file
qt5_wrap_ui(ui_Main.h src/Main.ui)
qt5_wrap_ui(ui_GetPassword.h src/GetPassword.ui)
qt5_wrap_ui(ui_GasPricing.h src/GasPricing.ui)

if (APPLE)
	add_custom_target(appdmg
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMAND ${CMAKE_COMMAND}
		-DAPP_DMG_EXE=${ETH_APP_DMG}
		-DAPP_DMG_FILE=appdmg.json.in
		-DAPP_DMG_ICON="alethzero/alethzero.icns"
		-DAPP_DMG_BACKGROUND="install-folder-bg.png"
		-DETH_BUILD_DIR="${CMAKE_BINARY_DIR}"
		-DETH_ALETHZERO_APP="$<TARGET_FILE_DIR:AlethZero>"
		-P "${ETH_SCRIPTS_DIR}/appdmg.cmake"
	)
endif ()

if (WIN32)
	# packaging stuff
	include(InstallRequiredSystemLibraries)
	set(CPACK_PACKAGE_NAME "Ethereum")
	set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "The Ethereum Toolset")
	set(CPACK_PACKAGE_VENDOR "ethereum.org")
	set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
	set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
	set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
	set(CPACK_GENERATOR "NSIS")
	
    # nsis specific stuff
	if (CMAKE_CL_64)
		set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
		set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "${CPACK_PACKAGE_NAME} ${CPACK_PACKAGE_VERSION} (Win64)")
	else ()
		set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES")
		set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "${CPACK_PACKAGE_NAME} ${CPACK_PACKAGE_VERSION}")
	endif()

	set(CPACK_NSIS_DISPLAY_NAME "Ethereum")
	set(CPACK_NSIS_HELP_LINK "https://github.com/ethereum/cpp-ethereum")
	set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/ethereum/cpp-ethereum")
	set(CPACK_NSIS_CONTACT "ethereum.org")
	set(CPACK_NSIS_MODIFY_PATH ON)
	set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/alethzero/alethzero.ico")
	set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/alethzero/alethzero.ico")

	include(CPack)
endif (WIN32)
