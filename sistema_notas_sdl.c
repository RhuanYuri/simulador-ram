/*
 * ==============================================================================
 * UNIVERSIDADE FEDERAL DO MARANHÃO - UFMA | CCET - DEPARTAMENTO DE INFORMÁTICA
 * DISCIPLINA: ARQUITETURA DE COMPUTADORES
 * SISTEMA DE NOTAS ACADÊMICO COM MEMÓRIA RAM/ROM SIMULADA (4 KB)
 * APLICAÇÃO GRÁFICA NATIVA EM C PURO USANDO SDL2
 * ==============================================================================
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define JANELA_LARGURA     1280
#define JANELA_ALTURA      720

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

    unsigned long total_leituras;
    unsigned long total_escritas;
    unsigned long violacoes_rom;
    char ultimo_log[128];
} MemoriaSistema;

static MemoriaSistema g_mem;

/* ==============================================================================
 * FONTE BITMAP 8x8 EMBUTIDA (ASCII 32 a 126 - 100% INDEPENDENTE DE ASSETS)
 * ============================================================================== */
static const uint8_t g_font8x8[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 32 ' '
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // 33 '!'
    {0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}, // 34 '"'
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, // 35 '#'
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, // 36 '$'
    {0x00,0x66,0xA6,0xD4,0x2B,0x65,0x66,0x00}, // 37 '%'
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, // 38 '&'
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, // 39 '''
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // 40 '('
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // 41 ')'
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // 42 '*'
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // 43 '+'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // 44 ','
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // 45 '-'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // 46 '.'
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, // 47 '/'
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, // 48 '0'
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 49 '1'
    {0x3C,0x66,0x06,0x1C,0x30,0x60,0x7E,0x00}, // 50 '2'
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, // 51 '3'
    {0x0C,0x1C,0x3C,0x6C,0xFE,0x0C,0x0C,0x00}, // 52 '4'
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, // 53 '5'
    {0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0x00}, // 54 '6'
    {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00}, // 55 '7'
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, // 56 '8'
    {0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0x00}, // 57 '9'
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, // 58 ':'
    {0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}, // 59 ';'
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, // 60 '<'
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // 61 '='
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, // 62 '>'
    {0x3C,0x66,0x0C,0x18,0x18,0x00,0x18,0x00}, // 63 '?'
    {0x3C,0x66,0x6E,0x6E,0x60,0x62,0x3C,0x00}, // 64 '@'
    {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00}, // 65 'A'
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, // 66 'B'
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, // 67 'C'
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, // 68 'D'
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00}, // 69 'E'
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}, // 70 'F'
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, // 71 'G'
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // 72 'H'
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 73 'I'
    {0x0E,0x06,0x06,0x06,0x66,0x66,0x3C,0x00}, // 74 'J'
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, // 75 'K'
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, // 76 'L'
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // 77 'M'
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, // 78 'N'
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // 79 'O'
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, // 80 'P'
    {0x3C,0x66,0x66,0x66,0x6E,0x3C,0x0E,0x00}, // 81 'Q'
    {0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00}, // 82 'R'
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, // 83 'S'
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // 84 'T'
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // 85 'U'
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, // 86 'V'
    {0x63,0x63,0x6B,0x6B,0x7F,0x77,0x63,0x00}, // 87 'W'
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, // 88 'X'
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, // 89 'Y'
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, // 90 'Z'
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // 91 '['
    {0x80,0xC0,0x60,0x30,0x18,0x0C,0x06,0x00}, // 92 '\'
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // 93 ']'
    {0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00}, // 94 '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00}, // 95 '_'
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // 96 '`'
    {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3B,0x00}, // 97 'a'
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, // 98 'b'
    {0x00,0x00,0x3C,0x66,0x60,0x66,0x3C,0x00}, // 99 'c'
    {0x06,0x06,0x3E,0x66,0x66,0x66,0x3B,0x00}, // 100 'd'
    {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, // 101 'e'
    {0x0C,0x18,0x18,0x7E,0x18,0x18,0x18,0x00}, // 102 'f'
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x3C}, // 103 'g'
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, // 104 'h'
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // 105 'i'
    {0x06,0x00,0x0E,0x06,0x06,0x66,0x3C,0x00}, // 106 'j'
    {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}, // 107 'k'
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 108 'l'
    {0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00}, // 109 'm'
    {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, // 110 'n'
    {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, // 111 'o'
    {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, // 112 'p'
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x07}, // 113 'q'
    {0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}, // 114 'r'
    {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, // 115 's'
    {0x18,0x18,0x7E,0x18,0x18,0x18,0x0E,0x00}, // 116 't'
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3B,0x00}, // 117 'u'
    {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, // 118 'v'
    {0x00,0x00,0x63,0x6B,0x7F,0x77,0x36,0x00}, // 119 'w'
    {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, // 120 'x'
    {0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x3C}, // 121 'y'
    {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, // 122 'z'
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // 123 '{'
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // 124 '|'
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // 125 '}'
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}  // 126 '~'
};

void desenhar_caractere(SDL_Renderer* ren, char c, int x, int y, int escala, SDL_Color cor) {
    if (c < 32 || c > 126) c = '.';
    int idx = c - 32;
    SDL_SetRenderDrawColor(ren, cor.r, cor.g, cor.b, cor.a);
    for (int r = 0; r < 8; r++) {
        uint8_t linha = g_font8x8[idx][r];
        for (int col = 0; col < 8; col++) {
            if (linha & (0x80 >> col)) {
                SDL_Rect pixel = { x + col * escala, y + r * escala, escala, escala };
                SDL_RenderFillRect(ren, &pixel);
            }
        }
    }
}

void desenhar_texto(SDL_Renderer* ren, const char* texto, int x, int y, int escala, SDL_Color cor) {
    int cur_x = x;
    while (*texto) {
        desenhar_caractere(ren, *texto, cur_x, y, escala, cor);
        cur_x += 8 * escala;
        texto++;
    }
}

/* ==============================================================================
 * SUBSISTEMA FÍSICO DE BARRAMENTO E MEMÓRIA
 * ============================================================================== */

void ciclo_memw(uint16_t endereco, uint8_t dado, const char* motivo) {
    g_mem.mar = endereco & 0x0FFF;
    g_mem.mbr = dado;

    if (g_mem.mar < TAM_ROM && g_mem.rom_protegida) {
        g_mem.ultimo_sinal = BUS_PROT_FAULT;
        g_mem.violacoes_rom++;
        snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log), "BLOQUEIO MPU: Tentativa de sobrescrever ROM [0x%03X]!", g_mem.mar);
        return;
    }

    g_mem.dados[g_mem.mar] = g_mem.mbr;
    g_mem.ocupado[g_mem.mar] = 1;
    g_mem.ultimo_sinal = BUS_WRITE;
    g_mem.total_escritas++;
    if (motivo) snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log), "MEMW [0x%03X] <- 0x%02X (%s)", g_mem.mar, g_mem.mbr, motivo);
}

