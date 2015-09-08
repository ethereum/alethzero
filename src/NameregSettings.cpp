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

/** @file NameregSettings.cpp
 * @author Arkadiy Paronyan <arkadiy@ethdev.com>
 * @date 2015
 */
#include "NameregSettings.h"
#include <QInputDialog>
#include "ui_NameregSettings.h"

using namespace dev;
using namespace az;
using namespace eth;
using namespace std;

NameregSettingsPage::NameregSettingsPage():
	m_ui(new Ui::NameregSettings())
{
	m_ui->setupUi(this);
}

QString NameregSettingsPage::address() const
{
	return m_ui->nameregAddress->text();
}


void NameregSettingsPage::setAddress(QString const& _address)
{
	m_ui->nameregAddress->setText(_address);
}
