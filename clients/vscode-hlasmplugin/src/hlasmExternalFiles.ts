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

import { TextDecoder, TextEncoder } from "util";
import * as vscode from "vscode";
import * as vscodelc from "vscode-languageclient";
import { isCancellationError } from "./helpers";

// This is a temporary demo implementation

export enum ExternalRequestType {
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
    data: {
        members: string[],
        suggested_extension: string | undefined,
    },
}
interface ExternalErrorResponse {
    id: number,
    error: {
        code: number,
        msg: string,
    },
}

export interface ClientUriDetails {
    toString(): string;
    normalizedPath(): string;
}

export interface ExternalFilesClient extends vscode.Disposable {
    listMembers(arg: ClientUriDetails): Promise<string[] | null>;
    readMember(arg: ClientUriDetails): Promise<string | null>;

    onStateChange: vscode.Event<boolean>;

    suspend(): void;
    resume(): void;

    suspended(): boolean;

    parseArgs(path: string, purpose: ExternalRequestType): ClientUriDetails;
}

const magicScheme = 'hlasm-external';

function invalidResponse(msg: ExternalRequest) {
    return Promise.resolve({ id: msg.id, error: { code: -5, msg: 'Invalid request' } });
}

const uriFriendlyBase16Stirng = 'abcdefghihjkmnop';
const uriFriendlyBase16StirngUC = 'ABCDEFGHIHJKMNOP';
const uriFriendlyBase16StirngBoth = 'abcdefghihjkmnopABCDEFGHIHJKMNOP';

const uriFriendlyBase16Map = (() => {
    const result = [];
    for (const c0 of uriFriendlyBase16Stirng)
        for (const c1 of uriFriendlyBase16Stirng)
            result.push(c0 + c1);

    return result;
})();

function uriFriendlyBase16Encode(s: string) {
    return [...new TextEncoder().encode(s)].map(x => uriFriendlyBase16Map[x]).join('');
}

function uriFriendlyBase16Decode(s: string) {
    if (s.length & 1) return '';
    const array = [];
    for (let i = 0; i < s.length; i += 2) {
        const c0 = uriFriendlyBase16StirngBoth.indexOf(s[i]);
        const c1 = uriFriendlyBase16StirngBoth.indexOf(s[i + 1]);
        if (c0 < 0 || c1 < 0) return '';
        array.push((c0 & 15) << 4 | (c1 & 15));
    }
    try {
        return new TextDecoder(undefined, { fatal: true, ignoreBOM: false }).decode(Uint8Array.from(array));
    } catch (e) {
        return '';
    }
}

function take<T>(it: IterableIterator<T>, n: number): T[] {
    const result: T[] = [];
    while (n) {
        const val = it.next();
        if (val.done)
            break;
        result.push(val.value);
        --n;
    }
    return result;
}

const not_exists = Object.freeze({});
const no_client = Object.freeze({});
interface in_error { message: string };

interface CacheEntry<T> {
    service: string,
    parsedArgs: ClientUriDetails,
    result: T | in_error | typeof not_exists | typeof no_client,
    references: Set<string>;
};

export class HLASMExternalFiles {
    private toDispose: vscode.Disposable[] = [];

    private memberLists = new Map<string, CacheEntry<string[]>>();
    private memberContent = new Map<string, CacheEntry<string>>();

    private pendingRequests = new Set<{ topic: string }>();

    private clients = new Map<string, {
        client: ExternalFilesClient;
        clientDisposables: vscode.Disposable[];
    }>();

    setClient(service: string, client: ExternalFilesClient) {
        if (!/^[A-Z]+$/.test(service))
            throw Error('Invalid service name');

        const oldClient = this.clients.get(service);
        if (oldClient) {
            this.clients.delete(service);

            oldClient.clientDisposables.forEach(x => x.dispose());
            oldClient.clientDisposables = [];
            oldClient.client.dispose();
        }

        if (!client) return;

        const newClient = { client: client, clientDisposables: [] as vscode.Disposable[] };
        this.clients.set(service, newClient);

        newClient.client = client;
        newClient.clientDisposables.push(client.onStateChange((suspended) => {
            if (suspended) {
                if (this.activeProgress) {
                    clearTimeout(this.pendingActiveProgressCancellation);
                    this.activeProgress.done();
                    this.activeProgress = null;
                }
                vscode.window.showInformationMessage("Retrieval of remote files has been suspended.");
            }
            else
                this.notifyAllWorkspaces(service, false);
        }));
        if (oldClient || !client.suspended())
            this.notifyAllWorkspaces(service, !!oldClient);
    }

