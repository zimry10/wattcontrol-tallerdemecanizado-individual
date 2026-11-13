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

// =============================================================
// 6. COMPOSICION: Medidor (es parte del Negocio)
//    Acumula kWh y costos tanto con gestor como sin gestor.
// =============================================================
class Medidor {
private:
    float kwhCon;
    float kwhSin;
    float costoCon;
    float costoSin;

public:
    Medidor() {
        reiniciar();
    }

    void reiniciar() {
        kwhCon = 0.0;
        kwhSin = 0.0;
        costoCon = 0.0;
        costoSin = 0.0;
    }

    // Registra el consumo (kWh) y costo ($) de una hora simulada
    void registrarHora(float kwhConGestor, float kwhSinGestor, float precio) {
        kwhCon += kwhConGestor;
        kwhSin += kwhSinGestor;
        costoCon += kwhConGestor * precio;
        costoSin += kwhSinGestor * precio;
    }

    // Calcula el ahorro economico total en pesos (COP)
    float calcularAhorroPesos() {
        return costoSin - costoCon;
    }

    // Calcula el porcentaje de ahorro respecto al escenario sin gestor
    float calcularAhorroPorcentaje() {
        if (costoSin > 0.0) {
            return ((costoSin - costoCon) / costoSin) * 100.0;
        }
        return 0.0;
    }

    float getKwhCon() { return kwhCon; }
    float getKwhSin() { return kwhSin; }
    float getCostoCon() { return costoCon; }
    float getCostoSin() { return costoSin; }
};

// =============================================================
// 7. ASOCIACION: Mercado
//    Entrega el precio horario (rangos reales XM) y el nivel
//    de los embalses nacionales cuando el Negocio los solicita.
// =============================================================
class Mercado {
public:
    // Genera el precio de bolsa (COP/kWh) dentro del rango real de la franja horaria
    float precioHora(int hora) {
        int minimo, maximo;
        if (hora <= 6) {
            minimo = 398; maximo = 640;     // Madrugada (0 a 6 h)
        } else if (hora <= 14) {
            minimo = 456; maximo = 970;     // Manana y mediodia (7 a 14 h)
        } else if (hora <= 17) {
            minimo = 596; maximo = 1040;    // Tarde (15 a 17 h)
        } else if (hora <= 20) {
            minimo = 705; maximo = 1612;    // Hora pico (18 a 20 h)
        } else {
            minimo = 565; maximo = 970;     // Noche (21 a 23 h)
        }
        return minimo + rand() % (maximo - minimo + 1);
    }

    // Genera el porcentaje de nivel de los embalses hidroelectricos (%)
    float nivelEmbalses(int hora) {
        return EMBALSE_MIN_SIM + rand() % EMBALSE_RANGO_SIM;
    }
};

// =============================================================
// 8. CLASE COORDINADORA: Negocio (Taller de mecanizado)
//    Integra Composicion (Medidor), Agregacion (Carga*[3]) y
//    Asociacion (Mercado*), y ejecuta las reglas de control.
// =============================================================
class Negocio {
private:
    string nombre;
    Medidor medidor;                 // Composicion: nace y muere con el Negocio
    Carga* cargas[MAX_CARGAS];       // Agregacion: arreglo de punteros a Carga
    int numCargas;
    Mercado* mercado;                // Asociacion: puntero a Mercado externo
    float potenciaContratada;
    int horasSimuladas;
    int fallas;

