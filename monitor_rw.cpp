#include "monitor_rw.h"
#include <stdexcept>

MonitorRW::MonitorRW(Politica p, bool con_traza_) : politica(p), con_traza(con_traza_) {}

void MonitorRW::anotar(Tipo tipo, Accion accion, long turno)
{
    if (!con_traza)
        return;
    traza.push_back(AnotacionTraza{id_hebra, tipo, accion, turno});
}

std::vector<AnotacionTraza> MonitorRW::obtener_traza()
{
    std::unique_lock<std::mutex> lk(m);
    return traza;
}

// Cada método público toma el mutex al comenzar y lo libera al retornar
// (el unique_lock lo suelta solo), así se ejecutan en exclusión mutua.
// Luego delegan según la política.
void MonitorRW::entrar_lectura()
{
    std::unique_lock<std::mutex> lk(m);
    switch (politica)
    {
    case Politica::Lectores:
        entrar_lectura_lectores(lk);
        break;
    case Politica::Escritores:
        entrar_lectura_escritores(lk);
        break;
    case Politica::Fifo:
        entrar_lectura_fifo(lk);
        break;
    }
}

void MonitorRW::entrar_escritura()
{
    std::unique_lock<std::mutex> lk(m);
    switch (politica)
    {
    case Politica::Lectores:
        entrar_escritura_lectores(lk);
        break;
    case Politica::Escritores:
        entrar_escritura_escritores(lk);
        break;
    case Politica::Fifo:
        entrar_escritura_fifo(lk);
        break;
    }
}

void MonitorRW::salir_lectura()
{
    std::unique_lock<std::mutex> lk(m);
    if (politica == Politica::Fifo)
    {
        salir_lectura_fifo();
        return;
    }

    anotar(Tipo::Lectora, Accion::Sale);
    lect_activas--;
    // Solo las escritoras pueden quedar habilitadas cuando sale la última lectora.
    if (lect_activas == 0)
    {
        cv_escritoras.notify_all();
    }
}

void MonitorRW::salir_escritura()
{
    std::unique_lock<std::mutex> lk(m);
    if (politica == Politica::Fifo)
    {
        salir_escritura_fifo();
        return;
    }

    anotar(Tipo::Escritora, Accion::Sale);
    esc_activa = false;
    // Puede quedar habilitada cualquiera de las dos; cada hebra revisa su condición.
    // Se usa notify_all() porque un notify_one() podría despertar a la hebra equivocada.
    cv_lectoras.notify_all();
    cv_escritoras.notify_all();
}

// Política A (R3), prioridad de lectores.
// La lectora solo espera si hay una escritora activa. Si no la hay, entra
// aunque haya escritoras esperando. La escritora, en cambio, también cede
// ante lectoras activas y pendientes, y por eso puede sufrir inanición.
void MonitorRW::entrar_lectura_lectores(std::unique_lock<std::mutex> &lk)
{
    anotar(Tipo::Lectora, Accion::Pide);
    lect_esperando++;
    cv_lectoras.wait(lk, [&]
                     { return !esc_activa; });
    lect_esperando--;
    lect_activas++;
    anotar(Tipo::Lectora, Accion::Entra);
}

void MonitorRW::entrar_escritura_lectores(std::unique_lock<std::mutex> &lk)
{
    anotar(Tipo::Escritora, Accion::Pide);
    esc_esperando++;
    cv_escritoras.wait(lk, [&]
                       { return !esc_activa && lect_activas == 0 && lect_esperando == 0; });
    esc_esperando--;
    esc_activa = true;
    anotar(Tipo::Escritora, Accion::Entra);
}

// Política B (R4), prioridad de escritores.
// Desde que hay una escritora esperando o activa, las lectoras nuevas no
// entran; las que ya estaban dentro terminan normalmente. Aquí las lectoras
// pueden sufrir inanición.
void MonitorRW::entrar_lectura_escritores(std::unique_lock<std::mutex> &lk)
{
    anotar(Tipo::Lectora, Accion::Pide);
    lect_esperando++;
    cv_lectoras.wait(lk, [&]
                     { return !esc_activa && esc_esperando == 0; });
    lect_esperando--;
    lect_activas++;
    anotar(Tipo::Lectora, Accion::Entra);
}

void MonitorRW::entrar_escritura_escritores(std::unique_lock<std::mutex> &lk)
{
    anotar(Tipo::Escritora, Accion::Pide);
    esc_esperando++;
    cv_escritoras.wait(lk, [&]
                       { return !esc_activa && lect_activas == 0; });
    esc_esperando--;
    esc_activa = true;
    anotar(Tipo::Escritora, Accion::Entra);
}

// Política C (R5), FIFO: pendiente (integrante 3).
// Hay que anotar Pide al adquirir el mutex (con turno consecutivo desde 0),
// Entra justo antes de retornar y Sale al salir.
void MonitorRW::entrar_lectura_fifo(std::unique_lock<std::mutex> &lk)
{
    long turno = siguiente_turno++;
    anotar(Tipo::Lectora, Accion::Pide, turno);

    cv_fifo.wait(lk, [&]
                 { return turno == turno_actual && !esc_activa; });

    lect_activas++;
    turnos_activos[id_hebra] = turno;
    turno_actual++;

    anotar(Tipo::Lectora, Accion::Entra, turno);
    cv_fifo.notify_all();
}
void MonitorRW::entrar_escritura_fifo(std::unique_lock<std::mutex> &lk)
{
    long turno = siguiente_turno++;
    anotar(Tipo::Escritora, Accion::Pide, turno);

    cv_fifo.wait(lk, [&]
                 { return turno == turno_actual && !esc_activa && lect_activas == 0; });

    esc_activa = true;
    turnos_activos[id_hebra] = turno;
    turno_actual++;

    anotar(Tipo::Escritora, Accion::Entra, turno);
}
void MonitorRW::salir_lectura_fifo()
{
    long turno = turnos_activos.at(id_hebra);

    lect_activas--;
    turnos_activos.erase(id_hebra);

    anotar(Tipo::Lectora, Accion::Sale, turno);

    if (lect_activas == 0)
    {
        cv_fifo.notify_all();
    }
}
void MonitorRW::salir_escritura_fifo() {
    long turno = turnos_activos.at(id_hebra);

    esc_activa = false;
    turnos_activos.erase(id_hebra);

    anotar(Tipo::Escritora, Accion::Sale, turno);

    cv_fifo.notify_all();
}