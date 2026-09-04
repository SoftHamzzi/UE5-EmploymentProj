#!/usr/bin/env python3
"""
code-mapper: Generate PROJECT_CONTEXT.md for token-efficient LLM code understanding.

Analyzes a project directory using AST parsing and regex, then outputs a single
markdown file containing:
  - File tree
  - Mermaid class diagram (inheritance + composition)
  - Mermaid module dependency graph
  - Ranked symbol index (public API surface)

Usage:
  python code_mapper.py [project_dir] [-o output_file] [--stdout]
"""

import ast
import os
import re
import sys
import json
import argparse
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional
from collections import defaultdict


# ─── Configuration ────────────────────────────────────────────────────────────

IGNORE_DIRS = {
    '.git', '__pycache__', 'node_modules', '.venv', 'venv', 'env',
    '.env', 'dist', 'build', '.claude', '.idea', '.vscode', 'coverage',
    '.pytest_cache', '.mypy_cache', 'htmlcov', '.eggs', 'eggs',
    'bower_components', '.next', '.nuxt', 'out', 'target', 'bin', 'obj',
    '.tox', 'site-packages', 'vendor',
    # Unreal Engine generated/cache directories
    'Intermediate', 'Binaries', 'Saved', 'DerivedDataCache', '.vs',
}

IGNORE_FILES = {
    '.DS_Store', 'Thumbs.db', 'package-lock.json', 'yarn.lock',
    'poetry.lock', 'Pipfile.lock', 'composer.lock',
}

LANGUAGE_MAP = {
    '.py': 'python',
    '.js': 'javascript', '.jsx': 'javascript', '.mjs': 'javascript',
    '.ts': 'typescript', '.tsx': 'typescript',
    '.java': 'java',
    '.go': 'go',
    '.rs': 'rust',
    '.cs': 'csharp',
    '.rb': 'ruby',
    '.php': 'php',
    '.cpp': 'cpp', '.cc': 'cpp', '.cxx': 'cpp', '.h': 'cpp', '.hpp': 'cpp',
    '.c': 'c',
    '.swift': 'swift',
    '.kt': 'kotlin',
}


# ─── Data Models ──────────────────────────────────────────────────────────────

@dataclass
class ClassInfo:
    name: str
    file: str
    line: int
    bases: list[str] = field(default_factory=list)
    interfaces: list[str] = field(default_factory=list)
    methods: list[str] = field(default_factory=list)
    attributes: list[str] = field(default_factory=list)
    docstring: Optional[str] = None
    is_abstract: bool = False
    is_dataclass: bool = False


@dataclass
class FunctionInfo:
    name: str
    file: str
    line: int
    params: list[str] = field(default_factory=list)
    return_type: Optional[str] = None
    docstring: Optional[str] = None
    is_async: bool = False


@dataclass
class ModuleInfo:
    path: str
    language: str
    classes: list[ClassInfo] = field(default_factory=list)
    functions: list[FunctionInfo] = field(default_factory=list)
    imports: list[str] = field(default_factory=list)
    description: Optional[str] = None


# ─── Python Parser (AST-based) ────────────────────────────────────────────────

