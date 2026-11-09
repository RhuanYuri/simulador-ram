/*
 * ==============================================================================
 * UNIVERSIDADE FEDERAL DO MARANHÃO - UFMA
 * CENTRO DE CIÊNCIAS EXATAS E TECNOLOGIA - CCET
 * DEPARTAMENTO DE INFORMÁTICA
 * DISCIPLINA: ARQUITETURA DE COMPUTADORES
 *
 * SIMULADOR DE MEMÓRIA (ROM + RAM) COM DASHBOARD E REGISTRADORES DE BARRAMENTO
 * Autores: Caio Ferreira Sousa, Danilo Nava Soares Lima, Raynara Ribeiro Alves,
 *          Rhuã Yuri Nascimento Sardinha, Yuri Aguiar Gonçalves
 * Ano: 2026
 *
 * Especificações da Arquitetura Simulada:
 * - Espaço Total de Endereçamento: 4096 bytes (4 KB - endereços de 12 bits: 0x000 a 0xFFF)
 * - Região ROM (Read-Only Memory / BIOS): Endereços 0 a 255 (0x000 a 0x0FF - 256 bytes)
 * - Região RAM (Random Access Memory): Endereços 256 a 4095 (0x100 a 0xFFF - 3840 bytes)
 * - Barramento de Dados: 8 bits (uint8_t)
 * - Registradores de Interface de Memória:
 *     * MAR (Memory Address Register) -> Registrador de Endereço de Memória (Ponteiro)
 *     * MDR / MBR (Memory Data / Buffer Register) -> Registrador de Dados da Memória
 * - Formatos de representação integrados: Decimal, Hexadecimal, Binário (8 bits) e ASCII
 * - Dashboard visual do estado da arquitetura, uso de memória e estatísticas de acesso
 * ==============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define TAM_MEMORIA        4096                         // 4 KB de espaço de memória
#define TAM_ROM            256                          // 256 bytes para ROM (0x000 a 0x0FF)
#define INICIO_RAM         TAM_ROM                      // Início da RAM em 0x100 (256 a 4095)
#define TAM_RAM            (TAM_MEMORIA - TAM_ROM)      // 3840 bytes de RAM livre

/* Sinais do Barramento de Controle da Memória */
typedef enum {
    BUS_IDLE = 0,         // Barramento inativo
    BUS_READ,             // Leitura de memória (MEMR)
    BUS_WRITE,            // Escrita de memória (MEMW)
    BUS_PROT_FAULT        // Falha por tentativa de escrita em memória protegida (ROM)
} SinalBarramento;

/* Estrutura representativa do Subsistema de Memória e Barramentos */
typedef struct {
    uint8_t dados[TAM_MEMORIA];        // Células de armazenamento de 8 bits
    uint8_t ocupado[TAM_MEMORIA];      // Flag de ocupação/inicialização da célula
    int rom_protegida;                 // 1 = ROM protegida contra escrita (padrão), 0 = desbloqueada

    /* Registradores da Arquitetura de Von Neumann */
    int mar;                           // MAR (Memory Address Register) - ponteiro do endereço ativo
    uint8_t mbr;                       // MBR/MDR (Memory Buffer/Data Register) - dado ativo
    SinalBarramento ultimo_sinal;      // Último sinal de controle trafegado no barramento

    /* Métricas e Estatísticas de Acesso */
    uint32_t total_escritas;           // Total de escritas bem-sucedidas
    uint32_t total_leituras;           // Total de leituras realizadas
    uint32_t leituras_rom;             // Leituras ocorridas na região ROM
    uint32_t leituras_ram;             // Leituras ocorridas na região RAM
    uint32_t escritas_ram;             // Escritas efetuadas na RAM
    uint32_t escritas_rom;             // Escritas efetuadas na ROM (se desbloqueada)
    uint32_t violacoes_rom;            // Tentativas bloqueadas de escrita na ROM
} MemoriaSistema;

/* ==============================================================================
 * FUNÇÕES UTILITÁRIAS E CONVERSÕES DE BASES
 * ============================================================================== */

