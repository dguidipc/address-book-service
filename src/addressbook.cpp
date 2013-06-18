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

#include "addressbook.h"
#include "addressbook-adaptor.h"
#include "view.h"
#include "contacts-map.h"
#include "qindividual.h"

#include "common/vcard-parser.h"

#include <QtCore/QPair>
#include <QtCore/QUuid>

using namespace QtContacts;

namespace
{
class UpdateContactsData
{
public:
    QList<QContact> m_contacts;
    QStringList m_request;
    int m_currentIndex;
    QStringList m_result;
    galera::AddressBook *m_addressbook;
    QDBusMessage m_message;
};

class RemoveContactsData
{
public:
    QStringList m_request;
    galera::AddressBook *m_addressbook;
    QDBusMessage m_message;
    int m_sucessCount;
};

}

namespace galera
{

AddressBook::AddressBook(QObject *parent)
    : QObject(parent),
      m_contacts(new ContactsMap),
      m_initializing(true),
      m_adaptor(0)
{
    prepareFolks();
}

AddressBook::~AddressBook()
{
    // destructor
}

QString AddressBook::objectPath()
{
    return CPIM_ADDRESSBOOK_OBJECT_PATH;
}

bool AddressBook::registerObject(QDBusConnection &connection)
{
    if (!m_adaptor) {
        m_adaptor = new AddressBookAdaptor(connection, this);
        if (!connection.registerObject(galera::AddressBook::objectPath(), this))
        {
            qWarning() << "Could not register object!" << objectPath();
            delete m_adaptor;
            m_adaptor = 0;
        } else {
            qDebug() << "Object registered:" << objectPath();
        }
    }

    return (m_adaptor != 0);
}

void AddressBook::prepareFolks()
{
    //TODO: filter EDS (FolksBackendStore)
    m_individualAggregator = folks_individual_aggregator_new();
    g_signal_connect(m_individualAggregator,
                     "individuals-changed-detailed",
                     (GCallback) AddressBook::individualsChangedCb,
                     this);
    folks_individual_aggregator_prepare(m_individualAggregator,
                                        (GAsyncReadyCallback) AddressBook::aggregatorPrepareCb,
                                        this);
}

SourceList AddressBook::availableSources(const QDBusMessage &message)
{
    FolksBackendStore *backendStore = folks_backend_store_dup();
    QDBusMessage *msg = new QDBusMessage(message);

    if (folks_backend_store_get_is_prepared(backendStore)) {
        availableSourcesReply(backendStore, 0, msg);
    } else {
        folks_backend_store_prepare(backendStore, (GAsyncReadyCallback) availableSourcesReply, msg);
    }
    return SourceList();
}

void AddressBook::availableSourcesReply(FolksBackendStore *backendStore, GAsyncResult *res, QDBusMessage *message)
{
    if (res) {
        folks_backend_store_prepare_finish(backendStore, res);
    }
    GeeCollection *backends = folks_backend_store_list_backends(backendStore);
    SourceList result;

    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(backends));
    while(gee_iterator_next(iter)) {
        FolksBackend *backend = FOLKS_BACKEND(gee_iterator_get(iter));

        GeeMap *stores = folks_backend_get_persona_stores(backend);
        GeeCollection *values =  gee_map_get_values(stores);
        GeeIterator *backendIter = gee_iterable_iterator(GEE_ITERABLE(values));

        while(gee_iterator_next(backendIter)) {
            FolksPersonaStore *store = FOLKS_PERSONA_STORE(gee_iterator_get(backendIter));

            QString id = QString::fromUtf8(folks_persona_store_get_id(store));
            bool canWrite = folks_persona_store_get_is_writeable(store);
            result << Source(id, !canWrite);

            g_object_unref(store);
        }

        g_object_unref(backendIter);
        g_object_unref(backend);
        g_object_unref(values);
    }
    g_object_unref(iter);

    QDBusMessage reply = message->createReply(QVariant::fromValue<SourceList>(result));
    QDBusConnection::sessionBus().send(reply);
    delete message;
}

