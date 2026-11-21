// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "LanguageController.h"

#include "core/AppConfig.h"

#include <QCoreApplication>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

bool TableTranslator::load(const QString &path)
{
    m_table.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const auto root = QJsonDocument::fromJson(file.readAll()).object();
    for (auto it = root.begin(); it != root.end(); ++it)
        if (!it.value().toString().isEmpty())
            m_table.insert(it.key(), it.value().toString());
    return !m_table.isEmpty();
}

QString TableTranslator::translate(const char *, const char *sourceText, const char *, int) const
{
    // A null string tells Qt to fall back to the source text.
    return sourceText ? m_table.value(QString::fromUtf8(sourceText)) : QString();
}

LanguageController::LanguageController(AppConfig *config, QObject *parent)
    : QObject(parent), m_config(config)
{
    m_choice = codes().contains(config->language) || config->language == QLatin1String("system") ? config->language
                                                                                                   : QStringLiteral("system");
    apply();
}

QStringList LanguageController::codes()
{
    return {QStringLiteral("zh-CN"), QStringLiteral("zh-TW"), QStringLiteral("en"), QStringLiteral("ja")};
}

QStringList LanguageController::fontFamilies(const QString &language)
{
    // Han characters take the shapes each language expects; Latin text stays on the
    // system's interface face, which comes first.
    const bool traditional = language == QLatin1String("zh-TW"), japanese = language == QLatin1String("ja");
#if defined(Q_OS_WIN)
    QStringList families{QStringLiteral("Segoe UI Variable"), QStringLiteral("Segoe UI")};
    if (traditional)
        families << QStringLiteral("Microsoft JhengHei UI");
    else if (japanese)
        families << QStringLiteral("Yu Gothic UI") << QStringLiteral("Meiryo UI");
    families << QStringLiteral("Microsoft YaHei UI");
#elif defined(Q_OS_MACOS)
    QStringList families{QStringLiteral(".AppleSystemUIFont"), QStringLiteral("Helvetica Neue")};
    if (traditional)
        families << QStringLiteral("PingFang TC");
    else if (japanese)
        families << QStringLiteral("Hiragino Sans");
    families << QStringLiteral("PingFang SC");
#else
    QStringList families{QStringLiteral("Noto Sans"), QStringLiteral("Cantarell"), QStringLiteral("Ubuntu"), QStringLiteral("DejaVu Sans")};
    if (traditional)
        families << QStringLiteral("Noto Sans CJK TC") << QStringLiteral("Source Han Sans TC");
    else if (japanese)
        families << QStringLiteral("Noto Sans CJK JP") << QStringLiteral("Source Han Sans JP");
    families << QStringLiteral("Noto Sans CJK SC") << QStringLiteral("Source Han Sans SC") << QStringLiteral("WenQuanYi Micro Hei");
#endif
    return families;
}

QString LanguageController::resolve(const QString &choice, const QStringList &uiLanguages)
{
    if (codes().contains(choice))
        return choice;
    for (const auto &tag : uiLanguages) {
        const QString lower = tag.toLower();
        if (lower.startsWith(QLatin1String("zh")))
            return lower.contains(QLatin1String("hant")) || lower.endsWith(QLatin1String("-tw")) || lower.endsWith(QLatin1String("-hk"))
                    || lower.endsWith(QLatin1String("-mo"))
                ? QStringLiteral("zh-TW")
                : QStringLiteral("zh-CN");
        if (lower.startsWith(QLatin1String("ja")))
            return QStringLiteral("ja");
        if (lower.startsWith(QLatin1String("en")))
            return QStringLiteral("en");
    }
    // Neither Chinese nor Japanese: English is the one most people can read.
    return QStringLiteral("en");
}

QVariantList LanguageController::choices() const
{
    // Each language under its own name, so it can be found whatever is showing now.
    return {QVariantMap{{"text", QCoreApplication::translate("Miku", "跟随系统")}, {"value", "system"}},
            QVariantMap{{"text", QStringLiteral("简体中文")}, {"value", "zh-CN"}},
            QVariantMap{{"text", QStringLiteral("繁體中文")}, {"value", "zh-TW"}},
            QVariantMap{{"text", QStringLiteral("English")}, {"value", "en"}},
            QVariantMap{{"text", QStringLiteral("日本語")}, {"value", "ja"}}};
}

bool LanguageController::choose(const QString &code)
{
    if (code != QLatin1String("system") && !codes().contains(code))
        return false;
    if (code == m_choice)
        return true;
    m_choice = code;
    m_config->language = code;
    apply();
    return m_config->save();
}

void LanguageController::apply()
{
    m_language = resolve(m_choice, QLocale::system().uiLanguages());
    if (m_installed) {
        QCoreApplication::removeTranslator(&m_translator);
        m_installed = false;
    }
    if (m_language != QLatin1String("zh-CN") && m_translator.load(QStringLiteral(":/i18n/%1.json").arg(m_language)))
        m_installed = QCoreApplication::installTranslator(&m_translator);

    QFont font = QGuiApplication::font();
    font.setFamilies(fontFamilies(m_language));
    QGuiApplication::setFont(font);
    QLocale::setDefault(QLocale(QString(m_language).replace(QLatin1Char('-'), QLatin1Char('_'))));
    emit changed();
}
