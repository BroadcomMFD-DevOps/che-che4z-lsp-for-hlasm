/*
 * Copyright (c) 2023 Broadcom.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *   Broadcom, Inc. - initial API and implementation
 */

import * as vscode from 'vscode';
import { EXTENSION_ID, initialBlanks, languageIdHlasmListing } from './constants';

const ordchar = /[A-Za-z0-9$#@_]/;

const listingStart = /^(.?)                                         High Level Assembler Option Summary                   .............   Page    1/;

type RegexSet = {
    objShortCode: RegExp,
    objLongCode: RegExp,
    lineText: RegExp,
    pageBoundary: RegExp,

    ordinaryRefFirstLine: RegExp,
    ordinaryRefAltSecondLine: RegExp,
    ordinaryRefRest: RegExp,

    externalRefFirstLine: RegExp,
    externalRefSecondLine: RegExp,

    dsectRefFirstLine: RegExp,
    dsectRefSecondLine: RegExp,

    usingMapLine: RegExp,

    sectionAddrShort: RegExp,
    sectionAddrLong: RegExp,
};

const withoutPrefix = {
    objShortCode: /^(?:([0-9ABCDEF]{6})| {6}) (.{26})( *\d+)\D(?:([a-zA-Z$#@_][a-zA-Z$#@0-9_]*) )?/,
    objLongCode: /^(?:([0-9ABCDEF]{8})| {8}) (.{32})( *\d+)\D(?:([a-zA-Z$#@_][a-zA-Z$#@0-9_]*) )?/,
    lineText: /^(?:(Return Code )|\*\* (ASMA\d\d\d[NIWES] .+)|((?:  |[CDR]-)Loc  Object Code    Addr1 Addr2  Stmt |(?:  |[CDR]-)Loc    Object Code      Addr1    Addr2    Stmt )|(.{111})Page +\d+)/,
    pageBoundary: /^.+(?:(High Level Assembler Option Summary)|(External Symbol Dictionary)|(Relocation Dictionary)|(Ordinary Symbol and Literal Cross Reference)|(Macro and Copy Code Source Summary)|(Dsect Cross Reference)|(Using Map)|(General Purpose Register Cross Reference)|(Diagnostic Cross Reference and Assembler Summary))/,

    ordinaryRefFirstLine: /^(?:([a-zA-Z$#@_][a-zA-Z$#@0-9_]{0,7}) +(\d+) ([0-9A-F]{8}) ([0-9A-F]{8}) (.) ..(.). ...  ....... +(\d+) +(\d.+|)|([a-zA-Z$#@_][a-zA-Z$#@0-9_]{8,}))/,
    ordinaryRefAltSecondLine: /^( {9,})(\d+) ([0-9A-F]{8}) ([0-9A-F]{8}) (.) ..(.). ...  ....... +(\d+) +(\d.+|)/,
    ordinaryRefRest: /^ {60,}(\d.+)/,

    externalRefFirstLine: /^([a-zA-Z$#@_][a-zA-Z$#@0-9_]{0,7}) +([A-Z]+) +([0-9A-F]+) ( {8}|[0-9A-F]{8}) ( {8}|[0-9A-F]{8})  ( {8}|[0-9A-F]{8}) |^([a-zA-Z$#@_][a-zA-Z$#@0-9_]{8,} +)/,
    externalRefSecondLine: /^( +)([A-Z]+) +([0-9A-F]+) ( {9}|[0-9A-F]{8} )( {9}|[0-9A-F]{8} ) ( {8}|[0-9A-F]{8})/,

    dsectRefFirstLine: /^(?:([a-zA-Z$#@_][a-zA-Z$#@0-9_]{0,7}) +([0-9A-F]{8}) +([0-9A-F]{8}) +(\d+)|([a-zA-Z$#@_][a-zA-Z$#@0-9_]{8,}))/,
    dsectRefSecondLine: /^( +)([0-9A-F]{8}) +([0-9A-F]{8}) +(\d+)/,

    usingMapLine: /^ *(\d+)  ([0-9A-F]{8})  ([0-9A-F]{8}) (?:USING|DROP|PUSH|POP) +/,

    sectionAddrShort: /^ {15}[0-9A-F]{6} [0-9A-F]{6}$/,
    sectionAddrLong: /^ {15}[0-9A-F]{8} [0-9A-F]{8}$/,
};

const withPrefix = {
    objShortCode: /^.(?:([0-9ABCDEF]{6})| {6}) (.{26})( *\d+)\D(?:([a-zA-Z$#@_][a-zA-Z$#@0-9_]*) )?/,
    objLongCode: /^.(?:([0-9ABCDEF]{8})| {8}) (.{32})( *\d+)\D(?:([a-zA-Z$#@_][a-zA-Z$#@0-9_]*) )?/,
    lineText: /^.(?:(Return Code )|\*\* (ASMA\d\d\d[NIWES] .+)|((?:  |[CDR]-)Loc  Object Code    Addr1 Addr2  Stmt |(?:  |[CDR]-)Loc    Object Code      Addr1    Addr2    Stmt )|(.{111})Page +\d+)/,
    pageBoundary: /^.+(?:(High Level Assembler Option Summary)|(External Symbol Dictionary)|(Relocation Dictionary)|(Ordinary Symbol and Literal Cross Reference)|(Macro and Copy Code Source Summary)|(Dsect Cross Reference)|(Using Map)|(General Purpose Register Cross Reference)|(Diagnostic Cross Reference and Assembler Summary))/,

    ordinaryRefFirstLine: /^.(?:([a-zA-Z$#@_][a-zA-Z$#@0-9_]{0,7}) +(\d+) ([0-9A-F]{8}) ([0-9A-F]{8}) (.) ..(.). ...  ....... +(\d+) +(\d.+|)|([a-zA-Z$#@_][a-zA-Z$#@0-9_]{8,}))/,
    ordinaryRefAltSecondLine: /^.( {9,})(\d+) ([0-9A-F]{8}) ([0-9A-F]{8}) (.) ..(.). ...  ....... +(\d+) +(\d.+|)/,
    ordinaryRefRest: /^. {60,}(\d.+)/,

    externalRefFirstLine: /^.([a-zA-Z$#@_][a-zA-Z$#@0-9_]{0,7}) +([A-Z]+) +([0-9A-F]+) ( {8}|[0-9A-F]{8}) ( {8}|[0-9A-F]{8})  ( {8}|[0-9A-F]{8}) |^([a-zA-Z$#@_][a-zA-Z$#@0-9_]{8,} +)/,
    externalRefSecondLine: /^.( +)([A-Z]+) +([0-9A-F]+) ( {9}|[0-9A-F]{8} )( {9}|[0-9A-F]{8} ) ( {8}|[0-9A-F]{8})/,

    dsectRefFirstLine: /^.(?:([a-zA-Z$#@_][a-zA-Z$#@0-9_]{0,7}) +([0-9A-F]{8}) +([0-9A-F]{8}) +(\d+)|([a-zA-Z$#@_][a-zA-Z$#@0-9_]{8,}))/,
    dsectRefSecondLine: /^.( +)([0-9A-F]{8}) +([0-9A-F]{8}) +(\d+)/,

    usingMapLine: /^. *(\d+)  ([0-9A-F]{8})  ([0-9A-F]{8}) (?:USING|DROP|PUSH|POP) +/,

    sectionAddrShort: /^ {15}[0-9A-F]{6} [0-9A-F]{6}$/,
    sectionAddrLong: /^ {15}[0-9A-F]{8} [0-9A-F]{8}$/,
};

const enum BoudnaryType {
    ReturnStatement,
    Diagnostic,
    ObjectCodeHeader,
    OptionsRef,
    ExternalRef,
    RelocationDict,
    OrdinaryRef,
    MacroRef,
    DsectRef,
    UsingsRef,
    RegistersRef,
    DiagnosticRef,
    OtherBoundary,
};

function identifyBoundary(s: string, r: RegexSet): { type: BoudnaryType, capture: string } | undefined {
    const l = r.lineText.exec(s);
    if (!l) return undefined;

    for (let i = 1; i < l.length - 1; ++i) {
        if (l[i])
            return { type: <BoudnaryType>(i - 1), capture: l[i] };
    }

    const m = r.pageBoundary.exec(l[l.length - 1]);
    if (!m) return { type: BoudnaryType.OtherBoundary, capture: l[l.length - 1] };

    for (let i = 1; i < m.length; ++i) {
        if (m[i])
            return { type: <BoudnaryType>(l.length - 2 + i - 1), capture: m[i] };
    }

    return undefined;
}

function asLevel(code: string) {
    if (code.endsWith('N')) return vscode.DiagnosticSeverity.Hint;
    if (code.endsWith('I')) return vscode.DiagnosticSeverity.Information;
    if (code.endsWith('W')) return vscode.DiagnosticSeverity.Warning;
    return vscode.DiagnosticSeverity.Error;
}

type Section = {
    start: number,
    end: number,
};

type ListingLine = {
    listingLine: number,
    address: number | undefined,
};

type CodeSection = Section & {
    title: string,
    codeStart: number,
    dsect: boolean,
    firstStmtNo: number | undefined,
};

type Listing = {
    start: number,
    end: number,
    hasPrefix: boolean,
    type?: 'long' | 'short';
    diagnostics: vscode.Diagnostic[],
    statementLines: Map<number, ListingLine>,
    symbols: Map<string, Symbol>,
    sections: Map<string, string>,
    csects: { name: string, address: number, length: number }[],
    codeSections: CodeSection[],
    maxStmtNum: number,
    sectionMap: (boolean | undefined)[],

    options?: Section,
    externals?: Section,
    relocations?: Section,
    ordinary?: Section,
    macro?: Section,
    dsects?: Section,
    usings?: Section,
    registers?: Section,
    summary?: Section,
};

function getCodeRange(l: Listing, line: number) {
    const start = +l.hasPrefix + (l.type === 'long' ? 49 : 40);
    return new vscode.Range(line, start, line, start + 72);
}

function updateSymbols(symbols: Map<string, Symbol>, symbol: Symbol) {
    const name = symbol.name.toUpperCase();
    const s = symbols.get(name);
    if (!s)
        symbols.set(name, symbol);
    else {
        s.defined = [...new Set([...s.defined, ...symbol.defined])];
        s.references = [...new Set([...s.references, ...symbol.references])];
        s.referencesPure = [...new Set([...s.referencesPure, ...symbol.referencesPure])];
    }
}

type SymbolDetails = {
    value: number,
    sectionId: number,
    reloc: boolean,
    undefined: boolean,
    loctr: boolean,
}

type Symbol = {
    name: string,
    defined: number[],
    references: number[],
    referencesPure: number[],
    details: SymbolDetails | undefined,
}

function computeReferences(symbol: Symbol, includeDefinition: boolean) {
    return includeDefinition ? [...new Set([...symbol.references, ...symbol.defined])] : symbol.references;
}

function isCsectLike(s: string) {
    return s == "SD" || s == "ED" || s == "PC" || s == "CM";
}

function processListing(doc: vscode.TextDocument, start: number, hasPrefix: boolean): { nexti: number, result: Listing } {
    const r: RegexSet = hasPrefix ? withPrefix : withoutPrefix;
    const result: Listing = {
        start,
        end: start,
        hasPrefix,
        diagnostics: [],
        statementLines: new Map<number, ListingLine>(),
        symbols: new Map<string, Symbol>(),
        sections: new Map<string, string>(),
        csects: [],
        codeSections: [],
        maxStmtNum: 0,
        sectionMap: [],
    };
    type ListingSections = Exclude<{ [key in keyof Listing]: Listing[key] extends (Section | undefined) ? key : never }[keyof Listing], undefined>;
    const updateCommonSection = <Name extends ListingSections>(name: Name, lineno: number): Listing[Name] => {
        if (!result[name]) {
            result[name] = { start: lineno, end: lineno };
        }
        return result[name];
    };
    const enum States {
        Options,
        ExternalRefs,
        Code,
        OrdinaryRefs,
        DsectRefs,
        UsingRef,
        Refs,
    };

    let symbol: Symbol | undefined;
    let lastListingSection: { end: number } | CodeSection | undefined = undefined;
    let lastTitle = '';
    let lastTitleLine = start;
    let lastCsect: string | undefined = undefined;
    let lastDsect: string | undefined = undefined;

    const symbolCandidates: { name: string, stmtNumber: number }[] = [];
    const knownCsectStmtNumbers: number[] = [];
    const addrTransionsStmtNumbers: number[] = [];

    let state = States.Options;
    let i = start;
    main: for (; i < doc.lineCount; ++i) {
        const line = doc.lineAt(i);
        const b = identifyBoundary(line.text, r);
        if (b) {
            if (lastListingSection) {
                lastListingSection.end = i;
                lastListingSection = undefined;
            }
            switch (b.type) {
                case BoudnaryType.ReturnStatement:
                    ++i
                    if (result.summary)
                        result.summary.end = i;
                    break main;
                case BoudnaryType.Diagnostic: {
                    const code = b.capture.substring(0, 8);
                    const text = b.capture.substring(9).trim();
                    const d = new vscode.Diagnostic(line.range, text, asLevel(code));
                    d.code = code;
                    result.diagnostics.push(d);
                    break;
                }
                case BoudnaryType.OptionsRef:
                    lastListingSection = updateCommonSection('options', i);
                    break;
                case BoudnaryType.ObjectCodeHeader: {
                    if (b.capture.length < 45)
                        result.type = 'short';
                    else
                        result.type = 'long';
                    const codeSection = {
                        start: lastTitleLine,
                        end: i + 1,
                        title: lastTitle,
                        codeStart: i + 1,
                        dsect: b.capture.startsWith('D'),
                        firstStmtNo: undefined,
                    };
                    lastListingSection = codeSection;
                    result.codeSections.push(codeSection);
                    break;
                }
                case BoudnaryType.ExternalRef:
                    lastListingSection = updateCommonSection('externals', i);
                    state = States.ExternalRefs;
                    break;
                case BoudnaryType.OrdinaryRef:
                    lastListingSection = updateCommonSection('ordinary', i);
                    state = States.OrdinaryRefs;
                    ++i; // skip header
                    break;
                case BoudnaryType.MacroRef:
                    lastListingSection = updateCommonSection('macro', i);
                    state = States.Refs;
                    break;
                case BoudnaryType.DsectRef:
                    lastListingSection = updateCommonSection('dsects', i);
                    state = States.DsectRefs;
                    break;
                case BoudnaryType.RegistersRef:
                    lastListingSection = updateCommonSection('registers', i);
                    state = States.Refs;
                    break;
                case BoudnaryType.DiagnosticRef:
                    lastListingSection = updateCommonSection('summary', i);
                    state = States.Refs;
                    break;
                case BoudnaryType.RelocationDict:
                    lastListingSection = updateCommonSection('relocations', i);
                    state = States.Refs;
                    break;
                case BoudnaryType.UsingsRef:
                    lastListingSection = updateCommonSection('usings', i);
                    state = States.UsingRef;
                    break;
                case BoudnaryType.OtherBoundary:
                    if (state === States.Options || state === States.ExternalRefs)
                        state = States.Code;
                    lastTitle = b.capture.substring(9).trim();
                    lastTitleLine = i;
                    break;
            }
        }
        else if (state === States.Code) {
            const obj = (result.type === 'short' ? r.objShortCode : r.objLongCode).exec(line.text);
            if (obj) {
                const address = obj[1] !== undefined ? parseInt(obj[1], 16) : undefined;
                const stmtNumber = parseInt(obj[3]);
                const symbolCandidate = obj[4];
                if (symbolCandidate) {
                    symbolCandidates.push({ name: symbolCandidate.toUpperCase(), stmtNumber });
                }
                result.statementLines.set(stmtNumber, { listingLine: i, address });
                if (stmtNumber > result.maxStmtNum)
                    result.maxStmtNum = stmtNumber;
                if (lastListingSection && 'codeStart' in lastListingSection && lastListingSection.codeStart === i)
                    lastListingSection.firstStmtNo = stmtNumber;

                if (address !== undefined && (result.type === 'short' ? r.sectionAddrShort : r.sectionAddrLong).test(obj[2])) {
                    addrTransionsStmtNumbers.push(stmtNumber);
                }
            }
        }
        else if (state === States.OrdinaryRefs) {
            const ref = r.ordinaryRefFirstLine.exec(line.text);
            let refs = '';
            if (ref) {
                if (symbol)
                    updateSymbols(result.symbols, symbol);

                symbol = {
                    name: ref[1] ? ref[1] : ref[9],
                    defined: ref[1] ? [+ref[7]] : [],
                    references: [],
                    referencesPure: [],
                    details: ref[1] ? {
                        value: parseInt(ref[3], 16),
                        sectionId: parseInt(ref[4], 16) | 0,
                        reloc: ref[5] === ' ',
                        undefined: ref[6] === 'U', // EQU mostly
                        loctr: ref[6] === 'J',
                    } : undefined,
                };

                refs = ref[8];
            }
            else if (symbol) {
                const alt = r.ordinaryRefAltSecondLine.exec(line.text);
                if (alt) {
                    symbol.details = {
                        value: parseInt(alt[3], 16),
                        sectionId: parseInt(alt[4], 16) | 0,
                        reloc: alt[5] === ' ',
                        undefined: alt[6] === 'U', // EQU mostly
                        loctr: alt[6] === 'J',
                    };
                    symbol.defined.push(+alt[7]);
                    refs = alt[8];
                }
                else {
                    const cont = r.ordinaryRefRest.exec(line.text);
                    if (cont) {
                        refs = cont[1];
                    }
                }
            }

            if (refs && symbol) {
                for (const m of refs.matchAll(/(\d+)([BDMUX])?/g)) {
                    const line = +m[1];
                    symbol.references.push(line);
                    if (!m[2])
                        symbol.referencesPure.push(line);
                }
            }
        }
        else if (state === States.ExternalRefs) {
            const ref = r.externalRefFirstLine.exec(line.text);
            let data;
            if (ref) {
                lastCsect = ref[1];
                if (ref[2]) {
                    data = ref;
                }
            } else if (lastCsect) {
                const rest = r.externalRefSecondLine.exec(line.text);
                if (rest) {
                    data = rest;
                }
            }
            if (lastCsect && data && isCsectLike(data[2])) {
                const ID = data[3];
                const ownerName = data[6].startsWith(' ') ? lastCsect : (result.sections.get(data[6]) || lastCsect);
                result.sections.set(ID, ownerName);
                if (!data[4].startsWith(' ') && !data[5].startsWith(' ')) {
                    const address = parseInt(data[4], 16);
                    const length = parseInt(data[5], 16);
                    result.csects.push({ name: ownerName, address, length });
                }
            }
        }
        else if (state === States.DsectRefs) {
            const ref = r.dsectRefFirstLine.exec(line.text);
            let data;
            if (ref) {
                lastDsect = ref[1] ? ref[1] : ref[5];
                lastDsect = lastDsect.toUpperCase();
                if (ref[2]) {
                    data = ref;
                }
            } else if (lastDsect) {
                const rest = r.dsectRefSecondLine.exec(line.text);
                if (rest) {
                    data = rest;
                }
            }
            if (lastDsect && data && result.symbols.get(lastDsect) === undefined) {
                symbol = {
                    name: lastDsect,
                    defined: [+data[4]],
                    references: [],
                    referencesPure: [],
                    details: {
                        value: 0,
                        sectionId: parseInt(data[3], 16) | 0,
                        reloc: true,
                        undefined: false,
                        loctr: true,
                    },
                };
                result.symbols.set(lastDsect, symbol);
            }
        }
        else if (state === States.UsingRef) {
            const ref = r.usingMapLine.exec(line.text);
            if (ref) {
                const stmtNo = +ref[1];
                const sectionId = parseInt(ref[3], 16) | 0;
                if (sectionId >= 0)
                    knownCsectStmtNumbers.push(stmtNo);
            }
        }
    }
    if (symbol)
        updateSymbols(result.symbols, symbol);
    if (lastListingSection) {
        lastListingSection.end = i;
    }

    for (const c of symbolCandidates) {
        if (result.symbols.has(c.name)) continue;
        result.symbols.set(c.name, {
            name: c.name,
            defined: [c.stmtNumber],
            references: [],
            referencesPure: [],
            details: undefined,
        });
    }

    result.sectionMap = createSectionMap(result, knownCsectStmtNumbers, addrTransionsStmtNumbers);

    for (const s of result.symbols.values())
        s.defined.sort((l, r) => l - r);

    result.end = i;
    return { nexti: i, result };
}

function produceListings(doc: vscode.TextDocument): Listing[] {
    const result: Listing[] = []

    for (let i = 0; i < doc.lineCount;) {
        const line = doc.lineAt(i);
        const m = listingStart.exec(line.text);
        if (!m) {
            ++i;
            continue;
        }
        const { nexti, result: listing } = processListing(doc, i, !!m[1]);
        result.push(listing);
        i = nexti;
    }

    return result;
}

function asHover(md: vscode.MarkdownString | undefined): vscode.Hover | undefined {
    return md ? new vscode.Hover(md) : undefined;
}

function isolateSymbolSimple(document: vscode.TextDocument, position: vscode.Position, hasPrefix: boolean) {
    if (position.line >= document.lineCount)
        return undefined;
    const line = document.lineAt(position.line).text;

    let start = position.character;
    let end = position.character;

    while (start > +hasPrefix && ordchar.test(line[start - 1]))
        --start;
    while (end < line.length && ordchar.test(line[end]))
        ++end;

    return line.substring(start, end).toUpperCase();
}

function codeColumns(type: 'short' | 'long', hasPrefix: boolean) {
    if (type === 'short')
        return { left: 40 + +hasPrefix, right: 111 + +hasPrefix };
    else
        return { left: 49 + +hasPrefix, right: 120 + +hasPrefix };
}

function isolateSymbol(l: Listing, document: vscode.TextDocument, position: vscode.Position) {
    const csi = l.codeSections.findIndex(s => s.codeStart <= position.line && position.line < s.end);
    if (!l.type || csi === -1) return isolateSymbolSimple(document, position, l.hasPrefix);
    const cs = l.codeSections[csi];
    const { left, right } = codeColumns(l.type, l.hasPrefix);
    if (position.character < left || position.character >= right) return isolateSymbolSimple(document, position, l.hasPrefix);

    let start = position.character;
    let end = position.character;

    const prevLine = cs.codeStart < position.line ? position.line - 1 : csi === 0 ? -1 : l.codeSections[csi - 1].end - 1;
    const nextLine = position.line + 1 < cs.end ? position.line + 1 : csi === l.codeSections.length - 1 ? -1 : l.codeSections[csi + 1].codeStart;

    const prevText = prevLine >= 0 ? document.lineAt(prevLine).text : '';
    const thisText = document.lineAt(position.line).text;
    const nextText = nextLine >= 0 ? document.lineAt(nextLine).text : '';

    const prevContinued = prevLine >= 0 && /[^ ]/.test(prevText[right]);
    const thisContinued = /[^ ]/.test(thisText[right]);

    const thisOffset = prevContinued ? initialBlanks : 0;

    while (start > left + thisOffset && ordchar.test(thisText[start - 1]))
        --start;
    while (end < right && ordchar.test(thisText[end]))
        ++end;

    let prefix = '';
    let result = thisText.substring(start, end);
    let suffix = '';

    // Handle continuation only one line up and down
    if (prevContinued && start == left + thisOffset) {
        start = right;
        while (start > left + thisOffset && ordchar.test(prevText[start - 1]))
            --start;
        prefix = prevText.substring(start, right);
    }
    if (thisContinued && end == right) {
        end = left + initialBlanks;
        while (end < right && ordchar.test(nextText[end]))
            ++end;
        suffix = nextText.substring(left + initialBlanks, end);
    }

    return (prefix + result + suffix).toUpperCase();
}

function sectionAsSymbol(s: Section, title: string, detail: string = '') {
    const r = new vscode.Range(s.start, 0, s.end, 0);
    return new vscode.DocumentSymbol(title, detail, vscode.SymbolKind.Namespace, r, r);
}

function codeSectionAsSymbol(s: Section & { title: string }) {
    const r = new vscode.Range(s.start, 0, s.end, 0);
    return new vscode.DocumentSymbol(s.title ? s.title : '(untitled)', '', vscode.SymbolKind.Package, r, r);
}

function labelAsSymbol(s: Symbol, pos: number) {
    const r = new vscode.Range(pos, 0, pos, 0);
    return new vscode.DocumentSymbol(s.name, '', vscode.SymbolKind.Object, r, r);
}

function listVisibleSymbolsOrdered(l: Listing) {
    const visibleSymbols = [];
    for (const s of l.symbols.values()) {
        if (s.name.startsWith('=')) continue; // skip literals
        for (const stmtNo of s.defined) {
            const fileLine = l.statementLines.get(stmtNo)?.listingLine;
            if (fileLine !== undefined) {
                visibleSymbols.push({ symbol: s, line: fileLine });
                break;
            }
        }
    }
    visibleSymbols.sort((l, r) => l.line - r.line);
    return visibleSymbols;
}

function createSectionMap(l: Listing, knownCsectStmtNumbers: number[], addrTransitions: number[]) {
    const limit = l.maxStmtNum;
    const result: (boolean | undefined)[] = new Array(limit).fill(undefined);
    const dsectRef = 1;
    const csectRef = 2;
    const loctrPureRef = 4;
    const refCount = 8;
    const hints = new Array(limit).fill(0);

    for (const s of l.symbols.values()) {
        const details = s.details;
        if (!details) continue;
        if (details.loctr) {
            for (const r of s.referencesPure) {
                if (r >= limit) continue;
                hints[r] += refCount;
                hints[r] |= details.sectionId < 0 ? dsectRef : csectRef | loctrPureRef;
            }
        }
        if (details.undefined || !details.reloc) continue;
        for (const d of s.defined) {
            if (d >= limit) continue;
            result[d] = details.sectionId >= 0;
            hints[d] += refCount;
            hints[d] |= details.sectionId < 0 ? dsectRef : csectRef;
        }
    }

    for (const s of l.codeSections) {
        const stmtNo = s.firstStmtNo;
        if (stmtNo === undefined || stmtNo >= limit) continue;
        result[stmtNo] = s.dsect == false;
    }

    for (const stmtNo of knownCsectStmtNumbers) {
        if (stmtNo >= limit) continue;
        result[stmtNo] = true;
    }

    const csectTransition = refCount | csectRef;
    const dsectTransition = refCount | dsectRef;
    for (const stmtNo of addrTransitions) {
        if (stmtNo >= limit) continue;
        const ignoreLoctr = hints[stmtNo] & ~loctrPureRef;
        if (ignoreLoctr == csectTransition)
            result[stmtNo] = true;
        else if (ignoreLoctr == dsectTransition)
            result[stmtNo] = false;
    }

    const lastValid = result.findIndex(e => e !== undefined);
    if (lastValid >= 0) {
        for (let i = lastValid + 1; i < result.length; ++i) {
            if (result[i] !== undefined) continue;
            result[i] = result[i - 1] || (hints[i] & loctrPureRef) != 0;
        }
    }

    return result;
}

function provideObjectCodeDetails(l: Listing, code: vscode.DocumentSymbol) {
    code.children = l.codeSections.reduce<typeof l.codeSections>((acc, c) => {
        const last = acc[acc.length - 1];
        if (last?.title === c.title) {
            last.start = Math.min(last.start, c.start);
            last.end = Math.max(last.end, c.end);
        }
        else {
            acc.push(c);
        }
        return acc;
    }, []).map(x => codeSectionAsSymbol(x));

    const visibleSymbols = listVisibleSymbolsOrdered(l);

    let titleId = -1;
    let parent = code.children;
    let titleIdLimit = code.children.length === 0 ? Number.MAX_SAFE_INTEGER : code.children[0].range.start.line;

    for (const { symbol, line } of visibleSymbols) {

        while (line >= titleIdLimit) {
            ++titleId
            parent = code.children[titleId].children;
            if (titleId >= code.children.length - 1) {
                titleIdLimit = Number.MAX_SAFE_INTEGER;
                break;
            }
            titleIdLimit = code.children[titleId + 1].range.start.line;
        }
        parent.push(labelAsSymbol(symbol, line));
    }
}

function makeListingRoot(l: Listing, id: number | undefined) {
    return new vscode.DocumentSymbol(
        id ? `Listing ${id}` : 'Listing',
        '',
        vscode.SymbolKind.Module,
        new vscode.Range(l.start, 0, l.end, 0),
        new vscode.Range(l.start, 0, l.end, 0)
    );
}

function listingAsSymbol(l: Listing, id: number | undefined) {
    const result = makeListingRoot(l, id);

    if (l.options) {
        result.children.push(sectionAsSymbol(l.options, 'High Level Assembler Option Summary'));
    }
    if (l.externals) {
        result.children.push(sectionAsSymbol(l.externals, 'External Symbol Dictionary'));
    }
    if (l.codeSections.length > 0) {
        const code = sectionAsSymbol({ start: l.codeSections[0].start, end: l.codeSections[l.codeSections.length - 1].end }, 'Object Code');
        result.children.push(code);
        provideObjectCodeDetails(l, code);
    }
    if (l.relocations) {
        result.children.push(sectionAsSymbol(l.relocations, 'Relocation Dictionary'));
    }
    if (l.ordinary) {
        result.children.push(sectionAsSymbol(l.ordinary, 'Ordinary Symbol and Literal Cross Reference'));
    }
    if (l.macro) {
        result.children.push(sectionAsSymbol(l.macro, 'Macro and Copy Code Source Summary'));
    }
    if (l.dsects) {
        result.children.push(sectionAsSymbol(l.dsects, 'Dsect Cross Reference'));
    }
    if (l.usings) {
        result.children.push(sectionAsSymbol(l.usings, 'Using Map'));
    }
    if (l.registers) {
        result.children.push(sectionAsSymbol(l.registers, 'General Purpose Register Cross Reference'));
    }
    if (l.summary) {
        result.children.push(sectionAsSymbol(l.summary, 'Diagnostic Cross Reference and Assembler Summary'));
    }

    return result;
}

function listingAsOffsets(l: Listing, id: number | undefined) {
    const result = makeListingRoot(l, id);
    const offsets = result.children;

    for (const [stmtNo, ll] of l.statementLines.entries()) {
        const address = ll.address;
        if (!l.sectionMap[stmtNo] || address === undefined) continue;

        const csect = l.csects.find(x => x.address <= address && address < x.address + x.length);

        let suffix = ''
        if (csect)
            suffix = ` (${csect.name}+${(address - csect.address).toString(16).toUpperCase().padStart(8, '0')})`;

        const rng = new vscode.Range(ll.listingLine, 0, ll.listingLine, 0);
        offsets.push(new vscode.DocumentSymbol(
            `Offset ${address.toString(16).toUpperCase().padStart(8, '0')}${suffix}`,
            '',
            vscode.SymbolKind.Object,
            rng,
            rng,
        ));
    }

    return result;
}

function findBestFitLine(m: Map<number, ListingLine>, s: number): number | undefined {
    let result;

    for (const k of m.keys()) {
        if (k > s) continue;
        result = result ? Math.max(k, result) : k;
    }

    if (result)
        return m.get(result)?.listingLine;
    else
        return undefined;
}

function compareNumbers(l: number, r: number) {
    return l - r;
}

export function createListingServices(diagCollection?: vscode.DiagnosticCollection) {
    const listings = new Map<string, Listing[]>();

    function handleListingContent(doc: vscode.TextDocument) {
        if (doc.languageId !== languageIdHlasmListing) return;
        const initVersion = doc.version;
        const data = produceListings(doc);
        if (initVersion !== doc.version || doc.languageId !== languageIdHlasmListing) return;
        listings.set(doc.uri.toString(), data);
        diagCollection?.set(doc.uri, data.flatMap(x => x.diagnostics));

        return data;
    }

    function releaseListingContent(doc: vscode.TextDocument) {
        const uri = doc.uri.toString();
        listings.delete(uri);
        diagCollection?.delete(doc.uri);
    }

    function symbolFunction<R, Args extends any[]>(f: (symbol: Symbol, l: Listing, document: vscode.TextDocument, ...args: Args) => R) {
        return (document: vscode.TextDocument, position: vscode.Position, ...args: Args): R | undefined => {
            const l = (listings.get(document.uri.toString()) ?? handleListingContent(document) ?? []).find(x => x.start <= position.line && position.line < x.end);
            if (!l) return undefined;
            const symName = isolateSymbol(l, document, position);
            if (!symName) return undefined;
            const symbol = l.symbols.get(symName);
            if (!symbol) return undefined;
            return f(symbol, l, document, ...args);
        }
    }

    return {
        handleListingContent,
        releaseListingContent,
        provideDefinition: symbolFunction((symbol, l, document) => symbol.defined
            .map(x => l.statementLines.get(x)?.listingLine || findBestFitLine(l.statementLines, x))
            .filter((x): x is number => typeof x === 'number')
            .sort(compareNumbers)
            .map(x => new vscode.Location(document.uri, getCodeRange(l, x))))
        ,
        provideReferences: symbolFunction((symbol, l, document, context: vscode.ReferenceContext) => computeReferences(symbol, context.includeDeclaration)
            .map(x => l.statementLines.get(x)?.listingLine)
            .filter((x): x is number => typeof x === 'number')
            .sort(compareNumbers)
            .map(x => new vscode.Location(document.uri, getCodeRange(l, x)))
        ),
        provideHover: symbolFunction((symbol, l, document) => asHover(symbol.defined
            .map(x => l.statementLines.get(x)?.listingLine)
            .filter((x): x is number => typeof x === 'number')
            .sort(compareNumbers)
            .map(x => document.lineAt(x).text)
            .reduce((acc, cur) => { return acc.appendCodeblock(cur, languageIdHlasmListing); }, new vscode.MarkdownString()))
        ),
        provideDocumentSymbols: (document: vscode.TextDocument) =>
            (listings.get(document.uri.toString()) ?? handleListingContent(document))?.map((l, id, ar) => listingAsSymbol(l, ar.length > 1 ? id + 1 : undefined))
        ,
        provideOffsets: (document: vscode.TextDocument) =>
            (listings.get(document.uri.toString()) ?? handleListingContent(document))?.map((l, id, ar) => listingAsOffsets(l, ar.length > 1 ? id + 1 : undefined))
        ,
    };
}

export function registerListingServices(context: vscode.ExtensionContext) {
    const diagCollection = vscode.languages.createDiagnosticCollection(EXTENSION_ID + '.listings');
    context.subscriptions.push(diagCollection);

    const services = createListingServices(diagCollection);

    vscode.workspace.onDidOpenTextDocument(services.handleListingContent, undefined, context.subscriptions);
    vscode.workspace.onDidChangeTextDocument(({ document: doc }) => services.handleListingContent(doc), undefined, context.subscriptions);
    vscode.workspace.onDidCloseTextDocument(services.releaseListingContent, undefined, context.subscriptions);

    Promise.allSettled(vscode.workspace.textDocuments.filter(x => x.languageId === languageIdHlasmListing).map(x => services.handleListingContent(x))).catch(() => { });

    context.subscriptions.push(vscode.languages.registerDefinitionProvider(languageIdHlasmListing, services));
    context.subscriptions.push(vscode.languages.registerReferenceProvider(languageIdHlasmListing, services));
    context.subscriptions.push(vscode.languages.registerHoverProvider(languageIdHlasmListing, services));
    // VSCode ordering - last one at the top
    context.subscriptions.push(vscode.languages.registerDocumentSymbolProvider(
        languageIdHlasmListing,
        { provideDocumentSymbols: (document) => services.provideOffsets(document) },
        { label: "Offsets" })
    );
    context.subscriptions.push(vscode.languages.registerDocumentSymbolProvider(languageIdHlasmListing, services));
}
