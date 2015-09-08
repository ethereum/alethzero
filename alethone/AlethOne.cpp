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
#include <QDateTime>
#include <libethcore/Common.h>
#include <libethcore/ICAP.h>
#include <libethereum/Client.h>
#include <libwebthree/WebThree.h>
#include <libaleth/SendDialog.h>
#include "BuildInfo.h"
#include "ui_AlethOne.h"
using namespace std;
using namespace dev;
using namespace p2p;
using namespace eth;
using namespace aleth;
using namespace one;

string niceVersion(string const& _v)
{
	if (_v.substr(1, 4) == ".9.9" && _v.size() >= 6)
		return toString(stoi(_v.substr(0, 1)) + 1) + ".0rc" + _v.substr(5);
	return _v;
}

AlethOne::AlethOne():
	m_ui(new Ui::AlethOne)
{
	setWindowFlags(Qt::Window);
	m_ui->setupUi(this);
	m_aleth.init(Aleth::Nothing, "AlethOne", "anon");

	auto g = new QButtonGroup(this);
	g->setExclusive(true);
	g->addButton(m_ui->noMining);
	connect(m_ui->noMining, &QToolButton::clicked, [=]()
	{
		if (m_aleth)
			m_aleth.ethereum()->stopMining();
		m_slave.stop();
	});

	connect(m_ui->local, &QRadioButton::toggled, [=](bool on){
		if (on)
		{
			if (!m_aleth.open(Aleth::Bootstrap))
				m_ui->pool->setChecked(true);
		}
		else
			m_aleth.close();
	});

	auto translate = [](QString s) { return s == "cpu" ? "CPU" : s == "opencl" ? "OpenCL" : s + " Miner"; };
	for (QString i: m_slave.sealers())
	{
		QToolButton* a = new QToolButton(this);
		a->setText(translate(i));
		a->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
		a->setCheckable(true);
		connect(a, &QToolButton::clicked, [=]()
		{
			if (m_ui->local->isChecked())
			{
				m_aleth.ethereum()->setSealer(i.toStdString());
				m_aleth.ethereum()->startMining();
			}
			else if (m_ui->pool->isChecked())
			{
				m_slave.setURL(m_ui->url->text());
				m_slave.setSealer(i);
				m_slave.start();
			}
		});
		g->addButton(a);
		m_ui->mine->addWidget(a);
	}

	m_ui->version->setText((c_network == eth::Network::Olympic ? "Olympic" : "Frontier") + QString(" AlethOne v") + QString::fromStdString(niceVersion(dev::Version)) + "-" DEV_QUOTED(ETH_BUILD_TYPE) "-" DEV_QUOTED(ETH_BUILD_PLATFORM) "-" + QString(DEV_QUOTED(ETH_COMMIT_HASH)).mid(0, 6) + (ETH_CLEAN_REPO ? "" : "+"));
	m_ui->beneficiary->setText(QString::fromStdString(ICAP(m_aleth.keyManager().accounts().front()).encoded()));
	m_ui->sync->setAleth(&m_aleth);

	{
		QTimer* t = new QTimer(this);
		connect(t, &QTimer::timeout, [=](){
			if (m_ui->noMining->isChecked())
			{
				m_ui->hashrate->setText("/");
				m_ui->underHashrate->setText("Not mining");
				m_ui->overHashrate->setText("Off");
			}
			else
			{
				bool s;
				bool m;
				u256 r;
				if (m_aleth)
				{
					m_ui->peers->setValue(m_aleth.web3()->peerCount());
					s = m_aleth.ethereum()->isSyncing();
					m = m_aleth.ethereum()->isMining();
					r = m_aleth.ethereum()->hashrate();
				}
				else
				{
					m_ui->peers->setValue(0);
					s = false;
					m = m_slave.isWorking();
					r = m ? m_slave.hashrate() : 0;
				}
				m_ui->stack->setCurrentIndex(s ? 0 : 1);
				if (!s)
				{
					m_ui->overHashrate->setText(translate(m_aleth ? QString::fromStdString(m_aleth.ethereum()->sealer()) : m_slave.sealer()));
					if (r > 0)
					{
						QStringList ss = QString::fromStdString(inUnits(r, { "hash/s", "Khash/s", "Mhash/s", "Ghash/s" })).split(" ");
						m_ui->hashrate->setText(ss[0]);
						m_ui->underHashrate->setText(ss[1]);
					}
					else if (m)
					{
						m_ui->hashrate->setText("...");
						m_ui->underHashrate->setText("Preparing...");
					}
					else
					{
						m_ui->hashrate->setText("...");
						m_ui->underHashrate->setText("Waiting...");
					}
				}
			}
		});
		t->start(1000);
	}

	m_aleth.installWatch(ChainChangedFilter, [&](LocalisedLogEntries const&){
		log(QString("New block #%1").arg(m_aleth.ethereum()->number()));
		u256 balance = 0;
		auto accounts = m_aleth.keyManager().accounts();
		for (auto const& a: accounts)
			balance += m_aleth.ethereum()->balanceAt(a);
		m_ui->balance->setText(QString("%1 across %2 accounts").arg(QString::fromStdString(formatBalance(balance))).arg(accounts.size()));
	});

	m_ui->stack->adjustSize();
	m_ui->stack->setMinimumWidth(m_ui->stack->height() * 1.25);
	m_ui->stack->setMaximumWidth(m_ui->stack->height() * 1.25);
}

AlethOne::~AlethOne()
{
	delete m_ui;
}

void AlethOne::on_send_clicked()
{
	SendDialog sd(this, &m_aleth);
	sd.exec();
}

void AlethOne::log(QString _s)
{
	QString s;
	s = QDateTime::currentDateTime().toString() + ": " + _s;
	m_ui->log->appendPlainText(s);
}
