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

#include "config.h"
#include "dummy-backend.h"
#include "scoped-loop.h"

#include "lib/contacts-map.h"
#include "lib/qindividual.h"

#include <QObject>
#include <QtTest>
#include <QDebug>

#include <QtContacts>

#include <glib.h>
#include <gio/gio.h>

class ContactMapTest : public QObject
{
    Q_OBJECT

private:
    DummyBackendProxy *m_dummy;
    galera::ContactsMap m_map;
    QList<FolksIndividual*> m_individuals;

    int randomIndex() const
    {
        return qrand() % m_map.size();
    }

    FolksIndividual *randomIndividual() const
    {
        return m_individuals[randomIndex()];
    }

    void createContactWithSuffix(const QString &suffix)
    {
        QtContacts::QContact contact;
        QtContacts::QContactName name;
        name.setFirstName(QString("Fulano_%1").arg(suffix));
        name.setMiddleName("de");
        name.setLastName("Tal");
        contact.saveDetail(&name);

        QtContacts::QContactEmailAddress email;
        email.setEmailAddress(QString("fulano_%1@ubuntu.com").arg(suffix));
        contact.saveDetail(&email);

        QtContacts::QContactPhoneNumber phone;
        phone.setNumber(QString("33331410%1").arg(suffix));
        contact.saveDetail(&phone);

        m_dummy->createContact(contact);
    }

    void createContactWithPhone(const QString &phoneNumber)
    {
        QtContacts::QContact contact;
        QtContacts::QContactName name;
        name.setFirstName("Foo");
        name.setLastName("Bar");
        contact.saveDetail(&name);

        QtContacts::QContactEmailAddress email;
        email.setEmailAddress("foo@ubuntu.com");
        contact.saveDetail(&email);

        QtContacts::QContactPhoneNumber phone;
        phone.setNumber(phoneNumber);
        contact.saveDetail(&phone);

        m_dummy->createContact(contact);
    }

    QStringList allPhones()
    {
        QStringList phones;
        phones << "12345678"
               << "1234-5678"
               << "+55(81)87042155"
               << "(81)87042155"
               << "12345678#123"
               << "(123)12345678#1"
               << "1234567#1"
               << "190"
               << "911"
               << "abcdefg"
               << "abc12345678"
               << "+352 691 123456"
               << "32634146"
               << "32634911";
        return phones;
    }

private Q_SLOTS:
    void initTestCase()
    {
        m_dummy = new DummyBackendProxy();
        m_dummy->start();
        QTRY_VERIFY(m_dummy->isReady());

        createContactWithSuffix("1");
        createContactWithSuffix("2");
        createContactWithSuffix("3");

        Q_FOREACH(galera::QIndividual *i, m_dummy->individuals()) {
            m_map.insert(new galera::ContactEntry(new galera::QIndividual(i->individual(), m_dummy->aggregator())));
            m_individuals << i->individual();
        }
    }

    void cleanupTestCase()
    {
        m_dummy->shutdown();
        delete m_dummy;
        m_map.clear();
    }

    void testLookupByFolksIndividual()
    {
        QVERIFY(m_map.size() > 0);
        FolksIndividual *fIndividual = randomIndividual();

        QVERIFY(m_map.contains(fIndividual));

        galera::ContactEntry *entry = m_map.value(fIndividual);
        QVERIFY(entry->individual()->individual() == fIndividual);
    }

    void testLookupByFolksIndividualId()
    {
        FolksIndividual *fIndividual = randomIndividual();
        QString id = QString::fromUtf8(folks_individual_get_id(fIndividual));
        galera::ContactEntry *entry = m_map.value(id);
        QVERIFY(entry->individual()->individual() == fIndividual);
    }

    void testValues()
    {
        QList<galera::ContactEntry*> entries = m_map.values();
        QCOMPARE(entries.size(), m_map.size());
        QCOMPARE(m_individuals.size(), m_map.size());

        Q_FOREACH(FolksIndividual *individual, m_individuals) {
            QVERIFY(m_map.contains(individual));
        }
    }

    void testTakeIndividual()
    {
        FolksIndividual *individual = folks_individual_new(0);

        QVERIFY(m_map.take(individual) == 0);
        QVERIFY(!m_map.contains(individual));

        g_object_unref(individual);

        individual = randomIndividual();
        galera::ContactEntry *entry = m_map.take(individual);
        QVERIFY(entry->individual()->individual() == individual);

        //put it back
        m_map.insert(entry);
    }

    void testLookupByVcard()
    {
        FolksIndividual *individual = randomIndividual();
        QString id = QString::fromUtf8(folks_individual_get_id(individual));
        QString vcard = QString("BEGIN:VCARD\r\n"
                "VERSION:3.0\r\n"
                "UID:%1\r\n"
                "N:Gump;Forrest\r\n"
                "FN:Forrest Gump\r\n"
                "REV:2008-04-24T19:52:43Z\r\n").arg(id);
        galera::ContactEntry *entry = m_map.valueFromVCard(vcard);
        QVERIFY(entry);
        QVERIFY(entry->individual()->individual() == individual);
    }

    void testLookupByPhone_data()
    {
        QStringList phones = allPhones();
        Q_FOREACH(const QString &phone, phones) {
            createContactWithPhone(phone);
        }
        Q_FOREACH(galera::QIndividual *i, m_dummy->individuals()) {
            m_map.insert(new galera::ContactEntry(new galera::QIndividual(i->individual(), m_dummy->aggregator())));
            m_individuals << i->individual();
        }

        QTest::addColumn<QString>("query");
        QTest::addColumn<int>("numberOfMatches");

        QTest::newRow("string equal") << "12345678" << 3;
        QTest::newRow("number with dash") << "1234-5678" << 3;
        QTest::newRow("plain number") << "87042155" << 2;
        QTest::newRow("number with area code") << "(81)87042155" << 2;
        QTest::newRow("number with country code") << "+55(81)87042155" << 2;
        QTest::newRow("number with extension") << "12345678" << 3;
        QTest::newRow("both numbers with extension") << "12345678#1" << 1;
        QTest::newRow("numbers with different extension") << "1234567#2" << 0;
        QTest::newRow("short/emergency numbers") << "190" << 1;
        QTest::newRow("different short/emergency numbers") << "11" << 0;
        QTest::newRow("different numbers") << "1234567" << 0;
        QTest::newRow("both non phone numbers") << "abcdefg" << 0;
        QTest::newRow("different non phone numbers") << "bcdefg" << 0;
        QTest::newRow("phone number and custom string") << "bcdefg12345678" << 3;
        QTest::newRow("partial match") << "+352 691 123456" << 1;
        QTest::newRow("Phone number and small value") << "146" << 0;
        QTest::newRow("Small phone numbers") << "911" << 1;
        QTest::newRow("Phone number and small number") << "+146" << 0;
    }

    void testLookupByPhone()
    {
        QFETCH(QString, query);
        QFETCH(int, numberOfMatches);

        QList<galera::ContactEntry*> entries = m_map.valueByPhone(query);
        QCOMPARE(entries.size(), numberOfMatches);
    }
};

QTEST_MAIN(ContactMapTest)

#include "contactmap-test.moc"
