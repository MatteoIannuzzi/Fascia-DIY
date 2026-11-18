/*
 * Prototipo.c — Fascia fitness fai-da-te (prototipo solo software)
 *
 * Legge le letture grezze dei sensori da un file CSV (una riga per lettura)
 * e calcola: passi, battito cardiaco, ossigeno nel sangue (SpO2), calorie,
 * stato (fermo / cammino / corsa) e percentuale di batteria.
 *
 * Formato di ogni riga in ingresso (10 valori separati da virgole):
 *   tempo_ms,rosso,ir,acc_x,acc_y,acc_z,giro_x,giro_y,giro_z,batteria_mv
 *
 * Compilare:  gcc -Wall Prototipo.c -o prot -lm
 * Eseguire:   ./prot < dati.csv
 */

#include <stdio.h>  // printf e scanf, per stampare e leggere
#include <stdint.h> // tipi interi di grandezza precisa (uint32_t, int16_t, uint16_t)
#include <math.h>   // funzioni matematiche: sqrt, fabs, atan2

/* ===================== TIPI DI DATO DEI SENSORI ===================== */

typedef struct MAX30102 // sensore ottico: misura la luce che torna indietro dalla pelle
{
    uint32_t rosso; // luce rossa riflessa (valore grezzo da 0 a 262143)
    uint32_t ir;    // luce infrarossa riflessa (valore grezzo da 0 a 262143)
} MAX30102;         // nome corto del tipo, creato da typedef

typedef struct Accelerometro // accelerometro: misura le accelerazioni del polso
{
    int16_t x, y, z; // valori grezzi sui tre assi (da -32768 a +32767)
} Accelerometro;     // nome corto del tipo

typedef struct Giroscopio // giroscopio: misura quanto velocemente ruota il polso
{
    int16_t x, y, z; // valori grezzi di rotazione sui tre assi (da -32768 a +32767)
} Giroscopio;        // nome corto del tipo

typedef uint16_t Batteria; // batteria, espressa come tensione in millivolt (es. 4200 = 4,2 V)

/* ============================ COSTANTI ============================== */

#define SENS 16384.0         // sensibilità accelerometro (±2 g) dal datasheet: 16384 valori grezzi = 1 g
#define PI 3.14159265358979  // pi greco, serve per convertire i radianti in gradi
#define SOGLIA_IR 20000      // soglia IR: sopra c'è la pelle davanti al sensore, sotto no
#define BATT_PIENA_MV 4200.0 // tensione della batteria carica al 100%
#define BATT_VUOTA_MV 3300.0 // tensione della batteria scarica (0%)
#define PESO_KG 72.0         // il tuo peso in kg, serve per le calorie
#define ETA_ANNI 23.0        // la tua età in anni, serve per le calorie

/* =================== CALCOLI CON UNA SOLA LETTURA =================== */

double converti_in_g(int16_t valore_grezzo) // trasforma il numero grezzo del sensore in accelerazione (g)
{
    return valore_grezzo / SENS; // divido per la sensibilità: 16384 diventa 1,0 g
}

double forza_movimento(double x, double y, double z) // forza totale del movimento in g, indipendente da come è girato il polso
{
    return sqrt(x * x + y * y + z * z); // Pitagora in 3D: radice della somma dei quadrati
}

void posizione_polso(double gx, double gy, double gz) // guarda su quale asse c'è la gravità e dice come è girato il polso
{
    if (gz > 0.8)                                                        // gravità quasi tutta su z positivo
        printf("Posizione:           polso piatto, schermo verso l'alto\n"); // schermo rivolto al cielo
    else if (gz < -0.8)                                                  // gravità quasi tutta su z negativo
        printf("Posizione:           schermo verso il basso\n");         // schermo rivolto al pavimento
    else if (fabs(gx) > 0.8)                                             // gravità quasi tutta su x (positivo o negativo)
        printf("Posizione:           braccio verticale\n");              // braccio lungo il fianco o alzato
    else if (fabs(gy) > 0.8)                                             // gravità quasi tutta su y
        printf("Posizione:           polso girato di lato\n");           // polso ruotato di 90 gradi
    else                                                                 // nessun asse prevale
        printf("Posizione:           inclinato\n");                      // posizione intermedia
}

double inclinazione_avanti(double gx, double gy, double gz) // di quanti gradi il braccio è inclinato in avanti o indietro
{
    return atan2(gx, sqrt(gy * gy + gz * gz)) * 180.0 / PI; // angolo dalla divisione della gravità, convertito in gradi
}

double rotazione_polso(double gy, double gz) // di quanti gradi il polso è ruotato verso destra o sinistra
{
    return atan2(gy, gz) * 180.0 / PI; // angolo tra asse y e asse z, convertito in gradi
}

int fascia_indossata(uint32_t ir) // controlla se c'è la pelle davanti al sensore ottico (1 = sì, 0 = no)
{
    return ir > SOGLIA_IR; // il confronto vale già 1 (vero) o 0 (falso)
}

