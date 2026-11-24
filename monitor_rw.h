// monitor_rw.h — Parte I (integrantes 2 y 3)
#pragma once
#include "comun.h"
#include <mutex>
#include <condition_variable>
#include <vector>

class MonitorRW : public ControlRW {
public:
    explicit MonitorRW(Politica p);
    void entrar_lectura() override;
    void salir_lectura() override;
    void entrar_escritura() override;
    void salir_escritura() override;
    std::vector<AnotacionTraza> obtener_traza();
private:
    Politica politica;
    std::mutex m;
    std::vector<AnotacionTraza> traza;
    // TODO: variables de condición y estado de admisión
};
