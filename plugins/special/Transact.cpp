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
/** @file Transact.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#include "Transact.h"
#include <libdevcore/Log.h>
#include <libethereum/Client.h>
#include <libethcore/KeyManager.h>
#include "TransactDialog.h"
using namespace std;
using namespace dev;
using namespace az;
using namespace eth;

DEV_AZ_NOTE_PLUGIN(Transact);

Transact::Transact(MainFace* _m):
	Plugin(_m, "Transact")
{
	m_dialog = new TransactDialog(_m);
	m_dialog->setWindowFlags(Qt::Dialog);
	m_dialog->setWindowModality(Qt::WindowModal);

	connect(addMenuItem("New Transaction...", "menuSpecial", true), SIGNAL(triggered()), SLOT(injectOne()));
}

Transact::~Transact()
{
	delete m_dialog;
}

void Transact::newTransaction()
{
	m_dialog->setEnvironment(main()->keyManager().accountsHash(), ethereum(), main()->natSpec());
	m_dialog->show();
}

