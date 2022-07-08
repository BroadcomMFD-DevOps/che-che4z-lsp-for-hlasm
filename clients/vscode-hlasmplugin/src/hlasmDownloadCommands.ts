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
import { mkdir, rmSync } from 'fs';

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

async function submit_jobs(client: job_client, jobcard: parsed_job_header, job_list: job_detail[]): Promise<submitted_job[]> {
    let id = 0;

    let result: submitted_job[] = [];

    for (const e of job_list) {
        const jcl = generate_jcl(id++, jobcard, e.dsn);
        const jobname = extract_job_name(jcl);

        const jobid = await client.submit_jcl(jcl);

        result.push({ jobname: jobname, jobid: jobid, details: e, downloaded: false });
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

async function download_job_and_process(client: job_client, file_info: job_description, job: submitted_job): Promise<{ unpacker: Promise<void> }> {
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

    job.downloaded = true;

    let all_dirs: Promise<void>[] = [];

    for (const dir__ of job.details.dirs) {
        const target_dir = fix_path(dir__);
        all_dirs.push(new Promise<void>((resolve, reject) => {
            mkdir(target_dir, { recursive: true }, (err) => {
                if (err) {
                    reject(err);
                    return;
                }

                const unpacker = fork(
                    path.join(__dirname, '..', 'bin', 'terse'),
                    [
                        "--op",
                        "unpack",
                        "-i",
                        tmp_file,
                        "-o",
                        target_dir
                    ],
                    {
                        execArgv: getWasmRuntimeArgs(),
                        stdio: ['ignore', 'ignore', 'pipe', 'ipc']
                    }
                );
                unpacker.stderr.on('data', (chunk) => {
                    console.log(chunk)
                });
                unpacker.on('exit', (code, signal) => {
                    if (signal)
                        reject("Signal received from unterse: " + signal);
                    if (code !== 0)
                        reject("Unterse ended with error code: " + code);
                    resolve();
                })
            })
        }));
    }
    return {
        unpacker: Promise.all(all_dirs).then(_ => { }).finally(() => { rmSync(tmp_file); })
    };
}

export async function download_copy_books_with_client(client: job_client,
    job_list: job_detail[],
    jobcard_pattern: string): Promise<{ failed: number; total: number; }> {

    try {
        const jobcard = prepare_job_header(jobcard_pattern);
        const jobs = await submit_jobs(client, jobcard, job_list);
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

            for (const l of list)
                l.job.unpacking = (await download_job_and_process(client, l.file_info, l.job)).unpacker
                    .then(_ => { result.total++; })
                    .catch(_ => { result.total++; result.failed++; });

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

function ask_user(prompt: string, password: boolean): Promise<string> {
    var input = vscode.window.createInputBox();
    return new Promise<string>((resolve, reject) => {
        input.prompt = prompt;
        input.password = password;
        input.onDidHide(() => reject("Action was cancelled"));
        input.onDidAccept(() => resolve(input.value));
        input.show();
    }).finally(() => { input.dispose(); });
}

async function gather_download_list() {
    const available_configs = (await Promise.all(vscode.workspace.workspaceFolders.map(x => {
        return new Promise<{ workspace: vscode.Uri, config: any }>((resolve) => {
            vscode.workspace.openTextDocument(vscode.Uri.joinPath(x.uri, ".hlasmplugin", "proc_grps.json")).then((doc) => resolve({ workspace: x.uri, config: JSON.parse(doc.getText()) }), _ => resolve(null))
        })
    }))).filter(x => !!x);

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
                            else {
                                const target_uri = vscode.Uri.joinPath(x.workspace, d);
                                if (target_uri.scheme === 'file')
                                    dirs.push({ dsn: dsn[1], path: fix_path(target_uri.fsPath) });
                            }
                        }
                    }
                }
            }
            return { workspace: x.workspace, dirs: dirs };
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

async function gather_connection_info(): Promise<{ host: string, port: number | undefined, user: string, password: string }> {
    const host_port = (await ask_user("host[:port] or @zowe-profile-name", false)).split(':');
    if (host_port.length < 1 || host_port.length > 2)
        throw Error("Invalid hostname or port");
    const host = host_port[0];
    const port = host_port.length > 1 ? +host_port[1] : undefined;
    if (port === undefined && host.startsWith('@')) {
        const zowe = vscode.extensions.getExtension("Zowe.vscode-extension-for-zowe");
        if (!zowe)
            throw Error("ZOWE Explorer extension not available");
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
            .loadNamedProfile(host.slice(1));

        return { host: loadedProfile.profile.host, port: loadedProfile.profile.port, user: loadedProfile.profile.user, password: loadedProfile.profile.password };
    }
    const user = await ask_user("user name", false);
    const password = await ask_user("password", true);
    return { host: host, port: port, user: user, password: password };
}

export async function download_copy_books() {
    const { host, port, user, password } = await gather_connection_info();

    const jobcard_pattern = await ask_user("//JOBNAME? JOB ACCTNO", false);

    const things_to_download = await gather_download_list();

    const result = await download_copy_books_with_client(
        await basic_ftp_job_client({
            host: host,
            user: user,
            password: password,
            port: port,
        }),
        things_to_download,
        jobcard_pattern);

    if (result.failed)
        vscode.window.showErrorMessage(result.failed + " jobs out of " + result.total + " failed");
    else
        vscode.window.showInformationMessage("All jobs (" + result.total + ") completed successfully");
}