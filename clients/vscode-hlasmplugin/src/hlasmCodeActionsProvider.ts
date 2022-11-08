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
import { hlasmplugin_folder, pgm_conf_file, proc_grps_file } from './constants';

export class HLASMCodeActionsProvider implements vscode.CodeActionProvider {
    provideCodeActions(document: vscode.TextDocument, range: vscode.Range | vscode.Selection, context: vscode.CodeActionContext, token: vscode.CancellationToken): vscode.ProviderResult<(vscode.CodeAction | vscode.Command)[]> {
        const result: vscode.CodeAction[] = [];
        const workspace = vscode.workspace.getWorkspaceFolder(document.uri);
        if (!workspace) return null;

        if (context.diagnostics.some(x => x.code === 'E049')) {
            result.push({
                title: 'Download dependencies',
                command: {
                    title: 'Download dependencies',
                    command: 'extension.hlasm-plugin.downloadDependencies',
                    arguments: ['newOnly']
                },
                kind: vscode.CodeActionKind.QuickFix
            });
            result.push({
                title: 'Download all dependencies',
                command: {
                    title: 'Download dependencies',
                    command: 'extension.hlasm-plugin.downloadDependencies'
                },
                kind: vscode.CodeActionKind.QuickFix
            });
            result.push({
                title: 'Modify proc_grps.json configuration file',
                command: {
                    title: 'Open proc_grps.json',
                    command: 'vscode.open',
                    arguments: [vscode.Uri.joinPath(workspace.uri, hlasmplugin_folder, proc_grps_file)]
                },
                kind: vscode.CodeActionKind.QuickFix
            });

        }
        if (context.diagnostics.some(x => x.code === 'SUP' || x.code === 'E049')) {
            result.push({
                title: 'Modify pgm_conf.json configuration file',
                command: {
                    title: 'Open pgm_conf.json',
                    command: 'vscode.open',
                    arguments: [vscode.Uri.joinPath(workspace.uri, hlasmplugin_folder, pgm_conf_file)]
                },
                kind: vscode.CodeActionKind.QuickFix
            });
        }
        return result.length !== 0 ? result : null;
    }
}
