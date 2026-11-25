#!/usr/bin/env python3
"""Measures C++ file, function, and struct/class sizes against the M13.5 size budget.

Scans C++ sources under modules/*/src, modules/*/include, tools/, examples/*/src,
and templates/*/src (excluding tests and _deps) and checks:
  - file length <= 600 lines
  - function body <= 80 lines
  - struct/class data members <= 25

Exceptions are loaded from tools/size_budget.json. An entry may carry "max" (a cap on the
excepted value, so an exception cannot grow unnoticed). Entries that match nothing are reported
as stale and fail --strict: remove them once the offender is fixed.

Files and functions at or above --near (default 0.9) of a limit are listed as [NEAR] so a split
can be planned before a change goes over (ADR-0030). [NEAR] never fails the check.

Usage:
  check_sizes.py [--root <dir>] [--budget <path>] [--summary] [--strict] [--near <ratio>]
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path
import sys
from typing import Iterable, Sequence

MAX_FILE_LINES = 600
MAX_FUNCTION_LINES = 80
MAX_STRUCT_MEMBERS = 25
DEFAULT_NEAR_RATIO = 0.9

# What to do about an offender (ADR-0030: split by responsibility, never just to fit the number).
HINTS = {
    "file": "split by responsibility into sibling files (ADR-0030 rule 2)",
    "function": "extract named steps; keep one coherent algorithm together (ADR-0030 rule 2)",
    "members": "group related fields into sub-structs or move them to a collaborator (ADR-0030 rule 2)",
}

# Findings are recorded from this fraction of a limit (analyze_source sets it); 1.0 = over only.
_report_ratio = 1.0


def _reported(value: int, limit: int) -> bool:
    """True if value is over the limit, or within the near ratio of it while near-scanning."""
    return value > limit or (_report_ratio < 1.0 and value >= limit * _report_ratio)

CPP_EXTENSIONS = frozenset({".cpp", ".cc", ".cxx", ".hpp", ".hh", ".hxx", ".h", ".inl"})
EXCLUDED_DIR_NAMES = frozenset({"tests", "_deps", "build", ".git"})

KEYWORDS_NOT_FUNCTIONS = frozenset({
    "if", "else", "for", "while", "switch", "catch", "do", "try",
    "namespace", "struct", "class", "union", "enum", "return", "co_return",
    "co_yield", "co_await", "sizeof", "alignof", "decltype", "noexcept",
    "static_assert", "requires", "concept", "template", "using", "typedef",
    "friend", "delete", "new", "throw", "case", "default", "goto", "break",
    "continue", "public", "private", "protected", "extern",
})

NON_MEMBER_LEAD_KEYWORDS = frozenset({
    "using", "typedef", "friend", "static_assert", "namespace", "template",
})

SPECIFIER_KEYWORDS = frozenset({
    "const", "volatile", "mutable", "constexpr", "consteval", "constinit",
    "inline", "virtual", "explicit", "override", "final", "noexcept",
    "static", "extern", "register", "thread_local", "typename", "struct",
    "class", "enum", "union", "signed", "unsigned", "short", "long",
})

QUALIFIER_AFTER_PARAMS = frozenset({
    "const", "volatile", "noexcept", "override", "final", "mutable",
    "requires", "throw",
})


@dataclass(frozen=True)
class Token:
    kind: str  # "ident", "sym", "str", "num"
    text: str
    line: int


@dataclass(frozen=True)
class Finding:
    path: str
    kind: str  # "file" | "function" | "members"
    name: str
    value: int
    limit: int
    line: int

    @property
    def over(self) -> bool:
        return self.value > self.limit


@dataclass(frozen=True)
class ExceptionEntry:
    path: str
    kind: str
    name: str
    reason: str
    max: int | None = None  # the excepted value may not grow beyond this


def _is_ident_start(ch: str) -> bool:
    return ch.isalpha() or ch == "_"


def _is_ident_char(ch: str) -> bool:
    return ch.isalnum() or ch == "_"


def strip_cpp(source: str) -> str:
    """Strip comments, string/char literals (incl. raw strings), and preprocessor directives.

    Every newline is preserved so 1-based line numbers remain exact.
    """
    n = len(source)
    out: list[str] = []
    i = 0
    at_line_start = True

    while i < n:
        ch = source[i]

        # Track newlines cleanly (handle \r\n and \n).
        if ch == "\n":
            out.append("\n")
            i += 1
            at_line_start = True
            continue
        if ch == "\r":
            out.append(" ")
            i += 1
            continue

        # Preprocessor directives at start of logical line.
        if at_line_start and ch == "#":
            while i < n:
                if source[i] == "\\" and i + 1 < n and source[i + 1] == "\n":
                    out.append(" ")
                    out.append("\n")
                    i += 2
                elif source[i] == "\\" and i + 2 < n and source[i + 1] == "\r" and source[i + 2] == "\n":
                    out.append("  \n")
                    i += 3
                elif source[i] == "\n":
                    break
                else:
                    out.append(" ")
                    i += 1
            continue

        if ch in (" ", "\t", "\v", "\f"):
            out.append(ch)
            i += 1
            continue

        # Line comment //
        if ch == "/" and i + 1 < n and source[i + 1] == "/":
            out.append("  ")
            i += 2
            while i < n:
                if source[i] == "\\" and i + 1 < n and source[i + 1] == "\n":
                    out.append(" \n")
                    i += 2
                elif source[i] == "\\" and i + 2 < n and source[i + 1] == "\r" and source[i + 2] == "\n":
                    out.append("  \n")
                    i += 3
                elif source[i] == "\n":
                    break
                else:
                    out.append(" ")
                    i += 1
            continue

        # Block comment /* ... */
        if ch == "/" and i + 1 < n and source[i + 1] == "*":
            out.append("  ")
            i += 2
            while i < n:
                if source[i] == "*" and i + 1 < n and source[i + 1] == "/":
                    out.append("  ")
                    i += 2
                    break
                out.append("\n" if source[i] == "\n" else " ")
                i += 1
            continue

        at_line_start = False

        # Raw string literal: [u8|u|U|L]R"delim(...)delim"
        raw_prefix = _match_raw_string_prefix(source, i)
        if raw_prefix is not None:
            delim_start = raw_prefix  # index right after R"
            paren_pos = source.find("(", delim_start)
            if paren_pos != -1 and paren_pos - delim_start <= 16:
                delim = source[delim_start:paren_pos]
                if not any(c in delim for c in " \t\v\f\n\r()\\"):
                    closing = ")" + delim + '"'
                    end_pos = source.find(closing, paren_pos + 1)
                    if end_pos == -1:
                        end_pos = n
                    else:
                        end_pos += len(closing)
                    out.append('"')
                    for k in range(i + 1, end_pos - 1):
                        out.append("\n" if source[k] == "\n" else " ")
                    if end_pos > i + 1:
                        out.append('"')
                    i = end_pos
                    continue

        # Numeric literal (consume C++14 digit separators like 1'000'000 so ' is not a char literal).
        if ch.isdigit() or (ch == "." and i + 1 < n and source[i + 1].isdigit()):
            start = i
            i += 1
            while i < n:
                c = source[i]
                if c.isalnum() or c in ("_", "'", "."):
                    if c in ("e", "E", "p", "P") and i + 1 < n and source[i + 1] in ("+", "-"):
                        i += 2
                        continue
                    i += 1
                else:
                    break
            out.append(source[start:i])
            continue

        # Normal string literal "..."
        if ch == '"':
            out.append('"')
            i += 1
            while i < n:
                c = source[i]
                if c == "\\" and i + 1 < n:
                    nxt = source[i + 1]
                    out.append(" ")
                    out.append("\n" if nxt == "\n" else " ")
                    i += 2
                elif c == '"':
                    out.append('"')
                    i += 1
                    break
                else:
                    out.append("\n" if c == "\n" else " ")
                    i += 1
            continue

        # Character literal '...'
        if ch == "'":
            out.append("'")
            i += 1
            while i < n:
                c = source[i]
                if c == "\\" and i + 1 < n:
                    nxt = source[i + 1]
                    out.append(" ")
                    out.append("\n" if nxt == "\n" else " ")
                    i += 2
                elif c == "'":
                    out.append("'")
                    i += 1
                    break
                else:
                    out.append("\n" if c == "\n" else " ")
                    i += 1
            continue

        # Identifier (consume whole identifier so raw-string prefix check cannot trigger mid-identifier).
        if _is_ident_start(ch):
            start = i
            i += 1
            while i < n and _is_ident_char(source[i]):
                i += 1
            out.append(source[start:i])
            continue

        out.append(ch)
        i += 1

    return "".join(out)


def _match_raw_string_prefix(source: str, i: int) -> int | None:
    """If source[i:] starts a raw string literal, return the index right after R\"."""
    n = len(source)
    if i > 0 and _is_ident_char(source[i - 1]):
        return None
    for prefix in ('u8R"', 'uR"', 'UR"', 'LR"', 'R"'):
        if source.startswith(prefix, i):
            return i + len(prefix)
    return None


