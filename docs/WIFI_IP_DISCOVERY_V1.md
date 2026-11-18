# dB Studio — descoberta IP e conexão Wi-Fi v1

Status: especificação de implementação. Não representa descoberta, associação Wi-Fi ou transporte de áudio já funcionais.

## 1. Modelo de rede

O dB Studio opera em uma LAN privada fornecida por um ponto de acesso/roteador Wi-Fi dedicado. O Desktop Server e os receptores conectam-se ao mesmo SSID. Internet/WAN não é requisito para operação; o roteador continua necessário para fornecer o enlace Wi-Fi e encaminhamento IP local.

O software não configura automaticamente qualquer roteador, não cria um hotspot universal e não substitui o driver Wi-Fi do sistema operacional. A criação de hotspot pelo próprio computador poderá ser avaliada por plataforma, mas não faz parte do requisito mínimo v1.

Topologia lógica:

```
Mixer/interface -> Desktop Server (mixagem + autoridade)
                         |
                  LAN Wi-Fi privada
                         |
             +-----------+-----------+
             |           |           |
          AUX 01       AUX 02      AUX 16
          Receiver     Receiver    Receiver
```

O áudio nunca é enviado por broadcast/multicast: cada sessão QUIC autenticada recebe somente o stream unicast do AUX autorizado.

## 2. Endereçamento IP

- IPv4 é obrigatório para a primeira implantação; IPv6 pode ser usado quando disponível e testado.
- O servidor escuta em interfaces LAN selecionadas, não apenas em loopback e nunca expõe o serviço na interface WAN por padrão.
- DHCP do roteador atribui endereços. Reserva DHCP para o Desktop Server é recomendada para instalações fixas, mas o cliente não depende de IP fixo.
- Não codificar sub-rede, gateway, DNS ou endereço do servidor no aplicativo.
- O cliente descobre o endpoint em tempo de execução e valida a identidade criptográfica do servidor após conectar.
- Endereço IP/porta são localizadores mutáveis, não identidade/autorização. Mudança de IP exige nova descoberta/conexão, mas não novo vínculo de dispositivo.

## 3. Descoberta local

Serviço recomendado: DNS-SD/mDNS com nome de serviço `_dbstudio._udp.local`.

Registros:
- PTR: instância do Desktop Server.
- SRV: porta UDP QUIC de escuta.
- TXT: versão de protocolo, identificador público aleatório da instalação, estado de disponibilidade e capacidade declarada.
- A/AAAA: endereços anunciados pelo host.

TXT nunca contém senha, token, chave privada, segredo de pareamento, nome pessoal, lista de músicos ou estado sensível.

O servidor anuncia somente enquanto o serviço estiver pronto e retira o anúncio no encerramento. O cliente lista instâncias encontradas, trata duplicatas/expiração e tenta conexão QUIC ao endpoint SRV. O identificador anunciado é apenas um hint; não prova identidade. A validação de certificado/identidade ocorre no handshake QUIC/TLS 1.3.

Fallbacks:
1. Código QR gerado pelo Desktop Server contendo endpoint LAN temporário + identificador público de pareamento de uso único; nunca incluir credencial permanente.
2. Entrada manual de IP/porta em rede sem mDNS.
3. Falha de mDNS não deve abrir portas inseguras nem iniciar áudio em texto claro.

O código de pareamento deve expirar, ser de uso único, ter entropia criptográfica, limite de tentativas e revogação. A confirmação final do dispositivo ocorre no canal QUIC autenticado.

## 4. Portas e firewall

- Porta UDP QUIC configurável; a implementação deve publicar a porta efetivamente ativa no registro DNS-SD.
- Não assumir porta fixa até a configuração de serviço ser implementada e documentada.
- Windows Firewall: permitir entrada UDP somente no perfil de rede Privado e somente para o executável/porta do servidor; nunca criar regra ampla para redes Públicas.
- Não exigir encaminhamento de portas no roteador, UPnP ou exposição à internet.
- Não abrir portas de administração HTTP, Telnet, SSH ou descoberta de terceiros.
- O tráfego de descoberta mDNS usa multicast local apenas para localizar o serviço; áudio, autenticação e controle são unicast QUIC.

## 5. Requisitos do ponto de acesso

Configuração inicial recomendada para validação:
- SSID dedicado ao áudio, WPA2-AES ou WPA3-Personal; senha forte e exclusiva.
- Banda 5 GHz preferencial quando cobertura e dispositivos permitirem; 2.4 GHz somente quando necessário e validado no local.
- Desativar isolamento de clientes/AP isolation para permitir tráfego entre servidor e receptores.
- Desativar portal cativo, rede de convidados isolada, repetidores instáveis e otimizações que bloqueiem multicast local.
- DHCP ativo, pool suficiente para servidor + até 16 receptores + margem para administração.
- Posicionar AP próximo aos músicos, com canal RF escolhido após inspeção local. Não fixar canal/largura como universal.
- Internet/WAN pode permanecer desconectada. Não conectar a rede de monitoração à LAN corporativa sem revisão de segurança.
- Preferir AP dedicado de qualidade; desempenho depende de rádio, interferência, distância, antenas, chipset e drivers.

