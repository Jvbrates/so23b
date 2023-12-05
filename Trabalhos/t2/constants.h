//
// Created by jvbrates on 12/4/23.
//

#ifndef SO23B_CONSTANTS_H
#define SO23B_CONSTANTS_H


//VARIAVEIS IMPORTANTES!

// Tamanho de página de memória
#define TAM_PAGINA 10


// intervalo entre interrupções do relógio
#define INTERVALO_INTERRUPCAO 50   // em instruções executadas

//Velocidade da memória secundária
#define DELAY_SEC_MEM (2*TAM_PAGINA)

//espaço de quadros reservados para o SO
#define MEM_SYS 100  // Palavras de memória reservados
#define OFFSET_MEM (MEM_SYS/TAM_PAGINA) // Palavras para Páginas

#define QUANTUM 4
#define NUM_ES 4 // 4 terminais

// valor em interrupcoes_do_relogio em que os bits acesso sao gerados
/* Se você colocar um valor muita baixo vai quebrar o desempenho,
 * Caso processo "p" seja suspenso ele ira esperar 200(DELAY_SEC_MEM) tempos,
 * se nesse tempo a pagina requerida ter seu bit acesso zerado, ela poderá
 * ser removida e entao quando o processo "p" acordar, o mesmo problema
 * ocorrerá novamente*/
#define CONTROLE_ZERA_T 5
/* Leia CONTROLE_ZERA_T*INTERVALO_INTERRUPÇÃO como o tempo para a rotina de
 * zerar os * bits de acesso
 * */

// constantes
#define MEM_TAM (300 + MEM_SYS)       // tamanho da memória principal 100 para o SO




#endif//SO23B_CONSTANTS_H
