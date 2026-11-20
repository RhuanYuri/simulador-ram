// ==============================================================================
// UFMA - SISTEMA DE NOTAS ACADÊMICO & HARDWARE BUS MONITOR
// FRONT-END GRÁFICO (CONEXÃO COM BACK-END EM C VIA REST)
// ==============================================================================

const API_BASE = '';
let estadoAtual = null;
let alertaTimer = null;

// Inicializa a aplicação
document.addEventListener('DOMContentLoaded', () => {
  atualizarDados();
  // Polling em tempo real a cada 300ms
  setInterval(atualizarDados, 300);
});

// Busca o estado completo do hardware no back-end em C
async function atualizarDados() {
  try {
    const res = await fetch(`${API_BASE}/api/estado`);
    if (!res.ok) throw new Error('Falha HTTP');
    const data = await res.json();
    estadoAtual = data;
    renderizarInterface(data);
    if (modalAberto) carregarMemoriaCompleta();
    marcarStatusBackend(true);
  } catch (err) {
    marcarStatusBackend(false);
  }
}

function marcarStatusBackend(online) {
  const el = document.getElementById('backend-status');
  if (online) {
    el.className = 'status-indicator online';
    el.innerHTML = '<span class="pulse"></span><span>Back-end em C: Conectado</span>';
  } else {
    el.className = 'status-indicator offline';
    el.innerHTML = '<span class="pulse" style="background:#ef4444;box-shadow:none;"></span><span>Back-end em C: Desconectado</span>';
  }
}

// Renderiza todos os painéis visuais
function renderizarInterface(data) {
  // 1. Registradores e Barramento
  document.getElementById('val-mar-hex').textContent = data.mar_hex;
  document.getElementById('val-mar-dec').textContent = `(End. ${data.mar})`;
  document.getElementById('val-mbr-hex').textContent = data.mbr_hex;
  document.getElementById('val-mbr-char').textContent = `'${data.mbr_char}'`;
  document.getElementById('val-mbr-dec').textContent = `(${data.mbr})`;

  const tagSinal = document.getElementById('val-sinal-tag');
  tagSinal.textContent = data.sinal_str;
  tagSinal.className = 'signal-tag';
  if (data.sinal_codigo === 1) tagSinal.classList.add('read');
  else if (data.sinal_codigo === 2) tagSinal.classList.add('write');
  else if (data.sinal_codigo === 3) tagSinal.classList.add('fault');

  document.getElementById('val-mpu-status').textContent =
    data.rom_protegida ? 'MPU: Protegida (Ativa)' : 'MPU: Desprotegida';

  // 2. Segmentos de Memória
  const segs = data.segmentos;
  atualizarBarra('rom', segs.rom.usados, segs.rom.total, `${segs.rom.usados} / ${segs.rom.total} B (100%)`);
  atualizarBarra('text', segs.text.usados, segs.text.total, `${segs.text.usados} / ${segs.text.total} B`);
  atualizarBarra('data', segs.data.usados, segs.data.total, `${segs.data.usados} / ${segs.data.total} B`);
  atualizarBarra('heap', segs.heap.usados, segs.heap.total, `${segs.heap.usados} / ${segs.heap.total} B • Topo: ${segs.heap.topo}`);
  atualizarBarra('stack', segs.stack.usados, segs.stack.total, `${segs.stack.usados} / ${segs.stack.total} B • SP: ${segs.stack.sp}`);

  // 3. Tabela do Diário de Classe
  renderizarTabelaAlunos(data.alunos);

  // 4. Log do Barramento
  document.getElementById('log-text').textContent = data.ultimo_log;

  // 5. Canvas Heatmap de 4 KB
  desenharCanvasMemoria(data);

  // 6. Hex Dump
  renderizarHexDump(data.dump, data.mar);
}

function atualizarBarra(tipo, usados, total, texto) {
  const pct = Math.min(100, Math.max(1, (usados / total) * 100));
  const elBar = document.getElementById(`bar-${tipo}`);
  const elStats = document.getElementById(`stats-${tipo}`);
  if (elBar) elBar.style.width = `${pct}%`;
  if (elStats) elStats.textContent = texto;
}

