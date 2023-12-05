# Análise Trabalho 2 - Memória Virtual

---
Os relatórios estão nomeados da seguinte forma:
relatorio_[Algoritmo]_[TAMANHO DE MEMÓRIA]_P[TAMANHO DE PÁGINA].txt
---

## Qual o Tamanho Mínimo de Memória que Permite a Execução dos Processos?
Será definida um valor inicial para executar o sistema de forma armazenar 
todos os programas na memória simultaneamente, então a quantidade de memória 
será diminuída consecutivamente.

**Valores Usados**

| Variável              | Valor     | Descrição                                                                   |
|-----------------------|-----------|-----------------------------------------------------------------------------|
| Algoritmo Escalonador | FIFO      | .                                                                           |
| QUANTUM               | 4         | .                                                                           |
| INTERVALO_INTERRUPCAO | 50        | Intervalo entres interrupções do relógio em instruções executadas.          |
| TAM_PAG               | 10        | Tamanho da Página de Memória em Palavras.                                   |
| DELAY_SEC_MEM         | 2*TAM_PAG | Tempo necessário para cada acesso(escrita/leitura) na memória secundária.   | 
| CONTROLE_ZERA_T       | 5         | A cada CONTROLE_ZERA_T Interrupções de Relógio o bit de acesso será zerado. | 

Serão feitas simulações para os dois algoritmos de paginação, FIFO e Segunda 
Chance(SC).

Esta é a tabela dos valores obtidos reduzindo a memória usando o algoritmo FIFO:

| Memória | Tempo de Execução | Falhas de Página |
|---------|-------------------|------------------|
| 920     | 26406             | 92               |
| 450     | 27709             | 145              |
| 300     | 29709             | 264              |
| 230     | 35419             | 455              |
| 120     | 49916             | 1030             |

Com 60 de memória o algoritmo FIFO entrou em thrashing no momento em que um 
segundo processo foi iniciado.
---

Esta é a tabela dos valores obtidos reduzindo a memória usando o algoritmo SC:

| Memória | Tempo de Execução | Falhas de Página |
|---------|-------------------|------------------|
| 920     | 26406             | 92               |
| 450     | 27510             | 153              |
| 300     | 29660             | 251              |
| 230     | 34117             | 427              |
| 120     | 46528             | 908              |
| 60      | 277556            | 11723            |

Com 30 de memória o algoritmo SC entrou em thrashing no momento em que um
segundo processo foi iniciado.
---

Observa-se que o tamanho da memória afeta diretamente o desempenho do 
processador, valores menores de memória geraram mais falhas de páginas e 
consequentemente aumentaram o tempo em que a CPU estava ociosa.  
Também nota-se que o algoritmo SC possuí um desempenho melhor (Para as 
configurações descritas acima, TAM_PAG=10) e 
permitiu um limite minimo de memória menor que o algoritmo FIFO.  
Outro fato interessante a se observar é que diferentes tamanhos de memória 
geram ordens em que os programas são finalizados diferentes.



## Tamanhos de Páginas
"Faça o experimento com 2 tamanhos de página, um bem pequeno (algumas palavras)
e outro pelo menos 4 vezes maior"

Foram escolhidos dois tamanhos de página para comparar, 5 e 25 palavras, 
com tamanho de memória de 300;
As demais variáveis coincidem com o descrito na primeira tabela 
deste texto. 

Tabela *Tamanho de Página e Tempo de Execução*

| Algoritmo | Tamanho de Página | Tempo de Execução |
|-----------|-------------------|-------------------|
| FIFO      | 5                 |  29968            |
| FIFO      | 25                |  36917            |
| SC        | 5                 |  30464            |
| SC        | 25                |  34206            |

Primeiramente comparando somente as diferenças no tamanho de página. Apesar 
de o número de falhas de páginas ser maior com 5 palavras por página, este 
obteve um melhor desempenho.  
Considera-se que a vantagem sobre o tamanho de página 25 seja devido ao 
menor tempo de suspensão dos processos, lembre-se que o tempo de suspensão
(DELAY_SEC_MEM) é definido como 2 vezes o tamanho de página. Para verificar 
isto, basta observar a informação *tempo total* associado ao número de 
vezes que cada processo foi suspenso.  

Quanto aos algoritmos, observa-se um comportamento muito interessante. Para 
um menor tamanho de memória o algoritmo FIFO tem um melhor desempenho, 
enquanto para 25 o desempenho é melhor para SC, por quê?
O algoritmo SC tenta prever qual página não está sendo utilizada 
pelo processo avaliando o bit de acesso, para que isto funcione corretamente 
é necessário que laços de execução caibam inteiramente numa página. Caso 
isto não ocorra, provavelmente somente parte do loop terá o bit de acesso em 
1 no momento de uma troca de página e assim, na próxima iteração do loop 
parte dele terá sido despaginado, gerando novamente uma falha de página.  
Para o tamanho de página 5 e algoritmo SC, isto deve ter sido a causa do 
baixo desempenho. Os laços ou são maiores que 5 instruções, ou encontram-se 
entre duas tabelas de páginas.
 