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
import { EXTENSION_ID } from './extension';

const languageIdhlasmListing = 'hlasmListing';

const listingStart = /^(?:.?)                                         High Level Assembler Option Summary                   .............   Page    1/;
const lineText = /^(?:.?)(?:(Return Code )|\*\* (ASMA\d\d\d[NIWES] .+)|(.{111})Page +\d+)/;
const pageBoundary = /^.+(?:(High Level Assembler Option Summary)|(Ordinary Symbol and Literal Cross Reference)|(Macro and Copy Code Source Summary)|(Dsect Cross Reference)|(General Purpose Register cross reference)|(Diagnostic Cross Reference and Assembler Summary))/;

const enum BoudnaryType {
    ReturnStatement,
    Diagnostic,
    OptionsRef,
    OrdinaryRef,
    MacroRef,
    DsectRef,
    RegistersRef,
    DiagnosticRef,
    OtherBoundary,
};

function testLine(s: string): { type: BoudnaryType, capture: string } | undefined {
    const l = lineText.exec(s);
    if (!l) return undefined;

    for (let i = 1; i < 3; ++i) {
        if (l[i])
            return { type: <BoudnaryType>(i - 1), capture: l[i] };
    }

    const m = pageBoundary.exec(l[3]);
    if (!m) return { type: BoudnaryType.OtherBoundary, capture: '' };

    for (let i = 1; i < 6; ++i) {
        if (m[i])
            return { type: <BoudnaryType>(2 + i - 1), capture: m[1] };
    }

    return undefined;
}

function asLevel(code: string) {
    if (code.endsWith('N')) return vscode.DiagnosticSeverity.Hint;
    if (code.endsWith('I')) return vscode.DiagnosticSeverity.Information;
    if (code.endsWith('W')) return vscode.DiagnosticSeverity.Warning;
    return vscode.DiagnosticSeverity.Error;
}

type Listing = {
    start: number,
    end: number,
    diagnostics: vscode.Diagnostic[],
    statementLines: Map<number, number>,
    symbols: Map<string, Symbol>,
};

function updateSymbols(symbols: Map<string, Symbol>, symbol: Symbol) {
    const name = symbol.name.toUpperCase();
    const s = symbols.get(name);
    if (!s)
        symbols.set(name, symbol);
    else {
        s.defined = [...new Set([...s.defined, ...symbol.defined])];
        s.references = [...new Set([...s.references, ...symbol.references])];
    }
}

class Symbol {
    name: string = '';
    defined: number[] = [];
    references: number[] = [];
}

