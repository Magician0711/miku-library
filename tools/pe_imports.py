# Miku Library - Copyright (C) 2026 Magician0711
# SPDX-License-Identifier: GPL-3.0-only

import struct
from pathlib import Path

def imported_dlls(path):
    data = path.read_bytes()
    if data[:2] != b'MZ':
        return []
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe+4] != b'PE\0\0':
        raise RuntimeError('Invalid PE: ' + str(path))
    n_sections = struct.unpack_from('<H', data, pe + 6)[0]
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    optional = pe + 24
    magic = struct.unpack_from('<H', data, optional)[0]
    directory = optional + (112 if magic == 0x20b else 96)
    image_base = struct.unpack_from('<Q' if magic == 0x20b else '<I', data, optional + (24 if magic == 0x20b else 28))[0]
    sections = []
    for i in range(n_sections):
        section = optional + optional_size + 40*i
        virtual_size, address, raw_size, raw_offset = struct.unpack_from('<IIII', data, section + 8)
        sections.append((address, max(virtual_size, raw_size), raw_offset))
    def offset(rva):
        for address, size, raw in sections:
            if address <= rva < address + size:
                return raw + rva-address
        if rva < optional + optional_size:
            return rva
        raise ValueError('RVA outside PE sections')
    def name_at(rva):
        start = offset(rva)
        return data[start:data.index(b'\0', start)].decode('ascii').lower()
    names = set()
    for index, step in ((1,20), (13,32)):
        rva, size = struct.unpack_from('<II', data, directory + index*8)
        if not rva:
            continue
        at = offset(rva)
        for _ in range(max(1,size//step)):
            values = struct.unpack_from('<'+'I'*(step//4), data, at)
            if not any(values):
                break
            name_rva = values[3] if index == 1 else values[1]
            if index == 13 and not values[0] & 1:
                name_rva -= image_base
            names.add(name_at(name_rva))
            at += step
    return sorted(names)
