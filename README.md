# dB Studio 0.3.1 — Virtual Lab

Prévia técnica do núcleo de áudio. Esta compilação não é ainda o mixer gráfico final nem um sistema pronto para uso em apresentações.

## Conteúdo
- Simulador determinístico de 32 canais.
- Renderização de 16 mixes estéreo independentes.
- Enquadramento PCM próprio e teste de transporte UDP local.
- Testes automatizados.

## Compilação
O GitHub Actions compila em runner Windows e publica o diretório executável como artefato. O alvo Python 3.10 x64 foi escolhido para ampliar a possibilidade de compatibilidade com Windows 8.1; ainda será necessário testar no computador de destino.

## Limitações
Não implementa Dante/AES67/AVB ou protocolos de fabricantes. Não inclui interface gráfica, aplicativo móvel, pareamento, autenticação ou criptografia. A compatibilidade de áudio físico/ASIO não está validada.
