#include "so.h"
#include "irq.h"
#include "programa.h"
#include "instrucao.h"
#include "tabpag.h"
#include "process_mng.h"
#include "scheduler_interface.h"
#include "map_tpag.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>



// Não tem processos nem memória virtual, mas é preciso usar a paginação,
//   pelo menos para implementar relocação, já que os programas estão sendo
//   todos montados para serem executados no endereço 0 e o endereço 0
//   físico é usado pelo hardware nas interrupções.
// Os programas vão ser carregados no início de um quadro, e usar quantos
//   quadros forem necessárias. Para isso a variável quadro_livre vai conter
//   o número do primeiro quadro da memória principal que ainda não foi usado.
//   Na carga do processo, a tabela de páginas (deveria ter uma por processo,
//   mas não tem processo) é alterada para que o endereço virtual 0 resulte
//   no quadro onde o programa foi carregado.

struct so_t {
  cpu_t *cpu;
  mem_t *mem;           // Memória Principal
  mem_t *sec_mem;       // Memória Secundária

  /* Como a memória secundária é desnecessariamente grande, os processos estão
   * sendo gravados de forma linear sem serem apagados ou reaproveitados.
   * Logo, sec_mem_quadro_livre é só um contador++*/
  int sec_mem_quadro_livre;       // Proximo quadro livre da memoria secundária
  int sec_req;          // Numero de requisições ao disco não atendidas
  int sec_tempo_livre;  // Próximo tempo livre do disco

  mmu_t *mmu;
  console_t *console;
  relogio_t *relogio;

  process_table_t * processTable;  // Administrador Tabela de processos
  scheduler_t * scheduler;         // Administrador Escalonador
  int runningP;                    // PID do úlitmo processo em execução
  process_t *p_runningP;           // Ponteiro do úlitmo processo em execução

  /*
   * Para cada pendência/requisição de escrita/leitura em dado dipositivo o valor
   * é incrementado, quando a pendência é atendida, o valor é decrementado.
   * */
  // [][0] -- Escrita
  // [][1] -- Leitura
  int es_pendencias[NUM_ES][2];

  metricas *log;                  // Administrador Logs do Sistema
  int tempo_chamda_anterior;      // Tempo da ultima interrupção, usado para log


  // quando tiver memória virtual, o controle de memória livre e ocupada
  //   é mais completo que isso


  /* Mapeador tabela de páginas e memória, mapeia e le/altera os bits da
   *tabela de páginas, mas não altera a memória. Define qual frame deve ser
   * usado e quando deve gravá-lo.*/
  map_tapg_t *mng_tpag;
};


typedef struct cpu_info_t_so {
  int PC;
  int X;
  int A;
  // estado interno da CPU
  int complemento;
  cpu_modo_t modo;
}cpu_info_t_so;

static int PID_COUNTER;
static int controle_zera;
//---------------------------------------------------------------------------


// função de tratamento de interrupção (entrada no SO)
static err_t so_trata_interrupcao(void *argC, int reg_A);

// funções auxiliares
static ret_so_carrega_programa so_carrega_programa(so_t *self, char *nome_do_executavel);
static int so_copia_str_do_processo(so_t *self, int tam, char str[tam],
                                    int end_virt, process_t *p);
// Cópia de uma memória para outra, from mem1 to mem2
static int so_mem_to_mem(mem_t *mem1, int mem1_addr, mem_t *mem2, int mem2_addr, int range);

//Processos
static process_t *process_save(so_t *self, process_t *p);
static err_t process_recover(so_t *self, process_t *process);
static int so_cria_proc(so_t *self, char nome[100]);
static int so_registra_proc(so_t *self, ret_so_carrega_programa address);
static int so_mata_proc(so_t *self, process_t *p);
int so_start_ptable_sched(so_t *self);



so_t *so_cria(cpu_t *cpu, mem_t *mem, mem_t *sec_mem, mmu_t *mmu,
              console_t *console, relogio_t *relogio)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;

  if(self->log)
    free(self->log);
  self->log = log_init("relatorio\0");

  self->cpu = cpu;
  self->mem = mem;
  self->console = console;
  self->relogio = relogio;
  memset(self->es_pendencias, 0, sizeof(self->es_pendencias));

  // quando a CPU executar uma instrução CHAMAC, deve chamar essa função
  cpu_define_chamaC(self->cpu, so_trata_interrupcao, self);
  // coloca o tratador de interrupção na memória



  // programa a interrupção do relógio
  rel_escr(self->relogio, 2, INTERVALO_INTERRUPCAO);

  self->tempo_chamda_anterior = rel_agora(self->relogio);


  // T2 < ----
  self->mmu = mmu;
  self->sec_mem = sec_mem;

  mmu_define_tabpag(self->mmu, NULL);
  // define o primeiro quadro livre de memória como o seguinte àquele que
  //   contém o endereço 99 (as 100 primeiras posições de memória (pelo menos)
  //   não vão ser usadas por programas de usuário)
  self->sec_mem_quadro_livre = 0;

  self->sec_req = 0;
  self->sec_tempo_livre = 0;


  //int num_pages = mem_tam(self->mem)/TAM_PAGINA  - 10;

  // self->mng_tpag = map_tpag_create(num_pages);

  //mem_escreve(self->mem, 10, CHAMAC);
  mmu_escreve(self->mmu, 10, CHAMAC, supervisor);

  //mem_escreve(self->mem, 11, RETI);
  mmu_escreve(self->mmu, 11, RETI, supervisor);


  return self;
}


