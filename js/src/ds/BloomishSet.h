/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ds_BloomishSet_h
#define ds_BloomishSet_h

#include <limits.h>

#include "js/Utility.h"
#include "jstypes.h"

class JSAtom;

namespace js {

struct BloomishSet {
    enum { TABLE_BITS = 4, COUNTER_BITS = 4, TEST_LIMIT = 8 };
    uint32_t size;
    uint64_t table[1 << TABLE_BITS];

    BloomishSet() : size(0) {
        memset(table, 0, sizeof table);
    }

    uint64_t tagAtom(JSAtom* atom) {
        // Create a COUNTER_BITS-bit counter in the bottom of the pointer, initially 1.
        uintptr_t p = (uintptr_t)atom;
        MOZ_ASSERT(p << COUNTER_BITS >> COUNTER_BITS == p);
        static_assert(sizeof(uintptr_t) <= sizeof(uint64_t), "");
        return (uint64_t)(p << COUNTER_BITS) | 1;
    }

    HashNumber hashAtom(JSAtom* atom) {
        // Fast hasher. Atoms don't move within the parser, so this is fine, right?
        // The non-determinstism is sad, though, and this at least needs some form
        // of HashString when JS_MORE_DETERMINISTIC is set.
        static_assert(sizeof(HashNumber) == 4, "");
        return detail::ScrambleHashCode((HashNumber)(uintptr_t)atom) >> (32 - TABLE_BITS);
    }

    bool isDefinitelyIncluded(JSAtom* atom) {
        // As long as size <= 2^COUNTER_BITS, this test has no false positives
        // due to counter wrap-around.  The probability of a false negative,
        // which happens if the atom is included in the table but collides with
        // something else, is roughly exp(-tableSize / (size - 1)); if this is
        // large enough the test is not worth performing.  Thus we set TEST_LIMIT
        // a bit smaller than 2^COUNTER_BITS (8 taken out of thin air).
        return 0 < size && size <= TEST_LIMIT && table[hashAtom(atom)] == tagAtom(atom);
    }

    void remove(JSAtom* atom) {
        MOZ_ASSERT(size != 0);
        MOZ_ASSERT(size >= (1 << COUNTER_BITS) ||
            (table[hashAtom(atom)] & ((1 << COUNTER_BITS) - 1)) > 0);
        size--;
        table[hashAtom(atom)] -= tagAtom(atom);
    }

    void add(JSAtom* atom) {
        size++;
        MOZ_ASSERT(size != 0);
        table[hashAtom(atom)] += tagAtom(atom);
    }
};

} /* namespace js */

#endif /* ds_BloomishSet_h */
