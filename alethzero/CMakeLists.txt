
set(CMAKE_INCLUDE_CURRENT_DIR ON)
file(GLOB_RECURSE SRC_LIST "src/*.cpp" "plugins/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h" "plugins/*.h")
file(GLOB_RECURSE UI_RESOURCES "src/*.ui" "plugins/*.ui")

# required for eth_add_resources, TODO: remove this
if (DEFINED ethereum_SOURCE_DIR)
	set(ETH_DIR ${ethereum_SOURCE_DIR})
else()
	set(ETH_DIR "${CMAKE_CURRENT_LIST_DIR}/../cpp-ethereum"         CACHE PATH "The path to the cpp-ethereum directory")
endif()

eth_add_resources("${CMAKE_CURRENT_SOURCE_DIR}/JSResources.cmake" "JSRES" "${ETH_DIR}")

eth_name(EXECUTABLE AlethZero)

include_directories(src)

# eth_add_executable is defined in cmake/EthExecutableHelper.cmake
eth_add_executable(${EXECUTABLE}
	ICON alethzero
	UI_RESOURCES ${UI_RESOURCES} src/alethzero.icns ${JSRES}
	WIN_RESOURCES src/alethzero.rc
)

target_link_libraries(${EXECUTABLE} aleth)
eth_use(${EXECUTABLE} REQUIRED Qt::Core Qt::Widgets Qt::WebEngineWidgets Eth::ethereum Web3::web3jsonrpc Eth::natspec)
eth_use(${EXECUTABLE} OPTIONAL Solidity)

# required because AUTOUIC can't handle multiple ui includes in one cpp file
qt5_wrap_ui(ui_AlethZero.h src/AlethZero.ui)
qt5_wrap_ui(ui_Connect.h src/Connect.ui)

eth_install_executable(${EXECUTABLE})

eth_package(${EXECUTABLE}
	NAME "Ethereum"
	DESCRIPTION "The Ethereum Toolset"
	WIN_ICON "${CMAKE_CURRENT_SOURCE_DIR}/src/alethzero.ico"
	OSX_ICON "${CMAKE_CURRENT_SOURCE_DIR}/src/alethzero.icns"
)
