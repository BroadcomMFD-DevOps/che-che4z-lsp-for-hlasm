/*
 * Copyright (c) 2019 Broadcom.
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

import { setCursor, isLineContinued } from './customEditorCommands'
import { getConfig } from './eventsHandler'

/**
 * Implements removeContinuation and insertContinuation commands.
 */
export class ContinuationHandler {
    dispose() { }

    private extractContinuationSymbolSingleLine(document: vscode.TextDocument, line: number, continuationOffset: number): string {
        if (line >= 0 && line < document.lineCount) {
            const lineText = document.lineAt(line).text;
            if (continuationOffset < lineText.length) {
                const character = lineText.at(continuationOffset);
                if (character != " ")
                    return character;
            }
        }
        return "";
    }

    private extractMostCommonContinuationSymbol(document: vscode.TextDocument, continuationOffset: number): string {
        let continuationSymbols: { [name: string]: number } = {};
        for (let i = 0; i < Math.min(document.lineCount, 5000); ++i) {
            const line = document.lineAt(i).text;
            if (continuationOffset >= line.length)
                continue;
            const candidate = line.at(continuationOffset);
            if (candidate == ' ')
                continue;
            continuationSymbols[candidate] = (continuationSymbols[candidate] || 0) + 1;
        }
        return Object.entries(continuationSymbols).reduce((prev, current) => prev[1] >= current[1] ? prev : current, ["X", 0])[0];
    }

    private extractContinuationSymbol(document: vscode.TextDocument, line: number, continuationOffset: number): string {
        return this.extractContinuationSymbolSingleLine(document, line, continuationOffset) ||
            this.extractContinuationSymbolSingleLine(document, line - 1, continuationOffset) ||
            this.extractContinuationSymbolSingleLine(document, line + 1, continuationOffset) ||
            this.extractMostCommonContinuationSymbol(document, continuationOffset);
    }

    // insert continuation character X to the current line
    insertContinuation(editor: vscode.TextEditor, edit: vscode.TextEditorEdit, continuationOffset: number, continueColumn: number) {
        const sel = editor.selection;

        // retrieve continuation information
        const line = sel.active.line;
        const col = sel.active.character;
        const isContinued = isLineContinued(editor.document, line, continuationOffset);
        const doc = editor.document;
        const lineText = doc.lineAt(line).text;
        const eol = doc.eol == vscode.EndOfLine.LF ? '\n' : '\r\n';

        let contSymbol = this.extractContinuationSymbol(doc, line, continuationOffset);
        if (!isContinued) {
            let initialLineSize = editor.document.lineAt(line).text.length;
            if (initialLineSize <= continuationOffset) {
                edit.insert(new vscode.Position(line, col),
                    ' '.repeat(continuationOffset - col) + contSymbol + eol +
                    ' '.repeat(continueColumn));
            }
            else {
                let reinsert = '';
                let ignoredPart = '';
                if (col < continuationOffset)
                    reinsert = lineText.substring(col, continuationOffset).trimEnd();

                ignoredPart = lineText.substring(continuationOffset + 1);
                // see https://github.com/microsoft/vscode/issues/32058 why replace does not work
                const insertionPoint = new vscode.Position(line, Math.min(col, continuationOffset));
                edit.delete(new vscode.Range(insertionPoint, new vscode.Position(line, lineText.length)));
                edit.insert(insertionPoint,
                    ' '.repeat(Math.max(continuationOffset - col, 0)) + contSymbol + ignoredPart + eol +
                    ' '.repeat(continueColumn) + reinsert);
            }
        }
        // add extra continuation on already continued line
        else {
            let prefix = '';
            let reinsert = '';
            if (col < continuationOffset) {
                reinsert = lineText.substring(col, continuationOffset).trimEnd();
                prefix = ' '.repeat(continuationOffset - col) + lineText.substring(continuationOffset, lineText.length);
            }
            else {
                prefix = lineText.substring(col, lineText.length);
            }
            const insertionPoint = new vscode.Position(line, col);
            edit.delete(new vscode.Range(insertionPoint, new vscode.Position(line, lineText.length)));
            edit.insert(insertionPoint,
                prefix + eol +
                ' '.repeat(continueColumn) + reinsert + ' '.repeat(continuationOffset - continueColumn - reinsert.length) + contSymbol);
            setImmediate(() => setCursor(editor, new vscode.Position(line + 1, continueColumn + reinsert.length)));
        }
    }

