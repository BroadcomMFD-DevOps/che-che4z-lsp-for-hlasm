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

suite('Pgm And Proc Grps Patterns Test Suite', () => {
    suiteTeardown(async function () {
        await helper.closeAllEditors();
    });

    async function openDocumentAndCheckDiags(workspace_file: string) {
        helper.showDocument(workspace_file);
        await helper.sleep(2000);

        const allDiags = vscode.languages.getDiagnostics();
        const patternDiags = allDiags.find(pair => pair[0].path.endsWith(workspace_file))

        if (patternDiags)
            assert.ok(patternDiags[1].length == 0, "Library patterns are not working");
    }

    // verify that general library patterns are working
    test('General', async () => {
        await openDocumentAndCheckDiags("pattern_test/test_pattern.hlasm");
    }).timeout(10000).slow(2500);

    test('1 Byte UTF-8 Encoding', async () => {
        await openDocumentAndCheckDiags("pattern_test/test_utf_8_+.hlasm");
    }).timeout(10000).slow(2500);

    test('2 Byte UTF-8 Encoding', async () => {
        await openDocumentAndCheckDiags("pattern_test/test_utf_8_߿.hlasm");
    }).timeout(10000).slow(2500);

    test('3 Byte UTF-8 Encoding', async () => {
        await openDocumentAndCheckDiags("pattern_test/test_utf_8_￿.hlasm");
    }).timeout(10000).slow(2500);

    test('4 Byte UTF-8 Encoding', async () => {
        await openDocumentAndCheckDiags("pattern_test/test_utf_8_🧿.hlasm");
    }).timeout(10000).slow(2500);

    test('Wildcards and UTF-8 Encoding (Part #1)', async () => {
        await openDocumentAndCheckDiags("pattern_test/$test�_utf🧽_8_߽.hlasm");
    }).timeout(10000).slow(2500);

    test('Wildcards and UTF-8 Encoding (Part #2)', async () => {
        await openDocumentAndCheckDiags("pattern_test/test߼_🧼utf@_8_￼.hlasm");
    }).timeout(10000).slow(2500);
});
