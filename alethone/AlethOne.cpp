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
#include <QSettings>
#include <QTimer>
#include <QTime>
#include <QClipboard>
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

AlethOne::AlethOne():
	m_ui(new Ui::AlethOne)
{
	g_logVerbosity = 1;

	setWindowFlags(Qt::Window);
	m_ui->setupUi(this);
	m_aleth.init(Aleth::Nothing, "AlethOne", "anon");

	m_ui->version->setText((c_network == eth::Network::Olympic ? "Olympic" : "Frontier") + QString(" AlethOne v") + QString::fromStdString(niceVersion(dev::Version)));
	m_ui->beneficiary->setPlaceholderText(QString::fromStdString(ICAP(m_aleth.keyManager().accounts().front()).encoded()));
	m_ui->sync->setAleth(&m_aleth);

	{
		QTimer* t = new QTimer(this);
		connect(t, SIGNAL(timeout()), SLOT(refresh()));
		t->start(1000);
	}

	readSettings();

	refresh();
}

AlethOne::~AlethOne()
{
	writeSettings();
	delete m_ui;
}

void AlethOne::refresh()
{
	m_ui->peers->setValue(m_aleth ? m_aleth.web3()->peerCount() : 0);
	bool s = m_aleth ? m_aleth.ethereum()->isSyncing() : false;
	pair<uint64_t, unsigned> gp = EthashAux::fullGeneratingProgress();
	m_ui->stack->setCurrentIndex(1);
	if (m_ui->mining->isChecked())
	{
		m_ui->mining->setText(tr("Stop Mining "));
		bool m;
		u256 r;
		if (m_aleth)
		{
			m = m_aleth.ethereum()->isMining();
			r = m_aleth.ethereum()->hashrate();
		}
		else
		{
			m = m_slave.isWorking();
			r = m ? m_slave.hashrate() : 0;
		}
		if (r > 0)
		{
			QStringList ss = QString::fromStdString(inUnits(r, { "hash/s", "Khash/s", "Mhash/s", "Ghash/s" })).split(" ");
			m_ui->hashrate->setText(ss[0]);
			m_ui->underHashrate->setText(ss[1]);
		}
		else if (gp.first != EthashAux::NotGenerating)
		{
			m_ui->hashrate->setText(QString("%1%").arg(gp.second));
			m_ui->underHashrate->setText(QString(tr("Preparing")).arg(gp.first));
		}
		else if (s)
			m_ui->stack->setCurrentIndex(0);
		else if (m)
		{
			m_ui->hashrate->setText("...");
			m_ui->underHashrate->setText(tr("Preparing"));
		}
		else
		{
			m_ui->hashrate->setText("...");
			m_ui->underHashrate->setText(tr("Waiting"));
		}
	}
	else
	{
		m_ui->mining->setText(tr("Start Mining"));
		if (s)
			m_ui->stack->setCurrentIndex(0);
		else
		{
			m_ui->hashrate->setText("/");
			m_ui->underHashrate->setText("Not mining");
		}
	}
}

void AlethOne::readSettings()
{
	QSettings s("ethereum", "alethone");
	if (s.value("mode", "solo").toString() == "solo")
		m_ui->local->setChecked(true);
	else
		m_ui->pool->setChecked(true);
	m_ui->url->setText(s.value("url", "http://127.0.0.1:8545").toString());
	m_ui->beneficiary->setText(s.value("beneficiary", "").toString());
}

void AlethOne::writeSettings()
{
	QSettings s("ethereum", "alethone");
	s.setValue("mode", m_ui->local->isChecked() ? "solo" : "pool");
	s.setValue("url", m_ui->url->text());
	s.setValue("beneficiary", m_ui->beneficiary->text());
}

void AlethOne::on_copy_clicked()
{
	static QTime t = QTime::currentTime();
	if (m_aleth)
	{
		if (t.elapsed() > 500)
			qApp->clipboard()->setText(QString::fromStdString(ICAP(m_aleth.beneficiary()).encoded()));
		else
			qApp->clipboard()->setText(QString::fromStdString(m_aleth.beneficiary().hex()));
		t.restart();
	}
}

void AlethOne::on_beneficiary_textEdited()
{
	QString addrText = m_ui->beneficiary->text().isEmpty() ? m_ui->beneficiary->placeholderText() : m_ui->beneficiary->text();
	pair<Address, bytes> a = m_aleth.readAddress(addrText.toStdString());
	if (!a.second.empty() || !a.first)
		m_ui->beneficiary->setStyleSheet("background: #fcc");
	else
	{
		m_aleth.setBeneficiary(a.first);
		m_ui->beneficiary->setStyleSheet("");
	}
}

void AlethOne::on_mining_toggled(bool _on)
{
	if (_on)
	{
#ifdef NDEBUG
		char const* sealer = "gpu";
#else
		char const* sealer = "cpu";
#endif
		if (m_ui->local->isChecked())
		{
			m_aleth.ethereum()->setSealer(sealer);
			m_aleth.ethereum()->startMining();
		}
		else if (m_ui->pool->isChecked())
		{
			m_slave.setURL(m_ui->url->text());
			m_slave.setSealer(sealer);
			m_slave.start();
		}
	}
	else
	{
		if (m_aleth)
			m_aleth.ethereum()->stopMining();
		m_slave.stop();
	}
	m_ui->local->setEnabled(!_on);
	m_ui->pool->setEnabled(!_on);
	m_ui->url->setEnabled(!_on);
	m_ui->beneficiary->setEnabled(!_on);
	refresh();
}

void AlethOne::on_send_clicked()
{
	SendDialog sd(this, &m_aleth);
	sd.exec();
}

void AlethOne::on_local_toggled(bool _on)
{
	if (_on)
	{
		if (!m_aleth.open(Aleth::Bootstrap))
			m_ui->pool->setChecked(true);
		else
			m_aleth.installWatch(ChainChangedFilter, [=](LocalisedLogEntries const&){
				log(QString(tr("New block #%1")).arg(this->m_aleth.ethereum()->number()));
				u256 balance = 0;
				auto accounts = this->m_aleth.keyManager().accounts();
				for (auto const& a: accounts)
					balance += this->m_aleth.ethereum()->balanceAt(a);
				this->m_ui->balance->setText(QString(tr("%1 across %2 accounts")).arg(QString::fromStdString(formatBalance(balance))).arg(accounts.size()));
			});
	}
	else
		m_aleth.close();

	m_ui->stackedWidget->setCurrentIndex(_on ? 0 : 1);
}

void AlethOne::log(QString _s)
{
	QString s;
	s = QTime::currentTime().toString("hh:mm:ss") + " " + _s;
	m_ui->log->appendPlainText(s);
}
