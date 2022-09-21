/*
 * Copyright (c) 2022 Broadcom.
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

export enum CommentOption {
    toggle,
    add,
    remove,
}
export function translateCommentOption(text: string): CommentOption | null {
    if (text === 'CommentOption.toggle')
        return CommentOption.toggle;
    if (text === 'CommentOption.add')
        return CommentOption.add;
    if (text === 'CommentOption.remove')
        return CommentOption.remove;

    return null;
}

function seq(low: number, high: number): number[] {
    return Array.from({ length: high + 1 - low }, (_, i) => i + low);
}

function isContinued(text: string): boolean {
    return text.length > 71 && text.substring(71, 72) !== ' ';
}

function findFirstLine(doc: vscode.TextDocument, lineno: number): number {
    while (lineno > 0 && isContinued(doc.lineAt(lineno - 1).text))
        --lineno;

    return lineno;
}

function isCommented(doc: vscode.TextDocument, lineno: number): boolean {
    const text = doc.lineAt(lineno).text;
    return text.startsWith('*') || text.startsWith('.*');
}

function addComment(editor: vscode.TextEditor, edit: vscode.TextEditorEdit, lineno: number) {
    if (editor.document.lineAt(lineno).text.length > 0)
        edit.replace(new vscode.Selection(new vscode.Position(lineno, 0), new vscode.Position(lineno, 1)), '*');
    else
        edit.insert(new vscode.Position(lineno, 0), '*');
}

function removeComment(editor: vscode.TextEditor, edit: vscode.TextEditorEdit, lineno: number) {
    const text = editor.document.lineAt(lineno).text;
    let replacement = ' ';
    let i = 1;
    for (; i < text.length; ++i) {
        if (text.charAt(i) !== '*') {
            replacement = text.charAt(i);
            break;
        }
    }
    replacement = replacement.repeat(i);

    edit.replace(new vscode.Selection(new vscode.Position(lineno, 0), new vscode.Position(lineno, i)), replacement);
}

export function lineCommentCommand(editor: vscode.TextEditor, edit: vscode.TextEditorEdit, args: CommentOption) {
    if (editor.document.isClosed) return;

    const lines = [...new Set(editor.selections.flatMap(x => seq(x.start.line, x.end.line)).map(x => findFirstLine(editor.document, x)))].sort((l, r) => l - r);
    const lineWithStatus = lines.map(x => { return { lineno: x, commented: isCommented(editor.document, x) }; })

    for (const { lineno, commented } of lineWithStatus) {
        switch (args) {
            case CommentOption.toggle:
                if (commented)
                    removeComment(editor, edit, lineno);
                else
                    addComment(editor, edit, lineno);
                break;

            case CommentOption.add:
                if (!commented)
                    addComment(editor, edit, lineno);
                break;

            case CommentOption.remove:
                if (commented)
                    removeComment(editor, edit, lineno);
                break;
        }
    }
}