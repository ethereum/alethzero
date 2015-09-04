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
/** @file AccountHolder.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#include "AccountHolder.h"
#include <QMessageBox>
#include <QAbstractButton>
#include <libdevcore/Log.h>
#include <libethcore/KeyManager.h>
#include <libwebthree/WebThree.h>
#include "QNatspec.h"
#include "AlethFace.h"
#include "ZeroFace.h"
using namespace std;
using namespace dev;
using namespace eth;
using namespace aleth;
using namespace zero;

zero::AccountHolder::AccountHolder(ZeroFace* _zero):
	eth::AccountHolder([=](){ return _zero->aleth()->ethereum(); }),
	m_zero(_zero)
{
	startTimer(500);
}

bool zero::AccountHolder::showAuthenticationPopup(string const& _title, string const& _text)
{
	QMessageBox userInput;
	userInput.setText(QString::fromStdString(_title));
	userInput.setInformativeText(QString::fromStdString(_text));
	userInput.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	userInput.button(QMessageBox::Ok)->setText("Allow");
	userInput.button(QMessageBox::Cancel)->setText("Reject");
	userInput.setDefaultButton(QMessageBox::Cancel);
	return userInput.exec() == QMessageBox::Ok;
}

h256 zero::AccountHolder::authenticate(TransactionSkeleton const& _t)
{
	Guard l(x_queued);
	m_queued.push(_t);
	return h256();
}

void zero::AccountHolder::doValidations()
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
			if (Secret s = m_zero->aleth()->retrieveSecret(t.from))
				m_zero->aleth()->ethereum()->submitTransaction(t, s);
	}
}

AddressHash zero::AccountHolder::realAccounts() const
{
	return m_zero->aleth()->keyManager().accountsHash();
}

bool zero::AccountHolder::validateTransaction(TransactionSkeleton const& _t, bool _toProxy)
{
	if (!m_isEnabled)
		return true;

	return showAuthenticationPopup("Transaction", _t.userReadable(_toProxy,
		[&](TransactionSkeleton const& _t) -> pair<bool, string>
		{
			h256 contractCodeHash = m_zero->aleth()->ethereum()->postState().codeHash(_t.to);
			if (contractCodeHash == EmptySHA3)
				return make_pair(false, std::string());
			string userNotice = m_zero->aleth()->natSpec()->userNotice(contractCodeHash, _t.data);
			QNatspecExpressionEvaluator evaluator;
			userNotice = evaluator.evalExpression(userNotice);
			return std::make_pair(true, userNotice);
		},
		[&](Address const& _a) -> string { return m_zero->aleth()->toName(_a); }
	));
}


void zero::AccountHolder::timerEvent(QTimerEvent*)
{
	doValidations();
}
