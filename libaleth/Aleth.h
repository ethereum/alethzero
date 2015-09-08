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
/** @file Aleth.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#pragma once

#include <unordered_set>
#include <libethcore/KeyManager.h>
#include <libwebthree/WebThree.h>
#include "NatspecHandler.h"
#include "AlethFace.h"

namespace dev
{

namespace aleth
{

enum
{
	DefaultPasswordFlags = 0,
	NeedConfirm = 1,
	IsRetry = 2
};

static auto DoNotVerify = [](std::string const&){ return true; };

class Aleth: public AlethFace
{
	Q_OBJECT

public:
	explicit Aleth(QObject* _parent = nullptr);
	virtual ~Aleth();

	/// Initialise everything. Must be called first. Begins start()ed.
	void init();

	WebThreeDirect* web3() const override { return m_webThree.get(); }
	NatSpecFace& natSpec() override { return m_natSpecDB; }
	eth::KeyManager& keyManager() override { return m_keyManager; }
	Secret retrieveSecret(Address const& _address) const override;

	/// Start the webthree subsystem.
	void open();
	/// Stop the webthree subsystem.
	void close();

//	void setBeneficiary(Address const& _a);

	// Watch API
	unsigned installWatch(eth::LogFilter const& _tf, WatchHandler const& _f) override;
	unsigned installWatch(h256 const& _tf, WatchHandler const& _f) override;
	void uninstallWatch(unsigned _w) override;

	// Account naming API
	void install(AccountNamer* _adopt) override;
	void uninstall(AccountNamer* _kill) override;
	void noteKnownAddressesChanged(AccountNamer*) override;
	void noteAddressNamesChanged(AccountNamer*) override;
	Address toAddress(std::string const&) const override;
	std::string toName(Address const&) const override;
	Addresses allKnownAddresses() const override;

protected:
	virtual std::pair<std::string, bool> getPassword(std::string const& _prompt, std::string const& _title = std::string(), std::string const& _hint = std::string(), std::function<bool(std::string const&)> const& _verify = DoNotVerify, int _flags = DefaultPasswordFlags, std::string const& _failMessage = std::string());

	virtual void openKeyManager();
	virtual void createKeyManager();

private slots:
	void checkHandlers();

private:
	std::unique_ptr<WebThreeDirect> m_webThree;
	eth::KeyManager m_keyManager;
	NatspecHandler m_natSpecDB;

	std::map<unsigned, WatchHandler> m_handlers;
	std::unordered_set<AccountNamer*> m_namers;

	bool m_destructing = false;
	std::string m_dbPath;
};

}

}
