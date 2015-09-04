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
/** @file ZeroFace.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "AlethFace.h"
#include <QMenu>
#include <libdevcore/Log.h>
#include <libethcore/ICAP.h>
#include <libethereum/Client.h>
using namespace std;
using namespace dev;
using namespace eth;
using namespace aleth;

void AccountNamer::noteKnownChanged()
{
	if (m_aleth)
		m_aleth->noteKnownAddressesChanged(this);
}

void AccountNamer::noteNamesChanged()
{
	if (m_aleth)
		m_aleth->noteAddressNamesChanged(this);
}

Plugin::Plugin(ZeroFace* _f, std::string const& _name):
	m_zero(_f),
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
		m_dock = new QDockWidget(_title, zero());
		m_dock->setObjectName(_title);
		zero()->addDockWidget(_area, m_dock);
		m_dock->setFeatures(QDockWidget::AllDockWidgetFeatures | QDockWidget::DockWidgetVerticalTitleBar);
	}
	return m_dock;
}

void Plugin::addToDock(Qt::DockWidgetArea _area, QDockWidget* _dockwidget, Qt::Orientation _orientation)
{
	zero()->addDockWidget(_area, _dockwidget, _orientation);
}

void Plugin::addAction(QAction* _a)
{
	zero()->addAction(_a);
}

QAction* Plugin::addMenuItem(QString _n, QString _menuName, bool _sep)
{
	QAction* a = new QAction(_n, aleth());
	QMenu* m = zero()->findChild<QMenu*>(_menuName);
	if (_sep)
		m->addSeparator();
	m->addAction(a);
	return a;
}

unordered_map<string, PluginFactory>* ZeroFace::s_linkedPlugins = nullptr;

void ZeroFace::notePlugin(string const& _name, PluginFactory const& _new)
{
	if (!s_linkedPlugins)
		s_linkedPlugins = new std::unordered_map<string, PluginFactory>();
	s_linkedPlugins->insert(make_pair(_name, _new));
}

void ZeroFace::unnotePlugin(string const& _name)
{
	if (s_linkedPlugins)
		s_linkedPlugins->erase(_name);
}

void ZeroFace::adoptPlugin(Plugin* _p)
{
	m_plugins[_p->name()] = shared_ptr<Plugin>(_p);
}

void ZeroFace::killPlugins()
{
	m_plugins.clear();
}

void ZeroFace::noteAllChange()
{
	for (auto const& p: m_plugins)
		p.second->onAllChange();
}

PluginRegistrarBase::PluginRegistrarBase(std::string const& _name, PluginFactory const& _f):
	m_name(_name)
{
	cdebug << "Noting linked plugin" << _name;
	ZeroFace::notePlugin(_name, _f);
}

PluginRegistrarBase::~PluginRegistrarBase()
{
	cdebug << "Noting unlinked plugin" << m_name;
	ZeroFace::unnotePlugin(m_name);
}

///////////////////////////////////////////
/// AlethFace
///////////////////////////////////////////

Address AlethFace::getNameReg()
{
	return Address("c6d9d2cd449a754c494264e1809c50e34d64562b");
}

Address AlethFace::getICAPReg()
{
	return Address("a1a111bc074c9cfa781f0c38e63bd51c91b8af00");
}

string AlethFace::toReadable(Address const& _a) const
{
	string p = toName(_a);
	string n;
/*	if (p.size() == 9 && p.find_first_not_of("QWERYUOPASDFGHJKLZXCVBNM1234567890") == string::npos)
		p = ICAP(p, "XREG").encoded();
	else*/
		DEV_IGNORE_EXCEPTIONS(n = ICAP(_a).encoded().substr(0, 8));
	if (!n.empty())
		n += " ";
	n += _a.abridged();
	return p.empty() ? n : (p + " " + n);
}

pair<Address, bytes> AlethFace::readAddress(std::string const& _n) const
{
	if (_n == "(Create Contract)")
		return make_pair(Address(), bytes());

	if (Address a = toAddress(_n))
		return make_pair(a, bytes());

	std::string n = _n;
	try {
		return ICAP::decoded(n).address([&](Address const& a, bytes const& b) -> bytes
		{
			return ethereum()->call(a, b).output;
		}, getICAPReg());
	}
	catch (...) {}

	if (n.find("0x") == 0)
		n.erase(0, 2);
	if (n.size() == 40)
		try
		{
			return make_pair(Address(fromHex(n, WhenError::Throw)), bytes());
		}
		catch (BadHexCharacter& _e)
		{
			cwarn << "invalid hex character, address rejected";
			cwarn << boost::diagnostic_information(_e);
			return make_pair(Address(), bytes());
		}
		catch (...)
		{
			cwarn << "address rejected";
			return make_pair(Address(), bytes());
		}
	return make_pair(Address(), bytes());
}

string AlethFace::toHTML(StateDiff const& _d) const
{
	stringstream s;

	auto indent = "<code style=\"white-space: pre\">     </code>";
	for (auto const& i: _d.accounts)
	{
		s << "<hr/>";

		AccountDiff ad = i.second;
		s << "<code style=\"white-space: pre; font-weight: bold\">" << lead(ad.changeType()) << "  </code>" << " <b>" << toReadable(i.first) << "</b>";
		if (!ad.exist.to())
			continue;

		if (ad.balance)
		{
			s << "<br/>" << indent << "Balance " << dec << ad.balance.to() << " [=" << formatBalance(ad.balance.to()) << "]";
			bigint d = (dev::bigint)ad.balance.to() - (dev::bigint)ad.balance.from();
			s << " <b>" << showpos << dec << d << " [=" << formatBalance(d) << "]" << noshowpos << "</b>";
		}
		if (ad.nonce)
		{
			s << "<br/>" << indent << "Count #" << dec << ad.nonce.to();
			s << " <b>" << showpos << (((dev::bigint)ad.nonce.to()) - ((dev::bigint)ad.nonce.from())) << noshowpos << "</b>";
		}
		if (ad.code)
		{
			s << "<br/>" << indent << "Code " << dec << ad.code.to().size() << " bytes";
			if (ad.code.from().size())
				 s << " (" << ad.code.from().size() << " bytes)";
		}

		for (pair<u256, dev::Diff<u256>> const& i: ad.storage)
		{
			s << "<br/><code style=\"white-space: pre\">";
			if (!i.second.from())
				s << " + ";
			else if (!i.second.to())
				s << "XXX";
			else
				s << " * ";
			s << "  </code>";

			s << toHTML(i.first);
/*			if (i.first > u256(1) << 246)
				s << (h256)i.first;
			else if (i.first > u160(1) << 150)
				s << (h160)(u160)i.first;
			else
				s << hex << i.first;
*/
			if (!i.second.from())
				s << ": " << toHTML(i.second.to());
			else if (!i.second.to())
				s << " (" << toHTML(i.second.from()) << ")";
			else
				s << ": " << toHTML(i.second.to()) << " (" << toHTML(i.second.from()) << ")";
		}
	}
	return s.str();
}

string AlethFace::dnsLookup(string const& _a) const
{
	// TODO: make it actually look up via the GlobalRegistrar.
	return _a;
}