void so_destroi(so_t *self)
{

  if(self->processTable)
    ptable_destruct(self->processTable);

  if(self->scheduler)
    sched_destruct(self->scheduler);

  if(self->mng_tpag)
    map_tpag_destruct(self->mng_tpag);

  if(self->log)
    free(self->log);


  cpu_define_chamaC(self->cpu, NULL, NULL);
  free(self);
}


// Tratamento de interrupção

// funções auxiliares para tratar cada tipo de interrupção
static err_t so_trata_irq(so_t *self, int irq);
static err_t so_trata_irq_reset(so_t *self);
static err_t so_trata_irq_err_cpu(so_t *self);
static err_t so_trata_irq_relogio(so_t *self);
static err_t so_trata_irq_desconhecida(so_t *self, int irq);
static err_t so_trata_chamada_sistema(so_t *self);
static err_t so_mem_virt_irq(so_t *self, process_t *p, bool cria_proc);
// funções auxiliares para o tratamento de interrupção
static void so_salva_estado_da_cpu(so_t *self);
static void so_escalona(so_t *self);
static void so_despacha(so_t *self);


//Tratamento de pendências
static err_t  so_trata_pendencias(so_t *self);
static int so_proc_pendencia(so_t *self, int PID_,
                             process_state_t state, int PID_or_device);
static int so_wait_proc(so_t *self);


// Chamadas de sistema
static void so_chamada_le(so_t *self);
static void so_chamada_escr(so_t *self);
static void so_chamada_cria_proc(so_t *self);
static void so_chamada_mata_proc(so_t *self);


//----------------------------------------------------------------------------
// Funções:


// NOTE [ENTRYPOINT]
static err_t so_trata_interrupcao(void *argC, int reg_A)
{
  so_t *self = argC;
  irq_t irq = reg_A;
  err_t err;

  console_printf(self->console, "SO: recebi IRQ %d (%s)", irq, irq_nome(irq));

  // 1 - Salva o processso
  if(self->runningP != 0) {
    self->p_runningP = ptable_search(self->processTable, self->runningP);
    process_save(self, self->p_runningP);
  }

  // 2 - Atende interrupções
  log_irq(self->log,irq);

  switch (irq) {
    case IRQ_RESET:
      err = so_trata_irq_reset(self);
      break;
    case IRQ_ERR_CPU:
      err = so_trata_irq_err_cpu(self);
      break;
    case IRQ_SISTEMA:
      err = so_trata_chamada_sistema(self);
      break;
    case IRQ_RELOGIO:
      err = so_trata_irq_relogio(self);
      break;
    default:
      err = so_trata_irq_desconhecida(self, irq);
  }

  // 3 - Verifica Pendencias
  so_trata_pendencias(self);

  // 4 - Escalonador
  process_t *to_run = sched_get(self->scheduler);
  if(!to_run) {
    console_printf(self->console, "SO: Nada para escalonar no momento, REL: %i",
                   rel_agora(self->relogio));

  }else {
    console_printf(self->console, "SO: Escolhido %i, prio %f", proc_get_PID(to_run),
                   proc_get_priority(to_run));


    mmu_define_tabpag(self->mmu, proc_get_tpag(to_run)); // <-----
    proc_set_state(to_run, running, rel_agora(self->relogio));
  }


  // 5 - Recupera processo, se existir um;
  process_recover(self, to_run);

  // Log
  //Tempo que cada processo ficou no estado anterior a este;
  int tempo_estado = rel_agora(self->relogio) - self->tempo_chamda_anterior;

  ptable_log_states(self->processTable, tempo_estado, self->log);
  self->tempo_chamda_anterior = rel_agora(self->relogio);

  // 6 - Retorna erro
  return err;
}


