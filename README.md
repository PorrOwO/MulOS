# MulOS
MultiPandOS Operating System Kernel code, main project of the UniBo Operating Systems course (y. 2024/2025) 

-   ## Dipendenze
Installare uriscv adeguato al proprio sistema operativo da https://github.com/virtualsquare/uriscv/releases/tag/latest

Eseguire:
```bash
    sudo dpkg -i "file.deb"
```
Installare le dipendenze mancanti (che non in tutte le macchine sono installate):
```bash
    sudo apt install libqt6widgets6 libqt6svg6 libboost-program-options 1.74.0
```

Potrebbero anche essercene altre. Nel caso basta guardare gli errori di installazione riguardo le dipendenze ed installare tutte quelle che servono.
Una volta installate tutte, eseguire il comando:
```bash
        sudo apt --fix-broken install
```
In questo modo tutti i pacchetti corrotti verrano cambiati con quelli nuovi appena installati.

-   ## Build
Eseguire il comando : 
```bash
    mkdir -p build && cd build && cmake .. && make && sudo make install 
```

Se mancasse questa dipendenza installarla:
```bash
    sudo apt install gcc-riscv64-unknown-elf
```

-   ## Esecuzione Programma
Eseguire il seguente comando per la UI di `uriscv`:
```bash
    uriscv
```
Oppure per visualizzare l'output tramite CLI:
```bash
    uriscv-cli
```

+   ### Testing Con `uriscv`
    Una volta aperto il programma con `uriscv`:
    + Inserire la configurazione della macchina (`config_machine.json`).
    + Premere il l'icona "▶" posta in alto nell'interfaccia.
    + Il programma verrà immediatamente eseguito. 
    + Per vedere le singole istruzioni basta:
        + cliccare sulla voce `windows`.
        + In base a cosa interessa vedere cliccare `processor 0`, per le singole istruzioni in Assembly, o su `terminal 0`, per vedere l'esecuzione del `main` della phase1.
    + Il programma si interrompe non appena incontra un errore nel codice.

+   ### Implementazione:
    + #### Fase 1:
        le funzioni principali hanno rispettato tutte le specifiche che sono state date, ma sono state aggiunte delle funzioni ausiliarie per facilitare l'esecuzione e la leggibilità di alcune delle funzioni principali, e queste sono:
        + `semd_t* allocSem(int* key)`:
            + prende un semaforo da `semdFree_h` eliminandolo da essa 
            + gli assegna la chiave specificata come parametro e ne inizializza la coda dei processi `s_procq` 
            + infine restituisce il semaforo inizializzato
                + prevede sempre un valore di ritorno valido (non `NULL`) e quindi si suppone che venga usata solo quando `semdFree_h` non sia una lista vuota
        + `semd_t* findSemd(int* key)`:
            + data una chiave restituisce il semaforo con quella chiave, se presente in `semd_h` (__ASL__), altrimenti restituisce `NULL`  
        + `void insertSortSem(int* key, pcb_t *p)`:
            + alloca un semaforo con chiave `s_key` uguale a `key`
            + inserisce il processo `p` nella coda dei processi `s_procq` del semaforo appena allocato
            + inserisce il semaforo appena allocato in `semd_h` (__ASL__) in modo che la lista sia ordinata in base alla chiave del semaforo 
    + #### Fase 2:
        + per aiutare nell'implementazione degli interrupt sono state create delle funzioni ausiliarie:
            + `int getDeviceSemaphoreIndex(int* commandAddr)`:
                + calcola l'indice del semaforo nell'array `DeviceSemaphores[NRSEMAPHORES]` attraverso l'indirizzo del comando del device che ha generato __l'interrupt__. Per prima cosa controlla se il device è un terminale, se lo è controlla se è in ricezione o trasmissione e in base a quello calcola il semaforo relativo a quel comando. Altrimenti se è un device non-terminale calcola l'offset dalla base rispetto al comando per poi trovare l'indice.
            + `int getHighestPriorityDeviceNumber()`:
                + prende tutti i device che in questo momento hanno un interrupt pending e calcola il numero di quello con la priorità più alta come viene indicato nelle specifiche della __fase 2__
            + `int getLineNo()`:
                + facendo il bitwise AND tra il codice della causa dell'eccezione e la costante `CAUSE_EXCCODE_MASK` e facendo uno switch sulle linee di interrupt per calcolare quella di questo interrupt.
            + `void terminateProcessSubTree(pcb_t* target)`:
                + controlla se il target da terminare ha figli e se ne ha chiama la funzione ricorsivamente sul primo.
                + controlla se il processo ha dei `p_sib` e in quel caso chiama la funzione ricorsivamente sul primo di loro 
                + rende il target orfano con `outChild` lo rimuove dalla `Readyqueue` lo rimuove dalla `blockedQueue` di un eventuale semaforo su cui è bloccato 
                + diminuisce il `ProcessCount` e poi fa `freePcb(target)`
    + Fase 3:
        + sono state implementate delle funzioni ausiliarie per relaizzare il livello supporto.
            + `int getDeviceSemIndex(int line, int dev)`:
                + questa funzione calcola l'indice del semaforo legato ad un determinato device e quindi alla sua relativa "interrupt line".
            + `static inline int _getFreeSwapFrameIndex(void)`:
                + questa funzione calcola l'indice di un frame libero va eseguita in mutua esclusione su di un `replaceSemaphore` per evitare che altri processi vadano a modificare questa variabile e quindi non ottenere il l'indice del frame libero giusto.
            + `static inline void _flashIO(int asid, int vpn, int command, memaddr starting_frame_addr)`:
                + questa funzione legge o scrive il backing store di un certo U-proc identificato dal suo ASID,
                + si imposta il data_0 con l'indirizzo del frame da leggere o su cui scriverci.
                + Si imposta il command register inserendo l'indice della pagina corrispondente al VPN, passato come parametro, insieme al comando da eseguire.
                + infine si esegue una SYSCALL DOIO e qualsiasi tipo di errore nell'esecuzione di questa SYSCALL viene gestito come una TRAP.
            + `void disableInterrupts(void)`:
                + questa funzione disabilita gli interrupt azzerando tutti i bit che permettono agli interrupt di partire.
            + `void enableInterrupts(void)`:
                + questa funzione abilita gli interrupt settando i bit dello status a 1 per permettergli di far partire gli interrupt. 
            + `void updateTLB_Probe(pteEntry_t* page)`:
                + questa funzione setta l'entryHI della pagina per poi eseguire un TLBP() il quale restituisce un indice che se mascherato con `PRESENTFLAG` e risulta 0 allora la pagina va modificata.
            + `static inline int _is_valid_address(memaddr virtAddr, int len)`:
                + controlla che l'indirizzo di partenza `virtAddr` sia nella _Text & data area_ e che se sommandoci la `len` non sfori nella _Stack area_.
             

