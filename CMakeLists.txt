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
set(BUILD_DIR_NAME "build" CACHE STRING "The name of the build directory in cpp-ethereum")
set(CPPETHEREUM_BUILD "${CMAKE_CURRENT_SOURCE_DIR}/../cpp-ethereum/${BUILD_DIR_NAME}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CPPETHEREUM_BUILD})
message("CPPETHEREUM_BUILD: ${CPPETHEREUM_BUILD}")

list(APPEND CMAKE_MODULE_PATH ${ETH_CMAKE_DIR})

set(GUI 1)		# TODO: remove once packages are all changed to optional
set(JSONRPC 1)	# TODO: remove once packages are all changed to optional

include(EthDependencies)
include(EthExecutableHelper)
include(EthCompilerSettings)

find_package(Qt5Core)
find_package(Qt5Widgets)

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

find_package (Qt5WebEngine)
find_package (Qt5WebEngineWidgets)
if (APPLE AND (NOT "${Qt5Core_VERSION_STRING}" VERSION_LESS "5.5"))
# TODO: remove indirect dependencies once macdeployqt is fixed
	find_package (Qt5WebEngineCore)
	find_package (Qt5DBus)
	find_package (Qt5PrintSupport)
endif()

file(GLOB HEADERS "*.h")

if (APPLE)
	set(EXECUTABLE AlethZero)
else ()
	set(EXECUTABLE alethzero)
endif ()
	
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

set(CMAKE_LIBRARY_PATH ${CPPETHEREUM_BUILD};${CMAKE_LIBRARY_PATH})
find_library(solidity_LIBRARY NAMES solidity)
if (solidity_LIBRARY)
	set(SOLIDITY 1)
endif()
find_library(serpent_LIBRARY NAMES serpent)
if (serpent_LIBRARY)
	set(SERPENT 1)
endif()
find_library(evmjit_LIBRARY NAMES evmjit)
if (evmjit_LIBRARY)
	set(EVMJIT 1)
endif()

set(LIBS web3jsonrpc;webthree;whisper;ethereum;evm;ethcore;lll;p2p;evmasm;devcrypto;evmcore;natspec;devcore;ethash-cl;ethash;secp256k1;scrypt;jsqrc)
foreach (l ${LIBS})
	find_library(${l}_LIBRARY NAMES ${l})
	target_link_libraries(${EXECUTABLE} ${${l}_LIBRARY})
endforeach()

if (SERPENT)
	target_link_libraries(${EXECUTABLE} ${serpent_LIBRARY})
	add_definitions(-DETH_SERPENT)
endif()
if (SOLIDITY)
	target_link_libraries(${EXECUTABLE} ${solidity_LIBRARY})
	add_definitions(-DETH_SOLIDITY)
endif()
if (EVMJIT)
	target_link_libraries(${EXECUTABLE} ${evmjit_LIBRARY})
	add_definitions(-DETH_EVMJIT)
endif()

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
		-DETH_MIX_APP="$<TARGET_FILE_DIR:mix>"
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
	# seems to be not working
	# set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/alethzero/alethzero.bmp")

	# our stuff
	#set(CPACK_COMPONENT_ALETHZERO_GROUP "Applications")
	#set(CPACK_COMPONENT_MIX_GROUP "Applications")
	#set(CPACK_COMPONENT_SOLC_GROUP "CLI")
	#set(CPACK_COMPONENT_ETH_GROUP "CLI")
	#set(CPACK_COMPONENT_ETHMINER_GROUP "CLI")
	#set(CPACK_COMPONENT_RLP_GROUP "CLI")
	#set(CPACK_COMPONENT_ABI_GROUP "CLI")

	#set(CPACK_COMPONENTS_ALL alethzero mix solc eth ethminer rlp abi)

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
