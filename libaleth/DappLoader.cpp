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
/** @file DappLoader.cpp
 * @author Arkadiy Paronyan <arkadiy@ethdev.org>
 * @date 2015
 */

#include <algorithm>
#include <functional>
#include <json/json.h>
#include <boost/algorithm/string.hpp>
#include <QUrl>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMimeDatabase>
#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>
#include <libdevcrypto/CryptoPP.h>
#include <libdevcore/SHA3.h>
#include <libethcore/CommonJS.h>
#include <libethereum/Client.h>
#include <libwebthree/WebThree.h>
#include "DappLoader.h"
#include "AlethResources.hpp"
using namespace std;
using namespace dev;
using namespace crypto;
using namespace eth;
using namespace aleth;

namespace dev { namespace aleth { QString contentsOfQResource(std::string const& res); } }

DappLoader::DappLoader(QObject* _parent, WebThreeDirect* _web3, Address _nameReg):
	QObject(_parent), m_web3(_web3), m_nameReg(_nameReg)
{
	connect(&m_net, &QNetworkAccessManager::finished, this, &DappLoader::downloadComplete);
}

strings decomposed(std::string const& _name)
{
	unsigned i = _name.find_first_of('/');
	strings parts;
	string ts = _name.substr(0, i);
	boost::algorithm::split(parts, ts, boost::algorithm::is_any_of("."));
	std::reverse(parts.begin(), parts.end());
	strings pathParts;
	boost::algorithm::split(pathParts, ts = _name.substr(i + 1), boost::is_any_of("/"));
	parts += pathParts;
	return parts;
}

template <class T> T lookup(Address const& _nameReg, std::function<bytes(Address, bytes)> const& _call, strings const& _path, std::string const& _query)
{
	Address address = _nameReg;
	for (unsigned i = 0; i < _path.size() - 1; ++i)
		address = abiOut<Address>(_call(address, abiIn("subRegistrar(string)", _path[i])));
	return abiOut<T>(_call(address, abiIn(_query + "(bytes32)", _path.back())));
}

template <class T> T lookup(Address const& _nameReg, Client* _c, strings const& _path, std::string const& _query)
{
	return lookup<T>(_nameReg, [&](Address a, bytes b){return _c->call(a, b).output;}, _path, _query);
}

DappLocation DappLoader::resolveAppUri(QString const& _uri)
{
	QUrl url(_uri);
	if (!url.scheme().isEmpty() && url.scheme() != "eth")
		throw dev::Exception(); //TODO:

	strings domainParts = decomposed((url.host() + url.path()).toUtf8().toStdString());
	h256 contentHash = lookup<h256>(m_nameReg, web3()->ethereum(), domainParts, "content");
	Address urlHint = lookup<Address>(m_nameReg, web3()->ethereum(), {"UrlHinter"}, "owner");
	string32 contentUrl = abiOut<string32>(web3()->ethereum()->call(urlHint, abiIn("url(bytes32)", contentHash)).output);

	QString path = QString::fromStdString(boost::algorithm::join(domainParts, "/"));
	QString contentUrlString = QString::fromUtf8(std::string(contentUrl.data(), contentUrl.size()).c_str());
	if (!contentUrlString.startsWith("http://") || !contentUrlString.startsWith("https://"))
		contentUrlString = "http://" + contentUrlString;
	return DappLocation { "/", path, contentUrlString, contentHash };
}

void DappLoader::downloadComplete(QNetworkReply* _reply)
{
	QUrl requestUrl = _reply->request().url();
	if (m_pageUrls.count(requestUrl) != 0)
	{
		//inject web3 js
		QByteArray content = "<script>\n";
		content.append(jsCode());
		content.append(("web3.admin.setSessionKey('" + m_sessionKey + "');").c_str());
		content.append("</script>\n");
		content.append(_reply->readAll());
		QString contentType = _reply->header(QNetworkRequest::ContentTypeHeader).toString();
		if (contentType.isEmpty())
		{
			QMimeDatabase db;
			contentType = db.mimeTypeForUrl(requestUrl).name();
		}
		pageReady(content, contentType, requestUrl);
		return;
	}

	try
	{
		//try to interpret as rlp
		QByteArray data = _reply->readAll();
		_reply->deleteLater();

		h256 expected = m_uriHashes[requestUrl];
		bytes package(reinterpret_cast<unsigned char const*>(data.constData()), reinterpret_cast<unsigned char const*>(data.constData() + data.size()));
		Secp256k1PP dec;
		dec.decrypt(Secret(expected), package);
		h256 got = sha3(package);
		if (got != expected)
		{
			//try base64
			data = QByteArray::fromBase64(data);
			package = bytes(reinterpret_cast<unsigned char const*>(data.constData()), reinterpret_cast<unsigned char const*>(data.constData() + data.size()));
			dec.decrypt(Secret(expected), package);
			got = sha3(package);
			if (got != expected)
				throw dev::Exception() << errinfo_comment("Dapp content hash does not match");
		}

		RLP rlp(package);
		loadDapp(rlp);
		bytesRef(&package).cleanse();	// TODO: replace with bytesSec once the crypto API is up to it.
	}
	catch (...)
	{
		qWarning() << tr("Error downloading DApp: ") << boost::current_exception_diagnostic_information().c_str();
		emit dappError();
	}
}

