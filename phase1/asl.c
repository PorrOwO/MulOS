#include "./headers/asl.h"
#include "./headers/pcb.h"



static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;


static inline semd_t* allocSem(int* key) {
    semd_t* newSem = container_of(semdFree_h.prev, semd_t, s_link); 
    list_del(&newSem->s_link); 
    newSem->s_key = key; 
    mkEmptyProcQ(&newSem->s_procq); 
    return newSem;
}

static inline semd_t* findSemd(int* key) {
    struct list_head *iter = semd_h.next;
    while (iter != NULL && !list_empty(&semd_h)) {
        semd_t* iterSem = container_of(iter, semd_t, s_link);
        if (iterSem->s_key == key) {
            return iterSem;
        }
        iter = iter->next;
    }
    return NULL;
}

void insertSem(int* key, pcb_t* p) {
  semd_t* newSem = allocSem(key);
  list_add_tail(&p->p_list, &newSem->s_procq);
  semd_t* iter = container_of(semd_h.next, semd_t, s_link);
  if (semd_h.next == NULL) { // if the semd_h is empty, add the new semd to the head. Making the semd_h a single linked list
        semd_h.next = &newSem->s_link;
        newSem->s_link.next = NULL;
  } else { 
    semd_t* iter = container_of(semd_h.next, semd_t, s_link);
    while (iter->s_link.next != NULL){
      iter = container_of(iter->s_link.next, semd_t, s_link); 
    }
    iter->s_link.next = &newSem->s_link;
    newSem->s_link.next = NULL;
  }
}

void initASL() {
    INIT_LIST_HEAD(&semdFree_h);
    semd_h.next=NULL;
    //INIT_LIST_HEAD(&semd_h);
    
    for (int i = 0; i < MAXPROC; i++) {
        INIT_LIST_HEAD(&semd_table[i].s_link);
        list_add_tail(&semd_table[i].s_link, &semdFree_h);   
    }
}

int insertBlocked(int* semAdd, pcb_t* p) {
    semd_t* sem = findSemd(semAdd);
    if (sem) { //if the semd already exists, add the process to the end of its process queue
        list_add_tail(&p->p_list, &sem->s_procq);
    } else {
        if (list_empty(&semdFree_h)) { //if there are no more semd_t available, return 1
            return 1;
        }
        insertSem(semAdd, p); //insert the new semd in the right place
        //semd_t* newSem = allocSem(semAdd);
        //list_add_tail(&p->p_list, &newSem->s_procq); //add the process to the end of the new semd's process queue
    }
    p->p_semAdd = semAdd;
    return 0;
}

pcb_t* removeBlocked(int* semAdd) {
    semd_t* sem = findSemd(semAdd);
    if (sem) { 
        pcb_t *head = removeProcQ(&sem->s_procq);
        if (emptyProcQ(&sem->s_procq)) {
            struct list_head* prev = &semd_h;

            while (prev->next != &sem->s_link) { //find the previous semd_t needed to remove the semd_t from the list
                prev = prev->next;
            }
            prev->next = sem->s_link.next; //remove the semd_t from the list
            sem->s_link.next = NULL;
            list_add_tail(&sem->s_link, &semdFree_h); //insert sem into semdFree_h
        }
        return head;
    }
    return NULL;
}

pcb_t* outBlocked(pcb_t* p) {
    semd_t* sem = findSemd(p->p_semAdd);
    if (sem == NULL) return NULL;

    return outProcQ(&sem->s_procq, p);
}

pcb_t* headBlocked(int* semAdd) {
    semd_t* sem = findSemd(semAdd);
    if (sem == NULL){
        return NULL; 
    }
    if(emptyProcQ(&sem->s_procq)) 
    {
        return NULL;
    }
    return headProcQ(&sem->s_procq);
}

// FIX: devo iterare su semdh e per ogni semaforo cerca il processo bloccato che ha quel pid
pcb_t* outBlockedPID(int pid) {
  struct list_head* iterSem;
  list_for_each(iterSem, &semd_h) {
    semd_t* sem = container_of(iterSem, semd_t, s_link);
    struct list_head* iterProc;
    list_for_each(iterProc, &sem->s_procq) {
      pcb_t* proc = container_of(iterProc, pcb_t, p_list);
      if (proc->p_pid == pid) {
        list_del(&proc->p_list);
        return proc;
      }
    }
  }
  return NULL;
}

