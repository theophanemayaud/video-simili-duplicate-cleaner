#include "video_matching_fixture_manifest.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QTextStream>

namespace
{
QStringList parseCsvRow(const QString& row, bool& valid)
{
    QStringList fields;
    QString field;
    bool quoted = false;

    for (qsizetype index = 0; index < row.size(); ++index) {
        const QChar character = row.at(index);
        if (character == '"') {
            if (quoted && index + 1 < row.size() && row.at(index + 1) == '"') {
                field += '"';
                ++index;
            }
            else {
                quoted = !quoted;
            }
        }
        else if (character == ',' && !quoted) {
            fields.append(field);
            field.clear();
        }
        else {
            field += character;
        }
    }

    fields.append(field);
    valid = !quoted;
    return fields;
}

bool readInteger(const QString& value, const QString& column, int lineNumber, int& result, QString& error)
{
    bool valid = false;
    result = value.toInt(&valid);
    if (!valid) {
        error = QStringLiteral("Line %1 has a non-integer %2: %3").arg(lineNumber).arg(column, value);
        return false;
    }
    return true;
}
} // namespace

bool MatchingFixtureManifest::load(const QString& path, QList<MatchingFixtureRecord>& records, QString& error)
{
    records.clear();
    error.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QStringLiteral("Could not open manifest: %1").arg(QFileInfo(path).absoluteFilePath());
        return false;
    }

    QTextStream stream(&file);
    if (stream.atEnd()) {
        error = QStringLiteral("Manifest is empty: %1").arg(path);
        return false;
    }

    bool validRow = false;
    const QStringList header = parseCsvRow(stream.readLine(), validRow);
    const QStringList expectedHeader = {
        QStringLiteral("file"),
        QStringLiteral("content_group"),
        QStringLiteral("expected_processing"),
        QStringLiteral("matching_orientation_degrees"),
        QStringLiteral("tags"),
    };
    if (!validRow || header != expectedHeader) {
        error = QStringLiteral("Unexpected manifest header in %1").arg(path);
        return false;
    }

    QSet<QString> files;
    int lineNumber = 1;
    while (!stream.atEnd()) {
        ++lineNumber;
        const QString row = stream.readLine();
        if (row.trimmed().isEmpty())
            continue;

        const QStringList fields = parseCsvRow(row, validRow);
        if (!validRow || fields.size() != expectedHeader.size()) {
            error = QStringLiteral("Malformed CSV row at line %1").arg(lineNumber);
            return false;
        }

        MatchingFixtureRecord record;
        record.file = fields.at(0);
        record.contentGroup = fields.at(1);
        if (record.file.isEmpty() || record.contentGroup.isEmpty()) {
            error = QStringLiteral("Line %1 must provide file and content_group").arg(lineNumber);
            return false;
        }
        if (files.contains(record.file)) {
            error = QStringLiteral("Duplicate manifest file at line %1: %2").arg(lineNumber).arg(record.file);
            return false;
        }
        files.insert(record.file);

        if (fields.at(2) == QStringLiteral("success"))
            record.expectedProcessing = true;
        else if (fields.at(2) == QStringLiteral("failure"))
            record.expectedProcessing = false;
        else {
            error = QStringLiteral("Line %1 has invalid expected_processing: %2").arg(lineNumber).arg(fields.at(2));
            return false;
        }

        if (!readInteger(fields.at(3), QStringLiteral("matching_orientation_degrees"), lineNumber,
                         record.matchingOrientationDegrees, error))
            return false;

        const QStringList tags = fields.at(4).split('|', Qt::SkipEmptyParts);
        for (const QString& tag : tags)
            record.tags.insert(tag.trimmed());
        records.append(record);
    }

    if (records.isEmpty()) {
        error = QStringLiteral("Manifest has no fixture rows: %1").arg(path);
        return false;
    }
    return true;
}
