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

import { relative } from 'path';
import * as vscode from 'vscode';
import { hlasmplugin_folder, pgm_conf_file, proc_grps_file } from './constants';
import { uriExists } from './helpers';

export class HLASMCodeActionsProvider implements vscode.CodeActionProvider {
    async provideCodeActions(document: vscode.TextDocument, range: vscode.Range | vscode.Selection, context: vscode.CodeActionContext, token: vscode.CancellationToken): Promise<(vscode.CodeAction | vscode.Command)[]> {
        const result: vscode.CodeAction[] = [];
        const workspace = vscode.workspace.getWorkspaceFolder(document.uri);
        if (!workspace) return null;

        const procGrps = vscode.Uri.joinPath(workspace.uri, hlasmplugin_folder, proc_grps_file);
        const pgmConf = vscode.Uri.joinPath(workspace.uri, hlasmplugin_folder, pgm_conf_file);
        const [procGrpsExists, pgmConfExists] = await Promise.all([uriExists(procGrps), uriExists(pgmConf)]);

        let suggestProcGrpsChange = false;
        let suggestPgmConfChange = false;

        if (context.diagnostics.some(x => x.code === 'E049')) {
            if (procGrpsExists) {
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
            }
            suggestProcGrpsChange = true;
        }
        if (context.diagnostics.some(x => x.code === 'SUP' || x.code === 'E049')) {
            suggestPgmConfChange = true;
        }

        if (suggestProcGrpsChange || suggestPgmConfChange) {
            if (!procGrpsExists && !pgmConfExists) {
                suggestProcGrpsChange = false;
                suggestPgmConfChange = false;
                result.push({
                    title: 'Create configuration files',
                    command: {
                        title: 'Create configuration files',
                        command: 'extension.hlasm-plugin.createCompleteConfig',
                        arguments: [workspace.uri, relative(workspace.uri.path, document.uri.path), 'GRP1']
                    },
                    kind: vscode.CodeActionKind.QuickFix
                });
            }
            if (suggestProcGrpsChange) {
                if (procGrpsExists)
                    result.push({
                        title: 'Modify proc_grps.json configuration file',
                        command: {
                            title: 'Open proc_grps.json',
                            command: 'vscode.open',
                            arguments: [procGrps]
                        },
                        kind: vscode.CodeActionKind.QuickFix
                    });
                else
                    result.push({
                        title: 'Create proc_grps.json configuration file',
                        command: {
                            title: 'Create proc_grps.json',
                            command: 'extension.hlasm-plugin.createCompleteConfig',
                            arguments: [workspace.uri, null, '']
                        },
                        kind: vscode.CodeActionKind.QuickFix
                    });
            }
            if (suggestPgmConfChange) {
                if (pgmConfExists)
                    result.push({
                        title: 'Modify pgm_conf.json configuration file',
                        command: {
                            title: 'Open pgm_conf.json',
                            command: 'vscode.open',
                            arguments: [pgmConf]
                        },
                        kind: vscode.CodeActionKind.QuickFix
                    });
                else
                    result.push({
                        title: 'Create pgm_conf.json configuration file',
                        command: {
                            title: 'Create pgm_conf.json',
                            command: 'extension.hlasm-plugin.createCompleteConfig',
                            arguments: [workspace.uri, relative(workspace.uri.path, document.uri.path), null]
                        },
                        kind: vscode.CodeActionKind.QuickFix
                    });
            }
        }

        return result.length !== 0 ? result : null;
    }
}