def tokenize(stripped: str) -> list[Token]:
    """Tokenize stripped C++ text into identifiers, symbols, strings, and numbers."""
    tokens: list[Token] = []
    n = len(stripped)
    i = 0
    line = 1

    two_char_syms = frozenset({
        "::", "->", "==", "!=", "<=", ">=", "&&", "||", "++", "--",
        "<<", ">>", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", ".*",
    })
    three_char_syms = frozenset({"<=>", "...", "<<=", ">>=", "->*"})

    while i < n:
        ch = stripped[i]
        if ch == "\n":
            line += 1
            i += 1
            continue
        if ch.isspace():
            i += 1
            continue

        if ch in ('"', "'"):
            quote = ch
            start_line = line
            i += 1
            while i < n and stripped[i] != quote:
                if stripped[i] == "\n":
                    line += 1
                i += 1
            if i < n:
                i += 1
            tokens.append(Token("str", quote + quote, start_line))
            continue

        if _is_ident_start(ch):
            start = i
            i += 1
            while i < n and _is_ident_char(stripped[i]):
                i += 1
            tokens.append(Token("ident", stripped[start:i], line))
            continue

        if ch.isdigit():
            start = i
            i += 1
            while i < n and (stripped[i].isalnum() or stripped[i] in ("_", "'", ".")):
                i += 1
            tokens.append(Token("num", stripped[start:i], line))
            continue

        if i + 3 <= n and stripped[i:i + 3] in three_char_syms:
            tokens.append(Token("sym", stripped[i:i + 3], line))
            i += 3
            continue
        if i + 2 <= n and stripped[i:i + 2] in two_char_syms:
            tokens.append(Token("sym", stripped[i:i + 2], line))
            i += 2
            continue

        tokens.append(Token("sym", ch, line))
        i += 1

    return tokens


