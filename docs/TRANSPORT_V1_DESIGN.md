# dB Studio — protocolo de transporte v1

Status: **proposta de implementação; não é uma implementação de rede funcional**.
Aplica-se ao Desktop Server, Android Receiver e futuro iOS Receiver.

## 1. Limites de responsabilidade

- JUCE renderiza 16 mixes AUX estéreo (32 canais float mono internos).
- O callback de áudio apenas publica blocos em `SpscAudioBlockQueue`; nunca chama socket, TLS, QUIC, aloca memória ou espera por rede.
- Uma thread de rede dedicada consome a fila, packetiza os canais estéreo e envia datagramas QUIC.
- O Android recebe datagramas em uma thread de rede, valida sessão/stream/timestamp e escreve PCM em um buffer de jitter limitado.
- Uma thread de áudio dedicada consome o buffer e alimenta AudioTrack. A thread de rede nunca bloqueia a thread de áudio.
- QUIC DATAGRAM é exclusivamente para áudio de baixa latência. Streams QUIC confiáveis carregam autenticação, autorização, estado, comandos e confirmações.

## 2. Transporte e segurança

- QUIC v1 sobre UDP, TLS 1.3, biblioteca fixada por commit e builds reproduzíveis.
- DATAGRAM (RFC 9221) precisa ser negociado explicitamente. Se não for negociado, a sessão não transmite áudio; não há fallback TCP/UDP em texto claro.
- 0-RTT/early data desabilitado. Comandos de controle só são aceitos após handshake completo e autenticação da aplicação.
- Certificado/identidade do servidor validado pelo cliente. Cada dispositivo tem credencial individual provisionada/pareada, revogável pelo servidor.
- A identidade do dispositivo não concede papel automaticamente. O servidor consulta cadastro e determina papel e AUX.
- O cliente nunca escolhe seu próprio AUX nem pode mudar a identidade no comando.
- Falha de validação, credencial revogada, autorização ausente, versão incompatível ou perda da sessão fecha o fluxo de áudio e desabilita controles remotos.
- Não registrar segredos, tokens, chaves privadas ou payloads de áudio em logs.

A seleção da biblioteca QUIC continua sujeita à POC, licença, SBOM e matriz de builds descritas em `QUIC_LIBRARY_EVALUATION.md`. Nenhuma biblioteca QUIC está integrada neste momento.

## 3. Fluxo de sessão

1. Cliente conecta ao endpoint configurado do Desktop Server.
2. QUIC completa TLS 1.3; o cliente valida a identidade do servidor.
3. Cliente envia mensagem `HELLO` em stream confiável: versão de protocolo, ID do dispositivo e prova de posse da credencial.
4. Servidor valida credencial, estado de revogação e política de acesso.
5. Servidor responde `WELCOME` com ID de sessão aleatório, papel atribuído, AUX autorizado (0–15 para músico; papel Master separado), parâmetros de áudio e versão.
6. Cliente confirma recebimento; somente então o servidor habilita DATAGRAM para o AUX atribuído.
7. Reautenticação/revogação, timeout ou encerramento fecha autorização e limpa buffers.
8. Reconexão cria nova sessão; datagramas de sessões anteriores são descartados.

O servidor deve rejeitar campos de AUX/role enviados pelo cliente como fonte de autorização. Se forem incluídos para compatibilidade, são apenas declarações não confiáveis e nunca substituem a atribuição do servidor.

## 4. Áudio DATAGRAM

O formato interno atual é `AudioPacketCodec.h`:
- magic `DBSA`, versão 1, cabeçalho de 48 bytes;
- session ID de 128 bits;
- stream ID 0–15, correspondente a um AUX estéreo;
- sequência uint32 por stream;
- timestamp absoluto em frames;
- sample rate uint32 e frames uint16;
- PCM float32 IEEE-754 intercalado L/R no payload lógico (codificado little-endian).

