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

import { Readable, Stream } from "stream";
import { FBWritable } from "../../FBWritable";
import * as assert from 'assert';
import { AsyncMutex, AsyncSemaphore } from "../../asyncMutex";
import { isCancellationError } from "../../helpers";
import { CancellationError } from "vscode";


suite('Utilities', () => {

    test('FB Convertor', async () => {
        const convertor = new FBWritable(10);
        const input = Buffer.from([0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2]);

        await Stream.promises.pipeline(Readable.from(input), convertor);

        assert.deepStrictEqual(convertor.getResult().split(/\r?\n/), ["AAAAAAAAAA", "BBBBBBBBBB", ""]);
    });

    test('Async mutex', async () => {
        const mutex = new AsyncMutex();

        let i = 0;

        let wakeupCallback: () => void;
        const wakeup = new Promise<void>(r => { wakeupCallback = r; });

        const a1 = mutex.locked(async () => {
            assert.strictEqual(i, 0);

            await wakeup;

            assert.strictEqual(i, 0);

            i = 1;
        });

        const a2 = mutex.locked(async () => {
            assert.strictEqual(i, 1);
        });

        wakeupCallback();

        await Promise.all([a1, a2]);

    }).timeout(10000).slow(2000);

    test('Async semaphore', async () => {
        const mutex = new AsyncSemaphore(2);

        let i = 0;

        let wakeupCallback: () => void;
        const wakeup = new Promise<void>(r => { wakeupCallback = r; });

        const a1 = mutex.locked(async () => {
            assert.strictEqual(i, 0);

            await wakeup;

            assert.notStrictEqual(i, 2);

            ++i;
        });

        const a2 = mutex.locked(async () => {
            assert.strictEqual(i, 0);

            await wakeup;

            assert.notStrictEqual(i, 2);

            ++i;
        });

        const a3 = mutex.locked(async () => {
            assert.notStrictEqual(i, 0);
        });

        wakeupCallback();

        await Promise.all([a1, a2, a3]);

        assert.strictEqual(i, 2);

    }).timeout(10000).slow(2000);

    test('Cancellation error detection', () => {
        assert.ok(isCancellationError(new CancellationError()));
        assert.ok(isCancellationError(new Error("Canceled")));
        assert.ok(!isCancellationError(new Error("Something")));
    });

});