def _find_matching(tokens: Sequence[Token], open_idx: int, open_sym: str, close_sym: str, end: int) -> int:
    """Return index of matching close_sym, or end - 1 if unbalanced."""
    depth = 0
    for i in range(open_idx, end):
        t = tokens[i].text
        if t == open_sym:
            depth += 1
        elif t == close_sym:
            depth -= 1
            if depth == 0:
                return i
    return end - 1


def _skip_angle_brackets(tokens: Sequence[Token], open_idx: int, end: int) -> int:
    """Skip a template argument/parameter list < ... >, returning index of matching '>' or open_idx."""
    depth = 0
    paren = 0
    bracket = 0
    i = open_idx
    while i < end:
        t = tokens[i].text
        if t == "(":
            paren += 1
        elif t == ")":
            if paren == 0:
                return open_idx
            paren -= 1
        elif t == "[":
            bracket += 1
        elif t == "]":
            if bracket == 0:
                return open_idx
            bracket -= 1
        elif t == "{" or t == ";" or t == "}":
            return open_idx
        elif paren == 0 and bracket == 0:
            if t == "<":
                depth += 1
            elif t == ">":
                depth -= 1
                if depth == 0:
                    return i
            elif t == ">>":
                depth -= 2
                if depth <= 0:
                    return i
        i += 1
    return open_idx


def _skip_attributes(tokens: Sequence[Token], idx: int, end: int) -> int:
    """Skip C++ attributes [[...]] and alignas(...)."""
    while idx < end:
        if (tokens[idx].text == "[" and idx + 1 < end and tokens[idx + 1].text == "["):
            close = _find_matching(tokens, idx, "[", "]", end)
            idx = close + 1
            continue
        if tokens[idx].text == "alignas" and idx + 1 < end and tokens[idx + 1].text == "(":
            close = _find_matching(tokens, idx + 1, "(", ")", end)
            idx = close + 1
            continue
        break
    return idx


def _parse_class_header(tokens: Sequence[Token], start: int, end: int) -> tuple[str | None, int | None]:
    """If tokens[start:end] begins a struct/class/union definition, return (name, open_brace_idx).

    Returns (None, None) if it is a forward declaration, elaborated type variable, or function
    returning an elaborated type (e.g. `struct Foo make() { ... }`).
    """
    idx = start
    if idx >= end or tokens[idx].text not in ("struct", "class", "union"):
        return None, None
    idx += 1
    idx = _skip_attributes(tokens, idx, end)

    # Collect optional qualified name (allowing export/DLL macro before actual name).
    name_parts: list[str] = []
    while idx < end:
        t = tokens[idx]
        if t.text == "{":
            name = "".join(name_parts) if name_parts else "<anonymous>"
            return name, idx
        if t.text in (";", "=", "("):
            return None, None
        if t.text == ":":
            # Inheritance list follows; scan to '{' or ';'.
            j = idx + 1
            while j < end:
                tj = tokens[j].text
                if tj == "{":
                    name = "".join(name_parts) if name_parts else "<anonymous>"
                    return name, j
                if tj in (";", "="):
                    return None, None
                if tj == "<":
                    j = _skip_angle_brackets(tokens, j, end) + 1
                    continue
                if tj == "(":
                    j = _find_matching(tokens, j, "(", ")", end) + 1
                    continue
                j += 1
            return None, None
        if t.text == "final":
            idx += 1
            continue
        if t.text == "<":
            # Template specialization: struct Foo<int> { ... }
            close = _skip_angle_brackets(tokens, idx, end)
            if close == idx:
                return None, None
            name_parts.append("<...>")
            idx = close + 1
            continue
        if t.kind == "ident" or t.text == "::":
            if t.kind == "ident" and name_parts and name_parts[-1] != "::":
                # Two adjacent identifiers without '::': either API macro (`ASTRAL_API Foo`)
                # or variable/function declaration (`struct Foo bar`). Look ahead: if followed
                # by `{`, `:`, `final`, or `<`, treat the first as an export macro and keep the second.
                nxt = tokens[idx + 1].text if idx + 1 < end else ""
                if nxt in ("{", ":", "final", "<", "::"):
                    name_parts = [t.text]
                else:
                    return None, None
            else:
                name_parts.append(t.text)
            idx += 1
            continue
        return None, None
    return None, None


