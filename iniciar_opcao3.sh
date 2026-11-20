#!/usr/bin/env bash
# ==============================================================================
# UFMA • ARQUITETURA DE COMPUTADORES
# INICIALIZADOR DA OPÇÃO 3: APLICAÇÃO GRÁFICA NATIVA EM C COM SDL2
# ==============================================================================

set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

echo "================================================================="
echo " Compilando Aplicacao Grafica Nativa em C (sistema_notas_sdl.c)..."
echo "================================================================="
gcc -O2 sistema_notas_sdl.c -o sistema_notas_sdl $(pkg-config --cflags --libs sdl2)

echo "[OK] Compilacao concluida com sucesso!"
echo "Iniciando janela nativa SDL2 (1280x720)..."
./sistema_notas_sdl
