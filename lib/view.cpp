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

#include "view.h"
#include "view-adaptor.h"
#include "contacts-map.h"
#include "contact-less-than.h"
#include "qindividual.h"

#include "common/vcard-parser.h"
#include "common/filter.h"
#include "common/fetch-hint.h"
#include "common/dbus-service-defs.h"

#include <QtContacts/QContact>

#include <QtVersit/QVersitDocument>

#include <QtCore/QReadWriteLock>
#include <QtCore/QCoreApplication>

using namespace QtContacts;
using namespace QtVersit;

namespace galera
{

class FilterThread: public QRunnable
{
public:
    FilterThread(QString filter, QString sort, int maxCount, ContactsMap *allContacts, QObject *parent)
        : m_parent(parent),
          m_filter(filter),
          m_sortClause(sort),
          m_maxCount(maxCount),
          m_allContacts(allContacts),
          m_canceled(false),
          m_running(false),
          m_done(false)
    {
        setAutoDelete(false);
    }

    QList<ContactEntry*> result() const
    {
        if (isRunning()) {
            return QList<ContactEntry*>();
        } else {
            return m_contacts;
        }
    }

    bool appendContact(ContactEntry *entry)
    {
        if (checkContact(entry)) {
            addSorted(&m_contacts, entry, m_sortClause);
            return true;
        }
        return false;
    }

    bool removeContact(ContactEntry *entry)
    {
        return m_contacts.removeAll(entry);
    }

    void chageSort(SortClause clause)
    {
        m_sortClause = clause;
        if (!clause.isEmpty()) {
            ContactLessThan lessThan(m_sortClause);
            qSort(m_contacts.begin(), m_contacts.end(), lessThan);
        }
    }

    void addSorted(QList<ContactEntry*>* sorted, ContactEntry* toAdd, const SortClause& sortOrder)
    {
        if (!sortOrder.isEmpty()) {
            ContactLessThan lessThan(sortOrder);
            QList<ContactEntry*>::iterator it(std::upper_bound(sorted->begin(), sorted->end(), toAdd, lessThan));
            sorted->insert(it, toAdd);
        } else {
            // no sort order? just add it to the end
            sorted->append(toAdd);
        }
    }

    void cancel()
    {
        m_canceledLock.lockForWrite();
        m_canceled = true;
        m_canceledLock.unlock();
    }

    bool isRunning() const
    {
        return m_running;
    }

    bool done() const
    {
        return m_done;
    }

protected:
    void notifyFinished()
    {
        m_running = false;
        m_done = true;
        QMetaObject::invokeMethod(m_parent, "onFilterDone", Qt::QueuedConnection);
    }