def _extract_function_name(header: Sequence[Token], lparen_idx: int, scope: Sequence[str]) -> str:
    """Extract a readable function name from header tokens up to the parameter list '('."""
    # Check for operator overload before lparen_idx.
    for k in range(lparen_idx - 1, -1, -1):
        if header[k].text == "operator":
            # Include any preceding qualifier `Foo::Bar::`
            qual_end = k
            qual_start = qual_end
            while qual_start >= 2 and header[qual_start - 1].text == "::":
                qual_start -= 2
                if qual_start > 0 and header[qual_start].text == ">":
                    # skip template args in qualifier
                    depth = 1
                    qual_start -= 1
                    while qual_start > 0 and depth > 0:
                        if header[qual_start].text == ">":
                            depth += 1
                        elif header[qual_start].text == "<":
                            depth -= 1
                        qual_start -= 1
            op_tokens = "".join(t.text for t in header[k:lparen_idx])
            if qual_start < qual_end:
                qual = "".join(t.text for t in header[qual_start:qual_end])
                return qual + op_tokens
            return "::".join([*scope, op_tokens]) if scope else op_tokens

    # Otherwise walk backwards from lparen_idx across name and qualifiers.
    i = lparen_idx - 1
    parts: list[str] = []
    # Skip trailing template args on function name e.g. foo<T>(...)
    if i >= 0 and header[i].text == ">":
        depth = 0
        while i >= 0:
            if header[i].text == ">":
                depth += 1
            elif header[i].text == "<":
                depth -= 1
                if depth == 0:
                    i -= 1
                    break
            i -= 1

    while i >= 0:
        t = header[i]
        if t.kind == "ident":
            parts.append(t.text)
            i -= 1
            if i >= 0 and header[i].text == "~":
                parts[-1] = "~" + parts[-1]
                i -= 1
            if i >= 0 and header[i].text == "::":
                parts.append("::")
                i -= 1
                # Handle Foo<T>::bar
                if i >= 0 and header[i].text == ">":
                    depth = 0
                    while i >= 0:
                        if header[i].text == ">":
                            depth += 1
                        elif header[i].text == "<":
                            depth -= 1
                            if depth == 0:
                                i -= 1
                                break
                        i -= 1
                continue
            break
        break

    if not parts:
        return "<anonymous>"
    raw_name = "".join(reversed(parts))
    if "::" in raw_name or not scope:
        return raw_name
    return "::".join([*scope, raw_name])


def _find_function_params_before_brace(header: Sequence[Token]) -> int | None:
    """If header tokens describe a function signature before '{', return index of parameter '('.

    Handles trailing qualifiers, trailing return types, noexcept(...), requires clauses,
    and constructor member-initializer lists (`: a(1), b{2}`).
    """
    n = len(header)
    if n == 0:
        return None
    if header[0].text in ("namespace", "extern", "enum"):
        return None

    # Find top-level '(' tokens (ignoring <...>, [[...]], alignas(...), decltype(...),
    # and anything after '=' or a constructor initializer ':').
    # A function signature has a top-level '(...)' whose preceding token is an identifier,
    # operator symbol, or '>' (template function), and which is followed only by valid
    # post-parameter tokens (cv/ref/noexcept/override/final/->/requires/attributes/: init-list).
    i = 0
    candidate_lparen: int | None = None
    while i < n:
        t = header[i].text
        if t == "[" and i + 1 < n and header[i + 1].text == "[":
            i = _find_matching(header, i, "[", "]", n) + 1
            continue
        if t == "<":
            close = _skip_angle_brackets(header, i, n)
            if close > i:
                i = close + 1
                continue
        if t == "=":
            # Assignment / variable initializer `Foo x = { ... }` — not a function!
            # (Exception: `operator=` before `(`!)
            if i > 0 and header[i - 1].text == "operator":
                i += 1
                continue
            return None
        if t == ":":
            # Constructor member-initializer list starts here (if we already saw params).
            if candidate_lparen is not None:
                return candidate_lparen
            return None
        if t == "(":
            # Skip alignas(...), decltype(...), noexcept(...), static_assert(...)
            prev = header[i - 1].text if i > 0 else ""
            if prev in ("alignas", "decltype", "noexcept", "sizeof", "alignof", "static_assert"):
                i = _find_matching(header, i, "(", ")", n) + 1
                continue
            if prev in KEYWORDS_NOT_FUNCTIONS:
                return None
            # Handle `operator()(...)`: the first `()` is the operator name!
            if prev == "operator":
                close_op = _find_matching(header, i, "(", ")", n)
                i = close_op + 1
                continue
            close_p = _find_matching(header, i, "(", ")", n)
            # Check if this is a function pointer return type `void (*foo())(int)` or similar;
            # otherwise verify what follows close_p.
            if _is_valid_post_params(header, close_p + 1):
                candidate_lparen = i
                return candidate_lparen
            # Might be a constructor/function whose params are followed by `: init_list`
            # or a macro/cast earlier in the header.
            i = close_p + 1
            continue
        i += 1
    return candidate_lparen


def _is_valid_post_params(header: Sequence[Token], idx: int) -> bool:
    """Return True if header[idx:] contains only valid tokens between ')' and function body '{'."""
    n = len(header)
    while idx < n:
        t = header[idx].text
        if t == ":":
            # Constructor member initializer list: valid!
            return True
        if t in ("&", "&&"):
            idx += 1
            continue
        if t in QUALIFIER_AFTER_PARAMS:
            idx += 1
            if t == "noexcept" and idx < n and header[idx].text == "(":
                idx = _find_matching(header, idx, "(", ")", n) + 1
            elif t == "requires":
                # Trailing requires clause runs up to '{' (end of header).
                return True
            continue
        if t == "->":
            # Trailing return type runs up to '{' or 'requires' or qualifier.
            return True
        if t == "[" and idx + 1 < n and header[idx + 1].text == "[":
            idx = _find_matching(header, idx, "[", "]", n) + 1
            continue
        return False
    return True


