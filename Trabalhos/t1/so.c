#include "so.h"
#include "irq.h"
#include "process_mng.h"
#include "programa.h"
#include "scheduler_interface.h"
#include "util/metricas.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// intervalo entre interrupções do relógio
#define INTERVALO_INTERRUPCAO 50   // em instruções executadas
#define QUANTUM 10
#define NUM_ES 4 // 4 terminais


struct so_t {
  cpu_t *cpu;
  mem_t *mem;
  console_t *console;
  relogio_t *relogio;
  process_table_t * processTable;
  scheduler_t * scheduler;
  int runningP;
  process_t *p_runningP;
  /*
   * Para cada pendência/requisição de escrita/leitura em dado dipositivo o valor
   * é incrementado, quando a pendência é atendida, o valor é decrementado.
   * */
  // [][0] -- Escrita
  // [][1] -- Leitura
  int es_pendencias[NUM_ES][2];
  metricas *log;

};

typedef struct cpu_info_t_so {
  int PC;
  int X;
  int A;
  // estado interno da CPU
  int complemento;
  cpu_modo_t modo;
}cpu_info_t_so;

static int PID;


//---------------------------------------------------------------------------

// função de tratamento de interrupção (entrada no SO)
static err_t so_trata_interrupcao(void *argC, int reg_A);

// funções auxiliares
static int so_carrega_programa(so_t *self, char *nome_do_executavel);
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender);

//Processos
static process_t *process_save(so_t *self, int PID);
static err_t process_recover(so_t *self, process_t *process);
static int so_cria_proc(so_t *self, char nome[100]);
static int so_registra_proc(so_t *self, int address);
static int so_mata_proc(so_t *self, process_t *p);
int so_start_ptable_sched(so_t *self);


// Tratamento de interrupção
static err_t so_trata_irq_reset(so_t *self);
static err_t so_trata_irq_err_cpu(so_t *self);
static err_t so_trata_irq_relogio(so_t *self);
static err_t so_trata_irq_desconhecida(so_t *self, int irq);
static err_t so_trata_chamada_sistema(so_t *self);

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


/*ENTRYPOINT*/
// função a ser chamada pela CPU quando executa a instrução CHAMAC
// essa instrução só deve ser executada quando for tratar uma interrupção
// o primeiro argumento é um ponteiro para o SO, o segundo é a identificação
//   da interrupção
// na inicialização do SO é colocada no endereço 10 uma rotina que executa
//   CHAMAC; quando recebe uma interrupção, a CPU salva os registradores
//   no endereço 0, e desvia para o endereço 10
static err_t so_trata_interrupcao(void *argC, int reg_A)
{
  so_t *self = argC;
  irq_t irq = reg_A;
  err_t err;

  console_printf(self->console, "SO: recebi IRQ %d (%s, %i)", irq,
                 irq_nome(irq), self->runningP);


  // 1 - Salva o processso
  if(self->runningP != 0) {
    process_save(self, self->runningP);
    self->p_runningP = ptable_search(self->processTable, self->runningP);
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
  }
  // 5 - Recupera processo, se existir um;
  process_recover(self, to_run);

  // Log
  ptable_log_states(self->processTable);

  // 6 - Retorna erro
  return err;
}



/*IRQ_RESET:
 * Destrói estruturas que já existam e cria novas
 * Define valores iniciais do sistema
 * */
static err_t so_trata_irq_reset(so_t *self)
{
  //Reset ptable e sched;
  if(self->processTable)
      ptable_destruct(self->processTable);

  if(self->scheduler)
      sched_destruct(self->scheduler);

  so_start_ptable_sched(self);

  //Reset tabela de pendências
  memset(self->es_pendencias, 0, sizeof(self->es_pendencias));

  //Reset o PID também;
  PID = 0;

  // coloca um programa na memória
  char init_name[100] = "init.maq";
  int ender = so_cria_proc(self, init_name);

  if (ender != 100) {
      console_printf(self->console, "SO: problema na carga do programa inicial");
      return ERR_CPU_PARADA;
  }

  return ERR_OK;
}


