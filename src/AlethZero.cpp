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
/** @file MainWin.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include <fstream>

// Make sure boost/asio.hpp is included before windows.h.
#include <boost/asio.hpp>

#pragma GCC diagnostic ignored "-Wpedantic"
//pragma GCC diagnostic ignored "-Werror=pedantic"
#include <QtNetwork/QNetworkReply>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QListWidgetItem>
#include <QtWidgets/QAbstractButton>
#include <QtGui/QClipboard>
#include <QtCore/QtCore>
#include <boost/algorithm/string.hpp>
#include <test/JsonSpiritHeaders.h>
#ifndef _MSC_VER
#include <libserpent/funcs.h>
#include <libserpent/util.h>
#endif
#include <libdevcore/FileSystem.h>
#include <libethcore/CommonJS.h>
#include <libethcore/EthashAux.h>
#include <libethcore/ICAP.h>
#include <liblll/Compiler.h>
#include <liblll/CodeFragment.h>
#include <libsolidity/Scanner.h>
#include <libsolidity/AST.h>
#include <libsolidity/SourceReferenceFormatter.h>
#include <libevm/VM.h>
#include <libevm/VMFactory.h>
#include <libethereum/CanonBlockChain.h>
#include <libethereum/ExtVM.h>
#include <libethereum/Client.h>
#include <libethereum/Utility.h>
#include <libethereum/EthereumHost.h>
#include <libethereum/DownloadMan.h>
#include <libweb3jsonrpc/WebThreeStubServer.h>
#include <libweb3jsonrpc/SafeHttpServer.h>
#include "AlethZero.h"
#include "SyncView.h"
#include "BuildInfo.h"
#include "OurWebThreeStubServer.h"
#include "Debugger.h"
#include "ui_AlethZero.h"
#include "ui_GetPassword.h"
using namespace std;
using namespace dev;
using namespace aleth;
using namespace p2p;
using namespace eth;
namespace js = json_spirit;

AlethZero::AlethZero(QWidget* _parent):
	Aleth(_parent),
	ui(new Ui::Main)
{
	setWindowFlags(Qt::Window);
	ui->setupUi(this);
	std::string dbPath = getDataDir();

	for (int i = 1; i < qApp->arguments().size(); ++i)
	{
		QString arg = qApp->arguments()[i];
		if (arg == "--frontier")
			resetNetwork(eth::Network::Frontier);
		else if (arg == "--olympic")
			resetNetwork(eth::Network::Olympic);
		else if (arg == "--genesis-json" && i + 1 < qApp->arguments().size())
			CanonBlockChain<Ethash>::setGenesis(contentsString(qApp->arguments()[++i].toStdString()));
		else if ((arg == "--db-path" || arg == "-d") && i + 1 < qApp->arguments().size())
			dbPath = qApp->arguments()[++i].toStdString();
	}

	if (c_network == eth::Network::Olympic)
		setWindowTitle("AlethZero Olympic");
	else if (c_network == eth::Network::Frontier)
		setWindowTitle("AlethZero Frontier");

	// Open Key Store
	bool opened = false;
	if (m_keyManager.exists())
		while (!opened)
		{
			QString s = QInputDialog::getText(nullptr, "Master password", "Enter your MASTER account password.", QLineEdit::Password, QString());
			if (m_keyManager.load(s.toStdString()))
				opened = true;
			else if (QMessageBox::question(
					nullptr,
					"Invalid password entered",
					"The password you entered is incorrect. If you have forgotten your password, and you wish to start afresh, manually remove the file: " + QString::fromStdString(getDataDir("ethereum")) + "/keys.info",
					QMessageBox::Retry,
					QMessageBox::Abort)
				== QMessageBox::Abort)
				exit(0);
		}
	if (!opened)
	{
		QString password;
		while (true)
		{
			password = QInputDialog::getText(nullptr, "Master password", "Enter a MASTER password for your key store. Make it strong. You probably want to write it down somewhere and keep it safe and secure; your identity will rely on this - you never want to lose it.", QLineEdit::Password, QString());
			QString confirm = QInputDialog::getText(nullptr, "Master password", "Confirm this password by typing it again", QLineEdit::Password, QString());
			if (password == confirm)
				break;
			QMessageBox::warning(nullptr, "Try again", "You entered two different passwords - please enter the same password twice.", QMessageBox::Ok);
		}
		m_keyManager.create(password.toStdString());
		m_keyManager.import(ICAP::createDirect(), "Default identity");
	}

#if ETH_DEBUG
	m_servers.append("127.0.0.1:30300");
#endif
	for (auto const& i: Host::pocHosts())
		m_servers.append(QString::fromStdString("enode://" + i.first.hex() + "@" + i.second));

	if (!dev::contents(dbPath + "/genesis.json").empty())
		CanonBlockChain<Ethash>::setGenesis(contentsString(dbPath + "/genesis.json"));

	cerr << "State root: " << CanonBlockChain<Ethash>::genesis().stateRoot() << endl;
	auto block = CanonBlockChain<Ethash>::createGenesisBlock();
	cerr << "Block Hash: " << CanonBlockChain<Ethash>::genesis().hash() << endl;
	cerr << "Block RLP: " << RLP(block) << endl;
	cerr << "Block Hex: " << toHex(block) << endl;
	cerr << "eth Network protocol version: " << eth::c_protocolVersion << endl;
	cerr << "Client database version: " << c_databaseVersion << endl;

//	statusBar()->addPermanentWidget(ui->cacheUsage);
	ui->cacheUsage->hide();
	statusBar()->addPermanentWidget(ui->balance);
	statusBar()->addPermanentWidget(ui->peerCount);
	statusBar()->addPermanentWidget(ui->mineStatus);
	statusBar()->addPermanentWidget(ui->syncStatus);
	statusBar()->addPermanentWidget(ui->chainStatus);
	statusBar()->addPermanentWidget(ui->blockCount);

	QSettings s("ethereum", "alethzero");
	m_networkConfig = s.value("peers").toByteArray();
	bytesConstRef network((byte*)m_networkConfig.data(), m_networkConfig.size());
	m_webThree.reset(new WebThreeDirect(string("AlethZero/v") + dev::Version + "/" DEV_QUOTED(ETH_BUILD_TYPE) "/" DEV_QUOTED(ETH_BUILD_PLATFORM), dbPath, WithExisting::Trust, {"eth"/*, "shh"*/}, p2p::NetworkPreferences(), network));

	ui->blockCount->setText(QString("PV%1.%2 D%3 %4-%5 v%6").arg(eth::c_protocolVersion).arg(eth::c_minorProtocolVersion).arg(c_databaseVersion).arg(QString::fromStdString(ethereum()->sealEngine()->name())).arg(ethereum()->sealEngine()->revision()).arg(dev::Version));

	m_httpConnector.reset(new dev::SafeHttpServer(SensibleHttpPort, "", "", dev::SensibleHttpThreads));
	auto w3ss = new OurWebThreeStubServer(*m_httpConnector, this);
	m_server.reset(w3ss);
	m_server->StartListening();

	setBeneficiary(m_keyManager.accounts().front());

	ethereum()->setDefault(LatestBlock);

	m_vmSelectionGroup = new QActionGroup{ui->menuConfig};
	m_vmSelectionGroup->addAction(ui->vmInterpreter);
	m_vmSelectionGroup->addAction(ui->vmJIT);
	m_vmSelectionGroup->addAction(ui->vmSmart);
	m_vmSelectionGroup->setExclusive(true);