def _count_declarators(tokens: Sequence[Token], start: int, end: int) -> int:
    """Count data member declarators in a single member declaration statement (excluding ';')."""
    # Strip trailing/leading empty.
    if start >= end:
        return 0

    # Walk top-level tokens (skipping <...>, (...), [...], {...}) and count commas.
    # Anything inside an initializer after '=' still stays inside ()/{}/[] or stops at top-level ','.
    commas = 0
    has_ident = False
    i = start
    while i < end:
        t = tokens[i]
        if t.kind == "ident":
            has_ident = True
        if t.text == "(":
            i = _find_matching(tokens, i, "(", ")", end) + 1
            continue
        if t.text == "[":
            i = _find_matching(tokens, i, "[", "]", end) + 1
            continue
        if t.text == "{":
            i = _find_matching(tokens, i, "{", "}", end) + 1
            continue
        if t.text == "<":
            close = _skip_angle_brackets(tokens, i, end)
            if close > i:
                i = close + 1
                continue
        if t.text == ",":
            commas += 1
        i += 1
    return (1 + commas) if has_ident else 0


def _is_member_function_decl(tokens: Sequence[Token], start: int, end: int) -> bool:
    """Return True if a semicolon-terminated statement in a class body is a method/macro, not data."""
    i = start
    while i < end:
        t = tokens[i].text
        if t == "[" and i + 1 < end and tokens[i + 1].text == "[":
            i = _find_matching(tokens, i, "[", "]", end) + 1
            continue
        if t == "<":
            close = _skip_angle_brackets(tokens, i, end)
            if close > i:
                i = close + 1
                continue
        if t == "{":
            # In-class brace initializer `Vec3 pos{0, 0, 0};` -> data member!
            return False
        if t == "=":
            # In-class default member initializer `int x = foo();` -> data member!
            if i > start and tokens[i - 1].text == "operator":
                i += 1
                continue
            return False
        if t == "(":
            prev = tokens[i - 1].text if i > start else ""
            if prev in ("alignas", "decltype", "noexcept", "sizeof", "alignof"):
                i = _find_matching(tokens, i, "(", ")", end) + 1
                continue
            # Function pointer data member: `void (*cb)(int);`
            if i + 1 < end and tokens[i + 1].text in ("*", "&"):
                return False
            # Member pointer: `void (Foo::*cb)(int);`
            close = _find_matching(tokens, i, "(", ")", end)
            if any(tokens[k].text == "*" for k in range(i + 1, close)) and close + 1 < end and tokens[close + 1].text == "(":
                return False
            # Otherwise top-level `(...)` before `=`/`{`/`;` in a class body is a member function
            # declaration or macro invocation (C++ forbids parenthesized default member initializers).
            return True
        i += 1
    return False


