#include "global.h"
#include "recomputils.h"
#include "zproxy.h"
#include "globalobjects_api.h"

static Gfx sSetBilerpDL[] = {
    gsDPSetTextureFilter(G_TF_BILERP),
    gsSPEndDisplayList(),
};

void ZProxy_initZProxy(ZProxy * this, ObjectId id, U32MemoryHashmapHandle customDisplayListMap) {
    this->vanillaObjId = id;

    this->vanillaDLToCustomDLMap = recomputil_create_u32_memory_hashmap(sizeof(ZProxy_ProxyContainer));
    
    this->vanillaDisplayLists = LinkedList_newList();

    this->customDisplayLists = customDisplayListMap;
}

void ZProxy_destroyZProxy(ZProxy *this) {
    recomputil_destroy_u32_memory_hashmap(this->vanillaDLToCustomDLMap);
    recomp_free(this->vanillaDisplayLists);
}

bool ZProxy_reserveContainer(ZProxy *this, Gfx *vanillaDisplayList) {
    if (!vanillaDisplayList) {
        return false;
    }

    u32 vanilla = (uintptr_t)vanillaDisplayList;

    ZProxy_ProxyContainer *container = recomputil_u32_memory_hashmap_get(this->vanillaDLToCustomDLMap, vanilla);

    if (!container) {
        if (recomputil_u32_memory_hashmap_create(this->vanillaDLToCustomDLMap, vanilla)) {

            LinkedList_addBack(this->vanillaDisplayLists, vanillaDisplayList);

            container = recomputil_u32_memory_hashmap_get(this->vanillaDLToCustomDLMap, vanilla);

            container->vanillaDisplayList = vanillaDisplayList;

            container->customDisplayListStack = LinkedList_newList();

            gSPDisplayList(&container->displayList[0], GlobalObjects_getGlobalGfxPtr(this->vanillaObjId, vanillaDisplayList));

            // Vanilla 3D models use G_TF_BILERP in all cases, so change this back in case a mod set a different filtering mode
            gSPBranchList(&container[1], sSetBilerpDL);

            return true;
        }
    }

    return false;
}

RECOMP_DECLARE_EVENT(onModelChange(ObjectId id, Gfx *vanillaDL, Gfx *newDL))

void refreshContainerDL(ZProxy *this, ZProxy_ProxyContainer *c) {
    Gfx *dl = NULL;

    LinkedListNode *curr = LinkedList_end(c->customDisplayListStack);

    while(curr && !dl) {
        ZProxy_CustomDisplayListEntry *entry = LinkedListNode_getData(curr);

        if (entry) {
            dl = entry->customDL;
        }

        curr = LinkedListNode_getPrev(curr);
    }
    
    if (!dl) {
        dl = GlobalObjects_getGlobalGfxPtr(this->vanillaObjId, c->vanillaDisplayList);
    }

    if ((uintptr_t)dl != c->displayList[0].words.w1) {
        gSPDisplayList(&c->displayList[0], dl);
        onModelChange(this->vanillaObjId, c->vanillaDisplayList, dl);
    }
}

bool ZProxy_refresh(ZProxy *this, ModelReplacerHandle handle) {

    ZProxy_CustomDisplayListEntry *entry = recomputil_u32_memory_hashmap_get(this->customDisplayLists, handle);

    if (!entry) {
        return false;
    }

    Gfx *vanillaDisplayList = entry->vanillaDL;

    ZProxy_reserveContainer(this, vanillaDisplayList);

    if (!vanillaDisplayList) {
        return false;
    }

    uintptr_t vanilla = (uintptr_t)vanillaDisplayList;

    ZProxy_ProxyContainer *container = recomputil_u32_memory_hashmap_get(this->vanillaDLToCustomDLMap, vanilla);

    if (container) {
        refreshContainerDL(this, container);
        return true;
    }

    return false;
}

bool ZProxy_addCustomDisplayList(ZProxy *this, ModelReplacerHandle handle) {
    ZProxy_CustomDisplayListEntry *entry = recomputil_u32_memory_hashmap_get(this->customDisplayLists, handle);

    if (!entry) {
        return false;
    }

    Gfx *vanillaDisplayList = entry->vanillaDL;

    Gfx *customDisplayList = entry->customDL;

    ZProxy_reserveContainer(this, vanillaDisplayList);

    if (!vanillaDisplayList) {
        return false;
    }

    uintptr_t vanilla = (uintptr_t)vanillaDisplayList;

    ZProxy_ProxyContainer *container = recomputil_u32_memory_hashmap_get(this->vanillaDLToCustomDLMap, vanilla);

    if (container) {
        LinkedList_addBack(container->customDisplayListStack, entry);
        refreshContainerDL(this, container);
        return true;
    }

    return false;
}

bool ZProxy_removeCustomDisplayList(ZProxy *this, ModelReplacerHandle handle) {
    ZProxy_CustomDisplayListEntry *entry = recomputil_u32_memory_hashmap_get(this->customDisplayLists, handle);

    if (!entry) {
        return false;
    }

    Gfx *vanillaDisplayList = entry->vanillaDL;

    Gfx *customDisplayList = entry->customDL;

    ZProxy_reserveContainer(this, vanillaDisplayList);

    if (!customDisplayList || !vanillaDisplayList) {
        return false;
    }

    uintptr_t vanilla = (uintptr_t)vanillaDisplayList;

    ZProxy_ProxyContainer *container = recomputil_u32_memory_hashmap_get(this->vanillaDLToCustomDLMap, vanilla);

    if (container && LinkedList_removeData(container->customDisplayListStack, entry)) {
        refreshContainerDL(this, container);
        return true;
    }

    return false;
}
