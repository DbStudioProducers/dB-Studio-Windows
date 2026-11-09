# dB Studio — plano mestre de implementação

Estado em 06/11/2026: fundação nativa JUCE 8.0.6 em desenvolvimento. Este plano organiza o escopo acordado; não declara como concluídos os módulos ausentes.

## Produto e limites
- Servidor desktop Windows/macOS como autoridade de áudio, sessões, usuários e atribuições.
- Aplicativos receptores Android e iOS.
- 32 entradas mono, 16 mixes AUX estéreo independentes e até 16 dispositivos atribuídos simultaneamente, um por AUX.
- Músico só consulta/altera seu AUX. Master autenticado administra usuários, dispositivos, AUX e Talkback.
- Nome editável sem alterar identidade estável.
- Sinal, bateria, conexão e duração somente com telemetria real; indisponível quando ausente.
- Primeira versão móvel: uma rota cabeada P2 ou USB-C, conforme o sistema. Bluetooth fora do escopo inicial.
- Talkback exclusivo do Master, PTT momentâneo, destinos selecionados e fechamento em release/desconexão/timeout.
- Cenas .dBs locais, sem sincronização automática; histórico de conexão separado.
- Multitrack desktop de até 32 entradas isoladas; WAV prioritário. FLAC/MP3 após integração e revisão de licenças.
- JUCE 8.0.6 fixo. Sem merge em main, release ou publicação sem autorização explícita.

## Fase 0 — Integridade e CI
1. Confirmar branch, PR, head SHA e ausência de merge.
2. Diagnosticar inicialização dos runners e expor falhas em logs/anotações.
3. Validar arquitetura, sintaxe Python, testes Python e laboratório UDP.
4. Compilar executáveis C++ de teste e executar CTest.
5. Somente depois compilar o desktop Windows e publicar artefato.
6. Guardar logs, artefatos, hashes e retenção para auditoria.
Aceite: execução completa, todos os testes aprovados e artefato verificável. Falha antes do runner iniciar não conta como teste de código.

## Fase 1 — Motor de áudio desktop
1. Validar seleção/troca de interfaces, canais disponíveis, taxa e bloco.
2. Suportar interfaces com menos de 32 entradas; canais inexistentes indisponíveis/silenciosos.
3. Testar 32 canais × 16 AUX, pan, ganho, MUTE, SOLO, master, clipping, silêncio e NaN/Inf.
4. Suavizar parâmetros para evitar zipper noise/clicks sem bloquear o callback.
5. Garantir callback sem alocação, I/O de rede/disco ou mutex bloqueante; medir tempo e overflow.
6. Persistir/restaurar mixes transacionalmente, com migrações e recuperação de arquivo inválido.
Aceite: testes determinísticos, callback dentro do orçamento e limite de canais real documentado.

## Fase 2 — Protocolo e transporte de áudio
1. [Em implementação] Contrato de datagrama v1: cabeçalho de 48 bytes, sessão de 128 bits, stream AUX 0–15, sequência, timestamp em frames, taxa e 128 frames estéreo float32 por pacote.
2. [Em implementação] Codificação de cabeçalho em big-endian e payload float32 little-endian, limite de 1200 bytes para evitar fragmentação IP comum.
3. Implementar sender como consumidor único da fila, com acquire/release seguro em todos os caminhos.
4. Implementar jitter buffer, reordenação, descarte de pacotes atrasados, métricas, heartbeat e reconexão.
5. Proteger áudio e controle com autenticação criptográfica; nunca confiar em IDs, role ou AUX enviados pelo cliente. O codec de pacote atual não fornece criptografia nem autenticação.
6. Manter rede e alocação fora do callback.
Aceite: testes de perda, duplicação, reordenação, jitter, MTU, replay, reconexão e isolamento de 16 streams; latência medida em hardware.

