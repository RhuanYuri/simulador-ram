# UFMA • Arquitetura de Computadores (2026)
## Opção 2: Sistema de Notas Acadêmico com Memória RAM/ROM Simulada
### Back-end em C Puro + Interface Gráfica Desktop Moderna

Esta branch implementa o **Sistema de Notas Acadêmico** acoplado diretamente a um **subsistema de memória de 4 KB (4096 bytes)** com barramento de 8 bits, registradores **MAR** e **MBR**, e linhas de controle (**MEMR**, **MEMW**, **BUS_PROT_FAULT**).

---

### Como Funciona a Arquitetura:
1. **Back-end em C Puro (`sistema_notas_backend.c`):**
   - Gerencia a memória física simulada de 4096 bytes dividida em 5 segmentos:
     * **ROM (`0x000`–`0x0FF`):** Regras do regimento da UFMA (média 7.0, freq. 75%). Protegida pela MPU de hardware.
     * **TEXT (`0x100`–`0x3FF`):** Instruções compiladas de cálculo.
     * **DATA (`0x400`–`0x7FF`):** Constantes globais e cabeçalhos.
     * **HEAP (`0x800`–`0xBFF`):** Alocação dinâmica de alunos (24 bytes por aluno, crescendo *Bottom-Up*).
     * **STACK (`0xC00`–`0xFFF`):** Pilha de execução (*Top-Down*). Durante o cálculo da média, empilha parâmetros e desempilha na conclusão.
   - Sobe um servidor HTTP REST nativo em C de alta performance (sem bibliotecas externas) na porta 8080.

2. **Front-end Gráfico Desktop (`gui_web/`):**
   - Interface moderna em Dark Mode com visual Glassmorphism.
   - Abre como uma janela desktop nativa (`google-chrome-stable --app="http://localhost:8080"`).
   - Telemetria de hardware em tempo real:
     * Registradores **MAR** e **MBR** atualizados a cada ciclo.
     * Indicador visual luminoso do sinal de barramento.
     * Mapa de calor (Heatmap) com matriz de 4096 células no Canvas.
     * Hex Dump ao vivo e log do barramento.
     * Alarme visual em caso de tentativa de violação da ROM pela MPU.

---

### Como Executar:

Basta rodar o script no terminal:
```bash
./iniciar_opcao2.sh
```

Ou manualmente:
```bash
gcc -O2 sistema_notas_backend.c -o sistema_notas_backend -lpthread
./sistema_notas_backend
```
E abrir no navegador: `http://localhost:8080` (ou janela nativa com `google-chrome-stable --app="http://localhost:8080"`).
