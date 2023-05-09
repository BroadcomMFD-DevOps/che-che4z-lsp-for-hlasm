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


import * as assert from 'assert';
import { HLASMExternalFiles } from '../../hlasmExternalFiles';

suite('External files', () => {

    test('Invalid messages', async () => {
        const ext = new HLASMExternalFiles('test', {
            onNotification: (_, __) => { return { dispose: () => { } }; },
            sendNotification: (_: any, __: any) => Promise.resolve(),
        });

        assert.strictEqual(await ext.handleRawMessage(null), null);
        assert.strictEqual(await ext.handleRawMessage(undefined), null);
        assert.strictEqual(await ext.handleRawMessage({}), null);
        assert.strictEqual(await ext.handleRawMessage(5), null);
        assert.strictEqual(await ext.handleRawMessage({ id: 'id', op: '' }), null);
        assert.strictEqual(await ext.handleRawMessage({ id: 5, op: 5 }), null);

        assert.deepEqual(await ext.handleRawMessage({ id: 5, op: '' }), { id: 5, error: { code: -5, msg: 'Invalid request' } });
        assert.deepEqual(await ext.handleRawMessage({ id: 5, op: 'read_file', url: 5 }), { id: 5, error: { code: -5, msg: 'Invalid request' } });
        assert.deepEqual(await ext.handleRawMessage({ id: 5, op: 'read_file', url: {} }), { id: 5, error: { code: -5, msg: 'Invalid request' } });

        assert.deepEqual(await ext.handleRawMessage({ id: 5, op: 'read_file', url: 'unknown:scheme' }), { id: 5, error: { code: -5, msg: 'Invalid request' } });

        assert.deepEqual(await ext.handleRawMessage({ id: 5, op: 'read_file', url: 'test:/SERVICE' }), { id: 5, error: { code: -1000, msg: 'No client' } });
    });
});