static err_t so_trata_pendencias(so_t *self)
{
  err_t erro = ERR_OK;


  // Pendencias de entrada e saída, bloqueio
  for (int i = 0; i < NUM_ES; ++i) {
    // Trata pendencia de escrita
    static int estado_escr, estado_leitura;
    term_le(self->console, i*4 + 3, &estado_escr);
    term_le(self->console, i*4 + 1, &estado_leitura);

    if(self->es_pendencias[i][0] > 0 && estado_escr != 0){
      process_t *p = ptable_search_pendencia(self->processTable, blocked_write, i);



      if(p == NULL) {
        console_printf(self->console,
                       "tabela de pendencias e ptable não sincronizadas");
        erro++;
        continue ;
      }

      cpu_info_t_so  *proc_info = proc_get_cpuinfo(p);
      int dado = proc_info->X;

      int ret = term_escr(self->console, i*4 + 2, dado);

      proc_info->A = ret;

      (self->es_pendencias[i][0])--;

      proc_set_state(p, waiting,rel_agora(self->relogio));

      sched_add(self->scheduler, p, proc_get_PID(p), QUANTUM);

    }

    //Trata pendencia de leitura
    if(self->es_pendencias[i][1] > 0 && estado_leitura != 0){
      process_t *p = ptable_search_pendencia(self->processTable, blocked_read, i);
      if(p == NULL) {
        console_printf(self->console,
                       "tabela de pendencias e ptable não sincronizadas");
        // Ou falha na funcao search...
        erro++;
        continue ;
      }
      cpu_info_t_so  *proc_info = proc_get_cpuinfo(p);

      int dado;
      if(term_le(self->console, i*4, &dado) != ERR_OK)
        erro++;

      proc_info->A = dado;

      self->es_pendencias[i][1]--;

      proc_set_state(p, waiting,rel_agora(self->relogio));
      proc_set_PID_or_device(p, 0);

      sched_add(self->scheduler, p, proc_get_PID(p), QUANTUM);

    }
  }


  // Pendencias de page_fault, suspend
  if(self->sec_req){
    process_t **lista_procs= calloc(self->sec_req, sizeof (process_t *));

    ptable_search_hthan(self->processTable, suspended_create_proc | suspended,
                        rel_agora(self->relogio), lista_procs);


    // Para toda lista de processos cuja paginação já foi feita
    for (int i = 0; i < self->sec_req && lista_procs[i] != NULL; ++i) {
              process_t  *p = lista_procs[i];


              // Agora o SO consegue atender a chamada CRIA_PROC
              if(proc_get_state(p) == suspended_create_proc){


                cpu_info_t_so *cpuInfoTSo = proc_get_cpuinfo(p);

                char nome[100];

                bool copy_ret = so_copia_str_do_processo(self, 100, nome,
                                                         cpuInfoTSo->X,
                                                         p);

                if(copy_ret) {
                  if (so_cria_proc(self, nome) != -1) {
                    cpuInfoTSo->A = PID_COUNTER;
                  } else {
                    cpuInfoTSo->A = -1;
                  }
                } else {
                    so_mem_virt_irq(self, p, true); // Sim, ele pode acabar caindo em page fault novamente
                    console_printf(self->console, "AGAIN PAGE FAULT");
                    continue ;
                }

              }

              console_printf(self->console, "MEMVIRT: Waking up p %i (t %i)", proc_get_PID(p), rel_agora(self->relogio));
              self->sec_req--;
              proc_set_state(p, waiting,rel_agora(self->relogio));
              proc_set_PID_or_device(p, 0);
              sched_add(self->scheduler, p, proc_get_PID(p), QUANTUM);
    }
  }


  return erro;
}


static err_t so_trata_irq(so_t *self, int irq)
{
  err_t err;
  switch (irq) {
    case IRQ_RESET:
      err = so_trata_irq_reset(self);
      break;
    case IRQ_ERR_CPU:
      err = so_trata_irq_err_cpu(self);
      break;
    case IRQ_SISTEMA:
      err = so_trata_chamada_sistema(self);
      break;
    case IRQ_RELOGIO:
      err = so_trata_irq_relogio(self);
      break;
    default:
      err = so_trata_irq_desconhecida(self, irq);
  }
  return err;
}

