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
import * as vscodelc from 'vscode-languageclient';
import { retrieveConfigurationNodes } from './configurationNodes';
import { generateShowConfigurationDiagsCodeAction } from './code_actions/showConfigurationDiagnosticsAction'
import { generateOpcodeSuggestionsCodeActions } from './code_actions/opcodeSuggestionsActions'
import { generateDownloadDependenciesCodeActions } from './code_actions/downloadDependenciesActions'
import { generateConfigurationFilesCodeActions } from './code_actions/configurationFilesActions'

export class HLASMCodeActionsProvider implements vscode.CodeActionProvider {
    constructor(private client: vscodelc.BaseLanguageClient) { }

    async provideCodeActions(document: vscode.TextDocument, range: vscode.Range | vscode.Selection, context: vscode.CodeActionContext, token: vscode.CancellationToken): Promise<(vscode.CodeAction | vscode.Command)[]> {
        const result: vscode.CodeAction[] = [];

        const isConfig = document.fileName.endsWith('\\.bridge.json') || document.fileName.endsWith('\\pgm_conf.json');
        const hasCriticalConfigDiags = context.diagnostics.some(x => x.code === 'W0004') || context.diagnostics.some(x => x.code === 'B4G002');
        const hasNonCriticalConfigDiags = context.diagnostics.some(x => x.code === 'CFG001');
        if (isConfig || hasCriticalConfigDiags || hasNonCriticalConfigDiags)
            result.push(generateShowConfigurationDiagsCodeAction(hasNonCriticalConfigDiags));

        const E049 = context.diagnostics.filter(x => x.code === 'E049');
        if (E049.length > 0)
            result.push(...await generateOpcodeSuggestionsCodeActions(E049, this.client, document));

        const wsUri = vscode.workspace.getWorkspaceFolder(document.uri)?.uri;
        if (!wsUri)
            return result;

        const configNodes = await retrieveConfigurationNodes(wsUri, document.uri);

        if (E049.length > 0 && configNodes.procGrps.exists)
            result.push(...generateDownloadDependenciesCodeActions());

        const suggestProcGrpsChange = E049.length > 0;
        const suggestPgmConfChange = E049.length > 0 || context.diagnostics.some(x => x.code === 'SUP');
        result.push(...generateConfigurationFilesCodeActions(suggestProcGrpsChange, suggestPgmConfChange, configNodes, wsUri, document));

        return result;
    }
}
