#include "./headers/pcb.h"

#include "../headers/const.h"
#include "../headers/listx.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

static inline void _initState(state_t* s) {
    // initialize state
    s->entry_hi = 0;
    s->cause = 0;
    s->status = 0;
    s->pc_epc = 0;
    s->mie = 0;
    for(int i = 0; i < STATE_GPR_LEN; ++i) {
        s->gpr[i] = 0;
    }
}

static inline void _initContext(context_t* c) {
    c->stackPtr = 0;
    c->status = 0;
    c->pc = 0;
}

static inline void _initEntry(pteEntry_t* e) {
    e->pte_entryHI = 0;
    e->pte_entryLO = 0;
}

// TODO
static inline void _initSupport(support_t* s);

static inline void _initPcb(pcb_t* pcb) {
    pcb->p_parent = NULL;
    INIT_LIST_HEAD(&pcb->p_child);
    INIT_LIST_HEAD(&pcb->p_sib);

    _initState(&pcb->p_s);
    // _initSupport(pcb->p_supportStruct);

    pcb->p_time = 0;
    pcb->p_semAdd = 0;
    pcb->p_pid = next_pid++;
}

void initPcbs() {
    INIT_LIST_HEAD(&pcbFree_h);
    for (int i = 0; i < MAXPROC; i++) {
        list_add_tail(&pcbFree_table[i].p_list, &pcbFree_h);
    }
}

void freePcb(pcb_t* p) {
    list_add_tail(&p->p_list, &pcbFree_h);
}

pcb_t* allocPcb() {
    if(list_empty(&pcbFree_h)) return NULL;

    pcb_t* pcb = container_of(pcbFree_h.prev, pcb_t, p_list);
    list_del(&pcb->p_list);
    
    _initPcb(pcb);
    return pcb;
}

void mkEmptyProcQ(struct list_head* head) {
    INIT_LIST_HEAD(head);
}

int emptyProcQ(struct list_head* head) {
    return (list_empty(head));
}

void insertProcQ(struct list_head* head, pcb_t* p) {
    list_add_tail(&p->p_list, head);
}

pcb_t* headProcQ(struct list_head* head) {
    if(list_empty(head)) return  NULL;

    return container_of(head->next, pcb_t, p_list);
}

pcb_t* removeProcQ(struct list_head* head) {
    if(list_empty(head)) return NULL;

    pcb_t* out = container_of(head->next, pcb_t, p_list);
    list_del(&out->p_list);
    return out;
}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {
    struct list_head* iter; 
    list_for_each(iter, head) {
        pcb_t* pcb = container_of(iter, pcb_t, p_list);
        if(pcb->p_pid == p->p_pid) {
            list_del(iter);
            return pcb;
        }
    }
    return NULL; //return NULL if the pcb is not found
}

int emptyChild(pcb_t* p) {
    return list_empty(&p->p_child);
}

void insertChild(pcb_t* prnt, pcb_t* p) {
    if (prnt == NULL || p == NULL) return;

    p->p_parent = prnt;
    list_add_tail(&p->p_sib, &prnt->p_child);
}

pcb_t* removeChild(pcb_t* p) {
    if(emptyChild(p)) return NULL;
        
    pcb_t *child = container_of(p->p_child.next, pcb_t, p_sib);
    
    // if(!list_empty(&child->p_sib)){
    //   pcb_t *newFirst = container_of(child->p_sib.next, pcb_t, p_child);
    //   //list_add_tail(&newFirst->p_child, &p->p_child);
    // }
    //
    
    child->p_parent = NULL;
    list_del(&child->p_sib);
    
    return child;
}

pcb_t* outChild(pcb_t* p) {
    if (p->p_parent == NULL) return NULL;
    
    list_del(&p->p_sib);
    p->p_parent = NULL;
    return p;
}
