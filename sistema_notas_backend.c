/*
 * ==============================================================================
 * UNIVERSIDADE FEDERAL DO MARANHÃO - UFMA | CCET - DEPARTAMENTO DE INFORMÁTICA
 * DISCIPLINA: ARQUITETURA DE COMPUTADORES
 * SISTEMA DE NOTAS ACADÊMICO COM MEMÓRIA RAM/ROM SIMULADA (4 KB)
 * BACK-END EM C PURO COM SERVIDOR HTTP REST EMBUTIDO
 *
 * Arquitetura de Memória:
 * - 0x000 - 0x0FF (256 B):  ROM (Firmware de Inicialização / Regras UFMA)
 * - 0x100 - 0x3FF (768 B):  TEXT (Código executável e rotinas de cálculo)
 * - 0x400 - 0x7FF (1024 B): DATA (Variáveis globais, cabeçalhos e constantes)
 * - 0x800 - 0xBFF (1024 B): HEAP (Alocação dinâmica de alunos - Bottom-Up)
 * - 0xC00 - 0xFFF (1024 B): STACK (Pilha de execução das funções - Top-Down)
 * ==============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>

#define TAM_MEMORIA        4096
#define TAM_ROM            256
#define INICIO_TEXT        0x100
#define FIM_TEXT           0x3FF
#define INICIO_DATA        0x400
#define FIM_DATA           0x7FF
#define INICIO_HEAP        0x800
#define FIM_HEAP           0xBFF
#define INICIO_STACK       0xC00
#define TOPO_STACK         0xFFF

#define TAM_ALUNO          24
#define PORTA_PADRAO       8080

typedef enum {
    BUS_IDLE = 0,
    BUS_READ,
    BUS_WRITE,
    BUS_PROT_FAULT
} SinalBarramento;

typedef struct {
    uint8_t  dados[TAM_MEMORIA];
    uint8_t  ocupado[TAM_MEMORIA];
    int      rom_protegida;
    uint16_t mar;
    uint8_t  mbr;
    SinalBarramento ultimo_sinal;

    uint16_t heap_pointer;
    uint16_t stack_pointer;

    // Estatísticas de Barramento
    unsigned long total_leituras;
    unsigned long total_escritas;
    unsigned long violacoes_rom;
    char ultimo_log[256];
} MemoriaSistema;

static MemoriaSistema g_mem;
static pthread_mutex_t g_mem_lock = PTHREAD_MUTEX_INITIALIZER;

/* ==============================================================================
 * SUBSISTEMA FÍSICO DE BARRAMENTO E MEMÓRIA
 * ============================================================================== */

const char* sinal_para_string(SinalBarramento s) {
    switch (s) {
        case BUS_READ: return "MEMR (Leitura)";
        case BUS_WRITE: return "MEMW (Escrita)";
        case BUS_PROT_FAULT: return "PROT_FAULT (Violação MPU)";
        case BUS_IDLE: default: return "BUS_IDLE (Inativo)";
    }
}

void ciclo_memw(uint16_t endereco, uint8_t dado, const char* motivo) {
    g_mem.mar = endereco & 0x0FFF;
    g_mem.mbr = dado;

    // Proteção de Hardware da ROM via MPU
    if (g_mem.mar < TAM_ROM && g_mem.rom_protegida) {
        g_mem.ultimo_sinal = BUS_PROT_FAULT;
        g_mem.violacoes_rom++;
        snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log),
                 "BLOQUEIO MPU: Tentativa de sobrescrever ROM [0x%03X] com 0x%02X! (%s)",
                 g_mem.mar, g_mem.mbr, motivo ? motivo : "Acesso Direto");
        return;
    }

    g_mem.dados[g_mem.mar] = g_mem.mbr;
    g_mem.ocupado[g_mem.mar] = 1;
    g_mem.ultimo_sinal = BUS_WRITE;
    g_mem.total_escritas++;
    if (motivo) {
        snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log),
                 "MEMW: Endereco 0x%03X <- 0x%02X (%d) | %s",
                 g_mem.mar, g_mem.mbr, g_mem.mbr, motivo);
    }
}

uint8_t ciclo_memr(uint16_t endereco, const char* motivo) {
    g_mem.mar = endereco & 0x0FFF;
    g_mem.mbr = g_mem.dados[g_mem.mar];
    g_mem.ultimo_sinal = BUS_READ;
    g_mem.total_leituras++;
    if (motivo) {
        snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log),
                 "MEMR: Endereco 0x%03X -> MBR=0x%02X (%d) | %s",
                 g_mem.mar, g_mem.mbr, g_mem.mbr, motivo);
    }
    return g_mem.mbr;
}

