#include "release_store.h"

void release_store_init(ReleaseStore* store) {
    if (store) {
        store->dummy = 0;
    }
}
