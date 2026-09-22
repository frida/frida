from __future__ import annotations

from pathlib import Path

import frida_bindgen_core as core

from .model import FACTORY, Customizations, Model

# The Swift binding models GLib/Gio helper objects (streams, cancellables,
# socket addresses) in separately compiled Swift modules, so unlike the Python
# binding we pull in no Gio object types. FileMonitorEvent is the exception: it
# is the payload of FileMonitor's generated `change` signal.
INCLUDED_GIO_OBJECT_TYPES: list[str] = []
INCLUDED_GIO_ENUMERATIONS: list[str] = ["FileMonitorEvent"]


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
        seed_object_first=True,
    )