/* Inicializa ou Restaura a Memória com Exemplos Vivos em TODAS as 5 Regiões */
void inicializar_sistema_memoria() {
    memset(g_mem.dados, 0, sizeof(g_mem.dados));
    memset(g_mem.ocupado, 0, sizeof(g_mem.ocupado));
    g_mem.rom_protegida = 0; // Desbloqueia temporariamente para gravação de fábrica
    g_mem.mar = 0;
    g_mem.mbr = 0;
    g_mem.ultimo_sinal = BUS_IDLE;
    g_mem.total_leituras = 0;
    g_mem.total_escritas = 0;
    g_mem.violacoes_rom = 0;
    g_mem.heap_pointer = INICIO_HEAP;
    g_mem.stack_pointer = TOPO_STACK;

    // =========================================================================
    // 1. REGIÃO ROM (0x000 a 0x0FF - 256 Bytes): BIOS & REGIMENTO DA UFMA
    // =========================================================================
    // 1.1 Assinatura de Boot do Firmware
    const char* rom_sig = "UFMA-CCET-DEINF: BIOS BOOTLOADER v2.0 - SISTEMA NOTAS (ARQ-2026)";
    for (size_t i = 0; i < strlen(rom_sig) && i < 64; i++) {
        ciclo_memw((uint16_t)i, (uint8_t)rom_sig[i], "ROM: Assinatura BIOS");
    }

    // 1.2 Vetores de Interrupção e Reset Vector (0x0040 a 0x005F)
    uint8_t reset_vec[] = { 0xEA, 0x00, 0x01, 0x00, 0xC3, 0x55, 0xAA, 0xFF,
                            0x90, 0x90, 0xEB, 0x04, 0xFA, 0xF4, 0x00, 0x00 };
    for (size_t i = 0; i < sizeof(reset_vec); i++) {
        ciclo_memw(0x0040 + i, reset_vec[i], "ROM: Vetor de Reset e Interrupcao");
    }

    // 1.3 Parâmetros do Regimento Acadêmico da UFMA (0x0070 a 0x007F)
    ciclo_memw(0x0070, 70,  "ROM: Media Minima Aprovacao (7.0)");
    ciclo_memw(0x0071, 75,  "ROM: Frequencia Minima Obrigatoria (75%)");
    ciclo_memw(0x0072, 50,  "ROM: Peso Avaliacao P1 (50%)");
    ciclo_memw(0x0073, 50,  "ROM: Peso Avaliacao P2 (50%)");
    ciclo_memw(0x0074, 100, "ROM: Nota Maxima Permitida (10.0)");
    ciclo_memw(0x0075, 40,  "ROM: Nota Minima para Prova Final (4.0)");
    ciclo_memw(0x0076, 60,  "ROM: Carga Horaria da Disciplina (60 Horas)");
    ciclo_memw(0x0077, 15,  "ROM: Limite Maximo de Faltas (15 Faltas)");

    // 1.4 Resolução do CONSEPE e Tabela de Códigos da MPU (0x0080 a 0x00FF)
    const char* rom_consepe = "RESOLUCAO CONSEPE UFMA N. 1826: REGRAS E CRITERIOS OFICIAIS DE AVALIACAO";
    for (size_t i = 0; i < strlen(rom_consepe) && i < 64; i++) {
        ciclo_memw(0x0080 + i, (uint8_t)rom_consepe[i], "ROM: Texto Normativo");
    }

    const char* rom_mpu_msg = "CHIP MPU ATIVA: BLOQUEIO GRAVACAO 0x000-0x0FF (MEMORIA SOMENTE LEITURA)";
    for (size_t i = 0; i < strlen(rom_mpu_msg) && i < 64; i++) {
        ciclo_memw(0x00C0 + i, (uint8_t)rom_mpu_msg[i], "ROM: Mensagem da MPU");
    }

    // Trava de Hardware: Ativa a MPU da ROM
    g_mem.rom_protegida = 1;

    // =========================================================================
    // 2. REGIÃO TEXT (0x100 a 0x3FF - 768 Bytes): INSTRUÇÕES COMPILADAS (OPCODES)
    // =========================================================================
    // Rotina 1: calcular_media (0x100 - PUSH EBP, MOV, ADD, SHR, POP, RET)
    uint8_t op_calc_media[] = {
        0x55, 0x89, 0xE5,             // push ebp; mov ebp, esp
        0x8B, 0x45, 0x08,             // mov eax, [ebp+8]   (P1)
        0x03, 0x45, 0x0C,             // add eax, [ebp+12]  (P2)
        0xD1, 0xE8,                   // shr eax, 1         (divide por 2)
        0x5D, 0xC3                    // pop ebp; ret
    };
    for (size_t i = 0; i < sizeof(op_calc_media); i++) {
        ciclo_memw(0x0100 + i, op_calc_media[i], "TEXT: Opcode calcular_media");
    }

    // Rotina 2: verificar_aprovacao (0x130 - CMP, JGE, MOV status, RET)
    uint8_t op_verif_aprov[] = {
        0x55, 0x89, 0xE5,             // push ebp; mov ebp, esp
        0x8A, 0x45, 0x08,             // mov al, [ebp+8]    (media)
        0x3C, 0x46,                   // cmp al, 70         (compara com 7.0)
        0x7C, 0x06,                   // jl reprovado
        0xB0, 0x41, 0xEB, 0x04,       // mov al, 'A'; jmp fim
        0xB0, 0x52,                   // mov al, 'R'        (reprovado)
        0x5D, 0xC3                    // pop ebp; ret
    };
    for (size_t i = 0; i < sizeof(op_verif_aprov); i++) {
        ciclo_memw(0x0130 + i, op_verif_aprov[i], "TEXT: Opcode verificar_aprovacao");
    }

    // Rotina 3: lancar_nota_barramento (0x160 - Movimentação via MAR/MBR)
    uint8_t op_lancar_nota[] = {
        0x55, 0x89, 0xE5,             // push ebp; mov ebp, esp
        0x66, 0x8B, 0x55, 0x08,       // mov dx, [ebp+8]    (Endereco MAR)
        0x8A, 0x45, 0x0C,             // mov al, [ebp+12]   (Dado MBR)
        0x88, 0x02,                   // mov [edx], al      (Ciclo MEMW)
        0x5D, 0xC3                    // pop ebp; ret
    };
    for (size_t i = 0; i < sizeof(op_lancar_nota); i++) {
        ciclo_memw(0x0160 + i, op_lancar_nota[i], "TEXT: Opcode lancar_nota");
    }

    // Rotina 4: trancar_matricula_heap (0x190 - Zeramento de 24 bytes)
    uint8_t op_trancar[] = {
        0x55, 0x89, 0xE5,             // push ebp; mov ebp, esp
        0x66, 0x8B, 0x7D, 0x08,       // mov di, [ebp+8]    (Base do Aluno)
        0xB9, 0x18, 0x00,             // mov cx, 24         (Tamanho struct)
        0x30, 0xC0,                   // xor al, al
        0xF3, 0xAA,                   // rep stosb          (Zera 24 bytes no HEAP)
        0x5D, 0xC3                    // pop ebp; ret
    };
    for (size_t i = 0; i < sizeof(op_trancar); i++) {
        ciclo_memw(0x0190 + i, op_trancar[i], "TEXT: Opcode trancar_matricula");
    }

    // Rotina 5: rotina_post_bios (0x1C0 - Inicialização de hardware)
    uint8_t op_post[] = {
        0xFA, 0x31, 0xC0, 0x8E, 0xD8, 0x8E, 0xC0, 0xBC, 0xFF, 0x0F, 0xFB, 0xC3
    };
    for (size_t i = 0; i < sizeof(op_post); i++) {
        ciclo_memw(0x01C0 + i, op_post[i], "TEXT: Opcode rotina_post_bios");
    }

    // =========================================================================
    // 3. REGIÃO DATA (0x400 a 0x7FF - 1024 Bytes): STRINGS E CONSTANTES GLOBAIS
    // =========================================================================
    const char* data_strings[] = {
        "DISCIPLINA: ARQUITETURA DE COMPUTADORES (CODIGO: INF02026)",
        "DOCENTE RESPONSAVEL: DEPARTAMENTO DE INFORMATICA - CCET",
        "SEMESTRE LETIVO: 2026.1 | LOCAL: LABORATORIO DE HARDWARE 02",
        "TABELA STATUS: [APROVADO, REPROVADO, PROVA FINAL, CURSANDO]",
        "CABECALHO: ID | NOME DO ALUNO | P1 | P2 | MEDIA | FREQ | SIT",
        "SISTEMA: DIARIO ELETRONICO INTEGRADO A MEMORIA FISICA 4KB",
        "MENSAGEM BOOT: SISTEMA OPERACIONAL SIMULADO CARREGADO COM EXITO"
    };
    uint16_t data_offsets[] = { 0x0400, 0x0440, 0x0480, 0x04C0, 0x0500, 0x0540, 0x0580 };

    for (int s = 0; s < 7; s++) {
        const char* str = data_strings[s];
        uint16_t base = data_offsets[s];
        for (size_t i = 0; i < strlen(str); i++) {
            ciclo_memw(base + i, (uint8_t)str[i], "DATA: Constante String");
        }
        ciclo_memw(base + strlen(str), 0x00, "DATA: Terminador Nulo \\0");
    }

    // =========================================================================
    // 4. REGIÃO STACK (0xC00 a 0xFFF - 1024 Bytes): PILHA DE EXECUÇÃO ATIVA
    // =========================================================================
    // Criamos um Quadro de Pilha (Stack Frame) realista ativo a partir de 0xFFF:
    ciclo_memw(0x0FFF, 0x00, "STACK: Retorno para OS/BIOS (End. 0x00)");
    ciclo_memw(0x0FFE, 0x01, "STACK: Argumento argc (1)");
    ciclo_memw(0x0FFD, 0x20, "STACK: Ponteiro argv");
    ciclo_memw(0x0FFC, 0xF0, "STACK: EBP Salvo da funcao main");
    ciclo_memw(0x0FFB, 0x01, "STACK: Retorno calcular_medias (High Byte 0x01)");
    ciclo_memw(0x0FFA, 0x30, "STACK: Retorno calcular_medias (Low Byte 0x30)");
    ciclo_memw(0x0FF9, 0xE0, "STACK: EBP Salvo do quadro anterior");
    ciclo_memw(0x0FF8, 0xAF, "STACK: Variavel local 'soma_temporaria' (175)");
    ciclo_memw(0x0FF7, 85,   "STACK: Parametro empilhado: P1 = 8.5 (85)");
    ciclo_memw(0x0FF6, 90,   "STACK: Parametro empilhado: P2 = 9.0 (90)");
    ciclo_memw(0x0FF5, 95,   "STACK: Parametro empilhado: Frequencia = 95%");
    ciclo_memw(0x0FF4, 'A',  "STACK: Retorno da comparacao: 'A' (Aprovado)");

    // O Stack Pointer (SP) aponta para o topo da pilha ativa
    g_mem.stack_pointer = 0x0FF4;

    snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log), "Sistema inicializado com dados realistas em todas as 5 regioes.");
}