static err_t so_trata_irq_reset(so_t *self)
{

  //Reset ptable e sched;
  if(self->processTable)
    ptable_destruct(self->processTable);

  if(self->scheduler)
    sched_destruct(self->scheduler);

  if(self->mng_tpag)
    map_tpag_destruct(self->mng_tpag);
  self->mng_tpag = map_tpag_create((MEM_TAM/TAM_PAGINA  - 10));

  so_start_ptable_sched(self);

  //Reset tabela de pendências
  memset(self->es_pendencias, 0, sizeof(self->es_pendencias));

  //Reset o PID também;
  PID_COUNTER = 0;

  // coloca um programa na memória
  char init_name[100] = "init.maq";
  int ender = so_cria_proc(self, init_name);

  // deveria criar um processo para o init, e inicializar o estado do
  //   processador para esse processo com os registradores zerados, exceto
  //   o PC e o modo.
  // como não tem suporte a processos, está carregando os valores dos
  //   registradores diretamente para a memória, de onde a CPU vai carregar
  //   para os seus registradores quando executar a instrução RETI

  // altera o PC para o endereço de carga (deve ter sido o endereço virtual 0)
  //mem_escreve(self->mem, IRQ_END_PC, ender);
  mmu_escreve(self->mmu, IRQ_END_PC, ender, supervisor);
  
  // passa o processador para modo usuário
  // mem_escreve(self->mem, IRQ_END_modo, usuario);
  mmu_escreve(self->mmu, IRQ_END_modo, usuario, supervisor);
  
  return ERR_OK;
}


/*IRQ_ERR_CPU:
 * Mata o processo que causou o erro, se possível*/
static err_t so_trata_irq_err_cpu(so_t *self) // <-----
{

  int err_int;
  mem_le(self->mem, IRQ_END_erro, &err_int);
  err_t err = err_int, retorno = ERR_CPU_PARADA;
  switch (err_int) {
    case ERR_CPU_PARADA: {
      cpu_modo_t modo;
      mem_le(self->mem, IRQ_END_modo, (int *) &modo);

      if (modo == supervisor) {

        console_printf(self->console, "SO: IRQ não tratada -- erro na CPU: %s",
                       err_nome(err));
        if (self->runningP != 0) {
          console_printf(self->console, "SO: Matando %i",
                         self->runningP);

          so_mata_proc(self, self->p_runningP);
        }


      } else {
        console_printf(self->console, "SO: Aparentemente todos os processos "
                                      "estavam bloqueados uma execução atrás",
                       err_nome(err));

        return ERR_OK;
      }

      break;
    }
    case ERR_PAG_AUSENTE:
    case ERR_END_INV: {
      console_printf(self->console, "Processo %i causou erro %s ", self->runningP, err_nome(err_int));

      retorno = so_mem_virt_irq(self, self->p_runningP, false);
    }

      /* NOTE: Como é possível erro de pagina ausente se eu ainda não programei
       * substituição de pag?
       */
      /* ANSWER: Considere que o programa contem as paginas A,B,C dispostas
       * sequencialmente em seu espaco de endereçamento.
       * A tabela de paginas, um vetor, do programa em dado momento é tab: [frame_A],
       * somente A está mapeado.
       * Caso A chame um função em C:
       *  Como a relação página-quadro é definido com base no indice da
       *  tabela, está será redimensionada para tab: [frame_A, -1, frame_C]
       *  Assim, caso o programa acesse a memória em B ele receberá um ERR_PAG_AUSENTE
       *  */

  }

  return retorno;

}


/*IRQ_RELOGIO: Rearma o relógio e atualiza o escalonador*/
static err_t so_trata_irq_relogio(so_t *self)
{

  // ocorreu uma interrupção do relógio
  // rearma o interruptor do relógio e reinicializa o timer para a próxima interrupção
  rel_escr(self->relogio, 3, 0); // desliga o sinalizador de interrupção
  rel_escr(self->relogio, 2, INTERVALO_INTERRUPCAO);
  console_printf(self->console, "SO: Sched Update");
  sched_update(self->scheduler);

  if(controle_zera++ % CONTROLE_ZERA_T == 0)
    map_tpag_zera_acessado(self->mng_tpag);
  map_tpag_dump(self->mng_tpag, self->console, "zerando bit");


  /* Caso ocorra uma interrupção de relógio e não há processo rodando, houve
  * um periodo ocioso */
  if(self->runningP == 0) {
    int time = rel_agora(self->relogio) - self->tempo_chamda_anterior;
    log_ocioso(self->log, time);
  }

  return ERR_OK;
}


/*Não Alterado*/
static err_t so_trata_irq_desconhecida(so_t *self, int irq)
{
  console_printf(self->console,
      "SO: não sei tratar IRQ %d (%s)", irq, irq_nome(irq));
  return ERR_CPU_PARADA;
}

// Chamadas de sistema

static void so_chamada_le(so_t *self);
static void so_chamada_escr(so_t *self);
static void so_chamada_cria_proc(so_t *self);
static void so_chamada_mata_proc(so_t *self);


