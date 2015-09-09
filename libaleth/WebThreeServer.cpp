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
/** @file OurWebThreeStubServer.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "WebThreeServer.h"
#include <QMessageBox>
#include <QAbstractButton>
#include <libwebthree/WebThree.h>
#include "AlethFace.h"
using namespace std;
using namespace dev;
using namespace eth;
using namespace aleth;

WebThreeServer::WebThreeServer(
	jsonrpc::AbstractServerConnector& _conn,
	AlethFace* _aleth
):
	WebThreeStubServer(_conn, *_aleth->web3(), make_shared<AccountHolder>(_aleth), vector<KeyPair>{}, _aleth->keyManager(), *static_cast<TrivialGasPricer*>(_aleth->ethereum()->gasPricer().get()))
{
}

string WebThreeServer::shh_newIdentity()
{
	KeyPair kp = dev::KeyPair::create();
	emit onNewId(QString::fromStdString(toJS(kp.sec())));
	return toJS(kp.pub());
}

std::shared_ptr<dev::aleth::AccountHolder> WebThreeServer::ethAccounts() const
{
	return static_pointer_cast<dev::aleth::AccountHolder>(WebThreeStubServerBase::ethAccounts());
}