// Renderiza a tabela de alunos lida da RAM simulada
function renderizarTabelaAlunos(alunos) {
  const corpo = document.getElementById('tabela-alunos-corpo');
  document.getElementById('total-alunos-count').textContent = alunos.length;

  if (alunos.length === 0) {
    corpo.innerHTML = `<tr><td colspan="9" class="empty-state">Nenhum aluno matriculado no HEAP. Preencha o formulário acima!</td></tr>`;
    return;
  }

  corpo.innerHTML = alunos.map(aluno => {
    let statusClass = 'cursando';
    let statusLabel = 'Pendente';
    if (aluno.status === 'A') { statusClass = 'aprovado'; statusLabel = 'Aprovado'; }
    else if (aluno.status === 'R') { statusClass = 'reprovado'; statusLabel = 'Reprovado'; }
    else if (aluno.status === 'F') { statusClass = 'final'; statusLabel = 'Prova Final'; }

    return `
      <tr>
        <td><strong>#${aluno.id}</strong></td>
        <td><strong>${aluno.nome}</strong></td>
        <td class="addr">${aluno.endereco}</td>
        <td>${aluno.p1.toFixed(1)}</td>
        <td>${aluno.p2.toFixed(1)}</td>
        <td><strong>${aluno.media > 0 ? aluno.media.toFixed(1) : '-'}</strong></td>
        <td>${aluno.freq}%</td>
        <td><span class="status-pill ${statusClass}">${statusLabel}</span></td>
        <td>
          <button class="btn-trash" onclick="trancarMatricula(${aluno.id})" title="Trancar Matrícula (Liberar 24 B no HEAP)">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 6h18M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/></svg>
          </button>
        </td>
      </tr>
    `;
  }).join('');
}

// Desenha a matriz visual de 4096 bytes no canvas
function desenharCanvasMemoria(data) {
  const canvas = document.getElementById('memory-canvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const cols = 64;
  const rows = 64; // 64 * 64 = 4096 células!
  const cellWidth = canvas.width / cols;
  const cellHeight = canvas.height / rows;

  ctx.fillStyle = '#030712';
  ctx.fillRect(0, 0, canvas.width, canvas.height);

  for (let i = 0; i < 4096; i++) {
    const col = i % cols;
    const row = Math.floor(i / cols);
    const x = col * cellWidth;
    const y = row * cellHeight;

    // Cor por região
    if (i < 256) ctx.fillStyle = '#ef4444'; // ROM
    else if (i < 0x400) ctx.fillStyle = '#38bdf8'; // TEXT
    else if (i < 0x800) ctx.fillStyle = '#10b981'; // DATA
    else if (i < 0xC00) {
      // HEAP: preenchido até heap_pointer
      const heapTopo = parseInt(data.segmentos.heap.topo, 16);
      ctx.fillStyle = (i < heapTopo) ? '#f59e0b' : '#332005';
    } else {
      // STACK: preenchido de stack_pointer até 0xFFF
      const stackSP = parseInt(data.segmentos.stack.sp, 16);
      ctx.fillStyle = (i >= stackSP) ? '#a855f7' : '#1e102d';
    }

    ctx.fillRect(x, y, cellWidth - 0.5, cellHeight - 0.5);

    // Destaca em dourado brilhante a célula onde o MAR está posicionado
    if (i === data.mar) {
      ctx.fillStyle = '#ffffff';
      ctx.fillRect(x - 1, y - 1, cellWidth + 2, cellHeight + 2);
    }
  }
}

// Renderiza o grid de Hex Dump
function renderizarHexDump(dump, marAtual) {
  const grid = document.getElementById('dump-grid-container');
  if (!grid || !dump) return;

  if (dump.length > 0) {
    const minAddr = dump[0].addr.toString(16).toUpperCase().padStart(3, '0');
    const maxAddr = dump[dump.length - 1].addr.toString(16).toUpperCase().padStart(3, '0');
    document.getElementById('dump-range-label').textContent = `0x${minAddr} a 0x${maxAddr}`;
  }

  grid.innerHTML = dump.map(byte => {
    const isMar = (byte.addr === marAtual);
    const isOcc = byte.ocupado;
    return `
      <div class="dump-cell ${isMar ? 'active-mar' : ''} ${isOcc ? 'occupied' : ''}" title="End. 0x${byte.addr.toString(16).toUpperCase()}: ${byte.val} ('${byte.char}')">
        ${byte.hex}
      </div>
    `;
  }).join('');
}

// AÇÕES DO USUÁRIO ENVIADAS PARA O BACK-END EM C

// 1. Cadastra Aluno
async function cadastrarAluno(e) {
  e.preventDefault();
  const nome = document.getElementById('nome-aluno').value.trim();
  const p1 = Math.round(parseFloat(document.getElementById('p1-aluno').value) * 10);
  const p2 = Math.round(parseFloat(document.getElementById('p2-aluno').value) * 10);
  const freq = parseInt(document.getElementById('freq-aluno').value, 10);

  if (!nome) return;

  try {
    const res = await fetch(`${API_BASE}/api/aluno`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ nome, p1, p2, freq })
    });
    if (res.ok) {
      document.getElementById('nome-aluno').value = '';
      atualizarDados();
    }
  } catch (err) {
    alert('Erro ao comunicar com o back-end em C!');
  }
}

