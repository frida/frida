# SPDX-License-Identifier: Apache-2.0
# Copyright 2021 The Meson development team

from pathlib import Path
import pickle

from .loaderbase import LoaderBase
from .model import ReferenceManual

class LoaderPickle(LoaderBase):
    def __init__(self, in_file: Path) -> None:
        super().__init__()
        self.in_file = in_file

    def load_impl(self) -> ReferenceManual:
        res = pickle.loads(self.in_file.read_bytes())
        assert isinstance(res, ReferenceManual)
        return res

    # Assume that the pickled data is OK and skip validation
    def load(self) -> ReferenceManual:
        return self.load_impl()