QString AddressBook::createContact(const QString &contact, const QString &source, const QDBusMessage &message)
{
    ContactEntry *entry = m_contacts->valueFromVCard(contact);
    if (entry) {
        qWarning() << "Contact exists";
    } else {
        QContact qcontact = VCardParser::vcardToContact(contact);
        if (!qcontact.isEmpty()) {
            GHashTable *details = QIndividual::parseDetails(qcontact);
            //TOOD: lookup for source and use the correct store
            QDBusMessage *cpy = new QDBusMessage(message);
            folks_individual_aggregator_add_persona_from_details(m_individualAggregator,
                                                                 NULL, //parent
                                                                 folks_individual_aggregator_get_primary_store(m_individualAggregator),
                                                                 details,
                                                                 (GAsyncReadyCallback) createContactDone,
                                                                 (void*) cpy);
            g_hash_table_destroy(details);
            return "";
        }
    }

    QDBusMessage reply = message.createReply(QString());
    QDBusConnection::sessionBus().send(reply);
    return "";
}

QString AddressBook::linkContacts(const QStringList &contacts)
{
    //TODO
    return "";
}

View *AddressBook::query(const QString &clause, const QString &sort, const QStringList &sources)
{
    View *view = new View(clause, sort, sources, m_contacts, this);
    m_views << view;
    connect(view, SIGNAL(closed()), this, SLOT(viewClosed()));
    return view;
}

void AddressBook::viewClosed()
{
    m_views.remove(qobject_cast<View*>(QObject::sender()));
}

int AddressBook::removeContacts(const QStringList &contactIds, const QDBusMessage &message)
{
    RemoveContactsData *data = new RemoveContactsData;
    data->m_addressbook = this;
    data->m_message = message;
    data->m_request = contactIds;
    data->m_sucessCount = 0;
    removeContactContinue(0, 0, data);
    return 0;
}

void AddressBook::removeContactContinue(FolksIndividualAggregator *individualAggregator,
                                        GAsyncResult *result,
                                        void *data)
{
    GError *error = 0;
    RemoveContactsData *removeData = static_cast<RemoveContactsData*>(data);

    if (result) {
        folks_individual_aggregator_remove_individual_finish(individualAggregator, result, &error);
        if (error) {
            qWarning() << "Fail to remove contact:" << error->message;
            g_error_free(error);
        } else {
            qDebug() << "contact removed";
            removeData->m_sucessCount++;
        }
    }


    if (!removeData->m_request.isEmpty()) {
        QString contactId = removeData->m_request.takeFirst();
        ContactEntry *entry = removeData->m_addressbook->m_contacts->value(contactId);
        if (entry) {
            folks_individual_aggregator_remove_individual(individualAggregator,
                                                          entry->individual()->individual(),
                                                          (GAsyncReadyCallback) removeContactContinue,
                                                          data);
        } else {
            qWarning() << "ContactId not found:" << contactId;
            removeContactContinue(individualAggregator, 0, data);
        }
    } else {
        QDBusMessage reply = removeData->m_message.createReply(removeData->m_sucessCount);
        QDBusConnection::sessionBus().send(reply);
        delete removeData;
    }
}

QStringList AddressBook::sortFields()
{
    return SortClause::supportedFields();
}

bool AddressBook::unlinkContacts(const QString &parent, const QStringList &contacts)
{
    //TODO
    return false;
}

QStringList AddressBook::updateContacts(const QStringList &contacts, const QDBusMessage &message)
{
    //TODO: support multiple update contacts calls
    Q_ASSERT(m_updateCommandPendingContacts.isEmpty());

    m_updateCommandReplyMessage = message;
    m_updateCommandResult = contacts;
    m_updateCommandPendingContacts << VCardParser::vcardToContact(contacts);

    updateContactsDone(0, QString());

    return QStringList();
}

void AddressBook::updateContactsDone(galera::QIndividual *individual, const QString &error)
{
    Q_UNUSED(individual);
    qDebug() << Q_FUNC_INFO;

    if (!error.isEmpty()) {
        // update the result with the error
        m_updateCommandResult[m_updateCommandResult.size() - m_updateCommandPendingContacts.size()] = error;
    }

    if (!m_updateCommandPendingContacts.isEmpty()) {
        QContact newContact = m_updateCommandPendingContacts.takeFirst();
        ContactEntry *entry = m_contacts->value(newContact.detail<QContactGuid>().guid());
        if (entry) {
            entry->individual()->update(newContact, this, "updateContactsDone(galera::QIndividual*, const QString&)"); //));
        } else {
            updateContactsDone(0, "Contact not found!");
        }
    } else {
        folks_persona_store_flush(folks_individual_aggregator_get_primary_store(m_individualAggregator), 0, 0);
        QDBusMessage reply = m_updateCommandReplyMessage.createReply(m_updateCommandResult);
        QDBusConnection::sessionBus().send(reply);

        // clear command data
        m_updateCommandResult.clear();
        m_updateCommandReplyMessage = QDBusMessage();
    }
}


