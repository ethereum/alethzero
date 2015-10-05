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
/** @file AlethFive.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#pragma once

#include <QMainWindow>
#include <libaleth/Aleth.h>
#include <libaleth/DappHost.h>
#include <libaleth/DappLoader.h>
#include <libaleth/RPCHost.h>

namespace Ui {
class AlethFive;
}

namespace dev
{
namespace aleth
{
namespace five
{

class AlethFive: public QMainWindow
{
	Q_OBJECT

public:
	AlethFive();
	~AlethFive();

private slots:
	void dappLoaded(Dapp& _dapp);
	void pageLoaded(QByteArray _content, QString _mimeType, QUrl _uri);
	void navigateTo(QString _url);
	void enterDestination(QString _url);

	void on_url_returnPressed();

	void updateURLBox();

private:
	void refresh();
	void rebuildBack();

	void readSettings();
	void writeSettings();

	bool eventFilter(QObject* _o, QEvent* _e) override;

	Ui::AlethFive* m_ui;
	QMenu* m_backMenu;
	Aleth m_aleth;
	RPCHost m_rpcHost;

	QList<QPair<QString, QString>> m_history;

	std::unique_ptr<DappHost> m_dappHost;
	DappLoader* m_dappLoader = nullptr;
};

}
}
}