    // Regla 6: Si la potencia encendida supera la potencia contratada (10 kW),
    // apaga la carga aplazable de mayor potencia y muestra una alerta.
    void verificarPotencia(bool mostrarAlerta) {
        float potenciaActiva = 0.0;
        for (int i = 0; i < numCargas; i++) {
            if (cargas[i]->estaEncendida()) {
                potenciaActiva += cargas[i]->getPotencia();
            }
        }

        while (potenciaActiva > potenciaContratada) {
            int indiceMayor = -1;
            float mayorPotencia = 0.0;

            // Busca la carga aplazable encendida con mayor potencia (ej. Torno CNC: 7.5 kW)
            for (int i = 0; i < numCargas; i++) {
                if (cargas[i]->estaEncendida() && cargas[i]->esAplazable()) {
                    if (cargas[i]->getPotencia() > mayorPotencia) {
                        mayorPotencia = cargas[i]->getPotencia();
                        indiceMayor = i;
                    }
                }
            }

            // Si no hay mas cargas aplazables encendidas, se detiene el ajuste
            if (indiceMayor == -1) {
                break;
            }

            if (mostrarAlerta) {
                cout << "  [ALERTA REGLA 6] Potencia solicitada (" << potenciaActiva
                     << " kW) supera contratada (" << potenciaContratada
                     << " kW) -> Se apaga: " << cargas[indiceMayor]->getNombre() << endl;
            }

            potenciaActiva -= cargas[indiceMayor]->getPotencia();
            cargas[indiceMayor]->deshacerEncendido();
        }
    }

public:
    // Constructor del Negocio
    Negocio(string n, float pMax, Mercado* m) {
        nombre = n;
        potenciaContratada = pMax;
        mercado = m;
        numCargas = 0;
        horasSimuladas = 0;
        fallas = 0;
    }

    // Agrega una carga creada en el exterior (Agregacion)
    void agregarCarga(Carga* c) {
        if (numCargas < MAX_CARGAS) {
            cargas[numCargas] = c;
            numCargas++;
        }
    }

    // Muestra la configuracion del negocio y sus cargas conectadas
    void mostrar() {
        cout << "============================================================" << endl;
        cout << "Negocio: " << nombre
             << " | Potencia contratada: " << potenciaContratada << " kW" << endl;
        cout << "Cargas registradas:" << endl;
        for (int i = 0; i < numCargas; i++) {
            cout << "  - " << setw(28) << left << cargas[i]->getNombre()
                 << right << fixed << setprecision(1) << cargas[i]->getPotencia() << " kW" << endl;
        }
        cout << "============================================================" << endl;
    }

    // Opcion 2: Evalua que haria cada carga ante una lectura puntual (sin alterar estadisticas)
    void evaluar(int hora, float precio, float nivel) {
        cout << fixed << setprecision(1);
        cout << "\nEvaluacion para Hora: " << hora << " h | Precio: $" << precio
             << " COP/kWh | Embalses: " << nivel << " %" << endl;

        if (nivel < NIVEL_ALERTA_EMBALSES) {
            cout << "  [ALERTA REGLA 2] Riesgo de racionamiento (embalses < "
                 << NIVEL_ALERTA_EMBALSES << " %): solo funciona carga critica." << endl;
        }

        // Paso 1: Cada carga decide polimorficamente (Reglas 1 a 5)
        float potenciaSolicitada = 0.0;
        bool quiereEncender[MAX_CARGAS];
        for (int i = 0; i < numCargas; i++) {
            quiereEncender[i] = cargas[i]->debeEncender(hora, precio, nivel);
            if (quiereEncender[i]) {
                potenciaSolicitada += cargas[i]->getPotencia();
            }
        }

        // Paso 2: Si supera potencia contratada, se apaga la aplazable de mayor potencia (Regla 6)
        if (potenciaSolicitada > potenciaContratada) {
            int indiceMayor = -1;
            float mayorPotencia = 0.0;
            for (int i = 0; i < numCargas; i++) {
                if (quiereEncender[i] && cargas[i]->esAplazable()) {
                    if (cargas[i]->getPotencia() > mayorPotencia) {
                        mayorPotencia = cargas[i]->getPotencia();
                        indiceMayor = i;
                    }
                }
            }
            if (indiceMayor != -1) {
                cout << "  [ALERTA REGLA 6] Potencia (" << potenciaSolicitada
                     << " kW) > contratada (" << potenciaContratada
                     << " kW) -> Se apaga: " << cargas[indiceMayor]->getNombre() << endl;
                quiereEncender[indiceMayor] = false;
            }
        }

        // Paso 3: Muestra la decision final para cada carga
        for (int i = 0; i < numCargas; i++) {
            cout << "  " << setw(28) << left << cargas[i]->getNombre() << right
                 << (quiereEncender[i] ? "ENCENDER" : "APAGAR / APLAZAR") << endl;
        }
    }

