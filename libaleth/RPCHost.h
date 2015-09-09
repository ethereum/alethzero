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

#pragma once

#include <libdevcore/Common.h>

namespace dev
{

class SafeHttpServer;

namespace aleth
{

class AlethFace;
class WebThreeServer;

class RPCHost
{
public:
	RPCHost() = default;
	RPCHost(AlethFace* _aleth) { init(_aleth); }
	~RPCHost();

	void init(AlethFace* _aleth);

	WebThreeServer* web3Server() const { return m_server.get(); }
	SafeHttpServer* web3ServerConnector() const { return m_httpConnector.get(); }

private:
	std::shared_ptr<SafeHttpServer> m_httpConnector;
	std::shared_ptr<WebThreeServer> m_server;
};

}
}