/* Limpa o buffer de entrada do teclado */
void limpar_buffer(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* Converte um byte de 8 bits para string binária formatada (ex: "01001111") */
void byte_para_binario(uint8_t valor, char *saida)
{
    for (int i = 7; i >= 0; i--) {
        saida[7 - i] = ((valor >> i) & 1) ? '1' : '0';
    }
    saida[8] = '\0';
}

/* Identifica se um caractere é imprimível no padrão ASCII; se não, retorna '.' */
char caractere_ascii(uint8_t valor)
{
    return (valor >= 32 && valor <= 126) ? (char)valor : '.';
}

/* Converte o sinal do barramento em texto descritivo */
const char* nome_sinal_barramento(SinalBarramento sinal)
{
    switch (sinal) {
        case BUS_READ:       return "MEMR (Leitura de Memoria)";
        case BUS_WRITE:      return "MEMW (Escrita de Memoria)";
        case BUS_PROT_FAULT: return "PROT_FAULT (Violacao de Acesso em ROM)";
        case BUS_IDLE:
        default:             return "IDLE (Barramento Ocioso)";
    }
}

/*
 * Analisador léxico flexível de números:
 * Aceita Decimal (ex: 255), Hexadecimal (ex: 0x1A ou 1Ah), e Binário (ex: 0b10101010).
 */
int interpretar_numero(const char *entrada, long *valor_saida)
{
    if (entrada == NULL) return 0;

    // Remove espaços no início
    while (isspace((unsigned char)*entrada)) entrada++;
    if (*entrada == '\0') return 0;

    // Caso 1: Notação binária "0b..." ou "0B..."
    if (entrada[0] == '0' && (entrada[1] == 'b' || entrada[1] == 'B')) {
        entrada += 2;
        if (*entrada == '\0') return 0;
        long acumulador = 0;
        while (*entrada != '\0' && !isspace((unsigned char)*entrada)) {
            if (*entrada == '0' || *entrada == '1') {
                acumulador = (acumulador << 1) | (*entrada - '0');
            } else {
                return 0; // Dígito binário inválido
            }
            entrada++;
        }
        *valor_saida = acumulador;
        return 1;
    }

    // Caso 2: Hexadecimal ou Decimal padrão via strtol
    char *fim;
    long resultado = strtol(entrada, &fim, 0);

    // Se falhou e pode ser hex sem o prefixo '0x' mas com terminação 'h'/'H'
    if (fim == entrada) {
        resultado = strtol(entrada, &fim, 16);
        if (fim == entrada) return 0;
    }

    if (*fim == 'h' || *fim == 'H') fim++;

    // Verifica se sobrou algo que não seja espaço
    while (isspace((unsigned char)*fim)) fim++;
    if (*fim != '\0') return 0;

    *valor_saida = resultado;
    return 1;
}

/* Lê uma entrada do usuário e valida contra um intervalo numérico */
int ler_inteiro_validado(const char *prompt, long min, long max, long *resultado)
{
    char buffer[64];
    printf("%s", prompt);

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return 0;
    }

    // Remove '\n' caso presente
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    long val;
    if (!interpretar_numero(buffer, &val)) {
        printf("ERRO: Formato numerico invalido! Utilize decimal (ex: 256), hex (ex: 0x100) ou binario (ex: 0b1000).\n");
        return 0;
    }

    if (val < min || val > max) {
        printf("ERRO: Valor fora da faixa permitida [%ld a %ld] (0x%lX a 0x%lX).\n", min, max, min, max);
        return 0;
    }

    *resultado = val;
    return 1;
}

/* ==============================================================================
 * GERENCIAMENTO DO SUBSISTEMA DE MEMÓRIA (ROM + RAM)
 * ============================================================================== */

/* Inicializa ou zera toda a estrutura da memória */
void inicializar_memoria(MemoriaSistema *mem)
{
    for (int i = 0; i < TAM_MEMORIA; i++) {
        mem->dados[i] = 0;
        mem->ocupado[i] = 0;
    }

    mem->rom_protegida = 1;      // Proteção de escrita em ROM ativada por padrão
    mem->mar = 0;
    mem->mbr = 0;
    mem->ultimo_sinal = BUS_IDLE;

    mem->total_escritas = 0;
    mem->total_leituras = 0;
    mem->leituras_rom = 0;
    mem->leituras_ram = 0;
    mem->escritas_ram = 0;
    mem->escritas_rom = 0;
    mem->violacoes_rom = 0;
}

/* Valida se o endereço está no espaço endereçável (0 a 4095) */
int endereco_valido(int endereco)
{
    return (endereco >= 0 && endereco < TAM_MEMORIA);
}