## Fase 3 — Identidade, usuários, dispositivos e Master
1. Autenticação real, armazenamento seguro, expiração e revogação.
2. Autorização server-side por ação e AUX em cada comando.
3. Descoberta/registro de dispositivos, vínculo de usuário, renomeação e atribuição atômica.
4. Até 16 atribuições exclusivas; nenhum AUX compartilhado.
5. Lista de dispositivos e telemetria reais, duração calculada pelo servidor.
6. Configuração e revogação segura do Master.
7. Histórico UTC com reconciliação de sessões interrompidas sem inventar horário.
Aceite: testes adversariais de acesso cruzado, Master forjado, replay, duplicidade e alteração de identidade.

## Fase 4 — Receptores Android e iOS
1. Projetos nativos e pipelines de build/teste independentes.
2. Onboarding, autenticação, vínculo e recuperação de sessão.
3. Receptor estéreo, jitter buffer, decoder, métricas e reconexão.
4. Saída P2/USB-C conforme rota ativa; preservar AUX na troca; sem Bluetooth.
5. Quatro bancos: 01–08, 09–16, 17–24, 25–32; faders, MUTE, SOLO, medidores, master e EQ quando suportado.
6. Interface Master com administração, atribuição, telemetria e Talkback.
7. Bateria/sinal somente quando o sistema operacional fornece dados válidos.
Aceite: integração e testes em dispositivos físicos, troca de rota, Wi-Fi perdido, background/foreground e bateria/temperatura.

## Fase 5 — Cenas .dBs
1. ZIP com manifest e JSON UTF-8 versionado.
2. Esquema comum desktop/Android/iOS para 32 canais, 16 mixes e atribuições.
3. Validar tamanho, integridade, campos e limites antes de aplicar.
4. Salvar atomicamente e aplicar somente após validação completa.
5. Save, Save As, Open, import/export manual e migrações; sem sync automático.
Aceite: vetores comuns, arquivos corrompidos, versões futuras e preservação de identidade/segredos/histórico.

## Fase 6 — Talkback
1. Rota/mix isolada.
2. Seleção de destinos e PTT momentâneo somente no Master.
3. Fechar em release, desconexão, expiração ou timeout.
4. Rejeitar comandos atrasados, duplicados ou replay.
Aceite: AUX não selecionados nunca recebem Talkback; PTT nunca fica preso.

## Fase 7 — Gravação multitrack
1. Thread e fila prealocada dedicadas.
2. Gravar até 32 entradas isoladas/alinhadas sem afetar distribuição.
3. Record/Pause/Resume/Stop, nomes, projeto, Save As e recuperação.
4. WAV PCM primeiro; FLAC/MP3 somente com encoder e licenças aprovados.
5. Tratar disco cheio, escrita falha, crash e sobrescrita.
Aceite: alinhamento, decoder independente, stress com streaming e recuperação.

## Fase 8 — Segurança e validação de palco
1. Revisar ameaças, dependências, licenças, segredos e logs.
2. Carga com 32 entradas, 16 mixes e 16 receptores.
3. Medir latência, jitter, perda, reconexão, CPU, memória, calor e autonomia.
4. Teste de longa duração e falhas.
5. Mesa/interface, access point dedicado, Android/iPhone e fones cabeados.
6. Corrigir bloqueadores e repetir a suíte antes de release candidate.

## Fase 9 — Distribuição
1. Instalador Windows assinado quando houver certificado; requisitos/drivers documentados.
2. macOS com assinatura/notarização.
3. Android/iOS conforme contas e regras das lojas.
4. Notas, hashes, matriz de compatibilidade, limitações e rollback.

## Regras de execução
- Cada fase gera commits pequenos, revisão de diff e testes específicos.
- Sempre verificar antes de compilar.
- Documentação, mocks e simulações não concluem uma fase.
- Recursos dependentes de hardware só são concluídos após teste físico.
- Distinguir falha de infraestrutura CI de falha de código; não repetir reruns sem evidência nova.
- Não fazer merge em main, release ou publicar instalador final sem autorização.