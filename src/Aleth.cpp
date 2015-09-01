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
/** @file Aleth.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#include "Aleth.h"
#include <QTimer>
#include <libdevcore/Log.h>
#include <libethereum/Client.h>
using namespace std;
using namespace dev;
using namespace eth;
using namespace aleth;

Aleth::Aleth(QWidget* _parent):
	AlethFace(_parent)
{
}

Aleth::~Aleth()
{
}

void Aleth::init()
{
	{
		QTimer* t = new QTimer(this);
		connect(t, SIGNAL(timeout()), SLOT(checkHandlers()));
		t->start(200);
	}
}

unsigned Aleth::installWatch(LogFilter const& _tf, WatchHandler const& _f)
{
	auto ret = ethereum()->installWatch(_tf, Reaping::Manual);
	m_handlers[ret] = _f;
	_f(LocalisedLogEntries());
	return ret;
}

unsigned Aleth::installWatch(h256 const& _tf, WatchHandler const& _f)
{
	auto ret = ethereum()->installWatch(_tf, Reaping::Manual);
	m_handlers[ret] = _f;
	_f(LocalisedLogEntries());
	return ret;
}

void Aleth::uninstallWatch(unsigned _w)
{
	cdebug << "!!! Main: uninstalling watch" << _w;
	ethereum()->uninstallWatch(_w);
	m_handlers.erase(_w);
}

void Aleth::checkHandlers()
{
	for (auto const& i: m_handlers)
	{
		auto ls = ethereum()->checkWatchSafe(i.first);
		if (ls.size())
		{
//				cnote << "FIRING WATCH" << i.first << ls.size();
			i.second(ls);
		}
	}
}
