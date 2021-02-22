/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qx509_generic_p.h"
#include "qasn1element_p.h"

#include "qssl_p.h"

#include <qhostaddress.h>
#include <qendian.h>
#include <qhash.h>

#include <memory>

QT_BEGIN_NAMESPACE

namespace QSsl {

namespace {

QByteArray colonSeparatedHex(const QByteArray &value)
{
    const int size = value.size();
    int i = 0;
    while (i < size && !value.at(i)) // skip leading zeros
       ++i;

    return value.mid(i).toHex(':');
}

} // Unnamed namespace.

bool X509CertificateGeneric::isEqual(const X509Certificate &rhs) const
{
    const auto &other = static_cast<const X509CertificateGeneric &>(rhs);
    return derData == other.derData;
}

bool X509CertificateGeneric::isSelfSigned() const
{
    if (null)
        return false;

    qCWarning(lcTlsBackend, "QSslCertificate::isSelfSigned: This function does not check, whether the certificate "
                            "is actually signed. It just checks whether issuer and subject are identical");
    return subjectMatchesIssuer;
}

QMultiMap<QSsl::AlternativeNameEntryType, QString> X509CertificateGeneric::subjectAlternativeNames() const
{
    return saNames;
}

#define BEGINCERTSTRING "-----BEGIN CERTIFICATE-----"
#define ENDCERTSTRING "-----END CERTIFICATE-----"

QByteArray X509CertificateGeneric::toPem() const
{
    QByteArray array = toDer();
    // Convert to Base64 - wrap at 64 characters.
    array = array.toBase64();
    QByteArray tmp;
    for (int i = 0; i <= array.size() - 64; i += 64) {
        tmp += QByteArray::fromRawData(array.data() + i, 64);
        tmp += '\n';
    }
    if (int remainder = array.size() % 64) {
        tmp += QByteArray::fromRawData(array.data() + array.size() - remainder, remainder);
        tmp += '\n';
    }

    return BEGINCERTSTRING "\n" + tmp + ENDCERTSTRING "\n";
}

QByteArray X509CertificateGeneric::toDer() const
{
    return derData;
}

QString X509CertificateGeneric::toText() const
{
    Q_UNIMPLEMENTED();
    return {};
}

Qt::HANDLE X509CertificateGeneric::handle() const
{
    Q_UNIMPLEMENTED();
    return nullptr;
}

size_t X509CertificateGeneric::hash(size_t seed) const noexcept
{
    return qHash(toDer(), seed);
}

QList<QSslCertificate> X509CertificateGeneric::certificatesFromPem(const QByteArray &pem, int count)
{
    QList<QSslCertificate> certificates;
    int offset = 0;
    while (count == -1 || certificates.size() < count) {
        int startPos = pem.indexOf(BEGINCERTSTRING, offset);
        if (startPos == -1)
            break;
        startPos += sizeof(BEGINCERTSTRING) - 1;
        if (!matchLineFeed(pem, &startPos))
            break;

        int endPos = pem.indexOf(ENDCERTSTRING, startPos);
        if (endPos == -1)
            break;

        offset = endPos + sizeof(ENDCERTSTRING) - 1;
        if (offset < pem.size() && !matchLineFeed(pem, &offset))
            break;

        QByteArray decoded = QByteArray::fromBase64(
            QByteArray::fromRawData(pem.data() + startPos, endPos - startPos));
        certificates << certificatesFromDer(decoded, 1);;
    }

    return certificates;
}

QList<QSslCertificate> X509CertificateGeneric::certificatesFromDer(const QByteArray &der, int count)
{
    QList<QSslCertificate> certificates;

    QByteArray data = der;
    while (count == -1 || certificates.size() < count) {
        QSslCertificate cert;
        auto *certBackend = QTlsBackend::backend<X509CertificateGeneric>(cert);
        if (!certBackend->parse(data))
            break;

        certificates << cert;
        data.remove(0, certBackend->derData.size());
    }

    return certificates;
}

bool X509CertificateGeneric::parse(const QByteArray &data)
{
    QAsn1Element root;

    QDataStream dataStream(data);
    if (!root.read(dataStream) || root.type() != QAsn1Element::SequenceType)
        return false;

    QDataStream rootStream(root.value());
    QAsn1Element cert;
    if (!cert.read(rootStream) || cert.type() != QAsn1Element::SequenceType)
        return false;

    // version or serial number
    QAsn1Element elem;
    QDataStream certStream(cert.value());
    if (!elem.read(certStream))
        return false;

    if (elem.type() == QAsn1Element::Context0Type) {
        QDataStream versionStream(elem.value());
        if (!elem.read(versionStream)
            || elem.type() != QAsn1Element::IntegerType
            || elem.value().isEmpty())
            return false;

        versionString = QByteArray::number(elem.value().at(0) + 1);
        if (!elem.read(certStream))
            return false;
    } else {
        versionString = QByteArray::number(1);
    }

    // serial number
    if (elem.type() != QAsn1Element::IntegerType)
        return false;
    serialNumberString = colonSeparatedHex(elem.value());

    // algorithm ID
    if (!elem.read(certStream) || elem.type() != QAsn1Element::SequenceType)
        return false;

    // issuer info
    if (!elem.read(certStream) || elem.type() != QAsn1Element::SequenceType)
        return false;

    QByteArray issuerDer = data.mid(dataStream.device()->pos() - elem.value().length(), elem.value().length());
    issuerInfoEntries = elem.toInfo();

    // validity period
    if (!elem.read(certStream) || elem.type() != QAsn1Element::SequenceType)
        return false;

    QDataStream validityStream(elem.value());
    if (!elem.read(validityStream) || (elem.type() != QAsn1Element::UtcTimeType && elem.type() != QAsn1Element::GeneralizedTimeType))
        return false;

    notValidBefore = elem.toDateTime();
    if (!notValidBefore.isValid())
        return false;

    if (!elem.read(validityStream) || (elem.type() != QAsn1Element::UtcTimeType && elem.type() != QAsn1Element::GeneralizedTimeType))
        return false;

    notValidAfter = elem.toDateTime();
    if (!notValidAfter.isValid())
        return false;


    // subject name
    if (!elem.read(certStream) || elem.type() != QAsn1Element::SequenceType)
        return false;

    QByteArray subjectDer = data.mid(dataStream.device()->pos() - elem.value().length(), elem.value().length());
    subjectInfoEntries = elem.toInfo();
    subjectMatchesIssuer = issuerDer == subjectDer;

    // public key
    qint64 keyStart = certStream.device()->pos();
    if (!elem.read(certStream) || elem.type() != QAsn1Element::SequenceType)
        return false;

    publicKeyDerData.resize(certStream.device()->pos() - keyStart);
    QDataStream keyStream(elem.value());
    if (!elem.read(keyStream) || elem.type() != QAsn1Element::SequenceType)
        return false;


    // key algorithm
    if (!elem.read(elem.value()) || elem.type() != QAsn1Element::ObjectIdentifierType)
        return false;

    const QByteArray oid = elem.toObjectId();
    if (oid == RSA_ENCRYPTION_OID)
        publicKeyAlgorithm = QSsl::Rsa;
    else if (oid == DSA_ENCRYPTION_OID)
        publicKeyAlgorithm = QSsl::Dsa;
    else if (oid == EC_ENCRYPTION_OID)
        publicKeyAlgorithm = QSsl::Ec;
    else
        publicKeyAlgorithm = QSsl::Opaque;

    certStream.device()->seek(keyStart);
    certStream.readRawData(publicKeyDerData.data(), publicKeyDerData.size());

    // extensions
    while (elem.read(certStream)) {
        if (elem.type() == QAsn1Element::Context3Type) {
            if (elem.read(elem.value()) && elem.type() == QAsn1Element::SequenceType) {
                QDataStream extStream(elem.value());
                while (elem.read(extStream) && elem.type() == QAsn1Element::SequenceType) {
                    X509CertificateExtension extension;
                    if (!parseExtension(elem.value(), extension))
                        return false;

                    if (extension.oid == QLatin1String("2.5.29.17")) {
                        // subjectAltName

                        // Note, parseExtension() returns true for this extensions,
                        // but considers it to be unsupported and assignes a useless
                        // value. OpenSSL also treats this extension as unsupported,
                        // but properly creates a map with 'name' and 'value' taken
                        // from the extension. We only support 'email', 'IP' and 'DNS',
                        // but this is what our subjectAlternativeNames map can contain
                        // anyway.
                        QVariantMap extValue;
                        QAsn1Element sanElem;
                        if (sanElem.read(extension.value.toByteArray()) && sanElem.type() == QAsn1Element::SequenceType) {
                            QDataStream nameStream(sanElem.value());
                            QAsn1Element nameElem;
                            while (nameElem.read(nameStream)) {
                                switch (nameElem.type()) {
                                case QAsn1Element::Rfc822NameType:
                                    saNames.insert(QSsl::EmailEntry, nameElem.toString());
                                    extValue[QStringLiteral("email")] = nameElem.toString();
                                    break;
                                case QAsn1Element::DnsNameType:
                                    saNames.insert(QSsl::DnsEntry, nameElem.toString());
                                    extValue[QStringLiteral("DNS")] = nameElem.toString();
                                    break;
                                case QAsn1Element::IpAddressType: {
                                    QHostAddress ipAddress;
                                    QByteArray ipAddrValue = nameElem.value();
                                    switch (ipAddrValue.length()) {
                                    case 4: // IPv4
                                        ipAddress = QHostAddress(qFromBigEndian(*reinterpret_cast<quint32 *>(ipAddrValue.data())));
                                        break;
                                    case 16: // IPv6
                                        ipAddress = QHostAddress(reinterpret_cast<quint8 *>(ipAddrValue.data()));
                                        break;
                                    default: // Unknown IP address format
                                        break;
                                    }
                                    if (!ipAddress.isNull()) {
                                        saNames.insert(QSsl::IpAddressEntry, ipAddress.toString());
                                        extValue[QStringLiteral("IP")] = ipAddress.toString();
                                    }
                                    break;
                                }
                                default:
                                    break;
                                }
                            }
                            extension.value = extValue;
                            extension.supported = true;
                        }
                    }

                    extensions << extension;
                }
            }
        }
    }

    derData = data.left(dataStream.device()->pos());
    null = false;
    return true;
}

bool X509CertificateGeneric::parseExtension(const QByteArray &data, X509CertificateExtension &extension)
{
    bool ok = false;
    bool critical = false;
    QAsn1Element oidElem, valElem;

    QDataStream seqStream(data);

    // oid
    if (!oidElem.read(seqStream) || oidElem.type() != QAsn1Element::ObjectIdentifierType)
        return false;

    const QByteArray oid = oidElem.toObjectId();
    // critical and value
    if (!valElem.read(seqStream))
        return false;

    if (valElem.type() == QAsn1Element::BooleanType) {
        critical = valElem.toBool(&ok);

        if (!ok || !valElem.read(seqStream))
            return false;
    }

    if (valElem.type() != QAsn1Element::OctetStringType)
        return false;

    // interpret value
    QAsn1Element val;
    bool supported = true;
    QVariant value;
    if (oid == "1.3.6.1.5.5.7.1.1") {
        // authorityInfoAccess
        if (!val.read(valElem.value()) || val.type() != QAsn1Element::SequenceType)
            return false;
        QVariantMap result;
        const auto elems = val.toList();
        for (const QAsn1Element &el : elems) {
            const auto items = el.toList();
            if (items.size() != 2)
                return false;
            const QString key = QString::fromLatin1(items.at(0).toObjectName());
            switch (items.at(1).type()) {
            case QAsn1Element::Rfc822NameType:
            case QAsn1Element::DnsNameType:
            case QAsn1Element::UniformResourceIdentifierType:
                result[key] = items.at(1).toString();
                break;
            }
        }
        value = result;
    } else if (oid == "2.5.29.14") {
        // subjectKeyIdentifier
        if (!val.read(valElem.value()) || val.type() != QAsn1Element::OctetStringType)
            return false;
        value = colonSeparatedHex(val.value()).toUpper();
    } else if (oid == "2.5.29.19") {
        // basicConstraints
        if (!val.read(valElem.value()) || val.type() != QAsn1Element::SequenceType)
            return false;

        QVariantMap result;
        const auto items = val.toList();
        if (items.size() > 0) {
            result[QStringLiteral("ca")] = items.at(0).toBool(&ok);
            if (!ok)
                return false;
        } else {
            result[QStringLiteral("ca")] = false;
        }
        if (items.size() > 1) {
            result[QStringLiteral("pathLenConstraint")] = items.at(1).toInteger(&ok);
            if (!ok)
                return false;
        }
        value = result;
    } else if (oid == "2.5.29.35") {
        // authorityKeyIdentifier
        if (!val.read(valElem.value()) || val.type() != QAsn1Element::SequenceType)
            return false;
        QVariantMap result;
        const auto elems = val.toList();
        for (const QAsn1Element &el : elems) {
            if (el.type() == 0x80) {
                const QString key = QStringLiteral("keyid");
                result[key] = el.value().toHex();
            } else if (el.type() == 0x82) {
                const QString serial = QStringLiteral("serial");
                result[serial] = colonSeparatedHex(el.value());
            }
        }
        value = result;
    } else {
        supported = false;
        value = valElem.value();
    }

    extension.critical = critical;
    extension.supported = supported;
    extension.oid = QString::fromLatin1(oid);
    extension.name = QString::fromLatin1(oidElem.toObjectName());
    extension.value = value;

    return true;
}

} // namespace QSsl

QT_END_NAMESPACE
