#!/bin/bash
# Uso: ./promediar.sh resultados_E2.txt > promedios_E2.txt
awk '
function flush(   j, k2) {
    if (n > 0) {
        print titulo
        printf "  n=%d correcto_SI=%d/%d\n  ", n, ok, n
        for (j = 1; j <= nk; j++) {
            k2 = claves[j]
            if (k2 in suma) printf "%s=%.3f ", k2, suma[k2] / n
        }
        printf "\n"
    }
    n = 0; ok = 0; split("", suma)
}
/^===/ { next }
/^---/ { flush(); titulo = $0; next }
/modo=/ {
    n++
    for (i = 1; i <= NF; i++) {
        split($i, kv, "=")
        if (kv[1] == "correcto" && kv[2] == "SI") ok++
        if (kv[2] ~ /^-?[0-9]+(\.[0-9]+)?$/) {
            if (!(kv[1] in visto)) { visto[kv[1]] = 1; claves[++nk] = kv[1] }
            suma[kv[1]] += kv[2]
        }
    }
}
END { flush() }
' "$1"