def _scan_scope(
    tokens: Sequence[Token],
    start: int,
    end: int,
    in_class: bool,
    scope: tuple[str, ...],
    rel_path: str,
    findings: list[Finding],
) -> int:
    """Scan tokens[start:end] for functions and structs/classes.

    Returns the number of non-static data members found directly in this scope (when in_class).
    """
    member_count = 0
    i = start

    while i < end:
        # Skip empty semicolons.
        if tokens[i].text == ";":
            i += 1
            continue

        # Inside a class, skip access specifiers (`public:`, `private:`, `protected:`).
        if in_class and tokens[i].text in ("public", "private", "protected") and i + 1 < end and tokens[i + 1].text == ":":
            i += 2
            continue

        # Skip leading `template <...>` and track whether this declaration is a template.
        is_template = False
        stmt_start = i
        while i < end and tokens[i].text == "template":
            is_template = True
            i += 1
            if i < end and tokens[i].text == "<":
                i = _skip_angle_brackets(tokens, i, end) + 1
            # Optional `requires ...` before declaration
            if i < end and tokens[i].text == "requires":
                i += 1
                while i < end and tokens[i].text not in (";", "{", "struct", "class", "union", "template"):
                    if tokens[i].text == "(":
                        i = _find_matching(tokens, i, "(", ")", end) + 1
                    else:
                        i += 1

        decl_start = _skip_attributes(tokens, i, end)
        if decl_start >= end:
            break

        # Handle `namespace ... { ... }`
        if tokens[decl_start].text == "namespace":
            j = decl_start + 1
            while j < end and tokens[j].text not in ("{", ";", "="):
                j += 1
            if j < end and tokens[j].text == "{":
                close = _find_matching(tokens, j, "{", "}", end)
                _scan_scope(tokens, j + 1, close, False, scope, rel_path, findings)
                i = close + 1
            else:
                # namespace alias `namespace fs = std::filesystem;`
                while j < end and tokens[j].text != ";":
                    j += 1
                i = j + 1
            continue

        # Handle `extern "C" { ... }`
        if tokens[decl_start].text == "extern" and decl_start + 2 < end and tokens[decl_start + 1].kind == "str" and tokens[decl_start + 2].text == "{":
            close = _find_matching(tokens, decl_start + 2, "{", "}", end)
            _scan_scope(tokens, decl_start + 3, close, False, scope, rel_path, findings)
            i = close + 1
            continue

        # Handle `enum [class|struct] [Name] [: type] { ... } [vars];`
        if tokens[decl_start].text == "enum":
            j = decl_start + 1
            while j < end and tokens[j].text not in ("{", ";"):
                j += 1
            if j < end and tokens[j].text == "{":
                close = _find_matching(tokens, j, "{", "}", end)
                semi = close + 1
                while semi < end and tokens[semi].text != ";":
                    semi += 1
                if in_class and not is_template:
                    member_count += _count_declarators(tokens, close + 1, semi)
                i = semi + 1
            else:
                i = j + 1
            continue

        # Handle `struct` / `class` / `union` definition.
        class_name, brace_idx = _parse_class_header(tokens, decl_start, end)
        if class_name is not None and brace_idx is not None:
            close = _find_matching(tokens, brace_idx, "{", "}", end)
            if "::" in class_name or not scope:
                full_name = class_name
                nested_scope = (class_name,)
            else:
                full_name = "::".join([*scope, class_name])
                nested_scope = (*scope, class_name)
            nested_members = _scan_scope(tokens, brace_idx + 1, close, True, nested_scope, rel_path, findings)
            if _reported(nested_members, MAX_STRUCT_MEMBERS):
                findings.append(Finding(
                    path=rel_path,
                    kind="members",
                    name=full_name,
                    value=nested_members,
                    limit=MAX_STRUCT_MEMBERS,
                    line=tokens[decl_start].line,
                ))
            # Check for trailing declarator(s) after `}` before `;` (e.g. `struct { int a; } field;`).
            semi = close + 1
            while semi < end and tokens[semi].text != ";":
                if tokens[semi].text == "{":
                    semi = _find_matching(tokens, semi, "{", "}", end) + 1
                elif tokens[semi].text == "(":
                    semi = _find_matching(tokens, semi, "(", ")", end) + 1
                else:
                    semi += 1
            if in_class and not is_template and semi > close + 1:
                member_count += _count_declarators(tokens, close + 1, semi)
            i = semi + 1
            continue

        # Handle semicolon-less top-level macro invocations such as ASTRAL_REFLECT(...).
        if (
            tokens[decl_start].kind == "ident"
            and tokens[decl_start].text.isupper()
            and decl_start + 1 < end
            and tokens[decl_start + 1].text == "("
        ):
            close_p = _find_matching(tokens, decl_start + 1, "(", ")", end)
            nxt = tokens[close_p + 1].text if close_p + 1 < end else ""
            if nxt not in ("{", ":", "->", "=", ",", "const", "volatile", "noexcept", "override", "final"):
                i = close_p + (2 if nxt == ";" else 1)
                continue

        # Handle non-member declarations (`using`, `typedef`, `friend`, `static_assert`).
        if tokens[decl_start].text in NON_MEMBER_LEAD_KEYWORDS:
            j = decl_start + 1
            while j < end and tokens[j].text != ";":
                if tokens[j].text == "{":
                    j = _find_matching(tokens, j, "{", "}", end) + 1
                elif tokens[j].text == "(":
                    j = _find_matching(tokens, j, "(", ")", end) + 1
                else:
                    j += 1
            i = j + 1
            continue

        # General statement/declaration: scan forward to top-level `;` or function-body `{`.
        is_static = False
        j = decl_start
        found_func_brace: int | None = None
        seen_equals = False
        seen_params = False
        resync_at: int | None = None
        while j < end:
            tj = tokens[j].text
            if tj == "namespace" and j > decl_start:
                resync_at = j
                break
            if tj == "static" and not seen_params and not seen_equals:
                is_static = True
            if tj == ";":
                break
            if tj == "=" and not (j > decl_start and tokens[j - 1].text == "operator"):
                seen_equals = True
                j += 1
                continue
            if tj == "(":
                prev = tokens[j - 1].text if j > decl_start else ""
                if not seen_equals and prev not in ("alignas", "decltype", "noexcept", "sizeof", "alignof", "operator"):
                    seen_params = True
                j = _find_matching(tokens, j, "(", ")", end) + 1
                continue
            if tj == "[":
                j = _find_matching(tokens, j, "[", "]", end) + 1
                continue
            if tj == "<" and not seen_equals and not (j > decl_start and tokens[j - 1].text == "operator"):
                close_a = _skip_angle_brackets(tokens, j, end)
                if close_a > j:
                    j = close_a + 1
                    continue
            if tj == "{":
                if not seen_equals and seen_params:
                    # Could be a brace initializer inside a constructor member-initializer list
                    # like `Foo() : a{1}, b{2} { ... }`! Distinguish by checking what follows `}`:
                    # if followed by `,` (another initializer) or an identifier, it's an init-list
                    # brace, not the function body.
                    close_b = _find_matching(tokens, j, "{", "}", end)
                    nxt = tokens[close_b + 1].text if close_b + 1 < end else ""
                    if _is_ctor_init_brace(tokens, decl_start, j) and nxt in (",", "{"):
                        j = close_b + 1
                        continue
                    found_func_brace = j
                    break
                # Brace initializer for a variable or member (`Vec3 pos{0, 0, 0};`).
                j = _find_matching(tokens, j, "{", "}", end) + 1
                continue
            j += 1

        if resync_at is not None:
            i = resync_at
            continue

        if found_func_brace is not None:
            close_b = _find_matching(tokens, found_func_brace, "{", "}", end)
            header = tokens[decl_start:found_func_brace]
            lparen = _find_function_params_before_brace(header)
            if lparen is not None:
                func_name = _extract_function_name(header, lparen, scope)
                open_line = tokens[found_func_brace].line
                close_line = tokens[close_b].line
                body_lines = close_line - open_line + 1
                if _reported(body_lines, MAX_FUNCTION_LINES):
                    findings.append(Finding(
                        path=rel_path,
                        kind="function",
                        name=func_name,
                        value=body_lines,
                        limit=MAX_FUNCTION_LINES,
                        line=open_line,
                    ))
            i = close_b + 1
            continue

        # Semicolon-terminated declaration inside a class body: check if it's a data member.
        if in_class and not is_template and not is_static and j < end and tokens[j].text == ";":
            if not _is_forward_type_decl(tokens, decl_start, j) and not _is_member_function_decl(tokens, decl_start, j):
                member_count += _count_declarators(tokens, decl_start, j)
        i = j + 1 if j < end else end

    return member_count


