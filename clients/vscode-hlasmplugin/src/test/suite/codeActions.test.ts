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
import * as helper from './testHelper';

async function queryCodeActions(uri: vscode.Uri, range: vscode.Range, sleep: number, max_attempts: number = 10) {
    for (; max_attempts > 0; --max_attempts) {
        // it seems that vscode occasionally makes its own request and cancels ours
        await helper.sleep(sleep);

        try {
            const codeActionsList: vscode.CodeAction[] = await vscode.commands.executeCommand('vscode.executeCodeActionProvider', uri, range);

            // empty list also points toward the request being cancelled
            if (codeActionsList.length !== 0)
                return codeActionsList;
        } catch (e) {
            assert.equal(e.message, 'Cancelled');
        }
    }
    return [];
}

suite('Code actions', () => {
    suiteSetup(async function () {
        this.timeout(20000);
    });

    suiteTeardown(async function () {
        await helper.closeAllEditors();
    });

    test('Diagnostics not suppressed', async () => {
        const file = 'code_action_1.hlasm';

        const diagnostic_event = helper.waitForDiagnostics(file);

        const { editor, document } = await helper.showDocument(file, 'hlasm');

        await diagnostic_event;

        const codeActionsList = await queryCodeActions(document.uri, new vscode.Range(0, 10, 0, 15), 500);

        assert.equal(codeActionsList.length, 4 + 3);

        await helper.closeAllEditors();
    }).timeout(10000).slow(5000);

    test('Diagnostics suppressed', async () => {
        const file = 'code_action_2.hlasm';
        const diagnostic_event = helper.waitForDiagnostics(file);

        const { editor, document } = await helper.showDocument(file, 'hlasm');

        await diagnostic_event;

        const codeActionsList = await queryCodeActions(document.uri, new vscode.Range(0, 10, 0, 15), 500);

        assert.equal(codeActionsList.length, 1);

        await helper.closeAllEditors();
    }).timeout(10000).slow(5000);
});
