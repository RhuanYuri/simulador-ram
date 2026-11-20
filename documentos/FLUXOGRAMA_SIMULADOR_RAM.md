# Especificação e Fluxogramas do Sistema: Simulador de Memória (ROM + RAM)

**Universidade Federal do Maranhão (UFMA)**  
**Centro de Ciências Exatas e Tecnologia (CCET) — Departamento de Informática**  
**Disciplina:** Arquitetura de Computadores  
**Autores:** Caio Ferreira Sousa, Danilo Nava Soares Lima, Raynara Ribeiro Alves, Rhuã Yuri Nascimento Sardinha, Yuri Aguiar Gonçalves  
**Código-fonte de referência:** [simulador_ram.c](file:///home/yuri/Documentos/UFMA/Computacao/Arquitetura%20de%20Computadores/projeto/documentos/simulador_ram.c)

---

## 1. Visão Geral da Arquitetura Simulada

O sistema implementa uma simulação da arquitetura de barramentos e subsistema de memória (modelo Von Neumann) com as seguintes características:
- **Espaço Total:** 4096 bytes (4 KB, endereçamento de 12 bits: `0x000` a `0xFFF`).
- **Segmento ROM (BIOS/Bootloader):** Endereços `0x000` a `0x0FF` (0 a 255 — 256 bytes) com proteção configurável contra escrita (MPU).
- **Segmento RAM:** Endereços `0x100` a `0xFFF` (256 a 4095 — 3840 bytes) para leitura e escrita irrestritas.
- **Barramentos e Registradores:**
  - **MAR (*Memory Address Register*):** Armazena o endereço ativo selecionado para a operação.
  - **MBR/MDR (*Memory Buffer Register*):** Armazena o dado trafegado no barramento de dados (8 bits).
  - **Linhas de Controle:** Sinais `BUS_IDLE`, `BUS_READ` (MEMR), `BUS_WRITE` (MEMW) e `BUS_PROT_FAULT`.

---

## 2. Fluxograma Geral do Sistema (`main`)

O loop principal gerencia a inicialização, a exibição do cabeçalho e menu com atualização em tempo real, o roteamento da opção selecionada pelo usuário e a persistência do estado até o encerramento.

```mermaid
flowchart TD
    Inicio(["Início (main)"]) --> InitMem["inicializar_memoria(&memoria)\n- Zera 4096 células\n- rom_protegida = 1\n- MAR = 0, MBR = 0\n- Sinal = BUS_IDLE\n- Zera métricas"]
    InitMem --> SetMsg["msg_status = 'Sistema pronto.'"]
    
    SetMsg --> LoopMenu["Loop do Sistema: do ... while (opcao != 0)"]
    LoopMenu --> LimpaTela["limpar_tela()"]
    LimpaTela --> ExibeCabecalho["exibir_cabecalho()\n- Dados institucionais\n- MAR, MBR, Último Sinal\n- msg_status"]
    ExibeCabecalho --> ExibeMenu["exibir_menu()\nOpções 0 a 7"]
    ExibeMenu --> LerOpcao[/"Entrada do Usuário (fgets)"/]
    
    LerOpcao --> ValidaOp{"interpretar_numero()\nEntrada numérica válida?"}
    ValidaOp -- Não --> ErroOp["msg_status = 'Opção inválida!'\npausar_tela()"]
    ErroOp --> LoopMenu
    
    ValidaOp -- Sim --> SwitchOp{"Qual a opção escolhida?"}
    
    SwitchOp -- "1" --> Rota1["Operação de Escrita (MEMW)"]
    SwitchOp -- "2" --> Rota2["Operação de Leitura (MEMR)"]
    SwitchOp -- "3" --> Rota3["Exibir DASHBOARD Completo"]
    SwitchOp -- "4" --> Rota4["Exibir Hex Dump / Mapa"]
    SwitchOp -- "5" --> Rota5["Carregar Programa Demo (Bootloader)"]
    SwitchOp -- "6" --> Rota6["Alternar Proteção da ROM"]
    SwitchOp -- "7" --> Rota7["Limpeza / Reset de Memória"]
    SwitchOp -- "0" --> Rota0["Encerrar Simulador"]
    SwitchOp -- "Outra" --> RotaDefault["msg_status = 'Opção inexistente!'"]
    
    Rota1 --> PosOp{"opcao != 0?"}
    Rota2 --> PosOp
    Rota3 --> PosOp
    Rota4 --> PosOp
    Rota5 --> PosOp
    Rota6 --> PosOp
    Rota7 --> PosOp
    RotaDefault --> PosOp
    
    PosOp -- Sim --> Pausa["pausar_tela()\nAguardar ENTER"]
    Pausa --> LoopMenu
    
    PosOp -- Não --> Fim(["Fim do Programa (return 0)"])
    Rota0 --> Fim
```

---

## 3. Fluxograma da Operação de Escrita (`MEMW` - Opção 1)

Ilustra a lógica de validação de formato numérico, envio de endereço ao MAR, barramento de dados ao MBR, verificação de violação da ROM (MPU) e armazenamento físico em célula de memória.

```mermaid
flowchart TD
    InicioEscrita(["Início: Opção 1 (MEMW)"]) --> LeEnd[/"ler_inteiro_validado: Endereço (0 a 4095)"/]
    LeEnd --> ValEnd{"Endereço válido e na faixa?"}
    ValEnd -- Não --> FalhaEnd["msg_status = 'Falha: endereço inválido'\nRetorna"]
    
    ValEnd -- Sim --> LeDado[/"ler_inteiro_validado: Dado (0 a 255)"/]
    LeDado --> ValDado{"Dado válido (8 bits)?"}
    ValDado -- Não --> FalhaDado["msg_status = 'Falha: dado inválido'\nRetorna"]
    
    ValDado -- Sim --> ExecEscrita["escrever_memoria(&memoria, endereco, dado)"]
    
    ExecEscrita --> ValidaLimite{"endereco_valido(endereco)?"}
    ValidaLimite -- Não --> ErroCritico["Exibe ERRO CRÍTICO de limites\nRetorna"]
    
    ValidaLimite -- Sim --> SetRegs["MAR = endereco\nMBR = dado"]
    SetRegs --> CheckROM{"eh_regiao_rom(endereco) E\nrom_protegida == 1?"}
    
    CheckROM -- Sim (Violação MPU) --> FalhaROM["Sinal = BUS_PROT_FAULT\nviolacoes_rom++\nExibe alerta de violação de proteção\nmsg_status = 'Escrita bloqueada em ROM'"]
    FalhaROM --> FimEscrita(["Retorna ao Menu"])
    
    CheckROM -- Não (Permitido) --> CheckSobrescrita{"mem->ocupado[endereco] == 1?"}
    CheckSobrescrita -- Sim --> AlertaSobrescrita["Exibe aviso de dado anterior sobrescrito"]
    CheckSobrescrita -- Não --> Gravacao
    AlertaSobrescrita --> Gravacao
    
    Gravacao["ptr_celula = obter_ponteiro_memoria()\n*ptr_celula = dado\nocupado[endereco] = 1\nSinal = BUS_WRITE\ntotal_escritas++"]
    
    Gravacao --> IncRegiao{"eh_regiao_rom(endereco)?"}
    IncRegiao -- Sim --> IncROM["escritas_rom++"]
    IncRegiao -- Não --> IncRAM["escritas_ram++"]
    
    IncROM --> SucessoEscrita["Exibe confirmação MEMW com dados em\nHexadecimal, Decimal, Binário e ASCII\nAtualiza msg_status"]
    IncRAM --> SucessoEscrita
    SucessoEscrita --> FimEscrita
```

---

## 4. Fluxograma da Operação de Leitura (`MEMR` - Opção 2)

Ilustra o ciclo de busca de dados no subsistema de memória através do MAR e MBR.

```mermaid
flowchart TD
    InicioLeitura(["Início: Opção 2 (MEMR)"]) --> LeEnd[/"ler_inteiro_validado: Endereço (0 a 4095)"/]
    LeEnd --> ValEnd{"Endereço válido?"}
    ValEnd -- Não --> FalhaEnd["msg_status = 'Falha: endereço inválido'\nRetorna"]
    
    ValEnd -- Sim --> ExecLeitura["ler_memoria(&memoria, endereco)"]
    ExecLeitura --> ValidaLimite{"endereco_valido(endereco)?"}
    ValidaLimite -- Não --> ErroCritico["Exibe ERRO CRÍTICO\nRetorna"]
    
    ValidaLimite -- Sim --> SetMAR["MAR = endereco"]
    SetMAR --> GetPtr["ptr_celula = obter_ponteiro_memoria()\ndado = *ptr_celula"]
    GetPtr --> SetMBR["MBR = dado\nSinal = BUS_READ\ntotal_leituras++"]
    
    SetMBR --> CheckRegiao{"eh_regiao_rom(endereco)?"}
    CheckRegiao -- Sim --> IncROM["leituras_rom++"]
    CheckRegiao -- Não --> IncRAM["leituras_ram++"]
    
    IncROM --> ExibeDados["Exibe dados lidos (Hex, Dec, Bin, ASCII)\ne estado da célula (OCUPADA / VAZIA)\nAtualiza msg_status"]
    IncRAM --> ExibeDados
    ExibeDados --> FimLeitura(["Retorna ao Menu"])
```

---

## 5. Fluxograma de Carga do Bootloader / Programa Demo (Opção 5)

Processo de gravação de fábrica simulando a gravação física de firmware na ROM e inicialização do espaço de dados na RAM.

```mermaid
flowchart TD
    InicioDemo(["Início: Opção 5 (Demo)"]) --> SalvaProt["protecao_anterior = rom_protegida"]
    SalvaProt --> Desprotege["rom_protegida = 0 (Desbloqueio temporário)"]
    
    Desprotege --> GravaBIOS["Grava string de identificação da BIOS\nna ROM (0x000 em diante)"]
    GravaBIOS --> GravaReset["Grava Vetor de Reset em 0x00F0 a 0x00FF"]
    GravaReset --> RestauraProt["rom_protegida = protecao_anterior\n(Restaura proteção original)"]
    
    RestauraProt --> GravaRAM["Grava string de demonstração na RAM\n(a partir de 0x0100 até 0x0140)"]
    GravaRAM --> AtualizaRegs["MAR = 0x100 (INICIO_RAM)\nMBR = dados[0x100]\nSinal = BUS_WRITE"]
    AtualizaRegs --> ExibeMsg["Exibe resumo de carga da BIOS e RAM\nmsg_status = 'Programa demo carregado'"]
    ExibeMsg --> FimDemo(["Retorna ao Menu"])
```

---

## 6. Fluxograma de Limpeza e Reset do Sistema (Opção 7)

Apresenta as opções de reset parcial (apenas variáveis em RAM) versus reset a frio completo (ROM, RAM, métricas e registradores).

```mermaid
flowchart TD
    InicioLimpeza(["Início: Opção 7 (Limpar Memória)"]) --> MenuLimpeza[/"Usuário escolhe opção de limpeza:\n1 - Apenas RAM\n2 - Reset Total\n0 - Cancelar"/]
    
    MenuLimpeza --> DecisaoLimpeza{"Opção selecionada?"}
    
    DecisaoLimpeza -- "1" --> LimparRAM["limpar_memoria(&memoria, 0)\n- Itera de 0x100 (INICIO_RAM) a 4095\n- Zera dados[i] e ocupado[i]\n- Zera escritas_ram e leituras_ram\n- ROM é preservada intacta"]
    LimparRAM --> MsgRAM["msg_status = 'Memória RAM limpa (ROM preservada)'"]
    
    DecisaoLimpeza -- "2" --> ResetTotal["limpar_memoria(&memoria, 1)\n- Itera de 0 a 4095 (ROM + RAM)\n- Zera dados[i] e ocupado[i]\n- Zera todos os contadores de acesso\n- MAR = 0, MBR = 0\n- Sinal = BUS_IDLE"]
    ResetTotal --> MsgTotal["msg_status = 'Reset total do sistema realizado'"]
    
    DecisaoLimpeza -- "0" --> Cancela["msg_status = 'Operação cancelada'"]
    DecisaoLimpeza -- "Outra" --> Invalida["msg_status = 'Opção de limpeza inválida'"]
    
    MsgRAM --> FimLimpeza(["Retorna ao Menu"])
    MsgTotal --> FimLimpeza
    Cancela --> FimLimpeza
    Invalida --> FimLimpeza
```

---

## 7. Fluxograma do Analisador Léxico Flexível (`interpretar_numero`)

Rotina fundamental que permite a entrada unificada em múltiplas bases numéricas (Decimal, Hexadecimal com prefixo `0x` ou sufixo `h`, e Binário com `0b`).

```mermaid
flowchart TD
    InicioParser(["interpretar_numero(entrada)"]) --> LimpaEspaco["Pula espaços em branco iniciais"]
    LimpaEspaco --> CheckVazio{"String vazia?"}
    CheckVazio -- Sim --> RetInvalido["Retorna 0 (Inválido)"]
    
    CheckVazio -- Não --> CheckBin{"Inicia com '0b' ou '0B'?"}
    
    CheckBin -- Sim --> LoopBin["Para cada caractere após '0b':\nacumulador = (acumulador << 1) | bit"]
    LoopBin --> ValidaBit{"Caractere é '0' ou '1'?"}
    ValidaBit -- Não --> RetInvalido
    ValidaBit -- Sim --> FimBin["*valor_saida = acumulador\nRetorna 1 (Sucesso)"]
    
    CheckBin -- Não --> TentaStrtol["resultado = strtol(entrada, &fim, 0)"]
    TentaStrtol --> CheckFimStrtol{"Leu algum caractere?"}
    
    CheckFimStrtol -- Não --> TentaHexH["resultado = strtol(entrada, &fim, 16)\n(Tenta hex puro sem 0x)"]
    TentaHexH --> CheckHexH{"Conseguiu ler?"}
    CheckHexH -- Não --> RetInvalido
    CheckHexH -- Sim --> CheckSufixoH
    
    CheckFimStrtol -- Sim --> CheckSufixoH{"Caractere final é 'h' ou 'H'?"}
    CheckSufixoH -- Sim --> PulaH["fim++"]
    CheckSufixoH -- Não --> ValidaFim
    PulaH --> ValidaFim
    
    ValidaFim["Pula espaços finais"] --> CheckFimStr{"Chegou a '\\0'?"}
    CheckFimStr -- Sim --> SucessoFinal["*valor_saida = resultado\nRetorna 1 (Sucesso)"]
    CheckFimStr -- Não --> RetInvalido
```

---

## 8. Resumo das Estruturas e Papéis na Arquitetura

| Componente | Tipo / Representação | Papel Arquitetural |
| :--- | :--- | :--- |
| `dados[4096]` | `uint8_t[]` | Matriz física de células de armazenamento de 8 bits. |
| `ocupado[4096]` | `uint8_t[]` | Tabela de validade/presença (*valid bit*) de dados gravados. |
| `mar` | `int` (12 bits úteis) | *Memory Address Register* — Ponteiro de barramento de endereços. |
| `mbr` | `uint8_t` (8 bits) | *Memory Buffer Register* — Registrador de transferência de dados. |
| `rom_protegida` | `int` (0 ou 1) | Mecanismo de hardware simulado de MPU (*Memory Protection Unit*). |
| `ultimo_sinal` | `SinalBarramento` | Linhas de controle do barramento de sistema (`MEMR`, `MEMW`, `PROT_FAULT`, `IDLE`). |
