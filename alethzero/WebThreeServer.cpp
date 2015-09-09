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
#include "QNatspec.h"
#include <libaleth/AlethFace.h>
#include "ZeroFace.h"
using namespace std;
using namespace dev;
using namespace eth;
using namespace aleth;
using namespace zero;

WebThreeServer::WebThreeServer(
	jsonrpc::AbstractServerConnector& _conn,
	ZeroFace* _zero
):
	WebThreeStubServer(_conn, *_zero->aleth()->web3(), make_shared<zero::AccountHolder>(_zero), vector<KeyPair>{}, _zero->aleth()->keyManager(), *static_cast<TrivialGasPricer*>(_zero->aleth()->ethereum()->gasPricer().get())),
	m_zero(_zero)
{
}

string WebThreeServer::shh_newIdentity()
{
	KeyPair kp = dev::KeyPair::create();
	emit onNewId(QString::fromStdString(toJS(kp.sec())));
	return toJS(kp.pub());
}

std::shared_ptr<dev::aleth::zero::AccountHolder> WebThreeServer::ethAccounts() const
{
	return static_pointer_cast<dev::aleth::zero::AccountHolder>(WebThreeStubServerBase::ethAccounts());
}
