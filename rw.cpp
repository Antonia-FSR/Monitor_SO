// rw.cpp — programa principal (integrante 1): R1, R2, R7, mediciones E1/E2
#include "comun.h"
#include "monitor_rw.h"
#include "control_atomico_rw.h"
#include "verificador.h"
#include <atomic>
#include <stdexcept>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <limits>
#include <memory>
#include <algorithm>
#include <ctime>
#include <iomanip>

thread_local int id_hebra = -1;

// Contadores de diagnóstico R2 (separados de la lógica de admisión)
std::atomic<int> diag_lectoras_activas{0};
std::atomic<int> diag_escritoras_activas{0};
std::atomic<int> diag_max_lectoras{0};
std::atomic<long> diag_inconsistencias{0};
std::atomic<long> diag_violaciones_exclusion{0};

struct StatsHebra {
    long long suma_espera_ns = 0;
    long long max_espera_ns = 0;
};

void actualizar_max_lectoras(int actual) {
    int prev = diag_max_lectoras.load(std::memory_order_relaxed);
    while (actual > prev &&
           !diag_max_lectoras.compare_exchange_weak(prev, actual, std::memory_order_relaxed)) {
    }
}

double obtener_tiempo_cpu_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (ts.tv_sec * 1000.0) + (ts.tv_nsec / 1e6);
}

