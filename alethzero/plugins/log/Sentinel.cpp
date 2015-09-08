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
/** @file Sentinel.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#include "Sentinel.h"
#include <QInputDialog>
#include <QAction>
#include <libethereum/Client.h>
#include <libaleth/AlethFace.h>
#include "ZeroFace.h"
using namespace std;
using namespace dev;
using namespace eth;
using namespace aleth;
using namespace zero;

DEV_AZ_NOTE_PLUGIN(Sentinel);

Sentinel::Sentinel(ZeroFace* _z):
	Plugin(_z, "Sentinel")
{
	connect(addMenuItem("Set Sentinel...", "menuConfig", true), &QAction::triggered, [=]()
	{
		bool ok;
		QString sentinel = QInputDialog::getText(nullptr, "Enter sentinel address", "Enter the sentinel address for bad block reporting (e.g. http://badblockserver.com:8080). Enter nothing to disable.", QLineEdit::Normal, QString::fromStdString(ethereum()->sentinel()), &ok);
		if (ok)
			ethereum()->setSentinel(sentinel.toStdString());
	});
}
