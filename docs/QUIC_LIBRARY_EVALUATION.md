# dB Studio — avaliação de bibliotecas QUIC
Data da revisão: 10/11/2026
Estado: XQUIC segue como candidata de POC, mas a compatibilidade Windows da versão atual tem uma contradição documental que precisa ser resolvida por build real. Nenhuma biblioteca QUIC foi integrada.

## Critérios
QUIC v1/TLS 1.3, QUIC DATAGRAM RFC 9221, streams confiáveis, builds reproduzíveis Windows/macOS/Android/iOS, validação de identidade, autenticação individual de dispositivo, licença compatível, manutenção ativa e nenhuma operação de rede no callback de áudio.

## Comparação
| Biblioteca | Plataformas declaradas | DATAGRAM | API/licença | Avaliação |
|---|---|---|---|---|
| XQUIC | Windows, macOS, iOS, Android | Sim, API xqc_datagram_send | C / Apache-2.0 | Candidata; Windows build atual é gate obrigatório |
| LSQUIC | Windows, macOS, Android; iOS não confirmado | Sim | C / MIT | Não prova matriz completa |
| TQUIC | Windows, macOS, iOS, Android | Não confirmado nas páginas consultadas | Rust/C/C++ / Apache-2.0 | Alternativa a avaliar |
| Apple Network.framework | Apple | QUICDatagram | Swift/Obj-C / sistema | Não cobre Windows/Android |

Fontes:
- https://github.com/alibaba/xquic
- https://github.com/alibaba/xquic/blob/v1.9.8/docs/Platforms.md
- https://github.com/alibaba/xquic/blob/v1.9.8/include/xquic/xquic.h
- https://github.com/alibaba/xquic/blob/v1.9.8/docs/API.md
- https://lsquic.readthedocs.io/en/latest/
- https://github.com/tencent/tquic
- https://tquic.net/docs/getting_started/installation/
- https://developer.apple.com/documentation/network/quicdatagram
- https://www.rfc-editor.org/rfc/rfc9221.html

## XQUIC — evidências e ressalvas
A release v1.9.8 foi publicada em 19/10/2026. README e documentação declaram QUIC v1, TLS 1.3, datagramas não confiáveis, Android/iOS/macOS/Windows, backend BoringSSL/BabaSSL e licença Apache-2.0.

**Ressalva Windows:** o arquivo docs/Platforms.md da própria tag v1.9.8 declara suporte geral às plataformas, mas preserva um aviso de que somente v1.1.0–v1.3.0 suportam Windows e que v1.4.0 removeu temporariamente esse suporte. Isso contradiz o cabeçalho e o README. Não presumir que v1.9.8 compila no Windows. A POC tenta build real no Windows x64 primeiro; se falhar por suporte de plataforma, XQUIC não passa para implementação compartilhada. Não fixar versão antiga apenas para contornar falha sem revisão de segurança/manutenção.

A API de conexão expõe no_crypto_flag. O adaptador dB Studio deve sempre forçar modo criptográfico e nunca expor opção insecure. A biblioteca possui APIs de tickets/early data: não persistir tickets e desabilitar 0-RTT na POC inicial. Provar em teste nos dois endpoints.

O callback xqc_cert_verify_pt documentado verifica certificados no cliente; isso não prova autenticação mútua do cliente no servidor. A POC deve confirmar essa capacidade no código e no teste cliente-servidor.

## Plano POC
1. Fixar tag/commit e hashes de XQUIC, backend TLS e submódulos.
2. Tentar build/endpoint mínimo Windows x64 com tag atual; depois macOS arm64, Android arm64 e iOS arm64.
3. Confirmar QUIC v1, DATAGRAM bidirecional, MSS/MTU e rejeição quando DATAGRAM não for negociado.
4. Testar certificado válido/inválido/expirado, hostname/SNI errado e servidor impostor.
5. Provar identidade individual de dispositivo no servidor, separada do handshake QUIC.
6. Desabilitar 0-RTT em ambos endpoints e testar que nenhum comando é aceito antes de handshake, autenticação e autorização.
7. Rede em thread dedicada; filas prealocadas e backpressure limitado.
8. Revisar dependências/licenças e gerar SBOM.
9. Testar perda, duplicação, reordenação, jitter, reconexão, revogação, flood, shutdown e 16 dispositivos.
10. Revisão de segurança e medições em hardware real.

## Regras
- Sem fallback em texto claro ou no_crypto.
- Nenhum comando de estado em 0-RTT.
- Handshake não equivale a autenticação nem autorização AUX.
- Servidor determina identidade, papel e AUX.
- Não transmitir áudio até handshake, autenticação e autorização.
- Falhas de certificado/autenticação/autorização/integridade fecham sessão.
- Nenhuma operação de rede no callback de áudio.

## Aprovação
Só promover a integração com builds verdes nas quatro plataformas, evidência reproduzível, testes adversariais, revisão de dependências/licenças e revisão de segurança. Até lá, transporte de produção permanece desabilitado.
