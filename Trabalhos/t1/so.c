#include "so.h"
#include "irq.h"
#include "programa.h"
#include "process_mng.h"
#include "scheduller_interface.h"

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
  scheduller_t * scheduller;
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
int so_start_ptable_sched(so_t *self, int process_zer0_addr);
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
static int so_device_busy(so_t *self, void *disp);
static int so_wait_proc(so_t *self, void *disp, unsigned int PID);
static int  so_broadcast_procs_block_PID(so_t *self, unsigned int PID);
// funções auxiliares para tratar cada tipo de interrupção

//Salva e Recover
static process_t *process_save(so_t *self, unsigned int PID);
static int process_recover(so_t *self, unsigned int PID);


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
  console_printf(self->console, "SO: recebi IRQ %d (%s)", irq, irq_nome(irq));

  process_t  *p = process_save(self, self->runningP);
  proc_set_state(p, waiting);

  /*TODO: No caso de um processo fazer uma requisição a um device ocupado, seu
   * PC deve ser PC-1 para quando o dispositivo estiver pronto ele fazer essa
   * requisição novamente?
   *
   * TODO: Na funcao mata proc, devo debloquear qualquer processo que esteja
   * esperando por ela e adicionar ao escalonador. Isto vai exigir uma estrutura
   * struct {proc_state_t tipo_bloq, int ID_dev_or_p, scheduller *}
   * */

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



  process_t  *to_run = sched_get_update(self->scheduller);

  self->runningP = proc_get_PID(p);

  process_recover(self, self->runningP);

  return err;
}

static err_t so_trata_irq_reset(so_t *self)
{
  //Reset ptable e sched;
  if(self->processTable)
      ptable_destruct(self->processTable);

  if(self->scheduller)
      sched_destruct(self->scheduller);

  // coloca um programa na memória
  int ender = so_carrega_programa(self, "init.maq");
  if (ender != 100) {
    console_printf(self->console, "SO: problema na carga do programa inicial");
    return ERR_CPU_PARADA;
  }

  //Quando o scheduler não tiver nada para escalonar ele vai chamar init.maq
  so_start_ptable_sched(self, ender);

  // altera o PC para o endereço de carga (deve ter sido 100)
  mem_escreve(self->mem, IRQ_END_PC, ender);
  // passa o processador para modo usuário
  mem_escreve(self->mem, IRQ_END_modo, usuario);
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
  for (;;) {
    int estado;
    term_le(self->console, 1, &estado);
    if (estado != 0) break;
    // como não está saindo do SO, o laço do processador não tá rodando
    // esta gambiarra faz o console andar
    console_tictac(self->console);
    console_atualiza(self->console);
  }
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
  term_escr(self->console, 2, dado);
  mem_escreve(self->mem, IRQ_END_A, 0);
}

static void so_chamada_cria_proc(so_t *self)
{
  // ainda sem suporte a processos, carrega programa e passa a executar ele
  // quem chamou o sistema não vai mais ser executado, coitado!

  // em X está o endereço onde está o nome do arquivo
  int ender_proc;
  if (mem_le(self->mem, IRQ_END_X, &ender_proc) == ERR_OK) {
    char nome[100];
    if (copia_str_da_mem(100, nome, self->mem, ender_proc)) {
      int ender_carga = so_carrega_programa(self, nome);
      if (ender_carga > 0) {
        so_registra_proc(self, ender_carga);
      } else {
        console_print_status(self->console, "mem: Erro ao carregar processo, "
                                            "não registrado");
      }
    }

  }
  mem_escreve(self->mem, IRQ_END_A, -1); //Todo remover este -1
}

