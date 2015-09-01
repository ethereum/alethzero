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
/** @file AlethFace.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "AlethFace.h"
#include <QMenu>
#include <libdevcore/Log.h>
using namespace std;
using namespace dev;
using namespace aleth;


void AccountNamer::noteKnownChanged()
{
	if (m_main)
		m_main->noteKnownAddressesChanged(this);
}

void AccountNamer::noteNamesChanged()
{
	if (m_main)
		m_main->noteAddressNamesChanged(this);
}

Plugin::Plugin(AlethFace* _f, std::string const& _name):
	m_main(_f),
	m_name(_name)
{
	_f->adoptPlugin(this);
}

QDockWidget* Plugin::dock(Qt::DockWidgetArea _area, QString _title)
{
	if (_title.isEmpty())
		_title = QString::fromStdString(m_name);
	if (!m_dock)
	{
		m_dock = new QDockWidget(_title, m_main);
		m_dock->setObjectName(_title);
		m_main->addDockWidget(_area, m_dock);
		m_dock->setFeatures(QDockWidget::AllDockWidgetFeatures | QDockWidget::DockWidgetVerticalTitleBar);
	}
	return m_dock;
}

void Plugin::addToDock(Qt::DockWidgetArea _area, QDockWidget* _dockwidget, Qt::Orientation _orientation)
{
	m_main->addDockWidget(_area, _dockwidget, _orientation);
}

void Plugin::addAction(QAction* _a)
{
	m_main->addAction(_a);
}

QAction* Plugin::addMenuItem(QString _n, QString _menuName, bool _sep)
{
	QAction* a = new QAction(_n, main());
	QMenu* m = main()->findChild<QMenu*>(_menuName);
	if (_sep)
		m->addSeparator();
	m->addAction(a);
	return a;
}

unordered_map<string, function<Plugin*(AlethFace*)>>* AlethFace::s_linkedPlugins = nullptr;

Address AlethFace::getNameReg()
{
	return Address("c6d9d2cd449a754c494264e1809c50e34d64562b");
//	return abiOut<Address>(ethereum()->call(c_newConfig, abiIn("lookup(uint256)", (u256)1)).output);
}
void AlethFace::notePlugin(string const& _name, PluginFactory const& _new)
{
	if (!s_linkedPlugins)
		s_linkedPlugins = new std::unordered_map<string, PluginFactory>();
	s_linkedPlugins->insert(make_pair(_name, _new));
}

void AlethFace::unnotePlugin(string const& _name)
{
	if (s_linkedPlugins)
		s_linkedPlugins->erase(_name);
}

void AlethFace::adoptPlugin(Plugin* _p)
{
	m_plugins[_p->name()] = shared_ptr<Plugin>(_p);
}

void AlethFace::killPlugins()
{
	m_plugins.clear();
}

void AlethFace::allChange()
{
	for (auto const& p: m_plugins)
		p.second->onAllChange();
}

PluginRegistrarBase::PluginRegistrarBase(std::string const& _name, PluginFactory const& _f):
	m_name(_name)
{
	cdebug << "Noting linked plugin" << _name;
	AlethFace::notePlugin(_name, _f);
}

PluginRegistrarBase::~PluginRegistrarBase()
{
	cdebug << "Noting unlinked plugin" << m_name;
	AlethFace::unnotePlugin(m_name);
}
