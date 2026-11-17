# dB Studio — avaliação de bibliotecas QUIC

Data da avaliação: 10/11/2026  
Estado: decisão provisória para prova de conceito; nenhuma biblioteca foi integrada ao produto.

## Critérios obrigatórios

A biblioteca precisa oferecer QUIC v1 com TLS 1.3, QUIC DATAGRAM (RFC 9221), streams confiáveis, builds verificáveis para Windows/macOS/Android/iOS, validação de certificado do servidor, caminho seguro para identidade individual dos dispositivos, licença compatível, manutenção ativa e API que permita manter todo o trabalho de rede fora do callback de áudio.

Uma página de suporte a plataforma não substitui um build real em cada SDK/arquitetura. Uma API de validação de certificado do servidor também não prova, por si só, autenticação mútua de certificados de cliente.

## Comparação documental

| Biblioteca | Plataformas declaradas | DATAGRAM RFC 9221 | Linguagem/API | Licença | Avaliação |
|---|---|---|---|---|---|
| XQUIC | Windows, macOS, iOS, Android | Sim; documentação apresenta callbacks, MSS e xqc_datagram_send | C | Apache-2.0 | Melhor candidata para a primeira prova de conceito |
| LSQUIC | Windows, macOS, Android; iOS não confirmado na matriz oficial consultada | Sim | C | MIT | Não atende ainda ao requisito de uma implementação compartilhada nas quatro plataformas |
| TQUIC | Windows, macOS, iOS, Android | Não confirmado explicitamente nas páginas oficiais consultadas | Rust com APIs Rust/C/C++ | Apache-2.0 | Alternativa para comparação; confirmar DATAGRAM e sua configuração |
| Apple Network.framework | Plataformas Apple | QUICDatagram documentado | Swift/Objective-C | API do sistema | Não resolve o servidor Windows nem os clientes Android |

Fontes primárias consultadas:
- XQUIC: https://github.com/alibaba/xquic
- XQUIC: https://doc.xquic.org.cn/en/tutorials/what_is_xquic.html
- XQUIC API: https://doc.xquic.org.cn/markdown/Files/xquic_8h.html
- XQUIC plataformas: https://github.com/alibaba/xquic/blob/main/docs/Platforms.md
- LSQUIC: https://lsquic.readthedocs.io/en/latest/
- TQUIC: https://github.com/tencent/tquic
- TQUIC instalação: https://tquic.net/docs/getting_started/installation/
- Apple QUICDatagram: https://developer.apple.com/documentation/network/quicdatagram
- RFC 9221: https://www.rfc-editor.org/rfc/rfc9221.html

## Decisão provisória

**XQUIC será a primeira biblioteca avaliada em uma prova de conceito isolada.** A escolha é apenas para POC, não é uma aprovação para produção nem um compromisso de dependência. A documentação pública consultada é a que melhor cobre simultaneamente as quatro plataformas e expõe uma API explícita de datagramas.

A documentação oficial mostra:
- suporte a QUIC v1 e integração TLS 1.3;
- suporte ao recurso de datagramas não confiáveis;
- callbacks de leitura/escrita/perda e notificação de MSS;
- API xqc_datagram_send e xqc_datagram_get_mss;
- callback de verificação de certificado do peer no cliente;
- builds documentados para Windows, macOS, Android e iOS;
- dependência de CMake e BoringSSL ou BabaSSL;
- licença Apache-2.0.

## Bloqueios que a POC precisa resolver antes de integrar

1. Fixar uma versão/tag ou commit imutável e registrar hashes de todos os componentes e submódulos; não usar branch móvel como dependência de produção.
2. Construir e executar testes em Windows x64, macOS arm64, Android arm64 e iOS arm64 com os SDKs suportados.
3. Confirmar QUIC v1 e negociação DATAGRAM real em ambos os sentidos; testar tamanho máximo de datagrama, MSS, caminho MTU e comportamento quando DATAGRAM não é negociado.
4. Demonstrar validação rigorosa do certificado/identidade do servidor e falha fechada para certificado inválido, expirado, hostname/SNI incorreto e servidor impostor.
5. Demonstrar autenticação individual do dispositivo. A API pública consultada identifica um callback de verificação de certificado no cliente, mas isso não comprova que a autenticação mútua de certificados de cliente está corretamente suportada e configurada no servidor. Verificar essa capacidade no código e em teste cliente-servidor; se não existir, rever formalmente o desenho de identidade antes de avançar.
6. Desabilitar 0-RTT/early data para todas as operações da aplicação e provar por teste que nenhum comando de estado é processado antes de 1-RTT autenticado. Não basta não chamar a API de early data: revisar tickets, flags e defaults da versão fixada.
7. Confirmar que a API nunca é chamada pelo callback de áudio; todas as chamadas ficam em uma thread de rede dedicada, com filas prealocadas, backpressure e shutdown ordenado.
8. Verificar licença e avisos de todas as dependências transitivas, incluindo BoringSSL/BabaSSL e submódulos; gerar SBOM e política de atualização.
9. Executar testes de perda, duplicação, reordenação, jitter, reconexão, revogação, flood, encerramento concorrente e 16 dispositivos.
10. Revisar a superfície de API que permite desabilitar criptografia. Em particular, a aplicação não pode expor configuração equivalente a no-crypto/insecure mode; qualquer flag desse tipo deve permanecer forçada para o modo criptográfico e ter teste estático/runtime.
11. Obter revisão de segurança do código de integração e medir latência/jitter/CPU/memória em hardware real.

## Regras de segurança da integração

- Nunca habilitar fallback UDP/TCP em texto claro.
- Nunca habilitar no-crypto mode.
- Não aceitar comandos de estado em 0-RTT.
- Não tratar o handshake QUIC como autorização de usuário; autenticação de dispositivo e autorização AUX são camadas separadas.
- O servidor determina a identidade e as permissões; o cliente não escolhe seu próprio papel ou AUX.
- Não transmitir áudio até o handshake seguro, a identidade e a autorização estarem concluídos.
- Falha de certificado, autenticação, autorização, integridade ou negociação fecha a sessão.
- Nenhuma chamada de socket, TLS ou QUIC no callback de áudio.

## Critério de aprovação

A POC só pode ser promovida a integração quando os bloqueios acima tiverem evidências reproduzíveis, builds verdes nas quatro plataformas, testes adversariais aprovados, revisão de licença/dependências e confirmação de segurança. Até lá, XQUIC permanece apenas candidata de POC e o transporte de produção continua desabilitado.
