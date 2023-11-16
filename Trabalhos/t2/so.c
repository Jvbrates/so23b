#include "so.h"
#include "irq.h"
#include "programa.h"
#include "instrucao.h"
#include "tabpag.h"
#include "process_mng.h"
#include "scheduler_interface.h"


#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// intervalo entre interrupções do relógio
#define INTERVALO_INTERRUPCAO 50   // em instruções executadas

//Velocidade da memória secundária
#define DELAY_SEC_MEM 20

#define QUANTUM 10
#define NUM_ES 4 // 4 terminais

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
  mem_t *mem;

  //memória secundária
  mem_t *sec_mem;
  int sm_req; // Numero de requisições ao disco

  mmu_t *mmu;
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
  int tempo_chamda_anterior;


  // quando tiver memória virtual, o controle de memória livre e ocupada
  //   é mais completo que isso
  int quadro_livre;
  int sec_mem_quadro_livre;
  // quando tiver processos, não tem essa tabela aqui, tem que tem uma para
  //   cada processo
  tabpag_t *tabpag;
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
static ret_so_carrega_programa so_carrega_programa(so_t *self, char *nome_do_executavel);
static bool so_copia_str_do_processo(so_t *self, int tam, char str[tam],
                                     int end_virt/*, processo*/);
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
  self->log = log_init("registro\0");

  self->cpu = cpu;
  self->mem = mem;
  self->console = console;
  self->relogio = relogio;
  memset(self->es_pendencias, 0, sizeof(self->es_pendencias));

  // quando a CPU executar uma instrução CHAMAC, deve chamar essa função
  cpu_define_chamaC(self->cpu, so_trata_interrupcao, self);
  // coloca o tratador de interrupção na memória

  mem_escreve(self->mem, 10, CHAMAC);
  mem_escreve(self->mem, 11, RETI);

  // programa a interrupção do relógio
  rel_escr(self->relogio, 2, INTERVALO_INTERRUPCAO);

  self->tempo_chamda_anterior = rel_agora(self->relogio);


  // T2 < ----
  self->mmu = mmu;
  self->sec_mem = sec_mem;
  // inicializa a tabela de páginas global, e entrega ela para a MMU
  // com processos, essa tabela não existiria, teria uma por processo
  self->tabpag = tabpag_cria();
  mmu_define_tabpag(self->mmu, self->tabpag);
  // define o primeiro quadro livre de memória como o seguinte àquele que
  //   contém o endereço 99 (as 100 primeiras posições de memória (pelo menos)
  //   não vão ser usadas por programas de usuário)
  self->sec_mem_quadro_livre = 0;

  self->quadro_livre = 99 / TAM_PAGINA + 1;
  return self;
}

void so_destroi(so_t *self)
{
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

static void so_salva_estado_da_cpu(so_t *self)
{
  // se não houver processo corrente, não faz nada
  // salva os registradores que compõem o estado da cpu no descritor do
  //   processo corrente
  // mem_le(self->mem, IRQ_END_A, endereco onde vai o A no descritor);
  // mem_le(self->mem, IRQ_END_X, endereco onde vai o X no descritor);
  // etc
}
static err_t so_trata_pendencias(so_t *self) // <-- FIXME Obvio que os processos não vão desbloquear se tu tratar isto
{
  // realiza ações que não são diretamente ligadar com a interrupção que
  //   está sendo atendida:
  // - E/S pendente
  // - desbloqueio de processos
  // - contabilidades

  return ERR_OK;
}
static void so_escalona(so_t *self)
{
  // escolhe o próximo processo a executar, que passa a ser o processo
  //   corrente; pode continuar sendo o mesmo de antes ou não
}
static void so_despacha(so_t *self)
{
  // se não houver processo corrente, coloca ERR_CPU_PARADA em IRQ_END_erro
  // se houver processo corrente, coloca todo o estado desse processo em
  //   IRQ_END_*
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

  so_start_ptable_sched(self);

  //Reset tabela de pendências
  memset(self->es_pendencias, 0, sizeof(self->es_pendencias));

  //Reset o PID também;
  PID = 0;

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
  mem_escreve(self->mem, IRQ_END_PC, ender);
  // passa o processador para modo usuário
  mem_escreve(self->mem, IRQ_END_modo, usuario);
  return ERR_OK;
}

/*IRQ_ERR_CPU:
 * Mata o processo que causou o erro, se possível*/
static err_t so_trata_irq_err_cpu(so_t *self) // <-----
{

  // Ocorreu um erro interno na CPU
  // O erro está codificado em IRQ_END_erro
  // Em geral, causa a morte do processo que causou o erro
  // Ainda não temos processos, causa a parada da CPU
  int err_int;
  mem_le(self->mem, IRQ_END_erro, &err_int);
  err_t err = err_int;
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

        log_ocioso(self->log);

        return ERR_OK;
      }

      break;
    }
    case ERR_PAG_AUSENTE:
      console_printf(self->console, "Processo que causou erro PAG_AUSENTE %i", self->runningP);
    case ERR_END_INV: {

      console_printf(self->console, "Processo que causou erro END_INV %i", self->runningP);
      // TODO: Continuar aqui, já está gerando ERR_END_INV
      /* 1. Verificar se o processo A está acessando um endereço válido;
       * 2. Encontrar o endereço do processo na memória secundária
       * 3. Escolher uma quadro para substituir
       * 4. Se este quadro estava sendo usada por outro processo B
       *  4.1 Caso B tenha alterado o quadro: Gravar este quadro na memória secundária na página correspondente de B
       *  4.2 Inválidar este quadro de memória na tpag de B
       * 5. Copiar a memória do disco de A para a memória principal
       * 6. Atualizar a tab_pag de A;
       * 7. Bloquear o processo pelo tempo que simula as operações de disco
       * */


      int addr;
      mmu_le(self->mmu, IRQ_END_complemento, &addr, supervisor);


      // 1 e 2
      int addr_mem_sec = proc_get_page_addr(self->p_runningP, addr, TAM_PAGINA);
      if(addr_mem_sec == -1) {
        console_printf(self->console, "SO: Matando %i [ERR_END_INV] %i",
                       self->runningP, addr);

        so_mata_proc(self, self->p_runningP);
        return  ERR_OK;
      }

      // 3.
      // 4.
      // 5.
      int quadro_mem = self->quadro_livre++;
      so_mem_to_mem(self->sec_mem, addr_mem_sec, self->mem, quadro_mem*TAM_PAGINA, TAM_PAGINA);

      // 6.
      tabpag_t *proc_pag = proc_get_tpag(self->p_runningP);
      tabpag_define_quadro(proc_pag, addr/TAM_PAGINA, quadro_mem);

      // 7.

      return ERR_OK;


      break ;
    }
    //case ERR_PAG_AUSENTE: {

      /* TODO: Como é possível erro de pagina ausente se eu ainda não programei
       * substituição de pag?
       */
      /* ANSWER: Considere que o programa contem as paginas A,B,C dispostas
       * sequencialmente em seu espaco de endereçamento.
       * A tabela de paginas, um vetor, do programa em dado momento é tab[frame_A],
       * somente A está mapeado.
       * Caso A chame um função em C:
       *  Como a relação página-quadro é definido com base no indice da
       *  tabela, está será redimensionada para tab[frame_A, -1, frame_C]
       *  Assim, caso o programa acesse a memória em B ele receberá um ERR_PAG_AUSENTE
       *  */


      //break ;
    //}
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


