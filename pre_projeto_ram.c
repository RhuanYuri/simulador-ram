/*
 * ============================================================
 * SIMULADOR DE MEMORIA RAM
 * Disciplina: Arquitetura de Computadores
 *
 * Funcionalidades:
 * 1 - Escrever dado na memoria
 * 2 - Ler dado da memoria
 * 3 - Exibir memoria
 * 4 - Limpar memoria
 * 0 - Sair
 *
 * Memoria simulada: 256 enderecos
 * ============================================================
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


#define TAM_MEMORIA 256
#define TAM_DADO 8


typedef struct {
    uint8_t dados[TAM_MEMORIA];
    int ocupado[TAM_MEMORIA];
    int total_escritas;
    int total_leituras;
} MemoriaRAM;




/* Inicializa a memoria */
void inicializar(MemoriaRAM *mem)
{
    int i;


    for (i = 0; i < TAM_MEMORIA; i++) {
        mem->dados[i] = 0;
        mem->ocupado[i] = 0;
    }


    mem->total_escritas = 0;
    mem->total_leituras = 0;
}




/* Verifica se o endereco e valido */
int endereco_valido(int endereco)
{
    return endereco >= 0 && endereco < TAM_MEMORIA;
}




/* Operacao de escrita */
void escrever(MemoriaRAM *mem, int endereco, int dado)
{
    if (!endereco_valido(endereco)) {
        printf("\nERRO: endereco invalido.\n");
        printf("Utilize um endereco entre 0 e %d.\n",
               TAM_MEMORIA - 1);
        return;
    }


    if (mem->ocupado[endereco]) {
        printf("\nEndereco %d ja possui o valor %d.\n",
               endereco, mem->dados[endereco]);


        printf("O valor sera sobrescrito.\n");
    }


    mem->dados[endereco] = dado;
    mem->ocupado[endereco] = 1;
    mem->total_escritas++;


    printf("\nESCRITA REALIZADA COM SUCESSO\n");
    printf("Endereco: %d\n", endereco);
    printf("Dado: %d\n", dado);
}




/* Operacao de leitura */
void ler(MemoriaRAM *mem, int endereco)
{
    if (!endereco_valido(endereco)) {
        printf("\nERRO: endereco invalido.\n");
        printf("Utilize um endereco entre 0 e %d.\n",
               TAM_MEMORIA - 1);
        return;
    }


    if (!mem->ocupado[endereco]) {
        printf("\nEndereco %d esta vazio.\n", endereco);
        return;
    }


    printf("\nLEITURA REALIZADA COM SUCESSO\n");
    printf("Endereco: %d\n", endereco);
    printf("Dado armazenado: %d\n", mem->dados[endereco]);


    mem->total_leituras++;
}




/* Exibe somente os enderecos utilizados */
void exibir_memoria(MemoriaRAM *mem)
{
    int i;
    int vazia = 1;


    printf("\n========================================\n");
    printf("         ESTADO DA MEMORIA RAM\n");
    printf("========================================\n");
    printf("Endereco\tDado\n");
    printf("----------------------------------------\n");


    for (i = 0; i < TAM_MEMORIA; i++) {


        if (mem->ocupado[i]) {
            printf("%03d\t\t%d\n", i, mem->dados[i]);
            vazia = 0;
        }
    }


    if (vazia) {
        printf("Memoria vazia.\n");
    }


    printf("----------------------------------------\n");
    printf("Total de escritas: %d\n", mem->total_escritas);
    printf("Total de leituras: %d\n", mem->total_leituras);
    printf("========================================\n");
}




/* Limpa toda a memoria */
void limpar_memoria(MemoriaRAM *mem)
{
    int i;


    for (i = 0; i < TAM_MEMORIA; i++) {
        mem->dados[i] = 0;
        mem->ocupado[i] = 0;
    }


    mem->total_escritas = 0;
    mem->total_leituras = 0;


    printf("\nMemoria limpa com sucesso.\n");
}




/* Limpa o buffer do teclado */
void limpar_buffer()
{
    int c;


    while ((c = getchar()) != '\n' && c != EOF);
}




/* Menu principal */
void exibir_menu()
{
    printf("\n");
    printf("========================================\n");
    printf("       SIMULADOR DE MEMORIA RAM\n");
    printf("========================================\n");
    printf("1 - Escrever na memoria\n");
    printf("2 - Ler da memoria\n");
    printf("3 - Exibir memoria\n");
    printf("4 - Limpar memoria\n");
    printf("0 - Sair\n");
    printf("========================================\n");
    printf("Escolha uma opcao: ");
}




/* Funcao principal */
int main()
{
    MemoriaRAM memoria;


    int opcao;
    int endereco;
    unsigned int dado;


    inicializar(&memoria);


    printf("\n");
    printf("========================================\n");
    printf("   SIMULADOR DE MEMORIA RAM\n");
    printf("   Arquitetura de Computadores\n");
    printf("   Memoria: 256 enderecos\n");
    printf("========================================\n");


    do {


        exibir_menu();


        if (scanf("%d", &opcao) != 1) {
            printf("\nERRO: opcao invalida.\n");
            limpar_buffer();
            continue;
        }


        limpar_buffer();


        switch (opcao) {


            case 1:


                printf("\n--- ESCRITA ---\n");


                printf("Digite o endereco (0 a %d): ",
                       TAM_MEMORIA - 1);


                if (scanf("%d", &endereco) != 1) {
                    printf("Endereco invalido.\n");
                    limpar_buffer();
                    break;
                }


                printf("Digite o dado(0 a 255): ");


                if (scanf("%u", &dado) != 1 || dado > 255) {
                    printf("Dado invalido. Digite um valor entre 0 e 255.\n");
                    limpar_buffer();
                    break;
                }


                limpar_buffer();


                escrever(&memoria, endereco, (uint8_t)dado);


                break;




            case 2:


                printf("\n--- LEITURA ---\n");


                printf("Digite o endereco (0 a %d): ",
                       TAM_MEMORIA - 1);


                if (scanf("%d", &endereco) != 1) {
                    printf("Endereco invalido.\n");
                    limpar_buffer();
                    break;
                }


                limpar_buffer();


                ler(&memoria, endereco);


                break;




            case 3:


                exibir_memoria(&memoria);


                break;




            case 4:


                limpar_memoria(&memoria);


                break;




            case 0:


                printf("\nEncerrando o simulador...\n");


                break;




            default:


                printf("\nOpcao invalida. Escolha entre 0 e 4.\n");
        }


    } while (opcao != 0);


    return 0;
}