/* ==============================================================================
 * ROTINAS DE ALOCAÇÃO ACADÊMICA NA RAM SIMULADA
 * ============================================================================== */

// Aloca um novo aluno no HEAP simulado
int cadastrar_aluno_simulado(const char* nome, int p1, int p2, int freq) {
    if (g_mem.heap_pointer + TAM_ALUNO > FIM_HEAP) {
        snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log), "ERRO: HEAP esgotado! Limite de memoria da turma atingido.");
        return -1;
    }

    uint16_t base = g_mem.heap_pointer;
    static uint8_t proximo_id = 1;
    uint8_t id = proximo_id++;

    // Byte 0: ID
    ciclo_memw(base + 0, id, "HEAP: Aluno ID");

    // Bytes 1 a 16: Nome em ASCII
    char nome_buf[16] = {0};
    strncpy(nome_buf, nome, 15);
    for (int i = 0; i < 16; i++) {
        ciclo_memw(base + 1 + i, (uint8_t)nome_buf[i], "HEAP: Aluno Nome ASCII");
    }

    // Byte 17: P1 (0 a 100)
    ciclo_memw(base + 17, (uint8_t)(p1 > 100 ? 100 : (p1 < 0 ? 0 : p1)), "HEAP: Nota P1");

    // Byte 18: P2 (0 a 100)
    ciclo_memw(base + 18, (uint8_t)(p2 > 100 ? 100 : (p2 < 0 ? 0 : p2)), "HEAP: Nota P2");

    // Byte 19: Media inicial 0 (pendente de cálculo)
    ciclo_memw(base + 19, 0, "HEAP: Media (Pendente)");

    // Byte 20: Frequência
    ciclo_memw(base + 20, (uint8_t)(freq > 100 ? 100 : (freq < 0 ? 0 : freq)), "HEAP: Frequencia");

    // Byte 21: Status ('C' = Cursando)
    ciclo_memw(base + 21, (uint8_t)'C', "HEAP: Status Cursando");

    // Byte 22-23: Reservados
    ciclo_memw(base + 22, 0, "HEAP: Padding");
    ciclo_memw(base + 23, 0, "HEAP: Padding");

    g_mem.heap_pointer += TAM_ALUNO;
    snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log), "Aluno '%s' alocado no HEAP [0x%03X - 0x%03X]",
             nome, base, base + TAM_ALUNO - 1);
    return id;
}

