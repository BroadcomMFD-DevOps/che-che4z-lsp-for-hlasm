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
import { fork } from 'child_process';
import { Client, FTPResponse, FileInfo, FTPError } from 'basic-ftp'
import { Readable, Writable } from 'stream';
import { EOL, homedir, type } from 'os';
import path = require('node:path');
import { mkdir, rmSync, access } from 'fs';
import { promises as fsp } from "fs"
import { resolve } from 'path';
import { rejects } from 'assert';

type job_id = string;
class job_description {
    constructor(
        public jobname: string,
        public id: job_id,
        public details: string) { }

    public get_detail_info(): { rc: number; spool_files: number } | undefined {
        const parsed = /^.*RC=(\d+)\s+(\d+) spool file/.exec(this.details);
        if (!parsed)
            return undefined;
        else
            return { rc: +parsed[1], spool_files: +parsed[2] };
    }
}
interface job_client {
    submit_jcl(jcl: string): Promise<job_id>;
    set_list_mask(mask: string): Promise<void>;
    list(): Promise<job_description[]>;
    download(target: Writable | string, id: job_id, spool_file: number): Promise<void>;
    close(): void;
}

type JobFileInfo = FileInfo & { details: string };

async function basic_ftp_job_client(connection: {
    host: string;
    port?: number;
    user: string;
    password: string;
}): Promise<job_client> {
    const client = new Client();
    client.parseList = (rawList: string): JobFileInfo[] => {
        return rawList.split(/\r?\n/).slice(1).filter(x => !/^\s*$/.test(x)).map((value) => {

            const parsed_line = /(\S+)\s+(\S+)\s+(.*)/.exec(value);
            if (!parsed_line)
                throw Error("Unable to parse the job list");
            const f: JobFileInfo = Object.assign(new FileInfo(parsed_line[1]), {
                details: parsed_line[3]
            });
            f.uniqueID = parsed_line[2];
            return f;
        })
    };
    await client.access({
        host: connection.host,
        user: connection.user,
        password: connection.password,
        port: connection.port,
    });

    const check_response = (resp: FTPResponse) => {
        if (resp.code < 200 || resp.code > 299)
            throw Error("FTP Error: " + resp.message);
    }
    const checked_command = async (command: string): Promise<string> => {
        const resp = await client.send(command);
        check_response(resp);
        return resp.message
    }
    const switch_text = async () => { await checked_command("TYPE A"); }
    const switch_binary = async () => { await checked_command("TYPE I"); }

    await checked_command("SITE FILE=JES");
    return {
        async submit_jcl(jcl: string): Promise<string> {
            await switch_text();
            const job_upload = await client.uploadFrom(Readable.from(jcl), "JOB");
            check_response(job_upload);
            return /^.*as ([Jj](?:[Oo][Bb])?\d+)/.exec(job_upload.message)[1];
        },
        async set_list_mask(mask: string): Promise<void> {
            await checked_command("SITE JESJOBNAME=" + mask);
            await checked_command("SITE JESSTATUS=OUTPUT");
        },
        async list(): Promise<job_description[]> {
            try {
                await switch_text();
                return (await client.list()).map((x: JobFileInfo) => new job_description(x.name, x.uniqueID, x.details));
            }
            catch (e) {
                if (e instanceof FTPError && e.code == 550)
                    return [];
                throw e;
            }
        },
        async download(target: string | Writable, id: job_id, spool_file: number): Promise<void> {
            await switch_binary();
            check_response(await client.downloadTo(target, id + "." + spool_file));
        },
        close(): void {
            client.close();
        }
    };
}


class job_detail {
    dsn: string;
    dirs: string[];
} [];

class dataset_download_list {
    connection: {
        host: string;
        port?: number;
        user: string;
        password: string;
    };
    list: job_detail[];
    job_header_pattern: string;
}

class submitted_job {
    jobname: string;
    jobid: string;
    details: job_detail;
    downloaded: boolean;
    unpacking?: Promise<void>;
}