// 2. Calcula Médias na STACK
async function calcularMedias() {
  try {
    await fetch(`${API_BASE}/api/calcular`, { method: 'POST' });
    atualizarDados();
  } catch (err) {
    alert('Erro ao executar rotina de cálculo na STACK!');
  }
}

// 3. Tenta Alterar Regra na ROM (Gera violação MPU)
async function tentarHackearRom() {
  try {
    const res = await fetch(`${API_BASE}/api/hack-rom`, { method: 'POST' });
    const data = await res.json();

    // Dispara alerta visual na tela
    const alerta = document.getElementById('alerta-mpu');
    document.getElementById('alerta-mpu-msg').textContent =
      'A MPU de hardware interceptou a instrução MEMW no endereço 0x070 e emitiu o sinal BUS_PROT_FAULT! A média da UFMA permanece protegida em 7.0.';
    alerta.classList.add('active');

    if (alertaTimer) clearTimeout(alertaTimer);
    alertaTimer = setTimeout(() => {
      alerta.classList.remove('active');
    }, 6000);

    atualizarDados();
  } catch (err) {
    alert('Erro ao tentar teste de violação!');
  }
}

// 4. Tranca Matrícula
async function trancarMatricula(id) {
  if (!confirm(`Deseja realmente trancar a matrícula do aluno #${id} e liberar seus 24 bytes no HEAP?`)) return;
  try {
    await fetch(`${API_BASE}/api/trancar`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id })
    });
    atualizarDados();
  } catch (err) {
    alert('Erro ao trancar matrícula!');
  }
}

// 5. Reset de Hardware
async function executarReset() {
  if (!confirm('Deseja executar o ciclo de Reset a Frio e reinicializar todos os 4096 bytes da memória?')) return;
  try {
    await fetch(`${API_BASE}/api/reset`, { method: 'POST' });
    atualizarDados();
  } catch (err) {
    alert('Erro ao resetar hardware!');
  }
}

// ==============================================================================
// MODAL EXPANDIDO: MAPA DA RAM PARA DEMONSTRAÇÃO
// ==============================================================================

let modalAberto = false;
let memoriaCompletaCache = null;
let filtroSegmentoAtivo = 'all';
let celulaSelecionada = 0x0800;
let paginaHexAtiva = 0x800;

function abrirModalMapaRAM() {
  const modal = document.getElementById('modal-ram-expandido');
  if (!modal) return;
  modal.classList.add('open');
  modalAberto = true;
  carregarMemoriaCompleta();
  configurarEventosCanvasModal();
}

