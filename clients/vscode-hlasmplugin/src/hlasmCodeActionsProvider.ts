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
import * as vscodelc from 'vscode-languageclient/node';
import { configurationExists } from './helpers';

interface OpcodeSuggestionResponse {
    uri: string;
    suggestions: {
        [key: string]: {
            opcode: string;
            distance: number;
        }[]
    };
};

function unique(a: string[]) {
    return [...new Set(a)];
}

export class HLASMCodeActionsProvider implements vscode.CodeActionProvider {
    constructor(private client: vscodelc.BaseLanguageClient) { }

    async provideCodeActions(document: vscode.TextDocument, range: vscode.Range | vscode.Selection, context: vscode.CodeActionContext, token: vscode.CancellationToken): Promise<(vscode.CodeAction | vscode.Command)[]> {
        const result: vscode.CodeAction[] = [];
        const workspace = vscode.workspace.getWorkspaceFolder(document.uri);
        if (!workspace) return null;

        const [procGrps, pgmConf, ebgConf] = await configurationExists(workspace.uri);

        let suggestProcGrpsChange = false;
        let suggestPgmConfChange = false;

        const E049 = context.diagnostics.filter(x => x.code === 'E049');

        const opcodeTasks = E049.map(diag => { return { diag, opcode: document.getText(diag.range).toUpperCase() }; });

        if (E049.length > 0) {
            if (procGrps.exists) {
                await this.client.onReady();
                const suggestionsResponse = await this.client.sendRequest<OpcodeSuggestionResponse>("textDocument/$/opcode_suggestion", {
                    textDocument: { uri: document.uri.toString() },
                    opcodes: unique(opcodeTasks.map(x => x.opcode)),
                    extended: true,
                });

                for (const { diag, opcode } of opcodeTasks) {
                    if (opcode in suggestionsResponse.suggestions) {
                        const subst = suggestionsResponse.suggestions[opcode];
                        for (const s of subst) {
                            const edit = new vscode.WorkspaceEdit();
                            edit.replace(document.uri, diag.range, s.opcode)
                            result.push({
                                title: `Did you mean '${s.opcode}'?`,
                                diagnostics: [diag],
                                kind: vscode.CodeActionKind.QuickFix,
                                edit: edit
                            });
                        }
                    }
                }

                result.push({
                    title: 'Download missing dependencies',
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
        if (E049.length > 0 || context.diagnostics.some(x => x.code === 'SUP')) {
            suggestPgmConfChange = true;
        }

        if (suggestProcGrpsChange || suggestPgmConfChange) {
            if (!procGrps.exists && !pgmConf.exists && !ebgConf.exists) {
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
                if (procGrps.exists)
                    result.push({
                        title: 'Modify proc_grps.json configuration file',
                        command: {
                            title: 'Open proc_grps.json',
                            command: 'vscode.open',
                            arguments: [procGrps.uri]
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
                if (pgmConf.exists)
                    result.push({
                        title: 'Modify pgm_conf.json configuration file',
                        command: {
                            title: 'Open pgm_conf.json',
                            command: 'vscode.open',
                            arguments: [pgmConf.uri]
                        },
                        kind: vscode.CodeActionKind.QuickFix
                    });
                else {
                    if (ebgConf.exists) {
                        // TODO: could we trigger B4G sync?
                    }
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
        }

        return result.length !== 0 ? result : null;
    }
}
