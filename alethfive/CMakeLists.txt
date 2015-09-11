
set(CMAKE_INCLUDE_CURRENT_DIR ON)
file(GLOB SRC_LIST "*.cpp" "*.qrc")
file(GLOB HEADERS "*.h")
file(GLOB UI_RESOURCES "*.ui")

eth_name(EXECUTABLE AlethFive)
eth_add_executable(${EXECUTABLE}
	ICON alethfive
	UI_RESOURCES ${UI_RESOURCES} alethfive.icns
	WIN_RESOURCES alethfive.rc
)

target_include_directories(${EXECUTABLE} PUBLIC ..)
set_target_properties(${EXECUTABLE} PROPERTIES AUTOUIC ON AUTOMOC ON AUTORCC ON)
target_link_libraries(${EXECUTABLE} aleth)
eth_use(${EXECUTABLE} REQUIRED Qt::Core Qt::Widgets Qt::WebEngine Qt::WebEngineWidgets Eth::ethereum)

eth_install_executable(${EXECUTABLE})
