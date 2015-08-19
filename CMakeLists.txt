cmake_minimum_required(VERSION 3.0.0)

# Figureout environment.
set(ETH_CMAKE_DIR   "${CMAKE_CURRENT_LIST_DIR}/../cpp-ethereum-cmake"   CACHE PATH "The the path to the cmake directory")
set(ETH_DIR         "${CMAKE_CURRENT_LIST_DIR}/../cpp-ethereum"         CACHE PATH "The path to the cpp-ethereum directory")
set(BUILD_DIR_NAME  "build"                                             CACHE STRING "The name of the build directory in cpp-ethereum")
set(ETH_BUILD_DIR   "${ETH_DIR}/${BUILD_DIR_NAME}")

# A place where should we look for *.cmake files
list(APPEND CMAKE_MODULE_PATH ${ETH_CMAKE_DIR})

# Set cmake policies
include(EthPolicy)

# Set project version. It must be done after setting Policies
set(PROJECT_VERSION "0.9.42")
project(alethzero VERSION ${PROJECT_VERSION})

# A place where we should look for ethereum libraries 
set(CMAKE_LIBRARY_PATH ${ETH_BUILD_DIR} ${CMAKE_LIBRARY_PATH})

set(GUI 1)		# TODO: remove once packages are all changed to optional
set(JSONRPC 1)	# TODO: remove once packages are all changed to optional

# Let's find our dependencies
include(EthDependencies)
include(EthDependenciesBoost)
include(EthDependenciesGui)

# Figure out what compiler and system are we using
include(EthCompilerSettings)

# Include helper macros
include(EthExecutableHelper)

# Include a directory with BuildInfo.h
include_directories(${ETH_BUILD_DIR})

find_package(Eth)

if (("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang") AND NOT (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "3.6") AND NOT APPLE)
	# Supress warnings for qt headers for clang+ccache
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-inconsistent-missing-override")
endif ()

set(CMAKE_AUTOMOC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
file(GLOB_RECURSE SRC_LIST "src/*.cpp" "plugins/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h" "plugins/*.h")

file(GLOB_RECURSE UI_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.ui" "${CMAKE_CURRENT_SOURCE_DIR}/plugins/*.ui")
foreach(ITEM ${UI_FILES})
    get_filename_component(UI_NAME ${ITEM} NAME_WE)
    qt5_wrap_ui(ui_${UI_NAME}.h ./${ITEM})
endforeach()

include_directories(BEFORE ${JSONCPP_INCLUDE_DIRS})
include_directories(BEFORE ..)
include_directories(${JSON_RPC_CPP_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../cpp-ethereum)
include_directories(${CPPETHEREUM_BUILD})

eth_name(EXECUTABLE AlethZero)
	
# eth_add_executable is defined in cmake/EthExecutableHelper.cmake
eth_add_executable(${EXECUTABLE} 
	ICON alethzero
	UI_RESOURCES ${UI_FILES} src/alethzero.icns
	WIN_RESOURCES src/alethzero.rc
)

target_link_libraries(${EXECUTABLE} Qt5::Core)
target_link_libraries(${EXECUTABLE} Qt5::Widgets)
target_link_libraries(${EXECUTABLE} Qt5::WebEngine)
target_link_libraries(${EXECUTABLE} Qt5::WebEngineWidgets)

if (APPLE AND (NOT "${Qt5Core_VERSION_STRING}" VERSION_LESS "5.5"))
	target_link_libraries(${EXECUTABLE} Qt5::WebEngineCore)
	target_link_libraries(${EXECUTABLE} Qt5::DBus)
	target_link_libraries(${EXECUTABLE} Qt5::PrintSupport)
endif()

target_link_libraries(${EXECUTABLE} ${ETH_CORE_LIBRARIES})
target_link_libraries(${EXECUTABLE} ${ETH_SOLIDITY_LIBRARIES})
target_link_libraries(${EXECUTABLE} ${JSON_RPC_CPP_SERVER_LIBRARIES})
target_link_libraries(${EXECUTABLE} ${JSONCPP_LIBRARIES})
target_link_libraries(${EXECUTABLE} ${CURL_LIBRARIES})
if (DEFINED WIN32 AND NOT DEFINED CMAKE_COMPILER_IS_MINGW)
	eth_copy_dlls(${EXECUTABLE} CURL_DLLS)
endif()
target_link_libraries(${EXECUTABLE} ${Boost_THREAD_LIBRARIES})
target_link_libraries(${EXECUTABLE} ${Boost_RANDOM_LIBRARIES})
target_link_libraries(${EXECUTABLE} ${Boost_FILESYSTEM_LIBRARIES})
target_link_libraries(${EXECUTABLE} ${Boost_SYSTEM_LIBRARIES})
target_link_libraries(${EXECUTABLE} ${LEVELDB_LIBRARIES})	#TODO: use the correct database library according to cpp-ethereum
target_link_libraries(${EXECUTABLE} ${CRYPTOPP_LIBRARIES})
target_link_libraries(${EXECUTABLE} ${MHD_LIBRARIES})
if (UNIX)
	target_link_libraries(${EXECUTABLE} pthread)
endif()

# eth_install_executable is defined in cmake/EthExecutableHelper.cmake
eth_install_executable(${EXECUTABLE} DLLS MHD_DLLS EVMJIT_DLLS OpenCL_DLLS)

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
