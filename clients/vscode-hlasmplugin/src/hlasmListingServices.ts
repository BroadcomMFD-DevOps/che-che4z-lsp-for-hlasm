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
const lineText = /^(?:.?)(?:(Return Code )|\*\* (ASMA\d\d\d[NIWES] .+)|[0-9A-F]{6} .{26}( *\d+)|[0-9A-F]{8} .{32}( *\d+)|(.{111})Page +\d+)/;
const pageBoundary = /^.+(?:(High Level Assembler Option Summary)|(Ordinary Symbol and Literal Cross Reference)|(Macro and Copy Code Source Summary)|(Dsect Cross Reference)|(General Purpose Register cross reference)|(Diagnostic Cross Reference and Assembler Summary))/;


const enum BoudnaryType {
    ReturnStatement,
    Diagnostic,
    ShortStmt,
    LongStmt,
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

    for (let i = 1; i < 5; ++i) {
        if (l[i])
            return { type: <BoudnaryType>(i - 1), capture: l[i] };
    }

    const m = pageBoundary.exec(l[5]);
    if (!m) return undefined;

    for (let i = 1; i < 6; ++i) {
        if (m[i])
            return { type: <BoudnaryType>(4 + i - 1), capture: m[1] };
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
};

function processListing(doc: vscode.TextDocument, start: number): { nexti: number, result: Listing } {
    const result: Listing = {
        start,
        end: start,
        diagnostics: [],
        statementLines: new Map<number, number>(),
    }
    const enum States {
        Options,
        Code,
        Refs,
    };
    let state = States.Options;
    let i = start;
    main: for (; i < doc.lineCount; ++i) {
        const line = doc.lineAt(i);
        const l = testLine(line.text);
        if (!l) continue;
        switch (l.type) {
            case BoudnaryType.ReturnStatement:
                ++i
                break main;
            case BoudnaryType.Diagnostic:
                const code = l.capture.substring(0, 8);
                const text = l.capture.substring(9).trim();
                const d = new vscode.Diagnostic(line.range, text, asLevel(code));
                d.code = code;
                result.diagnostics.push(d);
                break;
            case BoudnaryType.ShortStmt:
            case BoudnaryType.LongStmt:
                if (state === States.Code)
                    result.statementLines.set(parseInt(l.capture), i);
                break;
            case BoudnaryType.OptionsRef:
                break;
            case BoudnaryType.OrdinaryRef:
                state = States.Refs;
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

export function registerListingServices(context: vscode.ExtensionContext) {
    const listings = new Map<string, Listing[]>();
    const diags = vscode.languages.createDiagnosticCollection(EXTENSION_ID + '.listings');
    context.subscriptions.push(diags);

    async function handleListingContent(doc: vscode.TextDocument) {
        if (doc.languageId !== languageIdhlasmListing) return;

        const initVersion = doc.version;

        const data = produceListings(doc);

        if (initVersion !== doc.version || doc.languageId !== languageIdhlasmListing) return;

        listings.set(doc.uri.toString(), data);
        diags.set(doc.uri, data.flatMap(x => x.diagnostics));
    }

    vscode.workspace.onDidOpenTextDocument(handleListingContent, undefined, context.subscriptions);
    vscode.workspace.onDidChangeTextDocument(({ document: doc }) => handleListingContent(doc), undefined, context.subscriptions);
    vscode.workspace.onDidCloseTextDocument(doc => {
        const uri = doc.uri.toString();
        listings.delete(uri);
        diags.delete(doc.uri);
    }, undefined, context.subscriptions);

    Promise.allSettled(vscode.workspace.textDocuments.filter(x => x.languageId === languageIdhlasmListing).map(x => handleListingContent(x))).catch(() => { });

}