uint8_t ciclo_memr(uint16_t endereco, const char* motivo) {
    g_mem.mar = endereco & 0x0FFF;
    g_mem.mbr = g_mem.dados[g_mem.mar];
    g_mem.ultimo_sinal = BUS_READ;
    g_mem.total_leituras++;
    if (motivo) snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log), "MEMR [0x%03X] -> MBR=0x%02X (%s)", g_mem.mar, g_mem.mbr, motivo);
    return g_mem.mbr;
}

void inicializar_memoria() {
    memset(g_mem.dados, 0, sizeof(g_mem.dados));
    memset(g_mem.ocupado, 0, sizeof(g_mem.ocupado));
    g_mem.rom_protegida = 0;
    g_mem.mar = 0;
    g_mem.mbr = 0;
    g_mem.ultimo_sinal = BUS_IDLE;
    g_mem.total_leituras = 0;
    g_mem.total_escritas = 0;
    g_mem.violacoes_rom = 0;
    g_mem.heap_pointer = INICIO_HEAP;
    g_mem.stack_pointer = TOPO_STACK;

    const char* sig = "UFMA-CCET-DEINF: SISTEMA DE NOTAS SDL v1.0";
    for (size_t i = 0; i < strlen(sig) && i < 64; i++) {
        ciclo_memw((uint16_t)i, (uint8_t)sig[i], "Gravacao BIOS");
    }
    ciclo_memw(0x0070, 70, "ROM: Media Minima (7.0)");
    ciclo_memw(0x0071, 75, "ROM: Freq Minima (75%)");
    g_mem.rom_protegida = 1;

    snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log), "Sistema inicializado. ROM protegida.");
}

