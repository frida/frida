# SPDX-License-Identifier: Apache-2.0
# Copyright 2019 The Meson development team

# This class contains the basic functionality needed to run any interpreter
# or an interpreter-based tool
from __future__ import annotations

from .visitor import AstVisitor
import typing as T

if T.TYPE_CHECKING:
    from .. import mparser

class AstIndentationGenerator(AstVisitor):
    def __init__(self) -> None:
        self.level = 0

    def visit_default_func(self, node: mparser.BaseNode) -> None:
        # Store the current level in the node
        node.level = self.level

    def visit_ArrayNode(self, node: mparser.ArrayNode) -> None:
        self.visit_default_func(node)
        self.level += 1
        node.args.accept(self)
        self.level -= 1

    def visit_DictNode(self, node: mparser.DictNode) -> None:
        self.visit_default_func(node)
        self.level += 1
        node.args.accept(self)
        self.level -= 1

    def visit_MethodNode(self, node: mparser.MethodNode) -> None:
        self.visit_default_func(node)
        node.source_object.accept(self)
        self.level += 1
        node.args.accept(self)
        self.level -= 1

    def visit_FunctionNode(self, node: mparser.FunctionNode) -> None:
        self.visit_default_func(node)
        self.level += 1
        node.args.accept(self)
        self.level -= 1

    def visit_ForeachClauseNode(self, node: mparser.ForeachClauseNode) -> None:
        self.visit_default_func(node)
        self.level += 1
        node.items.accept(self)
        node.block.accept(self)
        self.level -= 1

    def visit_IfClauseNode(self, node: mparser.IfClauseNode) -> None:
        self.visit_default_func(node)
        for i in node.ifs:
            i.accept(self)
        if node.elseblock:
            self.level += 1
            node.elseblock.accept(self)
            self.level -= 1

    def visit_IfNode(self, node: mparser.IfNode) -> None:
        self.visit_default_func(node)
        self.level += 1
        node.condition.accept(self)
        node.block.accept(self)
        self.level -= 1

class AstIDGenerator(AstVisitor):
    def __init__(self) -> None:
        self.counter: T.Dict[str, int] = {}

    def visit_default_func(self, node: mparser.BaseNode) -> None:
        name = type(node).__name__
        if name not in self.counter:
            self.counter[name] = 0
        node.ast_id = name + '#' + str(self.counter[name])
        self.counter[name] += 1

class AstConditionLevel(AstVisitor):
    def __init__(self) -> None:
        self.condition_level = 0

    def visit_default_func(self, node: mparser.BaseNode) -> None:
        node.condition_level = self.condition_level

    def visit_ForeachClauseNode(self, node: mparser.ForeachClauseNode) -> None:
        self.visit_default_func(node)
        self.condition_level += 1
        node.items.accept(self)
        node.block.accept(self)
        self.condition_level -= 1

    def visit_IfClauseNode(self, node: mparser.IfClauseNode) -> None:
        self.visit_default_func(node)
        for i in node.ifs:
            i.accept(self)
        if node.elseblock:
            self.condition_level += 1
            node.elseblock.accept(self)
            self.condition_level -= 1

    def visit_IfNode(self, node: mparser.IfNode) -> None:
        self.visit_default_func(node)
        self.condition_level += 1
        node.condition.accept(self)
        node.block.accept(self)
        self.condition_level -= 1
