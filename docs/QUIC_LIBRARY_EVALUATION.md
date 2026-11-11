# dB Studio — avaliação de bibliotecas QUIC
Data da revisão: 11/11/2026
Estado: nenhuma biblioteca foi integrada. XQUIC v1.9.8 foi reprovada como base Windows. LSQUIC é a candidata técnica mais forte: o README v4.11.1 lista Windows x86_64, macOS x86_64, iOS ARM e Android ARM como plataformas testadas; a página Getting Started omite iOS, então a matriz iOS exige build independente. TQUIC permanece alternativa para QUIC confiável, mas a revisão do código não encontrou suporte de aplicação a QUIC DATAGRAM RFC 9221.

## Critérios eliminatórios
- QUIC v1 e TLS 1.3 com validação explícita de identidade do servidor.
- DATAGRAM RFC 9221 negociado para áudio não confiável; streams confiáveis para controle.
- Builds e testes reproduzíveis em Windows, macOS, Android e iOS, ou adaptadores interoperáveis com evidência de interoperabilidade.
- 0-RTT desabilitado no perfil inicial.
- Autenticação de dispositivo e autorização AUX na camada dB Studio; handshake TLS não substitui essas funções.
- Licença compatível, dependências fixadas, SBOM e processo de atualização de segurança.
- Nenhuma operação de rede no callback de áudio.

## Matriz atualizada

| Biblioteca | Plataformas documentadas | QUIC DATAGRAM RFC 9221 | TLS/0-RTT | Licença e decisão |
|---|---|---|---|---|
| XQUIC v1.9.8 | README menciona Windows/macOS/iOS/Android; Platforms.md alerta que Windows só foi suportado em v1.1.0–v1.3.0 | Sim | TLS 1.3; 0-RTT deve ser desabilitado | Apache-2.0; reprovada para POC Windows atual até existir evidência oficial/build atual |
| LSQUIC 4.11.1 | Windows x86_64, macOS x86_64, iOS ARM e Android ARM listados no README; Getting Started omite iOS | Sim; es_datagrams e callbacks on_datagram/on_dg_write | QUIC v1/v2; requer validação TLS e configuração explícita de 0-RTT | MIT; candidata primária para POC Windows/Android/macOS |
| TQUIC develop | Windows, macOS, iOS, Android, Linux e outros alvos Rust | Não confirmado; o código consultado anuncia max_datagram_frame_size=None e a API C não expõe envio/recepção de DATAGRAM de aplicação | TLS configurável; API permite ativar/desativar early data | Apache-2.0; não atende atualmente ao requisito de áudio DATAGRAM |
| Apple Network.framework | Plataformas Apple | API QUICDatagram | TLS integrado ao sistema; política 0-RTT precisa ser validada | Framework do sistema; possível adaptador iOS/macOS, sujeito a teste de interoperabilidade |
| MsQuic | Suporte oficial Windows x64/arm64 e Linux; macOS/iOS/Android são best-effort ou sem garantia | Implementação QUIC genérica; DATAGRAM precisa de prova na versão selecionada | Schannel no Windows requer Windows 11/Server 2022 para TLS 1.3; OpenSSL pode ampliar compatibilidade para Windows 10 | MIT; não cobre a matriz móvel de forma suportada |

## Evidências e conclusões

### XQUIC — bloqueada para Windows
A documentação oficial da tag v1.9.8 diz que a biblioteca suporta Windows no resumo, mas a seção Windows preserva o aviso de que apenas v1.1.0–v1.3.0 suportam Windows e que v1.4.0 removeu temporariamente esse suporte. Isso é uma contradição objetiva. Não integrar nem escolher versão antiga apenas para fazer o build passar; exigir confirmação dos mantenedores, build reprodutível e revisão de segurança da versão.

### LSQUIC — candidata primária, ainda não aprovada
A documentação atual 4.11.1, atualizada em 03/11/2026, documenta QUIC v1/v2, extensão DATAGRAM e licença MIT. A API permite habilitar es_datagrams e usar callbacks on_datagram/on_dg_write. O README da tag v4.11.1 lista Windows x86_64, macOS x86_64, iOS ARM e Android ARM como plataformas testadas. A página Getting Started lista Windows, macOS, FreeBSD e Android, omitindo iOS; essa divergência exige um build iOS independente antes de considerar a matriz aprovada. Dependências incluem zlib, BoringSSL e submódulos ls-hpack/ls-qpack. O socket e o event loop são fornecidos pelo integrador, o que permite uma thread de rede dedicada, mas exige projeto cuidadoso de callbacks, buffers e ciclo de vida.

Próximo gate: build Windows x64 com versão/tag fixada; teste de cliente-servidor real com DATAGRAM negociado; certificados e hostname; prova de rejeição de certificado inválido/expirado e servidor impostor; inspeção de 0-RTT; build Android, macOS e iOS ARM. Não assumir que o status “production-ready” do projeto equivale à aprovação para este produto.

### TQUIC — boa matriz de plataformas, DATAGRAM de aplicação não comprovado
A documentação declara Windows, macOS, iOS e Android e fornece FFI C. Entretanto, o cabeçalho C consultado não apresenta métodos de envio/recepção de DATAGRAM de aplicação, e a configuração de parâmetros de transporte define max_datagram_frame_size como None. O uso de “datagram” no código se refere principalmente a pacotes UDP/PMTU, não prova o frame QUIC DATAGRAM RFC 9221. Não selecionar para o caminho de áudio sem implementação e testes explícitos desse recurso. O cabeçalho expõe opções de early data; o adaptador deve desabilitá-lo e provar isso em ambos os endpoints.

