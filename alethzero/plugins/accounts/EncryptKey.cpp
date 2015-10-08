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
/** @file EncryptKey.h
 * @author Marek Kotewicz <marek@ethdev.com>
 * @date 2015
 */

#include "EncryptKey.h"
#include <QMenu>
#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <libdevcore/Log.h>
#include <libethcore/KeyManager.h>
#include <libethereum/Client.h>
#include <libaleth/AlethFace.h>
#include "ZeroFace.h"
#include "ui_EncryptKey.h"
using namespace std;
using namespace dev;
using namespace eth;
using namespace aleth;
using namespace zero;

ZERO_NOTE_PLUGIN(EncryptKey);

EncryptKey::EncryptKey(ZeroFace* _m):
	Plugin(_m, "EncryptKey")
{
	connect(addMenuItem("Re-Encrypt Key", "menuAccounts", true), SIGNAL(triggered()), SLOT(create()));
}

EncryptKey::~EncryptKey()
{
}

void EncryptKey::create()
{
	m_ui = new Ui::EncryptKey;
	QDialog d;
	m_ui->setupUi(&d);

	m_ui->keysPath->setText(QString::fromStdString(getDataDir("web3/keys")));

	Addresses as = aleth()->allKnownAddresses();
	for (auto const& i: as)
	{
		bool isContract = (ethereum()->codeHashAt(i) != EmptySHA3);
		if (isContract)
			continue;
		m_ui->accounts->addItem(QString::fromStdString(aleth()->toName(i)), QByteArray((char const*)i.data(), Address::size));
	}
	m_ui->accounts->model()->sort(0);
	
	auto updateStatus = [&]()
	{
		int index = m_ui->passwordType->currentIndex();
		bool useMaster = index == 0;
		m_ui->password->setEnabled(!useMaster);
		m_ui->confirm->setEnabled(!useMaster);
		
		auto data = m_ui->accounts->currentData();
		auto hba = data.toByteArray();
		assert(hba.size() == 20);
		auto address = h160((byte const*)hba.data(), h160::ConstructFromPointer);
		QString hint = QString::fromStdString(aleth()->keyManager().passwordHint(address));
		m_ui->oldPassword->setToolTip(!hint.isEmpty() ? hint : "-");

		if (useMaster)
		{
			m_ui->change->setEnabled(true);
			return;
		}

		PasswordValidation v = validatePassword(m_ui->password->text().toStdString(), m_ui->confirm->text().toStdString());
		m_ui->status->setText(QString::fromStdString(errorDescription(v)));
		m_ui->change->setEnabled(v == PasswordValidation::Valid);
	};
	
	updateStatus();
	void (QComboBox::*currentItemChanged)(int) = &QComboBox::currentIndexChanged;
	connect(m_ui->passwordType, currentItemChanged, updateStatus);
	connect(m_ui->accounts, currentItemChanged, updateStatus);
	connect(m_ui->oldPassword, &QLineEdit::textChanged, updateStatus);
	connect(m_ui->password, &QLineEdit::textChanged, updateStatus);
	connect(m_ui->confirm, &QLineEdit::textChanged, updateStatus);

	connect(m_ui->change, &QPushButton::pressed, [&]()
	{
		auto data = m_ui->accounts->currentData();
		auto hba = data.toByteArray();
		auto address = h160((byte const*)hba.data(), h160::ConstructFromPointer);
		bool ok = aleth()->keyManager().validatePassword(address, m_ui->oldPassword->text().toStdString());
		if (ok)
			d.accept();
		else
		{
			m_ui->status->setText("Old password is incorrect");
			m_ui->change->setEnabled(false);
		}
	});
	
	if (d.exec() == QDialog::Accepted)
		accepted();
}

void EncryptKey::accepted()
{
	auto data = m_ui->accounts->currentData();
	auto hba = data.toByteArray();
	auto address = h160((byte const*)hba.data(), h160::ConstructFromPointer);
	
	int index = m_ui->passwordType->currentIndex();
	if (index != 0)
	{
		aleth()->keyManager().recode(address,
									 m_ui->password->text().toStdString(),
									 m_ui->hint->toPlainText().toStdString(),
									 [this](){ return m_ui->oldPassword->text().toStdString(); },
									 index == 1 ? KDF::Scrypt : KDF::PBKDF2_SHA256);
	}
	else // use master password
		aleth()->keyManager().recode(address,
									 SemanticPassword::Master,
									 [this](){ return m_ui->oldPassword->text().toStdString(); },
									 KDF::Scrypt);
}