class PythonParser:
    def parse(self, path: Path, rel_path: str) -> Optional[ModuleInfo]:
        try:
            source = path.read_text(encoding='utf-8', errors='ignore')
            tree = ast.parse(source)
        except SyntaxError:
            return None

        module = ModuleInfo(path=rel_path, language='python')
        module.description = ast.get_docstring(tree)

        for node in ast.iter_child_nodes(tree):
            if isinstance(node, ast.ClassDef):
                module.classes.append(self._parse_class(node, rel_path))
            elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                if not node.name.startswith('_'):
                    module.functions.append(self._parse_function(node, rel_path))
            elif isinstance(node, ast.Import):
                for alias in node.names:
                    module.imports.append(alias.name)
            elif isinstance(node, ast.ImportFrom) and node.module:
                module.imports.append(node.module)

        return module

    def _parse_class(self, node: ast.ClassDef, file: str) -> ClassInfo:
        bases, interfaces = [], []
        for base in node.bases:
            name = self._name_from_node(base)
            if name:
                # Heuristic: PascalCase ending in 'Interface'/'Protocol'/'ABC' → interface
                if any(name.endswith(s) for s in ('Interface', 'Protocol', 'ABC', 'Mixin')):
                    interfaces.append(name)
                else:
                    bases.append(name)

        methods, attributes = [], []
        has_abstractmethod = False
        is_dataclass = any(
            (isinstance(d, ast.Name) and d.id == 'dataclass') or
            (isinstance(d, ast.Attribute) and d.attr == 'dataclass')
            for d in node.decorator_list
        )

        for item in node.body:
            if isinstance(item, (ast.FunctionDef, ast.AsyncFunctionDef)):
                methods.append(item.name)
                for d in item.decorator_list:
                    if (isinstance(d, ast.Name) and d.id == 'abstractmethod') or \
                       (isinstance(d, ast.Attribute) and d.attr == 'abstractmethod'):
                        has_abstractmethod = True
            elif isinstance(item, ast.AnnAssign) and isinstance(item.target, ast.Name):
                if not item.target.id.startswith('_'):
                    type_str = self._name_from_node(item.annotation) or ''
                    attributes.append(f'{item.target.id}: {type_str}' if type_str else item.target.id)

        return ClassInfo(
            name=node.name,
            file=file,
            line=node.lineno,
            bases=bases,
            interfaces=interfaces,
            methods=methods,
            attributes=attributes[:6],
            docstring=ast.get_docstring(node),
            is_abstract=has_abstractmethod,
            is_dataclass=is_dataclass,
        )

    def _parse_function(self, node, file: str) -> FunctionInfo:
        params = []
        for arg in node.args.args:
            if arg.arg in ('self', 'cls'):
                continue
            ann = self._name_from_node(arg.annotation) if arg.annotation else None
            params.append(f'{arg.arg}: {ann}' if ann else arg.arg)

        return_type = self._name_from_node(node.returns) if node.returns else None

        return FunctionInfo(
            name=node.name,
            file=file,
            line=node.lineno,
            params=params[:5],
            return_type=return_type,
            docstring=ast.get_docstring(node),
            is_async=isinstance(node, ast.AsyncFunctionDef),
        )

    def _name_from_node(self, node) -> Optional[str]:
        if node is None:
            return None
        if isinstance(node, ast.Name):
            return node.id
        if isinstance(node, ast.Attribute):
            return node.attr
        if isinstance(node, ast.Subscript):
            return self._name_from_node(node.value)
        if isinstance(node, ast.Constant):
            return str(node.value)
        return None


# ─── Generic Regex Parser ─────────────────────────────────────────────────────