/*IRQ_ERR_CPU:
 * Mata o processo que causou o erro, se possível*/
static err_t so_trata_irq_err_cpu(so_t *self)
{
  // Ocorreu um erro interno na CPU
  // O erro está codificado em IRQ_END_erro
  // Em geral, causa a morte do processo que causou o erro
  // Ainda não temos processos, causa a parada da CPU
  int err_int;
  mem_le(self->mem, IRQ_END_erro, &err_int);
  err_t err = err_int;
  if(err_int == ERR_CPU_PARADA){
    cpu_modo_t modo;
    mem_le(self->mem, IRQ_END_modo, (int *)&modo);

    if(modo == supervisor) {

      console_printf(self->console, "SO: IRQ não tratada -- erro na CPU: %s",
                     err_nome(err));
      if(self->runningP != 0){
        console_printf(self->console, "SO: Matando %i",
                       self->runningP);

        so_mata_proc(self, self->p_runningP);
      }


    } else {
      console_printf(self->console, "SO: Aparentemente todos os processos "
                                    "estavam bloqueados um execução atrás",
                     err_nome(err));

      log_ocioso(self->log);

      return ERR_OK;

    }
  }

  return ERR_CPU_PARADA;

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
  return ERR_OK;
}


/*Não alterado*/
static err_t so_trata_irq_desconhecida(so_t *self, int irq)
{
  console_printf(self->console,
                 "SO: não sei tratar IRQ %d (%s)", irq, irq_nome(irq));
  return ERR_CPU_PARADA;
}


/*Não Alterado*/
static err_t so_trata_chamada_sistema(so_t *self)
{
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
          "SO: chamada de sistema desconhecida (%d)", id_chamada);
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


