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
/** @file Browser.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#include "Browser.h"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <QFileDialog>
#include <QtWebEngine/QtWebEngine>
#include <QtWebEngineWidgets/QWebEngineView>
#include <QtWebEngineWidgets/QWebEngineCallback>
#include <QtWebEngineWidgets/QWebEngineSettings>
#include <libaleth/WebThreeServer.h>
#include <libaleth/AlethFace.h>
#include <libaleth/DappHost.h>
#include <libaleth/DappLoader.h>
#include "ZeroFace.h"
#include "ui_Browser.h"
using namespace std;
using namespace dev;
using namespace eth;
using namespace aleth;
using namespace zero;

ZERO_NOTE_PLUGIN(Browser);

Browser::Browser(ZeroFace* _m):
	Plugin(_m, "Web view"),
	m_ui(new Ui::Browser)
{
	QtWebEngine::initialize();
	dock(Qt::TopDockWidgetArea, "Browser")->setWidget(new QWidget());
	m_ui->setupUi(dock()->widget());
	m_ui->jsConsole->setWebView(m_ui->webView);
	std::string adminSessionKey = zero()->web3Server()->newSession(SessionPermissions{{Privilege::Admin}});
	m_dappHost.reset(new DappHost(8081));
	m_dappLoader = new DappLoader(this, web3(), AlethFace::getNameReg());
	m_dappLoader->setSessionKey(adminSessionKey);
	connect(m_dappLoader, &DappLoader::dappReady, this, &Browser::dappLoaded);
	connect(m_dappLoader, &DappLoader::pageReady, this, &Browser::pageLoaded);

	connect(addMenuItem("Load Javascript...", "menuAccounts", true), &QAction::triggered, this, &Browser::loadJs);

	connect(m_ui->webView, &QWebEngineView::titleChanged, [=]()
	{
//		m_ui->tabWidget->setTabText(0, m_ui->webView->title());
	});
	connect(m_ui->webView, &QWebEngineView::urlChanged, [=](QUrl const& _url)
	{
		if (!m_dappHost->servesUrl(_url))
			m_ui->urlEdit->setText(_url.toString());
	});
	connect(m_ui->urlEdit, &QLineEdit::returnPressed, this, &Browser::reloadUrl);
	m_ui->jsConsole->setVisible(false);
	connect(m_ui->consoleToggle, &QToolButton::clicked, [this]()
	{
		m_ui->jsConsole->setVisible(!m_ui->jsConsole->isVisible());
		m_ui->consoleToggle->setChecked(m_ui->jsConsole->isVisible());
	});
}

Browser::~Browser()
{
}

void Browser::readSettings(QSettings const& _s)
{
	m_ui->urlEdit->setText(_s.value("url", "about:blank").toString());
	m_ui->consoleToggle->setChecked(m_ui->jsConsole->isVisible());
	reloadUrl();
}

void Browser::writeSettings(QSettings& _s)
{
	_s.setValue("url", m_ui->urlEdit->text());
}

void Browser::reloadUrl()
{
	QString s = m_ui->urlEdit->text();
	QUrl url(s);
	if (url.scheme().isEmpty() || url.scheme() == "eth" || url.path().endsWith(".dapp"))
	{
		try
		{
			//try do resolve dapp url
			m_dappLoader->loadDapp(s);
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

void Browser::loadJs()
{
	QString f = QFileDialog::getOpenFileName(zero(), "Load Javascript", QString(), "Javascript (*.js);;All files (*)");
	if (f.size())
	{
		QString contents = QString::fromStdString(dev::asString(dev::contents(f.toStdString())));
		m_ui->webView->page()->runJavaScript(contents);
	}
}

void Browser::dappLoaded(Dapp& _dapp)
{
	QUrl url = m_dappHost->hostDapp(std::move(_dapp));
	m_ui->webView->page()->setUrl(url);
}

void Browser::pageLoaded(QByteArray const& _content, QString const& _mimeType, QUrl const& _uri)
{
	m_ui->webView->page()->setContent(_content, _mimeType, _uri);
}