#if ETH_EVMJIT
	ui->vmSmart->setChecked(true); // Default when JIT enabled
	on_vmSmart_triggered();
#else
	ui->vmInterpreter->setChecked(true);
	ui->vmJIT->setEnabled(false);
	ui->vmSmart->setEnabled(false);
#endif

	readSettings(true, false);

	installWatches();
	startTimer(100);

	{
		QSettings s("ethereum", "alethzero");
		if (s.value("splashMessage", true).toBool())
		{
			QMessageBox::information(this, "Here Be Dragons!", "This is beta software: it is not yet at the release stage.\nPlease don't blame us if it does something unexpected or if you're underwhelmed with the user-experience. We have great plans for it in terms of UX down the line but right now we just want to make sure everything works roughly as expected. We welcome contributions, be they in code, testing or documentation!\nAfter you close this message it won't appear again.");
			s.setValue("splashMessage", false);
		}
	}

	for (auto const& i: *s_linkedPlugins)
	{
		cdebug << "Initialising plugin:" << i.first;
		initPlugin(i.second(this));
	}

	readSettings(false, true);
}

AlethZero::~AlethZero()
{
	m_httpConnector->StopListening();

	// save all settings here so we don't have to explicitly finalise plugins.
	// NOTE: as soon as plugin finalisation means anything more than saving settings, this will
	// need to be rethought into something more like:
	// forEach([&](shared_ptr<Plugin> const& p){ finalisePlugin(p.get()); });
	writeSettings();

	m_destructing = true;
	killPlugins();
}

void AlethZero::install(AccountNamer* _adopt)
{
	m_namers.insert(_adopt);
	_adopt->m_main = this;
	refreshAll();
}

void AlethZero::uninstall(AccountNamer* _kill)
{
	m_namers.erase(_kill);
	if (!m_destructing)
		refreshAll();
}

void AlethZero::noteKnownAddressesChanged(AccountNamer*)
{
	emit knownAddressesChanged();
	refreshAll();
}

void AlethZero::noteAddressNamesChanged(AccountNamer*)
{
	emit addressNamesChanged();
	refreshAll();
}

Address AlethZero::toAddress(string const& _n) const
{
	for (AccountNamer* n: m_namers)
		if (n->toAddress(_n))
			return n->toAddress(_n);
	return Address();
}

string AlethZero::toName(Address const& _a) const
{
	for (AccountNamer* n: m_namers)
		if (!n->toName(_a).empty())
			return n->toName(_a);
	return string();
}

Addresses AlethZero::allKnownAddresses() const
{
	Addresses ret;
	for (AccountNamer* i: m_namers)
		ret += i->knownAddresses();
	sort(ret.begin(), ret.end());
	return ret;
}

bool AlethZero::confirm() const
{
	return true; //ui->natSpec->isChecked();
}

void AlethZero::on_sentinel_triggered()
{
	bool ok;
	QString sentinel = QInputDialog::getText(nullptr, "Enter sentinel address", "Enter the sentinel address for bad block reporting (e.g. http://badblockserver.com:8080). Enter nothing to disable.", QLineEdit::Normal, QString::fromStdString(ethereum()->sentinel()), &ok);
	if (ok)
		ethereum()->setSentinel(sentinel.toStdString());
}

