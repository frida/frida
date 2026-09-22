from __future__ import annotations

from collections import OrderedDict
from pathlib import Path

from typing import List

from .model import Factory, Model, parse_gir


def compute_model(
    frida_gir: Path,
    glib_gir: Path,
    gobject_gir: Path,
    gio_gir: Path,
    customizations: object,
    factory: Factory,
    included_gio_object_types: List[str],
    included_gio_enumerations: List[str],
    seed_object_first: bool,
) -> Model:
    glib = parse_gir(glib_gir, [], factory)
    gobject = parse_gir(gobject_gir, [glib], factory)
    gio = parse_gir(gio_gir, [glib, gobject], factory)
    frida = parse_gir(frida_gir, [glib, gobject, gio], factory)

    object_types = OrderedDict()
    if seed_object_first:
        object_types["Object"] = gobject.object_types["Object"]
        object_types.update(frida.object_types)
    else:
        object_types.update(frida.object_types)
        object_types["Object"] = gobject.object_types["Object"]
    for t in included_gio_object_types:
        object_types[t] = gio.object_types[t]

    enumerations = OrderedDict(frida.enumerations)
    for t in included_gio_enumerations:
        enumerations[t] = gio.enumerations[t]

    model = factory.model(
        frida.namespace,
        object_types,
        enumerations,
        customizations,
        error_domain=frida.error_domain,
        factory=factory,
    )

    for t in object_types.values():
        t.model = model
    for t in enumerations.values():
        t.model = model
    if model.error_domain is not None:
        model.error_domain.model = model

    return model
