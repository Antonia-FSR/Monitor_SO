// verificador.h — integrantes 2 y 3
#pragma once
#include "comun.h"
#include <vector>

struct ResultadoVerificacion {
    bool politica_ok = true;
    long evidencia = 0;
};

ResultadoVerificacion verificar_traza(const std::vector<AnotacionTraza>& traza, Politica politica);
