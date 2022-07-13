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
import { EOL, homedir } from 'os';
import path = require('node:path');
import { promises as fsp } from "fs";

export type job_id = string;
export interface job_description {
    jobname: string;
    id: job_id;
    details: string;
}

function get_job_detail_info(job: job_description): { rc: number; spool_files: number } | undefined {
    const parsed = /^.*RC=(\d+)\s+(\d+) spool file/.exec(job.details);
    if (!parsed)
        return undefined;
    else
        return { rc: +parsed[1], spool_files: +parsed[2] };
}
export interface job_client {
    submit_jcl(jcl: string): Promise<job_id>;
    set_list_mask(mask: string): Promise<void>;
    list(): Promise<job_description[]>;
    download(target: Writable | string, id: job_id, spool_file: number): Promise<void>;
    close(): void;
}

async function basic_ftp_job_client(connection: {
    host: string;
    port?: number;
    user: string;
    password: string;
    secure: connection_security_level
}): Promise<job_client> {
    const client = new Client();
    client.parseList = (rawList: string): FileInfo[] => {
        return rawList.split(/\r?\n/).slice(1).filter(x => !/^\s*$/.test(x)).map((value) => new FileInfo(value));
    };
    await client.access({
        host: connection.host,
        user: connection.user,
        password: connection.password,
        port: connection.port,
        secure: connection.secure !== connection_security_level.unsecure,
        secureOptions: connection.secure === connection_security_level.unsecure ? undefined : { rejectUnauthorized: connection.secure !== connection_security_level.rejectUnauthorized }
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
            const jobid = /^.*as ([Jj](?:[Oo][Bb])?\d+)/.exec(job_upload.message);
            if (!jobid)
                throw Error("Unable to extract the job id");
            return jobid[1];
        },
        async set_list_mask(mask: string): Promise<void> {
            await checked_command("SITE JESJOBNAME=" + mask);
            await checked_command("SITE JESSTATUS=OUTPUT");
        },
        async list(): Promise<job_description[]> {
            try {
                await switch_text();
                return (await client.list()).map((x: FileInfo): job_description => {
                    const parsed_line = /(\S+)\s+(\S+)\s+(.*)/.exec(x.name);
                    if (!parsed_line)
                        throw Error("Unable to parse the job list");
                    return { jobname: parsed_line[1], id: parsed_line[2], details: parsed_line[3] }
                });
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


export interface job_detail {
    dsn: string;
    dirs: string[];
}

interface submitted_job {
    jobname: string;
    jobid: string;
    details: job_detail;
    downloaded: boolean;
    unpacking?: Promise<void>;
}

interface parsed_job_header {
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

async function submit_jobs(client: job_client, jobcard: parsed_job_header, job_list: job_detail[], progress: stage_progress_reporter, check_cancel: () => void): Promise<submitted_job[]> {
    let id = 0;

    let result: submitted_job[] = [];

    for (const e of job_list) {
        check_cancel();
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
    while (path.endsWith('/')) // no lastIndexOfNot or trimEnd(x)?
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

function unterse(out_dir: string): { process: Promise<void>, input: Writable } {
    const unpacker = fork(
        path.join(__dirname, '..', 'bin', 'terse'),
        [
            "--op",
            "unpack",
            "--overwrite",
            "--copy-if-symlink-fails",
            "-o",
            out_dir
        ],
        {
            execArgv: getWasmRuntimeArgs(),
            stdio: ['pipe', 'ignore', 'pipe', 'ipc']
        }
    );
    const promise = new Promise<void>((resolve, reject) => {
        unpacker.stderr!.on('data', (chunk) => {
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
    return { process: promise, input: unpacker.stdin! };
}

const ibm1148_with_crlf_replacement = [
    Buffer.from('\u0000'),
    Buffer.from('\u0001'),
    Buffer.from('\u0002'),
    Buffer.from('\u0003'),
    Buffer.from('\u009C'),
    Buffer.from('\u0009'),
    Buffer.from('\u0086'),
    Buffer.from('\u007F'),
    Buffer.from('\u0097'),
    Buffer.from('\u008D'),
    Buffer.from('\u008E'),
    Buffer.from('\u000B'),
    Buffer.from('\u000C'),
    Buffer.from('\ue00D'),
    Buffer.from('\u000E'),
    Buffer.from('\u000F'),
    Buffer.from('\u0010'),
    Buffer.from('\u0011'),
    Buffer.from('\u0012'),
    Buffer.from('\u0013'),
    Buffer.from('\u009D'),
    Buffer.from('\ue025'),
    Buffer.from('\u0008'),
    Buffer.from('\u0087'),
    Buffer.from('\u0018'),
    Buffer.from('\u0019'),
    Buffer.from('\u0092'),
    Buffer.from('\u008F'),
    Buffer.from('\u001C'),
    Buffer.from('\u001D'),
    Buffer.from('\u001E'),
    Buffer.from('\u001F'),
    Buffer.from('\u0080'),
    Buffer.from('\u0081'),
    Buffer.from('\u0082'),
    Buffer.from('\u0083'),
    Buffer.from('\u0084'),
    Buffer.from('\u000A'),
    Buffer.from('\u0017'),
    Buffer.from('\u001B'),
    Buffer.from('\u0088'),
    Buffer.from('\u0089'),
    Buffer.from('\u008A'),
    Buffer.from('\u008B'),
    Buffer.from('\u008C'),
    Buffer.from('\u0005'),
    Buffer.from('\u0006'),
    Buffer.from('\u0007'),
    Buffer.from('\u0090'),
    Buffer.from('\u0091'),
    Buffer.from('\u0016'),
    Buffer.from('\u0093'),
    Buffer.from('\u0094'),
    Buffer.from('\u0095'),
    Buffer.from('\u0096'),
    Buffer.from('\u0004'),
    Buffer.from('\u0098'),
    Buffer.from('\u0099'),
    Buffer.from('\u009A'),
    Buffer.from('\u009B'),
    Buffer.from('\u0014'),
    Buffer.from('\u0015'),
    Buffer.from('\u009E'),
    Buffer.from('\u001A'),
    Buffer.from('\u0020'),
    Buffer.from('\u00A0'),
    Buffer.from('\u00E2'),
    Buffer.from('\u00E4'),
    Buffer.from('\u00E0'),
    Buffer.from('\u00E1'),
    Buffer.from('\u00E3'),
    Buffer.from('\u00E5'),
    Buffer.from('\u00E7'),
    Buffer.from('\u00F1'),
    Buffer.from('\u005B'),
    Buffer.from('\u002E'),
    Buffer.from('\u003C'),
    Buffer.from('\u0028'),
    Buffer.from('\u002B'),
    Buffer.from('\u0021'),
    Buffer.from('\u0026'),
    Buffer.from('\u00E9'),
    Buffer.from('\u00EA'),
    Buffer.from('\u00EB'),
    Buffer.from('\u00E8'),
    Buffer.from('\u00ED'),
    Buffer.from('\u00EE'),
    Buffer.from('\u00EF'),
    Buffer.from('\u00EC'),
    Buffer.from('\u00DF'),
    Buffer.from('\u005D'),
    Buffer.from('\u0024'),
    Buffer.from('\u002A'),
    Buffer.from('\u0029'),
    Buffer.from('\u003B'),
    Buffer.from('\u005E'),
    Buffer.from('\u002D'),
    Buffer.from('\u002F'),
    Buffer.from('\u00C2'),
    Buffer.from('\u00C4'),
    Buffer.from('\u00C0'),
    Buffer.from('\u00C1'),
    Buffer.from('\u00C3'),
    Buffer.from('\u00C5'),
    Buffer.from('\u00C7'),
    Buffer.from('\u00D1'),
    Buffer.from('\u00A6'),
    Buffer.from('\u002C'),
    Buffer.from('\u0025'),
    Buffer.from('\u005F'),
    Buffer.from('\u003E'),
    Buffer.from('\u003F'),
    Buffer.from('\u00F8'),
    Buffer.from('\u00C9'),
    Buffer.from('\u00CA'),
    Buffer.from('\u00CB'),
    Buffer.from('\u00C8'),
    Buffer.from('\u00CD'),
    Buffer.from('\u00CE'),
    Buffer.from('\u00CF'),
    Buffer.from('\u00CC'),
    Buffer.from('\u0060'),
    Buffer.from('\u003A'),
    Buffer.from('\u0023'),
    Buffer.from('\u0040'),
    Buffer.from('\u0027'),
    Buffer.from('\u003D'),
    Buffer.from('\u0022'),
    Buffer.from('\u00D8'),
    Buffer.from('\u0061'),
    Buffer.from('\u0062'),
    Buffer.from('\u0063'),
    Buffer.from('\u0064'),
    Buffer.from('\u0065'),
    Buffer.from('\u0066'),
    Buffer.from('\u0067'),
    Buffer.from('\u0068'),
    Buffer.from('\u0069'),
    Buffer.from('\u00AB'),
    Buffer.from('\u00BB'),
    Buffer.from('\u00F0'),
    Buffer.from('\u00FD'),
    Buffer.from('\u00FE'),
    Buffer.from('\u00B1'),
    Buffer.from('\u00B0'),
    Buffer.from('\u006A'),
    Buffer.from('\u006B'),
    Buffer.from('\u006C'),
    Buffer.from('\u006D'),
    Buffer.from('\u006E'),
    Buffer.from('\u006F'),
    Buffer.from('\u0070'),
    Buffer.from('\u0071'),
    Buffer.from('\u0072'),
    Buffer.from('\u00AA'),
    Buffer.from('\u00BA'),
    Buffer.from('\u00E6'),
    Buffer.from('\u00B8'),
    Buffer.from('\u00C6'),
    Buffer.from('\u20AC'),
    Buffer.from('\u00B5'),
    Buffer.from('\u007E'),
    Buffer.from('\u0073'),
    Buffer.from('\u0074'),
    Buffer.from('\u0075'),
    Buffer.from('\u0076'),
    Buffer.from('\u0077'),
    Buffer.from('\u0078'),
    Buffer.from('\u0079'),
    Buffer.from('\u007A'),
    Buffer.from('\u00A1'),
    Buffer.from('\u00BF'),
    Buffer.from('\u00D0'),
    Buffer.from('\u00DD'),
    Buffer.from('\u00DE'),
    Buffer.from('\u00AE'),
    Buffer.from('\u00A2'),
    Buffer.from('\u00A3'),
    Buffer.from('\u00A5'),
    Buffer.from('\u00B7'),
    Buffer.from('\u00A9'),
    Buffer.from('\u00A7'),
    Buffer.from('\u00B6'),
    Buffer.from('\u00BC'),
    Buffer.from('\u00BD'),
    Buffer.from('\u00BE'),
    Buffer.from('\u00AC'),
    Buffer.from('\u007C'),
    Buffer.from('\u00AF'),
    Buffer.from('\u00A8'),
    Buffer.from('\u00B4'),
    Buffer.from('\u00D7'),
    Buffer.from('\u007B'),
    Buffer.from('\u0041'),
    Buffer.from('\u0042'),
    Buffer.from('\u0043'),
    Buffer.from('\u0044'),
    Buffer.from('\u0045'),
    Buffer.from('\u0046'),
    Buffer.from('\u0047'),
    Buffer.from('\u0048'),
    Buffer.from('\u0049'),
    Buffer.from('\u00AD'),
    Buffer.from('\u00F4'),
    Buffer.from('\u00F6'),
    Buffer.from('\u00F2'),
    Buffer.from('\u00F3'),
    Buffer.from('\u00F5'),
    Buffer.from('\u007D'),
    Buffer.from('\u004A'),
    Buffer.from('\u004B'),
    Buffer.from('\u004C'),
    Buffer.from('\u004D'),
    Buffer.from('\u004E'),
    Buffer.from('\u004F'),
    Buffer.from('\u0050'),
    Buffer.from('\u0051'),
    Buffer.from('\u0052'),
    Buffer.from('\u00B9'),
    Buffer.from('\u00FB'),
    Buffer.from('\u00FC'),
    Buffer.from('\u00F9'),
    Buffer.from('\u00FA'),
    Buffer.from('\u00FF'),
    Buffer.from('\u005C'),
    Buffer.from('\u00F7'),
    Buffer.from('\u0053'),
    Buffer.from('\u0054'),
    Buffer.from('\u0055'),
    Buffer.from('\u0056'),
    Buffer.from('\u0057'),
    Buffer.from('\u0058'),
    Buffer.from('\u0059'),
    Buffer.from('\u005A'),
    Buffer.from('\u00B2'),
    Buffer.from('\u00D4'),
    Buffer.from('\u00D6'),
    Buffer.from('\u00D2'),
    Buffer.from('\u00D3'),
    Buffer.from('\u00D5'),
    Buffer.from('\u0030'),
    Buffer.from('\u0031'),
    Buffer.from('\u0032'),
    Buffer.from('\u0033'),
    Buffer.from('\u0034'),
    Buffer.from('\u0035'),
    Buffer.from('\u0036'),
    Buffer.from('\u0037'),
    Buffer.from('\u0038'),
    Buffer.from('\u0039'),
    Buffer.from('\u00B3'),
    Buffer.from('\u00DB'),
    Buffer.from('\u00DC'),
    Buffer.from('\u00D9'),
    Buffer.from('\u00DA'),
    Buffer.from('\u009F'),
];

function convert_buffer(buffer: Buffer, lrecl: number) {
    const EOLBuffer = Buffer.from(EOL);
    const result = Buffer.allocUnsafe(2 * buffer.length + Math.floor((buffer.length + lrecl - 1) / lrecl) * EOLBuffer.length);
    let pos = 0;
    let i = 0;
    for (const v of buffer) {
        pos += ibm1148_with_crlf_replacement[v].copy(result, pos, 0);
        if (i % lrecl === lrecl - 1)
            pos += EOLBuffer.copy(result, pos);
        ++i;
    }
    return result.slice(0, pos);
}

async function translate_files(dir: string) {
    const files = await fsp.readdir(dir, { withFileTypes: true });
    for (const file of files) {
        if (!file.isFile() || file.isSymbolicLink())
            continue;
        const filePath = path.join(dir, file.name);
        await fsp.writeFile(filePath, convert_buffer(await fsp.readFile(filePath), 80), "utf-8");
    }
}

async function copy_directory(source: string, target: string) {
    await fsp.mkdir(target, { recursive: true });
    const files = await fsp.readdir(source, { withFileTypes: true });
    for (const file of files) {
        if (!file.isFile() || file.isSymbolicLink())
            continue;
        await fsp.copyFile(path.join(source, file.name), path.join(target, file.name));
    }
    for (const file of files) {
        if (!file.isSymbolicLink())
            continue;
        fsp.symlink(await fsp.readlink(path.join(source, file.name)), path.join(target, file.name));
    }
}

export interface io_ops {
    unterse: (out_dir: string) => { process: Promise<void>, input: Writable };
    translate_files: (dir: string) => Promise<void>;
    copy_directory: (source: string, target: string) => Promise<void>;
}

async function download_job_and_process(
    client: job_client,
    file_info: job_description,
    job: submitted_job,
    progress: stage_progress_reporter,
    io: io_ops): Promise<{ unpacker: Promise<void> }> {
    const job_detail = get_job_detail_info(file_info);
    if (!job_detail || job_detail.rc !== 0) {
        job.downloaded = true; // nothing we can do ...
        return {
            unpacker: Promise.reject(Error("Job failed: " + job.jobname + "/" + job.jobid))
        };
    }

    const first_dir = fix_path(job.details.dirs[0]);

    try {
        await fsp.mkdir(first_dir, { recursive: true });
        const { process, input } = io.unterse(first_dir);
        await client.download(input, job.jobid, job_detail.spool_files!);
        progress.stage_completed();
        job.downloaded = true;

        return {
            unpacker:
                (async () => {
                    await process;
                    progress.stage_completed();

                    await io.translate_files(first_dir);

                    progress.stage_completed();

                    for (const dir__ of job.details.dirs.slice(1)) {
                        await io.copy_directory(first_dir, fix_path(dir__));
                        progress.stage_completed();
                    }
                })()
        };
    }
    catch (e) {
        return {
            unpacker: Promise.reject(e)
        };
    }
}

export async function download_copy_books_with_client(client: job_client,
    job_list: job_detail[],
    jobcard_pattern: string,
    progress: stage_progress_reporter,
    io: io_ops,
    cancelled: () => boolean): Promise<{ failed: job_detail[]; total: number; }> {

    const check_cancel = () => {
        if (cancelled())
            throw Error("Cancel requested");
    };

    try {
        const jobcard = prepare_job_header(jobcard_pattern);
        const jobs = await submit_jobs(client, jobcard, job_list, progress, check_cancel);
        const jobs_map: {
            [key: string]: submitted_job
        } = Object.assign({}, ...jobs.map(x => ({ [x.jobname + "." + x.jobid]: x })));

        await client.set_list_mask(jobcard.job_mask);

        let wait = 0;
        let result = { failed: new Array<job_detail>(), total: 0 };
        while (jobs.some(x => !x.downloaded)) {
            check_cancel();

            const list = (await client.list()).map(x => {
                const j = jobs_map[x.jobname + "." + x.id];
                return { file_info: x, job: j && !j.downloaded ? j : null };
            }).filter(x => !!x.job);

            for (const l of list) {
                const job = l.job!;
                job.unpacking = (await download_job_and_process(client, l.file_info, job, progress, io)).unpacker
                    .then(_ => { result.total++; })
                    .catch(_ => { result.total++; result.failed.push(job.details); });
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

function pick_user<T>(title: string, options: { label: string, value: T }[]): Promise<T> {
    var input = vscode.window.createQuickPick();
    return new Promise<T>((resolve, reject) => {
        input.title = title;
        input.items = options.map(x => { return { label: x.label }; });
        input.canSelectMany = false;
        input.onDidHide(() => reject("Action was cancelled"));
        input.onDidAccept(() => resolve(options.find(x => x.label === input.selectedItems[0].label)!.value));
        input.show();
    }).finally(() => { input.dispose(); });
}

async function gather_available_configs() {
    if (vscode.workspace.workspaceFolders === undefined) return [];
    const available_configs = (await Promise.all(vscode.workspace.workspaceFolders.map(x => {
        return new Promise<{ workspace: vscode.WorkspaceFolder, config: any } | null>((resolve) => {
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

    return available_configs.map(x => { return { workspace: x!.workspace, config: var_replacer(x!.workspace)(x!.config) } });
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
        for (const d of current!.dirs) {
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

enum connection_security_level {
    "rejectUnauthorized",
    "acceptUnauthorized",
    "unsecure",
}

interface connection_info {
    host: string;
    port: number | undefined;
    user: string;
    password: string;
    host_input: string;
    secure: connection_security_level;
}

function gather_security_level_from_zowe(profile: any) {
    if (profile.secureFtp !== false) {
        if (profile.rejectUnauthorized !== false)
            return connection_security_level.rejectUnauthorized;
        else
            return connection_security_level.acceptUnauthorized;
    }
    else
        return connection_security_level.unsecure;
}

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

    return {
        host: loadedProfile.profile.host,
        port: loadedProfile.profile.port,
        user: loadedProfile.profile.user,
        password: loadedProfile.profile.password,
        host_input: '@' + profile_name,
        secure: gather_security_level_from_zowe(loadedProfile.profile)
    };
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
    const secure_level = await pick_user("Select security option", [
        { label: "Use TLS, reject unauthorized certificated", value: connection_security_level.rejectUnauthorized },
        { label: "Use TLS, accept unauthorized certificated", value: connection_security_level.acceptUnauthorized },
        { label: "Unsecured connection", value: connection_security_level.unsecure },
    ]);
    return { host: host, port: port, user: user, password: password, host_input: host_input, secure: secure_level };
}

interface download_input_memento {
    host: string;
    user: string;
    jobcard: string;
}

const memento_key = "hlasm.download_copy_books";

function get_last_run_config(context: vscode.ExtensionContext): download_input_memento {
    let last_run = context.globalState.get(memento_key, { host: '', user: '', jobcard: '' });
    return {
        host: '' + (last_run.host || ''),
        user: '' + (last_run.user || ''),
        jobcard: '' + (last_run.jobcard || ''),
    };
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

export interface stage_progress_reporter {
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
    const { host, port, user, password, host_input, secure } = await gather_connection_info(last_input);

    const jobcard_pattern = await ask_user("Enter jobcard pattern (? will be substituted)", false, last_input.jobcard || "//" + user.slice(0, 7).padEnd(8, '?').toUpperCase() + " JOB ACCTNO");

    await context.globalState.update(memento_key, { host: host_input, user: user, jobcard: jobcard_pattern });

    const things_to_download = await gather_download_list();

    const dirs_exists = await check_for_existing_files(things_to_download);
    if (dirs_exists.size > 0) {
        const overwrite = "Overwrite";
        const what_to_do = await vscode.window.showQuickPick([overwrite, "Cancel"], { title: "Some of the directories (" + dirs_exists.size + ") exist and are not empty." });
        if (what_to_do !== overwrite)
            return;

        for (const d of dirs_exists) {
            const files = await fsp.readdir(d, { withFileTypes: true });
            for (const f of files) {
                if (f.isFile() || f.isSymbolicLink())
                    await fsp.unlink(path.join(d, f.name));
            }
        }
    }

    vscode.window.withProgress({ title: "Downloading copybooks", location: vscode.ProgressLocation.Notification, cancellable: true }, async (p, t) => {
        const result = await download_copy_books_with_client(
            await basic_ftp_job_client({
                host: host,
                user: user,
                password: password,
                port: port,
                secure: secure
            }),
            things_to_download,
            jobcard_pattern,
            new progress_reporter(p, things_to_download.reduce((prev, cur) => { return prev + cur.dirs.length + 3 }, 0)),
            { unterse, translate_files, copy_directory },
            () => t.isCancellationRequested);

        if (result.failed.length > 0) // TODO: offer re-run?
            vscode.window.showErrorMessage(result.failed.length + " jobs out of " + result.total + " failed");
        else
            vscode.window.showInformationMessage("All jobs (" + result.total + ") completed successfully");
    });
}
