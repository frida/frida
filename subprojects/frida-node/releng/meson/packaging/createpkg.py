#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2017-2021 The Meson development team

import subprocess
import shutil, sys, os
from glob import glob

import xml.etree.ElementTree as ET

sys.path.append(os.getcwd())
from mesonbuild import coredata

class PkgGenerator:

    def __init__(self):
        self.pkg_dir = 'macpkg'
        self.sharedir = os.path.join(self.pkg_dir, 'usr/local/share')
        self.bindir = os.path.join(self.pkg_dir, 'usr/local/bin')
        self.product_name = 'Meson Build System'
        self.identifier = 'com.mesonbuild.meson'
        self.version = coredata.version.replace('dev', '')
        self.mesonstashdir = os.path.join(self.sharedir, f'meson-{self.version}')
        self.pkgname = 'meson.pkg'
        self.productname = f'meson-{self.version}.pkg'
        self.distribution_file = 'meson-distribution.xml'
        self.resourcedir = 'packaging/macpages'

    def build_dist(self):
        if os.path.exists(self.pkg_dir):
            shutil.rmtree(self.pkg_dir)
        os.mkdir(self.pkg_dir)
        pyinstaller_bin = glob('/Users/jpakkane/Library/Python/*/bin/pyinstaller')
        if len(pyinstaller_bin) != 1:
            sys.exit('Could not determine unique installer.')
        pyinstaller_bin = pyinstaller_bin[0]
        pyinst_cmd = [pyinstaller_bin,
                      '--clean',
                      '--additional-hooks-dir=packaging',
                      '--distpath',
                      self.pkg_dir]
        pyinst_cmd += ['meson.py']
        subprocess.check_call(pyinst_cmd)
        tmpdir = os.path.join(self.pkg_dir, 'meson')
        shutil.move(tmpdir, self.mesonstashdir)
        os.makedirs(self.bindir)
        ln_base = os.path.relpath(self.mesonstashdir, self.bindir)
        ninja_bin = shutil.which('ninja')
        assert ninja_bin
        shutil.copy(ninja_bin, self.bindir)
        subprocess.check_call(['strip', os.path.join(self.bindir, 'ninja')])
        os.symlink(os.path.join(ln_base, 'meson'), os.path.join(self.bindir, 'meson'))

    def build_package(self):
        subprocess.check_call(['pkgbuild',
                               '--root',
                               self.pkg_dir,
                               '--identifier',
                               self.identifier,
                               self.pkgname])
        self.generate_distribution()
        subprocess.check_call(['productbuild',
                               '--distribution',
                               self.distribution_file,
                               '--resources',
                               self.resourcedir,
                               self.productname])

    def generate_distribution(self):
        root = ET.Element('installer-gui-script', {'minSpecVersion': '1'})
        ET.SubElement(root, 'welcome', {'file': 'welcome.html',
                                        'mime-type': 'text/html'})
        ET.SubElement(root, 'license', {'file': 'license.html',
                                        'mime-type': 'text/html'})
        ET.SubElement(root, 'conclusion', {'file': 'conclusion.html',
                                        'mime-type': 'text/html'})
        ET.SubElement(root, 'pkg-ref', {'id': self.identifier})
        ET.SubElement(root, 'options', {'customize': 'never',
                                        'require-scripts': 'false',
                                        'hostArchitectures': 'x86_64,arm64'})
        choices_outline = ET.SubElement(root, 'choices-outline')
        line = ET.SubElement(choices_outline, 'line', {'choice': 'default'})
        ET.SubElement(line, 'line', {'choice': self.identifier})
        ET.SubElement(root, 'choice', {'id': 'default'})
        choice = ET.SubElement(root, 'choice', {'id': self.identifier, 'visible': 'false'})
        ET.SubElement(choice, 'pkg-ref', {'id': self.identifier})
        ET.SubElement(root, 'pkg-ref', {'id': self.identifier,
                                        'version': '0', # self.version,
                                        'onConclusion': 'none'}).text = self.pkgname
        ET.ElementTree(root).write(self.distribution_file, encoding='utf-8', xml_declaration=True)
        # ElementTree cannot do pretty-printing, so do it manually
        import xml.dom.minidom
        doc = xml.dom.minidom.parse(self.distribution_file)
        with open(self.distribution_file, 'w') as open_file:
            open_file.write(doc.toprettyxml())

    def remove_tempfiles(self):
        shutil.rmtree('macpkg')
        os.unlink('meson-distribution.xml')
        os.unlink('meson.pkg')
        os.unlink('meson.spec')

if __name__ == '__main__':
    if not os.path.exists('meson.py'):
        sys.exit(print('Run me in the top level source dir.'))
    subprocess.check_call(['pip3', 'install', '--user', '--upgrade', 'pyinstaller'])

    pg = PkgGenerator()
    pg.build_dist()
    pg.build_package()
    pg.remove_tempfiles()