    // Opcion 3: Simula un dia completo de 24 horas con gestor y sin gestor
    void simularDia() {
        // Reinicia el medidor y los contadores de cada carga para la nueva simulacion
        medidor.reiniciar();
        for (int i = 0; i < numCargas; i++) {
            cargas[i]->reiniciar();
        }
        horasSimuladas = 0;
        fallas = 0;

        // Potencia total del escenario SIN gestor (las 3 cargas encendidas todo el dia)
        float potenciaSinGestor = 0.0;
        for (int i = 0; i < numCargas; i++) {
            potenciaSinGestor += cargas[i]->getPotencia();
        }

        cout << fixed << setprecision(0);
        cout << "\n--- SIMULACION DE 24 HORAS: " << nombre << " ---" << endl;
        cout << setw(5) << "Hora" << setw(8) << "Precio" << setw(9) << "Embalse"
             << setw(14) << "Critica" << setw(12) << "Torno" << setw(12) << "Compresor"
             << setw(8) << "kW" << setw(10) << "Costo" << endl;

        // Ciclo principal de 24 horas
        for (int h = 0; h < HORAS_DIA; h++) {
            // Asociacion: consulta al Mercado el precio y el nivel de embalses de la hora h
            float precio = mercado->precioHora(h);
            float nivel = mercado->nivelEmbalses(h);

            // break: Emergencia de racionamiento si precio > 1500 Y embalses < 40 %
            if (precio > PRECIO_EMERGENCIA && nivel < NIVEL_ALERTA_EMBALSES) {
                cout << setw(5) << h << setw(8) << precio << setw(8) << nivel << "%"
                     << "  EMERGENCIA DE RACIONAMIENTO: se detiene la simulacion (break)" << endl;
                break;
            }

            horasSimuladas++;

            // continue: Falla de lectura del sensor (1 de cada 20 veces)
            // En caso de falla, esa hora SOLO funciona la carga critica
            if (rand() % FALLA_CADA == 0) {
                float kwhCritica = 0.0;
                for (int i = 0; i < numCargas; i++) {
                    if (!cargas[i]->esAplazable()) {
                        cargas[i]->encender();
                        kwhCritica += cargas[i]->getPotencia();
                    } else {
                        cargas[i]->apagar();
                    }
                }
                medidor.registrarHora(kwhCritica, potenciaSinGestor, precio);
                fallas++;
                cout << setw(5) << h << setw(8) << "--" << setw(8) << nivel << "%"
                     << "  Falla de lectura: solo opera carga critica (continue)" << endl;
                continue;
            }

            // Operacion normal CON gestor:
            // 1. Cada carga aplica su regla polimorfica (Reglas 1 a 5)
            for (int i = 0; i < numCargas; i++) {
                if (cargas[i]->debeEncender(h, precio, nivel)) {
                    cargas[i]->encender();
                } else {
                    cargas[i]->apagar();
                }
            }

            // 2. Verifica potencia contratada y apaga la aplazable mayor si excede 10 kW (Regla 6)
            verificarPotencia(false);

            // 3. Calcula kWh y costo real de esta hora con gestor
            float kwhHora = 0.0;
            float costoHora = 0.0;
            for (int i = 0; i < numCargas; i++) {
                if (cargas[i]->estaEncendida()) {
                    kwhHora += cargas[i]->getPotencia();
                    costoHora += cargas[i]->costoHora(precio);
                }
            }

            // 4. Registra en el Medidor (composicion) los datos con y sin gestor
            medidor.registrarHora(kwhHora, potenciaSinGestor, precio);

            // 5. Imprime la fila de resultados de la hora h
            cout << setw(5) << h << setw(8) << precio << setw(8) << nivel << "%";
            cout << setw(14) << (cargas[0]->estaEncendida() ? "ON" : "off");
            cout << setw(12) << (cargas[1]->estaEncendida() ? "ON" : "off");
            cout << setw(12) << (cargas[2]->estaEncendida() ? "ON" : "off");
            cout << setw(8) << setprecision(1) << kwhHora << setprecision(0)
                 << setw(10) << costoHora << endl;
        }

        cout << "Simulacion finalizada. Seleccione la opcion 4 para ver el resumen." << endl;
    }

