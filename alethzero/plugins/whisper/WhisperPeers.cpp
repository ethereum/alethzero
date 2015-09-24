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

int const c_maxMessages = 512;

static QString const c_filterAll("-= all messages =-");

QString messageToString(shh::Envelope const& _e, shh::Message const& _m)
{
	time_t birth = _e.expiry() - _e.ttl();
	QString t(ctime(&birth));
	t.chop(6);
	t = t.right(t.size() - 4);

	QString seal = QString("{%1 => %2}").arg(_m.from() ? _m.from().abridged().c_str() : "?").arg(_m.to() ? _m.to().abridged().c_str() : "X");
	QString item = QString("[%1, ttl: %2] *%3 %4 %5").arg(t).arg(_e.ttl()).arg(_e.workProved()).arg(toString(_e.topic()).c_str()).arg(seal);

	bytes raw = _m.payload();
	if (raw.size())
	{
		QString plaintext = QString::fromUtf8((const char*)(raw.data()), raw.size());
		item += ": ";
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
}

void WhisperPeers::noteTopic(QString const _topic)
{
	m_ui->topics->addItem(_topic);

	if (m_topics.find(_topic) == m_topics.end())
	{
		shh::BuildTopic bt(_topic.toStdString());
		unsigned f = web3()->whisper()->installWatch(bt);
		m_topics[_topic] = f;
	}
}

void WhisperPeers::forgetTopics()
{
	m_topics.clear();
	m_ui->topics->clear();
	m_ui->whispers->clear();
	setDefaultTopics();
}

void WhisperPeers::timerEvent(QTimerEvent*)
{
	refreshWhispers(true);
}

void WhisperPeers::on_filter_changed()
{
	m_ui->whispers->clear();
	refreshWhispers(false);
}

void WhisperPeers::refreshWhispers(bool _timerEvent)
{
	QString const topic = m_ui->topics->currentText();
	if (!topic.compare(c_filterAll))
		refreshAll(_timerEvent);
	else
		refresh(topic, _timerEvent);
}

void WhisperPeers::addToView(std::multimap<time_t, QString> const& _messages)
{
	QScrollBar* scroll = m_ui->whispers->verticalScrollBar();
	bool wasAtBottom = (scroll->maximum() == scroll->sliderPosition());
	int newMessagesCount = static_cast<int>(_messages.size());
	int existingMessagesCount = m_ui->whispers->count();
	int target = c_maxMessages - newMessagesCount;
	if (target < 0)
		target = 0;

	while (m_ui->whispers->count() > target)
		m_ui->whispers->removeItemWidget(m_ui->whispers->item(0));

	for (auto const& i: _messages)
	{
		QListWidgetItem* item = new QListWidgetItem;
		item->setText(i.second);
		item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
		m_ui->whispers->addItem(item);
	}

	if (wasAtBottom)
		m_ui->whispers->scrollToBottom();
}

void WhisperPeers::refresh(QString const& _topic, bool _timerEvent)
{
	shh::BuildTopic bt(_topic.toStdString());
	unsigned const w = m_topics[_topic];
	multimap<time_t, QString> newMessages;
	multimap<time_t, QString>& chat = m_chats[_topic];

	if (m_topics.find(_topic) == m_topics.end())
	{
		unsigned f = web3()->whisper()->installWatch(bt);
		m_topics[_topic] = f;
		return;
	}

	for (auto i: web3()->whisper()->checkWatch(w))
	{
		shh::Envelope const& e = web3()->whisper()->envelope(i);
		shh::Message msg = e.open(web3()->whisper()->fullTopics(w));

		if (!msg)
			for (pair<Public, Secret> const& i: zero()->web3Server()->ids())
				if (!!(msg = e.open(bt, i.second)))
					break;

		if (msg)
		{
			time_t birth = e.expiry() - e.ttl();
			QString s = messageToString(e, msg);
			chat.emplace(birth, s);
			m_all.emplace(birth, s);
			if (_timerEvent)
				newMessages.emplace(birth, s);
		}
	}

	resizeMap(chat);
	resizeMap(m_all);
	resizeMap(newMessages);

	if (_timerEvent)
		addToView(newMessages);
	else
		addToView(chat);
}

void WhisperPeers::refreshAll(bool _timerEvent)
{
	multimap<time_t, QString> newMessages;

	for (auto t = m_topics.begin(); t != m_topics.end(); ++t)
	{
		multimap<time_t, QString>& chat = m_chats[t->first];

		for (auto i: web3()->whisper()->checkWatch(t->second))
		{
			shh::Envelope const& e = web3()->whisper()->envelope(i);
			shh::Message msg = e.open(web3()->whisper()->fullTopics(t->second));

			if (!msg)
				for (pair<Public, Secret> const& i: zero()->web3Server()->ids())
					if (!!(msg = e.open(shh::Topics(), i.second)))
						break;

			if (msg)
			{
				time_t birth = e.expiry() - e.ttl();
				QString s = messageToString(e, msg);
				chat.emplace(birth, s);
				m_all.emplace(birth, s);
				if (_timerEvent)
					newMessages.emplace(birth, s);
			}
		}

		resizeMap(chat);
	}

	resizeMap(m_all);
	resizeMap(newMessages);

	if (_timerEvent)
		addToView(newMessages);
	else
		addToView(m_all);
}

void WhisperPeers::resizeMap(std::multimap<time_t, QString>& _map)
{
	size_t const limit = static_cast<size_t>(c_maxMessages);
	while (_map.size() > limit)
		_map.erase(_map.begin());
}

