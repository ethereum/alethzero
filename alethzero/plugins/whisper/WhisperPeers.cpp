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
/** @file WhisperPeers.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#include "WhisperPeers.h"
#include <QSettings>
#include <QScrollBar>
#include <libethereum/Client.h>
#include <libwhisper/WhisperHost.h>
#include <libwebthree/WebThree.h>
#include <libweb3jsonrpc/WebThreeStubServerBase.h>
#include <libaleth/WebThreeServer.h>
#include <libaleth/AlethFace.h>
#include "ZeroFace.h"
#include "ui_WhisperPeers.h"

using namespace std;
using namespace dev;
using namespace eth;
using namespace aleth;
using namespace zero;

ZERO_NOTE_PLUGIN(WhisperPeers);

static QString const c_filterDecrypted("-= all I can decrypt =-");
static QString const c_filterToMe("-= messages addressed to me =-");
static QString const c_filterAll("-= show all messages =-");

enum { SpecificTopic = 1, KnownTopics = 2, AddressedToMe = 4, AllMessages = 8 };

QString messageToString(shh::Envelope const& _e, shh::Message const& _m)
{
	time_t birth = _e.expiry() - _e.ttl();
	QString t(ctime(&birth));
	t.chop(6);
	t = t.right(t.size() - 4);

	QString seal = QString("<%1 -> %2>").arg(_m.from() ? _m.from().abridged().c_str() : "?").arg(_m.to() ? _m.to().abridged().c_str() : "X");
	QString item = QString("[%1, ttl: %2] *%3 %4 %5 ").arg(t).arg(_e.ttl()).arg(_e.workProved()).arg(toString(_e.topic()).c_str()).arg(seal);

	bytes raw = _m.payload();
	if (raw.size())
	{
		QString plaintext = QString::fromUtf8((const char*)(raw.data()), raw.size());
		item += plaintext;
	}

	return item;
}

WhisperPeers::WhisperPeers(ZeroFace* _m):
	Plugin(_m, "WhisperPeers"),
	m_ui(new Ui::WhisperPeers)
{
	dock(Qt::RightDockWidgetArea, "Active Whispers")->setWidget(new QWidget);
	m_ui->setupUi(dock()->widget());
	setDefaultTopics();
	connect(m_ui->topics, SIGNAL(currentIndexChanged(QString)), this, SLOT(on_filter_changed()));
	m_ui->whispers->setAlternatingRowColors(true);
	m_ui->whispers->setStyleSheet("alternate-background-color: aquamarine;");
	m_ui->whispers->setWordWrap(true);
	startTimer(1000);
}

void WhisperPeers::setDefaultTopics()
{
	m_ui->topics->addItem(c_filterAll);
	m_ui->topics->addItem(c_filterToMe);
	m_ui->topics->addItem(c_filterDecrypted);
}

void WhisperPeers::noteTopic(QString const _topic)
{
	m_ui->topics->addItem(_topic);
	m_knownTopics.insert(_topic);
}

void WhisperPeers::forgetTopics()
{
	m_knownTopics.clear();
	m_ui->topics->clear();
	setDefaultTopics();
}

void WhisperPeers::timerEvent(QTimerEvent*)
{
	refreshWhispers();
}

void WhisperPeers::on_filter_changed()
{
	refreshWhispers();
}

unsigned WhisperPeers::getTarget(QString const& _topic)
{
	if (!_topic.compare(c_filterAll))
		return KnownTopics | AddressedToMe | AllMessages;
	else if (!_topic.compare(c_filterDecrypted))
		return KnownTopics | AddressedToMe;
	else if (!_topic.compare(c_filterToMe))
		return AddressedToMe;
	else
		return SpecificTopic;
}

void WhisperPeers::refreshWhispers()
{
	multimap<time_t, QString> chat;
	QString const topic = m_ui->topics->currentText();
	unsigned const target = getTarget(topic);

	for (auto const& w: web3()->whisper()->all())
	{
		shh::Envelope const& e = w.second;
		shh::Message m;

		if (target & SpecificTopic)
			if (!(m = e.open(shh::BuildTopic(topic.toStdString()))))
				continue;

		if (!m && (target & AddressedToMe))
			for (pair<Public, Secret> const& i: zero()->web3Server()->ids())
				if (!!(m = e.open(shh::Topics(), i.second)))
					break;

		if (!m && (target & KnownTopics))
			for (auto k: m_knownTopics)
				if (!!(m = e.open(shh::BuildTopic(k.toStdString()))))
					break;

		if (!m && (target & AllMessages))
			m = e.open(shh::BuildTopic(string()));

		if (m || (target & AllMessages))
			chat.emplace(e.expiry() - e.ttl(), messageToString(e, m));
	}

	redraw(chat);
}

void WhisperPeers::redraw(multimap<time_t, QString> const& _messages)
{
	//if (m_ui->whispers->count() == _messages.size()) return; // was needed for debug purposes

	QScrollBar* scroll = m_ui->whispers->verticalScrollBar();
	bool wasAtBottom = (scroll->maximum() == scroll->sliderPosition());

	m_ui->whispers->clear();

	QListWidgetItem *item = nullptr;

	for (auto const& i: _messages)
	{
		item = new QListWidgetItem;
		item->setText(i.second);
		item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
		m_ui->whispers->addItem(item);
	}

	if (wasAtBottom)
		m_ui->whispers->scrollToBottom();
}