/* Retorna o ponteiro direto para a célula física de memória (Conceito de Ponteiro em C) */
uint8_t* obter_ponteiro_memoria(MemoriaSistema *mem, int endereco)
{
    if (!endereco_valido(endereco)) {
        return NULL;
    }
    return &(mem->dados[endereco]);
}

/* Identifica se o endereço pertence à região ROM (0 a 255) */
int eh_regiao_rom(int endereco)
{
    return (endereco >= 0 && endereco < TAM_ROM);
}

/* Carrega um programa de teste / Bootloader e dados pré-definidos na ROM e RAM */
void carregar_programa_demonstracao(MemoriaSistema *mem)
{
    // Grava rotina simulada de inicialização / BIOS na ROM
    const char *assinatura_bios = "UFMA-ARQ-2026: BIOS BOOTLOADER v2.0 - ROM INICIALIZADA COM SUCESSO!";
    size_t tam_bios = strlen(assinatura_bios);

    int protecao_anterior = mem->rom_protegida;
    mem->rom_protegida = 0; // Desprotege momentaneamente para carga de fábrica

    for (size_t i = 0; i < tam_bios && i < TAM_ROM; i++) {
        mem->dados[i] = (uint8_t)assinatura_bios[i];
        mem->ocupado[i] = 1;
    }

    // Vetor de Reset na ROM (endereço 0x00F0 a 0x00FF)
    uint8_t vetor_reset[] = { 0xEA, 0x00, 0x01, 0x00, 0xC3, 0x55, 0xAA, 0xFF };
    for (size_t i = 0; i < sizeof(vetor_reset); i++) {
        mem->dados[0xF0 + i] = vetor_reset[i];
        mem->ocupado[0xF0 + i] = 1;
    }

    mem->rom_protegida = protecao_anterior; // Restaura estado da proteção

    // Grava dados de demonstração na RAM (a partir de 0x0100)
    const char *msg_ram = "DADOS DE EXECUCAO NA RAM (REGIAO DE LEITURA E ESCRITA)";
    size_t tam_ram = strlen(msg_ram);
    for (size_t i = 0; i < tam_ram && (INICIO_RAM + i) < TAM_MEMORIA; i++) {
        mem->dados[INICIO_RAM + i] = (uint8_t)msg_ram[i];
        mem->ocupado[INICIO_RAM + i] = 1;
    }

    // Atualiza registradores de barramento
    mem->mar = INICIO_RAM;
    mem->mbr = mem->dados[INICIO_RAM];
    mem->ultimo_sinal = BUS_WRITE;

    printf("\n>>> PROGRAMA DE DEMONSTRACAO CARREGADO COM SUCESSO! <<<\n");
    printf("- ROM (0x000 - 0x0FF): Firmware BIOS e Vetores de Boot carregados.\n");
    printf("- RAM (0x100 - 0x140): Variaveis e bloco de dados inicializados.\n");
}

/* ==============================================================================
 * OPERAÇÕES FUNDAMENTAIS DE BARRAMENTO: ESCRITA E LEITURA
 * ============================================================================== */

