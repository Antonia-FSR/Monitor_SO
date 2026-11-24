// control_atomico_rw.h — Parte II (integrante 4)
#pragma once
#include "comun.h"
#include <atomic>
#include <cstdint>

class ControlAtomicoRW : public ControlRW {
public:
    void entrar_lectura() override;
    void salir_lectura() override;
    void entrar_escritura() override;
    void salir_escritura() override;
    // TODO: forma de entregar FallosAtomic al main (acordar con integrante 1)
private:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    std::atomic<std::uint64_t> estado{0};
    // TODO: documentar formato de bits de 'estado'
};