class parsed_job_header {
    job_header: {
        prefix: string,
        repl_count: number,
        suffix: string
    } | string;
    job_mask: string;
}
const translation_table: string = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
function prepare_job_header(pattern: string): parsed_job_header {
    const match = /^(\/\/[^ ?]+)(\?*)( .*)$/.exec(pattern);

    if (!match)
        throw Error("Invalid JOB header");
    else if (match[2].length)
        return { job_header: { prefix: match[1], repl_count: match[2].length, suffix: match[3] }, job_mask: match[1].slice(2) + '*' };
    else
        return { job_header: pattern, job_mask: pattern.slice(2, pattern.indexOf(' ')) };
}
function generate_job_header(header: parsed_job_header, job_no: number): string {
    if (typeof header.job_header === 'string')
        return header.job_header;
    else {
        let jobname_suffix = '';
        while (job_no > 0) {
            let digit = job_no % translation_table.length;
            jobname_suffix = translation_table.charAt(digit) + jobname_suffix;
            job_no = job_no / translation_table.length | 0;
        }
        if (jobname_suffix.length > header.job_header.repl_count)
            jobname_suffix = jobname_suffix.slice(jobname_suffix.length - header.job_header.repl_count);
        else if (jobname_suffix.length < header.job_header.repl_count)
            jobname_suffix = jobname_suffix.padStart(header.job_header.repl_count, '0');

        return header.job_header.prefix + jobname_suffix + header.job_header.suffix;
    }

}

function generate_jcl(job_no: number, jobcard: parsed_job_header, dataset_name: string): string {
    return [
        generate_job_header(jobcard, job_no),
        "//AMATERSE EXEC PGM=AMATERSE,PARM=SPACK",
        "//SYSPRINT DD DUMMY",
        "//SYSIN    DD DUMMY",
        "//SYSUT1   DD DISP=SHR,DSN=" + dataset_name,
        "//SYSUT2   DD DISP=(,PASS),DSN=&&TERSED,SPACE=(CYL,(10,10))",
        "//*",
        "//PRINTIT  EXEC PGM=IEBGENER",
        "//SYSPRINT DD DUMMY",
        "//SYSIN    DD DUMMY",
        "//SYSUT1   DD DISP=OLD,DSN=&&TERSED",
        "//SYSUT2   DD SYSOUT=*"
    ].join(EOL)
}

function extract_job_name(jcl: string): string {
    return jcl.slice(2, jcl.indexOf(' '));
}

async function submit_jobs(client: job_client, jobcard: parsed_job_header, job_list: job_detail[], progress: stage_progress_reporter): Promise<submitted_job[]> {
    let id = 0;

    let result: submitted_job[] = [];

    for (const e of job_list) {
        const jcl = generate_jcl(id++, jobcard, e.dsn);
        const jobname = extract_job_name(jcl);

        const jobid = await client.submit_jcl(jcl);

        result.push({ jobname: jobname, jobid: jobid, details: e, downloaded: false });
        progress.stage_completed();
    }
    return result;
}

function fix_path(path: string): string {
    path = path.replace(/\\/g, '/');
    while (path.endsWith('/')) // no lastIndexOfNot?
        path = path.slice(0, path.length - 1);
    return path;
}

function getWasmRuntimeArgs(): Array<string> {
    const v8Version = process && process.versions && process.versions.v8 || "1.0";
    const v8Major = +v8Version.split(".")[0];
    if (v8Major >= 10)
        return [];
    else
        return [
            '--experimental-wasm-eh'
        ];
}

function unterse(in_file: string, out_dir: string): Promise<void> {
    return new Promise<void>((resolve, reject) => {
        const unpacker = fork(
            path.join(__dirname, '..', 'bin', 'terse'),
            [
                "--op",
                "unpack",
                "--overwrite",
                "--copy-if-symlink-fails",
                "-i",
                in_file,
                "-o",
                out_dir
            ],
            {
                execArgv: getWasmRuntimeArgs(),
                stdio: ['ignore', 'ignore', 'pipe', 'ipc']
            }
        );
        unpacker.stderr.on('data', (chunk) => {
            console.log(chunk.toString());
        });
        unpacker.on('exit', (code, signal) => {
            if (signal)
                reject("Signal received from unterse: " + signal);
            if (code !== 0)
                reject("Unterse ended with error code: " + code);

            resolve();
        })
    });
}

