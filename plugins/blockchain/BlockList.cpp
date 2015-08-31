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
/** @file BlockList.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#include "BlockList.h"
#include <fstream>
#include <QFileDialog>
#include <QTimer>
#include <libdevcore/Log.h>
#include <libethereum/Client.h>
#include <libethcore/EthashAux.h>
#include "Debugger.h"
#include "ConfigInfo.h"
#include "ui_BlockList.h"
using namespace std;
using namespace dev;
using namespace az;
using namespace eth;

DEV_AZ_NOTE_PLUGIN(BlockList);

BlockList::BlockList(MainFace* _m):
	Plugin(_m, "Blockchain"),
	m_ui(new Ui::BlockList)
{
	dock(Qt::RightDockWidgetArea, "Blockchain")->setWidget(new QWidget());
	m_ui->setupUi(dock()->widget());
	connect(dock(), SIGNAL(visibilityChanged(bool)), SLOT(refreshBlocks()));
	connect(m_ui->blocks, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), SLOT(refreshInfo()));
	connect(m_ui->debugCurrent, SIGNAL(pressed()), SLOT(debugCurrent()));
	connect(m_ui->dumpPre, &QPushButton::pressed, [&](){ dumpState(false); });
	connect(m_ui->dumpPost, &QPushButton::pressed, [&](){ dumpState(true); });
	connect(m_ui->filter, SIGNAL(textChanged(QString)), SLOT(filterChanged()));

	m_newBlockWatch = main()->installWatch(ChainChangedFilter, [=](LocalisedLogEntries const&){
		refreshBlocks();
	});
	refreshBlocks();
}

BlockList::~BlockList()
{
	main()->uninstallWatch(m_newBlockWatch);
}

void BlockList::filterChanged()
{
	static QTimer* s_delayed = nullptr;
	if (!s_delayed)
	{
		s_delayed = new QTimer(this);
		s_delayed->setSingleShot(true);
		connect(s_delayed, SIGNAL(timeout()), SLOT(refreshBlocks()));
	}
	s_delayed->stop();
	s_delayed->start(200);
}

void BlockList::refreshBlocks()
{
	if (!dock()->isVisible())// || !tabifiedDockWidgets(m_ui->blockChainDockWidget).isEmpty()))
		return;

	DEV_TIMED_FUNCTION_ABOVE(500);
	cwatch << "refreshBlockChain()";

	// TODO: keep the same thing highlighted.
	// TODO: refactor into MVC
	// TODO: use get by hash/number
	// TODO: transactions

	auto const& bc = ethereum()->blockChain();
	QStringList filters = m_ui->filter->text().toLower().split(QRegExp("\\s+"), QString::SkipEmptyParts);

	h256Hash blocks;
	for (QString f: filters)
		if (f.size() == 64)
		{
			h256 h(f.toStdString());
			if (bc.isKnown(h))
				blocks.insert(h);
			for (auto const& b: bc.withBlockBloom(LogBloom().shiftBloom<3>(sha3(h)), 0, -1))
				blocks.insert(bc.numberHash(b));
		}
		else if (f.toLongLong() <= bc.number())
			blocks.insert(bc.numberHash((unsigned)f.toLongLong()));
		else if (f.size() == 40)
		{
			Address h(f.toStdString());
			for (auto const& b: bc.withBlockBloom(LogBloom().shiftBloom<3>(sha3(h)), 0, -1))
				blocks.insert(bc.numberHash(b));
		}

	QByteArray oldSelected = m_ui->blocks->count() ? m_ui->blocks->currentItem()->data(Qt::UserRole).toByteArray() : QByteArray();
	m_ui->blocks->clear();
	auto showBlock = [&](h256 const& h) {
		auto d = bc.details(h);
		QListWidgetItem* blockItem = new QListWidgetItem(QString("#%1 %2").arg(d.number).arg(h.abridged().c_str()), m_ui->blocks);
		auto hba = QByteArray((char const*)h.data(), h.size);
		blockItem->setData(Qt::UserRole, hba);
		if (oldSelected == hba)
			blockItem->setSelected(true);

		int n = 0;
		try {
			auto b = bc.block(h);
			for (auto const& i: RLP(b)[1])
			{
				Transaction t(i.data(), CheckTransaction::Everything);
				QString s = t.receiveAddress() ?
					QString("    %2 %5> %3: %1 [%4]")
						.arg(formatBalance(t.value()).c_str())
						.arg(QString::fromStdString(main()->render(t.safeSender())))
						.arg(QString::fromStdString(main()->render(t.receiveAddress())))
						.arg((unsigned)t.nonce())
						.arg(ethereum()->codeAt(t.receiveAddress()).size() ? '*' : '-') :
					QString("    %2 +> %3: %1 [%4]")
						.arg(formatBalance(t.value()).c_str())
						.arg(QString::fromStdString(main()->render(t.safeSender())))
						.arg(QString::fromStdString(main()->render(right160(sha3(rlpList(t.safeSender(), t.nonce()))))))
						.arg((unsigned)t.nonce());
				QListWidgetItem* txItem = new QListWidgetItem(s, m_ui->blocks);
				auto hba = QByteArray((char const*)h.data(), h.size);
				txItem->setData(Qt::UserRole, hba);
				txItem->setData(Qt::UserRole + 1, n);
				if (oldSelected == hba)
					txItem->setSelected(true);
				n++;
			}
		}
		catch (...) {}
	};

	if (filters.empty())
	{
		unsigned i = 10;
		for (auto h = bc.currentHash(); bc.details(h) && i; h = bc.details(h).parent, --i)
		{
			showBlock(h);
			if (h == bc.genesisHash())
				break;
		}
	}
	else
		for (auto const& h: blocks)
			showBlock(h);

	if (!m_ui->blocks->currentItem())
		m_ui->blocks->setCurrentRow(0);
}

void BlockList::refreshInfo()
{
	m_ui->info->clear();
	m_ui->debugCurrent->setEnabled(false);
	if (auto item = m_ui->blocks->currentItem())
	{
		auto hba = item->data(Qt::UserRole).toByteArray();
		assert(hba.size() == 32);
		auto h = h256((byte const*)hba.data(), h256::ConstructFromPointer);
		auto details = ethereum()->blockChain().details(h);
		auto blockData = ethereum()->blockChain().block(h);
		auto block = RLP(blockData);
		Ethash::BlockHeader info(blockData);

		stringstream s;

		if (item->data(Qt::UserRole + 1).isNull())
		{
			char timestamp[64];
			time_t rawTime = (time_t)(uint64_t)info.timestamp();
			strftime(timestamp, 64, "%c", localtime(&rawTime));
			s << "<h3>" << h << "</h3>";
			s << "<h4>#" << info.number();
			s << "&nbsp;&emsp;&nbsp;<b>" << timestamp << "</b></h4>";
			try
			{
				RLP r(info.extraData());
				if (r[0].toInt<int>() == 0)
					s << "<div>Sealing client: <b>" << htmlEscaped(r[1].toString()) << "</b>" << "</div>";
			}
			catch (...) {}
			s << "<div>D/TD: <b>" << info.difficulty() << "</b>/<b>" << details.totalDifficulty << "</b> = 2^" << log2((double)info.difficulty()) << "/2^" << log2((double)details.totalDifficulty) << "</div>";
			s << "&nbsp;&emsp;&nbsp;Children: <b>" << details.children.size() << "</b></div>";
			s << "<div>Gas used/limit: <b>" << info.gasUsed() << "</b>/<b>" << info.gasLimit() << "</b>" << "</div>";
			s << "<div>Beneficiary: <b>" << htmlEscaped(main()->pretty(info.beneficiary())) << " " << info.beneficiary() << "</b>" << "</div>";
			s << "<div>Seed hash: <b>" << info.seedHash() << "</b>" << "</div>";
			s << "<div>Mix hash: <b>" << info.mixHash() << "</b>" << "</div>";
			s << "<div>Nonce: <b>" << info.nonce() << "</b>" << "</div>";
			s << "<div>Hash w/o nonce: <b>" << info.hashWithout() << "</b>" << "</div>";
			s << "<div>Difficulty: <b>" << info.difficulty() << "</b>" << "</div>";
			if (info.number())
			{
				auto e = EthashAux::eval(info.seedHash(), info.hashWithout(), info.nonce());
				s << "<div>Proof-of-Work: <b>" << e.value << " &lt;= " << (h256)u256((bigint(1) << 256) / info.difficulty()) << "</b> (mixhash: " << e.mixHash.abridged() << ")" << "</div>";
				s << "<div>Parent: <b>" << info.parentHash() << "</b>" << "</div>";
			}
			else
			{
				s << "<div>Proof-of-Work: <b><i>Phil has nothing to prove</i></b></div>";
				s << "<div>Parent: <b><i>It was a virgin birth</i></b></div>";
			}
			s << "<div>State root: " << ETH_HTML_SPAN(ETH_HTML_MONO) << info.stateRoot().hex() << "</span></div>";
			s << "<div>Extra data: " << ETH_HTML_SPAN(ETH_HTML_MONO) << toHex(info.extraData()) << "</span></div>";
			if (!!info.logBloom())
				s << "<div>Log Bloom: " << info.logBloom() << "</div>";
			else
				s << "<div>Log Bloom: <b><i>Uneventful</i></b></div>";
			s << "<div>Transactions: <b>" << block[1].itemCount() << "</b> @<b>" << info.transactionsRoot() << "</b>" << "</div>";
			s << "<div>Uncles: <b>" << block[2].itemCount() << "</b> @<b>" << info.sha3Uncles() << "</b>" << "</div>";
			for (auto u: block[2])
			{
				Ethash::BlockHeader uncle(u.data(), CheckNothing, h256(), HeaderData);
				char const* line = "<div><span style=\"margin-left: 2em\">&nbsp;</span>";
				s << line << "Hash: <b>" << uncle.hash() << "</b>" << "</div>";
				s << line << "Parent: <b>" << uncle.parentHash() << "</b>" << "</div>";
				s << line << "Number: <b>" << uncle.number() << "</b>" << "</div>";
				s << line << "Coinbase: <b>" << htmlEscaped(main()->pretty(uncle.beneficiary())) << " " << uncle.beneficiary() << "</b>" << "</div>";
				s << line << "Seed hash: <b>" << uncle.seedHash() << "</b>" << "</div>";
				s << line << "Mix hash: <b>" << uncle.mixHash() << "</b>" << "</div>";
				s << line << "Nonce: <b>" << uncle.nonce() << "</b>" << "</div>";
				s << line << "Hash w/o nonce: <b>" << uncle.headerHash(WithoutProof) << "</b>" << "</div>";
				s << line << "Difficulty: <b>" << uncle.difficulty() << "</b>" << "</div>";
				auto e = EthashAux::eval(uncle.seedHash(), uncle.hashWithout(), uncle.nonce());
				s << line << "Proof-of-Work: <b>" << e.value << " &lt;= " << (h256)u256((bigint(1) << 256) / uncle.difficulty()) << "</b> (mixhash: " << e.mixHash.abridged() << ")" << "</div>";
			}
			if (info.parentHash())
				s << "<div>Pre: <b>" << BlockInfo(ethereum()->blockChain().block(info.parentHash())).stateRoot() << "</b>" << "</div>";
			else
				s << "<div>Pre: <b><i>Nothing is before Phil</i></b>" << "</div>";

			s << "<div>Receipts: @<b>" << info.receiptsRoot() << "</b>:" << "</div>";
			BlockReceipts receipts = ethereum()->blockChain().receipts(h);
			unsigned ii = 0;
			for (auto const& i: block[1])
			{
				s << "<div>" << sha3(i.data()).abridged() << ": <b>" << receipts.receipts[ii].stateRoot() << "</b> [<b>" << receipts.receipts[ii].gasUsed() << "</b> used]" << "</div>";
				++ii;
			}
			s << "<div>Post: <b>" << info.stateRoot() << "</b>" << "</div>";
			s << "<div>Dump: " ETH_HTML_SPAN(ETH_HTML_MONO) << toHex(block[0].data()) << "</span>" << "</div>";
			s << "<div>Receipts-Hex: " ETH_HTML_SPAN(ETH_HTML_MONO) << toHex(receipts.rlp()) << "</span></div>";
		}
		else
		{
			unsigned txi = item->data(Qt::UserRole + 1).toInt();
			Transaction tx(block[1][txi].data(), CheckTransaction::Everything);
			auto ss = tx.safeSender();
			h256 th = sha3(rlpList(ss, tx.nonce()));
			TransactionReceipt receipt = ethereum()->blockChain().receipts(h).receipts[txi];
			s << "<h3>" << th << "</h3>";
			s << "<h4>" << h << "[<b>" << txi << "</b>]</h4>";
			s << "<div>From: <b>" << htmlEscaped(main()->pretty(ss)) << " " << ss << "</b>" << "</div>";
			if (tx.isCreation())
				s << "<div>Creates: <b>" << htmlEscaped(main()->pretty(right160(th))) << "</b> " << right160(th) << "</div>";
			else
				s << "<div>To: <b>" << htmlEscaped(main()->pretty(tx.receiveAddress())) << "</b> " << tx.receiveAddress() << "</div>";
			s << "<div>Value: <b>" << formatBalance(tx.value()) << "</b>" << "</div>";
			s << "&nbsp;&emsp;&nbsp;#<b>" << tx.nonce() << "</b>" << "</div>";
			s << "<div>Gas price: <b>" << formatBalance(tx.gasPrice()) << "</b>" << "</div>";
			s << "<div>Gas: <b>" << tx.gas() << "</b>" << "</div>";
			s << "<div>V: <b>" << hex << nouppercase << (int)tx.signature().v << " + 27</b>" << "</div>";
			s << "<div>R: <b>" << hex << nouppercase << tx.signature().r << "</b>" << "</div>";
			s << "<div>S: <b>" << hex << nouppercase << tx.signature().s << "</b>" << "</div>";
			s << "<div>Msg: <b>" << tx.sha3(eth::WithoutSignature) << "</b>" << "</div>";
			if (!tx.data().empty())
			{
				if (tx.isCreation())
					s << "<h4>Code</h4>" << disassemble(tx.data());
				else
					s << "<h4>Data</h4>" << dev::memDump(tx.data(), 16, true);
			}
			s << "<div>Hex: " ETH_HTML_SPAN(ETH_HTML_MONO) << toHex(block[1][txi].data()) << "</span></div>";
			s << "<hr/>";
			if (!!receipt.bloom())
				s << "<div>Log Bloom: " << receipt.bloom() << "</div>";
			else
				s << "<div>Log Bloom: <b><i>Uneventful</i></b></div>";
			s << "<div>Gas Used: <b>" << receipt.gasUsed() << "</b></div>";
			s << "<div>End State: <b>" << receipt.stateRoot().abridged() << "</b></div>";
			auto r = receipt.rlp();
			s << "<div>Receipt: " << toString(RLP(r)) << "</div>";
			s << "<div>Receipt-Hex: " ETH_HTML_SPAN(ETH_HTML_MONO) << toHex(receipt.rlp()) << "</span></div>";
			s << "<h4>Diff</h4>" << main()->renderDiff(ethereum()->diff(txi, h));

			m_ui->debugCurrent->setEnabled(true);
		}

		m_ui->info->appendHtml(QString::fromStdString(s.str()));
		m_ui->info->moveCursor(QTextCursor::Start);
	}
}

void BlockList::debugCurrent()
{
	if (QListWidgetItem* item = m_ui->blocks->currentItem())
	{
		auto hba = item->data(Qt::UserRole).toByteArray();
		assert(hba.size() == 32);
		auto h = h256((byte const*)hba.data(), h256::ConstructFromPointer);

		if (!item->data(Qt::UserRole + 1).isNull())
		{
			unsigned txi = item->data(Qt::UserRole + 1).toInt();
			bytes t = ethereum()->blockChain().transaction(h, txi);
			State s(ethereum()->state(txi, h));
			BlockInfo bi(ethereum()->blockChain().info(h));
			Executive e(s, ethereum()->blockChain(), EnvInfo(bi));
			Debugger dw(main(), main());
			dw.populate(e, Transaction(t, CheckTransaction::Everything));
			dw.exec();
		}
	}
}

void BlockList::dumpState(bool _post)
{
#if ETH_FATDB || !ETH_TRUE
	if (QListWidgetItem* item = m_ui->blocks->currentItem())
	{
		auto hba = item->data(Qt::UserRole).toByteArray();
		assert(hba.size() == 32);
		auto h = h256((byte const*)hba.data(), h256::ConstructFromPointer);

		QString fn = QFileDialog::getSaveFileName(main(), "Select file to output state dump");
		ofstream f(fn.toStdString());
		if (f.is_open())
		{
			if (item->data(Qt::UserRole + 1).isNull())
				ethereum()->block(h).state().streamJSON(f);
			else
				ethereum()->state(item->data(Qt::UserRole + 1).toInt() + (_post ? 1 : 0), h).streamJSON(f);
		}
	}
#endif
}