// Executa cálculo usando a STACK simulada
void calcular_medias_turma_simulada() {
    uint8_t media_corte = ciclo_memr(0x0070, "Leitura ROM Media Minima");
    uint8_t freq_corte = ciclo_memr(0x0071, "Leitura ROM Frequencia Minima");

    for (uint16_t ptr = INICIO_HEAP; ptr < g_mem.heap_pointer; ptr += TAM_ALUNO) {
        uint8_t id = ciclo_memr(ptr + 0, "Leitura ID");
        if (id == 0) continue; // Posição livre/trancada

        uint8_t p1 = ciclo_memr(ptr + 17, "Leitura P1 para Calculo");
        uint8_t p2 = ciclo_memr(ptr + 18, "Leitura P2 para Calculo");
        uint8_t freq = ciclo_memr(ptr + 20, "Leitura Freq para Calculo");

        // 1. EMPILHA NA STACK
        g_mem.stack_pointer--;
        ciclo_memw(g_mem.stack_pointer, p1, "STACK PUSH: Parametro P1");
        g_mem.stack_pointer--;
        ciclo_memw(g_mem.stack_pointer, p2, "STACK PUSH: Parametro P2");
        g_mem.stack_pointer--;
        ciclo_memw(g_mem.stack_pointer, freq, "STACK PUSH: Parametro Freq");

        // 2. PROCESSAMENTO DA CPU
        uint8_t arg_freq = ciclo_memr(g_mem.stack_pointer, "STACK POP: Freq");
        g_mem.stack_pointer++;
        uint8_t arg_p2 = ciclo_memr(g_mem.stack_pointer, "STACK POP: P2");
        g_mem.stack_pointer++;
        uint8_t arg_p1 = ciclo_memr(g_mem.stack_pointer, "STACK POP: P1");
        g_mem.stack_pointer++;

        uint8_t media = (uint8_t)((arg_p1 + arg_p2) / 2);
        char status = 'R';
        if (media >= media_corte && arg_freq >= freq_corte) {
            status = 'A'; // Aprovado
        } else if (media >= 40 && arg_freq >= freq_corte) {
            status = 'F'; // Prova Final
        }

        // 3. ATUALIZAÇÃO NO HEAP VIA BARRAMENTO
        ciclo_memw(ptr + 19, media, "HEAP UPDATE: Media Calculada");
        ciclo_memw(ptr + 21, (uint8_t)status, "HEAP UPDATE: Situacao Final");
    }

    snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log), "Medias calculadas com sucesso via STACK simulada!");
}