// Symbol   Length   Value     Id    R Type Asm  Program   Defn References                     
// A             4 00000000 00000001     A  A                 1   44    45    46    47    48    49    50    51    52    53 
//                                                                54    55    56    57    58    59    60    61    62    63 
//                                                                64    65    66    67    68    69    70    71    72    73 
//                                                                74    75    76    77    78    79    80    81    82    83 
//                                                                84    85    86    87    88    89    90    91    92    93 
//                                                                94                                                       
const ordinaryRefFirstLine = /^(?:.?)(?:([a-zA-Z$#@_][a-zA-Z$#@0-9_]{0,7}) +(\d+) ([0-9A-F]{8}) [0-9A-F]{8} . .... ...  ....... +(\d+) +(\d.+|)|([a-zA-Z$#@_][a-zA-Z$#@0-9_]{8,}))/;
const ordinaryRefAltSecondLine = /^(?:.?)( {9,})(\d+) ([0-9A-F]{8}) [0-9A-F]{8} . .... ...  ....... +(\d+) +(\d.+|)/;
const ordinaryRefRest = /^(?:.?[ ]{60,})(\d.+)/;

function processListing(doc: vscode.TextDocument, start: number): { nexti: number, result: Listing } {
    const result: Listing = {
        start,
        end: start,
        diagnostics: [],
        statementLines: new Map<number, number>(),
        symbols: new Map<string, Symbol>(),
    };
    const enum States {
        Options,
        Code,
        OrdinaryRefs,
        Refs,
    };

    let symbol: Symbol | undefined;

    let state = States.Options;
    let i = start;
    main: for (; i < doc.lineCount; ++i) {
        const line = doc.lineAt(i);
        const l = testLine(line.text);
        if (l) {
            switch (l.type) {
                case BoudnaryType.ReturnStatement:
                    ++i
                    break main;
                case BoudnaryType.Diagnostic:
                    const code = l!.capture.substring(0, 8);
                    const text = l!.capture.substring(9).trim();
                    const d = new vscode.Diagnostic(line.range, text, asLevel(code));
                    d.code = code;
                    result.diagnostics.push(d);
                    break;
                case BoudnaryType.OptionsRef:
                    break;
                case BoudnaryType.OrdinaryRef:
                    state = States.OrdinaryRefs;
                    ++i; // skip header
                    break;
                case BoudnaryType.MacroRef:
                    state = States.Refs;
                    break;
                case BoudnaryType.DsectRef:
                    state = States.Refs;
                    break;
                case BoudnaryType.RegistersRef:
                    state = States.Refs;
                    break;
                case BoudnaryType.DiagnosticRef:
                    state = States.Refs;
                    break;
                case BoudnaryType.OtherBoundary:
                    if (state === States.Options)
                        state = States.Code;
                    break;
            }
        }
        else if (state === States.Code) {
            const obj = /^(?:.?)(?:[0-9A-F]{6} .{26}( *\d+)|[0-9A-F]{8} .{32}( *\d+)|[ ]{18,22}[0-9A-F]{6}( *\d+)|[ ]{24}[0-9A-F]{8}( *\d+))/.exec(line.text);
            if (obj) {
                result.statementLines.set(parseInt(obj[1] || obj[2] || obj[3] || obj[4]), i);
            }
        }
        else if (state === States.OrdinaryRefs) {
            const ref = ordinaryRefFirstLine.exec(line.text);
            let refs = '';
            if (ref) {
                if (symbol)
                    updateSymbols(result.symbols, symbol);

                symbol = new Symbol();
                if (ref[1]) {
                    symbol.name = ref[1];
                    symbol.defined.push(+ref[4]);
                    refs = ref[5];
                }
                else {
                    symbol.name = ref[6];
                }
            }
            else {
                const alt = ordinaryRefAltSecondLine.exec(line.text);
                if (alt && symbol) {
                    symbol.defined.push(+alt[4]);
                    refs = alt[5];
                }
            }

            if (refs && symbol) {
                for (const m of refs.matchAll(/(\d+)[BDMUX]?/g)) {
                    symbol.references.push(+m[1]);
                }
            }
        }
    }
    if (symbol)
        updateSymbols(result.symbols, symbol);

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
        const { nexti, result: listing } = processListing(doc, i);
        result.push(listing);
        i = nexti;
    }

    return result;
}

type AsyncReturnType<T extends (...args: any) => Promise<any>> =
    T extends (...args: any) => Promise<infer R> ? R : any

const ordchar = /[A-Za-z0-9$#@_]/;

function isolateSymbol(document: vscode.TextDocument, position: vscode.Position) {

    if (position.line >= document.lineCount)
        return undefined;
    const line = document.lineAt(position.line).text;

    let start = position.character;
    let end = position.character;

    while (start > 0 && ordchar.test(line[start - 1]))
        --start;
    while (end < line.length && ordchar.test(line[end]))
        ++end;

    return line.substring(start, end).toUpperCase();
}

export function registerListingServices(context: vscode.ExtensionContext) {
    const listings = new Map<string, Listing[]>();
    const diags = vscode.languages.createDiagnosticCollection(EXTENSION_ID + '.listings');
    context.subscriptions.push(diags);

    function handleListingContent(doc: vscode.TextDocument) {
        if (doc.languageId !== languageIdhlasmListing) return;

        const initVersion = doc.version;

        const data = produceListings(doc);

        if (initVersion !== doc.version || doc.languageId !== languageIdhlasmListing) return;

        listings.set(doc.uri.toString(), data);
        diags.set(doc.uri, data.flatMap(x => x.diagnostics));

        return data;
    }

    function getListing(document: vscode.TextDocument, position: vscode.Position) {
        return (listings.get(document.uri.toString()) || handleListingContent(document) || []).find(x => x.start <= position.line && position.line < x.end);
    }

    vscode.workspace.onDidOpenTextDocument(handleListingContent, undefined, context.subscriptions);
    vscode.workspace.onDidChangeTextDocument(({ document: doc }) => handleListingContent(doc), undefined, context.subscriptions);
    vscode.workspace.onDidCloseTextDocument(doc => {
        const uri = doc.uri.toString();
        listings.delete(uri);
        diags.delete(doc.uri);
    }, undefined, context.subscriptions);

    Promise.allSettled(vscode.workspace.textDocuments.filter(x => x.languageId === languageIdhlasmListing).map(x => handleListingContent(x))).catch(() => { });

    const services = {
        provideDefinition: (document: vscode.TextDocument, position: vscode.Position, token: vscode.CancellationToken): vscode.ProviderResult<vscode.Definition | vscode.DefinitionLink[]> => {
            const symName = isolateSymbol(document, position);
            const l = getListing(document, position);
            if (!symName || !l)
                return undefined;

            return l?.symbols.get(symName)?.defined.map(x => l.statementLines.get(x)).filter((x): x is number => typeof x === 'number').map(x => new vscode.Location(document.uri, new vscode.Position(x, 0)));
        },
        provideReferences(document: vscode.TextDocument, position: vscode.Position, context: vscode.ReferenceContext, token: vscode.CancellationToken): vscode.ProviderResult<vscode.Location[]> {
            const symName = isolateSymbol(document, position);
            const l = getListing(document, position);
            if (!symName || !l)
                return undefined;

            return l?.symbols.get(symName)?.references.map(x => l.statementLines.get(x)).filter((x): x is number => typeof x === 'number').map(x => new vscode.Location(document.uri, new vscode.Position(x, 0)))
        },
    };

    context.subscriptions.push(vscode.languages.registerDefinitionProvider(languageIdhlasmListing, services));
    context.subscriptions.push(vscode.languages.registerReferenceProvider(languageIdhlasmListing, services));
}