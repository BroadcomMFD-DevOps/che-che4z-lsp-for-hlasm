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

import { Disposable } from "vscode";
import { BaseLanguageClient } from "vscode-languageclient";

// This is a temporary demo implementation

enum ExternalRequestType {
    read_file = 'read_file',
    read_directory = 'read_directory',
}

interface ExternalRequest {
    id: number,
    op: ExternalRequestType,
    url: string,
}

interface ExternalReadFileResponse {
    id: number,
    data: string,
}
interface ExternalReadDirectoryResponse {
    id: number,
    data: string[],
}
interface ExternalErrorResponse {
    id: number,
    error: {
        code: number,
        msg: string,
    },
}

const magicSchema = 'hlasm-external://';
export class HLASMExternalFiles {
    private toDispose: Disposable[] = [];

    constructor(client: BaseLanguageClient) {
        this.toDispose.push(client.onNotification('external_file_request', params => this.handleRawMessage(params).then(
            msg => { if (msg) client.sendNotification('external_file_response', msg); }
        )));
    }

    dispose() {
        this.toDispose.forEach(x => x.dispose());
    }

    private handleFileMessage(msg: ExternalRequest): Promise<ExternalReadFileResponse | ExternalReadDirectoryResponse | ExternalErrorResponse> {
        if (msg.url.startsWith(magicSchema) && /\/MAC[A-C]$/.test(msg.url))
            return Promise.resolve({
                id: msg.id,
                data: ` MACRO
                ${msg.url.substring(msg.url.length - 4)}
                MEND
                `});
        else
            return Promise.resolve({
                id: msg.id,
                error: { code: 0, msg: 'Not found' }
            });
    }
    private handleDirMessage(msg: ExternalRequest): Promise<ExternalReadFileResponse | ExternalReadDirectoryResponse | ExternalErrorResponse> {
        const magicSchema = 'hlasm-external://';
        if (msg.url.startsWith(magicSchema))
            return Promise.resolve({
                id: msg.id,
                data: ['MACA', 'MACB', 'MACC', 'MACD']
            });
        else
            return Promise.resolve({
                id: msg.id,
                error: { code: 0, msg: 'Not found' }
            });
    }

    public handleRawMessage(msg: any): Promise<ExternalReadFileResponse | ExternalReadDirectoryResponse | ExternalErrorResponse | null> {
        if (!msg || typeof msg.id !== 'number' || typeof msg.op !== 'string')
            return Promise.resolve(null);

        if (msg.op === ExternalRequestType.read_file && typeof msg.url === 'string')
            return this.handleFileMessage(msg);
        if (msg.op === ExternalRequestType.read_directory && typeof msg.url === 'string')
            return this.handleDirMessage(msg);

        return Promise.resolve({ id: msg.id, error: { code: -1, msg: 'Invalid request' } });
    }

}