/* Operação de Escrita de Memória (Memory Write - MEMW) */
void escrever_memoria(MemoriaSistema *mem, int endereco, uint8_t dado)
{
    if (!endereco_valido(endereco)) {
        printf("\nERRO CRITICO: Endereco 0x%03X (%d) invalido! Limite: 0x000 a 0x%03X (0 a %d).\n",
               endereco, endereco, TAM_MEMORIA - 1, TAM_MEMORIA - 1);
        return;
    }

    // Atualiza Registradores de Interface com Barramento (MAR e MDR/MBR)
    mem->mar = endereco;
    mem->mbr = dado;

    // Verificação de Proteção de Escrita da Região ROM (Memory Protection Unit)
    if (eh_regiao_rom(endereco) && mem->rom_protegida) {
        mem->ultimo_sinal = BUS_PROT_FAULT;
        mem->violacoes_rom++;

        printf("\n============================================================\n");
        printf(" [!] VIOLACAO DE PROTECAO DE MEMORIA (MEMORY PROTECTION FAULT)\n");
        printf("============================================================\n");
        printf(" Endereco requisitado: 0x%03X (%d) -> REGIAO ROM (0x000 a 0x0FF)\n", endereco, endereco);
        printf(" Causa: A memoria ROM e protegida contra escrita (Somente Leitura).\n");
        printf(" Dica: Escreva na regiao de memoria RAM (a partir de 0x%03X / %d)\n", INICIO_RAM, INICIO_RAM);
        printf("       ou altere o estado de protecao da ROM no menu de opcoes.\n");
        printf("============================================================\n");
        return;
    }

    // Se a posição já estava ocupada, notifica sobrescrita
    if (mem->ocupado[endereco]) {
        char bin_antigo[9];
        byte_para_binario(mem->dados[endereco], bin_antigo);
        printf("\n[AVISO] Endereco 0x%03X (%d) continha o valor: %3d | 0x%02X | 0b%s | '%c'\n",
               endereco, endereco, mem->dados[endereco], mem->dados[endereco],
               bin_antigo, caractere_ascii(mem->dados[endereco]));
        printf("        O dado anterior foi sobrescrito.\n");
    }

    // Ponteiro direto para a célula da memória simulando o acesso físico via barramento
    uint8_t *ptr_celula = obter_ponteiro_memoria(mem, endereco);
    *ptr_celula = dado;
    mem->ocupado[endereco] = 1;

    // Atualização de sinais e estatísticas
    mem->ultimo_sinal = BUS_WRITE;
    mem->total_escritas++;
    if (eh_regiao_rom(endereco)) {
        mem->escritas_rom++;
    } else {
        mem->escritas_ram++;
    }

    char bin_novo[9];
    byte_para_binario(dado, bin_novo);

    printf("\n============================================================\n");
    printf(" ESCRITA REALIZADA COM SUCESSO (Sinal de Barramento: MEMW)\n");
    printf("============================================================\n");
    printf(" Segmento:     %s\n", eh_regiao_rom(endereco) ? "ROM (Firmware/BIOS)" : "RAM (Dados/Variaveis)");
    printf(" MAR (Endereco): 0x%03X (Dec: %4d)\n", endereco, endereco);
    printf(" MBR (Dado):     0x%02X  (Dec: %4d | Bin: 0b%s | ASCII: '%c')\n",
           dado, dado, bin_novo, caractere_ascii(dado));
    printf("============================================================\n");
}

/* Operação de Leitura de Memória (Memory Read - MEMR) */
void ler_memoria(MemoriaSistema *mem, int endereco)
{
    if (!endereco_valido(endereco)) {
        printf("\nERRO CRITICO: Endereco 0x%03X (%d) invalido! Limite: 0x000 a 0x%03X (0 a %d).\n",
               endereco, endereco, TAM_MEMORIA - 1, TAM_MEMORIA - 1);
        return;
    }

    // Atualiza Registrador de Endereço (MAR)
    mem->mar = endereco;

    // Obtém o dado via ponteiro de memória para simulação direta do barramento de dados
    uint8_t *ptr_celula = obter_ponteiro_memoria(mem, endereco);
    uint8_t dado = *ptr_celula;
    mem->mbr = dado; // MBR recebe o dado lido do barramento

    mem->ultimo_sinal = BUS_READ;
    mem->total_leituras++;

    if (eh_regiao_rom(endereco)) {
        mem->leituras_rom++;
    } else {
        mem->leituras_ram++;
    }

    char bin[9];
    byte_para_binario(dado, bin);

    printf("\n============================================================\n");
    printf(" LEITURA REALIZADA COM SUCESSO (Sinal de Barramento: MEMR)\n");
    printf("============================================================\n");
    printf(" Segmento:       %s\n", eh_regiao_rom(endereco) ? "ROM [Somente Leitura]" : "RAM [Leitura/Escrita]");
    printf(" MAR (Endereco):   0x%03X (Dec: %4d)\n", endereco, endereco);
    printf(" Estado Célula:  %s\n", mem->ocupado[endereco] ? "OCUPADA (Dado inicializado)" : "VAZIA (Valor padrao 0x00)");
    printf(" MBR (Dado Lido):  0x%02X  (Dec: %4d | Bin: 0b%s | ASCII: '%c')\n",
           dado, dado, bin, caractere_ascii(dado));
    printf("============================================================\n");
}

/* ==============================================================================
 * DASHBOARD E MONITOR DE ARQUITETURA DE COMPUTADORES
 * ============================================================================== */