const ibm1148_with_crlf_replacement = [
    '\u0000',
    '\u0001',
    '\u0002',
    '\u0003',
    '\u009C',
    '\u0009',
    '\u0086',
    '\u007F',
    '\u0097',
    '\u008D',
    '\u008E',
    '\u000B',
    '\u000C',
    '\ue00D',
    '\u000E',
    '\u000F',
    '\u0010',
    '\u0011',
    '\u0012',
    '\u0013',
    '\u009D',
    '\ue025',
    '\u0008',
    '\u0087',
    '\u0018',
    '\u0019',
    '\u0092',
    '\u008F',
    '\u001C',
    '\u001D',
    '\u001E',
    '\u001F',
    '\u0080',
    '\u0081',
    '\u0082',
    '\u0083',
    '\u0084',
    '\u000A',
    '\u0017',
    '\u001B',
    '\u0088',
    '\u0089',
    '\u008A',
    '\u008B',
    '\u008C',
    '\u0005',
    '\u0006',
    '\u0007',
    '\u0090',
    '\u0091',
    '\u0016',
    '\u0093',
    '\u0094',
    '\u0095',
    '\u0096',
    '\u0004',
    '\u0098',
    '\u0099',
    '\u009A',
    '\u009B',
    '\u0014',
    '\u0015',
    '\u009E',
    '\u001A',
    '\u0020',
    '\u00A0',
    '\u00E2',
    '\u00E4',
    '\u00E0',
    '\u00E1',
    '\u00E3',
    '\u00E5',
    '\u00E7',
    '\u00F1',
    '\u005B',
    '\u002E',
    '\u003C',
    '\u0028',
    '\u002B',
    '\u0021',
    '\u0026',
    '\u00E9',
    '\u00EA',
    '\u00EB',
    '\u00E8',
    '\u00ED',
    '\u00EE',
    '\u00EF',
    '\u00EC',
    '\u00DF',
    '\u005D',
    '\u0024',
    '\u002A',
    '\u0029',
    '\u003B',
    '\u005E',
    '\u002D',
    '\u002F',
    '\u00C2',
    '\u00C4',
    '\u00C0',
    '\u00C1',
    '\u00C3',
    '\u00C5',
    '\u00C7',
    '\u00D1',
    '\u00A6',
    '\u002C',
    '\u0025',
    '\u005F',
    '\u003E',
    '\u003F',
    '\u00F8',
    '\u00C9',
    '\u00CA',
    '\u00CB',
    '\u00C8',
    '\u00CD',
    '\u00CE',
    '\u00CF',
    '\u00CC',
    '\u0060',
    '\u003A',
    '\u0023',
    '\u0040',
    '\u0027',
    '\u003D',
    '\u0022',
    '\u00D8',
    '\u0061',
    '\u0062',
    '\u0063',
    '\u0064',
    '\u0065',
    '\u0066',
    '\u0067',
    '\u0068',
    '\u0069',
    '\u00AB',
    '\u00BB',
    '\u00F0',
    '\u00FD',
    '\u00FE',
    '\u00B1',
    '\u00B0',
    '\u006A',
    '\u006B',
    '\u006C',
    '\u006D',
    '\u006E',
    '\u006F',
    '\u0070',
    '\u0071',
    '\u0072',
    '\u00AA',
    '\u00BA',
    '\u00E6',
    '\u00B8',
    '\u00C6',
    '\u20AC',
    '\u00B5',
    '\u007E',
    '\u0073',
    '\u0074',
    '\u0075',
    '\u0076',
    '\u0077',
    '\u0078',
    '\u0079',
    '\u007A',
    '\u00A1',
    '\u00BF',
    '\u00D0',
    '\u00DD',
    '\u00DE',
    '\u00AE',
    '\u00A2',
    '\u00A3',
    '\u00A5',
    '\u00B7',
    '\u00A9',
    '\u00A7',
    '\u00B6',
    '\u00BC',
    '\u00BD',
    '\u00BE',
    '\u00AC',
    '\u007C',
    '\u00AF',
    '\u00A8',
    '\u00B4',
    '\u00D7',
    '\u007B',
    '\u0041',
    '\u0042',
    '\u0043',
    '\u0044',
    '\u0045',
    '\u0046',
    '\u0047',
    '\u0048',
    '\u0049',
    '\u00AD',
    '\u00F4',
    '\u00F6',
    '\u00F2',
    '\u00F3',
    '\u00F5',
    '\u007D',
    '\u004A',
    '\u004B',
    '\u004C',
    '\u004D',
    '\u004E',
    '\u004F',
    '\u0050',
    '\u0051',
    '\u0052',
    '\u00B9',
    '\u00FB',
    '\u00FC',
    '\u00F9',
    '\u00FA',
    '\u00FF',
    '\u005C',
    '\u00F7',
    '\u0053',
    '\u0054',
    '\u0055',
    '\u0056',
    '\u0057',
    '\u0058',
    '\u0059',
    '\u005A',
    '\u00B2',
    '\u00D4',
    '\u00D6',
    '\u00D2',
    '\u00D3',
    '\u00D5',
    '\u0030',
    '\u0031',
    '\u0032',
    '\u0033',
    '\u0034',
    '\u0035',
    '\u0036',
    '\u0037',
    '\u0038',
    '\u0039',
    '\u00B3',
    '\u00DB',
    '\u00DC',
    '\u00D9',
    '\u00DA',
    '\u009F',
];

