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
/** @file AlethFive.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#include "AlethFive.h"
#include <QWebEngineView>
#include <QSettings>
#include <QTimer>
#include <QTime>
#include <QMenu>
#include <QClipboard>
#include <QToolButton>
#include <QButtonGroup>
#include <QDateTime>
#include <libethcore/Common.h>
#include <libethcore/ICAP.h>
#include <libethereum/Client.h>
#include <libwebthree/WebThree.h>
#include <libaleth/WebThreeServer.h>
#include "BuildInfo.h"
#include "ui_AlethFive.h"
using namespace std;
using namespace dev;
using namespace p2p;
using namespace eth;
using namespace aleth;
using namespace five;

AlethFive::AlethFive():
	m_ui(new Ui::AlethFive)
{
	g_logVerbosity = 1;

	setWindowFlags(Qt::Window);
	m_ui->setupUi(this);
	m_aleth.init(Aleth::Bootstrap, "AlethFive", "anon");
	m_rpcHost.init(&m_aleth);
	m_dappHost.reset(new DappHost(8081));
	m_dappLoader = new DappLoader(this, m_aleth.web3(), AlethFace::getNameReg());
	m_dappLoader->setSessionKey(m_rpcHost.web3Server()->newSession(SessionPermissions{{Privilege::Admin}}));
	connect(m_dappLoader, &DappLoader::dappReady, this, &AlethFive::dappLoaded);
	connect(m_dappLoader, &DappLoader::pageReady, this, &AlethFive::pageLoaded);
	connect(m_ui->webView, &QWebEngineView::urlChanged, [=](QUrl const& _url)
	{
		if (!m_dappHost->servesUrl(_url))
			m_ui->url->setText(_url.toString());
	});

	m_backMenu = new QMenu(this);
	m_ui->back->setMenu(m_backMenu);

	QMenu* popupMenu = new QMenu(this);
	popupMenu->addAction("Exit", qApp, SLOT(quit()));
	m_ui->menu->setMenu(popupMenu);

	readSettings();
	refresh();
}

AlethFive::~AlethFive()
{
	writeSettings();
	delete m_ui;
}

void AlethFive::refresh()
{
}

void AlethFive::readSettings()
{
	QSettings s("ethereum", "alethfive");
	m_ui->url->setText(s.value("url", "eth://wallet").toString());
}

void AlethFive::writeSettings()
{
	QSettings s("ethereum", "alethfive");
	s.setValue("url", m_ui->url->text());
}

void AlethFive::rebuildBack()
{
	m_backMenu->clear();
	for (int i = m_history.size() - 2; i >= 0; --i)
		connect(m_backMenu->addAction(m_history[i].first), &QAction::triggered, [=]()
		{
			navigateTo(m_history[i].second);
			m_history = m_history.mid(0, i);
			rebuildBack();
		});
}

void AlethFive::on_url_returnPressed()
{
	navigateTo(m_ui->url->text());
	m_history += QPair<QString, QString>(m_ui->url->text(), m_ui->url->text());
	rebuildBack();
}

void AlethFive::navigateTo(QString _url)
{
	QUrl url(_url);
	if (url.scheme().isEmpty() || url.scheme() == "eth" || url.path().endsWith(".dapp"))
	{
		try
		{
			//try do resolve dapp url
			m_dappLoader->loadDapp(_url);
			return;
		}
		catch (...)
		{
			qWarning() << boost::current_exception_diagnostic_information().c_str();
		}
	}

	if (url.scheme().isEmpty())
		if (url.path().indexOf('/') < url.path().indexOf('.'))
			url.setScheme("file");
		else
			url.setScheme("http");
	else {}
	m_dappLoader->loadPage(url.toString());
}

void AlethFive::dappLoaded(Dapp& _dapp)
{
	m_ui->webView->page()->setUrl(m_dappHost->hostDapp(std::move(_dapp)));
}

void AlethFive::pageLoaded(QByteArray _content, QString _mimeType, QUrl _uri)
{
	m_ui->webView->page()->setContent(_content, _mimeType, _uri);
}