// Simula tentativa de invasão/alteração da regra na ROM
void tentar_alterar_regra_rom() {
    ciclo_memw(0x0070, 50, "Tentativa de Fraudar Media Minima na ROM para 5.0");
}

// Tranca matrícula (desaloca aluno do HEAP)
int trancar_aluno_simulado(uint8_t id_procurado) {
    for (uint16_t ptr = INICIO_HEAP; ptr < g_mem.heap_pointer; ptr += TAM_ALUNO) {
        uint8_t id = ciclo_memr(ptr + 0, "Busca ID no HEAP");
        if (id == id_procurado) {
            char nome[16] = {0};
            for (int i = 0; i < 15; i++) {
                nome[i] = (char)ciclo_memr(ptr + 1 + i, "Leitura Nome");
            }
            // Zera o bloco no HEAP
            for (int i = 0; i < TAM_ALUNO; i++) {
                ciclo_memw(ptr + i, 0, "HEAP FREE: Liberacao de Memoria");
            }
            snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log), "Matricula do aluno '%s' trancada. 24 bytes liberados no HEAP.", nome);
            return 1;
        }
    }
    snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log), "Aluno ID %d nao localizado no HEAP.", id_procurado);
    return 0;
}

/* ==============================================================================
 * MICRO-SERVIDOR HTTP REST EM C PURO
 * ============================================================================== */

void gerar_json_memoria_completa(char* buffer, size_t max_len) {
    static char hex_str[TAM_MEMORIA * 2 + 1];
    static char occ_str[TAM_MEMORIA + 1];
    static const char hex_chars[] = "0123456789ABCDEF";

    for (int i = 0; i < TAM_MEMORIA; i++) {
        uint8_t b = g_mem.dados[i];
        hex_str[i * 2 + 0] = hex_chars[(b >> 4) & 0x0F];
        hex_str[i * 2 + 1] = hex_chars[b & 0x0F];
        occ_str[i] = g_mem.ocupado[i] ? '1' : '0';
    }
    hex_str[TAM_MEMORIA * 2] = '\0';
    occ_str[TAM_MEMORIA] = '\0';

    snprintf(buffer, max_len,
             "{\"mar\":%d,\"mbr\":%d,\"heap_topo\":%d,\"stack_sp\":%d,\"hex\":\"%s\",\"ocupado\":\"%s\"}",
             g_mem.mar, g_mem.mbr, g_mem.heap_pointer, g_mem.stack_pointer, hex_str, occ_str);
}

