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

#ifndef __GALERA_VIEW_H__
#define __GALERA_VIEW_H__

#include <common/filter.h>

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtDBus/QtDBus>

#include <QtContacts/QContactFilter>

namespace galera
{
class ContactEntry;
class ViewAdaptor;
class ContactsMap;
class FilterThread;

class View : public QObject
{
     Q_OBJECT
public:
    View(QString clause, QString sort, QStringList sources, ContactsMap *allContacts, QObject *parent);
    ~View();

    static QString objectPath();
    QString dynamicObjectPath() const;
    QObject *adaptor() const;
    bool registerObject(QDBusConnection &connection);
    void unregisterObject(QDBusConnection &connection);

    // contacts
    bool appendContact(ContactEntry *entry);
    bool removeContact(ContactEntry *entry);

    // Adaptor
    QString contactDetails(const QStringList &fields, const QString &id);
    int count();
    void sort(const QString &field);
    void close();

public Q_SLOTS:
    QStringList contactsDetails(const QStringList &fields, int startIndex, int pageSize, const QDBusMessage &message);

Q_SIGNALS:
    void closed();

private:
    QStringList m_sources;
    ViewAdaptor *m_adaptor;
    FilterThread *m_filterThread;

    void applyFilter();
    bool checkContact(ContactEntry *entry);
};

} //namespace

#endif

