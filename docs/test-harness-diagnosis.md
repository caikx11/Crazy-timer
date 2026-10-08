# Diagnóstico do harness — tarefa 01

Data: 08/10/2026. Estado atual: após reinicialização, compilação e execução voltaram a funcionar; ciclo de sucesso/falha intencional/sucesso validado. Revisão e commit pendentes.

## Resultado mais recente

Após o usuário informar reinicialização, o mesmo runner compilou e executou três vezes: 130 checks aprovados, asserção falsa detectada (exit 1), fonte restaurada e 130 checks aprovados. Não houve mudança de proteção/ACL pelo agente. O sintoma de sobrescrita foi resolvido nesta sessão, mas o componente causador não foi identificado. Estado atual de proteção não medido. Evidências completas em [validação da tarefa 01](validation/task-01.md).

## Diagnóstico anterior à reinicialização

08/10/2026: diagnóstico de acesso aprofundado, sem novas compilações. CreateFileW/OPEN_EXISTING com compartilhamento read/write/delete abre scheduler_test.exe para leitura, mas nega GENERIC_WRITE e DELETE com WinError 5; nenhum conteúdo alterado. Mesmo resultado fora do sandbox sob CAIO_PC\caio_. ACL permite FullControl ao usuário; atributo 0x20 (Archive). FileStandardInfo retornou DeletePending=False, um link, 1510912 bytes. Arquivo de texto temporário na mesma pasta foi criado, sobrescrito, relido e excluído com sucesso. scripts/test.ps1 abre para escrita; antiga .runner.lock nega leitura/escrita/exclusão. Restart Manager fora do sandbox não reportou processo comum usando o executável; consulta dentro do sandbox falhou com código 29 (não usar essa falha como prova de ausência de bloqueio). Diagnóstico confirma restrição específica no executável, mas não identifica o componente responsável. Próximo passo depende do usuário: salvar trabalho e reiniciar Windows, mantendo proteção ativada; depois repetir ./scripts/test.ps1 no mesmo caminho, sem mudar flags ou criar variantes. Reinicialização é teste diagnóstico, não correção garantida. Ainda faltam compilação completa e asserção intencionalmente falsa. Tarefa 01 ready-for-human/open; 02 bloqueada. Nenhuma proteção/ACL alterada, nenhuma operação real de energia, nenhum processo deixado em andamento. Recomendação Astra/high.

Referência do campo consultado: [FILE_STANDARD_INFO — Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_standard_info).

## Histórico anterior

Nova tentativa após o usuário informar que tentou adicionar o Codex à exceção: o arquivo existente no caminho fixo executou 130 verificações com código zero. O runner encontrou Acesso negado ao abrir a antiga .runner.lock, dentro e fora do sandbox. Substituída trava persistente por mutex nomeado por projeto/sessão Windows; verificação com processo filho confirmou recusa de concorrência antes de compilar e sintaxe validada. Nenhuma permissão alterada. Em seguida, o linker não conseguiu sobrescrever o executável restaurado, dentro e fora do sandbox. ACL visível concede controle ao usuário e arquivo não tem atributo somente leitura; Restart Manager não identificou processo comum usando o arquivo. A causa da negativa de sobrescrita não foi comprovada, portanto não atribuí-la automaticamente ao Bitdefender. Ciclo de recompilação e ensaio de asserção falha seguem pendentes.

Usuário enviou nova janela de erro de restauração referente ao build antigo test-fb2af97efd17482cbb8a7496b63c4551. Pasta antiga confirmada ausente. Pasta fixa build/tests existe e está sem scheduler_test.exe; não apareceu metadado desse destino nos 12 registros recentes consultados. Orientação: usar Procurar da própria janela para selecionar build/tests e restaurar somente o item escolhido nesse destino. Não houve recompilação, restauração ou execução pelo agente nesta verificação; aguardar confirmação do usuário e verificar o arquivo antes de tentar executar.

Após o usuário restaurar o build test-c81dafbfebe44b35ba084c6fb3dbd3a7, sua execução retornou `PASS: 130 deterministic checks; no power APIs linked.` e código zero. Uma compilação nova em diretório aleatório voltou a receber Acesso negado. Não foi feito o ensaio de asserção intencionalmente falha, porque novas compilações não iniciam.

O script passou a usar destino fixo `build/tests/scheduler_test.exe`, com trava exclusiva de arquivo para impedir compilações simultâneas. Confirmados: recusa de segunda execução antes de compilar, liberação da trava após erro e sintaxe PowerShell válida. O primeiro build nesse caminho fixo compilou e também recebeu Acesso negado; consulta aos oito metadados recentes não encontrou ainda quarentena correspondente. Não confundir ausência de metadado com liberação. Nenhuma proteção ou exceção alterada pelo agente. Usar somente esse caminho fixo em próximas orientações; evitar nova sequência de diretórios GUID.

## Reprodução

O comando `./scripts/test.ps1` compila o teste com o Clang do Windhawk e falha ao iniciar o executável, com Acesso negado. Nova tentativa do mesmo arquivo também retornou arquivo em uso por outro processo. O programa de testes não chegou a executar: compilação não significa teste aprovado.