void DappLoader::loadDapp(RLP const& _rlp)
{
	Dapp dapp;
	unsigned len = _rlp.itemCountStrict();
	dapp.manifest = loadManifest(_rlp[0].toString());
	for (unsigned c = 1; c < len; ++c)
	{
		bytesConstRef content = _rlp[c].toBytesConstRef();
		h256 hash = sha3(content);
		auto entry = std::find_if(dapp.manifest.entries.cbegin(), dapp.manifest.entries.cend(), [=](ManifestEntry const& _e) { return _e.hash == hash; });
		if (entry != dapp.manifest.entries.cend())
		{
			if (entry->path == "/deployment.js")
			{
				//inject web3 code
				bytes b(jsCode().data(), jsCode().data() + jsCode().size());
				b.insert(b.end(), content.begin(), content.end());
				dapp.content[hash] = b;
			}
			else
				dapp.content[hash] = content.toBytes();
		}
		else
			throw dev::Exception() << errinfo_comment("Dapp content hash does not match");
	}
	emit dappReady(dapp);
}

QByteArray const& DappLoader::jsCode() const
{
	if (m_web3JsCache.isEmpty())
		m_web3JsCache = makeJSCode().toLatin1();
	return m_web3JsCache;
}

Manifest DappLoader::loadManifest(std::string const& _manifest)
{
	/// https://github.com/ethereum/go-ethereum/wiki/URL-Scheme
	Manifest manifest;
	Json::Reader jsonReader;
	Json::Value root;
	jsonReader.parse(_manifest, root, false);

	Json::Value entries = root["entries"];
	for (Json::ValueIterator it = entries.begin(); it != entries.end(); ++it)
	{
		Json::Value const& entryValue = *it;
		std::string path = entryValue["path"].asString();
		if (path.size() == 0 || path[0] != '/')
			path = "/" + path;
		std::string contentType = entryValue["contentType"].asString();
		if (contentType.empty())
			contentType = QMimeDatabase().mimeTypesForFileName(QString::fromStdString(path))[0].name().toStdString();
		std::string strHash = entryValue["hash"].asString();
		if (strHash.length() == 64)
			strHash = "0x" + strHash;
		h256 hash = jsToFixed<32>(strHash);
		unsigned httpStatus = entryValue["status"].asInt();
		manifest.entries.push_back(ManifestEntry{ path, hash, contentType, httpStatus });
	}
	return manifest;
}

void DappLoader::loadDapp(QString const& _uri)
{
	QUrl uri(_uri);
	QUrl contentUri;
	h256 hash;
	qDebug() << "URI path:" << uri.path();
	qDebug() << "URI query:" << uri.query();
	if (uri.path().endsWith(".dapp") && uri.query().startsWith("hash="))
	{
		contentUri = uri;
		QString query = uri.query();
		query.remove("hash=");
		if (!query.startsWith("0x"))
			query.insert(0, "0x");
		hash = jsToFixed<32>(query.toStdString());
	}
	else
	{
		DappLocation location = resolveAppUri(_uri);
		contentUri = location.contentUri;
		hash = location.contentHash;
		uri = contentUri;
	}
	QNetworkRequest request(contentUri);
	m_uriHashes[uri] = hash;
	m_net.get(request);
}

void DappLoader::loadPage(QString const& _uri)
{
	QUrl uri(_uri);
	QNetworkRequest request(uri);
	m_pageUrls.insert(uri);
	m_net.get(request);
}

QString DappLoader::makeJSCode()
{
	AlethResources resources;
	QString content;
	content += QString::fromStdString(resources.loadResourceAsString("web3"));
	content += "\n";
	content += QString::fromStdString(resources.loadResourceAsString("setup"));
	content += "\n";
	content += QString::fromStdString(resources.loadResourceAsString("admin"));
	content += "\n";
	return content;
}

