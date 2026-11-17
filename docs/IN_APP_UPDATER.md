# dB Studio — atualização no próprio aplicativo

O botão **ATUALIZAR** inicia o atualizador Windows ao lado do executável e encerra o aplicativo para permitir a substituição segura do binário. As configurações do usuário ficam fora da pasta do executável e não são sobrescritas.

## Canal de atualização Windows 8.1

O atualizador consulta a última GitHub Release deste repositório e exige os dois assets:
- `dB-Studio-Windows-8.1.zip`
- `dB-Studio-Windows-8.1.zip.sha256`

O ZIP deve conter `dB Studio.exe`. O arquivo SHA-256 deve começar com o hash hexadecimal de 64 caracteres do ZIP. O atualizador baixa os assets por HTTPS, compara o hash, extrai em pasta temporária, aguarda o encerramento de todas as instâncias e substitui o executável. Mantém backup e restaura o executável anterior se a cópia/validação falhar.

A distribuição ainda precisa publicar esses assets em uma GitHub Release. A CI de validação não cria nem publica releases automaticamente. Não há certificado Authenticode configurado: SHA-256 detecta corrupção/inconsistência, mas não equivale a assinatura digital nem protege contra comprometimento da conta/release. A atualização em produção deve permanecer bloqueada até definir assinatura de código e política de publicação protegida.

O script usa Windows PowerShell 4/.NET ZIP para manter compatibilidade com Windows 8.1. O atualizador não eleva privilégios e não instala drivers.

## Escopo

- Atualização substitui o executável, sem reinstalação e sem apagar preferências.
- Não altera arquivos de sistema, drivers ou redistributables.
- Falha de download/hash/extracção não deve substituir o executável atual.
- A variante JUCE 8/Windows 10+ requer seu próprio pacote/canal e não deve receber o pacote Windows 8.1.
