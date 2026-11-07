// worker_auth.h

#ifndef WORKER_AUTH_H
#define WORKER_AUTH_H

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <string>

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QHash>
#include <QMessageAuthenticationCode>
#include <QMutex>
#include <QMutexLocker>
#include <QRandomGenerator>
#include <QString>

// Request signing for the worker protocol. The shared token never travels:
//
//   Authorization: Rendrop-HMAC ts=<unix ms>,nonce=<32 hex>,sig=<64 hex>
//   sig = HMAC-SHA256(token, "req|" METHOD "|" decoded path "|" ts "|" nonce)
//
// The coordinator accepts each nonce once within the clock window, so a
// captured request cannot be replayed. Bodies are not covered; on a hostile
// network an active attacker can still alter traffic, which only TLS fixes.
//
// Responses that tell a worker what to render carry
//   X-Rendrop-Response: HMAC-SHA256(token, "resp|" nonce "|" sha256(body))
// so a machine that merely pretends to be a coordinator (for example through a
// forged beacon) cannot hand out work.
//
// Beacons are signed the same way with the "beacon|" prefix.
namespace WorkerAuth {

constexpr qint64 clockWindowMs = 5 * 60 * 1000;

inline QByteArray hmacHex(const QByteArray &token, const QByteArray &message)
{
    return QMessageAuthenticationCode::hash(message, token, QCryptographicHash::Sha256).toHex();
}

inline bool constantTimeEquals(const QByteArray &left, const QByteArray &right)
{
    unsigned char difference = left.size() == right.size() ? 0 : 1;
    const qsizetype length = std::max(left.size(), right.size());
    for (qsizetype index = 0; index < length; ++index)
    {
        const unsigned char a = index < left.size() ? static_cast<unsigned char>(left[index]) : 0;
        const unsigned char b = index < right.size() ? static_cast<unsigned char>(right[index]) : 0;
        difference |= a ^ b;
    }
    return difference == 0;
}

inline QByteArray newNonce()
{
    QByteArray bytes(16, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(bytes.data()), 4);
    return bytes.toHex();
}

inline QByteArray requestMessage(const QByteArray &method, const QByteArray &path, qint64 timeMs, const QByteArray &nonce)
{
    return "req|" + method + '|' + path + '|' + QByteArray::number(timeMs) + '|' + nonce;
}

struct SignedRequest
{
    std::string header;
    QByteArray nonce;
};

inline SignedRequest signRequest(const QByteArray &token, const QByteArray &method, const QByteArray &path)
{
    const qint64 timeMs = QDateTime::currentMSecsSinceEpoch();
    const QByteArray nonce = newNonce();
    const QByteArray signature = hmacHex(token, requestMessage(method, path, timeMs, nonce));
    const QByteArray header = "Rendrop-HMAC ts=" + QByteArray::number(timeMs) + ",nonce=" + nonce + ",sig=" + signature;
    return {header.toStdString(), nonce};
}

inline QByteArray responseSignature(const QByteArray &token, const QByteArray &nonce, const QByteArray &body)
{
    return hmacHex(token, "resp|" + nonce + '|' + QCryptographicHash::hash(body, QCryptographicHash::Sha256).toHex());
}

inline QByteArray beaconSignature(const QByteArray &token, const QByteArray &payload)
{
    return hmacHex(token, "beacon|" + payload);
}

// Server side: verifies signed headers and remembers nonces. Thread-safe.
class Verifier
{
public:
    enum class Result
    {
        Ok,
        Missing,
        Invalid
    };

    // On success nonce receives the request's nonce for response signing.
    Result verify(
        const QByteArray &token,
        const QByteArray &header,
        const QByteArray &method,
        const QByteArray &path,
        QByteArray &nonce
    )
    {
        static const QByteArray prefix = "Rendrop-HMAC ";
        if (!header.startsWith(prefix))
        {
            return Result::Missing;
        }
        qint64 timeMs = -1;
        QByteArray signature;
        nonce.clear();
        for (const QByteArray &part : header.mid(prefix.size()).split(','))
        {
            const qsizetype equals = part.indexOf('=');
            if (equals <= 0)
            {
                return Result::Invalid;
            }
            const QByteArray key = part.left(equals).trimmed();
            const QByteArray value = part.mid(equals + 1).trimmed();
            if (key == "ts")
            {
                bool ok = false;
                timeMs = value.toLongLong(&ok);
                if (!ok)
                {
                    return Result::Invalid;
                }
            }
            else if (key == "nonce")
            {
                nonce = value;
            }
            else if (key == "sig")
            {
                signature = value;
            }
        }
        if (timeMs < 0 || nonce.size() < 16 || nonce.size() > 64 || signature.isEmpty())
        {
            return Result::Invalid;
        }
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (std::abs(now - timeMs) > clockWindowMs)
        {
            return Result::Invalid;
        }
        const QByteArray expected = hmacHex(token, requestMessage(method, path, timeMs, nonce));
        if (!constantTimeEquals(expected, signature))
        {
            return Result::Invalid;
        }
        QMutexLocker locker(&mutex);
        if (seen.size() > 4096)
        {
            for (auto iterator = seen.begin(); iterator != seen.end();)
            {
                iterator = now - iterator.value() > 2 * clockWindowMs ? seen.erase(iterator) : std::next(iterator);
            }
        }
        if (seen.contains(nonce))
        {
            return Result::Invalid;
        }
        seen.insert(nonce, now);
        return Result::Ok;
    }

private:
    QMutex mutex;
    QHash<QByteArray, qint64> seen;
};

}

#endif // WORKER_AUTH_H
