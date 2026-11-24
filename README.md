# Monitoramento de Contenção e Diagnóstico de Possível Inversão de Prioridade em Mutexes no Apache NuttX

Trabalho 2 da disciplina de Sistemas Operacionais Embarcados.

A implementação foi desenvolvida e testada no Apache NuttX para a plataforma ESP32-DevKitC, utilizando o modelo de compilação FLAT.

## Organização do repositório

A branch `main` contém somente os arquivos modificados ou adicionados, mantendo seus caminhos relativos dentro do NuttX.

A branch `implementation` contém a árvore completa e compilável do NuttX com a implementação integrada.

A branch `nuttx-base` contém a versão original utilizada como base:

- NuttX: `80a052ff9ccf29453329a8160e81bb050016ae8f`
- nuttx-apps: `0c56297b043157fccee9a56b2af06aa4c49d1ed0`

O arquivo `mutex-priority-monitor.patch` reúne todas as modificações em formato de patch.

## Descrição

O trabalho adiciona ao NuttX a interface `nxsched_get_mutexinfo()`, que permite consultar o estado de espera por mutex de uma tarefa identificada pelo seu TID.

A interface informa:

- se a tarefa está bloqueada esperando um mutex;
- o TID da tarefa que possui o mutex;
- as prioridades atuais da tarefa bloqueada e do dono;
- a existência de um possível risco de inversão de prioridade.

O risco é indicado quando a prioridade atual da tarefa bloqueada é maior que a prioridade atual do dono do mutex. Essa condição representa uma possibilidade de inversão, mas não comprova que uma tarefa intermediária esteja efetivamente impedindo o dono de executar.

A funcionalidade é apenas de observação: ela não altera prioridades, não libera mutexes e não interfere no escalonamento.

## Interface implementada

```c
int nxsched_get_mutexinfo(pid_t pid, FAR struct mutexinfo_s *info);
```

O parâmetro `pid` identifica a tarefa consultada. Quando seu valor é zero, a própria tarefa chamadora é consultada. O parâmetro `info` aponta para a estrutura que receberá o resultado:

```c
struct mutexinfo_s
{
  pid_t holder_pid;
  int waiter_priority;
  int holder_priority;
  bool waiting;
  bool inversion_risk;
};
```

Os campos têm os seguintes significados:

| Campo | Descrição |
| --- | --- |
| `holder_pid` | TID da tarefa que possui o mutex. |
| `waiter_priority` | Prioridade atual da tarefa consultada. |
| `holder_priority` | Prioridade atual do dono do mutex. |
| `waiting` | Indica se a tarefa está bloqueada esperando um mutex. |
| `inversion_risk` | Indica se o waiter tem prioridade atual maior que a do holder. |

A função retorna:

- `OK` quando a consulta é realizada, inclusive se a tarefa não estiver esperando um mutex;
- `-EINVAL` quando `info` é nulo;
- `-ESRCH` quando não existe uma tarefa com o TID informado.

## Funcionamento interno

A consulta é executada dentro de uma seção crítica para evitar que o estado das tarefas e do mutex mude durante a coleta das informações.

O fluxo da operação é:

```text
TID da tarefa
      |
      v
Localiza o TCB
      |
      v
Verifica se a tarefa está em espera por semáforo
      |
      v
Confirma que o objeto esperado é um mutex
      |
      v
Obtém o TID do holder e localiza seu TCB
      |
      v
Compara as prioridades atuais
      |
      v
Preenche mutexinfo_s
```

Em mais detalhes, a implementação:

1. valida o ponteiro de saída;
2. converte `pid == 0` no TID da tarefa chamadora;
3. inicializa a estrutura com valores que representam a ausência de espera;
4. entra em uma seção crítica;
5. localiza o TCB da tarefa consultada;
6. verifica o estado de bloqueio e o objeto pelo qual ela espera;
7. confirma que o objeto é um mutex;
8. obtém o holder do mutex e localiza seu TCB;
9. copia os TIDs e as prioridades atuais;
10. compara as prioridades e indica o possível risco de inversão.

## Arquivos modificados

### Núcleo do NuttX

| Arquivo | Finalidade |
| --- | --- |
| `include/nuttx/sched.h` | Declara a estrutura de resultado e a nova interface. |
| `sched/sched/sched_get_mutexinfo.c` | Implementa a consulta do estado de espera pelo mutex. |
| `sched/sched/Make.defs` | Inclui a implementação na compilação tradicional do NuttX. |
| `sched/sched/CMakeLists.txt` | Inclui a implementação na configuração equivalente com CMake. |
| `syscall/syscall.csv` | Registra a chamada para geração da syscall. |
| `include/sys/syscall_lookup.h` | Adiciona a entrada correspondente na tabela de syscalls. |

