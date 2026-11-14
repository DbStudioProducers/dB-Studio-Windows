# dB Studio — arquitetura de segurança e transporte

Estado: especificação obrigatória antes de habilitar transporte de produção. Não representa recursos já implementados.

## 1. Objetivos e modelo de ameaça

Proteger confidencialidade, integridade, autenticidade, disponibilidade e isolamento entre músicos/dispositivos, considerando:
- clientes não confiáveis na mesma rede Wi-Fi;
- dispositivos perdidos, roubados, comprometidos ou revogados;
- tentativa de assumir a identidade do Master ou de outro músico;
- captura, repetição, atraso, duplicação, reordenação, truncamento e adulteração de pacotes;
- mensagens malformadas, tamanhos excessivos, flood, exaustão de memória/CPU e reconexões em loop;
- perda de Wi-Fi, mudança de endereço IP, suspensão do aplicativo e reinício do servidor.

A rede local não é uma fronteira de confiança. SSID, endereço IP, MAC, nome do dispositivo, session ID e AUX enviados pelo cliente nunca provam identidade ou autorização.

## 2. Transporte padrão — não criar criptografia própria

### 2.1 Protocolo
- Usar QUIC v1 (RFC 9000), com TLS 1.3 conforme RFC 9001.
- Usar QUIC DATAGRAM (RFC 9221) para áudio em tempo real: entrega não confiável, sem retransmitir áudio vencido.
- Usar streams QUIC para autenticação pós-handshake, configuração, atribuição AUX, telemetria, heartbeat, Talkback e demais comandos que exigem entrega ordenada/confiável.
- Não criar protocolo próprio de troca de chaves, cifra, MAC, nonce ou handshake.
- Não permitir fallback para UDP/TCP em texto claro. Se a negociação segura falhar, a conexão falha fechada.
- Desabilitar TLS 1.3 0-RTT para comandos, autenticação, alteração de AUX, Talkback e qualquer operação com efeito. Não aceitar replay de operações de estado.
- Fixar versões e parâmetros de protocolo; rejeitar versões desconhecidas sem tentar interpretar payloads.

A biblioteca QUIC concreta deve ser escolhida por uma prova de compatibilidade auditável para Windows/macOS/Android/iOS, manutenção ativa, suporte a TLS 1.3 e QUIC DATAGRAM, histórico de segurança, licenciamento e possibilidade de atualização. Não implementar criptografia ou QUIC manualmente.

### 2.2 MTU e payload
- Respeitar o limite de pacote QUIC/UDP de 1200 bytes e evitar fragmentação IP.
- O payload atual do codec de áudio v1 (48 bytes de cabeçalho + até 1024 bytes de áudio = 1072 bytes) cabe como payload de aplicação, mas a implementação deve contabilizar também os cabeçalhos QUIC e o frame DATAGRAM.
- Validar o tamanho final antes do envio; se a biblioteca não conseguir transportar o datagrama sem fragmentação, reduzir o número de frames por pacote em uma versão negociada do protocolo.
- Não aumentar limites com base em dados recebidos do cliente.

## 3. Identidade, pareamento e credenciais

1. O servidor gera um código de pareamento aleatório criptograficamente seguro, de uso único, curta validade e limitado por tentativas.
2. O código vincula um dispositivo a uma conta/usuário após confirmação explícita no desktop. QR code contém somente material temporário de pareamento, nunca senha ou chave privada.
3. Cada dispositivo gera sua própria chave assimétrica localmente. A chave privada nunca é enviada ao servidor e deve ser armazenada no Keychain (Apple), Android Keystore ou Windows CNG/DPAPI, conforme plataforma.
4. O servidor emite uma credencial/certificado de cliente vinculada ao identificador estável do dispositivo e à conta. O cliente valida a identidade do servidor por certificado confiável/pinning com rotação planejada; nunca desabilitar validação TLS para “resolver” conexão.
5. Credenciais devem ser individuais, expiradas e revogáveis. Revogar um dispositivo encerra sessões ativas e impede novo handshake com aquela credencial.
6. O papel Master é uma autorização mantida pelo servidor, não um campo confiável enviado pelo cliente. Deve existir um único Master ativo por instalação/sessão, com recuperação administrativa segura.
7. Senhas, caso usadas no login, trafegam somente dentro do canal TLS 1.3 autenticado. Armazenar somente hashes Argon2id com salt individual e parâmetros versionados; nunca texto claro, hash rápido ou senha em log. Preferir MFA/passkey para ações administrativas sensíveis.
8. Limitar tentativas, aplicar atraso progressivo e auditoria sem registrar segredos. Não bloquear indefinidamente contas por uma origem não confiável.