def _is_forward_type_decl(tokens: Sequence[Token], start: int, end: int) -> bool:
    """Return True for `struct Foo;`, `class Bar;`, `union U;`, `enum class E : int;`."""
    if start >= end or tokens[start].text not in ("struct", "class", "union", "enum"):
        return False
    idx = start + 1
    if tokens[start].text == "enum" and idx < end and tokens[idx].text in ("class", "struct"):
        idx += 1
    idx = _skip_attributes(tokens, idx, end)
    # Expect a single (possibly qualified) type name, optionally followed by `: underlying` for enum.
    if idx >= end or tokens[idx].kind != "ident":
        return False
    idx += 1
    while idx + 1 < end and tokens[idx].text == "::" and tokens[idx + 1].kind == "ident":
        idx += 2
    if idx == end:
        return True
    if tokens[start].text == "enum" and tokens[idx].text == ":":
        return True
    return False


def _is_ctor_init_brace(tokens: Sequence[Token], decl_start: int, brace_idx: int) -> bool:
    """Return True if brace_idx is inside a constructor member-initializer list (`: a{1}, b{2}`)."""
    # Look backwards from brace_idx for top-level ':' after ')'.
    i = decl_start
    seen_rparen = False
    while i < brace_idx:
        t = tokens[i].text
        if t == "(":
            i = _find_matching(tokens, i, "(", ")", brace_idx) + 1
            seen_rparen = True
            continue
        if t == "{":
            i = _find_matching(tokens, i, "{", "}", brace_idx) + 1
            continue
        if t == ":" and seen_rparen:
            return True
        i += 1
    return False


def analyze_source(source: str, rel_path: str, near_ratio: float = 1.0) -> list[Finding]:
    """Analyze a single C++ source string and return all budget findings.

    With near_ratio < 1, values at or above near_ratio * limit are returned too (Finding.over is
    False for those)."""
    global _report_ratio
    previous, _report_ratio = _report_ratio, near_ratio
    try:
        return _analyze_source(source, rel_path)
    finally:
        _report_ratio = previous


def _analyze_source(source: str, rel_path: str) -> list[Finding]:
    findings: list[Finding] = []
    line_count = len(source.splitlines())
    if _reported(line_count, MAX_FILE_LINES):
        findings.append(Finding(
            path=rel_path,
            kind="file",
            name=Path(rel_path).name,
            value=line_count,
            limit=MAX_FILE_LINES,
            line=1,
        ))

    stripped = strip_cpp(source)
    tokens = tokenize(stripped)
    _scan_scope(tokens, 0, len(tokens), False, (), rel_path, findings)
    return findings


def discover_files(root: Path) -> list[Path]:
    """Find all C++ source/header files in the budgeted directories."""
    scan_roots: list[Path] = []
    for pattern in ("modules/*/src", "modules/*/include", "examples/*/src", "templates/*/src"):
        scan_roots.extend(sorted(root.glob(pattern)))
    tools_dir = root / "tools"
    if tools_dir.is_dir():
        scan_roots.append(tools_dir)

    seen: set[Path] = set()
    result: list[Path] = []
    for base in scan_roots:
        if not base.is_dir():
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = sorted(d for d in dirnames if d not in EXCLUDED_DIR_NAMES)
            for fname in sorted(filenames):
                p = Path(dirpath) / fname
                if p.suffix.lower() not in CPP_EXTENSIONS:
                    continue
                rel = p.relative_to(root)
                if any(part in EXCLUDED_DIR_NAMES for part in rel.parts):
                    continue
                if p not in seen:
                    seen.add(p)
                    result.append(p)
    result.sort(key=lambda p: p.relative_to(root).as_posix())
    return result


def load_exceptions(budget_path: Path) -> list[ExceptionEntry]:
    """Load exceptions from tools/size_budget.json."""
    if not budget_path.is_file():
        return []
    raw = json.loads(budget_path.read_text(encoding="utf-8"))
    items = raw["exceptions"] if isinstance(raw, dict) and "exceptions" in raw else raw
    entries: list[ExceptionEntry] = []
    for item in items:
        entries.append(ExceptionEntry(
            path=str(item["path"]).replace("\\", "/"),
            kind=str(item["kind"]),
            name=str(item.get("name", "")),
            reason=str(item.get("reason", "")),
            max=int(item["max"]) if item.get("max") is not None else None,
        ))
    return entries