/* Exibe o Dashboard completo de arquitetura, registradores e memória */
void exibir_dashboard(const MemoriaSistema *mem)
{
    // Contabiliza total de bytes ocupados na ROM e na RAM
    int ocupados_rom = 0;
    int ocupados_ram = 0;

    for (int i = 0; i < TAM_MEMORIA; i++) {
        if (mem->ocupado[i]) {
            if (eh_regiao_rom(i)) {
                ocupados_rom++;
            } else {
                ocupados_ram++;
            }
        }
    }

    int total_ocupados = ocupados_rom + ocupados_ram;
    double percentual_total = ((double)total_ocupados / TAM_MEMORIA) * 100.0;
    double percentual_rom   = ((double)ocupados_rom / TAM_ROM) * 100.0;
    double percentual_ram   = ((double)ocupados_ram / TAM_RAM) * 100.0;

    char mbr_bin[9];
    byte_para_binario(mem->mbr, mbr_bin);

    printf("\n================================================================================\n");
    printf("                  DASHBOARD DO SISTEMA DE MEMORIA E ARQUITETURA                \n");
    printf("================================================================================\n");

    /* 1. Visão Geral da Arquitetura */
    printf("[1] ESPECIFICACOES DO SUBSISTEMA DE MEMORIA\n");
    printf("  * Capacidade Total : %d Bytes (4.0 KB / 12 bits de enderecamento: 0x000-0xFFF)\n", TAM_MEMORIA);
    printf("  * Segmento ROM     : Enderecos 0x000 a 0x0FF (0 a 255)   | Tamanho: %d Bytes [%s]\n",
           TAM_ROM, mem->rom_protegida ? "PROTEGIDA CONTRA ESCRITA" : "ESCRITA HABILITADA");
    printf("  * Segmento RAM     : Enderecos 0x100 a 0xFFF (256 a 4095) | Tamanho: %d Bytes [LEITURA/ESCRITA]\n", TAM_RAM);
    printf("--------------------------------------------------------------------------------\n");

    /* 2. Registradores de Barramento (MAR e MBR) e Controle */
    printf("[2] REGISTRADORES DE INTERFACE DE BARRAMENTO (CPU <-> MEMORIA)\n");
    printf("  * MAR (Memory Address Register) : 0x%03X (Dec: %4d) -> Segmento: %s\n",
           mem->mar, mem->mar, eh_regiao_rom(mem->mar) ? "ROM" : "RAM");
    printf("  * MBR (Memory Buffer Register)  : 0x%02X  (Dec: %4d | Bin: 0b%s | ASCII: '%c')\n",
           mem->mbr, mem->mbr, mbr_bin, caractere_ascii(mem->mbr));
    printf("  * Sinal de Linha de Controle    : %s\n", nome_sinal_barramento(mem->ultimo_sinal));
    printf("--------------------------------------------------------------------------------\n");

    /* 3. Indicadores de Ocupação com Barra Gráfica */
    printf("[3] OCUPACAO DA MEMORIA\n");

    // Barra de progresso visual (largura 30 caracteres)
    const int LARGURA_BARRA = 30;
    int preenchimento = (int)((percentual_total / 100.0) * LARGURA_BARRA);
    if (total_ocupados > 0 && preenchimento == 0) preenchimento = 1;
    if (preenchimento > LARGURA_BARRA) preenchimento = LARGURA_BARRA;

    printf("  * Total Ocupado : [");
    for (int b = 0; b < LARGURA_BARRA; b++) {
        if (b < preenchimento) printf("#");
        else printf("-");
    }
    printf("] %5.2f%% (%d / %d Bytes)\n", percentual_total, total_ocupados, TAM_MEMORIA);

    printf("  * Ocupacao ROM  : %3d / %3d Bytes (%5.1f%%) | Livres: %3d Bytes\n",
           ocupados_rom, TAM_ROM, percentual_rom, TAM_ROM - ocupados_rom);
    printf("  * Ocupacao RAM  : %4d / %4d Bytes (%5.1f%%) | Livres: %4d Bytes\n",
           ocupados_ram, TAM_RAM, percentual_ram, TAM_RAM - ocupados_ram);
    printf("--------------------------------------------------------------------------------\n");

    /* 4. Estatísticas de Operações e Tráfego */
    printf("[4] METRICAS E ESTATISTICAS DE ACESSO\n");
    printf("  * Total de Leituras (MEMR) : %u  (Na ROM: %u | Na RAM: %u)\n",
           mem->total_leituras, mem->leituras_rom, mem->leituras_ram);
    printf("  * Total de Escritas (MEMW) : %u  (Na RAM: %u | Na ROM: %u)\n",
           mem->total_escritas, mem->escritas_ram, mem->escritas_rom);
    printf("  * Violacoes de ROM Travada : %u tentativas bloqueadas\n", mem->violacoes_rom);
    printf("--------------------------------------------------------------------------------\n");

    /* 5. Mini-Dump da Vizinhança do Ponteiro Atual (MAR) */
    printf("[5] MINI HEX-DUMP AO REDOR DO REGISTRADOR MAR (* = Ponteiro Ativo)\n");
    int base_vizinhanca = (mem->mar / 16) * 16; // Alinha em bloco de 16 bytes
    int inicio_bloco = base_vizinhanca - 16;
    if (inicio_bloco < 0) inicio_bloco = 0;
    int fim_bloco = base_vizinhanca + 16;
    if (fim_bloco >= TAM_MEMORIA) fim_bloco = TAM_MEMORIA - 16;

    printf(" Endereco | 00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F | ASCII\n");
    printf(" ---------+---------------------------------------------------+-----------------\n");

    for (int lin = inicio_bloco; lin <= fim_bloco; lin += 16) {
        printf("  0x%03X   | ", lin);

        // Bytes em hexadecimal (mantendo rigoroso alinhamento de 3 caracteres por coluna)
        for (int col = 0; col < 16; col++) {
            int addr = lin + col;
            char marcador = (addr == mem->mar) ? '*' : ' ';
            printf("%c%02X", marcador, mem->dados[addr]);
            if (col == 7) printf(" ");
        }

        printf(" | ");

        // Representação ASCII
        for (int col = 0; col < 16; col++) {
            int addr = lin + col;
            printf("%c", caractere_ascii(mem->dados[addr]));
        }

        if (lin <= mem->mar && mem->mar < (lin + 16)) {
            printf(" <-- MAR (0x%03X)", mem->mar);
        }
        printf("\n");
    }

    printf("================================================================================\n");
}