double percentuale_batteria(Batteria mv) // trasforma la tensione della batteria in percentuale di carica
{
    double p = (mv - BATT_VUOTA_MV) / (BATT_PIENA_MV - BATT_VUOTA_MV) * 100.0; // proporzione tra vuota e piena

    if (p > 100.0) // in carica la tensione può superare 4200 mV
        p = 100.0; // il massimo è 100%
    if (p < 0.0)   // molto scarica può scendere sotto 3300 mV
        p = 0.0;   // il minimo è 0%

    return p; // restituisco la percentuale
}

/* ================== CALCOLI CON TANTE LETTURE ======================= */
/* Le variabili "static" si ricordano il loro valore tra una chiamata   */
/* e l'altra: servono perché ogni funzione viene chiamata per ogni riga */

int conta_passi(double forza, double t) // conta i passi: un passo è una cresta della forza sopra 1,3 g
{
    static double filtrato = 1.0;      // forza "lisciata", senza i piccoli tremolii
    static int sopra = 0;              // 1 = siamo sulla cresta di un passo
    static double ultimo_passo = -1.0; // quando c'è stato l'ultimo passo (secondi)
    static int passi = 0;              // totale dei passi contati

    filtrato = filtrato + 0.3 * (forza - filtrato); // mi avvicino solo del 30% al valore nuovo

    if (!sopra && filtrato > 1.3 && t - ultimo_passo > 0.25) // non ero sulla cresta, supero 1,3 g e sono passati almeno 0,25 s
    {
        sopra = 1;        // ora sono sulla cresta
        passi++;          // conto un passo
        ultimo_passo = t; // mi segno quando è successo
    }
    else if (sopra && filtrato < 1.1) // ero sulla cresta e scendo sotto 1,1 g
    {
        sopra = 0; // cresta finita: posso contare il passo successivo
    }

    return passi; // restituisco il totale dei passi
}

double calcola_battito(uint32_t ir, double t) // calcola il battito (bpm) dall'onda della luce infrarossa
{
    static double media = 0.0;           // parte fissa del segnale (pelle, ossa, tessuti)
    static double altezza = 0.0;         // quanto è alta l'onda del battito
    static int sopra = 0;                // 1 = siamo sulla cresta di un battito
    static double ultimo_battito = -1.0; // quando c'è stato l'ultimo battito (secondi)
    static double bpm = 0.0;             // battiti al minuto calcolati

    if (media == 0.0)                    // alla primissima lettura
        media = ir;                      // parto dal valore letto
    media = media + 0.02 * (ir - media); // la media segue il segnale molto lentamente (2% alla volta)
    double onda = ir - media;            // tolgo la parte fissa: resta solo l'onda del battito

    if (fabs(onda) > altezza)      // se l'onda è più alta di quella ricordata
        altezza = fabs(onda);      // aggiorno l'altezza
    else                           // altrimenti
        altezza = altezza * 0.995; // la abbasso un pochino, così si adatta se il segnale si indebolisce

    if (!sopra && onda > 0.5 * altezza) // non ero sulla cresta e l'onda supera metà della sua altezza
    {
        sopra = 1;                 // ora sono sulla cresta: è un battito
        if (ultimo_battito >= 0.0) // se c'era già stato un battito prima
        {
            double nuovo = 60.0 / (t - ultimo_battito); // battiti al minuto dal tempo tra due battiti
            if (nuovo > 40.0 && nuovo < 220.0)          // scarto i valori impossibili
            {
                if (bpm == 0.0)                   // se è il primo valore valido
                    bpm = nuovo;                  // lo prendo così com'è
                else                              // altrimenti
                    bpm = 0.7 * bpm + 0.3 * nuovo; // tengo il 70% del vecchio e il 30% del nuovo, per non far saltare il numero
            }
        }
        ultimo_battito = t; // mi segno quando c'è stato questo battito
    }
    else if (sopra && onda < 0.0) // ero sulla cresta e l'onda torna sotto zero
    {
        sopra = 0; // cresta finita: posso contare il battito successivo
    }

    return bpm; // restituisco il battito
}

