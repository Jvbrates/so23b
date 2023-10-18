#include "so.h"
#include "irq.h"
#include "programa.h"
#include "process_mng.h"
#include "scheduler_interface.h"

#include <stdlib.h>
#include <stdbool.h>

// intervalo entre interrupções do relógio
#define INTERVALO_INTERRUPCAO 50   // em instruções executadas
#define QUANTUM 10
struct so_t {
  cpu_t *cpu;
  mem_t *mem;
  console_t *console;
  relogio_t *relogio;
  process_table_t * processTable;
  scheduler_t * scheduler;
  unsigned int runningP;
};


/*
 * TODO:
 * cpu_info_t == void* e struct cpu_info_t_so está confuso...Resolva isto
 * */
typedef struct cpu_info_t_so {
  int PC;
  int X;
  int A;
  // estado interno da CPU
  int complemento;
  cpu_modo_t modo;
}cpu_info_t_so;



static unsigned int PID;

// função de tratamento de interrupção (entrada no SO)
static err_t so_trata_interrupcao(void *argC, int reg_A);

// funções auxiliares
static int so_carrega_programa(so_t *self, char *nome_do_executavel);
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender);
static int so_registra_proc(so_t *self, unsigned int address);
static int so_mata_proc(so_t *self, unsigned int PID);
int so_start_ptable_sched(so_t *self);
static int so_cria_proc(so_t *self, char nome[100]);

so_t *so_cria(cpu_t *cpu, mem_t *mem, console_t *console, relogio_t *relogio)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;

  self->cpu = cpu;
  self->mem = mem;
  self->console = console;
  self->relogio = relogio;

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

static int so_device_busy(so_t *self, void *disp, unsigned int id);
static int so_wait_proc(so_t *self);
static int  so_broadcast_procs_block_dev(so_t *self, void *device,
                                        unsigned int PID);
static int so_broadcast_procs_block_PID(so_t *self, unsigned int PID);

// funções auxiliares para tratar cada tipo de interrupção

//Salva e Recover
static process_t *process_save(so_t *self, unsigned int PID);
static err_t process_recover(so_t *self, process_t *process);


void so_destroi(so_t *self)
{
  cpu_define_chamaC(self->cpu, NULL, NULL);
  free(self);
}

// Tratamento de interrupção
static err_t so_trata_irq_reset(so_t *self);
static err_t so_trata_irq_err_cpu(so_t *self);
static err_t so_trata_irq_relogio(so_t *self);
static err_t so_trata_irq_desconhecida(so_t *self, int irq);
static err_t so_trata_chamada_sistema(so_t *self);

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

  /* No caso de PID ser 0, então está é a primeira execução do trata_irq e não há
  * processo para ser salvo, então a etapa deve ser puladas
   * */
  if(PID != 0) {
    process_t *p = process_save(self, self->runningP);

    // Acho que isso não é necessário, mas por segurança. TODO: Remover?
    proc_set_state(p, waiting);
  }



  // 2 - Atende interrupções
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



  // 4 - Escalonador
  process_t *to_run = sched_get_update(self->scheduler);

  // 5 - Recupera processo, se existir um;
  err_t err_recover = process_recover(self, to_run);

  // Evita que process_recover sobrescreva erros de syscall
  if(err_recover != ERR_OK)
    err = err_recover;

  // 6 - Retorna o self->erro
  return err;
}

static err_t so_trata_irq_reset(so_t *self)
{
  //Reset ptable e sched;
  if(self->processTable)
      ptable_destruct(self->processTable);

  if(self->scheduler)
      sched_destruct(self->scheduler);

  so_start_ptable_sched(self);

  //Reseta o PID também;
  PID = 0;
  // coloca um programa na memória
  char init_name[100] = "init.maq";
  int ender = so_cria_proc(self, init_name);


  //int ender = so_carrega_programa(self, "init.maq");

  if (ender != 100) {
    console_printf(self->console, "SO: problema na carga do programa inicial");
    return ERR_CPU_PARADA;
  }



  // Comentando isto daqui, NÃO DEVE ser mais necessário.
  // altera o PC para o endereço de carga (deve ter sido 100)
  //mem_escreve(self->mem, IRQ_END_PC, ender);
  // passa o processador para modo usuário
  //mem_escreve(self->mem, IRQ_END_modo, usuario);
  return ERR_OK;
}

static err_t so_trata_irq_err_cpu(so_t *self)
{
  // Ocorreu um erro interno na CPU
  // O erro está codificado em IRQ_END_erro
  // Em geral, causa a morte do processo que causou o erro
  // Ainda não temos processos, causa a parada da CPU
  int err_int;
  mem_le(self->mem, IRQ_END_erro, &err_int);
  err_t err = err_int;
  console_printf(self->console,
      "SO: IRQ não tratada -- erro na CPU: %s", err_nome(err));
  return ERR_CPU_PARADA;
}

