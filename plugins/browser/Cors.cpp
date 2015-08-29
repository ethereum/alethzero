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
/** @file Cors.h
 * @author Marek Kotewicz <marek@ethdev.com>
 * @date 2015
 */

#include <libweb3jsonrpc/SafeHttpServer.h>
#include "Cors.h"
#include "ui_Cors.h"

using namespace std;
using namespace dev;
using namespace az;
using namespace eth;

DEV_AZ_NOTE_PLUGIN(Cors);

Cors::Cors(MainFace* _m):
	Plugin(_m, "Cors")
{
	connect(addMenuItem("Change cors domain", "menuTools", true), SIGNAL(triggered()), SLOT(create()));
}

Cors::~Cors()
{
}

void Cors::create()
{
	QDialog d;
	Ui::Cors u;
	u.setupUi(&d);

	u.domain->setText(QString::fromStdString(main()->web3ServerConnector()->allowedOrigin()));
	if (d.exec() == QDialog::Accepted)	
		main()->web3ServerConnector()->setAllowedOrigin(u.domain->text().toStdString());
}

