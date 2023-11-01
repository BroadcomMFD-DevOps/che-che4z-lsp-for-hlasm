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
import * as vscodelc from 'vscode-languageclient';
import { HlasmPluginMiddleware } from './languageClientMiddleware';

type BranchInfo = {
    line: number, col: number, up: boolean, down: boolean, somewhere: boolean,
};

type DecorationTypes = {
    upArrow: vscode.TextEditorDecorationType,
    downArrow: vscode.TextEditorDecorationType,
    updownArrow: vscode.TextEditorDecorationType,
    rightArrow: vscode.TextEditorDecorationType,
}

function getFirstOpenPromise(client: vscodelc.BaseLanguageClient) {
    const middleware = client.middleware as HlasmPluginMiddleware;
    return middleware.getFirstOpen();
}

async function getBranchInfo(client: vscodelc.BaseLanguageClient, uri: vscode.Uri, cancelToken: vscode.CancellationToken): Promise<BranchInfo[]> {
    return getFirstOpenPromise(client).then(() => client.sendRequest('textDocument/$/branch_information', { 'textDocument': { 'uri': uri.toString() } }, cancelToken));
}

function updateEditor(editor: vscode.TextEditor, bi: BranchInfo[], dt: DecorationTypes) {
    editor.setDecorations(dt.upArrow, bi.filter(x => !x.somewhere && x.up && !x.down && x.col > 0).map(x => new vscode.Range(x.line, x.col - 1, x.line, x.col - 1)));
    editor.setDecorations(dt.downArrow, bi.filter(x => !x.somewhere && !x.up && x.down && x.col > 0).map(x => new vscode.Range(x.line, x.col - 1, x.line, x.col - 1)));
    editor.setDecorations(dt.updownArrow, bi.filter(x => !x.somewhere && x.up && x.down && x.col > 0).map(x => new vscode.Range(x.line, x.col - 1, x.line, x.col - 1)));
    editor.setDecorations(dt.rightArrow, bi.filter(x => x.somewhere && x.col > 0).map(x => new vscode.Range(x.line, x.col - 1, x.line, x.col - 1)));
}

function updateVisibleEditors(uri: vscode.Uri, bi: BranchInfo[], dt: DecorationTypes) {
    const uriStr = uri.toString();
    for (const editor of vscode.window.visibleTextEditors) {
        if (editor.document.uri.toString() !== uriStr)
            continue;

        updateEditor(editor, bi, dt);
    }
}

function getCancellableBranchInfo(client: vscodelc.BaseLanguageClient, uri: vscode.Uri, delay: number): [Promise<BranchInfo[]>, () => void] {
    let timeoutId: ReturnType<typeof setTimeout> | undefined = undefined;
    let cancelSource: vscode.CancellationTokenSource | undefined = undefined;
    const p = new Promise((res, rej) => { timeoutId = setTimeout(res, delay); }).then(() => {
        timeoutId = undefined;
        cancelSource = new vscode.CancellationTokenSource();
        return getBranchInfo(client, uri, cancelSource.token);
    }).finally(() => {
        cancelSource?.dispose();
    });
    const cancel = () => {
        if (timeoutId) clearTimeout(timeoutId);
        if (cancelSource) cancelSource.cancel();
    };

    return [p, cancel];
}

function createDecorationTypes(context: vscode.ExtensionContext): DecorationTypes {
    const upArrow = vscode.window.createTextEditorDecorationType({
        before: {
            contentText: ' ‏↑‎ ',
            color: new vscode.ThemeColor('hlasmplugin.branchUpColor'),
            fontWeight: 'bold',
            width: '0',
        }
    });
    const downArrow = vscode.window.createTextEditorDecorationType({
        before: {
            contentText: ' ‏↓‎ ',
            color: new vscode.ThemeColor('hlasmplugin.branchDownColor'),
            fontWeight: 'bold',
            width: '0',
        }
    });
    const updownArrow = vscode.window.createTextEditorDecorationType({
        before: {
            contentText: ' ‏↕‎ ',
            color: new vscode.ThemeColor('hlasmplugin.branchUnknownColor'),
            fontWeight: 'bold',
            width: '0',
        }
    });
    const rightArrow = vscode.window.createTextEditorDecorationType({
        before: {
            contentText: ' ‏→‎ ',
            color: new vscode.ThemeColor('hlasmplugin.branchUnknownColor'),
            fontWeight: 'bold',
            width: '0',
        },
    });
    context.subscriptions.push(upArrow);
    context.subscriptions.push(downArrow);
    context.subscriptions.push(updownArrow);
    context.subscriptions.push(rightArrow);

    return {
        upArrow,
        downArrow,
        updownArrow,
        rightArrow,
    };
}

function initializeRequestHandling(client: vscodelc.BaseLanguageClient, requestTimeout: number) {
    const pendingRequests = new Map<string, () => void>();

    const scheduleRequest = (uri: vscode.Uri) => {
        const documentUri = uri.toString();
        const req = pendingRequests.get(documentUri);
        if (req) {
            req();
            pendingRequests.delete(documentUri);
        }

        const [resultPromise, cancel] = getCancellableBranchInfo(client, uri, requestTimeout);
        pendingRequests.set(documentUri, cancel);

        return resultPromise.finally(() => {
            if (pendingRequests.get(documentUri) === cancel)
                pendingRequests.delete(documentUri);
        });
    };
    const cancelRequest = (uri: vscode.Uri) => { pendingRequests.get(uri.toString())?.() };

    return { scheduleRequest, cancelRequest };

}

export function activateBranchDecorator(context: vscode.ExtensionContext, client: vscodelc.BaseLanguageClient) {
    const decorationTypes = createDecorationTypes(context);
    const { scheduleRequest, cancelRequest } = initializeRequestHandling(client, 1000);

    const activeEditorChanged = async (editor: vscode.TextEditor) => {
        const document = editor.document;
        if (document.languageId !== 'hlasm') return;

        const version = document.version;

        const result = await scheduleRequest(document.uri);

        if (document.isClosed || document.version !== version)
            return;

        updateEditor(editor, result, decorationTypes);
    };

    const updateEditorsRelatedToDocument = async (document: vscode.TextDocument) => {
        if (document.languageId !== 'hlasm') return;
        const version = document.version;

        const result = await scheduleRequest(document.uri);

        if (document.isClosed || document.version !== version)
            return;

        updateVisibleEditors(document.uri, result, decorationTypes);
    };

    context.subscriptions.push(vscode.window.onDidChangeActiveTextEditor(editor => editor && activeEditorChanged(editor)));
    context.subscriptions.push(vscode.workspace.onDidChangeTextDocument(async ({ document }) => updateEditorsRelatedToDocument(document)));
    context.subscriptions.push(vscode.workspace.onDidOpenTextDocument(updateEditorsRelatedToDocument));
    context.subscriptions.push(vscode.workspace.onDidCloseTextDocument(document => cancelRequest(document.uri)));

    getFirstOpenPromise(client).then(
        () => Promise.all(vscode.window.visibleTextEditors.map(e => activeEditorChanged(e))).catch(() => { })
    );
}
