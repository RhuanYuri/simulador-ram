#!/usr/bin/env bash
# ==============================================================================
# UFMA • ARQUITETURA DE COMPUTADORES
# INICIALIZADOR DA OPÇÃO 2: BACK-END EM C + INTERFACE GRÁFICA DESKTOP
# ==============================================================================

set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

echo "================================================================="
echo " Compilando Back-end em C Puro (sistema_notas_backend.c)..."
echo "================================================================="
gcc -O2 sistema_notas_backend.c -o sistema_notas_backend -lpthread

echo "[OK] Compilacao concluida com sucesso!"

# Encerra qualquer instancia anterior na porta 8080/8081
fuser -k 8080/tcp 2>/dev/null || true
fuser -k 8081/tcp 2>/dev/null || true

echo "Iniciando servidor de hardware..."
./sistema_notas_backend &
PID_BACKEND=$!

# Aguarda 1 segundo para o socket estar pronto
sleep 1

PORTA=8080
if ! nc -z localhost 8080 2>/dev/null; then
    PORTA=8081
fi

echo "Abrindo Interface Grafica Desktop em http://localhost:${PORTA}..."
if which google-chrome-stable >/dev/null 2>&1; then
    google-chrome-stable --app="http://localhost:${PORTA}" &
elif which google-chrome >/dev/null 2>&1; then
    google-chrome --app="http://localhost:${PORTA}" &
elif which xdg-open >/dev/null 2>&1; then
    xdg-open "http://localhost:${PORTA}" &
fi

echo "Pressione Ctrl+C para encerrar o sistema."
wait $PID_BACKEND
