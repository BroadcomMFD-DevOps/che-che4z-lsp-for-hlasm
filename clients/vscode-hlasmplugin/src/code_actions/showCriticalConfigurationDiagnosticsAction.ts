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

export function generateShowCriticalConfigurationDiagsCodeAction(showOnlyCritical: boolean): vscode.CodeAction {
    return showOnlyCritical ?
        {
            title: 'Show only critical configuration diagnostics',
            command: {
                title: 'Show only critical configuration diagnostics',
                command: 'extension.hlasm-plugin.toggleNonCriticalConfigurationDiagnostics'
            },
            kind: vscode.CodeActionKind.QuickFix
        } :
        {
            title: 'Show all configuration diagnostics',
            command: {
                title: 'Show all configuration diagnostics',
                command: 'extension.hlasm-plugin.toggleNonCriticalConfigurationDiagnostics'
            },
            kind: vscode.CodeActionKind.QuickFix
        };
}