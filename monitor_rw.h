#pragma once
#include "comun.h"
#include <mutex>
#include <condition_variable>
#include <vector>

class MonitorRW : public ControlRW {
public:
    // con_traza=false desactiva el guardado de la traza (modo notraza de E2)
    explicit MonitorRW(Politica p, bool con_traza = true);

    void entrar_lectura() override;
    void salir_lectura() override;
    void entrar_escritura() override;
    void salir_escritura() override;

    std::vector<AnotacionTraza> obtener_traza();

private:
    Politica politica;
    bool con_traza;
    std::mutex m;                       // mutex del monitor
    std::vector<AnotacionTraza> traza;

    // Estado de admisión de las políticas Lectores y Escritores.
    // Solo se toca con 'm' tomado.
    int lect_activas = 0;               // entraron y no han salido
    bool esc_activa = false;
    int lect_esperando = 0;             // pidieron y no han entrado
    int esc_esperando = 0;
    std::condition_variable cv_lectoras;
    std::condition_variable cv_escritoras;

    // Agrega una anotación a la traza. Se llama siempre con 'm' tomado.
    void anotar(Tipo tipo, Accion accion, long turno = -1);

    void entrar_lectura_lectores(std::unique_lock<std::mutex>& lk);
    void entrar_escritura_lectores(std::unique_lock<std::mutex>& lk);
    void entrar_lectura_escritores(std::unique_lock<std::mutex>& lk);
    void entrar_escritura_escritores(std::unique_lock<std::mutex>& lk);

    // Política FIFO (integrante 3): agregar aquí sus atributos (turnos, etc.)
    void entrar_lectura_fifo(std::unique_lock<std::mutex>& lk);
    void entrar_escritura_fifo(std::unique_lock<std::mutex>& lk);
    void salir_lectura_fifo();
    void salir_escritura_fifo();
};