Esses parâmetros são requisitos de configuração/ensaio, não garantia de latência. Wi-Fi compartilhado e congestionado não pode ser certificado apenas por CI.

## 6. Sessão e fluxo

1. Desktop inicia motor e serviço de rede em thread separada; callback de áudio não realiza descoberta, socket, TLS, QUIC ou alocação.
2. Serviço verifica interfaces ativas, seleciona endereços LAN elegíveis e inicia listener QUIC.
3. Servidor anuncia serviço DNS-SD/mDNS com porta real.
4. Cliente encontra instância e solicita conexão QUIC.
5. TLS 1.3 valida certificado/identidade do servidor; QUIC DATAGRAM deve ser negociado explicitamente.
6. Cliente apresenta identidade e prova de posse de credencial provisionada. Pairing é processo separado, autenticado e de uso único.
7. Servidor consulta cadastro, revogação e role. Nunca aceita role/AUX escolhidos pelo cliente.
8. Servidor retorna sessão, role e AUX autorizados em mensagem confiável.
9. Somente após autenticação, autorização e confirmação o servidor envia DATAGRAM unicast do AUX atribuído.
10. Heartbeat/timeout detecta perda de sessão. Revogação ou falha fecha fluxo, limpa jitter buffer e invalida comandos pendentes.
11. Reconexão executa nova validação; pacotes de sessões antigas são descartados.

## 7. Segurança de rede

- QUIC v1 + TLS 1.3; sem fallback em texto claro, sem 0-RTT para comandos ou dados de sessão.
- Credencial única por dispositivo; armazenamento protegido pelo SO; revogação pelo Master/servidor.
- Certificado/identidade do servidor validado; não confiar em hostname/IP/mDNS como autenticação.
- Limites de tamanho, taxa, tentativas de pareamento, conexões simultâneas e filas.
- Parsers defensivos; descartar datagramas malformados, sessão desconhecida, AUX não autorizado, sequência antiga e amostras não finitas.
- Controle confiável com ID único, sessão, versão, validação de faixa e ACK.
- Músico recebe exclusivamente o próprio AUX. Talkback é roteado pelo servidor ao canal designado dos AUX selecionados pelo Master; só role Master pode selecionar destinos ou operar PTT.
- Logs sem credenciais, tokens, chaves, payloads PCM ou dados pessoais desnecessários.
- Encerramento seguro em perda de rede, suspensão, troca de interface, revogação e falha do processo.

## 8. Gestão de interfaces e mudanças de rede

- Detectar interfaces de rede elegíveis e seus endereços; ignorar loopback, VPN e interfaces WAN por padrão, salvo seleção explícita futura.
- Se múltiplas LANs estiverem ativas, permitir seleção explícita no Desktop Server; nunca anunciar indiscriminadamente em todas as interfaces.
- Mudança de endereço, roaming, troca de AP, suspensão/resume ou perda de link invalida o endpoint antigo e dispara reanúncio/reconexão.
- Não migrar sessão de forma implícita se a biblioteca QUIC/implementação não suportar migração segura validada.
- Estado visual deve distinguir: servidor local pronto, anúncio ativo, cliente descoberto, handshake, autenticado, stream ativo, reconectando e desconectado. Não mostrar “conectado” antes de confirmação do servidor.

## 9. Critérios de teste

Automatizados:
- Parser de mensagens de descoberta e TXT com tamanhos/valores inválidos.
- Múltiplas instâncias, duplicatas, TTL/expiração, ausência de mDNS e fallback manual.
- Endereços IPv4/IPv6, múltiplas interfaces, IP alterado e porta dinâmica.
- Cliente não autorizado, certificado inválido, credencial revogada, replay, flood e conexão excedente.
- Verificação de que áudio não é enviado antes de WELCOME/ACK e nunca em broadcast.
- Isolamento de AUX e permissões Master/Musician, inclusive comandos forjados.

Físicos obrigatórios:
- Desktop Windows + AP dedicado + 1, 4, 8 e 16 Androids; iOS em fase própria.
- Testar distância, obstáculos, interferência, congestionamento, roaming, perda do AP, reinício do roteador e reconexão.
- Medir perda, jitter, underruns, p50/p95/p99, atraso de áudio ponta a ponta, CPU, memória, temperatura e estabilidade.
- Registrar modelo/firmware do AP, banda/canal/largura, dispositivos, distância e ambiente RF.

## 10. Limite de implementação atual

A especificação DNS-SD/mDNS, endereçamento e operação do AP não significa que o serviço esteja implementado. No estado atual do repositório não há servidor QUIC, anúncio mDNS, descoberta Android, pareamento, autenticação de rede, receiver QUIC ou reprodução de áudio remoto. Esses itens precisam de código, testes automatizados e validação física antes de declarar a conexão Wi-Fi pronta.
