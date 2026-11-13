// =============================================================
// WattControl - Gestor inteligente de energia (El Nino 2026)
// Negocio: Taller de mecanizado (Ultimo digito documento: 2 - 3)
// Evidencia de Aprendizaje 2 - Programacion Orientada a Objetos
// =============================================================

// --- Librerias estandar --------------------------------------
#include <iostream>   // cout y cin (entrada y salida por consola)
#include <iomanip>    // setw, fixed y setprecision (formato de tablas)
#include <string>     // tipo string para nombres y textos
#include <cstdlib>    // rand() y srand() para generacion aleatoria

using namespace std;

// =============================================================
// 1. DIRECTIVAS DEL PREPROCESADOR (#define)
// =============================================================
#define HORAS_DIA 24       // Duracion de la simulacion diaria (24 horas)
#define MAX_CARGAS 3       // Cantidad maxima de cargas del taller (1 critica + 2 aplazables)
#define SEMILLA 2026       // Semilla fija para reproducibilidad de precios y embalses

// =============================================================
// 2. CONSTANTES DEL NEGOCIO: TALLER DE MECANIZADO (const)
//    Ninguna regla ni validacion utiliza numeros literales.
// =============================================================

// --- Potencias nominales de las tres cargas (en kW) ---
const float POTENCIA_CRITICA = 1.5;      // Iluminacion y computadores (nunca se apaga)
const float POTENCIA_TORNO = 7.5;        // Aplazable 1: Torno CNC
const float POTENCIA_COMPRESOR = 3.0;    // Aplazable 2: Compresor de aire

// --- Potencia maxima contratada con el operador de red (en kW) ---
const float POTENCIA_CONTRATADA = 10.0;

// --- Umbrales de precio de bolsa (en COP/kWh) y embalses (en %) ---
const float PRECIO_CARO = 750.0;             // Umbral de precio "caro" para el taller
const float PRECIO_EMERGENCIA = 1500.0;      // Precio extremo para emergencia de racionamiento
const float NIVEL_ALERTA_EMBALSES = 40.0;    // Umbral de riesgo de racionamiento (< 40 %)

// --- Franja de hora pico (18:00 a 20:00 h inclusive) ---
const int PICO_INICIO = 18;
const int PICO_FIN = 20;

// --- Horas minimas de operacion diaria (justificadas en el informe) ---
// Torno CNC: minimo 6 horas para cumplir el mecanizado de pedidos del dia
const int HORAS_MIN_TORNO = 6;
// Compresor: minimo 4 horas para mantener presion neumatica y limpieza del taller
const int HORAS_MIN_COMPRESOR = 4;

// --- Constantes de validacion de entradas y fallas de sensores ---
const int HORA_MIN = 0;
const int HORA_MAX = 23;
const float PRECIO_MIN_VALIDO = 0.0;
const float NIVEL_MIN = 0.0;
const float NIVEL_MAX = 100.0;
const int FALLA_CADA = 20;               // 1 de cada 20 lecturas falla (continue)

// --- Rangos para simular el nivel de los embalses nacionales (%) ---
const int EMBALSE_MIN_SIM = 25;          // Permite que en algunas horas baje de 40 %
const int EMBALSE_RANGO_SIM = 71;        // 25 + (0..70) = entre 25 % y 95 %

// =============================================================
// 3. CLASE BASE ABSTRACTA: Carga
//    Define los atributos comunes (# protected) y el metodo
//    virtual puro debeEncender() para polimorfismo.
// =============================================================
class Carga {
protected:
    string nombre;
    float potenciaKW;
    bool encendida;
    int horasUso;

public:
    // Constructor: inicializa nombre, potencia, estado apagado y 0 horas de uso
    Carga(string n, float p) {
        nombre = n;
        potenciaKW = p;
        encendida = false;
        horasUso = 0;
    }

    // Destructor virtual obligatorio en clases base polimorficas
    virtual ~Carga() {}

    // Metodo virtual puro (= 0): convierte a Carga en clase ABSTRACTA.
    // Cada clase hija implementa su propia regla de encendido (Polimorfismo).
    virtual bool debeEncender(int hora, float precio, float nivel) = 0;

    // Identifica si la carga puede ser apagada por exceso de potencia (Regla 6)
    virtual bool esAplazable() = 0;

    // Enciende la carga y suma una hora de uso
    void encender() {
        encendida = true;
        horasUso++;
    }

    // Apaga la carga
    void apagar() {
        encendida = false;
    }

    // Revierte el encendido de la hora actual si fue apagada por Regla 6
    void deshacerEncendido() {
        if (encendida) {
            encendida = false;
            if (horasUso > 0) {
                horasUso--;
            }
        }
    }

    // Reinicia el estado y el contador de horas antes de una nueva simulacion
    void reiniciar() {
        encendida = false;
        horasUso = 0;
    }

    // Calcula el costo energetico de esta carga en una hora (kW * COP/kWh)
    float costoHora(float precio) {
        if (encendida) {
            return potenciaKW * precio;
        }
        return 0.0;
    }

    // Getters publicos de consulta
    string getNombre() { return nombre; }
    float getPotencia() { return potenciaKW; }
    bool estaEncendida() { return encendida; }
    int getHorasUso() { return horasUso; }
};

// =============================================================
// 4. HERENCIA: CargaCritica ES UNA Carga
//    Regla 1: Siempre permanece encendida.
// =============================================================
class CargaCritica : public Carga {
public:
    CargaCritica(string n, float p) : Carga(n, p) {}

    // Implementacion polimorfica: la carga critica nunca se apaga
    bool debeEncender(int hora, float precio, float nivel) override {
        return true;
    }

    bool esAplazable() override {
        return false;
    }
};

// =============================================================
// 5. HERENCIA: CargaAplazable ES UNA Carga
//    Reglas 2 a 5: Se aplaza con condicion compuesta (&&, ||, !)
//    si embalses < 40 %, si es hora pico o si precio > 750.
// =============================================================
class CargaAplazable : public Carga {
private:
    int horasMinimas;

public:
    CargaAplazable(string n, float p, int hMin) : Carga(n, p) {
        horasMinimas = hMin;
    }

    // Implementacion polimorfica con condicion logica compuesta
    bool debeEncender(int hora, float precio, float nivel) override {
        // Regla 2: Embalses por debajo del nivel de alerta (< 40 %)
        bool alertaEmbalses = (nivel < NIVEL_ALERTA_EMBALSES);

        // Regla 3: Hora pico (18, 19 o 20 h)
        bool esHoraPico = (hora >= PICO_INICIO && hora <= PICO_FIN);

        // Regla 4: Precio mayor al precio "caro" del negocio (> 750 COP/kWh)
        bool esPrecioCaro = (precio > PRECIO_CARO);

        // Regla 5: Enciende unicamente si NO se cumple ninguna condicion de aplazamiento
        return !(alertaEmbalses || esHoraPico || esPrecioCaro);
    }

    bool esAplazable() override {
        return true;
    }

    // Verifica si la maquina alcanzo su meta diaria de horas de trabajo
    bool cumpleHorasMinimas() {
        return horasUso >= horasMinimas;
    }

    int getHorasMinimas() { return horasMinimas; }
};

int main() { return 0; }
  