int cadastrar_aluno(const char* nome, int p1, int p2, int freq) {
    if (g_mem.heap_pointer + TAM_ALUNO > FIM_HEAP) {
        snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log), "ERRO: HEAP esgotado!");
        return -1;
    }
    uint16_t base = g_mem.heap_pointer;
    static uint8_t id_seq = 1;
    uint8_t id = id_seq++;

    ciclo_memw(base + 0, id, "HEAP: ID");
    char nbuf[16] = {0};
    strncpy(nbuf, nome, 15);
    for (int i = 0; i < 16; i++) {
        ciclo_memw(base + 1 + i, (uint8_t)nbuf[i], "HEAP: Nome");
    }
    ciclo_memw(base + 17, (uint8_t)p1, "HEAP: P1");
    ciclo_memw(base + 18, (uint8_t)p2, "HEAP: P2");
    ciclo_memw(base + 19, 0, "HEAP: Media");
    ciclo_memw(base + 20, (uint8_t)freq, "HEAP: Freq");
    ciclo_memw(base + 21, (uint8_t)'C', "HEAP: Status");
    ciclo_memw(base + 22, 0, "HEAP: Pad");
    ciclo_memw(base + 23, 0, "HEAP: Pad");

    g_mem.heap_pointer += TAM_ALUNO;
    snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log), "Aluno '%s' alocado no HEAP [0x%03X]", nome, base);
    return id;
}

void calcular_medias() {
    uint8_t media_min = ciclo_memr(0x0070, "ROM: Media Minima");
    uint8_t freq_min = ciclo_memr(0x0071, "ROM: Freq Minima");

    for (uint16_t ptr = INICIO_HEAP; ptr < g_mem.heap_pointer; ptr += TAM_ALUNO) {
        uint8_t id = ciclo_memr(ptr + 0, "ID");
        if (id == 0) continue;

        uint8_t p1 = ciclo_memr(ptr + 17, "P1");
        uint8_t p2 = ciclo_memr(ptr + 18, "P2");
        uint8_t freq = ciclo_memr(ptr + 20, "Freq");

        // STACK PUSH
        g_mem.stack_pointer--;
        ciclo_memw(g_mem.stack_pointer, p1, "STACK PUSH");
        g_mem.stack_pointer--;
        ciclo_memw(g_mem.stack_pointer, p2, "STACK PUSH");

        // STACK POP
        uint8_t val2 = ciclo_memr(g_mem.stack_pointer, "STACK POP");
        g_mem.stack_pointer++;
        uint8_t val1 = ciclo_memr(g_mem.stack_pointer, "STACK POP");
        g_mem.stack_pointer++;

        uint8_t media = (uint8_t)((val1 + val2) / 2);
        char st = (media >= media_min && freq >= freq_min) ? 'A' : (media >= 40 ? 'F' : 'R');

        ciclo_memw(ptr + 19, media, "HEAP: Media");
        ciclo_memw(ptr + 21, (uint8_t)st, "HEAP: Situacao");
    }
    snprintf(g_mem.ultimo_log, sizeof(g_mem.ultimo_log), "Medias calculadas com sucesso via STACK simulada!");
}

void testar_violacao_rom() {
    ciclo_memw(0x0070, 50, "Tentativa de Fraudar ROM para 5.0");
}