/* ==============================================================================
 * VISUALIZAÇÃO EM HEX DUMP (ESTILO PROFISSIONAL)
 * ============================================================================== */

/* Exibe o mapa de memória completo ou apenas linhas que contenham dados ocupados */
void exibir_hexdump_memoria(const MemoriaSistema *mem, int apenas_ocupados)
{
    printf("\n================================================================================\n");
    printf("            MAPA DE MEMORIA COMPLETO - HEXADECIMAL, BINARIO E ASCII            \n");
    printf("            Filtro: %s\n", apenas_ocupados ? "Apenas blocos com dados gravados" : "Toda a memoria (4096 bytes)");
    printf("================================================================================\n");
    printf(" Endereco  | Bytes Hexadecimais (00 a 0F)                     | Representacao ASCII\n");
    printf(" ----------+--------------------------------------------------+------------------\n");

    int linhas_exibidas = 0;

    for (int lin = 0; lin < TAM_MEMORIA; lin += 16) {
        // Verifica se a linha tem ao menos um byte ocupado ou com dado != 0
        int tem_dados = 0;
        for (int col = 0; col < 16; col++) {
            if (mem->ocupado[lin + col] || mem->dados[lin + col] != 0) {
                tem_dados = 1;
                break;
            }
        }

        if (apenas_ocupados && !tem_dados) {
            continue; // Pula linha vazia
        }

        linhas_exibidas++;

        // Endereço base e indicador de segmento (ROM ou RAM)
        const char *tag_seg = eh_regiao_rom(lin) ? "ROM" : "RAM";
        printf(" 0x%03X [%s]| ", lin, tag_seg);

        // Exibição Hexadecimal
        for (int col = 0; col < 16; col++) {
            int addr = lin + col;
            if (mem->ocupado[addr]) {
                printf("%02X ", mem->dados[addr]);
            } else {
                printf(".. "); // Ponto duplo indica célula não inicializada
            }
            if (col == 7) printf(" ");
        }

        printf("| ");

        // Exibição ASCII
        for (int col = 0; col < 16; col++) {
            int addr = lin + col;
            if (mem->ocupado[addr]) {
                printf("%c", caractere_ascii(mem->dados[addr]));
            } else {
                printf(".");
            }
        }
        printf("\n");
    }

    if (linhas_exibidas == 0) {
        printf("  [Nenhum dado gravado nesta visualizacao. Memoria esta vazia.]\n");
    }

    printf("================================================================================\n");
    printf(" Legenda: '..' indica celula nao gravada. Letras ASCII mostram caracteres validos.\n");
}