    // Opcion 4: Resumen de resultados, cumplimiento de horas minimas y ahorro
    void resumen() {
        cout << "\n============================================================" << endl;
        cout << "RESUMEN DE OPERACION: " << nombre << endl;
        cout << "============================================================" << endl;
        cout << "Horas simuladas: " << horasSimuladas << " de " << HORAS_DIA
             << " | Fallas de lectura: " << fallas << endl;
        cout << "Potencia contratada: " << fixed << setprecision(1) << potenciaContratada << " kW\n" << endl;

        bool produccionIncompleta = false;

        // Detalle de uso por cada carga y verificacion de horas minimas
        for (int i = 0; i < numCargas; i++) {
            float kwhCarga = cargas[i]->getHorasUso() * cargas[i]->getPotencia();
            cout << "  - " << setw(28) << left << cargas[i]->getNombre() << right
                 << cargas[i]->getHorasUso() << " h | "
                 << fixed << setprecision(1) << kwhCarga << " kWh";

            // Si es carga aplazable, verifica si cumplio sus horas minimas
            if (cargas[i]->esAplazable()) {
                CargaAplazable* aplazable = static_cast<CargaAplazable*>(cargas[i]);
                cout << " (minimo requerido: " << aplazable->getHorasMinimas() << " h)";
                if (!aplazable->cumpleHorasMinimas()) {
                    produccionIncompleta = true;
                }
            }
            cout << endl;
        }

        // Alerta obligatoria si alguna carga aplazable no cumplio sus horas minimas
        if (produccionIncompleta) {
            cout << "\n  [!] ALERTA: produccion incompleta (una o mas cargas aplazables "
                 << "no alcanzaron sus horas minimas)." << endl;
        } else {
            cout << "\n  [OK] Produccion diaria completada dentro de las horas minimas." << endl;
        }

        // Comparativa energetica y economica desde el Medidor
        cout << "\n--- Comparativa Energetica y Economica (Medidor) ---" << endl;
        cout << fixed << setprecision(1);
        cout << "Consumo con gestor:  " << medidor.getKwhCon() << " kWh" << endl;
        cout << "Consumo sin gestor:  " << medidor.getKwhSin() << " kWh" << endl;
        cout << fixed << setprecision(0);
        cout << "Costo con gestor:   $" << medidor.getCostoCon() << " COP" << endl;
        cout << "Costo sin gestor:   $" << medidor.getCostoSin() << " COP" << endl;
        cout << "Ahorro obtenido:    $" << medidor.calcularAhorroPesos() << " COP ("
             << fixed << setprecision(1) << medidor.calcularAhorroPorcentaje() << " %)" << endl;
        cout << "============================================================" << endl;
    }
};

