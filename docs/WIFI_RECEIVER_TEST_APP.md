# dB Studio — aplicativo receptor de bancada para validar Wi-Fi

Status: plano de validação; não representa aplicativo implementado.

## Objetivo

Desenvolver um receptor móvel mínimo em paralelo à POC de transporte QUIC para medir a transmissão de áudio de ponta a ponta antes de construir as interfaces completas Musician e Master.

O receptor de bancada é um instrumento de engenharia e não deve ser apresentado como versão final do aplicativo.

## Escopo do primeiro receptor

### Android primeiro
- Aplicativo nativo Android, inicialmente com interface mínima e instrumentada.
- Tela de conexão ao servidor, estado do handshake seguro e identificação da sessão.
- Pareamento de teste com credencial individual; não aceitar identidade, papel ou AUX escolhidos livremente pelo cliente.
- Receber somente o AUX autorizado pelo servidor.
- Receber QUIC DATAGRAM e validar versão, tamanho, sessão, sequência, timestamp, taxa e payload.
- Jitter buffer prealocado e limitado, com descarte de pacotes vencidos e sem bloquear a thread de áudio.
- Reproduzir áudio estéreo na saída cabeada ativa disponibilizada pelo Android.
- Mostrar estado da rota de áudio sem prometer preferência universal P2/USB-C.
- Painel de métricas: pacotes recebidos/perdidos/duplicados/fora de ordem, jitter, RTT, tamanho do buffer, underruns, reconexões, taxa de amostragem, latência estimada e estado de saída.
- Exportar log técnico sem áudio, senhas, tokens, chaves privadas ou dados sensíveis.

### iOS em seguida
- Criar receptor equivalente em Swift quando a POC demonstrar API e build XQUIC viáveis para iOS.
- Reutilizar o contrato de protocolo e os vetores de teste comuns.
- Expor as métricas equivalentes, respeitando as APIs de áudio e roteamento do iOS.

## Critérios de segurança

- QUIC v1 com TLS 1.3; áudio em QUIC DATAGRAM.
- Falhar fechado se certificado, identidade, autenticação ou autorização não forem válidos.
- Não permitir fallback UDP/TCP em texto claro.
- Desabilitar 0-RTT para comandos e estado.
- Não confiar em AUX, role, userId ou deviceId declarados pelo cliente.
- Não incluir segredo em QR code, log ou arquivo exportado.
- Credenciais individuais revogáveis e armazenadas em mecanismo seguro do sistema operacional.
- Nenhuma operação de rede, lock bloqueante ou alocação no callback de áudio.

## Plano de validação em camadas

1. Testes unitários do protocolo e do parser.
2. Loopback cliente-servidor QUIC em ambiente de desenvolvimento.
3. Receptor Android em emulador para integração e automação, reconhecendo que emulador não comprova latência nem rota física.
4. Dois dispositivos físicos na mesma rede Wi-Fi dedicada.
5. Testes de perda, duplicação, reordenação, jitter, interferência, mudança de IP, suspensão/retomada e reconexão.
6. Teste de carga com 1, 4, 8 e até 16 receptores, cada um vinculado a AUX exclusivo.
7. Repetir medições em hardware real e registrar configuração do access point, distância, banda, canal, taxa de amostragem e tamanho de buffer.

## Métricas e critérios de aceitação

Registrar em cada execução:
- latência fim a fim estimada e método de medição;
- RTT, jitter e distribuição de atraso;
- perda, duplicação e reordenação;
- underruns e duração de interrupções;
- tempo de pareamento, conexão e reconexão;
- CPU, memória e temperatura quando disponíveis;
- taxa de amostragem, tamanho do bloco e tamanho do jitter buffer;
- rota de saída realmente ativa.

Não inventar um limite universal de latência. Definir metas mensuráveis após a primeira baseline, considerando o uso de monitoração musical ao vivo. Comparar percentis (p50/p95/p99), não somente médias.

## Separação de escopo

- Este aplicativo de bancada não terá a interface completa Musician nem Master.
- EQ, faders, MUTE/SOLO, Talkback, cenas, histórico de usuários e administração ficam fora do primeiro instrumento.
- O controle remoto mínimo necessário para provar autorização e seleção de AUX será incluído apenas quando o protocolo seguro estiver pronto.
- Nenhum resultado de loopback ou emulador será descrito como validação física de palco.
- Não declarar transmissão funcional até validar servidor e receptor físicos em uma rede Wi-Fi real.

## Ordem de trabalho

1. Provar QUIC/TLS/DATAGRAM no desktop e no cliente de bancada.
2. Implementar a recepção e métricas antes de otimizar a interface.
3. Integrar reprodução estéreo e jitter buffer.
4. Medir baseline e ajustar packetizer/buffer.
5. Expandir para múltiplos receptores e isolamento dos 16 AUX.
6. Só então evoluir os receptores para os aplicativos Musician e Master completos.

Sempre executar testes de verificação antes de compilar. Não fazer merge em main nem publicar release sem autorização explícita.
