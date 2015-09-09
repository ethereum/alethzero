set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSTATICLIB")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

aux_source_directory(. SRC_LIST)

file(GLOB HEADERS "*.h")
file(GLOB UI_RESOURCES "*.ui")

set(EXECUTABLE aleth)
add_library(${EXECUTABLE} ${SRC_LIST} ${HEADERS} ${UI_RESOURCES})
set_target_properties(${EXECUTABLE} PROPERTIES AUTOUIC ON AUTOMOC ON)
eth_use(${EXECUTABLE} REQUIRED Qt::Core Qt::Widgets Web3::webthree Eth::devcore Eth::p2p Eth::devcrypto Eth::ethereum Web3::web3jsonrpc Eth::natspec)

install( TARGETS ${EXECUTABLE} RUNTIME DESTINATION bin ARCHIVE DESTINATION lib LIBRARY DESTINATION lib )
install( FILES ${HEADERS} DESTINATION include/${EXECUTABLE} )

