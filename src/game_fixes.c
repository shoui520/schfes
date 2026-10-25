#include "sif.h"
#include "reimpl/pthr.h"
#include <stdint.h>
#include <psp2/kernel/threadmgr.h>

/* libGame.so (GAME_SHA256) sound manager. Every path takes the manager's slot
 * lock (manager+0x34) before the global sound lock, except the buffer-queue
 * callback: it holds the global lock while it adds an active slot. A sound
 * released on the main thread meanwhile holds the slot lock and waits for the
 * global one, so both stop. The callback below takes the slot lock first. */
#define SOUND_LOCK 0x590dbc
#define SOUND_MANAGER 0x3c2188
#define SOUND_ADD_ACTIVE 0x3c21f0
#define SOUND_MANAGER_OWNS 0x3c2f00
#define SOUND_REFILL 0x3d09e8
#define SOUND_BUFFER_CALLBACK 0x3d1764
#define SLOT_LOCK 0x34
#define ACTIVE_SLOTS 0x38
#define ACTIVE_SLOT_COUNT 4

static uint8_t *(*sound_manager)(void);
static int (*sound_manager_owns)(uint8_t *manager,void *sound);
static void (*sound_refill)(void *sound,void *queue,int unused);
static pthread_mutex_t_bionic *sound_lock;
// The thread whose buffer callback holds the slot lock, so the slot it adds
// does not lock it again.
static SceUID slot_lock_owner=-1;

static pthread_mutex_t_bionic *slot_lock(uint8_t *manager) {
    return (pthread_mutex_t_bionic *)(manager+SLOT_LOCK);
}

static void add_active(uint8_t *manager,void *sound) {
    int held=__atomic_load_n(&slot_lock_owner,__ATOMIC_RELAXED)==sceKernelGetThreadId();
    if(!held)pthread_mutex_lock_soloader(slot_lock(manager));
    void **slots=(void **)(manager+ACTIVE_SLOTS);
    int added=0;
    for(int i=0;i<ACTIVE_SLOT_COUNT && !added;i++)
        if(!slots[i]){slots[i]=sound;added=1;}
    if(!held)pthread_mutex_unlock_soloader(slot_lock(manager));
    // The game asserts here too.
    if(!added)sif_stop("Sound manager active slots full");
}

static void buffer_callback(void *queue,void *sound) {
    uint8_t *manager=sound_manager();
    pthread_mutex_lock_soloader(slot_lock(manager));
    __atomic_store_n(&slot_lock_owner,sceKernelGetThreadId(),__ATOMIC_RELAXED);
    pthread_mutex_lock_soloader(sound_lock);
    if(sound_manager_owns(manager,sound)==1)sound_refill(sound,queue,0);
    pthread_mutex_unlock_soloader(sound_lock);
    __atomic_store_n(&slot_lock_owner,-1,__ATOMIC_RELAXED);
    pthread_mutex_unlock_soloader(slot_lock(manager));
}

void sif_install_game_fixes(void) {
    uintptr_t base=so_mod.text_base;
    sound_manager=(void *)(base+SOUND_MANAGER);
    sound_manager_owns=(void *)(base+SOUND_MANAGER_OWNS);
    sound_refill=(void *)(base+SOUND_REFILL);
    sound_lock=(pthread_mutex_t_bionic *)(base+SOUND_LOCK);
    hook_arm(base+SOUND_ADD_ACTIVE,(uintptr_t)add_active);
    hook_arm(base+SOUND_BUFFER_CALLBACK,(uintptr_t)buffer_callback);
}
