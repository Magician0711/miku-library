// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "NativeDialogs.h"

#include <QDir>
#include <QFileInfo>
#include <QWindow>

#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>

namespace {
struct Filter { const wchar_t *name; const wchar_t *pattern; };

template <class T> struct Com {
    T *p = nullptr;
    ~Com() { if (p) p->Release(); }
    T *operator->() const { return p; }
    T **out() { return &p; }
};

// Starting folder: start itself, or the nearest parent that exists.
void setStart(IFileDialog *dialog, const QString &start)
{
    if (start.isEmpty())
        return;
    QFileInfo info(start);
    QString folder = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    while (!QFileInfo(folder).isDir() && QFileInfo(folder).absolutePath() != folder)
        folder = QFileInfo(folder).absolutePath();
    Com<IShellItem> item;
    const auto native = QDir::toNativeSeparators(folder).toStdWString();
    if (SUCCEEDED(SHCreateItemFromParsingName(native.c_str(), nullptr, IID_PPV_ARGS(item.out()))))
        dialog->SetFolder(item.p);
}

QString run(IFileDialog *dialog, QWindow *parent, const QString &title)
{
    dialog->SetTitle(reinterpret_cast<const wchar_t *>(title.utf16()));
    if (FAILED(dialog->Show(parent ? reinterpret_cast<HWND>(parent->winId()) : nullptr)))
        return {};
    Com<IShellItem> item;
    if (FAILED(dialog->GetResult(item.out())))
        return {};
    PWSTR path = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
        return {};
    const QString result = QDir::fromNativeSeparators(QString::fromWCharArray(path));
    CoTaskMemFree(path);
    return result;
}

QString open(QWindow *parent, const QString &title, const QString &start, DWORD options, const Filter *filter)
{
    Com<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.out()))))
        return {};
    DWORD current = 0;
    dialog->GetOptions(&current);
    dialog->SetOptions(current | options | FOS_FORCEFILESYSTEM);
    if (filter) {
        const COMDLG_FILTERSPEC spec = {filter->name, filter->pattern};
        dialog->SetFileTypes(1, &spec);
    }
    setStart(dialog.p, start);
    return run(dialog.p, parent, title);
}
}

namespace NativeDialogs {
QString chooseDirectory(QWindow *parent, const QString &title, const QString &start)
{
    return open(parent, title, start, FOS_PICKFOLDERS, nullptr);
}

QString openExecutable(QWindow *parent, const QString &start)
{
    const Filter filter = {L"Windows 应用程序 (*.exe)", L"*.exe"};
    return open(parent, QStringLiteral("选择下载器程序"), start, FOS_FILEMUSTEXIST, &filter);
}

QString openJson(QWindow *parent, const QString &title)
{
    const Filter filter = {L"JSON 配置文件 (*.json)", L"*.json"};
    return open(parent, title, {}, FOS_FILEMUSTEXIST, &filter);
}

QString saveJson(QWindow *parent, const QString &title, const QString &fileName)
{
    Com<IFileSaveDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.out()))))
        return {};
    const COMDLG_FILTERSPEC spec = {L"JSON 配置文件 (*.json)", L"*.json"};
    dialog->SetFileTypes(1, &spec);
    dialog->SetDefaultExtension(L"json");
    const QFileInfo info(fileName);
    dialog->SetFileName(reinterpret_cast<const wchar_t *>(info.fileName().utf16()));
    if (info.isAbsolute())
        setStart(dialog.p, info.absolutePath());
    return run(dialog.p, parent, title);
}

void alert(const QString &title, const QString &text, bool error)
{
    MessageBoxW(nullptr, reinterpret_cast<const wchar_t *>(text.utf16()), reinterpret_cast<const wchar_t *>(title.utf16()),
                MB_OK | (error ? MB_ICONERROR : MB_ICONWARNING));
}
}
