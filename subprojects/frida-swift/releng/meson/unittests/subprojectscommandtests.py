# SPDX-License-Identifier: Apache-2.0
# Copyright 2016-2021 The Meson development team

import subprocess
import tempfile
import textwrap
import os
from pathlib import Path
import typing as T

from mesonbuild.mesonlib import (
    version_compare, git, search_version
)



from .baseplatformtests import BasePlatformTests
from .helpers import *

class SubprojectsCommandTests(BasePlatformTests):
    def setUp(self):
        super().setUp()
        self.root_dir = Path(self.builddir)

        self.project_dir = self.root_dir / 'src'
        self._create_project(self.project_dir)

        self.subprojects_dir = self.project_dir / 'subprojects'
        os.makedirs(str(self.subprojects_dir))
        self.packagecache_dir = self.subprojects_dir / 'packagecache'
        os.makedirs(str(self.packagecache_dir))

    def _create_project(self, path, project_name='dummy'):
        os.makedirs(str(path), exist_ok=True)
        with open(str(path / 'meson.build'), 'w', encoding='utf-8') as f:
            f.write(f"project('{project_name}')")

    def _git(self, cmd, workdir):
        return git(cmd, str(workdir), check=True)[1].strip()

    def _git_config(self, workdir):
        self._git(['config', 'user.name', 'Meson Test'], workdir)
        self._git(['config', 'user.email', 'meson.test@example.com'], workdir)

    def _git_remote(self, cmd, name):
        return self._git(cmd, self.root_dir / name)

    def _git_local(self, cmd, name):
        return self._git(cmd, self.subprojects_dir / name)

    def _git_local_branch(self, name):
        # Same as `git branch --show-current` but compatible with older git version
        branch = self._git_local(['rev-parse', '--abbrev-ref', 'HEAD'], name)
        return branch if branch != 'HEAD' else ''

    def _git_local_commit(self, name, ref='HEAD'):
        return self._git_local(['rev-parse', ref], name)

    def _git_remote_commit(self, name, ref='HEAD'):
        return self._git_remote(['rev-parse', ref], name)

    def _git_create_repo(self, path):
        # If a user has git configuration init.defaultBranch set we want to override that
        with tempfile.TemporaryDirectory() as d:
            out = git(['--version'], str(d))[1]
        if version_compare(search_version(out), '>= 2.28'):
            extra_cmd = ['--initial-branch', 'master']
        else:
            extra_cmd = []

        self._create_project(path)
        self._git(['init'] + extra_cmd, path)
        self._git_config(path)
        self._git(['add', '.'], path)
        self._git(['commit', '--no-gpg-sign', '-m', 'Initial commit'], path)

    def _git_create_remote_repo(self, name):
        self._git_create_repo(self.root_dir / name)

    def _git_create_local_repo(self, name):
        self._git_create_repo(self.subprojects_dir / name)

    def _git_create_remote_commit(self, name, branch):
        self._git_remote(['checkout', branch], name)
        self._git_remote(['commit', '--no-gpg-sign', '--allow-empty', '-m', f'initial {branch} commit'], name)

    def _git_create_remote_branch(self, name, branch):
        self._git_remote(['checkout', '-b', branch], name)
        self._git_remote(['commit', '--no-gpg-sign', '--allow-empty', '-m', f'initial {branch} commit'], name)

    def _git_create_remote_tag(self, name, tag):
        self._git_remote(['commit', '--no-gpg-sign', '--allow-empty', '-m', f'tag {tag} commit'], name)
        self._git_remote(['tag', '--no-sign', tag], name)

    def _wrap_create_git(self, name, revision='master', depth=None):
        path = self.root_dir / name
        with open(str((self.subprojects_dir / name).with_suffix('.wrap')), 'w', encoding='utf-8') as f:
            if depth is None:
                depth_line = ''
            else:
                depth_line = 'depth = {}'.format(depth)
            f.write(textwrap.dedent(
                '''
                [wrap-git]
                url={}
                revision={}
                {}
                '''.format(os.path.abspath(str(path)), revision, depth_line)))

    def _wrap_create_file(self, name, tarball='dummy.tar.gz'):
        path = self.root_dir / tarball
        with open(str((self.subprojects_dir / name).with_suffix('.wrap')), 'w', encoding='utf-8') as f:
            f.write(textwrap.dedent(
                f'''
                [wrap-file]
                source_url={os.path.abspath(str(path))}
                source_filename={tarball}
                '''))
        Path(self.packagecache_dir / tarball).touch()

    def _subprojects_cmd(self, args):
        return self._run(self.meson_command + ['subprojects'] + args, workdir=str(self.project_dir))

    def test_git_update(self):
        subp_name = 'sub1'

        # Create a fake remote git repository and a wrap file. Checks that
        # "meson subprojects download" works.
        self._git_create_remote_repo(subp_name)
        self._wrap_create_git(subp_name)
        self._subprojects_cmd(['download'])
        self.assertPathExists(str(self.subprojects_dir / subp_name))
        self._git_config(self.subprojects_dir / subp_name)

        # Create a new remote branch and update the wrap file. Checks that
        # "meson subprojects update --reset" checkout the new branch.
        self._git_create_remote_branch(subp_name, 'newbranch')
        self._wrap_create_git(subp_name, 'newbranch')
        self._subprojects_cmd(['update', '--reset'])
        self.assertEqual(self._git_local_branch(subp_name), 'newbranch')
        self.assertEqual(self._git_local_commit(subp_name), self._git_remote_commit(subp_name, 'newbranch'))

        # Update remote newbranch. Checks the new commit is pulled into existing
        # local newbranch. Make sure it does not print spurious 'git stash' message.
        self._git_create_remote_commit(subp_name, 'newbranch')
        out = self._subprojects_cmd(['update', '--reset'])
        self.assertNotIn('No local changes to save', out)
        self.assertEqual(self._git_local_branch(subp_name), 'newbranch')
        self.assertEqual(self._git_local_commit(subp_name), self._git_remote_commit(subp_name, 'newbranch'))

        # Update remote newbranch and switch to another branch. Checks that it
        # switch current branch to newbranch and pull latest commit.
        self._git_local(['checkout', 'master'], subp_name)
        self._git_create_remote_commit(subp_name, 'newbranch')
        self._subprojects_cmd(['update', '--reset'])
        self.assertEqual(self._git_local_branch(subp_name), 'newbranch')
        self.assertEqual(self._git_local_commit(subp_name), self._git_remote_commit(subp_name, 'newbranch'))

        # Stage some local changes then update. Checks that local changes got
        # stashed.
        self._create_project(self.subprojects_dir / subp_name, 'new_project_name')
        self._git_local(['add', '.'], subp_name)
        self._git_create_remote_commit(subp_name, 'newbranch')
        self._subprojects_cmd(['update', '--reset'])
        self.assertEqual(self._git_local_branch(subp_name), 'newbranch')
        self.assertEqual(self._git_local_commit(subp_name), self._git_remote_commit(subp_name, 'newbranch'))
        self.assertTrue(self._git_local(['stash', 'list'], subp_name))

        # Untracked files need to be stashed too, or (re-)applying a patch
        # creating one of those untracked files will fail.
        untracked = self.subprojects_dir / subp_name / 'untracked.c'
        untracked.write_bytes(b'int main(void) { return 0; }')
        self._subprojects_cmd(['update', '--reset'])
        self.assertTrue(self._git_local(['stash', 'list'], subp_name))
        assert not untracked.exists()
        # Ensure it was indeed stashed, and we can get it back.
        self.assertTrue(self._git_local(['stash', 'pop'], subp_name))
        assert untracked.exists()

        # Create a new remote tag and update the wrap file. Checks that
        # "meson subprojects update --reset" checkout the new tag in detached mode.
        self._git_create_remote_tag(subp_name, 'newtag')
        self._wrap_create_git(subp_name, 'newtag')
        self._subprojects_cmd(['update', '--reset'])
        self.assertEqual(self._git_local_branch(subp_name), '')
        self.assertEqual(self._git_local_commit(subp_name), self._git_remote_commit(subp_name, 'newtag'))

        # Create a new remote commit and update the wrap file with the commit id.
        # Checks that "meson subprojects update --reset" checkout the new commit
        # in detached mode.
        self._git_local(['checkout', 'master'], subp_name)
        self._git_create_remote_commit(subp_name, 'newbranch')
        new_commit = self._git_remote(['rev-parse', 'HEAD'], subp_name)
        self._wrap_create_git(subp_name, new_commit)
        self._subprojects_cmd(['update', '--reset'])
        self.assertEqual(self._git_local_branch(subp_name), '')
        self.assertEqual(self._git_local_commit(subp_name), new_commit)

        # Create a local project not in a git repository, then update it with
        # a git wrap. Without --reset it should print error message and return
        # failure. With --reset it should delete existing project and clone the
        # new project.
        subp_name = 'sub2'
        self._create_project(self.subprojects_dir / subp_name)
        self._git_create_remote_repo(subp_name)
        self._wrap_create_git(subp_name)
        with self.assertRaises(subprocess.CalledProcessError) as cm:
            self._subprojects_cmd(['update'])
        self.assertIn('Not a git repository', cm.exception.output)
        self._subprojects_cmd(['update', '--reset'])
        self.assertEqual(self._git_local_commit(subp_name), self._git_remote_commit(subp_name))

        # Create a fake remote git repository and a wrap file targeting
        # HEAD and depth = 1. Checks that "meson subprojects download" works.
        subp_name = 'sub3'
        self._git_create_remote_repo(subp_name)
        self._wrap_create_git(subp_name, revision='head', depth='1')
        self._subprojects_cmd(['download'])
        self.assertPathExists(str(self.subprojects_dir / subp_name))
        self._git_config(self.subprojects_dir / subp_name)

    @skipIfNoExecutable('true')
    def test_foreach(self):
        self._create_project(self.subprojects_dir / 'sub_file')
        self._wrap_create_file('sub_file')
        self._git_create_local_repo('sub_git')
        self._wrap_create_git('sub_git')
        self._git_create_local_repo('sub_git_no_wrap')

        def ran_in(s):
            ret = []
            prefix = 'Executing command in '
            for l in s.splitlines():
                if l.startswith(prefix):
                    ret.append(l[len(prefix):])
            return sorted(ret)

        dummy_cmd = ['true']
        out = self._subprojects_cmd(['foreach'] + dummy_cmd)
        self.assertEqual(ran_in(out), sorted(['subprojects/sub_file', 'subprojects/sub_git', 'subprojects/sub_git_no_wrap']))
        out = self._subprojects_cmd(['foreach', '--types', 'git,file'] + dummy_cmd)
        self.assertEqual(ran_in(out), sorted(['subprojects/sub_file', 'subprojects/sub_git']))
        out = self._subprojects_cmd(['foreach', '--types', 'file'] + dummy_cmd)
        self.assertEqual(ran_in(out), ['subprojects/sub_file'])
        out = self._subprojects_cmd(['foreach', '--types', 'git'] + dummy_cmd)
        self.assertEqual(ran_in(out), ['subprojects/sub_git'])

    def test_purge(self):
        self._create_project(self.subprojects_dir / 'sub_file')
        self._wrap_create_file('sub_file')
        self._git_create_local_repo('sub_git')
        self._wrap_create_git('sub_git')

        sub_file_subprojects_dir = self.subprojects_dir / 'sub_file' / 'subprojects'
        sub_file_subprojects_dir.mkdir(exist_ok=True, parents=True)
        real_dir = Path('sub_file') / 'subprojects' / 'real'

        self._wrap_create_file(real_dir, tarball='dummy2.tar.gz')

        with open(str((self.subprojects_dir / 'redirect').with_suffix('.wrap')), 'w', encoding='utf-8') as f:
            f.write(textwrap.dedent(
                f'''
                [wrap-redirect]
                filename = {real_dir}.wrap
                '''))

        def deleting(s: str) -> T.List[str]:
            ret = []
            prefix = 'Deleting '
            for l in s.splitlines():
                if l.startswith(prefix):
                    ret.append(l[len(prefix):])
            return sorted(ret)

        out = self._subprojects_cmd(['purge'])
        self.assertEqual(deleting(out), sorted([
            str(self.subprojects_dir / 'redirect.wrap'),
            str(self.subprojects_dir / 'sub_file'),
            str(self.subprojects_dir / 'sub_git'),
        ]))
        out = self._subprojects_cmd(['purge', '--include-cache'])
        self.assertEqual(deleting(out), sorted([
            str(self.subprojects_dir / 'sub_git'),
            str(self.subprojects_dir / 'redirect.wrap'),
            str(self.subprojects_dir / 'packagecache' / 'dummy.tar.gz'),
            str(self.subprojects_dir / 'packagecache' / 'dummy2.tar.gz'),
            str(self.subprojects_dir / 'sub_file'),
        ]))
        out = self._subprojects_cmd(['purge', '--include-cache', '--confirm'])
        self.assertEqual(deleting(out), sorted([
            str(self.subprojects_dir / 'sub_git'),
            str(self.subprojects_dir / 'redirect.wrap'),
            str(self.subprojects_dir / 'packagecache' / 'dummy.tar.gz'),
            str(self.subprojects_dir / 'packagecache' / 'dummy2.tar.gz'),
            str(self.subprojects_dir / 'sub_file'),
        ]))
        self.assertFalse(Path(self.subprojects_dir / 'packagecache' / 'dummy.tar.gz').exists())
        self.assertFalse(Path(self.subprojects_dir / 'sub_file').exists())
        self.assertFalse(Path(self.subprojects_dir / 'sub_git').exists())
        self.assertFalse(Path(self.subprojects_dir / 'redirect.wrap').exists())
