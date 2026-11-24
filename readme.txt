Tarea 2 - Sistemas Operativos 2026: Lectores y escritores

Compilar:
    make            # genera ./rw
    make tsan       # versión con ThreadSanitizer (./rw_tsan)

Ejecutar:
    ./rw <modo> <politica> <L> <E> <K> <dur_lec_us> <dur_esc_us> [notraza]
    modo: cond | atomic      politica: lectores | escritores | fifo
    (atomic solo con escritores)

Ejemplos:
    ./rw cond escritores 6 2 100 200 500
    ./rw atomic escritores 6 2 100 200 500
    ./rw cond escritores 6 2 100 200 500 notraza
