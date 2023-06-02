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


import * as vscode from "vscode";
import * as vscodelc from "vscode-languageclient";

interface ExternalConfigurationRequest {
    uri: string;
};
interface Proc_Grps_Schema_json { };
interface ExternalConfigurationResponse {
    configuration: string | Proc_Grps_Schema_json;
};

function isExternalConfigurationRequest(p: any): p is ExternalConfigurationRequest {
    return typeof p === 'object' && ('uri' in p);
}

export class HLASMExternalConfigurationProvider {
    private toDispose: vscode.Disposable[] = [];

    constructor(
        private channel: {
            onRequest<R, E>(method: string, handler: vscodelc.GenericRequestHandler<R, E>): vscode.Disposable;
        }) {
        this.toDispose.push(this.channel.onRequest('external_configuration_request', (...params: any[]) => this.handleRawMessage(...params)));
    }

    dispose() {
        this.toDispose.forEach(x => x.dispose());
    }

    async handleRequest(r: ExternalConfigurationRequest): Promise<ExternalConfigurationResponse | vscodelc.ResponseError> {
        return new vscodelc.ResponseError(0, 'Not found');
    }

    async handleRawMessage(...params: any[]): Promise<ExternalConfigurationResponse | vscodelc.ResponseError> {
        if (params.length !== 1 || !isExternalConfigurationRequest(params[0]))
            return new vscodelc.ResponseError(-5, 'Invalid request');

        return this.handleRequest(params[0]);
    }
}
