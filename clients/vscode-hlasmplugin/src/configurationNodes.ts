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
import { uriExists } from './helpers'
import { hlasmplugin_folder, proc_grps_file, pgm_conf_file, bridge_json_file, ebg_folder as ebg_folder } from './constants';

export type ConfigurationNodeDetails = {
    uri: vscode.Uri;
    exists: boolean;
};

export type SettingsConfigurationNodeDetails = {
    ws: vscode.Uri;
    scope: vscode.ConfigurationTarget;
    key: string;
    exists: true;
};

export type BridgeConfigurationNodeDetails = ConfigurationNodeDetails | {
    uri: null;
    exists: false;
};

export interface ConfigurationNodes {
    procGrps: ConfigurationNodeDetails | SettingsConfigurationNodeDetails;
    pgmConf: ConfigurationNodeDetails | SettingsConfigurationNodeDetails;
    bridgeJson: BridgeConfigurationNodeDetails;
    ebgFolder: ConfigurationNodeDetails;
}

function addAlternative(node: ConfigurationNodeDetails, ws: vscode.Uri, key: string): ConfigurationNodeDetails | SettingsConfigurationNodeDetails {
    if (node.exists)
        return node;

    const config = vscode.workspace.getConfiguration('hlasm', ws);
    const data = config.get(key);

    if (!data || typeof data !== 'object')
        return node;

    const details = config.inspect(key);
    return {
        ws,
        scope: details?.workspaceFolderValue
            ? vscode.ConfigurationTarget.WorkspaceFolder
            : details?.workspaceValue
                ? vscode.ConfigurationTarget.Workspace
                : vscode.ConfigurationTarget.Global,
        key: `hlasm.${key}`,
        exists: true,
    };
}

export async function retrieveConfigurationNodes(workspace: vscode.Uri, documentUri: vscode.Uri | undefined, fs: vscode.FileSystem = vscode.workspace.fs): Promise<ConfigurationNodes> {
    const procGrps = vscode.Uri.joinPath(workspace, hlasmplugin_folder, proc_grps_file);
    const pgmConf = vscode.Uri.joinPath(workspace, hlasmplugin_folder, pgm_conf_file);
    const bridgeJson = documentUri ? vscode.Uri.joinPath(documentUri, "..", bridge_json_file) : undefined;
    const ebgFolder = vscode.Uri.joinPath(workspace, ebg_folder);


    const results = await Promise.all([
        uriExists(procGrps, fs).then(b => { return { uri: procGrps, exists: b }; }),
        uriExists(pgmConf, fs).then(b => { return { uri: pgmConf, exists: b }; }),
        bridgeJson ? uriExists(bridgeJson, fs).then(b => { return { uri: bridgeJson, exists: b }; }) : { uri: null, exists: false } as BridgeConfigurationNodeDetails,
        uriExists(ebgFolder, fs).then(b => { return { uri: ebgFolder, exists: b }; }),
    ]);

    return {
        procGrps: addAlternative(results[0], workspace, 'proc_grps'),
        pgmConf: addAlternative(results[1], workspace, 'pgm_conf'),
        bridgeJson: results[2],
        ebgFolder: results[3],
    };
}

export function anyConfigurationNodeExists(configNodes: ConfigurationNodes) {
    return configNodes.procGrps.exists || configNodes.pgmConf.exists || configNodes.bridgeJson.exists || configNodes.ebgFolder.exists;
}