void gerar_json_estado(char* buffer, size_t max_len) {
    int usados_rom = 0, usados_text = 0, usados_data = 0, usados_heap = 0, usados_stack = 0;
    for (int i = 0; i < TAM_ROM; i++) if (g_mem.ocupado[i]) usados_rom++;
    for (int i = INICIO_TEXT; i <= FIM_TEXT; i++) if (g_mem.ocupado[i]) usados_text++;
    for (int i = INICIO_DATA; i <= FIM_DATA; i++) if (g_mem.ocupado[i]) usados_data++;
    for (int i = INICIO_HEAP; i < g_mem.heap_pointer; i++) if (g_mem.ocupado[i]) usados_heap++;
    for (int i = g_mem.stack_pointer; i <= TOPO_STACK; i++) if (g_mem.ocupado[i]) usados_stack++;

    // Extrai alunos do HEAP
    char alunos_json[4096] = "[";
    int count_alunos = 0;
    for (uint16_t ptr = INICIO_HEAP; ptr < g_mem.heap_pointer; ptr += TAM_ALUNO) {
        uint8_t id = g_mem.dados[ptr + 0];
        if (id == 0) continue;

        char nome[16] = {0};
        for (int i = 0; i < 15; i++) {
            char c = (char)g_mem.dados[ptr + 1 + i];
            if (c == 0) break;
            nome[i] = c;
        }

        uint8_t p1 = g_mem.dados[ptr + 17];
        uint8_t p2 = g_mem.dados[ptr + 18];
        uint8_t media = g_mem.dados[ptr + 19];
        uint8_t freq = g_mem.dados[ptr + 20];
        char status = (char)g_mem.dados[ptr + 21];

        char item[256];
        snprintf(item, sizeof(item),
                 "%s{\"id\":%d,\"nome\":\"%s\",\"p1\":%.1f,\"p2\":%.1f,\"media\":%.1f,\"freq\":%d,\"status\":\"%c\",\"endereco\":\"0x%03X\"}",
                 count_alunos > 0 ? "," : "", id, nome, p1 / 10.0, p2 / 10.0, media / 10.0, freq, status, ptr);
        strncat(alunos_json, item, sizeof(alunos_json) - strlen(alunos_json) - 1);
        count_alunos++;
    }
    strncat(alunos_json, "]", sizeof(alunos_json) - strlen(alunos_json) - 1);

    // Hexdump dos arredores do MAR (32 bytes)
    uint16_t base_dump = (g_mem.mar / 16) * 16;
    if (base_dump > TAM_MEMORIA - 32) base_dump = TAM_MEMORIA - 32;
    char dump_json[2048] = "[";
    for (int i = 0; i < 32; i++) {
        uint16_t addr = base_dump + i;
        char byte_item[64];
        snprintf(byte_item, sizeof(byte_item),
                 "%s{\"addr\":%d,\"val\":%d,\"hex\":\"%02X\",\"char\":\"%c\",\"ocupado\":%d}",
                 i > 0 ? "," : "", addr, g_mem.dados[addr], g_mem.dados[addr],
                 (g_mem.dados[addr] >= 32 && g_mem.dados[addr] <= 126) ? g_mem.dados[addr] : '.',
                 g_mem.ocupado[addr]);
        strncat(dump_json, byte_item, sizeof(dump_json) - strlen(dump_json) - 1);
    }
    strncat(dump_json, "]", sizeof(dump_json) - strlen(dump_json) - 1);

    // Regras atuais lidas da ROM
    float reg_media = g_mem.dados[0x0070] / 10.0;
    int reg_freq = g_mem.dados[0x0071];

    snprintf(buffer, max_len,
             "{"
             "\"mar\":%d,"
             "\"mar_hex\":\"0x%03X\","
             "\"mbr\":%d,"
             "\"mbr_hex\":\"0x%02X\","
             "\"mbr_char\":\"%c\","
             "\"sinal_codigo\":%d,"
             "\"sinal_str\":\"%s\","
             "\"rom_protegida\":%d,"
             "\"ultimo_log\":\"%s\","
             "\"estatisticas\":{"
             "  \"leituras\":%lu,"
             "  \"escritas\":%lu,"
             "  \"violacoes_rom\":%lu"
             "},"
             "\"regras\":{\"media_minima\":%.1f,\"freq_minima\":%d},"
             "\"segmentos\":{"
             "  \"rom\":{\"inicio\":\"0x000\",\"fim\":\"0x0FF\",\"usados\":%d,\"total\":%d},"
             "  \"text\":{\"inicio\":\"0x100\",\"fim\":\"0x3FF\",\"usados\":%d,\"total\":%d},"
             "  \"data\":{\"inicio\":\"0x400\",\"fim\":\"0x7FF\",\"usados\":%d,\"total\":%d},"
             "  \"heap\":{\"inicio\":\"0x800\",\"fim\":\"0xBFF\",\"usados\":%d,\"total\":%d,\"topo\":\"0x%03X\"},"
             "  \"stack\":{\"inicio\":\"0xC00\",\"fim\":\"0xFFF\",\"usados\":%d,\"total\":%d,\"sp\":\"0x%03X\"}"
             "},"
             "\"alunos\":%s,"
             "\"dump\":%s"
             "}",
             g_mem.mar, g_mem.mar, g_mem.mbr, g_mem.mbr,
             (g_mem.mbr >= 32 && g_mem.mbr <= 126) ? g_mem.mbr : '.',
             g_mem.ultimo_sinal, sinal_para_string(g_mem.ultimo_sinal),
             g_mem.rom_protegida, g_mem.ultimo_log,
             g_mem.total_leituras, g_mem.total_escritas, g_mem.violacoes_rom,
             reg_media, reg_freq,
             usados_rom, TAM_ROM,
             usados_text, (FIM_TEXT - INICIO_TEXT + 1),
             usados_data, (FIM_DATA - INICIO_DATA + 1),
             usados_heap, (FIM_HEAP - INICIO_HEAP + 1), g_mem.heap_pointer,
             usados_stack, (TOPO_STACK - INICIO_STACK + 1), g_mem.stack_pointer,
             alunos_json, dump_json);
}