## 4. Autorização e isolamento AUX

- O servidor resolve a identidade autenticada a partir da credencial da conexão e consulta seu estado atual.
- Para cada comando, verificar usuário, dispositivo, papel, sessão ativa, estado de revogação, AUX atribuído, alvo, versão e limites.
- Músico pode ler/alterar exclusivamente o AUX atribuído. Alterar o campo targetAux no cliente não altera a autorização.
- Somente Master pode renomear usuários/dispositivos, atribuir AUX, visualizar/administrar dispositivos, escolher destinos e acionar Talkback.
- Atribuições AUX devem ser atômicas e exclusivas; rejeitar conflito e estado inconsistente.
- Mudança de atribuição invalida imediatamente permissões/sessões antigas e notifica os dispositivos afetados.
- Não confiar em role, userId, deviceId, sessionId, streamId ou auxId declarados em payload sem cruzar com o contexto autenticado mantido no servidor.
- Aplicar negação por padrão para ações desconhecidas, versões desconhecidas e campos extras proibidos.

## 5. Áudio em tempo real

- Um QUIC connection autenticado por dispositivo; servidor envia somente o AUX autorizado para aquela conexão.
- Um stream lógico por AUX estéreo, com contador de sequência e timestamp de frames monotônicos; não usar relógio de parede para sincronização de áudio.
- O codec atual é um formato de payload, não um mecanismo de segurança. Seu session ID não é credencial nem segredo. O pacote v1 só pode circular dentro do QUIC autenticado e criptografado.
- A proteção de confidencialidade, integridade e anti-replay no transporte é fornecida pelo QUIC/TLS; não adicionar uma segunda cifra caseira.
- O receptor valida sessão/época, stream autorizado, taxa de amostragem, número de frames, tamanho exato, valores finitos e timestamp antes de enfileirar.
- Jitter buffer prealocado e limitado por tempo/memória. Pacotes duplicados, muito atrasados, fora da janela ou de sessão anterior são descartados.
- Áudio perdido não é retransmitido: aplicar política explícita de concealment/silêncio e contabilizar perda. Nunca bloquear o callback aguardando rede.
- Mudança de rota, reconexão ou nova sessão cria uma nova época; pacotes antigos não podem ser aceitos na sessão nova.
- Contadores de sequência devem tratar wraparound sem ambiguidade, usando janela e comparação modular definidas e testadas.
- Clock drift e jitter devem ser medidos; ajuste de buffer/reamostragem deve ser gradual e limitado.

## 6. Mensagens de controle

- Mensagens versionadas, com tipo, comprimento limitado, request ID aleatório/único, sessão e campos obrigatórios definidos por schema.
- Parsing com limites rígidos antes de alocar ou copiar; rejeitar overflow, truncamento, enum inválido, campos duplicados, strings inválidas e payloads excessivos.
- Comandos de alteração usam request ID/idempotency key e estado esperado/revision para evitar execução duplicada ou atualização concorrente perdida.
- Comandos sensíveis são executados uma única vez por sessão; duplicatas retornam o resultado anterior ou são rejeitadas, nunca repetem o efeito.
- Heartbeat tem timeout e limites; timeout fecha sessão e desarma Talkback.
- Talkback é lease momentâneo do servidor: desliga ao soltar PTT, perder conexão, expirar lease ou detectar estado inválido. O cliente não pode manter Talkback ativo apenas deixando de enviar release.
- Telemetria não disponível deve ser marcada como indisponível, nunca inventada.

## 7. Concorrência e áudio em tempo real

