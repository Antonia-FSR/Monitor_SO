#!/bin/bash
# Script de pruebas E1 y E2
# Ejecutar despues de compilar con 'make':
#   chmod +x ejecutar_experimentos.sh && ./ejecutar_experimentos.sh

echo "=== EXPERIMENTO E1 (5 repeticiones por caso) ===" > resultados_E1.txt

# Formato: L E K dur_lec_us dur_esc_us (segun tabla del enunciado)
casos_E1=(
    "1 1 20 500 500"
    "6 2 100 200 500"
    "2 6 100 500 200"
    "8 8 100 0 0"
    "8 1 200 1000 100"
)

combinaciones_E1=(
    "cond lectores"
    "cond escritores"
    "cond fifo"
    "atomic escritores"
)

for comb in "${combinaciones_E1[@]}"; do
    for caso in "${casos_E1[@]}"; do
        echo "--- $comb | $caso ---" >> resultados_E1.txt
        for rep in {1..5}; do
            ./rw $comb $caso >> resultados_E1.txt
        done
    done
done

echo "=== EXPERIMENTO E2 (Politica escritores, notraza, 5 repeticiones) ===" > resultados_E2.txt

# Igual que E1, pero la configuracion sin pausas usa K = 10000
casos_E2=(
    "1 1 20 500 500"
    "6 2 100 200 500"
    "2 6 100 500 200"
    "8 8 10000 0 0"
    "8 1 200 1000 100"
)

for modo in cond atomic; do
    for caso in "${casos_E2[@]}"; do
        echo "--- $modo escritores | $caso | notraza ---" >> resultados_E2.txt
        for rep in {1..5}; do
            ./rw $modo escritores $caso notraza >> resultados_E2.txt
        done
    done
done

echo "Listo. Resultados guardados en resultados_E1.txt y resultados_E2.txt"