O encoder atual divide blocos em segmentos de no máximo 128 frames. A maior mensagem de aplicação atual é 48 + 128×8 = 1072 bytes. Isso **não** garante que o datagrama caiba no caminho: a camada QUIC deve consultar MSS/tamanho máximo permitido e reduzir o número de frames ou rejeitar o envio. Não fragmentar IP e não assumir MTU Ethernet fixa.

Regras de recepção:
- descartar sessão desconhecida, stream não autorizado, tamanho inválido, sample rate incompatível e amostras não finitas;
- sequência/timestamp detectam duplicação, perda e reordenação; não esperar retransmissão de áudio atrasado;
- datagramas atrasados além da janela de reprodução são descartados;
- não misturar sessões ou AUXs;
- nunca inferir métricas quando não há dados observados.

## 5. Controle confiável

Mensagens versionadas, enquadradas e limitadas por tamanho em streams confiáveis:
- `HELLO`, `WELCOME`, `REJECT`, `PING`, `PONG`;
- `SET_CHANNEL_GAIN`, `SET_CHANNEL_PAN`, `SET_CHANNEL_MUTE`, `SET_CHANNEL_SOLO`;
- `SET_AUX_MASTER`, `SET_AUX_EQ`;
- `STATE_SNAPSHOT`, `COMMAND_ACK`, `ERROR`, `REVOKE`.

Cada comando inclui ID monotônico/único, sessão, versão, alvo e valor validado. O servidor verifica papel, AUX, faixa de valores e estado da sessão, aplica o comando e devolve ACK com estado aplicado. O cliente só mostra estado confirmado após ACK; estado pendente deve ser visualmente distinto. Master é a única role autorizada a enviar Talkback e selecionar destinos permitidos.

## 6. Buffer e sincronização

- Buffer de jitter pré-alocado e limitado por frames/tempo; sem crescimento ilimitado.
- Política inicial a calibrar: alvo 20–40 ms, limite máximo configurável e descarte do pacote atrasado. Esses números são metas de ensaio, não desempenho medido.
- Medir jitter e perda por stream com relógio monotônico local; RTT por PING/PONG. Não confundir RTT com latência de áudio ponta a ponta.
- Reportar underruns, frames descartados, reordenação, p50/p95/p99 e atraso real de saída quando a plataforma permitir.
- Mudança de rota P2/USB, suspensão/resume e perda de Wi-Fi devem silenciar/recuperar sem reproduzir áudio antigo.

## 7. Sequência de implementação e gates

1. POC QUIC independente da aplicação: Windows host ↔ Android arm64 client, TLS 1.3, DATAGRAM bidirecional, streams confiáveis e certificado inválido rejeitado.
2. Provar identidade de dispositivo, revogação, 0-RTT desabilitado e autorização AUX server-side.
3. Integrar thread de rede do Desktop à fila SPSC e ao encoder; validar shutdown, backpressure e ausência de alocação/locks no callback.
4. Implementar receiver Android, buffer de jitter e AudioTrack com áudio remoto; controles só após ACK.
5. Testes automatizados de perda, duplicação, reordenação, jitter, reconexão, pacotes malformados, flood, revogação e concorrência.
6. Escalar 1/4/8/16 dispositivos, medir CPU/memória/temperatura/underruns e documentar AP Wi-Fi usado.
7. Portar o mesmo protocolo para iOS e repetir a matriz.
8. Revisão independente de segurança/licenças/SBOM e validação física ponta a ponta.

## 8. Estado real atual

Implementado no repositório: renderização de 16 AUX estéreo; fila SPSC pré-alocada; codec/packetizer v1 e testes C++; shell Android com teste local de tom e layout Live Control em evolução.

Ainda não implementado: QUIC client/server, TLS/identidade de dispositivo, pareamento, autorização de AUX em rede, sender/receiver de áudio, jitter buffer, áudio remoto, canal de controle, app iOS e validação Wi-Fi física.

**Não distribuir este estado como monitor in-ear funcional.** A aprovação da CI Android só comprova que o APK compila.
