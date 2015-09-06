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
/** @file AlethOne.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "AlethOne.h"
#include <QTimer>
#include <QToolButton>
#include <QButtonGroup>
#include <libethcore/Common.h>
#include <libethcore/ICAP.h>
#include <libethereum/Client.h>
#include <libwebthree/WebThree.h>
#include "ui_AlethOne.h"
using namespace std;
using namespace dev;
using namespace p2p;
using namespace eth;
using namespace aleth;
using namespace one;

std::string inUnits(bigint const& _b, strings const& _units)
{
	ostringstream ret;
	u256 b;
	if (_b < 0)
	{
		ret << "-";
		b = (u256)-_b;
	}
	else
		b = (u256)_b;
	u256 biggest = (u256)exp10(_units.size() * 3);
	if (b > biggest * 1000)
	{
		ret << (b / biggest) << " " << _units.back();
		return ret.str();
	}
	ret << setprecision(5);

	u256 unit = biggest / 1000;
	for (auto it = _units.rbegin(); it != _units.rend(); ++it)
	{
		auto i = *it;
		if (i != _units.front() && b >= unit)
		{
			ret << (double(b / (unit / 1000)) / 1000.0) << " " << i;
			return ret.str();
		}
		else
			unit /= 1000;
	}
	ret << b << " " << _units.front();
	return ret.str();
}

AlethOne::AlethOne():
	m_ui(new Ui::AlethOne)
{
	setWindowFlags(Qt::Window);
	m_ui->setupUi(this);
	m_aleth.init();

	auto g = new QButtonGroup(this);
	g->setExclusive(true);
	auto nom = findChild<QToolButton*>("noMining");
	g->addButton(nom);
	connect(nom, &QToolButton::clicked, [=](){ m_aleth.ethereum()->stopMining(); });
	auto translate = [](string s) { return s == "cpu" ? "CPU" : s == "opencl" ? "OpenCL" : s + " Miner"; };
	for (auto i: m_aleth.ethereum()->sealers())
	{
		QToolButton* a = new QToolButton(this);
		a->setText(QString::fromStdString(translate(i)));
		a->setCheckable(true);
		connect(a, &QToolButton::clicked, [=](){ m_aleth.ethereum()->setSealer(i); m_aleth.ethereum()->startMining(); });
		g->addButton(a);
		m_ui->mine->addWidget(a);
	}

	m_aleth.web3()->setClientVersion(WebThreeDirect::composeClientVersion("AlethOne", "anon"));
	m_aleth.web3()->startNetwork();
	m_ui->sync->setEthereum(m_aleth.ethereum());
	m_ui->beneficiary->setText(QString::fromStdString(ICAP(m_aleth.beneficiary()).encoded()));

	for (auto const& i: Host::pocHosts())
		m_aleth.web3()->requirePeer(i.first, i.second);

	m_ui->hashrate->setText("      ");
	m_ui->hashrate->setMinimumWidth(m_ui->hashrate->width());

	{
		QTimer* t = new QTimer(this);
		connect(t, &QTimer::timeout, [=](){
			m_ui->peers->setValue(m_aleth.web3()->peerCount());
			m_ui->stack->setCurrentIndex(m_aleth.ethereum()->isSyncing() ? 0 : 1);
			QStringList ss = QString::fromStdString(inUnits(m_aleth.ethereum()->hashrate(), { "hash/s", "Khash/s", "Mhash/s", "Ghash/s" })).split(" ");
			m_ui->hashrate->setText(ss[0]);
			m_ui->underHashrate->setText(ss[1]);
		});
		t->start(1000);
	}
}

AlethOne::~AlethOne()
{
	delete m_ui;
}
