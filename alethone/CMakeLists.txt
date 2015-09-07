
set(CMAKE_INCLUDE_CURRENT_DIR ON)
file(GLOB SRC_LIST "*.cpp")
file(GLOB HEADERS "*.h")
file(GLOB UI_RESOURCES "*.ui")

eth_name(EXECUTABLE AlethOne)
eth_add_executable(${EXECUTABLE}
	ICON alethone
	UI_RESOURCES ${UI_RESOURCES} alethone.icns
	WIN_RESOURCES alethone.rc
)

set_target_properties(${EXECUTABLE} PROPERTIES AUTOUIC ON AUTOMOC ON)
target_include_directories(${EXECUTABLE} PUBLIC ..)
target_link_libraries(${EXECUTABLE} aleth)
eth_use(${EXECUTABLE} REQUIRED Qt::Core Qt::Widgets Qt::WebEngineWidgets Eth::ethereum)

eth_install_executable(${EXECUTABLE})