### MsQuic — alternativa de desktop, não solução única
A documentação oficial lista Windows e Linux como plataformas suportadas, enquanto macOS/iOS/Android não têm garantia de suporte. No Windows, Schannel exige Windows 11 ou Server 2022 para TLS 1.3; a configuração OpenSSL pode funcionar em versões Windows 10 mais amplas. Isso não atende sozinho à matriz do dB Studio.

### Estratégia de interoperabilidade a investigar
Se LSQUIC não puder ser compilada/operada no iOS, avaliar Apple Network.framework como adaptador alternativo para iOS/macOS e provar interoperabilidade com LSQUIC no mesmo protocolo QUIC v1/TLS 1.3 + RFC 9221. Só aceitar essa arquitetura após teste de interoperabilidade cruzada, negociação de ALPN e parâmetros DATAGRAM, certificado/identidade, limites MTU e reconexão. Não criar protocolo de áudio proprietário em UDP nem fallback em texto claro.

## Plano de POC revisado
1. Fixar versão/tag/commit e hashes de LSQUIC, BoringSSL, zlib e submódulos; gerar SBOM.
2. Fixar LSQUIC v4.11.1 no commit `2f4cd42f27cf8e9b0a0f43e376a618ec8ce4a7ff`, BoringSSL `0.20250807.0` e vcpkg no commit `e54ef0fcc9a82033a61e51002552a910a5d57282`; compilar LSQUIC no Windows x64 e executar testes unitários upstream.
3. Criar cliente e servidor mínimos separados, sem integração no aplicativo principal.
4. Provar QUIC v1, TLS 1.3, ALPN próprio, RFC 9221 DATAGRAM bidirecional e streams confiáveis.
5. Provar falha fechada para certificado inválido, expirado, hostname/SNI errado, servidor impostor e identidade de dispositivo não autorizada.
6. Desabilitar 0-RTT; testar replay e garantir que nenhum comando seja processado antes de autenticação e autorização.
7. Testar limite de DATAGRAM, MTU/PMTU, perda, duplicação, reordenação, jitter, NAT rebinding, troca de IP, suspensão/retomada e reconexão.
8. Testar Android, macOS e iOS ARM; manter Apple Network.framework como alternativa somente se a compilação/integração LSQUIC em iOS falhar.
9. Integrar apenas depois de evidência reproduzível, revisão de licença/dependências, SBOM, fuzzing e revisão de segurança.

## Regras de segurança do produto
- Sem fallback em texto claro, no_crypto ou downgrade silencioso.
- Nenhum comando de estado em 0-RTT.
- Handshake QUIC/TLS não equivale a login, autenticação de dispositivo ou autorização AUX.
- Servidor determina identidade, papel, dispositivo e AUX permitido.
- Não transmitir áudio até handshake, autenticação e autorização concluídos.
- Falhas de certificado, autenticação, autorização ou integridade fecham a sessão.
- Dados de áudio em DATAGRAM; comandos, configuração e estado em streams confiáveis.
- Thread de rede separada; callback de áudio sem rede, bloqueio, alocação, logging ou I/O.
- Filas limitadas, descarte controlado, sequência/timestamp e proteção contra replay na camada de aplicação.

## Fontes primárias
- XQUIC v1.9.8 Platforms: https://github.com/alibaba/xquic/blob/v1.9.8/docs/Platforms.md
- LSQUIC v4.11.1 release: https://github.com/litespeedtech/lsquic/releases/tag/v4.11.1\n- LSQUIC v4.11.1 commit: https://github.com/litespeedtech/lsquic/commit/2f4cd42f27cf8e9b0a0f43e376a618ec8ce4a7ff\n- LSQUIC docs 4.11.1: https://lsquic.readthedocs.io/en/latest/
- LSQUIC README e plataformas (inclui iOS ARM): https://github.com/litespeedtech/lsquic/blob/v4.11.1/README.md\n- LSQUIC Getting Started: https://lsquic.readthedocs.io/en/latest/gettingstarted.html
- LSQUIC API/datagrams: https://lsquic.readthedocs.io/en/latest/apiref.html
- TQUIC platforms/build: https://tquic.net/docs/getting_started/installation/
- TQUIC API header: https://github.com/Tencent/tquic/blob/develop/include/tquic.h
- TQUIC latest release: https://github.com/Tencent/tquic/releases
- MsQuic platform support: https://microsoft.github.io/msquic/msquicdocs/docs/Platforms.html
- MsQuic release/platform guarantees: https://microsoft.github.io/msquic/msquicdocs/docs/Release.html
- Apple QUIC datagrams: https://developer.apple.com/documentation/network/quicdatagram
- RFC 9221: https://www.rfc-editor.org/rfc/rfc9221.html

## Aprovação
Nenhuma biblioteca está aprovada para produção. A LSQUIC só passa a candidata de integração após build/testes reproduzíveis e avaliação de iOS. O transporte de produção permanece desabilitado até a matriz multiplataforma, segurança, interoperabilidade e testes de carga serem aprovados.