/*Não Alterado*/
static err_t so_trata_chamada_sistema(so_t *self)
{
  // com processos, a identificação da chamada está no reg A no descritor
  //   do processo
  int id_chamada;
  mem_le(self->mem, IRQ_END_A, &id_chamada);
  console_printf(self->console,
      "SO: chamada de sistema %d", id_chamada);
  switch (id_chamada) {
    case SO_LE:
      so_chamada_le(self);
      break;
    case SO_ESCR:
      so_chamada_escr(self);
      break;
    case SO_CRIA_PROC:
      so_chamada_cria_proc(self);
      break;
    case SO_MATA_PROC:
      so_chamada_mata_proc(self);
      break;
    case SO_ESPERA_PROC:
      so_wait_proc(self);
      break ;
    default:
      console_printf(self->console,
          "SO: P[%i] chamada de sistema desconhecida (%d)", self->runningP, id_chamada);
      return ERR_CPU_PARADA;
  }
  return ERR_OK;
}

/* Chamadas escrita e leitura:
 * Em ambas nada é escrito ou lido, os processos são bloqueados e as requisições
 * são registradas para serem resolvidas posteriormente*/
static void so_chamada_le(so_t *self)
{

  int device = ((self->runningP - 1)%4);

  //Configura processo
  so_proc_pendencia(self, self->runningP, blocked_read, device);

  //Registra a "requisição"
  (self->es_pendencias[device][1])++;


}

static void so_chamada_escr(so_t *self)
{

  int device = ((self->runningP - 1)%4);

  //Registra a "requisição"
  (self->es_pendencias[device][0])++;


  //Configura processo
  so_proc_pendencia(self, self->runningP, blocked_write, device);

}


static void so_chamada_cria_proc(so_t *self)
{
  
  process_t *process = self->p_runningP;
  cpu_info_t_so *cpuInfoTSo = proc_get_cpuinfo(process);

  char nome[100];

  bool copy_ret = so_copia_str_do_processo(self, 100, nome,
                                           cpuInfoTSo->X,
                                           process);

  if(copy_ret) {
    if (so_cria_proc(self, nome) != -1) {
      cpuInfoTSo->A = PID_COUNTER;
    } else {
      cpuInfoTSo->A = -1;
    }
  } else {
    so_mem_virt_irq(self, self->p_runningP, true);
  }

}

/* Faz as verificações necessárias e chama so_mata_proc*/
static void so_chamada_mata_proc(so_t *self)
{

  int proc_kill;
  mem_le(self->mem, IRQ_END_X, &proc_kill);

  process_t *pk;
  if(proc_kill) {
    pk = ptable_search(self->processTable, proc_kill);
  } else {
    pk = self->p_runningP;
  }

  if(pk){

    cpu_info_t_so *cpuInfo = proc_get_cpuinfo(self->p_runningP);
    cpuInfo->A = 0;
    so_mata_proc(self, pk);

  }else{
    cpu_info_t_so *cpuInfo = proc_get_cpuinfo(self->p_runningP);
    cpuInfo->A = -1;
  }
}


// carrega o programa na memória
// retorna o endereço de carga ou -1
// está simplesmente lendo para o próximo quadro que nunca foi ocupado,
//   nem testa se tem memória disponível
// com memória virtual, a forma mais simples de implementar a carga
//   de um programa é carregá-lo para a memória secundária, e mapear
//   todas as páginas da tabela de páginas como inválidas. assim, 
//   as páginas serão colocadas na memória principal por demanda.
//   para simplificar ainda mais, a memória secundária pode ser alocada
//   da forma como a principal está sendo alocada aqui (sem reuso)



static ret_so_carrega_programa so_carrega_programa(so_t *self,
                                                   char *nome_do_executavel)
{
  ret_so_carrega_programa r;

  // programa para executar na nossa CPU
  programa_t *prog = prog_cria(nome_do_executavel);

  if (prog == NULL) {
    console_printf(self->console,
        "Erro na leitura do programa '%s'\n", nome_do_executavel);
    r.end_virt_ini = -1;
    return r;
  }

  r.end_virt_ini = prog_end_carga(prog);
  r.end_virt_fim = r.end_virt_ini + prog_tamanho(prog) - 1;
  r.pagina_ini = r.end_virt_ini / TAM_PAGINA;
  r.pagina_fim = r.end_virt_fim / TAM_PAGINA;
   r.quadro_ini = self->sec_mem_quadro_livre;
   self->sec_mem_quadro_livre += 1 + r.pagina_fim;




   // carrega o programa na memória principal <--- TODO Alterada para carregar na mem secundaria
  int end_fis_ini = r.quadro_ini * TAM_PAGINA;
  int end_fis = end_fis_ini;
  for (int end_virt = r.end_virt_ini; end_virt <= r.end_virt_fim; end_virt++) {
    if (mem_escreve(self->sec_mem, end_fis, prog_dado(prog, end_virt)) != ERR_OK) {
      console_printf(self->console,
          "Erro na carga da memória secundária, end virt %d fís %d\n", end_virt, end_fis);
      r.end_virt_ini = -1;
      return r;
    }

    //console_printf(self->console,
    //               "Carregando em memória secundária, end virt %d fís %d\n", end_virt, end_fis);
    end_fis++;
  }
  prog_destroi(prog);

  return r;
}


