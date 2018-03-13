#include <inttypes.h>

#include "btree1.h"

void btree1InitPage (Btree1Page *page) {
	initLock(page->latch->readwr);
	initLock(page->latch->parent);
	initLock(page->latch->link);
}

//	create an empty page

uint64_t btree1NewPage (Handle *hndl, uint8_t lvl) {
Btree1Index *btree1 = btree1index(hndl->map);
Btree1PageType type;
Btree1Page *page;
uint32_t size;
DbAddr addr;

	size = btree1->pageSize;

	if (lvl)
		type = Btree1_interior;
	else {
		type = Btree1_leafPage;
		size <<= btree1->leafXtra;
	}

	if ((addr.bits = allocObj(hndl->map, listFree(hndl,type), NULL, type, size, true)))
		page = getObj(hndl->map, addr);
	else
		return 0;

	btree1InitPage(page);
	page->lvl = lvl;
	page->min = size;
	return addr.bits;
}

//	initialize btree1 root page

DbStatus btree1Init(Handle *hndl, Params *params) {
Btree1Index *btree1 = btree1index(hndl->map);
Btree1Page *page;
Btree1Slot *slot;
uint8_t *buff;

	if (params[Btree1Bits].intVal > Btree1_maxbits) {
		fprintf(stderr, "createIndex: bits = %" PRIu64 " > max = %d\n", params[Btree1Bits].intVal, Btree1_maxbits);
		exit(1);
	}

	if (params[Btree1Bits].intVal + params[Btree1Xtra].intVal > Btree1_maxbits) {
		fprintf(stderr, "createIndex: bits = %" PRIu64 " + xtra = %" PRIu64 " > max = %d\n", params[Btree1Bits].intVal, params[Btree1Xtra].intVal, Btree1_maxbits);
		exit(1);
	}

	btree1->pageSize = 1 << params[Btree1Bits].intVal;
	btree1->pageBits = (uint32_t)params[Btree1Bits].intVal;
	btree1->leafXtra = (uint32_t)params[Btree1Xtra].intVal;

	//	initial btree1 root & leaf pages

	if ((btree1->left.bits = btree1NewPage(hndl, 0)))
		page = getObj(hndl->map, btree1->left);
	else
		return DB_ERROR_outofmemory;

	//  set up new leaf page with stopper key

	btree1->left.type = Btree1_leafPage;
	btree1->right.bits = btree1->left.bits;

	page->min -= 1;
	page->cnt = 1;
	page->act = 1;

	buff = keyaddr(page, page->min);
	buff[0] = 0;

	//  set up stopper slot

	slot = slotptr(page, 1);
	slot->type = Btree1_stopper;
	slot->off = page->min;

	//	set  up the tree root page with stopper key

	if ((btree1->root.bits = btree1NewPage(hndl, 1)))
		page = getObj(hndl->map, btree1->root);
	else
		return DB_ERROR_outofmemory;

	//  set up new root page with stopper key

	btree1->root.type = Btree1_rootPage;
	page->min -= 1 + sizeof(uint64_t);
	page->cnt = 1;
	page->act = 1;

	//  set up stopper key

	buff = keyaddr(page, page->min);
	btree1PutPageNo(buff + 1, 0, btree1->left.bits);
	buff[0] = sizeof(uint64_t);

	//  set up slot

	slot = slotptr(page, 1);
	slot->type = Btree1_stopper;
	slot->off = page->min;

	hndl->map->arena->type[0] = Hndl_btree1Index;
	return DB_OK;
}

// place write, read, or parent lock on requested page_no.

void btree1LockPage(Btree1Page *page, Btree1Lock mode) {
	switch( mode ) {
	case Btree1_lockRead:
		readLock (page->latch->readwr);
		break;
	case Btree1_lockWrite:
		writeLock (page->latch->readwr);
		break;
	case Btree1_lockParent:
		writeLock (page->latch->parent);
		break;
	case Btree1_lockLink:
		writeLock (page->latch->link);
		break;
	}
}

void btree1UnlockPage(Btree1Page *page, Btree1Lock mode)
{
	switch( mode ) {
	case Btree1_lockWrite:
		writeUnlock (page->latch->readwr);
		break;
	case Btree1_lockRead:
		readUnlock (page->latch->readwr);
		break;
	case Btree1_lockParent:
		writeUnlock (page->latch->parent);
		break;
	case Btree1_lockLink:
		writeUnlock (page->latch->link);
		break;
	}
}

void btree1PutPageNo(uint8_t *key, uint32_t len, uint64_t bits) {
int idx = sizeof(uint64_t);

	while (idx--)
		key[len + idx] = (uint8_t)bits, bits >>= 8;
}

uint64_t btree1GetPageNo(uint8_t *key, uint32_t len) {
uint64_t result = 0;
int idx = 0;

	len -= sizeof(uint64_t);

	do result <<= 8, result |= key[len + idx];
	while (++idx < sizeof(uint64_t));

	return result;
}