function convert_buffer(buffer: Buffer, lrecl: number) {
    let result = '';
    let i = 0;
    for (const v of buffer) {
        result += ibm1148_with_crlf_replacement[v];
        if (i % lrecl === lrecl - 1)
            result += EOL;
        ++i;
    }
    return result;
}

async function download_job_and_process(client: job_client, file_info: job_description, job: submitted_job, progress: stage_progress_reporter): Promise<{ unpacker: Promise<void> }> {
    const { rc, spool_files } = file_info.get_detail_info();
    if (rc !== 0)
        throw Error("Job failed: " + job.jobname + "/" + job.jobid);

    await new Promise<void>((resolve, reject) => {
        mkdir(path.dirname(job.details.dirs[0]), { recursive: true }, (err) => {
            if (err)
                reject(err);
            else
                resolve();
        });
    });
    const tmp_file = fix_path(job.details.dirs[0] + ".download.tmpfile");

    await client.download(tmp_file, job.jobid, spool_files);
    progress.stage_completed();

    job.downloaded = true;

    let all_dirs: Promise<void>[] = [];

    for (const dir__ of job.details.dirs) {
        const target_dir = fix_path(dir__);
        all_dirs.push((async () => {
            await fsp.mkdir(target_dir, { recursive: true });
            await unterse(tmp_file, target_dir);
            progress.stage_completed();
            const files = await fsp.readdir(target_dir, { withFileTypes: true });
            for (const file of files) {
                if (!file.isFile() || file.isSymbolicLink())
                    continue;
                const filePath = path.join(target_dir, file.name);
                await fsp.writeFile(filePath, convert_buffer(await fsp.readFile(filePath), 80), "utf-8");
            }
            progress.stage_completed();
        })());
    }
    return {
        unpacker: Promise.all(all_dirs).then(_ => { }).finally(() => { rmSync(tmp_file); })
    };
}

export async function download_copy_books_with_client(client: job_client,
    job_list: job_detail[],
    jobcard_pattern: string,
    progress: stage_progress_reporter): Promise<{ failed: number; total: number; }> {

    try {
        const jobcard = prepare_job_header(jobcard_pattern);
        const jobs = await submit_jobs(client, jobcard, job_list, progress);
        const jobs_map: {
            [key: string]: submitted_job
        } = Object.assign({}, ...jobs.map(x => ({ [x.jobname + "." + x.jobid]: x })));

        await client.set_list_mask(jobcard.job_mask);

        let wait = 0;
        let result = { failed: 0, total: 0 };
        while (jobs.some(x => !x.downloaded)) {
            const list = (await client.list()).map(x => {
                const j = jobs_map[x.jobname + "." + x.id];
                return { file_info: x, job: j && !j.downloaded ? j : null };
            }).filter(x => !!x.job);

            for (const l of list) {
                l.job.unpacking = (await download_job_and_process(client, l.file_info, l.job, progress)).unpacker
                    .then(_ => { result.total++; })
                    .catch(_ => { result.total++; result.failed++; });
            }

            if (list.length === 0) {
                if (wait < 30)
                    wait += 1;
                await new Promise((resolve) => setTimeout(resolve, wait * 1000))
            }
            else
                wait = 0;
        }

        await Promise.all(jobs.map(x => x.unpacking));

        return result;
    }
    finally {
        client.close();
    }
}

function ask_user(prompt: string, password: boolean, default_value: string = ''): Promise<string> {
    var input = vscode.window.createInputBox();
    return new Promise<string>((resolve, reject) => {
        input.prompt = prompt;
        input.password = password;
        input.value = default_value || '';
        input.onDidHide(() => reject("Action was cancelled"));
        input.onDidAccept(() => resolve(input.value));
        input.show();
    }).finally(() => { input.dispose(); });
}