/* Criação de processo a partir de chamada do sistema, chama so_cria_proc*/
static void so_chamada_cria_proc(so_t *self)
{

  process_t *process = self->p_runningP;
  cpu_info_t_so *cpuInfoTSo = proc_get_cpuinfo(process);
  int ender_proc;
  if (mem_le(self->mem, IRQ_END_X, &ender_proc) == ERR_OK) {
    char nome[100];
    if (copia_str_da_mem(100, nome, self->mem, ender_proc)) {
      if( so_cria_proc(self, nome) != -1){
        cpuInfoTSo->A = PID;
      } else{
        cpuInfoTSo->A = -1;
      }

    }

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

/*Não alterado*/
// carrega o programa na memória
// retorna o endereço de carga ou -1
static int so_carrega_programa(so_t *self, char *nome_do_executavel)
{
  // programa para executar na nossa CPU
  programa_t *prog = prog_cria(nome_do_executavel);
  if (prog == NULL) {
    console_printf(self->console,
        "Erro na leitura do programa '%s'\n", nome_do_executavel);
    return -1;
  }

  int end_ini = prog_end_carga(prog);
  int end_fim = end_ini + prog_tamanho(prog);

  for (int end = end_ini; end < end_fim; end++) {
    if (mem_escreve(self->mem, end, prog_dado(prog, end)) != ERR_OK) {
      console_printf(self->console,
          "Erro na carga da memória, endereco %d\n", end);
      return -1;
    }
  }
  prog_destroi(prog);
  console_printf(self->console,
      "SO: carga de '%s' em %d-%d", nome_do_executavel, end_ini, end_fim);
  return end_ini;
}

/*Não Alterado*/
// copia uma string da memória do simulador para o vetor str.
// retorna false se erro (string maior que vetor, valor não ascii na memória,
//   erro de acesso à memória)
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender)
{
  for (int indice_str = 0; indice_str < tam; indice_str++) {
    int caractere;
    if (mem_le(mem, ender + indice_str, &caractere) != ERR_OK) {
      return false;
    }
    if (caractere < 0 || caractere > 255) {
      return false;
    }
    str[indice_str] = (char)caractere;
    if (caractere == 0) {
      return true;
    }
  }
  // estourou o tamanho de str
  return false;
}



/* Carrega processo na memória e registra ele no sistema*/
static int so_cria_proc(so_t *self, char nome[100]){
  int ender_carga = so_carrega_programa(self, nome);
  if (ender_carga > 0) {
    log_proc(self->log);
    so_registra_proc(self, ender_carga);
    return ender_carga;
  } else {
    console_printf(self->console, "mem: Erro ao carregar processo, "
                                  "não registrado");
    return -1;
  }
}


/*Salva o estado da CPU para o processo PID_;*/
static process_t *process_save(so_t *self, int PID_){


  struct cpu_info_t_so  *cpuInfo = calloc(1, sizeof(cpu_info_t));

  mem_le(self->mem, IRQ_END_A, &(cpuInfo->A));
  mem_le(self->mem, IRQ_END_X, &(cpuInfo->X));
  mem_le(self->mem, IRQ_END_complemento, &(cpuInfo->complemento));
  mem_le(self->mem, IRQ_END_PC, &(cpuInfo->PC));
  mem_le(self->mem, IRQ_END_modo, (int *)&(cpuInfo->modo));

  process_t  *p = ptable_search(self->processTable, PID_);

  if(p)
    proc_set_cpuinfo(p, cpuInfo);

  return p;
}

/*Restaura o estado da CPU do processo;*/
static err_t process_recover(so_t *self, process_t *process){

  err_t err = ERR_OK;

  if(!process) {
    if (ptable_is_empty(self->processTable)) { // Sem processos, parando...
      err = ERR_CPU_PARADA;
      mem_escreve(self->mem, IRQ_END_erro, ERR_CPU_PARADA);
      mem_escreve(self->mem, IRQ_END_modo, supervisor);
      self->runningP = 0;
      self->p_runningP = NULL;
      log_exectime(self->log, rel_agora(self->relogio));
      log_save_tofile(self->log);

    } else { // Tabela de processo bloqueada

      mem_escreve(self->mem, IRQ_END_erro, ERR_CPU_PARADA);
      mem_escreve(self->mem, IRQ_END_modo, usuario);
      mem_escreve(self->mem, IRQ_END_A, IRQ_ERR_CPU);


      self->runningP = 0;
      self->p_runningP = NULL;
    }

  } else {

    process_t  *p = process;
    struct cpu_info_t_so  *cpuInfo = proc_get_cpuinfo(p);

    mem_escreve(self->mem, IRQ_END_A, (cpuInfo->A));
    mem_escreve(self->mem, IRQ_END_X, (cpuInfo->X));
    mem_escreve(self->mem, IRQ_END_complemento, (cpuInfo->complemento));
    mem_escreve(self->mem, IRQ_END_PC, (cpuInfo->PC));
    mem_escreve(self->mem, IRQ_END_modo, (cpuInfo->modo));
    mem_escreve(self->mem, IRQ_END_erro, ERR_OK);
    proc_set_state(p, running);
    self->runningP = proc_get_PID(process);

  }
  return err;
}

/*Inicializa tabela de processo e escalonador*/
int so_start_ptable_sched(so_t *self){
  self->processTable = ptable_create();
  self->scheduler = sched_create(self->relogio, self->log);

  return 0;
}



/* Registra um novo processos na tabela de processos e no escalonador */
static int so_registra_proc(so_t *self, int address){

  struct cpu_info_t_so  *cpuInfo = calloc(1, sizeof(cpu_info_t));

  cpuInfo->modo = usuario;
  cpuInfo->PC = address;
  cpuInfo->complemento = 0;
  cpuInfo->X = 0;
  // cpu.A: Herdado do processo que cria ou do lixo que estiver na memória
  mem_le(self->mem, IRQ_END_A, &(cpuInfo->A));

  void *p =  ptable_add_proc(self->processTable, cpuInfo, ++PID, address, 0.5,
                            rel_agora(self->relogio));

  if (!p) {
    (console_printf(self->console, "ptable: Erro ao registrar processo"));
    return -1;
  }

  if(sched_add(self->scheduler, p, PID, QUANTUM) != 0)
    console_printf(self->console, "sched: Erro ao adicionar processo");

  return 0;

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
  log_save_proc_tofile(p, self->log);

  proc_set_state(p, dead);

  if(estado == waiting || estado == running) {
    sched_remove(self->scheduler, proc_get_PID(p));
    ptable_proc_wait(self->processTable, proc_get_PID(p),self->scheduler, QUANTUM);
  } else if( estado == blocked_read){
    (self->es_pendencias[proc_get_PID_or_device(p)][1])--;
  } else if(estado == blocked_write){
    (self->es_pendencias[proc_get_PID_or_device(p)][0])--;
  }
  ptable_delete(self->processTable, proc_get_PID(p));

  return 0;
}


/* Trateamento de pendências [Terminais]
 * Percorre a lista de pendências vendo se há pendencias
 * Caso haja pendência e seja possível atender:
 *    Busca um processo correspondente na tabela de processos;
 *    Atende a requisição;
 *    Decrementa o valor correspondente na lista de pendencias;
 * */
static err_t  so_trata_pendencias(so_t *self){
  err_t erro = ERR_OK;

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

      proc_set_state(p, waiting);

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

      proc_set_state(p, waiting);
      proc_set_PID_or_device(p, 0);

      sched_add(self->scheduler, p, proc_get_PID(p), QUANTUM);

    }
  }
  return erro;
}