// TODO fica para daqui a pouco
static void so_chamada_cria_proc(so_t *self)
{
  // ainda sem suporte a processos, carrega programa e passa a executar ele
  // quem chamou o sistema não vai mais ser executado, coitado!

  // em X está o endereço onde está o nome do arquivo
  int ender_proc;
  // deveria ler o X do descritor do processo criador
  if (mem_le(self->mem, IRQ_END_X, &ender_proc) == ERR_OK) {
    char nome[100];
    if (so_copia_str_do_processo(self, 100, nome, ender_proc)) {
      ret_so_carrega_programa ender_carga = so_carrega_programa(self, nome);
      // o endereço de carga é endereço virtual, deve ser 0
      if (ender_carga.end_virt_ini >= 0) {
        // deveria escrever no PC do descritor do processo criado
        mem_escreve(self->mem, IRQ_END_PC, ender_carga.end_virt_ini);
        return;
      }
    }
  }
  // deveria escrever -1 (se erro) ou o PID do processo criado (se OK) no reg A
  //   do processo que pediu a criação
  mem_escreve(self->mem, IRQ_END_A, -1);
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



static ret_so_carrega_programa so_carrega_programa(so_t *self, char *nome_do_executavel)
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

   //Nao mapeia, tudo começará na memória secundária
   // mapeia as páginas nos quadros
  /*int quadro = quadro_ini;
  for (int pagina = pagina_ini; pagina <= pagina_fim; pagina++) {
    tabpag_define_quadro(self->tabpag, pagina, quadro);
    quadro++;
  }
  self->quadro_livre = quadro;
*/


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

    console_printf(self->console,
                   "Carregando em memória secundária, end virt %d fís %d\n", end_virt, end_fis);
    end_fis++;
  }
  prog_destroi(prog);

  return r;
}


// TODO TRATAR ISTO AQUI MAIS TARDE
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
static bool so_copia_str_do_processo(so_t *self, int tam, char str[tam],
                                     int end_virt/*, processo*/)
{
  for (int indice_str = 0; indice_str < tam; indice_str++) {
    int caractere;
    // não tem memória virtual implementada, posso usar a mmu para traduzir
    //   os endereços e acessar a memória
    if (mmu_le(self->mmu, end_virt + indice_str, &caractere, usuario) != ERR_OK) {
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


/*Mata o processo:
 * - Remove do scheduller, caso necessário
 * - Desbloqueia processos que estevam esperando a morte deste
 * - Decrementa de uma das listas de pendência, caso necessário
 * - Remove da tabela de processos
 * */
static int so_mata_proc(so_t *self, process_t *p){

  process_state_t estado = proc_get_state(p);
  proc_set_end_time(p, rel_agora(self->relogio));

  proc_set_state(p, dead);

  if(estado == waiting || estado == running) {
    sched_remove(self->scheduler, proc_get_PID(p));

    // FIXME: Isto não deveria estar nos outros if's também?
    ptable_proc_wait(self->processTable, proc_get_PID(p),self->scheduler, QUANTUM);
  } else if( estado == blocked_read){
    (self->es_pendencias[proc_get_PID_or_device(p)][1])--;
  } else if(estado == blocked_write){
    (self->es_pendencias[proc_get_PID_or_device(p)][0])--;
  }
  //ptable_delete(self->processTable, proc_get_PID(p));

  return 0;
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
                   "Processo [%i] existe mas não está na ptable "
                   , PID_);
    return -1;
  }


  proc_set_state(p, state);
  proc_set_PID_or_device(p, PID_or_device);
  sched_remove(self->scheduler, PID_);


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

/*Inicializa tabela de processo e escalonador*/
int so_start_ptable_sched(so_t *self){
  self->processTable = ptable_create();
  self->scheduler = sched_create(self->relogio, self->log);

  return 0;
}