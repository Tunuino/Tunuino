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

## Painel e calibrações

- Seguir o painel compacto aprovado pelo utilizador: seis mostradores numa fila, oito indicadores, tabelas 16×16, apenas Estado do motor na lateral, sem coluna Registo, ignição e superfície 3D em baixo. Navegação principal plana; configurações adicionais no grupo secundário.
- Reunir calibrações em Sensores e calibrações. Assistentes preparam a edição; pontos medidos vêm do ADC real da ligação selecionada. Preservar modos, proteções e restantes calibrações ao guardar apenas um sensor.
- Modelos NTC indicam R25, Beta e pull-up; não os apresentar como calibrações universais de um motor ou sensor sem identificação.
- Pesquisas automáticas de portas/dispositivos são tarefas de descoberta, não operações exclusivas: não alternar os controlos entre ativo/inativo nem reconstruir listas inalteradas. Suspender DFU com COM ocupada ou página não apresentada.

- Usar o PNG original tunuino.png no cabeçalho; não substituir por um símbolo redesenhado.
- O desenho dos mapas acompanha a área atribuída sem participar no cálculo do tamanho preferido do seu contentor. Separadores ajustáveis não alteram dados nem voltam à posição inicial durante a monitorização.

## Gravação e relógio USB

- Depois de um dispatch que possa gravar na flash, atualizar o instante do ciclo USB antes de consultar/transmitir a resposta. O link começa o prazo de transmissão pelo relógio posterior ao dispatch; reutilizar o instante anterior causa um falso recuo do relógio e reinicia a ligação.
- Manter os testes do main real com persistência demorada, confirmação física de transmissão, falha de persistência e isolamento da sessão. Não desativar a deteção de relógio ou de prazo para mascarar uma falha.

## Modos e aprendizagem VE

- Manter o perfil sequencial formato 1 compatível. Modos 360° usam formato 2, byte 28: 1 semi-sequencial, 2 batch simultâneo, ambos wasted COP com uma saída por cilindro. Não aceitar esses modos como formato 1.
- Nos modos 360°, repartir combustível efetivo por dois eventos e somar tempo morto em cada pulso. Limitar dwell e injeção à ocupação do período real. A falta de cames só é permitida nos modos configurados para cambota; perda de cambota mantém corte retido.
- Autotune usa exclusivamente amostras USB e base lida da mesma ECU/geração. Não gravar durante recolha nem com saídas armadas. Antes de guardar, confirmar base/geração e criar cópia; usar escrita e releitura existentes. Não remover testes nem transformar resultados sintéticos em validação física.

## Bobinas partilhadas e projeto completo

- Perfil formato 3, modos 3/4: semi-sequencial/batch com bobina partilhada por exatamente dois cilindros a 360°. Só esses perfis permitem repetir uma IGN. Não repetir injetores; não permitir três cilindros numa bobina nem ajustes de avanço diferentes num par.
- Mostrar uma função e um teste de ignição por bobina partilhada, identificada pelo par. Mover essa função deve atualizar os dois descritores. Propostas de ligação do par não substituem a cablagem lida.
- O projeto .tnproj inclui as quatro páginas e mapas completos, valida alvo/formato/CRC e não arma saídas ao abrir ou restaurar. Restaurar exige snapshot limpo e atual da mesma ECU, cópia anterior criada primeiro, páginas preparadas e relidas, uma única gravação e releitura completa. Após pedido de Burn incerto, orientar para releitura sem repetição automática.