void responder_http(int client_sock, const char* content_type, const char* body, int status_code) {
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d OK\r\n"
             "Content-Type: %s; charset=UTF-8\r\n"
             "Content-Length: %zu\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
             "Access-Control-Allow-Headers: Content-Type\r\n"
             "Connection: close\r\n\r\n",
             status_code, content_type, strlen(body));
    send(client_sock, header, strlen(header), 0);
    send(client_sock, body, strlen(body), 0);
}

void servir_arquivo_estatico(int client_sock, const char* caminho_relativo, const char* content_type) {
    char caminho_completo[512];
    snprintf(caminho_completo, sizeof(caminho_completo), "gui_web/%s", caminho_relativo);

    FILE* f = fopen(caminho_completo, "rb");
    if (!f) {
        const char* not_found = "<h1>404 Arquivo Nao Encontrado</h1>";
        responder_http(client_sock, "text/html", not_found, 404);
        return;
    }

    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* conteudo = malloc(tam + 1);
    if (!conteudo) {
        fclose(f);
        return;
    }
    fread(conteudo, 1, tam, f);
    conteudo[tam] = '\0';
    fclose(f);

    responder_http(client_sock, content_type, conteudo, 200);
    free(conteudo);
}

void* tratar_requisicao(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);

    char buffer[4096];
    ssize_t bytes_lidos = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_lidos <= 0) {
        close(client_sock);
        return NULL;
    }
    buffer[bytes_lidos] = '\0';

    char metodo[16], uri[256];
    sscanf(buffer, "%15s %255s", metodo, uri);

    if (strcmp(metodo, "OPTIONS") == 0) {
        responder_http(client_sock, "text/plain", "", 200);
        close(client_sock);
        return NULL;
    }

    pthread_mutex_lock(&g_mem_lock);

    if (strcmp(uri, "/api/estado") == 0) {
        static char json_resp[16384];
        gerar_json_estado(json_resp, sizeof(json_resp));
        responder_http(client_sock, "application/json", json_resp, 200);
    }
    else if (strcmp(uri, "/api/memoria_completa") == 0) {
        static char json_mem[20480];
        gerar_json_memoria_completa(json_mem, sizeof(json_mem));
        responder_http(client_sock, "application/json", json_mem, 200);
    }
    else if (strcmp(uri, "/api/aluno") == 0 && strcmp(metodo, "POST") == 0) {
        char* corpo = strstr(buffer, "\r\n\r\n");
        if (corpo) corpo += 4;
        char nome[64] = "Aluno Teste";
        int p1 = 80, p2 = 80, freq = 90;

        char* p_nome = strstr(corpo, "\"nome\":\"");
        if (p_nome) {
            sscanf(p_nome, "\"nome\":\"%63[^\"]\"", nome);
        }
        char* p_p1 = strstr(corpo, "\"p1\":");
        if (p_p1) sscanf(p_p1, "\"p1\":%d", &p1);
        char* p_p2 = strstr(corpo, "\"p2\":");
        if (p_p2) sscanf(p_p2, "\"p2\":%d", &p2);
        char* p_freq = strstr(corpo, "\"freq\":");
        if (p_freq) sscanf(p_freq, "\"freq\":%d", &freq);

        int id = cadastrar_aluno_simulado(nome, p1, p2, freq);
        char resp[256];
        snprintf(resp, sizeof(resp), "{\"sucesso\":%s,\"id\":%d}", id > 0 ? "true" : "false", id);
        responder_http(client_sock, "application/json", resp, 200);
    }
    else if (strcmp(uri, "/api/calcular") == 0 && strcmp(metodo, "POST") == 0) {
        calcular_medias_turma_simulada();
        responder_http(client_sock, "application/json", "{\"sucesso\":true,\"msg\":\"Medias calculadas na STACK\"}", 200);
    }
    else if (strcmp(uri, "/api/hack-rom") == 0 && strcmp(metodo, "POST") == 0) {
        tentar_alterar_regra_rom();
        responder_http(client_sock, "application/json", "{\"sucesso\":false,\"bloqueado\":true,\"msg\":\"MPU impediu violacao da ROM\"}", 200);
    }
    else if (strcmp(uri, "/api/trancar") == 0 && strcmp(metodo, "POST") == 0) {
        char* corpo = strstr(buffer, "\r\n\r\n");
        if (corpo) corpo += 4;
        int id = 0;
        char* p_id = strstr(corpo, "\"id\":");
        if (p_id) sscanf(p_id, "\"id\":%d", &id);

        int res = trancar_aluno_simulado((uint8_t)id);
        char resp[256];
        snprintf(resp, sizeof(resp), "{\"sucesso\":%s}", res ? "true" : "false");
        responder_http(client_sock, "application/json", resp, 200);
    }
    else if (strcmp(uri, "/api/reset") == 0 && strcmp(metodo, "POST") == 0) {
        inicializar_sistema_memoria();
        responder_http(client_sock, "application/json", "{\"sucesso\":true,\"msg\":\"Reset efetuado\"}", 200);
    }
    // Servir Arquivos da Interface Gráfica
    else if (strcmp(uri, "/") == 0 || strcmp(uri, "/index.html") == 0) {
        servir_arquivo_estatico(client_sock, "index.html", "text/html");
    }
    else if (strcmp(uri, "/style.css") == 0) {
        servir_arquivo_estatico(client_sock, "style.css", "text/css");
    }
    else if (strcmp(uri, "/app.js") == 0) {
        servir_arquivo_estatico(client_sock, "app.js", "application/javascript");
    }
    else {
        responder_http(client_sock, "text/plain", "404 Not Found", 404);
    }

    pthread_mutex_unlock(&g_mem_lock);
    close(client_sock);
    return NULL;
}