double calcola_ossigeno(uint32_t rosso, uint32_t ir) // calcola l'ossigeno nel sangue confrontando quanto oscillano rosso e IR
{
    static uint32_t max_r, min_r, max_i, min_i; // valore massimo e minimo di rosso e IR nel blocco di letture
    static double somma_r = 0.0, somma_i = 0.0; // somme per calcolare le medie
    static int campioni = 0;                    // quante letture ho raccolto nel blocco
    static double spo2 = 0.0;                   // ossigeno calcolato (%)

    if (campioni == 0) // inizio di un nuovo blocco
    {
        max_r = min_r = rosso;   // massimo e minimo partono dal primo valore rosso
        max_i = min_i = ir;      // massimo e minimo partono dal primo valore IR
        somma_r = somma_i = 0.0; // azzero le somme
    }

    if (rosso > max_r) max_r = rosso; // aggiorno il massimo del rosso
    if (rosso < min_r) min_r = rosso; // aggiorno il minimo del rosso
    if (ir > max_i) max_i = ir;       // aggiorno il massimo dell'IR
    if (ir < min_i) min_i = ir;       // aggiorno il minimo dell'IR
    somma_r += rosso;                 // aggiungo il rosso alla somma
    somma_i += ir;                    // aggiungo l'IR alla somma
    campioni++;                       // una lettura in più nel blocco

    if (campioni == 100) // ogni 100 letture (2 secondi) faccio il calcolo
    {
        double media_r = somma_r / campioni; // media del rosso
        double media_i = somma_i / campioni; // media dell'IR
        double R = ((max_r - min_r) / media_r) / ((max_i - min_i) / media_i); // oscillazione relativa del rosso diviso quella dell'IR
        double nuovo = 110.0 - 25.0 * R; // formula empirica: trasforma R in percentuale
        if (nuovo > 100.0)               // l'ossigeno non può superare il 100%
            nuovo = 100.0;               // lo blocco al 100%

        if (spo2 == 0.0)                    // se è il primo calcolo
            spo2 = nuovo;                   // lo prendo così com'è
        else                                // altrimenti
            spo2 = 0.8 * spo2 + 0.2 * nuovo; // lo aggiorno piano, per evitare salti

        campioni = 0; // ricomincio un nuovo blocco
    }

    return spo2; // restituisco l'ossigeno
}

double calorie_al_minuto(double bpm) // calorie bruciate al minuto, dal battito (formula di Keytel 2005, per uomini)
{
    double kcal = (-55.0969 + 0.6309 * bpm + 0.1988 * PESO_KG + 0.2017 * ETA_ANNI) / 4.184; // la formula dà kJ, divido per 4,184 per avere kcal
    if (kcal < 0.0) // con battiti molto bassi la formula può dare un numero negativo
        kcal = 0.0; // le calorie non possono essere negative
    return kcal;    // restituisco le kcal al minuto
}

/* ======================== PROGRAMMA PRINCIPALE ====================== */

int main(void)
{
    uint32_t tempo_ms;     // istante della lettura, in millisecondi
    MAX30102 o;            // lettura del sensore ottico
    Accelerometro a;       // lettura dell'accelerometro
    Giroscopio g;          // lettura del giroscopio
    Batteria b;            // tensione della batteria
    int contatore = 0;     // quante righe abbiamo letto
    double calorie = 0.0;  // calorie totali bruciate
    double t_prima = 0.0;  // tempo della lettura precedente (secondi)
    int passi_prima = 0;   // passi contati alla stampa precedente

    printf("  Tempo | Stato        | Battito | Ossigeno | Passi | Calorie    | Batteria\n"); // intestazione della tabella
    printf("--------+--------------+---------+----------+-------+------------+---------\n"); // riga di separazione

    while (scanf("%u,%u,%u,%hd,%hd,%hd,%hd,%hd,%hd,%hu",                                // legge una riga: 10 numeri separati da virgole
                 &tempo_ms, &o.rosso, &o.ir, &a.x, &a.y, &a.z, &g.x, &g.y, &g.z, &b) == 10) // continua finché riesce a leggerli tutti e 10
    {
        contatore++;                  // una riga in più letta
        double t = tempo_ms / 1000.0; // tempo in secondi

        double gx = converti_in_g(a.x);             // accelerazione su x in g
        double gy = converti_in_g(a.y);             // accelerazione su y in g
        double gz = converti_in_g(a.z);             // accelerazione su z in g
        double forza = forza_movimento(gx, gy, gz); // forza totale del movimento

        int passi = conta_passi(forza, t);             // aggiorna il conteggio dei passi
        double bpm = calcola_battito(o.ir, t);         // aggiorna il battito
        double spo2 = calcola_ossigeno(o.rosso, o.ir); // aggiorna l'ossigeno

        if (bpm > 0.0)                                                // solo quando il battito è già stato calcolato
            calorie += calorie_al_minuto(bpm) * (t - t_prima) / 60.0; // aggiungo le calorie bruciate dall'ultima lettura
        t_prima = t;                                                  // mi ricordo il tempo per la prossima lettura

        if (contatore % 250 == 0) // ogni 250 letture (5 secondi) stampo una riga
        {
            printf("%5.0f s | ", t); // tempo in secondi

            int passi_recenti = passi - passi_prima; // passi fatti negli ultimi 5 secondi
            passi_prima = passi;                     // mi ricordo il totale per la prossima stampa

            if (passi_recenti == 0)        // nessun passo
                printf("fermo        | "); // sono fermo
            else if (passi_recenti <= 10)  // fino a 2 passi al secondo
                printf("cammino      | "); // sto camminando
            else                           // più di 2 passi al secondo
                printf("corsa        | "); // sto correndo

            printf("%3.0f bpm | %5.1f %%  | %5d | %5.2f kcal | %.0f%%\n", // battito, ossigeno, passi, calorie, batteria
                   bpm, spo2, passi, calorie, percentuale_batteria(b));   // valori da stampare
        }
    }

    printf("\nRighe lette: %d\n", contatore); // alla fine dico quante righe ho letto

    return 0; // il programma è terminato correttamente
}