    private getClient(service: string) { const c = this.clients.get(service); return c && c.client; }


    private extractUriDetails(uri: string | vscode.Uri, purpose: ExternalRequestType): { cacheKey: string, service: string, client: ExternalFilesClient, details: ClientUriDetails, associatedWorkspace: string } | null {
        if (typeof uri === 'string')
            uri = vscode.Uri.parse(uri, true);

        const pathParser = /^\/([A-Z]+)(\/.*)?/;

        const matches = pathParser.exec(uri.path);

        if (uri.scheme !== magicScheme || !matches)
            return {
                cacheKey: null,
                service: null,
                client: null,
                details: null,
                associatedWorkspace: null,
            };

        const service = matches[1];
        const subpath = matches[2] || '';

        const client = this.clients.get(service);
        const details = client?.client.parseArgs(subpath, purpose);

        return {
            cacheKey: details ? `/${service}${details.normalizedPath()}` : uri.path,
            service: client ? service : null,
            client: client?.client,
            details: details,
            associatedWorkspace: uriFriendlyBase16Decode(uri.authority)
        };
    }

    private prepareChangeNotification<T>(service: string, cache: Map<string, CacheEntry<T>>, all: boolean) {
        const changes = [...cache]
            .filter(([, v]) => v.service === service && (
                all ||
                typeof v.result === 'object' && (v.result === no_client || 'message' in v.result)
            ))
            .map(([path, value]) => { return { path: path, refs: value.references }; });

        changes.forEach(x => cache.delete(x.path));

        return changes.map(x => [...x.refs]).flat().map(x => {
            return {
                uri: x,
                type: vscodelc.FileChangeType.Changed
            };
        });
    }

    private notifyAllWorkspaces(service: string, all: boolean) {
        this.lspClient.sendNotification(vscodelc.DidChangeWatchedFilesNotification.type, {
            changes: (vscode.workspace.workspaceFolders || []).map(w => {
                return {
                    uri: `${magicScheme}://${uriFriendlyBase16Encode(w.uri.toString())}`,
                    type: vscodelc.FileChangeType.Changed
                };
            })
                .concat(this.prepareChangeNotification(service, this.memberContent, all))
                .concat(this.prepareChangeNotification(service, this.memberLists, all))
        });
    }

    activeProgress: { progressUpdater: vscode.Progress<{ message?: string; increment?: number }>, done: () => void } = null;
    pendingActiveProgressCancellation: ReturnType<typeof setTimeout> = null;
    addWIP<T>(service: string, topic: string) {
        clearTimeout(this.pendingActiveProgressCancellation);
        if (!this.activeProgress && !this.getClient(service).suspended()) {
            vscode.window.withProgress({ title: 'Retrieving remote files', location: vscode.ProgressLocation.Notification }, (progress, c) => {
                return new Promise<void>((resolve) => {
                    this.activeProgress = { progressUpdater: progress, done: resolve };
                });
            });
        }
        const wip = { topic };
        this.pendingRequests.add(wip);

        if (this.activeProgress)
            this.activeProgress.progressUpdater.report({
                message: take(this.pendingRequests.values(), 3)
                    .map((v, n) => { return n < 2 ? v.topic : '...' })
                    .join(', ')
            });

        return () => {
            const result = this.pendingRequests.delete(wip);

            if (this.pendingRequests.size === 0) {
                this.pendingActiveProgressCancellation = setTimeout(() => {
                    this.activeProgress.done();
                    this.activeProgress = null;
                }, 2500);
            }

            return result;
        };
    }

    public suspendAll() {
        this.clients.forEach(({ client }) => { client.suspend() });
    }

    public resumeAll() {
        this.clients.forEach(({ client }) => { client.resume() });
    }

    constructor(private lspClient: vscodelc.BaseLanguageClient) {
        this.toDispose.push(lspClient.onNotification('external_file_request', params => this.handleRawMessage(params).then(
            msg => { if (msg) lspClient.sendNotification('external_file_response', msg); }
        )));

        lspClient.onDidChangeState(e => {
            if (e.newState === vscodelc.State.Starting)
                this.reset();
        }, this, this.toDispose);

        const me = this;
        this.toDispose.push(vscode.workspace.registerTextDocumentContentProvider(magicScheme, {
            async provideTextDocumentContent(uri: vscode.Uri, token: vscode.CancellationToken): Promise<string | null> {
                const result = await me.handleFileMessage({ url: uri.toString(), id: -1, op: ExternalRequestType.read_file });
                if (result && 'data' in result && typeof result.data === 'string')
                    return result.data;
                else
                    return null;
            }
        }));
    }

