/*
 * Copyright (c) 2025 Broadcom.
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
import { getLastRunConfig } from "../../mfCreds";
import type { ExtensionContext } from 'vscode';

suite('MF Credentials', () => {

    test('Get last run config', async () => {
        const last = getLastRunConfig({
            globalState: {
                get() { return { host: 'h', user: 'u', jobcard: 'j' }; }
            }
        } as unknown as ExtensionContext);

        assert.deepStrictEqual(last, { host: 'h', user: 'u', jobcard: 'j' });
    });

    test('No last run', async () => {
        const last = getLastRunConfig({
            globalState: {
                get(_: unknown, def: unknown) { return def; }
            }
        } as unknown as ExtensionContext);

        assert.deepStrictEqual(last, { host: '', user: '', jobcard: '' });
    });
});
