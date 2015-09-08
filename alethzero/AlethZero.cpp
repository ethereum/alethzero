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
/** @file AlethZero.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include <fstream>
// Make sure boost/asio.hpp is included before windows.h.
#include <boost/asio.hpp>
#include "AlethZero.h"
#pragma GCC diagnostic ignored "-Wpedantic"
//pragma GCC diagnostic ignored "-Werror=pedantic"
#include <QtCore/QtCore>
#include <QtGui/QClipboard>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QListWidgetItem>
#include <libethcore/EthashAux.h>
#include <libethcore/ICAP.h>
#include <libethereum/Client.h>
#include <libethereum/EthereumHost.h>
#include <libweb3jsonrpc/SafeHttpServer.h>
#include <libaleth/SyncView.h>
#include "AccountHolder.h"
#include "WebThreeServer.h"
#include "BuildInfo.h"
#include "SettingsDialog.h"
#include "NetworkSettings.h"
#include "ui_AlethZero.h"
using namespace std;
using namespace dev;
using namespace aleth;
using namespace zero;
using namespace p2p;
using namespace eth;

void PrivateChainManager::setID(QString _id)
{
	if (m_id != _id)
	{
		m_id = _id;
		CanonBlockChain<Ethash>::forceGenesisExtraData(m_id.isEmpty() ? bytes() : sha3(m_id.toStdString()).asBytes());
		CanonBlockChain<Ethash>::forceGenesisDifficulty(m_id.isEmpty() ? u256() : c_minimumDifficulty);
		CanonBlockChain<Ethash>::forceGenesisGasLimit(m_id.isEmpty() ? u256() : u256(1) << 32);
		m_aleth->web3()->stopNetwork();
		m_aleth->ethereum()->setNetworkId(sha3(m_id.toStdString()));
		m_aleth->ethereum()->reopenChain();
		emit changed(_id);
	}
}

AlethZero::AlethZero():
	ui(new Ui::Main),
	m_aleth(this),
	m_privateChain(&m_aleth)
{
	setWindowFlags(Qt::Window);
	ui->setupUi(this);

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

	connect(&m_aleth, SIGNAL(beneficiaryChanged()), SLOT(onBeneficiaryChanged()));
	m_aleth.init();

	if (c_network == eth::Network::Olympic)
		setWindowTitle("AlethZero Olympic");
	else if (c_network == eth::Network::Frontier)
		setWindowTitle("AlethZero Frontier");

	ui->blockCount->setText(QString("PV%1.%2 D%3 %4-%5 v%6").arg(eth::c_protocolVersion).arg(eth::c_minorProtocolVersion).arg(c_databaseVersion).arg(QString::fromStdString(aleth()->ethereum()->sealEngine()->name())).arg(aleth()->ethereum()->sealEngine()->revision()).arg(dev::Version));

	m_httpConnector.reset(new dev::SafeHttpServer(SensibleHttpPort, "", "", dev::SensibleHttpThreads));
	m_server.reset(new WebThreeServer(*m_httpConnector, this));
	m_server->StartListening();

	createSettingsPages();
	readSettings(true, false);
	installWatches();

	{
		QTimer* t = new QTimer(this);
		connect(t, &QTimer::timeout, [&](){ refreshMining(); });
		t->start(100);
	}
	{
		QTimer* t = new QTimer(this);
		connect(t, &QTimer::timeout, [&](){ refreshNetwork(); refreshBlockCount(); });
		t->start(1000);
	}

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

	connect(this, SIGNAL(knownAddressesChanged(AccountNamer*)), SLOT(refreshAll()));
	connect(this, SIGNAL(addressNamesChanged(AccountNamer*)), SLOT(refreshAll()));
	connect(&m_aleth, &AlethFace::keysChanged, [&](){ refreshBalances(); });

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

	killPlugins();
}

void AlethZero::createSettingsPages()
{
	m_settingsDialog = new SettingsDialog(this);
	m_settingsDialog->addPage(0, "Network", [this](){
		NetworkSettingsPage* network = new NetworkSettingsPage();
		connect(m_settingsDialog, &SettingsDialog::displayed, [this, network]() { network->setPrefs(netPrefs()); });
		connect(m_settingsDialog, &SettingsDialog::applied, [this, network]() { setNetPrefs(network->prefs()); });
		return network;
	});
}

void AlethZero::setPrivateChain(QString const& _private, bool _forceConfigure)
{
	if (m_privateChain.id() == _private && !_forceConfigure)
		return;

	// Prep UI bits.
	allStop();

	m_privateChain.setID(_private);
	ui->usePrivate->setChecked(!!m_privateChain);

	// Rejig UI bits.
	carryOn();
}

void AlethZero::on_sentinel_triggered()
{
	bool ok;
	QString sentinel = QInputDialog::getText(nullptr, "Enter sentinel address", "Enter the sentinel address for bad block reporting (e.g. http://badblockserver.com:8080). Enter nothing to disable.", QLineEdit::Normal, QString::fromStdString(aleth()->ethereum()->sentinel()), &ok);
	if (ok)
		aleth()->ethereum()->setSentinel(sentinel.toStdString());
}

void AlethZero::onKeysChanged()
{
	// TODO: reinstall balance watchers
}

void AlethZero::installWatches()
{
	auto newBlockId = aleth()->installWatch(ChainChangedFilter, [=](LocalisedLogEntries const&){
		cwatch << "Blockchain changed!";

		// update blockchain dependent views.
		refreshBlockCount();

		// We must update balances since we can't filter updates to basic accounts.
		refreshBalances();
	});

	cdebug << "newBlock watch ID: " << newBlockId;
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

void AlethZero::on_about_triggered()
{
	QMessageBox::about(this, "About " + windowTitle(), QString("AlethZero/v") + dev::Version + "/" DEV_QUOTED(ETH_BUILD_TYPE) "/" DEV_QUOTED(ETH_BUILD_PLATFORM) "\n" DEV_QUOTED(ETH_COMMIT_HASH) + (ETH_CLEAN_REPO ? "\nCLEAN" : "\n+ LOCAL CHANGES") + "\n\nBy Gav Wood and the Berlin ÐΞV team, 2014, 2015.\nSee the README for contributors and credits.");
}

void AlethZero::writeSettings()
{
	QSettings s("ethereum", "alethzero");
	s.remove("address");

	forEach([&](std::shared_ptr<Plugin> p)
	{
		p->writeSettings(s);
	});

	s.setValue("upnp", ui->upnp->isChecked());
	s.setValue("hermitMode", ui->hermitMode->isChecked());
	s.setValue("forceMining", ui->forceMining->isChecked());
	s.setValue("turboMining", ui->turboMining->isChecked());
	s.setValue("paranoia", ui->paranoia->isChecked());
	s.setValue("natSpec", ui->natSpec->isChecked());
	s.setValue("showAll", ui->showAll->isChecked());
	s.setValue("privateChain", m_privateChain.id());

	s.setValue("geometry", saveGeometry());
	s.setValue("windowState", saveState());
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

	ui->upnp->setChecked(s.value("upnp", true).toBool());
	ui->dropPeers->setChecked(false);
	ui->hermitMode->setChecked(s.value("hermitMode", false).toBool());
	ui->natSpec->setChecked(s.value("natSpec", true).toBool());
	ui->showAll->setChecked(s.value("showAll", false).toBool());
	NetworkSettings netSettings = netPrefs();
	if (netSettings.clientName.isEmpty())
		netSettings.clientName = QInputDialog::getText(nullptr, "Enter identity", "Enter a name that will identify you on the peer network");
	setNetPrefs(netSettings);
}

void AlethZero::setNetPrefs(NetworkSettings const& _settings)
{
	aleth()->web3()->setIdealPeerCount(_settings.idealPeers);
	aleth()->web3()->setNetworkPreferences(_settings.p2pSettings, _settings.dropPeers);
	aleth()->web3()->setClientVersion(WebThreeDirect::composeClientVersion("AlethZero", _settings.clientName.toStdString()));
	setPrivateChain(_settings.privateChain ? _settings.privateChainId : "");
	QSettings s("ethereum", "alethzero");
	_settings.write(s);
}

NetworkSettings AlethZero::netPrefs() const
{
	NetworkSettings settings;
	QSettings s("ethereum", "alethzero");
	settings.read(s);
	return settings;
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

	if (!password.isEmpty() && _hint && !aleth()->keyManager().haveHint(password.toStdString()))
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
		Secret s = aleth()->retrieveSecret(h);
		QMessageBox::information(this, "Export Account Key", "Secret key to account " + QString::fromStdString(aleth()->toReadable(h) + " is:\n" + s.makeInsecure().hex()));
	}
}

void AlethZero::on_confirm_triggered()
{
	web3Server()->ethAccounts()->setEnabled(ui->confirm->isChecked());
}

void AlethZero::on_preview_triggered()
{
	aleth()->ethereum()->setDefault(ui->preview->isChecked() ? PendingBlock : LatestBlock);
	refreshAll();
}

void AlethZero::refreshMining()
{
	pair<uint64_t, unsigned> gp = EthashAux::fullGeneratingProgress();
	QString t;
	if (gp.first != EthashAux::NotGenerating)
		t = QString("DAG for #%1-#%2: %3% complete; ").arg(gp.first).arg(gp.first + ETHASH_EPOCH_LENGTH - 1).arg(gp.second);
	WorkingProgress p = aleth()->ethereum()->miningProgress();
	ui->mineStatus->setText(t + (aleth()->ethereum()->isMining() ? p.hashes > 0 ? QString("%1s @ %2kH/s").arg(p.ms / 1000).arg(p.ms ? p.hashes / p.ms : 0) : "Awaiting DAG" : "Not mining"));
}

void AlethZero::onBeneficiaryChanged()
{
	Address b = aleth()->beneficiary();
	for (int i = 0; i < ui->ourAccounts->count(); ++i)
	{
		auto hba = ui->ourAccounts->item(i)->data(Qt::UserRole).toByteArray();
		auto h = Address((byte const*)hba.data(), Address::ConstructFromPointer);
		ui->ourAccounts->item(i)->setCheckState(h == b ? Qt::Checked : Qt::Unchecked);
	}
}

void AlethZero::on_ourAccounts_itemClicked(QListWidgetItem* _i)
{
	auto hba = _i->data(Qt::UserRole).toByteArray();
	aleth()->setBeneficiary(Address((byte const*)hba.data(), Address::ConstructFromPointer));
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

	auto bene = aleth()->beneficiary();
	for (auto const& address: aleth()->keyManager().accounts())
	{
		u256 b = aleth()->ethereum()->balanceAt(address);
		QListWidgetItem* li = new QListWidgetItem(
			QString("<%1> %2: %3 [%4]")
				.arg(aleth()->keyManager().haveKey(address) ? "KEY" : "BRAIN")
				.arg(QString::fromStdString(aleth()->toReadable(address)))
				.arg(formatBalance(b).c_str())
				.arg((unsigned)aleth()->ethereum()->countAt(address))
			, ui->ourAccounts);
		li->setData(Qt::UserRole, QByteArray((char const*)address.data(), Address::size));
		li->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
		li->setCheckState(bene == address ? Qt::Checked : Qt::Unchecked);
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
	auto ps = aleth()->web3()->peers();

	ui->peerCount->setText(QString::fromStdString(toString(ps.size())) + " peer(s)");
	ui->peers->clear();
	ui->nodes->clear();

	if (aleth()->web3()->haveNetwork())
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

		auto ns = aleth()->web3()->nodes();
		for (p2p::Peer const& i: ns)
			ui->nodes->insertItem(sessions.count(i.id) ? 0 : ui->nodes->count(), QString("[%1 %3] %2 - ( %4 ) - *%5")
				.arg(QString::fromStdString(i.id.abridged()))
				.arg(QString::fromStdString(i.endpoint.address.to_string()))
				.arg(i.id == aleth()->web3()->id() ? "self" : sessions.count(i.id) ? sessions[i.id] : "disconnected")
				.arg(i.isOffline() ? " | " + QString::fromStdString(reasonOf(i.lastDisconnect())) + " | " + QString::number(i.failedAttempts()) + "x" : "")
				.arg(i.rating())
				);
	}
}

void AlethZero::refreshAll()
{
	refreshBlockCount();
	refreshBalances();
	noteAllChange();
}

void AlethZero::refreshBlockCount()
{
	auto d = aleth()->ethereum()->blockChain().details();
	SyncStatus sync = aleth()->ethereum()->syncStatus();
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
		.arg(m_privateChain ? "[" + m_privateChain.id() + "] " : c_network == eth::Network::Olympic ? "Olympic" : "Frontier").arg(d.number));
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
	allStop();
	aleth()->ethereum()->clearPending();
	carryOn();
}

void AlethZero::allStop()
{
	writeSettings();
	aleth()->ethereum()->stopMining();
	ui->net->setChecked(false);
	aleth()->web3()->stopNetwork();
}

void AlethZero::carryOn()
{
	readSettings(true);
	installWatches();
	refreshAll();
}

void AlethZero::on_net_triggered()
{
	if (ui->net->isChecked())
	{
		aleth()->web3()->startNetwork();
		ui->downloadView->setEthereum(aleth()->ethereum());
		ui->enode->setText(QString::fromStdString(aleth()->web3()->enode()));
	}
	else
	{
		ui->downloadView->setEthereum(nullptr);
		writeSettings();
		aleth()->web3()->stopNetwork();
	}
}

void AlethZero::on_connect_triggered()
{
	if (!ui->net->isChecked())
	{
		ui->net->setChecked(true);
		on_net_triggered();
	}

	if (m_connect.exec() == QDialog::Accepted)
	{
		m_connect.reset();
		try
		{
			aleth()->web3()->addPeer(NodeSpec(m_connect.host().toStdString()), m_connect.required() ? PeerType::Required : PeerType::Optional);
		}
		catch (...)
		{
			QMessageBox::warning(this, "Connect to Node", "Couldn't interpret that address. Ensure you've typed it in properly and that it's in a reasonable format.", QMessageBox::Ok);
		}
	}
}

void AlethZero::on_killAccount_triggered()
{
	if (ui->ourAccounts->currentRow() >= 0)
	{
		auto hba = ui->ourAccounts->currentItem()->data(Qt::UserRole).toByteArray();
		Address h((byte const*)hba.data(), Address::ConstructFromPointer);
		QString s = QInputDialog::getText(this, QString::fromStdString("Kill Account " + aleth()->keyManager().accountName(h) + "?!"),
			QString::fromStdString("Account " + aleth()->keyManager().accountName(h) + " (" + aleth()->toReadable(h) + ") has " + formatBalance(aleth()->ethereum()->balanceAt(h)) + " in it.\r\nIt, and any contract that this account can access, will be lost forever if you continue. Do NOT continue unless you know what you are doing.\n"
			"Are you sure you want to continue? \r\n If so, type 'YES' to confirm."),
			QLineEdit::Normal, "NO");
		if (s != "YES")
			return;
		aleth()->keyManager().kill(h);
		if (aleth()->keyManager().accounts().empty())
			aleth()->keyManager().import(Secret::random(), "Default account");
		aleth()->noteKeysChanged();
		if (aleth()->beneficiary() == h)
			aleth()->setBeneficiary(aleth()->keyManager().accounts().front());
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
				auto p = QInputDialog::getText(this, "Re-Encrypt Key", "Enter the original password for this key.\nHint: " + QString::fromStdString(aleth()->keyManager().passwordHint(a)), QLineEdit::Password, QString()).toStdString();
				if (p.empty())
					throw PasswordUnknown();
				return p;
			};
			while (!(password.empty() ? aleth()->keyManager().recode(a, SemanticPassword::Master, pw, kdf) : aleth()->keyManager().recode(a, password, hint, pw, kdf)))
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
		for (Address const& a: aleth()->keyManager().accounts())
			while (!aleth()->keyManager().recode(a, SemanticPassword::Existing, [&](){
				auto p = QInputDialog::getText(nullptr, "Re-Encrypt Key", QString("Enter the original password for key %1.\nHint: %2").arg(QString::fromStdString(aleth()->toName(a))).arg(QString::fromStdString(aleth()->keyManager().passwordHint(a))), QLineEdit::Password, QString()).toStdString();
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
		aleth()->web3()->requirePeer(i.first, i.second);
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

void AlethZero::addSettingsPage(int _index, QString const& _categoryName, std::function<SettingsPage*()> const& _pageFactory)
{
	m_settingsDialog->addPage(_index, _categoryName, _pageFactory);
}

void AlethZero::on_settings_triggered()
{
	m_settingsDialog->exec();
}