def match_exception(finding: Finding, exceptions: Sequence[ExceptionEntry]) -> ExceptionEntry | None:
    """Return the matching ExceptionEntry for a finding, or None."""
    for exc in exceptions:
        if exc.path != finding.path or exc.kind != finding.kind:
            continue
        if finding.kind == "file":
            if exc.name in ("", "*", finding.name, finding.path):
                return exc
        elif exc.name == finding.name:
            return exc
    return None


@dataclass(frozen=True)
class ScanResult:
    over_budget: list[Finding]
    excepted: list[tuple[Finding, ExceptionEntry]]
    near: list[Finding]
    stale: list[ExceptionEntry]
    capped: list[tuple[Finding, ExceptionEntry]]  # excepted but grown beyond the entry's max


def scan(root: Path, budget_path: Path | None = None, near_ratio: float = DEFAULT_NEAR_RATIO) -> ScanResult:
    """Scan all budgeted files and partition findings (over / excepted / near; stale and capped entries)."""
    if budget_path is None:
        budget_path = root / "tools" / "size_budget.json"
    exceptions = load_exceptions(budget_path)
    result = ScanResult([], [], [], [], [])
    used: set[int] = set()
    for path in discover_files(root):
        rel = path.relative_to(root).as_posix()
        text = path.read_text(encoding="utf-8", errors="replace")
        for f in analyze_source(text, rel, near_ratio):
            if not f.over:
                result.near.append(f)
                continue
            exc = match_exception(f, exceptions)
            if exc is None:
                result.over_budget.append(f)
                continue
            used.add(id(exc))
            if exc.max is not None and f.value > exc.max:
                result.capped.append((f, exc))
                result.over_budget.append(f)
            else:
                result.excepted.append((f, exc))
    result.stale.extend(e for e in exceptions if id(e) not in used)
    return result


def scan_repo(root: Path, budget_path: Path | None = None) -> tuple[list[Finding], list[tuple[Finding, ExceptionEntry]]]:
    """Scan all budgeted files under root and partition findings into (over_budget, excepted)."""
    r = scan(root, budget_path)
    return r.over_budget, r.excepted


def format_summary(over_budget: Sequence[Finding], excepted: Sequence[tuple[Finding, ExceptionEntry]],
                   near: Sequence[Finding] = (), stale: Sequence[ExceptionEntry] = ()) -> list[str]:
    """Format a concise 3-line summary."""
    kinds = ("file", "function", "members")
    over_by_kind = {k: sum(1 for f in over_budget if f.kind == k) for k in kinds}
    exc_by_kind = {k: sum(1 for f, _ in excepted if f.kind == k) for k in kinds}
    return [
        f"size budget (file <= {MAX_FILE_LINES}, function <= {MAX_FUNCTION_LINES}, members <= {MAX_STRUCT_MEMBERS}):",
        "  " + ", ".join(f"{k}: {over_by_kind[k]} ({exc_by_kind[k]} excepted)" for k in kinds),
        f"over budget: {len(over_budget)}, exceptions: {len(excepted)}, stale exceptions: {len(stale)}, near limit: {len(near)}",
    ]


def main(argv: Sequence[str] | None = None) -> int:
    default_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description="Check C++ file, function, and struct sizes against budget.")
    parser.add_argument("--root", type=Path, default=default_root, help="Repository root directory")
    parser.add_argument("--budget", type=Path, default=None, help="Path to size_budget.json")
    parser.add_argument("--summary", action="store_true", help="Print at most 3 summary lines")
    parser.add_argument("--strict", action="store_true",
                        help="Exit 1 if any finding is not in size_budget.json, an exception outgrew its max, or an exception is stale")
    parser.add_argument("--near", type=float, default=DEFAULT_NEAR_RATIO,
                        help="Also list files/functions/structs at or above this fraction of a limit (1 = off)")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    r = scan(root, args.budget, min(max(args.near, 0.0), 1.0))
    capped = {id(f): exc for f, exc in r.capped}

    if not args.summary:
        for f in r.over_budget:
            cap = capped.get(id(f))
            why = f"exception cap {cap.max} exceeded" if cap else HINTS[f.kind]
            print(f"[OVER]   {f.kind:8s} {f.path}:{f.line} {f.name} ({f.value} > {f.limit}) -- {why}")
        for e in r.stale:
            print(f"[STALE]  {e.kind:8s} {e.path} {e.name} -- no longer over budget: remove it from size_budget.json")
        for f, exc in r.excepted:
            print(f"[EXCEPT] {f.kind:8s} {f.path}:{f.line} {f.name} ({f.value} > {f.limit}) -- {exc.reason}")
        for f in sorted(r.near, key=lambda f: f.value / f.limit, reverse=True):
            print(f"[NEAR]   {f.kind:8s} {f.path}:{f.line} {f.name} ({f.value} of {f.limit})")
    for line in format_summary(r.over_budget, r.excepted, r.near, r.stale):
        print(line)

    if args.strict and (r.over_budget or r.stale):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
