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

export class HLASMCodeActionsProvider implements vscode.CodeActionProvider {
    provideCodeActions(document: vscode.TextDocument, range: vscode.Range | vscode.Selection, context: vscode.CodeActionContext, token: vscode.CancellationToken): vscode.ProviderResult<(vscode.CodeAction | vscode.Command)[]> {
        if (context.diagnostics.some(x => x.code === 'E049')) {
            return [
                { title: "Download dependencies", command: "extension.hlasm-plugin.downloadDependencies", arguments: ['newOnly'] },
                { title: "Download all dependencies", command: "extension.hlasm-plugin.downloadDependencies" },
            ];
        }
        return null;
    }
}
