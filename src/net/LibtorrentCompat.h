// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// The program is written against libtorrent 2.1 (vcpkg on Windows). Linux distributions
// and Homebrew ship 2.0.x, from 2.0.7 on (load_torrent.hpp); these bridge the difference.

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/announce_entry.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/version.hpp>

#include <memory>

namespace ltcompat {

// The files of a torrent: torrent_info::layout() in 2.1, files() before.
inline const libtorrent::file_storage &layout(const libtorrent::torrent_info &info)
{
#if LIBTORRENT_VERSION_NUM >= 20100
    return info.layout();
#else
    return info.files();
#endif
}

// What writing the torrent back to disk needs, with the metadata. 2.1 hands it over in
// one call; 2.0 only has the asynchronous resume-data alert, so it is put together here.
inline libtorrent::add_torrent_params torrentParams(const libtorrent::torrent_handle &handle)
{
#if LIBTORRENT_VERSION_NUM >= 20100
    return handle.get_resume_data(libtorrent::torrent_handle::save_info_dict);
#else
    libtorrent::add_torrent_params params;
    params.ti = std::const_pointer_cast<libtorrent::torrent_info>(handle.torrent_file());
    params.info_hashes = handle.info_hashes();
    params.save_path = handle.status(libtorrent::torrent_handle::query_save_path).save_path;
    for (const auto &tracker : handle.trackers()) {
        params.trackers.push_back(tracker.url);
        params.tracker_tiers.push_back(tracker.tier);
    }
    return params;
#endif
}

}