    private extractLineRanges(editor: vscode.TextEditor, continuationOffset: number): { start: number, end: number }[] {
        const selection_to_lines = (x: vscode.Selection) => {
            let result = [];
            for (let l = x.start.line; l <= x.end.line; ++l)
                result.push(l);
            return result;
        };
        const all_lines = [... new Set(editor.selections.map(selection_to_lines).flat(1))].sort();
        let last = -2;
        let result: { start: number, end: number }[] = [];
        for (let l of all_lines) {
            if (l != last + 1) {
                if (l > 0 && isLineContinued(editor.document, l - 1, continuationOffset)) {
                    result.push({ start: l, end: l });
                    last = l;
                }
            }
            else {
                result[result.length - 1].end = l;
                last = l;
            }
        }
        return result;
    }

    // remove continuation from previous line
    removeContinuation(editor: vscode.TextEditor, edit: vscode.TextEditorEdit, continuationOffset: number) {
        let new_selection: vscode.Selection[] = [];
        for (const line_range of this.extractLineRanges(editor, continuationOffset).sort((l, r) => { return l.start - r.start; })) {
            edit.delete(
                new vscode.Range(
                    new vscode.Position(line_range.start - 1, editor.document.lineAt(line_range.start - 1).text.length),
                    new vscode.Position(line_range.end, editor.document.lineAt(line_range.end).text.length)));
            if (!isLineContinued(editor.document, line_range.end, continuationOffset)) {
                const continuationPosition = new vscode.Position(line_range.start - 1, continuationOffset);
                edit.replace(
                    new vscode.Range(
                        new vscode.Position(line_range.start - 1, continuationOffset),
                        new vscode.Position(line_range.start - 1, continuationOffset + 1)
                    ),
                    ' '
                );
            }
            let new_cursor = new vscode.Position(
                line_range.start - 1,
                editor.document.lineAt(line_range.start - 1).text.substring(0, continuationOffset).trimEnd().length
            );
            new_selection.push(new vscode.Selection(new_cursor, new_cursor));
        }
        editor.selections = new_selection;
    }

    rearrangeSequenceNumbers(editor: vscode.TextEditor, edit: vscode.TextEditorEdit, continuationOffset: number) {
        const selections: { [line: number]: vscode.Selection[] } = editor.selections
            .filter(s => s.isSingleLine)
            .map((s) => ({ line: s.start.line, selection: s }))
            .reduce((r, v): { [line: number]: vscode.Selection[] } => { r[v.line] = r[v.line] || []; r[v.line].push(v.selection); return r; }, Object.create({}));
        for (const line_sel of Object.entries(selections).sort((a, b) => +b[0] - +a[0])) {
            const sel = line_sel[1];

            // retrieve continuation information
            const line = +line_sel[0];
            const doc = editor.document;
            const lineText = doc.lineAt(line).text;
            const selectionLength = sel.reduce((r, v) => r + (v.end.character - v.start.character), 0);

            if (lineText.length > continuationOffset) {
                const lastSpace = lineText.lastIndexOf(' ');
                const notSpace = lastSpace + 1;
                const deletionStart = continuationOffset + (notSpace + 1 == lineText.length || lineText.length - notSpace > 8 ? 0 : 1);
                if (notSpace - deletionStart < selectionLength) {
                    edit.insert(new vscode.Position(line, continuationOffset), ' '.repeat(selectionLength - (notSpace - deletionStart)));
                }
                else if (notSpace - deletionStart > selectionLength) {
                    // the end of line segment is either a single character, or longer than 8 => assume continuation symbol is present
                    // otherwise assume only sequence symbols are present
                    if (lineText.substring(deletionStart + selectionLength, notSpace).trim().length == 0)
                        edit.delete(
                            new vscode.Range(
                                new vscode.Position(line, deletionStart + selectionLength),
                                new vscode.Position(line, notSpace)
                            )
                        );
                }
            }
            for (let s of sel)
                if (!s.isEmpty)
                    edit.delete(s);
        }
    }
}