class RegexParser:
    CLASS_PATTERNS: dict[str, str] = {
        'javascript': r'(?:export\s+)?class\s+(\w+)(?:\s+extends\s+(\w+))?',
        'typescript': r'(?:export\s+)?(?:abstract\s+)?class\s+(\w+)(?:\s+extends\s+(\w+))?(?:\s+implements\s+([\w,\s<>]+))?',
        'java':       r'(?:public\s+)?(?:abstract\s+)?class\s+(\w+)(?:\s+extends\s+(\w+))?(?:\s+implements\s+([\w,\s<>]+))?',
        'go':         r'type\s+(\w+)\s+struct',
        'rust':       r'(?:pub\s+)?struct\s+(\w+)',
        'csharp':     r'(?:public\s+)?(?:abstract\s+)?class\s+(\w+)(?:\s*:\s*([\w,\s<>]+))?',
        'swift':      r'(?:class|struct|protocol|enum)\s+(\w+)(?:\s*:\s*([\w,\s]+))?',
        'kotlin':     r'(?:data\s+)?(?:abstract\s+)?class\s+(\w+)(?:\s*\(|\s*:\s*(\w+))?',
        'ruby':       r'class\s+(\w+)(?:\s*<\s*(\w+))?',
        'php':        r'class\s+(\w+)(?:\s+extends\s+(\w+))?(?:\s+implements\s+([\w,\s]+))?',
        # Optional FOO_API export macro (Unreal Engine, Windows DLL exports, etc.)
        # sits between 'class' and the real name, e.g. `class ENGINE_API AActor : public UObject`
        'cpp':        r'class\s+(?:\w+_API\s+)?(\w+)(?:\s*:\s*(?:public|private|protected)\s+(?:\w+_API\s+)?(\w+))?',
    }

    # Keywords that can precede a `(` like a function but never introduce a
    # declaration — excluded so control-flow statements aren't mistaken for
    # `TYPE NAME(...)` declarations by the cpp pattern below.
    _CPP_FUNC_EXCLUDE = (
        r'if|while|for|switch|return|catch|else|case|sizeof|new|delete|'
        r'using|namespace|typedef|template'
    )

    FUNC_PATTERNS: dict[str, str] = {
        'javascript': r'(?:export\s+)?(?:async\s+)?function\s+(\w+)\s*\(([^)]*)\)',
        'typescript': r'(?:export\s+)?(?:async\s+)?function\s+(\w+)\s*\(([^)]*)\)',
        'java':       r'(?:public|protected)\s+(?:static\s+)?[\w<>\[\]]+\s+(\w+)\s*\(([^)]*)\)',
        'go':         r'func\s+(\w+)\s*\(([^)]*)\)',
        'rust':       r'(?:pub\s+)?fn\s+(\w+)\s*\(([^)]*)\)',
        'csharp':     r'(?:public|protected)\s+(?:static\s+)?[\w<>\[\]]+\s+(\w+)\s*\(([^)]*)\)',
        # Single-line member declarations, e.g. `virtual void BeginPlay() override;`
        # or `void Fire(float Delay);`. Deliberately declaration-only (ends in `;`,
        # never `{`) to avoid matching control-flow statements inside bodies.
        'cpp':        (
            r'^[ \t]*(?:virtual[ \t]+|static[ \t]+|FORCEINLINE[ \t]+|inline[ \t]+|'
            r'friend[ \t]+|explicit[ \t]+)*(?:const[ \t]+)?'
            r'(?!(?:' + _CPP_FUNC_EXCLUDE + r')\b)[A-Za-z_]\w*(?:::\w+)?'
            r'(?:[ \t]*<[^;{}()\n]*>)?[ \t]*[\*&]*[ \t]+(\w+)[ \t]*'
            r'\(([^;{}()\n]*)\)[ \t]*(?:const)?[ \t]*(?:override)?[ \t]*(?:final)?[ \t]*;'
        ),
    }

    # Languages whose class bodies are brace-delimited and whose FUNC_PATTERNS
    # match member declarations — for these we can attribute each matched
    # function to its enclosing class (see _match_braces / parse below).
    _BRACE_LANGS = {'cpp', 'java', 'csharp'}

    # Same idea as _CPP_FUNC_EXCLUDE, for statements that look like
    # `TYPE NAME ... ;` but aren't a member-variable declaration.
    _CPP_ATTR_EXCLUDE = (
        r'return|using|typedef|friend|enum|struct|class|template|namespace|'
        r'static_assert|else|case|default|goto|break|continue|delete|new|sizeof'
    )

    ATTR_PATTERNS: dict[str, str] = {
        # Single-line member-variable declarations, e.g. `float Health = 100.f;`
        # or `TWeakObjectPtr<AEPCharacter> CachedCharacter;`. Matched only
        # against class-body text at the class's own brace depth (see parse),
        # so locals inside inline method bodies don't get mistaken for members.
        'cpp': (
            r'^[ \t]*(?:static[ \t]+|mutable[ \t]+)*(?:const[ \t]+)?'
            r'(?!(?:' + _CPP_ATTR_EXCLUDE + r')\b)[A-Za-z_]\w*(?:::\w+)?'
            r'(?:[ \t]*<[^;{}()\n]*>)?[ \t]*[\*&]*[ \t]+(\w+)'
            r'(?:\[[^\]\n]*\])?[ \t]*(?:=[^;\n]*)?;'
        ),
    }

    IMPORT_PATTERNS: dict[str, str] = {
        'javascript': r'(?:import|require)\s+.*?["\']([./][\w/.-]+)["\']',
        'typescript': r'from\s+["\']([./][\w/.-]+)["\']',
        'java':       r'import\s+([\w.]+);',
        'go':         r'"([\w./]+)"',
        'rust':       r'use\s+([\w:]+)',
        'csharp':     r'using\s+([\w.]+);',
        'ruby':       r'require(?:_relative)?\s+["\']([./][\w/.]+)["\']',
        'php':        r'(?:include|require)(?:_once)?\s+["\']([./][\w/.]+)["\']',
        'kotlin':     r'import\s+([\w.]+)',
        'swift':      r'import\s+(\w+)',
    }

    @staticmethod
    def _mask_for_braces(source: str) -> str:
        """Blank out string/char literals and comments (keeping length/positions
        intact) so a stray brace inside one doesn't throw off brace matching."""
        out = list(source)
        i, n = 0, len(source)
        while i < n:
            c = source[i]
            if c == '/' and i + 1 < n and source[i + 1] == '/':
                j = source.find('\n', i)
                j = n if j == -1 else j
                for k in range(i, j):
                    out[k] = ' '
                i = j
            elif c == '/' and i + 1 < n and source[i + 1] == '*':
                j = source.find('*/', i + 2)
                j = n if j == -1 else j + 2
                for k in range(i, j):
                    if out[k] != '\n':
                        out[k] = ' '
                i = j
            elif c in ('"', "'"):
                quote = c
                j = i + 1
                while j < n and source[j] != quote:
                    j += 2 if source[j] == '\\' else 1
                j = min(j + 1, n)
                for k in range(i, j):
                    if out[k] != '\n':
                        out[k] = ' '
                i = j
            else:
                i += 1
        return ''.join(out)

    @staticmethod
    def _match_braces(masked: str) -> dict[int, int]:
        """Map each '{' index to its matching '}' index (single pass)."""
        pairs: dict[int, int] = {}
        stack: list[int] = []
        for i, ch in enumerate(masked):
            if ch == '{':
                stack.append(i)
            elif ch == '}' and stack:
                pairs[stack.pop()] = i
        return pairs

    @staticmethod
    def _brace_depths(masked: str) -> list[int]:
        """Nesting depth *before* each character (index i = depth entering
        position i). Used to tell a direct class member (depth == class body
        depth) apart from a local variable inside a nested method body."""
        depths = [0] * (len(masked) + 1)
        depth = 0
        for i, ch in enumerate(masked):
            depths[i] = depth
            if ch == '{':
                depth += 1
            elif ch == '}':
                depth = max(0, depth - 1)
        depths[len(masked)] = depth
        return depths

    @staticmethod
    def _access_at(pos: int, labels: list[tuple[int, str]]) -> str:
        """Effective public/protected/private section at `pos`, given the
        class's sorted list of access-label positions. `class` in C++
        defaults to private until the first label."""
        level = 'private'
        for label_pos, lvl in labels:
            if label_pos >= pos:
                break
            level = lvl
        return level

    def parse(self, path: Path, rel_path: str, language: str) -> Optional[ModuleInfo]:
        try:
            source = path.read_text(encoding='utf-8', errors='ignore')
        except OSError:
            return None

        module = ModuleInfo(path=rel_path, language=language)
        cls_pattern = self.CLASS_PATTERNS.get(language, r'class\s+(\w+)')

        # Brace-delimited languages: precompute matching so each method can be
        # attributed to its enclosing class instead of landing in a flat,
        # unscoped function list.
        masked = self._mask_for_braces(source) if language in self._BRACE_LANGS else ''
        brace_match = self._match_braces(masked) if masked else {}
        depths = self._brace_depths(masked) if masked else []
        # cpp only: `public:`/`protected:`/`private:` section labels, so
        # private members can be left out of the "public API surface" output
        # the same way Python's `_foo` convention already is.
        label_matches = (
            [(m.start(), m.group(1)) for m in re.finditer(r'^[ \t]*(public|protected|private)[ \t]*:', masked, re.MULTILINE)]
            if language == 'cpp' and masked else []
        )
        class_spans: list[tuple[int, int, ClassInfo, list[tuple[int, str]]]] = []

        for match in re.finditer(cls_pattern, source, re.MULTILINE):
            name = match.group(1)
            if language == 'cpp':
                prefix = source[max(0, match.start() - 50):match.start()].rstrip()
                if prefix.endswith('<') or prefix.endswith(','):
                    continue  # elaborated-type-specifier in a template arg, e.g. `TArray<class Foo>`
                if source[match.end():match.end() + 200].lstrip().startswith(';'):
                    continue  # forward declaration (`class Foo;`), not a definition
            bases = []
            if match.lastindex and match.lastindex >= 2 and match.group(2):
                bases = [b.strip() for b in re.split(r'[,<>]', match.group(2)) if b.strip()]
            line = source[:match.start()].count('\n') + 1
            info = ClassInfo(name=name, file=rel_path, line=line, bases=bases)
            module.classes.append(info)

            if brace_match:
                semi_pos = masked.find(';', match.end())
                open_pos = masked.find('{', match.end())
                if open_pos != -1 and (semi_pos == -1 or open_pos < semi_pos):
                    close_pos = brace_match.get(open_pos)
                    if close_pos is not None:
                        body_depth = depths[open_pos] + 1
                        cls_labels = sorted(
                            (pos, lvl) for pos, lvl in label_matches
                            if open_pos < pos < close_pos and depths[pos] == body_depth
                        )
                        class_spans.append((open_pos, close_pos, info, cls_labels))

        # Innermost span first, so a method inside a nested class is claimed
        # by that class rather than the outer one.
        class_spans.sort(key=lambda s: s[1] - s[0])

        class_names = {c.name for c in module.classes}
        func_pattern = self.FUNC_PATTERNS.get(language)
        if func_pattern:
            for match in re.finditer(func_pattern, source, re.MULTILINE):
                name = match.group(1)
                if name in class_names:
                    continue  # skip constructors (name == class name)
                span = next(
                    (s for s in class_spans if s[0] < match.start() < s[1]),
                    None,
                )
                if span is not None:
                    _, _, owner, labels = span
                    if language != 'cpp' or self._access_at(match.start(), labels) != 'private':
                        owner.methods.append(name)
                else:
                    line = source[:match.start()].count('\n') + 1
                    module.functions.append(FunctionInfo(name=name, file=rel_path, line=line))

        attr_pattern = self.ATTR_PATTERNS.get(language)
        if attr_pattern and class_spans:
            for match in re.finditer(attr_pattern, masked, re.MULTILINE):
                name = match.group(1)
                # Only a direct member of a class (depth == class body depth),
                # not a local variable inside a nested method/lambda/enum body.
                span = next(
                    (
                        s for s in class_spans
                        if s[0] < match.start() < s[1] and depths[match.start()] == depths[s[0]] + 1
                    ),
                    None,
                )
                if span is not None:
                    _, _, owner, labels = span
                    if len(owner.attributes) < 10 and self._access_at(match.start(), labels) != 'private':
                        owner.attributes.append(name)

        imp_pattern = self.IMPORT_PATTERNS.get(language)
        if imp_pattern:
            for match in re.finditer(imp_pattern, source, re.MULTILINE):
                module.imports.append(match.group(1))

        return module