/* Usado para bloquear o processo
 * Altera seu estado
 * Define o valor associado ao bloqueio:
 *    Dispositivo que está esperando,
 *    Ou processo que está esperado
 * Remove o processo do escalonador*/
static int so_proc_pendencia(so_t *self, int PID_,
                             process_state_t state, int PID_or_device){

  process_t *p = ptable_search(self->processTable, PID_);

  if(!p) {
    console_printf(self->console,
                   "Processo [%ui] existe mas não está na ptable "
                   , PID_);
    return -1;
  }


  proc_set_state(p, state);
  proc_set_PID_or_device(p, PID_or_device);
  sched_remove(self->scheduler, PID_);


  return ERR_OK;
}


/* Bloqueia um processo até que outro morra
 * Caso o processo que ele deseja esperar exista*/
static int so_wait_proc(so_t *self){
  process_t  *p = ptable_search(self->processTable, self->runningP);
  cpu_info_t_so  *cpuInfo = proc_get_cpuinfo(p);

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

/*Não Alterado*/
so_t *so_cria(cpu_t *cpu, mem_t *mem, console_t *console, relogio_t *relogio)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;

  if(self->log)
    free(self->log);
  self->log = log_init("registro_provisorio\0"); //FIXME

  self->cpu = cpu;
  self->mem = mem;
  self->console = console;
  self->relogio = relogio;
  memset(self->es_pendencias, 0, sizeof(self->es_pendencias));

  // quando a CPU executar uma instrução CHAMAC, deve chamar essa função
  cpu_define_chamaC(self->cpu, so_trata_interrupcao, self);
  // coloca o tratador de interrupção na memória
  if (so_carrega_programa(self, "trata_irq.maq") != 10) {
    console_printf(console, "SO: problema na carga do tratador de interrupções");
    free(self);
    self = NULL;
  }
  // programa a interrupção do relógio
  rel_escr(self->relogio, 2, INTERVALO_INTERRUPCAO);

  return self;
}

/*Não Alterado*/
void so_destroi(so_t *self)
{
  cpu_define_chamaC(self->cpu, NULL, NULL);
  free(self);
}