async function gather_available_configs() {
    const available_configs = (await Promise.all(vscode.workspace.workspaceFolders.map(x => {
        return new Promise<{ workspace: vscode.WorkspaceFolder, config: any }>((resolve) => {
            vscode.workspace.openTextDocument(vscode.Uri.joinPath(x.uri, ".hlasmplugin", "proc_grps.json")).then((doc) => resolve({ workspace: x, config: JSON.parse(doc.getText()) }), _ => resolve(null))
        })
    }))).filter(x => !!x);

    // because VSCode does not expose the service???
    const var_replacer = (workspace: vscode.WorkspaceFolder) => {
        const config = vscode.workspace.getConfiguration(undefined, workspace);
        const replacer = (obj: any): any => {
            if (typeof obj === 'object') {
                for (const x in obj)
                    obj[x] = replacer(obj[x]);
            }
            else if (typeof obj === 'string') {
                while (true) {
                    const match = /\$\{config:([^}]+)\}|\$\{(workspaceFolder)\}/.exec(obj);
                    if (!match) break;

                    const replacement = match[1] ? '' + config.get(match[1]) : workspace.uri;
                    obj = obj.slice(0, match.index) + replacement + obj.slice(match.index + match[0].length);
                }
            }
            return obj;
        };
        return replacer;
    }

    return available_configs.map(x => { return { workspace: x.workspace, config: var_replacer(x.workspace)(x.config) } });
}

