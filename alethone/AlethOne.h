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

#pragma once

#include <QDialog>
#include <libaleth/Aleth.h>
#include <libaleth/Slave.h>

namespace Ui {
class AlethOne;
}

namespace dev
{
namespace aleth
{
namespace one
{

class AlethOne: public QDialog
{
	Q_OBJECT

public:
	AlethOne();
	~AlethOne();

private slots:
	void on_send_clicked();
	void on_local_toggled(bool _on);

private:
	void log(QString _s);

	void readSettings();
	void writeSettings();

	Ui::AlethOne* m_ui;
	Aleth m_aleth;
	Slave m_slave;
};

}
}
}