// copia uma string da memória do processo para o vetor str.
// retorna false se erro (string maior que vetor, valor não ascii na memória,
//   erro de acesso à memória)
// o endereço é um endereço virtual de um processo.
// Com processos e memória virtual implementados, esta função deve também
//   receber o processo como argumento
// Cada valor do espaço de endereçamento do processo pode estar em memória
//   principal ou secundária
// O endereço é um endereço virtual de um processo.
// Com processos e memória virtual implementados, esta função deve também
//   receber o processo como argumento
// Com memória virtual, cada valor do espaço de endereçamento do processo
//   pode estar em memória principal ou secundária

/* Retorna:
 *  0 - Tudo ok
 *  >0 - Erro de paginação, endereço que causou o erro estará em complemento
 *  <0 - Outro erro, caractere nao ascii ou maior que 100
 * */
static int so_copia_str_do_processo(so_t *self, int tam, char str[tam],
                                     int end_virt, process_t *p)
{
  for (int indice_str = 0; indice_str < tam; indice_str++) {
    int caractere;
    // não tem memória virtual implementada, posso usar a mmu para traduzir
    //   os endereços e acessar a memória
    mmu_define_tabpag(self->mmu, proc_get_tpag(p));

    if (mmu_le(self->mmu, end_virt + indice_str, &caractere, usuario) != ERR_OK)
    {

      cpu_info_t_so *cpuInfoTSo = proc_get_cpuinfo(p);
      cpuInfoTSo->complemento = end_virt + indice_str;
      return false;
    }
    if (caractere < 0 || caractere > 255) {
      return false;
    }
    str[indice_str] = caractere;
    if (caractere == 0) {
      return true;
    }
  }
  // estourou o tamanho de str
  return false;
}


// Cópia de uma memória para outra, from mem1 to mem2
static int so_mem_to_mem(mem_t *mem1, int mem1_addr, mem_t *mem2, int mem2_addr, int range){
  int size_mem1 = mem_tam(mem1);
  int size_mem2 = mem_tam(mem2);

  if((mem1_addr + range > size_mem1) || (mem2_addr + range > size_mem2))
    return -1;

  for (int i = 0; i < range; ++i){
    int aux, r1, r2;
    r1 = mem_le(mem1, mem1_addr+i, &aux);
    r2 = mem_escreve(mem2, mem2_addr + i, aux);
    if(r1 || r2)
      return -1;
  }

  return 0;
}


/*Salva o estado da CPU para o processo PID_;*/
static process_t *process_save(so_t *self, process_t *p){


  if(p == NULL || !p)
    return NULL;
  //struct cpu_info_t_so  *cpuInfo = calloc(1, sizeof(cpu_info_t));

  cpu_info_t_so *cpuInfo = proc_get_cpuinfo(p);

  mem_le(self->mem, IRQ_END_A, &(cpuInfo->A));
  mem_le(self->mem, IRQ_END_X, &(cpuInfo->X));
  mem_le(self->mem, IRQ_END_complemento, &(cpuInfo->complemento));
  mem_le(self->mem, IRQ_END_PC, &(cpuInfo->PC));
  mem_le(self->mem, IRQ_END_modo, (int *)&(cpuInfo->modo));

  return p;
}


/*Restaura o estado da CPU do processo;*/
static err_t process_recover(so_t *self, process_t *process){

  err_t err = ERR_OK;

  if(!process) {
    if (ptable_is_empty(self->processTable)) { // Sem processos, parando...
      err = ERR_CPU_PARADA;
      mmu_escreve(self->mmu, IRQ_END_erro, ERR_CPU_PARADA, supervisor);
      mmu_escreve(self->mmu, IRQ_END_modo, supervisor, supervisor);
      self->runningP = 0;
      self->p_runningP = NULL;
      log_exectime(self->log, rel_agora(self->relogio));
      log_save_tofile(self->log);

    } else { // Tabela de processo bloqueada

      mmu_escreve(self->mmu, IRQ_END_erro, ERR_CPU_PARADA, supervisor);
      mmu_escreve(self->mmu, IRQ_END_modo, usuario, supervisor);
      mmu_escreve(self->mmu, IRQ_END_A, IRQ_ERR_CPU, supervisor);


      self->runningP = 0;
      self->p_runningP = NULL;
    }

  } else {

    process_t  *p = process;
    struct cpu_info_t_so  *cpuInfo = proc_get_cpuinfo(p);

    mmu_escreve(self->mmu, IRQ_END_A, (cpuInfo->A), supervisor);
    mmu_escreve(self->mmu, IRQ_END_X, (cpuInfo->X), supervisor);
    mmu_escreve(self->mmu, IRQ_END_complemento, (cpuInfo->complemento), supervisor);
    mmu_escreve(self->mmu, IRQ_END_PC, (cpuInfo->PC), supervisor);
    mmu_escreve(self->mmu, IRQ_END_modo, (cpuInfo->modo), supervisor);
    mmu_escreve(self->mmu, IRQ_END_erro, ERR_OK, supervisor);
    proc_set_state(p, running,rel_agora(self->relogio));
    self->runningP = proc_get_PID(process);

  }
  return err;
}


