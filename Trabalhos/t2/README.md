Medição do sistema de memória virtual

Faça seu programa contar o número de falhas de página atendidas para cada processo. Defina a memória principal com tamanho suficiente para conter todas as páginas de todos os processos. Executando dessa forma com paginação por demanda, cada processo deve gerar um número de falhas de página igual ao número de páginas que corresponde ao seu tamanho.

Altere o tamanho da memória para ter metade das páginas necessárias para conter todos os processos. Execute nessa configuração e compare o número de falhas de página e os tempos de execução dos programas em relação à configuração anterior.

Continue diminuindo o tamanho da memória pela metade e refazendo a comparação.

Qual o tamanho mínimo de memória que permite a execução dos processos?

Faça o experimento com 2 tamanhos de página, um bem pequeno (algumas palavras) e outro pelo menos 4 vezes maior. Analise as diferenças no comportamento do sistema. Faça um relatório com suas observações e análises.

O SO deve manter algumas métricas, que devem ser apresentadas no final da execução (quando o init morrer):


Informações SO:  
- [x] número de processos criados
- [x] tempo total de execução
- [x] tempo total em que o sistema ficou ocioso (todos os processos bloqueados)
- [x] número de interrupções recebidas de cada tipo
- [x] número de preempções

Informações Processo:  
- [x] tempo de retorno de cada processo (diferença entre data do término e 
  da criação)
- [x] número de preempções de cada processo
- [x] número de vezes que cada processo entrou em cada estado (pronto, 
  bloqueado, executando)
- [x] tempo total de cada processo em cada estado (pronto, bloqueado, 
  executando)
- [x] tempo médio de resposta de cada processo (tempo médio em estado pronto)

** Gere um relatório de execuções do sistema em diferentes configurações. **