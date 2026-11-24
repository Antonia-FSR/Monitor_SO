// comun.h — Definiciones compartidas por todo el grupo (Paso 0).
// No cambiar sin avisar al grupo: todos dependen de este archivo.
#pragma once
#include <cstdint>
#include <vector>

// Interfaz común (Sección 1 del enunciado)
class ControlRW {
public:
    virtual ~ControlRW() = default;
    virtual void entrar_lectura() = 0;
    virtual void salir_lectura() = 0;
    virtual void entrar_escritura() = 0;
    virtual void salir_escritura() = 0;
};

enum class Politica { Lectores, Escritores, Fifo };

// Registro compartido (R2)
struct Registro {
    long version = 0;
    long doble_version = 0;
};

// Traza de admisión (R6)
enum class Tipo { Lectora, Escritora };
enum class Accion { Pide, Entra, Sale };

struct AnotacionTraza {
    int hebra;      // identificador lógico de la hebra
    Tipo tipo;
    Accion accion;
    long turno;     // turno FIFO; -1 en las otras políticas
};

// Cada hebra la asigna al comenzar (se define en rw.cpp)
extern thread_local int id_hebra;

// Fallos de admisión del modo atomic (R10), por hebra.
// Si en un intento se cumple más de una causa, cuenta como 'ocupado'.
struct FallosAtomic {
    long ocupado = 0;    // escritora activa o (para escritora) lectoras activas
    long esperando = 0;  // solo lectoras: había escritoras esperando
    long cas = 0;        // CAS fallido (incluye fallos espurios de _weak)
};
