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
/** @file Mining.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#include "Mining.h"
#include <QMenuBar>
#include <libdevcore/Log.h>
#include <libethcore/EthashAux.h>
#include <libethereum/Client.h>
#include "AlethFace.h"
#include "ZeroFace.h"
using namespace std;
using namespace dev;
using namespace eth;
using namespace aleth;
using namespace zero;

DEV_AZ_NOTE_PLUGIN(Mining);

Mining::Mining(ZeroFace* _m):
	Plugin(_m, "Mining")
{
	QAction* mine = addMenuItem("Mine", "menuMining", false, "&Mining");
	mine->setCheckable(true);
	connect(mine, &QAction::triggered, [&](){
		if (mine->isChecked())
			aleth()->ethereum()->startMining();
		else
			aleth()->ethereum()->stopMining();
	});
	connect(addMenuItem("Prepare Next DAG", "menuMining"), &QAction::triggered, [&](){
		EthashAux::computeFull(
			EthashAux::seedHash(
				aleth()->ethereum()->blockChain().number() + ETHASH_EPOCH_LENGTH
			)
		);
	});
}

Mining::~Mining()
{
}
