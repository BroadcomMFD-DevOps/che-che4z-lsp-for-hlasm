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

import * as assert from 'assert';
import * as vscode from 'vscode';
import { CommentOption, lineCommentCommand, translateCommentOption } from '../../commentEditorCommands';
import { TextDocumentMock, TextEditorEditMock, TextEditorMock } from '../mocks';

suite('Comment editor commands', () => {
    test('Toggle comment', () => {
        // prepare document
        const document = new TextDocumentMock();
        document.uri = vscode.Uri.file('file');
        // prepare editor and edit
        const editor = new TextEditorMock(document);
        const cursorPosition = new vscode.Position(0, 4);
        editor.selection = new vscode.Selection(cursorPosition, cursorPosition);
        const edit = new TextEditorEditMock('  this');
        document.text = edit.text;

        lineCommentCommand(editor, edit, CommentOption.toggle);
        document.text = edit.text;
        assert.ok(document.text === '* this');

        lineCommentCommand(editor, edit, CommentOption.toggle);
        document.text = edit.text;
        assert.ok(document.text === '  this');
    })

    test('Add comment', () => {
        // prepare document
        const document = new TextDocumentMock();
        document.uri = vscode.Uri.file('file');
        // prepare editor and edit
        const editor = new TextEditorMock(document);
        const cursorPosition = new vscode.Position(0, 4);
        editor.selection = new vscode.Selection(cursorPosition, cursorPosition);
        const edit = new TextEditorEditMock('  this');
        document.text = edit.text;

        lineCommentCommand(editor, edit, CommentOption.add);
        document.text = edit.text;
        assert.ok(document.text === '* this');

        lineCommentCommand(editor, edit, CommentOption.add);
        document.text = edit.text;
        assert.ok(document.text === '* this');
    })

    test('Remove comment', () => {
        // prepare document
        const document = new TextDocumentMock();
        document.uri = vscode.Uri.file('file');
        // prepare editor and edit
        const editor = new TextEditorMock(document);
        const cursorPosition = new vscode.Position(0, 4);
        editor.selection = new vscode.Selection(cursorPosition, cursorPosition);
        const edit = new TextEditorEditMock('* this');
        document.text = edit.text;

        lineCommentCommand(editor, edit, CommentOption.remove);
        document.text = edit.text;
        assert.ok(document.text === '  this');

        lineCommentCommand(editor, edit, CommentOption.remove);
        document.text = edit.text;
        assert.ok(document.text === '  this');
    })

    test('Toggle multiline comment', () => {
        // prepare document
        const document = new TextDocumentMock();
        document.uri = vscode.Uri.file('file');
        // prepare editor and edit
        const editor = new TextEditorMock(document);
        const cursorPosition = new vscode.Position(1, 4);
        editor.selection = new vscode.Selection(cursorPosition, cursorPosition);
        const edit = new TextEditorEditMock('AA this                                                                X\n               that');
        document.text = edit.text;

        lineCommentCommand(editor, edit, CommentOption.toggle);
        document.text = edit.text;
        assert.ok(document.text === '*A this                                                                X\n               that');

        lineCommentCommand(editor, edit, CommentOption.toggle);
        document.text = edit.text;
        assert.ok(document.text === 'AA this                                                                X\n               that');
    })

    test('Deserialize comment option', () => {
        assert.ok(translateCommentOption('CommentOption.toggle') === CommentOption.toggle);
        assert.ok(translateCommentOption('CommentOption.add') === CommentOption.add);
        assert.ok(translateCommentOption('CommentOption.remove') === CommentOption.remove);
        assert.ok(translateCommentOption('something') === null);
    })
    
    test('Toggle macro style comment', () => {
        // prepare document
        const document = new TextDocumentMock();
        document.uri = vscode.Uri.file('file');
        // prepare editor and edit
        const editor = new TextEditorMock(document);
        const cursorPosition = new vscode.Position(0, 4);
        editor.selection = new vscode.Selection(cursorPosition, cursorPosition);
        const edit = new TextEditorEditMock('.* this');
        document.text = edit.text;

        lineCommentCommand(editor, edit, CommentOption.toggle);
        document.text = edit.text;
        assert.ok(document.text === '   this');

        lineCommentCommand(editor, edit, CommentOption.toggle);
        document.text = edit.text;
        assert.ok(document.text === '*  this');
    })
})