async function gather_download_list() {
    const available_configs = await gather_available_configs();

    const guess_dsn_regex = /(?:.*[\\/])?((?:[A-Za-z0-9@#$]{1,8}\.)+(?:[A-Za-z0-9@#$]{1,8}))[\\/]*/;
    const collected_dsn_and_path = available_configs.map((x) => {
        try {
            const dirs: { dsn: string, path: string }[] = [];

            for (const pg of x.config.pgroups) {
                for (const l of pg.libs) {
                    let d = '';
                    if (typeof l === 'string')
                        d = l;
                    else if (typeof l.path === 'string')
                        d = l.path;
                    if (d.length > 0) {
                        const dsn = guess_dsn_regex.exec(d);
                        if (dsn) {
                            if (d.startsWith("~")) {
                                dirs.push({ dsn: dsn[1], path: fix_path(path.join(homedir(), /~[\\/]/.test(d) ? d.slice(2) : d.slice(1))) });
                            }
                            else if (/^[A-Za-z][A-Za-z0-9+.-]+:/.test(d)) { // url (and not windows path)
                                const uri = vscode.Uri.parse(d);
                                if (uri.scheme === 'file')
                                    dirs.push({ dsn: dsn[1], path: fix_path(uri.fsPath) });
                            }
                            else { // path
                                const uri = path.isAbsolute(d) ? vscode.Uri.file(d) : vscode.Uri.joinPath(x.workspace.uri, d);
                                if (uri.scheme === 'file')
                                    dirs.push({ dsn: dsn[1], path: fix_path(uri.fsPath) });
                            }
                        }
                    }
                }
            }
            return { dirs: dirs };
        }
        catch { return null; }
    }).filter(x => !!x && x.dirs.length > 0).reduce((prev: { [key: string]: string[] }, current) => {
        for (const d of current.dirs) {
            if (d.dsn in prev)
                prev[d.dsn].push(d.path);
            else
                prev[d.dsn] = [d.path];
        }
        return prev;
    }, {});

    const things_to_download: job_detail[] = [];
    for (const key in collected_dsn_and_path)
        things_to_download.push({ dsn: key, dirs: [... new Set<string>(collected_dsn_and_path[key])] });

    return things_to_download;
}

class connection_info { host: string; port: number | undefined; user: string; password: string; host_input: string; }

async function gather_connection_info_from_zowe(zowe: vscode.Extension<any>, profile_name: string): Promise<connection_info> {
    if (!zowe.isActive)
        await zowe.activate();
    if (!zowe.isActive)
        throw Error("Unable to activate ZOWE Explorer extension");
    const zoweExplorerApi = zowe?.exports;
    await zoweExplorerApi
        .getExplorerExtenderApi()
        .getProfilesCache()
        .refresh(zoweExplorerApi);
    const loadedProfile = zoweExplorerApi
        .getExplorerExtenderApi()
        .getProfilesCache()
        .loadNamedProfile(profile_name);

    return { host: loadedProfile.profile.host, port: loadedProfile.profile.port, user: loadedProfile.profile.user, password: loadedProfile.profile.password, host_input: '@' + profile_name };
}

async function gather_connection_info(last_input: download_input_memento): Promise<connection_info> {
    const zowe = vscode.extensions.getExtension("Zowe.vscode-extension-for-zowe");

    const host_input = await ask_user(zowe ? "host[:port] or @zowe-profile-name" : "host[:port]", false, !zowe && last_input.host.startsWith('@') ? '' : last_input.host);
    const host_port = host_input.split(':');
    if (host_port.length < 1 || host_port.length > 2)
        throw Error("Invalid hostname or port");

    const host = host_port[0];
    const port = host_port.length > 1 ? +host_port[1] : undefined;
    if (zowe && port === undefined && host.startsWith('@'))
        return await gather_connection_info_from_zowe(zowe, host.slice(1));

    const user = await ask_user("user name", false, last_input.user);
    const password = await ask_user("password", true);
    return { host: host, port: port, user: user, password: password, host_input: host_input };
}

class download_input_memento {
    host: string = '';
    user: string = '';
    jobcard: string = undefined;
}

const memento_key = "hlasm.download_copy_books";

function get_last_run_config(context: vscode.ExtensionContext) {
    let last_run = context.globalState.get(memento_key, new download_input_memento());
    last_run.host = '' + (last_run.host || '');
    last_run.user = '' + (last_run.user || '');
    last_run.jobcard = '' + (last_run.jobcard || '');
    return last_run;
}

async function check_for_existing_files(jobs: job_detail[]) {
    const unique_dirs = new Set<string>();
    jobs.forEach(x => x.dirs.forEach(y => unique_dirs.add(y)));

    const nonempty_dirs = new Set<string>();

    for (const x of unique_dirs) {
        try {
            const dirIter = await fsp.opendir(x);
            const { value, done } = await dirIter[Symbol.asyncIterator]().next();
            if (!done) {
                nonempty_dirs.add(x);
                await dirIter.close(); // async iterator is closed automatically when the last entry is produced
            }
        }
        catch (e) {
            if (e.code !== 'ENOENT') throw e;
        }
    }

    return nonempty_dirs;
}

interface stage_progress_reporter {
    stage_completed(): void;
}

class progress_reporter implements stage_progress_reporter {
    constructor(private p: vscode.Progress<{ message?: string; increment?: number }>, private stages: number) { }
    stage_completed(): void {
        this.p.report({ increment: 100 / this.stages });
    }
}

export async function download_copy_books(context: vscode.ExtensionContext) {
    const last_input = get_last_run_config(context);
    const { host, port, user, password, host_input } = await gather_connection_info(last_input);

    const jobcard_pattern = await ask_user("Enter jobcard pattern (? will be substituted)", false, last_input.jobcard || "//" + user.slice(0, 7).padEnd(8, '?').toUpperCase() + " JOB ACCTNO");

    await context.globalState.update(memento_key, { host: host_input, user: user, jobcard: jobcard_pattern });

    const things_to_download = await gather_download_list();

    const dirs_exists = await check_for_existing_files(things_to_download);
    if (dirs_exists.size > 0) {
        const overwrite = "Overwrite";
        const what_to_do = await vscode.window.showQuickPick([overwrite, "Cancel"], { title: "Some of the directories (" + dirs_exists.size + ") exist and are not empty." });
        if (what_to_do !== overwrite)
            return;

        // TODO:
        // for (const d of dirs_exists) {
        //     const files = await fsp.readdir(d, { withFileTypes: true });
        //     for (const f of files) {
        //         if (f.isFile() || f.isSymbolicLink())
        //             await fsp.unlink(path.join(d, f.name));
        //     }
        // }
    }

    vscode.window.withProgress({ title: "Downloading copybooks", location: vscode.ProgressLocation.Notification }, async (p, t) => {
        const result = await download_copy_books_with_client(
            await basic_ftp_job_client({
                host: host,
                user: user,
                password: password,
                port: port,
            }),
            things_to_download,
            jobcard_pattern,
            new progress_reporter(p, things_to_download.length * 4));

        if (result.failed)
            vscode.window.showErrorMessage(result.failed + " jobs out of " + result.total + " failed");
        else
            vscode.window.showInformationMessage("All jobs (" + result.total + ") completed successfully");
    });
}
