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
import * as ftp from 'basic-ftp';
import { ClientUriDetails, ExternalFilesClient, ExternalRequestType } from './hlasmExternalFiles';
import { ConnectionInfo, gatherConnectionInfo, getLastRunConfig, translateConnectionInfo, updateLastRunConfig } from './ftpCreds';
import { AsyncMutex } from './asyncMutex';
import { FBWritable } from './FBWritable';
import { isCancellationError } from './helpers';

const checkResponse = (resp: ftp.FTPResponse) => {
    if (resp.code < 200 || resp.code > 299) throw Error("FTP Error: " + resp.message);
    return resp;
}

const checkedCommand = async (client: ftp.Client, command: string): Promise<string> => {
    return checkResponse(await client.send(command)).message
}

class DatasetUriDetails implements ClientUriDetails {
    constructor(
        public readonly dataset: string,
        public readonly member: string | null) { }

    toString() {
        if (this.member)
            return `${this.dataset}(${this.member})`;
        else
            return this.dataset;
    }

    normalizedPath() {
        return `/${this.dataset}/${this.member || ''}`;
    }
};

export class HLASMExternalFilesFtp implements ExternalFilesClient {
    private activeConnectionInfo: ConnectionInfo = null;
    private clientSuspended = false;

    private pooledClient: ftp.Client = null;
    private pooledClientTimeout: ReturnType<typeof setTimeout> = null;

    private mutex = new AsyncMutex();

    private static pooledClientReleaseTimeout = 30000;

    private stateChanged = new vscode.EventEmitter<boolean>();

    get onStateChange() {
        return this.stateChanged.event;
    }

    private clearPoolClient() {
        clearTimeout(this.pooledClientTimeout);
        const client = this.pooledClient;
        this.pooledClient = null;
        if (client)
            client.close();
    }

    suspend() {
        if (this.clientSuspended) return;
        try {
            this.clearPoolClient();
        }
        finally {
            this.clientSuspended = true;
            this.stateChanged.fire(true);
        }
    }
    resume() {
        if (!this.clientSuspended) return;

        this.clientSuspended = false;
        this.stateChanged.fire(false);
    }
    suspended() {
        return this.clientSuspended;
    }

    dispose(): void {
        this.stateChanged.dispose();
        this.clearPoolClient();
    }

    constructor(private context: vscode.ExtensionContext) { }

    private async getConnInfo() {
        try {
            if (this.clientSuspended)
                throw new vscode.CancellationError();

            if (this.activeConnectionInfo)
                return this.activeConnectionInfo;

            const last = getLastRunConfig(this.context);
            const connection = await gatherConnectionInfo(last);
            await updateLastRunConfig(this.context, { host: connection.host, user: connection.user, jobcard: last.jobcard });

            return connection;
        }
        catch (e) {
            if (isCancellationError(e))
                this.suspend();

            throw e;
        }
    }

    private requestEndHandle(client: ftp.Client) {
        if (client.closed)
            return;

        if (this.pooledClient || this.clientSuspended) {
            client.close();
            return;
        }

        this.pooledClientTimeout = setTimeout(() => this.clearPoolClient(), HLASMExternalFilesFtp.pooledClientReleaseTimeout);
        this.pooledClient = client;
    }

    private async getConnectedClient() {
        if (this.clientSuspended)
            throw new vscode.CancellationError();

        if (this.pooledClient) {
            clearTimeout(this.pooledClientTimeout);
            const client = this.pooledClient;
            this.pooledClient = null;
            return client;
        }

        while (true) {
            const client = new ftp.Client();
            try {
                await this.mutex.locked(async () => {
                    const connection = await this.getConnInfo();

                    await client.access(translateConnectionInfo(connection));

                    client.parseList = (rawList: string): ftp.FileInfo[] => {
                        return rawList.split(/\r?\n/).slice(1).filter(x => !/^\s*$/.test(x)).map(value => new ftp.FileInfo(value.split(/\s/, 1)[0]));
                    };

                    this.activeConnectionInfo = connection;
                });

                return client;
            }
            catch (e) {
                this.activeConnectionInfo = null;

                if (e instanceof ftp.FTPError) {
                    vscode.window.showErrorMessage(e.message);
                    continue;
                }

                client.close();

                throw e;
            }
        }
    }

    async listMembersImpl(client: ftp.Client, dataset: string): Promise<string[] | null> {
        try {
            await checkedCommand(client, 'TYPE A');
            checkResponse(await client.cd(`'${dataset}'`));
            const list = await client.list();
            return list.map(x => x.name);
        }
        catch (e) {
            if (e instanceof ftp.FTPError && e.code == 550)
                return null;
            throw e;
        }
    }

    async listMembers(args: DatasetUriDetails): Promise<string[] | null> {
        const client = await this.getConnectedClient();
        return this.listMembersImpl(client, args.dataset).finally(() => { this.requestEndHandle(client) });
    }

    async readMemberImpl(client: ftp.Client, dataset: string, member: string): Promise<string | null> {
        try {
            const buffer = new FBWritable();
            buffer.on('error', err => { throw err });

            await checkedCommand(client, 'TYPE I');
            checkResponse(await client.downloadTo(buffer, `'${dataset}(${member})'`));

            return buffer.getResult();
        }
        catch (e) {
            if (e instanceof ftp.FTPError && e.code == 550)
                return null;
            throw e;
        }
    }
    async readMember(args: DatasetUriDetails): Promise<string | null> {
        const client = await this.getConnectedClient();
        return this.readMemberImpl(client, args.dataset, args.member).finally(() => { this.requestEndHandle(client); });
    }

    parseArgs(path: string, purpose: ExternalRequestType): ClientUriDetails {
        const [dataset, member] = path.split('/').slice(1).map(x => x.toUpperCase());

        if (!dataset || !/^(?:[A-Z$#@][A-Z$#@0-9]{1,7})(?:\.(?:[A-Z$#@][A-Z$#@0-9]{1,7}))*$/.test(dataset) || dataset.length > 44) return null;
        switch (purpose) {
            case ExternalRequestType.read_file:
                if (!member || !/^[A-Z$#@][A-Z$#@0-9]{1,7}(?:\..*)?$/.test(member)) return null; // ignore extension
                break;
            case ExternalRequestType.read_directory:
                if (member) return null;
                break;
        }

        return new DatasetUriDetails(dataset, member?.split('.', 1)[0] || null);
    }
}