// =============================================================
// 9. FUNCION PRINCIPAL (main)
//    Crea las instancias del Taller de Mecanizado y ejecuta
//    el menu interactivo con do-while, switch y validaciones.
// =============================================================
int main() {
    // Inicializa la semilla aleatoria
    srand(SEMILLA);

    // Asociacion: el Mercado existe de manera independiente al Negocio
    Mercado mercado;

    // Agregacion: las cargas se crean afuera y luego se agregan al Negocio
    CargaCritica iluminacionPC("Iluminacion y computadores", POTENCIA_CRITICA);
    CargaAplazable tornoCNC("Torno CNC", POTENCIA_TORNO, HORAS_MIN_TORNO);
    CargaAplazable compresor("Compresor", POTENCIA_COMPRESOR, HORAS_MIN_COMPRESOR);

    // Se instancia el Negocio (crea internamente su Medidor por Composicion)
    Negocio taller("Taller de Mecanizado", POTENCIA_CONTRATADA, &mercado);
    taller.agregarCarga(&iluminacionPC);
    taller.agregarCarga(&tornoCNC);
    taller.agregarCarga(&compresor);

    taller.mostrar();

    // Variables para la lectura manual (inician con valores validos por defecto)
    int hora = 10;
    float precio = 500.0;
    float nivelEmbalses = 65.0;
    bool haySimulacion = false;
    int opcion;

    // Menu principal con do-while y switch
    do {
        cout << "\n===== WattControl - Taller de Mecanizado =====" << endl;
        cout << "1. Ingresar lectura manual (hora, precio, embalses)" << endl;
        cout << "2. Evaluar decision de cada carga" << endl;
        cout << "3. Simular un dia completo (24 h)" << endl;
        cout << "4. Ver resumen de la ultima simulacion" << endl;
        cout << "0. Salir" << endl;
        cout << "Opcion: ";
        cin >> opcion;

        switch (opcion) {
        case 1:
            // Validacion de hora (0 a 23)
            do {
                cout << "Hora (" << HORA_MIN << " a " << HORA_MAX << "): ";
                cin >> hora;
                if (hora < HORA_MIN || hora > HORA_MAX) {
                    cout << "  Error: la hora debe estar entre " << HORA_MIN
                         << " y " << HORA_MAX << "." << endl;
                }
            } while (hora < HORA_MIN || hora > HORA_MAX);

            // Validacion de precio (> 0)
            do {
                cout << "Precio de bolsa (COP/kWh > " << PRECIO_MIN_VALIDO << "): ";
                cin >> precio;
                if (precio <= PRECIO_MIN_VALIDO) {
                    cout << "  Error: el precio debe ser mayor que "
                         << PRECIO_MIN_VALIDO << "." << endl;
                }
            } while (precio <= PRECIO_MIN_VALIDO);

            // Validacion de nivel de embalses (0 a 100 %)
            do {
                cout << "Nivel de embalses (" << NIVEL_MIN << " a " << NIVEL_MAX << " %): ";
                cin >> nivelEmbalses;
                if (nivelEmbalses < NIVEL_MIN || nivelEmbalses > NIVEL_MAX) {
                    cout << "  Error: el nivel de embalses debe estar entre "
                         << NIVEL_MIN << " % y " << NIVEL_MAX << " %." << endl;
                }
            } while (nivelEmbalses < NIVEL_MIN || nivelEmbalses > NIVEL_MAX);

            cout << "Lectura manual guardada correctamente." << endl;
            break;

        case 2:
            taller.evaluar(hora, precio, nivelEmbalses);
            break;

        case 3:
            taller.simularDia();
            haySimulacion = true;
            break;

        case 4:
            if (haySimulacion) taller.resumen();
            else cout << "Primero debe simular un dia con la opcion 3." << endl;
            break;

        case 0:
            cout << "Saliendo de WattControl. Hasta luego." << endl;
            break;

        default:
            cout << "Opcion invalida. Intente nuevamente." << endl;
            break;
        }
        if (opcion != 0) {
           cout << endl << "Presione Enter para continuar...";
           cin.ignore();
           cin.get();
        }
    } while (opcion != 0);

    return 0;
}
  