int main(int argc, char* argv[]) {
    if (argc < 8 || argc > 9) {
        std::cerr << "Uso: ./rw <modo> <politica> <L> <E> <K> <dur_lec_us> <dur_esc_us> [notraza]\n";
        return 1;
    }

    std::string modo = argv[1];
    std::string pol_str = argv[2];
    long L = 0, E = 0, K = 0, dur_lec_us = 0, dur_esc_us = 0;
    bool sin_traza = false;

    try {
        size_t pos = 0;
        L = std::stol(argv[3], &pos);
        if (pos != std::string(argv[3]).size()) throw std::invalid_argument("L");
        E = std::stol(argv[4], &pos);
        if (pos != std::string(argv[4]).size()) throw std::invalid_argument("E");
        K = std::stol(argv[5], &pos);
        if (pos != std::string(argv[5]).size()) throw std::invalid_argument("K");
        dur_lec_us = std::stol(argv[6], &pos);
        if (pos != std::string(argv[6]).size()) throw std::invalid_argument("dur_lec");
        dur_esc_us = std::stol(argv[7], &pos);
        if (pos != std::string(argv[7]).size()) throw std::invalid_argument("dur_esc");
    } catch (...) {
        std::cerr << "Error: Parametros numericos invalidos.\n";
        return 1;
    }

    if (argc == 9) {
        if (std::string(argv[8]) == "notraza") {
            sin_traza = true;
        } else {
            std::cerr << "Error: Octavo parametro opcional debe ser 'notraza'.\n";
            return 1;
        }
    }

    // Validaciones R1
    if (modo != "cond" && modo != "atomic") {
        std::cerr << "Error: Modo invalido. Use 'cond' o 'atomic'.\n";
        return 1;
    }

    Politica politica;
    if (pol_str == "lectores") {
        politica = Politica::Lectores;
    } else if (pol_str == "escritores") {
        politica = Politica::Escritores;
    } else if (pol_str == "fifo") {
        politica = Politica::Fifo;
    } else {
        std::cerr << "Error: Politica invalida. Use 'lectores', 'escritores' o 'fifo'.\n";
        return 1;
    }

    if (modo == "atomic" && politica != Politica::Escritores) {
        std::cerr << "Error: El modo 'atomic' solo implementa la politica 'escritores'.\n";
        return 1;
    }

    if (L < 1 || E < 1 || (L + E) > 1000 || K < 1 || dur_lec_us < 0 || dur_esc_us < 0) {
        std::cerr << "Error: Rangos invalidos (L>=1, E>=1, L+E<=1000, K>=1, duraciones>=0).\n";
        return 1;
    }

    // Verificar que 2EK y (L+E)K quepan en un long (R1)
    long max_long = std::numeric_limits<long>::max();
    if (E > max_long / 2 || (2 * E) > max_long / K || (L + E) > max_long / K) {
        std::cerr << "Error: Desbordamiento de long en 2EK o (L+E)K.\n";
        return 1;
    }

    Registro registro;
    std::unique_ptr<ControlRW> control;
    if (modo == "cond") {
        control = std::make_unique<MonitorRW>(politica, !sin_traza);

    } else {
        control = std::make_unique<ControlAtomicoRW>();
    }

    std::vector<StatsHebra> stats(L + E);
    std::vector<std::thread> hebras;
    hebras.reserve(L + E);

    // Inicio de medición (E2): desde antes de comenzar el trabajo hasta completar todos los join()
    auto t_pared_inicio = std::chrono::steady_clock::now();
    double t_cpu_inicio = obtener_tiempo_cpu_ms();

    bool error_creacion = false;

    // Crear L hebras lectoras (IDs lógicos: 0 .. L-1)
    for (int i = 0; i < L && !error_creacion; ++i) {
        try {
            hebras.emplace_back([&, i]() {
                id_hebra = i;
                for (long op = 0; op < K; ++op) {
                    auto t0 = std::chrono::steady_clock::now();
                    control->entrar_lectura();
                    auto t1 = std::chrono::steady_clock::now();

                    long long espera = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
                    stats[i].suma_espera_ns += espera;
                    stats[i].max_espera_ns = std::max(stats[i].max_espera_ns, espera);

                    // --- Intervalo admitido de lectura (R2 y R7: pausa fuera del monitor) ---
                    int act_lec = diag_lectoras_activas.fetch_add(1, std::memory_order_seq_cst) + 1;
                    actualizar_max_lectoras(act_lec);
                    if (diag_escritoras_activas.load(std::memory_order_seq_cst) > 0) {
                        diag_violaciones_exclusion.fetch_add(1, std::memory_order_relaxed);
                    }

                    long v = registro.version;
                    long dv = registro.doble_version;
                    if (dv != 2 * v) {
                        diag_inconsistencias.fetch_add(1, std::memory_order_relaxed);
                    }

                    if (dur_lec_us > 0) {
                        std::this_thread::sleep_for(std::chrono::microseconds(dur_lec_us));
                    }

                    diag_lectoras_activas.fetch_sub(1, std::memory_order_seq_cst);
                    // -----------------------------------------------------------------------

                    control->salir_lectura();
                }
            });
        } catch (const std::exception& ex) {
            std::cerr << "Error al crear hebra lectora " << i << ": " << ex.what() << "\n";
            error_creacion = true;
        }
    }

    // Crear E hebras escritoras (IDs lógicos: L .. L+E-1)
    for (int j = 0; j < E && !error_creacion; ++j) {
        int id = L + j;
        try {
            hebras.emplace_back([&, id]() {
                id_hebra = id;
                for (long op = 0; op < K; ++op) {
                    auto t0 = std::chrono::steady_clock::now();
                    control->entrar_escritura();
                    auto t1 = std::chrono::steady_clock::now();

                    long long espera = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
                    stats[id].suma_espera_ns += espera;
                    stats[id].max_espera_ns = std::max(stats[id].max_espera_ns, espera);

                    // --- Intervalo admitido de escritura (R2 y R7: pausa fuera del monitor) ---
                    int act_esc = diag_escritoras_activas.fetch_add(1, std::memory_order_seq_cst) + 1;
                    if (act_esc > 1 || diag_lectoras_activas.load(std::memory_order_seq_cst) > 0) {
                        diag_violaciones_exclusion.fetch_add(1, std::memory_order_relaxed);
                    }

                    registro.version++;
                    registro.doble_version = 2 * registro.version;

                    if (dur_esc_us > 0) {
                        std::this_thread::sleep_for(std::chrono::microseconds(dur_esc_us));
                    }

                    diag_escritoras_activas.fetch_sub(1, std::memory_order_seq_cst);
                    // -------------------------------------------------------------------------

                    control->salir_escritura();
                }
            });
        } catch (const std::exception& ex) {
            std::cerr << "Error al crear hebra escritora " << id << ": " << ex.what() << "\n";
            error_creacion = true;
        }
    }

    // Esperar a todas las hebras creadas (R1)
    for (auto& h : hebras) {
        if (h.joinable()) {
            h.join();
        }
    }

    if (error_creacion) {
        return 1;
    }

    double t_cpu_fin = obtener_tiempo_cpu_ms();
    auto t_pared_fin = std::chrono::steady_clock::now();
    double tiempo_pared_ms = std::chrono::duration<double, std::milli>(t_pared_fin - t_pared_inicio).count();
    double tiempo_cpu_ms = t_cpu_fin - t_cpu_inicio;

    // Estadísticas de espera (E2)
    long long total_espera_lec_ns = 0, max_espera_lec_ns = 0;
    for (int i = 0; i < L; ++i) {
        total_espera_lec_ns += stats[i].suma_espera_ns;
        max_espera_lec_ns = std::max(max_espera_lec_ns, stats[i].max_espera_ns);
    }

    long long total_espera_esc_ns = 0, max_espera_esc_ns = 0;
    for (int j = 0; j < E; ++j) {
        total_espera_esc_ns += stats[L + j].suma_espera_ns;
        max_espera_esc_ns = std::max(max_espera_esc_ns, stats[L + j].max_espera_ns);
    }

    double prom_espera_lec_us = (total_espera_lec_ns / 1000.0) / (L * K);
    double max_espera_lec_us = max_espera_lec_ns / 1000.0;
    double prom_espera_esc_us = (total_espera_esc_ns / 1000.0) / (E * K);
    double max_espera_esc_us = max_espera_esc_ns / 1000.0;

    // Verificación de invariantes finales (R2)
    bool correcto = (registro.version == E * K) &&
                    (registro.doble_version == 2 * E * K) &&
                    (diag_inconsistencias.load() == 0) &&
                    (diag_violaciones_exclusion.load() == 0);

    // Impresión de resumen (Sección 5, E1 y E2)
    std::cout << "modo=" << modo
              << " politica=" << pol_str
              << " ops=" << (L + E) * K
              << " version=" << registro.version
              << " doble_version=" << registro.doble_version
              << " inconsistencias=" << diag_inconsistencias.load()
              << " max_lectoras=" << diag_max_lectoras.load()
              << std::fixed << std::setprecision(3)
              << " t_pared_ms=" << tiempo_pared_ms
              << " t_cpu_ms=" << tiempo_cpu_ms
              << " espera_lec_prom_us=" << prom_espera_lec_us
              << " espera_lec_max_us=" << max_espera_lec_us
              << " espera_esc_prom_us=" << prom_espera_esc_us
              << " espera_esc_max_us=" << max_espera_esc_us
              << " correcto=" << (correcto ? "SI" : "NO");

    if (modo == "cond" && !sin_traza) {
        auto* mon = dynamic_cast<MonitorRW*>(control.get());
        ResultadoVerificacion res = verificar_traza(mon->obtener_traza(), politica);
        std::cout << " politica_ok=" << (res.politica_ok ? "SI" : "NO")
                  << " evidencia=" << res.evidencia;
    }

    /*
    // NOTA PARA R10 / E1 / E2 (ControlAtomicoRW):
    // Cuando agreguen `FallosAtomic obtener_fallos_totales() const;` en ControlAtomicoRW,
    // descomentar este bloque para imprimir los fallos por causa:
    if (modo == "atomic") {
        auto* atom = dynamic_cast<ControlAtomicoRW*>(control.get());
        FallosAtomic fallos = atom->obtener_fallos_totales();
        long total_fallos = fallos.ocupado + fallos.esperando + fallos.cas;
        std::cout << " fallos_total=" << total_fallos
                  << " fallos_ocupado=" << fallos.ocupado
                  << " fallos_esperando=" << fallos.esperando
                  << " fallos_cas=" << fallos.cas;
    }
    */

    std::cout << "\n";
    return correcto ? 0 : 1;
}