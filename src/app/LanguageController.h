// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QHash>
#include <QObject>
#include <QTranslator>
#include <QVariantList>

struct AppConfig;

// Translations keyed by the Simplified Chinese source text, read from i18n/<code>.json.
// Contexts are ignored: the same phrase reads the same everywhere in the interface.
class TableTranslator final : public QTranslator {
public:
    bool load(const QString &path);
    QString translate(const char *context, const char *sourceText, const char *disambiguation, int n) const override;
    bool isEmpty() const override { return m_table.isEmpty(); }
    int size() const { return int(m_table.size()); }

private:
    QHash<QString, QString> m_table;
};

// The interface language. Simplified Chinese is the source; the others are tables
// installed on top of it. A change applies at once: QML rebinds every qsTr, and
// whoever listens to changed() rebuilds the texts it made in C++.
class LanguageController final : public QObject {
    Q_OBJECT
    // What the user chose: "system" or a language code.
    Q_PROPERTY(QString choice READ choice NOTIFY changed)
    // The language in effect, never "system".
    Q_PROPERTY(QString language READ language NOTIFY changed)
    Q_PROPERTY(QVariantList choices READ choices NOTIFY changed)
public:
    explicit LanguageController(AppConfig *config, QObject *parent = nullptr);

    QString choice() const { return m_choice; }
    QString language() const { return m_language; }
    QVariantList choices() const;

    // Saves the choice; false when the configuration could not be written.
    Q_INVOKABLE bool choose(const QString &code);

    static QStringList codes();
    // "system" resolved against the system's preferred languages.
    static QString resolve(const QString &choice, const QStringList &uiLanguages);
    // The interface font for a language on this system, most preferred first.
    static QStringList fontFamilies(const QString &language);

signals:
    void changed();

private:
    void apply();

    AppConfig *m_config;
    QString m_choice, m_language;
    TableTranslator m_translator;
    bool m_installed = false;
};
