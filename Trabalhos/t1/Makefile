CC = gcc

# CFLAGS = -Wall -Werror -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -g
ifdef wno
	CFLAGS = -Wall -Werror -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast  \
	-Wno-unused-variable -Wno-unused-function -g
else
	# Vou ignorar pointer-int-cast em tod0 caso
	CFLAGS = -Wall -Werror -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -g
endif
# Defina o scheduler
ifdef rr2
	OBJS = cpu.o es.o memoria.o relogio.o console.o instrucao.o err.o util/linked_list.o \
    main.o programa.o controle.o process_mng.o round_robin2.o so.o irq.o util/metricas.o
else
	OBJS = cpu.o es.o memoria.o relogio.o console.o instrucao.o err.o util/linked_list.o \
    main.o programa.o controle.o process_mng.o round_robin.o so.o irq.o util/metricas.o
endif
LDLIBS = -lcurses

# OBJS = cpu.o es.o memoria.o relogio.o console.o instrucao.o err.o util/linked_list.o \
# main.o programa.o controle.o process_mng.o round_robin.o so.o irq.o
OBJS_MONT = instrucao.o err.o montador.o
# MAQS = trata_irq.maq init.maq ex1.maq ex2.maq ex3.maq ex4.maq ex5.maq ex6.maq
MAQS = init.maq ex1.maq ex2.maq ex3.maq ex4.maq ex5.maq ex6.maq p1.maq p2.maq p3.maq
TARGETS = main montador ${MAQS} trata_irq.maq

all: ${TARGETS}

# para gerar o montador, precisa de todos os .o do montador
montador: ${OBJS_MONT}

# para gerar o programa principal, precisa de todos os .o)
main: ${OBJS}
	$(CC) $(CFLAGS) -o main ${OBJS} $(LDLIBS)

# para transformar um .asm em .maq, precisamos do montador
# monta os programas de usuário no endereço 100
	#./montador -e 100 $*.asm > $*.maq
%.maq: %.asm montador
	end=100 ; \
	for arq in ${MAQS}; do \
	./montador -e $$end ./asm/`basename $$arq .maq`.asm > $$arq ;\
	end=`expr $$end + 1000` ;\
	done

maq:
	end=100 ; \
	for arq in ${MAQS}; do \
	./montador -e $$end ./asm/`basename $$arq .maq`.asm > ./bin/$$arq ;\
	end=`expr $$end + 1000` ;\
	done

# o módulo trata_irq tem que ser montado no endereço 10, que é para onde
# a CPU desvia quando recebe uma interrupção
trata_irq.maq: trata_irq.asm montador
	./montador -e 10 trata_irq.asm > trata_irq.maq

# apaga os arquivos gerados
clean:
	rm -f ${OBJS} ${OBJS_MONT} ${TARGETS} ${MAQS} ${OBJS:.o=.d}

# para calcular as dependências de cada arquivo .c (e colocar no .d)
%.d: %.c
	set -e; rm -f $@; \
	$(CC) -MM $(CPPFLAGS) $< > /tmp/$(notdir $@).$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < /tmp/$(notdir $@).$$$$ > $@; \
	rm -f /tmp/$@.$$$$

# inclui as dependências
include $(OBJS:.o=.d)