/* ==============================================================================
 * OPERAÇÕES DE LIMPEZA E CONFIGURAÇÃO
 * ============================================================================== */

/* Limpa a memória: opção de limpar apenas RAM ou zerar todo o espaço */
void limpar_memoria(MemoriaSistema *mem, int limpar_tudo)
{
    int inicio = limpar_tudo ? 0 : INICIO_RAM;
    int cont = 0;

    for (int i = inicio; i < TAM_MEMORIA; i++) {
        if (mem->ocupado[i] || mem->dados[i] != 0) {
            cont++;
        }
        mem->dados[i] = 0;
        mem->ocupado[i] = 0;
    }

    if (limpar_tudo) {
        mem->total_escritas = 0;
        mem->total_leituras = 0;
        mem->leituras_rom = 0;
        mem->leituras_ram = 0;
        mem->escritas_ram = 0;
        mem->escritas_rom = 0;
        mem->violacoes_rom = 0;
        mem->mar = 0;
        mem->mbr = 0;
        mem->ultimo_sinal = BUS_IDLE;
        printf("\n>>> RESET TOTAL DO SISTEMA REALIZADO: ROM e RAM completamente zeradas! <<<\n");
    } else {
        mem->escritas_ram = 0;
        mem->leituras_ram = 0;
        printf("\n>>> MEMORIA RAM LIMPA COM SUCESSO! (%d bytes redefinidos). A regiao ROM foi preservada. <<<\n", cont);
    }
}

/* Alterna a chave de proteção da região ROM */
void alternar_protecao_rom(MemoriaSistema *mem)
{
    mem->rom_protegida = !mem->rom_protegida;
    printf("\n>>> STATUS DE PROTECAO DA ROM ALTERADO <<<\n");
    printf("Novo estado: %s\n", mem->rom_protegida ?
           "PROTEGIDA CONTRA ESCRITA (Padrao de Fabrica / Read-Only)" :
           "DESBLOQUEADA (Permite escrita na ROM para testes de gravacao)");
}

/* ==============================================================================
 * INTERFACE DE MENU E INTERAÇÃO COM O USUÁRIO
 * ============================================================================== */

/* Exibe o menu principal */
void exibir_menu(const MemoriaSistema *mem)
{
    printf("\n");
    printf("============================================================\n");
    printf("      SIMULADOR DE MEMORIA (ROM + RAM) COM DASHBOARD        \n");
    printf("              UFMA - Arquitetura de Computadores            \n");
    printf("============================================================\n");
    printf(" [1] Escrever na Memoria (MEMW) [Decimal, Hex ou Binario]   \n");
    printf(" [2] Ler da Memoria (MEMR)     [Visualiza Dec/Hex/Bin/ASCII]\n");
    printf(" [3] Abrir DASHBOARD Geral e Registradores (MAR, MBR, Uso)  \n");
    printf(" [4] Exibir Hex Dump / Mapa da Memoria                      \n");
    printf(" [5] Carregar Programa Demo na ROM & RAM (Bootloader)       \n");
    printf(" [6] Alternar Protecao de Escrita da ROM [%s]      \n",
           mem->rom_protegida ? "PROTEGIDA" : "LIBERADA");
    printf(" [7] Limpar / Zerar Memoria (Apenas RAM ou Total)           \n");
    printf(" [0] Sair do Simulador                                      \n");
    printf("============================================================\n");
    printf(" Escolha uma opcao: ");
}

/* ==============================================================================
 * FUNÇÃO PRINCIPAL (MAIN)
 * ============================================================================== */

