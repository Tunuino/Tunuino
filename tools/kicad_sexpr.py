# Copyright (c) 2026 Tunuino.
"""Bounded reader for the S-expressions used in KiCad source files."""
import re
TOKEN = re.compile(r'\s+|\(|\)|"(?:\\.|[^"\\])*"|[^\s()"]+')
MAX_BYTES = 16 * 1024 * 1024
MAX_NODES = 1500000
MAX_DEPTH = 64


def parse(data):
    if not data or len(data) > MAX_BYTES:
        raise ValueError('Empty or oversized KiCad file')
    try:
        source = data.decode('utf-8-sig')
    except UnicodeError as error:
        raise ValueError('KiCad file must be UTF-8') from error
    if '\0' in source:
        raise ValueError('NUL in KiCad file')
    stack, roots = [], []
    position, count = 0, 0
    while position < len(source):
        match = TOKEN.match(source, position)
        if match is None:
            raise ValueError('Invalid or unterminated token')
        token = match.group(); position = match.end()
        if token.isspace():
            continue
        count += 1
        if count > MAX_NODES:
            raise ValueError('Too many KiCad tokens')
        if token == '(':
            if len(stack) >= MAX_DEPTH:
                raise ValueError('KiCad nesting exceeds limit')
            node = []
            if stack:
                stack[-1].append(node)
            else:
                roots.append(node)
            stack.append(node)
        elif token == ')':
            if not stack or not stack[-1]:
                raise ValueError('Unbalanced or empty S-expression')
            stack.pop()
        else:
            if not stack:
                raise ValueError('Atom outside root')
            if token.startswith('"'):
                # Keep unknown escape sequences unchanged; only KiCad string
                # delimiters/backslashes are unescaped, never evaluated as code.
                token = re.sub(r'\\(["\\])', r'\1', token[1:-1])
            stack[-1].append(token)
    if stack or len(roots) != 1:
        raise ValueError('Expected one complete KiCad root')
    return roots[0]


def children(node, key):
    return [value for value in node[1:] if isinstance(value, list) and value and value[0] == key]


def single(node, key, required=True):
    found = children(node, key)
    if len(found) != 1:
        if not found and not required:
            return None
        raise ValueError(f'Expected one {key}')
    return found[0]