/* ==============================================================================
 * LOOP PRINCIPAL E RENDERIZAÇÃO GRÁFICA SDL2
 * ============================================================================== */

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Erro ao inicializar SDL2: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* win = SDL_CreateWindow(
        "UFMA • Sistema de Notas em Memória RAM/ROM Simulada (SDL2 Nativo)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        JANELA_LARGURA, JANELA_ALTURA,
        SDL_WINDOW_SHOWN
    );
    if (!win) {
        fprintf(stderr, "Erro ao criar janela: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        ren = SDL_CreateRenderer(win, -1, 0);
    }

    inicializar_memoria();
    cadastrar_aluno("Yuri Martins", 85, 90, 95);
    cadastrar_aluno("Caio Ferreira", 70, 75, 80);
    cadastrar_aluno("Danilo Lima", 95, 100, 100);
    calcular_medias();

    bool rodando = true;
    SDL_Event e;

    // Retângulos dos botões interativos
    SDL_Rect btn_add  = { 30, 100, 200, 36 };
    SDL_Rect btn_calc = { 245, 100, 215, 36 };
    SDL_Rect btn_hack = { 475, 100, 215, 36 };
    SDL_Rect btn_rst  = { 705, 100, 160, 36 };

    int contador_demo = 1;

    while (rodando) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                rodando = false;
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) rodando = false;
            } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = e.button.x;
                int my = e.button.y;

                if (mx >= btn_add.x && mx <= btn_add.x + btn_add.w && my >= btn_add.y && my <= btn_add.y + btn_add.h) {
                    char nome_novo[32];
                    snprintf(nome_novo, sizeof(nome_novo), "Aluno Demo %d", contador_demo++);
                    int p1 = 60 + (rand() % 40);
                    int p2 = 50 + (rand() % 50);
                    int freq = 75 + (rand() % 25);
                    cadastrar_aluno(nome_novo, p1, p2, freq);
                } else if (mx >= btn_calc.x && mx <= btn_calc.x + btn_calc.w && my >= btn_calc.y && my <= btn_calc.y + btn_calc.h) {
                    calcular_medias();
                } else if (mx >= btn_hack.x && mx <= btn_hack.x + btn_hack.w && my >= btn_hack.y && my <= btn_hack.y + btn_hack.h) {
                    testar_violacao_rom();
                } else if (mx >= btn_rst.x && mx <= btn_rst.x + btn_rst.w && my >= btn_rst.y && my <= btn_rst.y + btn_rst.h) {
                    inicializar_memoria();
                    cadastrar_aluno("Yuri Martins", 85, 90, 95);
                    cadastrar_aluno("Caio Ferreira", 70, 75, 80);
                    cadastrar_aluno("Danilo Lima", 95, 100, 100);
                    calcular_medias();
                }
            }
        }

        // Fundo
        SDL_SetRenderDrawColor(ren, 10, 15, 29, 255);
        SDL_RenderClear(ren);

        // Header Superior
        SDL_Color c_branco = { 255, 255, 255, 255 };
        SDL_Color c_azul   = { 56, 189, 248, 255 };
        SDL_Color c_cinza  = { 148, 163, 184, 255 };
        SDL_Color c_verde  = { 52, 211, 153, 255 };
        SDL_Color c_verm   = { 239, 68, 68, 255 };
        SDL_Color c_roxo   = { 192, 132, 252, 255 };
        SDL_Color c_amarelo= { 251, 191, 36, 255 };

        desenhar_texto(ren, "UFMA / CCET - SISTEMA DE NOTAS ACADEMICO", 30, 24, 2, c_azul);
        desenhar_texto(ren, "SUBSISTEMA DE MEMORIA RAM/ROM (4 KB) COM REGISTRADORES MAR/MBR (SDL2 NATIVO)", 30, 48, 1, c_cinza);

        // Linha divisória
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 25);
        SDL_RenderDrawLine(ren, 30, 76, JANELA_LARGURA - 30, 76);

        // Desenhar Botões Interativos
        // Botão 1: Cadastrar Aluno
        SDL_SetRenderDrawColor(ren, 2, 132, 199, 255);
        SDL_RenderFillRect(ren, &btn_add);
        desenhar_texto(ren, "+ MATRICULAR ALUNO", btn_add.x + 18, btn_add.y + 11, 1, c_branco);

        // Botão 2: Calcular Médias (STACK)
        SDL_SetRenderDrawColor(ren, 147, 51, 234, 255);
        SDL_RenderFillRect(ren, &btn_calc);
        desenhar_texto(ren, "* CALCULAR MEDIAS (STACK)", btn_calc.x + 12, btn_calc.y + 11, 1, c_branco);

        // Botão 3: Testar Violação ROM
        SDL_SetRenderDrawColor(ren, 220, 38, 38, 255);
        SDL_RenderFillRect(ren, &btn_hack);
        desenhar_texto(ren, "! TESTAR VIOLACAO ROM", btn_hack.x + 16, btn_hack.y + 11, 1, c_branco);

        // Botão 4: Reset
        SDL_SetRenderDrawColor(ren, 71, 85, 105, 255);
        SDL_RenderFillRect(ren, &btn_rst);
        desenhar_texto(ren, "# RESET MEMORIA", btn_rst.x + 18, btn_rst.y + 11, 1, c_branco);

        // ==============================================================================
        // COLUNA ESQUERDA: DIÁRIO DE NOTAS (LIDO DO HEAP)
        // ==============================================================================
        SDL_Rect card_tabela = { 30, 155, 600, 490 };
        SDL_SetRenderDrawColor(ren, 16, 24, 40, 255);
        SDL_RenderFillRect(ren, &card_tabela);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 20);
        SDL_RenderDrawRect(ren, &card_tabela);

        desenhar_texto(ren, "DIARIO DE CLASSE (DADOS NA RAM SIMULADA - HEAP 0x800)", 46, 172, 1, c_azul);

        // Cabeçalho da Tabela
        int ty = 205;
        desenhar_texto(ren, "ID   NOME DO ALUNO       ADDR    P1    P2   MEDIA  FREQ  SITUACAO", 46, ty, 1, c_cinza);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 30);
        SDL_RenderDrawLine(ren, 46, ty + 16, 614, ty + 16);

        ty += 28;
        for (uint16_t ptr = INICIO_HEAP; ptr < g_mem.heap_pointer; ptr += TAM_ALUNO) {
            uint8_t id = g_mem.dados[ptr + 0];
            if (id == 0) continue;

            char nome[16] = {0};
            for (int i = 0; i < 15; i++) {
                char ch = (char)g_mem.dados[ptr + 1 + i];
                if (ch == 0) break;
                nome[i] = ch;
            }

            float p1 = g_mem.dados[ptr + 17] / 10.0f;
            float p2 = g_mem.dados[ptr + 18] / 10.0f;
            float media = g_mem.dados[ptr + 19] / 10.0f;
            int freq = g_mem.dados[ptr + 20];
            char st = (char)g_mem.dados[ptr + 21];

            char linha[128];
            char st_str[16] = "CURSANDO";
            SDL_Color c_status = c_cinza;
            if (st == 'A') { strcpy(st_str, "APROVADO"); c_status = c_verde; }
            else if (st == 'R') { strcpy(st_str, "REPROVADO"); c_status = c_verm; }
            else if (st == 'F') { strcpy(st_str, "RECUPERACAO"); c_status = c_amarelo; }

            snprintf(linha, sizeof(linha), "#%02d  %-16s 0x%03X  %4.1f  %4.1f  %4.1f   %3d%%",
                     id, nome, ptr, p1, p2, media, freq);
            desenhar_texto(ren, linha, 46, ty, 1, c_branco);
            desenhar_texto(ren, st_str, 520, ty, 1, c_status);

            ty += 24;
            if (ty > 620) break;
        }

        // ==============================================================================
        // COLUNA DIREITA: REGISTRADORES, SEGMENTAÇÃO E HEATMAP 4 KB
        // ==============================================================================
        int rx = 655;

        // Card Registradores
        SDL_Rect card_reg = { rx, 100, 595, 110 };
        SDL_SetRenderDrawColor(ren, 16, 24, 40, 255);
        SDL_RenderFillRect(ren, &card_reg);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 20);
        SDL_RenderDrawRect(ren, &card_reg);

        desenhar_texto(ren, "BARRAMENTO DE CONTROLE & REGISTRADORES DE HARDWARE", rx + 16, 114, 1, c_azul);

        char buf_mar[64], buf_mbr[64], buf_sinal[64];
        snprintf(buf_mar, sizeof(buf_mar), "MAR: 0x%03X (End. %d)", g_mem.mar, g_mem.mar);
        snprintf(buf_mbr, sizeof(buf_mbr), "MBR: 0x%02X ('%c' / %d)",
                 g_mem.mbr, (g_mem.mbr >= 32 && g_mem.mbr <= 126) ? g_mem.mbr : '.', g_mem.mbr);

        const char* s_str = "BUS_IDLE";
        SDL_Color c_sinal = c_cinza;
        if (g_mem.ultimo_sinal == BUS_WRITE) { s_str = "SINAL: MEMW (ESCRITA)"; c_sinal = c_verde; }
        else if (g_mem.ultimo_sinal == BUS_READ) { s_str = "SINAL: MEMR (LEITURA)"; c_sinal = c_azul; }
        else if (g_mem.ultimo_sinal == BUS_PROT_FAULT) { s_str = "SINAL: BUS_PROT_FAULT!"; c_sinal = c_verm; }

        desenhar_texto(ren, buf_mar, rx + 16, 142, 2, c_azul);
        desenhar_texto(ren, buf_mbr, rx + 16, 168, 2, c_amarelo);
        desenhar_texto(ren, s_str, rx + 330, 148, 1, c_sinal);

        snprintf(buf_sinal, sizeof(buf_sinal), "MPU: %s", g_mem.rom_protegida ? "PROTEGIDA (ATIVA)" : "DESPROTEGIDA");
        desenhar_texto(ren, buf_sinal, rx + 330, 172, 1, g_mem.rom_protegida ? c_verde : c_verm);

        // Card Segmentos de Memória
        SDL_Rect card_seg = { rx, 225, 595, 170 };
        SDL_SetRenderDrawColor(ren, 16, 24, 40, 255);
        SDL_RenderFillRect(ren, &card_seg);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 20);
        SDL_RenderDrawRect(ren, &card_seg);

        desenhar_texto(ren, "MAPEAMENTO DOS 5 SEGMENTOS (4096 BYTES)", rx + 16, 238, 1, c_azul);

        int sy = 265;
        // ROM
        desenhar_texto(ren, "ROM   [0x000-0x0FF] (BIOS / Regras):", rx + 16, sy, 1, c_verm);
        SDL_Rect b_rom = { rx + 310, sy + 2, 260, 10 };
        SDL_SetRenderDrawColor(ren, 239, 68, 68, 255);
        SDL_RenderFillRect(ren, &b_rom);
        sy += 22;

        // TEXT
        desenhar_texto(ren, "TEXT  [0x100-0x3FF] (Instrucoes):", rx + 16, sy, 1, c_azul);
        SDL_Rect b_text = { rx + 310, sy + 2, 260, 10 };
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 15);
        SDL_RenderFillRect(ren, &b_text);
        b_text.w = 20;
        SDL_SetRenderDrawColor(ren, 56, 189, 248, 255);
        SDL_RenderFillRect(ren, &b_text);
        sy += 22;

        // DATA
        desenhar_texto(ren, "DATA  [0x400-0x7FF] (Constantes):", rx + 16, sy, 1, c_verde);
        SDL_Rect b_data = { rx + 310, sy + 2, 260, 10 };
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 15);
        SDL_RenderFillRect(ren, &b_data);
        b_data.w = 30;
        SDL_SetRenderDrawColor(ren, 16, 185, 129, 255);
        SDL_RenderFillRect(ren, &b_data);
        sy += 22;

        // HEAP
        char txt_heap[64];
        snprintf(txt_heap, sizeof(txt_heap), "HEAP  [0x800-0xBFF] (Alunos ^): Topo 0x%03X", g_mem.heap_pointer);
        desenhar_texto(ren, txt_heap, rx + 16, sy, 1, c_amarelo);
        SDL_Rect b_heap = { rx + 310, sy + 2, 260, 10 };
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 15);
        SDL_RenderFillRect(ren, &b_heap);
        int ocup_heap = (int)(((float)(g_mem.heap_pointer - INICIO_HEAP) / 1024.0f) * 260);
        b_heap.w = ocup_heap < 4 ? 4 : ocup_heap;
        SDL_SetRenderDrawColor(ren, 245, 158, 11, 255);
        SDL_RenderFillRect(ren, &b_heap);
        sy += 22;

        // STACK
        char txt_stack[64];
        snprintf(txt_stack, sizeof(txt_stack), "STACK [0xC00-0xFFF] (Pilhas v): SP   0x%03X", g_mem.stack_pointer);
        desenhar_texto(ren, txt_stack, rx + 16, sy, 1, c_roxo);
        SDL_Rect b_stack = { rx + 310, sy + 2, 260, 10 };
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 15);
        SDL_RenderFillRect(ren, &b_stack);
        int ocup_stack = (int)(((float)(TOPO_STACK - g_mem.stack_pointer) / 1024.0f) * 260);
        b_stack.w = ocup_stack < 4 ? 4 : ocup_stack;
        SDL_SetRenderDrawColor(ren, 168, 85, 247, 255);
        SDL_RenderFillRect(ren, &b_stack);

        // Card Heatmap 4 KB (Matriz de 4096 células)
        SDL_Rect card_mat = { rx, 410, 595, 235 };
        SDL_SetRenderDrawColor(ren, 16, 24, 40, 255);
        SDL_RenderFillRect(ren, &card_mat);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 20);
        SDL_RenderDrawRect(ren, &card_mat);

        desenhar_texto(ren, "MATRIZ FISICA DE CELULAS (4096 BYTES NO HARDWARE)", rx + 16, 422, 1, c_azul);

        int grid_x = rx + 16;
        int grid_y = 444;
        int cols = 64;
        int rows = 64;
        float cw = 560.0f / cols;
        float ch = 120.0f / rows;

        for (int i = 0; i < 4096; i++) {
            int cx = i % cols;
            int cy = i / cols;
            SDL_Rect cell = { grid_x + (int)(cx * cw), grid_y + (int)(cy * ch), (int)cw, (int)ch };

            if (i < 256) SDL_SetRenderDrawColor(ren, 239, 68, 68, 255);
            else if (i < 0x400) SDL_SetRenderDrawColor(ren, 56, 189, 248, 255);
            else if (i < 0x800) SDL_SetRenderDrawColor(ren, 16, 185, 129, 255);
            else if (i < 0xC00) {
                if (i < g_mem.heap_pointer) SDL_SetRenderDrawColor(ren, 245, 158, 11, 255);
                else SDL_SetRenderDrawColor(ren, 40, 25, 5, 255);
            } else {
                if (i >= g_mem.stack_pointer) SDL_SetRenderDrawColor(ren, 168, 85, 247, 255);
                else SDL_SetRenderDrawColor(ren, 25, 15, 35, 255);
            }

            if (i == g_mem.mar) {
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            }
            SDL_RenderFillRect(ren, &cell);
        }

        // Hex Dump do MAR
        char txt_dump[128];
        uint16_t b_dump = (g_mem.mar / 16) * 16;
        snprintf(txt_dump, sizeof(txt_dump), "HEX DUMP [0x%03X]: %02X %02X %02X %02X %02X %02X %02X %02X",
                 b_dump,
                 g_mem.dados[b_dump+0], g_mem.dados[b_dump+1], g_mem.dados[b_dump+2], g_mem.dados[b_dump+3],
                 g_mem.dados[b_dump+4], g_mem.dados[b_dump+5], g_mem.dados[b_dump+6], g_mem.dados[b_dump+7]);
        desenhar_texto(ren, txt_dump, rx + 16, 580, 1, c_amarelo);

        // Rodapé de Log
        SDL_Rect bar_log = { 30, 660, JANELA_LARGURA - 60, 36 };
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
        SDL_RenderFillRect(ren, &bar_log);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 20);
        SDL_RenderDrawRect(ren, &bar_log);

        desenhar_texto(ren, "LOG BARRAMENTO:", 44, 672, 1, c_azul);
        desenhar_texto(ren, g_mem.ultimo_log, 180, 672, 1, c_branco);

        SDL_RenderPresent(ren);
        SDL_Delay(16); // ~60 FPS
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
