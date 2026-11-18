/*
 * genera_dati.c — Generatore di dati simulati per la fascia fitness
 *
 * Simula 60 secondi da fermo e 60 secondi di corsa, con 50 letture al
 * secondo (6000 righe), e le stampa nel formato che Prototipo.c si aspetta:
 *   tempo_ms,rosso,ir,acc_x,acc_y,acc_z,giro_x,giro_y,giro_z,batteria_mv
 *
 * Compilare:  gcc -Wall genera_dati.c -o genera -lm
 * Eseguire:   ./genera > dati.csv
 */

#include <stdio.h>  // printf, per stampare le righe
#include <stdlib.h> // rand e srand, per i numeri casuali
#include <math.h>   // sin e cos, per creare le onde

#define PI 3.14159265358979 // pi greco
#define FREQ 50             // letture al secondo
#define DURATA_S 120        // durata totale della simulazione in secondi
#define INIZIO_CORSA 60     // secondo in cui inizi a correre

double rumore(double ampiezza) // numero casuale tra -ampiezza e +ampiezza: simula il disturbo dei sensori
{
    return ampiezza * ((double)rand() / RAND_MAX * 2.0 - 1.0); // rand/RAND_MAX va da 0 a 1, lo porto tra -1 e +1 e lo moltiplico
}

int main(void)
{
    double fase_cuore = 0.0; // a che punto del battito siamo (1 = un battito completo)
    double fase_passo = 0.0; // a che punto del passo siamo (1 = un passo completo)

    srand(42); // seme fisso: ogni volta escono gli stessi numeri casuali, così i risultati sono ripetibili

    for (int i = 0; i < DURATA_S * FREQ; i++) // una volta per ogni lettura: 120 s × 50 = 6000
    {
        double t = (double)i / FREQ;     // tempo di questa lettura in secondi
        int corsa = (t >= INIZIO_CORSA); // 1 = sto correndo, 0 = fermo

        /* --- battito: 72 bpm da fermo, sale fino a 150 in 30 secondi di corsa --- */
        double bpm = 72.0;                        // battito da fermo
        if (corsa)                                // se sto correndo
        {
            double k = (t - INIZIO_CORSA) / 30.0; // quanto sono avanti nei primi 30 s di corsa (da 0 a 1)
            if (k > 1.0)                          // dopo 30 secondi
                k = 1.0;                          // resto al massimo
            bpm = 72.0 + k * (150.0 - 72.0);      // il battito sale gradualmente da 72 a 150
        }
        fase_cuore += bpm / 60.0 / FREQ;          // avanzo nel battito: più è alto il bpm, più avanzo veloce
        double onda = sin(2.0 * PI * fase_cuore); // onda del battito, va da -1 a +1

        /* --- sensore ottico: parte fissa + piccola onda del battito + rumore --- */
        int ir = (int)(50000 + 400 * onda + rumore(50));    // luce infrarossa
        int rosso = (int)(30000 + 125 * onda + rumore(15)); // luce rossa

        /* --- accelerometro: da fermo senti solo la gravità (16384 = 1 g su z) --- */
        double ax = rumore(300);         // asse x: quasi zero
        double ay = rumore(300);         // asse y: quasi zero
        double az = 16384 + rumore(300); // asse z: 1 g di gravità

        /* --- giroscopio: da fermo il polso non ruota --- */
        double gx = rumore(10); // rotazione su x: quasi zero
        double gy = rumore(10); // rotazione su y: quasi zero
        double gz = rumore(10); // rotazione su z: quasi zero

        if (corsa) // correndo aggiungo il movimento dei passi
        {
            fase_passo += 2.8 / FREQ;                   // 2,8 passi al secondo
            double passo = sin(2.0 * PI * fase_passo);  // onda del passo, va da -1 a +1
            ax += 5000 * passo;                         // il braccio oscilla avanti e indietro
            az += 13000 * passo;                        // colpo su e giù a ogni passo
            gx += 3000 * passo;                         // il polso ruota mentre il braccio oscilla
            gy += 1500 * cos(2.0 * PI * fase_passo);    // rotazione sfasata sull'altro asse
        }

        /* --- batteria: parte da 4150 mV e si scarica piano --- */
        int batteria = (int)(4150 - t * 0.2); // perde 0,2 mV al secondo

        printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", // stampo una riga con i 10 valori separati da virgole
               i * 1000 / FREQ, rosso, ir,        // tempo in ms e sensore ottico
               (int)ax, (int)ay, (int)az,         // accelerometro
               (int)gx, (int)gy, (int)gz,         // giroscopio
               batteria);                         // batteria
    }

    return 0; // il programma è terminato correttamente
}