    void run()
    {
        if (m_canceled || !m_allContacts) {
            notifyFinished();
            return;
        }

        m_allContacts->lockForRead();
        // only sort contacts if the contacts was stored in a different order into the contacts map
        bool needSort = (!m_sortClause.isEmpty() &&
                         (m_sortClause.toContactSortOrder() != m_allContacts->sort().toContactSortOrder()));
        // filter contacts if necessary
        if (m_filter.isValid()) {
            if (m_filter.isEmpty()) {
                m_contacts = m_allContacts->values();
                if ((m_maxCount > 0) && (m_maxCount < m_contacts.size())) {
                    m_contacts = m_contacts.mid(0, m_maxCount);
                }
                if (needSort) {
                    chageSort(m_sortClause);
                }
            } else {
                // optmization
                // if it is a query by phone number do a initial filter
                const QSet<ContactEntry *> &preFilter = m_allContacts->valueByPhone(m_filter.phoneNumberToFilter());
                Q_FOREACH(ContactEntry *entry, preFilter) {
                    m_canceledLock.lockForRead();
                    if (m_canceled) {
                        m_canceledLock.unlock();
                        m_allContacts->unlock();
                        notifyFinished();
                        return;
                    }
                    m_canceledLock.unlock();
                    if (checkContact(entry)) {
                        if (needSort) {
                            addSorted(&m_contacts, entry, m_sortClause);
                        } else {
                            m_contacts.append(entry);
                        }

                        if ((m_maxCount > 0) && (m_contacts.size() >= m_maxCount)) {
                            break;
                        }
                    }
                }
            }
        } else {
            // invalid filter
            m_contacts.clear();
        }

        m_allContacts->unlock();
        notifyFinished();
    }

private:
    QObject *m_parent;
    Filter m_filter;
    SortClause m_sortClause;
    ContactsMap *m_allContacts;
    QList<ContactEntry*> m_contacts;
    int m_maxCount;
    bool m_canceled;
    QReadWriteLock m_canceledLock;
    bool m_running;
    bool m_done;
    bool checkContact(ContactEntry *entry)
    {
        return m_filter.test(entry->individual()->contact());
    }
};

View::View(const QString &clause, const QString &sort, int maxCount,
           const QStringList &sources, ContactsMap *allContacts,
           QObject *parent)
    : QObject(parent),
      m_sources(sources),
      m_filterThread(new FilterThread(clause, sort, maxCount, allContacts, this)),
      m_adaptor(0),
      m_waiting(0)
{
    if (allContacts) {
        QThreadPool::globalInstance()->start(m_filterThread);
    }
}

View::~View()
{
    close();
}

void View::close()
{
    if (m_adaptor) {
        Q_EMIT m_adaptor->contactsRemoved(0, m_filterThread->result().count());
        Q_EMIT closed();

        QDBusConnection conn = QDBusConnection::sessionBus();
        unregisterObject(conn);
        m_adaptor->destroy();
        m_adaptor = 0;
    }

    if (m_filterThread) {
        if (!m_filterThread->done()) {
            m_filterThread->cancel();
            waitFilter();
        }
        delete m_filterThread;
        m_filterThread = 0;
    }
}

bool View::isOpen() const
{
    return (m_adaptor != 0);
}

QString View::contactDetails(const QStringList &fields, const QString &id)
{
    Q_ASSERT(FALSE);
    return QString();
}

QStringList View::contactsDetails(const QStringList &fields, int startIndex, int pageSize, const QDBusMessage &message)
{
    if (!m_filterThread || !isOpen()) {
        return QStringList();
    }

     waitFilter();

    QList<ContactEntry*> entries = m_filterThread->result();
    if (startIndex < 0) {
        startIndex = 0;
    }

    if ((pageSize < 0) || ((startIndex + pageSize) >= entries.count())) {
        pageSize = entries.count() - startIndex;
    }

    QList<QContact> contacts;
    for(int i = startIndex, iMax = (startIndex + pageSize); i < iMax; i++) {
        contacts << entries[i]->individual()->copy(FetchHint::parseFieldNames(fields));
    }

    VCardParser *parser = new VCardParser(this);
    parser->setProperty("DATA", QVariant::fromValue<QDBusMessage>(message));
    connect(parser, &VCardParser::vcardParsed,
            this, &View::onVCardParsed);
    parser->contactToVcard(contacts);
    return QStringList();
}

void View::onVCardParsed(const QStringList &vcards)
{
    QObject *sender = QObject::sender();
    QDBusMessage reply = sender->property("DATA").value<QDBusMessage>().createReply(vcards);
    QDBusConnection::sessionBus().send(reply);
    sender->deleteLater();
}

void View::onFilterDone()
{
    if (m_waiting) {
        m_waiting->quit();
        m_waiting = 0;
    }
}

void View::waitFilter()
{
    if (m_filterThread && !m_filterThread->done()) {
        QEventLoop loop;
        m_waiting = &loop;
        loop.exec();
    }
}

int View::count()
{
    if (!isOpen()) {
        return 0;
    }

    waitFilter();

    return m_filterThread->result().count();
}

void View::sort(const QString &field)
{
    if (!isOpen()) {
        return;
    }

    m_filterThread->chageSort(SortClause(field));
}

QString View::objectPath()
{
    return CPIM_ADDRESSBOOK_VIEW_OBJECT_PATH;
}

QString View::dynamicObjectPath() const
{
    return objectPath() + "/" + QString::number((long)this);
}

bool View::registerObject(QDBusConnection &connection)
{
    if (!m_adaptor) {
        m_adaptor = new ViewAdaptor(connection, this);
        if (!connection.registerObject(dynamicObjectPath(), this))
        {
            qWarning() << "Could not register object!" << objectPath();
            delete m_adaptor;
            m_adaptor = 0;
        } else {
            connect(this, SIGNAL(countChanged(int)), m_adaptor, SIGNAL(countChanged(int)));
        }
    }

    return (m_adaptor != 0);
}

void View::unregisterObject(QDBusConnection &connection)
{
    if (m_adaptor) {
        connection.unregisterObject(dynamicObjectPath());
    }
}

bool View::appendContact(ContactEntry *entry)
{
    if (!isOpen()) {
        return false;
    }

    if (m_filterThread->appendContact(entry)) {
        Q_EMIT countChanged(m_filterThread->result().count());
        return true;
    }
    return false;
}

bool View::removeContact(ContactEntry *entry)
{
    if (!isOpen()) {
        return false;
    }

    if (m_filterThread->removeContact(entry)) {
        Q_EMIT countChanged(m_filterThread->result().count());
        return true;
    }
    return false;
}

QObject *View::adaptor() const
{
    return m_adaptor;
}

} //namespace