/*Mata o processo:
 * - Remove do scheduller, caso necessário
 * - Desbloqueia processos que estevam esperando a morte deste
 * - Decrementa de uma das listas de pendência, caso necessário
 * - Remove da tabela de processos
 * */
static int so_mata_proc(so_t *self, process_t *p){

  process_state_t estado = proc_get_state(p);
  proc_set_end_time(p, rel_agora(self->relogio));
  int pid = proc_get_PID(p);

  proc_set_state(p, dead,rel_agora(self->relogio));

  if(estado == waiting || estado == running) {
    sched_remove(self->scheduler, proc_get_PID(p));
  } else if( estado == blocked_read){
    (self->es_pendencias[proc_get_PID_or_device(p)][1])--;
  } else if(estado == blocked_write){
    (self->es_pendencias[proc_get_PID_or_device(p)][0])--;
  } else if(estado == suspended_create_proc || estado == suspended){
    self->sec_req--;
  }

  ptable_proc_wait(self->processTable, proc_get_PID(p),self->scheduler, QUANTUM);
  map_tpag_free(self->mng_tpag, pid);

  //ptable_delete(self->processTable, proc_get_PID(p));

  return 0;
}


/* Usado para bloquear o processo
 * Altera seu estado
 * Define o valor associado ao bloqueio:
 *    Dispositivo que está esperando,
 *    Ou processo que está esperado
 * Remove o processo do escalonador*/
static int so_proc_pendencia(so_t *self, int PID_local,
                             process_state_t state, int PID_or_device){

  process_t *p = ptable_search(self->processTable, PID_local);


  if(!p) {
    console_printf(self->console,
                   "Processo [%i] existe mas não está na ptable "
                   , PID_local);
    return -1;
  }
  process_state_t estado_anterior = proc_get_state(p);

  // Casos falha de página

  if(state == suspended || state == suspended_create_proc) {

    int t_now = rel_agora(self->relogio), when_wake;
    int aux =  t_now + (PID_or_device * DELAY_SEC_MEM);
    int aux2 = self->sec_tempo_livre + (PID_or_device * DELAY_SEC_MEM);
    when_wake = aux2>aux?aux2:aux;
    self->sec_tempo_livre = when_wake;

    console_printf(self->console, "MEMVIRT: process %i suspended until %i",
                   proc_get_PID(p), when_wake);

    PID_or_device = when_wake;
  }


  proc_set_state(p, state,rel_agora(self->relogio));
  proc_set_PID_or_device(p, PID_or_device);

  if(estado_anterior == running || estado_anterior == waiting)
    sched_remove(self->scheduler, proc_get_PID(p));


  return ERR_OK;
}


/* Carrega processo na memória secundária e registra ele no sistema */
static int so_cria_proc(so_t *self, char nome[100]) {

  ret_so_carrega_programa ender_carga = so_carrega_programa(self, nome);
  if (ender_carga.end_virt_ini >= 0) {
    log_proc(self->log);
    so_registra_proc(self, ender_carga);
    return ender_carga.end_virt_ini;
  } else {
    console_printf(self->console, "mem: Erro ao carregar processo, "
                                  "não registrado");
    return -1;
  }
}




/* Registra um novo processos na tabela de processos e no escalonador */
static int so_registra_proc(so_t *self, ret_so_carrega_programa address){

  struct cpu_info_t_so  *cpuInfo = calloc(1, sizeof(cpu_info_t));

  cpuInfo->modo = usuario;
  cpuInfo->PC = address.end_virt_ini;
  cpuInfo->complemento = 0;
  cpuInfo->X = 0;
  // cpu.A: Herdado do processo que cria ou do lixo que estiver na memória
  mem_le(self->mem, IRQ_END_A, &(cpuInfo->A));


  void *p =  ptable_add_proc(self->processTable, cpuInfo, ++PID_COUNTER, address, 0.5,
                            rel_agora(self->relogio));

  if (!p) {
    (console_printf(self->console, "ptable: Erro ao registrar processo"));
    return -1;
  }

  if(sched_add(self->scheduler, p, PID_COUNTER, QUANTUM) != 0)
    console_printf(self->console, "sched: Erro ao adicionar processo");

  return 0;

}

