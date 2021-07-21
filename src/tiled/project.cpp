/*
 * project.cpp
 * Copyright 2019, Thorbjørn Lindeijer <bjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "project.h"
#include "preferences.h"
#include "properties.h"
#include "savefile.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "qtcompat_p.h"

namespace Tiled {

static QString relative(const QDir &dir, const QString &fileName)
{
    QString rel = dir.relativeFilePath(fileName);
    return rel.isEmpty() ? QStringLiteral(".") : rel;
}

static QString absolute(const QDir &dir, const QString &fileName)
{
    if (fileName.isEmpty())
        return QString();

    return QDir::cleanPath(dir.absoluteFilePath(fileName));
}

Project::Project()
{
}

bool Project::save()
{
    if (!mFileName.isEmpty())
        return save(mFileName);
    return false;
}

bool Project::save(const QString &fileName)
{
    QString extensionsPath = mExtensionsPath;

    // Initialize extensions path to its default value
    if (mFileName.isEmpty() && extensionsPath.isEmpty())
        extensionsPath = QFileInfo(fileName).dir().filePath(QLatin1String("extensions"));

    const QDir dir = QFileInfo(fileName).dir();

    QJsonArray folders;
    for (auto &folder : qAsConst(mFolders))
        folders.append(relative(dir, folder));

    QJsonArray commands;
    for (const Command &command : qAsConst(mCommands))
        commands.append(QJsonObject::fromVariantHash(command.toVariant()));

    QJsonArray propertyTypes;
    for (const PropertyType &type : qAsConst(mPropertyTypes))
        propertyTypes.append(QJsonObject::fromVariantHash(type.toVariant()));

    const QJsonObject project {
        { QStringLiteral("propertyTypes"), propertyTypes },
        { QStringLiteral("folders"), folders },
        { QStringLiteral("extensionsPath"), relative(dir, extensionsPath) },
        { QStringLiteral("objectTypesFile"), dir.relativeFilePath(mObjectTypesFile) },
        { QStringLiteral("automappingRulesFile"), dir.relativeFilePath(mAutomappingRulesFile) },
        { QStringLiteral("commands"), commands }
    };

    const QJsonDocument document(project);

    SaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    file.device()->write(document.toJson());
    if (!file.commit())
        return false;

    mLastSaved = QFileInfo(fileName).lastModified();
    mFileName = fileName;
    mExtensionsPath = extensionsPath;
    return true;
}

bool Project::load(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QJsonParseError error;
    const QByteArray json = file.readAll();
    const QJsonDocument document(QJsonDocument::fromJson(json, &error));
    if (error.error != QJsonParseError::NoError)
        return false;

    mFileName = fileName;

    const QDir dir = QFileInfo(fileName).dir();

    const QJsonObject project = document.object();

    mExtensionsPath = absolute(dir, project.value(QLatin1String("extensionsPath")).toString(QLatin1String("extensions")));
    mObjectTypesFile = absolute(dir, project.value(QLatin1String("objectTypesFile")).toString());
    mAutomappingRulesFile = absolute(dir, project.value(QLatin1String("automappingRulesFile")).toString());


    mPropertyTypes.clear();
    const QJsonArray propertyTypes = project.value(QLatin1String("propertyTypes")).toArray();
    for (const QJsonValue &typeValue : propertyTypes) {
        PropertyType propertyType = PropertyType::fromVariant(typeValue.toVariant());
        mPropertyTypes.append(propertyType);
    }

    mFolders.clear();
    const QJsonArray folders = project.value(QLatin1String("folders")).toArray();
    for (const QJsonValue &folderValue : folders)
        mFolders.append(QDir::cleanPath(dir.absoluteFilePath(folderValue.toString())));

    mCommands.clear();
    const QJsonArray commands = project.value(QLatin1String("commands")).toArray();
    for (const QJsonValue &commandValue : commands)
        mCommands.append(Command::fromVariant(commandValue.toVariant()));

    //load actual new custom properties into the preferences
    Preferences *prefs = Preferences::instance();
    prefs->setPropertyTypes(mPropertyTypes);

    return true;
}

void Project::addFolder(const QString &folder)
{
    mFolders.append(folder);
}

void Project::removeFolder(int index)
{
    Q_ASSERT(index >= 0 && index < mFolders.size());
    mFolders.removeAt(index);
}

} // namespace Tiled