NetworkPreferences AlethZero::netPrefs() const
{
	auto listenIP = ui->listenIP->text().toStdString();
	try
	{
		listenIP = bi::address::from_string(listenIP).to_string();
	}
	catch (...)
	{
		listenIP.clear();
	}

	auto publicIP = ui->forcePublicIP->text().toStdString();
	try
	{
		publicIP = bi::address::from_string(publicIP).to_string();
	}
	catch (...)
	{
		publicIP.clear();
	}

	NetworkPreferences ret;

	if (isPublicAddress(publicIP))
		ret = NetworkPreferences(publicIP, listenIP, ui->port->value(), ui->upnp->isChecked());
	else
		ret = NetworkPreferences(listenIP, ui->port->value(), ui->upnp->isChecked());

	ret.discovery = m_privateChain.isEmpty() && !ui->hermitMode->isChecked();
	ret.pin = !ret.discovery;

	return ret;
}

void AlethZero::onKeysChanged()
{
	// TODO: reinstall balance watchers
}

void AlethZero::installWatches()
{
	auto newBlockId = installWatch(ChainChangedFilter, [=](LocalisedLogEntries const&){
		onNewBlock();
	});

	cdebug << "newBlock watch ID: " << newBlockId;
}

bool AlethZero::doConfirm()
{
	return ui->confirm->isChecked();
}

void AlethZero::onNewBlock()
{
	cwatch << "Blockchain changed!";

	// update blockchain dependent views.
	refreshBlockCount();

	// We must update balances since we can't filter updates to basic accounts.
	refreshBalances();
}

void AlethZero::on_forceMining_triggered()
{
	ethereum()->setForceMining(ui->forceMining->isChecked());
}

QString AlethZero::contents(QString _s)
{
	return QString::fromStdString(dev::asString(dev::contents(_s.toStdString())));
}

void AlethZero::note(QString _s)
{
	cnote << _s.toStdString();
}

void AlethZero::debug(QString _s)
{
	cdebug << _s.toStdString();
}

void AlethZero::warn(QString _s)
{
	cwarn << _s.toStdString();
}

std::string AlethZero::pretty(dev::Address const& _a) const
{
	for (auto i: m_namers)
	{
		auto n = i->toName(_a);
		if (!n.empty())
			return n;
	}
	return string();
}

std::string AlethZero::render(dev::Address const& _a) const
{
	string p = pretty(_a);
	string n;
	if (p.size() == 9 && p.find_first_not_of("QWERYUOPASDFGHJKLZXCVBNM1234567890") == string::npos)
		p = ICAP(p, "XREG").encoded();
	else
		DEV_IGNORE_EXCEPTIONS(n = ICAP(_a).encoded().substr(0, 8));
	if (!n.empty())
		n += " ";
	n += _a.abridged();
	return p.empty() ? n : (p + " " + n);
}

