# UFMA • Arquitetura de Computadores (2026)
## Opção 3: Sistema de Notas Acadêmico em Janela Gráfica Nativa (SDL2 em C)

Esta branch implementa o **Sistema de Notas Acadêmico com Memória RAM/ROM Simulada de 4 KB** como uma **aplicação gráfica desktop nativa desenvolvida inteiramente em linguagem C puro usando SDL2**.

---

### Destaques da Implementação:
1. **100% em C Puro com SDL2:**
   - Abre uma janela gráfica nativa (1280x720) com renderização acelerada por hardware (OpenGL).
   - Fonte bitmap embutida no próprio código (não depende de arquivos externos `.ttf` nem assets).
2. **Arquitetura de Memória e Barramento:**
   - Subsistema de 4096 bytes dividido em **ROM, TEXT, DATA, HEAP e STACK**.
   - Registradores de barramento **MAR** (12 bits) e **MBR** (8 bits).
   - Linhas de controle: `MEMR`, `MEMW`, `BUS_PROT_FAULT`.
3. **Interatividade com Mouse:**
   - Botão **[+ MATRICULAR ALUNO]**: Aloca novo aluno sequencialmente no HEAP via ciclos `MEMW`.
   - Botão **[* CALCULAR MÉDIAS (STACK)]**: Empilha parâmetros na STACK, calcula na CPU e salva de volta no HEAP.
   - Botão **[! TESTAR VIOLAÇÃO ROM]**: Tenta sobrescrever a ROM; a MPU de hardware bloqueia e acusa `BUS_PROT_FAULT`.
   - Botão **[# RESET MEMÓRIA]**: Restaura o hardware aos padrões de fábrica.
4. **Telemetria ao Vivo:**
   - Matriz física das 4096 células de memória em tempo real.
   - Barras de segmentação com ponteiros do Heap ($\uparrow$) e da Stack ($\downarrow$).
   - Hex Dump do endereço apontado pelo registrador MAR.

---

### Como Compilar e Executar:

```bash
chmod +x iniciar_opcao3.sh
./iniciar_opcao3.sh
```

Ou diretamente com GCC:
```bash
gcc -O2 sistema_notas_sdl.c -o sistema_notas_sdl $(pkg-config --cflags --libs sdl2)
./sistema_notas_sdl
```
Pressione `Esc` ou feche a janela para sair.
