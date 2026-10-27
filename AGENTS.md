# Instruções do projeto

## README e publicações

- O README.md da raiz contém apenas uma descrição curta do que é a Tunuino e para que serve.
- Publicar atualizações de progresso, listas de funcionalidades, resultados de testes, limitações, planos e instruções de utilização em https://github.com/Tunuino/Tunuino_documents (branch main).
- Não acrescentar essas publicações ao README.md deste repositório. Manter a documentação técnica junto do código quando necessária à implementação.
- Ao alterar funcionalidades, atualizar a publicação correspondente em Tunuino_documents e distinguir testes simulados de validação física.

## Identidade do projeto

- Usar Tunuino na apresentação do projeto, interface, comentários próprios, documentação e notas de release.
- Não inserir marcas de outros projetos de ECU nem apresentar a Tunuino como baseada noutro projeto de ECU.
- Preservar os avisos de autoria e licença exigidos pelos componentes de terceiros, sem os transformar em marca da Tunuino.

## Interface e configurações rápidas

- Organizar o fluxo normal pela função do sensor/atuador, com valores de base editáveis e a ligação habitual da Proteus identificada por conector e contacto.
- Para uma função, mostrar apenas Desativada, Recomendada/Proposta e a ligação Atual quando diferente. Não repetir o mesmo nome de função para todos os canais possíveis.
- Colocar a lista completa de contactos em Personalizar e a matriz manual de terminais em Avançado, fechada por predefinição.
- Fundamentar contactos no pinout e no PCB da revisão suportada. Distinguir ligações habituais de propostas da aplicação; não inventar recomendações para funções sem ligação fixa.
- Preservar configurações existentes ao ler ou atualizar a Console. Nunca alterar a cablagem guardada, ativar um controlo ou armar saídas só por mostrar uma referência.
- As escolhas rápidas não podem retirar uma ligação ocupada por outra função. Conflitos exigem escolha explícita.
- Não mostrar GPIO do MCU como escolhas normais. Mostrar tempos de injeção, dwell e tempo morto em milissegundos.
