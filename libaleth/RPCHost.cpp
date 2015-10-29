/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file RPCHost.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#include "RPCHost.h"
#include <libdevcore/Log.h>
#include <libweb3jsonrpc/ModularServer.h>
#include <libweb3jsonrpc/MemoryDB.h>
#include <libweb3jsonrpc/SafeHttpServer.h>
#include <libweb3jsonrpc/Net.h>
#include <libweb3jsonrpc/Bzz.h>
#include <libweb3jsonrpc/Web3.h>
#include <libwebthree/WebThree.h>
#include "WebThreeServer.h"
#include "AlethFace.h"

using namespace std;
using namespace dev;
using namespace aleth;

void RPCHost::init(AlethFace* _aleth)
{
	m_web3Face = new WebThreeServer(_aleth);
	m_whisperFace = new AlethWhisper(*_aleth->web3(), {});
	m_rpcServer.reset(new ModularServer<
					  WebThreeServer,
					  rpc::DBFace,
					  rpc::WhisperFace,
					  rpc::NetFace,
					  rpc::BzzFace,
					  rpc::Web3Face>(m_web3Face, new rpc::MemoryDB(), m_whisperFace, new rpc::Net(*_aleth->web3()), new rpc::Bzz(*_aleth->web3()->swarm()), new rpc::Web3(_aleth->web3()->clientVersion())));
	m_httpConnectorId = m_rpcServer->addConnector(new dev::SafeHttpServer(8545, "", "", 4));
	m_ipcConnectorId = m_rpcServer->addConnector(new dev::IpcServer("geth"));
	m_rpcServer->StartListening();
}

RPCHost::~RPCHost()
{
}

std::string RPCHost::newSession(SessionPermissions const& _p)
{
	return m_web3Face->newSession(_p);
}

SafeHttpServer* RPCHost::httpConnector() const
{
	return static_cast<dev::SafeHttpServer*>(m_rpcServer->connector(m_httpConnectorId));
}

IpcServer* RPCHost::ipcConnector() const
{
	return static_cast<dev::IpcServer*>(m_rpcServer->connector(m_ipcConnectorId));
}
