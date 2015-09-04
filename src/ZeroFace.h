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
/** @file AlethFace.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <memory>
#include <map>
#include <unordered_map>
#include <string>
#include <functional>
#include <QtWidgets/QMainWindow>
#include "Common.h"

class QSettings;

namespace dev
{

class SafeHttpServer;

namespace aleth
{

class AlethFace;

namespace zero
{

class Plugin;
class ZeroFace;

using PluginFactory = std::function<Plugin*(ZeroFace*)>;

class OurWebThreeStubServer;

class ZeroFace: public QMainWindow
{
	Q_OBJECT

public:
	explicit ZeroFace(QWidget* _parent = nullptr): QMainWindow(_parent) {}
	virtual ~ZeroFace()
	{
		killPlugins();
	}

	// Plugin API
	static void notePlugin(std::string const& _name, PluginFactory const& _new);
	static void unnotePlugin(std::string const& _name);
	void adoptPlugin(Plugin* _p);
	void killPlugins();
	void noteAllChange();

	virtual AlethFace const* aleth() const = 0;
	virtual AlethFace* aleth() = 0;

	virtual OurWebThreeStubServer* web3Server() const = 0;
	virtual dev::SafeHttpServer* web3ServerConnector() const = 0;

protected:
	template <class F> void forEach(F const& _f) { for (auto const& p: m_plugins) _f(p.second); }
	std::shared_ptr<Plugin> takePlugin(std::string const& _name) { auto it = m_plugins.find(_name); std::shared_ptr<Plugin> ret; if (it != m_plugins.end()) { ret = it->second; m_plugins.erase(it); } return ret; }

	static std::unordered_map<std::string, PluginFactory>* s_linkedPlugins;

private:
	std::unordered_map<std::string, std::shared_ptr<Plugin>> m_plugins;
};

}
}
}