    reset() {
        this.pendingRequests.clear();
    }

    dispose() {
        this.toDispose.forEach(x => x.dispose());
        [...this.clients.keys()].forEach(x => this.setClient(x, null));
    }

    private async getFile(client: ExternalFilesClient, service: string, parsedArgs: ClientUriDetails): Promise<string | in_error | typeof not_exists | typeof no_client | null> {
        const interest = this.addWIP(service, parsedArgs.toString());

        try {
            const result = await client.readMember(parsedArgs);

            if (!interest()) return null;

            if (!result)
                return not_exists;

            return result;

        } catch (e) {
            if (!isCancellationError(e)) {
                this.suspendAll();
                vscode.window.showErrorMessage(e.message);
            }

            if (!interest()) return null;

            return { message: e.message };
        }
    }

    private async handleFileMessage(msg: ExternalRequest): Promise<ExternalReadFileResponse | ExternalErrorResponse | null> {
        const { cacheKey, service, client, details, associatedWorkspace } = this.extractUriDetails(msg.url, ExternalRequestType.read_file);
        if (!cacheKey || client && !details)
            return invalidResponse(msg);

        let content = this.memberContent.get(cacheKey);
        if (content === undefined) {
            const result = client ? await this.getFile(client, service, details) : no_client;
            if (!result) return Promise.resolve(null);
            content = {
                service: service,
                parsedArgs: details,
                result: result,
                references: new Set<string>(),
            };

            this.memberContent.set(cacheKey, content);
        }
        content.references.add(msg.url)

        return this.transformResult(msg.id, content, result => result);
    }

    private async getDir(client: ExternalFilesClient, service: string, parsedArgs: ClientUriDetails): Promise<string[] | in_error | typeof not_exists | typeof no_client | null> {
        const interest = this.addWIP(service, parsedArgs.toString());

        try {
            const result = await client.listMembers(parsedArgs);

            if (!interest()) return null;

            if (!result)
                return not_exists;

            return result;
        }
        catch (e) {
            if (!isCancellationError(e)) {
                this.suspendAll();
                vscode.window.showErrorMessage(e.message);
            }

            if (!interest()) return null;

            return { message: e.message };
        }
    }

    private async handleDirMessage(msg: ExternalRequest): Promise<ExternalReadDirectoryResponse | ExternalErrorResponse | null> {
        const { cacheKey, service, client, details, associatedWorkspace } = this.extractUriDetails(msg.url, ExternalRequestType.read_directory);
        if (!cacheKey || client && !details)
            return invalidResponse(msg);

        let content = this.memberLists.get(cacheKey);
        if (content === undefined) {
            const result = client ? await this.getDir(client, service, details) : no_client;
            if (!result) return Promise.resolve(null);
            content = {
                service: service,
                parsedArgs: details,
                result: result,
                references: new Set<string>(),
            };

            this.memberLists.set(cacheKey, content);
        }
        content.references.add(msg.url);

        return this.transformResult(msg.id, content, (result) => {
            return {
                members: result,
                suggested_extension: '.hlasm',
            };
        });
    }

    private transformResult<T>(
        id: number,
        content: CacheEntry<T>,
        transform: (result: T) => (T extends string[] ? ExternalReadDirectoryResponse : ExternalReadFileResponse)['data']
    ): Promise<(T extends string[] ? ExternalReadDirectoryResponse : ExternalReadFileResponse) | ExternalErrorResponse> {

        if (content.result === not_exists)
            return Promise.resolve({
                id,
                error: { code: 0, msg: 'Not found' }
            });
        else if (content.result === no_client)
            return Promise.resolve({
                id,
                error: { code: -1000, msg: 'No client' }
            });
        else if (typeof content.result === 'object' && 'message' in content.result)
            return Promise.resolve({
                id,
                error: { code: -1000, msg: content.result.message }
            });
        else
            return Promise.resolve(<T extends string[] ? ExternalReadDirectoryResponse : ExternalReadFileResponse>{
                id,
                data: transform(<T>content.result),
            });

    }

    public handleRawMessage(msg: any): Promise<ExternalReadFileResponse | ExternalReadDirectoryResponse | ExternalErrorResponse | null> {
        if (!msg || typeof msg.id !== 'number' || typeof msg.op !== 'string')
            return Promise.resolve(null);

        if (msg.op === ExternalRequestType.read_file && typeof msg.url === 'string')
            return this.handleFileMessage(msg);
        if (msg.op === ExternalRequestType.read_directory && typeof msg.url === 'string')
            return this.handleDirMessage(msg);

        return invalidResponse(msg);
    }

}