int main() {
    inicializar_sistema_memoria();

    // Cadastra 5 alunos iniciais (Autores do Projeto da UFMA) no HEAP
    cadastrar_aluno_simulado("Yuri Martins", 85, 90, 95);
    cadastrar_aluno_simulado("Caio Ferreira", 70, 75, 80);
    cadastrar_aluno_simulado("Danilo Lima", 95, 100, 100);
    cadastrar_aluno_simulado("Raynara Alves", 80, 85, 90);
    cadastrar_aluno_simulado("Rhua Sardinha", 60, 55, 75);
    calcular_medias_turma_simulada();

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Erro ao criar socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORTA_PADRAO);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        // Tenta porta alternativa caso 8080 esteja ocupada
        server_addr.sin_port = htons(8081);
        if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Erro no bind da porta HTTP");
            close(server_sock);
            return 1;
        }
    }

    if (listen(server_sock, 10) < 0) {
        perror("Erro no listen");
        close(server_sock);
        return 1;
    }

    uint16_t porta_real = ntohs(server_addr.sin_port);
    printf("===================================================================\n");
    printf(" UFMA - SIMULADOR DE MEMORIA (4 KB) | SISTEMA DE NOTAS ACADEMICO\n");
    printf(" Servidor HTTP Backend em C Puro ativo na porta %d\n", porta_real);
    printf(" Abra a interface: http://localhost:%d\n", porta_real);
    printf("===================================================================\n");
    fflush(stdout);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int* client_sock = malloc(sizeof(int));
        *client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (*client_sock < 0) {
            free(client_sock);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, tratar_requisicao, client_sock);
        pthread_detach(tid);
    }

    close(server_sock);
    return 0;
}