QString AddressBook::removeContact(FolksIndividual *individual)
{
    if (m_contacts->contains(individual)) {
        ContactEntry *ci = m_contacts->take(individual);
        if (ci) {
            QString id = QString::fromUtf8(folks_individual_get_id(individual));
            qDebug() << "Remove contact" << id;
            delete ci;
            return id;
        }
    }
    return QString();
}

QString AddressBook::addContact(FolksIndividual *individual)
{
    QString id = QString::fromUtf8(folks_individual_get_id(individual));
    ContactEntry *entry = m_contacts->value(id);
    if (entry) {
        entry->individual()->setIndividual(individual);
    } else {
        m_contacts->insert(new ContactEntry(new QIndividual(individual, m_individualAggregator)));
        qDebug() << "Add contact" << folks_individual_get_id(individual);
        //TODO: Notify view
    }
    return id;
}

void AddressBook::individualsChangedCb(FolksIndividualAggregator *individualAggregator,
                                       GeeMultiMap *changes,
                                       AddressBook *self)
{
    qDebug() << Q_FUNC_INFO;
    QStringList removedIds;
    QStringList addedIds;

    GeeIterator *iter;
    GeeSet *removed = gee_multi_map_get_keys(changes);
    GeeCollection *added = gee_multi_map_get_values(changes);

    iter = gee_iterable_iterator(GEE_ITERABLE(added));
    while(gee_iterator_next(iter)) {
        FolksIndividual *individual = FOLKS_INDIVIDUAL(gee_iterator_get(iter));
        if (individual) {
            // add contact to the map
            addedIds << self->addContact(individual);
            g_object_unref(individual);
        }
    }
    g_object_unref (iter);


    iter = gee_iterable_iterator(GEE_ITERABLE(removed));
    while(gee_iterator_next(iter)) {
        FolksIndividual *individual = FOLKS_INDIVIDUAL(gee_iterator_get(iter));
        if (individual) {
            QString id = QString::fromUtf8(folks_individual_get_id(individual));
            if (!addedIds.contains(id)) {
                // delete from contact map
                removedIds << self->removeContact(individual);
            }
            g_object_unref(individual);
        }
    }
    g_object_unref (iter);

    if (!removedIds.isEmpty()) {
        Q_EMIT self->m_adaptor->contactsRemoved(removedIds);
    }

    if (!addedIds.isEmpty()) {
        Q_EMIT self->m_adaptor->contactsAdded(addedIds);
    }
    self->m_initializing = false;
}

void AddressBook::aggregatorPrepareCb(GObject *source,
                                      GAsyncResult *res,
                                      AddressBook *self)
{
}

void AddressBook::createContactDone(FolksIndividualAggregator *individualAggregator,
                                    GAsyncResult *res,
                                    QDBusMessage *msg)
{
    qDebug() << "Create Contact Done" << msg;
    FolksPersona *persona;
    GError *error = NULL;
    QDBusMessage reply;
    persona = folks_individual_aggregator_add_persona_from_details_finish(individualAggregator, res, &error);
    if (error != NULL) {
        qWarning() << "Failed to create individual from contact:" << error->message;
        reply = msg->createErrorReply("Failed to create individual from contact", error->message);
        g_clear_error(&error);
    } else if (persona == NULL) {
        qWarning() << "Failed to create individual from contact: Persona already exists";
        reply = msg->createErrorReply("Failed to create individual from contact", "Contact already exists");
    } else {
        qDebug() << "Persona created:" << persona;
        FolksIndividual *individual;
        individual = folks_persona_get_individual(persona);
        reply = msg->createReply(QString::fromUtf8(folks_individual_get_id(individual)));
    }
    //TODO: use dbus connection
    QDBusConnection::sessionBus().send(reply);
    delete msg;
}

} //namespace
