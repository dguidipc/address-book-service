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

#ifndef __GALERA_QINDIVIDUAL_H__
#define __GALERA_QINDIVIDUAL_H__

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMultiHash>
#include <QVersitProperty>

#include <QtContacts/QContact>
#include <QtContacts/QContactDetail>

#include <folks/folks.h>

namespace galera
{
typedef void (*UpdateDoneCB)(const QString&, void*);
typedef GHashTable* (ParseDetailsFunc)(GHashTable*, const QList<QtContacts::QContactDetail> &);

typedef QList<QtVersit::QVersitProperty> PropertyList;
class QIndividual
{
public:
    enum Field {
        All = 0x0000,
        Name = 0x0001,
        FullName = 0x0002,
        NickName = 0x0004,
        Birthday = 0x0008,
        Photo = 0x0016,
        Role = 0x0032,
        Email = 0x0064,
        Phone = 0x0128,
        Address = 0x0256,
        Im = 0x512,
        TimeZone = 0x1024,
        Url = 0x2048
    };
    Q_DECLARE_FLAGS(Fields, Field)

    QIndividual(FolksIndividual *individual, FolksIndividualAggregator *aggregator);
    ~QIndividual();

    QtContacts::QContact &contact();
    QtContacts::QContact copy(Fields fields = QIndividual::All);
    bool update(const QString &vcard, UpdateDoneCB cb, void* data);
    bool update(const QtContacts::QContact &contact, UpdateDoneCB cb, void* data);
    FolksIndividual *individual() const;
    void setIndividual(FolksIndividual *individual);

    static GHashTable *parseDetails(const QtContacts::QContact &contact);

private:
    FolksIndividual *m_individual;
    FolksPersona *m_primaryPersona;
    FolksIndividualAggregator *m_aggregator;
    QtContacts::QContact m_contact;
    QMap<QString, QPair<QtContacts::QContactDetail, FolksAbstractFieldDetails*> > m_fieldsMap;


    bool fieldsContains(Fields fields, Field value) const;
    QList<QtVersit::QVersitProperty> parseFieldList(const QString &fieldName, GeeSet *values) const;
    QMultiHash<QString, QString> parseDetails(FolksAbstractFieldDetails *details) const;
    void updateContact();

    FolksPersona *primaryPersona();
    QtContacts::QContactDetail detailFromUri(QtContacts::QContactDetail::DetailType type, const QString &uri) const;

    // QContact
    QList<QtContacts::QContactDetail> getDetails() const;
    void appendDetailsForPersona(QList<QtContacts::QContactDetail> *list, QtContacts::QContactDetail detail, const QString &personaIndex, bool readOnly) const;
    void appendDetailsForPersona(QList<QtContacts::QContactDetail> *list, QList<QtContacts::QContactDetail> details, const QString &personaIndex, bool readOnly) const;

    QtContacts::QContactDetail getUid() const;
    QList<QtContacts::QContactDetail> getClientPidMap() const;
    QtContacts::QContactDetail getPersonaName(FolksPersona *persona) const;
    QtContacts::QContactDetail getPersonaFullName(FolksPersona *persona) const;
    QtContacts::QContactDetail getPersonaNickName(FolksPersona *persona) const;
    QtContacts::QContactDetail getPersonaBirthday(FolksPersona *persona) const;
    QtContacts::QContactDetail getPersonaPhoto(FolksPersona *persona) const;
    //TODO: organization
    QList<QtContacts::QContactDetail> getPersonaRoles(FolksPersona *persona) const;
    QList<QtContacts::QContactDetail> getPersonaEmails(FolksPersona *persona) const;
    QList<QtContacts::QContactDetail> getPersonaPhones(FolksPersona *persona) const;
    QList<QtContacts::QContactDetail> getPersonaAddresses(FolksPersona *persona) const;
    QList<QtContacts::QContactDetail> getPersonaIms(FolksPersona *persona) const;
    QList<QtContacts::QContactDetail> getPersonaUrls(FolksPersona *persona) const;


    // update
    void updateFullName(const QtContacts::QContactDetail &name, void* data);
    void updateName(const QtContacts::QContactDetail &name, void* data);
    void updateNickname(const QtContacts::QContactDetail &detail, void* data);
    void updateBirthday(const QtContacts::QContactDetail &detail, void* data);
    void updatePhoto(const QtContacts::QContactDetail &detail, void* data);
    void updateTimezone(const QtContacts::QContactDetail &detail, void* data);
    void updateRole(QtContacts::QContactDetail detail, void* data);
    void updatePhone(QtContacts::QContactDetail detail, void* data);
    void updateEmail(QtContacts::QContactDetail detail, void* data);
    void updateIm(QtContacts::QContactDetail detail, void* data);
    void updateUrl(QtContacts::QContactDetail details, void* data);
    void updateNote(QtContacts::QContactDetail detail, void* data);
    void updateAddress(QtContacts::QContactDetail detail, void* data);
    void createPersonaForDetail(QList<QtContacts::QContactDetail> detail, ParseDetailsFunc parseFunc, void *data) const;

    static void createPersonaDone(GObject *detail, GAsyncResult *result, gpointer userdata);
    static void updateDetailsDone(GObject *detail, GAsyncResult *result, gpointer userdata);
    static QString callDetailChangeFinish(QtContacts::QContactDetail::DetailType type, FolksPersona *persona, GAsyncResult *result);
    //static bool detailListIsEqual(QList<QtContacts::QContactDetail> original, QList<QtContacts::QContactDetail> details);

    // translate details
    static GHashTable *parseAddressDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parsePhotoDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parsePhoneNumbersDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseOrganizationDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseImDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseNoteDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseFullNameDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseNicknameDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseNameDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseGenderDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseFavoriteDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseEmailDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseBirthdayDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseUrlDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);


    // parse context and parameters
    static void parseParameters(QtContacts::QContactDetail &detail, FolksAbstractFieldDetails *fd);
    static void parsePhoneParameters(QtContacts::QContactDetail &phone, const QStringList &parameters);
    static void parseAddressParameters(QtContacts::QContactDetail &address, const QStringList &parameters);
    static void parseOnlineAccountParameters(QtContacts::QContactDetail &im, const QStringList &parameters);

    static void parseContext(FolksAbstractFieldDetails *fd, const QtContacts::QContactDetail &detail);
    static QStringList parseContext(const QtContacts::QContactDetail &detail);
    static QStringList parsePhoneContext(const QtContacts::QContactDetail &detail);
    static QStringList parseAddressContext(const QtContacts::QContactDetail &detail);
    static QStringList parseOnlineAccountContext(const QtContacts::QContactDetail &detail);

    static QStringList listParameters(FolksAbstractFieldDetails *details);
    static QStringList listContext(const QtContacts::QContactDetail &detail);
    static QList<int> contextsFromParameters(QStringList &parameters);

    static int onlineAccountProtocolFromString(const QString &protocol);
    static QString onlineAccountProtocolFromEnum(int protocol);


    // glib callbacks
    static void folksIndividualAggregatorAddPersonaFromDetailsDone(GObject *source,
                                                                   GAsyncResult *res,
                                                                   QIndividual *individual);
    friend class QIndividualUtils;
};

} //namespace
#endif