function fecharModalMapaRAM() {
  const modal = document.getElementById('modal-ram-expandido');
  if (!modal) return;
  modal.classList.remove('open');
  modalAberto = false;
}

// Carrega os 4096 bytes brutos do back-end em C
async function carregarMemoriaCompleta() {
  try {
    const res = await fetch(`${API_BASE}/api/memoria_completa`);
    if (!res.ok) return;
    const data = await res.json();
    memoriaCompletaCache = data;
    desenharCanvasModal();
    renderizarHexContinuoModal();
    inspecionarCelula(celulaSelecionada);
  } catch (err) {
    console.error('Erro ao carregar memoria completa:', err);
  }
}

function filtrarSegmentoModal(seg) {
  filtroSegmentoAtivo = seg;
  document.querySelectorAll('.modal-ram-filters .filter-btn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.filter === seg);
  });
  desenharCanvasModal();
}

function mudarPaginaHexModal(val) {
  paginaHexAtiva = parseInt(val, 16);
  renderizarHexContinuoModal();
}

// Configura eventos de mouse sobre o canvas de 600x600 do modal
function configurarEventosCanvasModal() {
  const canvas = document.getElementById('modal-ram-canvas');
  if (!canvas || canvas.dataset.configured) return;
  canvas.dataset.configured = 'true';

  function tratarHover(evt) {
    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    const mouseX = (evt.clientX - rect.left) * scaleX;
    const mouseY = (evt.clientY - rect.top) * scaleY;

    const cols = 64;
    const rows = 64;
    const cellW = canvas.width / cols;
    const cellH = canvas.height / rows;

    const col = Math.floor(mouseX / cellW);
    const row = Math.floor(mouseY / cellH);

    if (col >= 0 && col < cols && row >= 0 && row < rows) {
      const idx = row * cols + col;
      if (idx >= 0 && idx < 4096) {
        celulaSelecionada = idx;
        inspecionarCelula(idx);
        desenharCanvasModal();
      }
    }
  }

  canvas.addEventListener('mousemove', tratarHover);
  canvas.addEventListener('click', tratarHover);
}