# ─── Core Mapper ──────────────────────────────────────────────────────────────

class CodeMapper:
    def __init__(self, root_dir: str):
        self.root = Path(root_dir).resolve()
        self.modules: dict[str, ModuleInfo] = {}
        self._py_parser = PythonParser()
        self._rx_parser = RegexParser()

    # ── Scanning ──────────────────────────────────────────────────────────────

    def scan(self) -> None:
        for path in sorted(self.root.rglob('*')):
            if not path.is_file():
                continue
            if not self._should_process(path):
                continue
            lang = LANGUAGE_MAP.get(path.suffix.lower())
            if not lang:
                continue
            rel = str(path.relative_to(self.root))
            if lang == 'python':
                module = self._py_parser.parse(path, rel)
            else:
                module = self._rx_parser.parse(path, rel, lang)
            if module:
                self.modules[rel] = module

    def _should_process(self, path: Path) -> bool:
        if path.name in IGNORE_FILES:
            return False
        if any(part in IGNORE_DIRS for part in path.parts):
            return False
        try:
            if path.stat().st_size > 500_000:
                return False
        except OSError:
            return False
        return True

    # ── Helpers ───────────────────────────────────────────────────────────────

    @staticmethod
    def _safe_id(name: str) -> str:
        """Make a string safe for use as a Mermaid node ID."""
        return re.sub(r'[^a-zA-Z0-9_]', '_', name)

    def _all_classes(self) -> dict[str, ClassInfo]:
        out: dict[str, ClassInfo] = {}
        for m in self.modules.values():
            for c in m.classes:
                out[c.name] = c
        return out

    def _source_line_count(self) -> int:
        total = 0
        for rel in self.modules:
            try:
                total += (self.root / rel).read_text(errors='ignore').count('\n')
            except OSError:
                pass
        return total

    # ── Generators ────────────────────────────────────────────────────────────

    def _file_tree(self) -> str:
        lines = ['```', f'{self.root.name}/']
        dirs_seen: set[str] = set()
        for path in sorted(self.modules):
            parts = Path(path).parts
            for i, part in enumerate(parts):
                key = '/'.join(parts[:i + 1])
                indent = '  ' * i
                if i < len(parts) - 1:
                    if key not in dirs_seen:
                        lines.append(f'{indent}├── {part}/')
                        dirs_seen.add(key)
                else:
                    lines.append(f'{indent}└── {part}')
        lines.append('```')
        return '\n'.join(lines)

    def _class_diagram(self) -> str:
        all_cls = self._all_classes()
        if not all_cls:
            return '_No classes found._'

        lines = ['```mermaid', 'classDiagram', '    direction LR']

        for name, cls in sorted(all_cls.items()):
            sid = self._safe_id(name)
            qualifier = '«abstract» ' if cls.is_abstract else ('«dataclass» ' if cls.is_dataclass else '')
            lines.append(f'    class {sid}["{qualifier}{name}"] {{')

            # Show typed attributes (dataclasses / annotated classes)
            for attr in cls.attributes[:4]:
                lines.append(f'        +{attr}')

            # Public methods first, then __init__, skip private
            pub = [m for m in cls.methods if not m.startswith('_')]
            init = ['__init__'] if '__init__' in cls.methods else []
            shown = (pub + init)[:7]
            for m in shown:
                lines.append(f'        +{m}()')
            if len(pub) + len(init) > 7:
                lines.append(f'        ...')

            lines.append('    }')

            for base in cls.bases:
                if base in all_cls:
                    lines.append(f'    {self._safe_id(base)} <|-- {sid} : extends')
            for iface in cls.interfaces:
                if iface in all_cls:
                    lines.append(f'    {self._safe_id(iface)} <|.. {sid} : implements')

        lines.append('```')
        return '\n'.join(lines)

    def _dependency_graph(self) -> str:
        stem_map: dict[str, str] = {Path(p).stem: p for p in self.modules}
        edges: set[tuple[str, str]] = set()

        for path, module in self.modules.items():
            src_stem = Path(path).stem
            for imp in module.imports:
                # Check the last segment of the import path
                candidate = imp.split('.')[-1].split('/')[-1]
                if candidate in stem_map and candidate != src_stem:
                    edges.add((self._safe_id(src_stem), self._safe_id(candidate)))

        if not edges:
            return '_No internal dependencies detected._'

        lines = ['```mermaid', 'graph TD']
        for src, dst in sorted(edges):
            lines.append(f'    {src} --> {dst}')
        lines.append('```')
        return '\n'.join(lines)

    def _symbol_index(self) -> str:
        parts = []
        for path, module in sorted(self.modules.items()):
            if not module.classes and not module.functions:
                continue
            lang_badge = f'`{module.language}`'
            parts.append(f'\n**`{path}`** {lang_badge}')
            if module.description:
                parts.append(f'> {module.description.splitlines()[0][:100]}')

            for cls in module.classes:
                abstract = '«abstract» ' if cls.is_abstract else ''
                dc = '«dataclass» ' if cls.is_dataclass else ''
                bases_str = f'({", ".join(cls.bases + cls.interfaces)})' if cls.bases or cls.interfaces else ''
                desc = cls.docstring.splitlines()[0][:80] if cls.docstring else ''
                parts.append(f'- `{abstract}{dc}class {cls.name}{bases_str}`{f"  — {desc}" if desc else ""}')

                pub_methods = [m for m in cls.methods if not m.startswith('_')]
                if pub_methods:
                    shown = pub_methods[:8]
                    more = f', +{len(pub_methods) - 8} more' if len(pub_methods) > 8 else ''
                    parts.append(f'  methods: `{"`, `".join(shown)}`{more}')

            for fn in module.functions:
                async_pfx = 'async ' if fn.is_async else ''
                keyword = 'def ' if module.language == 'python' else ''
                params = ', '.join(fn.params[:4])
                ret = f' → {fn.return_type}' if fn.return_type else ''
                desc = fn.docstring.splitlines()[0][:80] if fn.docstring else ''
                parts.append(f'- `{async_pfx}{keyword}{fn.name}({params}){ret}`{f"  — {desc}" if desc else ""}')

        return '\n'.join(parts)

    # ── Top-level generate ────────────────────────────────────────────────────

    def generate(self, output_path: Optional[str] = None) -> str:
        self.scan()
        if output_path is None:
            output_path = str(self.root / 'PROJECT_CONTEXT.md')

        source_lines = self._source_line_count()
        lang_counts: dict[str, int] = defaultdict(int)
        for m in self.modules.values():
            lang_counts[m.language] += 1

        lang_summary = ', '.join(f'{lang}×{n}' for lang, n in sorted(lang_counts.items()))
        total_classes = sum(len(m.classes) for m in self.modules.values())
        total_fns = sum(len(m.functions) for m in self.modules.values())

        header = '\n'.join([
            '# Project Context',
            '',
            '> **Auto-generated by [code-mapper](https://github.com/your-org/code-mapper).**  ',
            '> Read this file instead of scanning source files — it captures the full structure at a fraction of the token cost.',
            f'> `{self.root.name}` · {len(self.modules)} files ({lang_summary}) · '
            f'{total_classes} classes · {total_fns} functions · ~{source_lines:,} source lines',
            '',
            '---',
        ])

        body = '\n'.join([
            '\n## File Structure\n',
            self._file_tree(),
            '\n## Class Diagram\n',
            self._class_diagram(),
            '\n## Module Dependencies\n',
            self._dependency_graph(),
            '\n## Symbol Index\n',
            self._symbol_index(),
        ])

        content = header + body
        Path(output_path).write_text(content, encoding='utf-8')

        # ── Stats ──────────────────────────────────────────────────────────
        approx_tokens = len(content) // 4
        src_tokens = source_lines * 5  # rough: ~5 tokens/line average
        savings_pct = max(0, (1 - approx_tokens / src_tokens) * 100) if src_tokens else 0

        print(f'✓  Generated: {output_path}')
        print(f'   Files analyzed : {len(self.modules)}  ({lang_summary})')
        print(f'   Classes        : {total_classes}  |  Functions: {total_fns}')
        print(f'   Output size    : {len(content):,} chars  (~{approx_tokens:,} tokens)')
        print(f'   Source lines   : ~{source_lines:,}  (~{src_tokens:,} tokens to read raw)')
        if savings_pct > 0:
            print(f'   Token savings  : ~{savings_pct:.0f}%')

        return content


# ─── CLI ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description='Generate PROJECT_CONTEXT.md for token-efficient LLM code understanding.',
        epilog='Example: python code_mapper.py ./my-project -o ./my-project/.claude/PROJECT_CONTEXT.md',
    )
    parser.add_argument('project_dir', nargs='?', default='.',
                        help='Root directory to analyze (default: current directory)')
    parser.add_argument('-o', '--output',
                        help='Output file path (default: PROJECT_CONTEXT.md inside project_dir)')
    parser.add_argument('--stdout', action='store_true',
                        help='Print to stdout instead of writing a file')
    args = parser.parse_args()

    mapper = CodeMapper(args.project_dir)

    if args.stdout:
        mapper.scan()
        print(mapper._file_tree())
        print(mapper._class_diagram())
        print(mapper._dependency_graph())
        print(mapper._symbol_index())
    else:
        mapper.generate(args.output)


if __name__ == '__main__':
    main()
