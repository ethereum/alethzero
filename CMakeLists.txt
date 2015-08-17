cmake_minimum_required(VERSION 2.8.12)

cmake_policy(SET CMP0015 NEW)
# let cmake autolink dependencies on windows
cmake_policy(SET CMP0020 NEW)
# this policy was introduced in cmake 3.0
# remove if, once 3.0 will be used on unix
if (${CMAKE_MAJOR_VERSION} GREATER 2)
	cmake_policy(SET CMP0043 OLD)
endif()

set(CMAKE_AUTOMOC ON)

set(BUILD_DIR_NAME "build" CACHE STRING "The name of the build directory in cpp-ethereum")
set(CPPETHEREUM_BUILD "${CMAKE_CURRENT_SOURCE_DIR}/../cpp-ethereum/${BUILD_DIR_NAME}")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-ethereum/cmake)
set(GUI 1)
set(JSONRPC 1)
add_definitions(-DETH_TRUE)
include(EthDependencies)
include(EthExecutableHelper)
include(EthCompilerSettings)

set(CMAKE_LIBRARY_PATH ${CPPETHEREUM_BUILD} ${CMAKE_LIBRARY_PATH})
find_package(Eth)
find_package(Qt5Core)
find_package(Qt5Widgets)

if (("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang") AND NOT (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "3.6") AND NOT APPLE)
	# Supress warnings for qt headers for clang+ccache
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-inconsistent-missing-override")
endif ()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
file(GLOB_RECURSE SRC_LIST "./plugins/*.cpp" "./src/*.cpp")
file(GLOB_RECURSE HEADERS "./plugins/*.h" "./src/*.h")

file(GLOB_RECURSE UI_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/*.ui")
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

target_link_libraries(${EXECUTABLE} ${ETH_CORE_LIBRARIES})
target_link_libraries(${EXECUTABLE} ${ETH_SOLIDITY_LIBRARY})
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

if (SERPENT)
	target_link_libraries(${EXECUTABLE} serpent)
endif()

# eth_install_executable is defined in cmake/EthExecutableHelper.cmake
eth_install_executable(${EXECUTABLE} DLLS MHD_DLLS EVMJIT_DLLS OpenCL_DLLS)

