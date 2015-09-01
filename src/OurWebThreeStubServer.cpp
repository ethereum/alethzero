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

#include "OurWebThreeStubServer.h"
#include <QMessageBox>
#include <QAbstractButton>
#include <libwebthree/WebThree.h>
#include "QNatspec.h"
#include "AlethZero.h"
using namespace std;
using namespace dev;
using namespace aleth;
using namespace eth;

OurWebThreeStubServer::OurWebThreeStubServer(
	jsonrpc::AbstractServerConnector& _conn,
	AlethZero* _main
):
	WebThreeStubServer(_conn, *_main->web3(), make_shared<OurAccountHolder>(_main), vector<KeyPair>{}, _main->keyManager(), *static_cast<TrivialGasPricer*>(_main->ethereum()->gasPricer().get())),
	m_main(_main)
{
}

string OurWebThreeStubServer::shh_newIdentity()
{
	KeyPair kp = dev::KeyPair::create();
	emit onNewId(QString::fromStdString(toJS(kp.sec())));
	return toJS(kp.pub());
}

OurAccountHolder::OurAccountHolder(AlethZero* _main):
	AccountHolder([=](){ return _main->ethereum(); }),
	m_main(_main)
{
	connect(_main, SIGNAL(poll()), this, SLOT(doValidations()));
}

bool OurAccountHolder::showAuthenticationPopup(string const& _title, string const& _text)
{
	if (!m_main->confirm())
	{
		cnote << "Skipping confirmation step for: " << _title << "\n" << _text;
		return true;
	}

	QMessageBox userInput;
	userInput.setText(QString::fromStdString(_title));
	userInput.setInformativeText(QString::fromStdString(_text));
	userInput.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	userInput.button(QMessageBox::Ok)->setText("Allow");
	userInput.button(QMessageBox::Cancel)->setText("Reject");
	userInput.setDefaultButton(QMessageBox::Cancel);
	return userInput.exec() == QMessageBox::Ok;
	//QMetaObject::invokeMethod(m_main, "authenticate", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, button), Q_ARG(QString, QString::fromStdString(_title)), Q_ARG(QString, QString::fromStdString(_text)));
	//return button == QMessageBox::Ok;
}

h256 OurAccountHolder::authenticate(TransactionSkeleton const& _t)
{
	Guard l(x_queued);
	m_queued.push(_t);
	return h256();
}

void OurAccountHolder::doValidations()
{
	Guard l(x_queued);
	while (!m_queued.empty())
	{
		auto t = m_queued.front();
		m_queued.pop();

		bool proxy = isProxyAccount(t.from);
		if (!proxy && !isRealAccount(t.from))
		{
			cwarn << "Trying to send from non-existant account" << t.from;
			return;
		}

		// TODO: determine gas price.

		if (!validateTransaction(t, proxy))
			return;

		if (proxy)
			queueTransaction(t);
		else
			// sign and submit.
			if (Secret s = m_main->retrieveSecret(t.from))
				m_main->ethereum()->submitTransaction(t, s);
	}
}

AddressHash OurAccountHolder::realAccounts() const
{
	return m_main->keyManager().accountsHash();
}

bool OurAccountHolder::validateTransaction(TransactionSkeleton const& _t, bool _toProxy)
{
	if (!m_main->doConfirm())
		return true;

	return showAuthenticationPopup("Transaction", _t.userReadable(_toProxy,
		[&](TransactionSkeleton const& _t) -> pair<bool, string>
		{
			h256 contractCodeHash = m_main->ethereum()->postState().codeHash(_t.to);
			if (contractCodeHash == EmptySHA3)
				return make_pair(false, std::string());
			string userNotice = m_main->natSpec()->userNotice(contractCodeHash, _t.data);
			QNatspecExpressionEvaluator evaluator;
			userNotice = evaluator.evalExpression(userNotice);
			return std::make_pair(true, userNotice);
		},
		[&](Address const& _a) -> string { return m_main->pretty(_a); }
	));
}