// Inspeciona os detalhes de qualquer byte de 0 a 4095
function inspecionarCelula(addr) {
  if (!memoriaCompletaCache) return;

  const bHex = memoriaCompletaCache.hex.substr(addr * 2, 2);
  const bVal = parseInt(bHex, 16);
  const bBin = bVal.toString(2).padStart(8, '0');
  const bChar = (bVal >= 32 && bVal <= 126) ? String.fromCharCode(bVal) : '·';
  const isOcupado = (memoriaCompletaCache.ocupado.charAt(addr) === '1');

  document.getElementById('insp-addr').textContent = `0x${addr.toString(16).toUpperCase().padStart(3, '0')} (Decimal ${addr})`;
  document.getElementById('insp-hex').textContent = `0x${bHex}`;
  document.getElementById('insp-dec').textContent = bVal;
  document.getElementById('insp-bin').textContent = bBin;
  document.getElementById('insp-ascii').textContent = `'${bChar}'`;

  // Identifica Região e Significado
  let seg = '';
  let desc = '';

  if (addr < 256) {
    seg = 'ROM (BIOS / Firmware e Regimento UFMA)';
    if (addr === 0x0070) desc = 'Média Mínima para Aprovação (7.0) [PROTEGIDO PELA MPU]';
    else if (addr === 0x0071) desc = 'Frequência Mínima Obrigatória (75%) [PROTEGIDO PELA MPU]';
    else if (addr === 0x0072) desc = 'Peso da Avaliação P1 (50%)';
    else if (addr === 0x0073) desc = 'Peso da Avaliação P2 (50%)';
    else if (addr === 0x0074) desc = 'Nota Máxima Permitida no Sistema (10.0)';
    else if (addr === 0x0075) desc = 'Nota Mínima para Direito à Prova Final (4.0)';
    else if (addr === 0x0076) desc = 'Carga Horária Total da Disciplina (60 Horas)';
    else if (addr === 0x0077) desc = 'Limite Máximo de Faltas Permitidas (15 Faltas)';
    else if (addr >= 0x0040 && addr <= 0x005F) desc = 'Vetor de Interrupção e Reset Vector da CPU (Ponteiro de Bootloader)';
    else if (addr < 64) desc = `Assinatura de Boot do Firmware: '${bChar}' ("UFMA-CCET-DEINF...")`;
    else if (addr >= 0x0080 && addr <= 0x00BF) desc = `Texto Normativo: Resolução CONSEPE UFMA N. 1826 ('${bChar}')`;
    else if (addr >= 0x00C0 && addr <= 0x00FF) desc = `Diretiva MPU: Bloqueio de Escrita por Hardware (Somente Leitura)`;
    else desc = 'Célula de Firmware da Instituição';
  } else if (addr < 0x400) {
    seg = 'TEXT (Código Executável / Opcodes Compilados)';
    if (addr >= 0x0100 && addr <= 0x012F) {
      desc = `Opcode Assembly da rotina 'calcular_media()' (Byte: 0x${bHex} - Instruções: PUSH EBP, MOV, ADD, SHR, RET)`;
    } else if (addr >= 0x0130 && addr <= 0x015F) {
      desc = `Opcode Assembly da rotina 'verificar_aprovacao()' (Byte: 0x${bHex} - Compara nota com 7.0 e define status)`;
    } else if (addr >= 0x0160 && addr <= 0x018F) {
      desc = `Opcode Assembly da rotina 'lancar_nota_barramento()' (Byte: 0x${bHex} - Movimentação via MAR/MBR)`;
    } else if (addr >= 0x0190 && addr <= 0x01BF) {
      desc = `Opcode Assembly da rotina 'trancar_matricula_heap()' (Byte: 0x${bHex} - Instrução REP STOSB de limpeza)`;
    } else if (addr >= 0x01C0 && addr <= 0x01EF) {
      desc = `Opcode da rotina de autoteste POST da BIOS (Byte: 0x${bHex} - Inicialização dos barramentos da CPU)`;
    } else {
      desc = 'Área de instruções compiladas do programa (Permissão de Leitura e Execução)';
    }
  } else if (addr < 0x800) {
    seg = 'DATA (Constantes Globais, Nomes e Cabeçalhos)';
    if (addr >= 0x0400 && addr <= 0x043F) desc = `String Global: Nome da Disciplina e Código ('${bChar}')`;
    else if (addr >= 0x0440 && addr <= 0x047F) desc = `String Global: Docente e Departamento Responsável ('${bChar}')`;
    else if (addr >= 0x0480 && addr <= 0x04BF) desc = `String Global: Semestre Letivo e Laboratório de Hardware ('${bChar}')`;
    else if (addr >= 0x04C0 && addr <= 0x04FF) desc = `String Global: Tabela de Status Acadêmicos ('${bChar}')`;
    else if (addr >= 0x0500 && addr <= 0x053F) desc = `String Global: Cabeçalho das Colunas do Diário de Classe ('${bChar}')`;
    else if (addr >= 0x0540 && addr <= 0x057F) desc = `String Global: Descrição do Sistema Integrado ('${bChar}')`;
    else if (addr >= 0x0580 && addr <= 0x05BF) desc = `String Global: Mensagem de Boot Concluído ('${bChar}')`;
    else desc = `Cadeia de texto ASCII terminada em \\0 (Caractere '${bChar}')`;
  } else if (addr < 0xC00) {
    seg = 'HEAP (Alocação Dinâmica de Alunos - Bottom-Up)';
    const offsetHeap = addr - 0x800;
    const alunoNum = Math.floor(offsetHeap / 24) + 1;
    const campoIdx = offsetHeap % 24;

    if (!isOcupado) {
      desc = `Espaço livre no HEAP disponível para novas matrículas`;
    } else if (campoIdx === 0) {
      desc = `Byte de Matrícula/ID do Aluno #${alunoNum}`;
    } else if (campoIdx >= 1 && campoIdx <= 16) {
      desc = `Caractere ASCII do Nome do Aluno #${alunoNum} ('${bChar}')`;
    } else if (campoIdx === 17) {
      desc = `Nota da Prova 1 (P1 = ${(bVal / 10).toFixed(1)}) do Aluno #${alunoNum}`;
    } else if (campoIdx === 18) {
      desc = `Nota da Prova 2 (P2 = ${(bVal / 10).toFixed(1)}) do Aluno #${alunoNum}`;
    } else if (campoIdx === 19) {
      desc = `Média Final Calculada (${(bVal / 10).toFixed(1)}) do Aluno #${alunoNum}`;
    } else if (campoIdx === 20) {
      desc = `Frequência Acadêmica (${bVal}%) do Aluno #${alunoNum}`;
    } else if (campoIdx === 21) {
      desc = `Situação de Aprovação ('${bChar}' = ${bChar === 'A' ? 'Aprovado' : bChar === 'R' ? 'Reprovado' : 'Final'})`;
    } else {
      desc = `Byte de alinhamento estrutural do Aluno #${alunoNum}`;
    }
  } else {
    seg = 'STACK (Pilha de Execução de Funções - Top-Down)';
    if (addr === 0x0FFF) desc = 'Quadro STACK: Endereço de Retorno para o OS/BIOS (0x00)';
    else if (addr === 0x0FFE) desc = 'Quadro STACK: Argumento argc (Contagem = 1)';
    else if (addr === 0x0FFD) desc = 'Quadro STACK: Ponteiro do vetor de argumentos argv';
    else if (addr === 0x0FFC) desc = 'Quadro STACK: EBP Salvo da função principal main()';
    else if (addr === 0x0FFB) desc = 'Quadro STACK: Endereço de Retorno (High Byte 0x01) da função calcular_medias()';
    else if (addr === 0x0FFA) desc = 'Quadro STACK: Endereço de Retorno (Low Byte 0x30) da rotina';
    else if (addr === 0x0FF9) desc = 'Quadro STACK: EBP Salvo do quadro da função de cálculo';
    else if (addr === 0x0FF8) desc = 'Quadro STACK: Variável Local Temporária: \'soma_notas\' (Valor: 175)';
    else if (addr === 0x0FF7) desc = 'Quadro STACK: Parâmetro empilhado: Nota P1 (8.5 / 85)';
    else if (addr === 0x0FF6) desc = 'Quadro STACK: Parâmetro empilhado: Nota P2 (9.0 / 90)';
    else if (addr === 0x0FF5) desc = 'Quadro STACK: Parâmetro empilhado: Frequência (95%)';
    else if (addr === 0x0FF4) desc = 'Quadro STACK: Valor Retornado no Acumulador: Status \'A\' (Aprovado)';
    else {
      desc = 'Espaço livre na STACK reservado para novas chamadas de funções aninhadas';
    }
  }

  document.getElementById('insp-seg').textContent = seg;
  document.getElementById('insp-desc').textContent = desc;
}

