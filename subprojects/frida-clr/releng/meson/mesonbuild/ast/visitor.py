# SPDX-License-Identifier: Apache-2.0
# Copyright 2019 The Meson development team

# This class contains the basic functionality needed to run any interpreter
# or an interpreter-based tool
from __future__ import annotations

import typing as T

if T.TYPE_CHECKING:
    from .. import mparser

class AstVisitor:
    def __init__(self) -> None:
        pass

    def visit_default_func(self, node: mparser.BaseNode) -> None:
        pass

    def visit_BooleanNode(self, node: mparser.BooleanNode) -> None:
        self.visit_default_func(node)

    def visit_IdNode(self, node: mparser.IdNode) -> None:
        self.visit_default_func(node)

    def visit_NumberNode(self, node: mparser.NumberNode) -> None:
        self.visit_default_func(node)

    def visit_StringNode(self, node: mparser.StringNode) -> None:
        self.visit_default_func(node)

    def visit_FormatStringNode(self, node: mparser.FormatStringNode) -> None:
        self.visit_default_func(node)

    def visit_MultilineStringNode(self, node: mparser.MultilineFormatStringNode) -> None:
        self.visit_default_func(node)

    def visit_FormatMultilineStringNode(self, node: mparser.FormatStringNode) -> None:
        self.visit_default_func(node)

    def visit_ContinueNode(self, node: mparser.ContinueNode) -> None:
        self.visit_default_func(node)

    def visit_BreakNode(self, node: mparser.BreakNode) -> None:
        self.visit_default_func(node)

    def visit_SymbolNode(self, node: mparser.SymbolNode) -> None:
        self.visit_default_func(node)

    def visit_WhitespaceNode(self, node: mparser.WhitespaceNode) -> None:
        self.visit_default_func(node)

    def visit_ArrayNode(self, node: mparser.ArrayNode) -> None:
        self.visit_default_func(node)
        node.args.accept(self)

    def visit_DictNode(self, node: mparser.DictNode) -> None:
        self.visit_default_func(node)
        node.args.accept(self)

    def visit_EmptyNode(self, node: mparser.EmptyNode) -> None:
        self.visit_default_func(node)

    def visit_OrNode(self, node: mparser.OrNode) -> None:
        self.visit_default_func(node)
        node.left.accept(self)
        node.right.accept(self)

    def visit_AndNode(self, node: mparser.AndNode) -> None:
        self.visit_default_func(node)
        node.left.accept(self)
        node.right.accept(self)

    def visit_ComparisonNode(self, node: mparser.ComparisonNode) -> None:
        self.visit_default_func(node)
        node.left.accept(self)
        node.right.accept(self)

    def visit_ArithmeticNode(self, node: mparser.ArithmeticNode) -> None:
        self.visit_default_func(node)
        node.left.accept(self)
        node.right.accept(self)

    def visit_NotNode(self, node: mparser.NotNode) -> None:
        self.visit_default_func(node)
        node.value.accept(self)

    def visit_CodeBlockNode(self, node: mparser.CodeBlockNode) -> None:
        self.visit_default_func(node)
        for i in node.lines:
            i.accept(self)

    def visit_IndexNode(self, node: mparser.IndexNode) -> None:
        self.visit_default_func(node)
        node.iobject.accept(self)
        node.index.accept(self)

    def visit_MethodNode(self, node: mparser.MethodNode) -> None:
        self.visit_default_func(node)
        node.source_object.accept(self)
        node.name.accept(self)
        node.args.accept(self)

    def visit_FunctionNode(self, node: mparser.FunctionNode) -> None:
        self.visit_default_func(node)
        node.func_name.accept(self)
        node.args.accept(self)

    def visit_AssignmentNode(self, node: mparser.AssignmentNode) -> None:
        self.visit_default_func(node)
        node.var_name.accept(self)
        node.value.accept(self)

    def visit_PlusAssignmentNode(self, node: mparser.PlusAssignmentNode) -> None:
        self.visit_default_func(node)
        node.var_name.accept(self)
        node.value.accept(self)

    def visit_ForeachClauseNode(self, node: mparser.ForeachClauseNode) -> None:
        self.visit_default_func(node)
        for varname in node.varnames:
            varname.accept(self)
        node.items.accept(self)
        node.block.accept(self)

    def visit_IfClauseNode(self, node: mparser.IfClauseNode) -> None:
        self.visit_default_func(node)
        for i in node.ifs:
            i.accept(self)
        node.elseblock.accept(self)

    def visit_UMinusNode(self, node: mparser.UMinusNode) -> None:
        self.visit_default_func(node)
        node.value.accept(self)

    def visit_IfNode(self, node: mparser.IfNode) -> None:
        self.visit_default_func(node)
        node.condition.accept(self)
        node.block.accept(self)

    def visit_ElseNode(self, node: mparser.ElseNode) -> None:
        self.visit_default_func(node)
        node.block.accept(self)

    def visit_TernaryNode(self, node: mparser.TernaryNode) -> None:
        self.visit_default_func(node)
        node.condition.accept(self)
        node.trueblock.accept(self)
        node.falseblock.accept(self)

    def visit_ArgumentNode(self, node: mparser.ArgumentNode) -> None:
        self.visit_default_func(node)
        for i in node.arguments:
            i.accept(self)
        for key, val in node.kwargs.items():
            key.accept(self)
            val.accept(self)

    def visit_ParenthesizedNode(self, node: mparser.ParenthesizedNode) -> None:
        self.visit_default_func(node)
        node.inner.accept(self)
