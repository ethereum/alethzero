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
/** @file AllAccounts.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#include "AllAccounts.h"
#include <sstream>
#include <QClipboard>
#include <libdevcore/Log.h>
#include <libdevcore/SHA3.h>
#include <libevmcore/Instruction.h>
#include <libethcore/ICAP.h>
#include <libethcore/KeyManager.h>
#include <libethereum/Client.h>
#include "ConfigInfo.h"
#include <libaleth/AlethFace.h>
#include "ZeroFace.h"
#include "ui_AllAccounts.h"
using namespace std;
using namespace dev;
using namespace eth;
using namespace aleth;
using namespace zero;

static inline QString deletedButtonName(AccountData const& _account)
{
	return _account.isDeleted ? "Undo" : "Delete";
}

static inline QString displayName(AccountData const& _account)
{
	return _account.isDeleted ? _account.name + " [Deleted]" : _account.name;
}

ZERO_NOTE_PLUGIN(AllAccounts);

AllAccounts::AllAccounts(ZeroFace* _m):
	Plugin(_m, "AllAccounts")
{
	connect(addMenuItem("All Accounts", "menuAccounts", true), SIGNAL(triggered()), SLOT(create()));
}

AllAccounts::~AllAccounts()
{
}

void AllAccounts::installWatches()
{
	aleth()->installWatch(ChainChangedFilter, [=](LocalisedLogEntries const&){ onAllChange(); });
	aleth()->installWatch(PendingChangedFilter, [=](LocalisedLogEntries const&){ onAllChange(); });

	connect(m_ui->accounts, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), SLOT(on_accounts_currentItemChanged()));
	connect(m_ui->accounts, SIGNAL(doubleClicked(QModelIndex)), SLOT(on_accounts_doubleClicked()));

	connect(m_ui->showBasic, &QCheckBox::toggled, [this]() { refresh(); });
	connect(m_ui->showContracts, &QCheckBox::toggled, [this]() { refresh(); });
	connect(m_ui->onlyKnown, &QCheckBox::toggled, [this]() { refresh(); });
	connect(m_ui->accountsFilter, &QLineEdit::textChanged, [this]() { filterAndUpdateUi(); });
	
	auto copyText = [](QLabel* _label)
	{
		qApp->clipboard()->setText(_label->text());
	};

	connect(m_ui->addressCopy, &QPushButton::clicked, [&] () { copyText(m_ui->addressValue); });
	connect(m_ui->ICAPCopy, &QPushButton::clicked, [&] () { copyText(m_ui->ICAPValue); });
	connect(m_ui->codeCopy, &QPushButton::clicked, [&] () { copyText(m_ui->codeValue); });
}

void AllAccounts::create()
{
	m_ui = new Ui::AllAccounts;
	QDialog d;
	m_ui->setupUi(&d);
	d.setWindowTitle("All Accounts");

	installWatches();
	m_ui->detailsView->setVisible(false);
	QPixmap pixmap("/Users/marek/projects/umbrella/webthree-umbrella/mix/qml/img/clipboard_copy.png");
	m_ui->addressCopy->setIcon(pixmap);
	m_ui->ICAPCopy->setIcon(pixmap);
	m_ui->codeCopy->setIcon(pixmap);

	refresh();

	if (d.exec() == QDialog::Accepted)
		save();

	delete m_ui;
}

void AllAccounts::save()
{
	bool deletedBeneficiary = false;
	for (auto const& account: m_accounts)
	{
		if (account.second.isDeleted)
		{
			aleth()->keyManager().kill(account.first);
			deletedBeneficiary |= aleth()->beneficiary() == account.first;
		}
	}

	if (aleth()->keyManager().accounts().empty())
		aleth()->keyManager().import(Secret::random(), "Default account");
	if (deletedBeneficiary)
		aleth()->setBeneficiary(aleth()->keyManager().accounts().front());
	aleth()->noteKeysChanged();
}

void AllAccounts::refresh()
{
	DEV_TIMED_FUNCTION;
	bool showContract = m_ui->showContracts->isChecked();
	bool showBasic = m_ui->showBasic->isChecked();
	bool onlyKnown = m_ui->onlyKnown->isChecked();
	m_ui->detailsView->setVisible(false);

	Addresses as;
	if (onlyKnown)
		as = aleth()->allKnownAddresses();
#if ETH_FATDB || !ETH_TRUE
	else
		as = ethereum()->addresses();
#endif

	std::map<Address, AccountData> newAccounts;
	for (auto const& i: as)
	{
		bool isContract = (ethereum()->codeHashAt(i) != EmptySHA3);
		if (!((showContract && isContract) || (showBasic && !isContract)))
			continue;

		if (m_accounts.count(i))
		{
			newAccounts[i] = m_accounts[i];
			continue;
		}
		
		AccountData data;
		data.address = i;
		data.name = QString::fromStdString(aleth()->toName(i));
		data.isContract = isContract;
		data.isVisible = true; // TODO: fix this
		newAccounts[i] = data;
	}
	m_accounts = newAccounts;
	
	filterAndUpdateUi();
}

void AllAccounts::filterAndUpdateUi()
{
	m_ui->accounts->clear();
	QString filterString = m_ui->accountsFilter->text();
	for (auto& account: m_accounts)
	{
		if (!account.second.name.contains(filterString, Qt::CaseSensitivity::CaseInsensitive))
			continue;

		QListWidgetItem* li = new QListWidgetItem(displayName(account.second), m_ui->accounts);
		li->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
		li->setData(Qt::UserRole, QByteArray((char const*)account.first.data(), Address::size));
		li->setCheckState(account.second.isVisible ? Qt::Checked : Qt::Unchecked);
	}
	m_ui->accounts->sortItems();
}

void AllAccounts::on_accounts_currentItemChanged()
{
	m_ui->detailsView->setVisible(false);
	if (auto item = m_ui->accounts->currentItem())
	{
		m_ui->detailsView->setVisible(true);
		auto hba = item->data(Qt::UserRole).toByteArray();
		assert(hba.size() == 20);
		auto address = h160((byte const*)hba.data(), h160::ConstructFromPointer);
		try
		{
			AccountData account = m_accounts.at(address);

			m_ui->codeLabel->setVisible(account.isContract);
			m_ui->codeValue->setVisible(account.isContract);
			m_ui->codeCopy->setVisible(account.isContract);
			m_ui->defaultLabel->setVisible(!account.isContract);
			m_ui->defaultValue->setVisible(!account.isContract);

			m_ui->nameValue->setText(account.name);
			m_ui->addressValue->setText(QString::fromStdString(toHex(address.ref())));
			m_ui->ICAPValue->setText(QString::fromStdString(ICAP(address).encoded()));
			m_ui->balanceValue->setText(QString::fromStdString(formatBalance(ethereum()->balanceAt(address))));
			m_ui->codeValue->setText(QString::fromStdString(toHex(ethereum()->codeAt(address))));
			m_ui->deleteButton->setText(deletedButtonName(account));

			m_ui->deleteButton->disconnect();
			connect(m_ui->deleteButton, &QPushButton::clicked, [this, address, item] ()
			{
				AccountData a = m_accounts.at(address);
				a.isDeleted = !a.isDeleted;
				m_ui->deleteButton->setText(deletedButtonName(a));
				m_accounts[address] = a;
				item->setText(displayName(a));
			});
		}
		catch (dev::InvalidTrie)
		{
			//			m_ui->accountInfo->appendHtml("Corrupted trie.");
		}
	}
}

void AllAccounts::on_accounts_doubleClicked()
{
	if (m_ui->accounts->count())
	{
		auto hba = m_ui->accounts->currentItem()->data(Qt::UserRole).toByteArray();
		auto h = Address((byte const*)hba.data(), Address::ConstructFromPointer);
		qApp->clipboard()->setText(QString::fromStdString(toHex(h.asArray())));
	}
}
