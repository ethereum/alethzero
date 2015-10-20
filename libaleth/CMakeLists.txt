set(CMAKE_INCLUDE_CURRENT_DIR ON)

aux_source_directory(. SRC_LIST)

file(GLOB HEADERS "*.h")
file(GLOB UI_RESOURCES "*.ui")

# This is a hack until someone proposes a better solution. When building split repos
# ETH_DIR has not been set yet but is nonetheless used in the eth_add_resources() call.
# TODO: fix properly
set(ETH_DIR "${ETH_CMAKE_DIR}/../../libethereum" CACHE PATH "The path to the ethereum directory")

eth_add_resources("${CMAKE_CURRENT_SOURCE_DIR}/JSResources.cmake" "JSRES" "${ETH_DIR}/..")

# package the wallet
set(WALLET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../wallet")
set(WALLETRES "${CMAKE_CURRENT_BINARY_DIR}/WalletResources.hpp")
set(RLP_DIR "${ETH_CMAKE_DIR}/../../libweb3core/build/rlp")
add_custom_command(
    OUTPUT ${WALLETRES}
    COMMAND find ${WALLET_DIR} -type f ! -name ".git" -printf \"'%p' \" | xargs ${RLP_DIR}/./rlp assemble --dapp ${WALLET_DIR} > ${PROJECT_BINARY_DIR}/wallet.dapp 2> ${PROJECT_BINARY_DIR}/wallet.key
    COMMAND ${CMAKE_COMMAND} -DCMAKE_CURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR} -DETH_RES_FILE="${CMAKE_CURRENT_SOURCE_DIR}/WalletResources.cmake" -DETH_SCRIPTS_DIR="${ETH_SCRIPTS_DIR}"  -DOUT_FILE="WalletRes" -DPROJECT_BINARY_DIR="${PROJECT_BINARY_DIR}" -P "${ETH_SCRIPTS_DIR}/wallet_resources.cmake"
    COMMENT "Generating wallet resources"
)

set(EXECUTABLE aleth)

# it should be static library, cause there is a problem with creating QWidgets from dynamic libraries
# it fails with "Must construct a QApplication before a QWidget"
# https://forum.qt.io/topic/39042/can-t-create-a-qwidget-from-dynamic-library-must-construct-a-qapplication-before-a-qwidget
add_library(${EXECUTABLE} STATIC ${SRC_LIST} ${JSRES} ${WALLETRES} ${HEADERS} ${UI_RESOURCES})

set_target_properties(${EXECUTABLE} PROPERTIES AUTOUIC ON AUTOMOC ON)
eth_use(${EXECUTABLE} REQUIRED Qt::Core Qt::Widgets Qt::Network Web3::webthree Eth::ethcore Eth::p2p Eth::devcrypto Eth::ethereum Web3::web3jsonrpc Eth::natspec)