Reprodução mínima em pasta de diagnóstico ignorada pelo Git: um programa que só retorna zero executou; programas que imprimem uma mensagem por iostream ou cstdio não iniciaram. Logo, a hipótese de falha exclusiva de iostream foi descartada. Arquivos comparados tinham permissões equivalentes e nenhum stream de origem da Internet.

## Evidência confirmada

Consulta somente leitura ao metadado de quarentena do Bitdefender identificou o executável da tentativa desta sessão, no diretório de build `test-c662c25c93bf4bbf89c66972b767ab49`, com detecção **Gen:Variant.Application.Fragtor.851**. Isso confirma a quarentena para o scheduler_test.exe desta tentativa. Não prova qual característica do binário causou a detecção nem que seja falso positivo.

Defender e Bitdefender constam como produtos registrados. Eventos recentes de Code Integrity/AppLocker/Defender não explicaram este bloqueio. Não foi alterada nenhuma proteção, política, ACL ou exceção; nenhum conteúdo foi restaurado da quarentena.

## Mudança pequena no script

O runner informa quando a compilação termina e diferencia erro ao iniciar de resultado dos testes. Preserva a exceção original e restaura TMP/TEMP em finally. Não repete automaticamente a execução bloqueada nem decide sozinho que a detecção é falso positivo.

## Verificação ainda necessária

### Falha na restauração manual

Usuário enviou imagem do Bitdefender com falha ao restaurar no destino, referente ao build antigo test-6287ab8aec24455eb3c5782dd743eea3. Verificação por caminhos absolutos constatou que tanto essa pasta quanto a do build test-c81dafbfebe44b35ba084c6fb3dbd3a7 não existiam. Recriadas somente essas duas pastas vazias. Ausência do destino é hipótese para a falha de restauração, não causa definitivamente comprovada. Nenhum arquivo restaurado, exceção criada ou proteção alterada pelo agente. Orientação oficial consultada: https://www.bitdefender.com.br/consumer/support/answer/50457/ (restauração pela UI, privilégios administrativos quando necessários).

### Tentativa solicitada sem recompilar

Em nova tentativa pedida pelo usuário, o executável `test-c81dafbfebe44b35ba084c6fb3dbd3a7/scheduler_test.exe` já não estava presente. Consulta somente leitura confirmou agora o metadado de quarentena desse caminho, datado de 08/10/2026 16:01:11 (horário local exibido), detecção Gen:Variant.Application.Fragtor.851. Nenhum teste executado nem recompilação realizada nessa tentativa. Usuário avisado conforme solicitado; aguardando sua decisão/ação antes de repetir. O agente não restaurou nem removeu itens da quarentena.

### Retentativa após intervenção do usuário

O usuário informou que desativou a detecção. Nova compilação gerou o build `test-c81dafbfebe44b35ba084c6fb3dbd3a7`; a execução falhou com Acesso negado e, fora do sandbox, arquivo em uso por outro processo. Não foi encontrada correspondência desta tentativa nos quatro metadados de quarentena mais recentes; isso não comprova ausência de bloqueio.

Consulta somente leitura via Restart Manager identificou **Bitdefender Virus Shield, serviço VSSERV, PID 2988** usando o executável desta nova tentativa. Assim, o arquivo continua retido pelo serviço, mesmo após a mudança relatada pelo usuário. Nenhum serviço foi encerrado/reiniciado e nenhuma configuração de proteção foi alterada pelo agente. Script diagnóstico descartável preservado apenas na pasta build/diagnostic-01, ignorada pelo Git. A suíte e o ensaio de asserção falha continuam sem executar.

- Após resolução apropriada da detecção ou disponibilização de ambiente isolado autorizado, executar a suíte original.
- Introduzir uma asserção temporariamente incorreta no teste, comprovar falha do comando e restaurá-la; executar a suíte final novamente.
- Confirmar isolamento do executor de energia e documentar os resultados.

O comando wsl está presente, mas informa que WSL não está instalado; Docker não foi encontrado no PATH. Nenhum pacote ou ambiente foi instalado. Revisão da detecção pelo usuário/fornecedor ou ambiente de validação autorizado é a dependência externa atual.

Sem repositório Git local e sem identidade de autor configurada detectável. Commit e revisão formal por diff ainda não realizados; não inventar autoria. Nenhuma ação de energia executada.

### Comparação após desativação temporária relatada pelo usuário

08/10/2026: usuário informou desativação temporária do Bitdefender para comparação. Repeti ./scripts/test.ps1 no mesmo caminho fixo, sem alterar código/configurações, dentro e fora do sandbox. Ambas as tentativas falharam no linker: failed to write output scheduler_test.exe: Permission denied; unable to remove file: Permission denied; exit 1. A execução da suíte não foi alcançada. A mudança relatada não eliminou o sintoma; resultado não prova nem descarta interferência residual do Bitdefender e não confirma nova quarentena. Usuário informado que pode reativar a proteção. Próximo passo: identificar a origem da negativa de sobrescrita; ainda faltam recompilação completa e ensaio de asserção falsa. Tarefa 01 needs-info/open; 02 bloqueada. Nenhuma proteção/ACL alterada pelo agente. Modelo recomendado Astra/high.
