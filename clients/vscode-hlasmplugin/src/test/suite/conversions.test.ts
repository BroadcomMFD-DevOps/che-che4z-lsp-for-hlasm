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
import { convertBuffer, uriFriendlyBase16Decode, uriFriendlyBase16Encode } from '../../conversions';

suite('Conversions', () => {
    test('Buffer conversion', () => {
        assert.equal(convertBuffer(Buffer.from([0x40, 0xC1, 0x40]), 80), ' A ');
    });

    test('URI friendly Base16 encode', () => {
        assert.strictEqual(uriFriendlyBase16Encode(""), "");
        assert.strictEqual(uriFriendlyBase16Encode("A"), "eb");
        assert.strictEqual(uriFriendlyBase16Encode("AB"), "ebec");
    });

    test('URI friendly Base16 decode', () => {
        assert.strictEqual(uriFriendlyBase16Decode(""), "");
        assert.strictEqual(uriFriendlyBase16Decode("eb"), "A");
        assert.strictEqual(uriFriendlyBase16Decode("ebec"), "AB");
        assert.strictEqual(uriFriendlyBase16Decode("EB"), "A");
        assert.strictEqual(uriFriendlyBase16Decode("EBEC"), "AB");
        assert.strictEqual(uriFriendlyBase16Decode("EbeC"), "AB");
        assert.strictEqual(uriFriendlyBase16Decode("e"), "");
        assert.strictEqual(uriFriendlyBase16Decode("aX"), "");
        assert.strictEqual(uriFriendlyBase16Decode("Xa"), "");
        assert.strictEqual(uriFriendlyBase16Decode("ax"), "");
        assert.strictEqual(uriFriendlyBase16Decode("xa"), "");
    });
});

