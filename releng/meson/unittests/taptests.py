# SPDX-License-Identifier: Apache-2.0
# Copyright 2016-2021 The Meson development team

import unittest
import io

from mesonbuild.mtest import TAPParser, TestResult


class TAPParserTests(unittest.TestCase):
    def assert_test(self, events, **kwargs):
        if 'explanation' not in kwargs:
            kwargs['explanation'] = None
        self.assertEqual(next(events), TAPParser.Test(**kwargs))

    def assert_plan(self, events, **kwargs):
        if 'skipped' not in kwargs:
            kwargs['skipped'] = False
        if 'explanation' not in kwargs:
            kwargs['explanation'] = None
        self.assertEqual(next(events), TAPParser.Plan(**kwargs))

    def assert_version(self, events, **kwargs):
        self.assertEqual(next(events), TAPParser.Version(**kwargs))

    def assert_error(self, events):
        self.assertEqual(type(next(events)), TAPParser.Error)

    def assert_unexpected(self, events, **kwargs):
        self.assertEqual(next(events), TAPParser.UnknownLine(**kwargs))

    def assert_bailout(self, events, **kwargs):
        self.assertEqual(next(events), TAPParser.Bailout(**kwargs))

    def assert_last(self, events):
        with self.assertRaises(StopIteration):
            next(events)

    def parse_tap(self, s):
        parser = TAPParser()
        return iter(parser.parse(io.StringIO(s)))

    def parse_tap_v13(self, s):
        events = self.parse_tap('TAP version 13\n' + s)
        self.assert_version(events, version=13)
        return events

    def test_empty(self):
        events = self.parse_tap('')
        self.assert_last(events)

    def test_empty_plan(self):
        events = self.parse_tap('1..0')
        self.assert_plan(events, num_tests=0, late=False, skipped=True)
        self.assert_last(events)

    def test_plan_directive(self):
        events = self.parse_tap('1..0 # skipped for some reason')
        self.assert_plan(events, num_tests=0, late=False, skipped=True,
                         explanation='for some reason')
        self.assert_last(events)

        events = self.parse_tap('1..1 # skipped for some reason\nok 1')
        self.assert_error(events)
        self.assert_plan(events, num_tests=1, late=False, skipped=True,
                         explanation='for some reason')
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_last(events)

        events = self.parse_tap('1..1 # todo not supported here\nok 1')
        self.assert_error(events)
        self.assert_plan(events, num_tests=1, late=False, skipped=False,
                         explanation='not supported here')
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_last(events)

    def test_one_test_ok(self):
        events = self.parse_tap('ok')
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_last(events)

    def test_one_test_with_number(self):
        events = self.parse_tap('ok 1')
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_last(events)

    def test_one_test_with_name(self):
        events = self.parse_tap('ok 1 abc')
        self.assert_test(events, number=1, name='abc', result=TestResult.OK)
        self.assert_last(events)

    def test_one_test_not_ok(self):
        events = self.parse_tap('not ok')
        self.assert_test(events, number=1, name='', result=TestResult.FAIL)
        self.assert_last(events)

    def test_one_test_todo(self):
        events = self.parse_tap('not ok 1 abc # TODO')
        self.assert_test(events, number=1, name='abc', result=TestResult.EXPECTEDFAIL)
        self.assert_last(events)

        events = self.parse_tap('ok 1 abc # TODO')
        self.assert_test(events, number=1, name='abc', result=TestResult.UNEXPECTEDPASS)
        self.assert_last(events)

    def test_one_test_skip(self):
        events = self.parse_tap('ok 1 abc # SKIP')
        self.assert_test(events, number=1, name='abc', result=TestResult.SKIP)
        self.assert_last(events)

    def test_one_test_skip_failure(self):
        events = self.parse_tap('not ok 1 abc # SKIP')
        self.assert_test(events, number=1, name='abc', result=TestResult.FAIL)
        self.assert_last(events)

    def test_many_early_plan(self):
        events = self.parse_tap('1..4\nok 1\nnot ok 2\nok 3\nnot ok 4')
        self.assert_plan(events, num_tests=4, late=False)
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_test(events, number=2, name='', result=TestResult.FAIL)
        self.assert_test(events, number=3, name='', result=TestResult.OK)
        self.assert_test(events, number=4, name='', result=TestResult.FAIL)
        self.assert_last(events)

    def test_many_late_plan(self):
        events = self.parse_tap('ok 1\nnot ok 2\nok 3\nnot ok 4\n1..4')
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_test(events, number=2, name='', result=TestResult.FAIL)
        self.assert_test(events, number=3, name='', result=TestResult.OK)
        self.assert_test(events, number=4, name='', result=TestResult.FAIL)
        self.assert_plan(events, num_tests=4, late=True)
        self.assert_last(events)

    def test_directive_case(self):
        events = self.parse_tap('ok 1 abc # skip')
        self.assert_test(events, number=1, name='abc', result=TestResult.SKIP)
        self.assert_last(events)

        events = self.parse_tap('ok 1 abc # ToDo')
        self.assert_test(events, number=1, name='abc', result=TestResult.UNEXPECTEDPASS)
        self.assert_last(events)

    def test_directive_explanation(self):
        events = self.parse_tap('ok 1 abc # skip why')
        self.assert_test(events, number=1, name='abc', result=TestResult.SKIP,
                         explanation='why')
        self.assert_last(events)

        events = self.parse_tap('ok 1 abc # ToDo Because')
        self.assert_test(events, number=1, name='abc', result=TestResult.UNEXPECTEDPASS,
                         explanation='Because')
        self.assert_last(events)

    def test_one_test_early_plan(self):
        events = self.parse_tap('1..1\nok')
        self.assert_plan(events, num_tests=1, late=False)
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_last(events)

    def test_one_test_late_plan(self):
        events = self.parse_tap('ok\n1..1')
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_plan(events, num_tests=1, late=True)
        self.assert_last(events)

    def test_out_of_order(self):
        events = self.parse_tap('ok 2')
        self.assert_error(events)
        self.assert_test(events, number=2, name='', result=TestResult.OK)
        self.assert_last(events)

    def test_middle_plan(self):
        events = self.parse_tap('ok 1\n1..2\nok 2')
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_plan(events, num_tests=2, late=True)
        self.assert_error(events)
        self.assert_test(events, number=2, name='', result=TestResult.OK)
        self.assert_last(events)

    def test_too_many_plans(self):
        events = self.parse_tap('1..1\n1..2\nok 1')
        self.assert_plan(events, num_tests=1, late=False)
        self.assert_error(events)
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_last(events)

    def test_too_many(self):
        events = self.parse_tap('ok 1\nnot ok 2\n1..1')
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_test(events, number=2, name='', result=TestResult.FAIL)
        self.assert_plan(events, num_tests=1, late=True)
        self.assert_error(events)
        self.assert_last(events)

        events = self.parse_tap('1..1\nok 1\nnot ok 2')
        self.assert_plan(events, num_tests=1, late=False)
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_test(events, number=2, name='', result=TestResult.FAIL)
        self.assert_error(events)
        self.assert_last(events)

    def test_too_few(self):
        events = self.parse_tap('ok 1\nnot ok 2\n1..3')
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_test(events, number=2, name='', result=TestResult.FAIL)
        self.assert_plan(events, num_tests=3, late=True)
        self.assert_error(events)
        self.assert_last(events)

        events = self.parse_tap('1..3\nok 1\nnot ok 2')
        self.assert_plan(events, num_tests=3, late=False)
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_test(events, number=2, name='', result=TestResult.FAIL)
        self.assert_error(events)
        self.assert_last(events)

    def test_too_few_bailout(self):
        events = self.parse_tap('1..3\nok 1\nnot ok 2\nBail out! no third test')
        self.assert_plan(events, num_tests=3, late=False)
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_test(events, number=2, name='', result=TestResult.FAIL)
        self.assert_bailout(events, message='no third test')
        self.assert_last(events)

    def test_diagnostics(self):
        events = self.parse_tap('1..1\n# ignored\nok 1')
        self.assert_plan(events, num_tests=1, late=False)
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_last(events)

        events = self.parse_tap('# ignored\n1..1\nok 1\n# ignored too')
        self.assert_plan(events, num_tests=1, late=False)
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_last(events)

        events = self.parse_tap('# ignored\nok 1\n1..1\n# ignored too')
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_plan(events, num_tests=1, late=True)
        self.assert_last(events)

    def test_empty_line(self):
        events = self.parse_tap('1..1\n\nok 1')
        self.assert_plan(events, num_tests=1, late=False)
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_last(events)

    def test_unexpected(self):
        events = self.parse_tap('1..1\ninvalid\nok 1')
        self.assert_plan(events, num_tests=1, late=False)
        self.assert_unexpected(events, message='invalid', lineno=2)
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_last(events)

    def test_version(self):
        events = self.parse_tap('TAP version 13\n')
        self.assert_version(events, version=13)
        self.assert_last(events)

        events = self.parse_tap('TAP version 12\n')
        self.assert_error(events)
        self.assert_last(events)

        events = self.parse_tap('1..0\nTAP version 13\n')
        self.assert_plan(events, num_tests=0, late=False, skipped=True)
        self.assert_error(events)
        self.assert_last(events)

    def test_yaml(self):
        events = self.parse_tap_v13('ok\n ---\n foo: abc\n  bar: def\n ...\nok 2')
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_test(events, number=2, name='', result=TestResult.OK)
        self.assert_last(events)

        events = self.parse_tap_v13('ok\n ---\n foo: abc\n  bar: def')
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_error(events)
        self.assert_last(events)

        events = self.parse_tap_v13('ok 1\n ---\n foo: abc\n  bar: def\nnot ok 2')
        self.assert_test(events, number=1, name='', result=TestResult.OK)
        self.assert_error(events)
        self.assert_test(events, number=2, name='', result=TestResult.FAIL)
        self.assert_last(events)