### Aplicação de teste

| Arquivo | Finalidade |
| --- | --- |
| `apps/examples/mutex_monitor/mutex_monitor_main.c` | Cria o cenário de contenção e valida a interface. |
| `apps/examples/mutex_monitor/Kconfig` | Disponibiliza a aplicação no `menuconfig`. |
| `apps/examples/mutex_monitor/Make.defs` | Integra a aplicação à compilação do `nuttx-apps`. |
| `apps/examples/mutex_monitor/Makefile` | Define o nome do programa, prioridade e fontes. |
| `apps/examples/mutex_monitor/.gitignore` | Ignora os artefatos locais de compilação da aplicação. |

## Aplicação de teste

A aplicação `MutexMonitor` cria três contextos com prioridades diferentes:

- tarefa `low`, prioridade 80, que adquire o mutex primeiro;
- tarefa principal, prioridade 100, que coordena o teste;
- tarefa `high`, prioridade 150, que tenta adquirir o mutex já ocupado.

Semáforos são usados somente para tornar a sequência determinística. Depois que a tarefa de alta prioridade está efetivamente bloqueada, a tarefa principal chama `nxsched_get_mutexinfo()` e verifica os dados retornados.

A aplicação também testa os seguintes casos:

- ponteiro de saída nulo;
- TID inexistente;
- tarefa existente que não está esperando um mutex.

## Compilação

### Usando a branch de implementação

Clone o repositório e selecione a branch completa:

```sh
git clone https://github.com/ribasnath/mutex-priority-monitor.git nuttx
cd nuttx
git switch implementation
```

No diretório ao lado, obtenha a versão correspondente do `nuttx-apps`:

```sh
cd ..
git clone https://github.com/apache/nuttx-apps.git apps
cd apps
git checkout 0c56297b043157fccee9a56b2af06aa4c49d1ed0
ln -s ../../nuttx/apps/examples/mutex_monitor examples/mutex_monitor
cd ../nuttx
```

Configure o ESP32-DevKitC:

```sh
./tools/configure.sh esp32-devkitc:nsh
make menuconfig
```

No `menuconfig`, habilite:

```text
Application Configuration
  -> Examples
    -> Mutex priority monitor
```

Em seguida, compile:

```sh
make -j4
```

### Usando o patch

Também é possível partir do commit base do NuttX e aplicar `mutex-priority-monitor.patch`:

```sh
git checkout 80a052ff9ccf29453329a8160e81bb050016ae8f
git apply mutex-priority-monitor.patch
```

O `nuttx-apps` ainda deve estar no commit informado anteriormente, com o diretório da aplicação disponibilizado em `apps/examples/mutex_monitor`.

## Execução no ESP32

Com a placa conectada, grave a imagem indicando a porta serial apropriada:

```sh
make download ESPTOOL_PORT=/dev/ttyACM0 ESPTOOL_BAUD=115200
```

Abra o terminal serial:

```sh
picocom /dev/ttyACM0 -b 115200
```

No NSH, execute:

```text
nsh> MutexMonitor
```

## Resultado observado

```text
nsh> MutexMonitor
[low] mutex adquirido
[main] low confirmou a aquisição
[high] tentando adquirir mutex
[main] high iniciou a tentativa
TID consultado=5 Waiting?=1 Prioridade do waiter=150
PID do holder=4 Prioridade do holder=80 Risco de inversão?=1
[TEST] NULL info: PASS
[TEST] invalid PID: PASS
[TEST] running task: PASS, priority=100
[high] mutex adquirido
[high] mutex liberado
[low] mutex liberado
```

Os valores dos TIDs podem mudar entre execuções. O resultado importante é que a tarefa `high` aparece esperando o mutex da tarefa `low`, com prioridade maior e `inversion_risk` igual a 1.

## Limitações

- A interface analisa apenas a espera direta de uma tarefa por um mutex.
- O resultado representa um retrato do estado no instante da consulta.
- A indicação de risco considera a relação entre as prioridades atuais do waiter e do holder.
- A interface não detecta cadeias de dependência entre vários mutexes.
- A interface não confirma a presença de uma tarefa intermediária em execução.
- Nenhuma ação corretiva é executada automaticamente.

## Verificação de estilo

Os arquivos C foram verificados com a ferramenta `nxstyle` do NuttX:

```sh
cc tools/nxstyle.c -o /tmp/nxstyle
/tmp/nxstyle sched/sched/sched_get_mutexinfo.c
/tmp/nxstyle apps/examples/mutex_monitor/mutex_monitor_main.c
```

Também foi verificada a ausência de erros de espaço em branco no patch:

```sh
git diff --check
```