pair<Address, bytes> AlethZero::fromString(std::string const& _n) const
{
	if (_n == "(Create Contract)")
		return make_pair(Address(), bytes());

	std::string n = _n;

	for (auto i: m_namers)
		if (auto a = i->toAddress(_n))
			return make_pair(a, bytes());

	try {
		return ICAP::decoded(n).address([&](Address const& a, bytes const& b) -> bytes
		{
			return ethereum()->call(a, b).output;
		}, Address("a1a111bc074c9cfa781f0c38e63bd51c91b8af00"));
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

QString AlethZero::lookup(QString const& _a) const
{
	if (!_a.endsWith(".eth"))
		return _a;

	string sn = _a.mid(0, _a.size() - 4).toStdString();
	if (sn.size() > 32)
		sn = sha3(sn, false);
	h256 n;
	memcpy(n.data(), sn.data(), sn.size());

	h256 ret;
	// TODO: fix with the new DNSreg contract
//	if (h160 dnsReg = (u160)ethereum()->stateAt(c_config, 4, PendingBlock))
//		ret = ethereum()->stateAt(dnsReg, n);
/*	if (!ret)
		if (h160 nameReg = (u160)ethereum()->stateAt(c_config, 0, PendingBlock))
			ret = ethereum()->stateAt(nameReg, n2);
*/
	if (ret && !((u256)ret >> 32))
		return QString("%1.%2.%3.%4").arg((int)ret[28]).arg((int)ret[29]).arg((int)ret[30]).arg((int)ret[31]);
	// TODO: support IPv6.
	else if (ret)
		return QString::fromStdString(fromRaw(ret));
	else
		return _a;
}

void AlethZero::on_about_triggered()
{
	QMessageBox::about(this, "About " + windowTitle(), QString("AlethZero/v") + dev::Version + "/" DEV_QUOTED(ETH_BUILD_TYPE) "/" DEV_QUOTED(ETH_BUILD_PLATFORM) "\n" DEV_QUOTED(ETH_COMMIT_HASH) + (ETH_CLEAN_REPO ? "\nCLEAN" : "\n+ LOCAL CHANGES") + "\n\nBy Gav Wood and the Berlin ÐΞV team, 2014, 2015.\nSee the README for contributors and credits.");
}

void AlethZero::on_paranoia_triggered()
{
	ethereum()->setParanoia(ui->paranoia->isChecked());
}

void AlethZero::writeSettings()
{
	QSettings s("ethereum", "alethzero");
	s.remove("address");

	forEach([&](std::shared_ptr<Plugin> p)
	{
		p->writeSettings(s);
	});

	s.setValue("askPrice", QString::fromStdString(toString(static_cast<TrivialGasPricer*>(ethereum()->gasPricer().get())->ask())));
	s.setValue("bidPrice", QString::fromStdString(toString(static_cast<TrivialGasPricer*>(ethereum()->gasPricer().get())->bid())));
	s.setValue("upnp", ui->upnp->isChecked());
	s.setValue("hermitMode", ui->hermitMode->isChecked());
	s.setValue("forceAddress", ui->forcePublicIP->text());
	s.setValue("forceMining", ui->forceMining->isChecked());
	s.setValue("turboMining", ui->turboMining->isChecked());
	s.setValue("paranoia", ui->paranoia->isChecked());
	s.setValue("natSpec", ui->natSpec->isChecked());
	s.setValue("showAll", ui->showAll->isChecked());
	s.setValue("clientName", ui->clientName->text());
	s.setValue("idealPeers", ui->idealPeers->value());
	s.setValue("listenIP", ui->listenIP->text());
	s.setValue("port", ui->port->value());
	s.setValue("privateChain", m_privateChain);
	if (auto vm = m_vmSelectionGroup->checkedAction())
		s.setValue("vm", vm->text());

	bytes d = m_webThree->saveNetwork();
	if (!d.empty())
		m_networkConfig = QByteArray((char*)d.data(), (int)d.size());
	s.setValue("peers", m_networkConfig);

	s.setValue("geometry", saveGeometry());
	s.setValue("windowState", saveState());
}

void AlethZero::setPrivateChain(QString const& _private, bool _forceConfigure)
{
	if (m_privateChain == _private && !_forceConfigure)
		return;

	m_privateChain = _private;
	ui->usePrivate->setChecked(!m_privateChain.isEmpty());

	CanonBlockChain<Ethash>::forceGenesisExtraData(m_privateChain.isEmpty() ? bytes() : sha3(m_privateChain.toStdString()).asBytes());
	CanonBlockChain<Ethash>::forceGenesisDifficulty(m_privateChain.isEmpty() ? u256() : c_minimumDifficulty);
	CanonBlockChain<Ethash>::forceGenesisGasLimit(m_privateChain.isEmpty() ? u256() : u256(1) << 32);

	// rejig blockchain now.
	writeSettings();
	ui->mine->setChecked(false);
	ui->net->setChecked(false);
	web3()->stopNetwork();

	web3()->setNetworkPreferences(netPrefs());
	ethereum()->reopenChain();

	readSettings(true);
	installWatches();
	refreshAll();
}

void AlethZero::readSettings(bool _skipGeometry, bool _onlyGeometry)
{
	QSettings s("ethereum", "alethzero");

	if (!_skipGeometry)
		restoreGeometry(s.value("geometry").toByteArray());
	restoreState(s.value("windowState").toByteArray());
	if (_onlyGeometry)
		return;

	forEach([&](std::shared_ptr<Plugin> p)
	{
		p->readSettings(s);
	});
	static_cast<TrivialGasPricer*>(ethereum()->gasPricer().get())->setAsk(u256(s.value("askPrice", QString::fromStdString(toString(DefaultGasPrice))).toString().toStdString()));
	static_cast<TrivialGasPricer*>(ethereum()->gasPricer().get())->setBid(u256(s.value("bidPrice", QString::fromStdString(toString(DefaultGasPrice))).toString().toStdString()));

	ui->upnp->setChecked(s.value("upnp", true).toBool());
	ui->forcePublicIP->setText(s.value("forceAddress", "").toString());
	ui->dropPeers->setChecked(false);
	ui->hermitMode->setChecked(s.value("hermitMode", false).toBool());
	ui->forceMining->setChecked(s.value("forceMining", false).toBool());
	on_forceMining_triggered();
	ui->turboMining->setChecked(s.value("turboMining", false).toBool());
	on_turboMining_triggered();
	ui->paranoia->setChecked(s.value("paranoia", false).toBool());
	ui->natSpec->setChecked(s.value("natSpec", true).toBool());
	ui->showAll->setChecked(s.value("showAll", false).toBool());
	ui->clientName->setText(s.value("clientName", "").toString());
	if (ui->clientName->text().isEmpty())
		ui->clientName->setText(QInputDialog::getText(nullptr, "Enter identity", "Enter a name that will identify you on the peer network"));
	ui->idealPeers->setValue(s.value("idealPeers", ui->idealPeers->value()).toInt());
	ui->listenIP->setText(s.value("listenIP", "").toString());
	ui->port->setValue(s.value("port", ui->port->value()).toInt());
	setPrivateChain(s.value("privateChain", "").toString());

#if ETH_EVMJIT // We care only if JIT is enabled. Otherwise it can cause misconfiguration.
	auto vmName = s.value("vm").toString();
	if (!vmName.isEmpty())
	{
		if (vmName == ui->vmInterpreter->text())
		{
			ui->vmInterpreter->setChecked(true);
			on_vmInterpreter_triggered();
		}
		else if (vmName == ui->vmJIT->text())
		{
			ui->vmJIT->setChecked(true);
			on_vmJIT_triggered();
		}
		else if (vmName == ui->vmSmart->text())
		{
			ui->vmSmart->setChecked(true);
			on_vmSmart_triggered();
		}
	}
#endif
}

Secret AlethZero::retrieveSecret(Address const& _address) const
{
	while (true)
	{
		Secret s = m_keyManager.secret(_address, [&](){
			QDialog d;
			Ui_GetPassword gp;
			gp.setupUi(&d);
			d.setWindowTitle("Unlock Account");
			gp.label->setText(QString("Enter the password for the account %2 (%1).").arg(QString::fromStdString(_address.abridged())).arg(QString::fromStdString(m_keyManager.accountName(_address))));
			gp.entry->setPlaceholderText("Hint: " + QString::fromStdString(m_keyManager.passwordHint(_address)));
			return d.exec() == QDialog::Accepted ? gp.entry->text().toStdString() : string();
		});
		if (s || QMessageBox::warning(nullptr, "Unlock Account", "The password you gave is incorrect for this key.", QMessageBox::Retry, QMessageBox::Cancel) == QMessageBox::Cancel)
			return s;
	}
}

std::string AlethZero::getPassword(std::string const& _title, std::string const& _for, std::string* _hint, bool* _ok)
{
	QString password;
	while (true)
	{
		bool ok;
		password = QInputDialog::getText(nullptr, QString::fromStdString(_title), QString::fromStdString(_for), QLineEdit::Password, QString(), &ok);
		if (!ok)
		{
			if (_ok)
				*_ok = false;
			return string();
		}
		if (password.isEmpty())
			break;
		QString confirm = QInputDialog::getText(nullptr, QString::fromStdString(_title), "Confirm this password by typing it again", QLineEdit::Password, QString());
		if (password == confirm)
			break;
		QMessageBox::warning(nullptr, QString::fromStdString(_title), "You entered two different passwords - please enter the same password twice.", QMessageBox::Ok);
	}

	if (!password.isEmpty() && _hint && !m_keyManager.haveHint(password.toStdString()))
		*_hint = QInputDialog::getText(this, "Create Account", "Enter a hint to help you remember this password.").toStdString();

	if (_ok)
		*_ok = true;
	return password.toStdString();
}

void AlethZero::on_exportKey_triggered()
{
	if (ui->ourAccounts->currentRow() >= 0)
	{
		auto hba = ui->ourAccounts->currentItem()->data(Qt::UserRole).toByteArray();
		Address h((byte const*)hba.data(), Address::ConstructFromPointer);
		Secret s = retrieveSecret(h);
		QMessageBox::information(this, "Export Account Key", "Secret key to account " + QString::fromStdString(render(h) + " is:\n" + s.makeInsecure().hex()));
	}
}

void AlethZero::on_usePrivate_triggered()
{
	QString pc;
	if (ui->usePrivate->isChecked())
	{
		bool ok;
		pc = QInputDialog::getText(this, "Enter Name", "Enter the name of your private chain", QLineEdit::Normal, QString("NewChain-%1").arg(time(0)), &ok);
		if (!ok)
		{
			ui->usePrivate->setChecked(false);
			return;
		}
	}
	setPrivateChain(pc);
}

void AlethZero::on_vmInterpreter_triggered() { VMFactory::setKind(VMKind::Interpreter); }
void AlethZero::on_vmJIT_triggered() { VMFactory::setKind(VMKind::JIT); }
void AlethZero::on_vmSmart_triggered() { VMFactory::setKind(VMKind::Smart); }

void AlethZero::on_rewindChain_triggered()
{
	bool ok;
	int n = QInputDialog::getInt(this, "Rewind Chain", "Enter the number of the new chain head.", ethereum()->number() * 9 / 10, 1, ethereum()->number(), 1, &ok);
	if (ok)
	{
		ethereum()->rewind(n);
		refreshAll();
	}
}

void AlethZero::on_preview_triggered()
{
	ethereum()->setDefault(ui->preview->isChecked() ? PendingBlock : LatestBlock);
	refreshAll();
}

void AlethZero::on_prepNextDAG_triggered()
{
	EthashAux::computeFull(
		EthashAux::seedHash(
			ethereum()->blockChain().number() + ETHASH_EPOCH_LENGTH
		)
	);
}

void AlethZero::refreshMining()
{
	pair<uint64_t, unsigned> gp = EthashAux::fullGeneratingProgress();
	QString t;
	if (gp.first != EthashAux::NotGenerating)
		t = QString("DAG for #%1-#%2: %3% complete; ").arg(gp.first).arg(gp.first + ETHASH_EPOCH_LENGTH - 1).arg(gp.second);
	WorkingProgress p = ethereum()->miningProgress();
	ui->mineStatus->setText(t + (ethereum()->isMining() ? p.hashes > 0 ? QString("%1s @ %2kH/s").arg(p.ms / 1000).arg(p.ms ? p.hashes / p.ms : 0) : "Awaiting DAG" : "Not mining"));
}

void AlethZero::setBeneficiary(Address const& _b)
{
	for (int i = 0; i < ui->ourAccounts->count(); ++i)
	{
		auto hba = ui->ourAccounts->item(i)->data(Qt::UserRole).toByteArray();
		auto h = Address((byte const*)hba.data(), Address::ConstructFromPointer);
		ui->ourAccounts->item(i)->setCheckState(h == _b ? Qt::Checked : Qt::Unchecked);
	}
	m_beneficiary = _b;
	ethereum()->setBeneficiary(_b);
}

void AlethZero::on_ourAccounts_itemClicked(QListWidgetItem* _i)
{
	auto hba = _i->data(Qt::UserRole).toByteArray();
	setBeneficiary(Address((byte const*)hba.data(), Address::ConstructFromPointer));
}

void AlethZero::refreshBalances()
{
	cwatch << "refreshBalances()";
	// update all the balance-dependent stuff.
	ui->ourAccounts->clear();
	u256 totalBalance = 0;
/*	map<Address, tuple<QString, u256, u256>> altCoins;
	Address coinsAddr = getCurrencies();
	for (unsigned i = 0; i < ethereum()->stateAt(coinsAddr, PendingBlock); ++i)
	{
		auto n = ethereum()->stateAt(coinsAddr, i + 1);
		auto addr = right160(ethereum()->stateAt(coinsAddr, n));
		auto denom = ethereum()->stateAt(coinsAddr, sha3(h256(n).asBytes()));
		if (denom == 0)
			denom = 1;
//		cdebug << n << addr << denom << sha3(h256(n).asBytes());
		altCoins[addr] = make_tuple(fromRaw(n), 0, denom);
	}*/
	for (auto const& address: m_keyManager.accounts())
	{
		u256 b = ethereum()->balanceAt(address);
		QListWidgetItem* li = new QListWidgetItem(
			QString("<%1> %2: %3 [%4]")
				.arg(m_keyManager.haveKey(address) ? "KEY" : "BRAIN")
				.arg(QString::fromStdString(render(address)))
				.arg(formatBalance(b).c_str())
				.arg((unsigned)ethereum()->countAt(address))
			, ui->ourAccounts);
		li->setData(Qt::UserRole, QByteArray((char const*)address.data(), Address::size));
		li->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
		li->setCheckState(m_beneficiary == address ? Qt::Checked : Qt::Unchecked);
		totalBalance += b;

//		for (auto& c: altCoins)
//			get<1>(c.second) += (u256)ethereum()->stateAt(c.first, (u160)i.address());
	}

	QString b;
/*	for (auto const& c: altCoins)
		if (get<1>(c.second))
		{
			stringstream s;
			s << setw(toString(get<2>(c.second) - 1).size()) << setfill('0') << (get<1>(c.second) % get<2>(c.second));
			b += QString::fromStdString(toString(get<1>(c.second) / get<2>(c.second)) + "." + s.str() + " ") + get<0>(c.second).toUpper() + " | ";
		}*/
	ui->balance->setText(b + QString::fromStdString(formatBalance(totalBalance)));
}

void AlethZero::refreshNetwork()
{
	auto ps = web3()->peers();

	ui->peerCount->setText(QString::fromStdString(toString(ps.size())) + " peer(s)");
	ui->peers->clear();
	ui->nodes->clear();

	if (web3()->haveNetwork())
	{
		map<h512, QString> sessions;
		for (PeerSessionInfo const& i: ps)
			ui->peers->addItem(QString("[%8 %7] %3 ms - %1:%2 - %4 %5 %6")
				.arg(QString::fromStdString(i.host))
				.arg(i.port)
				.arg(chrono::duration_cast<chrono::milliseconds>(i.lastPing).count())
				.arg(sessions[i.id] = QString::fromStdString(i.clientVersion))
				.arg(QString::fromStdString(toString(i.caps)))
				.arg(QString::fromStdString(toString(i.notes)))
				.arg(i.socketId)
				.arg(QString::fromStdString(i.id.abridged())));

		auto ns = web3()->nodes();
		for (p2p::Peer const& i: ns)
			ui->nodes->insertItem(sessions.count(i.id) ? 0 : ui->nodes->count(), QString("[%1 %3] %2 - ( %4 ) - *%5")
				.arg(QString::fromStdString(i.id.abridged()))
				.arg(QString::fromStdString(i.endpoint.address.to_string()))
				.arg(i.id == web3()->id() ? "self" : sessions.count(i.id) ? sessions[i.id] : "disconnected")
				.arg(i.isOffline() ? " | " + QString::fromStdString(reasonOf(i.lastDisconnect())) + " | " + QString::number(i.failedAttempts()) + "x" : "")
				.arg(i.rating())
				);
	}
}

void AlethZero::refreshAll()
{
	refreshBlockCount();
	refreshBalances();
	allChange();
}

void AlethZero::refreshBlockCount()
{
	auto d = ethereum()->blockChain().details();
	SyncStatus sync = ethereum()->syncStatus();
	QString syncStatus = QString("PV%1 %2").arg(sync.protocolVersion).arg(EthereumHost::stateName(sync.state));
	if (sync.state == SyncState::Hashes)
		syncStatus += QString(": %1/%2%3").arg(sync.hashesReceived).arg(sync.hashesEstimated ? "~" : "").arg(sync.hashesTotal);
	if (sync.state == SyncState::Blocks || sync.state == SyncState::NewBlocks)
		syncStatus += QString(": %1/%2").arg(sync.blocksReceived).arg(sync.blocksTotal);
	ui->syncStatus->setText(syncStatus);
//	BlockQueueStatus b = ethereum()->blockQueueStatus();
//	ui->chainStatus->setText(QString("%3 importing %4 ready %5 verifying %6 unverified %7 future %8 unknown %9 bad  %1 #%2")
//		.arg(m_privateChain.size() ? "[" + m_privateChain + "] " : c_network == eth::Network::Olympic ? "Olympic" : "Frontier").arg(d.number).arg(b.importing).arg(b.verified).arg(b.verifying).arg(b.unverified).arg(b.future).arg(b.unknown).arg(b.bad));
	ui->chainStatus->setText(QString("%1 #%2")
		.arg(m_privateChain.size() ? "[" + m_privateChain + "] " : c_network == eth::Network::Olympic ? "Olympic" : "Frontier").arg(d.number));
}

void AlethZero::on_turboMining_triggered()
{
	ethereum()->setTurboMining(ui->turboMining->isChecked());
}

void AlethZero::on_refresh_triggered()
{
	refreshAll();
}
/*
static std::string niceUsed(unsigned _n)
{
	static const vector<std::string> c_units = { "bytes", "KB", "MB", "GB", "TB", "PB" };
	unsigned u = 0;
	while (_n > 10240)
	{
		_n /= 1024;
		u++;
	}
	if (_n > 1000)
		return toString(_n / 1000) + "." + toString((min<unsigned>(949, _n % 1000) + 50) / 100) + " " + c_units[u + 1];
	else
		return toString(_n) + " " + c_units[u];
}

void Main::refreshCache()
{
	BlockChain::Statistics s = ethereum()->blockChain().usage();
	QString t;
	auto f = [&](unsigned n, QString l)
	{
		t += ("%1 " + l).arg(QString::fromStdString(niceUsed(n)));
	};
	f(s.memTotal(), "total");
	t += " (";
	f(s.memBlocks, "blocks");
	t += ", ";
	f(s.memReceipts, "receipts");
	t += ", ";
	f(s.memLogBlooms, "blooms");
	t += ", ";
	f(s.memBlockHashes + s.memTransactionAddresses, "hashes");
	t += ", ";
	f(s.memDetails, "family");
	t += ")";
	ui->cacheUsage->setText(t);
}
*/
void AlethZero::timerEvent(QTimerEvent*)
{
	// 7/18, Alex: aggregating timers, prelude to better threading?
	// Runs much faster on slower dual-core processors
	static int interval = 100;

	// refresh mining every 200ms
	if (interval / 100 % 2 == 0)
		refreshMining();

	if ((interval / 100 % 2 == 0 && m_webThree->ethereum()->isSyncing()) || interval == 1000)
		ui->downloadView->update();

	// refresh peer list every 1000ms, reset counter
	if (interval == 1000)
	{
		interval = 0;
		refreshNetwork();
		refreshBlockCount();
		poll();
	}
	else
		interval += 100;
}

string AlethZero::renderDiff(StateDiff const& _d) const
{
	stringstream s;

	auto indent = "<code style=\"white-space: pre\">     </code>";
	for (auto const& i: _d.accounts)
	{
		s << "<hr/>";

		AccountDiff ad = i.second;
		s << "<code style=\"white-space: pre; font-weight: bold\">" << lead(ad.changeType()) << "  </code>" << " <b>" << render(i.first) << "</b>";
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

void AlethZero::on_injectBlock_triggered()
{
	QString s = QInputDialog::getText(this, "Inject Block", "Enter block dump in hex");
	try
	{
		bytes b = fromHex(s.toStdString(), WhenError::Throw);
		ethereum()->injectBlock(b);
	}
	catch (BadHexCharacter& _e)
	{
		cwarn << "invalid hex character, transaction rejected";
		cwarn << boost::diagnostic_information(_e);
	}
	catch (...)
	{
		cwarn << "block rejected";
	}
}

void AlethZero::on_idealPeers_valueChanged(int)
{
	m_webThree->setIdealPeerCount(ui->idealPeers->value());
}

void AlethZero::on_ourAccounts_doubleClicked()
{
	auto hba = ui->ourAccounts->currentItem()->data(Qt::UserRole).toByteArray();
	auto h = Address((byte const*)hba.data(), Address::ConstructFromPointer);
	qApp->clipboard()->setText(QString::fromStdString(ICAP(h).encoded()) + " (" + QString::fromStdString(h.hex()) + ")");
}

/*void Main::on_log_doubleClicked()
{
	ui->log->setPlainText("");
	m_logHistory.clear();
}*/

void AlethZero::on_clearPending_triggered()
{
	writeSettings();
	ui->mine->setChecked(false);
	ui->net->setChecked(false);
	web3()->stopNetwork();
	ethereum()->clearPending();
	readSettings(true);
	installWatches();
	refreshAll();
}

void AlethZero::on_retryUnknown_triggered()
{
	ethereum()->retryUnknown();
}

void AlethZero::on_killBlockchain_triggered()
{
	writeSettings();
	ui->mine->setChecked(false);
	ui->net->setChecked(false);
	web3()->stopNetwork();
	ethereum()->killChain();
	readSettings(true);
	installWatches();
	refreshAll();
}

void AlethZero::on_net_triggered()
{
	ui->port->setEnabled(!ui->net->isChecked());
	ui->clientName->setEnabled(!ui->net->isChecked());
	web3()->setClientVersion(WebThreeDirect::composeClientVersion("AlethZero", ui->clientName->text().toStdString()));
	if (ui->net->isChecked())
	{
		web3()->setIdealPeerCount(ui->idealPeers->value());
		web3()->setNetworkPreferences(netPrefs(), ui->dropPeers->isChecked());
		ethereum()->setNetworkId((h256)(u256)(int)c_network);
		web3()->startNetwork();
		ui->downloadView->setEthereum(ethereum());
		ui->enode->setText(QString::fromStdString(web3()->enode()));
	}
	else
	{
		ui->downloadView->setEthereum(nullptr);
		writeSettings();
		web3()->stopNetwork();
	}
}

void AlethZero::on_connect_triggered()
{
	if (!ui->net->isChecked())
	{
		ui->net->setChecked(true);
		on_net_triggered();
	}

	m_connect.setEnvironment(m_servers);
	if (m_connect.exec() == QDialog::Accepted)
	{
		m_connect.reset();
		try
		{
			web3()->addPeer(NodeSpec(m_connect.host().toStdString()), m_connect.required() ? PeerType::Required : PeerType::Optional);
		}
		catch (...)
		{
			QMessageBox::warning(this, "Connect to Node", "Couldn't interpret that address. Ensure you've typed it in properly and that it's in a reasonable format.", QMessageBox::Ok);
		}
	}
}

void AlethZero::on_mine_triggered()
{
	if (ui->mine->isChecked())
	{
//		EthashAux::computeFull(ethereum()->blockChain().number());
		ethereum()->setBeneficiary(m_beneficiary);
		ethereum()->startMining();
	}
	else
		ethereum()->stopMining();
}

void AlethZero::keysChanged()
{
	emit keyManagerChanged();
	refreshBalances();
}

void AlethZero::on_killAccount_triggered()
{
	if (ui->ourAccounts->currentRow() >= 0)
	{
		auto hba = ui->ourAccounts->currentItem()->data(Qt::UserRole).toByteArray();
		Address h((byte const*)hba.data(), Address::ConstructFromPointer);
		QString s = QInputDialog::getText(this, QString::fromStdString("Kill Account " + m_keyManager.accountName(h) + "?!"),
			QString::fromStdString("Account " + m_keyManager.accountName(h) + " (" + render(h) + ") has " + formatBalance(ethereum()->balanceAt(h)) + " in it.\r\nIt, and any contract that this account can access, will be lost forever if you continue. Do NOT continue unless you know what you are doing.\n"
			"Are you sure you want to continue? \r\n If so, type 'YES' to confirm."),
			QLineEdit::Normal, "NO");
		if (s != "YES")
			return;
		m_keyManager.kill(h);
		if (m_keyManager.accounts().empty())
			m_keyManager.import(Secret::random(), "Default account");
		m_beneficiary = m_keyManager.accounts().front();
		keysChanged();
		if (m_beneficiary == h)
			setBeneficiary(m_keyManager.accounts().front());
	}
}

void AlethZero::on_reencryptKey_triggered()
{
	if (ui->ourAccounts->currentRow() >= 0)
	{
		auto hba = ui->ourAccounts->currentItem()->data(Qt::UserRole).toByteArray();
		Address a((byte const*)hba.data(), Address::ConstructFromPointer);
		QStringList kdfs = {"PBKDF2-SHA256", "Scrypt"};
		bool ok = true;
		KDF kdf = (KDF)kdfs.indexOf(QInputDialog::getItem(this, "Re-Encrypt Key", "Select a key derivation function to use for storing your key:", kdfs, kdfs.size() - 1, false, &ok));
		if (!ok)
			return;
		std::string hint;
		std::string password = getPassword("Create Account", "Enter the password you would like to use for this key. Don't forget it!\nEnter nothing to use your Master password.", &hint, &ok);
		if (!ok)
			return;
		try {
			auto pw = [&](){
				auto p = QInputDialog::getText(this, "Re-Encrypt Key", "Enter the original password for this key.\nHint: " + QString::fromStdString(m_keyManager.passwordHint(a)), QLineEdit::Password, QString()).toStdString();
				if (p.empty())
					throw PasswordUnknown();
				return p;
			};
			while (!(password.empty() ? m_keyManager.recode(a, SemanticPassword::Master, pw, kdf) : m_keyManager.recode(a, password, hint, pw, kdf)))
				if (QMessageBox::question(this, "Re-Encrypt Key", "Password given is incorrect. Would you like to try again?", QMessageBox::Retry, QMessageBox::Cancel) == QMessageBox::Cancel)
					return;
		}
		catch (PasswordUnknown&) {}
	}
}

void AlethZero::on_reencryptAll_triggered()
{
	QStringList kdfs = {"PBKDF2-SHA256", "Scrypt"};
	bool ok = false;
	QString kdf = QInputDialog::getItem(this, "Re-Encrypt Key", "Select a key derivation function to use for storing your keys:", kdfs, kdfs.size() - 1, false, &ok);
	if (!ok)
		return;
	try {
		for (Address const& a: m_keyManager.accounts())
			while (!m_keyManager.recode(a, SemanticPassword::Existing, [&](){
				auto p = QInputDialog::getText(nullptr, "Re-Encrypt Key", QString("Enter the original password for key %1.\nHint: %2").arg(QString::fromStdString(pretty(a))).arg(QString::fromStdString(m_keyManager.passwordHint(a))), QLineEdit::Password, QString()).toStdString();
				if (p.empty())
					throw PasswordUnknown();
				return p;
			}, (KDF)kdfs.indexOf(kdf)))
				if (QMessageBox::question(this, "Re-Encrypt Key", "Password given is incorrect. Would you like to try again?", QMessageBox::Retry, QMessageBox::Cancel) == QMessageBox::Cancel)
					return;
	}
	catch (PasswordUnknown&) {}
}

void AlethZero::on_go_triggered()
{
	if (!ui->net->isChecked())
	{
		ui->net->setChecked(true);
		on_net_triggered();
	}
	for (auto const& i: Host::pocHosts())
		web3()->requirePeer(i.first, i.second);
}

int AlethZero::authenticate(QString _title, QString _text)
{
	QMessageBox userInput(this);
	userInput.setText(_title);
	userInput.setInformativeText(_text);
	userInput.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	userInput.button(QMessageBox::Ok)->setText("Allow");
	userInput.button(QMessageBox::Cancel)->setText("Reject");
	userInput.setDefaultButton(QMessageBox::Cancel);
	return userInput.exec();
}

void AlethZero::initPlugin(Plugin* _p)
{
	QSettings s("ethereum", "alethzero");
	_p->readSettings(s);
}

void AlethZero::finalisePlugin(Plugin* _p)
{
	QSettings s("ethereum", "alethzero");
	_p->writeSettings(s);
}

void AlethZero::unloadPlugin(string const& _name)
{
	shared_ptr<Plugin> p = takePlugin(_name);
	if (p)
		finalisePlugin(p.get());
}
