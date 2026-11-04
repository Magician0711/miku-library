// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "Models.h"

#include <functional>
#include <memory>

class HttpClient;

class SearchProvider {
public:
    virtual ~SearchProvider() = default;
    virtual void search(const QString &term,
                        std::function<void(QVector<ResourceResult>, QString)> callback) = 0;
    virtual void testConnection(std::function<void(bool, QString)> callback);

protected:
    SearchProvider(const SiteEntry &site, HttpClient *http)
        : m_site(site)
        , m_http(http)
    {
    }

    SiteEntry m_site;
    HttpClient *m_http;
};

std::unique_ptr<SearchProvider> makeProvider(const SiteEntry &site, HttpClient *http);
