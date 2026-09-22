from __future__ import annotations

from pathlib import Path

import frida_bindgen_core as core

from .model import FACTORY, Customizations, Model

INCLUDED_GIO_OBJECT_TYPES = [
    "Cancellable",
    "IOStream",
    "InputStream",
    "OutputStream",
    "InetSocketAddress",
    "InetAddress",
    "UnixSocketAddress",
    "SocketAddress",
    "SocketAddressEnumerator",
    "SocketConnectable",
]
INCLUDED_GIO_ENUMERATIONS = [
    "FileMonitorEvent",
    "SocketFamily",
    "UnixSocketAddressType",
]


def compute_model(
    frida_gir: Path,
    glib_gir: Path,
    gobject_gir: Path,
    gio_gir: Path,
    customizations: Customizations,
) -> Model:
    return core.compute_model(
        frida_gir,
        glib_gir,
        gobject_gir,
        gio_gir,
        customizations,
        FACTORY,
        INCLUDED_GIO_OBJECT_TYPES,
        INCLUDED_GIO_ENUMERATIONS,
        seed_object_first=False,
    )
