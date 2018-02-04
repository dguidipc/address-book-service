/*
 * Copyright 2013 Canonical Ltd.
 *
 * This file is part of contact-service-app.
 *
 * contact-service-app is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * contact-service-app is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GALERA_QCONTACT_ENGINEID_H__
#define __GALERA_QCONTACT_ENGINEID_H__

#include <QtCore/QSharedData>

namespace galera
{

class GaleraEngineId : public QSharedData
{
public:
    GaleraEngineId();
    ~GaleraEngineId();
    GaleraEngineId(const QString &contactId, const QString &managerUri);
    GaleraEngineId(const GaleraEngineId &other);
    GaleraEngineId(const QMap<QString, QString> &parameters, const QString &engineIdString);

    bool isEqualTo(const GaleraEngineId *other) const;
    bool isLessThan(const GaleraEngineId *other) const;

    QString managerUri() const;

    GaleraEngineId* clone() const;

    QString toString() const;
    QByteArray toByteArray() const;

#ifndef QT_NO_DEBUG_STREAM
    QDebug& debugStreamOut(QDebug &dbg) const;
#endif
#ifndef QT_NO_DATASTREAM
    friend QDataStream& operator<<(QDataStream& out, const GaleraEngineId& engineId);
    friend QDataStream& operator>>(QDataStream& in, GaleraEngineId& engineId);
#endif
    uint hash() const;

private:
    QString m_contactId;
    QString m_managerUri;
};

} //namespace

#endif