// Desenha o Canvas de 600x600 do Modal com nitidez total
function desenharCanvasModal() {
  const canvas = document.getElementById('modal-ram-canvas');
  if (!canvas || !memoriaCompletaCache) return;
  const ctx = canvas.getContext('2d');

  const cols = 64;
  const rows = 64;
  const cellW = canvas.width / cols;
  const cellH = canvas.height / rows;

  ctx.fillStyle = '#020617';
  ctx.fillRect(0, 0, canvas.width, canvas.height);

  for (let i = 0; i < 4096; i++) {
    const col = i % cols;
    const row = Math.floor(i / cols);
    const x = col * cellW;
    const y = row * cellH;

    let corBase = '#334155';
    let pertenceFiltro = true;

    if (i < 256) {
      corBase = '#ef4444'; // ROM
      if (filtroSegmentoAtivo !== 'all' && filtroSegmentoAtivo !== 'rom') pertenceFiltro = false;
    } else if (i < 0x400) {
      corBase = '#38bdf8'; // TEXT
      if (filtroSegmentoAtivo !== 'all' && filtroSegmentoAtivo !== 'text') pertenceFiltro = false;
    } else if (i < 0x800) {
      corBase = '#10b981'; // DATA
      if (filtroSegmentoAtivo !== 'all' && filtroSegmentoAtivo !== 'data') pertenceFiltro = false;
    } else if (i < 0xC00) {
      // HEAP
      const isOcupado = (i < memoriaCompletaCache.heap_topo);
      corBase = isOcupado ? '#f59e0b' : '#3d2605';
      if (filtroSegmentoAtivo !== 'all' && filtroSegmentoAtivo !== 'heap') pertenceFiltro = false;
    } else {
      // STACK
      const isOcupado = (i >= memoriaCompletaCache.stack_sp);
      corBase = isOcupado ? '#a855f7' : '#221133';
      if (filtroSegmentoAtivo !== 'all' && filtroSegmentoAtivo !== 'stack') pertenceFiltro = false;
    }

    ctx.fillStyle = pertenceFiltro ? corBase : 'rgba(255,255,255,0.04)';
    ctx.fillRect(x, y, cellW - 0.5, cellH - 0.5);

    // Célula do MAR pulsante
    if (i === memoriaCompletaCache.mar) {
      ctx.fillStyle = '#38bdf8';
      ctx.fillRect(x - 1, y - 1, cellW + 2, cellH + 2);
    }

    // Célula selecionada pelo mouse
    if (i === celulaSelecionada) {
      ctx.fillStyle = '#ffffff';
      ctx.fillRect(x - 2, y - 2, cellW + 4, cellH + 4);
    }
  }
}