static void so_chamada_mata_proc(so_t *self)
{
  // ainda sem suporte a processos, retorna erro -1
  console_printf(self->console, "SO: SO_MATA_PROC não implementada");
  unsigned int ID_kill;
  if (mem_le(self->mem, IRQ_END_X, &ID_kill) == ERR_OK) {
    if(!ID_kill){
      console_print_status(self->console, "SO(kill): tentativa de matar init.asm!");
    } else {
      process_t  *p = ptable_search(self->processTable, ID_kill);
      if(!p) {
        console_print_status(self->console, "SO(kill): Processo inesitente");
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
static process_t *process_save(so_t *self, unsigned int PID){

  struct cpu_info_t_so  *cpuInfo = calloc(1, sizeof(cpu_info_t));

  mem_le(self->mem, IRQ_END_A, &(cpuInfo->A));
  mem_le(self->mem, IRQ_END_X, &(cpuInfo->X));
  mem_le(self->mem, IRQ_END_complemento, &(cpuInfo->complemento));
  mem_le(self->mem, IRQ_END_PC, &(cpuInfo->PC));
  mem_le(self->mem, IRQ_END_modo, &(cpuInfo->modo));

  process_t  *p = ptable_search(self->processTable, PID);
  proc_set_cpuinfo(p, cpuInfo);

  return p;
}
//Restaura o estado da CPU do processo PID;
static int process_recover(so_t *self, unsigned int PID){

  process_t  *p = ptable_search(self->processTable, PID);
  struct cpu_info_t_so  *cpuInfo = proc_get_cpuinfo(p);

  mem_escreve(self->mem, IRQ_END_A, (cpuInfo->A));
  mem_escreve(self->mem, IRQ_END_X, (cpuInfo->X));
  mem_escreve(self->mem, IRQ_END_complemento, (cpuInfo->complemento));
  mem_escreve(self->mem, IRQ_END_PC, (cpuInfo->PC));
  mem_escreve(self->mem, IRQ_END_modo, (cpuInfo->modo));

  // Escrever na memória é suficiente, a instrução RETI carrega para o cpu

  return 0;

}

int so_start_ptable_sched(so_t *self, int process_zer0_addr){
  self->processTable = ptable_create();

  struct cpu_info_t_so  *cpuInfo = calloc(1, sizeof(cpu_info_t));
  cpuInfo->modo = usuario;
  cpuInfo->PC = process_zer0_addr;
  cpuInfo->complemento = 0;
  cpuInfo->X = 0;
  cpuInfo->A = 0;

  void *pzer0 =  ptable_add_proc(self->processTable,
                            cpuInfo, PID++,
                            process_zer0_addr);

  if (!pzer0) {
    (console_print_status(self->console, "ptable: Erro ao registrar processo 0"));
    return -1;
  }

  self->scheduller = sched_create(pzer0, self->relogio);

  return 0;
}

// Registra processos criados na CPU !! Não é usado no processo zer0
static int so_registra_proc(so_t *self, unsigned int address){
  struct cpu_info_t_so  *cpuInfo = calloc(1, sizeof(cpu_info_t));
  cpuInfo->modo = usuario;
  cpuInfo->PC = address;
  cpuInfo->complemento = 0;
  cpuInfo->X = 0;
  // cpu.A: Herdado do processo que cria ou do lixo que estiver na memória
  mem_le(self->mem, IRQ_END_A, &(cpuInfo->A));
  void *p =  ptable_add_proc(self->processTable, cpuInfo, PID++, address);
  if (!p) {
    (console_print_status(self->console, "ptable: Erro ao registrar processo"));
    return -1;
  }
  if(sched_add(self->scheduller, p, PID, QUANTUM) != 0)
    console_printf(self->console, "sched: Erro ao adicionar processo");

  return 0;

}

//Mata um processo, não faz verificações.
// Na prática: Remove da lista scheduller(necessário) e remove da lista de
// processos
static int so_mata_proc(so_t *self, unsigned int PID){

  sched_remove(self->scheduller, PID);
  int r =  proc_delete(self->processTable, PID);

  so_broadcast_procs_block_PID(self, PID);

}

// Define processo com bloqueado e esperando por certo dispositivo
static int so_device_busy(so_t *self, void *disp){
  process_t  *p = ptable_search(self->processTable, self->runningP);

  // Quando processo retornar deve requerir novamente ao dispositivo
  cpu_info_t_so *c = proc_get_cpuinfo(p);
  c->PC = c->PC -1;
  proc_set_cpuinfo(p, c);

  // Altera estado do processo, define que está esperando device,
  // remove processo do escalonador
  proc_set_state(p, blocked_dev);
  proc_set_waiting_disp(p, disp);
  sched_remove(self->scheduller, self->runningP);

}

// Define processo com bloqueado e esperando por certo Processo
static int so_wait_proc(so_t *self, void *disp, unsigned int PID){
  process_t  *p = ptable_search(self->processTable, self->runningP);

  // Altera estado do processo, define que está esperando processo,
  // remove processo do escalonador
  proc_set_state(p, blocked_proc);
  proc_set_waiting_PID(p, PID);
  sched_remove(self->scheduller, self->runningP);

  return 0;
}

// Parecido como pthread_signal_broadcast
static int  so_broadcast_procs_block_PID(so_t *self, unsigned int PID){
  ptable_wakeup_PID(self->processTable, PID, self->scheduller, QUANTUM);
}

static int  so_broadcast_procs_block_dev(so_t *self, void *device){
  ptable_wakeup_dev(self->processTable, device, self->scheduller, QUANTUM);
}