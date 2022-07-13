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
import * as assert from 'assert';
import { PassThrough, Writable } from 'stream';

import { download_copy_books_with_client, job_description } from '../../hlasmDownloadCommands';

suite('HLASM Download datasets', () => {
    const get_client = (list_responses: job_description[][]) => {
        return {
            set_list_mask_calls: new Array<string>(),
            list_calls: 0,
            jcls: new Array<string>(),
            download_requests: new Array<{ id: string, spool_file: number }>(),
            close_calls: 0,
            next_job_id: 0,

            close() { ++this.close_calls },
            async download(target: Writable, id: string, spool_file: number) {
                if (target instanceof Writable) {
                    this.download_requests.push({ id, spool_file });
                }
                else
                    assert.fail("Writable stream expected");
            },
            async list() {
                const to_return = this.list_calls++;
                if (to_return >= list_responses.length)
                    return [];
                else
                    return list_responses[to_return];
            },
            async set_list_mask(mask: string) {
                this.set_list_mask_calls.push(mask);
            },
            async submit_jcl(jcl: string) {
                this.jcls.push(jcl);
                return "JOBID" + this.next_job_id++;
            },
        }
    }

    const get_io_ops = () => {
        return {
            unterse_calls: new Array<string>(),
            translate_calls: new Array<string>(),
            copy_calls: new Array<{ source: string, target: string }>(),
            unterse(out_dir: string) {
                this.unterse_calls.push(out_dir);

                const process = Promise.resolve();
                const input = new PassThrough();
                return { process, input };
            },
            async translate_files(dir: string) {
                this.translate_calls.push(dir);
            },
            async copy_directory(source: string, target: string) {
                this.copy_calls.push({ source, target });
            },
        };
    }

    const get_stage_counter = () => {
        return {
            stages: 0,

            stage_completed() {
                ++this.stages;
            },
        };
    }

    test('Simple jobcard', async () => {
        const client = get_client([
            [],
            [{ jobname: "JOBNAME", id: "JOBID0", details: "RC=0000 3 spool files" }]
        ]);
        const io = get_io_ops();
        const stages = get_stage_counter();

        assert.deepEqual(await download_copy_books_with_client(
            client,
            [{ dsn: 'A.B', dirs: ['/dir1'] }],
            '//JOBNAME JOB 1',
            stages,
            io,
            () => false),
            { failed: [], total: 1 }
        );

        assert.equal(client.close_calls, 1);
        assert.deepEqual(client.download_requests, [{ id: "JOBID0", spool_file: 3 }]);
        assert.equal(client.jcls.length, 1);
        assert.ok(client.jcls[0].startsWith("//JOBNAME JOB 1"));
        assert.notEqual(client.jcls[0].indexOf("DSN=A.B"), -1);
        assert.equal(client.list_calls, 2);
        assert.deepEqual(client.set_list_mask_calls, ['JOBNAME']);
        assert.equal(io.copy_calls.length, 0);
        assert.deepEqual(io.translate_calls, ['/dir1']);
        assert.deepEqual(io.unterse_calls, ['/dir1']);
        assert.equal(stages.stages, 4);
    });

    test('Jobcard pattern', async () => {
        const client = get_client([
            [{ jobname: "JOBNAME0", id: "JOBID0", details: "RC=0000 3 spool files" }]
        ]);
        const io = get_io_ops();
        const stages = get_stage_counter();

        assert.deepEqual(await download_copy_books_with_client(
            client,
            [{ dsn: 'A.B', dirs: ['/dir1'] }],
            '//JOBNAME? JOB 1',
            stages,
            io,
            () => false),
            { failed: [], total: 1 }
        );

        assert.equal(client.close_calls, 1);
        assert.deepEqual(client.download_requests, [{ id: "JOBID0", spool_file: 3 }]);
        assert.equal(client.jcls.length, 1);
        assert.ok(client.jcls[0].startsWith("//JOBNAME0 JOB 1"));
        assert.notEqual(client.jcls[0].indexOf("DSN=A.B"), -1);
        assert.equal(client.list_calls, 1);
        assert.deepEqual(client.set_list_mask_calls, ['JOBNAME*']);
        assert.equal(io.copy_calls.length, 0);
        assert.deepEqual(io.translate_calls, ['/dir1']);
        assert.deepEqual(io.unterse_calls, ['/dir1']);
        assert.equal(stages.stages, 4);
    });

    test('Cancelled', async () => {
        const client = get_client([
            [{ jobname: "JOBNAME0", id: "JOBID0", details: "RC=0000 3 spool files" }]
        ]);
        const io = get_io_ops();
        const stages = get_stage_counter();

        try {
            await download_copy_books_with_client(
                client,
                [{ dsn: 'A.B', dirs: ['/dir1'] }],
                '//JOBNAME? JOB 1',
                stages,
                io,
                () => true);
            assert.fail();
        }
        catch (e) {
            assert.equal(e.message, "Cancel requested");
        }

        assert.equal(client.close_calls, 1);
        assert.deepEqual(client.download_requests, []);
        assert.equal(client.jcls.length, 0);
        assert.equal(client.list_calls, 0);
        assert.deepEqual(client.set_list_mask_calls, []);
        assert.equal(io.copy_calls.length, 0);
        assert.deepEqual(io.translate_calls, []);
        assert.deepEqual(io.unterse_calls, []);
        assert.equal(stages.stages, 0);
    });


    test('Multiple datasets', async () => {
        const client = get_client([
            [{ jobname: "JOBNAME0", id: "JOBID0", details: "RC=0000 3 spool files" }],
            [
                { jobname: "JOBNAME0", id: "JOBID0", details: "RC=0000 3 spool files" },
                { jobname: "JOBNAME1", id: "JOBID1", details: "RC=0000 6 spool files" }
            ]
        ]);
        const io = get_io_ops();
        const stages = get_stage_counter();

        assert.deepEqual(await download_copy_books_with_client(
            client,
            [
                { dsn: 'A.B', dirs: ['/dir1'] },
                { dsn: 'C.D', dirs: ['/dir2', '/dir3'] },
            ],
            '//JOBNAME? JOB 1',
            stages,
            io,
            () => false),
            { failed: [], total: 2 }
        );

        assert.equal(client.close_calls, 1);
        assert.deepEqual(client.download_requests, [{ id: "JOBID0", spool_file: 3 }, { id: "JOBID1", spool_file: 6 }]);
        assert.equal(client.jcls.length, 2);
        assert.ok(client.jcls[0].startsWith("//JOBNAME0 JOB 1"));
        assert.notEqual(client.jcls[0].indexOf("DSN=A.B"), -1);
        assert.ok(client.jcls[1].startsWith("//JOBNAME1 JOB 1"));
        assert.notEqual(client.jcls[1].indexOf("DSN=C.D"), -1);
        assert.equal(client.list_calls, 2);
        assert.deepEqual(client.set_list_mask_calls, ['JOBNAME*']);
        assert.deepEqual(io.copy_calls, [{ source: '/dir2', target: '/dir3' }]);
        assert.deepEqual(io.translate_calls, ['/dir1', '/dir2']);
        assert.deepEqual(io.unterse_calls, ['/dir1', '/dir2']);
        assert.equal(stages.stages, 4 + 5);
    });

    test('Failed job', async () => {
        const client = get_client([
            [{ jobname: "JOBNAME", id: "JOBID0", details: "RC=0008 3 spool files" }]
        ]);
        const io = get_io_ops();
        const stages = get_stage_counter();

        assert.deepEqual(await download_copy_books_with_client(
            client,
            [{ dsn: 'A.B', dirs: ['/dir1'] }],
            '//JOBNAME JOB 1',
            stages,
            io,
            () => false),
            { failed: [{ dsn: 'A.B', dirs: ['/dir1'] }], total: 1 }
        );

        assert.equal(client.close_calls, 1);
        assert.deepEqual(client.download_requests, []);
        assert.equal(client.jcls.length, 1);
        assert.ok(client.jcls[0].startsWith("//JOBNAME JOB 1"));
        assert.notEqual(client.jcls[0].indexOf("DSN=A.B"), -1);
        assert.equal(client.list_calls, 1);
        assert.deepEqual(client.set_list_mask_calls, ['JOBNAME']);
        assert.equal(io.copy_calls.length, 0);
        assert.equal(stages.stages, 1);
    });
});