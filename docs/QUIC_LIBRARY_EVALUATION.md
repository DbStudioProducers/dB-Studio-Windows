# dB Studio — avaliação de bibliotecas QUIC

Data da avaliação: 10/11/2026  
Estado: XQUIC v1.9.8 é a biblioteca escolhida pelo projeto. A integração ainda não está concluída nem aprovada para uso de campo.

## Critérios obrigatórios

A biblioteca precisa oferecer QUIC v1 com TLS 1.3, QUIC DATAGRAM (RFC 9221), streams confiáveis, builds verificáveis para Windows/macOS/Android/iOS, validação de certificado do servidor, caminho seguro para identidade individual dos dispositivos, licença compatível, manutenção ativa e API que permita manter todo o trabalho de rede fora do callback de áudio.

Uma página de suporte a plataforma não substitui um build real em cada SDK/arquitetura. Uma API de validação de certificado do servidor também não prova, por si só, autenticação mútua de certificados de cliente.

## Decisão do projeto

**XQUIC v1.9.8 é a implementação QUIC selecionada para o dB Studio.** Não será conduzida uma trilha comparativa com MsQuic ou outras bibliotecas. A versão fica fixada no commit `b2d1f2ca05d3456614838c82bf50cafe5c060262`, com licença Apache-2.0 declarada no repositório upstream. A escolha não substitui a necessidade de validar build, API, TLS, DATAGRAM, autenticação, dependências e comportamento nas plataformas do produto.

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


## Atualização de baseline da POC — 16/11/2026

A release pública mais recente encontrada durante esta revisão é **XQUIC v1.9.8**, publicada em 19/10/2026. O ref GitHub `refs/tags/v1.9.8` resolve atualmente para o commit:

```text
b2d1f2ca05d3456614838c82bf50cafe5c060262
```

Esse SHA deve ser usado como baseline reproduzível da POC, em vez de referenciar `main` ou uma tag móvel. A release informa correções recentes no transporte, validação de parâmetros de transporte e tratamento de early data/tickets inválidos. Isso é sinal favorável para avaliação, mas não constitui auditoria de segurança nem prova de que 0-RTT esteja desabilitado por padrão.

### Decisão de engenharia

- **Prosseguir com XQUIC v1.9.8 apenas em uma POC isolada**, fixada no SHA acima.
- Não adicionar XQUIC ao CMake principal nem ao binário distribuível enquanto a matriz de builds e licenças não estiver aprovada.
- Não ativar 0-RTT; a configuração deve ser explicitamente desabilitada e coberta por teste negativo.
- Usar a API QUIC DATAGRAM apenas para áudio; autenticação, atribuição AUX e comandos devem usar streams confiáveis após handshake autenticado.
- Manter o codec `AudioPacketCodec.h` como formato interno do payload, nunca como mecanismo de autenticação ou criptografia.
- A camada de transporte deve expor uma interface pequena e independente da biblioteca, para permitir substituição caso XQUIC falhe em iOS/Android ou na revisão de licença.

### Portões antes de integrar

1. Checkout pelo SHA fixo e captura recursiva de submódulos; registrar SHA de cada dependência e SBOM.
2. Inspecionar o código da versão fixada para confirmar opções de criptografia, callbacks de certificado, autenticação de cliente e controle de early data; não confiar apenas na documentação.
3. Compilar exemplos upstream sem alterações primeiro; depois criar um cliente/servidor mínimo do dB Studio em Windows x64.
4. Confirmar negociação QUIC v1 + DATAGRAM em ambas as direções e provar rejeição de certificado inválido, expirado e identidade incorreta.
5. Confirmar que credencial revogada não estabelece sessão autorizada e que o servidor ignora qualquer AUX/role informado pelo cliente.
6. Repetir a prova no Android arm64; só então iniciar o cliente iOS com a mesma interface de transporte.
7. Comparar tamanho final de datagrama com MTU real: o codec atual gera no máximo 1072 bytes de payload de aplicação, mas cabeçalhos QUIC/UDP/IP precisam ser contabilizados separadamente.
8. Testar perda, reorder, duplicação, jitter, suspensão/retomada Android, mudança de IP e shutdown concorrente antes de integrar ao motor JUCE.

### Regra de implementação

XQUIC é a trilha de implementação do produto. Se surgir um bloqueio técnico, a equipe deve resolver ou documentar formalmente o bloqueio antes de qualquer mudança de biblioteca; não iniciar comparativos paralelos por padrão. Não usar UDP próprio, criptografia caseira, fallback em texto claro ou modo sem criptografia.

**Estado:** XQUIC ainda não está integrada; nenhum tráfego QUIC do dB Studio foi transmitido e nenhum requisito de segurança foi marcado como aprovado.