// Renderiza a listagem contínua em Hex e ASCII no modal
function renderizarHexContinuoModal() {
  const container = document.getElementById('modal-hex-view');
  if (!container || !memoriaCompletaCache) return;

  const inicio = paginaHexAtiva;
  const fim = Math.min(4096, inicio + 256); // 256 bytes por visualização

  let html = '';
  for (let r = inicio; r < fim; r += 16) {
    const offsetHex = '0x' + r.toString(16).toUpperCase().padStart(3, '0');
    let bytesHex = '';
    let charsAscii = '';

    for (let c = 0; c < 16 && (r + c) < 4096; c++) {
      const addr = r + c;
      const bHex = memoriaCompletaCache.hex.substr(addr * 2, 2);
      const bVal = parseInt(bHex, 16);
      const bChar = (bVal >= 32 && bVal <= 126) ? String.fromCharCode(bVal) : '.';

      bytesHex += bHex + ' ';
      charsAscii += bChar;
    }

    html += `
      <div class="hex-line">
        <span class="hex-offset">${offsetHex}:</span>
        <span class="hex-bytes">${bytesHex}</span>
        <span class="hex-ascii">|${charsAscii}|</span>
      </div>
    `;
  }

  container.innerHTML = html;
}

// Disparador de Aluno Demo ao vivo
let demoSeq = 100;
async function inserirAlunoDemo() {
  const nomes = ["Beatriz Costa", "Lucas Mendes", "Mariana Silva", "Gabriel Santos", "Fernanda Lima"];
  const nome = `${nomes[demoSeq % nomes.length]} (${demoSeq++})`;
  const p1 = 60 + Math.floor(Math.random() * 40);
  const p2 = 50 + Math.floor(Math.random() * 50);
  const freq = 75 + Math.floor(Math.random() * 25);

  try {
    await fetch(`${API_BASE}/api/aluno`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ nome, p1, p2, freq })
    });
    atualizarDados();
    if (modalAberto) carregarMemoriaCompleta();
  } catch (err) {
    alert('Erro ao inserir aluno demo!');
  }
}

// Tecla ESC para fechar modal
window.addEventListener('keydown', (e) => {
  if (e.key === 'Escape' && modalAberto) {
    fecharModalMapaRAM();
  }
});