int main(void)
{
    MemoriaSistema memoria;
    inicializar_memoria(&memoria);

    printf("\n");
    printf("********************************************************************************\n");
    printf("*            UNIVERSIDADE FEDERAL DO MARANHÃO - CCET - DEINF                   *\n");
    printf("*       SIMULADOR AVANÇADO DE MEMÓRIA RAM / ROM - ARQUITETURA DE COMPUTADORES  *\n");
    printf("*   Equipe: Caio Ferreira, Danilo Nava, Raynara Alves, Rhuã Yuri, Yuri Aguiar  *\n");
    printf("********************************************************************************\n");
    printf(" Sistema inicializado com 4096 Bytes (4 KB).\n");
    printf(" - ROM : 0x000 a 0x0FF (256 Bytes) | RAM : 0x100 a 0xFFF (3840 Bytes)\n");

    int opcao = -1;

    do {
        exibir_menu(&memoria);

        char linha_opcao[64];
        if (fgets(linha_opcao, sizeof(linha_opcao), stdin) == NULL) {
            break;
        }

        long val_op;
        if (!interpretar_numero(linha_opcao, &val_op)) {
            printf("\nOpcao invalida. Digite um numero de 0 a 7.\n");
            continue;
        }

        opcao = (int)val_op;

        switch (opcao) {
            case 1: {
                printf("\n--- [1] OPERACAO DE ESCRITA (MEMW) ---\n");
                printf("Dica: Aceita Decimal (ex: 256), Hex (ex: 0x100) ou Binario (ex: 0b100000000)\n");

                long endereco;
                if (!ler_inteiro_validado("Digite o Endereco (0 a 4095 / 0x000 a 0xFFF): ",
                                          0, TAM_MEMORIA - 1, &endereco)) {
                    break;
                }

                long dado;
                if (!ler_inteiro_validado("Digite o Dado a ser gravado (0 a 255 / 0x00 a 0xFF): ",
                                          0, 255, &dado)) {
                    break;
                }

                escrever_memoria(&memoria, (int)endereco, (uint8_t)dado);
                break;
            }

            case 2: {
                printf("\n--- [2] OPERACAO DE LEITURA (MEMR) ---\n");
                printf("Dica: Aceita Decimal (ex: 256), Hex (ex: 0x100) ou Binario (ex: 0b100000000)\n");

                long endereco;
                if (!ler_inteiro_validado("Digite o Endereco para leitura (0 a 4095 / 0x000 a 0xFFF): ",
                                          0, TAM_MEMORIA - 1, &endereco)) {
                    break;
                }

                ler_memoria(&memoria, (int)endereco);
                break;
            }

            case 3: {
                exibir_dashboard(&memoria);
                break;
            }

            case 4: {
                printf("\n--- [4] MAPA DE MEMORIA / HEX DUMP ---\n");
                printf("1 - Exibir apenas blocos com dados gravados (Recomendado)\n");
                printf("2 - Exibir toda a memoria (4096 posicoes completas)\n");
                printf("Escolha o modo: ");

                char resp[64];
                if (fgets(resp, sizeof(resp), stdin) != NULL) {
                    long modo;
                    if (interpretar_numero(resp, &modo)) {
                        if (modo == 1) {
                            exibir_hexdump_memoria(&memoria, 1);
                        } else if (modo == 2) {
                            exibir_hexdump_memoria(&memoria, 0);
                        } else {
                            printf("Opcao invalida. Exibindo modo compacto.\n");
                            exibir_hexdump_memoria(&memoria, 1);
                        }
                    } else {
                        exibir_hexdump_memoria(&memoria, 1);
                    }
                }
                break;
            }

            case 5: {
                carregar_programa_demonstracao(&memoria);
                break;
            }

            case 6: {
                alternar_protecao_rom(&memoria);
                break;
            }

            case 7: {
                printf("\n--- [7] LIMPEZA DE MEMORIA ---\n");
                printf("1 - Limpar APENAS a regiao de RAM (Preserva a ROM e dados de Boot)\n");
                printf("2 - Reset TOTAL do Sistema (Zera ROM, RAM, Registradores e Metricas)\n");
                printf("0 - Cancelar\n");
                printf("Escolha uma opcao: ");

                char resp[64];
                if (fgets(resp, sizeof(resp), stdin) != NULL) {
                    long modo_limpeza;
                    if (interpretar_numero(resp, &modo_limpeza)) {
                        if (modo_limpeza == 1) {
                            limpar_memoria(&memoria, 0);
                        } else if (modo_limpeza == 2) {
                            limpar_memoria(&memoria, 1);
                        } else if (modo_limpeza == 0) {
                            printf("Operacao cancelada.\n");
                        } else {
                            printf("Opcao invalida.\n");
                        }
                    }
                }
                break;
            }

            case 0: {
                printf("\n============================================================\n");
                printf("  Encerrando o Simulador de Memoria RAM/ROM. Ate breve!     \n");
                printf("  Universidade Federal do Maranhao - 2026                   \n");
                printf("============================================================\n");
                break;
            }

            default: {
                printf("\nOpcao inexistente! Escolha entre 0 e 7.\n");
                break;
            }
        }

    } while (opcao != 0);

    return 0;
}
