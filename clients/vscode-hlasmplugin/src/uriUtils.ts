/*
 * Copyright (c) 2026 Broadcom.
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

export function makeUriPath(...segments: string[]) {
    return '/' + segments.map(s => encodeURIComponent(s)).join('/');
}

export function makeUriPathWithSuffix(suffix: string, ...segments: string[]) {
    return '/' + segments.map(s => encodeURIComponent(s)).join('/') + suffix;
}
