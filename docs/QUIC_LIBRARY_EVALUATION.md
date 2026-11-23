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


## Verificação de viabilidade Windows — 23/11/2026

A leitura direta de `docs/Platforms.md` no próprio tag **v1.9.8** revelou uma inconsistência importante: o parágrafo introdutório lista Windows entre as plataformas suportadas, mas a seção Windows ainda declara explicitamente que o suporte se limita a v1.1.0–v1.3.0 e que v1.4.0 não oferece suporte temporário. A seção não confirma que o suporte foi restaurado nas versões posteriores. Portanto, não é correto considerar Windows x64 validado para v1.9.8 com base apenas na lista geral de plataformas.

Para transformar essa incerteza em evidência executável, foi adicionado o workflow isolado `.github/workflows/xquic-windows-poc.yml`. Ele fixa XQUIC no SHA `b2d1f2ca05d3456614838c82bf50cafe5c060262`, fixa também o snapshot BoringSSL usado na tentativa e tenta compilar a biblioteca estática x64 em runner Windows 2022. O workflow não adiciona dependências ao produto nem ao CMake principal.

### Gate obrigatório

- Se o build falhar por incompatibilidade de plataforma/código upstream, manter XQUIC fora do Desktop Windows e registrar o erro exato; não contornar com UDP próprio, modo sem criptografia ou fallback em texto claro.
- Se o build passar, isso prova somente compilação da biblioteca. Ainda será necessário construir e executar um cliente-servidor dB Studio real, negociar QUIC v1/DATAGRAM, validar TLS 1.3 e identidade, desabilitar 0-RTT, testar revogação e autorização AUX.
- O BoringSSL fixado no workflow é um snapshot de POC, não uma aprovação de dependência para distribuição. Licença, avisos, SBOM e atualização segura continuam pendentes.
- Nenhuma integração de produção deve ser ativada antes de o gate Windows e os gates Android/iOS/macOS serem comprovados.

**Estado em 23/11/2026:** risco de compatibilidade Windows identificado; build de viabilidade isolado adicionado, resultado ainda não disponível. XQUIC continua escolhida pelo projeto, mas não aprovada para integração de produção.


## Resultado da primeira compilação Windows — 23/11/2026

A execução GitHub Actions **XQUIC pinned Windows feasibility POC #5** chegou até a compilação da biblioteca. BoringSSL compilou, a configuração CMake do XQUIC passou e os arquivos C começaram a ser compilados no runner Windows Server 2022. A falha não foi falta de suporte de plataforma nem falha de TLS: o MSVC rejeitou as declarações `_Static_assert` de `src/transport/xqc_conn.h` porque a configuração anterior não solicitava explicitamente o padrão C11. Os erros primários foram C2143/C2059 nas linhas 234–240; os demais erros em cascata derivaram desse ponto.

### Correção aplicada

O workflow agora configura explicitamente:

```text
-DCMAKE_C_STANDARD=11
-DCMAKE_C_STANDARD_REQUIRED=ON
```

Isso habilita o modo C11 no MSVC, necessário para as verificações estáticas usadas pelo XQUIC. A alteração é restrita ao workflow da POC e não modifica fontes upstream nem o CMake principal do dB Studio.

**Estado:** causa identificada e correção enviada; aguardar a nova execução. Ainda não há aprovação do build XQUIC até o job terminar com sucesso.