/*Inicializa tabela de processo e escalonador*/
int so_start_ptable_sched(so_t *self){
  self->processTable = ptable_create();
  self->scheduler = sched_create(self->relogio, self->log);

  return 0;
}


/* Bloqueia um processo até que outro morra
 * Caso o processo que ele deseja esperar exista*/
static int so_wait_proc(so_t *self){
  process_t  *p = ptable_search(self->processTable, self->runningP);
  cpu_info_t_so  *cpuInfo = proc_get_cpuinfo(p);

  if(!cpuInfo)
    return -1;

  int PID_ = cpuInfo->X;

  process_t * espera = ptable_search(self->processTable, PID_);
  if(!espera) {
    cpuInfo->A = -1;
  } else {
    so_proc_pendencia(self, self->runningP, blocked_proc, PID_);
    cpuInfo->A = 0;
  }

  return 0;
}

static err_t  so_mem_virt_irq(so_t *self, process_t *p, bool cria_proc){

  /* 1. Verificar se o processo A está acessando um endereço válido; <- FEITO
     * 2. Encontrar o endereço do processo na memória secundária <- FEITO
     * 3. Escolher uma quadro para substituir
     * 4. Se este quadro estava sendo usada por outro processo B
     *  4.1 Caso B tenha alterado o quadro: Gravar este quadro na memória secundária na página correspondente de B
     *  4.2 Inválidar este quadro de memória na tpag de B
     * 5. Copiar a memória do disco de A para a memória principal <- FEITO
     * 6. Atualizar a tab_pag de A; <- FEITO
     * 7. Bloquear o processo pelo tempo que simula as operações de disco <- FEITO
     * */

  err_t  err = ERR_OK;

  int addr, acessos = 1;

  cpu_info_t_so *cpuInfoTSo = proc_get_cpuinfo(p);
  addr = cpuInfoTSo->complemento;


  // 1 e 2
  int addr_mem_sec = proc_get_page_addr(self->p_runningP, addr, TAM_PAGINA);

  // Mata o processo caso ele tente acessar um endereço inválido
  if(addr_mem_sec == -1) {
    console_printf(self->console, "MEMVIRT: Matando %i [ERR_END_INV] %i",
                   self->runningP, addr);

    so_mata_proc(self, self->p_runningP);
    return  ERR_OK;
  }

  // 3.

  map_node mp = {
          self->runningP,
          proc_get_tpag(self->p_runningP),
          addr/TAM_PAGINA,
          0 //Frame nao importa aqui
  };

  map_node substituido = map_tpag_choose(self->mng_tpag, mp);


  // 4.
  if(substituido.PID != 0){
    bool alterado = tabpag_bit_alteracao(substituido.tpag, substituido.pag);
    if(alterado){
      acessos++;

      process_t *p_substituido = ptable_search(self->processTable, substituido.PID);

      // Endereço a ser posto na memoria secundaria
      int addr_to_put = proc_get_page_addr(p_substituido, substituido.pag*TAM_PAGINA, TAM_PAGINA);

      so_mem_to_mem(self->mem, TAM_PAGINA*(substituido.frame+OFFSET_MEM),
                   self->sec_mem, addr_to_put, TAM_PAGINA);

    }
    tabpag_define_quadro(substituido.tpag, substituido.pag, -1);
  }

  // 5.
  so_mem_to_mem(self->sec_mem, addr_mem_sec, self->mem, (substituido.frame+OFFSET_MEM)*TAM_PAGINA, TAM_PAGINA);

  // 6.
  tabpag_t *proc_pag = proc_get_tpag(self->p_runningP);
  tabpag_define_quadro(proc_pag, addr/TAM_PAGINA, substituido.frame+OFFSET_MEM);


  // NOTE: Posso fazer isso?
  // Isto evita que seja removido antes do processo acabar
  tabpag_marca_bit_acesso(proc_pag, addr/TAM_PAGINA, false);


  //log
  console_printf(self->console, "MEMVIRT: p %i. pag %i to frame %i %s, (err)%s",
                 proc_get_PID(p), addr/TAM_PAGINA, OFFSET_MEM+substituido.frame,
                 (cria_proc == true?"suspended_create_proc":"suspended"),
                 err_nome(err));

  map_tpag_dump(self->mng_tpag, self->console, "alteracao mapeamento");


  // 7.
  so_proc_pendencia(self, self->runningP,
                    (cria_proc == true?suspended_create_proc:suspended),
                    acessos);
  self->sec_req++;


  return err;
}