static err_t so_trata_irq_relogio(so_t *self)
{
  // ocorreu uma interrupção do relógio
  // rearma o interruptor do relógio e reinicializa o timer para a próxima interrupção
  rel_escr(self->relogio, 3, 0); // desliga o sinalizador de interrupção
  rel_escr(self->relogio, 2, INTERVALO_INTERRUPCAO);
  // trata a interrupção
  // ...
  console_printf(self->console, "SO: interrupção do relógio (não tratada)");
  return ERR_OK;
}

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
      console_printf(self->console,
                     "Tá em espera proc %i %i", self->runningP, PID);
      break ;
    default:
      console_printf(self->console,
          "SO: chamada de sistema desconhecida (%d)", id_chamada);
      return ERR_CPU_PARADA;
  }
  return ERR_OK;
}

//TODO alterar so_chamada_le
static void so_chamada_le(so_t *self)
{
  // implementação com espera ocupada
  //   deveria bloquear o processo se leitura não disponível
  // implementação lendo direto do terminal A
  //   deveria usar dispositivo corrente de entrada do processo

  int estado;
  err_t status = term_le(self->console, 1, &estado);
  if(estado == 0 || status == ERR_OCUP)
  {
  // i.e. Ainda não há nada para ler ou dispositivo ocupado
      so_device_busy(self, self->console, 1);
  }
  /*
  for (;;) {
    int estado;
    term_le(self->console, 1, &estado);
    if (estado != 0) break;
    // como não está saindo do SO, o laço do processador não tá rodando
    // esta gambiarra faz o console andar
    console_tictac(self->console);
    console_atualiza(self->console);
  }
  */
  int dado;
  term_le(self->console, 0, &dado);
  mem_escreve(self->mem, IRQ_END_A, dado);
}

static void so_chamada_escr(so_t *self)
{
  // implementação com espera ocupada
  //   deveria bloquear o processo se dispositivo ocupado
  // implementação escrevendo direto do terminal A
  //   deveria usar dispositivo corrente de saída do processo
  for (;;) {
    int estado;
    term_le(self->console, 3, &estado);
    if (estado != 0) break;
    // como não está saindo do SO, o laço do processador não tá rodando
    // esta gambiarra faz o console andar
    console_tictac(self->console);
    console_atualiza(self->console);
  }
  int dado;
  mem_le(self->mem, IRQ_END_X, &dado);
  term_escr(self->console, ((self->runningP -1 )%4)*4 + 2, dado);
  mem_escreve(self->mem, IRQ_END_A, 0);
}

static int so_cria_proc(so_t *self, char nome[100]){
  int ender_carga = so_carrega_programa(self, nome);
  if (ender_carga > 0) {
    so_registra_proc(self, ender_carga);
    return ender_carga;
  } else {
    console_print_status(self->console, "mem: Erro ao carregar processo, "
                                        "não registrado");
    return -1;
  }
}

static void so_chamada_cria_proc(so_t *self)
{

  process_t *process = ptable_search(self->processTable, self->runningP);
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

      proc_set_cpuinfo(process, cpuInfoTSo);
    }

  }
  mem_escreve(self->mem, IRQ_END_A, -1); //Todo remover este -1
}

static void so_chamada_mata_proc(so_t *self)
{
  // ainda sem suporte a processos, retorna erro -1
  console_printf(self->console, "SO: SO_MATA_PROC não implementada");
   int ID_kill;
  if (mem_le(self->mem, IRQ_END_X, &ID_kill) == ERR_OK) {
    if(!ID_kill){
      console_print_status(self->console, "SO(kill): tentativa de matar init.asm!");
    } else {
      process_t  *p = ptable_search(self->processTable, (unsigned  int)ID_kill);
      if(!p) {
        console_print_status(self->console, "SO(kill): Processo inexistente");
        mem_escreve(self->mem, IRQ_END_A, -1);
        return ;
      }
      process_state_t  processState = proc_get_state(p);

      if(processState == running ||
         processState == waiting ||
         processState == blocked_dev ||
         processState == blocked_proc) {

        so_mata_proc(self, ID_kill);

      } else {
        console_print_status(self->console, "SO(kill): Processo inexistente");
        mem_escreve(self->mem, IRQ_END_A, -1);
        return ;
      }
    }
  } else {
    console_print_status(self->console, "mem: erro ao ler da memoria");
  }

  mem_escreve(self->mem, IRQ_END_A, -1);
}


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
    str[indice_str] = caractere;
    if (caractere == 0) {
      return true;
    }
  }
  // estourou o tamanho de str
  return false;
}

//-----------------------------------------------------------------------------|



