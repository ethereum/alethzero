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
#include <libaleth/SyncView.h>
#include <libaleth/WebThreeServer.h>
#include "Connect.h"
#include "alethzero/BuildInfo.h"
#include "SettingsDialog.h"
#include "NetworkSettings.h"
#include "ui_AlethZero.h"
using namespace std;
using namespace dev;
using namespace aleth;
using namespace zero;
using namespace p2p;
using namespace eth;

AlethZero::AlethZero():
	m_ui(new Ui::Main),
	m_aleth(this)
{
	setWindowFlags(Qt::Window);
	m_ui->setupUi(this);

//	statusBar()->addPermanentWidget(m_ui->cacheUsage);
	m_ui->cacheUsage->hide();
	statusBar()->addPermanentWidget(m_ui->balance);
	statusBar()->addPermanentWidget(m_ui->peerCount);
	statusBar()->addPermanentWidget(m_ui->mineStatus);
	statusBar()->addPermanentWidget(m_ui->syncStatus);
	statusBar()->addPermanentWidget(m_ui->chainStatus);
	statusBar()->addPermanentWidget(m_ui->blockCount);

	m_aleth.init(Aleth::OpenOnly, "AlethZero", "anon");
	m_rpcHost.init(&m_aleth);

	cerr << "State root: " << CanonBlockChain<Ethash>::genesis().stateRoot() << endl;
	auto block = CanonBlockChain<Ethash>::createGenesisBlock();
	cerr << "Block Hash: " << CanonBlockChain<Ethash>::genesis().hash() << endl;
	cerr << "Block RLP: " << RLP(block) << endl;
	cerr << "Block Hex: " << toHex(block) << endl;
	cerr << "eth Network protocol version: " << eth::c_protocolVersion << endl;
	cerr << "Client database version: " << c_databaseVersion << endl;

	if (c_network == eth::Network::Olympic)
		setWindowTitle("AlethZero Olympic");
	else if (c_network == eth::Network::Frontier)
		setWindowTitle("AlethZero Frontier");

	m_ui->blockCount->setText(QString("PV%1.%2 D%3 %4-%5 v%6").arg(eth::c_protocolVersion).arg(eth::c_minorProtocolVersion).arg(c_databaseVersion).arg(QString::fromStdString(aleth()->ethereum()->sealEngine()->name())).arg(aleth()->ethereum()->sealEngine()->revision()).arg(dev::Version));

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

	for (auto const& i: *s_linkedPlugins)
	{
		cdebug << "Initialising plugin:" << i.first;
		initPlugin(i.second(this));
	}

	connect(aleth(), SIGNAL(knownAddressesChanged(AccountNamer*)), SLOT(refreshAll()));
	connect(aleth(), SIGNAL(addressNamesChanged(AccountNamer*)), SLOT(refreshAll()));

	readSettings(false, true);
}

