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
import { isCancellationError } from '../../helpers';
import { hlasmplugin_folder, pgm_conf_file, bridge_json_file } from '../../constants'
import * as path from 'path'

async function queryCodeActions(uri: vscode.Uri, range: vscode.Range, sleep: number, attempts: number = 10) {
    for (let i = 0; i < attempts; ++i) {
        // it seems that vscode occasionally makes its own request and cancels ours
        await helper.sleep(sleep);

        try {
            const codeActionsList: vscode.CodeAction[] = await vscode.commands.executeCommand('vscode.executeCodeActionProvider', uri, range);

            // empty list also points towards the request being cancelled
            if (codeActionsList.length === 0)
                continue;

            if (i > 0)
                console.log(`Code actions required ${i + 1} attempts`);

            return codeActionsList;
        } catch (e) {
            assert.ok(isCancellationError(e));
        }
    }
    throw Error("Code actions query failed");
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

    test('Missing processor groups - pgm_conf.json', async () => {
        const file = 'missing_pgroup/A.hlasm';
        const pgmConfRelPath = path.join(hlasmplugin_folder, pgm_conf_file);
        let diagnostic_event = helper.waitForDiagnostics(pgmConfRelPath);
        const pgmConf = await helper.showDocument(pgmConfRelPath);

        helper.assertMatchingMessageCodes(await diagnostic_event, [2]); // 2 represents usage deprecated option in pgm_conf.json

        diagnostic_event = helper.waitForDiagnostics(pgmConfRelPath);
        await helper.showDocument(file, 'hlasm');

        helper.assertMatchingMessageCodes(await diagnostic_event, [2, 'W0004']);

        let codeActionsList = await queryCodeActions(pgmConf.document.uri, new vscode.Range(0, 0, 0, 0), 500);

        assert.equal(codeActionsList.length, 1);
        assert.ok(codeActionsList[0].command);
        assert.strictEqual(codeActionsList[0].command.command, 'extension.hlasm-plugin.showConfigurationDiagnostics');
        assert.strictEqual(codeActionsList[0].command.title, 'Show all configuration diagnostics');

        diagnostic_event = helper.waitForDiagnostics(pgmConfRelPath);
        vscode.commands.executeCommand<void>(codeActionsList[0].command.command, pgmConf.document.uri, new vscode.Range(0, 0, 0, 0));

        helper.assertMatchingMessageCodes(await diagnostic_event, [2, 'W0004', 'CFG001']);

        codeActionsList = await queryCodeActions(pgmConf.document.uri, new vscode.Range(0, 0, 0, 0), 500);

        assert.equal(codeActionsList.length, 1);
        assert.ok(codeActionsList[0].command);
        assert.strictEqual(codeActionsList[0].command.command, 'extension.hlasm-plugin.showConfigurationDiagnostics');
        assert.strictEqual(codeActionsList[0].command.title, 'Show only critical configuration diagnostics');

        diagnostic_event = helper.waitForDiagnostics(pgmConfRelPath);
        vscode.commands.executeCommand<void>(codeActionsList[0].command.command, pgmConf.document.uri, new vscode.Range(0, 0, 0, 0));

        helper.assertMatchingMessageCodes(await diagnostic_event, [2, 'W0004']);

        await helper.closeAllEditors();
    }).timeout(15000).slow(10000);

    test('Missing processor groups - .bridge.json', async () => {
        const file = path.join("missing_pgroup", "b4g", "A");
        const b4gPath = path.join("missing_pgroup", "b4g", bridge_json_file);
        let diagnostic_event = helper.waitForDiagnostics(b4gPath);
        const bridgeJson = await helper.showDocument(b4gPath);
        await helper.showDocument(file, 'hlasm');

        helper.assertMatchingMessageCodes(await diagnostic_event, ['B4G002']);

        let codeActionsList = await queryCodeActions(bridgeJson.document.uri, new vscode.Range(0, 0, 0, 0), 500);

        assert.equal(codeActionsList.length, 1);
        assert.ok(codeActionsList[0].command);
        assert.strictEqual(codeActionsList[0].command.command, 'extension.hlasm-plugin.showConfigurationDiagnostics');
        assert.strictEqual(codeActionsList[0].command.title, 'Show all configuration diagnostics');

        diagnostic_event = helper.waitForDiagnostics(b4gPath);
        vscode.commands.executeCommand<void>(codeActionsList[0].command.command, bridgeJson.document.uri, new vscode.Range(0, 0, 0, 0));

        helper.assertMatchingMessageCodes(await diagnostic_event, ['B4G002', 'CFG001']);

        codeActionsList = await queryCodeActions(bridgeJson.document.uri, new vscode.Range(0, 0, 0, 0), 500);

        assert.equal(codeActionsList.length, 1);
        assert.ok(codeActionsList[0].command);
        assert.strictEqual(codeActionsList[0].command.command, 'extension.hlasm-plugin.showConfigurationDiagnostics');
        assert.strictEqual(codeActionsList[0].command.title, 'Show only critical configuration diagnostics');

        diagnostic_event = helper.waitForDiagnostics(b4gPath);
        vscode.commands.executeCommand<void>(codeActionsList[0].command.command, bridgeJson.document.uri, new vscode.Range(0, 0, 0, 0));

        helper.assertMatchingMessageCodes(await diagnostic_event, ['B4G002']);

        await helper.closeAllEditors();
    }).timeout(15000).slow(10000);
});
