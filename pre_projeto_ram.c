/*
 * ==============================================================================
 * UNIVERSIDADE FEDERAL DO MARANHÃO - UFMA
 * CENTRO DE CIÊNCIAS EXATAS E TECNOLOGIA - CCET
 * DEPARTAMENTO DE INFORMÁTICA
 * DISCIPLINA: ARQUITETURA DE COMPUTADORES
 *
 * SIMULADOR DE MEMÓRIA RAM / ROM (4 KB)
 * Autores: Caio Ferreira Sousa, Danilo Nava Soares Lima, Raynara Ribeiro Alves,
 *          Rhuã Yuri Nascimento Sardinha, Yuri Aguiar Gonçalves
 * Ano: 2026
 *
 * Especificações da Arquitetura de Memória:
 * - Espaço Total de Endereçamento: 4096 bytes (4 KB - endereços de 0 a 4095 / 0x000 a 0xFFF)
 * - Região ROM (Read-Only Memory / BIOS): Endereços 0 a 255 (0x000 a 0x0FF - 256 bytes)
 *   * Protegida contra escrita indevida (Memory Protection)
 * - Região RAM (Random Access Memory): Endereços 256 a 4095 (0x100 a 0xFFF - 3840 bytes)
 *   * Livre para operações de leitura e escrita
 * - Barramento de Dados: 8 bits (uint8_t)
 * - Suporte a representação em Decimal, Hexadecimal, Binário (8 bits) e ASCII
 * ==============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define TAM_MEMORIA        4096                         // 4 KB de espaço total de memória
#define TAM_ROM            256                          // 256 bytes reservados para ROM (0x000 a 0x0FF)
#define INICIO_RAM         TAM_ROM                      // RAM inicia em 0x100 (256 a 4095)
#define TAM_RAM            (TAM_MEMORIA - TAM_ROM)      // 3840 bytes de RAM livre

/* Estrutura representativa do Subsistema de Memória (ROM e RAM) */
typedef struct {
    uint8_t dados[TAM_MEMORIA];        // Vetor de células de 8 bits
    uint8_t ocupado[TAM_MEMORIA];      // Flag de controle de ocupação da célula
    int rom_protegida;                 // 1 = Protegida contra escrita (ROM padrão)
    int total_escritas;                // Contador de operações de escrita
    int total_leituras;                // Contador de operações de leitura
    int violacoes_rom;                 // Tentativas bloqueadas de escrita na ROM
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

/* Analisador flexível de números: aceita Decimal, Hexadecimal (0x...) e Binário (0b...) */
int interpretar_numero(const char *entrada, long *valor_saida)
{
    if (entrada == NULL) return 0;

    while (isspace((unsigned char)*entrada)) entrada++;
    if (*entrada == '\0') return 0;

    // Caso 1: Notação binária (0b...)
    if (entrada[0] == '0' && (entrada[1] == 'b' || entrada[1] == 'B')) {
        entrada += 2;
        if (*entrada == '\0') return 0;
        long acumulador = 0;
        while (*entrada != '\0' && !isspace((unsigned char)*entrada)) {
            if (*entrada == '0' || *entrada == '1') {
                acumulador = (acumulador << 1) | (*entrada - '0');
            } else {
                return 0;
            }
            entrada++;
        }
        *valor_saida = acumulador;
        return 1;
    }

    // Caso 2: Hexadecimal ou Decimal padrão via strtol
    char *fim;
    long resultado = strtol(entrada, &fim, 0);

    if (fim == entrada) {
        resultado = strtol(entrada, &fim, 16);
        if (fim == entrada) return 0;
    }

    if (*fim == 'h' || *fim == 'H') fim++;

    while (isspace((unsigned char)*fim)) fim++;
    if (*fim != '\0') return 0;

    *valor_saida = resultado;
    return 1;
}

/* Lê uma entrada do usuário e valida contra os limites permitidos */
int ler_inteiro_validado(const char *prompt, long min, long max, long *resultado)
{
    char buffer[64];
    printf("%s", prompt);

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return 0;
    }

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    long val;
    if (!interpretar_numero(buffer, &val)) {
        printf("ERRO: Formato invalido! Aceita decimal (ex: 256), hex (ex: 0x100) ou binario (ex: 0b1000).\n");
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

/* Inicializa a memória */
void inicializar_memoria(MemoriaSistema *mem)
{
    for (int i = 0; i < TAM_MEMORIA; i++) {
        mem->dados[i] = 0;
        mem->ocupado[i] = 0;
    }

    mem->rom_protegida = 1;      // Proteção de escrita ativada por padrão
    mem->total_escritas = 0;
    mem->total_leituras = 0;
    mem->violacoes_rom = 0;
}

/* Valida se o endereço está no intervalo permitido (0 a 4095) */
int endereco_valido(int endereco)
{
    return (endereco >= 0 && endereco < TAM_MEMORIA);
}

/* Retorna o ponteiro direto para a célula física de memória */
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

/* ==============================================================================
 * OPERAÇÕES FUNDAMENTAIS: ESCRITA E LEITURA
 * ============================================================================== */

/* Operação de escrita na memória */
void escrever_memoria(MemoriaSistema *mem, int endereco, uint8_t dado)
{
    if (!endereco_valido(endereco)) {
        printf("\nERRO: Endereco invalido. Utilize um valor entre 0 e %d (0x000 a 0x%03X).\n",
               TAM_MEMORIA - 1, TAM_MEMORIA - 1);
        return;
    }

    // Verificação de proteção da ROM
    if (eh_regiao_rom(endereco) && mem->rom_protegida) {
        mem->violacoes_rom++;
        printf("\n============================================================\n");
        printf(" [!] ERRO: VIOLACAO DE ESCRITA NA REGIAO ROM\n");
        printf("============================================================\n");
        printf(" O endereco 0x%03X (%d) pertence a memoria ROM (0 a 255).\n", endereco, endereco);
        printf(" Esta regiao e somente leitura (Read-Only Memory).\n");
        printf(" Escreva na regiao de memoria RAM (a partir de 0x%03X / %d).\n", INICIO_RAM, INICIO_RAM);
        printf("============================================================\n");
        return;
    }

    // Alerta de sobrescrita se a posição já estiver ocupada
    if (mem->ocupado[endereco]) {
        char bin_antigo[9];
        byte_para_binario(mem->dados[endereco], bin_antigo);
        printf("\n[AVISO] Endereco 0x%03X (%d) ja continha: Dec: %d | Hex: 0x%02X | Bin: 0b%s\n",
               endereco, endereco, mem->dados[endereco], mem->dados[endereco], bin_antigo);
        printf("        O valor anterior sera sobrescrito.\n");
    }

    // Escrita via ponteiro de memória
    uint8_t *ptr = obter_ponteiro_memoria(mem, endereco);
    *ptr = dado;
    mem->ocupado[endereco] = 1;
    mem->total_escritas++;

    char bin_novo[9];
    byte_para_binario(dado, bin_novo);

    printf("\n============================================================\n");
    printf(" ESCRITA REALIZADA COM SUCESSO\n");
    printf("============================================================\n");
    printf(" Segmento:       %s\n", eh_regiao_rom(endereco) ? "ROM" : "RAM");
    printf(" Endereco:       0x%03X (Decimal: %d)\n", endereco, endereco);
    printf(" Dado Gravado:   0x%02X  (Decimal: %d | Binario: 0b%s | ASCII: '%c')\n",
           dado, dado, bin_novo, caractere_ascii(dado));
    printf("============================================================\n");
}

/* Operação de leitura da memória */
void ler_memoria(MemoriaSistema *mem, int endereco)
{
    if (!endereco_valido(endereco)) {
        printf("\nERRO: Endereco invalido. Utilize um valor entre 0 e %d (0x000 a 0x%03X).\n",
               TAM_MEMORIA - 1, TAM_MEMORIA - 1);
        return;
    }

    uint8_t *ptr = obter_ponteiro_memoria(mem, endereco);
    uint8_t dado = *ptr;
    mem->total_leituras++;

    char bin[9];
    byte_para_binario(dado, bin);

    printf("\n============================================================\n");
    printf(" LEITURA REALIZADA COM SUCESSO\n");
    printf("============================================================\n");
    printf(" Segmento:       %s\n", eh_regiao_rom(endereco) ? "ROM [Somente Leitura]" : "RAM [Leitura/Escrita]");
    printf(" Endereco:       0x%03X (Decimal: %d)\n", endereco, endereco);
    printf(" Estado:         %s\n", mem->ocupado[endereco] ? "OCUPADO" : "VAZIO (Valor padrao 0x00)");
    printf(" Dado Lido:      0x%02X  (Decimal: %d | Binario: 0b%s | ASCII: '%c')\n",
           dado, dado, bin, caractere_ascii(dado));
    printf("============================================================\n");
}

/* Exibe a lista dos endereços de memória que foram utilizados */
void exibir_memoria(const MemoriaSistema *mem)
{
    printf("\n================================================================================\n");
    printf("                 ESTADO DA MEMORIA (ENDERECOS UTILIZADOS)                       \n");
    printf("================================================================================\n");
    printf(" Segmento | Endereco (Hex/Dec) | Dado (Hex) | Dado (Dec) | Binario (8b) | ASCII \n");
    printf(" ---------+--------------------+------------+------------+--------------+-------\n");

    int vazia = 1;
    char bin[9];

    for (int i = 0; i < TAM_MEMORIA; i++) {
        if (mem->ocupado[i]) {
            byte_para_binario(mem->dados[i], bin);
            printf("   %-4s   | 0x%03X (%4d)      |    0x%02X    |    %3d     |   %8s   |   %c   \n",
                   eh_regiao_rom(i) ? "ROM" : "RAM",
                   i, i,
                   mem->dados[i],
                   mem->dados[i],
                   bin,
                   caractere_ascii(mem->dados[i]));
            vazia = 0;
        }
    }

    if (vazia) {
        printf("  [Nenhum dado gravado. Toda a memoria esta vazia.]\n");
    }

    printf("================================================================================\n");
    printf(" Estatisticas: Total de Escritas: %d | Total de Leituras: %d | Bloqueios em ROM: %d\n",
           mem->total_escritas, mem->total_leituras, mem->violacoes_rom);
    printf("================================================================================\n");
}

/* Limpa a memória RAM ou realiza o reset completo */
void limpar_memoria(MemoriaSistema *mem)
{
    for (int i = 0; i < TAM_MEMORIA; i++) {
        mem->dados[i] = 0;
        mem->ocupado[i] = 0;
    }

    mem->total_escritas = 0;
    mem->total_leituras = 0;
    mem->violacoes_rom = 0;

    printf("\nMemoria completamente limpa e zerada com sucesso.\n");
}

/* ==============================================================================
 * MENU E INTERAÇÃO COM O USUÁRIO
 * ============================================================================== */

void exibir_menu(void)
{
    printf("\n");
    printf("============================================================\n");
    printf("         SIMULADOR DE MEMORIA (ROM + RAM - 4 KB)            \n");
    printf("               UFMA - Arquitetura de Computadores           \n");
    printf("============================================================\n");
    printf(" 1 - Escrever na memoria (ROM/RAM)                          \n");
    printf(" 2 - Ler da memoria (ROM/RAM)                               \n");
    printf(" 3 - Exibir estado da memoria                               \n");
    printf(" 4 - Limpar memoria                                         \n");
    printf(" 0 - Sair                                                   \n");
    printf("============================================================\n");
    printf(" Escolha uma opcao: ");
}

int main(void)
{
    MemoriaSistema memoria;
    inicializar_memoria(&memoria);

    printf("\n============================================================\n");
    printf("   SIMULADOR DE MEMORIA (ROM + RAM)\n");
    printf("   Arquitetura de Computadores - UFMA 2026\n");
    printf("   Capacidade: 4096 Enderecos (4 KB)\n");
    printf("   ROM: 0 a 255 (0x000 a 0x0FF) | RAM: 256 a 4095 (0x100 a 0xFFF)\n");
    printf("============================================================\n");

    int opcao = -1;

    do {
        exibir_menu();

        char linha[64];
        if (fgets(linha, sizeof(linha), stdin) == NULL) {
            break;
        }

        long val_op;
        if (!interpretar_numero(linha, &val_op)) {
            printf("\nOpcao invalida. Digite uma opcao de 0 a 4.\n");
            continue;
        }

        opcao = (int)val_op;

        switch (opcao) {
            case 1: {
                printf("\n--- ESCRITA NA MEMORIA ---\n");
                printf("Formatos aceitos: Decimal (ex: 256), Hex (ex: 0x100) ou Binario (ex: 0b1000)\n");

                long endereco;
                if (!ler_inteiro_validado("Digite o endereco (0 a 4095 / 0x000 a 0xFFF): ",
                                          0, TAM_MEMORIA - 1, &endereco)) {
                    break;
                }

                long dado;
                if (!ler_inteiro_validado("Digite o dado (0 a 255 / 0x00 a 0xFF): ",
                                          0, 255, &dado)) {
                    break;
                }

                escrever_memoria(&memoria, (int)endereco, (uint8_t)dado);
                break;
            }

            case 2: {
                printf("\n--- LEITURA DA MEMORIA ---\n");

                long endereco;
                if (!ler_inteiro_validado("Digite o endereco para leitura (0 a 4095 / 0x000 a 0xFFF): ",
                                          0, TAM_MEMORIA - 1, &endereco)) {
                    break;
                }

                ler_memoria(&memoria, (int)endereco);
                break;
            }

            case 3: {
                exibir_memoria(&memoria);
                break;
            }

            case 4: {
                limpar_memoria(&memoria);
                break;
            }

            case 0: {
                printf("\nEncerrando o simulador de memoria...\n");
                break;
            }

            default: {
                printf("\nOpcao invalida. Escolha entre 0 e 4.\n");
                break;
            }
        }

    } while (opcao != 0);

    return 0;
}