//Salva o estado da CPU para o processo PID;
static process_t *process_save(so_t *self, unsigned int PID_){


  struct cpu_info_t_so  *cpuInfo = calloc(1, sizeof(cpu_info_t));

  mem_le(self->mem, IRQ_END_A, &(cpuInfo->A));
  mem_le(self->mem, IRQ_END_X, &(cpuInfo->X));
  mem_le(self->mem, IRQ_END_complemento, &(cpuInfo->complemento));
  mem_le(self->mem, IRQ_END_PC, &(cpuInfo->PC));
  // Pq tenho que converter um enum * para int *???
  mem_le(self->mem, IRQ_END_modo, (int *)&(cpuInfo->modo));

  process_t  *p = ptable_search(self->processTable, PID_);

  if(p)
    proc_set_cpuinfo(p, cpuInfo);

  return p;
}
//Restaura o estado da CPU do processo PID;
static err_t process_recover(so_t *self, process_t *process){

  err_t err = ERR_OK;

  if(!process) {
    if (ptable_is_empty(self->processTable)) { // Sem processos
      err = ERR_CPU_PARADA;
    } else {                                  // Tabela de processo bloqueada
      err = ERR_CPU_PARADA;
    }

  } else {

    process_t  *p = process;
    struct cpu_info_t_so  *cpuInfo = proc_get_cpuinfo(p);

    mem_escreve(self->mem, IRQ_END_A, (cpuInfo->A));
    mem_escreve(self->mem, IRQ_END_X, (cpuInfo->X));
    mem_escreve(self->mem, IRQ_END_complemento, (cpuInfo->complemento));
    mem_escreve(self->mem, IRQ_END_PC, (cpuInfo->PC));
    mem_escreve(self->mem, IRQ_END_modo, (cpuInfo->modo));

    self->runningP = proc_get_PID(process);

  }
  return err;
}

int so_start_ptable_sched(so_t *self){
  self->processTable = ptable_create();
  self->scheduler = sched_create(self->relogio);

  return 0;
}

static int so_registra_proc(so_t *self, unsigned int address){
  struct cpu_info_t_so  *cpuInfo = calloc(1, sizeof(cpu_info_t));
  cpuInfo->modo = usuario;
  cpuInfo->PC = address;
  cpuInfo->complemento = 0;
  cpuInfo->X = 0;
  // cpu.A: Herdado do processo que cria ou do lixo que estiver na memória
  mem_le(self->mem, IRQ_END_A, &(cpuInfo->A));
  void *p =  ptable_add_proc(self->processTable, cpuInfo, ++PID, address);
  if (!p) {
    (console_print_status(self->console, "ptable: Erro ao registrar processo"));
    return -1;
  }
  if(sched_add(self->scheduler, p, PID, QUANTUM) != 0)
    console_printf(self->console, "sched: Erro ao adicionar processo");

  return 0;

}

//Mata um processo, não faz verificações.
// Na prática: Remove da lista scheduler(necessário) e remove da lista de
// processos
static int so_mata_proc(so_t *self, unsigned int PID){

  sched_remove(self->scheduler, PID);
  int r =  proc_delete(self->processTable, PID);

  return  so_broadcast_procs_block_PID(self, PID);
}

// Define processo com bloqueado e esperando por certo dispositivo
static int so_device_busy(so_t *self, void *disp, unsigned int id){
  process_t  *p = ptable_search(self->processTable, self->runningP);

  // Quando processo retornar deve requerir novamente ao dispositivo
  cpu_info_t_so *c = proc_get_cpuinfo(p);
  //c->PC = c->PC -1;
  proc_set_cpuinfo(p, c);

  // Altera estado do processo, define que está esperando device,
  // remove processo do escalonador
  proc_set_state(p, blocked_dev);
  //É necessario passar a controladora e o ID
  proc_set_waiting_disp(p, disp, id);
  sched_remove(self->scheduler, self->runningP);

  return 0;

}

// Define processo com bloqueado e esperando por certo Processo
static int so_wait_proc(so_t *self){
  process_t  *p = ptable_search(self->processTable, self->runningP);
  cpu_info_t_so  *cpuInfo = proc_get_cpuinfo(p);
  unsigned  int PID = cpuInfo->X;

  process_t * espera = ptable_search(self->processTable, PID);
  if(!espera) {
    cpuInfo->A = -1;
  } else {
    // Altera estado do processo, define que está esperando processo,
    // remove processo do escalonador
    proc_set_state(p, blocked_proc);
    proc_set_waiting_PID(p, PID);
    sched_remove(self->scheduler, self->runningP);
    cpuInfo->A = 0;
  }

  proc_set_cpuinfo(p, cpuInfo);
  return 0;
}

// Parecido como pthread_signal_broadcast
static int so_broadcast_procs_block_PID(so_t *self, unsigned int PID){
  return ptable_wakeup_PID(self->processTable, PID, self->scheduler, QUANTUM);
}

static int  so_broadcast_procs_block_dev(so_t *self, void *device,
                                        unsigned int PID){
  return ptable_wakeup_dev(self->processTable, device, PID, self->scheduler, QUANTUM);
}