AlethZero::~AlethZero()
{
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

void AlethZero::allStop()
{
	writeSettings();
	aleth()->ethereum()->stopMining();
	m_ui->net->setChecked(false);
	aleth()->web3()->stopNetwork();
}

void AlethZero::carryOn()
{
	readSettings(true);
	installWatches();
	refreshAll();
}

void AlethZero::installWatches()
{
	auto newBlockId = aleth()->installWatch(ChainChangedFilter, [=](LocalisedLogEntries const&){
		cwatch << "Blockchain changed!";

		// update blockchain dependent views.
		refreshBlockCount();
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

	s.setValue("geometry", saveGeometry());
	s.setValue("windowState", saveState());
}

void AlethZero::readSettings(bool _skipGeometry, bool _onlyGeometry)
{
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
	}
	NetworkSettings netSettings = netPrefs();
	if (netSettings.clientName.isEmpty())
		netSettings.clientName = QInputDialog::getText(nullptr, "Enter identity", "Enter a name that will identify you on the peer network");
	setNetPrefs(netSettings);
}

void AlethZero::setNetPrefs(NetworkSettings const& _settings)
{
	aleth()->web3()->setIdealPeerCount(_settings.idealPeers);
	auto p = _settings.p2pSettings;
	p.discovery = p.discovery && !CanonBlockChain<Ethash>::isNonStandard();
	p.pin = p.pin || CanonBlockChain<Ethash>::isNonStandard();
	aleth()->web3()->setNetworkPreferences(p, _settings.dropPeers);
	aleth()->web3()->setClientVersion(WebThreeDirect::composeClientVersion("AlethZero", _settings.clientName.toStdString()));
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

void AlethZero::onKeysChanged()
{
	// TODO: reinstall balance watchers
}

void AlethZero::on_confirm_triggered()
{
	web3Server()->ethAccounts()->setEnabled(m_ui->confirm->isChecked());
}

void AlethZero::on_preview_triggered()
{
	aleth()->ethereum()->setDefault(m_ui->preview->isChecked() ? PendingBlock : LatestBlock);
	refreshAll();
}

void AlethZero::refreshMining()
{
	pair<uint64_t, unsigned> gp = EthashAux::fullGeneratingProgress();
	QString t;
	if (gp.first != EthashAux::NotGenerating)
		t = QString("DAG for #%1-#%2: %3% complete; ").arg(gp.first).arg(gp.first + ETHASH_EPOCH_LENGTH - 1).arg(gp.second);
	WorkingProgress p = aleth()->ethereum()->miningProgress();
	m_ui->mineStatus->setText(t + (aleth()->ethereum()->isMining() ? p.hashes > 0 ? QString("%1s @ %2kH/s").arg(p.ms / 1000).arg(p.ms ? p.hashes / p.ms : 0) : "Awaiting DAG" : "Not mining"));
}

void AlethZero::refreshNetwork()
{
	auto ps = aleth()->web3()->peers();

	m_ui->peerCount->setText(QString::fromStdString(toString(ps.size())) + " peer(s)");
	m_ui->peers->clear();
	m_ui->nodes->clear();

	if (aleth()->web3()->haveNetwork())
	{
		map<h512, QString> sessions;
		for (PeerSessionInfo const& i: ps)
			m_ui->peers->addItem(QString("[%8 %7] %3 ms - %1:%2 - %4 %5 %6")
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
			m_ui->nodes->insertItem(sessions.count(i.id) ? 0 : m_ui->nodes->count(), QString("[%1 %3] %2 - ( %4 ) - *%5")
				.arg(QString::fromStdString(i.id.abridged()))
				.arg(QString::fromStdString(i.endpoint.address.to_string()))
				.arg(i.id == aleth()->web3()->id() ? "self" : sessions.count(i.id) ? sessions[i.id] : "disconnected")
				.arg(i.isOffline() ? " | " + QString::fromStdString(reasonOf(i.lastDisconnect())) + " | " + QString::number(i.failedAttempts()) + "x" : "")
				.arg(i.rating())
				);
	}
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
	m_ui->syncStatus->setText(syncStatus);
//	BlockQueueStatus b = ethereum()->blockQueueStatus();
//	m_ui->chainStatus->setText(QString("%3 importing %4 ready %5 verifying %6 unverified %7 future %8 unknown %9 bad  %1 #%2")
//		.arg(m_privateChain.size() ? "[" + m_privateChain + "] " : c_network == eth::Network::Olympic ? "Olympic" : "Frontier").arg(d.number).arg(b.importing).arg(b.verified).arg(b.verifying).arg(b.unverified).arg(b.future).arg(b.unknown).arg(b.bad));
	m_ui->chainStatus->setText(QString("%1 #%2")
		.arg(/*m_privateChain ? "[" + m_privateChain.id() + "] " :*/ c_network == eth::Network::Olympic ? "Olympic" : c_network == eth::Network::Morden ? "Morden" : "Frontier").arg(d.number));		// TODO: some way for the plugin to display this
}

void AlethZero::refreshAll()
{
	refreshBlockCount();
	noteAllChange();
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
	m_ui->cacheUsage->setText(t);
}
*/


//////////////////////// NETWORK MANAGEMENT /////////////////////////////////

void AlethZero::on_go_triggered()
{
	if (!m_ui->net->isChecked())
	{
		m_ui->net->setChecked(true);
		on_net_triggered();
	}
	for (auto const& i: Host::pocHosts())
		aleth()->web3()->requirePeer(i.first, i.second);
}

void AlethZero::on_net_triggered()
{
	if (m_ui->net->isChecked())
	{
		aleth()->web3()->startNetwork();
		m_ui->downloadView->setAleth(aleth());
		m_ui->enode->setText(QString::fromStdString(aleth()->web3()->enode()));
	}
	else
	{
		m_ui->downloadView->setAleth(nullptr);
		writeSettings();
		aleth()->web3()->stopNetwork();
	}
}

void AlethZero::on_connect_triggered()
{
	if (!m_ui->net->isChecked())
	{
		m_ui->net->setChecked(true);
		on_net_triggered();
	}

	Connect m_connect(this);
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


//////////////////////// ACCOUNT MANAGEMENT /////////////////////////////////

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

void AlethZero::addSettingsPage(int _index, QString const& _categoryName, std::function<SettingsPage*()> const& _pageFactory)
{
	m_settingsDialog->addPage(_index, _categoryName, _pageFactory);
}

void AlethZero::on_settings_triggered()
{
	if (m_settingsDialog->exec() == QDialog::Accepted)
		writeSettings();
}
