#include "verificador.h"
#include <iostream>

namespace
{
    void violacion(size_t i, const char *regla)
    {
        std::cerr << "VIOLACION en anotacion " << i << ": " << regla << "\n";
    }
}

// Recorre la traza reconstruyendo cuántas lectoras y escritoras hay activas
// (entraron y no han salido) y pendientes (pidieron y no han entrado), y en
// cada Entra comprueba las reglas. Como las anotaciones se agregan con el
// mutex tomado, el orden del vector es el orden real de los cambios.
ResultadoVerificacion verificar_traza(const std::vector<AnotacionTraza> &traza, Politica politica)
{
    ResultadoVerificacion res;

    long lect_act = 0, esc_act = 0;
    long lect_pend = 0, esc_pend = 0;
    long entradas = 0; // cuántos Entra van (lo usa la regla FIFO)

    for (size_t i = 0; i < traza.size(); ++i)
    {
        const AnotacionTraza &a = traza[i];
        const bool es_lect = (a.tipo == Tipo::Lectora);

        switch (a.accion)
        {
        case Accion::Pide:
            // Evidencia (Escritores): la lectora pidió con escritoras pendientes.
            if (politica == Politica::Escritores && es_lect && esc_pend > 0)
            {
                res.evidencia++;
            }
            // Lectores: si no hay escritora activa, la lectora no debe esperar,
            // así que la anotación siguiente tiene que ser su propio Entra.
            if (politica == Politica::Lectores && es_lect && esc_act == 0)
            {
                bool ok = (i + 1 < traza.size()) &&
                          traza[i + 1].hebra == a.hebra &&
                          traza[i + 1].tipo == Tipo::Lectora &&
                          traza[i + 1].accion == Accion::Entra;
                if (!ok)
                {
                    violacion(i, "Lectores: lectora que pide sin escritora activa no entra de inmediato");
                    res.politica_ok = false;
                }
            }
            if (es_lect)
                lect_pend++;
            else
                esc_pend++;
            break;

        case Accion::Entra:
            // Regla de todas las políticas: exclusión de la escritora.
            if (es_lect)
            {
                if (esc_act > 0)
                {
                    violacion(i, "Todas: lectora entra con escritora activa");
                    res.politica_ok = false;
                }
            }
            else if (lect_act > 0 || esc_act > 0)
            {
                violacion(i, "Todas: escritora entra con lectoras o escritora activas");
                res.politica_ok = false;
            }

            if (politica == Politica::Lectores)
            {
                if (!es_lect && lect_pend > 0)
                {
                    violacion(i, "Lectores: escritora entra con lectoras pendientes");
                    res.politica_ok = false;
                }
                // Evidencia: la lectora entró mientras había escritoras pendientes.
                if (es_lect && esc_pend > 0)
                    res.evidencia++;
            }
            else if (politica == Politica::Escritores)
            {
                if (es_lect && esc_pend > 0)
                {
                    violacion(i, "Escritores: lectora entra con escritoras pendientes");
                    res.politica_ok = false;
                }
            }
            else if (politica == Politica::Fifo)
            {
                if (a.turno != entradas)
                {
                    violacion(i, "FIFO: ingreso fuera del orden de turnos");
                    res.politica_ok = false;
                }

                if (es_lect && lect_act > 0)
                {
                    res.evidencia++;
                }
            }

            if (es_lect)
            {
                lect_pend--;
                lect_act++;
            }
            else
            {
                esc_pend--;
                esc_act++;
            }
            entradas++;
            break;

        case Accion::Sale:
            if ((es_lect && lect_act == 0) || (!es_lect && esc_act == 0))
            {
                violacion(i, "Traza mal formada: Sale sin Entra previo");
                res.politica_ok = false;
            }
            if (es_lect)
                lect_act--;
            else
                esc_act--;
            break;
        }
    }
    return res;
}