- Callback de áudio não executa sockets, TLS/QUIC, locks bloqueantes, logging, disco, alocação dinâmica ou espera.
- Fila SPSC com produtor e consumidor únicos, buffers prealocados, política explícita de overflow e métricas atômicas.
- Consumidor de rede usa RAII/guard para garantir release do bloco em todos os caminhos de retorno/erro.
- Encerrar o serviço em ordem: impedir novas sessões, parar/join da thread de rede, liberar leases, fechar conexões e só então destruir motor/fila.
- Nunca destruir buffers enquanto consumidor os mantém adquiridos.
- Backpressure: descartar blocos de áudio antigos de forma controlada e contar perdas; nunca deixar fila crescer sem limite.
- Erros de rede não podem propagar exceções ou bloquear a thread de áudio.

## 8. Disponibilidade, limites e observabilidade

- Limites por IP/dispositivo/conta para handshakes, pareamento, mensagens, conexões simultâneas, tamanho e frequência.
- Limite global de conexões e memória; timeout de handshake e leitura; cancelamento limpo.
- Backoff exponencial com jitter para reconexão; impedir loops rápidos e tempestades de reconexão.
- Métricas: perda, atraso, jitter, reordenação, duplicatas, RTT, reconexões, filas cheias, underruns/overruns e latência fim a fim.
- Logs estruturados sem áudio, senha, token, chave privada, código de pareamento ou payload sensível. IDs devem ser pseudonimizados quando possível.
- Falha de autenticação, autorização ou integridade fecha/rejeita a sessão; nunca degrada para modo inseguro.

## 9. Dependências e cadeia de fornecimento

- Preferir biblioteca QUIC/TLS madura e mantida; fixar versão/hash e acompanhar advisories.
- Inventário de dependências (SBOM), licenças, CVEs e política de atualização.
- Builds reproduzíveis quando possível; artefatos com SHA-256; assinatura de instaladores e atualizações.
- Segredos de assinatura fora do repositório e dos logs; menor privilégio nos workflows.
- Revisar ações GitHub de terceiros e fixar por SHA completo quando praticável.
- Não implementar primitives criptográficas próprias nem copiar código criptográfico sem revisão especializada.

## 10. Plano obrigatório de testes de segurança e robustez

### Unitários/property/fuzz
- Todos os campos nos limites mínimo/máximo, overflow aritmético, comprimentos inconsistentes, enums inválidos, NaN/Inf, timestamp e sequência extremos.
- Fuzzing dos parsers de controle e codec com ASan/UBSan quando suportado.
- Property tests para encode/decode, atomicidade de falha e ausência de escrita fora dos buffers.
- Testes de wraparound de sequência, janela anti-replay, sessão antiga e troca de época.

### Transporte e falhas
- Perda, duplicação, reordenação, jitter, atraso extremo, datagramas truncados e MTU reduzida.
- Wi-Fi desligado/ligado, mudança de IP, suspensão/retomada, servidor reiniciado, cliente reiniciado e certificado expirado/revogado.
- Handshake incompleto, downgrade, certificado inválido, servidor impostor, cliente não pareado e tentativa de replay.
- Flood, mensagens gigantes, reconexão em loop, exaustão de filas e encerramento durante envio.
- Garantir que um cliente nunca receba outro AUX, inclusive após reassociação e reconexão.
- Garantir que comando duplicado não duplique ação e que comando atrasado não reative Talkback.

### Aceitação
- Nenhum transporte de produção antes de handshake TLS 1.3/QUIC autenticado e validação server-side.
- Nenhum acesso cruzado em testes adversariais.
- Nenhuma alocação/lock/I/O de rede no callback.
- Testes de carga com 16 dispositivos/16 AUX e 32 entradas; medir latência, CPU, memória, jitter e perda em hardware real.
- Revisão independente de segurança antes de release candidate. Simulador e CI não substituem teste físico nem auditoria.

## 11. Estado atual e bloqueios explícitos

- O codec AudioPacketCodec.h implementa apenas serialização/validação do payload. Não criptografa, autentica remetente, autentica sessão nem impede replay sozinho.
- SessionAuthorization.h implementa política de autorização para uma sessão já autenticada. Não autentica senhas nem cria credenciais.
- A fila de áudio e o packetizer não constituem transporte de rede.
- O laboratório UDP Python é simulação e não pode ser promovido a transporte de produção.
- Não enviar áudio ou comandos sensíveis por UDP/TCP sem proteção QUIC/TLS 1.3.
- A biblioteca QUIC multiplataforma ainda precisa de prova de compatibilidade e avaliação de dependência antes da integração.
